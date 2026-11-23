$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot
New-Item -ItemType Directory -Force build/native,dist/native | Out-Null
$common = @('/nologo','/O2','/MT','/utf-8','/D_WIN64','/DNOMINMAX','/D_WIN32_WINNT=0x0602','/I.','/Inative','/Ivendor/nlohmann','/Ivendor/imgui','/Ivendor/imgui/backends','/Ivendor/minhook/include','/Ivendor/minhook/src')
$c = @('vendor/minhook/src/buffer.c','vendor/minhook/src/hook.c','vendor/minhook/src/trampoline.c','vendor/minhook/src/hde/hde64.c')
$objects = @()
foreach ($source in $c) {
 $obj = 'build/native/' + [IO.Path]::GetFileNameWithoutExtension($source) + '.obj'
 & cl.exe @common /c $source "/Fo$obj"
 if ($LASTEXITCODE -ne 0) { throw 'C compilation failed' }
 $objects += $obj
}
$cpp = @('native/core.cpp','native/assets.cpp','native/panel.cpp','native/overlay.cpp','vendor/imgui/imgui.cpp','vendor/imgui/imgui_draw.cpp','vendor/imgui/imgui_tables.cpp','vendor/imgui/imgui_widgets.cpp','vendor/imgui/backends/imgui_impl_dx11.cpp','vendor/imgui/backends/imgui_impl_win32.cpp')
foreach ($source in $cpp) {
 $obj = 'build/native/' + [IO.Path]::GetFileNameWithoutExtension($source) + '.obj'
 & cl.exe @common /EHsc /std:c++17 /c $source "/Fo$obj"
 if ($LASTEXITCODE -ne 0) { throw 'C++ compilation failed' }
 $objects += $obj
}
& link.exe /nologo /DLL /OUT:dist/native/SkySpiritBridge.dll $objects winhttp.lib bcrypt.lib shell32.lib ole32.lib uuid.lib d3d11.lib dxgi.lib d3dcompiler.lib dwmapi.lib user32.lib gdi32.lib imm32.lib advapi32.lib
if ($LASTEXITCODE -ne 0) { throw 'Native mod linking failed' }
& cl.exe @common /LD native/loader.c /link /OUT:dist/native/powrprof.dll user32.lib
if ($LASTEXITCODE -ne 0) { throw 'Loader build failed' }
Get-Item dist/native/SkySpiritBridge.dll,dist/native/powrprof.dll | Out-Null
