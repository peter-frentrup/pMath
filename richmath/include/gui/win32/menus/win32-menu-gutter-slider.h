#ifndef RICHMATH__GUI__WIN32__MENUS__WIN32_MENU_GUTTER_SLIDER_H__INCLUDED
#define RICHMATH__GUI__WIN32__MENUS__WIN32_MENU_GUTTER_SLIDER_H__INCLUDED

#include <gui/win32/menus/win32-menu-item-overlays.h>

namespace richmath {
  class MenuGutterSlider : public MenuItemOverlay {
    public:
      virtual void update_rect(HWND hwnd, HMENU menu) override;
      virtual void initialize(HWND hwnd, HMENU menu) override;
      virtual bool handle_mouse_message(UINT msg, WPARAM wParam, const POINT &pt, HMENU menu) override;
      
      bool collect_float_values(Array<float> &values, HMENU menu);
      static float interpolation_index(const Array<float> &values, float val, bool clip);
      static float interpolation_value(const Array<float> &values, float index);
      
      int slider_pos_from_point(const POINT &pt);
      void apply_slider_pos(HMENU menu, int pos);
      
    public:
      Expr lhs;
      Expr scope;
      
      static const int ScaleFactor = 100;
  };
}

#endif // RICHMATH__GUI__WIN32__MENUS__WIN32_MENU_GUTTER_SLIDER_H__INCLUDED
