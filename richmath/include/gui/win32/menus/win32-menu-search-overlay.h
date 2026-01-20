#ifndef RICHMATH__GUI__WIN32__MENUS__WIN32_MENU_SEARCH_OVERLAY_H__INCLUDED
#define RICHMATH__GUI__WIN32__MENUS__WIN32_MENU_SEARCH_OVERLAY_H__INCLUDED

#ifndef RICHMATH_USE_WIN32_GUI
#  error this header is win32 specific
#endif

#include <gui/win32/menus/win32-custom-menu-overlay.h>
#include <gui/menus.h>

namespace richmath {
  class Win32MenuSearchOverlay : public Win32CustomMenuOverlay {
      using base = Win32CustomMenuOverlay;
      class Impl;
    public:
      Win32MenuSearchOverlay(HMENU menu);
      
      static void init();
      
      static void collect_menu_matches(Array<MenuSearchResult> &results, String query, HMENU menu, String prefix);
      
      virtual bool calc_rect(RECT &rect, HWND hwnd, HMENU menu) override;
      
      virtual bool handle_char_message(   WPARAM wParam, LPARAM lParam, HMENU menu) override;
      virtual bool handle_keydown_message(WPARAM wParam, LPARAM lParam, HMENU menu) override;
      virtual bool handle_mouse_message(UINT msg, WPARAM wParam, const POINT &pt, HMENU menu) override;
      virtual void on_mouse_leave() override;
      
    protected:
      virtual bool on_create(CREATESTRUCTW *args) override;
      virtual void on_paint(HDC hdc) override;
      virtual bool on_timer(UINT_PTR id, TIMERPROC proc) override;
    
    private:
      HMENU menu;
      int glyph_x;
      int glyph_width;
      int content_x;
      bool hide_caret: 1;
      bool over_cancel_button: 1;
      bool pressing_cancel_button: 1;
  };
}

#endif // RICHMATH__GUI__WIN32__MENUS__WIN32_MENU_SEARCH_OVERLAY_H__INCLUDED
