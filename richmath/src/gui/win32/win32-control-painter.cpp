#define WINVER 0x0600

#include <gui/win32/win32-control-painter.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cwchar>

#include <windows.h>

#ifdef min
#  undef min
#endif
#ifdef max
#  undef max
#endif

#include <cairo-win32.h>

#include <boxes/box.h>
#include <eval/observable.h>
#include <graphics/context.h>
#include <gui/win32/basic-win32-widget.h>
#include <gui/win32/win32-highdpi.h>
#include <gui/win32/win32-themes.h>
#include <util/array.h>
#include <util/style.h>

#ifndef WM_DWMCOMPOSITIONCHANGED
#define WM_DWMCOMPOSITIONCHANGED   0x031E
#endif

using namespace richmath;
using namespace std;

class Win32ControlPainterInfo: public BasicWin32Widget {
  public:
    Win32ControlPainterInfo()
      : BasicWin32Widget(0, 0, 0, 0, 0, 0, nullptr)
    {
      init(); // total exception!!! normally not callable in constructor
    }
    
  protected:
    virtual LRESULT callback(UINT message, WPARAM wParam, LPARAM lParam) override {
      switch(message) {
        case WM_DWMCOMPOSITIONCHANGED:
        case WM_THEMECHANGED: {
            Win32ControlPainter::win32_painter.clear_cache();
            style_observations.notify_all();
          } break;
      }
      
      return BasicWin32Widget::callback(message, wParam, lParam);
    }
  
  public:
    Observable style_observations;
};

static Win32ControlPainterInfo w32cpinfo;

namespace {
  static const Color InputFieldBlurColor = Color::from_rgb24(0x8080FF);
  
  static Color get_sys_color(int index) {
    return Color::from_bgr24(GetSysColor(index));
  }
  
  static bool dark_mode_is_fake(ContainerType type) {
    switch(type) {
      case ProgressIndicatorBackground:
      case SliderHorzChannel:
      case PanelControl:
      case TabHeadAbuttingRight:
      case TabHeadAbuttingLeftRight:
      case TabHeadAbuttingLeft:
      case TabHead:
      case TabHeadBackground:
      case TabBodyBackground:
        return true;
      
      default: return false;
    }
  }
};

static class Win32ControlPainterCache {
  public:
    Win32ControlPainterCache() {
    }
    
    ~Win32ControlPainterCache() {
      clear();
    }
  
  public:
    HANDLE addressband_theme(int dpi, bool dark) {
      // TODO: ABComposited::ADRESSBAND
      if(dark)
        return get_theme_for_dpi(dark_addressband_theme_for_dpi, L"DarkMode_ABComposited::ADDRESSBAND;ADDRESSBAND", dpi);
      else
        return get_theme_for_dpi(addressband_theme_for_dpi, L"AB::ADDRESSBAND;ADDRESSBAND", dpi);
    }
    HANDLE addressband_combobox_theme(int dpi, bool dark) {
      if(dark)
        return get_theme_for_dpi(dark_addressband_combobox_theme_for_dpi, L"DarkMode_AddressComposited::Combobox;AddressComposited::Combobox;Combobox", dpi);
      else
        return get_theme_for_dpi(addressband_combobox_theme_for_dpi, L"AddressComposited::Combobox;Combobox", dpi);
    }
    HANDLE addressband_edit_theme(int dpi, bool dark) {
      if(dark)
        return get_theme_for_dpi(dark_addressband_edit_theme_for_dpi, L"DarkMode_AddressComposited::Edit;AddressComposited::Edit;Edit", dpi);
      else
        return get_theme_for_dpi(addressband_edit_theme_for_dpi, L"AddressComposited::Edit;Edit", dpi);
    }
    HANDLE button_theme(int dpi, bool dark) {
      if(dark)
        return get_theme_for_dpi(dark_button_theme_for_dpi, L"DarkMode_Explorer::BUTTON;BUTTON", dpi);
      else
        return get_theme_for_dpi(button_theme_for_dpi, L"BUTTON", dpi);
    }
    HANDLE edit_theme(int dpi, bool dark) {
      if(dark)
        return get_theme_for_dpi(dark_edit_theme_for_dpi, L"DarkMode_CFD::EDIT;Explorer::EDIT;EDIT", dpi);
      else
        return get_theme_for_dpi(edit_theme_for_dpi, L"Explorer::EDIT;EDIT", dpi);
    }
    HANDLE explorer_listview_theme(int dpi, bool dark) {
      if(dark)
        return get_theme_for_dpi(dark_explorer_listview_theme_for_dpi, L"DarkMode_ItemsView::LISTVIEW;LISTVIEW", dpi);
      else
        return get_theme_for_dpi(explorer_listview_theme_for_dpi, L"Explorer::LISTVIEW;LISTVIEW", dpi);
    }
    HANDLE explorer_treeview_theme(int dpi, bool dark) {
      if(dark)
        return get_theme_for_dpi(dark_explorer_treeview_theme_for_dpi, L"DarkMode_Explorer::TREEVIEW;TREEVIEW", dpi);
      else
        return get_theme_for_dpi(explorer_treeview_theme_for_dpi, L"Explorer::TREEVIEW;TREEVIEW", dpi);
    }
    HANDLE navigation_theme(int dpi, bool dark) {
      if(dark)
        return get_theme_for_dpi(navigation_theme_for_dpi, L"DarkMode::Navigation;Navigation", dpi);
      else
        return get_theme_for_dpi(navigation_theme_for_dpi, L"Navigation", dpi);
    }
    HANDLE tooltip_theme(int dpi, bool dark) {
      if(dark)
        return get_theme_for_dpi(dark_tooltip_theme_for_dpi, L"DarkMode_Explorer::TOOLTIP;TOOLTIP", dpi);
      else
        return get_theme_for_dpi(tooltip_theme_for_dpi, L"TOOLTIP", dpi);
    }
    HANDLE progress_theme(int dpi) {
      return get_theme_for_dpi(progress_theme_for_dpi, L"PROGRESS", dpi);
    }
    HANDLE scrollbar_theme(int dpi) {
      return get_theme_for_dpi(scrollbar_theme_for_dpi, L"SCROLLBAR", dpi);
    }
    HANDLE slider_theme(int dpi) {
      return get_theme_for_dpi(slider_theme_for_dpi, L"TRACKBAR", dpi);
    }
    HANDLE tab_theme(int dpi) {
      return get_theme_for_dpi(tab_theme_for_dpi, L"TAB", dpi);
    }
    HANDLE toolbar_theme(int dpi, bool dark) {
      if(dark)
        return get_theme_for_dpi(dark_toolbar_theme_for_dpi, L"DarkMode::TOOLBAR;TOOLBAR", dpi);
      else
        return get_theme_for_dpi(toolbar_theme_for_dpi, L"TOOLBAR", dpi);
    }
    HANDLE toolbar_go_theme(int dpi) {
      // TODO: "GoComposited::TOOLBAR"
      return get_theme_for_dpi(toolbar_go_theme_for_dpi, L"Go::TOOLBAR", dpi);
    }
    
    void clear() {
      close_themes(     addressband_theme_for_dpi);
      close_themes(dark_addressband_theme_for_dpi);
      close_themes(     addressband_combobox_theme_for_dpi);
      close_themes(dark_addressband_combobox_theme_for_dpi);
      close_themes(     addressband_edit_theme_for_dpi);
      close_themes(dark_addressband_edit_theme_for_dpi);
      close_themes(     button_theme_for_dpi);
      close_themes(dark_button_theme_for_dpi);
      close_themes(     edit_theme_for_dpi);
      close_themes(dark_edit_theme_for_dpi);
      close_themes(     explorer_listview_theme_for_dpi);
      close_themes(dark_explorer_listview_theme_for_dpi);
      close_themes(     explorer_treeview_theme_for_dpi);
      close_themes(dark_explorer_treeview_theme_for_dpi);
      close_themes(     navigation_theme_for_dpi);
      close_themes(dark_navigation_theme_for_dpi);
      close_themes(     tooltip_theme_for_dpi);
      close_themes(dark_tooltip_theme_for_dpi);
      close_themes(progress_theme_for_dpi);
      close_themes(scrollbar_theme_for_dpi);
      close_themes(slider_theme_for_dpi);
      close_themes(tab_theme_for_dpi);
      close_themes(     toolbar_theme_for_dpi);
      close_themes(dark_toolbar_theme_for_dpi);
      close_themes(toolbar_go_theme_for_dpi);
    }
  
  private:
    static void close_themes(Hashtable<int, HANDLE> &cache) {
      if(Win32Themes::CloseThemeData) {
        for(auto &e : cache.entries())
          Win32Themes::CloseThemeData(e.value);
        
        cache.clear();
      }
    }
    
    static HANDLE get_theme_for_dpi(Hashtable<int, HANDLE> &cache, const wchar_t *name, int dpi) {
      if(HANDLE *h = cache.search(dpi))
        return *h;
      
      if(Win32Themes::OpenThemeDataForDpi) {
        HANDLE h = Win32Themes::OpenThemeDataForDpi(nullptr, name, (UINT)dpi);
        if(h) 
          cache.set(dpi, h);
          
        return h;
      }
      
      if(Win32Themes::OpenThemeData) {
        HANDLE h = Win32Themes::OpenThemeData(nullptr, name);
        if(h) 
          cache.set(dpi, h);
          
        return h;
      }
      
      return nullptr;
    }
  
