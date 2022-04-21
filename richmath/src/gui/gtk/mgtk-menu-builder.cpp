#include <gui/gtk/mgtk-menu-builder.h>

#include <eval/application.h>
#include <eval/binding.h>
#include <eval/interpolation.h>
#include <eval/observable.h>

#include <graphics/canvas.h>

#include <gui/gtk/mgtk-control-painter.h>
#include <gui/gtk/mgtk-document-window.h>
#include <gui/documents.h>
#include <gui/menus.h>

#include <util/autovaluereset.h>
#include <util/hashtable.h>

#if GTK_MAJOR_VERSION >= 3
#  include <gdk/gdkkeysyms-compat.h>
#else
#  include <gdk/gdkkeysyms.h>
#endif

#include <cmath>
#include <limits>

#ifdef _MSC_VER
namespace std {
  static bool isnan(double d) {return _isnan(d);}
}
#endif

#ifndef NAN
#  define NAN  (std::numeric_limits<double>::quiet_NaN())
#endif


using namespace richmath;


namespace richmath { namespace strings {
  extern String EmptyString;
  extern String MenuListSearchCommands;
  extern String SearchMenuItems;
}}

extern pmath_symbol_t richmath_System_Delimiter;
extern pmath_symbol_t richmath_System_Document;
extern pmath_symbol_t richmath_System_Inherited;
extern pmath_symbol_t richmath_System_List;
extern pmath_symbol_t richmath_System_Menu;
extern pmath_symbol_t richmath_System_MenuItem;
extern pmath_symbol_t richmath_System_Section;

extern pmath_symbol_t richmath_FE_Private_SubStringMatch;

static const char accel_path_prefix[] = "<Richmath>/";
static Hashtable<String,  Expr> accel_path_to_cmd;
static Hashtable<Expr, String>  cmd_to_accel_path;

static bool ignore_activate_signal = false;

const uint16_t MenuLevelSeparatorChar = 0x25B8; // PMATH_CHAR_RULE


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
      
      static Expr get_command(GtkMenuItem *menu_item);
      
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
  
  class MathGtkMenuSearch {
    public:
      static void ensure_init();
      
      static GtkWidget *create(FrontEndReference doc_id);
      static void init_item(GtkMenuItem *menu_item);
      
      static bool contains_search_menu_list(GtkMenuShell *menu);
      
      static bool on_menu_key_press(GdkEventKey *event, GtkWidget *menu_item, FrontEndReference doc_id);
      static bool on_menu_key_release(GdkEventKey *event, GtkWidget *menu_item, FrontEndReference doc_id);
      
      static void on_menu_unmap(GtkWidget *menu_item);
      
      static GtkWidget *get_entry(GtkWidget *menu_item);
      
    private:
      static void on_text_changed(GtkEditable *entry, void *doc_id_as_ptr);
      static void select_next_group(GtkMenuShell *menu, LogicalDirection direction);
      
      static bool do_open_help_menu(Expr cmd);
  };
  
  class MathGtkMenuSliderRegion {
    public:
      void paint(GtkWidget *menu, Canvas &canvas, FrontEndReference id);
      
      void get_allocation(GtkAllocation *rect);
      
      bool on_menu_button_press(GtkWidget *menu, GdkEvent *e, FrontEndReference id);
      bool on_menu_button_release(GtkWidget *menu, GdkEvent *e, FrontEndReference id);
      bool on_menu_motion_notify(GtkWidget *menu, GdkEvent *e, FrontEndReference id);
      
    private:
      StyledObject *resolve_scope(Document *doc);
      
      static bool try_get_rhs_value(Expr cmd, float *value);
      bool collect_float_values(Array<float> &values);
      
      bool value_index_from_point(int y, int *index, float *rel_offset);
      void apply_slider_pos(GtkWidget *menu, FrontEndReference id, int index, float rel_offset);
      
    public:
      Expr scope;
      Expr lhs;
      Array<GtkWidget*> menu_items;
  };
  
  class MathGtkMenuGutterSliders : public Base {
    public:
      MathGtkMenuGutterSliders();
      
    public:
      static void connect(GtkMenu *menu, FrontEndReference doc_id);
      
      static void menu_gutter_sliders(GtkWidget *menu, MathGtkMenuGutterSliders *gutters);
      static MathGtkMenuGutterSliders *menu_gutter_sliders(GtkWidget *menu);
      
      bool on_menu_button_press(GtkWidget *menu, GdkEvent *e, FrontEndReference id);
      bool on_menu_button_release(GtkWidget *menu, GdkEvent *e, FrontEndReference id);
      bool on_menu_motion_notify(GtkWidget *menu, GdkEvent *e, FrontEndReference id);
      
    private:
      static gboolean menu_expose_callback(GtkWidget *menu, GdkEvent *e, void *eval_box_id_as_ptr);
      static gboolean menu_draw_callback(  GtkWidget *menu, cairo_t *cr, void *eval_box_id_as_ptr);
      void on_menu_draw(GtkWidget *menu, Canvas &canvas, FrontEndReference id);
    
    public:
      Array<MathGtkMenuSliderRegion> regions;
      int grabbed_region_index;
      
    private:
      static const char *data_key;
  };
}

namespace richmath {
  class MathGtkMenuBuilder::Impl {
    public:
      static gboolean on_map_menu(           GtkWidget *menu, GdkEventAny *event, void *eval_box_id_as_ptr);
      static gboolean on_unmap_menu(         GtkWidget *menu, GdkEventAny *event, void *eval_box_id_as_ptr);
      static gboolean on_menu_key_press(     GtkWidget *menu, GdkEvent *e, void *eval_box_id_as_ptr);
      static gboolean on_menu_key_release(   GtkWidget *menu, GdkEvent *e, void *eval_box_id_as_ptr);
      static gboolean on_menu_button_press(  GtkWidget *menu, GdkEvent *e, void *eval_box_id_as_ptr);
      static gboolean on_menu_button_release(GtkWidget *menu, GdkEvent *e, void *eval_box_id_as_ptr);
      static gboolean on_menu_motion_notify( GtkWidget *menu, GdkEvent *e, void *eval_box_id_as_ptr);
  };
}

//{ class MathGtkMenuBuilder ...

MathGtkMenuBuilder MathGtkMenuBuilder::main_menu;

MathGtkMenuBuilder::MathGtkMenuBuilder() {
}

MathGtkMenuBuilder::MathGtkMenuBuilder(Expr _expr)
  : expr(_expr)
{
  MathGtkMenuSearch::ensure_init();
}

void MathGtkMenuBuilder::done() {
  main_menu = MathGtkMenuBuilder();
  
  accel_path_to_cmd.clear();
  cmd_to_accel_path.clear();
}

