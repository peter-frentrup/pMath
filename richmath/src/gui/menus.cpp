#include <gui/menus.h>

#include <eval/application.h>

#ifdef RICHMATH_USE_WIN32_GUI
#  include <gui/win32/menus/win32-menu.h>
#  include <gui/win32/menus/win32-menu-search-overlay.h>
#endif

#ifdef RICHMATH_USE_GTK_GUI
#  include <gui/gtk/mgtk-menu-builder.h>
#  include <gui/gtk/mgtk-document-window.h>
#endif

#include <gui/documents.h>

#include <algorithm>


using namespace richmath;

namespace richmath { namespace strings {
  extern String SearchMenuNoMatch_label;
  extern String MenuListSearchCommands;
  extern String SearchMenuItems;
  extern String ShowHideMenu;
}}

extern pmath_symbol_t richmath_System_DollarFailed;
extern pmath_symbol_t richmath_System_Delimiter;
extern pmath_symbol_t richmath_System_List;
extern pmath_symbol_t richmath_System_Menu;
extern pmath_symbol_t richmath_System_MenuItem;

extern pmath_symbol_t richmath_Documentation_FindDocumentationPages;
extern pmath_symbol_t richmath_FE_ScopedCommand;
extern pmath_symbol_t richmath_FrontEnd_DocumentOpen;
extern pmath_symbol_t richmath_FrontEnd_SetSelectedDocument;

static Hashtable<Expr, bool              ( *)(Expr)>       menu_commands;
static Hashtable<Expr, MenuCommandStatus ( *)(Expr)>       menu_command_testers;
static Hashtable<Expr, Expr              ( *)(Expr)>       dynamic_menu_lists;
static Hashtable<Expr, bool              ( *)(Expr, Expr)> dynamic_menu_list_item_deleters;
static Hashtable<Expr, bool              ( *)(Expr, Expr)> dynamic_menu_list_item_locators;
static Hashtable<Expr, MenuCommandStatus ( *)(Expr, Expr)> dynamic_menu_list_item_locator_testers;

static String current_query;
static Expr get_search_commands(Expr name);

//{ class Menus ...

MenuCommandScope Menus::current_scope = MenuCommandScope::Selection;

void Menus::init() {
  register_dynamic_submenu(strings::MenuListSearchCommands, get_search_commands);
}

void Menus::done() {
  menu_commands.clear();
  menu_command_testers.clear();
  dynamic_menu_lists.clear();
  dynamic_menu_list_item_deleters.clear();
  dynamic_menu_list_item_locators.clear();
  dynamic_menu_list_item_locator_testers.clear();
  current_query = String();
}

void Menus::run_command_async(Expr cmd) {
  Application::notify(ClientNotification::MenuCommand, cmd);
}

bool Menus::run_command_now(Expr cmd) {
  bool (*func)(Expr);
  
  func = menu_commands[cmd];
  if(func && func(cmd))
    return true;
    
  func = menu_commands[cmd[0]];
  if(func && func(std::move(cmd)))
    return true;
    
  return false;
}

Expr Menus::selected_item_command() {
#ifdef RICHMATH_USE_GTK_GUI
  return MathGtkMenuBuilder::selected_item_command();
#endif
#ifdef RICHMATH_USE_WIN32_GUI
  return Win32Menu::selected_item_command();
#endif
  return {};
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
  
  func = dynamic_menu_list_item_deleters[submenu_cmd];
  if(func)
    return func(std::move(submenu_cmd), std::move(item_cmd));
    
  return false;
}

bool Menus::locate_dynamic_submenu_item_source(Expr submenu_cmd, Expr item_cmd) {
  bool (*func)(Expr, Expr);
  
  if(submenu_cmd.is_null())
    return false;
  
  func = dynamic_menu_list_item_locators[submenu_cmd];
  if(func)
    return func(std::move(submenu_cmd), std::move(item_cmd));
    
  return false;
}

MenuCommandStatus Menus::test_locate_dynamic_submenu_item_source(Expr submenu_cmd, Expr item_cmd) {
  MenuCommandStatus (*func)(Expr, Expr);
  
  if(submenu_cmd.is_null())
    return false;
  
  func = dynamic_menu_list_item_locator_testers[submenu_cmd];
  if(func)
    return func(std::move(submenu_cmd), std::move(item_cmd));
    
  return MenuCommandStatus(true);
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
    dynamic_menu_list_item_deleters.set(std::move(submenu_cmd), func);
  else
    dynamic_menu_list_item_deleters.remove(std::move(submenu_cmd));
}

bool Menus::has_submenu_item_deleter(Expr submenu_cmd) {
  return dynamic_menu_list_item_deleters[std::move(submenu_cmd)] != nullptr;
}

