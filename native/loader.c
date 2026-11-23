#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string.h>
#include <wchar.h>
/* Minimal power API proxy for Sky. Rendering libraries stay untouched. */
static INIT_ONCE once=INIT_ONCE_STATIC_INIT;static HMODULE systemPower,self;
static BOOL CALLBACK realPower(PINIT_ONCE o,void *p,void **ctx){
 (void)o;(void)p;(void)ctx;wchar_t path[MAX_PATH];
 UINT n=GetSystemDirectoryW(path,MAX_PATH);
 if(n&&n+14<MAX_PATH){wcscat(path,L"\\powrprof.dll");systemPower=LoadLibraryExW(path,NULL,LOAD_LIBRARY_SEARCH_SYSTEM32);}
 return TRUE;
}
static FARPROC symbol(const char *name){InitOnceExecuteOnce(&once,realPower,NULL,NULL);return systemPower?GetProcAddress(systemPower,name):NULL;}
__declspec(dllexport) BOOLEAN WINAPI GetPwrCapabilities(void *out){
 BOOLEAN (WINAPI *fn)(void*)=(void*)symbol("GetPwrCapabilities");return fn?fn(out):FALSE;
}
__declspec(dllexport) DWORD WINAPI PowerGetActiveScheme(HKEY key,GUID **scheme){
 DWORD (WINAPI *fn)(HKEY,GUID**)=(void*)symbol("PowerGetActiveScheme");return fn?fn(key,scheme):ERROR_NOT_SUPPORTED;
}
__declspec(dllexport) DWORD WINAPI PowerSetActiveScheme(HKEY key,const GUID *scheme){
 DWORD (WINAPI *fn)(HKEY,const GUID*)=(void*)symbol("PowerSetActiveScheme");return fn?fn(key,scheme):ERROR_NOT_SUPPORTED;
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
 wchar_t exe[MAX_PATH];DWORD n=GetModuleFileNameW(NULL,exe,MAX_PATH);wchar_t *name=wcsrchr(exe,L'\\');
 if(n&&n<MAX_PATH&&name&&!_wcsicmp(name+1,L"Sky.exe")){HANDLE t=CreateThread(NULL,0,startMod,NULL,0,NULL);if(t)CloseHandle(t);}
#endif
 }return TRUE;
}
