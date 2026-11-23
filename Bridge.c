#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#include <sddl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "MinHook.h"
#include "auth.h"
static SRWLOCK gate=SRWLOCK_INIT;
static char user[37],session[33];
static DWORD build,pathOffset;static volatile LONG state; /* 1 waiting, 2 ready, 3 unsupported, 4 hook error */
static void *(__fastcall *original)(void*,void*,void*,void*);
static BOOL readMem(const void *p,void *out,SIZE_T size){SIZE_T n=0;return ReadProcessMemory(GetCurrentProcess(),p,out,size,&n)&&n==size;}
static BOOL hashFile(const wchar_t *path,char out[65]){
 HANDLE f=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
 if(f==INVALID_HANDLE_VALUE)return FALSE;
 BCRYPT_ALG_HANDLE a=NULL;BCRYPT_HASH_HANDLE h=NULL;BYTE digest[32],*buf=malloc(65536);DWORD n;BOOL ok=FALSE;
 if(!buf){CloseHandle(f);return FALSE;}
 if(BCryptOpenAlgorithmProvider(&a,BCRYPT_SHA256_ALGORITHM,NULL,0)<0)goto done;
 if(BCryptCreateHash(a,&h,NULL,0,NULL,0,0)<0)goto done;
 for(;;){if(!ReadFile(f,buf,65536,&n,NULL))goto done;if(!n)break;if(BCryptHashData(h,buf,n,0)<0)goto done;}
 if(BCryptFinishHash(h,digest,32,0)<0)goto done;
 for(int i=0;i<32;i++)sprintf(out+i*2,"%02x",digest[i]);out[64]=0;ok=TRUE;
 done:if(h)BCryptDestroyHash(h);if(a)BCryptCloseAlgorithmProvider(a,0);free(buf);CloseHandle(f);return ok;
}
static int sso(BYTE *p,char *out,size_t capacity){
 BYTE v[32];if(!readMem(p,v,32))return 0;
 size_t n=*(size_t*)(v+16),cap=*(size_t*)(v+24);
 if(!n||n>=capacity||n>cap)return 0;
 if(cap>=16){if(!readMem(*(void**)v,out,n))return 0;}else memcpy(out,v,n);
 out[n]=0;return 1;
}
static void observe(BYTE *url,BYTE *body){
 char host[64]={0},path[10]={0},u[37]={0},s[33]={0};
 if(!readMem(url+8,host,63)||strcmp(host,"live.radiance.thatgamecompany.com"))return;
 if(!readMem(url+pathOffset,path,9)||memcmp(path,"/account/",9))return;
 DWORD n=0;void *data=NULL;
 if(readMem(body+0x818,&n,4)&&n>0&&n<49152&&readMem(body+0x810,&data,8)){
  char *text=malloc(n);if(text){if(readMem(data,text,n)){
   if(!authField(text,n,"user",u,1))authField(text,n,"user_id",u,1);
   authField(text,n,"session",s,0);
  }SecureZeroMemory(text,n);free(text);}
 }
 DWORD count=0;
 if(readMem(body+0x808,&count,4)&&count<=32)for(DWORD i=0;i<count;i++){
  char name[64],value[80];BYTE *entry=body+8+i*64;
  if(!sso(entry,name,sizeof(name))||!sso(entry+32,value,sizeof(value)))continue;
  if((!strcmp(name,"user-id")||!strcmp(name,"x-user-id"))&&identifier(value,strlen(value),1))strcpy(u,value);
  if((!strcmp(name,"session")||!strcmp(name,"x-session-token"))&&identifier(value,strlen(value),0))strcpy(s,value);
  SecureZeroMemory(value,sizeof(value));
 }
 if(u[0]&&s[0]){AcquireSRWLockExclusive(&gate);memcpy(user,u,37);memcpy(session,s,33);InterlockedExchange(&state,2);ReleaseSRWLockExclusive(&gate);}
 SecureZeroMemory(u,sizeof(u));SecureZeroMemory(s,sizeof(s));
}
static void *__fastcall hook(void *a,void *b,void *url,void *body){observe(url,body);return original(a,b,url,body);}
static DWORD WINAPI serve(void *unused){
 (void)unused;
 wchar_t exe[MAX_PATH],pipe[100];char hash[65];DWORD rva=0;
 DWORD length=GetModuleFileNameW(NULL,exe,MAX_PATH);
 if(length>0&&length<MAX_PATH&&hashFile(exe,hash)){
  if(!strcmp(hash,"4514c85f73cfe4b6d7d9910b36585ce5e135cfd3a04167c1dbc88d14ea96dba4")){build=410941;rva=0x6ff50;pathOffset=0x8e;}
  if(!strcmp(hash,"9723e07be346a57e3f99dce52641edc47dcb9b278727e41b459e87c7d9f46567")){build=423272;rva=0x707f0;pathOffset=0x90;}
  if(!strcmp(hash,"bdd780f88a7d34f7c3a9b336e82dc57393c3a8a7f0e639f28c41e709b9134b19")){build=426499;rva=0x707f0;pathOffset=0x90;}
 }
 state=3;
 if(rva){
  const BYTE prefix[]={0x55,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x56,0x57,0x53,0x48,0x81,0xec,0x98,0x09,0,0};
  BYTE *target=(BYTE*)GetModuleHandleW(NULL)+rva;
  state=4;
  if(!memcmp(target,prefix,sizeof(prefix))&&MH_Initialize()==MH_OK&&MH_CreateHook(target,hook,(void**)&original)==MH_OK){
   state=1;if(MH_EnableHook(target)!=MH_OK)state=4;
  }
 }
 /* Same Windows user and SYSTEM only; no remote pipe connections. */
 HANDLE token=NULL;DWORD needed=0;PSECURITY_DESCRIPTOR sd=NULL;
 if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token))return 0;
 GetTokenInformation(token,TokenUser,NULL,0,&needed);TOKEN_USER *tu=malloc(needed);
 if(!tu||!GetTokenInformation(token,TokenUser,tu,needed,&needed)){if(tu)free(tu);CloseHandle(token);return 0;}
 wchar_t *sid=NULL,sddl[512];if(!ConvertSidToStringSidW(tu->User.Sid,&sid)){free(tu);CloseHandle(token);return 0;}
 swprintf(sddl,512,L"D:P(A;;GA;;;SY)(A;;GA;;;%s)",sid);LocalFree(sid);free(tu);CloseHandle(token);
 if(!ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl,SDDL_REVISION_1,&sd,NULL))return 0;
 SECURITY_ATTRIBUTES sa={sizeof(sa),sd,FALSE};
 swprintf(pipe,100,L"\\\\.\\pipe\\SkySpiritExport-%lu",GetCurrentProcessId());
 for(;;){
  HANDLE p=CreateNamedPipeW(pipe,PIPE_ACCESS_OUTBOUND|FILE_FLAG_FIRST_PIPE_INSTANCE,PIPE_TYPE_BYTE|PIPE_READMODE_BYTE|PIPE_WAIT|PIPE_REJECT_REMOTE_CLIENTS,1,512,0,2000,&sa);
  if(p==INVALID_HANDLE_VALUE)break;
  if(ConnectNamedPipe(p,NULL)||GetLastError()==ERROR_PIPE_CONNECTED){
   char packet[256];DWORD written;
   AcquireSRWLockShared(&gate);
   int n=sprintf(packet,"{\"state\":%ld,\"build\":%lu,\"user\":\"%s\",\"session\":\"%s\"}\n",state,build,user,session);
   ReleaseSRWLockShared(&gate);
   WriteFile(p,packet,n,&written,NULL);SecureZeroMemory(packet,sizeof(packet));
  }
  DisconnectNamedPipe(p);CloseHandle(p);
 }
 LocalFree(sd);return 0;
}
BOOL WINAPI DllMain(HINSTANCE mod,DWORD reason,void *reserved){(void)reserved;if(reason==DLL_PROCESS_ATTACH){DisableThreadLibraryCalls(mod);HANDLE t=CreateThread(NULL,0,serve,NULL,0,NULL);if(t)CloseHandle(t);}return TRUE;}
