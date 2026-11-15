#if PREPARER_TEST
using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using System.Windows.Forms;

namespace CrossNext.Preparer {
  static class DpiTestTrace {
    public static void Write(string[] args,string message) {
      int at=Array.IndexOf(args,"--layout-output"); if(at<0 || at+1>=args.Length) return;
      Directory.CreateDirectory(args[at+1]); File.AppendAllText(Path.Combine(args[at+1],"trace.txt"),message+Environment.NewLine);
    }
  }
  partial class MainForm {
    bool InitializeDpiTest(string[] args) {
      int at=Array.IndexOf(args,"--layout-output");
      if(at<0 || at+1>=args.Length) return false;
      string output=args[at+1]; Directory.CreateDirectory(output);
      ShowInTaskbar=false; Opacity=0;
      Shown+=delegate {
        BeginInvoke((Action)(() => {
          try {
            int dpi=Int32.Parse(args[Array.IndexOf(args,"--layout-dpi")+1]);
            string[] dimensions=args[Array.IndexOf(args,"--layout-work")+1].Split('x');
            var work=new Rectangle(0,0,Int32.Parse(dimensions[0]),Int32.Parse(dimensions[1]));
            card.Text="E:/"; profileText.Text="EPUB engine: Modern · PT Serif · 14 pt";
            for(int i=0;i<40;i++) {
              string name="Custom reading font "+i; var points=new SortedSet<int>();
              for(int size=8;size<=22;size++) points.Add(size);
              fontSizes[name]=points; fonts.Items.Add(name,i==37);
            }
            readerSize=14; RefreshSizeChoices();
            for(int step=0;step<3;step++) {
              int current=step==0 ? 96 : step==1 ? 192 : dpi;
              ApplyWindowDpi(current,work,PreferredWindowBounds(current,work));
              PerformLayout();
            }
            fonts.ShowSelection(); state.Text=T("Listo para preparar","Ready to prepare"); counts.Text=T("40 fuentes disponibles. Ajustes del lector seleccionados.","40 fonts available. Reader settings selected.");
            start.Enabled=true; cancel.Enabled=false;
            SaveLayoutShot(output,"ready");
            foreach(Control control in options) control.Enabled=false;
            browse.Enabled=detect.Enabled=drives.Enabled=language.Enabled=false;
            start.Enabled=false; cancel.Enabled=true;
            state.Text=T("Preparando libros…","Preparing books…");
            currentBook.Visible=detail.Visible=true;
            currentBook.Text=T("/Biblioteca/Ciencia/Un título de libro largo para verificar el ajuste de la ventana.epub","/Library/Science/A long book title to verify window reflow and progress display.epub");
            counts.Text=T("5 / 786 archivos · 42% del libro actual","5 / 786 files · 42% of current book");
            detail.Text=T("PT Serif · 14 pt · Capítulo 8 / 25","PT Serif · 14 pt · Chapter 8 / 25"); progress.Value=42;
            PerformLayout(); updateAdaptiveLayout(); PerformLayout();
            Rectangle button=RectangleToClient(start.RectangleToScreen(start.ClientRectangle));
            Rectangle cancelBox=RectangleToClient(cancel.RectangleToScreen(cancel.ClientRectangle));
            if(!ClientRectangle.Contains(button) || !ClientRectangle.Contains(cancelBox)) throw new Exception("Footer actions are clipped: "+button+" / "+ClientRectangle);
            if(!work.Contains(Bounds)) throw new Exception("Window exceeds work area: "+Bounds+" / "+work);
            ValidateLayout(this);
            using(var image=new Bitmap(start.Width,start.Height)) {
              start.DrawToBitmap(image,start.ClientRectangle);
              int white=0;
              for(int y=8;y<image.Height-8;y++) for(int x=8;x<image.Width-8;x++) { Color c=image.GetPixel(x,y); if(c.R>230 && c.G>230 && c.B>230) ++white; }
              if(white<20) throw new Exception("Disabled primary-button text is invisible.");
              image.Save(Path.Combine(output,"disabled-primary.png"));
            }
            SaveLayoutShot(output,"busy");
            var metrics=new Dictionary<string,object> {{"dpi",dpi},{"width",Width},{"height",Height},{"fontPixels",Font.Size},{"baseFontPixels",baseTextPixels},{"button",button.ToString()},{"work",work.ToString()},{"passed",true}};
            File.WriteAllText(Path.Combine(output,"result.json"),json.Serialize(metrics));
          } catch(Exception error) { File.WriteAllText(Path.Combine(output,"failure.txt"),error.ToString()); }
          Close();
        }));
      };
      return true;
    }
    void SaveLayoutShot(string output,string name) {
      PerformLayout();
      using(var image=new Bitmap(Width,Height)) { DrawToBitmap(image,new Rectangle(Point.Empty,Size)); image.Save(Path.Combine(output,name+".png")); }
    }
    void ValidateLayout(Control parent) {
      foreach(Control child in parent.Controls) {
        if(!child.Visible) continue;
        if(child.Left<0 || child.Right>parent.ClientSize.Width+1) throw new Exception("Horizontal overflow: "+child.GetType().Name+" "+child.Text+" "+child.Bounds+" / "+parent.ClientRectangle);
        var button=child as Button;
        if(button!=null && child.Text.Length>1) {
          Size text=TextRenderer.MeasureText(child.Text,child.Font);
          if(text.Width+8>child.Width || text.Height>child.Height) throw new Exception("Button text clipped: "+child.Text);
        }
        ValidateLayout(child);
      }
    }
  }
}
#endif
