using System;
using System.Collections.Generic;
using System.Drawing;
using System.Runtime.InteropServices;
using System.Windows.Forms;

namespace CrossNext.Preparer {
  static class DisplayDpi {
    [DllImport("user32.dll")] static extern bool SetProcessDpiAwarenessContext(IntPtr context);
    [DllImport("shcore.dll")] static extern int SetProcessDpiAwareness(int awareness);
    [DllImport("user32.dll")] static extern bool SetProcessDPIAware();
    [DllImport("user32.dll")] static extern uint GetDpiForWindow(IntPtr window);
    public static void Enable() {
      try { if(SetProcessDpiAwarenessContext((IntPtr)(-4))) return; } catch(EntryPointNotFoundException) {} catch(DllNotFoundException) {}
      try { SetProcessDpiAwareness(2); return; } catch(EntryPointNotFoundException) {} catch(DllNotFoundException) {}
      try { SetProcessDPIAware(); } catch(EntryPointNotFoundException) {}
    }
    public static int Read(Control control) {
      try { uint value=GetDpiForWindow(control.Handle); if(value>=96) return (int)value; } catch(EntryPointNotFoundException) {}
      using(Graphics graphics=control.CreateGraphics()) return Math.Max(96,(int)Math.Round(graphics.DpiX));
    }
  }

  // Reapply original logical measurements so moving between monitors cannot accumulate rounding errors.
  sealed class DpiLayout : IDisposable {
    sealed class Item {
      public Control Control;
      public Size Size,Minimum,Maximum;
      public Padding Padding,Margin;
      public float FontSize;
      public FontStyle FontStyle;
      public string FontName;
      public readonly List<ColumnStyle> Columns=new List<ColumnStyle>();
      public readonly List<RowStyle> Rows=new List<RowStyle>();
    }
    readonly List<Item> items=new List<Item>();
    // WinForms can retain an equivalent Font instance; keep each DPI's fonts alive until the form closes.
    readonly Dictionary<string,Font> fonts=new Dictionary<string,Font>();
    public float Scale { get; private set; }
    public DpiLayout(Control root) { Scale=1; Capture(root); }
    void Capture(Control control) {
      var item=new Item {Control=control,Size=control.Size,Minimum=control.MinimumSize,Maximum=control.MaximumSize,
        Padding=control.Padding,Margin=control.Margin,FontSize=control.Font.Size,FontName=control.Font.Name,FontStyle=control.Font.Style};
      var table=control as TableLayoutPanel;
      if(table!=null) {
        foreach(ColumnStyle column in table.ColumnStyles) item.Columns.Add(new ColumnStyle(column.SizeType,column.Width));
        foreach(RowStyle row in table.RowStyles) item.Rows.Add(new RowStyle(row.SizeType,row.Height));
      }
      items.Add(item);
      if(control is MonoFontList) return;
      foreach(Control child in control.Controls) Capture(child);
    }
    public int P(int value) { return (int)Math.Round(value*Scale); }
    Size Scaled(Size size) { return new Size(P(size.Width),P(size.Height)); }
    Padding Scaled(Padding value) { return new Padding(P(value.Left),P(value.Top),P(value.Right),P(value.Bottom)); }
    public void Apply(int dpi) {
      Scale=Math.Max(1,dpi/96F);
      foreach(Item item in items) item.Control.SuspendLayout();
      foreach(Item item in items) {
        Control c=item.Control;
        string key=item.FontName+"|"+item.FontSize+"|"+item.FontStyle+"|"+dpi;
        Font font;
        if(!fonts.TryGetValue(key,out font)) { font=new Font(item.FontName,item.FontSize*Scale,item.FontStyle,GraphicsUnit.Pixel); fonts.Add(key,font); }
        c.Font=font; c.Padding=Scaled(item.Padding); c.Margin=Scaled(item.Margin);
        if(!(c is Form)) { c.MinimumSize=Scaled(item.Minimum); c.MaximumSize=Scaled(item.Maximum); if(!c.AutoSize) c.Size=Scaled(item.Size); }
        var table=c as TableLayoutPanel;
        if(table!=null) {
          table.ColumnStyles.Clear(); table.RowStyles.Clear();
          foreach(ColumnStyle column in item.Columns) table.ColumnStyles.Add(new ColumnStyle(column.SizeType,column.Width*(column.SizeType==SizeType.Absolute ? Scale : 1)));
          foreach(RowStyle row in item.Rows) table.RowStyles.Add(new RowStyle(row.SizeType,row.Height*(row.SizeType==SizeType.Absolute ? Scale : 1)));
        }
        var choice=c as MonoChoice; if(choice!=null) choice.UiScale=Scale;
        var list=c as MonoFontList; if(list!=null) list.ApplyScale(Scale);
      }
      for(int n=items.Count-1;n>=0;n--) items[n].Control.ResumeLayout(true);
    }
    public void Dispose() { foreach(Font font in fonts.Values) font.Dispose(); fonts.Clear(); }
  }

  // Native disabled-button colors can disappear on the black primary surface.
  sealed class MonoButton : Button {
    public MonoButton() { SetStyle(ControlStyles.UserPaint|ControlStyles.AllPaintingInWmPaint|ControlStyles.OptimizedDoubleBuffer,true); }
    protected override void OnPaint(PaintEventArgs e) {
      bool dark=BackColor.GetBrightness()<0.25;
      e.Graphics.Clear(BackColor);
      Color ink=dark ? Color.White : Enabled ? ForeColor : Color.DimGray;
      if(FlatAppearance.BorderSize>0) using(var pen=new Pen(Enabled || dark ? ink : Color.Gray,FlatAppearance.BorderSize)) e.Graphics.DrawRectangle(pen,0,0,Width-1,Height-1);
      TextRenderer.DrawText(e.Graphics,Text,Font,ClientRectangle,ink,TextFormatFlags.HorizontalCenter|TextFormatFlags.VerticalCenter|TextFormatFlags.SingleLine|TextFormatFlags.NoPrefix);
      if(Focused) { Rectangle focus=ClientRectangle; focus.Inflate(-4,-4); ControlPaint.DrawFocusRectangle(e.Graphics,focus,ink,BackColor); }
    }
  }
}
