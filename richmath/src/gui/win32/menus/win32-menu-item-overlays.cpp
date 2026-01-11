#include <gui/win32/menus/win32-menu-item-overlays.h>

#include <gui/win32/api/win32-highdpi.h>
#include <gui/win32/api/win32-version.h>


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
  
  int menu_gutter_width = 16;
  if(area != All) {
    int dpi = Win32HighDpi::get_dpi_for_window(hwnd);
    menu_gutter_width = Win32HighDpi::get_system_metrics_for_dpi(SM_CXMENUCHECK, dpi);
  }
  
  switch(area) {
    case OnlyGutter:
      rect.right = rect.left + menu_gutter_width;
      break;
    
    case OnlyContentArea:
      rect.left += menu_gutter_width;
      break;
    
    case All:
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
