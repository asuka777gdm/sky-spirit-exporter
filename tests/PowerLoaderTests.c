#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
int main(void){
 HMODULE h=LoadLibraryW(L"build\\test-powrprof.dll");if(!h)return 1;
 BOOLEAN (WINAPI *get)(void*)=(void*)GetProcAddress(h,"GetPwrCapabilities");
 unsigned char capabilities[256]={0};if(!get||!get(capabilities))return 2;
 HMODULE mod=LoadLibraryExW(L"dist\\native\\SkySpiritBridge.dll",NULL,DONT_RESOLVE_DLL_REFERENCES);
 if(!mod||!GetProcAddress(mod,"SkySpiritOpenPanel"))return 3;
 FreeLibrary(mod);FreeLibrary(h);puts("Native power loader checks passed");return 0;
}
