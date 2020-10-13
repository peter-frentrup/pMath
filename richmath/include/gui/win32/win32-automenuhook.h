#ifndef RICHMATH__GUI__WIN32__WIN32_MENUHOOK_H__INCLUDED
#define RICHMATH__GUI__WIN32__WIN32_MENUHOOK_H__INCLUDED

#ifndef RICHMATH_USE_WIN32_GUI
#  error this header is win32 specific
#endif

#include <windows.h>


namespace richmath {
  enum class MenuExitReason : char {
    Other,
    LeftKey,
    RightKey,
    ExplicitCmd,
  };
  
  class Win32AutoMenuHook {
      class Impl;
    public:
      Win32AutoMenuHook(HMENU tracked_popup, HWND owner, HWND mouse_notifications, bool allow_leave_left, bool allow_leave_right);
      ~Win32AutoMenuHook();
      
      Win32AutoMenuHook(const Win32AutoMenuHook &) = delete;
      Win32AutoMenuHook &operator=(const Win32AutoMenuHook &) = delete;
    
    private:
      bool handle(MSG &msg);
    
    private:
      Win32AutoMenuHook *_next;
      HMENU _current_popup;
      HWND _owner;
      HWND _mouse_notifications;
      bool _allow_leave_left;
      bool _allow_leave_right;
      bool _is_over_menu;
      
    public:
      MenuExitReason exit_reason; 
      DWORD          exit_cmd;
  };
}

#endif // RICHMATH__GUI__WIN32__WIN32_MENUHOOK_H__INCLUDED
