# Third-party components

- Dear ImGui, version 1.93.0 WIP, MIT. Vendored core, Win32 and DX11 backends
  from HerokeyVN/SkyLoader commit 80c0807d9012095ca03ab85da1df31669697caef,
  libraries/SkyOverlay/third_party/imgui. Only ImGui sources are reused;
  the catalog interface and native overlay host are implemented in this project.
  Upstream: https://github.com/ocornut/imgui.
- nlohmann/json, single header from the existing local Rund dependency;
  version is recorded in vendor/nlohmann/json.hpp, MIT license adjacent.
  Upstream: https://github.com/nlohmann/json.
- MinHook 1.3.4, upstream https://github.com/TsudaKageyu/minhook;
  bundled license: vendor/minhook/LICENSE.txt.

No proprietary mod implementation or game assets are included.
