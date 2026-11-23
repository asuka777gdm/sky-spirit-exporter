#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
mkdir -p build dist
sources=(Bridge.c vendor/minhook/src/buffer.c vendor/minhook/src/hook.c vendor/minhook/src/trampoline.c vendor/minhook/src/hde/hde64.c)
objects=()
for source in "${sources[@]}"; do
 object="build/$(basename "$source" .c).obj"
 clang --target=x86_64-pc-windows-msvc -fms-extensions -ffreestanding -fno-stack-protector -O2 \
  -D_WIN64 -D_WIN32_WINNT=0x0601 -D__movsb=memcpy -isystem /usr/include/wine/windows -isystem /usr/include/wine/msvcrt \
  -Ivendor/minhook/include -Ivendor/minhook/src -c "$source" -o "$object"
 objects+=("$object")
done
lld-link /dll /entry:DllMain /nodefaultlib /out:dist/SkySpiritBridge.dll "${objects[@]}" \
 /usr/lib/wine/x86_64-windows/libadvapi32.a /usr/lib/wine/x86_64-windows/libkernel32.a /usr/lib/wine/x86_64-windows/libbcrypt.a \
 /usr/lib/wine/x86_64-windows/libmsvcrt.a /usr/lib/wine/x86_64-windows/libucrtbase.a
task_win="$(winepath -w "$PWD")"
WINEDEBUG=-all wine "$HOME/.wine/drive_c/windows/mono/mono-2.0/lib/mono/4.5/mcs.exe" \
 -sdk:4.5 -target:exe -platform:x64 "-out:$task_win\\dist\\SkySpiritExporter.exe" \
 -r:System.Web.Extensions -r:System.Windows.Forms -r:System.Drawing \
 "$task_win\\Launcher.cs" "$task_win\\Catalog.cs"
