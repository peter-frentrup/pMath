#include <gui/gtk/mgtk-menu-builder.h>

#include <eval/application.h>
#include <eval/binding.h>
#include <eval/observable.h>

#include <gui/menus.h>
#include <gui/gtk/mgtk-document-window.h>

#include <util/hashtable.h>

#if GTK_MAJOR_VERSION >= 3
#  include <gdk/gdkkeysyms-compat.h>
#else
#  include <gdk/gdkkeysyms.h>
#endif


using namespace richmath;


extern pmath_symbol_t richmath_FE_Delimiter;
extern pmath_symbol_t richmath_FE_Menu;
extern pmath_symbol_t richmath_FE_MenuItem;

static const char accel_path_prefix[] = "<Richmath>/";
static Hashtable<String,  Expr> accel_path_to_cmd;
static Hashtable<Expr, String>  cmd_to_accel_path;

static bool ignore_activate_signal = false;


static String add_command(Expr cmd) {
  if(auto ap_ptr = cmd_to_accel_path.search(cmd))
    return *ap_ptr;
    
  String ap;
  if(cmd.is_string()) {
    ap = String(accel_path_prefix) + cmd;
  }
  else {
    static int cmd_id = 0;
    int id = ++cmd_id;
    
    ap = String(accel_path_prefix) + Expr(id).to_string();
  }
  
  cmd_to_accel_path.set(cmd, ap);
  accel_path_to_cmd.set(ap, cmd);
  
  return ap;
}

namespace {
  class MenuItemBuilder {
    public:
      static bool is_valid(Expr item, MenuItemType *type);
      static bool has_type(GtkWidget *widget, MenuItemType type);

      static void reset(GtkMenuItem *menu_item, MenuItemType type);
      static GtkWidget *create(MenuItemType type, FrontEndReference for_document_window_id);
      
      static void init(GtkMenuItem *menu_item, MenuItemType type, Expr item, GtkAccelGroup *accel_group, FrontEndReference for_document_window_id);
      
      static void inline_menu_list_data(GtkMenuItem *menu_item, Expr data);
      static Expr inline_menu_list_data(GtkWidget *menu_item);
      
      static GtkMenuItem *selected_item() { return _selected_item; }
      
    private:
      static void on_activate(GtkMenuItem *menu_item, void *doc_id_as_ptr);
      static void on_select(GtkMenuItem *menu_item, void *doc_id_as_ptr);
      static void on_deselect(GtkMenuItem *menu_item, void *doc_id_as_ptr);
      static void on_destroy(GtkMenuItem *menu_item, void *doc_id_as_ptr);
      
    private:
      static void set_label(GtkMenuItem *menu_item, String label);

      static void init_command(GtkMenuItem *menu_item, Expr item);
      static void init_sub_menu(GtkMenuItem *menu_item, Expr item, GtkAccelGroup *accel_group, FrontEndReference for_document_window_id);
      static void init_inline_menu(GtkMenuItem *menu_item, Expr item);
      
      static void destroy_inline_menu_list_data(void *data);
    
    private:
      static const char *inline_menu_list_data_key;
      static ObservableValue<GtkMenuItem*> _selected_item;
  };
}

static void collect_container_children(GtkContainer *container, Array<GtkWidget*> &array) {
  gtk_container_foreach(
    container, 
    [](GtkWidget *item, void *_array) {
      Array<GtkWidget*> *array = (Array<GtkWidget*>*)_array;
      array->add(item);
    }, 
    &array);
}

//{ class MathGtkMenuBuilder ...

MathGtkMenuBuilder MathGtkMenuBuilder::main_menu;
MathGtkMenuBuilder MathGtkMenuBuilder::popup_menu;

MathGtkMenuBuilder::MathGtkMenuBuilder() {
}

MathGtkMenuBuilder::MathGtkMenuBuilder(Expr _expr)
  : expr(_expr)
{
}

void MathGtkMenuBuilder::done() {
  accel_path_to_cmd.clear();
  cmd_to_accel_path.clear();
}

