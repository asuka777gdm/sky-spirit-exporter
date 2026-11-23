#define WIN32_LEAN_AND_MEAN
#include "app.hpp"
#include "assets.hpp"
#include <d3d11.h>
#include <dxgi.h>
#include <windowsx.h>
#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"
#include <filesystem>
#include <fstream>
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND,UINT,WPARAM,LPARAM);
namespace spirit {
namespace {
HWND gameWindow=nullptr,panelWindow=nullptr;ID3D11Device* device=nullptr;ID3D11DeviceContext* context=nullptr;IDXGISwapChain* swap=nullptr;ID3D11RenderTargetView* target=nullptr;
bool shown=true,guiReady=false;unsigned pendingWidth=0,pendingHeight=0;
BOOL CALLBACK findWindow(HWND window,LPARAM){DWORD pid=0;GetWindowThreadProcessId(window,&pid);RECT r{};GetClientRect(window,&r);if(pid==GetCurrentProcessId()&&IsWindowVisible(window)&&GetWindow(window,GW_OWNER)==nullptr&&r.right>=640&&r.bottom>=360){gameWindow=window;return FALSE;}return TRUE;}
void renderTarget(){ID3D11Texture2D* buffer=nullptr;if(SUCCEEDED(swap->GetBuffer(0,IID_PPV_ARGS(&buffer)))){device->CreateRenderTargetView(buffer,nullptr,&target);buffer->Release();}}
LRESULT CALLBACK windowProc(HWND window,UINT message,WPARAM wParam,LPARAM lParam){
 if(guiReady&&ImGui_ImplWin32_WndProcHandler(window,message,wParam,lParam))return 1;
 if(message==WM_HOTKEY){shown=!shown;ShowWindow(window,shown?SW_SHOWNOACTIVATE:SW_HIDE);if(!shown&&gameWindow)SetForegroundWindow(gameWindow);return 0;}
 if(message==WM_CLOSE){shown=false;ShowWindow(window,SW_HIDE);if(gameWindow)SetForegroundWindow(gameWindow);return 0;}
 if(message==WM_SIZE&&wParam!=SIZE_MINIMIZED){pendingWidth=LOWORD(lParam);pendingHeight=HIWORD(lParam);return 0;}
 if(message==WM_NCHITTEST){LRESULT hit=DefWindowProcW(window,message,wParam,lParam);POINT p{GET_X_LPARAM(lParam),GET_Y_LPARAM(lParam)};ScreenToClient(window,&p);if(p.y>=0&&p.y<28)return HTCAPTION;return hit;}
 if(message==WM_SYSCOMMAND&&(wParam&0xfff0)==SC_KEYMENU)return 0;
 if(message==WM_DESTROY){PostQuitMessage(0);return 0;}return DefWindowProcW(window,message,wParam,lParam);
}
void screenshot(){
 ID3D11Texture2D* source=nullptr;if(FAILED(swap->GetBuffer(0,IID_PPV_ARGS(&source))))return;
 D3D11_TEXTURE2D_DESC d{};source->GetDesc(&d);d.Usage=D3D11_USAGE_STAGING;d.BindFlags=0;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;d.MiscFlags=0;
 ID3D11Texture2D* staging=nullptr;if(FAILED(device->CreateTexture2D(&d,nullptr,&staging))){source->Release();return;}context->CopyResource(staging,source);source->Release();
 D3D11_MAPPED_SUBRESOURCE map{};if(SUCCEEDED(context->Map(staging,0,D3D11_MAP_READ,0,&map))){
  BITMAPFILEHEADER file{};BITMAPINFOHEADER info{};file.bfType=0x4d42;file.bfOffBits=sizeof(file)+sizeof(info);file.bfSize=file.bfOffBits+d.Width*d.Height*4;info.biSize=sizeof(info);info.biWidth=d.Width;info.biHeight=-LONG(d.Height);info.biPlanes=1;info.biBitCount=32;info.biCompression=BI_RGB;
  std::ofstream out(std::filesystem::path(app.cacheDirectory+L"\\panel-validation.bmp"),std::ios::binary);out.write(reinterpret_cast<char*>(&file),sizeof(file));out.write(reinterpret_cast<char*>(&info),sizeof(info));
  for(UINT y=0;y<d.Height;y++){auto* pixel=static_cast<BYTE*>(map.pData)+y*map.RowPitch;std::vector<BYTE> row(pixel,pixel+d.Width*4);for(UINT x=0;x<d.Width;x++)std::swap(row[x*4],row[x*4+2]);out.write(reinterpret_cast<char*>(row.data()),row.size());}context->Unmap(staging,0);
 }staging->Release();
}
}
void overlay(HMODULE module){
 try{
  for(int i=0;i<1200&&!gameWindow;i++){EnumWindows(findWindow,0);if(!gameWindow)Sleep(100);}require(gameWindow!=nullptr,"Не найдено окно Sky");
  WNDCLASSEXW wc{sizeof(wc)};wc.style=CS_CLASSDC;wc.lpfnWndProc=windowProc;wc.hInstance=module;wc.hCursor=LoadCursorW(nullptr,MAKEINTRESOURCEW(32512));wc.lpszClassName=L"SkySpiritNativeOverlay";
  RegisterClassExW(&wc);RECT game{};GetWindowRect(gameWindow,&game);RECT client{};GetClientRect(gameWindow,&client);
  int width=std::min(1040,std::max(640,int(client.right)-50)),height=std::min(700,std::max(420,int(client.bottom)-70));
  panelWindow=CreateWindowExW(WS_EX_TOOLWINDOW,wc.lpszClassName,L"Дерево духов — Rund",WS_POPUP,game.left+25,game.top+45,width,height,gameWindow,nullptr,module,nullptr);require(panelWindow!=nullptr,"Не удалось создать окно мода");
  DXGI_SWAP_CHAIN_DESC desc{};desc.BufferCount=2;desc.BufferDesc.Width=width;desc.BufferDesc.Height=height;desc.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;desc.OutputWindow=panelWindow;desc.SampleDesc.Count=1;desc.Windowed=TRUE;desc.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
  D3D_FEATURE_LEVEL level;HRESULT hr=D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&desc,&swap,&device,&level,&context);
  if(FAILED(hr))hr=D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&desc,&swap,&device,&level,&context);
  require(SUCCEEDED(hr),"Не удалось запустить D3D11 интерфейс мода");renderTarget();require(target!=nullptr,"Нет поверхности интерфейса");
  setIconDevice(device);IMGUI_CHECKVERSION();ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard;
  wchar_t fonts[MAX_PATH];GetWindowsDirectoryW(fonts,MAX_PATH);std::filesystem::path font=std::filesystem::path(fonts)/L"Fonts"/L"segoeui.ttf";
  if(!std::filesystem::exists(font))font=std::filesystem::path(fonts)/L"Fonts"/L"arial.ttf";
  if(!io.Fonts->AddFontFromFileTTF(font.string().c_str(),17,nullptr,io.Fonts->GetGlyphRangesCyrillic()))io.Fonts->AddFontDefault();
  ImGui::StyleColorsDark();auto& style=ImGui::GetStyle();style.WindowRounding=9;style.ChildRounding=7;style.FrameRounding=6;style.WindowPadding={14,12};style.ItemSpacing={9,8};
  style.Colors[ImGuiCol_WindowBg]={.045f,.065f,.095f,.98f};style.Colors[ImGuiCol_ChildBg]={.055f,.085f,.115f,1};style.Colors[ImGuiCol_Button]={.10f,.29f,.29f,1};style.Colors[ImGuiCol_ButtonHovered]={.15f,.43f,.39f,1};style.Colors[ImGuiCol_Header]={.13f,.32f,.34f,1};style.Colors[ImGuiCol_FrameBg]={.10f,.15f,.20f,1};
  ImGui_ImplWin32_Init(panelWindow);ImGui_ImplDX11_Init(device,context);guiReady=true;RegisterHotKey(panelWindow,1,MOD_NOREPEAT,VK_F8);ShowWindow(panelWindow,SW_SHOWNOACTIVATE);log("overlay_ready");
  MSG msg{};bool running=true,captured=false;unsigned frames=0;
  while(running&&IsWindow(gameWindow)){
   while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){if(msg.message==WM_QUIT){running=false;break;}TranslateMessage(&msg);DispatchMessageW(&msg);}if(!running)break;
   HWND foreground=GetForegroundWindow();bool active=foreground==gameWindow||foreground==panelWindow;
   if(!shown||!active||IsIconic(gameWindow)){ShowWindow(panelWindow,SW_HIDE);Sleep(30);continue;}if(!IsWindowVisible(panelWindow))ShowWindow(panelWindow,SW_SHOWNOACTIVATE);
   if(pendingWidth&&pendingHeight){if(target){target->Release();target=nullptr;}swap->ResizeBuffers(0,pendingWidth,pendingHeight,DXGI_FORMAT_UNKNOWN,0);pendingWidth=pendingHeight=0;renderTarget();}
   beginIconFrame();ImGui_ImplDX11_NewFrame();ImGui_ImplWin32_NewFrame();ImGui::NewFrame();drawPanel();ImGui::Render();const float clear[]={.045f,.065f,.095f,1};context->OMSetRenderTargets(1,&target,nullptr);context->ClearRenderTargetView(target,clear);ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
#ifdef SPIRIT_VALIDATION
   if(app.liveCatalog&&!app.busy){if(++frames>90&&!captured){screenshot();captured=true;log("validation_ui_captured");}}else frames=0;
#endif
   swap->Present(1,0);Sleep(8);
  }
  guiReady=false;UnregisterHotKey(panelWindow,1);releaseIcons();setIconDevice(nullptr);ImGui_ImplDX11_Shutdown();ImGui_ImplWin32_Shutdown();ImGui::DestroyContext();if(target)target->Release();if(swap)swap->Release();if(context)context->Release();if(device)device->Release();DestroyWindow(panelWindow);
 }catch(const std::exception& error){log("overlay_failed");MessageBoxA(nullptr,error.what(),"Sky Spirit",MB_OK|MB_ICONERROR);}
}
}
