#include <gui/menus.h>

#include <eval/application.h>


using namespace richmath;


extern pmath_symbol_t richmath_FE_Delimiter;
extern pmath_symbol_t richmath_FE_Menu;
extern pmath_symbol_t richmath_FE_MenuItem;
extern pmath_symbol_t richmath_FE_ScopedCommand;
extern pmath_symbol_t richmath_FrontEnd_SetSelectedDocument;


static Hashtable<Expr, bool              ( *)(Expr)>       menu_commands;
static Hashtable<Expr, MenuCommandStatus ( *)(Expr)>       menu_command_testers;
static Hashtable<Expr, Expr              ( *)(Expr)>       dynamic_menu_lists;
static Hashtable<Expr, bool              ( *)(Expr, Expr)> dynamic_menu_list_item_deleter;

//{ class Menus ...

MenuCommandScope Menus::current_scope = MenuCommandScope::Selection;

void Menus::init() {
}

void Menus::done() {
  menu_commands.clear();
  menu_command_testers.clear();
  dynamic_menu_lists.clear();
  dynamic_menu_list_item_deleter.clear();
}

void Menus::run_command(Expr cmd) {
  Application::notify(ClientNotification::MenuCommand, cmd);
}

bool Menus::run_recursive_command(Expr cmd) {
  bool (*func)(Expr);
  
  func = menu_commands[cmd];
  if(func && func(cmd))
    return true;
    
  func = menu_commands[cmd[0]];
  if(func && func(std::move(cmd)))
    return true;
    
  return false;
}

MenuCommandStatus Menus::test_command_status(Expr cmd) {
  MenuCommandStatus (*func)(Expr);
  
  if(cmd.is_null())
    return MenuCommandStatus(true);
  
  func = menu_command_testers[cmd];
  if(func)
    return func(std::move(cmd));
    
  func = menu_command_testers[cmd[0]];
  if(func)
    return func(std::move(cmd));
    
  return MenuCommandStatus(true);
}

Expr Menus::generate_dynamic_submenu(Expr cmd) {
  Expr (*func)(Expr);
  
  if(cmd.is_null())
    return Expr();
  
  func = dynamic_menu_lists[cmd];
  if(func)
    return func(std::move(cmd));
    
  return Expr();
}

bool Menus::remove_dynamic_submenu_item(Expr submenu_cmd, Expr item_cmd) {
  bool (*func)(Expr, Expr);
  
  if(submenu_cmd.is_null())
    return false;
  
  func = dynamic_menu_list_item_deleter[submenu_cmd];
  if(func)
    return func(std::move(submenu_cmd), std::move(item_cmd));
    
  return false;
}

void Menus::register_command(
  Expr cmd,
  bool              (*func)(Expr cmd),
  MenuCommandStatus (*test)(Expr cmd)
) {
  if(cmd.is_null()) {
    menu_commands.default_value        = func;
    menu_command_testers.default_value = test;
    return;
  }
  
  if(func)
    menu_commands.set(cmd, func);
  else
    menu_commands.remove(cmd);
    
  if(test)
    menu_command_testers.set(cmd, test);
  else
    menu_command_testers.remove(cmd);
}

void Menus::register_dynamic_submenu(Expr cmd, Expr (*func)(Expr cmd)) {
  if(cmd.is_null()) {
    dynamic_menu_lists.default_value = func;
    return;
  }
  
  if(func)
    dynamic_menu_lists.set(std::move(cmd), func);
  else
    dynamic_menu_lists.remove(std::move(cmd));
}

void Menus::register_submenu_item_deleter(Expr submenu_cmd, bool (*func)(Expr submenu_cmd, Expr item_cmd)) {
  if(func)
    dynamic_menu_list_item_deleter.set(std::move(submenu_cmd), func);
  else
    dynamic_menu_list_item_deleter.remove(std::move(submenu_cmd));
}

bool Menus::has_submenu_item_deleter(Expr submenu_cmd) {
  return dynamic_menu_list_item_deleter[std::move(submenu_cmd)] != nullptr;
}

MenuItemType Menus::menu_item_type(Expr item) {
  if(item == richmath_FE_Delimiter) 
    return MenuItemType::Delimiter;
  
  if(item[0] == richmath_FE_MenuItem && item.expr_length() == 2) {
    if(!item[1].is_string())
      return MenuItemType::Invalid;
    
    return command_type(item[2]);
  }
  
  if(item[0] == richmath_FE_Menu) {
    if(!item[1].is_string())
      return MenuItemType::Invalid;
    
    Expr submenu = item[2];
    if(submenu.is_string()) 
      return MenuItemType::InlineMenu;
    
    if(submenu[0] == PMATH_SYMBOL_LIST) 
      return MenuItemType::SubMenu;
  }
  
  return MenuItemType::Invalid;
}

MenuItemType Menus::command_type(Expr cmd) {
  if(cmd[0] == richmath_FE_ScopedCommand)
    cmd = cmd[1];
  
  if(String str = cmd) {
    if(str.starts_with("SelectStyle")) // SelectStyle1 ... SelectStyle9
      return MenuItemType::RadioButton;
    
    if(str.equals("ShowHideMenu"))
      return MenuItemType::CheckButton;
  }
  
  if(cmd.is_rule()) // style->value  is a simple setter (does not toggle)
    return MenuItemType::RadioButton;
  
  if(cmd[0] == richmath_FrontEnd_SetSelectedDocument)
    return MenuItemType::RadioButton;
  
  return MenuItemType::Normal;
}

//} ... class Menus