void MathGtkMenuBuilder::connect_events(GtkMenu *menu, FrontEndReference doc_id) {
  gtk_widget_add_events(GTK_WIDGET(menu), GDK_STRUCTURE_MASK);
  
  MathGtkMenuGutterSliders::connect(menu, doc_id);
  
  g_signal_connect(menu, "map-event",            G_CALLBACK(Impl::on_map_menu),            FrontEndReference::unsafe_cast_to_pointer(doc_id));
  g_signal_connect(menu, "unmap-event",          G_CALLBACK(Impl::on_unmap_menu),          FrontEndReference::unsafe_cast_to_pointer(doc_id));
  g_signal_connect(menu, "key-press-event",      G_CALLBACK(Impl::on_menu_key_press),      FrontEndReference::unsafe_cast_to_pointer(doc_id));
  g_signal_connect(menu, "key-release-event",    G_CALLBACK(Impl::on_menu_key_release),    FrontEndReference::unsafe_cast_to_pointer(doc_id));
  g_signal_connect(menu, "button-press-event",   G_CALLBACK(Impl::on_menu_button_press),   FrontEndReference::unsafe_cast_to_pointer(doc_id));
  g_signal_connect(menu, "button-release-event", G_CALLBACK(Impl::on_menu_button_release), FrontEndReference::unsafe_cast_to_pointer(doc_id));
  g_signal_connect(menu, "motion-notify-event",  G_CALLBACK(Impl::on_menu_motion_notify),  FrontEndReference::unsafe_cast_to_pointer(doc_id));
}

void MathGtkMenuBuilder::expand_inline_lists(GtkMenu *menu, FrontEndReference id) {
  AutoValueReset<Document*> auto_menu_redirect{ Menus::current_document_redirect };
  
  if(auto box = FrontEndObject::find_cast<Box>(id))
    Menus::current_document_redirect = box->find_parent<Document>(true);
  
  GtkAccelGroup *accel_group = gtk_menu_get_accel_group(menu);
  
  Array<GtkWidget*> old_menu_items;
  BasicGtkWidget::container_foreach(
    GTK_CONTAINER(menu), 
    [&](GtkWidget *item) {
      old_menu_items.add(item);
    });
    
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
      
      if(item_list[0] == richmath_System_List) {
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
  
  BasicGtkWidget::container_foreach(
    GTK_CONTAINER(menu), 
    [&](GtkWidget *menu_item) {
      if(!GTK_IS_MENU_ITEM(menu_item))
        return;
      if(Expr cmd = MenuItemBuilder::get_command(GTK_MENU_ITEM(menu_item))) {
        MenuCommandStatus status = Menus::test_command_status(cmd);
        gtk_widget_set_sensitive(menu_item, status.enabled);
          
        if(GTK_IS_CHECK_MENU_ITEM(menu_item)) {
          // emits "activate" signal if toggled
          ignore_activate_signal = true;
          gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu_item), status.checked);
          ignore_activate_signal = false;
        }
        else if(status.checked) {
          pmath_debug_print_object("[cannot display checked status for menu item: ", cmd.get(), "]\n");
        }
      }
    });
  
  MathGtkMenuGutterSliders *gutter_sliders = MathGtkMenuGutterSliders::menu_gutter_sliders(GTK_WIDGET(menu));
  if(gutter_sliders)
    gutter_sliders->regions.length(0);
  
  MathGtkMenuSliderRegion region;
  
  bool has_gutter_sliders = false;
  BasicGtkWidget::container_foreach(
    GTK_CONTAINER(menu), 
    [&](GtkWidget *menu_item) {
      if(!GTK_IS_MENU_ITEM(menu_item))
        return;
      
      if(!gtk_widget_is_drawable(menu_item))
        return;
      
      Expr scope;
      Expr lhs;
      if(Expr cmd = MenuItemBuilder::get_command(GTK_MENU_ITEM(menu_item))) {
        if(cmd[0] == richmath_FE_ScopedCommand) {
          scope = cmd[2];
          cmd = cmd[1];
        }
        
        if(cmd.is_rule()) {
          lhs = cmd[1];
          
          Expr rhs = cmd[2];
          if(!rhs.is_number() && rhs != richmath_System_Inherited) {
            // Inherited is used in Magnification->Inherited for 100%
            lhs = Expr();
          }
        }
      }
      
      if(region.menu_items.length() > 0) {
        if(region.lhs != lhs || region.scope != scope) {
          if(region.menu_items.length() > 1) {
            if(!gutter_sliders)
              gutter_sliders = new MathGtkMenuGutterSliders();
            
            gutter_sliders->regions.add(std::move(region));
          }
          
          region = MathGtkMenuSliderRegion();
        }
      }
      
      if(lhs) {
        if(region.menu_items.length() == 0) {
          region.lhs = lhs;
          region.scope = scope;
        }
        
        region.menu_items.add(menu_item);
      }
    });
  
  if(region.menu_items.length() > 1) {
    if(!gutter_sliders)
      gutter_sliders = new MathGtkMenuGutterSliders();
   
    gutter_sliders->regions.add(std::move(region));
  }
  
  MathGtkMenuGutterSliders::menu_gutter_sliders(GTK_WIDGET(menu), gutter_sliders);
}

void MathGtkMenuBuilder::collect_menu_matches(Array<MenuSearchResult> &results, String query, GtkMenuShell *menu, String prefix, FrontEndReference doc_id) {
  if(MathGtkMenuSearch::contains_search_menu_list(menu))
    return;
  
  if(GTK_IS_MENU(menu))
    expand_inline_lists(GTK_MENU(menu), doc_id);
  
  BasicGtkWidget::container_foreach(
    GTK_CONTAINER(menu), 
    [&](GtkWidget *menu_item) {
      if(GTK_IS_MENU_ITEM(menu_item) && !GTK_IS_SEPARATOR_MENU_ITEM(menu_item)) {
        const char *utf8 = gtk_menu_item_get_label(GTK_MENU_ITEM(menu_item));
        
        String label = prefix + String::FromUtf8(utf8);
        if(gtk_menu_item_get_use_underline(GTK_MENU_ITEM(menu_item))) {
          int final_len = 0;
          label.edit([&](uint16_t *buf, int len) {
            int i = prefix.length();
            int j = i;
            for(; i < len; ++i) {
              if(buf[i] != '_')
                buf[j++] = buf[i];
            }
            final_len = j;
          });
          label = label.part(0, final_len);
        }
        
        if(GtkWidget *sub = gtk_menu_item_get_submenu(GTK_MENU_ITEM(menu_item)))
          collect_menu_matches(results, query, GTK_MENU_SHELL(sub), label + String::FromChar(MenuLevelSeparatorChar), doc_id);
        
        if(Expr cmd = MenuItemBuilder::get_command(GTK_MENU_ITEM(menu_item))) {
          Expr matches = Evaluate(Call(Symbol(richmath_FE_Private_SubStringMatch), label, query));
          if(matches.is_int32()) {
            int num_matches = PMATH_AS_INT32(matches.get());
            if(num_matches > 0) {
              results.add(
                MenuSearchResult{
                  num_matches, 
                  results.length(), 
                  Call(Symbol(richmath_System_MenuItem), std::move(label), std::move(cmd))});
            }
          }
        }
      }
    });
}

