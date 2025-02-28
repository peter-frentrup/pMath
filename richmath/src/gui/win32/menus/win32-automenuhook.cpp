#include <gui/win32/menus/win32-automenuhook.h>

#include <gui/win32/api/win32-highdpi.h>
#include <gui/win32/api/win32-version.h>
#include <gui/win32/menus/win32-menu.h>

#include <gui/menus.h>


using namespace pmath;
using namespace richmath;

namespace richmath {
  class Win32AutoMenuHook::Impl {
    public:
      Impl(Win32AutoMenuHook &self) : self(self) {}
      
    public:
      static void push(Win32AutoMenuHook *handler);
      static void pop(Win32AutoMenuHook *handler);
      
      static LRESULT CALLBACK menu_hook_proc(int code, WPARAM wParam, LPARAM lParam);
      
    public:
      bool handle_mouse_movement(UINT message, WPARAM wParam, POINT pt);
      void handle_popup(HMENU menu, DWORD subitems_cmd_id, DWORD cmd_id, POINT pt);
      bool handle_key_down(DWORD keycode);
      
    private:
      static HHOOK the_hook;
      static Win32AutoMenuHook *current;
      
    private:
      Win32AutoMenuHook &self;
  };
  
  class Win32MenuItemPopupMenu {
    public:
      static HMENU create_popup_for(Expr list_cmd, Expr cmd);
      static void append(HMENU menu, SpecialCommandID id, String text, UINT flags = MF_ENABLED);
      static SpecialCommandID show_popup_for(HWND owner, POINT pt, Expr list_cmd, Expr cmd);
  };
}

namespace richmath { namespace strings {
  extern String CloseMenu_label;
  extern String MenuListSearchCommands;
}}

HHOOK              Win32AutoMenuHook::Impl::the_hook = nullptr;
Win32AutoMenuHook *Win32AutoMenuHook::Impl::current = nullptr;

extern pmath_symbol_t richmath_FrontEnd_DocumentOpen;
extern pmath_symbol_t richmath_FrontEnd_SetSelectedDocument;

static const char MenuWindowClass[] = "#32768";

static int find_hilite_menuitem_cmd(HMENU *menu, DWORD *list_cmd, DWORD *cmd);
static int find_hilite_menuitem_cmd(HMENU *menu, Expr *list_cmd, Expr *cmd);

static POINT dword_to_point(DWORD dw) {
  POINT pt;
  pt.x = (short)LOWORD(dw);
  pt.y = (short)HIWORD(dw);
  return pt;
}

static DWORD point_to_dword(const POINT &pt) {
  return ((short)pt.x) | (((short)pt.y) << 16);
}

//{ class Win32AutoMenuHook ...


bool MenuExitInfo::handle_after_exit() {
  switch(reason) {
    case MenuExitReason::Other: 
    case MenuExitReason::LeftKey:
    case MenuExitReason::RightKey:
    case MenuExitReason::ExplicitCmd:
      return false;
    
    case MenuExitReason::LocateItemSource:
      return Menus::locate_dynamic_submenu_item_source(
               Win32Menu::id_to_command(list_cmd), 
               Win32Menu::id_to_command(cmd));
  }
  
  return false;
}

//} ... class Win32AutoMenuHook

//{ class Win32AutoMenuHook ...

Win32AutoMenuHook::Win32AutoMenuHook(HMENU tracked_popup, HWND owner, HWND mouse_notifications, bool allow_leave_left, bool allow_leave_right) 
  : _next(nullptr),
    _current_popup(tracked_popup),
    _owner(owner),
    _mouse_notifications(mouse_notifications),
    _mouse_capture(nullptr),
    _allow_leave_left(allow_leave_left),
    _allow_leave_right(allow_leave_right),
    _is_over_menu(false),
    _ignore_syschar(true)
{
  Impl::push(this);
}

Win32AutoMenuHook::~Win32AutoMenuHook() {
  Impl::pop(this);
}