  private:
    Hashtable<int, HANDLE> addressband_theme_for_dpi;
    Hashtable<int, HANDLE> addressband_combobox_theme_for_dpi;
    Hashtable<int, HANDLE> addressband_edit_theme_for_dpi;
    Hashtable<int, HANDLE> button_theme_for_dpi;
    Hashtable<int, HANDLE> edit_theme_for_dpi;
    Hashtable<int, HANDLE> explorer_listview_theme_for_dpi;
    Hashtable<int, HANDLE> explorer_treeview_theme_for_dpi;
    Hashtable<int, HANDLE> navigation_theme_for_dpi;
    Hashtable<int, HANDLE> tooltip_theme_for_dpi;
    Hashtable<int, HANDLE> progress_theme_for_dpi;
    Hashtable<int, HANDLE> scrollbar_theme_for_dpi;
    Hashtable<int, HANDLE> slider_theme_for_dpi;
    Hashtable<int, HANDLE> tab_theme_for_dpi;
    Hashtable<int, HANDLE> toolbar_theme_for_dpi;
    Hashtable<int, HANDLE> toolbar_go_theme_for_dpi;
    Hashtable<int, HANDLE> dark_addressband_theme_for_dpi;
    Hashtable<int, HANDLE> dark_addressband_combobox_theme_for_dpi;
    Hashtable<int, HANDLE> dark_addressband_edit_theme_for_dpi;
    Hashtable<int, HANDLE> dark_button_theme_for_dpi;
    Hashtable<int, HANDLE> dark_edit_theme_for_dpi;
    Hashtable<int, HANDLE> dark_explorer_listview_theme_for_dpi;
    Hashtable<int, HANDLE> dark_explorer_treeview_theme_for_dpi;
    Hashtable<int, HANDLE> dark_navigation_theme_for_dpi;
    Hashtable<int, HANDLE> dark_tooltip_theme_for_dpi;
    Hashtable<int, HANDLE> dark_toolbar_theme_for_dpi;
} w32cp_cache;

Win32ControlPainter Win32ControlPainter::win32_painter;

void Win32ControlPainter::done() {
  win32_painter.clear_cache();
  w32cpinfo.style_observations.notify_all(); // to clear the observers array
}

Win32ControlPainter::Win32ControlPainter()
  : ControlPainter(),
    blur_input_field(true)
{
  ControlPainter::std = this;
}

Win32ControlPainter::~Win32ControlPainter() {
  clear_cache();
}

static void round_extents(Canvas &canvas, BoxSize *extents) {
  extents->width = canvas.pixel_round_dx(extents->width);
  extents->ascent = canvas.pixel_round_dy(extents->ascent);
  extents->descent = canvas.pixel_round_dy(extents->descent);
}

void Win32ControlPainter::calc_container_size(
  ControlContext &control,
  Canvas         &canvas,
  ContainerType   type,
  BoxSize        *extents // in/out
) {
  int theme_part, theme_state;
  HANDLE theme = get_control_theme(control, type, Normal, &theme_part, &theme_state);
  
  switch(type) {
    case InputField: {
        if(theme) {
          extents->width +=   3;
          extents->ascent +=  1.5;
          extents->descent += 1.5;
          round_extents(canvas, extents);
          return;
        }
        
        extents->width +=   4.5;
        extents->ascent +=  3;
        extents->descent += 2.25;
        round_extents(canvas, extents);
      } return;
      
    case AddressBandInputField: {
        if(theme) {
          extents->width +=   1.5;
          extents->ascent +=  0.75;
          extents->descent += 0.75;
          round_extents(canvas, extents);
          return;
        }
        
        extents->width +=   3.0;
        extents->ascent +=  2.25;
        extents->descent += 1.5;
        round_extents(canvas, extents);
      } return;
    
    case AddressBandBackground: 
      if(!theme) {
        extents->width +=   1.5;
        extents->ascent +=  0.75;
        extents->descent += 0.75;
        round_extents(canvas, extents);
        return;
      }
      break;
      
    case ListViewItem:
    case ListViewItemSelected:
      ControlPainter::calc_container_size(control, canvas, type, extents);
      round_extents(canvas, extents);
      return;
    
    case PanelControl:
    case PopupPanel:
      if(theme && Win32Themes::GetThemeMargins) {
        extents->width +=   9.0;
        extents->ascent +=  4.5;
        extents->descent += 4.5;
      }
      break;
      
    case ProgressIndicatorBackground: {
        if(theme && Win32Themes::GetThemePartSize) {
          SIZE size;
          if(SUCCEEDED(Win32Themes::GetThemePartSize(theme, nullptr, theme_part, theme_state, nullptr, Win32Themes::TS_TRUE, &size))) {
            extents->ascent = 0.75 * size.cy;
            extents->descent = 0;
            round_extents(canvas, extents);
            return;
          }
        }
      } break;
      
    case ProgressIndicatorBar: {
        if(!theme || theme_part != 5) {
          extents->width -=   4.5;
          extents->ascent -=  2.25;
          extents->descent -= 2.25;
        }
        round_extents(canvas, extents);
      } return;
      
    case SliderHorzChannel: {
        float dx = 0;
        float dy = 4;
        if(theme && Win32Themes::GetThemePartSize) {
          SIZE size;
          if(SUCCEEDED(Win32Themes::GetThemePartSize(theme, nullptr, theme_part, theme_state, nullptr, Win32Themes::TS_TRUE, &size))) {
            dy = size.cy;
          }
        }
        canvas.device_to_user_dist(&dx, &dy);
        extents->ascent = max(fabs(dx), fabs(dy));
        extents->descent = 0;
      } return;
      
    case SliderHorzThumb: {
        if(theme && Win32Themes::GetThemePartSize) {
          SIZE size;
          if(SUCCEEDED(Win32Themes::GetThemePartSize(theme, nullptr, theme_part, theme_state, nullptr, Win32Themes::TS_TRUE, &size))) {
            extents->ascent = 0.75 * size.cy;
            extents->descent = 0;
            //extents->width = 0.75 * size.cx;
            extents->width = 0.5 * extents->height();
            round_extents(canvas, extents);
            return;
          }
        }
      } break;
      
    case TooltipWindow: {
        if(!theme) {
          extents->width +=   6;
          extents->ascent +=  3;
          extents->descent += 3;
          round_extents(canvas, extents);
          return;
        }
      } break;
    
    case NavigationBack:
    case NavigationForward: 
      if(theme && Win32Themes::GetThemePartSize) {
        SIZE size;
        if(SUCCEEDED(Win32Themes::GetThemePartSize(theme, nullptr, theme_part, theme_state, nullptr, Win32Themes::TS_TRUE, &size))) {
          /* For the Navigation parts, TS_TRUE on 144 DPI monitor gives 45x45 and on
             96 PDI monitor gives 30, so here the TS_TRUE size is not in terms of the primary monitor DPI (unlike PushButton?)
           */
          int dpi = control.dpi();
          double scale = 72.0 / dpi;
          extents->width   = std::max(size.cx * scale, (double)extents->width);
          float axis = canvas.get_font_size() * 0.4;
          extents->ascent  = std::max(axis + size.cy * scale * 0.5, (double)extents->ascent);
          extents->descent = std::max(-axis + size.cy * scale * 0.5, (double)extents->descent);
          return;
        }
      }
      break;
      
    case TabHeadBackground:
      return;
    
    default: break;
  }
  
  if(Win32Themes::GetThemeMargins && Win32Themes::GetThemePartSize) {
    SIZE size = {0, 0};
    Win32Themes::MARGINS mar = {0, 0, 0, 0};
    // TMT_SIZINGMARGINS  = 3601
    // TMT_CONTENTMARGINS = 3602
    // TMT_CAPTIONMARGINS = 3603
    if( theme && 
        SUCCEEDED(Win32Themes::GetThemeMargins(theme, nullptr, theme_part, theme_state, 3602, nullptr, &mar)) && 
        ( mar.cxLeftWidth > 0 || 
          mar.cxRightWidth > 0 || 
          mar.cyBottomHeight > 0 || 
          mar.cyTopHeight > 0))
    {
      extents->width +=   0.75f * (mar.cxLeftWidth + mar.cxRightWidth);
      extents->ascent +=  0.75f * mar.cyTopHeight;
      extents->descent += 0.75f * mar.cyBottomHeight;
      
      Win32Themes::GetThemePartSize(
        theme, nullptr, theme_part, theme_state, nullptr, Win32Themes::TS_TRUE, &size);
        
      if(extents->width < 0.75 * size.cx)
        extents->width = 0.75 * size.cx;
        
      if(extents->height() < 0.75 * size.cy) {
        float axis = 0.4 * canvas.get_font_size();
        
        extents->ascent  = size.cy * 0.75 / 2 + axis;
        extents->descent = size.cy * 0.75 - extents->ascent;
      }
      
      if(type == TabBodyBackground) {
        if(SUCCEEDED(Win32Themes::GetThemeMargins(theme, nullptr, theme_part, theme_state, 3601, nullptr, &mar))) {
          extents->ascent-= 0.75f * mar.cyTopHeight;
        }
      }
      
      round_extents(canvas, extents);
      return;
    }
  }
  
  ControlPainter::calc_container_size(control, canvas, type, extents);
  round_extents(canvas, extents);
}

