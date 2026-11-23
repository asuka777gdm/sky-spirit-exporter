#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
mkdir -p build/native dist/native
native_toolchain="${SPIRIT_TOOLCHAIN:-$PWD/build/toolchain}"
cc="$native_toolchain/bin/x86_64-w64-mingw32-clang"
cxx="$native_toolchain/bin/x86_64-w64-mingw32-clang++"
common=(-O2 -D_WIN32_WINNT=0x0602 -D_WIN64 -DNOMINMAX -DWIN32_LEAN_AND_MEAN -Ivendor/minhook/include -Ivendor/minhook/src -Ivendor/nlohmann -Ivendor/imgui -Ivendor/imgui/backends -I. -Inative)
if [[ "${SPIRIT_VALIDATION:-0}" == 1 ]]; then common+=(-DSPIRIT_VALIDATION); fi
objects=()
for source in vendor/minhook/src/{buffer.c,hook.c,trampoline.c,hde/hde64.c}; do
 obj="build/native/$(basename "$source").o"; if [[ "${SPIRIT_FAST:-0}" != 1 || ! -f "$obj" || "$source" -nt "$obj" || native/app.hpp -nt "$obj" || native/catalog.hpp -nt "$obj" ]]; then "$cc" "${common[@]}" -c "$source" -o "$obj"; fi; objects+=("$obj")
done
sources=(native/core.cpp native/assets.cpp native/panel.cpp native/overlay.cpp vendor/imgui/imgui.cpp vendor/imgui/imgui_draw.cpp vendor/imgui/imgui_tables.cpp vendor/imgui/imgui_widgets.cpp vendor/imgui/backends/imgui_impl_dx11.cpp vendor/imgui/backends/imgui_impl_win32.cpp)
for source in "${sources[@]}"; do
 obj="build/native/$(basename "$source").o"; if [[ "${SPIRIT_FAST:-0}" != 1 || ! -f "$obj" || "$source" -nt "$obj" || native/app.hpp -nt "$obj" || native/catalog.hpp -nt "$obj" ]]; then "$cxx" "${common[@]}" -std=c++17 -c "$source" -o "$obj"; fi; objects+=("$obj")
done
"$cxx" -shared -static "${objects[@]}" -o dist/native/SkySpiritBridge.dll -lwinhttp -lbcrypt -lshell32 -lole32 -luuid -ld3d11 -ldxgi -ld3dcompiler -ldwmapi -luser32 -lgdi32 -limm32 -ladvapi32
"$cc" "${common[@]}" -shared -static native/loader.c -o dist/native/powrprof.dll -luser32
