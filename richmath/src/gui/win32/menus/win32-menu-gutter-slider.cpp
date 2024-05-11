#include <gui/win32/menus/win32-menu-gutter-slider.h>
#include <gui/win32/menus/win32-menu.h>
#include <gui/win32/api/win32-themes.h>
#include <gui/win32/api/win32-version.h>

#include <gui/documents.h>
#include <gui/document.h>

#include <eval/interpolation.h>

#include <commctrl.h>
#include <cmath>
#include <limits>


#ifdef _MSC_VER
namespace std {
  static bool isnan(double d) {return _isnan(d);}
}
#endif

#ifndef NAN
#  define NAN  (std::numeric_limits<double>::quiet_NaN())
#endif

using namespace richmath;

extern pmath_symbol_t richmath_System_Document;
extern pmath_symbol_t richmath_System_Inherited;
extern pmath_symbol_t richmath_System_Section;

extern pmath_symbol_t richmath_FE_ScopedCommand;

namespace richmath {
  class Win32MenuGutterSlider::Impl {
    public:
      Impl(Win32MenuGutterSlider &self);
      
      StyledObject *resolve_scope(Document *doc);
      
      bool collect_float_values(Array<float> &values, HMENU menu);
      
      RECT get_channel_rect();
      RECT get_thumb_rect();
      
      float slider_interpolation_pos_from_mouse(HMENU menu, POINT pt);
      void apply_slider_pos(HMENU menu, float rel_idx);
      void set_control_slider_pos(HMENU menu, float rel_idx);
      
    private:
      Win32MenuGutterSlider &self;
  };
}

//{ class Win32MenuGutterSlider ...

void Win32MenuGutterSlider::update_rect(HWND hwnd, HMENU menu) {
  RECT rect;
  bool valid_rect = calc_rect(rect, hwnd, menu, Win32MenuItemOverlay::OnlyGutter);
  
  //pmath_debug_print("[Win32MenuGutterSlider::update_rect LT(%d, %d) BR(%d, %d) %d x %d]\n", rect.left, rect.top, rect.right, rect.bottom, rect.right - rect.left, rect.bottom - rect.top);
  
  if(!control) {
    prepare_menu_window_for_children(hwnd);
    
    INITCOMMONCONTROLSEX icc = {sizeof(icc)};
    icc.dwICC = ICC_BAR_CLASSES;
    InitCommonControlsEx(&icc);
    
    control = CreateWindowExW(
                WS_EX_NOACTIVATE,
                TRACKBAR_CLASSW,
                L"menu item slider",
                WS_CHILD | WS_VISIBLE | TBS_VERT | TBS_BOTH | TBS_NOTICKS,
                rect.left,
                rect.top,
                rect.right - rect.left,
                rect.bottom - rect.top,
                hwnd,
                nullptr,
                nullptr,
                nullptr);
    
    if(Win32Themes::SetWindowTheme && Win32Menu::use_dark_mode)
      Win32Themes::SetWindowTheme(control, L"DarkMode", nullptr);
    
    int range_min = 0;
    int range_max = 10000;
    
    SendMessageW(control, TBM_SETRANGE, false, MAKELPARAM(range_min, range_max));
  }
  else if(valid_rect) {
    MoveWindow(
      control,
      rect.left,
      rect.top,
      rect.right - rect.left,
      rect.bottom - rect.top,
      TRUE);
    
    // StaticMenuOverride will call update_rect() again (dalayed_resize), but not initialize() again.
    initialize(hwnd, menu);
  }
}

void Win32MenuGutterSlider::initialize(HWND hwnd, HMENU menu) {
  if(!control)
    return;
  
  if(Document *doc = Menus::current_document()) {
    DWORD style = GetWindowLongW(control, GWL_STYLE);
    if(StyledObject *obj = Impl(*this).resolve_scope(doc)) {
      StyleOptionName key = StyleData::get_key(lhs);
      StyleType type = StyleData::get_type(key);
      if(type == StyleType::Number) {
        float val = obj->get_style((FloatStyleOptionName)key, NAN);
        Array<float> values;
        if(!std::isnan(val) && Impl(*this).collect_float_values(values, menu)) {
          float rel_idx = Interpolation::interpolation_index(values, val, false);
          Impl(*this).set_control_slider_pos(menu, rel_idx);
          return;
        }
      }
    }
    
    Impl(*this).set_control_slider_pos(menu, NAN);
  }
}