void Win32ControlPainter::calc_container_radii(
  ControlContext &control,
  ContainerType   type,
  BoxRadius      *radii
) {
  int theme_part, theme_state;
  HANDLE theme = get_control_theme(control, type, Normal, &theme_part, &theme_state);
  
  switch(type) {
    case TooltipWindow: {
        if(!theme || Win32Themes::is_windows_8_or_newer()) {
          *radii = BoxRadius(0);
          return;
        }
        
        *radii = BoxRadius(1.5); // 2 pixels all
        return;
      } break;
    
    default:
      break;
  }
  
  ControlPainter::calc_container_radii(control, type, radii);
}

Color Win32ControlPainter::control_font_color(ControlContext &control, ContainerType type, ControlState state) {
  if(is_very_transparent(control, type, state))
    return Color::None;
    
  if(type == AddressBandBackground)
    type = AddressBandInputField;
  
  int theme_part, theme_state;
  HANDLE theme = get_control_theme(control, type, state, &theme_part, &theme_state);
  
  if(theme && Win32Themes::GetThemeColor) {
    COLORREF col = 0;
    
    if(type == AddressBandInputField) {
      theme = w32cp_cache.addressband_edit_theme(control.dpi(), control.is_using_dark_mode());
      theme_part = 1; // EP_EDITTEXT
      theme_state = 1; // ETS_NORMAL
    }
    else if(type == InputField) {
      theme_part = 1; // EP_EDITTEXT
      theme_state = 1; // ETS_NORMAL
    }
    
    // TMT_TEXTCOLOR  = 3803
    // TMT_WINDOWTEXT  = 1609
    // TMT_BTNTEXT    = 1619
    if( SUCCEEDED(Win32Themes::GetThemeColor(
                    theme, theme_part, theme_state, 3803, &col)) ||
        SUCCEEDED(Win32Themes::GetThemeColor(
                    theme, theme_part, theme_state, 1609, &col)) ||
        SUCCEEDED(Win32Themes::GetThemeColor(
                    theme, theme_part, theme_state, 1619, &col)))
    {
      if(dark_mode_is_fake(type) && control.is_using_dark_mode()) {
        col = 0xFFFFFF & ~col;
      }
      return Color::from_bgr24(col);
    }
    
    //return Color::None;
  }
  
  switch(type) {
    case NoContainerType:
    case FramelessButton:
    case GenericButton:
      return ControlPainter::control_font_color(control, type, state);
    
    case PopupPanel:
      return control.is_using_dark_mode() ? Color::White : get_sys_color(COLOR_BTNTEXT);
    
    case AddressBandGoButton:
    case PushButton:
    case DefaultPushButton:
    case PaletteButton:
    case PanelControl:
    case TabHeadAbuttingRight:
    case TabHeadAbuttingLeftRight:
    case TabHeadAbuttingLeft:
    case TabHead:
    case TabBodyBackground: {
        if(dark_mode_is_fake(type) && control.is_using_dark_mode()) {
          return Color::White;
        }
        return get_sys_color(COLOR_BTNTEXT);
      } break;
    
    case ListViewItem:
      if(state == Normal)
        return Color::None;
      else
        return get_sys_color(state == Disabled ? COLOR_GRAYTEXT : COLOR_WINDOWTEXT);
    
    case AddressBandInputField: // AddressBandBackground
    case InputField:
      return get_sys_color(state == Disabled ? COLOR_GRAYTEXT : COLOR_WINDOWTEXT);
    
    case ListViewItemSelected:
      return get_sys_color(COLOR_HIGHLIGHTTEXT);
      
    case TooltipWindow:
      return get_sys_color(COLOR_INFOTEXT);
      
    case SliderHorzChannel:
    case SliderHorzThumb:
    case ProgressIndicatorBackground:
    case ProgressIndicatorBar:
    case CheckboxUnchecked:
    case CheckboxChecked:
    case CheckboxIndeterminate:
    case RadioButtonUnchecked:
    case RadioButtonChecked:
      break;
  }
  
  return Color::None;
}

bool Win32ControlPainter::is_very_transparent(ControlContext &control, ContainerType type, ControlState state) {
  switch(type) {
    case NoContainerType:
    case FramelessButton:
    case GenericButton:
    case AddressBandInputField:
      return ControlPainter::is_very_transparent(control, type, state);
    
    case ListViewItem:
      return state == Normal;
      
    case PaletteButton:
    case AddressBandGoButton: {
        if(!Win32Themes::GetThemeBool)
          return false;
          
        int theme_part, theme_state;
        HANDLE theme = get_control_theme(control, type, state, &theme_part, &theme_state);
        
        // TMT_TRANSPARENT = 2201
        BOOL value;
        if(theme && SUCCEEDED(Win32Themes::GetThemeBool(theme, theme_part, theme_state, 2201, &value))) {
          return value;
        }
      } break;
      
    default: break;
  }
  
  return false;
}

static bool rect_in_clip(Canvas &canvas, const RectangleF &rect) {
  cairo_rectangle_list_t *clip_rects = cairo_copy_clip_rectangle_list(canvas.cairo());
  
  if(clip_rects->status == CAIRO_STATUS_SUCCESS) {
    for(int i = 0; i < clip_rects->num_rectangles; ++i) {
      const cairo_rectangle_t &clip_rect = clip_rects->rectangles[i];
      if(RectangleF(clip_rect.x, clip_rect.y, clip_rect.width, clip_rect.height).contains(rect)) {
        cairo_rectangle_list_destroy(clip_rects);
        return true;
      }
    }
  }
  
  cairo_rectangle_list_destroy(clip_rects);
  return false;
}

