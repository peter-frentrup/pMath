#include <gui/fltk/mfltk-menu-items.h>

#include <FL/Fl.H>
#include <FL/Fl_Menu_.H>
#include <FL/Fl_Menu_Item.H>

#ifdef PMATH_DEBUG_LOG
#  include <Fl/names.H>
#endif

#include <gui/document.h>
#include <gui/menus.h>

#include <eval/observable.h>

#include <util/autovaluereset.h>
#include <util/hashtable.h>


namespace richmath {
  class MathFltkMenuItems::Impl {
    public:
      explicit Impl(MathFltkMenuItems &self) : self{self} {}
      
      void load(Expr expr);
      void add_all(Expr list);
      
      Fl_Menu_Item *add_empty();
      Fl_Menu_Item *add_delimiter(Fl_Menu_Item *prev);
      Fl_Menu_Item *add_submenu(Expr item);
      Fl_Menu_Item *add_simple(MenuItemType type, Expr item);
      Fl_Menu_Item *add_inline_menu(Expr item);
      
      void clear();
      
      static void init_simple(Fl_Menu_Item *mi, MenuItemType type, Expr item);
      static void update_simple_state(Fl_Menu_Item *mi);
      
      static Fl_Menu_Item *update_all_states(Fl_Menu_Item *mi);
      
      static void set_label(Fl_Menu_Item *mi, String label);
      static void free_item_label(Fl_Menu_Item *item);
      
      static void menu_item_callback(Fl_Widget *sender, void *command_id);
      
      static int need_command_id(Expr expr);
      static Expr find_command(int id);
      
      static int decode_shortcut_key(Expr expr);
      
    public:
      static Array<Expr> commands;
      static Hashtable<Expr, int> command_ids;
      static ObservableValue<int> selected_menu_command_id;
      
      static Hashtable<int, int> shortcut_by_command;
      static Hashtable<int, int> command_by_shortcut;
      
    private:
      MathFltkMenuItems &self;
  };
}

using namespace richmath;

static const char Label_inline_menu_start[] = "[inline menu start]";
static const char Label_inline_menu_end[]   = "[inline menu end]";
static const char Label_empty[]             = "(empty)";

extern pmath_symbol_t richmath_System_Menu;
extern pmath_symbol_t richmath_System_MenuItem;
extern pmath_symbol_t richmath_System_List;
extern pmath_symbol_t richmath_FE_KeyAlt;
extern pmath_symbol_t richmath_FE_KeyControl;
extern pmath_symbol_t richmath_FE_KeyEvent;
extern pmath_symbol_t richmath_FE_KeyShift;

//{ class MathFltkMenuItems ...

MathFltkMenuItems MathFltkMenuItems::main_menu;

MathFltkMenuItems::MathFltkMenuItems()
: Base()
{
  SET_BASE_DEBUG_TAG(typeid(*this).name());
}

MathFltkMenuItems::~MathFltkMenuItems() {
  Impl(*this).clear();
}

void MathFltkMenuItems::reload(Expr expr) {
  Impl(main_menu).clear();
  Impl(*this).load(PMATH_CPP_MOVE(expr));
}

Expr MathFltkMenuItems::selected_item_command() {
  return Impl::find_command(Impl::selected_menu_command_id);
}

void MathFltkMenuItems::load_accelerators(Expr expr) {
  Impl::shortcut_by_command.default_value = 0;
  Impl::command_by_shortcut.default_value = 0;
  
  if(!expr.item_equals(0, richmath_System_List))
    return;
  
  for(Expr item: expr.items()) {
    if(item.item_equals(0, richmath_System_MenuItem) && item.expr_length() == 2) {
      int shortcut = Impl::decode_shortcut_key(item[1]);
      if(!shortcut)
        continue;
      
      int cmd_id = Impl::need_command_id(item[2]);
      if(!cmd_id)
        continue;
      
      //pmath_debug_print("[new shortcut 0x%x for cmd %d]\n", shortcut, cmd_id);
      if(!Impl::shortcut_by_command.search(cmd_id))
        Impl::shortcut_by_command.set(cmd_id, shortcut);
      
      Impl::command_by_shortcut.set(shortcut, cmd_id);
    }
  }
  
  pmath_debug_print("[have %d shortcuts for %d commands]\n", Impl::command_by_shortcut.size(), Impl::shortcut_by_command.size());
}

