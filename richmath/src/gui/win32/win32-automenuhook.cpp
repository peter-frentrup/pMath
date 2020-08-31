#include <gui/win32/win32-automenuhook.h>
#include <gui/win32/win32-menu.h>

#include <eval/application.h>


using namespace pmath;
using namespace richmath;

namespace richmath {
  class Win32AutoMenuHook::Impl {
    public:
      static void push(Win32AutoMenuHook *handler);
      static void pop(Win32AutoMenuHook *handler);
      
      static LRESULT CALLBACK menu_hook_proc(int code, WPARAM wParam, LPARAM lParam);
    
    private:
      static HHOOK the_hook;
      static Win32AutoMenuHook *current;
  };
  
  class Win32MenuItemPopupMenu {
    public:
      enum class CommandId : DWORD {
        None = 0,
        Select,
        Remove
      };
      
      static HMENU create_popup_for(Expr list_cmd, Expr cmd);
      static void append(HMENU menu, CommandId id, String text, UINT flags = MF_ENABLED);
      static CommandId show_popup_for(HWND owner, POINT pt, Expr list_cmd, Expr cmd);
  };
}

HHOOK              Win32AutoMenuHook::Impl::the_hook = nullptr;
Win32AutoMenuHook *Win32AutoMenuHook::Impl::current = nullptr;

extern pmath_symbol_t richmath_FrontEnd_DocumentOpen;
extern pmath_symbol_t richmath_FrontEnd_SetSelectedDocument;

static const char MenuWindowClass[] = "#32768";

static int find_hilite_menuitem(HMENU *menu);
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

Win32AutoMenuHook::Win32AutoMenuHook(HMENU tracked_popup, HWND owner, HWND mouse_notifications, bool allow_leave_left, bool allow_leave_right) 
  : _next(nullptr),
    _current_popup(tracked_popup),
    _owner(owner),
    _mouse_notifications(mouse_notifications),
    _allow_leave_left(allow_leave_left),
    _allow_leave_right(allow_leave_right),
    _is_over_menu(false),
    exit_reason(MenuExitReason::Other),
    exit_cmd(0)
{
  Impl::push(this);
}

Win32AutoMenuHook::~Win32AutoMenuHook() {
  Impl::pop(this);
}

bool Win32AutoMenuHook::handle(MSG *msg) {
  {
    const int len = 20;
    char clsname[len];
    GetClassNameA(msg->hwnd, clsname, len);
    clsname[len-1] = '\0';
    if(0 == strcmp(clsname, MenuWindowClass))
      pmath_debug_print("[Win32AutoMenuHook msg 0x%x for menu window %p]\n", msg->message, msg->hwnd);
  }
  
  switch(msg->message) {
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_MOUSEMOVE: {
        POINT pt = dword_to_point(msg->lParam);
        // Strangely, lParam is in screen coordinates during the message hook.
        
        const int len = 20;
        char hover_name[len];
        HWND hover_wnd = WindowFromPoint(pt);
        GetClassNameA(hover_wnd, hover_name, len);
        hover_name[len-1] = '\0';
        
        bool is_over_menu = 0 == strcmp(hover_name, MenuWindowClass);
        if(!is_over_menu) {
          if(_is_over_menu) {
            HMENU menu = _current_popup;
            int item = find_hilite_menuitem(&menu);
            
            if(item < 0)
              Win32Menu::on_menuselect(0xFFFF0000U, (LPARAM)_current_popup);
          }
          if(_mouse_notifications) {
            ScreenToClient(_mouse_notifications, &pt);
            SendMessageW(_mouse_notifications, msg->message, msg->wParam, point_to_dword(pt));
          }
        }
        
        _is_over_menu = is_over_menu;
        //return false;
      } break;
    
    // WM_MENURBUTTONUP is not sent through the hook, so we handle WM_RBUTTONUP instead
    case WM_RBUTTONUP: {
        DWORD subitems_cmd_id;
        DWORD cmd_id;
        HMENU menu = _current_popup;
        int item = find_hilite_menuitem_cmd(&menu, &subitems_cmd_id, &cmd_id);
        
        if(menu && item >= 0) {
          Expr subitems_cmd = Win32Menu::id_to_command(subitems_cmd_id);
          Expr cmd          = Win32Menu::id_to_command(cmd_id);
          auto id = Win32MenuItemPopupMenu::show_popup_for(
                      _owner, 
                      dword_to_point(msg->lParam), 
                      subitems_cmd, 
                      cmd);
          switch(id) {
            case Win32MenuItemPopupMenu::CommandId::None: break;
            
            case Win32MenuItemPopupMenu::CommandId::Select:
              exit_cmd = cmd_id;
              exit_reason = MenuExitReason::ExplicitCmd;
              EndMenu();
              return true;
            
            case Win32MenuItemPopupMenu::CommandId::Remove: 
              if(Application::remove_dynamic_submenu_item(subitems_cmd, cmd)) 
                Win32Menu::init_popupmenu(menu);
              break;
          }
          
          Win32Menu::on_menuselect(MAKEWPARAM(cmd_id, MF_MOUSESELECT), (LPARAM)menu);
          
          return true;
        }
      } break;
    
    
    case WM_KEYDOWN: {
          switch(msg->wParam) {
            case VK_DELETE: {
                Expr cmd;
                Expr subitems_cmd;
                HMENU menu = _current_popup;
                int item = find_hilite_menuitem_cmd(&menu, &subitems_cmd, &cmd);
                
                if(menu && item >= 0) {
                  if(Application::remove_dynamic_submenu_item(subitems_cmd, cmd)) 
                    Win32Menu::init_popupmenu(menu);
                }
              } break;
            
            case VK_LEFT: if(_allow_leave_left) {
                HMENU menu = _current_popup;
                int item = find_hilite_menuitem(&menu);
                
                if(menu == _current_popup) {
                  exit_reason = MenuExitReason::LeftKey;
                  EndMenu();
                  return true;
                }
              } break;
              
            case VK_RIGHT: if(_allow_leave_right) {
                HMENU menu = _current_popup;
                int item = find_hilite_menuitem(&menu);
                
                if(item < 0 || (GetMenuState(menu, item, MF_BYPOSITION) & MF_POPUP) == 0) {
                  exit_reason = MenuExitReason::RightKey;
                  EndMenu();
                  return true;
                }
              } break;
          }
        } break;
  }
  
  return false;
}

