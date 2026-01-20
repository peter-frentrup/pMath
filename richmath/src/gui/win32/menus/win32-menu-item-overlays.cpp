#include <gui/win32/menus/win32-menu-item-overlays.h>

#include <gui/win32/api/win32-highdpi.h>
#include <gui/win32/api/win32-themes.h>
#include <gui/win32/api/win32-version.h>

#ifdef max
#  undef max
#endif

using namespace richmath;

//{ class Win32MenuItemOverlay ...

Win32MenuItemOverlay::Win32MenuItemOverlay()
: next{nullptr},
  control{nullptr},
  start_index{-1},
  end_index{-1}
{
  SET_BASE_DEBUG_TAG(typeid(*this).name());
}

Win32MenuItemOverlay::~Win32MenuItemOverlay() {
  if(control)
    WIN32report(DestroyWindow(control));
}

void Win32MenuItemOverlay::delete_all() {
  delete_all(this);
}

void Win32MenuItemOverlay::delete_all(Win32MenuItemOverlay *first_overlay) {
  while(auto tmp = first_overlay) {
    first_overlay = first_overlay->next;
    delete tmp;
  }
}

bool Win32MenuItemOverlay::consumes_navigation_key(DWORD keycode, HMENU menu, int sel_item) {
  return false;
}

bool Win32MenuItemOverlay::handle_char_message(WPARAM wParam, LPARAM lParam, HMENU menu) {
  return false;
}

bool Win32MenuItemOverlay::handle_keydown_message(WPARAM wParam, LPARAM lParam, HMENU menu) {
  return false;
}

bool Win32MenuItemOverlay::handle_mouse_message(UINT msg, WPARAM wParam, const POINT &pt, HMENU menu) {
  return false;
}

void Win32MenuItemOverlay::on_mouse_leave() {
}