gboolean MathGtkMenuBuilder::on_map_menu(GtkWidget *menu, GdkEventAny *event, void *doc_id_as_ptr) {
  // todo: handle tearoff menus
  
  FrontEndReference id = FrontEndReference::unsafe_cast_from_pointer(doc_id_as_ptr);
  expand_inline_lists(GTK_MENU(menu), id);
  
  gtk_container_foreach(
    GTK_CONTAINER(menu), 
    [](GtkWidget *item, void*) {
      if(GTK_IS_MENU_ITEM(item)) {
        const char *accel_path_str = (const char *)gtk_menu_item_get_accel_path(GTK_MENU_ITEM(item));
        if(!accel_path_str)
          return;
          
        Expr cmd = accel_path_to_cmd[String(accel_path_str)];
        if(!cmd.is_null()) {
          MenuCommandStatus status = Menus::test_command_status(cmd);
          
          gtk_widget_set_sensitive(item, status.enabled);
          
          if(GTK_IS_CHECK_MENU_ITEM(item)) {
            
            // emits "activate" signal if toggled
            ignore_activate_signal = true;
            gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), status.checked);
            ignore_activate_signal = false;
          }
          else if(status.checked) {
            pmath_debug_print_object("[cannot display checked status for menu item: ", cmd.get(), "]\n");
          }
        }
      }
    }, 
    nullptr);
  return FALSE;
}

gboolean MathGtkMenuBuilder::on_unmap_menu(GtkWidget *menu, GdkEventAny *event, void *doc_id_as_ptr) {
  gtk_container_foreach(
    GTK_CONTAINER(menu), 
    [](GtkWidget *item, void*) {      
      if(GTK_IS_MENU_ITEM(item)) {
        if(gtk_menu_item_get_accel_path(GTK_MENU_ITEM(item)))
          gtk_widget_set_sensitive(item, TRUE);
      }
    }, 
    nullptr);
  return FALSE;
}

gboolean MathGtkMenuBuilder::on_menu_key_press(GtkWidget *menu, GdkEvent *e, void *doc_id_as_ptr) {
  GdkEventKey *event = &e->key;
  
  bool (*action)(Expr, Expr) = nullptr;
  bool keep_open = false;
  switch(event->keyval) {
    case GDK_Delete: action = Menus::remove_dynamic_submenu_item; keep_open = true; break;
    case GDK_F12:    action = Menus::locate_dynamic_submenu_item_source; break; 
  }
  
  if(action) {
    GtkMenuItem *menu_item = MenuItemBuilder::selected_item();//gtk_menu_get_active(GTK_MENU(menu));
    if(!menu_item)
      return FALSE;
    
    const char *accel_path_str = (const char *)gtk_menu_item_get_accel_path(menu_item);
    if(!accel_path_str)
      return FALSE;
      
    Expr cmd = accel_path_to_cmd[String(accel_path_str)];
    if(cmd.is_null())
      return FALSE;
    
    Expr inline_list_data = MenuItemBuilder::inline_menu_list_data(GTK_WIDGET(menu_item));
    if(inline_list_data.is_null())
      return FALSE;
    
    if(action(inline_list_data, cmd)) {
      if(keep_open)
        MathGtkMenuBuilder::on_map_menu(menu, nullptr, doc_id_as_ptr);
      else
        gtk_widget_hide(menu);
      return TRUE;
    }
  }
  
  return FALSE;
}

