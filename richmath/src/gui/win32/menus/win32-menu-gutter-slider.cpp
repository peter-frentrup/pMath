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
      
    private:
      Win32MenuGutterSlider &self;
  };
}

//{ class Win32MenuGutterSlider ...

void Win32MenuGutterSlider::update_rect(HWND hwnd, HMENU menu) {
  RECT rect;
  bool valid_rect = calc_rect(rect, hwnd, menu, Win32MenuItemOverlay::OnlyGutter);
  
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
    
    SendMessageW(control, TBM_SETRANGE, false, MAKELPARAM(0, (end_index - start_index) * ScaleFactor));
  }
  else if(valid_rect) {
    MoveWindow(
      control,
      rect.left,
      rect.top,
      rect.right - rect.left,
      rect.bottom - rect.top,
      TRUE);
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
        if(!std::isnan(val) && collect_float_values(values, menu)) {
          float rel_idx = Interpolation::interpolation_index(values, val, false);
          if(0 <= rel_idx && rel_idx <= values.length() - 1) {
            int slider_pos = (int)round(rel_idx * 100);
            SendMessageW(control, TBM_SETPOS, TRUE, (LPARAM)slider_pos);
            if(style & TBS_NOTHUMB)
              SetWindowLongW(control, GWL_STYLE, style & ~TBS_NOTHUMB);
            
            return;
          }
        }
      }
    }
    
    if(!(style & TBS_NOTHUMB))
      SetWindowLongW(control, GWL_STYLE, style | TBS_NOTHUMB);
  }
}

bool Win32MenuGutterSlider::collect_float_values(Array<float> &values, HMENU menu) {
  values.length(end_index - start_index + 1);
  for(int i = 0; i < values.length(); ++i) {
    MENUITEMINFOW mii = { sizeof(mii) };
    mii.fMask = MIIM_ID;
    if(!WIN32report(GetMenuItemInfoW(menu, start_index + i, TRUE, &mii)))
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

bool Win32MenuGutterSlider::handle_mouse_message(UINT msg, WPARAM wParam, const POINT &pt, HMENU menu) {
  
  int pos = slider_pos_from_point(pt);
//  pmath_debug_print("[mouse msg %x over menu slider: w=%x pos %d]\n", msg, wParam, pos);
  
  if(!(wParam & MK_SHIFT)) {
    int nearest = ((pos + ScaleFactor/2)/ScaleFactor) * ScaleFactor;
    if(nearest - ScaleFactor/4 < pos && pos < nearest + ScaleFactor/4)
      pos = nearest;
  }
  
  if(pos == SendMessageW(control, TBM_GETPOS, 0, 0))
    return true;
  
  switch(msg) {
    case WM_MOUSEMOVE: 
      if(wParam & MK_LBUTTON) {
        apply_slider_pos(menu, pos);
      }
      break;
    
    case WM_LBUTTONDOWN: apply_slider_pos(menu, pos); break;
    case WM_LBUTTONUP:   apply_slider_pos(menu, pos); break;
  }
  
  return true;
}

int Win32MenuGutterSlider::slider_pos_from_point(const POINT &pt) {
  RECT channel_rect = {};
  RECT thumb_rect = {};
  
  SendMessageW(control, TBM_GETCHANNELRECT, 0, (LPARAM)&channel_rect);
  // Work around windows bug (e.g. on Windows 10, 1909): TBM_GETCHANNELRECT acts as if TBS_HORZ was given
  if(channel_rect.right - channel_rect.left > channel_rect.bottom - channel_rect.top) {
    using std::swap;
    swap(channel_rect.left,  channel_rect.top);
    swap(channel_rect.right, channel_rect.bottom);
  }
  int channel_length = channel_rect.bottom - channel_rect.top;
  
  SendMessageW(control, TBM_GETTHUMBRECT, 0, (LPARAM)&thumb_rect);
  int thumb_thickness = thumb_rect.bottom - thumb_rect.top;
  
  if(channel_length > thumb_thickness) {
    int thumb_pos_px = pt.y - (channel_rect.top + thumb_thickness/2);
    if(thumb_pos_px < 0)
      thumb_pos_px = 0;
    else if(thumb_pos_px > channel_length - thumb_thickness)
      thumb_pos_px = channel_length - thumb_thickness;
    
    int range_min = 0; //SendMessageW(control, TBM_GETRANGEMIN, 0, 0);
    int range_max = (end_index - start_index) * ScaleFactor; // SendMessageW(control, TBM_GETRANGEMAX, 0, 0);
    return range_min + MulDiv(thumb_pos_px, range_max - range_min, channel_length - thumb_thickness);
  }
  else
    return 0;
}

void Win32MenuGutterSlider::apply_slider_pos(HMENU menu, int pos) {
  Array<float> values;
  if(!collect_float_values(values, menu))
    return;
  
  int i = pos / ScaleFactor;
  if(i < 0 || i >= values.length())
    return;
  
  float val;
  if(pos == i * ScaleFactor) {
    val = values[i];
  }
  else {
    if(i + 1 >= values.length())
      return;
    
    float v0 = values[i];
    float v1 = values[i+1];
    val = v0 + ((v1 - v0) * (pos - i * ScaleFactor)) / ScaleFactor;
  }
  
  Expr cmd = Rule(lhs, val);
  if(scope)
    cmd = Call(Symbol(richmath_FE_ScopedCommand), PMATH_CPP_MOVE(cmd), scope);
  
  if(!Menus::run_command_now(PMATH_CPP_MOVE(cmd)))
    return;
  
  // TBM_SETPOSNOTIFY exists since Windows 7
  SendMessageW(control, TBM_SETPOS, TRUE, (LPARAM)pos);
  
  DWORD style = GetWindowLongW(control, GWL_STYLE);
  if(style & TBS_NOTHUMB)
    SetWindowLongW(control, GWL_STYLE, style & ~TBS_NOTHUMB);
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

//} ... class Win32MenuGutterSlider::Impl
