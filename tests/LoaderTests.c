#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
int main(void){
 HMODULE m=LoadLibraryW(L"build\\test-dxgi.dll");if(!m){puts("Cannot load DXGI proxy");return 1;}
 const char *names[]={"CreateDXGIFactory","CreateDXGIFactory1","CreateDXGIFactory2","DXGIGetDebugInterface1","DXGIDeclareAdapterRemovalSupport"};
 for(int i=0;i<5;i++)if(!GetProcAddress(m,names[i])){puts("Missing forwarding export");return 2;}
 /* IUnknown: factory creation without a graphics device or a running game. */
 const GUID iid={0,0,0,{0xc0,0,0,0,0,0,0,0x46}};void *factory=NULL;
 HRESULT (WINAPI *create)(const GUID*,void**)=(void*)GetProcAddress(m,"CreateDXGIFactory1");
 HRESULT hr=create(&iid,&factory);if(FAILED(hr)||!factory){printf("DXGI forwarding failed: %lx\n",(unsigned long)hr);return 3;}
 void **vtable=*(void***)factory;ULONG (WINAPI *release)(void*)=(void*)vtable[2];release(factory);
 HMODULE mod=LoadLibraryExW(L"dist\\SkySpiritBridge.dll",NULL,LOAD_LIBRARY_AS_DATAFILE);
 HRSRC resource=mod?FindResourceW(mod,MAKEINTRESOURCEW(101),MAKEINTRESOURCEW(10)):NULL;
 if(!resource||SizeofResource(mod,resource)<1024){puts("Embedded panel missing");return 4;}
 const BYTE *bytes=LockResource(LoadResource(mod,resource));if(!bytes||bytes[0]!='M'||bytes[1]!='Z'){puts("Invalid panel resource");return 5;}
 FreeLibrary(mod);FreeLibrary(m);puts("DXGI forwarding and embedded panel checks passed");return 0;
}