void MathGtkMenuBuilder::expand_inline_lists(GtkMenu *menu, FrontEndReference id) {
  GtkAccelGroup *accel_group = gtk_menu_get_accel_group(menu);
  
  Array<GtkWidget*> old_menu_items;
  collect_container_children(GTK_CONTAINER(menu), old_menu_items);
  
  int old_index = 0;
  int num_insertions = 0;
  for(; old_index < old_menu_items.length() ; ++old_index) {
    GtkWidget *menu_item = old_menu_items[old_index];
    
    Expr inline_list_data = MenuItemBuilder::inline_menu_list_data(menu_item);
    if(!inline_list_data.is_null()) {
      //pmath_debug_print_object("[inline list ", inline_list_data.get(), "]\n");
      Expr item_list = Menus::generate_dynamic_submenu(inline_list_data);
      //pmath_debug_print_object("[inline list yields ", item_list.get(), "]\n");
      
      ++old_index;
      bool is_empty = true;
      
      if(item_list[0] == PMATH_SYMBOL_LIST) {
        for(Expr item : item_list.items()) {
          MenuItemType type = Menus::menu_item_type(item);
          if(type == MenuItemType::Invalid)
            continue;
          
          is_empty = false;
          
          if(old_index < old_menu_items.length()) {
            GtkWidget *next_widget = old_menu_items[old_index];
            
            if(MenuItemBuilder::has_type(next_widget, type)) {
              if(MenuItemBuilder::inline_menu_list_data(next_widget) == inline_list_data) {
                GtkMenuItem *next_menu_item = GTK_MENU_ITEM(next_widget);
                MenuItemBuilder::reset(next_menu_item, type);
                gtk_widget_show(next_widget);
                MenuItemBuilder::init(next_menu_item, type, std::move(item), accel_group, id);
                ++old_index;
                continue;
              }
            }
          }
          
          GtkWidget *new_menu_item = MenuItemBuilder::create(type, id);
          gtk_widget_show(new_menu_item);
          gtk_menu_shell_insert(GTK_MENU_SHELL(menu), new_menu_item, old_index + num_insertions);
          ++num_insertions;
          MenuItemBuilder::init(GTK_MENU_ITEM(new_menu_item), type, std::move(item), accel_group, id);
          MenuItemBuilder::inline_menu_list_data(GTK_MENU_ITEM(new_menu_item), inline_list_data);
        }
        
        while(old_index < old_menu_items.length()) {
          GtkWidget *next_widget = old_menu_items[old_index];
          if(MenuItemBuilder::inline_menu_list_data(next_widget) != inline_list_data)
            break;
          
          gtk_widget_hide(next_widget);
          ++old_index;
        }
      }
      
      --old_index;
      
      if(is_empty) 
        gtk_widget_show(menu_item);
      else
        gtk_widget_hide(menu_item);
    }
  }
}

void MathGtkMenuBuilder::append_to(GtkMenuShell *menu, GtkAccelGroup *accel_group, FrontEndReference for_document_window_id) {
  if(expr[0] != richmath_FE_Menu || expr.expr_length() != 2)
    return;
    
  if(!expr[1].is_string())
    return;
    
  Expr list = expr[2];
  if(list[0] != PMATH_SYMBOL_LIST)
    return;
    
  for(size_t i = 1; i <= list.expr_length(); ++i) {
    Expr item = list[i];
    
    MenuItemType type = Menus::menu_item_type(item);
    if(type == MenuItemType::Invalid)
      continue;
    
    GtkWidget *menu_item = MenuItemBuilder::create(type, for_document_window_id);
    gtk_widget_show(menu_item);
    gtk_menu_shell_append(menu, menu_item);
    MenuItemBuilder::init(GTK_MENU_ITEM(menu_item), type, std::move(item), accel_group, for_document_window_id);
  }
}

Expr MathGtkMenuBuilder::selected_item_command() {
  GtkMenuItem *menu_item = MenuItemBuilder::selected_item();
  if(!menu_item)
    return Expr{};
  
  const char *accel_path_str = (const char *)gtk_menu_item_get_accel_path(menu_item);
  if(!accel_path_str)
    return Expr{};
  
  Expr cmd = accel_path_to_cmd[String(accel_path_str)];
  if(cmd.is_null()) 
    return Expr{};
  
  return cmd;
}

//} ... class MathGtkMenuBuilder

//{ class MenuItemBuilder ...

const char *MenuItemBuilder::inline_menu_list_data_key = "richmath:inline_menu_list_data_key";

ObservableValue<GtkMenuItem*> MenuItemBuilder::_selected_item { nullptr };


bool MenuItemBuilder::has_type(GtkWidget *widget, MenuItemType type) {
  switch(type) {
    case MenuItemType::Normal:
    case MenuItemType::InlineMenu:
    case MenuItemType::SubMenu:
      return GTK_IS_MENU_ITEM(widget) && 
             !GTK_IS_CHECK_MENU_ITEM(widget) && 
             !GTK_IS_SEPARATOR_MENU_ITEM(widget);
    
    case MenuItemType::CheckButton:
    case MenuItemType::RadioButton:
      return GTK_IS_CHECK_MENU_ITEM(widget);
    
    case MenuItemType::Delimiter:
      return GTK_IS_SEPARATOR_MENU_ITEM(widget);
  }
  
  return false;
}