void Menus::register_submenu_item_locator(
    Expr                submenu_cmd, 
    bool              (*func)(Expr submenu_cmd, Expr item_cmd),
    MenuCommandStatus (*test)(Expr submenu_cmd, Expr item_cmd)
) {
  if(test)
    dynamic_menu_list_item_locator_testers.set(submenu_cmd, test);
  else
    dynamic_menu_list_item_locator_testers.remove(submenu_cmd);
  
  if(func)
    dynamic_menu_list_item_locators.set(std::move(submenu_cmd), func);
  else
    dynamic_menu_list_item_locators.remove(std::move(submenu_cmd));
}

bool Menus::has_submenu_item_locator(Expr submenu_cmd) {
  return dynamic_menu_list_item_locators[std::move(submenu_cmd)] != nullptr;
}

MenuItemType Menus::menu_item_type(Expr item) {
  if(item == richmath_System_Delimiter) 
    return MenuItemType::Delimiter;
  
  if(item[0] == richmath_System_MenuItem && item.expr_length() == 2) {
    if(!item[1].is_string())
      return MenuItemType::Invalid;
    
    return command_type(item[2]);
  }
  
  if(item[0] == richmath_System_Menu) {
    if(!item[1].is_string())
      return MenuItemType::Invalid;
    
    Expr submenu = item[2];
    if(submenu.is_string()) 
      return MenuItemType::InlineMenu;
    
    if(submenu[0] == richmath_System_List) 
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
    
    if(cmd == strings::ShowHideMenu)
      return MenuItemType::CheckButton;
    
    if(cmd == strings::SearchMenuItems)
      return MenuItemType::SearchBox;
  }
  
  if(cmd.is_rule()) // style->value  is a simple setter (does not toggle)
    return MenuItemType::RadioButton;
  
  if(cmd[0] == richmath_FrontEnd_SetSelectedDocument)
    return MenuItemType::RadioButton;
  
  return MenuItemType::Normal;
}

void Menus::current_menu_search_text(String text) {
  current_query = text;
}

String Menus::current_menu_search_text() {
  return current_query;
}

//} ... class Menus

static Expr get_search_commands(Expr name) {
  Gather g;
  
  Gather::emit(Call(Symbol(richmath_System_MenuItem), String("Search"), strings::SearchMenuItems));
  if(current_query.length() > 0) {
    //Gather::emit(Call(Symbol(richmath_System_MenuItem), current_query, Expr()));
    
    Array<MenuSearchResult> results;
#ifdef RICHMATH_USE_WIN32_GUI
    if(auto mm = Win32Menu::main_menu) {
      //mm->find_menu_items(results, current_query);
      Win32MenuSearchOverlay::collect_menu_matches(results, current_query, mm->hmenu(), String());
    }
#endif
#ifdef RICHMATH_USE_GTK_GUI
    if(auto doc = Documents::current()) {
      if(auto wid = dynamic_cast<MathGtkWidget*>(doc->native())) {
        GtkWidget *toplevel = gtk_widget_get_toplevel(wid->widget());
        if(auto win = dynamic_cast<MathGtkDocumentWindow*>(BasicGtkWidget::from_widget(toplevel))) {
          MathGtkMenuBuilder::collect_menu_matches(results, current_query, GTK_MENU_SHELL(win->menubar()), String(), doc->id());
        }
      }
    }
#endif
    
    std::sort(results.items(), results.items() + results.length());
    
    int max_results = 10;
    for(auto &res : results) {
      if(max_results-- == 0)
        break;
      Gather::emit(std::move(res.item));
    }
    
    Expr docu_pages = Application::interrupt_wait(
                        Call(Symbol(richmath_Documentation_FindDocumentationPages), current_query));
    
    bool need_delimiter = results.length() > 0;
    max_results = 10;
    for(Expr label_and_file : docu_pages.items()) {
      if(max_results-- == 0)
        break;
      
      if(need_delimiter) {
        Gather::emit(Symbol(richmath_System_Delimiter));
        need_delimiter = false;
      }
      
      if(label_and_file.is_rule()) {
        Gather::emit(
          Call(Symbol(richmath_System_MenuItem), 
            String(label_and_file[1]) + String::FromUcs2((const uint16_t*)L" \x2013 Documenation"), 
            Call(Symbol(richmath_FrontEnd_DocumentOpen), label_and_file[2])));
      }
    }
    
    if(results.length() == 0 && docu_pages.expr_length() == 0) {
      Gather::emit(Call(Symbol(richmath_System_MenuItem), strings::SearchMenuNoMatch_label, Symbol(richmath_System_DollarFailed)));
    }
    
    Gather::emit(Symbol(richmath_System_Delimiter));
  }
  
  return g.end();
}
