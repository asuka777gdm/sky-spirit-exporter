#include <assert.h>
#include <stdio.h>
#include "auth.h"
int main(void){
 const char *u="12345678-1234-5678-90ab-123456789abc",*s="123456789abcdef0123456789abcdef0";
 char text[512],out[40];
 sprintf(text,"{\"nested\":{\"user\":\"%s\"},\"user\": \"%s\",\"session\":\"%s\"}",u,u,s);
 assert(authField(text,strlen(text),"user",out,1)&&!strcmp(out,u));
 assert(authField(text,strlen(text),"session",out,0)&&!strcmp(out,s));
 sprintf(text,"{\"nested\":{\"user\":\"%s\"}}",u);assert(!authField(text,strlen(text),"user",out,1));
 assert(!identifier("00000000000000000000000000000000",32,0));
 assert(!identifier(s,31,0));assert(!authField("{\"user\":\"bad",12,"user",out,1));
 puts("auth checks passed");return 0;
}
