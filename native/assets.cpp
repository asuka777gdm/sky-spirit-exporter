#include "assets.hpp"
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <map>
#include <cmath>
namespace spirit {
namespace {
struct Region{std::string atlas;float uv[4]{};};
std::filesystem::path resources,images;
std::map<std::string,Region> regions;std::map<std::string,std::string> icons;
ID3D11Device* iconDevice=nullptr;
struct Texture{ID3D11ShaderResourceView* srv=nullptr;unsigned used=0;};std::map<std::string,Texture> textures;unsigned frame=0;
struct Raw{std::vector<unsigned char> bytes;unsigned w=0,h=0,format=0,type=0,base=0;size_t offset=0;unsigned used=0;};std::map<std::string,Raw> rawCache;
std::string readText(const std::filesystem::path& p,size_t limit){std::ifstream f(p,std::ios::binary|std::ios::ate);if(!f)return {};auto size=f.tellg();if(size<0||size>std::streamoff(limit))return {};std::string text(size_t(size),'\0');f.seekg(0);if(!f.read(text.data(),size))return {};return text;}
float number(std::string s){auto slash=s.find('/');float v=slash==std::string::npos?std::stof(s):std::stof(s.substr(0,slash))/std::stof(s.substr(slash+1));require(std::isfinite(v)&&v>=0&&v<=1,"Неверные координаты иконки");return v;}
std::string available(std::initializer_list<const char*> names){for(auto name:names)if(regions.count(name))return name;return "UiMiscQuestion";}
unsigned u32(const std::vector<unsigned char>& b,size_t p){require(p+4<=b.size(),"Неполный KTX");return unsigned(b[p])|(unsigned(b[p+1])<<8)|(unsigned(b[p+2])<<16)|(unsigned(b[p+3])<<24);}
Raw* atlas(const std::string& name){
 auto old=rawCache.find(name);if(old!=rawCache.end()){old->second.used=frame;return &old->second;}
 if(name.empty()||name.size()>160||name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-")!=std::string::npos)return nullptr;
 auto text=readText(images/(name+".ktx"),32*1024*1024);if(text.size()<68||text.compare(0,12,"\xabKTX 11\xbb\r\n\x1a\n",12))return nullptr;
 Raw raw;raw.bytes.assign(text.begin(),text.end());if(u32(raw.bytes,12)!=0x04030201)return nullptr;
 raw.type=u32(raw.bytes,16);raw.format=u32(raw.bytes,28);raw.base=u32(raw.bytes,32);raw.w=u32(raw.bytes,36);raw.h=u32(raw.bytes,40);raw.offset=68+u32(raw.bytes,60);raw.used=frame;
 if(!raw.w||!raw.h||raw.w>8192||raw.h>8192||raw.offset>=raw.bytes.size()||u32(raw.bytes,44)>1||u32(raw.bytes,52)!=1)return nullptr;
 size_t required=raw.format==0x8dbb?size_t((raw.w+3)/4)*((raw.h+3)/4)*8:raw.type==0x1401&&raw.base==0x1908?size_t(raw.w)*raw.h*4:0;
 if(!required||required>raw.bytes.size()-raw.offset)return nullptr;
 if(rawCache.size()>=4){auto oldest=std::min_element(rawCache.begin(),rawCache.end(),[](auto&a,auto&b){return a.second.used<b.second.used;});rawCache.erase(oldest);}
 return &rawCache.emplace(name,std::move(raw)).first->second;
}
unsigned alpha(const Raw& raw,unsigned x,unsigned y){
 if(raw.format!=0x8dbb)return raw.bytes[raw.offset+(size_t(y)*raw.w+x)*4+3];
 size_t offset=raw.offset+(size_t(y/4)*((raw.w+3)/4)+x/4)*8;auto* b=raw.bytes.data()+offset;unsigned palette[8]{b[0],b[1]};
 if(b[0]>b[1])for(unsigned i=1;i<=6;i++)palette[i+1]=((7-i)*b[0]+i*b[1])/7;
 else{for(unsigned i=1;i<=4;i++)palette[i+1]=((5-i)*b[0]+i*b[1])/5;palette[6]=0;palette[7]=255;}
 unsigned long long bits=0;for(int i=0;i<6;i++)bits|=static_cast<unsigned long long>(b[i+2])<<(8*i);return palette[(bits>>(3*((y%4)*4+x%4)))&7];
}
}
void loadGameAssets(){
 wchar_t path[32768];GetModuleFileNameW(nullptr,path,32768);auto game=std::filesystem::path(path).parent_path();resources=game/L"data/assets/initial/Data/Resources";images=game/L"data/assets/initial/Data/Images/Bin/BC";
 std::string text=readText(resources/L"UIPackedAtlas.lua",8*1024*1024);
 std::regex record(R"rx(resource\s+"ImageRegion"\s+"([^"]+)"\s*\{\s*image\s*=\s*"([^"]+)"\s*,\s*uv\s*=\s*\{([^}]+)\})rx");
 for(std::sregex_iterator it(text.begin(),text.end(),record),end;it!=end;++it){try{Region r;r.atlas=(*it)[2];std::stringstream values((*it)[3]);std::string token;for(int i=0;i<4;i++){require(bool(std::getline(values,token,',')),"Короткая область иконки");r.uv[i]=number(token);}if(r.uv[2]>r.uv[0]&&r.uv[3]>r.uv[1])regions[(*it)[1]]=r;}catch(...){} }
 for(const auto* filename:{L"OutfitDefs.json",L"CollectibleDefs.json"}){auto data=Json::parse(readText(resources/filename,32*1024*1024),nullptr,false);if(!data.is_array())continue;for(auto& row:data){if(row.contains("name")&&row.at("name").is_string()&&row.contains("icon")&&row.at("icon").is_string())icons[row.at("name").get<std::string>()]=row.at("icon").get<std::string>();}}
}
std::string itemIcon(const Json& row){
 const auto name=row.value("nm",std::string{}),type=row.value("typ",std::string{});auto direct=icons.find(name);if(direct!=icons.end())return direct->second;
 if(type=="spirit_upgrade"&&name.rfind("emote_upgrade_",0)==0){auto base=name.substr(14);auto suffix=base.find_last_of('_');if(suffix!=std::string::npos)base.resize(suffix);auto found=icons.find(base);if(found!=icons.end())return found->second;}
 if(type=="heart"||type=="season_heart")return available({"UiMenuCurrencyHeart","UiMiscHeart"});
 if(type=="tier_unlock")return available({"UiMenuWingBuff","UiMenuAscend","UiMenuLock","UiMiscStar"});
 if(type=="lootbox"){
  if(name.rfind("lootbox_color_",0)==0)return available({"UiMenuLootBoxDye","UiMenuBuffPotionShop"});
  if(name.rfind("lootbox_epic_",0)==0)return available({"UiMenuLootBoxEpic","UiMenuBuffPotionShop"});
  if(name.rfind("lootbox_rare_",0)==0)return available({"UiMenuLootBoxRare","UiMenuBuffPotionShop"});
  if(name.rfind("lootbox_legendary_",0)==0)return available({"UiMenuLootBoxLegendary","UiMenuBuffPotionShop"});
  return available({"UiMenuLootBoxCommon","UiMenuBuffPotionShop"});
 }
 if(type=="relationship_ability"&&name.find("hug")!=std::string::npos)return available({"UiEmoteSocialHug","UiEmoteHug","UiMiscHeart"});
 if(type=="quest"||type=="ap06unlock"||type=="timeline")return available({"UiMenuQuest","UiMiscCheck"});
 if(type=="music")return available({"UiMiscMusicNote"});if(type=="teleport_to")return available({"UiMenuGate"});
 return "UiMiscQuestion";
}
void enrichCatalog(Json& catalog){if(!catalog.is_object()||!catalog.contains("items"))return;for(auto& row:catalog["items"]){auto name=itemIcon(row);row["icon"]=name;auto it=regions.find(name);if(it!=regions.end()){row["icon_atlas"]=it->second.atlas;row["icon_uv"]=it->second.uv;}}}
void beginIconFrame(){++frame;if(textures.size()>192)for(auto it=textures.begin();it!=textures.end()&&textures.size()>192;){if(it->second.used+2<frame){if(it->second.srv)it->second.srv->Release();it=textures.erase(it);}else ++it;}}
void setIconDevice(ID3D11Device* device){iconDevice=device;}
ID3D11ShaderResourceView* iconTexture(ID3D11Device* device,const std::string& name){
 if(!device)device=iconDevice;if(!device)return nullptr;
 auto cached=textures.find(name);if(cached!=textures.end()){cached->second.used=frame;return cached->second.srv;}
 Region region;auto found=regions.find(name);if(found==regions.end()){region.atlas=name;region.uv[2]=region.uv[3]=1;}else region=found->second;
 Raw* raw=atlas(region.atlas);if(!raw){textures[name]={nullptr,frame};return nullptr;}std::vector<unsigned char> pixels(64*64*4);
 for(unsigned y=0;y<64;y++)for(unsigned x=0;x<64;x++){
  auto sx=std::min(raw->w-1,unsigned((region.uv[0]+(x+.5f)/64*(region.uv[2]-region.uv[0]))*raw->w));auto sy=std::min(raw->h-1,unsigned((region.uv[1]+(y+.5f)/64*(region.uv[3]-region.uv[1]))*raw->h));size_t p=(y*64+x)*4;
  if(raw->format==0x8dbb){pixels[p]=pixels[p+1]=pixels[p+2]=255;pixels[p+3]=static_cast<unsigned char>(alpha(*raw,sx,sy));}else memcpy(pixels.data()+p,raw->bytes.data()+raw->offset+(size_t(sy)*raw->w+sx)*4,4);
 }
 D3D11_TEXTURE2D_DESC d{};d.Width=d.Height=64;d.MipLevels=d.ArraySize=1;d.Format=DXGI_FORMAT_R8G8B8A8_UNORM;d.SampleDesc.Count=1;d.Usage=D3D11_USAGE_IMMUTABLE;d.BindFlags=D3D11_BIND_SHADER_RESOURCE;
 D3D11_SUBRESOURCE_DATA data{};data.pSysMem=pixels.data();data.SysMemPitch=64*4;ID3D11Texture2D* texture=nullptr;ID3D11ShaderResourceView* srv=nullptr;
 if(SUCCEEDED(device->CreateTexture2D(&d,&data,&texture))){device->CreateShaderResourceView(texture,nullptr,&srv);texture->Release();}textures[name]={srv,frame};return srv;
}
void releaseIcons(){for(auto& [_,entry]:textures)if(entry.srv)entry.srv->Release();textures.clear();rawCache.clear();}
}
