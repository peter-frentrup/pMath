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

  class Menus {
    public:
      static MenuItemType menu_item_type(Expr item);
      static MenuItemType command_type(Expr cmd);
  };
}


#endif // RICHMATH__GUI__MENUS_H__INCLUDED