void Win32ControlPainter::draw_container(
  ControlContext &control, 
  Canvas         &canvas,
  ContainerType   type,
  ControlState    state,
  RectangleF      rect
) {
  switch(type) {
    case NoContainerType:
    case FramelessButton:
    case GenericButton:
      ControlPainter::generic_painter.draw_container(
        control, canvas, type, state, rect);
      return;
      
    case ListViewItem:
      if(state == Normal)
        return;
      break;
    
    case AddressBandInputField:
      if(state == Normal || state == Hovered)
        return;
      break;
     
    default: break;
  }
  
  double x_scale;
  double y_scale;
  {
    double a = 1;
    double b = 0;
    canvas.user_to_device_dist(&a, &b);
    x_scale = hypot(a, b);
    
    a = 0;
    b = 1;
    canvas.user_to_device_dist(&a, &b);
    y_scale = hypot(a, b);
  }
  
  switch(type) {
    case TabHeadAbuttingRight:
    case TabHeadAbuttingLeftRight:
    case TabHeadAbuttingLeft:
    case TabHead: {
        if(state != Pressed && state != PressedHovered) {
          rect.y+=      1.5f;
          rect.height-= 1.5f;
          
          if(y_scale > 0) {
            int _part, _state;
            if(HANDLE theme = get_control_theme(control, TabHeadBackground, Normal, &_part, &_state)) {
              Win32Themes::MARGINS mar;
              // 3601 = TMT_SIZINGMARGINS
              if(SUCCEEDED(Win32Themes::GetThemeMargins(theme, nullptr, _part, _state, 3601, nullptr, &mar))) {
                rect.height-= mar.cyTopHeight / y_scale;
              }
            }
            else
              rect.height-= 2 / y_scale;
          }
        }
      } break;
    
    case TabHeadBackground: {
        rect.y+= rect.height;
        rect.height = 0;
        
        if(y_scale > 0) {
          int _part, _state;
          if(HANDLE theme = get_control_theme(control, TabHeadBackground, Normal, &_part, &_state)) {
            Win32Themes::MARGINS mar;
            // 3601 = TMT_SIZINGMARGINS
            if(SUCCEEDED(Win32Themes::GetThemeMargins(theme, nullptr, _part, _state, 3601, nullptr, &mar))) {
              rect.height = mar.cyTopHeight / y_scale;
            }
          }
          rect.height = 2 / y_scale;
        }
        
        rect.y-= rect.height;
      } break;
  }
  
  rect.pixel_align(canvas, false);
  if(rect.width <= 0 || rect.height <= 0)
    return;
  
  switch(type) {
    case PopupPanel: {
        CanvasAutoSave saved(canvas);
        Color c = canvas.get_color();
        rect.add_rect_path(canvas, false);
        canvas.set_color(win32_button_face_color(control.is_using_dark_mode()));
        canvas.fill_preserve();
        rect.grow(-1.0/x_scale, -1.0/y_scale);
        rect.add_rect_path(canvas, true);
        canvas.set_color(Color::Black, 0.5);
        canvas.fill();
        canvas.set_color(c);
      } return;
  }
  
  Win32Themes::MARGINS margins = {0};
  
  int dc_x = 0;
  int dc_y = 0;
  int w = (int)ceil(x_scale * rect.width - 0.5);
  int h = (int)ceil(y_scale * rect.height - 0.5);
  
  if(w == 0 || h == 0)
    return;
  
  HDC dc = safe_cairo_win32_surface_get_dc(canvas.target());
  if(dc) {
    cairo_matrix_t ctm = canvas.get_matrix();
    
    if( (ctm.xx == 0) == (ctm.yy == 0) &&
        (ctm.xy == 0) == (ctm.yx == 0) &&
        (ctm.xx == 0) != (ctm.xy == 0) &&
        rect_in_clip(canvas, rect))
    {
      Point upos = canvas.user_to_device(rect.top_left());
      dc_x = (int)round(upos.x);
      dc_y = (int)round(upos.y);
      cairo_surface_flush(cairo_get_target(canvas.cairo()));
    }
    else {
      dc = nullptr;
    }
  }
  
  cairo_surface_t *surface = nullptr;
  
  bool need_vector_overlay = false;
  if( Win32Themes::OpenThemeData && 
      Win32Themes::CloseThemeData && 
      Win32Themes::DrawThemeBackground &&
      Win32Themes::GetThemeBool &&
      Win32Themes::GetThemeMargins &&
      Win32Themes::GetThemePartSize) 
  {
    int _part, _state;
    HANDLE theme = get_control_theme(control, type, state, &_part, &_state);
    if(!theme)
      goto FALLBACK;
    
    SIZE size = {0,0};
    if( SUCCEEDED(Win32Themes::GetThemePartSize(theme, nullptr, _part, _state, nullptr, Win32Themes::TS_DRAW, &size)) && 
        size.cx > 0 && 
        size.cy > 0) 
    {
      if(size.cx <= w && size.cy <= h) {
        if(dc) {
          dc_x+= (w - size.cx) / 2;
          dc_y+= (h - size.cy) / 2;
        }
        w = size.cx;
        h = size.cy;
      }
    }
    
    if(!dc) {
      surface = cairo_win32_surface_create_with_dib(
                  CAIRO_FORMAT_ARGB32,
                  w,
                  h);
      dc = safe_cairo_win32_surface_get_dc(surface);
      if(!dc) {
        cairo_surface_destroy(surface);
        surface = nullptr;
        ControlPainter::draw_container(control, canvas, type, state, rect);
        return;
      }
    }
    
    RECT irect;
    irect.left   = dc_x;
    irect.top    = dc_y;
    irect.right  = dc_x + w;
    irect.bottom = dc_y + h;
    
    RECT clip = irect;
  
    bool two_times = false;
    if(canvas.glass_background) {
      switch(type) {
        case PaletteButton: {
          if(state == Normal)
            state = Hovered;
          else if(state == Hovered)
            two_times = true;
        } break;
          
        //case CheckboxUnchecked:
        //case CheckboxChecked:
        //case CheckboxIndeterminate:
        //case RadioButtonUnchecked:
        //case RadioButtonChecked:
        //case OpenerTriangleClosed:
        //case OpenerTriangleOpened:
        //case SliderHorzChannel:
        //case SliderHorzThumb:
        case NavigationBack:
        case NavigationForward: 
          if(!canvas.show_only_text) {
            cairo_surface_t *tmp = cairo_win32_surface_create_with_dib(CAIRO_FORMAT_ARGB32, w, h);
            
            if(cairo_surface_status(tmp) == CAIRO_STATUS_SUCCESS) {
              HDC tmp_dc = cairo_win32_surface_get_dc(tmp);
              
              RECT tmp_rect;
              tmp_rect.left = 0;
              tmp_rect.right = w;
              tmp_rect.top = 0;
              tmp_rect.bottom = h;
              Win32Themes::DrawThemeBackground(
                theme, tmp_dc, _part, _state, &tmp_rect, 0);
              
              cairo_surface_flush(tmp);
              
              canvas.save();
              cairo_pattern_t *tmp_pat = cairo_pattern_create_for_surface(tmp);
              
              // 0.75 is one pixel at default scaling
              float r = 0.75;
              struct {float dx; float dy; Color c; float alpha; } shadows[] = {
                {-r, 0, Color::Black, 0.5 },
                {0, -r, Color::Black, 0.5 },
                {r, 0, Color::White, 0.5 },
                {0, r, Color::White, 0.5 },
              };
              
              cairo_matrix_t mat {};
              mat.xx = w / rect.width;
              mat.yy = h / rect.height;
              mat.xy = mat.yx = 0;
              
              Color oldc = canvas.get_color();
              for(auto &s : shadows) {
                mat.x0 = -(rect.x + s.dx) * mat.xx;
                mat.y0 = -(rect.y + s.dy) * mat.yy;
                
                cairo_pattern_set_matrix(tmp_pat, &mat);
                
                canvas.set_color(s.c, s.alpha);
                cairo_mask(canvas.cairo(), tmp_pat);
                
                canvas.fill();
              
              }
              
              cairo_pattern_destroy(tmp_pat);
              canvas.set_color(oldc);
              canvas.restore();
            }

            cairo_surface_destroy(tmp);
          } 
          break;
        
        default: break;
      }
    }
    
    if(!Win32Themes::IsCompositionActive) { /* XP not enough */
      FillRect(dc, &irect, (HBRUSH)(COLOR_BTNFACE + 1));
    }
    
    // 3601 = TMT_SIZINGMARGINS
    if(FAILED(Win32Themes::GetThemeMargins(theme, dc, _part, _state, 3601, nullptr, &margins))) {
      margins = {0};
    }
    
    switch(type) {
      case TabHeadBackground: {
          irect.top = irect.bottom - margins.cyTopHeight;
          irect.bottom+= margins.cyBottomHeight;
        } break;
      case TabBodyBackground: {
          irect.top-= margins.cyTopHeight;
        } break;
    }
    
    Win32Themes::DrawThemeBackground(
      theme,
      dc,
      _part,
      _state,
      &irect,
      &clip);
    
    if(two_times) {
      Win32Themes::DrawThemeBackground(
        theme,
        dc,
        _part,
        _state,
        &irect,
        &clip);
      Win32Themes::DrawThemeBackground(
        theme,
        dc,
        _part,
        _state,
        &irect,
        &clip);
    }
  }
  else {
  FALLBACK: ;
    if(dc) {
      if(cairo_surface_get_content(canvas.target()) != CAIRO_CONTENT_COLOR) {
        dc = nullptr;
        dc_x = 0;
        dc_y = 0;
      }
    }
    
    if(!dc) {
      surface = cairo_win32_surface_create_with_dib(
                  CAIRO_FORMAT_RGB24,
                  w,
                  h);
      dc = safe_cairo_win32_surface_get_dc(surface);
      if(!dc) {
        cairo_surface_destroy(surface);
        surface = nullptr;
        ControlPainter::draw_container(control, canvas, type, state, rect);
        return;
      }
    }
    
    RECT irect;
    irect.left   = dc_x;
    irect.top    = dc_y;
    irect.right  = dc_x + w;
    irect.bottom = dc_y + h;
    
    switch(type) {
      case NoContainerType:
      case FramelessButton:
        break;
      
      default: 
        if(state == Pressed) {
          if(!control.is_focused_widget())
            state = Normal;
        }
        else if(state == PressedHovered) {
          if(!control.is_focused_widget())
            state = Hovered;
        }
        break;
    }
    
    switch(type) {
      case NoContainerType:
      case FramelessButton:
        break;
        
      case AddressBandGoButton:
      case PaletteButton: {
          FillRect(dc, &irect, (HBRUSH)(COLOR_BTNFACE + 1));
          
          if(state == PressedHovered)
            DrawEdge(dc, &irect, BDR_SUNKENOUTER, BF_RECT);
          else if(state == Hovered)
            DrawEdge(dc, &irect, BDR_RAISEDINNER, BF_RECT);
        } break;
        
      case DefaultPushButton:
      case GenericButton:
      case PushButton: {
          if(type == DefaultPushButton) {
            FrameRect(dc, &irect, (HBRUSH)GetStockObject(BLACK_BRUSH));
            
            InflateRect(&irect, -1, -1);
          }
          
          FillRect(dc, &irect, (HBRUSH)(COLOR_BTNFACE + 1));
          
          if(state == PressedHovered) {
            DrawEdge(dc, &irect, EDGE_SUNKEN, BF_RECT);
          }
          else {
            DrawEdge(dc, &irect, EDGE_RAISED, BF_RECT);
          }
          
//        DrawFrameControl(
//          dc,
//          &irect,
//          DFC_BUTTON,
//          _state);
        } break;
        
      case InputField: {
          FillRect(dc, &irect, (HBRUSH)(COLOR_WINDOW + 1));
          
          DrawEdge(
            dc,
            &irect,
            EDGE_SUNKEN,
            BF_RECT);
        } break;
        
      case AddressBandInputField: {
          if(state == Pressed || state == PressedHovered) {
            FillRect(dc, &irect, (HBRUSH)(COLOR_WINDOW + 1));
            DrawEdge(
              dc,
              &irect,
              BDR_SUNKENINNER,
              BF_RECT);
          }
        } break;
        
      case AddressBandBackground: {
          FillRect(dc, &irect, (HBRUSH)(COLOR_BTNFACE + 1));
          
          DrawEdge(
            dc,
            &irect,
            BDR_SUNKENOUTER,
            BF_RECT);
        } break;
        
      case TooltipWindow: {
          FrameRect(dc, &irect, GetSysColorBrush(COLOR_WINDOWFRAME));
          
          InflateRect(&irect, -1, -1);
          
          FillRect(dc, &irect, (HBRUSH)(COLOR_INFOBK + 1));
        } break;
        
      case ListViewItem:
        FillRect(dc, &irect, (HBRUSH)(COLOR_WINDOW + 1));
        break;
        
      case ListViewItemSelected:
        FillRect(dc, &irect, (HBRUSH)(COLOR_HIGHLIGHT + 1));
        break;
      
      case PanelControl:
        FillRect(dc, &irect, (HBRUSH)(COLOR_BTNFACE + 1));
        DrawEdge(dc, &irect, BDR_RAISEDOUTER, BF_RECT);
        break;
      
      case ProgressIndicatorBackground:
      case SliderHorzChannel: {
          FillRect(dc, &irect, (HBRUSH)(COLOR_BTNFACE + 1));
          
          DrawEdge(
            dc,
            &irect,
            EDGE_SUNKEN,
            BF_RECT);
        } break;
        
      case ProgressIndicatorBar: {
          int chunk = (irect.bottom - irect.top) / 2;
          RECT chunk_rect = irect;
          
          if(chunk < 2)
            chunk = 2;
            
          int x;
          for(x = irect.left; x + chunk <= irect.right; x += chunk + 2) {
            chunk_rect.left = x;
            chunk_rect.right = x + chunk;
            
            FillRect(dc, &chunk_rect, (HBRUSH)(COLOR_HIGHLIGHT + 1));
          }
          
          if(x < irect.right) {
            chunk_rect.left = x;
            chunk_rect.right = irect.right;
            
            FillRect(dc, &chunk_rect, (HBRUSH)(COLOR_HIGHLIGHT + 1));
          }
        } break;
        
      case SliderHorzThumb: {
          DrawFrameControl(
            dc,
            &irect,
            DFC_BUTTON,
            DFCS_BUTTONPUSH);
        } break;
        
      case CheckboxUnchecked:
      case CheckboxChecked:
      case CheckboxIndeterminate: {
          UINT _state = DFCS_BUTTONCHECK;
          
          if(type == CheckboxChecked)       _state |= DFCS_CHECKED;
          if(type == CheckboxIndeterminate) _state = DFCS_BUTTON3STATE | DFCS_CHECKED;
          
          switch(state) {
            case Disabled:       _state |= DFCS_INACTIVE; break;
            case PressedHovered: _state |= DFCS_PUSHED;   break;
            default: break;
          }
          
          DrawFrameControl(
            dc,
            &irect,
            DFC_BUTTON,
            _state);
        } break;
        
      case RadioButtonUnchecked:
      case RadioButtonChecked: {
          UINT _state = DFCS_BUTTONRADIO;
          
          if(type == RadioButtonChecked)    _state |= DFCS_CHECKED;
          
          switch(state) {
            case Disabled:       _state |= DFCS_INACTIVE; break;
            case PressedHovered: _state |= DFCS_PUSHED;   break;
            default: break;
          }
          
          if(irect.right - irect.left > 0) {
            double w = (irect.right - irect.left) * 0.75;
            double c = (irect.right + irect.left) * 0.5;
            irect.left  = (int)(c - w / 2);
            irect.right = (int)(c + w / 2);
          }
          
          if(irect.bottom - irect.top > 12) {
            double w = (irect.bottom - irect.top) * 0.75;
            double c = (irect.bottom + irect.top) * 0.5;
            irect.top    = (int)(c - w / 2);
            irect.bottom = (int)(c + w / 2);
          }
          
          DrawFrameControl(
            dc,
            &irect,
            DFC_BUTTON,
            _state);
        } break;
      
      case OpenerTriangleClosed:
      case OpenerTriangleOpened:
        ControlPainter::draw_container(control, canvas, type, state, rect);
        return;
        
      case NavigationBack:
      case NavigationForward: {
          int rw = irect.right - irect.left;
          int rh = irect.bottom - irect.top;
          int cx = irect.left + rw/2;
          int cy = irect.top + rh/2;
          rw = rh = std::min(rw, rh);
          
          irect.left = cx - rw/2;
          irect.right = irect.left + rw;
          irect.top = cy - rh/2;
          irect.bottom = irect.top + rh;
          
          FillRect(dc, &irect, (HBRUSH)(COLOR_BTNFACE + 1));
          if(state == PressedHovered)
            DrawEdge(dc, &irect, BDR_SUNKENOUTER, BF_RECT);
          else if(state == Hovered)
            DrawEdge(dc, &irect, BDR_RAISEDINNER, BF_RECT);
          
          need_vector_overlay = true;
        } break;
      
      case TabHeadAbuttingRight:
      case TabHeadAbuttingLeftRight:
      case TabHeadAbuttingLeft:
      case TabHead: {
          FillRect(dc, &irect, (HBRUSH)(COLOR_BTNFACE + 1));
          
          DWORD flags = 0;
          if(state == Disabled)
            flags |= BF_MONO;
          
          RECT edge = irect;
          edge.bottom = edge.top + 3;
          
          irect.top+= 2;
          DrawEdge(dc, &irect, BDR_RAISEDINNER | BDR_RAISEDOUTER, flags | BF_LEFT | BF_RIGHT);
          
          edge.right = irect.left + 3;
          DrawEdge(dc, &edge, BDR_RAISEDINNER | BDR_RAISEDOUTER, flags | BF_DIAGONAL | BF_TOP | BF_RIGHT);
          
          edge.right = irect.right;
          edge.left = edge.right - 3;
          DrawEdge(dc, &edge, BDR_RAISEDINNER | BDR_RAISEDOUTER, flags | BF_DIAGONAL | BF_BOTTOM | BF_RIGHT);
          
          irect.top = edge.top;
          irect.left+= 2;
          irect.right-= 2;
          DrawEdge(dc, &irect, BDR_RAISEDINNER | BDR_RAISEDOUTER, flags | BF_TOP);
          
        } break;
      
      case TabHeadBackground: {
          if(state == Disabled)
            DrawEdge(dc, &irect, BDR_RAISEDINNER | BDR_RAISEDOUTER, BF_ADJUST | BF_LEFT | BF_TOP | BF_RIGHT | BF_MONO);
          else
            DrawEdge(dc, &irect, BDR_RAISEDINNER | BDR_RAISEDOUTER, BF_ADJUST | BF_LEFT | BF_TOP | BF_RIGHT);
          
          FillRect(dc, &irect, (HBRUSH)(COLOR_BTNFACE + 1));
        } break;
      
      case TabBodyBackground: {
          if(state == Disabled)
            DrawEdge(dc, &irect, BDR_RAISEDINNER | BDR_RAISEDOUTER, BF_ADJUST | BF_LEFT | BF_BOTTOM | BF_RIGHT | BF_MONO);
          else
            DrawEdge(dc, &irect, BDR_RAISEDINNER | BDR_RAISEDOUTER, BF_ADJUST | BF_LEFT | BF_BOTTOM | BF_RIGHT);
          
          FillRect(dc, &irect, (HBRUSH)(COLOR_BTNFACE + 1));
        } break;
    }
  }
  
  if(surface) {
    cairo_surface_mark_dirty(surface);
    
    canvas.save();
    
    //cairo_reset_clip(canvas.cairo());
    
    cairo_set_source_surface(
      canvas.cairo(),
      surface,
      rect.x, rect.y);
      
    if(w != rect.width || h != rect.height) {
      cairo_matrix_t mat;
      mat.xx = w / rect.width;
      mat.yy = h / rect.height;
      mat.xy = mat.yx = 0;
      mat.x0 = -rect.x * mat.xx;
      mat.y0 = -rect.y * mat.yy;
      
      cairo_pattern_set_matrix(cairo_get_source(canvas.cairo()), &mat);
    }
    
    canvas.paint();
    
    canvas.restore();
    cairo_surface_destroy(surface);
  }
  else {
    cairo_surface_mark_dirty(cairo_get_target(canvas.cairo()));
  }
  
  if(dark_mode_is_fake(type) && control.is_using_dark_mode()) {
    rect.add_rect_path(canvas, false);
    
    canvas.set_color(Color::Black, 0.667);
    canvas.fill();
  }
  
  if(need_vector_overlay) {
    switch(type) {
      case NavigationBack:
      case NavigationForward: {
          float cx = rect.x + rect.width/2;
          float cy = rect.y + rect.height/2;
          rect.width = rect.height = std::min(rect.width, rect.height);
          rect.x = cx - rect.width/2;
          rect.y = cy - rect.height/2;
          
          if(state == PressedHovered) {
            rect.x+= 0.75;
            rect.y+= 0.75;
          }
          
          if(type == NavigationForward) {
            rect.x+= rect.width;
            rect.width = -rect.width;
          }
          
          Color old_col = canvas.get_color();
          
          canvas.move_to(rect.x + rect.width/4, rect.y + rect.height/2);
          canvas.rel_line_to( rect.width/4,                  rect.height/4);
          canvas.rel_line_to( rect.width/6,                  0);
          canvas.rel_line_to(-rect.width/4 + rect.width/12, -rect.height/4 + rect.height/12);
          canvas.rel_line_to( rect.width/4,                  0);
          canvas.rel_line_to(0,                             -rect.height/6);
          canvas.rel_line_to(-rect.width/4,                  0);
          canvas.rel_line_to( rect.width/4 - rect.width/12, -rect.height/4 + rect.height/12);
          canvas.rel_line_to(-rect.width/6,                  0);
          canvas.rel_line_to(-rect.width/4,                  rect.height/4);
          canvas.close_path();
          
          Color fill_col;
          Color stroke_col;
          switch(state) {
            case Disabled:
              fill_col = get_sys_color(COLOR_BTNFACE);
              stroke_col = get_sys_color(COLOR_GRAYTEXT);
              break;
            
            default:
              fill_col = Color::White;
              stroke_col = Color::Black;
              break;
          }
          
          canvas.set_color(fill_col);
          canvas.fill_preserve();
          canvas.set_color(stroke_col);
          canvas.stroke();
          
          canvas.set_color(old_col);
        } break;
    }
  }
}

