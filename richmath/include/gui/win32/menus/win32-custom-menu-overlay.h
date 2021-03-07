#ifndef RICHMATH__GUI__WIN32__MENUS__WIN32_CUSTOM_MENU_OVERLAY_H__INCLUDED
#define RICHMATH__GUI__WIN32__MENUS__WIN32_CUSTOM_MENU_OVERLAY_H__INCLUDED

#ifndef RICHMATH_USE_WIN32_GUI
#  error this header is win32 specific
#endif

#include <gui/win32/menus/win32-menu-item-overlays.h>

namespace richmath {
  class Win32CustomMenuOverlay : public Win32MenuItemOverlay {
      class Impl;
    public:
      ~Win32CustomMenuOverlay();
      
      virtual void update_rect(HWND hwnd, HMENU menu) override;
      
      virtual bool calc_rect(RECT &rect, HWND hwnd, HMENU menu);
      
      String text();
      void text(String str);
    
    protected:
      virtual bool on_create(CREATESTRUCTW *args);
      virtual void on_paint(HDC hdc) {}
      virtual LPARAM on_wndproc(UINT message, WPARAM wParam, LPARAM lParam);
  };
}

#endif // RICHMATH__GUI__WIN32__MENUS__WIN32_CUSTOM_MENU_OVERLAY_H__INCLUDED