bool Win32AutoMenuHook::handle(MSG &msg) {
//  {
//    const int len = 20;
//    char clsname[len];
//    GetClassNameA(msg.hwnd, clsname, len);
//    clsname[len-1] = '\0';
////    if(0 == strcmp(clsname, MenuWindowClass)) {
//      if(msg.message != WM_TIMER) {
//        pmath_debug_print("[Win32AutoMenuHook hwnd=%p (%s) msg 0x%x w=%x l=%x]\n", 
//          msg.hwnd, clsname, msg.message, msg.wParam, msg.lParam);
//      }
////    }
//  }
  
  switch(msg.message) {
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_MOUSEMOVE: 
      // Strangely, lParam is in screen coordinates during the message hook.
      return Impl(*this).handle_mouse_movement(msg.message, msg.wParam, dword_to_point(msg.lParam));
    
    // WM_MENURBUTTONUP is not sent through the hook, so we handle WM_RBUTTONUP instead
    case WM_RBUTTONUP: {
        DWORD subitems_cmd_id;
        DWORD cmd_id;
        HMENU menu = _current_popup;
        int item = find_hilite_menuitem_cmd(&menu, &subitems_cmd_id, &cmd_id);
        
        if(menu && item >= 0) {
          Impl(*this).handle_popup(menu, subitems_cmd_id, cmd_id, dword_to_point(msg.lParam));
          return true;
        }
      } break;
    
    case WM_KEYDOWN: return Impl(*this).handle_key_down(msg.wParam);
    
    case WM_SYSCHAR: 
      if(_ignore_syschar) {
        _ignore_syschar = false;
        return true;
      }
      break;
  }
  
  return false;
}

//} ... class Win32AutoMenuHook

//{ class Win32AutoMenuHook::Impl ...

void Win32AutoMenuHook::Impl::push(Win32AutoMenuHook *handler) {
  RICHMATH_ASSERT(handler != nullptr);
  RICHMATH_ASSERT(handler->_next == nullptr);
  
  if(!current) {
    if(!the_hook)
      the_hook = SetWindowsHookEx(WH_MSGFILTER, menu_hook_proc, nullptr, GetCurrentThreadId());
  }
  
  handler->_next = current;
  current = handler;
}

void Win32AutoMenuHook::Impl::pop(Win32AutoMenuHook *handler) {
  RICHMATH_ASSERT(handler == current);
  
  current = current->_next;
  if(!current) {
    if(the_hook)
      UnhookWindowsHookEx(the_hook);
    
    the_hook = nullptr;
  }
}

LRESULT CALLBACK Win32AutoMenuHook::Impl::menu_hook_proc(int code, WPARAM h_wParam, LPARAM h_lParam) {
  if(code == MSGF_MENU && current) {
    MSG *msg = (MSG *)h_lParam;
    
    Win32AutoMenuHook *handlers = current;
    for(;handlers; handlers = handlers->_next) {
      if(handlers->handle(*msg))
        return TRUE;
    }
  }
  
  return CallNextHookEx(0, code, h_wParam, h_lParam);
}

bool Win32AutoMenuHook::Impl::handle_mouse_movement(UINT message, WPARAM wParam, POINT pt) {
  HWND dst = self._mouse_capture ? self._mouse_capture : WindowFromPoint(pt);
  HWND menu_window = nullptr;
  for(HWND hwnd = dst; hwnd; hwnd = GetAncestor(hwnd, GA_PARENT)) {
    const int len = 20;
    char class_name[len];
    GetClassNameA(hwnd, class_name, len);
    class_name[len-1] = '\0';
    
    if(0 == strcmp(class_name, MenuWindowClass)) {
      menu_window = hwnd;
      break;
    }
  }
  
  bool handled = false;
  bool is_over_menu = menu_window != nullptr;
  if(is_over_menu) {
    handled = Win32Menu::handle_child_window_mouse_message(menu_window, dst, message, wParam, pt);
    
    if(message == WM_LBUTTONDOWN && handled)
      self._mouse_capture = dst;
  }
  else {
    if(self._is_over_menu) {
      HMENU menu = self._current_popup;
      int item = Win32Menu::find_hilite_menuitem(&menu);
      
      if(item < 0)
        Win32Menu::on_menuselect(0xFFFF0000U, (LPARAM)self._current_popup);
    }
    
    if(self._mouse_notifications) {
      ScreenToClient(self._mouse_notifications, &pt);
      SendMessageW(self._mouse_notifications, message, wParam, point_to_dword(pt));
    }
  }
  
  if(!is_over_menu || message == WM_LBUTTONUP)
    self._mouse_capture = nullptr;
  
  self._is_over_menu = is_over_menu;
  return handled;
}

