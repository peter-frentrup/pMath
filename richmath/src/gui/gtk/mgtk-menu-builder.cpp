#include <gui/gtk/mgtk-menu-builder.h>

#include <eval/application.h>
#include <eval/binding.h>

#include <gui/gtk/mgtk-document-window.h>

#include <util/hashtable.h>

#if GTK_MAJOR_VERSION >= 3
#  include <gdk/gdkkeysyms-compat.h>
#else
#  include <gdk/gdkkeysyms.h>
#endif


using namespace richmath;

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

static void on_menu_item_activate(GtkMenuItem *menuitem, void *id_ptr) {
  if(ignore_activate_signal)
    return;
  
  const char *accel_path_str = (const char *)gtk_menu_item_get_accel_path(menuitem);
  if(!accel_path_str)
    return;
    
  Expr cmd = accel_path_to_cmd[String(accel_path_str)];
  if(!cmd.is_null()) {
    if(auto doc = dynamic_cast<Document *>(Box::find((int)(uintptr_t)id_ptr))) {
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
    
    Application::run_menucommand(cmd);
  }
}

static void on_map_menu_callback(GtkWidget *item, void *data) {
  if(GTK_IS_MENU_ITEM(item)) {
    const char *accel_path_str = (const char *)gtk_menu_item_get_accel_path(GTK_MENU_ITEM(item));
    if(!accel_path_str)
      return;
      
    Expr cmd = accel_path_to_cmd[String(accel_path_str)];
    if(!cmd.is_null()) {
      MenuCommandStatus status = Application::test_menucommand_status(cmd);
      
      gtk_widget_set_sensitive(item, status.enabled);
      
      if(GTK_IS_CHECK_MENU_ITEM(item)) {
        
        // emits "activate" signal if toggled
        ignore_activate_signal = true;
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), status.checked);
        ignore_activate_signal = false;
      }
    }
  }
}