bool Win32MenuItemOverlay::calc_layout(Layout &layout, HWND hwnd, HMENU menu) {
  memset(&layout, 0, sizeof(layout));
  
  bool first = true;
  for(int i = start_index; i <= end_index; ++i) {
    RECT item_rect;
    if(GetMenuItemRect(nullptr, menu, (UINT)i, &item_rect)) {
      if(first) {
        layout.rect = item_rect;
        first = false;
      }
      else
        UnionRect(&layout.rect, &layout.rect, &item_rect);
    }
    else 
      return false;
  }
  
  int dpi = Win32HighDpi::get_dpi_for_window(hwnd);
  
  SIZE glyph_size = {0,0};
  SIZE gutter_size = {0, 0};
  Win32Themes::MARGINS glyph_margins = {0, 0, 0, 0};
  Win32Themes::MARGINS content_margins = {0, 0, 0, 0};
  Win32Themes::MARGINS popup_margins = {0, 0, 0, 0};
  bool has_glyph_size = false;
  bool has_gutter_size = false;
  bool has_glyph_margins = false;
  bool has_content_margins = false;
  bool has_popup_margins = false;
  
  if(Win32Themes::is_app_themed() &&
      Win32Themes::OpenThemeData && 
      Win32Themes::CloseThemeData && 
      Win32Themes::GetThemeMargins &&
      Win32Themes::GetThemePartSize
  ) {
    if(HANDLE theme = Win32Themes::open_theme_data_for_dpi(nullptr, L"MENU", (UINT)dpi)) {
      has_glyph_size = SUCCEEDED(Win32Themes::GetThemePartSize(
        theme, nullptr, 
        11, // MENU_POPUPCHECK
        1, // MC_CHECKMARKNORMAL
        nullptr, 
        Win32Themes::TS_TRUE, 
        &glyph_size));
      
      // TMT_CONTENTMARGINS = 3602
      has_glyph_margins = SUCCEEDED(Win32Themes::GetThemeMargins(
        theme, nullptr, 
        11, // MENU_POPUPCHECK
        1, // MC_CHECKMARKNORMAL
        3602, // TMT_CONTENTMARGINS
        nullptr, 
        &glyph_margins));
      
      {
        Win32Themes::MARGINS check_bg_margins = {0,0,0,0};
        bool has_check_bg_margins = SUCCEEDED(Win32Themes::GetThemeMargins(
          theme, nullptr, 
          12, // MENU_POPUPCHECKBACKGROUND
          1, // MCB_NORMAL
          3602, // TMT_CONTENTMARGINS
          nullptr, 
          &check_bg_margins));
        if(has_check_bg_margins) {
          glyph_margins.cxLeftWidth  += check_bg_margins.cxLeftWidth; // normally 0
          glyph_margins.cxRightWidth += check_bg_margins.cxRightWidth;
        }
      }
      
      has_gutter_size = SUCCEEDED(Win32Themes::GetThemePartSize(
        theme, nullptr, 
        13, // MENU_POPUPGUTTER
        1, // MC_CHECKMARKNORMAL
        nullptr, 
        Win32Themes::TS_TRUE, 
        &gutter_size));
        
      
      has_content_margins = SUCCEEDED(Win32Themes::GetThemeMargins(
        theme, nullptr, 
        14, // MENU_POPUPITEM
        1, // MPI_NORMAL
        3602, // TMT_CONTENTMARGINS
        nullptr, 
        &content_margins));
      
      
      if(Win32Version::is_windows_11_or_newer()) {
        // Windows 11 has part 26 for keyboard focus menu item background
        //            and part 27 for selection/hover menu item background
        // Those have CONTENTMARGINS:  (6,6,2,2) for part 26
        //                             (3,3,2,2) for part 27
        // Windows 10 did not have those it seems (at least version 10.0.18362.329).
        has_popup_margins = SUCCEEDED(Win32Themes::GetThemeMargins(
          theme, nullptr, 
          27, // selection/hover menu item background
          1, // 
          3602, // TMT_CONTENTMARGINS
          nullptr, 
          &popup_margins));
        
        Win32Themes::MARGINS kb_popup_margins = {0,0,0,0};
        bool has_kb_popup_margins = SUCCEEDED(Win32Themes::GetThemeMargins(
          theme, nullptr, 
          26, // keyboard focus menu item background
          1, // 
          3602, // TMT_CONTENTMARGINS
          nullptr, 
          &kb_popup_margins));
        
        if(has_kb_popup_margins) {
          glyph_margins.cxLeftWidth += std::max(0, kb_popup_margins.cxLeftWidth  - popup_margins.cxLeftWidth);
          //glyph_margins.cxRightWidth += kb_popup_margins.cxRightWidth - popup_margins.cxRightWidth;
        }
      }
      
      Win32Themes::CloseThemeData(theme);
    }
  }
  
  if(!has_glyph_size) {
    glyph_size.cx = Win32HighDpi::get_system_metrics_for_dpi(SM_CXMENUCHECK, dpi);
    glyph_size.cy = Win32HighDpi::get_system_metrics_for_dpi(SM_CYMENUCHECK, dpi);
  }
  
  if(!has_gutter_size) {
    gutter_size.cx = 2;
    gutter_size.cy = 0;
  }
  
  if(!has_glyph_margins) {
    glyph_margins.cxLeftWidth    = 2;
    glyph_margins.cxRightWidth   = 2;
    glyph_margins.cyTopHeight    = 2;
    glyph_margins.cyBottomHeight = 2;
  }
  
  if(!has_content_margins) {
    content_margins.cxLeftWidth    = 2;
    content_margins.cxRightWidth   = 2;
    content_margins.cyTopHeight    = 2;
    content_margins.cyBottomHeight = 2;
  }
  
  if(!has_popup_margins) {
    content_margins.cxLeftWidth    = 0;
    content_margins.cxRightWidth   = 0;
    content_margins.cyTopHeight    = 0;
    content_margins.cyBottomHeight = 0;
  }
  
  MapWindowPoints(nullptr, hwnd, (POINT*)&layout.rect, 2);
  
  layout.popup_left = layout.rect.left + popup_margins.cxLeftWidth;
  layout.glyph_left = layout.popup_left + glyph_margins.cxLeftWidth;
  layout.glyph_right = layout.glyph_left + glyph_size.cx;
  layout.content_left = layout.glyph_right + glyph_margins.cxRightWidth + gutter_size.cx + content_margins.cxLeftWidth;
  
  layout.popup_right = layout.rect.right - popup_margins.cxRightWidth;
  layout.content_right = layout.popup_right - content_margins.cxRightWidth;
  return true;
}

bool Win32MenuItemOverlay::calc_rect(RECT &rect, HWND hwnd, HMENU menu, Area area) {
  Layout layout;
  if(calc_layout(layout, hwnd, menu)) {
    rect = layout.rect_for(area);
    return true;
  }
  
  rect = {0,0,0,0};
  return false;
}

void Win32MenuItemOverlay::prepare_menu_window_for_children(HWND hwnd) {
  DWORD style = GetWindowLongW(hwnd, GWL_STYLE);
  if(!(style & WS_CLIPCHILDREN))
    SetWindowLongW(hwnd, GWL_STYLE, style | WS_CLIPCHILDREN);
}

//} ... class Win32MenuItemOverlay

//{ class Win32MenuItemOverlay::Layout ...

RECT Win32MenuItemOverlay::Layout::rect_for(Area area) const {
  switch(area) {
    default:
    case All:             return { popup_left,   rect.top, popup_right,   rect.bottom};
    case OnlyGutter:      return { glyph_left,   rect.top, glyph_right,   rect.bottom};
    case OnlyContentArea: return { content_left, rect.top, content_right, rect.bottom};
  }
}

//} ... class Win32MenuItemOverlay::Layout
