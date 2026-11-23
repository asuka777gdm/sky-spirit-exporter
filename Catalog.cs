using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Net;
using System.Text;
using System.Web.Script.Serialization;

namespace SkySpiritExport {
 public sealed class Login {
  public int Build; public string User, Session;
  public bool Same(Login other) { return other!=null && Build==other.Build && User==other.User && Session==other.Session; }
 }
 public static class Catalog {
  public static JavaScriptSerializer Json() { return new JavaScriptSerializer { MaxJsonLength=16*1024*1024, RecursionLimit=64 }; }
  public static long Integer(object v) {
   if(!(v is int || v is long || v is uint))throw new InvalidDataException("Ожидалось целое число.");
   return Convert.ToInt64(v);
  }
  static long Required(Dictionary<string,object> row,string key) {
   object v; if(!row.TryGetValue(key,out v))throw new InvalidDataException("Нет поля "+key);
   return Integer(v);
  }
  static Dictionary<string,object> Clean(Dictionary<string,object> row) {
   var clean=new Dictionary<string,object>();
   foreach(string key in new[]{"id","cst","dep"}) {
    object v;if(!row.TryGetValue(key,out v)){if(key=="id")throw new InvalidDataException("Нет ID.");continue;}
    long n=Integer(v);if(n<0 || n>uint.MaxValue || (key=="id" && n==0))throw new InvalidDataException("Неверное поле "+key);
    clean[key]=n;
   }
   foreach(string key in new[]{"nm","spirit","typ","ct","currency","currency_type"}) {
    object v;if(!row.TryGetValue(key,out v))continue;
    if(v is string && ((string)v).Length<=256)clean[key]=v;
    else if(key=="ct" || key=="currency" || key=="currency_type") {
     long n=Integer(v);if(n<0||n>uint.MaxValue)throw new InvalidDataException("Неверная валюта.");clean[key]=n;
    }else throw new InvalidDataException("Неверное имя.");
   }
   if(!clean.ContainsKey("nm") || !clean.ContainsKey("spirit") || String.IsNullOrEmpty(clean["spirit"] as string))throw new InvalidDataException("Нет имени предмета или духа.");
   object pass;if(row.TryGetValue("ap",out pass)){if(!(pass is bool))throw new InvalidDataException("Неверный флаг пропуска.");clean["ap"]=pass;}
   return clean;
  }
  public static Dictionary<string,object> Fetch(Login login,Func<Login> current,Func<Dictionary<string,object>,Dictionary<string,object>> post,Action<int,int> progress) {
   var items=new List<Dictionary<string,object>>();var ids=new HashSet<long>();long version=0,total=-1;
   for(int page=0;page<100;page++) {
    if(!login.Same(current()))throw new InvalidOperationException("Аккаунт или сессия изменились. Экспорт отменён.");
    var reply=post(new Dictionary<string,object>{{"l",1000},{"o",items.Count},{"v",version}});
    if(!login.Same(current()))throw new InvalidOperationException("Сессия изменилась. Ответ отброшен.");
    long count=Required(reply,"spirit_shops_total_count"),v=Required(reply,"spirit_shops_version");
    if(count<0||count>50000||v<0||(page>0&&(count!=total||v!=version)))throw new InvalidDataException("Каталог изменился или слишком велик.");
    total=count;version=v;object rows;
    if(!reply.TryGetValue("spirit_shops",out rows)||(!(rows is object[])&&!(rows is ArrayList)))throw new InvalidDataException("Нет списка духов.");
    var batch=rows is object[]?(object[])rows:((ArrayList)rows).ToArray();
    if(batch.Length>1000)throw new InvalidDataException("Превышен размер страницы.");
    foreach(object entry in batch) {
     var row=entry as Dictionary<string,object>;if(row==null)throw new InvalidDataException("Неверный узел.");
     var clean=Clean(row);if(!ids.Add((long)clean["id"]))throw new InvalidDataException("Повтор ID.");
     items.Add(clean);if(items.Count>total)throw new InvalidDataException("Лишние узлы.");
    }
    progress(items.Count,(int)total);
    if(items.Count==total) {
     items.Sort((a,b)=>((long)a["id"]).CompareTo((long)b["id"]));
     return new Dictionary<string,object>{{"version",version},{"build",login.Build},{"items",items}};
    }
    if(batch.Length==0)break;
   }
   throw new InvalidDataException("Каталог неполный. Старый файл сохранён.");
  }
  public static Dictionary<string,object> Post(Login login,Dictionary<string,object> page) {
   var payload=new Dictionary<string,object>(page){{"user",login.User},{"user_id",login.User},{"session",login.Session}};
   var request=(HttpWebRequest)WebRequest.Create("https://live.radiance.thatgamecompany.com/account/get_spirit_shops");
   request.Method="POST";request.ContentType="application/json";request.AllowAutoRedirect=false;
   request.Timeout=20000;request.ReadWriteTimeout=20000;request.Proxy=null;
   request.AutomaticDecompression=DecompressionMethods.GZip|DecompressionMethods.Deflate;
   request.UserAgent="SkySpiritExport/0.1 (Windows; build "+login.Build+")";
   request.Headers["user-id"]=login.User;request.Headers["x-user-id"]=login.User;
   request.Headers["session"]=login.Session;request.Headers["x-session-token"]=login.Session;
   byte[] body=Encoding.UTF8.GetBytes(Json().Serialize(payload));request.ContentLength=body.Length;
   try {
    using(var stream=request.GetRequestStream())stream.Write(body,0,body.Length);
    using(var response=(HttpWebResponse)request.GetResponse()) {
     if(response.StatusCode!=HttpStatusCode.OK)throw new IOException("Сервер вернул HTTP "+(int)response.StatusCode);
     using(var stream=response.GetResponseStream())using(var memory=new MemoryStream()) {
      byte[] buf=new byte[8192];int n;
      while((n=stream.Read(buf,0,buf.Length))>0){if(memory.Length+n>16*1024*1024)throw new InvalidDataException("Ответ слишком большой.");memory.Write(buf,0,n);}
      var result=Json().DeserializeObject(Encoding.UTF8.GetString(memory.ToArray())) as Dictionary<string,object>;
      if(result==null)throw new InvalidDataException("Неверный ответ каталога.");return result;
     }
    }
   } catch(WebException e) {
    var response=e.Response as HttpWebResponse;
    string message=response==null?"Ошибка HTTPS. Проверьте соединение и вход в игру.":"Сервер вернул HTTP "+(int)response.StatusCode+". Повторного входа утилита не выполняет.";
    if(response!=null)response.Close();throw new IOException(message);
   } finally { Array.Clear(body,0,body.Length); }
  }
  public static void Save(string path,Dictionary<string,object> catalog) {
   path=Path.GetFullPath(path);Directory.CreateDirectory(Path.GetDirectoryName(path));
   string temp=path+".tmp-"+Guid.NewGuid().ToString("N");
   try {
    byte[] data=new UTF8Encoding(false).GetBytes(Json().Serialize(catalog)+"\n");
    using(var f=new FileStream(temp,FileMode.CreateNew,FileAccess.Write,FileShare.None)){f.Write(data,0,data.Length);f.Flush(true);}
    if(File.Exists(path))File.Replace(temp,path,path+".previous",true);else File.Move(temp,path);
   } finally { if(File.Exists(temp))File.Delete(temp); }
  }
 }
}
