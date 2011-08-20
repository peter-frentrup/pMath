#ifndef __GUI__WIN32__WIN32_MENUBAR_H__
#define __GUI__WIN32__WIN32_MENUBAR_H__

#ifndef RICHMATH_USE_WIN32_GUI
#error this header is win32 specific
#endif

#include <windows.h>
#include <commctrl.h>

#include <util/base.h>
#include <util/sharedptr.h>


namespace richmath {
  class Win32DocumentWindow;
  class Win32Menu;
  
  typedef enum {
    MaAllwaysShow,
    MaAutoShow,
    MaNeverShow
  } MenuAppearence;
  
  class Win32Menubar: public Base {
    public:
      Win32Menubar(Win32DocumentWindow *window, HWND parent, SharedPtr<Win32Menu> menu);
      ~Win32Menubar();
      
      HWND                 hwnd() { return _hwnd; }
      SharedPtr<Win32Menu> menu() { return _menu; }
      
      bool visible();
      int height();
      
      MenuAppearence appearence() { return _appearence; }
      void appearence(MenuAppearence value);
      bool is_pinned();
      
      void resized();
      bool callback(LRESULT *result, UINT message, WPARAM wParam, LPARAM lParam);
      
    protected:
      void init_image_list();
      
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
      HWND                 _hwnd;
      SharedPtr<Win32Menu> _menu;
      
      HIMAGELIST image_list;
      
      bool focused;
      bool menu_animation;
      
      HMENU current_popup;
      int current_item;
      int next_item;
      
      int separator_index;
      int pin_index;
      
      static LRESULT CALLBACK menu_hook_proc(int code, WPARAM wParam, LPARAM lParam);
  };
}

#endif // __GUI__WIN32__WIN32_MENUBAR_H__
