#define WIN32_LEAN_AND_MEAN
#include "app.hpp"
#include "assets.hpp"
#include "MinHook.h"
#include <bcrypt.h>
#include <shlobj.h>
#include <winhttp.h>
#include <fstream>
#include <filesystem>
#include <thread>
#include <cctype>
namespace spirit {
App app;
namespace {
struct Login {std::string user,session,agent;unsigned long long generation=0;};
std::mutex loginLock;Login login;unsigned long long contextCounter=0;
Json history=nullptr;
void (*originalUpdate)(void*);
bool readMem(const void* p,void* out,size_t size){SIZE_T n=0;return ReadProcessMemory(GetCurrentProcess(),p,out,size,&n)&&n==size;}
template<class T>bool readValue(const BYTE* p,T& value){return readMem(p,&value,sizeof(value));}
std::string hashFile(const std::wstring& path){
 BCRYPT_ALG_HANDLE a=nullptr;BCRYPT_HASH_HANDLE h=nullptr;BYTE digest[32];std::string result;
 HANDLE f=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);if(f==INVALID_HANDLE_VALUE)return {};
 if(BCryptOpenAlgorithmProvider(&a,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0&&BCryptCreateHash(a,&h,nullptr,0,nullptr,0,0)>=0){
  std::vector<BYTE> buffer(65536);DWORD n=0;bool ok=true;while(ok){if(!ReadFile(f,buffer.data(),DWORD(buffer.size()),&n,nullptr)){ok=false;break;}if(!n)break;ok=BCryptHashData(h,buffer.data(),n,0)>=0;}
  if(ok&&BCryptFinishHash(h,digest,32,0)>=0){for(auto b:digest){result+="0123456789abcdef"[b>>4];result+="0123456789abcdef"[b&15];}}
 }
 if(h)BCryptDestroyHash(h);if(a)BCryptCloseAlgorithmProvider(a,0);CloseHandle(f);return result;
}
std::wstring folder(const GUID& id){wchar_t* p=nullptr;require(SUCCEEDED(SHGetKnownFolderPath(id,0,nullptr,&p)),"Не найдена папка Windows");std::wstring result=p;CoTaskMemFree(p);return result;}
void observeGame(BYTE* game){
 BYTE *client=nullptr,*barn=nullptr,*http=nullptr;BYTE user[16]{},session[16]{},secondUser[16]{},secondSession[16]{};DWORD state=0;char agent[128]{};
 bool valid=readValue(game+0x1c0,client)&&client&&readValue(game+0x1d8,barn)&&barn&&readValue(barn+0x14b4,state)&&state==0x1c&&
  readMem(client+0x2c0,user,16)&&readMem(client+0x2d0,session,16)&&readMem(client+0x2c0,secondUser,16)&&readMem(client+0x2d0,secondSession,16)&&!memcmp(user,secondUser,16)&&!memcmp(session,secondSession,16);
 bool userNonzero=false,sessionNonzero=false;for(int i=0;i<16;i++){userNonzero|=user[i]!=0;sessionNonzero|=session[i]!=0;}valid=valid&&userNonzero&&sessionNonzero;
 valid=valid&&readValue(client+0x10,http)&&http&&readMem(http+0x14,agent,127)&&!strncmp(agent,"Sky-",4)&&!strchr(agent,'\r')&&!strchr(agent,'\n');
 std::string u,s;
 if(valid)for(int i=0;i<16;i++){
  if(i==4||i==6||i==8||i==10)u+='-';u+="0123456789abcdef"[user[i]>>4];u+="0123456789abcdef"[user[i]&15];s+="0123456789abcdef"[session[i]>>4];s+="0123456789abcdef"[session[i]&15];
 }
 {std::lock_guard<std::mutex> guard(loginLock);if(login.user!=u||login.session!=s||login.agent!=agent){login.user=std::move(u);login.session=std::move(s);login.agent=agent;login.generation=++contextCounter;}app.ready=valid;}
 SecureZeroMemory(user,sizeof(user));SecureZeroMemory(session,sizeof(session));SecureZeroMemory(secondUser,sizeof(secondUser));SecureZeroMemory(secondSession,sizeof(secondSession));
}
void gameUpdate(void* game){originalUpdate(game);try{observeGame(static_cast<BYTE*>(game));}catch(...){} }
bool current(const Login& value){std::lock_guard<std::mutex> guard(loginLock);return app.ready&&value.generation&&value.generation==login.generation&&value.user==login.user&&value.session==login.session;}
struct Http {HINTERNET handle=nullptr;explicit Http(HINTERNET h):handle(h){require(h!=nullptr,"Не удалось создать HTTPS-запрос");}~Http(){if(handle)WinHttpCloseHandle(handle);}operator HINTERNET()const{return handle;}};
Json post(const Login& context,const Json& page){
 std::wstring agent(context.agent.begin(),context.agent.end());
 Http internet(WinHttpOpen(agent.c_str(),WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0));
 WinHttpSetTimeouts(internet,10000,10000,20000,20000);
 Http connection(WinHttpConnect(internet,L"live.radiance.thatgamecompany.com",INTERNET_DEFAULT_HTTPS_PORT,0));
 Http request(WinHttpOpenRequest(connection,L"POST",L"/account/get_spirit_shops",nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE));
 DWORD redirects=WINHTTP_OPTION_REDIRECT_POLICY_NEVER;WinHttpSetOption(request,WINHTTP_OPTION_REDIRECT_POLICY,&redirects,sizeof(redirects));
 Json payload=page;payload["user"]=context.user;payload["user_id"]=context.user;payload["session"]=context.session;std::string body=payload.dump();
 auto widen=[](const std::string& s){return std::wstring(s.begin(),s.end());};
 std::wstring headers=L"Content-Type: application/json\r\nuser-id: "+widen(context.user)+L"\r\nx-user-id: "+widen(context.user)+L"\r\nsession: "+widen(context.session)+L"\r\nx-session-token: "+widen(context.session)+L"\r\n";
 BOOL sent=WinHttpSendRequest(request,headers.c_str(),DWORD(headers.size()),body.data(),DWORD(body.size()),DWORD(body.size()),0);
 SecureZeroMemory(body.data(),body.size());SecureZeroMemory(headers.data(),headers.size()*sizeof(wchar_t));
 require(sent&&WinHttpReceiveResponse(request,nullptr),"Ошибка HTTPS. Проверьте подключение и вход в игру");
 DWORD status=0,size=sizeof(status);require(WinHttpQueryHeaders(request,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&status,&size,WINHTTP_NO_HEADER_INDEX),"Нет HTTP-статуса");
 log("catalog_http="+std::to_string(status));
 if(status!=200)throw std::runtime_error("Сервер вернул HTTP "+std::to_string(status)+(status==401?". Сессия отклонена":". Запрос клиента не принят"));
 std::string response;std::vector<char> buffer(8192);
 for(;;){DWORD n=0;require(WinHttpReadData(request,buffer.data(),DWORD(buffer.size()),&n),"Ответ сервера прерван");if(!n)break;require(response.size()+n<=16*1024*1024,"Ответ слишком большой");response.append(buffer.data(),n);}
 try{return Json::parse(response);}catch(...){throw std::runtime_error("Сервер вернул неверный JSON");}
}
void status(const std::string& message){std::lock_guard<std::mutex> guard(app.lock);app.status=message;}
void refresh(){
 Login context;{std::lock_guard<std::mutex> guard(loginLock);context=login;}require(app.ready&&context.user.size()==36&&context.session.size()==32,"Дождитесь входа в Sky");
 auto fresh=readCatalog(app.build,[&]{return current(context);},[&](const Json& page){return post(context,page);},[](size_t n,size_t total){status("Загрузка дерева: "+std::to_string(n)+" / "+std::to_string(total));});
 require(current(context),"Сессия изменилась до сохранения");enrichCatalog(fresh);auto next=advanceHistory(history,fresh);saveJson(app.cacheDirectory+L"\\history.json",next,false);saveJson(app.cacheDirectory+L"\\catalog.json",fresh,false);history=std::move(next);
 {std::lock_guard<std::mutex> guard(app.lock);app.catalog=std::move(fresh);app.baseline=history.at("baseline");app.status="Каталог получен из текущей сессии Sky";app.liveCatalog=true;app.revision++;}log("catalog_complete");
#ifdef SPIRIT_VALIDATION
 app.exportRequested=true;
#endif
}
bool bind(){
 wchar_t path[32768];DWORD n=GetModuleFileNameW(nullptr,path,32768);if(!n||n>=32768)return false;
 auto hash=hashFile(path);log("sky_sha256="+hash);
 if(hash!="bdd780f88a7d34f7c3a9b336e82dc57393c3a8a7f0e639f28c41e709b9134b19")return false;
 app.build=426499;
 const BYTE prefix[]={0x55,0x56,0x57,0x53,0x48,0x81,0xec,0xf8,0x09,0,0,0x48,0x8d,0xac,0x24,0x80};BYTE found[sizeof(prefix)];auto target=reinterpret_cast<BYTE*>(GetModuleHandleW(nullptr))+0x116fe00;
 return readMem(target,found,sizeof(found))&&!memcmp(found,prefix,sizeof(prefix))&&MH_Initialize()==MH_OK&&MH_CreateHook(target,reinterpret_cast<void*>(&gameUpdate),reinterpret_cast<void**>(&originalUpdate))==MH_OK&&MH_EnableHook(target)==MH_OK;
}
}
void log(const std::string& text){if(app.cacheDirectory.empty())return;try{std::ofstream out(std::filesystem::path(app.cacheDirectory+L"\\diagnostic.log"),std::ios::app);out<<GetTickCount64()<<" "<<text<<"\n";}catch(...){} }
void saveJson(const std::wstring& path,const Json& value,bool backup){
 std::filesystem::create_directories(std::filesystem::path(path).parent_path());auto text=value.dump(2)+"\n";auto temp=path+L".tmp-"+std::to_wstring(GetCurrentProcessId());
 HANDLE f=CreateFileW(temp.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);require(f!=INVALID_HANDLE_VALUE,"Не удалось создать JSON");DWORD written=0;bool ok=WriteFile(f,text.data(),DWORD(text.size()),&written,nullptr)&&written==text.size()&&FlushFileBuffers(f);CloseHandle(f);
 if(ok&&backup&&GetFileAttributesW(path.c_str())!=INVALID_FILE_ATTRIBUTES)ok=ReplaceFileW(path.c_str(),temp.c_str(),(path+L".previous").c_str(),REPLACEFILE_IGNORE_MERGE_ERRORS,nullptr,nullptr)!=FALSE;
 else if(ok)ok=MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=FALSE;
 if(!ok){DeleteFileW(temp.c_str());throw std::runtime_error("Не удалось сохранить JSON; прежний файл оставлен");}
}
void worker(HMODULE module){
 try{
  const GUID downloads={0x374de290,0x123f,0x4565,{0x91,0x64,0x39,0xc4,0x92,0x5e,0x46,0x7b}},local={0xf1b32785,0x6fba,0x4fcf,{0x9d,0x55,0x7b,0x8e,0x7f,0x15,0x70,0x91}};
  app.downloadDirectory=folder(downloads)+L"\\sky-spirit";app.cacheDirectory=folder(local)+L"\\SkySpiritExport\\native";std::filesystem::create_directories(app.cacheDirectory);
  loadGameAssets();
  try{
   auto readStored=[&](const wchar_t* filename){std::ifstream f(std::filesystem::path(app.cacheDirectory+L"\\"+filename),std::ios::binary|std::ios::ate);if(!f)return Json();auto n=f.tellg();require(n>=0&&n<=32*1024*1024,"Снимок слишком большой");f.seekg(0);return Json::parse(f);};
   auto stored=readStored(L"history.json");if(stored.is_object()&&stored.value("schema",0)==1){auto c=sanitizedCatalog(stored.at("current"));Json b=stored.value("baseline",Json());if(!b.is_null())b=sanitizedCatalog(b);history={{"schema",1},{"current",c},{"baseline",b}};}
   else {stored=readStored(L"catalog.json");if(stored.is_object())history=advanceHistory(nullptr,sanitizedCatalog(stored));}
   if(history.is_object()){enrichCatalog(history["current"]);if(!history["baseline"].is_null())enrichCatalog(history["baseline"]);app.catalog=history["current"];app.baseline=history["baseline"];app.revision++;}
  }catch(...){log("stored_catalog_rejected");}
  log("native_start_no_ipc pid="+std::to_string(GetCurrentProcessId()));app.supported=bind();
  if(!app.supported)status("Эта сборка Sky не поддерживается. Обновите DLL мода");else{status("Войдите в Sky. После входа дерево загрузится автоматически");log("hook_installed");}
  std::thread([module]{overlay(module);}).detach();
  unsigned long long attempted=0;bool authLogged=false;
  for(;;){
   Login context;{std::lock_guard<std::mutex> guard(loginLock);context=login;}
   if(app.ready&&context.generation&&!authLogged){log("session_observed");authLogged=true;}
   bool automatic=app.ready&&context.generation&&context.generation!=attempted;
   if(app.readRequested.exchange(false)||automatic){attempted=context.generation;app.busy=true;try{refresh();}catch(const std::exception& e){status(e.what());log("catalog_failed");}catch(...){status("Не удалось получить каталог");}app.busy=false;}
   if(app.exportRequested.exchange(false)){app.busy=true;try{Json snapshot;{std::lock_guard<std::mutex> guard(app.lock);snapshot=app.catalog;}require(!snapshot.is_null(),"Сначала загрузите каталог");saveJson(app.downloadDirectory+L"\\spirit-catalog-export.json",snapshot,true);status("JSON сохранён в Загрузки\\sky-spirit");log("export_complete");}catch(const std::exception& e){status(e.what());}app.busy=false;}
   if(app.baselineRequested.exchange(false)&&history.is_object()){app.busy=true;try{auto next=history;next["baseline"]=next["current"];saveJson(app.cacheDirectory+L"\\history.json",next,false);history=std::move(next);{std::lock_guard<std::mutex> guard(app.lock);app.baseline=app.catalog;app.status="Текущий каталог принят за базу сравнения";app.revision++;}}catch(...){status("Не удалось сохранить базу сравнения");}app.busy=false;}
   Sleep(100);
  }
 }catch(...){MessageBoxW(nullptr,L"Не удалось запустить мод дерева духов. Проверьте доступ к папкам пользователя.",L"Дерево духов",MB_OK|MB_ICONERROR);}
}
}
static DWORD WINAPI launch(void* module){spirit::worker(static_cast<HMODULE>(module));return 0;}
extern "C" __declspec(dllexport) void WINAPI SkySpiritOpenPanel(){} // Loader ABI; the native overlay starts once from DllMain.
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,void*){if(reason==DLL_PROCESS_ATTACH){DisableThreadLibraryCalls(module);HANDLE thread=CreateThread(nullptr,0,launch,module,0,nullptr);if(thread)CloseHandle(thread);}return TRUE;}
