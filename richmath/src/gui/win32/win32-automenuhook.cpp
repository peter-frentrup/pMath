#include <gui/win32/win32-automenuhook.h>
#include <gui/win32/win32-menu.h>

#include <eval/application.h>


using namespace pmath;
using namespace richmath;

namespace richmath {
  class Win32AutoMenuHookImpl {
    public:
      static void push(Win32AutoMenuHook *handler);
      static void pop(Win32AutoMenuHook *handler);
      
      static LRESULT CALLBACK menu_hook_proc(int code, WPARAM wParam, LPARAM lParam);
    
    private:
      static HHOOK the_hook;
      static Win32AutoMenuHook *current;
  };
}

HHOOK              Win32AutoMenuHookImpl::the_hook = nullptr;
Win32AutoMenuHook *Win32AutoMenuHookImpl::current = nullptr;


static int find_hilite_menuitem(HMENU *menu);

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

Win32AutoMenuHook::Win32AutoMenuHook(HMENU tracked_popup, HWND mouse_notifications, bool allow_leave_left, bool allow_leave_right) 
  : _next(nullptr),
    _current_popup(tracked_popup),
    _mouse_notifications(mouse_notifications),
    _allow_leave_left(allow_leave_left),
    _allow_leave_right(allow_leave_right),
    exit_reason(MenuExitReason::Other)
{
  Win32AutoMenuHookImpl::push(this);
}

Win32AutoMenuHook::~Win32AutoMenuHook() {
  Win32AutoMenuHookImpl::pop(this);
}

bool Win32AutoMenuHook::handle(MSG *msg) {
  switch(msg->message) {
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_MOUSEMOVE: if(_mouse_notifications) {
        POINT pt = dword_to_point(msg->lParam);
        
        ScreenToClient(_mouse_notifications, &pt);
        
        SendMessageW(_mouse_notifications, msg->message, msg->wParam, point_to_dword(pt));
        //return false;
      } break;
      
      case WM_KEYDOWN: {
          switch(msg->wParam) {
            case VK_DELETE: {
                HMENU menu = _current_popup;
                int item = find_hilite_menuitem(&menu);
                
                if(menu && item >= 0) {
                  MENUITEMINFOW info;
                  memset(&info, 0, sizeof(info));
                  info.cbSize = sizeof(info);
                  info.fMask = MIIM_DATA | MIIM_ID;
                  if(GetMenuItemInfoW(menu, item, TRUE, &info)) {
                    if(info.dwItemData != 0) {
                      Expr subitems_cmd = Win32Menu::id_to_command((DWORD)info.dwItemData);
                      Expr cmd = Win32Menu::id_to_command(info.wID);
                      if(Application::remove_dynamic_submenu_item(subitems_cmd, cmd)) {
                        Win32Menu::init_popupmenu(menu);
                      }
                    }
                  }
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

//{ class Win32AutoMenuHookImpl ...

void Win32AutoMenuHookImpl::push(Win32AutoMenuHook *handler) {
  assert(handler != nullptr);
  assert(handler->_next == nullptr);
  
  if(!current) {
    if(!the_hook)
      the_hook = SetWindowsHookEx(WH_MSGFILTER, menu_hook_proc, nullptr, GetCurrentThreadId());
  }
  
  handler->_next = current;
  current = handler;
}

void Win32AutoMenuHookImpl::pop(Win32AutoMenuHook *handler) {
  assert(handler == current);
  
  current = current->_next;
  if(!current) {
    if(the_hook)
      UnhookWindowsHookEx(the_hook);
    
    the_hook = nullptr;
  }
}

LRESULT CALLBACK Win32AutoMenuHookImpl::menu_hook_proc(int code, WPARAM h_wParam, LPARAM h_lParam) {
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

//} ... class Win32AutoMenuHookImpl

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