bool Win32MenuGutterSlider::handle_mouse_message(UINT msg, WPARAM wParam, const POINT &pt, HMENU menu) {
  
  RECT thumb_rect  = Impl(*this).get_thumb_rect();
  int thumb_center = thumb_rect.top + (thumb_rect.bottom - thumb_rect.top) / 2;
  if(pt.y == thumb_center)
    return true;
  
  float rel_idx = Impl(*this).slider_interpolation_pos_from_mouse(menu, pt);
  if(!(wParam & MK_SHIFT)) {
    int nearest = (int)(rel_idx + 0.5f);
    if(fabsf(rel_idx - nearest) < 0.25)
      rel_idx = nearest;
  }
  
  switch(msg) {
    case WM_MOUSEMOVE: 
      if(wParam & MK_LBUTTON) {
        Impl(*this).apply_slider_pos(menu, rel_idx);
      }
      break;
    
    case WM_LBUTTONDOWN: Impl(*this).apply_slider_pos(menu, rel_idx); break;
    case WM_LBUTTONUP:   Impl(*this).apply_slider_pos(menu, rel_idx); break;
  }
  
  return true;
}

//} ... class Win32MenuGutterSlider

//{ class Win32MenuGutterSlider::Impl ...

Win32MenuGutterSlider::Impl::Impl(Win32MenuGutterSlider &self)
 : self{self}
{
}

// same as MathGtkMenuSliderRegion::resolve_scope()
StyledObject *Win32MenuGutterSlider::Impl::resolve_scope(Document *doc) {
  if(self.scope == richmath_System_Document) 
    return doc;
  
  if(!doc)
    return nullptr;
  
  if(Box *sel = doc->selection_box()) {
    if(self.scope == richmath_System_Section)
      return sel->find_parent<Section>(true);
    else
      return sel;
  }
  
  return nullptr;
}

bool Win32MenuGutterSlider::Impl::collect_float_values(Array<float> &values, HMENU menu) {
  values.length(self.end_index - self.start_index + 1);
  for(int i = 0; i < values.length(); ++i) {
    MENUITEMINFOW mii = { sizeof(mii) };
    mii.fMask = MIIM_ID;
    if(!WIN32report(GetMenuItemInfoW(menu, self.start_index + i, TRUE, &mii)))
      return false;
    
    Expr cmd = Win32Menu::id_to_command(mii.wID);
    if(cmd[0] == richmath_FE_ScopedCommand)
      cmd = cmd[1];
    
    if(!cmd.is_rule())
      return false;
    
    Expr rhs = cmd[2];
    if(rhs.is_number()) {
      values[i] = rhs.to_double();
    }
    else if(rhs == richmath_System_Inherited) {
      // Magnification -> Inherited
      values[i] = 1;
    }
    else
      return false;
  }
  
  return true;
}

RECT Win32MenuGutterSlider::Impl::get_channel_rect() {
  RECT channel_rect = {};
  SendMessageW(self.control, TBM_GETCHANNELRECT, 0, (LPARAM)&channel_rect);
  // Work around windows bug (e.g. on Windows 10, 1909): TBM_GETCHANNELRECT acts as if TBS_HORZ was given
  if(channel_rect.right - channel_rect.left > channel_rect.bottom - channel_rect.top) {
    using std::swap;
    swap(channel_rect.left,  channel_rect.top);
    swap(channel_rect.right, channel_rect.bottom);
  }
  return channel_rect;
}

RECT Win32MenuGutterSlider::Impl::get_thumb_rect() {
  RECT thumb_rect = {};
  SendMessageW(self.control, TBM_GETTHUMBRECT, 0, (LPARAM)&thumb_rect);
  return thumb_rect;
}

float Win32MenuGutterSlider::Impl::slider_interpolation_pos_from_mouse(HMENU menu, POINT pt) {
  ClientToScreen(self.control, &pt);
  
  int prev_item_center = 0;
  for(int i = self.start_index; i <= self.end_index; ++i) {
    RECT item_rect;
    if(GetMenuItemRect(nullptr, menu, (UINT)i, &item_rect)) {
      int item_center = item_rect.top + (item_rect.bottom - item_rect.top) / 2;
      
      if(pt.y == item_center)
        return i - self.start_index;
      
      if(pt.y < item_center) {
        if(i == self.start_index)
          return 0.0f;
        
        if(prev_item_center >= item_center)
          return i - self.start_index;
        
        return (i - 1 - self.start_index) + (pt.y - prev_item_center) / (float)(item_center - prev_item_center);
      }
      
      prev_item_center = item_center;
    }
  }
  
  return self.end_index - self.start_index;
}

