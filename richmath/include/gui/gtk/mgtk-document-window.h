#ifndef __GUI__GTK__MGTK_DOCUMENT_WINDOW_H__
#define __GUI__GTK__MGTK_DOCUMENT_WINDOW_H__

#ifndef RICHMATH_USE_GTK_GUI
  #error this header is gtk specific
#endif

#include <gui/gtk/basic-gtk-widget.h>
#include <gui/gtk/mgtk-widget.h>

namespace richmath {
  class MathGtkDock;
  class MathGtkWorkingArea;
  
  class MathGtkDocumentWindow: public BasicGtkWidget{
    protected:
      virtual void after_construction();
    
    public:
      MathGtkDocumentWindow();
      virtual ~MathGtkDocumentWindow();
      
      const String title(){ return _title; }
      void title(String text);
      
      bool is_palette(){ return _is_palette; }
      void is_palette(bool value);
      
      void run_menucommand(Expr cmd);
      
      Document *top(){          return ((MathGtkWidget*)_top_area)->document();     }
      Document *document(){     return ((MathGtkWidget*)_working_area)->document(); }
      Document *bottom(){       return ((MathGtkWidget*)_bottom_area)->document();  }
      
      MathGtkWidget *top_area(){     return (MathGtkWidget*)_top_area;     }
      MathGtkWidget *working_area(){ return (MathGtkWidget*)_working_area; }
      MathGtkWidget *bottom_area(){  return (MathGtkWidget*)_bottom_area;  }
      
      // all windows are arranged in a ring buffer:
      static MathGtkDocumentWindow *first_window();
      MathGtkDocumentWindow *prev_window(){ return _prev_window; }
      MathGtkDocumentWindow *next_window(){ return _next_window; }
      
    private:
      String _title;
      bool _is_palette;
      
      MathGtkDock        *_top_area;
      MathGtkWorkingArea *_working_area;
      MathGtkDock        *_bottom_area;
      
      MathGtkDocumentWindow *_prev_window;
      MathGtkDocumentWindow *_next_window;
      
      GtkWidget *_menu_bar;
  };
}

#endif // __GUI__GTK__MGTK_DOCUMENT_WINDOW_H__