static void on_unmap_menu_callback(GtkWidget *item, void *data) {
  if(GTK_IS_MENU_ITEM(item)) {
    gtk_widget_set_sensitive(item, TRUE);
  }
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


gboolean MathGtkMenuBuilder::on_map_menu(GtkWidget *menu, GdkEventAny *event, void *dummy) {
  // todo: handle tearoff menus
  Application::delay_dynamic_updates(true);
  
  gtk_container_foreach(GTK_CONTAINER(menu), on_map_menu_callback, nullptr);
  return FALSE;
}

gboolean MathGtkMenuBuilder::on_unmap_menu(GtkWidget *menu, GdkEventAny *event, void *dummy) {
  Application::delay_dynamic_updates(false);
  
  gtk_container_foreach(GTK_CONTAINER(menu), on_unmap_menu_callback, nullptr);
  return FALSE;
}

static GtkWidget *create_menu_item_for_command(const char *label, Expr cmd) {
  if(cmd.is_rule())
    return gtk_check_menu_item_new_with_mnemonic(label);
  
  if(cmd[0] == GetSymbol(ScopedCommandSymbol))
    return create_menu_item_for_command(label, cmd[1]);
  
  return gtk_menu_item_new_with_mnemonic(label);
}

void MathGtkMenuBuilder::append_to(GtkMenuShell *menu, GtkAccelGroup *accel_group, int for_document_window_id) {
  if(expr[0] != GetSymbol(MenuSymbol) || expr.expr_length() != 2)
    return;
    
  String name(expr[1]);
  if(name.is_null())
    return;
    
  Expr list = expr[2];
  if(list[0] != PMATH_SYMBOL_LIST)
    return;
    
  for(size_t i = 1; i <= list.expr_length(); ++i) {
    Expr item = list[i];
    
    if( item[0] == GetSymbol(ItemSymbol) &&
        item.expr_length() == 2)
    {
      String name(item[1]);
      Expr cmd(item[2]);
      
      if(name.length() > 0 && cmd.is_valid()) {
        GtkWidget *menu_item;
        
        int len;
        if(char *label = pmath_string_to_utf8(name.get(), &len)) {
          for(int i = 0; i < len; ++i) {
            if(label[i] == '&')
              label[i] = '_';
          }
          menu_item = create_menu_item_for_command(label, cmd);
          pmath_mem_free(label);
        }
        
        String accel_path = add_command(cmd);
        if(char *accel_path_str = pmath_string_to_utf8(accel_path.get(), nullptr)) {
          gtk_menu_item_set_accel_path(GTK_MENU_ITEM(menu_item), accel_path_str);
          pmath_mem_free(accel_path_str);
        }
        
        g_signal_connect(menu_item, "activate", G_CALLBACK(on_menu_item_activate), (void *)(intptr_t)for_document_window_id);
        
        gtk_widget_show(menu_item);
        gtk_menu_shell_append(menu, menu_item);
      }
      
      continue;
    }
    
    if(item == GetSymbol(DelimiterSymbol)) {
      GtkWidget *menu_item = gtk_separator_menu_item_new();
      
      gtk_widget_show(menu_item);
      gtk_menu_shell_append(menu, menu_item);
      continue;
    }
    
    if( item[0] == GetSymbol(MenuSymbol) &&
        item.expr_length() == 2)
    {
      String name(item[1]);
      
      if(name.length() > 0) {
        GtkWidget *menu_item;
        
        int len;
        if(char *label = pmath_string_to_utf8(name.get(), &len)) {
          for(int i = 0; i < len; ++i) {
            if(label[i] == '&')
              label[i] = '_';
          }
          menu_item = gtk_menu_item_new_with_mnemonic(label);
          pmath_mem_free(label);
        }
        
        //g_signal_connect(menu_item, "activate-item", G_CALLBACK(on_show_menu), nullptr);
        
        GtkWidget *submenu = gtk_menu_new();
        
        gtk_widget_add_events(GTK_WIDGET(submenu), GDK_STRUCTURE_MASK);
        
        g_signal_connect(GTK_WIDGET(submenu), "map-event",   G_CALLBACK(on_map_menu),   nullptr);
        g_signal_connect(GTK_WIDGET(submenu), "unmap-event", G_CALLBACK(on_unmap_menu), nullptr);
        
        gtk_menu_set_accel_group(
          GTK_MENU(submenu),
          GTK_ACCEL_GROUP(accel_group));
          
//        gtk_menu_shell_append(GTK_MENU_SHELL(submenu), gtk_tearoff_menu_item_new());

        MathGtkMenuBuilder(item).append_to(
          GTK_MENU_SHELL(submenu),
          accel_group,
          for_document_window_id);
          
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item), submenu);
        gtk_widget_show(menu_item);
        gtk_menu_shell_append(menu, menu_item);
      }
      continue;
    }
  }
}

//} ... class MathGtkMenuBuilder

//{ class MathGtkAccelerators ...

Array<String> MathGtkAccelerators::all_accelerators;

static bool set_accel_key(Expr expr, guint *accel_key, GdkModifierType *accel_mods) {
  if(expr[0] != GetSymbol(KeyEventSymbol) || expr.expr_length() != 2)
    return false;
    
  Expr modifiers = expr[2];
  if(modifiers[0] != PMATH_SYMBOL_LIST)
    return false;
    
  int mods = 0;
  for(size_t i = modifiers.expr_length(); i > 0; --i) {
    Expr item = modifiers[i];
    
    if(item == GetSymbol(KeyAltSymbol))
      mods |= GDK_MOD1_MASK;
    else if(item == GetSymbol(KeyControlSymbol))
      mods |= GDK_CONTROL_MASK;
    else if(item == GetSymbol(KeyShiftSymbol))
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
    
    if( item[0] == GetSymbol(ItemSymbol)               &&
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
    Expr  cmd;
    int   document_id;
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
  
  Application::run_menucommand(accel_data->cmd);
  
  return TRUE;
}

void MathGtkAccelerators::connect_all(GtkAccelGroup *accel_group, int document_id) {
  for(int i = 0; i < all_accelerators.length(); ++i) {
    char *path = pmath_string_to_utf8(all_accelerators[i].get_as_string(), 0);
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
