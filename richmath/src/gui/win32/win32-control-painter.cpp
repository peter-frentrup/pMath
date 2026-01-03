#define WINVER 0x0600

#include <gui/win32/win32-control-painter.h>

#include <boxes/box.h>
#include <eval/observable.h>
#include <graphics/context.h>
#include <gui/win32/api/win32-highdpi.h>
#include <gui/win32/api/win32-themes.h>
#include <gui/win32/api/win32-version.h>
#include <gui/win32/basic-win32-widget.h>
#include <util/array.h>
#include <util/style.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cwchar>

#include <windows.h>
#include <cairo-win32.h>

#ifdef min
#  undef min
#endif
#ifdef max
#  undef max
#endif


#ifndef WM_DWMCOMPOSITIONCHANGED
#define WM_DWMCOMPOSITIONCHANGED   0x031E
#endif

using namespace richmath;
using namespace std;

class Win32ControlPainterInfo final: public BasicWin32Widget {
  public:
    Win32ControlPainterInfo()
      : BasicWin32Widget(0, 0, 0, 0, 0, 0, nullptr)
    {
      init(); // total exception!!! Calling init in consructor is only allowd since this class is final
    }
    
    ~Win32ControlPainterInfo() {
      begin_destruction(); // total exception: no dynamic memory management => no safe_destroy()
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

namespace richmath {
  class Win32ControlPainter::Impl {
    public:
      explicit Impl(Win32ControlPainter &self);
      
      static Color get_sys_color(int index);
      static bool dark_mode_is_fake(ContainerType type);
      
      static void draw_toggle_switch_channel(Canvas &canvas, RectangleF rect, ControlState state, bool active, bool dark);
      static void draw_toggle_switch_thumb(  Canvas &canvas, RectangleF rect, ControlState state, bool active, bool dark);
      
      bool try_get_sizing_margin(ControlContext &control, ContainerType type, ControlState state, Win32Themes::MARGINS *mar);
      
    private:
      Win32ControlPainter &self;
  };
  
  static const Color InputFieldBlurColor = Color::from_rgb24(0x8080FF);
};

static class Win32ControlPainterCache {
  public:
    Win32ControlPainterCache() {
      has_dark_menu_color  = 0;
      has_light_menu_color = 0;
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
    
    bool has_menu_color(bool dark) { return dark ? has_dark_menu_color : has_light_menu_color; }
    COLORREF menu_color(bool dark) { return dark ? dark_menu_color_value : light_menu_color_value; }
    void cache_menu_color(bool dark, COLORREF val) { 
      if(dark) {
        dark_menu_color_value = val;
        has_dark_menu_color = 1; 
      } 
      else {
        light_menu_color_value = val;
        has_light_menu_color = 1; 
      }
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
      
      has_dark_menu_color  = 0;
      has_light_menu_color = 0;
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
    
    COLORREF light_menu_color_value;
    COLORREF dark_menu_color_value;
    unsigned has_light_menu_color : 1;
    unsigned has_dark_menu_color : 1;
} w32cp_cache;

//{ class Win32ControlPainter ...

Win32ControlPainter Win32ControlPainter::win32_painter;

void Win32ControlPainter::done() {
  win32_painter.clear_cache();
  w32cpinfo.style_observations.notify_all(); // to clear the observers array
}

Win32ControlPainter::Win32ControlPainter()
  : ControlPainter()
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
  HANDLE theme = get_control_theme(control, type, ControlState::Normal, &theme_part, &theme_state);
  
  switch(type) {
    case ContainerType::InputField: {
        extents->width +=   4.5;
        extents->ascent +=  3;
        extents->descent += 2.25;
        round_extents(canvas, extents);
      } return;
      
    case ContainerType::AddressBandInputField: {
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
    
    case ContainerType::AddressBandBackground: 
      if(!theme) {
        extents->width +=   1.5;
        extents->ascent +=  0.75;
        extents->descent += 0.75;
        round_extents(canvas, extents);
        return;
      }
      break;
      
    case ContainerType::ListViewItem:
    case ContainerType::ListViewItemSelected:
      ControlPainter::calc_container_size(control, canvas, type, extents);
      round_extents(canvas, extents);
      return;
    
    case ContainerType::Panel:
    case ContainerType::PopupPanel:
      if(theme && Win32Themes::GetThemeMargins) {
        extents->width +=   9.0;
        extents->ascent +=  4.5;
        extents->descent += 4.5;
      }
      break;
    
    case ContainerType::TabPanelCenter: 
      if(theme && Win32Themes::GetThemeMargins) {
        extents->width +=   6.0;
        extents->ascent +=  3.0;
        extents->descent += 3.0;
      }
      break;
    
    case ContainerType::TabBodyBackground: 
      if(theme && Win32Themes::GetThemeMargins) {
        extents->width +=   9.0;
        extents->ascent +=  3.0;
        extents->descent += 4.5;
      }
      break;
      
    case ContainerType::ProgressIndicatorBackground: {
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
      
    case ContainerType::ProgressIndicatorBar: {
        if(!theme || theme_part != 5) {
          extents->width -=   4.5;
          extents->ascent -=  2.25;
          extents->descent -= 2.25;
        }
        round_extents(canvas, extents);
      } return;
      
    case ContainerType::HorizontalSliderChannel: {
        extents->width = 8 * canvas.get_font_size();
        
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
      
    case ContainerType::VerticalSliderChannel: {
        extents->ascent = 8 * canvas.get_font_size();
        extents->descent = 0;
        
        float dx = 4;
        float dy = 0;
        if(theme && Win32Themes::GetThemePartSize) {
          SIZE size;
          if(SUCCEEDED(Win32Themes::GetThemePartSize(theme, nullptr, theme_part, theme_state, nullptr, Win32Themes::TS_TRUE, &size))) {
            dy = size.cy;
          }
        }
        canvas.device_to_user_dist(&dx, &dy);
        extents->width = max(fabs(dx), fabs(dy));
      } return;
      
    case ContainerType::HorizontalSliderThumb:
    case ContainerType::HorizontalSliderDownArrowButton:
    case ContainerType::HorizontalSliderUpArrowButton: {
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
      
    case ContainerType::VerticalSliderThumb:
    case ContainerType::VerticalSliderLeftArrowButton:
    case ContainerType::VerticalSliderRightArrowButton: {
        if(theme && Win32Themes::GetThemePartSize) {
          SIZE size;
          if(SUCCEEDED(Win32Themes::GetThemePartSize(theme, nullptr, theme_part, theme_state, nullptr, Win32Themes::TS_TRUE, &size))) {
            // TKP_THUMBVERT wrongly gives same TS_TRUE size as TKP_THUMB, so we play safe and use max(cx, cy)
            extents->width = 0.75 * std::max(size.cx, size.cy); // 0.75 * size.cx;
            //extents->width = 0.75 * size.cx;
            extents->ascent = 0.5 * extents->width;
            extents->descent = 0;
            round_extents(canvas, extents);
            return;
          }
        }
      } break;
      
    case ContainerType::TooltipWindow: {
        if(!theme) {
          extents->width +=   6;
          extents->ascent +=  3;
          extents->descent += 3;
          round_extents(canvas, extents);
          return;
        }
      } break;
    
    case ContainerType::NavigationBack:
    case ContainerType::NavigationForward: 
      if(theme && Win32Themes::GetThemePartSize) {
        SIZE size;
        if(SUCCEEDED(Win32Themes::GetThemePartSize(theme, nullptr, theme_part, theme_state, nullptr, Win32Themes::TS_TRUE, &size))) {
          /* For the Navigation parts, TS_TRUE on 144 DPI monitor gives 45x45 and on
             96 PDI monitor gives 30, so here the TS_TRUE size is not in terms of the primary monitor DPI (unlike ContainerType::PushButton?)
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
      
    case ContainerType::TabHeadBackground:
    case ContainerType::TabPanelTopLeft:
    case ContainerType::TabPanelTopCenter:
    case ContainerType::TabPanelTopRight:
    case ContainerType::TabPanelCenterLeft:
    case ContainerType::TabPanelCenterRight:
    case ContainerType::TabPanelBottomLeft:
    case ContainerType::TabPanelBottomCenter:
    case ContainerType::TabPanelBottomRight:
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
        float extra = 0.75 * size.cy - extents->height();
        extents->ascent  += 0.5f * extra;
        extents->descent += 0.5f * extra;
      }
      
      if(type == ContainerType::TabBodyBackground) {
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
  HANDLE theme = get_control_theme(control, type, ControlState::Normal, &theme_part, &theme_state);
  
  switch(type) {
    case ContainerType::TooltipWindow: {
        if(!theme) {
          *radii = BoxRadius(0);
          return;
        }
        
        if(Win32Version::is_windows_8_or_newer() && !Win32Version::is_windows_11_or_newer()) {
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
    
  if(type == ContainerType::AddressBandBackground)
    type = ContainerType::AddressBandInputField;
  
  int theme_part, theme_state;
  HANDLE theme = get_control_theme(control, type, state, &theme_part, &theme_state);
  
  if(theme && Win32Themes::GetThemeColor) {
    COLORREF col = 0;
    
    if(type == ContainerType::AddressBandInputField) {
      theme = w32cp_cache.addressband_edit_theme(control.dpi(), control.is_using_dark_mode());
      theme_part = 1; // EP_EDITTEXT
      theme_state = 1; // ETS_NORMAL
    }
    else if(type == ContainerType::InputField) {
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
      if(Impl::dark_mode_is_fake(type) && control.is_using_dark_mode()) {
        col = 0xFFFFFF & ~col;
      }
      return Color::from_bgr24(col);
    }
    
    //return Color::None;
  }
  
  switch(type) {
    case ContainerType::None:
    case ContainerType::FramelessButton:
    case ContainerType::GenericButton:
      return ControlPainter::control_font_color(control, type, state);
    
    case ContainerType::PopupPanel:
      return control.is_using_dark_mode() ? Color::White : Impl::get_sys_color(COLOR_BTNTEXT);
    
    case ContainerType::AddressBandGoButton:
    case ContainerType::PushButton:
    case ContainerType::DefaultPushButton:
    case ContainerType::PaletteButton:
    case ContainerType::Panel:
    case ContainerType::TabHeadAbuttingRight:
    case ContainerType::TabHeadAbuttingLeftRight:
    case ContainerType::TabHeadAbuttingLeft:
    case ContainerType::TabHead:
    case ContainerType::TabHeadBottomAbuttingRight:
    case ContainerType::TabHeadBottomAbuttingLeftRight:
    case ContainerType::TabHeadBottomAbuttingLeft:
    case ContainerType::TabHeadBottom:
    case ContainerType::TabHeadLeftAbuttingBottom:
    case ContainerType::TabHeadLeftAbuttingTopBottom:
    case ContainerType::TabHeadLeftAbuttingTop:
    case ContainerType::TabHeadLeft:
    case ContainerType::TabHeadRightAbuttingBottom:
    case ContainerType::TabHeadRightAbuttingTopBottom:
    case ContainerType::TabHeadRightAbuttingTop:
    case ContainerType::TabHeadRight:
    case ContainerType::TabBodyBackground:
    case ContainerType::TabPanelCenter: {
        if(Impl::dark_mode_is_fake(type) && control.is_using_dark_mode()) {
          return Color::White;
        }
        return Impl::get_sys_color(COLOR_BTNTEXT);
      } break;
    
    case ContainerType::ListViewItem:
      if(theme)
        return Color::None;
      if(state == ControlState::Normal)
        return Color::None;
      else
        return Impl::get_sys_color(state == ControlState::Disabled ? COLOR_GRAYTEXT : COLOR_WINDOWTEXT);
    
    case ContainerType::AddressBandInputField: // ContainerType::AddressBandBackground
    case ContainerType::InputField:
      return Impl::get_sys_color(state == ControlState::Disabled ? COLOR_GRAYTEXT : COLOR_WINDOWTEXT);
    
    case ContainerType::ListViewItemSelected:
      if(theme)
        return Color::None;
      return Impl::get_sys_color(COLOR_HIGHLIGHTTEXT);
      
    case ContainerType::TooltipWindow:
      return Impl::get_sys_color(COLOR_INFOTEXT);
      
    case ContainerType::HorizontalSliderChannel:
    case ContainerType::HorizontalSliderThumb:
    case ContainerType::HorizontalSliderDownArrowButton:
    case ContainerType::HorizontalSliderUpArrowButton:
    case ContainerType::VerticalSliderChannel:
    case ContainerType::VerticalSliderThumb:
    case ContainerType::VerticalSliderLeftArrowButton:
    case ContainerType::VerticalSliderRightArrowButton:
    case ContainerType::ProgressIndicatorBackground:
    case ContainerType::ProgressIndicatorBar:
    case ContainerType::CheckboxUnchecked:
    case ContainerType::CheckboxChecked:
    case ContainerType::CheckboxIndeterminate:
    case ContainerType::RadioButtonUnchecked:
    case ContainerType::RadioButtonChecked:
      break;
  }
  
  return Color::None;
}

bool Win32ControlPainter::is_very_transparent(ControlContext &control, ContainerType type, ControlState state) {
  switch(type) {
    case ContainerType::None:
    case ContainerType::FramelessButton:
    case ContainerType::GenericButton:
    case ContainerType::AddressBandInputField:
      return ControlPainter::is_very_transparent(control, type, state);
    
    case ContainerType::ListViewItem:
      return state == ControlState::Normal;
      
    case ContainerType::PaletteButton:
    case ContainerType::AddressBandGoButton: {
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
  if(canvas.suppress_output)
    return;
  
  switch(type) {
    case ContainerType::None:
    case ContainerType::FramelessButton:
    case ContainerType::GenericButton:
//    case ContainerType::TabPanelTopLeft:
//    case ContainerType::TabPanelTopCenter:
//    case ContainerType::TabPanelTopRight:
//    case ContainerType::TabPanelCenterLeft:
//    case ContainerType::TabPanelCenter:
//    case ContainerType::TabPanelCenterRight:
//    case ContainerType::TabPanelBottomLeft:
//    case ContainerType::TabPanelBottomCenter:
//    case ContainerType::TabPanelBottomRight:
      ControlPainter::generic_painter.draw_container(control, canvas, type, state, rect);
      return;
      
    case ContainerType::ListViewItem:
      if(state == ControlState::Normal)
        return;
      break;
    
    case ContainerType::AddressBandInputField:
      if(state == ControlState::Normal || state == ControlState::Hovered)
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
    case ContainerType::TabHeadAbuttingRight:
    case ContainerType::TabHeadAbuttingLeftRight:
    case ContainerType::TabHeadAbuttingLeft:
    case ContainerType::TabHead: {
        if(state != ControlState::Pressed && state != ControlState::PressedHovered) {
          rect.grow(Side::Top, -1.5f);
          
          if(y_scale > 0) {
            Win32Themes::MARGINS mar;
            if(!Impl(*this).try_get_sizing_margin(control, ContainerType::TabHeadBackground, ControlState::Normal, &mar)) {
              mar = {2,2,2,2};
            }
            rect.grow(Side::Bottom, -mar.cyTopHeight / y_scale);
          }
        }
      } break;
      
    case ContainerType::TabHeadBottomAbuttingRight:
    case ContainerType::TabHeadBottomAbuttingLeftRight:
    case ContainerType::TabHeadBottomAbuttingLeft:
    case ContainerType::TabHeadBottom: {
        if(state != ControlState::Pressed && state != ControlState::PressedHovered) {
          rect.grow(Side::Bottom, -1.5f);
          
          if(y_scale > 0) {
            Win32Themes::MARGINS mar;
            if(!Impl(*this).try_get_sizing_margin(control, ContainerType::TabHeadBackground, ControlState::Normal, &mar)) {
              mar = {2,2,2,2};
            }
            rect.grow(Side::Top, -mar.cyBottomHeight / y_scale);
          }
        }
      } break;
    
    case ContainerType::TabHeadLeftAbuttingBottom:
    case ContainerType::TabHeadLeftAbuttingTopBottom:
    case ContainerType::TabHeadLeftAbuttingTop:
    case ContainerType::TabHeadLeft: {
        if(state != ControlState::Pressed && state != ControlState::PressedHovered) {
          rect.grow(Side::Left, -1.5f);
          
          if(x_scale > 0) {
            Win32Themes::MARGINS mar;
            if(!Impl(*this).try_get_sizing_margin(control, ContainerType::TabHeadBackground, ControlState::Normal, &mar)) {
              mar = {2,2,2,2};
            }
            rect.grow(Side::Right, -mar.cxLeftWidth / x_scale);
          }
        }
      } break;
    
    case ContainerType::TabHeadRightAbuttingBottom:
    case ContainerType::TabHeadRightAbuttingTopBottom:
    case ContainerType::TabHeadRightAbuttingTop:
    case ContainerType::TabHeadRight: {
        if(state != ControlState::Pressed && state != ControlState::PressedHovered) {
          rect.grow(Side::Right, -1.5f);
          
          if(x_scale > 0) {
            Win32Themes::MARGINS mar;
            if(!Impl(*this).try_get_sizing_margin(control, ContainerType::TabHeadBackground, ControlState::Normal, &mar)) {
              mar = {2,2,2,2};
            }
            rect.grow(Side::Left, -mar.cxRightWidth / x_scale);
          }
        }
      } break;
    
    case ContainerType::TabHeadBackground: {
        rect.y += rect.height;
        rect.height = 0;
        
        if(y_scale > 0) {
          Win32Themes::MARGINS mar;
          if(!Impl(*this).try_get_sizing_margin(control, type, ControlState::Normal, &mar)) {
            mar = {2,2,2,2};
          }
          rect.height = mar.cyTopHeight / y_scale;
        }
        
        rect.y-= rect.height;
      } break;
    
    case ContainerType::TabPanelTopLeft: {
        rect.x+= rect.width;
        rect.y+= rect.height;
        rect.width = 0;
        rect.height = 0;
        
        if(y_scale > 0 && x_scale > 0) {
          Win32Themes::MARGINS mar;
          if(!Impl(*this).try_get_sizing_margin(control, type, ControlState::Normal, &mar)) {
            mar = {2,2,2,2};
          }
          rect.width  = mar.cxLeftWidth / x_scale;
          rect.height = mar.cyTopHeight / y_scale;
        }
        
        rect.x-= rect.width;
        rect.y-= rect.height;
      } break;
    
    case ContainerType::TabPanelTopCenter: {
        rect.y+= rect.height;
        rect.height = 0;
        
        if(y_scale > 0 && x_scale > 0) {
          Win32Themes::MARGINS mar;
          if(!Impl(*this).try_get_sizing_margin(control, type, ControlState::Normal, &mar)) {
            mar = {2,2,2,2};
          }
          rect.height = mar.cyTopHeight / y_scale;
        }
        
        rect.y-= rect.height;
      } break;
    
    case ContainerType::TabPanelTopRight: {
        rect.y+= rect.height;
        rect.width = 0;
        rect.height = 0;
        
        if(y_scale > 0 && x_scale > 0) {
          Win32Themes::MARGINS mar;
          if(!Impl(*this).try_get_sizing_margin(control, type, ControlState::Normal, &mar)) {
            mar = {2,2,2,2};
          }
          rect.width  = mar.cxRightWidth / x_scale;
          rect.height = mar.cyTopHeight / y_scale;
        }
        
        rect.y-= rect.height;
      } break;
    
    case ContainerType::TabPanelCenterLeft: {
        rect.x+= rect.width;
        rect.width = 0;
        
        if(y_scale > 0 && x_scale > 0) {
          Win32Themes::MARGINS mar;
          if(!Impl(*this).try_get_sizing_margin(control, type, ControlState::Normal, &mar)) {
            mar = {2,2,2,2};
          }
          rect.width = mar.cxLeftWidth / x_scale;
        }
        
        rect.x-= rect.width;
      } break;
    
    case ContainerType::TabPanelCenterRight: {
        rect.width = 0;
        
        if(y_scale > 0 && x_scale > 0) {
          Win32Themes::MARGINS mar;
          if(!Impl(*this).try_get_sizing_margin(control, type, ControlState::Normal, &mar)) {
            mar = {2,2,2,2};
          }
          rect.width = mar.cxRightWidth / x_scale;
        }
      } break;
      
    case ContainerType::TabPanelBottomLeft: {
        rect.x+= rect.width;
        rect.width = 0;
        rect.height = 0;
        
        if(y_scale > 0 && x_scale > 0) {
          Win32Themes::MARGINS mar;
          if(!Impl(*this).try_get_sizing_margin(control, type, ControlState::Normal, &mar)) {
            mar = {2,2,2,2};
          }
          rect.width  = mar.cxLeftWidth    / x_scale;
          rect.height = mar.cyBottomHeight / y_scale;
        }
        
        rect.x-= rect.width;
      } break;
    
    case ContainerType::TabPanelBottomCenter: {
        rect.height = 0;
        
        if(y_scale > 0 && x_scale > 0) {
          Win32Themes::MARGINS mar;
          if(!Impl(*this).try_get_sizing_margin(control, type, ControlState::Normal, &mar)) {
            mar = {2,2,2,2};
          }
          rect.height = mar.cyBottomHeight / y_scale;
        }
      } break;
    
    case ContainerType::TabPanelBottomRight: {
        rect.width = 0;
        rect.height = 0;
        
        if(y_scale > 0 && x_scale > 0) {
          Win32Themes::MARGINS mar;
          if(!Impl(*this).try_get_sizing_margin(control, type, ControlState::Normal, &mar)) {
            mar = {2,2,2,2};
          }
          rect.width  = mar.cxRightWidth   / x_scale;
          rect.height = mar.cyBottomHeight / y_scale;
        }
      } break;
      
    default: break;
  }
  
  rect.pixel_align(canvas, false);
  if(rect.width <= 0 || rect.height <= 0)
    return;
  
  switch(type) {
    case ContainerType::PopupPanel: {
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
    
    default: break;
  }
  
  if(Win32Themes::is_app_themed()) {
    switch(type) {
      case ContainerType::ToggleSwitchChannelUnchecked: 
        Impl::draw_toggle_switch_channel(canvas, rect, state, false, control.is_using_dark_mode());
        return;
        
      case ContainerType::ToggleSwitchChannelChecked: 
        Impl::draw_toggle_switch_channel(canvas, rect, state, true, control.is_using_dark_mode());
        return;
      
      case ContainerType::ToggleSwitchThumbUnchecked: 
        Impl::draw_toggle_switch_thumb(canvas, rect, state, false, control.is_using_dark_mode());
        return;
        
      case ContainerType::ToggleSwitchThumbChecked: 
        Impl::draw_toggle_switch_thumb(canvas, rect, state, true, control.is_using_dark_mode());
        return;
        
      default: break;
    }
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
    
    switch(type) {
      case ContainerType::TabHeadLeftAbuttingBottom:
      case ContainerType::TabHeadLeftAbuttingTopBottom:
      case ContainerType::TabHeadLeftAbuttingTop:
      case ContainerType::TabHeadLeft:
      case ContainerType::TabHeadRightAbuttingBottom:
      case ContainerType::TabHeadRightAbuttingTopBottom:
      case ContainerType::TabHeadRightAbuttingTop:
      case ContainerType::TabHeadRight: {
          using std::swap;
          swap(w, h);
          dc = nullptr;
          dc_x = dc_y = 0;
        } break;
        
      case ContainerType::TabHeadBottomAbuttingRight:
      case ContainerType::TabHeadBottomAbuttingLeftRight:
      case ContainerType::TabHeadBottomAbuttingLeft:
      case ContainerType::TabHeadBottom: {
          dc = nullptr;
          dc_x = dc_y = 0;
        } break;
    
      default:
        break;
    }
    
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
        case ContainerType::PaletteButton: {
          if(state == ControlState::Normal)
            state = ControlState::Hovered;
          else if(state == ControlState::Hovered)
            two_times = true;
        } break;
          
        //case ContainerType::CheckboxUnchecked:
        //case ContainerType::CheckboxChecked:
        //case ContainerType::CheckboxIndeterminate:
        //case ContainerType::RadioButtonUnchecked:
        //case ContainerType::RadioButtonChecked:
        //case ContainerType::OpenerTriangleClosed:
        //case ContainerType::OpenerTriangleOpened:
        //case ContainerType::HorizontalSliderChannel:
        //case ContainerType::HorizontalSliderThumb:
        case ContainerType::NavigationBack:
        case ContainerType::NavigationForward: 
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
                theme, tmp_dc, _part, _state, &tmp_rect, nullptr);
              
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
      case ContainerType::TabHeadBackground: {
          irect.top = irect.bottom - margins.cyTopHeight;
          irect.bottom+= margins.cyBottomHeight;
        } break;
      case ContainerType::TabBodyBackground: {
          irect.top-= margins.cyTopHeight;
        } break;
      case ContainerType::TabPanelTopLeft: {
          irect.top  = irect.bottom;
          irect.left = irect.right;
          irect.top    -= margins.cyTopHeight;
          irect.left   -= margins.cxLeftWidth;
          irect.bottom += margins.cyBottomHeight;
          irect.right  += margins.cxRightWidth;
        } break;
      case ContainerType::TabPanelTopCenter: {
          irect.top = irect.bottom;
          irect.top    -= margins.cyTopHeight;
          irect.left   -= margins.cxLeftWidth;
          irect.bottom += margins.cyBottomHeight;
          irect.right  += margins.cxRightWidth;
        } break;
      case ContainerType::TabPanelTopRight: {
          irect.top   = irect.bottom;
          irect.right = irect.left;
          irect.top    -= margins.cyTopHeight;
          irect.left   -= margins.cxLeftWidth;
          irect.bottom += margins.cyBottomHeight;
          irect.right  += margins.cxRightWidth;
        } break;
      case ContainerType::TabPanelCenterLeft: {
          irect.left = irect.right;
          irect.top    -= margins.cyTopHeight;
          irect.left   -= margins.cxLeftWidth;
          irect.bottom += margins.cyBottomHeight;
          irect.right  += margins.cxRightWidth;
        } break;
      case ContainerType::TabPanelCenter: {
          irect.top    -= margins.cyTopHeight;
          irect.left   -= margins.cxLeftWidth;
          irect.bottom += margins.cyBottomHeight;
          irect.right  += margins.cxRightWidth;
        } break;
      case ContainerType::TabPanelCenterRight: {
          irect.right = irect.left;
          irect.top    -= margins.cyTopHeight;
          irect.left   -= margins.cxLeftWidth;
          irect.bottom += margins.cyBottomHeight;
          irect.right  += margins.cxRightWidth;
        } break;
      case ContainerType::TabPanelBottomLeft: {
          irect.left   = irect.right;
          irect.bottom = irect.top;
          irect.top    -= margins.cyTopHeight;
          irect.left   -= margins.cxLeftWidth;
          irect.bottom += margins.cyBottomHeight;
          irect.right  += margins.cxRightWidth;
        } break;
      case ContainerType::TabPanelBottomCenter: {
          irect.bottom = irect.top;
          irect.top    -= margins.cyTopHeight;
          irect.left   -= margins.cxLeftWidth;
          irect.bottom += margins.cyBottomHeight;
          irect.right  += margins.cxRightWidth;
        } break;
      case ContainerType::TabPanelBottomRight: {
          irect.right  = irect.left;
          irect.bottom = irect.top;
          irect.top    -= margins.cyTopHeight;
          irect.left   -= margins.cxLeftWidth;
          irect.bottom += margins.cyBottomHeight;
          irect.right  += margins.cxRightWidth;
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
      case ContainerType::None:
      case ContainerType::FramelessButton:
        break;
      
      default: 
        if(state == ControlState::Pressed) {
          if(!control.is_focused_widget())
            state = ControlState::Normal;
        }
        else if(state == ControlState::PressedHovered) {
          if(!control.is_focused_widget())
            state = ControlState::Hovered;
        }
        break;
    }
    
    switch(type) {
      case ContainerType::None:
      case ContainerType::FramelessButton:
        break;
        
      case ContainerType::AddressBandGoButton:
      case ContainerType::PaletteButton: {
          FillRect(dc, &irect, (HBRUSH)(COLOR_BTNFACE + 1));
          
          if(state == ControlState::PressedHovered)
            DrawEdge(dc, &irect, BDR_SUNKENOUTER, BF_RECT);
          else if(state == ControlState::Hovered)
            DrawEdge(dc, &irect, BDR_RAISEDINNER, BF_RECT);
        } break;
        
      case ContainerType::DefaultPushButton:
      case ContainerType::GenericButton:
      case ContainerType::PushButton: {
          if(type == ContainerType::DefaultPushButton) {
            FrameRect(dc, &irect, (HBRUSH)GetStockObject(BLACK_BRUSH));
            
            InflateRect(&irect, -1, -1);
          }
          
          FillRect(dc, &irect, (HBRUSH)(COLOR_BTNFACE + 1));
          
          if(state == ControlState::PressedHovered) {
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
        
      case ContainerType::InputField: {
          FillRect(dc, &irect, (HBRUSH)(COLOR_WINDOW + 1));
          
          DrawEdge(
            dc,
            &irect,
            EDGE_SUNKEN,
            BF_RECT);
        } break;
        
      case ContainerType::AddressBandInputField: {
          if(state == ControlState::Pressed || state == ControlState::PressedHovered) {
            FillRect(dc, &irect, (HBRUSH)(COLOR_WINDOW + 1));
            DrawEdge(
              dc,
              &irect,
              BDR_SUNKENINNER,
              BF_RECT);
          }
        } break;
        
      case ContainerType::AddressBandBackground: {
          FillRect(dc, &irect, (HBRUSH)(COLOR_BTNFACE + 1));
          
          DrawEdge(
            dc,
            &irect,
            BDR_SUNKENOUTER,
            BF_RECT);
        } break;
        
      case ContainerType::TooltipWindow: {
          FrameRect(dc, &irect, GetSysColorBrush(COLOR_WINDOWFRAME));
          
          InflateRect(&irect, -1, -1);
          
          FillRect(dc, &irect, (HBRUSH)(COLOR_INFOBK + 1));
        } break;
        
      case ContainerType::ListViewItem:
        FillRect(dc, &irect, (HBRUSH)(COLOR_WINDOW + 1));
        break;
        
      case ContainerType::ListViewItemSelected:
        FillRect(dc, &irect, (HBRUSH)(COLOR_HIGHLIGHT + 1));
        break;
      
      case ContainerType::Panel:
        FillRect(dc, &irect, (HBRUSH)(COLOR_BTNFACE + 1));
        DrawEdge(dc, &irect, BDR_RAISEDOUTER, BF_RECT);
        break;
      
      case ContainerType::ProgressIndicatorBackground:
      case ContainerType::HorizontalSliderChannel:
      case ContainerType::VerticalSliderChannel:
      case ContainerType::ToggleSwitchChannelUnchecked: {
          FillRect(dc, &irect, (HBRUSH)(COLOR_BTNFACE + 1));
          
          DrawEdge(
            dc,
            &irect,
            EDGE_SUNKEN,
            BF_RECT);
        } break;
        
      case ContainerType::ToggleSwitchChannelChecked: {
          FillRect(dc, &irect, (HBRUSH)(COLOR_HIGHLIGHT + 1));
          
          DrawEdge(
            dc,
            &irect,
            EDGE_SUNKEN,
            BF_RECT);
        } break;
        
      case ContainerType::ProgressIndicatorBar: {
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
        
      case ContainerType::HorizontalSliderThumb:
      case ContainerType::VerticalSliderThumb: {
          DrawFrameControl(
            dc,
            &irect,
            DFC_BUTTON,
            DFCS_BUTTONPUSH);
        } break;
      
      case ContainerType::HorizontalSliderDownArrowButton: {
          RECT tmp = irect;
          int h = (tmp.right - tmp.left) / 2;
          
          tmp.bottom-= h;
          DrawEdge(dc, &tmp, BDR_RAISEDINNER | BDR_RAISEDOUTER, BF_LEFT | BF_RIGHT | BF_TOP | BF_MIDDLE);
          
          tmp.bottom = irect.bottom;
          tmp.top = tmp.bottom - h;
          tmp.right = tmp.left + h;
          DrawEdge(dc, &tmp, BDR_RAISEDINNER | BDR_RAISEDOUTER, BF_DIAGONAL | BF_TOP | BF_LEFT | BF_MIDDLE);
          
          tmp.left = tmp.right;
          tmp.right = irect.right;
          DrawEdge(dc, &tmp, BDR_RAISEDINNER | BDR_RAISEDOUTER, BF_DIAGONAL | BF_BOTTOM | BF_LEFT | BF_MIDDLE);
        } break;
        
      case ContainerType::HorizontalSliderUpArrowButton: {
          RECT tmp = irect;
          int h = (tmp.right - tmp.left) / 2;
          
          tmp.top+= h;
          DrawEdge(dc, &tmp, BDR_RAISEDINNER | BDR_RAISEDOUTER, BF_LEFT | BF_RIGHT | BF_BOTTOM | BF_MIDDLE);
          
          tmp.top    = irect.top;
          tmp.bottom = tmp.top + h;
          tmp.right = tmp.left + h;
          DrawEdge(dc, &tmp, BDR_RAISEDINNER | BDR_RAISEDOUTER, BF_DIAGONAL | BF_TOP | BF_RIGHT | BF_MIDDLE);
          
          tmp.left = tmp.right;
          tmp.right = irect.right;
          DrawEdge(dc, &tmp, BDR_RAISEDINNER | BDR_RAISEDOUTER, BF_DIAGONAL | BF_BOTTOM | BF_RIGHT | BF_MIDDLE);
        } break;
        
      case ContainerType::VerticalSliderLeftArrowButton: {
          RECT tmp = irect;
          int h = (tmp.bottom - tmp.top) / 2;
          
          tmp.left+= h;
          DrawEdge(dc, &tmp, BDR_RAISEDINNER | BDR_RAISEDOUTER,  BF_RIGHT | BF_TOP | BF_BOTTOM | BF_MIDDLE);
          
          tmp.left  = irect.left;
          tmp.right = tmp.left + h;
          tmp.bottom = tmp.top + h;
          DrawEdge(dc, &tmp, BDR_RAISEDINNER | BDR_RAISEDOUTER, BF_DIAGONAL | BF_TOP | BF_RIGHT | BF_MIDDLE);
          
          tmp.top = tmp.bottom;
          tmp.bottom = irect.bottom;
          DrawEdge(dc, &tmp, BDR_RAISEDINNER | BDR_RAISEDOUTER, BF_DIAGONAL | BF_TOP | BF_LEFT | BF_MIDDLE);
        } break;
      
      case ContainerType::VerticalSliderRightArrowButton: {
          RECT tmp = irect;
          int h = (tmp.bottom - tmp.top) / 2;
          
          tmp.right-= h;
          DrawEdge(dc, &tmp, BDR_RAISEDINNER | BDR_RAISEDOUTER, BF_LEFT | BF_TOP | BF_BOTTOM | BF_MIDDLE);
          
          tmp.right = irect.right;
          tmp.left = tmp.right - h;
          tmp.bottom = tmp.top + h;
          DrawEdge(dc, &tmp, BDR_RAISEDINNER | BDR_RAISEDOUTER, BF_DIAGONAL | BF_BOTTOM | BF_RIGHT | BF_MIDDLE);
          
          tmp.top = tmp.bottom;
          tmp.bottom = irect.bottom;
          DrawEdge(dc, &tmp, BDR_RAISEDINNER | BDR_RAISEDOUTER, BF_DIAGONAL | BF_BOTTOM | BF_LEFT | BF_MIDDLE);
        } break;
        
      case ContainerType::ToggleSwitchThumbChecked:
      case ContainerType::ToggleSwitchThumbUnchecked: {
          InflateRect(&irect, -2, -1);
          DrawFrameControl(
            dc,
            &irect,
            DFC_BUTTON,
            DFCS_BUTTONPUSH);
        } break;
        
      case ContainerType::CheckboxUnchecked:
      case ContainerType::CheckboxChecked:
      case ContainerType::CheckboxIndeterminate: {
          UINT _state = DFCS_BUTTONCHECK;
          
          if(type == ContainerType::CheckboxChecked)       _state |= DFCS_CHECKED;
          if(type == ContainerType::CheckboxIndeterminate) _state = DFCS_BUTTON3STATE | DFCS_CHECKED;
          
          switch(state) {
            case ControlState::Disabled:       _state |= DFCS_INACTIVE; break;
            case ControlState::PressedHovered: _state |= DFCS_PUSHED;   break;
            default: break;
          }
          
          DrawFrameControl(
            dc,
            &irect,
            DFC_BUTTON,
            _state);
        } break;
        
      case ContainerType::RadioButtonUnchecked:
      case ContainerType::RadioButtonChecked: {
          UINT _state = DFCS_BUTTONRADIO;
          
          if(type == ContainerType::RadioButtonChecked)    _state |= DFCS_CHECKED;
          
          switch(state) {
            case ControlState::Disabled:       _state |= DFCS_INACTIVE; break;
            case ControlState::PressedHovered: _state |= DFCS_PUSHED;   break;
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
      
      case ContainerType::OpenerTriangleClosed:
      case ContainerType::OpenerTriangleOpened:
        ControlPainter::draw_container(control, canvas, type, state, rect);
        return;
        
      case ContainerType::NavigationBack:
      case ContainerType::NavigationForward: {
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
          if(state == ControlState::PressedHovered)
            DrawEdge(dc, &irect, BDR_SUNKENOUTER, BF_RECT);
          else if(state == ControlState::Hovered)
            DrawEdge(dc, &irect, BDR_RAISEDINNER, BF_RECT);
          
          need_vector_overlay = true;
        } break;
      
      case ContainerType::TabHeadAbuttingRight:
      case ContainerType::TabHeadAbuttingLeftRight:
      case ContainerType::TabHeadAbuttingLeft:
      case ContainerType::TabHead: {
          FillRect(dc, &irect, (HBRUSH)(COLOR_BTNFACE + 1));
          
          DWORD flags = 0;
          if(state == ControlState::Disabled)
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
      
      case ContainerType::TabHeadBottomAbuttingRight:
      case ContainerType::TabHeadBottomAbuttingLeftRight:
      case ContainerType::TabHeadBottomAbuttingLeft:
      case ContainerType::TabHeadBottom: {
          FillRect(dc, &irect, (HBRUSH)(COLOR_BTNFACE + 1));
          
          DWORD flags = 0;
          if(state == ControlState::Disabled)
            flags |= BF_MONO;
          
          RECT edge = irect;
          edge.top = edge.bottom - 3;
          
          irect.bottom-= 2;
          DrawEdge(dc, &irect, BDR_RAISEDINNER | BDR_RAISEDOUTER, flags | BF_LEFT | BF_RIGHT);
          
          edge.right = irect.left + 3;
          DrawEdge(dc, &edge, BDR_RAISEDINNER | BDR_RAISEDOUTER, flags | BF_DIAGONAL | BF_TOP | BF_LEFT);
          
          edge.right = irect.right;
          edge.left = edge.right - 3;
          DrawEdge(dc, &edge, BDR_RAISEDINNER | BDR_RAISEDOUTER, flags | BF_DIAGONAL | BF_BOTTOM | BF_LEFT);
          
          irect.bottom = edge.bottom;
          irect.left+= 2;
          irect.right-= 2;
          DrawEdge(dc, &irect, BDR_RAISEDINNER | BDR_RAISEDOUTER, flags | BF_BOTTOM);
        } break;
      
      case ContainerType::TabHeadLeftAbuttingBottom:
      case ContainerType::TabHeadLeftAbuttingTopBottom:
      case ContainerType::TabHeadLeftAbuttingTop:
      case ContainerType::TabHeadLeft: {
          FillRect(dc, &irect, (HBRUSH)(COLOR_BTNFACE + 1));
          
          DWORD flags = 0;
          if(state == ControlState::Disabled)
            flags |= BF_MONO;
          
          RECT edge = irect;
          edge.right = edge.left + 3;
          
          irect.left+= 2;
          DrawEdge(dc, &irect, BDR_RAISEDINNER | BDR_RAISEDOUTER, flags | BF_TOP | BF_BOTTOM);
          
          edge.bottom = irect.top + 3;
          DrawEdge(dc, &edge, BDR_RAISEDINNER | BDR_RAISEDOUTER, flags | BF_DIAGONAL | BF_TOP | BF_RIGHT);
          
          edge.bottom = irect.bottom;
          edge.top = edge.bottom - 3;
          DrawEdge(dc, &edge, BDR_RAISEDINNER | BDR_RAISEDOUTER, flags | BF_DIAGONAL | BF_TOP | BF_LEFT);
          
          irect.left = edge.left;
          irect.top+= 2;
          irect.bottom-= 2;
          DrawEdge(dc, &irect, BDR_RAISEDINNER | BDR_RAISEDOUTER, flags | BF_LEFT);
        } break;
      
      case ContainerType::TabHeadRightAbuttingBottom:
      case ContainerType::TabHeadRightAbuttingTopBottom:
      case ContainerType::TabHeadRightAbuttingTop:
      case ContainerType::TabHeadRight: {
          FillRect(dc, &irect, (HBRUSH)(COLOR_BTNFACE + 1));
          
          DWORD flags = 0;
          if(state == ControlState::Disabled)
            flags |= BF_MONO;
          
          RECT edge = irect;
          edge.left = edge.right - 3;
          
          irect.right-= 2;
          DrawEdge(dc, &irect, BDR_RAISEDINNER | BDR_RAISEDOUTER, flags | BF_TOP | BF_BOTTOM);
          
          edge.bottom = irect.top + 3;
          DrawEdge(dc, &edge, BDR_RAISEDINNER | BDR_RAISEDOUTER, flags | BF_DIAGONAL | BF_BOTTOM | BF_RIGHT);
          
          edge.bottom = irect.bottom;
          edge.top = edge.bottom - 3;
          DrawEdge(dc, &edge, BDR_RAISEDINNER | BDR_RAISEDOUTER, flags | BF_DIAGONAL | BF_BOTTOM | BF_LEFT);
          
          irect.right = edge.right;
          irect.top+= 2;
          irect.bottom-= 2;
          DrawEdge(dc, &irect, BDR_RAISEDINNER | BDR_RAISEDOUTER, flags | BF_RIGHT);
        } break;
    
      case ContainerType::TabHeadBackground: {
          DrawEdge(dc, &irect, BDR_RAISEDINNER | BDR_RAISEDOUTER, BF_ADJUST | BF_LEFT | BF_TOP | BF_RIGHT | (state == ControlState::Disabled ? BF_MONO : 0));
          FillRect(dc, &irect, (HBRUSH)(COLOR_BTNFACE + 1));
        } break;
      
      case ContainerType::TabBodyBackground: {
          DrawEdge(dc, &irect, BDR_RAISEDINNER | BDR_RAISEDOUTER, BF_ADJUST | BF_LEFT | BF_BOTTOM | BF_RIGHT | (state == ControlState::Disabled ? BF_MONO : 0));
          FillRect(dc, &irect, (HBRUSH)(COLOR_BTNFACE + 1));
        } break;
        
      case ContainerType::TabPanelTopLeft: {
          //DrawEdge(dc, &irect, BDR_RAISEDINNER | BDR_RAISEDOUTER, BF_ADJUST | BF_DIAGONAL | BF_TOP | BF_RIGHT | (state == ControlState::Disabled ? BF_MONO : 0));
          DrawEdge(dc, &irect, BDR_RAISEDINNER | BDR_RAISEDOUTER, BF_ADJUST | BF_TOP | BF_LEFT | (state == ControlState::Disabled ? BF_MONO : 0));
          FillRect(dc, &irect, (HBRUSH)(COLOR_BTNFACE + 1));
        } break;
      case ContainerType::TabPanelTopCenter: {
          DrawEdge(dc, &irect, BDR_RAISEDINNER | BDR_RAISEDOUTER, BF_ADJUST | BF_TOP | (state == ControlState::Disabled ? BF_MONO : 0));
          FillRect(dc, &irect, (HBRUSH)(COLOR_BTNFACE + 1));
        } break;
      case ContainerType::TabPanelTopRight: {
          //DrawEdge(dc, &irect, BDR_RAISEDINNER | BDR_RAISEDOUTER, BF_ADJUST | BF_DIAGONAL | BF_BOTTOM | BF_RIGHT | (state == ControlState::Disabled ? BF_MONO : 0));
          DrawEdge(dc, &irect, BDR_RAISEDINNER | BDR_RAISEDOUTER, BF_ADJUST | BF_TOP | BF_RIGHT | (state == ControlState::Disabled ? BF_MONO : 0));
          FillRect(dc, &irect, (HBRUSH)(COLOR_BTNFACE + 1));
        } break;
      case ContainerType::TabPanelCenterLeft: {
          DrawEdge(dc, &irect, BDR_RAISEDINNER | BDR_RAISEDOUTER, BF_ADJUST | BF_LEFT | (state == ControlState::Disabled ? BF_MONO : 0));
          FillRect(dc, &irect, (HBRUSH)(COLOR_BTNFACE + 1));
        } break;
      case ContainerType::TabPanelCenter: {
          FillRect(dc, &irect, (HBRUSH)(COLOR_BTNFACE + 1));
        } break;
      case ContainerType::TabPanelCenterRight: {
          DrawEdge(dc, &irect, BDR_RAISEDINNER | BDR_RAISEDOUTER, BF_ADJUST | BF_RIGHT | (state == ControlState::Disabled ? BF_MONO : 0));
          FillRect(dc, &irect, (HBRUSH)(COLOR_BTNFACE + 1));
        } break;
      case ContainerType::TabPanelBottomLeft: {
          //DrawEdge(dc, &irect, BDR_RAISEDINNER | BDR_RAISEDOUTER, BF_ADJUST | BF_DIAGONAL | BF_TOP | BF_LEFT | (state == ControlState::Disabled ? BF_MONO : 0));
          DrawEdge(dc, &irect, BDR_RAISEDINNER | BDR_RAISEDOUTER, BF_ADJUST | BF_BOTTOM | BF_LEFT | (state == ControlState::Disabled ? BF_MONO : 0));
          FillRect(dc, &irect, (HBRUSH)(COLOR_BTNFACE + 1));
        } break;
      case ContainerType::TabPanelBottomCenter: {
          DrawEdge(dc, &irect, BDR_RAISEDINNER | BDR_RAISEDOUTER, BF_ADJUST | BF_BOTTOM | (state == ControlState::Disabled ? BF_MONO : 0));
          FillRect(dc, &irect, (HBRUSH)(COLOR_BTNFACE + 1));
        } break;
      case ContainerType::TabPanelBottomRight: {
          //DrawEdge(dc, &irect, BDR_RAISEDINNER | BDR_RAISEDOUTER, BF_ADJUST | BF_DIAGONAL | BF_BOTTOM | BF_LEFT | (state == ControlState::Disabled ? BF_MONO : 0));
          DrawEdge(dc, &irect, BDR_RAISEDINNER | BDR_RAISEDOUTER, BF_ADJUST | BF_BOTTOM | BF_RIGHT | (state == ControlState::Disabled ? BF_MONO : 0));
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
    
    switch(type) {
      case ContainerType::TabHeadLeftAbuttingBottom:
      case ContainerType::TabHeadLeftAbuttingTopBottom:
      case ContainerType::TabHeadLeftAbuttingTop:
      case ContainerType::TabHeadLeft: {
          cairo_matrix_t mat;
          mat.xx = mat.yy = 0;
          mat.xy = w / rect.height; // w and h where swapped, but not the rect fields
          mat.yx = h / rect.width;
          mat.x0 = -mat.xy * rect.y;
          mat.y0 = -mat.yx * rect.x;
          
          cairo_pattern_set_matrix(cairo_get_source(canvas.cairo()), &mat);
        } break;
        
      case ContainerType::TabHeadRightAbuttingBottom:
      case ContainerType::TabHeadRightAbuttingTopBottom:
      case ContainerType::TabHeadRightAbuttingTop:
      case ContainerType::TabHeadRight: {
          cairo_matrix_t mat;
          mat.xx = mat.yy = 0;
          mat.xy =  w / rect.height; // w and h where swapped, but not the rect fields
          mat.yx = -h / rect.width;
          mat.x0 = -mat.xy * rect.y;
          mat.y0 = -mat.yx * rect.right();
          
          cairo_pattern_set_matrix(cairo_get_source(canvas.cairo()), &mat);
        } break;
        
      case ContainerType::TabHeadBottomAbuttingRight:
      case ContainerType::TabHeadBottomAbuttingLeftRight:
      case ContainerType::TabHeadBottomAbuttingLeft:
      case ContainerType::TabHeadBottom: {
          cairo_matrix_t mat;
          mat.xx =  w / rect.width;
          mat.yy = -h / rect.height;
          mat.xy = mat.yx = 0;
          mat.x0 = -mat.xx * rect.x;
          mat.y0 = -mat.yy * rect.bottom();
          
          cairo_pattern_set_matrix(cairo_get_source(canvas.cairo()), &mat);
        } break;
        
      default:
        if(w != rect.width || h != rect.height) {
          cairo_matrix_t mat;
          mat.xx = w / rect.width;
          mat.yy = h / rect.height;
          mat.xy = mat.yx = 0;
          mat.x0 = -mat.xx * rect.x;
          mat.y0 = -mat.yy * rect.y;
          
          cairo_pattern_set_matrix(cairo_get_source(canvas.cairo()), &mat);
        }
        break;
    }
      
    canvas.paint();
    
    canvas.restore();
    cairo_surface_destroy(surface);
  }
  else {
    cairo_surface_mark_dirty(cairo_get_target(canvas.cairo()));
  }
  
  if(Impl::dark_mode_is_fake(type) && control.is_using_dark_mode()) {
    rect.add_rect_path(canvas, false);
    
    canvas.set_color(Color::Black, 0.667);
    canvas.fill();
  }
  
  if(need_vector_overlay) {
    switch(type) {
      case ContainerType::NavigationBack:
      case ContainerType::NavigationForward: {
          float cx = rect.x + rect.width/2;
          float cy = rect.y + rect.height/2;
          rect.width = rect.height = std::min(rect.width, rect.height);
          rect.x = cx - rect.width/2;
          rect.y = cy - rect.height/2;
          
          if(state == ControlState::PressedHovered) {
            rect.x+= 0.75;
            rect.y+= 0.75;
          }
          
          if(type == ContainerType::NavigationForward) {
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
            case ControlState::Disabled:
              fill_col = Impl::get_sys_color(COLOR_BTNFACE);
              stroke_col = Impl::get_sys_color(COLOR_GRAYTEXT);
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

bool Win32ControlPainter::enable_animations() {
  BOOL has_animations = TRUE;
  if(SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &has_animations, FALSE) )
    return !!has_animations;
  
  return true;
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
  
  if(!enable_animations())
    return nullptr;
  
  ControlContext &control = ControlContext::find(FrontEndObject::find_cast<Box>(widget_id));
  
  bool repeat = false;
  if(type2 == ContainerType::DefaultPushButton && state1 == ControlState::Normal && state2 == ControlState::Normal) {
    if(control.is_foreground_window()) {
      // On Windows 10 there is no animation (and both states look the same), 
      // but PBS_DEFAULTED -> PBS_DEFAULTED_ANIMATING has duration 1000ms,
      // wheas PBS_DEFAULTED_ANIMATING -> PBS_DEFAULTED has duration 0ms.
      // So we adjust state1 instead of state2 to get 0ms in that case.
      state1 = ControlState::Hot; 
      repeat = true;
    }
  }
  
  if( state1 == ControlState::Normal                      &&
      (state2 == ControlState::Hot || state2 == ControlState::Hovered)  &&
      type2 == ContainerType::PaletteButton)
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
    case ContainerType::PushButton:
    case ContainerType::DefaultPushButton:
    case ContainerType::PaletteButton:
    case ContainerType::TabHeadAbuttingRight:
    case ContainerType::TabHeadAbuttingLeftRight:
    case ContainerType::TabHeadAbuttingLeft:
    case ContainerType::TabHead:
    case ContainerType::TabHeadBottomAbuttingRight:
    case ContainerType::TabHeadBottomAbuttingLeftRight:
    case ContainerType::TabHeadBottomAbuttingLeft:
    case ContainerType::TabHeadBottom:
    case ContainerType::TabHeadLeftAbuttingBottom:
    case ContainerType::TabHeadLeftAbuttingTopBottom:
    case ContainerType::TabHeadLeftAbuttingTop:
    case ContainerType::TabHeadLeft: 
    case ContainerType::TabHeadRightAbuttingBottom:
    case ContainerType::TabHeadRightAbuttingTopBottom:
    case ContainerType::TabHeadRightAbuttingTop:
    case ContainerType::TabHeadRight: {
        if(state2 == ControlState::PressedHovered/* || state1 == ControlState::Normal*/)
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
    
    if(type2 == ContainerType::InputField) // bigger buffer for glow rectangle
      rect1.grow(4.5, 4.5);
    
    SharedPtr<FadeAnimation> anim = new FadeAnimation(
      widget_id,
      canvas,
      rect1,
      duration / 1000.0);
      
    if(!anim->box_id.is_valid() || !anim->buf1 || !anim->buf2) {
      return nullptr;
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
    case ContainerType::FramelessButton:
    case ContainerType::None:
    case ContainerType::Panel:
    case ContainerType::PopupPanel:
    case ContainerType::TabBodyBackground:
    case ContainerType::TabHeadBackground:
    case ContainerType::TabPanelTopLeft:
    case ContainerType::TabPanelTopCenter:
    case ContainerType::TabPanelTopRight:
    case ContainerType::TabPanelCenterLeft:
    case ContainerType::TabPanelCenter:
    case ContainerType::TabPanelCenterRight:
    case ContainerType::TabPanelBottomLeft:
    case ContainerType::TabPanelBottomCenter:
    case ContainerType::TabPanelBottomRight:
    case ContainerType::TooltipWindow:
      return false;
    
    case ContainerType::GenericButton:
      return ControlPainter::container_hover_repaint(control, type);
  }
  
  return Win32Themes::OpenThemeData && 
         Win32Themes::CloseThemeData && 
         Win32Themes::DrawThemeBackground;
}

bool Win32ControlPainter::control_glow_margins(
    ControlContext &control, 
    ContainerType   type,
    ControlState    state, 
    Margins<float> *outer, 
    Margins<float> *inner
) {
  switch(type) {
    case ContainerType::TabHead:
    case ContainerType::TabHeadAbuttingLeft:
    case ContainerType::TabHeadAbuttingLeftRight:
    case ContainerType::TabHeadAbuttingRight:
    case ContainerType::TabHeadBottom:
    case ContainerType::TabHeadBottomAbuttingLeft:
    case ContainerType::TabHeadBottomAbuttingLeftRight:
    case ContainerType::TabHeadBottomAbuttingRight:
      if(state == ControlState::Pressed || state == ControlState::PressedHovered) {
        Win32Themes::MARGINS mar;
        if(!Impl(*this).try_get_sizing_margin(control, type, ControlState::Normal, &mar)) {
          mar = {2,2,2,0};
        }
        
        double scale = 72.0 / control.dpi();
        if(outer) *outer = Margins<float>(mar.cxLeftWidth * scale, mar.cxRightWidth * scale, 0, 0);
        if(inner) *inner = Margins<float>(mar.cxLeftWidth * scale, mar.cxRightWidth * scale, 0, 0);
        return true;
      }
      return false;
      
    case ContainerType::TabHeadLeftAbuttingBottom:
    case ContainerType::TabHeadLeftAbuttingTopBottom:
    case ContainerType::TabHeadLeftAbuttingTop:
    case ContainerType::TabHeadLeft:
    case ContainerType::TabHeadRightAbuttingBottom:
    case ContainerType::TabHeadRightAbuttingTopBottom:
    case ContainerType::TabHeadRightAbuttingTop:
    case ContainerType::TabHeadRight:
      if(state == ControlState::Pressed || state == ControlState::PressedHovered) {
        Win32Themes::MARGINS mar;
        if(!Impl(*this).try_get_sizing_margin(control, type, ControlState::Normal, &mar)) {
          mar = {2,2,2,0}; // theming parts correspond to TabHead/..., on the top
        }
        
        double scale = 72.0 / control.dpi();
        if(outer) *outer = Margins<float>(0, 0, /* top: */0.5 * mar.cxLeftWidth * scale, /* bottom: */0.5 * mar.cxRightWidth * scale);
        if(inner) *inner = Margins<float>(0, 0, /* top: */mar.cxLeftWidth * scale, /* bottom: */mar.cxRightWidth * scale);
        return true;
      }
      return false;
    
    default: break;
  }
  
  return ControlPainter::control_glow_margins(control, type, state, outer, inner);
}

void Win32ControlPainter::system_font_style(ControlContext &control, StyleData *style) {
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
    Win32HighDpi::get_nonclient_metrics_for_dpi(&nonclientmetrics, 96.0f/*control.dpi()*/);
    dpi_scale = 1.0f;//96.0f / (float)control.dpi();
    logfont = &nonclientmetrics.lfMessageFont;
  }
  
  style->set(FontFamilies, String::FromUcs2((const uint16_t *)logfont->lfFaceName));
  style->set(FontSize, Length(abs(logfont->lfHeight) * dpi_scale * 3 / 4.f));
  
  if(logfont->lfWeight > FW_NORMAL)
    style->set(FontWeight, FontWeightBold);
  else
    style->set(FontWeight, FontWeightPlain);
    
  style->set(FontSlant, logfont->lfItalic ? FontSlantItalic : FontSlantPlain);
}

Color Win32ControlPainter::selection_color(ControlContext &control) {
  return Impl::get_sys_color(COLOR_HIGHLIGHT);
}

Color Win32ControlPainter::win32_button_face_color(bool dark) {
  if(dark) {
    // TODO: do not hardcode dark-mode COLOR_BTNFACE but get it from the system somehow
    // dark mode Popup menu background: 0x2B2B2B
    return Color::from_rgb24(0x2B2B2B);
  }
  
  return Color::from_bgr24(GetSysColor(COLOR_BTNFACE));
}

Color Win32ControlPainter::win32_menu_popup_color(bool dark) {
  if(w32cp_cache.has_menu_color(dark))
    return Color::from_bgr24(w32cp_cache.menu_color(dark));
  
  RECT rect = {0, 0, 5, 5};
  HBITMAP bmp = CreateBitmap(rect.right, rect.bottom, 1, 32, nullptr);
  if(bmp) {
    HDC memDC          = CreateCompatibleDC(NULL);
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(memDC, bmp);
    
    draw_menu_popup(memDC, &rect, dark);
    COLORREF col = GetPixel(memDC, rect.right / 2, rect.bottom / 2);
    
    SelectObject(memDC, hOldBitmap);
    DeleteDC(memDC);
    DeleteObject(bmp);
    
    w32cp_cache.cache_menu_color(dark, col);
    return Color::from_bgr24(col);
  }
  
  return win32_button_face_color(dark);
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
  if(part == ScrollbarPart::Nowhere)
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
      case ScrollbarPart::UpLeft:
      case ScrollbarPart::DownRight:
        _part = 1; // SBP_ARROWBTN
        break;
        
      case ScrollbarPart::Thumb: {
          if(dir == ScrollbarDirection::Horizontal)
            _part = 2;
          else
            _part = 3;
        } break;
        
      case ScrollbarPart::LowerRange: {
          if(dir == ScrollbarDirection::Horizontal)
            _part = 4;
          else
            _part = 6;
        } break;
        
      case ScrollbarPart::UpperRange: {
          if(dir == ScrollbarDirection::Horizontal)
            _part = 5;
          else
            _part = 7;
        } break;
        
      case ScrollbarPart::Nowhere:
      case ScrollbarPart::SizeGrip: _part = 10; break;
    }
    
    if(_part == 1) { // arrow button
      switch(state) {
        case ControlState::Disabled:        _state = 4; break;
        case ControlState::PressedHovered:  _state = 3; break;
        case ControlState::Hovered:         _state = 2; break;
        case ControlState::Hot:
        case ControlState::Pressed:
        case ControlState::Normal:          _state = 1; break;
        
        default: ;
      }
      
      if(_state) {
        if(dir == ScrollbarDirection::Horizontal) {
          if(part == ScrollbarPart::DownRight)
            _state += 12;
          else
            _state += 8;
        }
        else {
          if(part == ScrollbarPart::DownRight)
            _state += 4;
        }
      }
      else {
        if(dir == ScrollbarDirection::Horizontal) {
          if(part == ScrollbarPart::DownRight)
            _state = 20;
          else
            _state = 19;
        }
        else {
          if(part == ScrollbarPart::DownRight)
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
        case ControlState::Pressed:
        case ControlState::Normal:         _state = 1; break;
        case ControlState::Hot:            _state = 2; break;
        case ControlState::PressedHovered: _state = 3; break;
        case ControlState::Disabled:       _state = 4; break;
        case ControlState::Hovered:        _state = 5; break;
      }
    }
    
    Win32Themes::DrawThemeBackground(
      scrollbar_theme,
      dc,
      _part,
      _state,
      &irect,
      nullptr);
      
    if(part == ScrollbarPart::Thumb) {
      if(dir == ScrollbarDirection::Horizontal) {
        if(rect.width >= rect.height) {
          Win32Themes::DrawThemeBackground(
            scrollbar_theme,
            dc,
            8,
            _state,
            &irect,
            nullptr);
        }
      }
      else if(rect.height >= rect.width) {
        Win32Themes::DrawThemeBackground(
          scrollbar_theme,
          dc,
          9,
          _state,
          &irect,
          nullptr);
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
      case ScrollbarPart::UpLeft: {
          if(dir == ScrollbarDirection::Horizontal)
            _state = DFCS_SCROLLLEFT;
          else
            _state = DFCS_SCROLLUP;
        } break;
        
      case ScrollbarPart::DownRight: {
          if(dir == ScrollbarDirection::Horizontal)
            _state = DFCS_SCROLLRIGHT;
          else
            _state = DFCS_SCROLLDOWN;
        } break;
        
      case ScrollbarPart::SizeGrip: _state = DFCS_SCROLLSIZEGRIP; break;
      
      case ScrollbarPart::Thumb:
        _type = DFC_BUTTON;
        _state = DFCS_BUTTONPUSH;
        break;
        
      default: bg = true;
    }
    
    if(!bg) {
      switch(state) {
        case ControlState::PressedHovered:  _state |= DFCS_PUSHED;   break;
        case ControlState::Hot:
        case ControlState::Hovered:         _state |= DFCS_HOT;      break;
        case ControlState::Disabled:        _state |= DFCS_INACTIVE; break;
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

void Win32ControlPainter::draw_menu_popup(HDC dc, RECT *rect, bool dark_mode) {
  if( Win32Themes::OpenThemeData  &&
      Win32Themes::CloseThemeData &&
      Win32Themes::DrawThemeBackground)
  {
    bool need_invert = false;
    
    HANDLE theme = nullptr;
    if(dark_mode) {
      theme = Win32Themes::OpenThemeData(nullptr, L"DarkMode::MENU");
      if(!theme)
        need_invert = true;
    }
    
    if(!theme)
      theme = Win32Themes::OpenThemeData(nullptr, L"MENU");
    
    if(!theme)
      goto FALLBACK;
      
    Win32Themes::DrawThemeBackground(
      theme,
      dc,
      9, // MENU_POPUPBACKGROUND
      0,
      rect,
      nullptr);
      
    Win32Themes::CloseThemeData(theme);
    if(!need_invert)
      return;
  }
  else {
  FALLBACK: 
    FillRect(dc, rect, GetSysColorBrush(COLOR_MENU));
  }
  
  if(dark_mode) {
    BitBlt(dc, rect->left, rect->top, rect->right - rect->left, rect->bottom - rect->top, nullptr, 0, 0, DSTINVERT);
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
      theme = Win32Themes::OpenThemeData(nullptr, L"DarkMode::MENU");
      if(theme) {  
        Win32Themes::DrawThemeBackground(
          theme,
          dc,
          9, // MENU_POPUPBACKGROUND
          0,
          rect,
          nullptr);
        
        Win32Themes::CloseThemeData(theme);
        return;
      }
    }
    
    theme = Win32Themes::OpenThemeData(nullptr, L"MENU");
    if(!theme)
      goto FALLBACK;
      
    Win32Themes::DrawThemeBackground(
      theme,
      dc,
      7,  // MENU_BARBACKGROUND
      0,  // MB_ACTIVE
      rect,
      nullptr);
      
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
    HANDLE theme = Win32Themes::OpenThemeData(nullptr, L"MENU");
    
    if(!theme)
      goto FALLBACK;
      
    int _state;
    switch(state) {
      case ControlState::Hovered:        _state = 2; break;
      case ControlState::Pressed:        _state = 3; break;
      case ControlState::PressedHovered: _state = 3; break;
      default:             _state = 1; break;
    }
    
    Win32Themes::DrawThemeBackground(
      theme,
      dc,
      8,
      _state,
      rect,
      nullptr);
      
    Win32Themes::CloseThemeData(theme);
    return;
  }
  
FALLBACK:
  if(state == ControlState::Normal) 
    return;
  
  UINT edge;
  switch(state) {
    case ControlState::Hovered:        edge = BDR_RAISEDINNER; break;
    case ControlState::Pressed:        edge = BDR_SUNKENOUTER; break;
    case ControlState::PressedHovered: edge = BDR_SUNKENOUTER; break;
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
    return nullptr;
    
  HANDLE theme = nullptr;
  
  switch(type) {
    case ContainerType::PushButton:
    case ContainerType::DefaultPushButton:
    case ContainerType::CheckboxUnchecked:
    case ContainerType::CheckboxChecked:
    case ContainerType::CheckboxIndeterminate:
    case ContainerType::RadioButtonUnchecked:
    case ContainerType::RadioButtonChecked: 
      theme = w32cp_cache.button_theme(control.dpi(), control.is_using_dark_mode());
      break;
      
    case ContainerType::PaletteButton: 
      theme = w32cp_cache.toolbar_theme(control.dpi(), control.is_using_dark_mode());
      break;
      
    case ContainerType::InputField: 
      theme = w32cp_cache.edit_theme(control.dpi(), control.is_using_dark_mode());
      break;
    
    case ContainerType::AddressBandBackground:
      theme = w32cp_cache.addressband_theme(control.dpi(), control.is_using_dark_mode());
      break;
      
    case ContainerType::AddressBandInputField: 
      theme = w32cp_cache.addressband_combobox_theme(control.dpi(), control.is_using_dark_mode());
      break;
      
    case ContainerType::AddressBandGoButton:
      theme = w32cp_cache.toolbar_go_theme(control.dpi());
      break;
      
    case ContainerType::ListViewItem:
    case ContainerType::ListViewItemSelected:
      theme = w32cp_cache.explorer_listview_theme(control.dpi(), control.is_using_dark_mode());
      break;
    
    case ContainerType::OpenerTriangleClosed:
    case ContainerType::OpenerTriangleOpened:
      theme = w32cp_cache.explorer_treeview_theme(control.dpi(), control.is_using_dark_mode());
      break;
      
    case ContainerType::Panel:
    case ContainerType::TabHeadAbuttingRight:
    case ContainerType::TabHeadAbuttingLeftRight:
    case ContainerType::TabHeadAbuttingLeft:
    case ContainerType::TabHead:
    case ContainerType::TabHeadBackground:
    case ContainerType::TabBodyBackground:
    case ContainerType::TabPanelTopLeft:
    case ContainerType::TabPanelTopCenter:
    case ContainerType::TabPanelTopRight:
    case ContainerType::TabPanelCenterLeft:
    case ContainerType::TabPanelCenter:
    case ContainerType::TabPanelCenterRight:
    case ContainerType::TabPanelBottomLeft:
    case ContainerType::TabPanelBottomCenter:
    case ContainerType::TabPanelBottomRight:
    case ContainerType::TabHeadLeftAbuttingBottom:
    case ContainerType::TabHeadLeftAbuttingTopBottom:
    case ContainerType::TabHeadLeftAbuttingTop:
    case ContainerType::TabHeadLeft:
    case ContainerType::TabHeadRightAbuttingBottom:
    case ContainerType::TabHeadRightAbuttingTopBottom:
    case ContainerType::TabHeadRightAbuttingTop:
    case ContainerType::TabHeadRight:
    case ContainerType::TabHeadBottomAbuttingRight:
    case ContainerType::TabHeadBottomAbuttingLeftRight:
    case ContainerType::TabHeadBottomAbuttingLeft:
    case ContainerType::TabHeadBottom:
      theme = w32cp_cache.tab_theme(control.dpi());
      break;
    
    case ContainerType::ProgressIndicatorBackground:
    case ContainerType::ProgressIndicatorBar:
      theme = w32cp_cache.progress_theme(control.dpi());
      break;
    
    case ContainerType::HorizontalSliderChannel:
    case ContainerType::HorizontalSliderThumb: 
    case ContainerType::HorizontalSliderDownArrowButton: 
    case ContainerType::HorizontalSliderUpArrowButton: 
    case ContainerType::VerticalSliderChannel:
    case ContainerType::VerticalSliderThumb: 
    case ContainerType::VerticalSliderLeftArrowButton: 
    case ContainerType::VerticalSliderRightArrowButton: 
      theme = w32cp_cache.slider_theme(control.dpi());
      break;
      
    case ContainerType::TooltipWindow: 
      theme = w32cp_cache.tooltip_theme(control.dpi(), control.is_using_dark_mode());
      break;
      
    case ContainerType::NavigationBack: 
    case ContainerType::NavigationForward: 
      theme = w32cp_cache.navigation_theme(control.dpi(), control.is_using_dark_mode());
      break;
    
    default: return nullptr;
  }
  
  if(!theme)
    return nullptr;
  
  switch(type) {
    case ContainerType::TabHeadAbuttingRight:
    case ContainerType::TabHeadAbuttingLeftRight:
    case ContainerType::TabHeadAbuttingLeft:
    case ContainerType::TabHead:
    case ContainerType::TabHeadBottomAbuttingRight:
    case ContainerType::TabHeadBottomAbuttingLeftRight:
    case ContainerType::TabHeadBottomAbuttingLeft:
    case ContainerType::TabHeadBottom:
    case ContainerType::TabHeadLeftAbuttingBottom:
    case ContainerType::TabHeadLeftAbuttingTopBottom:
    case ContainerType::TabHeadLeftAbuttingTop:
    case ContainerType::TabHeadLeft:
    case ContainerType::TabHeadRightAbuttingBottom:
    case ContainerType::TabHeadRightAbuttingTopBottom:
    case ContainerType::TabHeadRightAbuttingTop:
    case ContainerType::TabHeadRight:
      break;
    
    default:
      if(state == ControlState::Pressed) {
        if(!control.is_focused_widget())
          state = ControlState::Normal;
      }
      else if(state == ControlState::PressedHovered) {
        if(!control.is_focused_widget())
          state = ControlState::Hovered;
      }
      break;
  }
  
  switch(type) {
    case ContainerType::GenericButton:
    case ContainerType::PushButton:
    case ContainerType::DefaultPushButton:
    case ContainerType::AddressBandGoButton:
    case ContainerType::PaletteButton: {
        *theme_part = 1;//BP_PUSHBUTTON / TP_BUTTON
        
        switch(state) {
          case ControlState::Disabled:        *theme_state = 4; break;
          case ControlState::PressedHovered:  *theme_state = 3; break;
          case ControlState::Hovered:         *theme_state = 2; break;
          case ControlState::Hot: {
              if(type == ContainerType::DefaultPushButton) {
                *theme_state = 6;
              }
              else
                *theme_state = 2;
            } break;
            
          case ControlState::Pressed:
          case ControlState::Normal:
            if(type == ContainerType::DefaultPushButton) {
              *theme_state = 5;
            }
            else
              *theme_state = 1;
        }
      } break;
      
    case ContainerType::AddressBandInputField: {
        *theme_part = 4;//CP_BORDER
        
        switch(state) {
          case ControlState::Normal:         *theme_state = 1; break;
          case ControlState::Hot:
          case ControlState::Hovered:        *theme_state = 2; break;
          case ControlState::Pressed:
          case ControlState::PressedHovered: *theme_state = 3; break; // = focused
          case ControlState::Disabled:       *theme_state = 4; break;
        }
      } break;
      
    case ContainerType::InputField: {
        *theme_part = 6;//EP_EDITBORDER_NOSCROLL
        
        switch(state) {
          case ControlState::Normal:         *theme_state = 1; break;
          case ControlState::Hot:
          case ControlState::Hovered:        *theme_state = 2; break;
          case ControlState::Pressed:
          case ControlState::PressedHovered: *theme_state = 3; break; // = focused
          case ControlState::Disabled:       *theme_state = 4; break;
        }
      } break;
    
    case ContainerType::AddressBandBackground: {
        *theme_part = 1; // ABBACKGROUND
        
        switch(state) {
          case ControlState::Normal:         *theme_state = 1; break;
          case ControlState::Hot:
          case ControlState::Hovered:        *theme_state = 2; break;
          case ControlState::Pressed:
          case ControlState::PressedHovered: *theme_state = 4; break; // = focused
          case ControlState::Disabled:       *theme_state = 3; break;
        }
      } break;
    
    case ContainerType::ListViewItem: {
        *theme_part = 1;//LVP_LISTITEM
        
        switch(state) {
          case ControlState::Normal:          theme = nullptr; break; // the LISS_NORMAL part (1) has a border
          case ControlState::Hot:
          case ControlState::Hovered:        *theme_state = 2; break;
          case ControlState::Pressed:
          case ControlState::PressedHovered: *theme_state = 6; break; // *theme_state = 3;
          case ControlState::Disabled:        theme = nullptr; break; // the LISS_DISABLED part (4) has a border
        }
      } break;
      
    case ContainerType::ListViewItemSelected: {
        *theme_part = 1;//LVP_LISTITEM
        
        switch(state) {
          case ControlState::Pressed:
          case ControlState::Normal:         *theme_state = 6; break; // *theme_state = 3;
          case ControlState::Hot:
          case ControlState::Hovered:        *theme_state = 6; break;
          case ControlState::PressedHovered: *theme_state = 6; break;
          case ControlState::Disabled:       *theme_state = 5; break;
        }
      } break;
      
    case ContainerType::TooltipWindow: {
        *theme_part  = 1; // TTP_STANDARD
        *theme_state = 1;
      } break;
    
    case ContainerType::Panel: {
        *theme_part  = 9; // TABP_PANE
        *theme_state = 0;
      } break;
    
    case ContainerType::ProgressIndicatorBackground: {
        if(Win32Themes::IsThemePartDefined(theme, 11, 0))
          *theme_part = 11; // PP_TRANSPARENTBAR
        else
          *theme_part = 1;  // PP_BAR
          
        *theme_state = 1;
      } break;
      
    case ContainerType::ProgressIndicatorBar: {
        if(Win32Themes::IsThemePartDefined(theme, 5, 0))
          *theme_part = 5; // PP_FILL
        else
          *theme_part = 3; // PP_CHUNK
          
        *theme_state = 1;
      } break;
      
    case ContainerType::HorizontalSliderChannel: {
        *theme_part  = 1; // TKP_TRACK
        *theme_state = 1;
      } break;
      
    case ContainerType::VerticalSliderChannel: {
        *theme_part  = 2; // TKP_TRACKVERT
        *theme_state = 1;
      } break;
      
    case ContainerType::HorizontalSliderThumb:
    case ContainerType::HorizontalSliderDownArrowButton:
    case ContainerType::HorizontalSliderUpArrowButton:
    case ContainerType::VerticalSliderThumb:
    case ContainerType::VerticalSliderLeftArrowButton:
    case ContainerType::VerticalSliderRightArrowButton: {
        switch(type) {
          default:
          case ContainerType::HorizontalSliderThumb:           *theme_part = 3; break; // TKP_THUMB
          case ContainerType::HorizontalSliderDownArrowButton: *theme_part = 4; break; // TKP_THUMBBOTTOM
          case ContainerType::HorizontalSliderUpArrowButton:   *theme_part = 5; break; // TKP_THUMBTOP
          case ContainerType::VerticalSliderThumb:             *theme_part = 6; break; // TKP_THUMBVERT
          case ContainerType::VerticalSliderLeftArrowButton:   *theme_part = 7; break; // TKP_THUMBLEFT
          case ContainerType::VerticalSliderRightArrowButton:  *theme_part = 8; break; // TKP_THUMBRIGHT
        }
        switch(state) {
          case ControlState::Hot:
          case ControlState::Normal:         *theme_state = 1; break;
          case ControlState::Pressed:        *theme_state = 4; break;
          case ControlState::Hovered:        *theme_state = 2; break;
          case ControlState::PressedHovered: *theme_state = 3; break;
          case ControlState::Disabled:       *theme_state = 5; break;
        }
      } break;
      
    case ContainerType::CheckboxUnchecked: {
        *theme_part = 3; // BP_CHECKBOX
        switch(state) {
          case ControlState::Normal:         *theme_state = 1; break;
          case ControlState::Pressed:
          case ControlState::Hot:
          case ControlState::Hovered:        *theme_state = 2; break;
          case ControlState::PressedHovered: *theme_state = 3; break;
          case ControlState::Disabled:       *theme_state = 4; break;
        }
      } break;
      
    case ContainerType::CheckboxChecked: {
        *theme_part = 3; // BP_CHECKBOX
        switch(state) {
          case ControlState::Normal:         *theme_state = 5; break;
          case ControlState::Pressed:
          case ControlState::Hot:
          case ControlState::Hovered:        *theme_state = 6; break;
          case ControlState::PressedHovered: *theme_state = 7; break;
          case ControlState::Disabled:       *theme_state = 8; break;
        }
      } break;
      
    case ContainerType::CheckboxIndeterminate: {
        *theme_part = 3; // BP_CHECKBOX
        switch(state) {
          case ControlState::Normal:         *theme_state = 9; break;
          case ControlState::Pressed:
          case ControlState::Hot:
          case ControlState::Hovered:        *theme_state = 10; break;
          case ControlState::PressedHovered: *theme_state = 11; break;
          case ControlState::Disabled:       *theme_state = 12; break;
        }
      } break;
      
    case ContainerType::RadioButtonUnchecked: {
        *theme_part = 2; // BP_RADIOBUTTON
        switch(state) {
          case ControlState::Normal:         *theme_state = 1; break;
          case ControlState::Pressed:
          case ControlState::Hot:
          case ControlState::Hovered:        *theme_state = 2; break;
          case ControlState::PressedHovered: *theme_state = 3; break;
          case ControlState::Disabled:       *theme_state = 4; break;
        }
      } break;
      
    case ContainerType::RadioButtonChecked: {
        *theme_part = 2; // BP_RADIOBUTTON
        switch(state) {
          case ControlState::Normal:         *theme_state = 5; break;
          case ControlState::Pressed:
          case ControlState::Hot:
          case ControlState::Hovered:        *theme_state = 6; break;
          case ControlState::PressedHovered: *theme_state = 7; break;
          case ControlState::Disabled:       *theme_state = 8; break;
        }
      } break;
    
    case ContainerType::OpenerTriangleClosed: {
        *theme_state = 1; // GLPS_CLOSED / HGLPS_CLOSED
        switch(state) {
          case ControlState::Disabled:
          case ControlState::Normal:         *theme_part = 2; break; // TVP_GLYPH
          case ControlState::Pressed:
          case ControlState::Hot:
          case ControlState::Hovered:        
          case ControlState::PressedHovered: *theme_part = 4; break; // TVP_HOTGLYPH
        }
      } break;
      
    case ContainerType::OpenerTriangleOpened: {
        *theme_state = 2; // GLPS_OPENED / HGLPS_OPENED
        switch(state) {
          case ControlState::Disabled:
          case ControlState::Normal:         *theme_part = 2; break; // TVP_GLYPH
          case ControlState::Pressed:
          case ControlState::Hot:
          case ControlState::Hovered:        
          case ControlState::PressedHovered: *theme_part = 4; break; // TVP_HOTGLYPH
        }
      } break;
    
    case ContainerType::NavigationBack:
    case ContainerType::NavigationForward: {
        *theme_part = type == ContainerType::NavigationBack ? 1 : 2; // NAV_BACKBUTTON, NAV_FORWARDBUTTON
        *theme_state = 1; // NAV_BB_NORMAL
        switch(state) {
          case ControlState::Disabled:       *theme_state = 4; break; // NAV_BB_DISABLED
          case ControlState::Normal:
          case ControlState::Pressed:        *theme_state = 1; break; // NAV_BB_NORMAL
          case ControlState::Hot:
          case ControlState::Hovered:        *theme_state = 2; break; // NAV_BB_HOT
          case ControlState::PressedHovered: *theme_state = 3; break; // NAV_BB_PRESSED
        }
    } break;
    
    case ContainerType::TabHeadAbuttingRight:
    case ContainerType::TabHeadAbuttingLeftRight:
    case ContainerType::TabHeadAbuttingLeft:
    case ContainerType::TabHead: 
    case ContainerType::TabHeadBottomAbuttingRight:
    case ContainerType::TabHeadBottomAbuttingLeftRight:
    case ContainerType::TabHeadBottomAbuttingLeft:
    case ContainerType::TabHeadBottom: 
    case ContainerType::TabHeadLeftAbuttingBottom:
    case ContainerType::TabHeadLeftAbuttingTopBottom:
    case ContainerType::TabHeadLeftAbuttingTop:
    case ContainerType::TabHeadLeft: 
    case ContainerType::TabHeadRightAbuttingBottom:
    case ContainerType::TabHeadRightAbuttingTopBottom:
    case ContainerType::TabHeadRightAbuttingTop:
    case ContainerType::TabHeadRight: {
        *theme_state = 1; // TIS_NORMAL
        switch(state) {
          case ControlState::Disabled:       *theme_state = 4; break; // TIS_DISABLED
          case ControlState::Normal:         *theme_state = 1; break; // TIS_NORMAL
          case ControlState::Hot:            *theme_state = 1; break; // TIS_NORMAL
          case ControlState::Hovered:        *theme_state = 2; break; // TIS_HOT
          case ControlState::Pressed:        *theme_state = 3; break; // TIS_SELECTED
          case ControlState::PressedHovered: *theme_state = 3; break; // TIS_SELECTED
        }
    } break;
    
    case ContainerType::TabHeadBackground: 
    case ContainerType::TabBodyBackground: 
    case ContainerType::TabPanelTopLeft:
    case ContainerType::TabPanelTopCenter:
    case ContainerType::TabPanelTopRight:
    case ContainerType::TabPanelCenterLeft:
    case ContainerType::TabPanelCenter:
    case ContainerType::TabPanelCenterRight:
    case ContainerType::TabPanelBottomLeft:
    case ContainerType::TabPanelBottomCenter:
    case ContainerType::TabPanelBottomRight: {
        *theme_part = 9; // TABP_PANE  // TODO: combine with TABP_BODY ....
        *theme_state = 0;
    } break;
    
    default: break;
  }
  
  switch(type) {
    case ContainerType::TabHead:
    case ContainerType::TabHeadBottom: 
    case ContainerType::TabHeadLeft: 
    case ContainerType::TabHeadRight: 
      // Windows 10: TABP_TABITEM has no line on the left, looks exactly like TABP_TABITEMBOTHEDGE
      //*theme_part = 1; // TABP_TABITEM
      //break;
    case ContainerType::TabHeadAbuttingRight: 
    case ContainerType::TabHeadBottomAbuttingRight: 
    case ContainerType::TabHeadLeftAbuttingBottom:
    case ContainerType::TabHeadRightAbuttingBottom:
      *theme_part = 2; // TABP_TABITEMLEFTEDGE
      break;
    case ContainerType::TabHeadAbuttingLeft:
    case ContainerType::TabHeadBottomAbuttingLeft:
    case ContainerType::TabHeadLeftAbuttingTop:
    case ContainerType::TabHeadRightAbuttingTop:
      *theme_part = 3; // TABP_TABITEMRIGHTEDGE
      break;
    case ContainerType::TabHeadAbuttingLeftRight:
    case ContainerType::TabHeadBottomAbuttingLeftRight:
    case ContainerType::TabHeadLeftAbuttingTopBottom:
    case ContainerType::TabHeadRightAbuttingTopBottom:
      *theme_part = 4; // TABP_TABITEMBOTHEDGE
      break;
  }
  
  return theme;
}

void Win32ControlPainter::clear_cache() {
  w32cp_cache.clear();
}

//} ... class Win32ControlPainter

//{ class Win32ControlPainter::Impl ...

Win32ControlPainter::Impl::Impl(Win32ControlPainter &self)
  : self{self}
{
}

Color Win32ControlPainter::Impl::get_sys_color(int index) {
  return Color::from_bgr24(GetSysColor(index));
}

bool Win32ControlPainter::Impl::dark_mode_is_fake(ContainerType type) {
  switch(type) {
    case ContainerType::ProgressIndicatorBackground:
    case ContainerType::HorizontalSliderChannel:
    case ContainerType::Panel:
    case ContainerType::TabHeadAbuttingRight:
    case ContainerType::TabHeadAbuttingLeftRight:
    case ContainerType::TabHeadAbuttingLeft:
    case ContainerType::TabHead:
    case ContainerType::TabHeadBottomAbuttingRight:
    case ContainerType::TabHeadBottomAbuttingLeftRight:
    case ContainerType::TabHeadBottomAbuttingLeft:
    case ContainerType::TabHeadBottom:
    case ContainerType::TabHeadBackground:
    case ContainerType::TabBodyBackground:
    case ContainerType::TabPanelTopLeft:
    case ContainerType::TabPanelTopCenter:
    case ContainerType::TabPanelTopRight:
    case ContainerType::TabPanelCenterLeft:
    case ContainerType::TabPanelCenter:
    case ContainerType::TabPanelCenterRight:
    case ContainerType::TabPanelBottomLeft:
    case ContainerType::TabPanelBottomCenter:
    case ContainerType::TabPanelBottomRight:
    case ContainerType::TabHeadLeftAbuttingBottom:
    case ContainerType::TabHeadLeftAbuttingTopBottom:
    case ContainerType::TabHeadLeftAbuttingTop:
    case ContainerType::TabHeadLeft:
    case ContainerType::TabHeadRightAbuttingBottom:
    case ContainerType::TabHeadRightAbuttingTopBottom:
    case ContainerType::TabHeadRightAbuttingTop:
    case ContainerType::TabHeadRight:
      return true;
    
    default: return false;
  }
}

void Win32ControlPainter::Impl::draw_toggle_switch_channel(Canvas &canvas, RectangleF rect, ControlState state, bool active, bool dark) {
  Color c = canvas.get_color();
  BoxRadius radii(rect.height/2);
  rect.add_round_rect_path(canvas, radii, false);
  
  if(active) {
    Color accent = Color::None;
    if(Win32Version::is_windows_10_or_newer()) {
      Win32Themes::ColorizationInfo info {};
      if(Win32Themes::try_read_win10_colorization(&info))
        accent = Color::from_bgr24(info.accent_color & 0xFFFFFF);
    }
    
    if(!accent)
      accent = get_sys_color(COLOR_HIGHLIGHT);
    
    switch(state) {
      case ControlState::Normal:         canvas.set_color(accent); break;
      case ControlState::Hovered:
      case ControlState::Hot:            canvas.set_color(Color::blend(accent, Color::White, 0.25)); break;
      case ControlState::Pressed:        
      case ControlState::PressedHovered: canvas.set_color(Color::blend(accent, Color::Black, 0.25)); break;
      case ControlState::Disabled:       canvas.set_color(Color::Black, 0.5); break;
    }
  }
  else {
    Color fg = dark ? Color::White : Color::Black;
    
    switch(state) {
      case ControlState::Pressed:
      case ControlState::PressedHovered:
        canvas.set_color(fg, 0.4);
        canvas.fill_preserve();
        break;
      default: break;
    }
    
    rect.grow(-0.75, -0.75);
    radii+= BoxRadius(-0.75);
    rect.add_round_rect_path(canvas, radii, true);
    
    switch(state) {
      case ControlState::Normal:         canvas.set_color(fg, 0.6); break;
      case ControlState::Hovered:
      case ControlState::Hot:            canvas.set_color(fg, 0.8); break;
      case ControlState::Pressed:        
      case ControlState::PressedHovered: canvas.set_color(fg); break;
      case ControlState::Disabled:       canvas.set_color(fg, 0.5); break;
    }
  }
  canvas.fill();
  canvas.set_color(c);
}

void Win32ControlPainter::Impl::draw_toggle_switch_thumb(Canvas &canvas, RectangleF rect, ControlState state, bool active, bool dark) {
  Color c = canvas.get_color();
  
  auto size = std::min(rect.width, rect.height);
  auto center = rect.center();
  rect = {center, center};
  rect.grow(size/4);
  BoxRadius radii(rect.height/2);
  rect.add_round_rect_path(canvas, radii, false);
  
  if(active)
    canvas.set_color(Color::White);
  else if(dark)
    canvas.set_color(Color::White);
  else
    canvas.set_color(Color::Black);
    
  canvas.fill();
  canvas.set_color(c);
}

bool Win32ControlPainter::Impl::try_get_sizing_margin(ControlContext &control, ContainerType type, ControlState state, Win32Themes::MARGINS *mar) {
  int _part, _state;
  if(HANDLE theme = self.get_control_theme(control, type, state, &_part, &_state)) {
    // 3601 = TMT_SIZINGMARGINS
    return SUCCEEDED(Win32Themes::GetThemeMargins(theme, nullptr, _part, _state, 3601, nullptr, mar));
  }
  else
    return false;
}

//} ... class Win32ControlPainter::Impl
