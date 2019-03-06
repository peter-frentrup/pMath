#ifndef RICHMATH__GUI__WIN32__WIN32_MENUBAR_H__INCLUDED
#define RICHMATH__GUI__WIN32__WIN32_MENUBAR_H__INCLUDED

#ifndef RICHMATH_USE_WIN32_GUI
#  error this header is win32 specific
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
    protected:
      ~Win32Menubar();
      
    public:
      Win32Menubar(Win32DocumentWindow *window, HWND parent, SharedPtr<Win32Menu> menu);
      void destroy() { delete this; }
      
      HWND                 hwnd() { return _hwnd; }
      SharedPtr<Win32Menu> menu() { return _menu; }
      
      bool visible();
      int best_height();
      
      MenuAppearence appearence() { return _appearence; }
      void appearence(MenuAppearence value);
      bool is_pinned();
      
      void theme_changed();
      void resized();
      bool callback(LRESULT *result, UINT message, WPARAM wParam, LPARAM lParam);
      
    private:
      void on_dpi_changed(int new_dpi);
      void reload_image_list();
      
      void show_menu(int item);
      void show_sysmenu();
      
      void kill_focus();
      void set_focus(int item);
      
      HHOOK register_hook(int item);
      void unregister_hook(HHOOK hook);
      
      static int find_hilite_menuitem(HMENU *menu);
      
    private:
      MenuAppearence _appearence;
      
    private:
      Win32DocumentWindow *_window;
      HWND                 _hwnd;
      SharedPtr<Win32Menu> _menu;
      HFONT                _font;
      
      HIMAGELIST image_list;
      
      HMENU current_popup;
      int current_item;
      int next_item;
      int hot_item;
      
      int separator_index;
      int pin_index;
      
      int dpi;
      
      bool focused : 1;
      bool menu_animation : 1;
      
      static LRESULT CALLBACK menu_hook_proc(int code, WPARAM wParam, LPARAM lParam);
  };
}

#endif // RICHMATH__GUI__WIN32__WIN32_MENUBAR_H__INCLUDED