SharedPtr<BoxAnimation> Win32ControlPainter::control_transition(
  FrontEndReference  widget_id,
  Canvas            &canvas,
  ContainerType      type1,
  ContainerType      type2,
  ControlState       state1,
  ControlState       state2,
  RectangleF         rect
) {
  if(!Win32Themes::GetThemeTransitionDuration || !widget_id.is_valid())
    return nullptr;
  
  BOOL has_animations = TRUE;
  if(SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &has_animations, FALSE) && !has_animations)
    return nullptr;
  
  ControlContext &control = ControlContext::find(FrontEndObject::find_cast<Box>(widget_id));
  
  bool repeat = false;
  if(type2 == DefaultPushButton && state1 == Normal && state2 == Normal) {
    if(control.is_foreground_window()) {
      state2 = Hot;
      repeat = true;
    }
  }
  
  if( state1 == Normal                      &&
      (state2 == Hot || state2 == Hovered)  &&
      type2 == PaletteButton)
  {
    return nullptr;
  }
  
  if(type1 == type2 && state1 == state2)
    return nullptr;
  
  int theme_part, theme_state1, theme_state2;
  HANDLE theme = get_control_theme(control, type1, state1, &theme_part, &theme_state1);
  get_control_theme(control, type2, state2, &theme_part, &theme_state2);
  if(!theme)
    return nullptr;
  
  if(theme_state1 == theme_state2)
    return nullptr;
  
  switch(type2) {
    case PushButton:
    case DefaultPushButton:
    case PaletteButton:
    case TabHeadAbuttingRight:
    case TabHeadAbuttingLeftRight:
    case TabHeadAbuttingLeft:
    case TabHead: {
        if(state2 == PressedHovered/* || state1 == Normal*/)
          return nullptr;
      } break;
  }
  
  DWORD duration = 0;
  /* TMT_TRANSITIONDURATIONS = 6000 */
  Win32Themes::GetThemeTransitionDuration(
    theme, theme_part, theme_state1, theme_state2, 6000, &duration);
    
  if(duration > 0) {
    Point p0 = canvas.current_pos();
    
    RectangleF rect1 = rect;
    rect1.x = p0.x;
    
    if(type2 == InputField) // bigger buffer for glow rectangle
      rect1.grow(4.5, 4.5);
    
    SharedPtr<LinearTransition> anim = new LinearTransition(
      widget_id,
      canvas,
      rect1,
      duration / 1000.0);
      
    if( !anim->box_id.is_valid() || 
        !anim->buf1 || 
        !anim->buf2)
    {
      return 0;
    }
    
    anim->repeat = repeat;
    
    rect.x-= p0.x;
    rect.y-= p0.y;
    draw_container(
      control,
      *anim->buf1->canvas(),
      type1,
      state1,
      rect);
      
    draw_container(
      control,
      *anim->buf2->canvas(),
      type2,
      state2,
      rect);
      
    return anim;
  }
  
  return nullptr;
}