void MenuItemBuilder::reset(GtkMenuItem *menu_item, MenuItemType type) {
  gtk_menu_item_set_submenu(menu_item, nullptr);
  if(GTK_IS_CHECK_MENU_ITEM(menu_item)) {
    gtk_check_menu_item_set_draw_as_radio(GTK_CHECK_MENU_ITEM(menu_item), type == MenuItemType::RadioButton);
  }
}

GtkWidget *MenuItemBuilder::create(MenuItemType type, FrontEndReference for_document_window_id) {
  GtkWidget *menu_item = nullptr;
  
  switch(type) {
    case MenuItemType::Delimiter: 
      menu_item = gtk_separator_menu_item_new();
      return menu_item;
    
    case MenuItemType::CheckButton: 
      menu_item = gtk_check_menu_item_new();
      break;
    
    case MenuItemType::RadioButton: 
      menu_item = gtk_check_menu_item_new();
      gtk_check_menu_item_set_draw_as_radio(GTK_CHECK_MENU_ITEM(menu_item), true);
      break;
    
    case MenuItemType::Normal:
    case MenuItemType::InlineMenu:
    case MenuItemType::SubMenu:
      menu_item = gtk_menu_item_new();
      break;
  }
  
  g_signal_connect(
    menu_item, 
    "destroy", 
    G_CALLBACK(MenuItemBuilder::on_destroy), 
    FrontEndReference::unsafe_cast_to_pointer(for_document_window_id));
  
  g_signal_connect(
    menu_item, 
    "activate", 
    G_CALLBACK(MenuItemBuilder::on_activate), 
    FrontEndReference::unsafe_cast_to_pointer(for_document_window_id));
  
  g_signal_connect(
    menu_item, 
    "select", 
    G_CALLBACK(MenuItemBuilder::on_select), 
    FrontEndReference::unsafe_cast_to_pointer(for_document_window_id));
  
  g_signal_connect(
    menu_item, 
    "deselect", 
    G_CALLBACK(MenuItemBuilder::on_deselect), 
    FrontEndReference::unsafe_cast_to_pointer(for_document_window_id));
  
  return menu_item;
}

void MenuItemBuilder::init(GtkMenuItem *menu_item, MenuItemType type, Expr item, GtkAccelGroup *accel_group, FrontEndReference for_document_window_id) {
  switch(type) {
    case MenuItemType::Delimiter:
      break;
    
    case MenuItemType::Normal:
    case MenuItemType::CheckButton: 
    case MenuItemType::RadioButton: 
      init_command(menu_item, std::move(item));
      break;
      
    case MenuItemType::InlineMenu: 
      init_inline_menu(menu_item, std::move(item));
      break;
      
    case MenuItemType::SubMenu: 
      init_sub_menu(menu_item, std::move(item), accel_group, for_document_window_id);
      break;
  }
}

void MenuItemBuilder::inline_menu_list_data(GtkMenuItem *menu_item, Expr data) {
  if(data.is_pointer() && !data.is_null()) {
    g_object_set_data_full(
      G_OBJECT(menu_item), 
      inline_menu_list_data_key,
      PMATH_AS_PTR(data.release()),
      destroy_inline_menu_list_data);
    
    return;
  }
  
  pmath_debug_print_object("[cannot attach to menu item: ", data.get(), "]\n");
}

Expr MenuItemBuilder::inline_menu_list_data(GtkWidget *menu_item) {
  return Expr(pmath_ref(PMATH_FROM_PTR(g_object_get_data(G_OBJECT(menu_item), inline_menu_list_data_key))));
}

void MenuItemBuilder::destroy_inline_menu_list_data(void *data) {
  pmath_t expr = PMATH_FROM_PTR(data);
  pmath_unref(expr);
}

void MenuItemBuilder::set_label(GtkMenuItem *menu_item, String label) {
  int len;
  if(char *str = pmath_string_to_utf8(label.get(), &len)) {
    for(int i = 0; i < len; ++i) {
      if(str[i] == '&')
        str[i] = '_';
    }
    gtk_menu_item_set_use_underline(GTK_MENU_ITEM(menu_item), TRUE);
    gtk_menu_item_set_label(GTK_MENU_ITEM(menu_item), str);
    pmath_mem_free(str);
  }
}

