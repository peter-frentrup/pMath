#include <gui/gtk/mgtk-menu-builder.h>

#include <eval/application.h>
#include <eval/binding.h>

#include <gui/gtk/mgtk-document-window.h>

#include <util/hashtable.h>

#include <gdk/gdkkeysyms.h>

static const char accel_path_prefix[] = "<Richmath>/";

using namespace richmath;

static void on_menu_item_activate(GtkMenuItem *menuitem, void *id_ptr) {
  const char *accel_path_str = (const char*)gtk_menu_item_get_accel_path(menuitem);
  if(!accel_path_str)
    return;
    
  String cmd(accel_path_str);
  if(cmd.starts_with(accel_path_prefix)) {
    cmd = cmd.part(sizeof(accel_path_prefix) - 1, -1);
    
    Document *doc = dynamic_cast<Document*>(Box::find((int)id_ptr));
    if(doc) {
      BasicGtkWidget *wid = dynamic_cast<BasicGtkWidget*>(doc->native());
      
      while(wid) {
        MathGtkDocumentWindow *win = dynamic_cast<MathGtkDocumentWindow*>(wid);
        if(win) {
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
    const char *accel_path_str = (const char*)gtk_menu_item_get_accel_path(GTK_MENU_ITEM(item));
    if(!accel_path_str)
      return;
      
    String cmd(accel_path_str);
    if(cmd.starts_with(accel_path_prefix)) {
      cmd = cmd.part(sizeof(accel_path_prefix) - 1, -1);
      
      gtk_widget_set_sensitive(item, Application::is_menucommand_runnable(cmd));
    }
  }
}

static void on_unmap_menu_callback(GtkWidget *item, void *data) {
  if(GTK_IS_MENU_ITEM(item)) {
    gtk_widget_set_sensitive(item, TRUE);
  }
}

static gboolean on_map_menu(GtkWidget *menu, GdkEventAny *event, void *user_data) {
  // todo: handle tearoff menus
  Application::delay_dynamic_updates(true);
  
  gtk_container_foreach(GTK_CONTAINER(menu), on_map_menu_callback, NULL);
  return FALSE;
}

static gboolean on_unmap_menu(GtkWidget *menu, GdkEventAny *event, void *user_data) {
  Application::delay_dynamic_updates(false);
  
  gtk_container_foreach(GTK_CONTAINER(menu), on_unmap_menu_callback, NULL);
  return FALSE;
}

//{ class MathGtkMenuBuilder ...

MathGtkMenuBuilder MathGtkMenuBuilder::main_menu;

MathGtkMenuBuilder::MathGtkMenuBuilder() {
}

MathGtkMenuBuilder::MathGtkMenuBuilder(Expr _expr)
  : expr(_expr)
{
}

MathGtkMenuBuilder::~MathGtkMenuBuilder() {
}

void MathGtkMenuBuilder::append_to(GtkMenuShell *menu, GtkAccelGroup *accel_group, int for_document_window_id) {
  if(expr[0] != GetSymbol(MenuSymbol)
      || expr.expr_length() != 2)
    return;
    
  String name(expr[1]);
  if(name.is_null())
    return;
    
  Expr list = expr[2];
  if(list[0] != PMATH_SYMBOL_LIST)
    return;
    
  for(size_t i = 1; i <= list.expr_length(); ++i) {
    Expr item = list[i];
    
    if(item[0] == GetSymbol(ItemSymbol)
        && item.expr_length() == 2) {
      String name(item[1]);
      String cmd(item[2]);
      
      if(name.length() > 0 && cmd.is_valid()) {
        GtkWidget *menu_item;
        
        int len;
        char *label = pmath_string_to_utf8(name.get(), &len);
        if(label) {
          for(int i = 0; i < len; ++i) {
            if(label[i] == '&')
              label[i] = '_';
          }
          menu_item = gtk_menu_item_new_with_mnemonic(label);
          pmath_mem_free(label);
        }
        
        String accel_path(accel_path_prefix);
        accel_path += cmd;
        char *accel_path_str = pmath_string_to_utf8(accel_path.get(), NULL);
        if(accel_path_str) {
          gtk_menu_item_set_accel_path(GTK_MENU_ITEM(menu_item), accel_path_str);
          pmath_mem_free(accel_path_str);
        }
        
        g_signal_connect(menu_item, "activate", G_CALLBACK(on_menu_item_activate), (void*)for_document_window_id);
        
        gtk_menu_shell_append(menu, menu_item);
      }
      
      continue;
    }
    
    if(item == GetSymbol(DelimiterSymbol)) {
      GtkWidget *menu_item = gtk_separator_menu_item_new();
      
      gtk_menu_shell_append(menu, menu_item);
      continue;
    }
    
    if(item[0] == GetSymbol(MenuSymbol)
        && item.expr_length() == 2) {
      String name(item[1]);
      
      if(name.length() > 0) {
        GtkWidget *menu_item;
        
        int len;
        char *label = pmath_string_to_utf8(name.get(), &len);
        if(label) {
          for(int i = 0; i < len; ++i) {
            if(label[i] == '&')
              label[i] = '_';
          }
          menu_item = gtk_menu_item_new_with_mnemonic(label);
          pmath_mem_free(label);
        }
        
        //g_signal_connect(menu_item, "activate-item", G_CALLBACK(on_show_menu), NULL);
        
        GtkWidget *submenu = gtk_menu_new();
        
        gtk_widget_add_events(GTK_WIDGET(submenu), GDK_STRUCTURE_MASK);
        
        g_signal_connect(GTK_WIDGET(submenu), "map-event",   G_CALLBACK(on_map_menu),   NULL);
        g_signal_connect(GTK_WIDGET(submenu), "unmap-event", G_CALLBACK(on_unmap_menu), NULL);
        
        gtk_menu_set_accel_group(
          GTK_MENU(submenu),
          GTK_ACCEL_GROUP(accel_group));
          
//        gtk_menu_shell_append(GTK_MENU_SHELL(submenu), gtk_tearoff_menu_item_new());

        MathGtkMenuBuilder(item).append_to(
          GTK_MENU_SHELL(submenu),
          accel_group,
          for_document_window_id);
          
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item), submenu);
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
  if(expr[0] != GetSymbol(KeyEventSymbol)
      || expr.expr_length() != 2)
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
  
  if(key.equals("F1"))                      *accel_key = GDK_KEY_F1;
  else if(key.equals("F2"))                 *accel_key = GDK_KEY_F2;
  else if(key.equals("F3"))                 *accel_key = GDK_KEY_F3;
  else if(key.equals("F4"))                 *accel_key = GDK_KEY_F4;
  else if(key.equals("F5"))                 *accel_key = GDK_KEY_F5;
  else if(key.equals("F6"))                 *accel_key = GDK_KEY_F6;
  else if(key.equals("F7"))                 *accel_key = GDK_KEY_F7;
  else if(key.equals("F8"))                 *accel_key = GDK_KEY_F8;
  else if(key.equals("F9"))                 *accel_key = GDK_KEY_F9;
  else if(key.equals("F10"))                *accel_key = GDK_KEY_F10;
  else if(key.equals("F11"))                *accel_key = GDK_KEY_F11;
  else if(key.equals("F12"))                *accel_key = GDK_KEY_F12;
  else if(key.equals("F13"))                *accel_key = GDK_KEY_F13;
  else if(key.equals("F14"))                *accel_key = GDK_KEY_F14;
  else if(key.equals("F15"))                *accel_key = GDK_KEY_F15;
  else if(key.equals("F16"))                *accel_key = GDK_KEY_F16;
  else if(key.equals("F17"))                *accel_key = GDK_KEY_F17;
  else if(key.equals("F18"))                *accel_key = GDK_KEY_F18;
  else if(key.equals("F19"))                *accel_key = GDK_KEY_F19;
  else if(key.equals("F20"))                *accel_key = GDK_KEY_F20;
  else if(key.equals("F21"))                *accel_key = GDK_KEY_F21;
  else if(key.equals("F22"))                *accel_key = GDK_KEY_F22;
  else if(key.equals("F23"))                *accel_key = GDK_KEY_F23;
  else if(key.equals("F24"))                *accel_key = GDK_KEY_F24;
  else if(key.equals("Enter"))              *accel_key = GDK_KEY_Return;
  else if(key.equals("Tab"))                *accel_key = GDK_KEY_Tab;
  else if(key.equals("Esc"))                *accel_key = GDK_KEY_Escape;
  else if(key.equals("PageUp"))             *accel_key = GDK_KEY_Page_Up;
  else if(key.equals("PageDown"))           *accel_key = GDK_KEY_Page_Down;
  else if(key.equals("End"))                *accel_key = GDK_KEY_End;
  else if(key.equals("Home"))               *accel_key = GDK_KEY_Home;
  else if(key.equals("Left"))               *accel_key = GDK_KEY_Left;
  else if(key.equals("Up"))                 *accel_key = GDK_KEY_Up;
  else if(key.equals("Right"))              *accel_key = GDK_KEY_Right;
  else if(key.equals("Down"))               *accel_key = GDK_KEY_Down;
  else if(key.equals("Insert"))             *accel_key = GDK_KEY_Insert;
  else if(key.equals("Delete"))             *accel_key = GDK_KEY_Delete;
  else if(key.equals("Numpad0"))            *accel_key = GDK_KEY_0;
  else if(key.equals("Numpad1"))            *accel_key = GDK_KEY_1;
  else if(key.equals("Numpad2"))            *accel_key = GDK_KEY_2;
  else if(key.equals("Numpad3"))            *accel_key = GDK_KEY_3;
  else if(key.equals("Numpad4"))            *accel_key = GDK_KEY_4;
  else if(key.equals("Numpad5"))            *accel_key = GDK_KEY_5;
  else if(key.equals("Numpad6"))            *accel_key = GDK_KEY_6;
  else if(key.equals("Numpad7"))            *accel_key = GDK_KEY_7;
  else if(key.equals("Numpad8"))            *accel_key = GDK_KEY_8;
  else if(key.equals("Numpad9"))            *accel_key = GDK_KEY_9;
  else if(key.equals("Play"))               *accel_key = GDK_KEY_AudioPlay;
  else if(key.equals("Zoom"))               *accel_key = GDK_KEY_ZoomIn;
  else                                      return false;
  
  return true;
}

void MathGtkAccelerators::load(Expr expr) {
  if(expr[0] != PMATH_SYMBOL_LIST)
    return;
    
  for(size_t i = 1; i <= expr.expr_length(); ++i) {
    Expr item = expr[i];
    String cmd(item[2]);
    
    guint           accel_key;
    GdkModifierType accel_mod;
    
    if(item[0] == GetSymbol(ItemSymbol)
        && item.expr_length() == 2
        && cmd.length() > 0
        && set_accel_key(item[1], &accel_key, &accel_mod)
        && gtk_accelerator_valid(accel_key, accel_mod)) {
      String accel_path(accel_path_prefix);
      accel_path += cmd;
      
      while(true) {
        char *str = pmath_string_to_utf8(accel_path.get(), NULL);
        if(!str)
          break;
          
        if(gtk_accel_map_lookup_entry(str, 0)) {
          pmath_mem_free(str);
          accel_path += " ";
        }
        else {
          gtk_accel_map_add_entry(str, accel_key, accel_mod);
          all_accelerators.add(accel_path);
          //gtk_accel_group_connect_by_path(str, str, 0);
          pmath_mem_free(str);
          break;
        }
      }
    }
    else
      pmath_debug_print_object("Cannot add shortcut ", item.get(), ".\n");
  }
}

struct accel_data {
  char *path;
  int   document_id;
};

static void accel_data_destroy(void *data, GClosure *closure) {
  struct accel_data *accel_data = (struct accel_data*)data;
  
  pmath_mem_free(accel_data->path);
  pmath_mem_free(accel_data);
}

static gboolean closure_callback(
  GtkAccelGroup   *group,
  GObject         *acceleratable,
  guint            keyval,
  GdkModifierType  modifier,
  void            *user_data
) {
  struct accel_data *accel_data = (struct accel_data*)user_data;
  
  String cmd(accel_data->path);
  if(cmd.starts_with(accel_path_prefix)) {
    cmd = cmd.part(sizeof(accel_path_prefix) - 1, -1);
    
    Document *doc = dynamic_cast<Document*>(Box::find(accel_data->document_id));
    if(doc) {
      BasicGtkWidget *wid = dynamic_cast<BasicGtkWidget*>(doc->native());
      
      while(wid) {
        MathGtkDocumentWindow *win = dynamic_cast<MathGtkDocumentWindow*>(wid);
        if(win) {
          win->run_menucommand(cmd);
          return TRUE;
        }
        
        wid = wid->parent();
      }
    }
    
    g_warning("no MathGtkDocumentWindow parent found.");
    
    Application::run_menucommand(cmd);
  }
  
  return TRUE;
}

void MathGtkAccelerators::connect_all(GtkAccelGroup *accel_group, int document_id) {
  for(int i = 0; i < all_accelerators.length(); ++i) {
    char *path = pmath_string_to_utf8(all_accelerators[i].get_as_string(), 0);
    struct accel_data *accel_data = (struct accel_data*)pmath_mem_alloc(sizeof(struct accel_data));
    
    if(path && accel_data) {
      accel_data->document_id = document_id;
      accel_data->path = path;
      
      gtk_accel_group_connect_by_path(
        accel_group,
        path,
        g_cclosure_new(
          G_CALLBACK(closure_callback),
          accel_data,
          accel_data_destroy));
    }
    else {
      pmath_mem_free(path);
      pmath_mem_free(accel_data);
    }
  }
}

//} ... class MathGtkAccelerators
