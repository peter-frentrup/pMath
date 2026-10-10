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
      
    public:
      static Array<Expr> commands;
      static Hashtable<Expr, int> command_ids;
      static ObservableValue<int> selected_menu_command_id;
      
    private:
      MathFltkMenuItems &self;
  };
}

using namespace richmath;

static const char Label_inline_menu_start[] = "[inline menu start]";
static const char Label_inline_menu_end[]   = "[inline menu end]";
static const char Label_empty[]             = "(empty)";

extern pmath_symbol_t richmath_System_Menu;
extern pmath_symbol_t richmath_System_List;

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
  
  mi->callback(menu_item_callback, (void*)(intptr_t)need_command_id(item[2]));
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
  return commands.length();
}

Expr MathFltkMenuItems::Impl::find_command(int id) {
  if(id > 0 && id <= commands.length())
    return commands[id - 1];
  else
    return Expr();
}

//} ... class MathFltkMenuItems::Impl
