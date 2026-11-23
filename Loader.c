#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string.h>
#include <wchar.h>
/* Minimal DXGI proxy for Sky. Only our fixed mod path is loaded. */
static INIT_ONCE once=INIT_ONCE_STATIC_INIT;static HMODULE systemDxgi,self;
static BOOL CALLBACK realDxgi(PINIT_ONCE o,void *p,void **ctx){
 (void)o;(void)p;(void)ctx;wchar_t path[MAX_PATH];
 UINT n=GetSystemDirectoryW(path,MAX_PATH);
 if(n&&n+10<MAX_PATH){wcscat(path,L"\\dxgi.dll");systemDxgi=LoadLibraryExW(path,NULL,LOAD_LIBRARY_SEARCH_SYSTEM32);}
 return TRUE;
}
static FARPROC symbol(const char *name){InitOnceExecuteOnce(&once,realDxgi,NULL,NULL);return systemDxgi?GetProcAddress(systemDxgi,name):NULL;}
__declspec(dllexport) HRESULT WINAPI CreateDXGIFactory(const GUID *id,void **out){
 HRESULT (WINAPI *fn)(const GUID*,void**)=(void*)symbol("CreateDXGIFactory");if(!fn){if(out)*out=NULL;return E_FAIL;}return fn(id,out);
}
__declspec(dllexport) HRESULT WINAPI CreateDXGIFactory1(const GUID *id,void **out){
 HRESULT (WINAPI *fn)(const GUID*,void**)=(void*)symbol("CreateDXGIFactory1");if(!fn){if(out)*out=NULL;return E_FAIL;}return fn(id,out);
}
__declspec(dllexport) HRESULT WINAPI CreateDXGIFactory2(UINT flags,const GUID *id,void **out){
 HRESULT (WINAPI *fn)(UINT,const GUID*,void**)=(void*)symbol("CreateDXGIFactory2");if(!fn){if(out)*out=NULL;return E_FAIL;}return fn(flags,id,out);
}
__declspec(dllexport) HRESULT WINAPI DXGIGetDebugInterface1(UINT flags,const GUID *id,void **out){
 HRESULT (WINAPI *fn)(UINT,const GUID*,void**)=(void*)symbol("DXGIGetDebugInterface1");if(!fn){if(out)*out=NULL;return E_NOINTERFACE;}return fn(flags,id,out);
}
__declspec(dllexport) HRESULT WINAPI DXGIDeclareAdapterRemovalSupport(void){
 HRESULT (WINAPI *fn)(void)=(void*)symbol("DXGIDeclareAdapterRemovalSupport");return fn?fn():E_NOTIMPL;
}
static DWORD WINAPI startMod(void *unused){
 (void)unused;wchar_t path[MAX_PATH];DWORD n=GetModuleFileNameW(self,path,MAX_PATH);
 if(!n||n>=MAX_PATH)return 0;
 wchar_t *slash=wcsrchr(path,L'\\');if(!slash)return 0;*slash=0;
 if(wcslen(path)+27>=MAX_PATH)return 0;wcscat(path,L"\\mods\\SkySpiritBridge.dll");
 HMODULE mod=LoadLibraryExW(path,NULL,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
 if(!mod){MessageBoxW(NULL,L"Не удалось загрузить mods\\SkySpiritBridge.dll. Проверьте, что архив распакован целиком рядом с Sky.exe.",L"Дерево духов — загрузчик",MB_OK|MB_ICONERROR);return 0;}
 void (WINAPI *open)(void)=(void*)GetProcAddress(mod,"SkySpiritOpenPanel");
 if(open)open();else MessageBoxW(NULL,L"Установлена старая DLL мода. Замените mods\\SkySpiritBridge.dll файлом из полного комплекта.",L"Дерево духов",MB_OK|MB_ICONERROR);
 return 0;
}
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,void *reserved){
 (void)reserved;if(reason==DLL_PROCESS_ATTACH){self=module;DisableThreadLibraryCalls(module);
#ifndef LOADER_FORWARD_TEST
 HANDLE t=CreateThread(NULL,0,startMod,NULL,0,NULL);if(t)CloseHandle(t);
#endif
 }return TRUE;
}
