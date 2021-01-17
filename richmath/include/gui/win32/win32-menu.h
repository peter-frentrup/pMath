#ifndef RICHMATH__GUI__WIN32__WIN32_MENU_H__INCLUDED
#define RICHMATH__GUI__WIN32__WIN32_MENU_H__INCLUDED

#ifndef RICHMATH_USE_WIN32_GUI
#  error this header is win32 specific
#endif

#include <pmath-cpp.h>
#include <util/base.h>
#include <util/sharedptr.h>

#include <gui/win32/ole/combase.h>

#include <shlobj.h>


namespace richmath {
  using namespace pmath; // bad style!!!
  
  class Win32Menu: public Shareable {
    public:
      explicit Win32Menu(Expr expr, bool is_popup);
      virtual ~Win32Menu();
      
      static Expr   id_to_command(DWORD  id);
      static DWORD  command_to_id(Expr cmd);
      
      HMENU hmenu() { return _hmenu; }
      
      static void init_popupmenu(HMENU sub);
      static Expr selected_item_command();
      
      static void on_menuselect(WPARAM wParam, LPARAM lParam);
      static LRESULT on_menudrag(WPARAM wParam, LPARAM lParam);
      static LRESULT on_menugetobject(WPARAM wParam, LPARAM lParam);
      
    public:
      static SharedPtr<Win32Menu>  main_menu;
      static bool                  use_dark_mode;
      
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


#endif // RICHMATH__GUI__WIN32__WIN32_MENU_H__INCLUDED