Vector2F Win32ControlPainter::container_content_offset(
  ControlContext &control, 
  ContainerType   type,
  ControlState    state
) {
  Vector2F delta = ControlPainter::container_content_offset(control, type, state);
  if(delta.x != 0 || delta.y != 0) {
    double scale = control.dpi() / 72.0;
    if(delta.x != 0)
      delta.x = round(delta.x * scale) / scale;
    if(delta.y != 0)
      delta.y = round(delta.y * scale) / scale;
  }
  return delta;
}

bool Win32ControlPainter::container_hover_repaint(ControlContext &control, ContainerType type) {
  switch(type) {
    case FramelessButton:
    case NoContainerType:
    case PanelControl:
    case PopupPanel:
    case TabBodyBackground:
    case TabHeadBackground:
    case TooltipWindow:
      return false;
    
    case GenericButton:
      return ControlPainter::container_hover_repaint(control, type);
  }
  
  return Win32Themes::OpenThemeData && 
         Win32Themes::CloseThemeData && 
         Win32Themes::DrawThemeBackground;
}

void Win32ControlPainter::system_font_style(ControlContext &control, Style *style) {
  NONCLIENTMETRICSW nonclientmetrics;
  LOGFONTW *logfont = nullptr;
  
  float dpi_scale = 1.0f;
  
  if(Win32Themes::GetThemeSysFont) {
    logfont = &nonclientmetrics.lfMessageFont;
    if(!Win32Themes::GetThemeSysFont(nullptr, 0x0325/*TMT_MSGBOXFONT*/, logfont))
      logfont = nullptr;
    
    if(logfont) {
      // GetThemeSysFont gets metrics in dots per inch for the current logical screen
      
      dpi_scale = Win32HighDpi::get_dpi_for_system() / (float)control.dpi();
    }
  }
  
  if(!logfont) {
    nonclientmetrics.cbSize = sizeof(nonclientmetrics);
    Win32HighDpi::get_nonclient_metrics_for_dpi(&nonclientmetrics, control.dpi());
    dpi_scale = 1.0f;
    logfont = &nonclientmetrics.lfMessageFont;
  }
  
  style->set(FontFamilies, String::FromUcs2((const uint16_t *)logfont->lfFaceName));
  style->set(FontSize, abs(logfont->lfHeight) * dpi_scale * 3 / 4.f);
  
  if(logfont->lfWeight > FW_NORMAL)
    style->set(FontWeight, FontWeightBold);
  else
    style->set(FontWeight, FontWeightPlain);
    
  style->set(FontSlant, logfont->lfItalic ? FontSlantItalic : FontSlantPlain);
}

Color Win32ControlPainter::selection_color(ControlContext &control) {
  return get_sys_color(COLOR_HIGHLIGHT);
}

Color Win32ControlPainter::win32_button_face_color(bool dark) {
  if(dark) {
    // TODO: do not hardcode dark-mode COLOR_BTNFACE but get it from the system somehow
    // dark mode Popup menu background: 0x2B2B2B
    return Color::from_rgb24(0x2B2B2B);
  }
  
  return Color::from_bgr24(GetSysColor(COLOR_BTNFACE));
}

float Win32ControlPainter::scrollbar_width() {
  return GetSystemMetrics(SM_CXHTHUMB) * 3 / 4.f;
}

void Win32ControlPainter::paint_scrollbar_part(
  ControlContext     &control, 
  Canvas             &canvas,
  ScrollbarPart       part,
  ScrollbarDirection  dir,
  ControlState        state,
  RectangleF          rect
) {
  if(part == ScrollbarNowhere)
    return;
  
  rect.pixel_align(canvas, false);
  int dc_x = 0;
  int dc_y = 0;
  
  Vector2F device_size = canvas.user_to_device_dist(rect.size());
  device_size.x = fabs(device_size.x);
  device_size.y = fabs(device_size.y);
  int w = (int)ceil(device_size.x);
  int h = (int)ceil(device_size.y);
  
  if(w == 0 || h == 0)
    return;
  
  HDC dc = safe_cairo_win32_surface_get_dc(cairo_get_target(canvas.cairo()));
  cairo_surface_t *surface = nullptr;
  
  if(dc) {
    cairo_matrix_t ctm = canvas.get_matrix();
    
    if(ctm.xx > 0 && ctm.yy > 0 && ctm.xy == 0 && ctm.yx == 0) {
      Point dc_pos = canvas.user_to_device(rect.top_left());
      dc_x = (int)dc_pos.x;
      dc_y = (int)dc_pos.y;
      
      cairo_surface_flush(cairo_get_target(canvas.cairo()));
    }
    else
      dc = 0;
  }
  
  if(!dc) {
    if( Win32Themes::OpenThemeData && 
        Win32Themes::CloseThemeData && 
        Win32Themes::DrawThemeBackground
    ) {
      surface = cairo_win32_surface_create_with_dib(
                  CAIRO_FORMAT_ARGB32,
                  w,
                  h);
    }
    else {
      surface = cairo_win32_surface_create_with_dib(
                  CAIRO_FORMAT_RGB24,
                  w,
                  h);
    }
    
    dc = safe_cairo_win32_surface_get_dc(surface);
    if(!dc) {
      ControlPainter::paint_scrollbar_part(control, canvas, part, dir, state, rect);
      return;
    }
  }
  
  RECT irect;
  irect.left   = dc_x;
  irect.top    = dc_y;
  irect.right  = dc_x + w;
  irect.bottom = dc_y + h;
  
  if( Win32Themes::OpenThemeData  &&
      Win32Themes::CloseThemeData &&
      Win32Themes::DrawThemeBackground)
  {
    HANDLE scrollbar_theme = w32cp_cache.scrollbar_theme(control.dpi());
    if(!scrollbar_theme)
      goto FALLBACK;
      
    int _part = 0;
    int _state = 0;
    
    switch(part) {
      case ScrollbarUpLeft:
      case ScrollbarDownRight:
        _part = 1; // SBP_ARROWBTN
        break;
        
      case ScrollbarThumb: {
          if(dir == ScrollbarHorizontal)
            _part = 2;
          else
            _part = 3;
        } break;
        
      case ScrollbarLowerRange: {
          if(dir == ScrollbarHorizontal)
            _part = 4;
          else
            _part = 6;
        } break;
        
      case ScrollbarUpperRange: {
          if(dir == ScrollbarHorizontal)
            _part = 5;
          else
            _part = 7;
        } break;
        
      case ScrollbarNowhere:
      case ScrollbarSizeGrip: _part = 10; break;
    }
    
    if(_part == 1) { // arrow button
      switch(state) {
        case Disabled:        _state = 4; break;
        case PressedHovered:  _state = 3; break;
        case Hovered:         _state = 2; break;
        case Hot:
        case Pressed:
        case Normal:          _state = 1; break;
        
        default: ;
      }
      
      if(_state) {
        if(dir == ScrollbarHorizontal) {
          if(part == ScrollbarDownRight)
            _state += 12;
          else
            _state += 8;
        }
        else {
          if(part == ScrollbarDownRight)
            _state += 4;
        }
      }
      else {
        if(dir == ScrollbarHorizontal) {
          if(part == ScrollbarDownRight)
            _state = 20;
          else
            _state = 19;
        }
        else {
          if(part == ScrollbarDownRight)
            _state = 18;
          else
            _state = 17;
        }
      }
    }
    else if(_part == 10) { // size box
      _state = 1;
    }
    else { // thumbs, ranges
      switch(state) {
        case Pressed:
        case Normal:         _state = 1; break;
        case Hot:            _state = 2; break;
        case PressedHovered: _state = 3; break;
        case Disabled:       _state = 4; break;
        case Hovered:        _state = 5; break;
      }
    }
    
    Win32Themes::DrawThemeBackground(
      scrollbar_theme,
      dc,
      _part,
      _state,
      &irect,
      0);
      
    if(part == ScrollbarThumb) {
      if(dir == ScrollbarHorizontal) {
        if(rect.width >= rect.height) {
          Win32Themes::DrawThemeBackground(
            scrollbar_theme,
            dc,
            8,
            _state,
            &irect,
            0);
        }
      }
      else if(rect.height >= rect.width) {
        Win32Themes::DrawThemeBackground(
          scrollbar_theme,
          dc,
          9,
          _state,
          &irect,
          0);
      }
    }
    
//    Win32Themes::CloseThemeData(theme);
  }
  else {
  FALLBACK: ;
    UINT _type = DFC_SCROLL;
    UINT _state = 0;
    
    bool bg = false;
    
    switch(part) {
      case ScrollbarUpLeft: {
          if(dir == ScrollbarHorizontal)
            _state = DFCS_SCROLLLEFT;
          else
            _state = DFCS_SCROLLUP;
        } break;
        
      case ScrollbarDownRight: {
          if(dir == ScrollbarHorizontal)
            _state = DFCS_SCROLLRIGHT;
          else
            _state = DFCS_SCROLLDOWN;
        } break;
        
      case ScrollbarSizeGrip: _state = DFCS_SCROLLSIZEGRIP; break;
      
      case ScrollbarThumb:
        _type = DFC_BUTTON;
        _state = DFCS_BUTTONPUSH;
        break;
        
      default: bg = true;
    }
    
    if(!bg) {
      switch(state) {
        case PressedHovered:  _state |= DFCS_PUSHED;   break;
        case Hot:
        case Hovered:         _state |= DFCS_HOT;      break;
        case Disabled:        _state |= DFCS_INACTIVE; break;
        default: break;
      }
      
      DrawFrameControl(
        dc,
        &irect,
        _type,
        _state);
    }
  }
  
  if(surface) {
    cairo_surface_mark_dirty(surface);
    
    canvas.save();
    
    cairo_reset_clip(canvas.cairo());
    
    cairo_set_source_surface(
      canvas.cairo(),
      surface,
      rect.x, rect.y);
      
    if(w != rect.width || h != rect.height) {
      cairo_matrix_t mat;
      mat.xx = w / rect.width;
      mat.yy = h / rect.height;
      mat.xy = mat.yx = 0;
      mat.x0 = -rect.x * mat.xx;
      mat.y0 = -rect.y * mat.yy;
      
      cairo_pattern_set_matrix(cairo_get_source(canvas.cairo()), &mat);
    }
    
    canvas.paint();
    
    canvas.restore();
    cairo_surface_destroy(surface);
  }
  else {
    cairo_surface_mark_dirty(cairo_get_target(canvas.cairo()));
  }
}