void Win32AutoMenuHook::Impl::handle_popup(HMENU menu, DWORD subitems_cmd_id, DWORD cmd_id, POINT pt) {
  Expr subitems_cmd = Win32Menu::id_to_command(subitems_cmd_id);
  Expr cmd          = Win32Menu::id_to_command(cmd_id);
  
  auto old_mouse_notifications = self._mouse_notifications;
  self._mouse_notifications = nullptr;
  auto id = Win32MenuItemPopupMenu::show_popup_for(self._owner, pt, subitems_cmd, cmd);
  self._mouse_notifications = old_mouse_notifications;
  
  switch(id) {
    case SpecialCommandID::None: break;
    
    case SpecialCommandID::Select:
      self.exit_info.cmd    = cmd_id;
      self.exit_info.reason = MenuExitReason::ExplicitCmd;
      WIN32report(EndMenu());
      return;
    
    case SpecialCommandID::Remove: 
      if(Menus::remove_dynamic_submenu_item(PMATH_CPP_MOVE(subitems_cmd), PMATH_CPP_MOVE(cmd))) 
        Win32Menu::init_popupmenu(menu);
      break;
    
    case SpecialCommandID::GoToDefinition:
      self.exit_info.cmd      = cmd_id;
      self.exit_info.list_cmd = subitems_cmd_id;
      self.exit_info.reason   = MenuExitReason::LocateItemSource;
      WIN32report(EndMenu());
      return;
  }
  
  Win32Menu::on_menuselect(MAKEWPARAM(cmd_id, MF_MOUSESELECT), (LPARAM)menu);
}

