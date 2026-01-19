#ifndef RICHMATH__GUI__WIN32__MENUS__WIN32_MENU_TABLE_WIZARD_H__INCLUDED
#define RICHMATH__GUI__WIN32__MENUS__WIN32_MENU_TABLE_WIZARD_H__INCLUDED

#ifndef RICHMATH_USE_WIN32_GUI
#  error this header is win32 specific
#endif

#include <gui/win32/menus/win32-custom-menu-overlay.h>
#include <gui/menus.h>

namespace richmath {
  class Win32MenuTableWizard : public Win32CustomMenuOverlay {
      using base = Win32CustomMenuOverlay;
      class Impl;
    public:
      Win32MenuTableWizard(HMENU menu);
      
      static void init();
      
      static int preferred_height();
      virtual bool calc_rect(RECT &rect, HWND hwnd, HMENU menu) override;
      
      virtual bool consumes_navigation_key(DWORD keycode, HMENU menu, int sel_item) override;
      virtual bool handle_char_message(   WPARAM wParam, LPARAM lParam, HMENU menu) override;
      virtual bool handle_keydown_message(WPARAM wParam, LPARAM lParam, HMENU menu) override;
      virtual bool handle_mouse_message(UINT msg, WPARAM wParam, const POINT &pt, HMENU menu) override;
      virtual void on_mouse_leave() override;
      
    protected:
      virtual void on_paint(HDC hdc) override;
    
    private:
      HMENU menu;
      int gutter_width;
      bool shown_selected : 1;
  };
}

#endif // RICHMATH__GUI__WIN32__MENUS__WIN32_MENU_TABLE_WIZARD_H__INCLUDED