//} ... class Win32AutoMenuHook

//{ class Win32AutoMenuHook::Impl ...

void Win32AutoMenuHook::Impl::push(Win32AutoMenuHook *handler) {
  assert(handler != nullptr);
  assert(handler->_next == nullptr);
  
  if(!current) {
    if(!the_hook)
      the_hook = SetWindowsHookEx(WH_MSGFILTER, menu_hook_proc, nullptr, GetCurrentThreadId());
  }
  
  handler->_next = current;
  current = handler;
}

void Win32AutoMenuHook::Impl::pop(Win32AutoMenuHook *handler) {
  assert(handler == current);
  
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
      if(handlers->handle(msg))
        return TRUE;
    }
  }
  
  return CallNextHookEx(0, code, h_wParam, h_lParam);
}

//} ... class Win32AutoMenuHook::Impl

//{ class Win32MenuItemPopupMenu ...

HMENU Win32MenuItemPopupMenu::create_popup_for(Expr list_cmd, Expr cmd) {
  if(list_cmd.is_null())
    return nullptr;
  
  HMENU menu = CreatePopupMenu();
  String select_label;
  if(cmd[0] == richmath_FrontEnd_DocumentOpen)
    select_label = "Open";
  else
    select_label = "Select";
  
  //select_label+= '\t';
  //select_label+= Win32AcceleratorTable::accel_text(FVIRTKEY, VK_RETURN);
  append(menu, CommandId::Select, std::move(select_label));
  
  //append(menu, CommandId::None, String("Go to Definition\t") + Win32AcceleratorTable::accel_text(FVIRTKEY, VK_F12), MF_DISABLED | MF_GRAYED);
  
  if(Application::has_submenu_item_deleter(list_cmd)) {
    String remove_label;
    if(cmd[0] == richmath_FrontEnd_SetSelectedDocument)
      remove_label = String("Close");
    else
      remove_label = String("Remove");
    
    remove_label+= '\t';
    remove_label+= Win32AcceleratorTable::accel_text(FVIRTKEY, VK_DELETE);
    append(menu, CommandId::Remove, std::move(remove_label));
  }
  
  //append(menu, CommandId::None, String("Help\t") + Win32AcceleratorTable::accel_text(FVIRTKEY, VK_F1), MF_DISABLED | MF_GRAYED);
  
  SetMenuDefaultItem(menu, (UINT)CommandId::Select, FALSE);
  return menu;
}

void Win32MenuItemPopupMenu::append(HMENU menu, CommandId id, String text, UINT flags) {
  text+= String::FromChar(0);
  if(const uint16_t *buf = text.buffer()) {
    AppendMenuW(menu, MF_STRING | flags, (UINT_PTR)id, (const wchar_t*)buf);
  }
}

Win32MenuItemPopupMenu::CommandId Win32MenuItemPopupMenu::show_popup_for(HWND owner, POINT pt, Expr list_cmd, Expr cmd) {
  HMENU popup = create_popup_for(std::move(list_cmd), std::move(cmd));
  if(!popup)
    return CommandId::None;
  
  UINT flags = TPM_RECURSE | TPM_RETURNCMD;
  if(GetSystemMetrics(SM_MENUDROPALIGNMENT) == 0)
    flags |= TPM_LEFTALIGN;
  else
    flags |= TPM_RIGHTALIGN;
  
  DWORD id;
  {
    Win32AutoMenuHook menu_hook(popup, owner, nullptr, false, false);
    id = TrackPopupMenuEx(popup, flags, pt.x, pt.y, owner, nullptr);
    if(!id && menu_hook.exit_reason == MenuExitReason::ExplicitCmd)
      id = menu_hook.exit_cmd;
  }
  
  DestroyMenu(popup);
  return (CommandId)id;
}

//} ... class Win32MenuItemPopupMenu

static int find_hilite_menuitem(HMENU *menu) {
  for(int i = 0; i < GetMenuItemCount(*menu); ++i) {
    UINT state = GetMenuState(*menu, i, MF_BYPOSITION);
    
    if(state & MF_HILITE) {
      if(state & MF_POPUP) {
        HMENU old = *menu;
        *menu = GetSubMenu(*menu, i);
        int result = find_hilite_menuitem(menu);
        if(result >= 0)
          return result;
          
        *menu = old;
      }
    
      return i;
    }
  }
  
  return -1;
}

static int find_hilite_menuitem_cmd(HMENU *menu, DWORD *list_cmd, DWORD *cmd) {
  int item = find_hilite_menuitem(menu);
  
  if(*menu && item >= 0) {
    MENUITEMINFOW info;
    memset(&info, 0, sizeof(info));
    info.cbSize = sizeof(info);
    info.fMask = MIIM_DATA | MIIM_ID;
    if(GetMenuItemInfoW(*menu, item, TRUE, &info)) {
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
