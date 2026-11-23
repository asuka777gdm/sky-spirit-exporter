using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.IO.Pipes;
using System.Linq;
using System.Net;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;
using Microsoft.Win32.SafeHandles;

namespace SkySpiritExport {
 public static class Bridge {
  [DllImport("kernel32.dll",SetLastError=true)] static extern IntPtr OpenProcess(uint access,bool inherit,int pid);
  [DllImport("kernel32.dll",SetLastError=true)] static extern bool CloseHandle(IntPtr handle);
  [DllImport("kernel32.dll",SetLastError=true)] static extern IntPtr VirtualAllocEx(IntPtr process,IntPtr address,UIntPtr size,uint type,uint protect);
  [DllImport("kernel32.dll",SetLastError=true)] static extern bool VirtualFreeEx(IntPtr process,IntPtr address,UIntPtr size,uint type);
  [DllImport("kernel32.dll",SetLastError=true)] static extern bool WriteProcessMemory(IntPtr process,IntPtr address,byte[] data,UIntPtr size,out UIntPtr count);
  [DllImport("kernel32.dll",SetLastError=true)] static extern IntPtr CreateRemoteThread(IntPtr process,IntPtr attrs,UIntPtr stack,IntPtr start,IntPtr arg,uint flags,out uint thread);
  [DllImport("kernel32.dll")] static extern uint WaitForSingleObject(IntPtr handle,uint timeout);
  [DllImport("kernel32.dll",CharSet=CharSet.Unicode)] static extern IntPtr GetModuleHandle(string name);
  [DllImport("kernel32.dll",CharSet=CharSet.Ansi)] static extern IntPtr GetProcAddress(IntPtr module,string name);
  [DllImport("kernel32.dll",CharSet=CharSet.Unicode)] static extern bool GetModuleHandleEx(uint flags,IntPtr address,out IntPtr module);
  [DllImport("kernel32.dll",CharSet=CharSet.Unicode)] static extern uint GetModuleFileName(IntPtr module,StringBuilder name,int count);
  [DllImport("kernel32.dll",SetLastError=true)] static extern bool GetNamedPipeServerProcessId(SafePipeHandle pipe,out uint pid);
  public static readonly Dictionary<string,int> Builds=new Dictionary<string,int> {
   {"4514c85f73cfe4b6d7d9910b36585ce5e135cfd3a04167c1dbc88d14ea96dba4",410941},
   {"9723e07be346a57e3f99dce52641edc47dcb9b278727e41b459e87c7d9f46567",423272},
   {"bdd780f88a7d34f7c3a9b336e82dc57393c3a8a7f0e639f28c41e709b9134b19",426499}
  };
  public static Process Find(int pid) {
   if(pid>0){var p=Process.GetProcessById(pid);if(!String.Equals(p.ProcessName,"Sky",StringComparison.OrdinalIgnoreCase))throw new IOException("Выбранный процесс не Sky.exe.");return p;}
   var all=Process.GetProcessesByName("Sky");if(all.Length!=1)throw new IOException("Запустите одну копию Steam Sky. Для нескольких укажите --pid.");return all[0];
  }
  public static void Inject(Process p) {
   string path=p.MainModule.FileName,hash;
   using(var f=File.OpenRead(path))using(var sha=SHA256.Create())hash=BitConverter.ToString(sha.ComputeHash(f)).Replace("-","").ToLowerInvariant();
   if(!Builds.ContainsKey(hash))throw new IOException("Неизвестная сборка Sky. Нужна новая проверенная привязка. SHA-256: "+hash);
   string dll=Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"SkySpiritBridge.dll");
   if(!File.Exists(dll))throw new IOException("SkySpiritBridge.dll должна лежать рядом с лаунчером.");
   foreach(ProcessModule m in p.Modules)if(String.Equals(m.ModuleName,"SkySpiritBridge.dll",StringComparison.OrdinalIgnoreCase)) {
    if(!String.Equals(Path.GetFullPath(m.FileName),dll,StringComparison.OrdinalIgnoreCase))throw new IOException("Уже загружена DLL из другой папки. Перезапустите игру.");return;
   }
   IntPtr load=GetProcAddress(GetModuleHandle("kernel32.dll"),"LoadLibraryW"),owner;
   if(load==IntPtr.Zero||!GetModuleHandleEx(6,load,out owner))throw new IOException("Не найден LoadLibraryW.");
   var name=new StringBuilder(32768);if(GetModuleFileName(owner,name,name.Capacity)==0)throw new IOException("Не найден модуль загрузчика.");
   string ownerName=Path.GetFileName(name.ToString());IntPtr remoteLoad=IntPtr.Zero;
   foreach(ProcessModule m in p.Modules)if(String.Equals(m.ModuleName,ownerName,StringComparison.OrdinalIgnoreCase))remoteLoad=new IntPtr(m.BaseAddress.ToInt64()+load.ToInt64()-owner.ToInt64());
   if(remoteLoad==IntPtr.Zero)throw new IOException("Не найден системный модуль в Sky.");
   IntPtr process=OpenProcess(0x2|0x8|0x10|0x20|0x400,false,p.Id);if(process==IntPtr.Zero)throw new IOException("Не удалось открыть Sky. Запустите игру и утилиту с одинаковыми правами.");
   IntPtr memory=IntPtr.Zero,thread=IntPtr.Zero;bool uncertain=false;
   try {
    byte[] bytes=Encoding.Unicode.GetBytes(dll+"\0");memory=VirtualAllocEx(process,IntPtr.Zero,(UIntPtr)bytes.Length,0x3000,4);
    UIntPtr written;if(memory==IntPtr.Zero||!WriteProcessMemory(process,memory,bytes,(UIntPtr)bytes.Length,out written)||written.ToUInt64()!=(ulong)bytes.Length)throw new IOException("Ошибка передачи пути DLL.");
    uint tid;thread=CreateRemoteThread(process,IntPtr.Zero,UIntPtr.Zero,remoteLoad,memory,0,out tid);
    if(thread==IntPtr.Zero)throw new IOException("Не удалось загрузить DLL.");
    if(WaitForSingleObject(thread,15000)!=0){uncertain=true;throw new IOException("Загрузка не завершилась. Перезапустите Sky перед повтором.");}
   } finally {if(thread!=IntPtr.Zero)CloseHandle(thread);if(memory!=IntPtr.Zero&&!uncertain)VirtualFreeEx(process,memory,UIntPtr.Zero,0x8000);CloseHandle(process);}
  }
  public static Login Read(Process p) {
   if(p.HasExited)throw new IOException("Sky закрыт.");
   using(var pipe=new NamedPipeClientStream(".","SkySpiritExport-"+p.Id,PipeDirection.In,PipeOptions.Asynchronous)) {
    pipe.Connect(1500);uint server;if(!GetNamedPipeServerProcessId(pipe.SafePipeHandle,out server)||server!=p.Id)throw new IOException("Неверный процесс канала.");
    byte[] buf=new byte[512];int size=0;
    while(size<buf.Length) {
     var task=pipe.ReadAsync(buf,size,buf.Length-size);
     if(!task.Wait(2000))throw new IOException("Канал DLL не ответил.");
     int n=task.Result;if(n==0)break;size+=n;if(Array.IndexOf(buf,(byte)10,0,size)>=0)break;
    }
    try {
     if(size==512)throw new IOException("Неверный пакет DLL.");
     var data=Catalog.Json().Deserialize<Dictionary<string,object>>(Encoding.UTF8.GetString(buf,0,size));
     int state=(int)Catalog.Integer(data["state"]);
     if(state==3)throw new IOException("DLL отклонила сборку игры.");if(state==4)throw new IOException("Не удалось установить перехват. Проверьте конфликт с другими модами.");
     if(state!=2)return null;
     var login=new Login{Build=(int)Catalog.Integer(data["build"]),User=data["user"] as string,Session=data["session"] as string};
     if(!System.Text.RegularExpressions.Regex.IsMatch(login.User??"","^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$")||!System.Text.RegularExpressions.Regex.IsMatch(login.Session??"","^[0-9a-f]{32}$"))throw new IOException("Неверный контекст сессии.");
     return login;
    } finally {Array.Clear(buf,0,buf.Length);}
   }
  }
 }
 public static class Paths {
  [DllImport("shell32.dll")] static extern int SHGetKnownFolderPath(ref Guid folder,uint flags,IntPtr token,out IntPtr path);
  public static string DefaultOutput() {
   Guid downloads=new Guid("374DE290-123F-4565-9164-39C4925E467B");IntPtr path;
   int result=SHGetKnownFolderPath(ref downloads,0,IntPtr.Zero,out path);
   if(result!=0)Marshal.ThrowExceptionForHR(result);
   try {return Path.Combine(Marshal.PtrToStringUni(path),"sky-spirit","spirit-catalog-export.json");}
   finally {Marshal.FreeCoTaskMem(path);}
  }
 }
 public sealed class Window:Form {
  [DllImport("user32.dll")] static extern bool RegisterHotKey(IntPtr window,int id,uint modifiers,uint key);
  [DllImport("user32.dll")] static extern bool UnregisterHotKey(IntPtr window,int id);
  readonly Label status=new Label{AutoSize=false,Dock=DockStyle.Fill,Text="Войдите в Sky. После входа станет доступен экспорт дерева духов."};
  readonly Button connect=new Button{Text="Подключить к Sky",AutoSize=true};
  readonly Button refresh=new Button{Text="Экспорт дерева духов",AutoSize=true,Enabled=false};
  readonly Button folder=new Button{Text="Открыть папку JSON",AutoSize=true};
  readonly CheckBox watch=new CheckBox{Text="Обновлять каждые 5 минут",AutoSize=true};
  readonly TextBox output=new TextBox{Dock=DockStyle.Fill};
  readonly System.Windows.Forms.Timer timer=new System.Windows.Forms.Timer{Interval=300000};
  readonly System.Windows.Forms.Timer sessionTimer=new System.Windows.Forms.Timer{Interval=1000};
  Process sky;bool busy,closing,polling,exitRequested;readonly int pid;readonly bool attached;
  public Window(int target,string destination,bool autoAttached) {
   pid=target;attached=autoAttached;Text="Дерево духов — Sky (F8: показать / скрыть)";
   Size=new Size(660,240);MinimumSize=new Size(600,240);StartPosition=FormStartPosition.CenterScreen;TopMost=attached;
   output.Text=destination;output.ReadOnly=attached;connect.Visible=!attached;
   var layout=new TableLayoutPanel{Dock=DockStyle.Fill,Padding=new Padding(12),ColumnCount=1,RowCount=5};
   for(int i=0;i<4;i++)layout.RowStyles.Add(new RowStyle(SizeType.AutoSize));layout.RowStyles.Add(new RowStyle(SizeType.Percent,100));
   var buttons=new FlowLayoutPanel{AutoSize=true,Dock=DockStyle.Fill};buttons.Controls.Add(connect);buttons.Controls.Add(refresh);buttons.Controls.Add(folder);
   layout.Controls.Add(buttons);layout.Controls.Add(new Label{Text="JSON сохраняется в:",AutoSize=true});layout.Controls.Add(output);layout.Controls.Add(watch);layout.Controls.Add(status);Controls.Add(layout);
   connect.Click+=async(s,e)=>await Run(true);refresh.Click+=async(s,e)=>await Run(false);
   folder.Click+=(s,e)=>{try{string dir=Path.GetDirectoryName(Path.GetFullPath(output.Text));Directory.CreateDirectory(dir);Process.Start("explorer.exe",dir);}catch{status.Text="Не удалось открыть папку JSON.";}};
   watch.CheckedChanged+=(s,e)=>{timer.Enabled=watch.Checked;};timer.Tick+=async(s,e)=>{if(sky!=null&&!busy&&refresh.Enabled)await Run(false);};
   sessionTimer.Tick+=async(s,e)=>await Poll();
   Shown+=(s,e)=>{if(attached){try{sky=Bridge.Find(pid);sessionTimer.Start();}catch{status.Text="Sky закрыт или выбран неверный процесс.";}}};
   FormClosing+=(s,e)=>{if(attached&&!exitRequested){e.Cancel=true;Hide();return;}closing=true;timer.Stop();sessionTimer.Stop();UnregisterHotKey(Handle,1);};
  }
  protected override bool ShowWithoutActivation {get{return attached;}}
  protected override void OnHandleCreated(EventArgs e){base.OnHandleCreated(e);if(attached&&!RegisterHotKey(Handle,1,0x4000,0x77))status.Text="F8 занят другой программой. Панель можно открыть через панель задач.";}
  protected override void WndProc(ref Message message){if(message.Msg==0x0312&&message.WParam.ToInt32()==1){if(Visible)Hide();else Show();}base.WndProc(ref message);}
  async Task Poll(){
   if(polling||closing||busy)return;polling=true;
   try {
    if(sky.HasExited){exitRequested=true;Close();return;}
    Login login=await Task.Run(()=>Bridge.Read(sky));
    if(!busy){refresh.Enabled=login!=null;if(login==null)status.Text="Войдите в аккаунт Sky. Если вход уже выполнен, откройте магазин духа.";else if(!(refresh.Tag is bool && (bool)refresh.Tag)){status.Text="Вход получен. Нажмите «Экспорт дерева духов».";refresh.Tag=true;}}
   }catch(IOException e){if(!busy){refresh.Enabled=false;status.Text=e.Message;}}
   catch{if(!busy){refresh.Enabled=false;status.Text="Ожидание подключения мода к игре…";}}
   finally{polling=false;}
  }
  void ShowStatus(string message){if(!closing&&!IsDisposed)try{BeginInvoke((Action)(()=>status.Text=message));}catch(InvalidOperationException){}}
  async Task Run(bool attach) {
   if(busy)return;busy=true;connect.Enabled=refresh.Enabled=false;string destination=output.Text;
   try { await Task.Run(()=>{
    if(attach){sky=Bridge.Find(pid);Bridge.Inject(sky);}
    Login login=null;ShowStatus("Ожидание запроса игры. Войдите в аккаунт или откройте магазин духа.");
    for(int i=0;i<120&&!closing;i++){try{login=Bridge.Read(sky);}catch(TimeoutException){if(i>5)throw;}catch(IOException){if(i>5)throw;}if(login!=null)break;Thread.Sleep(1000);}
    if(closing)return;if(login==null)throw new IOException("Сессия не получена. Повторите после запроса игры.");
    var result=Catalog.Fetch(login,()=>closing?null:Bridge.Read(sky),body=>Catalog.Post(login,body),(n,total)=>ShowStatus("Получение дерева: "+n+" / "+total));
    if(closing||!login.Same(Bridge.Read(sky)))throw new IOException("Сессия изменилась до сохранения.");
    Catalog.Save(destination,result);ShowStatus("Готово. JSON сохранён: "+Path.GetFullPath(destination));
   }); }catch(Exception e){if(!closing)status.Text=e is IOException||e is InvalidOperationException?e.Message:"Ошибка чтения дерева. Прежний JSON сохранён.";}
   finally{busy=false;if(!closing){connect.Enabled=true;refresh.Enabled=sky!=null;}}
  }
 }
 public static class Program {
  [STAThread] public static int Main(string[] args) {
   ServicePointManager.SecurityProtocol=SecurityProtocolType.Tls12;
   string output=null;int pid=0,watch=0;bool once=false,attached=false;
   try {
    for(int i=0;i<args.Length;i++)switch(args[i]) {
     case "--output":output=Path.GetFullPath(args[++i]);break;
     case "--pid":pid=int.Parse(args[++i]);break;
     case "--once":once=true;break;
     case "--attached":attached=true;break;
     case "--watch":watch=int.Parse(args[++i]);if(watch<60)throw new ArgumentException("Интервал минимум 60 секунд.");once=true;break;
     default:throw new ArgumentException("Параметры: --once [--watch секунды] [--pid PID] [--output файл.json]");
    }
    if(output==null)output=Paths.DefaultOutput();
    if(attached&&(pid<=0||once))throw new ArgumentException("Панель мода должна запускаться загрузчиком вместе с Sky.");
    if(!once){
     bool created;using(var single=new Mutex(true,"Local\\SkySpiritPanel-"+(attached?pid:0),out created)){
      if(!created)return 0;Application.EnableVisualStyles();Application.Run(new Window(pid,output,attached));
     }return 0;
    }
    var sky=Bridge.Find(pid);Bridge.Inject(sky);
    do {
     Login login=null;for(int i=0;i<120;i++){try{login=Bridge.Read(sky);}catch(TimeoutException){if(i>5)throw;}if(login!=null)break;Thread.Sleep(1000);}
     if(login==null)throw new IOException("Нет сессии. Войдите в игру после подключения DLL.");
     var result=Catalog.Fetch(login,()=>Bridge.Read(sky),body=>Catalog.Post(login,body),(n,t)=>{});
     if(!login.Same(Bridge.Read(sky)))throw new IOException("Сессия изменилась до сохранения.");Catalog.Save(output,result);
     Console.WriteLine("Сохранён каталог: "+Path.GetFullPath(output));if(watch>0)Thread.Sleep(watch*1000);
    }while(watch>0);return 0;
   }catch(Exception e){string message=e is IOException||e is ArgumentException||e is InvalidOperationException?e.Message:"Ошибка подключения или чтения каталога.";Console.Error.WriteLine(message);if(!once)MessageBox.Show(message,"Sky Spirit Exporter");return 1;}
  }
 }
}