void MathGtkMenuBuilder::append_to(GtkMenuShell *menu, GtkAccelGroup *accel_group, FrontEndReference evalution_box_id) {
  if(expr[0] != richmath_System_Menu || expr.expr_length() != 2)
    return;
    
  if(!expr[1].is_string())
    return;
    
  Expr list = expr[2];
  if(list[0] != richmath_System_List)
    return;
    
  for(size_t i = 1; i <= list.expr_length(); ++i) {
    Expr item = list[i];
    
    MenuItemType type = Menus::menu_item_type(item);
    if(type == MenuItemType::Invalid)
      continue;
    
    GtkWidget *menu_item = MenuItemBuilder::create(type, evalution_box_id);
    gtk_widget_show(menu_item);
    gtk_menu_shell_append(menu, menu_item);
    MenuItemBuilder::init(GTK_MENU_ITEM(menu_item), type, std::move(item), accel_group, evalution_box_id);
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

//{ class MathGtkMenuBuilder::Impl ...

gboolean MathGtkMenuBuilder::Impl::on_map_menu(GtkWidget *menu, GdkEventAny *event, void *doc_id_as_ptr) {
  // todo: handle tearoff menus
  
  FrontEndReference id = FrontEndReference::unsafe_cast_from_pointer(doc_id_as_ptr);
  expand_inline_lists(GTK_MENU(menu), id);
  
  return FALSE;
}

gboolean MathGtkMenuBuilder::Impl::on_unmap_menu(GtkWidget *menu, GdkEventAny *event, void *doc_id_as_ptr) {
  gtk_container_foreach(
    GTK_CONTAINER(menu), 
    [](GtkWidget *item, void*) {      
      if(GTK_IS_MENU_ITEM(item)) {
        if(gtk_menu_item_get_accel_path(GTK_MENU_ITEM(item)))
          gtk_widget_set_sensitive(item, TRUE);
      }
      if(MenuItemBuilder::has_type(item, MenuItemType::SearchBox))
        MathGtkMenuSearch::on_menu_unmap(item);
    }, 
    nullptr);
  return FALSE;
}

gboolean MathGtkMenuBuilder::Impl::on_menu_key_press(GtkWidget *menu, GdkEvent *e, void *doc_id_as_ptr) {
  GdkEventKey *event = &e->key;
  bool handled = false;
  
  BasicGtkWidget::container_foreach(
    GTK_CONTAINER(menu),
    [&](GtkWidget *item) {
      if(handled)
        return;
      if(MenuItemBuilder::has_type(item, MenuItemType::SearchBox)) {
        handled = MathGtkMenuSearch::on_menu_key_press(event, item, FrontEndReference::unsafe_cast_from_pointer(doc_id_as_ptr));
      }
    });
  
  if(handled)
    return TRUE;
  
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
        on_map_menu(menu, nullptr, doc_id_as_ptr);
      else
        gtk_menu_shell_cancel(GTK_MENU_SHELL(menu));
      
      return TRUE;
    }
  }
  
  return FALSE;
}

gboolean MathGtkMenuBuilder::Impl::on_menu_key_release(GtkWidget *menu, GdkEvent *e, void *doc_id_as_ptr) {
  GdkEventKey *event = &e->key;
  bool handled = false;
  
  BasicGtkWidget::container_foreach(
    GTK_CONTAINER(menu), 
    [&](GtkWidget *item) {
      if(handled)
        return;
      if(MenuItemBuilder::has_type(item, MenuItemType::SearchBox)) {
        handled = MathGtkMenuSearch::on_menu_key_release(event, item, FrontEndReference::unsafe_cast_from_pointer(doc_id_as_ptr));
      }
    });
  
  return handled;
}

gboolean MathGtkMenuBuilder::Impl::on_menu_button_press(GtkWidget *menu, GdkEvent *e, void *eval_box_id_as_ptr) {
  if(auto gutter_sliders = MathGtkMenuGutterSliders::menu_gutter_sliders(menu)) {
    if(gutter_sliders->on_menu_button_press(menu, e, FrontEndReference::unsafe_cast_from_pointer(eval_box_id_as_ptr)))
      return true;
  }
  
  GtkWidget *wid = gtk_get_event_widget(e);
  if(GTK_IS_MENU_ITEM(wid)) {
    Expr cmd = MenuItemBuilder::get_command(GTK_MENU_ITEM(wid));
    
    if(cmd == strings::SearchMenuItems)
      return true;
  }
  
  return false;
}

gboolean MathGtkMenuBuilder::Impl::on_menu_button_release(GtkWidget *menu, GdkEvent *e, void *eval_box_id_as_ptr) {
  if(auto gutter_sliders = MathGtkMenuGutterSliders::menu_gutter_sliders(menu)) {
    if(gutter_sliders->on_menu_button_release(menu, e, FrontEndReference::unsafe_cast_from_pointer(eval_box_id_as_ptr)))
      return true;
  }
  
  GtkWidget *wid = gtk_get_event_widget(e);
  if(GTK_IS_MENU_ITEM(wid)) {
    Expr cmd = MenuItemBuilder::get_command(GTK_MENU_ITEM(wid));
    
    if(cmd == strings::SearchMenuItems)
      return true;
  }
  
  return false;
}

gboolean MathGtkMenuBuilder::Impl::on_menu_motion_notify(GtkWidget *menu, GdkEvent *e, void *eval_box_id_as_ptr) {
  if(auto gutter_sliders = MathGtkMenuGutterSliders::menu_gutter_sliders(menu)) {
    if(gutter_sliders->on_menu_motion_notify(menu, e, FrontEndReference::unsafe_cast_from_pointer(eval_box_id_as_ptr)))
      return true;
  }
  
  GtkWidget *wid = gtk_get_event_widget(e);
  if(GTK_IS_MENU_ITEM(wid)) {
    Expr cmd = MenuItemBuilder::get_command(GTK_MENU_ITEM(wid));
    
    if(cmd == strings::SearchMenuItems)
      return true;
  }
  
  return false;
}

//} ... class MathGtkMenuBuilder::Impl

//{ class MenuItemBuilder ...

const char *MenuItemBuilder::inline_menu_list_data_key = "richmath:inline_menu_list_data_key";

ObservableValue<GtkMenuItem*> MenuItemBuilder::_selected_item { nullptr };