void MathFltkMenuItems::update_states(Fl_Menu_Item *menu, FrontEndReference id) {
  AutoValueReset<Document*> auto_menu_redirect{ Menus::current_document_redirect };
  
  if(auto box = FrontEndObject::find_cast<Box>(id))
    Menus::current_document_redirect = box->find_parent<Document>(true);
  
  Impl::update_all_states(menu);
}

void MathFltkMenuItems::done() {
  Impl(main_menu).clear();
  {
    Array<Expr> empty;
    swap(Impl::commands, empty);
  }
  Impl::command_ids.clear();
}

Fl_Menu_Item *MathFltkMenuItems::menutable() {
  return items.length() ? items.items() : nullptr;
}

//} ... class MathFltkMenuItems

//{ class MathFltkMenuItems::Impl ...

Array<Expr> MathFltkMenuItems::Impl::commands;
Hashtable<Expr, int> MathFltkMenuItems::Impl::command_ids;
ObservableValue<int> MathFltkMenuItems::Impl::selected_menu_command_id;

Hashtable<int, int> MathFltkMenuItems::Impl::shortcut_by_command;
Hashtable<int, int> MathFltkMenuItems::Impl::command_by_shortcut;

void MathFltkMenuItems::Impl::load(Expr expr) {
  if(!expr.item_equals(0, richmath_System_Menu) || expr.expr_length() != 2)
    return;
    
  if(!expr[1].is_string())
    return;
  
  add_all(expr[2]);
  
  add_empty();
}

void MathFltkMenuItems::Impl::add_all(Expr list) {
  if(!list.item_equals(0, richmath_System_List))
    return;
  
  Fl_Menu_Item *mi = nullptr;
  
  for(size_t i = 1; i <= list.expr_length(); ++i) {
    Expr item = list[i];
    
    MenuItemType type = Menus::menu_item_type(item);
    
    switch(type) {
      case MenuItemType::Invalid: continue;
      case MenuItemType::Delimiter: mi = add_delimiter(mi); break;
      case MenuItemType::SubMenu:   mi = add_submenu(item); break;
      
      case MenuItemType::Normal:    
      case MenuItemType::SearchBox:    
      case MenuItemType::CheckButton:    
      case MenuItemType::RadioButton: mi = add_simple(type, item); break;
      
      case MenuItemType::InlineMenu: mi = add_inline_menu(item); break;
    }
    
  }
}

Fl_Menu_Item *MathFltkMenuItems::Impl::add_empty() {
  self.items.add({});
  return &self.items[self.items.length() - 1];
}

Fl_Menu_Item *MathFltkMenuItems::Impl::add_delimiter(Fl_Menu_Item *prev) {
  if(prev && !(prev->flags & FL_MENU_DIVIDER)) {
    prev->flags |= FL_MENU_DIVIDER;
    return prev;
  }
  
  prev = add_empty();
  prev->label(FL_NO_LABEL, Label_empty);
  prev->flags = FL_MENU_DIVIDER;
  return prev;
}

Fl_Menu_Item *MathFltkMenuItems::Impl::add_submenu(Expr item) {
  int i = self.items.length();
  Fl_Menu_Item *mi = add_empty();
  RICHMATH_ASSERT(mi == &self.items[i]);
  
  set_label(mi, item[1]);
  
  mi->flags = FL_SUBMENU;
  
  add_all(item[2]); // may invalidate `mi`
  
  add_empty();
  return &self.items[i];
}

Fl_Menu_Item *MathFltkMenuItems::Impl::add_simple(MenuItemType type, Expr item) {
  Fl_Menu_Item *mi = add_empty();
  init_simple(mi, type, PMATH_CPP_MOVE(item));
  return mi;
}

Fl_Menu_Item *MathFltkMenuItems::Impl::add_inline_menu(Expr item) {
  int start = self.items.length();
  {
    Fl_Menu_Item *mi_start = add_empty();
    mi_start->label(FL_NORMAL_LABEL, Label_inline_menu_start);
    mi_start->flags = FL_MENU_INACTIVE | FL_MENU_INVISIBLE;
    mi_start->callback(menu_item_callback, (void*)(intptr_t)need_command_id(item[2]));
  }
  
  for(int i = 0; i < 30; ++i) {
    Fl_Menu_Item *mi = add_empty();
    mi->label(FL_NORMAL_LABEL, Label_empty);
    mi->flags = FL_MENU_INACTIVE | FL_MENU_INVISIBLE;
    mi->callback(menu_item_callback, nullptr);
  }
  
  {
    Fl_Menu_Item *mi_end = add_empty();
    mi_end->label(FL_NORMAL_LABEL, Label_inline_menu_end);
    mi_end->flags = FL_MENU_INACTIVE | FL_MENU_INVISIBLE;
  }
  
  Fl_Menu_Item *mi_first = &self.items[start + 1];
  mi_first->flags &= ~FL_MENU_INVISIBLE;
  
  return mi_first;
}

