#include "app.hpp"
#include "assets.hpp"
#include "imgui.h"
#include <shellapi.h>
#include <cmath>
namespace spirit {
namespace {
std::string fold(std::string s){for(char& c:s)if(c>='A'&&c<='Z')c+=32;return s;}
bool contains(const std::string& text,const std::string& query){return query.empty()||fold(text).find(fold(query))!=std::string::npos;}
std::string shorten(std::string text,size_t max){if(text.size()<=max)return text;size_t n=max;while(n>0&&(static_cast<unsigned char>(text[n])&0xc0)==0x80)--n;return text.substr(0,n)+"…";}
const char* badge(const std::string& change){return change=="new"?"Новое":change=="changed"?"Изменено":change=="removed"?"Удалено":"";}
bool kindMatches(const Json& row,int kind){auto t=row.value("typ",std::string{});return kind==0||(kind==1&&t=="outfit")||(kind==2&&t=="spirit_upgrade")||(kind==3&&t=="lootbox")||(kind==4&&t!="outfit"&&t!="spirit_upgrade"&&t!="lootbox");}
bool deltaMatches(const std::string& d,int filter){return filter==0||(filter==1&&(d=="new"||d=="changed"))||(filter==2&&d=="new")||(filter==3&&d=="changed")||(filter==4&&d=="removed");}
}
void drawPanel(){
 static Json snapshot=nullptr,baseline=nullptr;static unsigned long long revision=~0ULL;static std::string message;static std::map<std::string,Json> groups;
 static std::map<unsigned,std::string> delta;static std::map<unsigned,Json> previous;static unsigned added=0,changed=0,removed=0;
 static char query[128]{};static std::string selected;static unsigned selectedId=0;static int filter=0,kind=0,sort=0;static bool asTree=true,eventsOnly=false;
 {std::lock_guard<std::mutex> guard(app.lock);message=app.status;if(revision!=app.revision.load()){
  revision=app.revision;snapshot=app.catalog;baseline=app.baseline;groups.clear();previous.clear();delta.clear();added=changed=removed=0;
  if(snapshot.is_object()){
   delta=changes(snapshot,baseline);for(auto& [id,d]:delta){added+=d=="new";changed+=d=="changed";removed+=d=="removed";}
   if(baseline.is_object())for(auto& row:baseline.at("items"))previous[row.at("id").get<unsigned>()]=row;
   auto add=[&](const Json& row){auto name=row.at("spirit").get<std::string>();if(!groups.count(name))groups[name]=Json::array();groups[name].push_back(row);};
   for(auto& row:snapshot.at("items"))add(row);for(auto& [id,d]:delta)if(d=="removed")add(previous[id]);
  }
 }}
 auto& io=ImGui::GetIO();ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize(io.DisplaySize);
 ImGui::Begin("Дерево духов##root",nullptr,ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoSavedSettings);
 ImGui::TextColored({.44f,.89f,.80f,1},"RUND / ДЕРЕВО ДУХОВ");ImGui::SameLine();ImGui::TextDisabled("  F8 — скрыть");ImGui::Separator();
 ImGui::BeginDisabled(app.busy||!app.ready||!app.supported);if(ImGui::Button(app.busy?"Загрузка…":"Обновить каталог"))app.readRequested=true;ImGui::EndDisabled();ImGui::SameLine();
 ImGui::BeginDisabled(app.busy||snapshot.is_null());if(ImGui::Button("Экспорт JSON"))app.exportRequested=true;ImGui::SameLine();if(ImGui::Button("Принять за базу"))app.baselineRequested=true;ImGui::EndDisabled();
 ImGui::SameLine();if(ImGui::Button("Папка")){CreateDirectoryW(app.downloadDirectory.c_str(),nullptr);ShellExecuteW(nullptr,L"open",app.downloadDirectory.c_str(),nullptr,nullptr,SW_SHOWNORMAL);}
 if(snapshot.is_object())ImGui::TextDisabled("Sky %u · каталог %lld · %zu узлов · +%u новых / %u изменённых / %u удалённых",app.build.load(),snapshot.at("version").get<long long>(),snapshot.at("items").size(),added,changed,removed);else ImGui::TextDisabled("Sky %u · каталог ещё не загружен",app.build.load());
 ImGui::TextWrapped("%s",message.c_str());ImGui::SetNextItemWidth(-1);ImGui::InputTextWithHint("##search","Поиск духа или предмета",query,sizeof(query));
 ImGui::SetNextItemWidth(210);const char* filters[]={"Все предметы","Новые и изменённые","Только новые","Только изменённые","Удалённые"};ImGui::Combo("##changes",&filter,filters,5);ImGui::SameLine();
 ImGui::SetNextItemWidth(140);const char* kinds[]={"Все типы","Одежда","Эмоции","Заклинания","Прочее"};ImGui::Combo("##kind",&kind,kinds,5);ImGui::SameLine();ImGui::Checkbox("Дерево",&asTree);ImGui::SameLine();ImGui::Checkbox("События",&eventsOnly);
 if(filter&&!baseline.is_object())ImGui::TextWrapped("Это первый снимок. Нажми «Принять за базу», чтобы отслеживать дальнейшие изменения.");
 auto matches=[&](const Json& row,const std::string& branch){unsigned id=row.at("id").get<unsigned>();std::string d=delta.count(id)?delta[id]:"";return deltaMatches(d,filter)&&kindMatches(row,kind)&&(!eventsOnly||!row.value("evn",std::string{}).empty())&&(contains(branch,query)||contains(row.at("nm").get<std::string>(),query));};
 float height=std::max(150.f,ImGui::GetContentRegionAvail().y-57);float sidebar=std::min(235.f,io.DisplaySize.x*.28f);
 ImGui::BeginChild("spirits",{sidebar,height},ImGuiChildFlags_Borders);ImGui::TextDisabled("ДУХИ И СОБЫТИЯ");
 std::vector<std::string> visible;std::map<std::string,unsigned> counts;
 for(auto& [name,rows]:groups){for(auto& row:rows)if(matches(row,name))counts[name]++;if(counts[name])visible.push_back(name);}
 if(!visible.empty()&&std::find(visible.begin(),visible.end(),selected)==visible.end()){selected=visible.front();selectedId=0;}
 for(auto& name:visible){ImGui::PushID(name.c_str());auto label=shorten(name,25)+" ("+std::to_string(counts[name])+")";if(ImGui::Selectable(label.c_str(),name==selected)){selected=name;selectedId=0;}if(ImGui::IsItemHovered())ImGui::SetTooltip("%s\n%u подходящих узлов",name.c_str(),counts[name]);ImGui::PopID();}
 if(visible.empty())ImGui::TextWrapped(snapshot.is_null()?"Войдите в Sky и дождитесь каталога.":filter?"Нет предметов для выбранного фильтра.":"Ничего не найдено.");ImGui::EndChild();ImGui::SameLine();
 ImGui::BeginChild("branch",{0,height},ImGuiChildFlags_Borders);
 if(!visible.empty()&&groups.count(selected)){
  ImGui::TextWrapped("%s",selected.c_str());
  if(asTree)ImGui::TextDisabled("Зависимости · начальные узлы сверху · прокрутка по двум осям");
  else {ImGui::SetNextItemWidth(180);const char* sorts[]={"Порядок дерева","По названию","По цене"};ImGui::Combo("##sort",&sort,sorts,3);}
  static std::vector<Node> nodes;static std::vector<size_t> order;static std::string cachedBranch;static unsigned long long cachedRevision=~0ULL;
  if(cachedBranch!=selected||cachedRevision!=revision){nodes=tree(groups[selected]);order=treeOrder(nodes);cachedBranch=selected;cachedRevision=revision;}
  std::vector<bool> matched(nodes.size()),show(nodes.size());for(size_t i=0;i<nodes.size();i++)matched[i]=matches(nodes[i].data,selected);
  for(size_t i=0;i<nodes.size();i++)if(matched[i]){show[i]=true;if(asTree){int p=nodes[i].parent;unsigned guard=0;while(p>=0&&guard++<64){show[size_t(p)]=true;p=nodes[size_t(p)].parent;}}}
  if(!asTree&&sort)std::stable_sort(order.begin(),order.end(),[&](size_t a,size_t b){return sort==1?nodes[a].data.at("nm").get<std::string>()<nodes[b].data.at("nm").get<std::string>():nodes[a].data.value("cst",0u)<nodes[b].data.value("cst",0u);});else if(asTree||sort==0)order=treeOrder(nodes);
  ImGui::BeginChild("canvas",{0,-72},ImGuiChildFlags_Borders,asTree?ImGuiWindowFlags_HorizontalScrollbar:0);
  if(asTree){
   auto origin=ImGui::GetCursorScreenPos();auto draw=ImGui::GetWindowDrawList();const float width=245,stepX=275,stepY=103;auto mouse=io.MousePos;
   std::vector<ImVec2> positions(nodes.size());size_t count=0;unsigned maxDepth=0;
   for(auto i:order)if(show[i]&&count<1000){positions[i]={origin.x+10+nodes[i].depth*stepX,origin.y+10+count*stepY};maxDepth=std::max(maxDepth,nodes[i].depth);++count;}
   for(auto i:order)if(show[i]&&nodes[i].parent>=0&&show[size_t(nodes[i].parent)]){auto a=positions[size_t(nodes[i].parent)],b=positions[i];if(!a.x||!b.x)continue;ImVec2 p1{a.x+width,a.y+40},p2{b.x,b.y+40};draw->AddBezierCubic(p1,{p1.x+25,p1.y},{p2.x-25,p2.y},p2,IM_COL32(79,154,152,210),2.f);}
   for(auto i:order)if(show[i]&&positions[i].x){
    auto& node=nodes[i];auto& row=node.data;auto p=positions[i];ImVec2 end{p.x+width,p.y+84};unsigned id=row.at("id").get<unsigned>();bool chosen=id==selectedId;std::string d=delta.count(id)?delta[id]:"";
    if(!ImGui::IsRectVisible(p,end))continue;bool dim=!matched[i];int opacity=dim?110:255;
    draw->AddRectFilled(p,end,chosen?IM_COL32(35,79,83,opacity):IM_COL32(25,41,54,opacity),9);
    ImU32 border=d=="new"?IM_COL32(99,229,155,opacity):d=="changed"?IM_COL32(241,193,97,opacity):d=="removed"?IM_COL32(222,112,119,opacity):node.broken?IM_COL32(228,151,85,opacity):IM_COL32(58,92,106,opacity);
    draw->AddRect(p,end,border,9,0,chosen||!d.empty()?2.f:1.f);
    auto srv=iconTexture(nullptr,itemIcon(row));if(srv)draw->AddImage(ImTextureRef((ImTextureID)(uintptr_t)srv),{p.x+8,p.y+13},{p.x+56,p.y+61},{0,0},{1,1},IM_COL32(245,245,245,opacity));else draw->AddText({p.x+20,p.y+30},IM_COL32(180,190,200,opacity),"?");
    auto name=shorten(row.at("nm").get<std::string>(),22);draw->AddText({p.x+64,p.y+9},IM_COL32(228,237,245,opacity),name.c_str());
    auto cost=shorten(price(row),30);draw->AddText({p.x+64,p.y+31},IM_COL32(112,218,186,opacity),cost.c_str());
    std::string note=row.value("ap",false)?"Пропуск":"";if(!d.empty()){if(!note.empty())note+=" · ";note+=badge(d);}if(dim)note="Контекст дерева";if(note.empty())note="#"+std::to_string(id);draw->AddText({p.x+64,p.y+56},IM_COL32(153,170,188,opacity),note.c_str());
    if(ImGui::IsWindowHovered()&&mouse.x>=p.x&&mouse.x<=end.x&&mouse.y>=p.y&&mouse.y<=end.y){ImGui::BeginTooltip();ImGui::TextUnformatted(row.at("nm").get_ref<const std::string&>().c_str());ImGui::TextUnformatted(price(row).c_str());if(d=="changed"&&previous.count(id))ImGui::Text("Раньше: %s",price(previous[id]).c_str());if(row.contains("ctyp"))ImGui::Text("Валюта: %s",row.at("ctyp").get_ref<const std::string&>().c_str());if(node.broken)ImGui::TextUnformatted("Зависимость отсутствует или циклична");ImGui::EndTooltip();if(ImGui::IsMouseClicked(0))selectedId=id;}
   }
   ImGui::Dummy({(maxDepth+1)*stepX+15,count*stepY+15});
  }else{
   for(auto i:order)if(matched[i]){auto& row=nodes[i].data;unsigned id=row.at("id").get<unsigned>();auto srv=iconTexture(nullptr,itemIcon(row));if(srv){ImGui::Image(ImTextureRef((ImTextureID)(uintptr_t)srv),{32,32});ImGui::SameLine();}std::string d=delta.count(id)?delta[id]:"";auto label=(!d.empty()?"["+std::string(badge(d))+"] ":"")+row.at("nm").get<std::string>();ImGui::PushID(int(id));if(ImGui::Selectable(label.c_str(),selectedId==id))selectedId=id;ImGui::TextDisabled("%s",price(row).c_str());ImGui::PopID();ImGui::Separator();}
  }
  ImGui::EndChild();
  for(auto& node:nodes)if(node.data.at("id").get<unsigned>()==selectedId){auto& row=node.data;ImGui::TextWrapped("%s",row.at("nm").get_ref<const std::string&>().c_str());ImGui::TextWrapped("%s · предшественник #%u",price(row).c_str(),row.value("dep",0u));auto d=delta.count(selectedId)?delta[selectedId]:"";if(d=="changed"&&previous.count(selectedId))ImGui::TextDisabled("Раньше: %s",price(previous[selectedId]).c_str());break;}
 }else ImGui::TextWrapped(snapshot.is_null()?"Каталог загружается после входа в игру.":"Выбери другой фильтр или дождись следующего изменения каталога.");
 ImGui::EndChild();ImGui::TextDisabled("Экспорт всегда полный: Загрузки\\sky-spirit\\spirit-catalog-export.json");ImGui::TextDisabled("Каталог не подтверждает активность события; серые узлы — контекст фильтра.");ImGui::End();
}
}
