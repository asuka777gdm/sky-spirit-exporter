$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot
New-Item -ItemType Directory -Force build,dist | Out-Null
# Run from the x64 Native Tools Command Prompt for Visual Studio 2022.
$sources = @('Bridge.c','vendor/minhook/src/buffer.c','vendor/minhook/src/hook.c','vendor/minhook/src/trampoline.c','vendor/minhook/src/hde/hde64.c')
& cl.exe /nologo /O2 /LD /MT /D_WIN64 /D_WIN32_WINNT=0x0601 /Ivendor/minhook/include /Ivendor/minhook/src $sources /link /OUT:dist/SkySpiritBridge.dll bcrypt.lib advapi32.lib
if ($LASTEXITCODE -ne 0) { throw 'DLL build failed' }
$csc = "$env:WINDIR\Microsoft.NET\Framework64\v4.0.30319\csc.exe"
& $csc /nologo /target:exe /platform:x64 /out:dist/SkySpiritExporter.exe /reference:System.Web.Extensions.dll /reference:System.Windows.Forms.dll /reference:System.Drawing.dll Launcher.cs Catalog.cs
if ($LASTEXITCODE -ne 0) { throw 'Launcher build failed' }
