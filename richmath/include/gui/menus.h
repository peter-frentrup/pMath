#ifndef RICHMATH__GUI__MENUS_H__INCLUDED
#define RICHMATH__GUI__MENUS_H__INCLUDED


#include <util/pmath-extra.h>

namespace richmath {
  enum class MenuItemType {
    Invalid,
    Normal,
    CheckButton,
    RadioButton,
    Delimiter,
    SubMenu,
    InlineMenu
  };

  class MenuCommandStatus {
    public:
      MenuCommandStatus(bool _enabled)
        : enabled(_enabled),
          checked(false)
      {
      }
      
    public:
      bool enabled;
      bool checked;
  };
  
  enum class MenuCommandScope {
    Selection,
    Document
  };
  
  class Menus {
    public:
      static void init();
      static void done();
      
      static MenuCommandScope current_scope;
      static void run_command_async(Expr cmd); // callable from non-GUI thread
      
      static bool run_command_now(Expr cmd);
      
      static Expr selected_item_command();
      static MenuCommandStatus test_command_status(Expr cmd);
      static Expr generate_dynamic_submenu(Expr cmd);
      static bool remove_dynamic_submenu_item(Expr submenu_cmd, Expr item_cmd);
      static bool locate_dynamic_submenu_item_source(Expr submenu_cmd, Expr item_cmd);
      
      static void register_command(
        Expr cmd,
        bool              (*func)(Expr cmd),
        MenuCommandStatus (*test)(Expr cmd) = nullptr);
      
      static void register_dynamic_submenu(Expr cmd, Expr (*func)(Expr cmd));
      
      static void register_submenu_item_deleter(Expr submenu_cmd, bool (*func)(Expr submenu_cmd, Expr item_cmd));
      static bool has_submenu_item_deleter(Expr submenu_cmd);
      
      static void register_submenu_item_locator(Expr submenu_cmd, bool (*func)(Expr submenu_cmd, Expr item_cmd));
      static bool has_submenu_item_locator(Expr submenu_cmd);
      
      static MenuItemType menu_item_type(Expr item);
      static MenuItemType command_type(Expr cmd);
  };
}


#endif // RICHMATH__GUI__MENUS_H__INCLUDED
