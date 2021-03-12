#ifndef RICHMATH__GUI__GTK__MGTK_MENU_BUILDER_H__INCLUDED
#define RICHMATH__GUI__GTK__MGTK_MENU_BUILDER_H__INCLUDED

#ifndef RICHMATH_USE_GTK_GUI
#  error this header is gtk specific
#endif

#include <pmath-cpp.h>

#include <gtk/gtk.h>

#include <util/array.h>
#include <util/frontendobject.h>

#include <gui/menus.h>


namespace richmath {
  using namespace pmath; // bad style!!!
  
  class MathGtkMenuBuilder {
      class Impl;
    public:
      MathGtkMenuBuilder();
      explicit MathGtkMenuBuilder(Expr _expr);
      
      void append_to(GtkMenuShell *menu, GtkAccelGroup *accel_group, FrontEndReference evalution_box_id);
      
      static void connect_events(GtkMenu *menu, FrontEndReference doc_id);
      static void expand_inline_lists(GtkMenu *menu, FrontEndReference id);
      
      static void collect_menu_matches(Array<MenuSearchResult> &results, String query, GtkMenuShell *menu, String prefix, FrontEndReference doc_id);
      
      static void done();
      static Expr selected_item_command();
      
    public:
      static MathGtkMenuBuilder main_menu;
      
    private:
      Expr expr;
  };
  
  class MathGtkAccelerators {
    public:
      static void load(Expr expr);
      
      static void done() {
        all_accelerators = Array<String>();
      }
      
      static void connect_all(GtkAccelGroup *accel_group, FrontEndReference evaluation_box_id);
      
    public:
      static Array<String> all_accelerators;
  };
}

#endif // RICHMATH__GUI__GTK__MGTK_MENU_BUILDER_H__INCLUDED
