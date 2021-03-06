#ifndef RICHMATH__WIN32__GUI__MENUS__WIN32_MENU_ITEM_OVERLAYS_H__INCLUDED
#define RICHMATH__WIN32__GUI__MENUS__WIN32_MENU_ITEM_OVERLAYS_H__INCLUDED

#ifndef RICHMATH_USE_WIN32_GUI
#  error this header is win32 specific
#endif

#include <util/array.h>
#include <util/base.h>
#include <util/pmath-extra.h>

namespace richmath {
  class MenuItemOverlay : public Base {
    public:
      enum Area {
        All,
        OnlyGutter,
        OnlyContentArea,
      };
    public:
      MenuItemOverlay();
      virtual ~MenuItemOverlay();
    
      void delete_all();
      static void delete_all(MenuItemOverlay *first_overlay);
      
      virtual void update_rect(HWND hwnd, HMENU menu) = 0;
      virtual void initialize(HWND hwnd, HMENU menu) {}
      virtual bool handle_mouse_message(UINT msg, WPARAM wParam, const POINT &pt, HMENU menu);
      
      void calc_rect(RECT &rect, HWND hwnd, HMENU menu, Area area);
      
    public:
      MenuItemOverlay *next;
      HWND             control;
      int              start_index;
      int              end_index;
  };
}

#endif // RICHMATH__WIN32__GUI__MENUS__WIN32_MENU_ITEM_OVERLAYS_H__INCLUDED
