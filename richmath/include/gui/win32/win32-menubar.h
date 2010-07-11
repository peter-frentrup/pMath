#ifndef __GUI__WIN32__WIN32_MENUBAR_H__
#define __GUI__WIN32__WIN32_MENUBAR_H__

#include <windows.h>

#include <util/base.h>

namespace richmath{
  class Win32DocumentWindow;
  
  typedef enum{
    MaAllwaysShow,
    MaAutoShow,
    MaNeverShow
  }MenuAppearence;
  
  class Win32Menubar: public Base {
    public:
      Win32Menubar(Win32DocumentWindow *window, HWND parent, HMENU menu);
      ~Win32Menubar();
      
      HWND  hwnd(){ return _hwnd; }
      HMENU menu(){ return _menu; }
      
      bool visible();
      int height();
      
      MenuAppearence appearence(){ return _appearence; }
      void appearence(MenuAppearence value);
      
      bool callback(LRESULT *result, UINT message, WPARAM wParam, LPARAM lParam);
      
    protected:
      void show_menu(int item);
      void show_sysmenu();
      
      void kill_focus();
      void set_focus(int item);
      
      HHOOK register_hook(int item);
      void unregister_hook(HHOOK hook);
      
      static int find_hilite_menuitem(HMENU *menu);
      
    protected:
      MenuAppearence _appearence;
      
    protected:
      Win32DocumentWindow *_window;
      HWND  _hwnd;
      HMENU _menu;
      
      bool focused;
      
      HMENU current_popup;
      int current_item;
      int next_item;
      
      static LRESULT CALLBACK menu_hook_proc(int code, WPARAM wParam, LPARAM lParam);
  };
}

#endif // __GUI__WIN32__WIN32_MENUBAR_H__
