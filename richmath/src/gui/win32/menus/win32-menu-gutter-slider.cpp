#include <gui/win32/menus/win32-menu-gutter-slider.h>
#include <gui/win32/menus/win32-menu.h>

#include <gui/documents.h>
#include <gui/document.h>

#include <commctrl.h>

#include <algorithm>
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

//{ class MenuGutterSlider ...

void MenuGutterSlider::update_rect(HWND hwnd, HMENU menu) {
  RECT rect;
  calc_rect(rect, hwnd, menu, MenuItemOverlay::OnlyGutter);
  
  if(!control) {
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
    
    SendMessageW(control, TBM_SETRANGE, false, MAKELPARAM(0, (end_index - start_index) * ScaleFactor));
    
    DWORD hwnd_exstyle = GetWindowLongW(hwnd, GWL_EXSTYLE);
    DWORD hwnd_style   = GetWindowLongW(hwnd, GWL_STYLE);
    pmath_debug_print("[menu hwnd style 0x%x  exstyle 0x%x]", hwnd_style, hwnd_exstyle);
    hwnd_style |= WS_CLIPCHILDREN;
    
    SetWindowLongW(hwnd, GWL_STYLE, hwnd_style);
  }
  else {
    MoveWindow(
      control,
      rect.left,
      rect.top,
      rect.right - rect.left,
      rect.bottom - rect.top,
      TRUE);
  }
}

void MenuGutterSlider::initialize(HWND hwnd, HMENU menu) {
  if(!control)
    return;
  
  if(Document *doc = Documents::current()) {
    StyledObject *obj = nullptr;
    if(scope == richmath_System_Document) {
      obj = doc;
    }
    else if(Box *sel = doc->selection_box()) {
      if(scope == richmath_System_Section)
        obj = sel->find_parent<Section>(true);
      else
        obj = sel;
    }
    
    DWORD style = GetWindowLongW(control, GWL_STYLE);
    if(obj) {
      StyleOptionName key = Style::get_key(lhs);
      StyleType type = Style::get_type(key);
      if(type == StyleType::Number) {
        float val = obj->get_style((FloatStyleOptionName)key, NAN);
        Array<float> values;
        if(!std::isnan(val) && collect_float_values(values, menu)) {
          float rel_idx = interpolation_index(values, val, false);
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

bool MenuGutterSlider::collect_float_values(Array<float> &values, HMENU menu) {
  values.length(end_index - start_index + 1);
  for(int i = 0; i < values.length(); ++i) {
    MENUITEMINFOW mii = { sizeof(mii) };
    mii.fMask = MIIM_ID;
    if(!GetMenuItemInfoW(menu, start_index + i, TRUE, &mii))
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

float MenuGutterSlider::interpolation_index(const Array<float> &values, float val, bool clip) {
  float prev = NAN;
  for(int i = 0; i < values.length(); ++i) {
    float cur = values[i];
    if(val == cur)
      return i;
    
    float rel = (cur - val) / (cur - prev);
    if(0 <= rel && rel <= 1) 
      return i - rel;
    
    prev = cur;
  }
  
  if(clip) {
    if(values.length() >= 2) {
      if(values[0] < values[1]) {
        if(val < values[0])
          return 0;
        else
          return values.length() - 1;
      }
      else {
        if(val > values[0])
          return values.length() - 1;
        else
          return 0;
      }
    }
  }
  return NAN;
}

bool MenuGutterSlider::handle_mouse_message(UINT msg, WPARAM wParam, const POINT &pt, HMENU menu) {
  
  int pos = slider_pos_from_point(pt);
//  pmath_debug_print("[mouse msg %x over menu slider: w=%x pos %d]\n", msg, wParam, pos);
  
  if(!(wParam & MK_SHIFT)) {
    int nearest = ((pos + ScaleFactor/2)/ScaleFactor) * ScaleFactor;
    if(nearest - ScaleFactor/4 < pos && pos < nearest + ScaleFactor/4)
      pos = nearest;
  }
  
  if(pos == SendMessageW(control, TBM_GETPOS, 0, 0))
    return false;
  
  switch(msg) {
    case WM_MOUSEMOVE: 
      if(wParam & MK_LBUTTON) {
        apply_slider_pos(menu, pos);
      }
      break;
    
    case WM_LBUTTONDOWN: apply_slider_pos(menu, pos); break;
    case WM_LBUTTONUP:   apply_slider_pos(menu, pos); break;
  }
  
  return false;
}

int MenuGutterSlider::slider_pos_from_point(const POINT &pt) {
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

void MenuGutterSlider::apply_slider_pos(HMENU menu, int pos) {
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
    cmd = Call(Symbol(richmath_FE_ScopedCommand), std::move(cmd), scope);
  
  if(!Menus::run_command_now(std::move(cmd)))
    return;
  
  // TBM_SETPOSNOTIFY exists since Windows 7
  SendMessageW(control, TBM_SETPOS, TRUE, (LPARAM)pos);
  
  DWORD style = GetWindowLongW(control, GWL_STYLE);
  if(style & TBS_NOTHUMB)
    SetWindowLongW(control, GWL_STYLE, style & ~TBS_NOTHUMB);
}

//} ... class MenuGutterSlider