void MathFltkMenuItems::Impl::clear() {
  for(Fl_Menu_Item &item : self.items) {
    Impl::free_item_label(&item);
  }
  self.items.length(0);
}

void MathFltkMenuItems::Impl::init_simple(Fl_Menu_Item *mi, MenuItemType type, Expr item) {
  mi->flags = FL_MENU_CHATTY;
  
  set_label(mi, item[1]);
  
  switch(type) {
    case MenuItemType::CheckButton: mi->flags |= FL_MENU_TOGGLE; break;
    case MenuItemType::RadioButton: mi->flags |= FL_MENU_RADIO;  break;
  }
  
  int cmd_id = need_command_id(item[2]);
  mi->shortcut(shortcut_by_command[cmd_id]);
  mi->callback(menu_item_callback, (void*)(intptr_t)cmd_id);
}

void MathFltkMenuItems::Impl::update_simple_state(Fl_Menu_Item *mi) {
  if(Expr cmd = find_command((int)(intptr_t)mi->user_data())) {
    MenuCommandStatus status = Menus::test_command_status(cmd);
    
    if(status.enabled)
      mi->flags &= ~FL_MENU_INACTIVE;
    else
      mi->flags |= FL_MENU_INACTIVE;
      
    if(status.checked)
      mi->flags |= FL_MENU_VALUE;
    else
      mi->flags &= ~FL_MENU_VALUE;
  }
}

Fl_Menu_Item *MathFltkMenuItems::Impl::update_all_states(Fl_Menu_Item *mi) {
  if(!mi)
    return mi;
  
  Fl_Menu_Item *prev_visible = nullptr;
  
  for(; mi->text; ++mi) {
    if(mi->flags & FL_SUBMENU) {
      if(mi->visible())
        prev_visible = mi;
      
      mi = update_all_states(mi + 1);
      continue;
    }
    
    if(mi->text == Label_inline_menu_start) {
      Fl_Menu_Item *mi_start = mi;
      
      // inner submenus are not supported
      Fl_Menu_Item *mi_inner = mi_start + 1;
      for(mi = mi_inner; mi && mi->text && mi->text != Label_inline_menu_end; ++mi) {
      }
      
      if(Expr inline_list_data = find_command((int)(intptr_t)mi_start->user_data())) {
        if(Expr item_list = Menus::generate_dynamic_submenu(inline_list_data)) {
          
          bool final_delimiter = false;
          Fl_Menu_Item *mi_old_end = mi;
          while(mi_old_end != mi_inner) {
            --mi_old_end;
            if(mi_old_end->visible()) {
              final_delimiter = 0 != (mi_old_end->flags & FL_MENU_DIVIDER);
              break;
            }
          }
          
          prev_visible = nullptr;
//          if(prev_visible && (prev_visible->flags & FL_MENU_DIVIDER)) {
//            prev_visible->flags &= ~FL_MENU_DIVIDER;
//          }
          
          for(size_t i = 1; mi_inner != mi && i <= item_list.expr_length(); ++i) {
            Expr item = item_list[i];
            
            MenuItemType type = Menus::menu_item_type(item);
            
            switch(type) {
              case MenuItemType::Delimiter:  {
                if(prev_visible) {
                  prev_visible->flags |= FL_MENU_DIVIDER;
                  prev_visible = nullptr;
                }
                else {
                  free_item_label(mi_inner);
                  mi_inner->flags = FL_MENU_INACTIVE | FL_MENU_DIVIDER;
                  mi_inner->label(FL_NO_LABEL, Label_empty);
                  mi_inner->user_data(nullptr);
                  mi_inner->shortcut(0);
                  ++mi_inner;
                }
              } break;
              
              case MenuItemType::Normal:    
              case MenuItemType::SearchBox:    
              case MenuItemType::CheckButton:    
              case MenuItemType::RadioButton:
                init_simple(mi_inner, type, item);
                update_simple_state(mi_inner);
                prev_visible = mi_inner++;
                break;
                
              default: break; // Invalid, SubMenu, InlineMenu
            }
          }
          
          if(mi_inner == mi_start + 1 && mi_inner != mi) {
            free_item_label(mi_inner);
            mi_inner->label(FL_NORMAL_LABEL, Label_empty);
            mi_inner->flags = FL_MENU_INACTIVE;
            mi_inner->user_data(nullptr);
            mi_inner->shortcut(0);
            prev_visible = mi_inner++;
          }
          
          if(prev_visible) {
            if(final_delimiter)
              prev_visible->flags |= FL_MENU_DIVIDER;
            else
              prev_visible->flags &= ~FL_MENU_DIVIDER;
          }
          
          for(;mi_inner != mi; ++mi_inner) {
            mi_inner->flags |= FL_MENU_INVISIBLE;
            mi_inner->user_data(nullptr);
            mi_inner->shortcut(0);
          }
        }
      }
      
      if(!mi->text)
        return mi;
      
      continue;
    }
    
    update_simple_state(mi);
    
    if(mi->visible())
      prev_visible = mi;
  }
  
  return mi;
}

