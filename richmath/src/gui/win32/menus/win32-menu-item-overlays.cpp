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

bool Win32MenuItemOverlay::calc_rect(RECT &rect, HWND hwnd, HMENU menu, Area area) {
  rect = {0,0,0,0};
  
  bool first = true;
  for(int i = start_index; i <= end_index; ++i) {
    RECT item_rect;
    if(GetMenuItemRect(nullptr, menu, (UINT)i, &item_rect)) {
      if(first) {
        rect = item_rect;
        first = false;
      }
      else
        UnionRect(&rect, &rect, &item_rect);
    }
    else 
      return false;
  }
  
  int dpi = Win32HighDpi::get_dpi_for_window(hwnd);
  
  SIZE glyph_size = {0,0};
  Win32Themes::MARGINS glyph_margins = {0, 0, 0, 0};
  Win32Themes::MARGINS popup_margins = {0, 0, 0, 0};
  
  if(true/*area != All*/) {
    bool has_glyph_size = false;
    bool has_glyph_margins = false;
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
        
        {
          SIZE gutter_size;
          bool has_gutter_size = SUCCEEDED(Win32Themes::GetThemePartSize(
            theme, nullptr, 
            13, // MENU_POPUPGUTTER
            1, // MC_CHECKMARKNORMAL
            nullptr, 
            Win32Themes::TS_TRUE, 
            &gutter_size));
            
          if(has_gutter_size) {
            glyph_margins.cxRightWidth += gutter_size.cx;
          }
        }
        
        {
          Win32Themes::MARGINS item_margins = {0,0,0,0};
          bool has_item_margins = SUCCEEDED(Win32Themes::GetThemeMargins(
            theme, nullptr, 
            14, // MENU_POPUPITEM
            1, // MPI_NORMAL
            3602, // TMT_CONTENTMARGINS
            nullptr, 
            &item_margins));
          
          if(has_item_margins) {
            glyph_margins.cxRightWidth += item_margins.cxLeftWidth;
          }
        }
        
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
    
    if(!has_glyph_margins) {
      glyph_margins.cxLeftWidth    = 2;
      glyph_margins.cxRightWidth   = 2;
      glyph_margins.cyTopHeight    = 2;
      glyph_margins.cyBottomHeight = 2;
    }
    
    if(!has_popup_margins) {
      memset(&popup_margins, 0, sizeof(popup_margins));
    }
  }
  
  switch(area) {
    case OnlyGutter:
      rect.left += popup_margins.cxLeftWidth + glyph_margins.cxLeftWidth;
      rect.right = rect.left + glyph_size.cx;
      break;
    
    case OnlyContentArea:
      rect.left += popup_margins.cxLeftWidth;
      rect.left += glyph_margins.cxLeftWidth + glyph_size.cx + glyph_margins.cxRightWidth;
      break;
    
    case All:
      rect.left  += popup_margins.cxLeftWidth;
      rect.right -= popup_margins.cxRightWidth;
    default:
      break;
  }
  
  MapWindowPoints(nullptr, hwnd, (POINT*)&rect, 2);
  
  pmath_debug_print("[menu overlay %d..%d: %d %d  %d x %d]\n",
    start_index, end_index,
    rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top);
  return true;
}

void Win32MenuItemOverlay::prepare_menu_window_for_children(HWND hwnd) {
  DWORD style = GetWindowLongW(hwnd, GWL_STYLE);
  if(!(style & WS_CLIPCHILDREN))
    SetWindowLongW(hwnd, GWL_STYLE, style | WS_CLIPCHILDREN);
}

//} ... class Win32MenuItemOverlay