void Win32MenuGutterSlider::Impl::apply_slider_pos(HMENU menu, float rel_idx) {
  Array<float> values;
  if(!collect_float_values(values, menu)) {
    set_control_slider_pos(menu, NAN);
    return;
  }
  
  if(!(0 <= rel_idx && rel_idx <= values.length() - 1)){
    set_control_slider_pos(menu, NAN);
    return;
  }
    
  float val;
  
  int i = (int)rel_idx;
  if(rel_idx == i) {
    val = values[i];
  }
  else {
    if(i + 1 >= values.length()){
      set_control_slider_pos(menu, NAN);
      return;
    }
    
    float v0 = values[i];
    float v1 = values[i+1];
    val = v0 + (v1 - v0) * (rel_idx - i);
  }
  
  Expr cmd = Rule(self.lhs, val);
  if(self.scope)
    cmd = Call(Symbol(richmath_FE_ScopedCommand), PMATH_CPP_MOVE(cmd), self.scope);
  
  if(!Menus::run_command_now(PMATH_CPP_MOVE(cmd)))
    return;
  
  set_control_slider_pos(menu, rel_idx);
}

void Win32MenuGutterSlider::Impl::set_control_slider_pos(HMENU menu, float rel_idx) {
  DWORD style = GetWindowLongW(self.control, GWL_STYLE);  
  if(0 <= rel_idx && rel_idx <= self.end_index - self.start_index) {
    if(style & TBS_NOTHUMB)
      SetWindowLongW(self.control, GWL_STYLE, style & ~TBS_NOTHUMB);
  }
  else {
    if(!(style & TBS_NOTHUMB))
      SetWindowLongW(self.control, GWL_STYLE, style | TBS_NOTHUMB);
    
    return;
  }
  
  RECT channel_rect = get_channel_rect();
  RECT thumb_rect   = get_thumb_rect();
  int thumb_thickness = thumb_rect.bottom - thumb_rect.top;
  
  //pmath_debug_print("[channel_rect LT(%d, %d) BR(%d, %d) %d x %d]\n", channel_rect.left, channel_rect.top, channel_rect.right, channel_rect.bottom, channel_rect.right - channel_rect.left, channel_rect.bottom - channel_rect.top);
  
  MapWindowPoints(self.control, nullptr, (POINT*)&channel_rect, 2);
  
  int prev_item_index = self.start_index + (int)rel_idx;
  RECT item_rect = {};
  GetMenuItemRect(nullptr, menu, (UINT)prev_item_index, &item_rect);
  
  int prev_item_center = item_rect.top + (item_rect.bottom - item_rect.top) / 2;
  
  int final_thumb_in_menu = prev_item_center;
  if(rel_idx != (int)rel_idx) {
    int next_item_index = prev_item_index + 1;
    
    GetMenuItemRect(nullptr, menu, (UINT)next_item_index, &item_rect);
    int next_item_center = item_rect.top + (item_rect.bottom - item_rect.top) / 2;
    
    final_thumb_in_menu = prev_item_center + (int)((rel_idx - (int)rel_idx) * (next_item_center - prev_item_center));
  }
  
  int min_thumb_in_menu = channel_rect.top    + thumb_thickness / 2;
  int max_thumb_in_menu = channel_rect.bottom - thumb_thickness + thumb_thickness/2;
  
  int range_min = SendMessageW(self.control, TBM_GETRANGEMIN, 0, 0);
  int range_max = SendMessageW(self.control, TBM_GETRANGEMAX, 0, 0);
  
  if(min_thumb_in_menu <= final_thumb_in_menu && final_thumb_in_menu <= max_thumb_in_menu && min_thumb_in_menu < max_thumb_in_menu) {
    int slider_pos = range_min + MulDiv(final_thumb_in_menu - min_thumb_in_menu, range_max - range_min, max_thumb_in_menu - min_thumb_in_menu);
    
    SendMessageW(self.control, TBM_SETPOS, TRUE, (LPARAM)slider_pos);
  }
  else
    SetWindowLongW(self.control, GWL_STYLE, style | TBS_NOTHUMB);
}

//} ... class Win32MenuGutterSlider::Impl
