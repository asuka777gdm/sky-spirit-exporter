#pragma once
#include "json.hpp"
#include <algorithm>
#include <functional>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
namespace spirit {
using Json=nlohmann::json;
inline void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
inline long long integer(const Json& v){require(v.is_number_integer(),"Некорректное число в каталоге");if(v.is_number_unsigned())require(v.get<unsigned long long>()<=0x7fffffffffffffffULL,"Слишком большое число");return v.get<long long>();}
inline Json clean(const Json& row){
 require(row.is_object()&&row.contains("id"),"Узел без ID");Json out=Json::object();
 for(const char* key:{"id","cst","dep","tt","to","td"})if(row.contains(key)){auto n=integer(row.at(key));require(n>=0&&n<=0xffffffffLL&&(std::string(key)!="id"||n>0),"Неверный ID, цена или зависимость");out[key]=n;}
 for(const char* key:{"nm","spirit","typ","ctyp","evn","ta","ct","currency","currency_type"})if(row.contains(key)){
  auto& v=row.at(key);if(v.is_string()){require(v.get_ref<const std::string&>().size()<=256,"Слишком длинное имя");out[key]=v;}
  else {require(std::string(key)=="ct"||std::string(key)=="currency"||std::string(key)=="currency_type","Неверное имя узла");auto n=integer(v);require(n>=0&&n<=0xffffffffLL,"Неверная валюта");out[key]=n;}
 }
 require(out.contains("nm")&&out.contains("spirit")&&!out.at("spirit").get_ref<const std::string&>().empty(),"Нет имени предмета или духа");
 if(row.contains("ap")){require(row.at("ap").is_boolean(),"Неверный флаг пропуска");out["ap"]=row.at("ap");}return out;
}
inline Json readCatalog(unsigned build,const std::function<bool()>& current,const std::function<Json(const Json&)>& post,const std::function<void(size_t,size_t)>& progress){
 Json rows=Json::array();std::set<unsigned> ids;long long version=0,total=-1;
 for(int page=0;page<100;page++){
  require(current(),"Аккаунт или сессия изменились");auto reply=post({{"l",1000},{"o",rows.size()},{"v",version}});require(current(),"Сессия изменилась во время чтения");
  require(reply.is_object()&&reply.contains("spirit_shops_version")&&reply.contains("spirit_shops_total_count")&&reply.contains("spirit_shops")&&reply.at("spirit_shops").is_array(),"Неизвестный ответ сервера");
  auto v=integer(reply.at("spirit_shops_version")),n=integer(reply.at("spirit_shops_total_count"));
  require(v>=0&&n>=0&&n<=50000&&(page==0||(v==version&&n==total)),"Каталог изменился во время чтения");version=v;total=n;
  auto& batch=reply.at("spirit_shops");require(batch.size()<=1000,"Слишком большая страница");
  for(auto& row:batch){auto item=clean(row);require(ids.insert(item.at("id").get<unsigned>()).second,"Повтор ID в каталоге");rows.push_back(std::move(item));require(rows.size()<=size_t(total),"Лишние узлы каталога");}
  progress(rows.size(),size_t(total));if(rows.size()==size_t(total)){std::sort(rows.begin(),rows.end(),[](auto&a,auto&b){return a.at("id").template get<unsigned>()<b.at("id").template get<unsigned>();});return {{"build",build},{"version",version},{"items",rows}};}
  require(!batch.empty(),"Каталог неполный; прежний снимок сохранён");
 }throw std::runtime_error("Превышено число страниц каталога");
}
inline std::string currencyLabel(const std::string& name){
 static const std::map<std::string,std::string> labels={{"candles","свечей"},{"candle","свечей"},{"heart","сердец"},{"hearts","сердец"},{"prestige","вознесённых свечей"},{"ascended_candles","вознесённых свечей"},{"season_candle","сезонных свечей"},{"season_candles","сезонных свечей"},{"seasonal_candles","сезонных свечей"},{"season_heart","сезонных сердец"},{"season_hearts","сезонных сердец"},{"event_candle","билетов события"},{"season_pass_token","жетонов пропуска"}};
 auto it=labels.find(name);if(it!=labels.end())return it->second;if(name.rfind("event_candle_",0)==0)return "билетов события";return name;
}
inline std::string price(const Json& row){
 if(!row.contains("cst"))return "Цена неизвестна";std::string unit="неизв. валюта";
 if(integer(row.at("cst"))==0)return "Бесплатно";
 for(const char* key:{"ctyp","ct","currency_type","currency"})if(row.contains(key)&&row.at(key).is_string()){unit=currencyLabel(row.at(key).get<std::string>());break;}
 return std::to_string(row.at("cst").get<unsigned>())+" "+unit;
}
inline bool sameItem(const Json& a,const Json& b){
 for(const char* key:{"id","nm","spirit","typ","cst","dep","ap"})if(a.value(key,Json())!=b.value(key,Json()))return false;
 // Newly resolved visual metadata is not a gameplay price change.
 if(a.contains("ct")&&b.contains("ct")&&a.at("ct")!=b.at("ct"))return false;
 if(a.contains("ctyp")&&b.contains("ctyp"))for(const char* key:{"ctyp","evn","ta","tt","to","td"})if(a.value(key,Json())!=b.value(key,Json()))return false;return true;
}
inline bool sameCatalog(const Json& a,const Json& b){
 if(!a.is_object()||!b.is_object()||!a.contains("items")||!b.contains("items")||a.at("items").size()!=b.at("items").size())return false;
 std::map<unsigned,Json> index;for(auto& row:a.at("items"))index[row.at("id").get<unsigned>()]=row;
 for(auto& row:b.at("items")){auto found=index.find(row.at("id").get<unsigned>());if(found==index.end()||!sameItem(found->second,row))return false;}return true;
}
inline Json advanceHistory(const Json& history,const Json& fresh){
 Json baseline=nullptr;
 if(history.is_object()&&history.contains("current")){
  if(sameCatalog(history.at("current"),fresh))baseline=history.value("baseline",Json());
  else baseline=history.at("current");
 }
 return {{"schema",1},{"current",fresh},{"baseline",baseline}};
}
inline std::map<unsigned,std::string> changes(const Json& current,const Json& baseline){
 std::map<unsigned,std::string> result;if(!baseline.is_object())return result;
 std::map<unsigned,Json> before,now;for(auto& r:baseline.at("items"))before[r.at("id").get<unsigned>()]=r;for(auto& r:current.at("items"))now[r.at("id").get<unsigned>()]=r;
 for(auto& [id,row]:now){auto it=before.find(id);if(it==before.end())result[id]="new";else if(!sameItem(row,it->second))result[id]="changed";}
 for(auto& [id,row]:before)if(!now.count(id))result[id]="removed";return result;
}
inline Json sanitizedCatalog(const Json& input){
 require(input.is_object()&&input.contains("build")&&input.contains("version")&&input.contains("items")&&input.at("items").is_array()&&input.at("items").size()<=50000,"Неверный снимок каталога");
 Json result={{"build",integer(input.at("build"))},{"version",integer(input.at("version"))},{"items",Json::array()}};std::set<unsigned> seen;
 for(auto& row:input.at("items")){auto value=clean(row);require(seen.insert(value.at("id").get<unsigned>()).second,"Повтор ID в снимке");result["items"].push_back(std::move(value));}return result;
}
struct Node {Json data;int parent=-1;unsigned depth=0;bool broken=false;};
inline std::vector<Node> tree(const Json& rows){
 std::vector<Node> nodes;std::map<unsigned,int> index;
 for(auto& row:rows){auto item=clean(row);require(index.emplace(item.at("id").get<unsigned>(),int(nodes.size())).second,"Повтор ID в дереве");nodes.push_back({item});}
 for(auto& node:nodes){unsigned id=node.data.value("dep",0u);if(!id)continue;auto p=index.find(id);if(p==index.end()||nodes[p->second].data.at("spirit")!=node.data.at("spirit")){node.broken=true;continue;}node.parent=p->second;}
 for(size_t i=0;i<nodes.size();i++){
  std::set<int> seen{int(i)};int p=nodes[i].parent;
  while(p>=0){if(!seen.insert(p).second||nodes[i].depth>=64){nodes[i].broken=true;nodes[i].parent=-1;break;}++nodes[i].depth;p=nodes[p].parent;}
  if(nodes[i].broken)nodes[i].depth=0;
 }return nodes;
}
inline std::vector<size_t> treeOrder(const std::vector<Node>& nodes){
 std::vector<std::vector<size_t>> children(nodes.size());std::vector<size_t> roots,result;std::vector<bool> seen(nodes.size());
 for(size_t i=0;i<nodes.size();i++){if(nodes[i].parent>=0)children[size_t(nodes[i].parent)].push_back(i);else roots.push_back(i);}
 std::function<void(size_t)> visit=[&](size_t i){if(seen[i])return;seen[i]=true;result.push_back(i);for(auto child:children[i])visit(child);};
 for(auto root:roots)visit(root);for(size_t i=0;i<nodes.size();i++)if(!seen[i])visit(i);return result;
}
}
