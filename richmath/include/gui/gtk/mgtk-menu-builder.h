#ifndef __GUI__GTK__MGTK_MENU_BUILDER_H__
#define __GUI__GTK__MGTK_MENU_BUILDER_H__

#ifndef RICHMATH_USE_GTK_GUI
#  error this header is gtk specific
#endif

#include <pmath-cpp.h>

#include <gtk/gtk.h>

#include <util/array.h>


namespace richmath {
  using namespace pmath; // bad style!!!
  
  class MathGtkMenuBuilder {
    public:
      MathGtkMenuBuilder();
      explicit MathGtkMenuBuilder(Expr _expr);
      ~MathGtkMenuBuilder();
      
      void append_to(GtkMenuShell *menu, GtkAccelGroup *accel_group, int for_document_window_id);
      
      static gboolean on_map_menu(GtkWidget *menu, GdkEventAny *event, void *dummy);
      static gboolean on_unmap_menu(GtkWidget *menu, GdkEventAny *event, void *dummy);
      
    public:
      static MathGtkMenuBuilder main_menu;
      static MathGtkMenuBuilder popup_menu;
      
    private:
      Expr expr;
  };
  
  class MathGtkAccelerators {
    public:
      static void load(Expr expr);
      
      static void done() {
        all_accelerators = Array<String>();
      }
      
      static void connect_all(GtkAccelGroup *accel_group, int document_id);
      
    public:
      static Array<String> all_accelerators;
  };
}

#endif // __GUI__GTK__MGTK_MENU_BUILDER_H__
