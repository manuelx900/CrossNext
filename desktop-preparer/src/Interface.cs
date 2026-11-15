using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Windows.Forms;

namespace CrossNext.Preparer {
  sealed class MonoProgress : Control {
    int value;
    public int Value { get { return value; } set { this.value = Math.Max(0,Math.Min(100,value)); Invalidate(); } }
    public MonoProgress() { DoubleBuffered = true; Height = 12; AccessibleRole = AccessibleRole.ProgressBar; AccessibleName = "Progress"; }
    protected override void OnPaint(PaintEventArgs e) { e.Graphics.Clear(Color.White); e.Graphics.DrawRectangle(Pens.Black,0,0,Width-1,Height-1); if(Value>0) e.Graphics.FillRectangle(Brushes.Black,3,3,Math.Max(1,(Width-6)*Value/100),Math.Max(1,Height-6)); }
  }
  sealed class MonoChoice : CheckBox {
    public float UiScale=1;
    int P(int value) { return (int)Math.Round(value*UiScale); }
    public MonoChoice() { AutoSize = false; Height = 32; BackColor = Color.White; SetStyle(ControlStyles.UserPaint | ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer,true); }
    protected override void OnPaint(PaintEventArgs e) {
      e.Graphics.Clear(BackColor); int side=Math.Min(P(18),Height-P(8)), y=(Height-side)/2;
      e.Graphics.DrawRectangle(Enabled ? Pens.Black : Pens.Gray,P(2),y,side,side);
      if(Checked) { e.Graphics.FillRectangle(Enabled ? Brushes.Black : Brushes.Gray,P(2),y,side+1,side+1); using(var pen=new Pen(Color.White,P(2))) e.Graphics.DrawLines(pen,new [] {new Point(P(6),y+P(9)),new Point(P(10),y+P(13)),new Point(P(17),y+P(5))}); }
      TextRenderer.DrawText(e.Graphics,Text,Font,new Rectangle(side+P(12),0,Width-side-P(12),Height),Enabled ? Color.Black : Color.Gray,TextFormatFlags.Left|TextFormatFlags.VerticalCenter|TextFormatFlags.NoPadding);
      if(Focused) ControlPaint.DrawFocusRectangle(e.Graphics,ClientRectangle);
    }
  }
  sealed class MonoFontList : FlowLayoutPanel {
    public sealed class Entries {
      readonly MonoFontList owner;
      public Entries(MonoFontList owner) { this.owner=owner; }
      public int Count { get { return owner.Controls.Count; } }
      public void Clear() { while(owner.Controls.Count>0) owner.Controls[0].Dispose(); }
      public void Add(object value,bool check) { var box=new MonoChoice {Text=value.ToString(),Checked=check,Margin=new Padding((int)(7*owner.uiScale),0,(int)(7*owner.uiScale),0),Tag=value,Height=(int)(34*owner.uiScale),Font=owner.Font,UiScale=owner.uiScale}; box.CheckedChanged+=delegate { if(owner.SelectionChanged!=null) owner.SelectionChanged(owner,EventArgs.Empty); }; owner.Controls.Add(box); owner.Fit(); }
    }
    public event EventHandler SelectionChanged;
    float uiScale=1;
    public readonly Entries Items;
    public IList<object> CheckedItems { get { var result=new List<object>(); foreach(MonoChoice box in Controls) if(box.Checked) result.Add(box.Tag); return result; } }
    public void SetItemChecked(int index,bool value) { ((MonoChoice)Controls[index]).Checked=value; }
    public void ShowSelection() { foreach(MonoChoice box in Controls) if(box.Checked) { ScrollControlIntoView(box); break; } }
    public MonoFontList() { Items=new Entries(this); FlowDirection=FlowDirection.TopDown; WrapContents=false; AutoScroll=true; BackColor=Color.White; BorderStyle=BorderStyle.FixedSingle; Resize+=delegate { Fit(); }; }
    public void ApplyScale(float scale) { uiScale=scale; foreach(MonoChoice box in Controls) { box.UiScale=scale; box.Font=Font; box.Height=(int)Math.Round(34*scale); box.Margin=new Padding((int)(7*scale),0,(int)(7*scale),0); } Fit(); }
    void Fit() { foreach(Control control in Controls) control.Width=Math.Max(80,ClientSize.Width-(int)Math.Round(34*uiScale)); }
  }
  partial class MainForm {
    DpiLayout dpiLayout;
    bool applyingDpi;
    Action updateAdaptiveLayout;
    float baseTextPixels;
    Action resetAdaptiveLayout=delegate {};
    [StructLayout(LayoutKind.Sequential)] struct NativeRect { public int Left,Top,Right,Bottom; }
    [StructLayout(LayoutKind.Sequential)] struct MinMaxInfo { public Point Reserved,MaxSize,MaxPosition,MinTrackSize,MaxTrackSize; }
    int P(int value) { return dpiLayout==null ? value : dpiLayout.P(value); }
    static Rectangle PreferredWindowBounds(int dpi,Rectangle work) {
      float scale=dpi/96F;
      int width=Math.Min((int)(900*scale),(int)(work.Width*0.84));
      int height=Math.Min((int)(700*scale),(int)(work.Height*0.84));
      return new Rectangle(work.X+(work.Width-width)/2,work.Y+(work.Height-height)/2,width,height);
    }
    void ApplyWindowDpi(int dpi,Rectangle work,Rectangle desired) {
      applyingDpi=true; SuspendLayout();
      try {
        dpiLayout.Apply(dpi);
        MinimumSize=Size.Empty;
        int gap=Math.Min(P(12),Math.Min(work.Width,work.Height)/20);
        Size available=new Size(Math.Max(1,work.Width-2*gap),Math.Max(1,work.Height-2*gap));
        MinimumSize=new Size(Math.Min(P(540),available.Width),Math.Min(P(360),available.Height));
        Size size=new Size(Math.Max(MinimumSize.Width,Math.Min(desired.Width,available.Width)),Math.Max(MinimumSize.Height,Math.Min(desired.Height,available.Height)));
        Bounds=new Rectangle(Math.Max(work.Left,Math.Min(desired.X,work.Right-size.Width)),Math.Max(work.Top,Math.Min(desired.Y,work.Bottom-size.Height)),size.Width,size.Height);
        MaximizedBounds=work;
      } finally { ResumeLayout(true); applyingDpi=false; }
      resetAdaptiveLayout();
      Invalidate(true);
    }
    readonly ComboBox language = new ComboBox();
    readonly Dictionary<Control,string[]> captions = new Dictionary<Control,string[]>();
    [DllImport("user32.dll")] static extern bool ReleaseCapture();
    [DllImport("user32.dll")] static extern IntPtr SendMessage(IntPtr window,int message,IntPtr first,IntPtr second);
    protected override void WndProc(ref Message message) {
      if(message.Msg==0x2e0 && dpiLayout!=null) {
        int dpi=message.WParam.ToInt32()&65535;
        NativeRect rect=(NativeRect)Marshal.PtrToStructure(message.LParam,typeof(NativeRect));
        Rectangle desired=Rectangle.FromLTRB(rect.Left,rect.Top,rect.Right,rect.Bottom);
        base.WndProc(ref message);
        ApplyWindowDpi(dpi,Screen.FromRectangle(desired).WorkingArea,desired);
        return;
      }
      base.WndProc(ref message);
      if(message.Msg==0x24 && message.LParam!=IntPtr.Zero) {
        Screen monitor=Screen.FromHandle(message.HWnd); Rectangle work=monitor.WorkingArea;
        MinMaxInfo limits=(MinMaxInfo)Marshal.PtrToStructure(message.LParam,typeof(MinMaxInfo));
        limits.MaxPosition=new Point(work.Left-monitor.Bounds.Left,work.Top-monitor.Bounds.Top);
        limits.MaxSize=new Point(work.Width,work.Height); limits.MaxTrackSize=limits.MaxSize;
        Marshal.StructureToPtr(limits,message.LParam,false);
      }
      if(message.Msg==0x84 && (int)message.Result==1 && WindowState==FormWindowState.Normal) {
        int packed=unchecked((int)message.LParam.ToInt64()); Point p=PointToClient(new Point((short)(packed&65535),(short)(packed>>16)));
        int edge=P(7); bool left=p.X<edge,right=p.X>=Width-edge,top=p.Y<edge,bottom=p.Y>=Height-edge;
        if(left&&top) message.Result=(IntPtr)13; else if(right&&top) message.Result=(IntPtr)14;
        else if(left&&bottom) message.Result=(IntPtr)16; else if(right&&bottom) message.Result=(IntPtr)17;
        else if(left) message.Result=(IntPtr)10; else if(right) message.Result=(IntPtr)11;
        else if(top) message.Result=(IntPtr)12; else if(bottom) message.Result=(IntPtr)15;
      }
    }
    Button WindowButton(string text,string name,Action action) {
      var button=new MonoButton {Text=text,AccessibleName=name,FlatStyle=FlatStyle.Flat,BackColor=Color.Black,ForeColor=Color.White,Font=UiFont(16),Size=new Size(44,36),Margin=new Padding(0),TabStop=true};
      button.FlatAppearance.BorderSize=0; button.FlatAppearance.MouseOverBackColor=Color.FromArgb(50,50,50); button.Click+=delegate { action(); }; return button;
    }
    void Caption(Control control,string spanish,string english) { captions[control]=new[]{spanish,english}; control.Text=T(spanish,english); }
    Label Heading(string spanish,string english) { var label=Label("",true); label.Margin=new Padding(0,0,0,6); Caption(label,spanish,english); return label; }
    void StyleButton(Button button,bool primary) { button.AutoSize=true; button.AutoSizeMode=AutoSizeMode.GrowAndShrink; button.FlatStyle=FlatStyle.Flat; button.FlatAppearance.BorderSize=1; button.FlatAppearance.BorderColor=Color.Black; button.Padding=new Padding(15,6,15,6); button.BackColor=primary ? Color.Black : Color.White; button.ForeColor=primary ? Color.White : Color.Black; button.UseVisualStyleBackColor=false; button.MinimumSize=new Size(95,36); }
    Panel Frame(Control content) { var panel=new Panel {Dock=DockStyle.Fill,Padding=new Padding(12),BackColor=Color.White,Margin=new Padding(0,0,0,12)}; panel.Paint+=delegate(object sender,PaintEventArgs e) { e.Graphics.DrawRectangle(Pens.Black,0,0,panel.Width-1,panel.Height-1); }; panel.Controls.Add(content); return panel; }
    static Font UiFont(float size,FontStyle style=FontStyle.Regular) { return new Font("Segoe UI",size,style,GraphicsUnit.Pixel); }
    void InitializeInterface() {
      Text="CrossNext · Book Preparer";
      baseTextPixels=Math.Max(12,SystemFonts.MessageBoxFont.SizeInPoints*96/72);
      Font=UiFont(baseTextPixels); AutoScaleMode=AutoScaleMode.None;
      BackColor=Color.White; ForeColor=Color.Black;
      FormBorderStyle=FormBorderStyle.None; Padding=new Padding(1);
      Paint+=delegate(object sender,PaintEventArgs e) { e.Graphics.DrawRectangle(Pens.Black,0,0,Width-1,Height-1); };
      Rectangle work=Screen.PrimaryScreen.WorkingArea;
      ClientSize=new Size(Math.Min(900,work.Width-24),Math.Min(700,work.Height-24));
      MinimumSize=new Size(Math.Min(820,work.Width-24),Math.Min(640,work.Height-24));
      StartPosition=FormStartPosition.CenterScreen;
      MaximizedBounds=Screen.FromHandle(Handle).WorkingArea;
      using(Stream stream=Assembly.GetExecutingAssembly().GetManifestResourceStream("crossnext.ico")) Icon=new Icon(stream);
      var root=new TableLayoutPanel {Dock=DockStyle.Fill,ColumnCount=1,RowCount=3,Margin=new Padding(0)};
      root.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,100));
      root.RowStyles.Add(new RowStyle(SizeType.Absolute,52)); root.RowStyles.Add(new RowStyle(SizeType.Percent,100)); root.RowStyles.Add(new RowStyle(SizeType.AutoSize)); Controls.Add(root);
      var header=new TableLayoutPanel {Dock=DockStyle.Fill,BackColor=Color.Black,ForeColor=Color.White,ColumnCount=3,RowCount=1,Padding=new Padding(18,6,18,6),Margin=new Padding(0)};
      header.RowStyles.Add(new RowStyle(SizeType.Percent,100));
      header.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute,56)); header.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,100)); header.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute,132));
      MouseEventHandler drag=delegate(object sender,MouseEventArgs e) { if(e.Button==MouseButtons.Left) { ReleaseCapture(); SendMessage(Handle,0xa1,(IntPtr)2,IntPtr.Zero); } };
      header.MouseDown+=drag; header.DoubleClick+=delegate { WindowState=WindowState==FormWindowState.Maximized ? FormWindowState.Normal : FormWindowState.Maximized; };
      using(Stream stream=Assembly.GetExecutingAssembly().GetManifestResourceStream("crossnext-white.png")) {
        var logo=new PictureBox {Dock=DockStyle.Fill,SizeMode=PictureBoxSizeMode.Zoom,Image=new Bitmap(Image.FromStream(stream)),Margin=new Padding(0,0,12,0),AccessibleName="CrossNext"}; logo.MouseDown+=drag; header.Controls.Add(logo,0,0);
      }
      var name=new Label {Text="CrossNext",Font=UiFont(24,FontStyle.Bold),ForeColor=Color.White,BackColor=Color.Black,Dock=DockStyle.Fill,TextAlign=ContentAlignment.MiddleLeft,Margin=new Padding(0)}; name.MouseDown+=drag; header.Controls.Add(name,1,0);
      var windowButtons=new FlowLayoutPanel {FlowDirection=FlowDirection.LeftToRight,Dock=DockStyle.Fill,WrapContents=false,Margin=new Padding(0),Padding=new Padding(0,2,0,0)};
      windowButtons.Controls.Add(WindowButton("−","Minimize",()=>WindowState=FormWindowState.Minimized));
      windowButtons.Controls.Add(WindowButton("□","Maximize",()=>WindowState=WindowState==FormWindowState.Maximized ? FormWindowState.Normal : FormWindowState.Maximized));
      windowButtons.Controls.Add(WindowButton("×","Close",()=>Close())); header.Controls.Add(windowButtons,2,0); root.Controls.Add(header,0,0);
      language.DropDownStyle=ComboBoxStyle.DropDownList; language.Items.AddRange(new object[]{"English","Español"}); language.SelectedIndex=es ? 1 : 0; language.ForeColor=Color.Black; language.BackColor=Color.White; language.AccessibleName="Language / Idioma";
      var scroll=new Panel {Dock=DockStyle.Fill,AutoScroll=true,Padding=new Padding(24,16,24,12),Margin=new Padding(0)};
      var body=new TableLayoutPanel {Dock=DockStyle.Top,AutoSize=true,AutoSizeMode=AutoSizeMode.GrowAndShrink,ColumnCount=1,Margin=new Padding(0)};
      body.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,100));
      scroll.Controls.Add(body); root.Controls.Add(scroll,0,1);
      var titleRow=new TableLayoutPanel {Dock=DockStyle.Top,AutoSize=true,ColumnCount=2}; titleRow.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,100)); titleRow.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute,150));
      var title=Heading("Preparar biblioteca","Prepare library"); title.Font=UiFont(22,FontStyle.Bold); title.Margin=new Padding(0,0,0,3); titleRow.Controls.Add(title,0,0); language.Dock=DockStyle.Fill; language.Margin=new Padding(0,0,0,4); titleRow.Controls.Add(language,1,0); body.Controls.Add(titleRow);
      var intro=Label("",false); Caption(intro,"Libros y portadas listos para seguir leyendo.","Books and covers ready for your next read."); intro.Margin=new Padding(0,0,0,14); body.Controls.Add(intro);
      var cardGrid=new TableLayoutPanel {Dock=DockStyle.Fill,ColumnCount=4,RowCount=3,AutoSize=true,Margin=new Padding(0)};
      cardGrid.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,100)); cardGrid.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute,95)); cardGrid.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize)); cardGrid.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
      cardGrid.Controls.Add(Heading("1  MicroSD","1  MicroSD"),0,0); cardGrid.SetColumnSpan(cardGrid.GetControlFromPosition(0,0),4);
      card.ReadOnly=true; card.Dock=DockStyle.Fill; card.BorderStyle=BorderStyle.FixedSingle; card.AccessibleName="MicroSD"; card.Margin=new Padding(0,6,10,0);
      drives.DropDownStyle=ComboBoxStyle.DropDownList; drives.Dock=DockStyle.Fill; drives.AccessibleName=T("Unidades extraíbles","Removable drives"); drives.Margin=new Padding(0,6,8,0);
      Caption(browse,"Elegir…","Browse…"); Caption(detect,"Actualizar","Refresh"); StyleButton(browse,false); StyleButton(detect,false); browse.Margin=new Padding(0,0,8,0); detect.Margin=new Padding(0);
      cardGrid.Controls.Add(card,0,1); cardGrid.Controls.Add(drives,1,1); cardGrid.Controls.Add(browse,2,1); cardGrid.Controls.Add(detect,3,1);
      profileText.AutoSize=true; profileText.Margin=new Padding(0,8,0,0); Caption(profileText,"Selecciona la raíz de la tarjeta, no la carpeta books.","Select the card root, rather than the books folder."); cardGrid.Controls.Add(profileText,0,2); cardGrid.SetColumnSpan(profileText,4);
      var engineRow=new TableLayoutPanel {Dock=DockStyle.Top,AutoSize=true,ColumnCount=2,Margin=new Padding(0,10,0,0)};
      engineRow.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize)); engineRow.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,100));
      var engineLabel=Heading("Motor EPUB","EPUB engine"); engineLabel.Margin=new Padding(0,4,12,0); engineRow.Controls.Add(engineLabel,0,0);
      engines.DropDownStyle=ComboBoxStyle.DropDownList; engines.Dock=DockStyle.Fill; engines.Margin=new Padding(0); engines.AccessibleName="EPUB engine / Motor EPUB"; RefreshEngineChoices(); options.Add(engines); engineRow.Controls.Add(engines,1,0);
      engines.SelectedIndexChanged+=delegate { UpdateProfileText(); };
      cardGrid.Controls.Add(engineRow,0,3); cardGrid.SetColumnSpan(engineRow,4);
      var cardFrame=Frame(cardGrid); cardFrame.AutoSize=true; cardFrame.AutoSizeMode=AutoSizeMode.GrowAndShrink; body.Controls.Add(cardFrame);
      var formatRow=new FlowLayoutPanel {Dock=DockStyle.Fill,AutoSize=true,WrapContents=true,Margin=new Padding(0)};
      var formatLabel=Heading("2  Formatos","2  Formats"); formatLabel.Margin=new Padding(0,5,16,0); formatRow.Controls.Add(formatLabel);
      foreach(var box in new [] {epub,xtc,xtch}) { box.Checked=true; box.Width=100; box.Height=32; box.Margin=new Padding(0,0,10,0); formatRow.Controls.Add(box); options.Add(box); }
      epub.Text="EPUB"; xtc.Text="XTC"; xtch.Text="XTCH";
      var formatFrame=Frame(formatRow); formatFrame.AutoSize=true; formatFrame.AutoSizeMode=AutoSizeMode.GrowAndShrink; body.Controls.Add(formatFrame);
      var fontGrid=new TableLayoutPanel {Dock=DockStyle.Fill,ColumnCount=2,RowCount=2,Height=204,Margin=new Padding(0)};
      fontGrid.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,50)); fontGrid.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,50)); fontGrid.RowStyles.Add(new RowStyle(SizeType.AutoSize)); fontGrid.RowStyles.Add(new RowStyle(SizeType.Percent,100));
      Label fontHeading=Heading("3  Fuentes","3  Fonts"), sizeHeading=Heading("4  Tamaños de letra","4  Font sizes");
      fontGrid.Controls.Add(fontHeading,0,0); fontGrid.Controls.Add(sizeHeading,1,0);
      fonts.Dock=DockStyle.Fill; fonts.Margin=new Padding(0,0,16,0); fonts.AccessibleName=T("Fuentes que se prepararán","Fonts to prepare"); options.Add(fonts); fontGrid.Controls.Add(fonts,0,1);
      var fontSettings=new TableLayoutPanel {Dock=DockStyle.Fill,ColumnCount=1,RowCount=4,Margin=new Padding(0)}; fontSettings.RowStyles.Add(new RowStyle(SizeType.AutoSize)); fontSettings.RowStyles.Add(new RowStyle(SizeType.AutoSize)); fontSettings.RowStyles.Add(new RowStyle(SizeType.AutoSize)); fontSettings.RowStyles.Add(new RowStyle(SizeType.Percent,100));
      fontSettings.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,100));
      sizes.DropDownStyle=ComboBoxStyle.DropDownList; sizes.Dock=DockStyle.Top; sizes.Margin=new Padding(0,0,0,8); sizes.AccessibleName=T("Tamaños de letra","Font sizes"); options.Add(sizes); fontSettings.Controls.Add(sizes,0,0);
      sizes.IntegralHeight=false; sizes.MaxDropDownItems=8; sizes.DropDown+=delegate { sizes.DropDownHeight=Math.Min(P(210),Screen.FromControl(sizes).WorkingArea.Height/2); };
      supportedSizes.AutoSize=true; supportedSizes.Margin=new Padding(0,0,0,8); supportedSizes.Text=T("Selecciona una fuente.","Select a font."); fontSettings.Controls.Add(supportedSizes,0,1);
      var note=Label("",false); Caption(note,"Solo se preparan tamaños compatibles con el lector. Las páginas corresponden al motor elegido; las portadas sirven para todas las apariencias.","Only reader-supported sizes are prepared. Pages use the selected engine; covers work with every appearance."); note.Dock=DockStyle.Top; note.MaximumSize=new Size(330,0); fontSettings.Controls.Add(note,0,2); fontGrid.Controls.Add(fontSettings,1,1);
      var fontFrame=Frame(fontGrid); fontFrame.Height=Math.Max(228,ClientSize.Height-640); body.Controls.Add(fontFrame);
      var warning=Label("",false); Caption(warning,"Las bibliotecas grandes pueden tardar horas. Puedes cancelar y continuar después sin perder lo preparado.","Large libraries may take hours. You can cancel and continue later without losing prepared files."); warning.MaximumSize=new Size(780,0); warning.Margin=new Padding(0,16,0,0); fontSettings.Controls.Add(warning,0,3);
      var footer=new TableLayoutPanel {Dock=DockStyle.Fill,AutoSize=true,ColumnCount=1,Padding=new Padding(24,10,24,16),Margin=new Padding(0)};
      footer.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,100));
      footer.Paint+=delegate(object sender,PaintEventArgs e) { e.Graphics.DrawLine(Pens.Black,P(20),0,footer.Width-P(20),0); };
      state.AutoSize=true; state.Font=new Font(Font,FontStyle.Bold); Caption(state,"Elige una tarjeta para comenzar","Choose a card to begin"); state.Margin=new Padding(0,0,0,3);
      currentBook.AutoSize=true; currentBook.MaximumSize=new Size(1200,0); currentBook.Margin=new Padding(0,0,0,4);
      currentBook.Visible=false; detail.Visible=false;
      progress.Dock=DockStyle.Fill; progress.Margin=new Padding(0,4,0,5); counts.AutoSize=true; counts.Margin=new Padding(0,0,0,2); detail.AutoSize=true; detail.Margin=new Padding(0,0,0,7);
      footer.Controls.Add(state); footer.Controls.Add(currentBook); footer.Controls.Add(progress); footer.Controls.Add(counts); footer.Controls.Add(detail);
      var actions=new FlowLayoutPanel {Dock=DockStyle.Fill,AutoSize=true,FlowDirection=FlowDirection.RightToLeft,Margin=new Padding(0)};
      Caption(start,"Preparar libros","Prepare books"); Caption(cancel,"Cancelar","Cancel"); StyleButton(start,true); StyleButton(cancel,false); start.MinimumSize=new Size(180,40); cancel.MinimumSize=new Size(120,40); start.Margin=new Padding(10,0,0,0); cancel.Margin=new Padding(0); start.Enabled=false; cancel.Enabled=false; actions.Controls.Add(start); actions.Controls.Add(cancel); footer.Controls.Add(actions); root.Controls.Add(footer,0,2);
      bool? narrowLayout=null;
      updateAdaptiveLayout=delegate {
        if(applyingDpi) return;
        body.Width=Math.Max(P(260),scroll.ClientSize.Width-scroll.Padding.Horizontal-P(18));
        bool narrow=body.Width<P(760);
        if(narrowLayout!=narrow) {
          narrowLayout=narrow; fontGrid.SuspendLayout(); cardGrid.SuspendLayout();
          fontGrid.ColumnCount=narrow ? 1 : 2; fontGrid.RowCount=narrow ? 4 : 2;
          fontGrid.ColumnStyles.Clear(); fontGrid.RowStyles.Clear();
          fontGrid.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,narrow ? 100 : 50));
          if(!narrow) fontGrid.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,50));
          fontGrid.RowStyles.Add(new RowStyle(SizeType.AutoSize));
          fontGrid.RowStyles.Add(new RowStyle(narrow ? SizeType.Absolute : SizeType.Percent,narrow ? P(170) : 100));
          if(narrow) { fontGrid.RowStyles.Add(new RowStyle(SizeType.AutoSize)); fontGrid.RowStyles.Add(new RowStyle(SizeType.AutoSize)); }
          fontGrid.SetCellPosition(fontHeading,new TableLayoutPanelCellPosition(0,0));
          fontGrid.SetCellPosition(fonts,new TableLayoutPanelCellPosition(0,1));
          fontGrid.SetCellPosition(sizeHeading,new TableLayoutPanelCellPosition(narrow ? 0 : 1,narrow ? 2 : 0));
          fontGrid.SetCellPosition(fontSettings,new TableLayoutPanelCellPosition(narrow ? 0 : 1,narrow ? 3 : 1));
          fonts.Margin=new Padding(0,0,narrow ? 0 : P(16),narrow ? P(12) : 0);
          cardGrid.SetColumnSpan(card,1); cardGrid.SetColumnSpan(drives,1);
          cardGrid.ColumnStyles.Clear(); cardGrid.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,narrow ? 50 : 100));
          cardGrid.ColumnStyles.Add(new ColumnStyle(narrow ? SizeType.Percent : SizeType.Absolute,narrow ? 50 : P(95)));
          cardGrid.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize)); cardGrid.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
          cardGrid.SetCellPosition(card,new TableLayoutPanelCellPosition(0,1)); cardGrid.SetColumnSpan(card,narrow ? 4 : 1);
          cardGrid.SetCellPosition(drives,new TableLayoutPanelCellPosition(narrow ? 0 : 1,narrow ? 2 : 1)); cardGrid.SetColumnSpan(drives,narrow ? 2 : 1);
          cardGrid.SetCellPosition(browse,new TableLayoutPanelCellPosition(2,narrow ? 2 : 1)); cardGrid.SetCellPosition(detect,new TableLayoutPanelCellPosition(3,narrow ? 2 : 1));
          cardGrid.SetCellPosition(profileText,new TableLayoutPanelCellPosition(0,narrow ? 3 : 2)); cardGrid.SetColumnSpan(profileText,4);
          cardGrid.SetCellPosition(engineRow,new TableLayoutPanelCellPosition(0,narrow ? 4 : 3)); cardGrid.SetColumnSpan(engineRow,4);
          fontGrid.ResumeLayout(true); cardGrid.ResumeLayout(true);
        }
        int noteWidth=Math.Max(P(100),narrow ? body.Width-P(26) : body.Width/2-P(28));
        note.MaximumSize=new Size(noteWidth,0); warning.MaximumSize=new Size(noteWidth,0); supportedSizes.MaximumSize=new Size(noteWidth,0);
        profileText.MaximumSize=new Size(Math.Max(P(200),body.Width-P(26)),0);
        title.MaximumSize=new Size(Math.Max(P(100),body.Width-P(180)),0);
        currentBook.MaximumSize=new Size(Math.Max(P(200),ClientSize.Width-P(48)),0);
        counts.MaximumSize=currentBook.MaximumSize; detail.MaximumSize=currentBook.MaximumSize; state.MaximumSize=currentBook.MaximumSize;
        int settingsHeight=sizes.PreferredHeight+P(32)+supportedSizes.GetPreferredSize(new Size(noteWidth,0)).Height+note.GetPreferredSize(new Size(noteWidth,0)).Height+warning.GetPreferredSize(new Size(noteWidth,0)).Height;
        fontFrame.Height=narrow ? P(254)+settingsHeight : Math.Max(P(200),Math.Max(P(52)+settingsHeight,Math.Min(P(330),scroll.ClientSize.Height-P(326))));
      };
      resetAdaptiveLayout=delegate { narrowLayout=null; updateAdaptiveLayout(); };
      scroll.SizeChanged+=delegate { updateAdaptiveLayout(); };
      language.SelectedIndexChanged+=delegate { es=language.SelectedIndex==1; foreach(var pair in captions) pair.Key.Text=pair.Value[es ? 0 : 1]; RefreshEngineChoices(); Text="CrossNext · "+T("Preparador de libros","Book Preparer"); if(catalogReady && worker==null) { UpdateProfileText(); if(finishResult!=null) ShowFinished(finishResult); else ShowInspection(); } if(sizes.Items.Count>=2) { RefreshSizeChoices(); } updateAdaptiveLayout(); };
      fonts.SelectionChanged+=delegate { if(catalogReady) RefreshSizeChoices(); };
      dpiLayout=new DpiLayout(this);
      int initialDpi=DisplayDpi.Read(this);
      Rectangle monitor=Screen.FromControl(this).WorkingArea;
      ApplyWindowDpi(initialDpi,monitor,PreferredWindowBounds(initialDpi,monitor));
      Shown+=delegate { updateAdaptiveLayout(); };
      FormClosed+=delegate { dpiLayout.Dispose(); };
      Text="CrossNext · "+T("Preparador de libros","Book Preparer");
    }
  }
}