void MenuItemBuilder::init_command(GtkMenuItem *menu_item, Expr item) {
  set_label(menu_item, String(item[1]));
  
  String accel_path = add_command(item[2]);
  if(char *accel_path_str = pmath_string_to_utf8(accel_path.get(), nullptr)) {
    gtk_menu_item_set_accel_path(menu_item, accel_path_str);
    pmath_mem_free(accel_path_str);
  }
}

void MenuItemBuilder::init_sub_menu(GtkMenuItem *menu_item, Expr item, GtkAccelGroup *accel_group, FrontEndReference for_document_window_id) {
  set_label(menu_item, String(item[1]));
  
  GtkWidget *submenu = gtk_menu_new();
  
  gtk_widget_add_events(GTK_WIDGET(submenu), GDK_STRUCTURE_MASK);
  
  g_signal_connect(submenu, "map-event",       G_CALLBACK(MathGtkMenuBuilder::on_map_menu),       FrontEndReference::unsafe_cast_to_pointer(for_document_window_id));
  g_signal_connect(submenu, "unmap-event",     G_CALLBACK(MathGtkMenuBuilder::on_unmap_menu),     FrontEndReference::unsafe_cast_to_pointer(for_document_window_id));
  g_signal_connect(submenu, "key-press-event", G_CALLBACK(MathGtkMenuBuilder::on_menu_key_press), FrontEndReference::unsafe_cast_to_pointer(for_document_window_id));
  
  gtk_menu_set_accel_group(
    GTK_MENU(submenu),
    GTK_ACCEL_GROUP(accel_group));
    
  //gtk_menu_shell_append(GTK_MENU_SHELL(submenu), gtk_tearoff_menu_item_new());

  MathGtkMenuBuilder(item).append_to(
    GTK_MENU_SHELL(submenu),
    accel_group,
    for_document_window_id);
    
  gtk_menu_item_set_submenu(menu_item, submenu);
}

void MenuItemBuilder::init_inline_menu(GtkMenuItem *menu_item, Expr item) {
  //set_label(menu_item, String(item[1]));
  gtk_menu_item_set_label(menu_item, "(empty)");
  gtk_widget_set_sensitive(GTK_WIDGET(menu_item), FALSE);
  
  inline_menu_list_data(menu_item, item[2]);
}

void MenuItemBuilder::on_activate(GtkMenuItem *menu_item, void *doc_id_as_ptr) {
  if(ignore_activate_signal)
    return;
  
  FrontEndReference id = FrontEndReference::unsafe_cast_from_pointer(doc_id_as_ptr);
  
  const char *accel_path_str = (const char *)gtk_menu_item_get_accel_path(menu_item);
  if(!accel_path_str)
    return;
  
  Expr cmd = accel_path_to_cmd[String(accel_path_str)];
  if(!cmd.is_null()) {
    if(auto doc = FrontEndObject::find_cast<Document>(id)) {
      auto wid = dynamic_cast<BasicGtkWidget *>(doc->native());
      while(wid) {
        if(auto win = dynamic_cast<MathGtkDocumentWindow *>(wid)) {
          win->run_menucommand(cmd);
          return;
        }
        
        wid = wid->parent();
      }
    }
    
    g_warning("no MathGtkDocumentWindow parent found.");
    
    Menus::run_command(cmd);
  }
}

void MenuItemBuilder::on_select(GtkMenuItem *menu_item, void *doc_id_as_ptr) {
  _selected_item = menu_item;
}

void MenuItemBuilder::on_deselect(GtkMenuItem *menu_item, void *doc_id_as_ptr) {
  if(_selected_item.unobserved_equals(menu_item))
    _selected_item = nullptr;
}

void MenuItemBuilder::on_destroy(GtkMenuItem *menu_item, void *doc_id_as_ptr) {
  if(_selected_item.unobserved_equals(menu_item))
    _selected_item = nullptr;
}

//} ... class MenuItemBuilder

//{ class MathGtkAccelerators ...

Array<String> MathGtkAccelerators::all_accelerators;

