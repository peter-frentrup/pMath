#ifndef RICHMATH__GUI__WIN32__MENUS__WIN32_MENU_H__INCLUDED
#define RICHMATH__GUI__WIN32__MENUS__WIN32_MENU_H__INCLUDED

#ifndef RICHMATH_USE_WIN32_GUI
#  error this header is win32 specific
#endif

#include <util/pmath-extra.h>
#include <util/base.h>
#include <util/sharedptr.h>

#include <gui/win32/ole/combase.h>

#include <shlobj.h>


namespace richmath {
  struct MenuSearchResult;
  
  struct Win32MenuSelector {
    virtual void init_popupmenu(HWND menu_wnd, HMENU menu) = 0;
    virtual void on_menuselect(HMENU menu, unsigned item_or_index, unsigned flags) {}
  };
  
  class Win32Menu: public Shareable {
    public:
      explicit Win32Menu(Expr expr, bool is_popup);
      virtual ~Win32Menu();
      
      static Expr   id_to_command(DWORD  id);
      static DWORD  command_to_id(Expr cmd);
      
      HMENU hmenu() { return _hmenu; }
      
      static void init_popupmenu(HMENU sub);
      static Expr selected_item_command();
      
      static int find_hilite_menuitem(HMENU *menu, bool recursive = true);
      
      static void on_menuselect(WPARAM wParam, LPARAM lParam);
      static LRESULT on_menudrag(WPARAM wParam, LPARAM lParam);
      static LRESULT on_menugetobject(WPARAM wParam, LPARAM lParam);
      
      static bool handle_child_window_mouse_message(HWND hwnd_menu, HWND hwnd_child, UINT msg, WPARAM wParam, const POINT &screen_pt);
      
    public:
      static SharedPtr<Win32Menu>  main_menu;
      static bool                  use_dark_mode;
      static Win32MenuSelector    *menu_selector;
      
    private:
      HMENU _hmenu;
  };
  
  class Win32AcceleratorTable: public Shareable {
    public:
      explicit Win32AcceleratorTable(Expr expr);
      virtual ~Win32AcceleratorTable();
      
      HACCEL haccel() { return _haccel; }
      static String accel_text(BYTE fVirt, WORD key);
      
      bool translate_accelerator(HWND hwnd, MSG *msg);
      
    public:
      static SharedPtr<Win32AcceleratorTable>  main_table;
      
    private:
      HACCEL _haccel;
  };
}


#endif // RICHMATH__GUI__WIN32__MENUS__WIN32_MENU_H__INCLUDED
