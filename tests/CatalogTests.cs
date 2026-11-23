using System;
using System.Collections.Generic;
using System.IO;
using SkySpiritExport;
class CatalogTests {
 static int checks;
 static void Assert(bool value){checks++;if(!value)throw new Exception("Failed check "+checks);}
 static void Reject(Action action){bool rejected=false;try{action();}catch(InvalidDataException){rejected=true;}catch(InvalidOperationException){rejected=true;}Assert(rejected);}
 static Dictionary<string,object> Page(int version,int total,params object[] rows){return new Dictionary<string,object>{{"spirit_shops_version",version},{"spirit_shops_total_count",total},{"spirit_shops",rows}};}
 static Dictionary<string,object> Row(int id){return new Dictionary<string,object>{{"id",id},{"nm","item"},{"spirit","spirit"},{"cst",3},{"ct","candles"},{"ap",false},{"session","SECRET"},{"private_key","SECRET"}};}
 static void Main(){
  var login=new Login{Build=426499,User="test",Session="test"};int calls=0;
  var good=Catalog.Fetch(login,()=>login,body=>{Assert(Convert.ToInt32(body["o"])==calls);calls++;return Page(7,2,Row(calls));},(n,t)=>{});
  Assert(calls==2);string json=Catalog.Json().Serialize(good);Assert(!json.Contains("SECRET")&&!json.Contains("session")&&!json.Contains("private_key"));
  Reject(()=>Catalog.Fetch(login,()=>login,b=>Page(1,2,Row(1),Row(1)),(n,t)=>{}));
  calls=0;Reject(()=>Catalog.Fetch(login,()=>login,b=>Page(++calls,2,Row(calls)),(n,t)=>{}));
  Reject(()=>Catalog.Fetch(login,()=>login,b=>Page(1,2),(n,t)=>{}));
  Reject(()=>Catalog.Fetch(login,()=>null,b=>Page(1,0),(n,t)=>{}));
  int reads=0;Reject(()=>Catalog.Fetch(login,()=>++reads==1?login:null,b=>Page(1,0),(n,t)=>{}));
  var bad=Row(1);bad["cst"]=1.5;Reject(()=>Catalog.Fetch(login,()=>login,b=>Page(1,1,bad),(n,t)=>{}));
  Reject(()=>Catalog.Fetch(login,()=>login,b=>Page(1,50001),(n,t)=>{}));
  var parsed=Catalog.Json().Deserialize<Dictionary<string,object>>("{\"spirit_shops_version\":1,\"spirit_shops_total_count\":0,\"spirit_shops\":[]}");
  var empty=Catalog.Fetch(login,()=>login,b=>parsed,(n,t)=>{});Assert(empty.ContainsKey("items"));
  string dir=Path.Combine(Path.GetTempPath(),"sky-catalog-test-"+Guid.NewGuid().ToString("N"));Directory.CreateDirectory(dir);
  try{string path=Path.Combine(dir,"catalog.json");Catalog.Save(path,good);string old=File.ReadAllText(path);Catalog.Save(path,empty);Assert(File.ReadAllText(path+".previous")==old);Assert(!File.ReadAllText(path).Contains("SECRET"));}finally{Directory.Delete(dir,true);}
  Console.WriteLine("Catalog checks passed: "+checks);
 }
}
