#ifndef RICHMATH__WIN32__GUI__MENUS__WIN32_MENU_ITEM_OVERLAYS_H__INCLUDED
#define RICHMATH__WIN32__GUI__MENUS__WIN32_MENU_ITEM_OVERLAYS_H__INCLUDED

#ifndef RICHMATH_USE_WIN32_GUI
#  error this header is win32 specific
#endif

#include <util/array.h>
#include <util/base.h>
#include <util/pmath-extra.h>

namespace richmath {
  class Win32MenuItemOverlay : public Base {
    public:
      enum Area {
        All,
        OnlyGutter,
        OnlyContentArea,
      };
    public:
      Win32MenuItemOverlay();
      virtual ~Win32MenuItemOverlay();
    
      void delete_all();
      static void delete_all(Win32MenuItemOverlay *first_overlay);
      
      virtual void update_rect(HWND hwnd, HMENU menu) = 0;
      virtual void initialize(HWND hwnd, HMENU menu) {}
      
      virtual bool handle_char_message(WPARAM wParam, LPARAM lParam, HMENU menu);
      virtual bool handle_keydown_message(WPARAM wParam, LPARAM lParam, HMENU menu);
      virtual bool handle_mouse_message(UINT msg, WPARAM wParam, const POINT &pt, HMENU menu);
      
      bool calc_rect(RECT &rect, HWND hwnd, HMENU menu, Area area);
      
    protected:
      void prepare_menu_window_for_children(HWND hwnd);
      
    public:
      Win32MenuItemOverlay *next;
      HWND             control;
      int              start_index;
      int              end_index;
  };
}

#endif // RICHMATH__WIN32__GUI__MENUS__WIN32_MENU_ITEM_OVERLAYS_H__INCLUDED