void MathFltkMenuItems::Impl::set_label(Fl_Menu_Item *mi, String label) {
  free_item_label(mi);
  
  int len;
  if(char *str = pmath_string_to_utf8(label.get(), &len)) {
    // TODO: escape @
    mi->label(FL_NORMAL_LABEL, str);
  }
  else {
    mi->label(FL_NO_LABEL, Label_empty);
  }
}

void MathFltkMenuItems::Impl::free_item_label(Fl_Menu_Item *item) {
  if(!item->text
  || item->text == Label_empty
  || item->text == Label_inline_menu_start
  || item->text == Label_inline_menu_end)
    return;
  
  switch(item->labeltype()) {
    case _FL_MULTI_LABEL: {
      // TODO: free Fl_Multi_Label
    } break;
    case _FL_IMAGE_LABEL: {
      // TODO: free Fl_Image
    } break;
    default:
      pmath_mem_free((void*)item->text);
      break;
  }
  
  item->label(FL_NO_LABEL, Label_empty);
}

void MathFltkMenuItems::Impl::menu_item_callback(Fl_Widget *sender, void *_command_id) {
//  Fl_Menu_ *menu = static_cast<Fl_Menu_*>(sender);
//  const Fl_Menu_Item *mi = menu->mvalue();
  
  int command_id = (int)(intptr_t)_command_id;
  if(Expr cmd = find_command(command_id)) {
    switch(Fl::callback_reason()) {
      case FL_REASON_GOT_FOCUS:
        selected_menu_command_id = command_id;
        break;
      
      case FL_REASON_LOST_FOCUS:
        if(selected_menu_command_id.unobserved_equals(command_id)) {
          selected_menu_command_id = 0;
        }
        break;
      
      case FL_REASON_SELECTED:
        pmath_debug_print("[menu_item_callback on %p reason %s: ", sender, fl_callback_reason_str(Fl::callback_reason()).c_str());
        pmath_debug_print_object("", cmd.get(), "]\n");

        Menus::run_command_now(cmd);
        break;
    }
  }
   else {
    pmath_debug_print("[menu_item_callback(%p, %p) reason=%d]\n", sender, command_id, fl_callback_reason_str(Fl::callback_reason()).c_str());
  }
}

int MathFltkMenuItems::Impl::need_command_id(Expr expr) {
  if(int *cmd_id = command_ids.search(expr))
    return *cmd_id;
  
  commands.add(expr);
  int id = commands.length();
  command_ids.set(expr, id);
  return id;
}

Expr MathFltkMenuItems::Impl::find_command(int id) {
  if(id > 0 && id <= commands.length())
    return commands[id - 1];
  else
    return Expr();
}