void Win32ControlPainter::draw_menubar(HDC dc, RECT *rect, bool dark_mode) {
  if( Win32Themes::OpenThemeData  &&
      Win32Themes::CloseThemeData &&
      Win32Themes::DrawThemeBackground)
  {
    HANDLE theme;
    if(dark_mode) {
      // Windows 10, 1903 (Build 18362): DarkMode::MENU exists but only gives dark colors for popup menu parts
      theme = Win32Themes::OpenThemeData(0, L"DarkMode::MENU");
      if(theme) {  
        Win32Themes::DrawThemeBackground(
          theme,
          dc,
          9, // MENU_POPUPBACKGROUND
          0,
          rect,
          0);
        
        Win32Themes::CloseThemeData(theme);
        return;
      }
    }
    
    theme = Win32Themes::OpenThemeData(0, L"MENU");
    if(!theme)
      goto FALLBACK;
      
    Win32Themes::DrawThemeBackground(
      theme,
      dc,
      7,  // MENU_BARBACKGROUND
      0,  // MB_ACTIVE
      rect,
      0);
      
    Win32Themes::CloseThemeData(theme);
  }
  else {
  FALLBACK: 
    FillRect(dc, rect, GetSysColorBrush(COLOR_BTNFACE));
  }
  
  if(dark_mode) {
    BitBlt(dc, rect->left, rect->top, rect->right - rect->left, rect->bottom - rect->top, nullptr, 0, 0, DSTINVERT);
  }
}

void Win32ControlPainter::draw_menubar_itembg(HDC dc, RECT *rect, ControlState state, bool dark_mode) {
  if( Win32Themes::OpenThemeData  && 
      Win32Themes::CloseThemeData && 
      Win32Themes::DrawThemeBackground) 
  {
    // Windows 10, 1903 (Build 18362): DarkMode::MENU exists but only gives dark colors for popup menu parts
    HANDLE theme = Win32Themes::OpenThemeData(0, L"MENU");
    
    if(!theme)
      goto FALLBACK;
      
    int _state;
    switch(state) {
      case Hovered:        _state = 2; break;
      case Pressed:        _state = 3; break;
      case PressedHovered: _state = 3; break;
      default:             _state = 1; break;
    }
    
    Win32Themes::DrawThemeBackground(
      theme,
      dc,
      8,
      _state,
      rect,
      0);
      
    Win32Themes::CloseThemeData(theme);
    return;
  }
  
FALLBACK:
  if(state == Normal) 
    return;
  
  UINT edge;
  switch(state) {
    case Hovered:        edge = BDR_RAISEDINNER; break;
    case Pressed:        edge = BDR_SUNKENOUTER; break;
    case PressedHovered: edge = BDR_SUNKENOUTER; break;
    default:             edge = 0; break;
  }
  RECT edge_rect = *rect;
  edge_rect.bottom-= 1;
  edge_rect.right-= 1;
  DrawEdge(dc, &edge_rect, edge, BF_RECT);

  if(dark_mode) {
    BitBlt(dc, rect->left, rect->top, rect->right - rect->left, rect->bottom - rect->top, nullptr, 0, 0, DSTINVERT);
  }
}

