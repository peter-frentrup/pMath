#ifndef __GUI__WIN32__WIN32_MENU_H__
#define __GUI__WIN32__WIN32_MENU_H__

#ifndef RICHMATH_USE_WIN32_GUI
#  error this header is win32 specific
#endif

#include <pmath-cpp.h>
#include <util/base.h>
#include <util/sharedptr.h>

#include <windows.h>


namespace richmath {
  using namespace pmath; // bad style!!!
  
  class Win32Menu: public Shareable {
    public:
      explicit Win32Menu(Expr expr, bool is_popup);
      virtual ~Win32Menu();
      
      static Expr   id_to_command(DWORD  id);
      static DWORD  command_to_id(Expr cmd);
      
      HMENU hmenu() { return _hmenu; }
      
    public:
      static SharedPtr<Win32Menu>  main_menu;
      static SharedPtr<Win32Menu>  popup_menu;
      
    private:
      HMENU _hmenu;
  };
  
  class Win32AcceleratorTable: public Shareable {
    public:
      explicit Win32AcceleratorTable(Expr expr);
      virtual ~Win32AcceleratorTable();
      
      HACCEL haccel() { return _haccel; }
      
    public:
      static SharedPtr<Win32AcceleratorTable>  main_table;
      
    private:
      HACCEL _haccel;
  };
}


#endif // __GUI__WIN32__WIN32_MENU_H__