int MathFltkMenuItems::Impl::decode_shortcut_key(Expr expr) {
  if(!expr.item_equals(0, richmath_FE_KeyEvent) || expr.expr_length() != 2)
    return 0;
  
  Expr modifiers = expr[2];
  if(!modifiers.item_equals(0, richmath_System_List))
    return 0;
  
  int mods = 0;
  for(Expr item : modifiers.items()) {
    if(item == richmath_FE_KeyAlt)
      mods |= FL_ALT;
    else if(item == richmath_FE_KeyControl)
      mods |= FL_CTRL;
    else if(item == richmath_FE_KeyShift)
      mods |= FL_SHIFT;
    else
      return 0;
  }
  
  String key(expr[1]);
  if(key.length() == 0)
    return 0;
  
  if(key.length() == 1) {
    uint16_t ch = key.buffer()[0];
    
    if(ch >= 'A' && ch <= 'Z')
      ch -= 'A' - 'a';
    
    return mods + ch;
  }
  
  if(key.equals("F1"))            return mods + (FL_F + 1);
  if(key.equals("F2"))            return mods + (FL_F + 2);
  if(key.equals("F3"))            return mods + (FL_F + 3);
  if(key.equals("F4"))            return mods + (FL_F + 4);
  if(key.equals("F5"))            return mods + (FL_F + 5);
  if(key.equals("F6"))            return mods + (FL_F + 6);
  if(key.equals("F7"))            return mods + (FL_F + 7);
  if(key.equals("F8"))            return mods + (FL_F + 8);
  if(key.equals("F9"))            return mods + (FL_F + 9);
  if(key.equals("F10"))           return mods + (FL_F + 10);
  if(key.equals("F11"))           return mods + (FL_F + 11);
  if(key.equals("F12"))           return mods + (FL_F + 12);
  if(key.equals("F13"))           return mods + (FL_F + 13);
  if(key.equals("F14"))           return mods + (FL_F + 14);
  if(key.equals("F15"))           return mods + (FL_F + 15);
  if(key.equals("F16"))           return mods + (FL_F + 16);
  if(key.equals("F17"))           return mods + (FL_F + 17);
  if(key.equals("F18"))           return mods + (FL_F + 18);
  if(key.equals("F19"))           return mods + (FL_F + 19);
  if(key.equals("F20"))           return mods + (FL_F + 20);
  if(key.equals("F21"))           return mods + (FL_F + 21);
  if(key.equals("F22"))           return mods + (FL_F + 22);
  if(key.equals("F23"))           return mods + (FL_F + 23); static_assert(FL_F + 23 <= FL_F_Last, "");
  if(key.equals("Enter"))         return mods + FL_Enter;
  if(key.equals("Tab"))           return mods + FL_Tab;
  if(key.equals("Esc"))           return mods + FL_Escape;
  if(key.equals("Pause"))         return mods + FL_Pause;
  if(key.equals("PageUp"))        return mods + FL_Page_Up;
  if(key.equals("PageDown"))      return mods + FL_Page_Down;
  if(key.equals("End"))           return mods + FL_End;
  if(key.equals("Home"))          return mods + FL_Home;
  if(key.equals("LeftArrow"))     return mods + FL_Left;
  if(key.equals("UpArrow"))       return mods + FL_Up;
  if(key.equals("RightArrow"))    return mods + FL_Right;
  if(key.equals("DownArrow"))     return mods + FL_Down;
  if(key.equals("Insert"))        return mods + FL_Insert;
  if(key.equals("Delete"))        return mods + FL_Delete;
  if(key.equals("Numpad0"))       return mods + FL_KP + '0';
  if(key.equals("Numpad1"))       return mods + FL_KP + '1';
  if(key.equals("Numpad2"))       return mods + FL_KP + '2';
  if(key.equals("Numpad3"))       return mods + FL_KP + '3';
  if(key.equals("Numpad4"))       return mods + FL_KP + '4';
  if(key.equals("Numpad5"))       return mods + FL_KP + '5';
  if(key.equals("Numpad6"))       return mods + FL_KP + '6';
  if(key.equals("Numpad7"))       return mods + FL_KP + '7';
  if(key.equals("Numpad8"))       return mods + FL_KP + '8';
  if(key.equals("Numpad9"))       return mods + FL_KP + '9';
  if(key.equals("Numpad+"))       return mods + FL_KP + '+';
  if(key.equals("Numpad-"))       return mods + FL_KP + '-';
  if(key.equals("Numpad*"))       return mods + FL_KP + '*';
  if(key.equals("Numpad/"))       return mods + FL_KP + '/';
  if(key.equals("NumpadDecimal")) return mods + FL_KP + '.';
  if(key.equals("NumpadEnter"))   return mods + FL_KP_Enter;
  if(key.equals("Play"))          return mods + FL_Media_Play;
  //if(key.equals("Zoom"))          return mods + ???;
  
  return 0;
}

//} ... class MathFltkMenuItems::Impl