static bool set_accel_key(Expr expr, guint *accel_key, GdkModifierType *accel_mods) {
  if(expr[0] != richmath_FE_KeyEvent || expr.expr_length() != 2)
    return false;
    
  Expr modifiers = expr[2];
  if(modifiers[0] != PMATH_SYMBOL_LIST)
    return false;
    
  int mods = 0;
  for(size_t i = modifiers.expr_length(); i > 0; --i) {
    Expr item = modifiers[i];
    
    if(item == richmath_FE_KeyAlt)
      mods |= GDK_MOD1_MASK;
    else if(item == richmath_FE_KeyControl)
      mods |= GDK_CONTROL_MASK;
    else if(item == richmath_FE_KeyShift)
      mods |= GDK_SHIFT_MASK;
    else
      return false;
  }
  
  *accel_mods = (GdkModifierType)mods;
  
  String key(expr[1]);
  if(key.length() == 0)
    return false;
    
  if(key.length() == 1) {
    uint16_t ch = key.buffer()[0];
    
    if(ch >= 'A' && ch <= 'Z')
      ch -= 'A' - 'a';
      
    *accel_key = gdk_unicode_to_keyval(ch);
    return true;
  }
  
  if(key.equals("F1"))                      *accel_key = GDK_F1;
  else if(key.equals("F2"))                 *accel_key = GDK_F2;
  else if(key.equals("F3"))                 *accel_key = GDK_F3;
  else if(key.equals("F4"))                 *accel_key = GDK_F4;
  else if(key.equals("F5"))                 *accel_key = GDK_F5;
  else if(key.equals("F6"))                 *accel_key = GDK_F6;
  else if(key.equals("F7"))                 *accel_key = GDK_F7;
  else if(key.equals("F8"))                 *accel_key = GDK_F8;
  else if(key.equals("F9"))                 *accel_key = GDK_F9;
  else if(key.equals("F10"))                *accel_key = GDK_F10;
  else if(key.equals("F11"))                *accel_key = GDK_F11;
  else if(key.equals("F12"))                *accel_key = GDK_F12;
  else if(key.equals("F13"))                *accel_key = GDK_F13;
  else if(key.equals("F14"))                *accel_key = GDK_F14;
  else if(key.equals("F15"))                *accel_key = GDK_F15;
  else if(key.equals("F16"))                *accel_key = GDK_F16;
  else if(key.equals("F17"))                *accel_key = GDK_F17;
  else if(key.equals("F18"))                *accel_key = GDK_F18;
  else if(key.equals("F19"))                *accel_key = GDK_F19;
  else if(key.equals("F20"))                *accel_key = GDK_F20;
  else if(key.equals("F21"))                *accel_key = GDK_F21;
  else if(key.equals("F22"))                *accel_key = GDK_F22;
  else if(key.equals("F23"))                *accel_key = GDK_F23;
  else if(key.equals("F24"))                *accel_key = GDK_F24;
  else if(key.equals("Enter"))              *accel_key = GDK_Return;
  else if(key.equals("Tab"))                *accel_key = GDK_Tab;
  else if(key.equals("Esc"))                *accel_key = GDK_Escape;
  else if(key.equals("Pause"))              *accel_key = GDK_Pause;
  else if(key.equals("PageUp"))             *accel_key = GDK_Page_Up;
  else if(key.equals("PageDown"))           *accel_key = GDK_Page_Down;
  else if(key.equals("End"))                *accel_key = GDK_End;
  else if(key.equals("Home"))               *accel_key = GDK_Home;
  else if(key.equals("Left"))               *accel_key = GDK_Left;
  else if(key.equals("Up"))                 *accel_key = GDK_Up;
  else if(key.equals("Right"))              *accel_key = GDK_Right;
  else if(key.equals("Down"))               *accel_key = GDK_Down;
  else if(key.equals("Insert"))             *accel_key = GDK_Insert;
  else if(key.equals("Delete"))             *accel_key = GDK_Delete;
  else if(key.equals("Numpad0"))            *accel_key = GDK_0; // GDK_KP_0 ...?
  else if(key.equals("Numpad1"))            *accel_key = GDK_1;
  else if(key.equals("Numpad2"))            *accel_key = GDK_2;
  else if(key.equals("Numpad3"))            *accel_key = GDK_3;
  else if(key.equals("Numpad4"))            *accel_key = GDK_4;
  else if(key.equals("Numpad5"))            *accel_key = GDK_5;
  else if(key.equals("Numpad6"))            *accel_key = GDK_6;
  else if(key.equals("Numpad7"))            *accel_key = GDK_7;
  else if(key.equals("Numpad8"))            *accel_key = GDK_8;
  else if(key.equals("Numpad9"))            *accel_key = GDK_9;
  else if(key.equals("Numpad+"))            *accel_key = GDK_KP_Add;
  else if(key.equals("Numpad-"))            *accel_key = GDK_KP_Subtract;
  else if(key.equals("Numpad*"))            *accel_key = GDK_KP_Multiply;
  else if(key.equals("Numpad/"))            *accel_key = GDK_KP_Divide;
  else if(key.equals("NumpadDecimal"))      *accel_key = GDK_KP_Decimal;
  else if(key.equals("NumpadEnter"))        *accel_key = GDK_KP_Enter;
  else if(key.equals("Play"))               *accel_key = GDK_AudioPlay;
  else if(key.equals("Zoom"))               *accel_key = GDK_ZoomIn;
  else                                      return false;
  
  return true;
}