bool MenuItemBuilder::has_type(GtkWidget *widget, MenuItemType type) {
  switch(type) {
    case MenuItemType::Normal:
    case MenuItemType::InlineMenu:
    case MenuItemType::SubMenu:
      if( GTK_IS_MENU_ITEM(widget) && 
          !GTK_IS_CHECK_MENU_ITEM(widget) && 
          !GTK_IS_SEPARATOR_MENU_ITEM(widget)) 
      {
        if(get_command(GTK_MENU_ITEM(widget)) == strings::SearchMenuItems)
          return type == MenuItemType::SearchBox;
        else
          return type != MenuItemType::SearchBox;
      }
      else
        return false;
    
    case MenuItemType::SearchBox:
      return GTK_IS_IMAGE_MENU_ITEM(widget);
      
    case MenuItemType::CheckButton:
    case MenuItemType::RadioButton:
      return GTK_IS_CHECK_MENU_ITEM(widget);
    
    case MenuItemType::Delimiter:
      return GTK_IS_SEPARATOR_MENU_ITEM(widget);
    
    case MenuItemType::Invalid:
      return false;
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
    
    case MenuItemType::SearchBox:
      menu_item = MathGtkMenuSearch::create(for_document_window_id);
      break;
      
    case MenuItemType::Invalid:
      return nullptr;
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
      
    case MenuItemType::SearchBox: 
      init_command(menu_item, std::move(item));
      MathGtkMenuSearch::init_item(menu_item);
      break;
      
    case MenuItemType::InlineMenu: 
      init_inline_menu(menu_item, std::move(item));
      break;
      
    case MenuItemType::SubMenu: 
      init_sub_menu(menu_item, std::move(item), accel_group, for_document_window_id);
      break;
    
    case MenuItemType::Invalid:
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

Expr MenuItemBuilder::get_command(GtkMenuItem *menu_item) {
  const char *accel_path_str = (const char *)gtk_menu_item_get_accel_path(menu_item);
  if(!accel_path_str)
    return Expr{};
  
  return accel_path_to_cmd[String(accel_path_str)];
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
    if(GtkWidget *entry = MathGtkMenuSearch::get_entry(GTK_WIDGET(menu_item))) {
#if GTK_CHECK_VERSION(3, 2, 0)
      gtk_entry_set_placeholder_text(GTK_ENTRY(entry), str);
#endif
    }
    else {
      gtk_menu_item_set_use_underline(GTK_MENU_ITEM(menu_item), TRUE);
      gtk_menu_item_set_label(GTK_MENU_ITEM(menu_item), str);
    }
    pmath_mem_free(str);
  }
}

void MenuItemBuilder::init_command(GtkMenuItem *menu_item, Expr item) {
  set_label(menu_item, String(item[1]));
  
  Expr cmd = item[2];
  String accel_path = add_command(cmd);
  if(char *accel_path_str = pmath_string_to_utf8(accel_path.get(), nullptr)) {
    gtk_menu_item_set_accel_path(menu_item, accel_path_str);
    pmath_mem_free(accel_path_str);
  }
}

void MenuItemBuilder::init_sub_menu(GtkMenuItem *menu_item, Expr item, GtkAccelGroup *accel_group, FrontEndReference for_document_window_id) {
  set_label(menu_item, String(item[1]));
  
  GtkWidget *submenu = gtk_menu_new();
//  gtk_menu_shell_append(GTK_MENU_SHELL(submenu), gtk_tearoff_menu_item_new());
  
  MathGtkMenuBuilder::connect_events(GTK_MENU(submenu), for_document_window_id);
  
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

void MenuItemBuilder::on_activate(GtkMenuItem *menu_item, void *eval_box_id_as_ptr) {
  if(ignore_activate_signal)
    return;
  
  FrontEndReference id = FrontEndReference::unsafe_cast_from_pointer(eval_box_id_as_ptr);
  
  if(Expr cmd = get_command(menu_item))
    Application::with_evaluation_box(FrontEndObject::find_cast<Box>(id), [&](){ Menus::run_command_now(cmd); });
}

void MenuItemBuilder::on_select(GtkMenuItem *menu_item, void *eval_box_id_as_ptr) {
  _selected_item = menu_item;
}

void MenuItemBuilder::on_deselect(GtkMenuItem *menu_item, void *eval_box_id_as_ptr) {
  if(_selected_item.unobserved_equals(menu_item))
    _selected_item = nullptr;
}

void MenuItemBuilder::on_destroy(GtkMenuItem *menu_item, void *eval_box_id_as_ptr) {
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
  if(modifiers[0] != richmath_System_List)
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
  if(expr[0] != richmath_System_List)
    return;
    
  for(size_t i = 1; i <= expr.expr_length(); ++i) {
    Expr item = expr[i];
    Expr cmd(item[2]);
    
    guint           accel_key = 0;
    GdkModifierType accel_mod = (GdkModifierType)0;
    
    if( item[0] == richmath_System_MenuItem                &&
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
    FrontEndReference   evaluation_box_id;
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
  
  Application::with_evaluation_box(FrontEndObject::find_cast<Box>(accel_data->evaluation_box_id), [&](){
    Menus::run_command_now(accel_data->cmd);
  });
  
  return TRUE;
}

void MathGtkAccelerators::connect_all(GtkAccelGroup *accel_group, FrontEndReference evaluation_box_id) {
  for(auto accel : all_accelerators) {
    char *path = pmath_string_to_utf8(accel.get_as_string(), 0);
    AccelData *accel_data = new AccelData;
    
    if(path && accel_data) {
      accel_data->evaluation_box_id = evaluation_box_id;
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

//{ class MathGtkMenuSearch ...

void MathGtkMenuSearch::ensure_init() {
  static bool initialized = false;
  if(initialized)
    return;
  
  initialized = true;
  Menus::register_command(strings::SearchMenuItems, do_open_help_menu);
}

GtkWidget *MathGtkMenuSearch::create(FrontEndReference doc_id) {
  GtkWidget *menu_item = gtk_image_menu_item_new();
  
  gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menu_item), gtk_image_new_from_icon_name("edit-find-symbolic", GTK_ICON_SIZE_MENU));
  gtk_image_menu_item_set_always_show_image(GTK_IMAGE_MENU_ITEM(menu_item), true);
  
  if(GtkWidget *label = gtk_bin_get_child(GTK_BIN(menu_item))) {
    gtk_container_remove(GTK_CONTAINER(menu_item), label);
  }
  
  auto entry = gtk_entry_new();
  g_signal_connect(GTK_EDITABLE(entry), "changed", G_CALLBACK(on_text_changed), FrontEndReference::unsafe_cast_to_pointer(doc_id));
  // TODO: on Gtk >= 3.6.0, we could also use a GtkSearchEntry, but have to detect the click on the "clear" icon 
  // ourselved with gtk_entry_get_icon_area() because this entry does not receive focus or mouse events
  
//#if GTK_CHECK_VERSION(3, 2, 0)
//  {
//    auto overlay = gtk_overlay_new();
//    gtk_container_add(GTK_CONTAINER(overlay), entry);
//    auto label = gtk_accel_label_new("???");
//    gtk_accel_label_set_accel_widget(GTK_ACCEL_LABEL(label), menu_item);
//    gtk_accel_label_refetch(GTK_ACCEL_LABEL(label));
//    //gtk_accel_label_set_accel(GTK_ACCEL_LABEL(label), GDK_F1, GDK_CONTROL_MASK);
//    g_object_set(
//      G_OBJECT(label), 
//      "halign", GTK_ALIGN_END,
//      "valign", GTK_ALIGN_CENTER, 
//      nullptr);
//    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), label);
//    entry = overlay;
//  }
//#endif

  gtk_container_add(GTK_CONTAINER(menu_item), entry);
  gtk_widget_show_all(entry);
  
  return menu_item;
}

void MathGtkMenuSearch::init_item(GtkMenuItem *menu_item) {
  if(auto entry = get_entry(GTK_WIDGET(menu_item))) {
    String current_query = Menus::current_menu_search_text();
    if(current_query.length() > 0) {
      int len;
      if(char *utf8 = pmath_string_to_utf8(current_query.get(), &len)) {
        gtk_entry_set_text(GTK_ENTRY(entry), utf8);
        pmath_mem_free(utf8);
      }
    }
    else
      gtk_entry_set_text(GTK_ENTRY(entry), "");
  }
}

bool MathGtkMenuSearch::contains_search_menu_list(GtkMenuShell *menu) {
  bool found = false;
  BasicGtkWidget::container_foreach(
    GTK_CONTAINER(menu), 
    [&](GtkWidget *menu_item) {
      if(!found) {
        if(strings::MenuListSearchCommands == MenuItemBuilder::inline_menu_list_data(menu_item))
          found = true;
      }
    });
  return found;
}

bool MathGtkMenuSearch::on_menu_key_press(GdkEventKey *event, GtkWidget *menu_item, FrontEndReference doc_id) {
  switch(event->keyval) {
    case GDK_Left:
    case GDK_Right:
    case GDK_Up:
    case GDK_Down:
    case GDK_Return:
    case GDK_Escape:
      return false;
    
    case GDK_ISO_Left_Tab: // shift+tab
    case GDK_Tab:
      return true;
  }
  
  if(GtkWidget *entry = get_entry(menu_item)) {
    gboolean ret_val = FALSE;
    g_signal_emit_by_name(entry, "key-press-event", event, &ret_val);
  }
  
  return true;
}

bool MathGtkMenuSearch::on_menu_key_release(GdkEventKey *event, GtkWidget *menu_item, FrontEndReference doc_id) {
  switch(event->keyval) {
    case GDK_Left:
    case GDK_Right:
    case GDK_Up:
    case GDK_Down:
    case GDK_Return:
    case GDK_Escape:
      return false;
    
    case GDK_ISO_Left_Tab: // shift+tab
    case GDK_Tab:
      select_next_group(
        GTK_MENU_SHELL(gtk_widget_get_ancestor(menu_item, GTK_TYPE_MENU_SHELL)), 
        (event->state & GDK_SHIFT_MASK) ? LogicalDirection::Backward : LogicalDirection::Forward);
      return true;
  }
  
  if(GtkWidget *entry = get_entry(menu_item)) {
    gboolean ret_val = FALSE;
    g_signal_emit_by_name(entry, "key-release-event", event, &ret_val);
  }
  
  return true;
}

void MathGtkMenuSearch::on_menu_unmap(GtkWidget *menu_item) {
  Menus::current_menu_search_text(String());
}

void MathGtkMenuSearch::on_text_changed(GtkEditable *entry, void *doc_id_as_ptr) {
  char *txt = gtk_editable_get_chars(entry, 0, -1);
  Menus::current_menu_search_text(String::FromUtf8(txt));
  g_free(txt);
  
  if(GtkWidget *menu_item = gtk_widget_get_ancestor(GTK_WIDGET(entry), GTK_TYPE_MENU_ITEM)) {
    if(GtkWidget *menu = gtk_widget_get_ancestor(menu_item, GTK_TYPE_MENU)) { // gtk_widget_get_parent(menu_item)
      MathGtkMenuBuilder::expand_inline_lists(GTK_MENU(menu), FrontEndReference::unsafe_cast_from_pointer(doc_id_as_ptr));
      
      GtkWidget *next_sel_item = nullptr;
      bool sel_next = false;
      
      BasicGtkWidget::container_foreach(
        GTK_CONTAINER(menu),
        [&](GtkWidget *child) {
          if(!GTK_IS_MENU_ITEM(child))
            return;
            
          if(child == menu_item) {
            sel_next = true;
          }
          else if(sel_next) {
            if(gtk_widget_is_sensitive(child)) {
              next_sel_item = child;
              sel_next = false;
            }
          }
        });
      
      if(next_sel_item) {
        //gtk_menu_set_active(GTK_MENU(menu), ctx.found_index + 1);
        gtk_menu_shell_select_item(GTK_MENU_SHELL(menu), next_sel_item);
      }
    }
  }
}

void MathGtkMenuSearch::select_next_group(GtkMenuShell *menu, LogicalDirection direction) {
#if GTK_MAJOR_VERSION < 3
  GtkWidget *current = GTK_WIDGET(MenuItemBuilder::selected_item());// GTK_IS_MENU(menu) ? gtk_menu_get_active(GTK_MENU(menu)) : nullptr;
#else
  GtkWidget *current = gtk_menu_shell_get_selected_item(menu);
#endif
  GtkWidget *first_group_item = nullptr;
  GtkWidget *last_group_item_before = nullptr;
  GtkWidget *first_group_item_after = nullptr;
  GtkWidget *last_group_item = nullptr;
  bool have_group_start = true;
  bool found_current_selection = false;
  
  BasicGtkWidget::container_foreach(
    GTK_CONTAINER(menu),
    [&](GtkWidget *child) {
      if(!gtk_widget_is_drawable(child)) 
        return;
      
      if(GTK_IS_SEPARATOR_MENU_ITEM(child)) {
        have_group_start = true;
        return;
      }
      
      if(!GTK_IS_MENU_ITEM(child))
        return;
      
      if(!gtk_widget_is_sensitive(child) || get_entry(child)) 
        return;
      
      if(child == current) {
        found_current_selection = true;
        have_group_start = false;
        return;
      }
      
      if(!have_group_start)
        return;
      
      have_group_start = false;
      
      last_group_item = child;
      if(!first_group_item) {
        first_group_item = child;
      }
      
      if(found_current_selection) {
        if(!first_group_item_after) {
          first_group_item_after = child;
        }
      }
      else {
        last_group_item_before = child;
      }
    });
  
  if(!first_group_item_after) first_group_item_after = first_group_item;
  if(!last_group_item_before) last_group_item_before = last_group_item;
    
  if(direction == LogicalDirection::Forward) {
    if(first_group_item_after)
      gtk_menu_shell_select_item(menu, first_group_item_after);
  }
  else if(last_group_item_before)
    gtk_menu_shell_select_item(menu, last_group_item_before);
}

GtkWidget *MathGtkMenuSearch::get_entry(GtkWidget *menu_item) {
  GtkWidget *entry = nullptr;
  gtk_container_foreach(
    GTK_CONTAINER(menu_item),
    [](GtkWidget *child, void *_entry_ptr) {
      GtkWidget **entry_ptr = (GtkWidget**)_entry_ptr;
      if(*entry_ptr)
        return;
      
      if(GTK_IS_ENTRY(child)) {
        *entry_ptr = child;
        return;
      }
      
      if(GTK_IS_CONTAINER(child)) {
        gtk_container_foreach(
          GTK_CONTAINER(child),
          [](GtkWidget *grandchild, void *_entry_ptr) {
            GtkWidget **entry_ptr = (GtkWidget**)_entry_ptr;
            if(*entry_ptr)
              return;
            
            if(GTK_IS_ENTRY(grandchild)) {
              *entry_ptr = grandchild;
              return;
            }
          },
          _entry_ptr);
      }
    },
    & entry);
  
  return entry;
}

bool MathGtkMenuSearch::do_open_help_menu(Expr cmd) {
  auto doc = Menus::current_document();
  if(!doc)
    return false;
    
  auto wid = dynamic_cast<MathGtkWidget*>(doc->native());
  if(!wid)
    return false;
  
  GtkWidget *toplevel = gtk_widget_get_toplevel(wid->widget());
  auto win = dynamic_cast<MathGtkDocumentWindow*>(BasicGtkWidget::from_widget(toplevel));
  if(!win)
    return false;
  
  bool found = false;
  Menus::current_menu_search_text(strings::EmptyString);
  BasicGtkWidget::container_foreach(
    GTK_CONTAINER(win->menubar()),
    [&](GtkWidget *child) {
      if(found || !GTK_IS_MENU_ITEM(child))
        return;
      
      GtkWidget *sub = gtk_menu_item_get_submenu(GTK_MENU_ITEM(child));
      if(!sub)
        return;
      
      if(MathGtkMenuSearch::contains_search_menu_list(GTK_MENU_SHELL(sub))) {
        /* gtk_menu_shell_activate_item() works on Windows (Gtk2),
           but not on Linux (WSL, Xming, Gtk2 and Gtk3).
           Emitting "activate-current" works on both systems.
         */
        
        gtk_widget_grab_focus(win->menubar());
        gtk_menu_shell_select_item(GTK_MENU_SHELL(win->menubar()), child);
        //gtk_menu_shell_activate_item(GTK_MENU_SHELL(win->menubar()), child, false);
        g_signal_emit_by_name(GTK_MENU_SHELL(win->menubar()), "activate-current", FALSE);
        found = true;
        return;
      }
      
      // Maybe the help menu is inside the overflow menu. Dig one level deeper:
      BasicGtkWidget::container_foreach(
        GTK_CONTAINER(sub),
        [&](GtkWidget *grandchild) {
          if(found || !GTK_IS_MENU_ITEM(grandchild))
            return;
            
          GtkWidget *subsub = gtk_menu_item_get_submenu(GTK_MENU_ITEM(grandchild));
          if(!subsub)
            return;
          
          if(MathGtkMenuSearch::contains_search_menu_list(GTK_MENU_SHELL(subsub))) {
            gtk_widget_grab_focus(win->menubar());
            gtk_menu_shell_select_item(GTK_MENU_SHELL(win->menubar()), child);
            g_signal_emit_by_name(     GTK_MENU_SHELL(win->menubar()), "activate-current", FALSE);
            gtk_menu_shell_select_item(GTK_MENU_SHELL(sub), grandchild);
            g_signal_emit_by_name(     GTK_MENU_SHELL(sub), "activate-current", FALSE);
            found = true;
          }
        });
    });
  
  return true;
}

//} ... class MathGtkMenuSearch

//{ class MathGtkMenuSliderRegion ...

void MathGtkMenuSliderRegion::paint(GtkWidget *menu, Canvas &canvas, FrontEndReference id) {
  if(menu_items.length() == 0)
    return;
  
  GtkAllocation rect;
  get_allocation(&rect);
  
  canvas.save();
  {
    #if GTK_MAJOR_VERSION >= 3
    {
      GtkStyleContext *style_ctx = gtk_widget_get_style_context(menu);
      GtkBorder padding = {0,0,0,0};
      gtk_style_context_get_padding(style_ctx, GTK_STATE_FLAG_NORMAL, &padding);
      
      canvas.translate(padding.left, padding.top);
    }
    #endif
    
    auto doc = FrontEndObject::find_cast<Document>(id);
    NativeWidget *doc_window = doc ? doc->native() : NativeWidget::dummy;
    
    struct DummyContext : public ControlContext {
      virtual bool is_foreground_window() override { return true; }
      virtual bool is_focused_widget() override {    return true; }
      virtual bool is_using_dark_mode() override {   return doc_window->is_using_dark_mode(); }
      virtual int dpi() override {                   return doc_window->dpi(); }
      
      NativeWidget *doc_window;
      
      DummyContext(NativeWidget *doc_window) : doc_window{doc_window} {}
    } ctx {doc_window};
    
    #if GTK_MAJOR_VERSION >= 3
    {
      //gtk_style_apply_default_background(gtk_widget_get_style(menu), )
      GtkStyleContext *style_ctx = gtk_widget_get_style_context(menu);
      gtk_render_background(style_ctx, canvas.cairo(), rect.x, rect.y, rect.width, rect.height);
    }
    #endif
    
    RectangleF gutter_rect(rect.x, rect.y, rect.width, rect.height);
    
    const ContainerType thumb = ContainerType::VerticalSliderRightArrowButton;
    BoxSize thumb_extents{gutter_rect.width - 4, gutter_rect.height - 4, 0.0f};
    MathGtkControlPainter::gtk_painter.calc_container_size(ctx, canvas, thumb, &thumb_extents);
    
//    canvas.translate(rect.x, rect.y);
//    canvas.scale(0.75, 0.75);
//    RectangleF gutter_rect(Point(0, 0), Vector2F(rect.width, rect.height) / 0.75);

    float y1 = rect.y;
    float y2 = rect.y + rect.height;
    
    GtkAllocation item_rect{0,0,0,0};
    
    gtk_widget_get_allocation(menu_items[0], &item_rect);
    y1 = y1 + (item_rect.height - thumb_extents.height()) / 2;
    
    gtk_widget_get_allocation(menu_items[menu_items.length() - 1], &item_rect);
    y2 = y2 - (item_rect.height - thumb_extents.height()) / 2;
    
    RectangleF channel_rect(rect.x + rect.width/2 - 2, y1, 4, y2 - y1);
    
    MathGtkControlPainter::gtk_painter.draw_container(
      ctx, canvas, ContainerType::VerticalSliderChannel, ControlState::Normal, channel_rect);
    
    if(auto obj = resolve_scope(doc)) {
      StyleOptionName key = Style::get_key(lhs);
      StyleType type = Style::get_type(key);
      if(type == StyleType::Number) {
        float val = obj->get_style((FloatStyleOptionName)key, NAN);
        Array<float> values;
        if(!std::isnan(val) && collect_float_values(values)) {
          float rel_idx = Interpolation::interpolation_index(values, val, false);
          if(0 <= rel_idx && rel_idx <= values.length() - 1) {
            int index_before = (int)rel_idx;
            
            gtk_widget_get_allocation(menu_items[index_before], &item_rect);
            
            float thumb_center_y = item_rect.y + 0.5f * item_rect.height + (rel_idx - index_before) * item_rect.height;
            
            MathGtkControlPainter::gtk_painter.draw_container(
              ctx, canvas, thumb, ControlState::Normal, 
              RectangleF(
                rect.x + 0.5f * (gutter_rect.width - thumb_extents.width), 
                thumb_center_y - 0.5f * thumb_extents.height(), 
                thumb_extents.width, 
                thumb_extents.height()));
          }
        }
      }
    }
  }
  canvas.restore();
}

void MathGtkMenuSliderRegion::get_allocation(GtkAllocation *rect) {
  if(menu_items.length() == 0) {
    *rect = {0,0,0,0};
    return;
  }
  
  gtk_widget_get_allocation(menu_items[0], rect);
  
  GtkAllocation end_rect;
  gtk_widget_get_allocation(menu_items[menu_items.length() - 1], &end_rect);
  
  rect->height = end_rect.y + end_rect.height - rect->y;
  
  GtkAllocation label_rect = {0,0,0,0};
  gtk_widget_get_allocation(gtk_bin_get_child(GTK_BIN(menu_items[0])), &label_rect);
  
  rect->width = label_rect.x;
  
  rect->x     += 1;
  rect->width -= 2;
  rect->y     += 1;
  rect->height-= 2;
}

bool MathGtkMenuSliderRegion::on_menu_button_press(GtkWidget *menu, GdkEvent *e, FrontEndReference id) {
  auto event = &e->button;
  
  GtkWidget *wid = gtk_get_event_widget(e);
  if(!GTK_IS_MENU_ITEM(wid))
    return true;
  
  GtkAllocation wid_rect;
  gtk_widget_get_allocation(wid, &wid_rect);
  
  int y = event->y + wid_rect.y;
  
  int index;
  float rel_offset;
  if(value_index_from_point(y, &index, &rel_offset)) {
    if((event->state & GDK_SHIFT_MASK) == 0) {
      if(fabsf(rel_offset) < 0.25f)
        rel_offset = 0.0f;
    }
  
    apply_slider_pos(menu, id, index, rel_offset);
  }
  
  return true;
}

bool MathGtkMenuSliderRegion::on_menu_button_release(GtkWidget *menu, GdkEvent *e, FrontEndReference id) {
  auto event = &e->button;
  
  GtkWidget *wid = gtk_get_event_widget(e);
  if(!GTK_IS_MENU_ITEM(wid))
    return true;
  
  GtkAllocation wid_rect;
  gtk_widget_get_allocation(wid, &wid_rect);
  
  int y = event->y + wid_rect.y;
  
  int index;
  float rel_offset;
  if(value_index_from_point(y, &index, &rel_offset)) {
    if((event->state & GDK_SHIFT_MASK) == 0) {
      if(fabsf(rel_offset) < 0.25f)
        rel_offset = 0.0f;
    }
    
    apply_slider_pos(menu, id, index, rel_offset);
  }
  
  return true;
}

bool MathGtkMenuSliderRegion::on_menu_motion_notify(GtkWidget *menu, GdkEvent *e, FrontEndReference id) {
  auto event = &e->motion;
  
  if((event->state & GDK_BUTTON1_MASK) == 0)
    return false;
  
  GtkWidget *wid = gtk_get_event_widget(e);
  if(!GTK_IS_MENU_ITEM(wid))
    return true;
  
  GtkAllocation wid_rect;
  gtk_widget_get_allocation(wid, &wid_rect);
  
  int y = event->y + wid_rect.y;
  
  int index;
  float rel_offset;
  if(value_index_from_point(y, &index, &rel_offset)) {
    if((event->state & GDK_SHIFT_MASK) == 0) {
      if(fabsf(rel_offset) < 0.25f)
        rel_offset = 0.0f;
    }
  
    apply_slider_pos(menu, id, index, rel_offset);
  }
  
  return true;
}

StyledObject *MathGtkMenuSliderRegion::resolve_scope(Document *doc) {
  // same as Win32MenuGutterSlider::Impl::resolve_scope()

  if(scope == richmath_System_Document) 
    return doc;
  
  if(!doc)
    return nullptr;
  
  if(Box *sel = doc->selection_box()) {
    if(scope == richmath_System_Section)
      return sel->find_parent<Section>(true);
    else
      return sel;
  }
  
  return nullptr;
}

bool MathGtkMenuSliderRegion::try_get_rhs_value(Expr cmd, float *value) {
  *value = NAN;
  
  if(cmd[0] == richmath_FE_ScopedCommand)
    cmd = cmd[1];

  if(!cmd.is_rule())
    return false;
  
  Expr rhs = cmd[2];
  if(rhs.is_number()) {
    *value = rhs.to_double();
    return true;
  }
  
  if(rhs == richmath_System_Inherited) {
    // Magnification -> Inherited
    *value = 1;
    return true;
  }
  
  return false;
}

bool MathGtkMenuSliderRegion::collect_float_values(Array<float> &values) {
  // similar to Win32MenuGutterSlider::collect_float_values()

  values.length(menu_items.length());
  for(int i = 0; i < menu_items.length(); ++i) {
    float val;
    if(!try_get_rhs_value(MenuItemBuilder::get_command(GTK_MENU_ITEM(menu_items[i])), &val))
      return false;
      
    values[i] = val;
  }
  
  return true;
}

bool MathGtkMenuSliderRegion::value_index_from_point(int y, int *index, float *rel_offset) {
  int previous_bottom = 0;
  
  *index      = -1;
  *rel_offset = 0.0f;
  
  for(int i = 0; i < menu_items.length(); ++i) {
    GtkAllocation item_rect;
    
    gtk_widget_get_allocation(menu_items[i], &item_rect);
    
    int center = item_rect.y + item_rect.height / 2;
    int bottom = item_rect.y + item_rect.height;
    
    if(i == 0) {
      previous_bottom = item_rect.y;
      if(y < item_rect.y)
        break;
    }
    
    if(y <= item_rect.y + item_rect.height) {
      *rel_offset = (y - center) /((float)bottom - previous_bottom);
      if(i == 0 && *rel_offset < 0)
        *rel_offset = 0;
      
      if(i == menu_items.length() - 1 && *rel_offset > 0)
        *rel_offset = 0;
      
      *index = i;
      return true;
    }
    
    previous_bottom = bottom;
  }
  
  return false;
}

void MathGtkMenuSliderRegion::apply_slider_pos(GtkWidget *menu, FrontEndReference id, int index, float rel_offset) {
  Expr cmd = MenuItemBuilder::get_command(GTK_MENU_ITEM(menu_items[index]));
  
  if(rel_offset != 0) {
    float value;
    if(!try_get_rhs_value(cmd, &value))
      return;
    
    int other_index = index + ((rel_offset > 0) ? 1 : -1);
    float other_value;
    if(!try_get_rhs_value(MenuItemBuilder::get_command(GTK_MENU_ITEM(menu_items[other_index])), &other_value))
      return;
    
    value+= fabsf(rel_offset) * (other_value - value);
    cmd = Rule(lhs, value);
    if(scope)
      cmd = Call(Symbol(richmath_FE_ScopedCommand), std::move(cmd), scope);
  }
  
  if(!Menus::run_command_now(std::move(cmd)))
    return;
  
  //expand_inline_lists(GTK_MENU(menu), id);
  gtk_widget_queue_draw(menu);
}

//} ... class MathGtkMenuSliderRegion

//{ class MathGtkMenuGutterSliders ...

const char *MathGtkMenuGutterSliders::data_key = "richmath:menu_gutter_sliders";

MathGtkMenuGutterSliders::MathGtkMenuGutterSliders()
  : Base(),
    grabbed_region_index(-1)
{
  SET_BASE_DEBUG_TAG(typeid(*this).name());
}

void MathGtkMenuGutterSliders::menu_gutter_sliders(GtkWidget *menu, MathGtkMenuGutterSliders *gutters) {
  if(auto old = menu_gutter_sliders(menu)) {
    if(old == gutters)
      return;
  }
  
  g_object_set_data_full(
    G_OBJECT(menu), 
    data_key,
    gutters,
    [](void *data) { delete (MathGtkMenuGutterSliders*)data; });
}

MathGtkMenuGutterSliders *MathGtkMenuGutterSliders::menu_gutter_sliders(GtkWidget *menu) {
  return (MathGtkMenuGutterSliders*)g_object_get_data(G_OBJECT(menu), data_key);
}

void MathGtkMenuGutterSliders::connect(GtkMenu *menu, FrontEndReference doc_id) {
  #if GTK_MAJOR_VERSION >= 3
    g_signal_connect_after(menu, "draw", G_CALLBACK(MathGtkMenuGutterSliders::menu_draw_callback), FrontEndReference::unsafe_cast_to_pointer(doc_id));
  #else
    gtk_widget_add_events(GTK_WIDGET(menu), GDK_EXPOSURE_MASK);
    g_signal_connect_after(menu, "expose-event", G_CALLBACK(MathGtkMenuGutterSliders::menu_expose_callback), FrontEndReference::unsafe_cast_to_pointer(doc_id));
  #endif
}

bool MathGtkMenuGutterSliders::on_menu_button_press(GtkWidget *menu, GdkEvent *e, FrontEndReference id) {
  auto event = &e->button;
  
  GtkWidget *wid = gtk_get_event_widget(e);
  if(!GTK_IS_MENU_ITEM(wid))
    return false;
  
  GtkAllocation wid_rect;
  gtk_widget_get_allocation(wid, &wid_rect);
  
  int x = event->x + wid_rect.x;
  int y = event->y + wid_rect.y;
  
  grabbed_region_index = -1;
  for(int i = 0; i < regions.length(); ++i) {
    GtkAllocation rect;
    regions[i].get_allocation(&rect);
    
    if( rect.x <= x && x <= rect.x + rect.width &&
        rect.y <= y && y <= rect.y + rect.height)
    {
      grabbed_region_index = i;
      break;
    }
  }
  
  if(grabbed_region_index >= 0 && grabbed_region_index < regions.length())
    return regions[grabbed_region_index].on_menu_button_press(menu, e, id);
  
  return false;
}

bool MathGtkMenuGutterSliders::on_menu_button_release(GtkWidget *menu, GdkEvent *e, FrontEndReference id) {
  int i = grabbed_region_index;
  grabbed_region_index = -1;
  
  if(i >= 0 && i < regions.length()) 
    return regions[i].on_menu_button_release(menu, e, id);
  
  return false;
}

bool MathGtkMenuGutterSliders::on_menu_motion_notify(GtkWidget *menu, GdkEvent *e, FrontEndReference id) {
  if(grabbed_region_index >= 0 && grabbed_region_index < regions.length())
    return regions[grabbed_region_index].on_menu_motion_notify(menu, e, id);
  
  return false;
}

gboolean MathGtkMenuGutterSliders::menu_expose_callback(GtkWidget *menu, GdkEvent *e, void *eval_box_id_as_ptr) {
  GdkEventExpose *event = &e->expose;
  
  cairo_t *cr = gdk_cairo_create(event->window);
  
  cairo_move_to(cr, event->area.x, event->area.y);
  cairo_rel_line_to(cr, event->area.width, 0);
  cairo_rel_line_to(cr, 0, event->area.height);
  cairo_rel_line_to(cr, -event->area.width, 0);
  cairo_close_path(cr);
  cairo_clip(cr);
  
  gboolean result = menu_draw_callback(menu, cr, eval_box_id_as_ptr);
  cairo_destroy(cr);
  
  return result;
}

gboolean MathGtkMenuGutterSliders::menu_draw_callback(GtkWidget *menu, cairo_t *cr, void *eval_box_id_as_ptr) {
  if(auto sliders = menu_gutter_sliders(menu)) {
    Canvas canvas(cr);
    sliders->on_menu_draw(menu, canvas, FrontEndReference::unsafe_cast_from_pointer(eval_box_id_as_ptr));
  }
  
  return false;
}

void MathGtkMenuGutterSliders::on_menu_draw(GtkWidget *menu, Canvas &canvas, FrontEndReference id) {
  for(auto &region : regions)
    region.paint(menu, canvas, id);
}

//} ... class MathGtkMenuGutterSliders
