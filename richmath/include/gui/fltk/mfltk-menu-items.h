#ifndef RICHMATH__GUI__FLTK__MFLTK_MENU_ITEMS_H__INCLUDED
#define RICHMATH__GUI__FLTK__MFLTK_MENU_ITEMS_H__INCLUDED

#ifndef RICHMATH_USE_FLTK_GUI
#  error This header is FLTK specific.
#endif

#include <pmath-cpp.h>

#include <util/array.h>
#include <util/frontendobject.h>
#include <util/hashtable.h>

struct Fl_Menu_Item;

namespace richmath {
  using namespace pmath; // bad style!!!
  
  class MathFltkMenuItems: public Base {
      class Impl;
    public:
      MathFltkMenuItems();
      ~MathFltkMenuItems();
      
      void reload(Expr expr);
      Fl_Menu_Item *menutable();
      
      static Expr selected_item_command();
      
      static void load_accelerators(Expr expr);
      
      static void update_states(Fl_Menu_Item *menu, FrontEndReference id);
      static void done();
      
    public:
      static MathFltkMenuItems main_menu;
      
    private:
      Array<Fl_Menu_Item> items;
  };
}

#endif // RICHMATH__GUI__FLTK__MFLTK_MENU_ITEMS_H__INCLUDED