HANDLE Win32ControlPainter::get_control_theme(
  ControlContext &control, 
  ContainerType   type,
  ControlState    state,
  int            *theme_part,
  int            *theme_state
) {
  w32cpinfo.style_observations.register_observer();

  if(!theme_part) {
    static int dummy_part;
    theme_part = &dummy_part;
  }
  
  if(!theme_state) {
    static int dummy_state;
    theme_state = &dummy_state;
  }
  
  *theme_part = 0;
  *theme_state = 0;
  if(!Win32Themes::OpenThemeData)
    return 0;
    
  HANDLE theme = nullptr;
  
  switch(type) {
    case PushButton:
    case DefaultPushButton:
    case CheckboxUnchecked:
    case CheckboxChecked:
    case CheckboxIndeterminate:
    case RadioButtonUnchecked:
    case RadioButtonChecked: 
      theme = w32cp_cache.button_theme(control.dpi(), control.is_using_dark_mode());
      break;
      
    case PaletteButton: 
      theme = w32cp_cache.toolbar_theme(control.dpi(), control.is_using_dark_mode());
      break;
      
    case InputField: 
      theme = w32cp_cache.edit_theme(control.dpi(), control.is_using_dark_mode());
      break;
    
    case AddressBandBackground:
      theme = w32cp_cache.addressband_theme(control.dpi(), control.is_using_dark_mode());
      break;
      
    case AddressBandInputField: 
      theme = w32cp_cache.addressband_combobox_theme(control.dpi(), control.is_using_dark_mode());
      break;
      
    case AddressBandGoButton:
      theme = w32cp_cache.toolbar_go_theme(control.dpi());
      break;
      
    case ListViewItem:
    case ListViewItemSelected:
      theme = w32cp_cache.explorer_listview_theme(control.dpi(), control.is_using_dark_mode());
      break;
    
    case OpenerTriangleClosed:
    case OpenerTriangleOpened:
      theme = w32cp_cache.explorer_treeview_theme(control.dpi(), control.is_using_dark_mode());
      break;
      
    case PanelControl:
    case TabHeadAbuttingRight:
    case TabHeadAbuttingLeftRight:
    case TabHeadAbuttingLeft:
    case TabHead:
    case TabHeadBackground:
    case TabBodyBackground:
      theme = w32cp_cache.tab_theme(control.dpi());
      break;
    
    case ProgressIndicatorBackground:
    case ProgressIndicatorBar:
      theme = w32cp_cache.progress_theme(control.dpi());
      break;
    
    case SliderHorzChannel:
    case SliderHorzThumb: 
      theme = w32cp_cache.slider_theme(control.dpi());
      break;
      
    case TooltipWindow: 
      theme = w32cp_cache.tooltip_theme(control.dpi(), control.is_using_dark_mode());
      break;
      
    case NavigationBack: 
    case NavigationForward: 
      theme = w32cp_cache.navigation_theme(control.dpi(), control.is_using_dark_mode());
      break;
    
    default: return nullptr;
  }
  
  if(!theme)
    return nullptr;
  
  switch(type) {
    case TabHeadAbuttingRight:
    case TabHeadAbuttingLeftRight:
    case TabHeadAbuttingLeft:
    case TabHead:
      break;
    
    default:
      if(state == Pressed) {
        if(!control.is_focused_widget())
          state = Normal;
      }
      else if(state == PressedHovered) {
        if(!control.is_focused_widget())
          state = Hovered;
      }
      break;
  }
  
  switch(type) {
    case GenericButton:
    case PushButton:
    case DefaultPushButton:
    case AddressBandGoButton:
    case PaletteButton: {
        *theme_part = 1;//BP_PUSHBUTTON / TP_BUTTON
        
        switch(state) {
          case Disabled:        *theme_state = 4; break;
          case PressedHovered:  *theme_state = 3; break;
          case Hovered:         *theme_state = 2; break;
          case Hot: {
              if(type == DefaultPushButton) {
                *theme_state = 6;
              }
              else
                *theme_state = 2;
            } break;
            
          case Pressed:
          case Normal:
            if(type == DefaultPushButton) {
              *theme_state = 5;
            }
            else
              *theme_state = 1;
        }
      } break;
      
    case AddressBandInputField: {
        *theme_part = 4;//CP_BORDER
        
        switch(state) {
          case Normal:         *theme_state = 1; break;
          case Hot:
          case Hovered:        *theme_state = 2; break;
          case Pressed:
          case PressedHovered: *theme_state = 3; break; // = focused
          case Disabled:       *theme_state = 4; break;
        }
      } break;
      
    case InputField: {
        *theme_part = 6;//EP_EDITBORDER_NOSCROLL
        
        switch(state) {
          case Normal:         *theme_state = 1; break;
          case Hot:
          case Hovered:        *theme_state = 2; break;
          case Pressed:
          case PressedHovered: *theme_state = 3; break; // = focused
          case Disabled:       *theme_state = 4; break;
        }
      } break;
    
    case AddressBandBackground: {
        *theme_part = 1; // ABBACKGROUND
        
        switch(state) {
          case Normal:         *theme_state = 1; break;
          case Hot:
          case Hovered:        *theme_state = 2; break;
          case Pressed:
          case PressedHovered: *theme_state = 4; break; // = focused
          case Disabled:       *theme_state = 3; break;
        }
      } break;
    
    case ListViewItem: {
        *theme_part = 1;//LVP_LISTITEM
        
        switch(state) {
          case Normal:          theme = nullptr; break; // the LISS_NORMAL part (1) has a border
          case Hot:
          case Hovered:        *theme_state = 2; break;
          case Pressed:
          case PressedHovered: *theme_state = 3; break;
          case Disabled:        theme = nullptr; break; // the LISS_DISABLED part (4) has a border
        }
      } break;
      
    case ListViewItemSelected: {
        *theme_part = 1;//LVP_LISTITEM
        
        switch(state) {
          case Pressed:
          case Normal:         *theme_state = 3; break;
          case Hot:
          case Hovered:        *theme_state = 6; break;
          case PressedHovered: *theme_state = 6; break;
          case Disabled:       *theme_state = 5; break;
        }
      } break;
      
    case TooltipWindow: {
        *theme_part  = 1; // TTP_STANDARD
        *theme_state = 1;
      } break;
    
    case PanelControl: {
        *theme_part  = 9; // TABP_PANE
        *theme_state = 0;
      } break;
    
    case ProgressIndicatorBackground: {
        if(Win32Themes::IsThemePartDefined(theme, 11, 0))
          *theme_part = 11; // PP_TRANSPARENTBAR
        else
          *theme_part = 1;  // PP_BAR
          
        *theme_state = 1;
      } break;
      
    case ProgressIndicatorBar: {
        if(Win32Themes::IsThemePartDefined(theme, 5, 0))
          *theme_part = 5; // PP_FILL
        else
          *theme_part = 3; // PP_CHUNK
          
        *theme_state = 1;
      } break;
      
    case SliderHorzChannel: {
        *theme_part  = 1; // TKP_TRACK
        *theme_state = 1;
      } break;
      
    case SliderHorzThumb: {
        *theme_part = 3; // TKP_THUMB
        switch(state) {
          case Normal:         *theme_state = 1; break;
          case Pressed:        *theme_state = 4; break;
          case Hot:
          case Hovered:        *theme_state = 2; break;
          case PressedHovered: *theme_state = 3; break;
          case Disabled:       *theme_state = 5; break;
        }
      } break;
      
    case CheckboxUnchecked: {
        *theme_part = 3; // BP_CHECKBOX
        switch(state) {
          case Normal:         *theme_state = 1; break;
          case Pressed:
          case Hot:
          case Hovered:        *theme_state = 2; break;
          case PressedHovered: *theme_state = 3; break;
          case Disabled:       *theme_state = 4; break;
        }
      } break;
      
    case CheckboxChecked: {
        *theme_part = 3; // BP_CHECKBOX
        switch(state) {
          case Normal:         *theme_state = 5; break;
          case Pressed:
          case Hot:
          case Hovered:        *theme_state = 6; break;
          case PressedHovered: *theme_state = 7; break;
          case Disabled:       *theme_state = 8; break;
        }
      } break;
      
    case CheckboxIndeterminate: {
        *theme_part = 3; // BP_CHECKBOX
        switch(state) {
          case Normal:         *theme_state = 9; break;
          case Pressed:
          case Hot:
          case Hovered:        *theme_state = 10; break;
          case PressedHovered: *theme_state = 11; break;
          case Disabled:       *theme_state = 12; break;
        }
      } break;
      
    case RadioButtonUnchecked: {
        *theme_part = 2; // BP_RADIOBUTTON
        switch(state) {
          case Normal:         *theme_state = 1; break;
          case Pressed:
          case Hot:
          case Hovered:        *theme_state = 2; break;
          case PressedHovered: *theme_state = 3; break;
          case Disabled:       *theme_state = 4; break;
        }
      } break;
      
    case RadioButtonChecked: {
        *theme_part = 2; // BP_RADIOBUTTON
        switch(state) {
          case Normal:         *theme_state = 5; break;
          case Pressed:
          case Hot:
          case Hovered:        *theme_state = 6; break;
          case PressedHovered: *theme_state = 7; break;
          case Disabled:       *theme_state = 8; break;
        }
      } break;
    
    case OpenerTriangleClosed: {
        *theme_state = 1; // GLPS_CLOSED / HGLPS_CLOSED
        switch(state) {
          case Disabled:
          case Normal:         *theme_part = 2; break; // TVP_GLYPH
          case Pressed:
          case Hot:
          case Hovered:        
          case PressedHovered: *theme_part = 4; break; // TVP_HOTGLYPH
        }
      } break;
      
    case OpenerTriangleOpened: {
        *theme_state = 2; // GLPS_OPENED / HGLPS_OPENED
        switch(state) {
          case Disabled:
          case Normal:         *theme_part = 2; break; // TVP_GLYPH
          case Pressed:
          case Hot:
          case Hovered:        
          case PressedHovered: *theme_part = 4; break; // TVP_HOTGLYPH
        }
      } break;
    
    case NavigationBack:
    case NavigationForward: {
        *theme_part = type == NavigationBack ? 1 : 2; // NAV_BACKBUTTON, NAV_FORWARDBUTTON
        *theme_state = 1; // NAV_BB_NORMAL
        switch(state) {
          case Disabled:       *theme_state = 4; break; // NAV_BB_DISABLED
          case Normal:
          case Pressed:        *theme_state = 1; break; // NAV_BB_NORMAL
          case Hot:
          case Hovered:        *theme_state = 2; break; // NAV_BB_HOT
          case PressedHovered: *theme_state = 3; break; // NAV_BB_PRESSED
        }
    } break;
    
    case TabHeadAbuttingRight:
    case TabHeadAbuttingLeftRight:
    case TabHeadAbuttingLeft:
    case TabHead: {
        *theme_state = 1; // TIS_NORMAL
        switch(state) {
          case Disabled:       *theme_state = 4; break; // TIS_DISABLED
          case Normal:         *theme_state = 1; break; // TIS_NORMAL
          case Hot:            *theme_state = 1; break; // TIS_NORMAL
          case Hovered:        *theme_state = 2; break; // TIS_HOT
          case Pressed:        *theme_state = 3; break; // TIS_SELECTED
          case PressedHovered: *theme_state = 3; break; // TIS_SELECTED
        }
    } break;
    
    case TabHeadBackground: 
    case TabBodyBackground: {
        *theme_part = 9; // TABP_PANE  // TODO: combine with TABP_BODY ....
        *theme_state = 0;
    } break;
    
    default: break;
  }
  
  switch(type) {
    case TabHead: // Windows 10: TABP_TABITEM has no line on the left, looks exactly like TABP_TABITEMBOTHEDGE
      //*theme_part = 1; // TABP_TABITEM
      //break;
    case TabHeadAbuttingRight: 
      *theme_part = 2; // TABP_TABITEMLEFTEDGE
      break;
    case TabHeadAbuttingLeft:
      *theme_part = 3; // TABP_TABITEMRIGHTEDGE
      break;
    case TabHeadAbuttingLeftRight:
      *theme_part = 4; // TABP_TABITEMBOTHEDGE
      break;
  }
  
  return theme;
}

void Win32ControlPainter::clear_cache() {
  w32cp_cache.clear();
}
