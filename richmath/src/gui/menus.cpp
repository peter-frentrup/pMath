#include <gui/menus.h>


using namespace richmath;


extern pmath_symbol_t richmath_FE_Delimiter;
extern pmath_symbol_t richmath_FE_Menu;
extern pmath_symbol_t richmath_FE_MenuItem;
extern pmath_symbol_t richmath_FE_ScopedCommand;
extern pmath_symbol_t richmath_FrontEnd_SetSelectedDocument;

//{ class Menus ...

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