bool Win32AutoMenuHook::Impl::handle_key_down(DWORD keycode) {
  switch(keycode) {
    case VK_DELETE: {
        Expr cmd;
        Expr subitems_cmd;
        HMENU menu = self._current_popup;
        int item = find_hilite_menuitem_cmd(&menu, &subitems_cmd, &cmd);
        
        if(menu && item >= 0) {
          if(Menus::remove_dynamic_submenu_item(PMATH_CPP_MOVE(subitems_cmd), PMATH_CPP_MOVE(cmd))) 
            Win32Menu::init_popupmenu(menu);
        }
      } break;
    
    case VK_F12: {
        DWORD cmd_id;
        DWORD subitems_cmd_id;
        HMENU menu = self._current_popup;
        int item = find_hilite_menuitem_cmd(&menu, &subitems_cmd_id, &cmd_id);
        if(menu && item >= 0) {
          Expr subitems_cmd = Win32Menu::id_to_command(subitems_cmd_id);
          if(Menus::has_submenu_item_locator(subitems_cmd)) {
            self.exit_info.list_cmd = subitems_cmd_id;
            self.exit_info.cmd      = cmd_id;
            self.exit_info.reason   = MenuExitReason::LocateItemSource;
            WIN32report(EndMenu());
            return true;
          }
        }
      } break;
    
    case VK_LEFT: if(self._allow_leave_left) {
        HMENU menu = self._current_popup;
        int item = Win32Menu::find_hilite_menuitem(&menu);
        
        if(menu == self._current_popup) {
          if(!Win32Menu::consumes_navigation_key(keycode, menu, item)) {
            self.exit_info.reason = MenuExitReason::LeftKey;
            WIN32report(EndMenu());
            return true;
          }
        }
      } break;
      
    case VK_RIGHT: if(self._allow_leave_right) {
        HMENU menu = self._current_popup;
        int item = Win32Menu::find_hilite_menuitem(&menu);
        
        if(item < 0 || (GetMenuState(menu, item, MF_BYPOSITION) & MF_POPUP) == 0) {
          if(!Win32Menu::consumes_navigation_key(keycode, menu, item)) {
            self.exit_info.reason = MenuExitReason::RightKey;
            WIN32report(EndMenu());
            return true;
          }
        }
      } break;
  
    case VK_APPS: {
      DWORD subitems_cmd_id;
      DWORD cmd_id;
      HMENU menu = self._current_popup;
      int item = find_hilite_menuitem_cmd(&menu, &subitems_cmd_id, &cmd_id);
      if(menu && item >= 0) {
        POINT pt = {0,0};
        RECT menu_item_rect {0,0,0,0};
        if(GetMenuItemRect(nullptr, menu, (unsigned)item, &menu_item_rect)) {
          HWND wnd = WindowFromPoint({menu_item_rect.left, menu_item_rect.top});
          int dpi = Win32HighDpi::get_dpi_for_window(wnd);
          
          int border = Win32HighDpi::get_system_metrics_for_dpi(SM_CXEDGE, dpi);
          menu_item_rect.left+=  border;
          menu_item_rect.right-= border;
          
          if(GetSystemMetrics(SM_MENUDROPALIGNMENT) == 0) {
            pt.x = menu_item_rect.left + (menu_item_rect.bottom - menu_item_rect.top)/2;
            pt.x+= Win32HighDpi::get_system_metrics_for_dpi(SM_CXMENUCHECK, dpi);
          }
          else {
            pt.x = menu_item_rect.right - (menu_item_rect.bottom - menu_item_rect.top)/2;
          }
          pt.y = menu_item_rect.top + (menu_item_rect.bottom - menu_item_rect.top)/2;
        }
        handle_popup(menu, subitems_cmd_id, cmd_id, pt);
        return true;
      }
    } break;
  }
  
  return false;
}

//} ... class Win32AutoMenuHook::Impl

//{ class Win32MenuItemPopupMenu ...

HMENU Win32MenuItemPopupMenu::create_popup_for(Expr list_cmd, Expr cmd) {
  if(list_cmd.is_null())
    return nullptr;
  
  HMENU menu = WIN32report(CreatePopupMenu());
  String select_label;
  if(cmd.item_equals(0, richmath_FrontEnd_DocumentOpen))
    select_label = "Open";
  else
    select_label = "Select";
  
  //select_label+= '\t';
  //select_label+= Win32AcceleratorTable::accel_text(FVIRTKEY, VK_RETURN);
  append(menu, SpecialCommandID::Select, PMATH_CPP_MOVE(select_label));
  
  if(Menus::has_submenu_item_locator(list_cmd)) {
    String gotodef_label;
    if(list_cmd == strings::MenuListSearchCommands)
      gotodef_label = String("Go to Definition");
    else if(cmd.item_equals(0, richmath_FrontEnd_DocumentOpen) || cmd.item_equals(0, richmath_FrontEnd_SetSelectedDocument))
      gotodef_label = String("Open containing folder");
    else
      gotodef_label = String("Go to Definition");
    
    UINT flags = 0;
    auto locator_status = Menus::test_locate_dynamic_submenu_item_source(list_cmd, cmd);
    if(!locator_status.enabled)
      flags |= MF_DISABLED | MF_GRAYED;
    
    gotodef_label+= '\t';
    gotodef_label+= Win32AcceleratorTable::accel_text(FVIRTKEY, VK_F12);
    append(menu, SpecialCommandID::GoToDefinition, PMATH_CPP_MOVE(gotodef_label), flags);
  }
  
  if(Menus::has_submenu_item_deleter(list_cmd)) {
    String remove_label;
    if(cmd.item_equals(0, richmath_FrontEnd_SetSelectedDocument))
      remove_label = strings::CloseMenu_label;
    else
      remove_label = String("Remove");
    
    remove_label+= '\t';
    remove_label+= Win32AcceleratorTable::accel_text(FVIRTKEY, VK_DELETE);
    append(menu, SpecialCommandID::Remove, PMATH_CPP_MOVE(remove_label));
  }

  append(menu, SpecialCommandID::None, String(), MF_SEPARATOR);
  append(menu, SpecialCommandID::None, String("Cancel"));
  
  //append(menu, SpecialCommandID::None, String("Help\t") + Win32AcceleratorTable::accel_text(FVIRTKEY, VK_F1), MF_DISABLED | MF_GRAYED);
  
  WIN32report(SetMenuDefaultItem(menu, (UINT)SpecialCommandID::Select, FALSE));
  return menu;
}