void MathGtkAccelerators::load(Expr expr) {
  if(expr[0] != PMATH_SYMBOL_LIST)
    return;
    
  for(size_t i = 1; i <= expr.expr_length(); ++i) {
    Expr item = expr[i];
    Expr cmd(item[2]);
    
    guint           accel_key = 0;
    GdkModifierType accel_mod = (GdkModifierType)0;
    
    if( item[0] == richmath_FE_MenuItem                &&
        item.expr_length() == 2                        &&
        set_accel_key(item[1], &accel_key, &accel_mod) &&
        gtk_accelerator_valid(accel_key, accel_mod))
    {
      String accel_path = add_command(cmd);
      
      while(true) {
        char *str = pmath_string_to_utf8(accel_path.get(), nullptr);
        if(!str)
          break;
          
        if(gtk_accel_map_lookup_entry(str, 0)) {
          pmath_mem_free(str);
          accel_path += " ";
        }
        else {
          gtk_accel_map_add_entry(str, accel_key, accel_mod);
          all_accelerators.add(accel_path);
          accel_path_to_cmd.set(accel_path, cmd);
          //gtk_accel_group_connect_by_path(str, str, 0);
          pmath_mem_free(str);
          break;
        }
      }
    }
    else 
      pmath_debug_print_object("Cannot add shortcut ", item.get(), "\n");
  }
}

class AccelData {
  public:
    Expr                cmd;
    FrontEndReference   document_id;
};

static void accel_data_destroy(void *data, GClosure *closure) {
  AccelData *accel_data = (AccelData *)data;
  
  delete accel_data;
}

static gboolean closure_callback(
  GtkAccelGroup   *group,
  GObject         *acceleratable,
  guint            keyval,
  GdkModifierType  modifier,
  void            *user_data
) {
  AccelData *accel_data = (AccelData *)user_data;
  
  if(auto doc = dynamic_cast<Document *>(Box::find(accel_data->document_id))) {
    auto wid = dynamic_cast<BasicGtkWidget *>(doc->native());
    while(wid) {
      if(auto win = dynamic_cast<MathGtkDocumentWindow *>(wid)) {
        win->run_menucommand(accel_data->cmd);
        return TRUE;
      }
      
      wid = wid->parent();
    }
  }
  
  g_warning("no MathGtkDocumentWindow parent found.");
  
  Menus::run_command(accel_data->cmd);
  
  return TRUE;
}

void MathGtkAccelerators::connect_all(GtkAccelGroup *accel_group, FrontEndReference document_id) {
  for(auto accel : all_accelerators) {
    char *path = pmath_string_to_utf8(accel.get_as_string(), 0);
    AccelData *accel_data = new AccelData;
    
    if(path && accel_data) {
      accel_data->document_id = document_id;
      accel_data->cmd = accel_path_to_cmd[path];
      
      gtk_accel_group_connect_by_path(
        accel_group,
        path,
        g_cclosure_new(
          G_CALLBACK(closure_callback),
          accel_data,
          accel_data_destroy));
    }
    else 
      delete accel_data;
    
    pmath_mem_free(path);
  }
}

//} ... class MathGtkAccelerators
