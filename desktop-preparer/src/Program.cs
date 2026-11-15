using System;
using System.Collections;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.Globalization;
using System.IO;
using System.IO.Compression;
using System.Reflection;
using System.Security.Cryptography;
using System.Text;
using System.Web.Script.Serialization;
using System.Windows.Forms;
using System.Threading;

namespace CrossNext.Preparer {
  static class Program {
    [STAThread] static void Main(string[] args) {
      DisplayDpi.Enable();
      Application.EnableVisualStyles();
      Application.SetCompatibleTextRenderingDefault(false);
      try {
#if PREPARER_TEST
        DpiTestTrace.Write(args,"Unpacking runtime");
#endif
        string runtime=RuntimeFiles.Unpack();
#if PREPARER_TEST
        DpiTestTrace.Write(args,"Creating window");
#endif
        Application.Run(new MainForm(runtime, args));
      }
      catch (Exception error) {
#if PREPARER_TEST
        int output=Array.IndexOf(args,"--layout-output");
        if(output>=0 && output+1<args.Length) { Directory.CreateDirectory(args[output+1]); File.WriteAllText(Path.Combine(args[output+1],"failure.txt"),error.ToString()); return; }
#endif
        MessageBox.Show(error.Message, "CrossNext", MessageBoxButtons.OK, MessageBoxIcon.Error);
      }
    }
  }
  static class RuntimeFiles {
    public static string Unpack() {
      using (Stream resource = Assembly.GetExecutingAssembly().GetManifestResourceStream("runtime.zip")) {
        string hash;
        using (SHA256 sha = SHA256.Create()) hash = BitConverter.ToString(sha.ComputeHash(resource)).Replace("-", "").Substring(0, 20);
        resource.Position = 0;
        string root = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "CrossNext", "Preparer", hash);
        Directory.CreateDirectory(root);
        CheckLinks(root);
        using(var gate=new Mutex(false,"Local\\CrossNextPreparerRuntime-"+hash)) {
          try { gate.WaitOne(); } catch(AbandonedMutexException) {}
          try {
        using (var archive = new ZipArchive(resource, ZipArchiveMode.Read)) {
          foreach (ZipArchiveEntry entry in archive.Entries) {
            string target = Path.GetFullPath(Path.Combine(root, entry.FullName));
            if (!target.StartsWith(root + Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase)) throw new IOException("Invalid runtime archive.");
            Directory.CreateDirectory(Path.GetDirectoryName(target));
            CheckLinks(target);
            byte[] expected;
            using (SHA256 sha = SHA256.Create()) using (Stream input = entry.Open()) expected = sha.ComputeHash(input);
            if (File.Exists(target)) {
              byte[] actual;
              using (SHA256 sha = SHA256.Create()) using (Stream input = File.OpenRead(target)) actual = sha.ComputeHash(input);
              if (Convert.ToBase64String(expected) == Convert.ToBase64String(actual)) continue;
            }
            string temporary = target + ".new";
            CheckLinks(temporary);
            using (Stream input = entry.Open()) using (Stream output = File.Create(temporary)) input.CopyTo(output);
            if (File.Exists(target)) File.Delete(target);
            File.Move(temporary, target);
          }
        }
          } finally { gate.ReleaseMutex(); }
        }
        return root;
      }
    }
    static void CheckLinks(string target) {
      string current = Path.GetFullPath(target);
      while (!String.IsNullOrEmpty(current)) {
        if ((File.Exists(current) || Directory.Exists(current)) && (File.GetAttributes(current) & FileAttributes.ReparsePoint) != 0)
          throw new IOException("Linked runtime folders are not supported.");
        current = Path.GetDirectoryName(current);
      }
    }
  }
  partial class MainForm : Form {
    readonly string runtime;
    readonly JavaScriptSerializer json = new JavaScriptSerializer();
    bool es;
    readonly TextBox card = new TextBox();
    readonly Button browse = new MonoButton(), detect = new MonoButton(), start = new MonoButton(), cancel = new MonoButton();
    readonly ComboBox drives = new ComboBox(), sizes = new ComboBox(), engines = new ComboBox();
    readonly MonoFontList fonts = new MonoFontList();
    readonly MonoChoice epub = new MonoChoice(), xtc = new MonoChoice(), xtch = new MonoChoice();
    readonly Label profileText = new Label(), state = new Label(), currentBook = new Label(), counts = new Label(), detail = new Label(), supportedSizes = new Label();
    readonly MonoProgress progress = new MonoProgress();
    readonly List<Control> options = new List<Control>();
    Process worker;
    string cancellation, configPath;
    Dictionary<string, object> profile;
    readonly Dictionary<string,SortedSet<int>> fontSizes = new Dictionary<string,SortedSet<int>>();
    bool preparing, stopping, catalogReady;
    int readerSize;
    string savedFont, requestedFont;
    bool fontMissing;
    Dictionary<string,object> finishResult;
    string diagnosticFolder;
    bool bookFailures;
    readonly ToolTip tips = new ToolTip();
    string T(string spanish, string english) { return es ? spanish : english; }
    public MainForm(string runtime, string[] args) {
      this.runtime = runtime;
      es = Array.IndexOf(args, "--es") >= 0;
      InitializeInterface();
#if PREPARER_TEST
      DpiTestTrace.Write(args,"Interface initialized");
      if(InitializeDpiTest(args)) return;
      int sizeArg=Array.IndexOf(args,"--test-size");
      if(sizeArg>=0 && sizeArg+1<args.Length) { string[] parts=args[sizeArg+1].Split('x'); ClientSize=new Size(Int32.Parse(parts[0]),Int32.Parse(parts[1])); }
#endif
      options.Add(browse); options.Add(drives); options.Add(detect); options.Add(language);
      browse.Click += delegate { using(var dialog = new FolderBrowserDialog { Description = T("Selecciona la raíz de la microSD", "Select the microSD root"), ShowNewFolderButton = false }) if(dialog.ShowDialog(this) == DialogResult.OK) Inspect(dialog.SelectedPath); };
      detect.Click += delegate { DetectDrives(); if(!String.IsNullOrWhiteSpace(card.Text)) Inspect(card.Text); };
      drives.SelectionChangeCommitted += delegate { if(drives.SelectedItem != null) Inspect(drives.SelectedItem.ToString()); };
      start.Click += delegate { StartPreparation(); };
      cancel.Click += delegate { RequestCancel(); };
      FormClosing += delegate(object sender, FormClosingEventArgs e) { if(worker != null && !worker.HasExited) { e.Cancel = true; if(preparing) RequestCancel(); } };
      Shown += delegate { DetectDrives(); int index = Array.IndexOf(args,"--card"); if(index >= 0 && index + 1 < args.Length) Inspect(args[index+1]); };
      tips.SetToolTip(card,T("La ruta completa de la microSD seleccionada.", "Full path of the selected microSD."));
#if PREPARER_TEST
      int shot = Array.IndexOf(args,"--test-output");
      if(shot >= 0 && shot + 1 < args.Length) {
        ShowInTaskbar=false; Opacity=0;
        string output = args[shot+1]; Directory.CreateDirectory(output); int ticks = 0, step = 0;
        var timer = new System.Windows.Forms.Timer { Interval = 250 };
        timer.Tick += delegate {
          try {
            if(++ticks > 240) throw new Exception("GUI test timed out: " + state.Text);
            if(step == 0 && catalogReady && worker == null) {
              if(state.Text.StartsWith("Error:")) throw new Exception(state.Text);
              int engineArg=Array.IndexOf(args,"--test-engine");
              if(engineArg>=0) engines.SelectedIndex=Int32.Parse(args[engineArg+1]);
              var sizeChoices=new List<string>(); foreach(object item in sizes.Items) sizeChoices.Add(item.ToString());
              var selection=new Dictionary<string,object> {{"fonts",fonts.CheckedItems},{"size",sizes.SelectedItem},{"sizeChoices",sizeChoices},{"supportedSizes",supportedSizes.Text},{"engine",engines.SelectedIndex},{"availableFonts",fonts.Items.Count},{"width",Width},{"height",Height},{"fontHeight",Font.Height}};
              File.WriteAllText(Path.Combine(output,"selection.json"),json.Serialize(selection));
              using(var bitmap = new Bitmap(Width,Height)) { DrawToBitmap(bitmap,new Rectangle(Point.Empty,Size)); bitmap.Save(Path.Combine(output,"ready.png")); }
              if(!start.Enabled || fonts.Items.Count < 1) throw new Exception("Preparation controls unavailable.");
              if(Array.IndexOf(args,"--test-inspect-only")>=0) { timer.Stop(); Close(); return; }
              step = 1; start.PerformClick();
            } else if(step == 1 && worker == null && !preparing) {
              using(var bitmap = new Bitmap(Width,Height)) { DrawToBitmap(bitmap,new Rectangle(Point.Empty,Size)); bitmap.Save(Path.Combine(output,"finished.png")); }
              File.WriteAllText(Path.Combine(output,"result.txt"),state.Text + "\n" + counts.Text + "\n" + detail.Text);
              File.WriteAllText(Path.Combine(output,"engine.json"),json.Serialize(profile["epubEngine"]));
              if(finishResult==null || state.Text.StartsWith("Error:")) throw new Exception(state.Text);
              timer.Stop(); Close();
            }
          } catch(Exception error) { File.WriteAllText(Path.Combine(output,"failure.txt"),error.ToString()); Environment.Exit(1); }
        };
        timer.Start();
      }
#endif
    }
    Label Label(string text, bool bold) { return new Label { Text = text, AutoSize = true, Font = bold ? new Font(Font,FontStyle.Bold) : Font, Margin = new Padding(0,0,0,3) }; }
    void DetectDrives() { drives.Items.Clear(); foreach(var drive in DriveInfo.GetDrives()) try { if(drive.IsReady && drive.DriveType == DriveType.Removable) drives.Items.Add(drive.RootDirectory.FullName); } catch(IOException) {} }
    void Inspect(string folder) {
      if(worker != null && !worker.HasExited) return;
      card.Text = folder; catalogReady = false; fonts.Items.Clear(); sizes.Items.Clear(); start.Enabled = false;
      finishResult=null; progress.Value=0; currentBook.Visible=false; detail.Visible=false; counts.Text="";
      state.Text = T("Leyendo ajustes y fuentes…", "Reading settings and fonts…");
      Launch("inspect",new Dictionary<string,object> { {"card", folder} });
    }
    void StartPreparation() {
      if(!catalogReady || fonts.CheckedItems.Count == 0) { MessageBox.Show(T("Selecciona al menos una fuente.","Select at least one font.")); return; }
      var formats = new List<string>(); if(epub.Checked) formats.Add("epub"); if(xtc.Checked) formats.Add("xtc"); if(xtch.Checked) formats.Add("xtch");
      if(formats.Count == 0) { MessageBox.Show(T("Selecciona al menos un formato.","Select at least one format.")); return; }
      var selected = new List<string>(); foreach(var item in fonts.CheckedItems) selected.Add(item.ToString());
      object sizeChoice = sizes.SelectedIndex == 1 ? (object)"all" : sizes.SelectedIndex <= 0 ? (object)"current" : new [] { Int32.Parse(sizes.SelectedItem.ToString().Split(' ')[0]) };
      var configuration = new Dictionary<string,object> { {"card",card.Text}, {"fonts",selected}, {"formats",formats}, {"sizes",sizeChoice}, {"profile",new Dictionary<string,object> {{"epubEngine",engines.SelectedIndex}}} };
      preparing = true; stopping = false; finishResult=null; progress.Value=0; Launch("run",configuration);
    }
    void RequestCancel() { if(stopping) return; stopping = true; if(cancellation != null) File.WriteAllText(cancellation, "cancel"); cancel.Enabled = false; state.Text = T("Cancelando al terminar el paso actual…", "Cancelling after the current step…"); }
    void Launch(string mode, Dictionary<string,object> configuration) {
      string jobFolder = Path.Combine(Path.GetTempPath(),"CrossNext-job-" + Guid.NewGuid().ToString("N")); Directory.CreateDirectory(jobFolder);
      diagnosticFolder=jobFolder; bookFailures=false;
      configPath = Path.Combine(jobFolder,"job.json"); cancellation = Path.Combine(jobFolder,"cancel"); configuration["cancelFile"] = cancellation;
      File.WriteAllText(configPath,json.Serialize(configuration),new UTF8Encoding(false));
      foreach(Control control in options) control.Enabled = false; start.Enabled = false; cancel.Enabled = preparing;
      worker = new Process { StartInfo = new ProcessStartInfo(Path.Combine(runtime,"node.exe"), "\"" + Path.Combine(runtime,"worker.cjs") + "\" " + mode + " \"" + configPath + "\"") { UseShellExecute = false, CreateNoWindow = true, RedirectStandardOutput = true, RedirectStandardError = true, StandardOutputEncoding = Encoding.UTF8, WorkingDirectory = runtime }, EnableRaisingEvents = true };
      Process launched=worker;
      worker.OutputDataReceived += delegate(object sender, DataReceivedEventArgs e) { if(e.Data != null && !IsDisposed) BeginInvoke((Action)(() => HandleEvent(e.Data,mode))); };
      worker.ErrorDataReceived += delegate(object sender, DataReceivedEventArgs e) { if(e.Data != null) File.AppendAllText(Path.Combine(jobFolder,"error.log"), e.Data + Environment.NewLine); };
      worker.Exited += delegate { launched.WaitForExit(); if(!IsDisposed) BeginInvoke((Action)(() => {
        bool failed=launched.ExitCode!=0;
        foreach(Control control in options) control.Enabled=true; cancel.Enabled=false; preparing=false;
        if(failed) {
          catalogReady=false;
          if(!state.Text.StartsWith("Error:")) state.Text=T("Error: no se pudo completar. Vuelve a elegir la tarjeta.","Error: unable to finish. Select the card again.");
          detail.Visible=true; detail.Text=T("Diagnóstico: ","Diagnostic: ")+jobFolder; tips.SetToolTip(detail,jobFolder);
        }
        start.Enabled=catalogReady; launched.Dispose(); worker=null;
        if(!failed && !bookFailures) try { Directory.Delete(jobFolder,true); } catch(IOException) {}
      })); };
      try { worker.Start(); worker.BeginOutputReadLine(); worker.BeginErrorReadLine(); } catch(Exception error) { state.Text = "Error: " + error.Message; preparing = false; worker.Dispose(); worker=null; foreach(Control control in options) control.Enabled = true; }
    }
    void RefreshSizeChoices() {
      string previous=sizes.SelectedIndex>1 ? Convert.ToString(sizes.SelectedItem) : null;
      bool all=sizes.SelectedIndex==1;
      SortedSet<int> common=null;
      foreach(object item in fonts.CheckedItems) {
        SortedSet<int> points;
        if(!fontSizes.TryGetValue(item.ToString(),out points)) continue;
        if(common==null) common=new SortedSet<int>(points); else common.IntersectWith(points);
      }
      sizes.Items.Clear(); sizes.Items.Add(T("Tamaño del lector: ","Reader size: ")+readerSize+" pt");
      sizes.Items.Add(T("Todos los tamaños compatibles por fuente","All supported sizes per font"));
      if(common!=null) foreach(int point in common) sizes.Items.Add(point+" pt");
      sizes.SelectedIndex=all ? 1 : previous!=null && sizes.Items.Contains(previous) ? sizes.Items.IndexOf(previous) : 0;
      SortedSet<int> same=null; bool identical=true;
      foreach(object item in fonts.CheckedItems) {
        SortedSet<int> points; if(!fontSizes.TryGetValue(item.ToString(),out points)) continue;
        if(same==null) same=points; else if(!same.SetEquals(points)) identical=false;
      }
      supportedSizes.Text=same==null ? T("Selecciona una fuente.","Select a font.") : identical
        ? T("Tamaños del lector: ","Reader-supported sizes: ")+FormatPointSizes(same)
        : T("Cada fuente utiliza solo sus tamaños compatibles.","Each font uses only its supported sizes.");
      if(catalogReady) start.Enabled=worker==null && fonts.CheckedItems.Count>0;
      if(updateAdaptiveLayout!=null) updateAdaptiveLayout();
    }
    static string FormatPointSizes(SortedSet<int> points) {
      var groups=new List<string>(); int first=-1,last=-1;
      foreach(int point in points) {
        if(first<0) { first=last=point; continue; }
        if(point==last+1) { last=point; continue; }
        groups.Add(first==last ? first.ToString() : first+"–"+last); first=last=point;
      }
      if(first>=0) groups.Add(first==last ? first.ToString() : first+"–"+last);
      return String.Join(", ",groups)+" pt";
    }
    void RefreshEngineChoices() {
      int selected=Math.Max(0,engines.SelectedIndex);
      engines.Items.Clear(); engines.Items.Add(T("Nuevo","Modern")); engines.Items.Add(T("Clásico (antiguo)","Classic (legacy)")); engines.SelectedIndex=selected;
    }
    void UpdateProfileText() {
      if(profile==null) return;
      string engine=engines.SelectedIndex==1 ? T("Clásico","Classic") : T("Nuevo","Modern");
      profileText.Text=T("Motor EPUB: ","EPUB engine: ")+engine+" · "+savedFont+" · "+readerSize+" pt";
    }
    void ShowInspection() {
      state.Text=T("Listo para preparar","Ready to prepare");
      counts.Text=fontMissing
        ? T("La fuente guardada no está instalada: ","Saved font is not installed: ")+requestedFont+T(". Se ha seleccionado Ubuntu.",". Ubuntu has been selected.")
        : fonts.Items.Count+T(" fuentes disponibles. Ajustes del lector seleccionados."," fonts available. Reader settings selected.");
    }
    void ShowFinished(Dictionary<string,object> message) {
      bool cancelled=Convert.ToBoolean(message["cancelled"]);
      state.Text=cancelled ? T("Cancelado. Puedes continuar más adelante.","Cancelled. You can continue later.") : T("Preparación terminada","Preparation complete");
      if(!cancelled) progress.Value=100;
      counts.Text=message["completed"]+" / "+message["total"]+T(" preparados"," prepared")+" · "+message["failed"]+T(" con errores"," with errors");
      if(message.ContainsKey("alreadyPrepared") && Convert.ToInt32(message["alreadyPrepared"])>0)
        counts.Text+=" · "+message["alreadyPrepared"]+T(" ya preparados"," already prepared");
      detail.Visible=true; detail.Text=T("Expulsa la microSD con Windows antes de retirarla.","Safely eject the microSD in Windows before removing it.");
      if(Convert.ToInt32(message["failed"])>0) {
        bookFailures=true; state.Text=T("Preparación terminada con errores","Preparation finished with errors");
        detail.Text=T("Informe: ","Report: ")+diagnosticFolder; tips.SetToolTip(detail,diagnosticFolder);
      }
    }
    void HandleEvent(string line, string mode) {
      Dictionary<string,object> message;
      try { message = json.Deserialize<Dictionary<string,object>>(line); } catch { return; }
      string type = Convert.ToString(message["type"]);
      if(type == "catalog") {
        profile = (Dictionary<string,object>)message["profile"];
        savedFont=Convert.ToString(message["selectedFont"]); readerSize=Convert.ToInt32(message["selectedSize"]);
        UpdateProfileText();
        if(mode == "inspect") {
          engines.SelectedIndex=Convert.ToInt32(profile["epubEngine"]);
          string selected=Convert.ToString(message["selectedFont"]); readerSize=Convert.ToInt32(message["selectedSize"]);
          fontSizes.Clear(); fonts.Items.Clear();
          foreach(Dictionary<string,object> family in (ArrayList)message["fonts"]) {
            string name=Convert.ToString(family["name"]); var points=new SortedSet<int>();
            foreach(object point in (ArrayList)family["sizes"]) points.Add(Convert.ToInt32(point));
            fontSizes[name]=points; fonts.Items.Add(name,name==selected);
          }
          RefreshSizeChoices();
          catalogReady=true; fontMissing=Convert.ToBoolean(message["fontMissing"]); requestedFont=Convert.ToString(message["requestedFont"]);
          ShowInspection(); BeginInvoke((Action)(() => fonts.ShowSelection()));
        }
      } else if(type == "checking") {
        state.Text=T("Comprobando libros preparados…","Checking prepared books…");
        counts.Text=Convert.ToString(message["total"])+T(" archivos encontrados"," files found");
      } else if(type == "scanned") { counts.Text = "0 / " + message["total"] + T(" archivos pendientes"," pending files"); }
      else if(type == "progress") {
        currentBook.Visible=true; detail.Visible=true;
        if(!stopping) state.Text = T("Preparando libros…","Preparing books…");
        currentBook.Text = Convert.ToString(message["book"]); tips.SetToolTip(currentBook,currentBook.Text);
        int percent = Convert.ToInt32(message["percent"]); progress.Value = Math.Max(0,Math.Min(100,percent));
        counts.Text = message["completed"] + " / " + message["total"] + T(" archivos"," files") + " · " + percent + "% " + T("del libro actual","of current book");
        detail.Text = message["font"] + " · " + message["size"] + " pt" + (message.ContainsKey("chapters") && Convert.ToInt32(message["chapters"]) > 0 ? " · " + T("Capítulo ","Chapter ") + message["chapter"] + " / " + message["chapters"] : "");
      } else if(type == "finished") {
        finishResult=message; ShowFinished(message);
      } else if(type == "bookError" || type == "diagnostic") {
        File.AppendAllText(Path.Combine(diagnosticFolder,"report.jsonl"),line+Environment.NewLine,Encoding.UTF8);
      } else if(type == "error") { state.Text = "Error: " + message["message"]; currentBook.Text = T("Comprueba la tarjeta y vuelve a intentarlo.","Check the card and try again."); }
    }
  }
}