void Win32MenuItemPopupMenu::append(HMENU menu, SpecialCommandID id, String text, UINT flags) {
  text+= String::FromChar(0);
  if(const wchar_t *buf = text.buffer_wchar()) {
    WIN32report(AppendMenuW(menu, MF_STRING | flags, (UINT_PTR)id, buf));
  }
}

SpecialCommandID Win32MenuItemPopupMenu::show_popup_for(HWND owner, POINT pt, Expr list_cmd, Expr cmd) {
  HMENU popup = create_popup_for(PMATH_CPP_MOVE(list_cmd), PMATH_CPP_MOVE(cmd));
  if(!popup)
    return SpecialCommandID::None;
  
  UINT flags = TPM_RECURSE | TPM_RETURNCMD;
  if(GetSystemMetrics(SM_MENUDROPALIGNMENT) == 0)
    flags |= TPM_LEFTALIGN;
  else
    flags |= TPM_RIGHTALIGN;
  
  MenuExitInfo exit_info;
  DWORD id;
  {
    Win32AutoMenuHook menu_hook(popup, owner, nullptr, false, false);
    WIN32report(id = TrackPopupMenuEx(popup, flags, pt.x, pt.y, owner, nullptr));
    exit_info = menu_hook.exit_info;
  }
  
  WIN32report(DestroyMenu(popup));
  
  if(!id) {
    if(exit_info.handle_after_exit())
      return SpecialCommandID::None;
    
    if(exit_info.reason == MenuExitReason::ExplicitCmd)
      id = exit_info.cmd;
  }
  
  return (SpecialCommandID)id;
}

//} ... class Win32MenuItemPopupMenu

static int find_hilite_menuitem_cmd(HMENU *menu, DWORD *list_cmd, DWORD *cmd) {
  int item = Win32Menu::find_hilite_menuitem(menu);
  
  if(*menu && item >= 0) {
    MENUITEMINFOW info;
    memset(&info, 0, sizeof(info));
    info.cbSize = sizeof(info);
    info.fMask = MIIM_DATA | MIIM_ID;
    if(WIN32report(GetMenuItemInfoW(*menu, item, TRUE, &info))) {
      if(list_cmd) *list_cmd = (DWORD)info.dwItemData;
      if(cmd)      *cmd      = info.wID;
    }
  }
  
  return item;
}

static int find_hilite_menuitem_cmd(HMENU *menu, Expr *list_cmd, Expr *cmd) {
  DWORD list_cmd_id;
  DWORD cmd_id;
  
  int item = find_hilite_menuitem_cmd(menu, list_cmd ? &list_cmd_id : nullptr, cmd ? &cmd_id : nullptr);
  
  if(*menu && item >= 0) {
    if(list_cmd) *list_cmd = Win32Menu::id_to_command(list_cmd_id);
    if(cmd)      *cmd      = Win32Menu::id_to_command(cmd_id);
  }
  
  return item;
}
