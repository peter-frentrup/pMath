#ifndef RICHMATH__GUI__WIN32__MENUS__WIN32_MENU_GUTTER_SLIDER_H__INCLUDED
#define RICHMATH__GUI__WIN32__MENUS__WIN32_MENU_GUTTER_SLIDER_H__INCLUDED

#ifndef RICHMATH_USE_WIN32_GUI
#  error this header is win32 specific
#endif

#include <gui/win32/menus/win32-menu-item-overlays.h>

namespace richmath {
  class Win32MenuGutterSlider : public Win32MenuItemOverlay {
      class Impl;
    public:
      virtual void update_rect(HWND hwnd, HMENU menu) override;
      virtual void initialize(HWND hwnd, HMENU menu) override;
      virtual bool handle_mouse_message(UINT msg, WPARAM wParam, const POINT &pt, HMENU menu) override;
      
    public:
      Expr lhs;
      Expr scope;
  };
}

#endif // RICHMATH__GUI__WIN32__MENUS__WIN32_MENU_GUTTER_SLIDER_H__INCLUDED
