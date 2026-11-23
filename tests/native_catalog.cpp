#include "catalog.hpp"
#include <iostream>
using namespace spirit;
static void check(bool value){if(!value)throw std::runtime_error("test failed");}
template<class F>void rejects(F f){bool caught=false;try{f();}catch(...){caught=true;}check(caught);}
Json row(unsigned id,unsigned dep=0){return {{"id",id},{"dep",dep},{"nm","Test"},{"spirit","Spirit"},{"ct","candles"},{"cst",4},{"ap",false},{"session","DO_NOT_EXPORT"}};}
Json page(int v,int count,Json rows){return {{"spirit_shops_version",v},{"spirit_shops_total_count",count},{"spirit_shops",rows}};}
int main(){
 int calls=0;auto value=readCatalog(426499,[]{return true;},[&](const Json& q){check(q.at("o")==calls);++calls;return page(1,2,Json::array({row(calls,calls==2?1:0)}));},[](auto,auto){});
 check(calls==2&&value.at("items").size()==2&&value.dump().find("DO_NOT_EXPORT")==std::string::npos);
 auto t=tree(value.at("items"));check(t[1].parent==0&&t[1].depth==1);
 rejects([&]{readCatalog(1,[]{return true;},[](auto){return page(1,2,Json::array({row(1),row(1)}));},[](auto,auto){});});
 rejects([&]{readCatalog(1,[]{return true;},[](auto){return page(1,2,Json::array());},[](auto,auto){});});
 rejects([&]{readCatalog(1,[]{return false;},[](auto){return page(1,0,Json::array());},[](auto,auto){});});
 calls=0;rejects([&]{readCatalog(1,[]{return true;},[&](auto){return page(++calls,2,Json::array({row(calls)}));},[](auto,auto){});});
 int reads=0;rejects([&]{readCatalog(1,[&]{return ++reads==1;},[](auto){return page(1,0,Json::array());},[](auto,auto){});});
 auto cycle=tree(Json::array({row(1,2),row(2,1)}));check(cycle[0].broken&&cycle[0].parent==-1);
 auto missing=tree(Json::array({row(1,99)}));check(missing[0].broken&&missing[0].parent==-1);
 auto bad=row(1);bad["cst"]=-1;rejects([&]{clean(bad);});bad=row(1);bad["id"]=0;rejects([&]{clean(bad);});
 auto unknown=row(1);unknown.erase("ct");check(price(unknown).find("неизв.")!=std::string::npos);
 auto authentic=row(3);authentic["ctyp"]="prestige";authentic["evn"]="global_test";authentic["ta"]="spirit_unlock";authentic["tt"]=2;authentic["to"]=1;
 auto cleaned=clean(authentic);check(cleaned.at("ctyp")=="prestige"&&cleaned.at("tt")==2&&price(cleaned)=="4 вознесённых свечей");
 auto first=advanceHistory(nullptr,value);check(first.at("baseline").is_null()&&changes(first.at("current"),first.at("baseline")).empty());
 auto next=value;next["items"][0]["cst"]=9;next["items"].erase(next["items"].begin()+1);next["items"].push_back(clean(row(77)));
 auto history=advanceHistory(first,next);auto diff=changes(history.at("current"),history.at("baseline"));check(diff[1]=="changed"&&diff[2]=="removed"&&diff[77]=="new");
 auto repeated=advanceHistory(history,next);check(repeated.at("baseline")==history.at("baseline"));
 auto ordered=tree(Json::array({row(1,99),row(99)}));auto indices=treeOrder(ordered);check(indices[0]==1&&indices[1]==0);
 auto oldMetadata=value,newMetadata=value;for(auto& r:newMetadata["items"])r["icon"]="ChangedVisual";check(sameCatalog(oldMetadata,newMetadata));
 std::cout<<"Native catalog checks passed\n";
}
