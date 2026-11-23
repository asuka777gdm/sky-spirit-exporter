#ifndef SKY_AUTH_H
#define SKY_AUTH_H
#include <string.h>
#include <stddef.h>
/* Only plain hexadecimal session identifiers; never accept JSON escapes. */
static int identifier(const char *s, size_t n, int uuid) {
 if(n!=(uuid?36u:32u))return 0;
 int nonzero=0;
 for(size_t i=0;i<n;i++) {
  if(uuid&&(i==8||i==13||i==18||i==23)){if(s[i]!='-')return 0;continue;}
  if(!((s[i]>='0'&&s[i]<='9')||(s[i]>='a'&&s[i]<='f')))return 0;
  if(s[i]!='0')nonzero=1;
 }return nonzero;
}
/* Bounded top-level string lookup. Nested objects and escaped names cannot match. */
static int authField(const char *s,size_t n,const char *key,char *out,int uuid) {
 int depth=0;size_t k=strlen(key);
 for(size_t i=0;i<n;i++) {
  if(s[i]=='{'||s[i]=='['){depth++;continue;}
  if(s[i]=='}'||s[i]==']'){depth--;continue;}
  if(s[i]!='"')continue;
  size_t begin=++i;int escaped=0;
  for(;i<n&&s[i]!='"';i++)if(s[i]=='\\'){escaped=1;if(++i>=n)return 0;}
  if(i>=n)return 0;
  if(depth!=1||escaped||i-begin!=k||memcmp(s+begin,key,k))continue;
  size_t p=i+1;while(p<n&&(s[p]==' '||s[p]=='\r'||s[p]=='\n'||s[p]=='\t'))p++;
  if(p>=n||s[p++]!=':')continue;
  while(p<n&&(s[p]==' '||s[p]=='\r'||s[p]=='\n'||s[p]=='\t'))p++;
  if(p>=n||s[p++]!='"')continue;
  size_t start=p;while(p<n&&s[p]!='"')p++;
  if(p<n&&identifier(s+start,p-start,uuid)){memcpy(out,s+start,p-start);out[p-start]=0;return 1;}
 }return 0;
}
#endif
