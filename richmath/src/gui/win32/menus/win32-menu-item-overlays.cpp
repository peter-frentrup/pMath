#include <gui/win32/menus/win32-menu-item-overlays.h>


using namespace richmath;

//{ class MenuItemOverlay ...

MenuItemOverlay::MenuItemOverlay()
: next{nullptr},
  control{nullptr},
  start_index{-1},
  end_index{-1}
{
  SET_BASE_DEBUG_TAG(typeid(*this).name());
}

MenuItemOverlay::~MenuItemOverlay() {
  if(control)
    DestroyWindow(control);
}

void MenuItemOverlay::delete_all() {
  delete_all(this);
}

void MenuItemOverlay::delete_all(MenuItemOverlay *first_overlay) {
  while(auto tmp = first_overlay) {
    first_overlay = first_overlay->next;
    delete tmp;
  }
}

bool MenuItemOverlay::handle_mouse_message(UINT msg, WPARAM wParam, const POINT &pt, HMENU menu) {
  return false;
}

void MenuItemOverlay::calc_rect(RECT &rect, HWND hwnd, HMENU menu, Area area) {
  rect = {0,0,0,0};
  
  int menu_item_height = 16;
  bool first = true;
  for(int i = start_index; i <= end_index; ++i) {
    RECT item_rect;
    if(GetMenuItemRect(nullptr, menu, (UINT)i, &item_rect)) {
      if(first) {
        rect = item_rect;
        menu_item_height = item_rect.bottom - item_rect.top;
        first = false;
      }
      else
        UnionRect(&rect, &rect, &item_rect);
    }
  }
  
  switch(area) {
    case OnlyGutter:
      rect.right = rect.left + menu_item_height;
      break;
    
    case OnlyContentArea:
      rect.left += menu_item_height;
      break;
    
    case All:
    default:
      break;
  }
  
  MapWindowPoints(nullptr, hwnd, (POINT*)&rect, 2);
}

//} ... class MenuItemOverlay
