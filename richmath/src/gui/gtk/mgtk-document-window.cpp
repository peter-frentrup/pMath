#include <gui/gtk/mgtk-document-window.h>

#include <boxes/section.h>

#include <eval/application.h>
#include <eval/binding.h>

#include <gui/documents.h>
#include <gui/messagebox.h>
#include <gui/gtk/mgtk-control-painter.h>
#include <gui/gtk/mgtk-css.h>
#include <gui/gtk/mgtk-menu-builder.h>

#include <cmath>

#if GTK_MAJOR_VERSION >= 3
#  include <gdk/gdkkeysyms-compat.h>
#else
#  include <gdk/gdkkeysyms.h>
#endif

#ifdef GDK_WINDOWING_X11
#  include <gdk/gdkx.h>
#  include <X11/Xlib.h>
#  include <X11/Xatom.h>
#  ifdef None
#    undef None
#  endif
#  if GTK_MAJOR_VERSION < 3
#    define  GDK_IS_X11_WINDOW(w)  ((w) != nullptr)
#  endif
#endif

using namespace richmath;

namespace richmath { namespace strings {
  extern String EmptyString;
  extern String Close;
  extern String CloseMenu_label;
  extern String Docked;
  extern String ShowHideMenu_label;
  extern String ShowHideMenu;
  extern String Docked;
}}

extern pmath_symbol_t richmath_System_Delimiter;
extern pmath_symbol_t richmath_System_DollarFailed;
extern pmath_symbol_t richmath_System_Menu;
extern pmath_symbol_t richmath_System_MenuItem;

static const int SnapDistance = 4;

class richmath::MathGtkDocumentWindowImpl {
    using Impl = MathGtkDocumentWindowImpl;
  public:
    MathGtkDocumentWindowImpl(MathGtkDocumentWindow &self) : self(self) {}
    
    static void add_remove_window(int delta);
    
    void connect_scrollbar_overlay_signals();
    void connect_adjustment_changed_signals();
    void connect_menubar_signals();
    static void register_theme_observer();
    
    void append_overflow_menu_item();
    void update_menu_bar_overflow_item(int max_width);
    
    void append_menu_bar_pin();
    bool menu_is_auto_hidden();
    void on_auto_hide_menu(bool hide);
    
    void update_scrollbar_overlay();
  
  private:
    static gboolean scrollbar_expose_callback(GtkWidget *scrollbar, GdkEvent *e, void *user_data);
    static gboolean scrollbar_draw_callback(  GtkWidget *scrollbar, cairo_t *cr, void *user_data);
    void on_scrollbar_draw(GtkWidget *scrollbar, Canvas &canvas);
    
    static void activate_pin_item_callback(GtkMenuItem *menu_item, void *user_data);
    void on_activate_pin_item(GtkMenuItem *menu_item);
    
    static void adjustment_changed_callback(GtkAdjustment *adjustment, void *user_data);
    void on_adjustment_changed(GtkAdjustment *adjustment);
    
    static void on_theme_changed(GObject*, GParamSpec*);
    
  private:
    MathGtkDocumentWindow &self;
  
#if GTK_MAJOR_VERSION >= 3
    static GtkStyleProvider *global_style_provider;
#endif
};

class MathGtkDocumentChildWidget: public MathGtkWidget {
    using base = MathGtkWidget;
    friend class MathGtkDocumentWindow;
  public:
    MathGtkDocumentChildWidget(MathGtkDocumentWindow *parent)
      : base(new Document()),
        _parent(parent)
    {
      SET_BASE_DEBUG_TAG(typeid(*this).name());
    }
  
    MathGtkDocumentWindow *parent() { return _parent; }
    
    virtual void bring_to_front() override {
      _parent->bring_to_front();
      gtk_widget_grab_focus(_widget);
    }
    
    virtual void close() override {
      _parent->close();
    }
    
    virtual void invalidate_options() override {
      base::invalidate_options();
      if(_parent)
        _parent->invalidate_options();
    }
    
    virtual void running_state_changed() override {
      _parent->reset_title();
    }
    
    virtual String directory() override { return _parent->directory(); }
    virtual void directory(String new_directory) override { _parent->directory(new_directory); }
    
    virtual String filename() override { return _parent->filename(); }
    virtual void filename(String new_filename) override { _parent->filename(new_filename); }
    
    virtual String full_filename() override { return _parent->full_filename(); }
    virtual void full_filename(String new_full_filename) override { _parent->full_filename(new_full_filename); }
    
    virtual bool can_toggle_menubar() override {          return _parent->can_toggle_menubar(); }
    virtual bool has_menubar() override {                 return _parent->has_menubar(); }
    virtual bool try_set_menubar(bool visible) override { return _parent->try_set_menubar(visible); }
    
    virtual String window_title() override { return _parent->title(); }
    
    virtual void on_idle_after_edit() override { 
      base::on_idle_after_edit();
      _parent->on_idle_after_edit(this); 
    }
    virtual void on_saved() override { _parent->on_saved(); }
    
    virtual bool is_foreground_window() override { return _parent->is_foreground_window(); }
    virtual bool is_focused_widget() override { return _parent->is_foreground_window() && base::is_focused_widget(); }
    virtual bool is_using_dark_mode() override { return _parent->is_using_dark_mode(); }
    
  private:
    MathGtkDocumentWindow *_parent;
    
  protected:
    virtual void do_set_selected_document() override {
      Documents::selected_document(_parent->document());
    }
};

class richmath::MathGtkWorkingArea: public MathGtkDocumentChildWidget {
    using base = MathGtkDocumentChildWidget;
    friend class MathGtkDocumentWindow;
  public:
    MathGtkWorkingArea(MathGtkDocumentWindow *parent)
      : base(parent)
    {
      SET_BASE_DEBUG_TAG(typeid(*this).name());
    }
    
    virtual Vector2F page_size() override {
      Vector2F size = base::page_size();
      
      if(parent()->window_frame() != WindowFrameNormal)
        size.x = HUGE_VAL;
      
      return size;
    }
    
    int width() {
      int h = (int)(document()->unfilled_width * scale_factor() + 0.5f);
      if(h < 1)
        return 1;
      return h;
    }
    
    int height() {
      int h = (int)(document()->extents().height() * scale_factor() + 0.5f);
      if(h < 1)
        return 1;
      return h;
    }
    
  protected:
    virtual void on_changed_dark_mode() override {
      parent()->update_dark_mode();
      base::on_changed_dark_mode();
    }
    
    void rearrange() {
      if(parent()->window_frame() != WindowFrameNormal) {
        GtkAllocation rect;
        gtk_widget_get_allocation(_widget, &rect);
        
        int h = height();
        int w = width();
        if(h != rect.height || w != rect.width) {
          // GTK 3 shows the menu bar even if we said not to do so before,
          // so we say it again.
          parent()->reset_window_frame();
          
          parent()->set_gravity();
          
          gtk_widget_set_size_request(_widget, w, h);
          
          bool was_resizable = gtk_window_get_resizable(GTK_WINDOW(parent()->widget()));
          if(!was_resizable)
            gtk_window_set_resizable(GTK_WINDOW(parent()->widget()), true);
          
          gtk_window_set_default_size(GTK_WINDOW(parent()->widget()), 1, 1);
          gtk_window_resize(GTK_WINDOW(parent()->widget()), 1, 1);
          
          if(!was_resizable)
            gtk_window_set_resizable(GTK_WINDOW(parent()->widget()), false);
          
          parent()->invalidate_popup_window_positions();
        }
      }
    }
    
    virtual bool on_draw(cairo_t *cr) override {
      //double clip_x1, clip_y1, clip_x2, clip_y2;
      //cairo_clip_extents(cr, &clip_x1, &clip_y1, &clip_x2, &clip_y2);
      //pmath_debug_print("[working area on_draw %f %f .. %f %f (%f x %f)]\n", clip_x1, clip_y1, clip_x2, clip_y2, clip_x2 - clip_x1, clip_y2 - clip_y1);
      bool result = base::on_draw(cr);
      rearrange();
      MathGtkDocumentWindowImpl(*parent()).update_scrollbar_overlay();
      return result;
    }
};

class richmath::MathGtkDock: public MathGtkDocumentChildWidget {
    using base = MathGtkDocumentChildWidget;
    friend class MathGtkDocumentWindow;
  public:
    MathGtkDock(MathGtkDocumentWindow *parent)
      : base(parent)
    {
      SET_BASE_DEBUG_TAG(typeid(*this).name());
     
      document()->style.reset(strings::Docked);
    }
    
    void reload(Expr content) {
      if(content == _content)
        return;
        
      _content = content;
      int i = 0;
      
      document()->insert_pmath(&i, content, document()->count());
      document()->remove(i, document()->count());
      document()->invalidate_all();
    }
    
    virtual Vector2F page_size() override {
      Vector2F size = base::page_size();
      size.x = HUGE_VAL;
      return size;
    }
    
    virtual bool is_scrollable() override { return false; }
    
    virtual void invalidate() override {
      if(document()->length() > 0) {
        //gtk_widget_set_size_request(_widget, 1, 1);
        gtk_widget_show(_widget);
      }
      else {
        gtk_widget_set_size_request(_widget, 1, 1);
        gtk_widget_hide(_widget);
      }
      
      base::invalidate();
    }
    
    int height() {
      int h = (int)(document()->extents().height() * scale_factor() + 0.5f);
      if(h < 1)
        return 1;
      return h;
    }
    
    virtual int min_width() {
      return (int)(document()->unfilled_width * scale_factor() + 0.5f);
    }
    
    virtual Document *working_area_document() override { return parent()->working_area()->document(); }
    
  protected:
    virtual void after_construction() override {
      base::after_construction();
      
      document()->style.set(Background,         Color::None);
      document()->style.set(Editable,           false); // redirect Print() to console
      document()->style.set(Selectable,         AutoBoolFalse);
      document()->style.set(ShowSectionBracket, AutoBoolFalse);
      
      document()->select(nullptr, 0, 0);
    }
    
    void rearrange() {
      GtkAllocation rect;
      gtk_widget_get_allocation(_widget, &rect);
      
      int h = height();
      if(h != rect.height) {
        gtk_widget_set_size_request(_widget, 1, h);
      }
    }
    
    virtual bool on_draw(cairo_t *cr) override {
      bool result = base::on_draw(cr);
      rearrange();
      return result;
    }
    
  private:
    Expr  _content;
};

class MathGtkTitlebarDock: public MathGtkDock {
    using base = MathGtkDock;
  public:
    MathGtkTitlebarDock(MathGtkDocumentWindow *parent)
      : base(parent)
    {
      SET_BASE_DEBUG_TAG(typeid(*this).name());
    }
    
  protected:
    virtual void paint_canvas(Canvas &canvas, bool resize_only) override {
#    if GTK_CHECK_VERSION(3, 0, 0)
      if(!resize_only) {
        if(GtkStyleContext *ctx = gtk_widget_get_style_context(_widget)) {
          GdkRGBA fg = {0, 0, 0, 0};
          gtk_style_context_get_color(ctx, GTK_STATE_FLAG_NORMAL, &fg);
          
          if(fg.alpha > 0)
            document()->style.set(FontColor, Color::from_rgb(fg.red, fg.green, fg.blue));
        }
        
        bool has_text_shadow = false;
        if(GtkWidget *titlebar = gtk_window_get_titlebar(GTK_WINDOW(parent()->widget()))) {
          if(GTK_IS_HEADER_BAR(titlebar)) {
            Expr text_shadow = MathGtkCss::parse_text_shadow(gtk_widget_get_style_context(titlebar));
            if(text_shadow.expr_length() > 0) {
              pmath_debug_print_object("[found text-shadow: ", text_shadow.get(), "]\n");
              document()->style.set(TextShadow, PMATH_CPP_MOVE(text_shadow));
              has_text_shadow = true;
            }
          }
        }
        
        if(!has_text_shadow)
          document()->style.remove(TextShadow);
      }
#    endif
      
      canvas.glass_background = true;
      base::paint_canvas(canvas, resize_only);
    }
};

//{ class MathGtkDocumentWindow ...

MathGtkDocumentWindow::MathGtkDocumentWindow()
  : BasicGtkWidget(),
    _menu_bar(nullptr),
    _overflow_menu_item(nullptr),
    _hadjustment(GTK_ADJUSTMENT(gtk_adjustment_new(0, 0, 0, 0, 0, 0))),
    _vadjustment(GTK_ADJUSTMENT(gtk_adjustment_new(0, 0, 0, 0, 0, 0))),
    _hscrollbar(nullptr),
    _vscrollbar(nullptr),
#if GTK_MAJOR_VERSION >= 3
    _style_provider(nullptr),
    _menu_bar_pin(nullptr),
    _icon_box(nullptr),
#endif
    _window_frame(WindowFrameNormal),
    _active(true),
    _use_dark_mode(false)
{
  SET_EXPLICIT_BASE_DEBUG_TAG(CommonDocumentWindow, typeid(*this).name());
  SET_EXPLICIT_BASE_DEBUG_TAG(BasicGtkWidget,       typeid(*this).name());
  
  Impl::add_remove_window(+1);
  
  _previous_rect.x = 0;
  _previous_rect.y = 0;
  _previous_rect.width = 1;
  _previous_rect.height = 1;
  
  _working_area   = new MathGtkWorkingArea(this);
  _top_glass_area = new MathGtkTitlebarDock(this);
  _top_area       = new MathGtkDock(this);
  _bottom_area    = new MathGtkDock(this);
  
  _content = _working_area->document();
}

void MathGtkDocumentWindow::after_construction() {
  if(!_widget)
    _widget = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    
  BasicGtkWidget::after_construction();
  
  gtk_window_set_icon_list(GTK_WINDOW(_widget), icons.get_app_icon_list());
  
  _working_area->init();
  _top_glass_area->init();
  _top_area->init();
  _bottom_area->init();
  
  top()->main_document    = document();
  bottom()->main_document = document();
  
  GtkAccelGroup *accel_group = gtk_accel_group_new();
  
  _menu_bar = gtk_menu_bar_new();
  gtk_widget_set_size_request(_menu_bar, 0, -1);
  MathGtkMenuBuilder::main_menu.append_to(GTK_MENU_SHELL(_menu_bar), accel_group, document()->id());
  Impl(*this).append_overflow_menu_item();
  Impl(*this).append_menu_bar_pin();
  MathGtkAccelerators::connect_all(accel_group, document()->id());
  
  // Work around GTK 3 regression that ignores our explicit gtk_widget_set_size_request() above.
  // This problem is also discussed on https://bugs.documentfoundation.org/show_bug.cgi?id=116290
  GtkWidget *menu_area = _menu_bar;
#if GTK_MAJOR_VERSION >= 3
#  if !GTK_CHECK_VERSION(3, 16, 0)
#    define   GTK_POLICY_EXTERNAL   ((GtkPolicyType)3)
#  endif
  if(nullptr == gtk_check_version(3, 16, 0)) {
    menu_area = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_container_add(GTK_CONTAINER(menu_area), _menu_bar);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(menu_area), GTK_POLICY_EXTERNAL, GTK_POLICY_NEVER);
  }
#endif

#if GTK_CHECK_VERSION(3,10,0)
  {
    const char *gtk_csd_env = g_getenv("GTK_CSD");
    if(!gtk_csd_env || !*gtk_csd_env) gtk_csd_env = "1";
    if(strcmp(gtk_csd_env, "1") == 0) {
      GtkWidget *header_bar = gtk_header_bar_new();
      
      gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(header_bar), true);
      gtk_header_bar_set_has_subtitle(GTK_HEADER_BAR(header_bar), false);
      
      //gtk_header_bar_set_decoration_layout(GTK_HEADER_BAR(header_bar), "menu:minimize,maximize,close");
      
      {
        GtkWidget *icon = gtk_image_new_from_pixbuf(icons.get_app_icon(GTK_ICON_SIZE_SMALL_TOOLBAR));
        
        _icon_box = gtk_menu_button_new();
        gtk_container_add(GTK_CONTAINER(_icon_box), icon);
        gtk_button_set_relief(GTK_BUTTON(_icon_box), GTK_RELIEF_NONE);
        
        GtkWidget *icon_menu = gtk_menu_new();
        
        GtkAccelGroup *dummy_accel_group = gtk_accel_group_new();
        
        static const char menu_tag[] = "Richmath Window Menu Tag";
        enum class SpecialItemTag {
          Restore = 1,
          Move,
          Resize,
          Minimize,
          Maximize,
        };
        
        if(auto item = gtk_image_menu_item_new_with_mnemonic("Restore")) {
          gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), gtk_image_new_from_icon_name("window-restore-symbolic", GTK_ICON_SIZE_SMALL_TOOLBAR));
          gtk_image_menu_item_set_always_show_image(GTK_IMAGE_MENU_ITEM(item), true);
          gtk_widget_show_all(item);
          gtk_menu_shell_append(GTK_MENU_SHELL(icon_menu), item);
          g_object_set_data(G_OBJECT(item), menu_tag, (void*)SpecialItemTag::Restore);
          
          g_signal_connect(G_OBJECT(item), "activate", 
            G_CALLBACK((void(*)(GtkMenuItem*,void*))[](GtkMenuItem *sender, void *_self) {
              MathGtkDocumentWindow &self = *(MathGtkDocumentWindow*)_self;
              
              GdkWindowState state = gdk_window_get_state(gtk_widget_get_window(self.widget()));
              
              if(state & GDK_WINDOW_STATE_MAXIMIZED) {
                gtk_window_unmaximize(GTK_WINDOW(self.widget()));
                return;
              }
              
              if(state & GDK_WINDOW_STATE_ICONIFIED) 
                gtk_window_deiconify(GTK_WINDOW(self.widget()));
            }), 
            this);
        }
        
        if(auto item = gtk_image_menu_item_new_with_mnemonic("Move")) {
          gtk_widget_show_all(item);
          gtk_menu_shell_append(GTK_MENU_SHELL(icon_menu), item);
          g_object_set_data(G_OBJECT(item), menu_tag, (void*)SpecialItemTag::Move);
          
          g_signal_connect(G_OBJECT(item), "activate", 
            G_CALLBACK((void(*)(GtkMenuItem*,void*))[](GtkMenuItem *sender, void *_self) {
              MathGtkDocumentWindow &self = *(MathGtkDocumentWindow*)_self;
              
              gtk_window_begin_move_drag(GTK_WINDOW(self.widget()), 0, 0, 0, GDK_CURRENT_TIME);
            }), 
            this);
        }
        
        if(auto item = gtk_image_menu_item_new_with_mnemonic("Resize")) {
          gtk_widget_show_all(item);
          gtk_menu_shell_append(GTK_MENU_SHELL(icon_menu), item);
          g_object_set_data(G_OBJECT(item), menu_tag, (void*)SpecialItemTag::Resize);
          
          g_signal_connect(G_OBJECT(item), "activate", 
            G_CALLBACK((void(*)(GtkMenuItem*,void*))[](GtkMenuItem *sender, void *_self) {
              MathGtkDocumentWindow &self = *(MathGtkDocumentWindow*)_self;
              
              gtk_window_begin_resize_drag(GTK_WINDOW(self.widget()), GDK_WINDOW_EDGE_NORTH_WEST, 0, 0, 0, GDK_CURRENT_TIME);
            }), 
            this);
        }
        
        if(auto item = gtk_image_menu_item_new_with_mnemonic("Minimize")) {
          gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), gtk_image_new_from_icon_name("window-minimize-symbolic", GTK_ICON_SIZE_MENU));
          gtk_image_menu_item_set_always_show_image(GTK_IMAGE_MENU_ITEM(item), true);
          gtk_widget_show_all(item);
          gtk_menu_shell_append(GTK_MENU_SHELL(icon_menu), item);
          g_object_set_data(G_OBJECT(item), menu_tag, (void*)SpecialItemTag::Minimize);
          
          g_signal_connect(G_OBJECT(item), "activate", 
            G_CALLBACK((void(*)(GtkMenuItem*,void*))[](GtkMenuItem *sender, void *_self) {
              MathGtkDocumentWindow &self = *(MathGtkDocumentWindow*)_self;
              
              gtk_window_iconify(GTK_WINDOW(self.widget()));
            }), 
            this);
        }
        
        if(auto item = gtk_image_menu_item_new_with_mnemonic("Maximize")) {
          gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), gtk_image_new_from_icon_name("window-maximize-symbolic", GTK_ICON_SIZE_MENU));
          gtk_image_menu_item_set_always_show_image(GTK_IMAGE_MENU_ITEM(item), true);
          gtk_widget_show_all(item);
          gtk_menu_shell_append(GTK_MENU_SHELL(icon_menu), item);
          g_object_set_data(G_OBJECT(item), menu_tag, (void*)SpecialItemTag::Maximize);
          
          g_signal_connect(G_OBJECT(item), "activate", 
            G_CALLBACK((void(*)(GtkMenuItem*,void*))[](GtkMenuItem *sender, void *_self) {
              MathGtkDocumentWindow &self = *(MathGtkDocumentWindow*)_self;
              
              gtk_window_maximize(GTK_WINDOW(self.widget()));
            }), 
            this);
        }
        
        if(auto item = gtk_separator_menu_item_new()) {
          gtk_widget_show(item);
          gtk_menu_shell_append(GTK_MENU_SHELL(icon_menu), item);
        }
        
        if(auto item = gtk_image_menu_item_new_with_mnemonic("_Close")) {
          gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), gtk_image_new_from_icon_name("window-close-symbolic", GTK_ICON_SIZE_MENU));
          gtk_image_menu_item_set_always_show_image(GTK_IMAGE_MENU_ITEM(item), true);
          
//          static const bool CloseIsDefault = true;
//          if(CloseIsDefault) {
//            if(auto label = gtk_bin_get_child(GTK_BIN(item))) {
//              if(GTK_IS_LABEL(label)) {
//                gtk_label_set_markup_with_mnemonic(GTK_LABEL(label), "<b>_Close</b>");
//              }
//            }
//            
//            // FIXME: intercepting double clicks does not work, becuse the menu is opened on first click and grabs focus
//            g_signal_connect(
//              icon_box, "button-press-event",
//              G_CALLBACK((gboolean(*)(GtkWidget*,GdkEventButton*,void*))[](GtkWidget *sender, GdkEventButton *e, void *_self) -> gboolean {
//                pmath_debug_print("[icon press %d %d]", e->type, e->button);
//                MathGtkDocumentWindow &self = *(MathGtkDocumentWindow*)_self;
//                if(e->type == GDK_2BUTTON_PRESS && e->button == 1) {
//                  self.close();
//                  return true;
//                }
//                return false;
//              }),
//              this);
//          }
          
          gtk_widget_show_all(item);
          gtk_menu_shell_append(GTK_MENU_SHELL(icon_menu), item);
          g_signal_connect(
            G_OBJECT(item), "activate",
            G_CALLBACK((void(*)(GtkMenuItem*, void*))[](GtkMenuItem *sender, void *_self) {
              MathGtkDocumentWindow &self = *(MathGtkDocumentWindow*)_self;
              self.close();
            }), 
            this);
        }
        
        struct MapEvent {
          static gboolean callback(GtkWidget *menu, GdkEventAny *e, void *_self) {
            MathGtkDocumentWindow &self = *(MathGtkDocumentWindow*)_self;
            
            GdkWindowState state = gdk_window_get_state(gtk_widget_get_window(self.widget()));
            
            bool resizable = gtk_window_get_resizable(GTK_WINDOW(self.widget()));
            bool iconified = (state & GDK_WINDOW_STATE_ICONIFIED) != 0;
            bool maximized = (state & GDK_WINDOW_STATE_MAXIMIZED) != 0;
            bool is_normal_window = gtk_window_get_type_hint(GTK_WINDOW(self.widget())) == GDK_WINDOW_TYPE_HINT_NORMAL;
            
            container_foreach(GTK_CONTAINER(menu), 
              [&](GtkWidget *menuitem) {
                if(!GTK_IS_MENU_ITEM(menuitem))
                  return;
                
                auto tag = (SpecialItemTag)(intptr_t)g_object_get_data(G_OBJECT(menuitem), menu_tag);
                switch(tag) {
                  case SpecialItemTag::Restore: {
                    bool enabled = true;
                    if(gtk_widget_is_visible(self.widget()) && !(maximized || iconified))
                      enabled = false;
                    else if(!iconified && !resizable)
                      enabled = false;
                    else if(!is_normal_window)
                      enabled = false;
                    
                    gtk_widget_set_sensitive(menuitem, enabled);
                  } break;
                  
                  case SpecialItemTag::Move: 
                    gtk_widget_set_sensitive(menuitem, !maximized && !iconified);
                    break;
                  
                  case SpecialItemTag::Resize: 
                    gtk_widget_set_sensitive(menuitem, resizable && !maximized && !iconified);
                    break;
                  
                  case SpecialItemTag::Minimize: 
                    gtk_widget_set_sensitive(menuitem, !iconified && is_normal_window);
                    break;
                  
                  case SpecialItemTag::Maximize: 
                    gtk_widget_set_sensitive(menuitem, !maximized && resizable && is_normal_window);
                    break;
                }
              });
            return false;
          }
        };
        
        g_signal_connect(icon_menu, "map-event", G_CALLBACK(MapEvent::callback), this);
        
        MathGtkMenuBuilder(
            Call(Symbol(richmath_System_Menu), strings::EmptyString, 
              List(
                // "Close" command applies to selected document even in palette windows.
                //Call(Symbol(richmath_System_MenuItem), strings::CloseMenu_label, strings::Close),
                Symbol(richmath_System_Delimiter),
                Call(Symbol(richmath_System_MenuItem), strings::ShowHideMenu_label, strings::ShowHideMenu),
                Symbol(richmath_System_Delimiter)
              ))
          ).append_to(GTK_MENU_SHELL(icon_menu), dummy_accel_group, document()->id());
        MathGtkMenuBuilder::main_menu.append_to(GTK_MENU_SHELL(icon_menu), dummy_accel_group, document()->id());
        MathGtkMenuBuilder::connect_events(GTK_MENU(icon_menu), document()->id());
        g_object_unref(dummy_accel_group);
        
        gtk_menu_button_set_popup(GTK_MENU_BUTTON(_icon_box), icon_menu);
        
        gtk_header_bar_pack_start(GTK_HEADER_BAR(header_bar), _icon_box);
      }
      
      if(_menu_bar_pin) {
        GtkWidget *menu_button = gtk_button_new();
      
        GIcon *menu_icon = g_themed_icon_new("open-menu-symbolic"); // "view-more-symbolic"
        GtkWidget *image = gtk_image_new_from_gicon(menu_icon, GTK_ICON_SIZE_BUTTON);
        g_object_unref(menu_icon);
        gtk_container_add(GTK_CONTAINER(menu_button), image);
        gtk_button_set_relief(GTK_BUTTON(menu_button), GTK_RELIEF_NONE);
        gtk_widget_set_tooltip_text(menu_button, "Show/Hide Menu");
        
        g_signal_connect(
          menu_button, 
          "clicked", 
          G_CALLBACK((void(*)(GtkButton*,void*))[](GtkButton *sender, void *_self) {
            MathGtkDocumentWindow &self = *(MathGtkDocumentWindow*)_self;
            self.try_set_menubar(!self.has_menubar());
          }), 
          this);
        
        g_object_bind_property(
          _menu_bar, "visible", menu_button, "visible", 
          GBindingFlags(G_BINDING_SYNC_CREATE));
        
        gtk_header_bar_pack_start(GTK_HEADER_BAR(header_bar), menu_button);
      }
      
      {
        GtkWidget *background_header_bar = gtk_header_bar_new();
        gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(background_header_bar), false);
        //gtk_header_bar_set_title(            GTK_HEADER_BAR(background_header_bar), "");
        gtk_header_bar_set_has_subtitle(     GTK_HEADER_BAR(background_header_bar), false);
        
        GtkWidget *vbox = gtk_vbox_new(false, 0);
        gtk_widget_set_halign( vbox,  GTK_ALIGN_FILL);
        gtk_widget_set_hexpand(vbox, true);
        gtk_header_bar_set_custom_title(GTK_HEADER_BAR(background_header_bar), vbox);
        gtk_box_pack_start(GTK_BOX(vbox), header_bar, false, false, 0);
        
        gtk_style_context_add_class(gtk_widget_get_style_context(background_header_bar), "pmath-outer-headerbar");
        gtk_style_context_add_class(gtk_widget_get_style_context(header_bar),            "pmath-inner-headerbar");
        
        gtk_box_pack_start(GTK_BOX(vbox), _top_glass_area->widget(), false, false, 0);
        
        g_object_bind_property(background_header_bar, "title", header_bar, "title", G_BINDING_DEFAULT);
        
        header_bar = background_header_bar;
      }
      
      gtk_window_set_titlebar(GTK_WINDOW(_widget), header_bar);
      gtk_widget_show_all(header_bar);
    }
  }
#endif
  
  gtk_window_add_accel_group(GTK_WINDOW(_widget), accel_group);
  g_object_unref(accel_group);
  
  GtkWidget *client_area = gtk_vbox_new(false, 0);
  gtk_container_add(GTK_CONTAINER(_widget), client_area);
  
  gtk_box_pack_start(GTK_BOX(client_area), menu_area, false, false, 0);
  
  if(gtk_widget_get_parent(_top_glass_area->widget()) == nullptr) {
    gtk_box_pack_start(GTK_BOX(client_area), _top_glass_area->widget(), false, false, 0);
  }
  
  gtk_box_pack_start(GTK_BOX(client_area), _top_area->widget(), false, false, 0);
  
  gtk_box_pack_end(GTK_BOX(client_area), _bottom_area->widget(), false, false, 0);
  
  
  
  GtkWidget *scroll_grid = gtk_table_new(2, 2, FALSE);
  gtk_box_pack_start(GTK_BOX(client_area), scroll_grid, true, true, 0);
  
  gtk_table_attach(GTK_TABLE(scroll_grid), _working_area->widget(), 0, 1, 0, 1, (GtkAttachOptions)(GTK_EXPAND | GTK_FILL | GTK_SHRINK), (GtkAttachOptions)(GTK_EXPAND | GTK_FILL | GTK_SHRINK), 1, 1); // Need padding to cater for GTK 3 bug causing scrollbar redraw requests to leak into the working area widget
  
  g_object_ref(_hadjustment);
  g_object_ref(_vadjustment);
  _hscrollbar = gtk_hscrollbar_new(_hadjustment);
  _vscrollbar = gtk_vscrollbar_new(_vadjustment);
  gtk_table_attach(GTK_TABLE(scroll_grid), _vscrollbar, 1, 2, 0, 1, (GtkAttachOptions)0, (GtkAttachOptions)(GTK_EXPAND | GTK_FILL | GTK_SHRINK), 0, 0);
  gtk_table_attach(GTK_TABLE(scroll_grid), _hscrollbar, 0, 1, 1, 2, (GtkAttachOptions)(GTK_EXPAND | GTK_FILL | GTK_SHRINK), (GtkAttachOptions)0, 0, 0);
  
  Impl(*this).connect_scrollbar_overlay_signals();
  Impl(*this).connect_adjustment_changed_signals();
  Impl(*this).connect_menubar_signals();
  
  g_object_ref(_hadjustment);
  g_object_ref(_vadjustment);
  _working_area->hadjustment(_hadjustment);
  _working_area->vadjustment(_vadjustment);
  
  gtk_widget_show_all(client_area);
  gtk_widget_set_can_focus(_widget, FALSE);
  
  GList *focus_chain = nullptr;
  focus_chain = g_list_prepend(focus_chain, _working_area->widget());
  gtk_container_set_focus_chain(GTK_CONTAINER(client_area), focus_chain);
  g_list_free(focus_chain);
  
  gtk_window_set_default_size(GTK_WINDOW(_widget), 500, 550);
  
  top()->invalidate();
  bottom()->invalidate();
  
  if(!Documents::selected_document()) 
    Documents::selected_document(document());
  
  signal_connect<MathGtkDocumentWindow, GdkEvent *, &MathGtkDocumentWindow::on_configure>("configure-event");
  signal_connect<MathGtkDocumentWindow, GdkEvent *, &MathGtkDocumentWindow::on_delete>("delete-event");
  signal_connect<MathGtkDocumentWindow, GdkEvent *, &MathGtkDocumentWindow::on_focus_in>("focus-in-event");
  signal_connect<MathGtkDocumentWindow, GdkEvent *, &MathGtkDocumentWindow::on_focus_out>("focus-out-event");
  signal_connect<MathGtkDocumentWindow, GdkEvent *, &MathGtkDocumentWindow::on_scroll>("scroll-event");
  signal_connect<MathGtkDocumentWindow, GdkEvent *, &MathGtkDocumentWindow::on_map>("map-event");
  signal_connect<MathGtkDocumentWindow, GdkEvent *, &MathGtkDocumentWindow::on_unmap>("unmap-event");
  signal_connect<MathGtkDocumentWindow, GdkEvent *, &MathGtkDocumentWindow::on_window_state>("window-state-event");

  title(String());
  
  document()->style.set(Visible,                         true);
  document()->style.set(InternalHasModifiedWindowOption, true);
  update_dark_mode();
  Impl::register_theme_observer();
  Impl(*this).on_auto_hide_menu(true); // too early if menus get realized and mapped asynchronously via MathGtkMenuBuilder::connect_events() workaroung for GTK >=3.24.6 misfeature "Fix submenu size"
}

MathGtkDocumentWindow::~MathGtkDocumentWindow() {
  pmath_debug_print("[MathGtkDocumentWindow::~MathGtkDocumentWindow]\n");
  
  static bool deleting_all = false;
  if(!deleting_all) {
    bool have_only_palettes = true;
    Array<MathGtkDocumentWindow*> palettes;
    for(auto _win : CommonDocumentWindow::All) {
      if(auto win = dynamic_cast<MathGtkDocumentWindow*>(_win)) {
        if(win == this)
          continue;
        
        if(!win->is_palette() && win->document()->get_style(Visible, true)) {
          have_only_palettes = false;
          break;
        }
        
        palettes.add(win);
      }
    }
    
    if(have_only_palettes) {
      deleting_all = true;
      
      for(auto win : palettes) {
        win->destroy();
      }
      
      deleting_all = false;
      
      gtk_main_quit();
    }
  }
  
#if GTK_MAJOR_VERSION >= 3
  if(_style_provider) {
    g_object_unref(_style_provider);
  }
#endif
  g_object_unref(_hadjustment);
  g_object_unref(_vadjustment);
  
  Impl::add_remove_window(-1);
}

void MathGtkDocumentWindow::update_dark_mode() {
#if GTK_MAJOR_VERSION >= 3
  if(_style_provider) {
    BasicGtkWidget::internal_forall_recursive(
      _widget, 
      [=](GtkWidget *w) { 
        gtk_style_context_remove_provider(gtk_widget_get_style_context(w), _style_provider);
      });
    if(gtk_widget_has_screen(_widget)) {
      GdkScreen *screen = gtk_widget_get_screen(_widget);
      pmath_debug_print("[remove_provider_for_screen(%p, %p)]\n", screen, _style_provider);
      gtk_style_context_remove_provider_for_screen(screen, _style_provider);
    }
    
    g_object_unref(_style_provider);
    _style_provider = nullptr;
  }
#endif
  
  _use_dark_mode = _working_area->has_dark_background();
  
#if GTK_MAJOR_VERSION >= 3
  _style_provider = _use_dark_mode ? MathGtkControlPainter::gtk_painter.current_theme_dark() : MathGtkControlPainter::gtk_painter.current_theme_light();
  (void)g_object_ref(_style_provider);
  
  BasicGtkWidget::internal_forall_recursive(
    _widget, 
    [=](GtkWidget *w) { 
      gtk_style_context_add_provider(gtk_widget_get_style_context(w), _style_provider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    });
  
  if(_active.unobserved_equals(true)) {
    if(gtk_widget_has_screen(_widget)) {
      GdkScreen *screen = gtk_widget_get_screen(_widget);
      pmath_debug_print("[add_provider_for_screen(%p, %p)]\n", screen, _style_provider);
      gtk_style_context_add_provider_for_screen(screen, _style_provider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    }
  }
  
#ifdef GDK_WINDOWING_X11
  GdkWindow *gdk_window = gtk_widget_get_window(_widget);
  if(GDK_IS_X11_WINDOW(gdk_window)) {
    gdk_x11_window_set_theme_variant(gdk_window, (char*)(_use_dark_mode ? "dark" : "light"));
  }
#endif // GDK_WINDOWING_X11
#endif // GTK_MAJOR_VERSION
}

void MathGtkDocumentWindow::invalidate_options() {
  Document *doc = document();
  
  if(doc->load_stylesheet()) {
    _top_glass_area->document()->invalidate_all();
    _top_area->document(      )->invalidate_all();
    _bottom_area->document(   )->invalidate_all();
  }
  
  String s = doc->get_style(WindowTitle, _default_title);
  if(!_title.unobserved_equals(s))
    title(s);
  
  {
    float progress = doc->get_own_style(WindowProgress, 0.0f);
  
#ifdef GDK_WINDOWING_X11
    GdkWindow *gdk_window = gtk_widget_get_window(_widget);
    GdkDisplay *display = gdk_window_get_display(gdk_window);
    if(GDK_IS_X11_WINDOW(gdk_window)) {
      if(progress > 0 && progress <= 1) {
        uint32_t cardinal = (uint32_t)(progress * 100);
        XChangeProperty(
          GDK_DISPLAY_XDISPLAY(display),
          GDK_WINDOW_XID(gdk_window),
          gdk_x11_get_xatom_by_name_for_display(display, "_NET_WM_XAPP_PROGRESS"),
          XA_CARDINAL, 32,
          PropModeReplace,
          (guchar*)&cardinal, 1);
      }
      else {
        XDeleteProperty(
          GDK_DISPLAY_XDISPLAY(display),
          GDK_WINDOW_XID(gdk_window),
          gdk_x11_get_xatom_by_name_for_display(display, "_NET_WM_XAPP_PROGRESS"));
      }
    }
#endif // GDK_WINDOWING_X11
  }
  
  WindowFrameType f = (WindowFrameType)doc->get_style(WindowFrame, _window_frame);
  if(_window_frame != f)
    window_frame(f);
  
  Expr bottom       = SectionList::group(doc->get_finished_flatlist_style(DockedSectionsBottom));
  Expr bottom_glass = SectionList::group(doc->get_finished_flatlist_style(DockedSectionsBottomGlass));
  
  _top_glass_area->document()->stylesheet(doc->stylesheet());
  _top_area->document(      )->stylesheet(doc->stylesheet());
  _bottom_area->document(   )->stylesheet(doc->stylesheet());
    
  _top_glass_area->reload(SectionList::group(SectionList::group(doc->get_finished_flatlist_style(DockedSectionsTopGlass))));
  _top_area->reload(      SectionList::group(SectionList::group(doc->get_finished_flatlist_style(DockedSectionsTop))));
  _bottom_area->reload(   SectionList::group(List(bottom, bottom_glass)));
  
  if(doc->get_style(Visible, true)) {
    //gtk_window_present(GTK_WINDOW(_widget));
    
    if(!gtk_widget_get_visible(_widget))
      gtk_widget_show(_widget);
  }
  else {
    if(gtk_widget_get_visible(_widget))
      gtk_widget_hide(_widget);
  }
}

void MathGtkDocumentWindow::invalidate_popup_window_positions() {
  top(     )->invalidate_popup_window_positions();
  document()->invalidate_popup_window_positions();
  bottom(  )->invalidate_popup_window_positions();
}

void MathGtkDocumentWindow::reset_title() {
  title(document()->get_style(WindowTitle, _default_title));
}

void MathGtkDocumentWindow::finish_apply_title(String displayed_title) {
  char *str = pmath_string_to_utf8(displayed_title.get(), nullptr);
  if(str)
    gtk_window_set_title(GTK_WINDOW(_widget), str);
    
  pmath_mem_free(str);
}

bool MathGtkDocumentWindow::can_toggle_menubar() {
#if GTK_MAJOR_VERSION >= 3
  return gtk_widget_get_visible(_menu_bar);
#else
  return false;
#endif
}

bool MathGtkDocumentWindow::has_menubar() {
#if GTK_MAJOR_VERSION >= 3
  if(_menu_bar_pin) {
    return 0 != (gtk_widget_get_state_flags(_menu_bar_pin) & GTK_STATE_FLAG_CHECKED);
  }
#endif
  return true;
}

bool MathGtkDocumentWindow::try_set_menubar(bool visible) {
#if GTK_MAJOR_VERSION >= 3
  if(_menu_bar_pin) {
    if(visible)
      gtk_widget_set_state_flags(GTK_WIDGET(_menu_bar_pin), GTK_STATE_FLAG_CHECKED, false);
    else
      gtk_widget_unset_state_flags(GTK_WIDGET(_menu_bar_pin), GTK_STATE_FLAG_CHECKED);
  }
#endif
  Impl(*this).on_auto_hide_menu(!visible);
  return has_menubar() == visible;
}

void MathGtkDocumentWindow::window_frame(WindowFrameType type) {
  if(!_widget) {
    _window_frame = type;
    return;
  }
  
  gtk_window_set_focus_on_map(GTK_WINDOW(_widget), type == WindowFrameNormal);
  
//    GdkWindow *gdk = gtk_widget_get_window(_widget);
  
  _working_area->invalidate();
  
  bool was_mapped = gtk_widget_get_mapped(_widget);
  if(was_mapped) {
    gtk_widget_set_mapped(_widget, false);
  }
//  int x, y;
//  bool was_visible = gtk_widget_get_visible(_widget);
//  if(was_visible) {
//    gtk_window_get_position(GTK_WINDOW(_widget), &x, &y);
//    gtk_widget_hide(_widget);
//    //gdk_window_hide(gdk);
//  }
  
  switch(type) {
    case WindowFrameNormal:
      gtk_window_set_type_hint(        GTK_WINDOW(_widget), GDK_WINDOW_TYPE_HINT_NORMAL);
      gtk_window_set_skip_taskbar_hint(GTK_WINDOW(_widget), false);
      gtk_window_set_resizable(        GTK_WINDOW(_widget), true);
      gtk_window_set_decorated(        GTK_WINDOW(_widget), true);
      gtk_container_set_border_width(  GTK_CONTAINER(_widget), 0);
      _working_area->_autohide_vertical_scrollbar = false;
      gtk_widget_set_visible(_menu_bar, true);
#if GTK_MAJOR_VERSION >= 3
      if(_icon_box) gtk_widget_set_visible(_icon_box, true);
#endif
      break;
      
    case WindowFramePalette:
      gtk_window_set_type_hint(        GTK_WINDOW(_widget), GDK_WINDOW_TYPE_HINT_UTILITY);
      gtk_window_set_skip_taskbar_hint(GTK_WINDOW(_widget), true);
      gtk_window_set_resizable(        GTK_WINDOW(_widget), false);
      gtk_window_set_decorated(        GTK_WINDOW(_widget), true);
      gtk_container_set_border_width(  GTK_CONTAINER(_widget), 0);
      _working_area->_autohide_vertical_scrollbar = true;
      gtk_widget_set_visible(_menu_bar, false);
#if GTK_MAJOR_VERSION >= 3
      if(_icon_box) gtk_widget_set_visible(_icon_box, false);
#endif
      break;
      
    case WindowFrameDialog:
      gtk_window_set_type_hint(        GTK_WINDOW(_widget), GDK_WINDOW_TYPE_HINT_DIALOG);
      gtk_window_set_skip_taskbar_hint(GTK_WINDOW(_widget), true);
      gtk_window_set_resizable(        GTK_WINDOW(_widget), false);
      gtk_window_set_decorated(        GTK_WINDOW(_widget), true);
      gtk_container_set_border_width(  GTK_CONTAINER(_widget), 0);
      _working_area->_autohide_vertical_scrollbar = true;
      gtk_widget_set_visible(_menu_bar, false);
#if GTK_MAJOR_VERSION >= 3
      if(_icon_box) gtk_widget_set_visible(_icon_box, true);
#endif
      break;
    
    case WindowFrameNone:
      gtk_window_set_type_hint(        GTK_WINDOW(_widget), GDK_WINDOW_TYPE_HINT_POPUP_MENU);
      gtk_window_set_skip_taskbar_hint(GTK_WINDOW(_widget), true);
      gtk_window_set_resizable(        GTK_WINDOW(_widget), false);
      gtk_window_set_decorated(        GTK_WINDOW(_widget), false);
      gtk_container_set_border_width(  GTK_CONTAINER(_widget), 0);
      _working_area->_autohide_vertical_scrollbar = true;
      gtk_widget_set_visible(_menu_bar, false);
#if GTK_MAJOR_VERSION >= 3
      if(_icon_box) gtk_widget_set_visible(_icon_box, false);
#endif
      break;
    
    case WindowFrameThin:
    case WindowFrameThinCallout:
      // TODO: draw a border ...
      gtk_window_set_type_hint(        GTK_WINDOW(_widget), GDK_WINDOW_TYPE_HINT_POPUP_MENU);
      gtk_window_set_skip_taskbar_hint(GTK_WINDOW(_widget), true);
      gtk_window_set_resizable(        GTK_WINDOW(_widget), false);
      gtk_window_set_decorated(        GTK_WINDOW(_widget), false);
      gtk_container_set_border_width(  GTK_CONTAINER(_widget), 1);
      _working_area->_autohide_vertical_scrollbar = true;
      gtk_widget_set_visible(_menu_bar, false);
#if GTK_MAJOR_VERSION >= 3
      if(_icon_box) gtk_widget_set_visible(_icon_box, false);
#endif
      break;
  }
  
  if(was_mapped) {
    gtk_widget_set_mapped(_widget, true);
  }
//  if(was_visible) {
//    //gdk_window_hide(gdk);
//    gtk_widget_show(_widget);
//    gtk_window_move(GTK_WINDOW(_widget), x, y);
//  }
  
  _window_frame = type;
}

void MathGtkDocumentWindow::bring_to_front() {
  gtk_window_present(GTK_WINDOW(_widget));
}

void MathGtkDocumentWindow::close() {
  pmath_debug_print("[MathGtkDocumentWindow::close: %d %p]\n", destroying(), _widget);
  if(!_widget || destroying()) 
    return;
  
  GdkEvent *ev = gdk_event_new(GDK_DELETE);
  if(!gtk_widget_event(_widget, ev)) {
    destroy();
  }
  gdk_event_free(ev);
}

bool MathGtkDocumentWindow::is_foreground_window() {
  return _active && Documents::focused_document_id != FrontEndReference::None; 
}
int MathGtkDocumentWindow::dpi() {
  GdkScreen *screen = gtk_window_get_screen(GTK_WINDOW(_widget));
  double dpi = gdk_screen_get_resolution(screen);
  if(dpi <= 0)
    return 96;
  return (int)dpi;
}
      
void MathGtkDocumentWindow::set_gravity() {
  if(!_widget)
    return;
    
  if(!is_palette()) {
    gtk_window_set_gravity(GTK_WINDOW(_widget), GDK_GRAVITY_NORTH_WEST);
  }
  else {
    GdkRectangle outer;
    get_outer_rect(&outer);
    
    for(auto _win : CommonDocumentWindow::All) {
      if(auto win = dynamic_cast<MathGtkDocumentWindow*>(_win)) {
        if(!win->is_palette()) {
          GdkRectangle rect;
          win->get_outer_rect(&rect);
          
          int dx = SnapDistance;
          int dy = SnapDistance;
          GdkWindowEdge edge;
          if(test_rects_touch(outer, rect, &dx, &dy, &edge)) {
            GdkGravity gravity = GDK_GRAVITY_NORTH_WEST;
            switch(edge) {
              case GDK_WINDOW_EDGE_NORTH_EAST:
              case GDK_WINDOW_EDGE_EAST:
              case GDK_WINDOW_EDGE_SOUTH_EAST:
                gravity = GDK_GRAVITY_NORTH_EAST;
                break;
                
              default: break;
            }
            
            gtk_window_set_gravity(GTK_WINDOW(_widget), gravity);
          }
        }
      }
    }
  }
}

void MathGtkDocumentWindow::set_initial_rect(int x, int y, int w, int h) {
  if(!_widget || gtk_widget_get_visible(_widget))
    return;
    
  _previous_rect.x      = x;
  _previous_rect.y      = y;
  _previous_rect.width  = w;
  _previous_rect.height = h;
  
  gtk_window_set_default_size(GTK_WINDOW(_widget), w, h);
  gtk_window_move(GTK_WINDOW(_widget), x, y);
}

void MathGtkDocumentWindow::get_window_margins(int *left, int *right, int *top, int *bottom) {
  static int l, r, t, b;
  if(!left)   left   = &l;
  if(!right)  right  = &r;
  if(!top)    top    = &t;
  if(!bottom) bottom = &b;
  
  int x1, x2, y1, y2;
  GdkGravity old_gravity = gtk_window_get_gravity(GTK_WINDOW(_widget));
  gtk_window_set_gravity(GTK_WINDOW(_widget), GDK_GRAVITY_NORTH_WEST);
  gtk_window_get_position(GTK_WINDOW(_widget), &x1, &y1);
  gtk_window_set_gravity(GTK_WINDOW(_widget), GDK_GRAVITY_SOUTH_EAST);
  gtk_window_get_position(GTK_WINDOW(_widget), &x2, &y2); // seems not to take the client width/height into account
  
  int cx, cy;
  gtk_window_set_gravity(GTK_WINDOW(_widget), GDK_GRAVITY_STATIC);
  gtk_window_get_position(GTK_WINDOW(_widget), &cx, &cy);
  
  gtk_window_set_gravity(GTK_WINDOW(_widget), old_gravity);
  
  *left   = cx - x1;
  *right  = x2 - cx;
  *top    = cy - y1;
  *bottom = y2 - cy;
}

void MathGtkDocumentWindow::get_outer_rect(GdkRectangle *rect) {
  if(!rect || !_widget)
    return;
    
  {
    GdkGravity old_gravity = gtk_window_get_gravity(GTK_WINDOW(_widget));
    gtk_window_set_gravity(GTK_WINDOW(_widget), GDK_GRAVITY_STATIC);
    gtk_window_get_position(GTK_WINDOW(_widget), &rect->x, &rect->y);
    gtk_window_set_gravity(GTK_WINDOW(_widget), old_gravity);
  }
  
  gtk_window_get_size(GTK_WINDOW(_widget), &rect->width, &rect->height);
  
  int l, r, t, b;
  get_window_margins(&l, &r, &t, &b);
  rect->x -= l;
  rect->y -= t;
  rect->width += l + r;
  rect->height += t + b;
}

bool MathGtkDocumentWindow::test_rects_touch(const GdkRectangle &rect1, const GdkRectangle &rect2, int *maxdx, int *maxdy, GdkWindowEdge *edge) {
  static int global_maxdx;
  static int global_maxdy;
  static GdkWindowEdge global_edge;
  
  if(!maxdx) {
    global_maxdx = 0;
    maxdx = &global_maxdx;
  }
  
  if(!maxdy) {
    global_maxdy = 0;
    maxdy = &global_maxdy;
  }
  
  if(!edge)
    edge = &global_edge;
    
  int ldist = abs(rect1.x                - (rect2.x + rect2.width));
  int rdist = abs(rect1.x + rect1.width  - rect2.x);
  int tdist = abs(rect1.y                - (rect2.y + rect2.height));
  int bdist = abs(rect1.y + rect1.height - rect2.y);
  
  int xdir = 0;
  int ydir = 0;
  
  if(ldist <= rdist && ldist <= *maxdx) {
    xdir = -1;
    *maxdx = ldist;
  }
  else if(rdist < ldist && rdist <= *maxdx) {
    xdir = 1;
    *maxdx = rdist;
  }
  
  if(tdist <= bdist && tdist <= *maxdy) {
    ydir = -1;
    *maxdy = tdist;
  }
  else if(bdist < tdist && bdist <= *maxdy) {
    ydir = 1;
    *maxdy = bdist;
  }
  
  if(xdir == 0 && ydir == 0)
    return false;
    
  static GdkWindowEdge results[9] = {
    GDK_WINDOW_EDGE_NORTH_WEST, GDK_WINDOW_EDGE_NORTH, GDK_WINDOW_EDGE_NORTH_EAST,
    GDK_WINDOW_EDGE_WEST,       (GdkWindowEdge) - 1,   GDK_WINDOW_EDGE_EAST,
    GDK_WINDOW_EDGE_SOUTH_WEST, GDK_WINDOW_EDGE_SOUTH, GDK_WINDOW_EDGE_SOUTH_EAST
  };
  
  *edge = results[xdir + 1 + 3 * (ydir + 1)];
  return true;
}

void MathGtkDocumentWindow::move_palettes() {
  if(_snapped_documents.length() < 1)
    return;
    
  const DocumentPosition &pos0 = _snapped_documents[0];
  int dx = _previous_rect.x - pos0.x;
  int dy = _previous_rect.y - pos0.y;
  
  if(dx != 0 || dy != 0) {
    for(int i = 1; i < _snapped_documents.length(); ++i) {
      const DocumentPosition &pos = _snapped_documents[i];
      auto doc = FrontEndObject::find_cast<Document>(pos.id);
      if(!doc)
        continue;
        
      auto wid = dynamic_cast<MathGtkWorkingArea*>(doc->native());
      if(!wid)
        continue;
        
      MathGtkDocumentWindow *win = wid->parent();
      GdkGravity old_gravity = gtk_window_get_gravity(GTK_WINDOW(win->widget()));
      gtk_window_set_gravity(GTK_WINDOW(win->widget()), GDK_GRAVITY_NORTH_WEST);
      gtk_window_move(GTK_WINDOW(win->widget()), _previous_rect.x + pos.x - pos0.x, _previous_rect.y + pos.y - pos0.y);
      
      gtk_window_set_gravity(GTK_WINDOW(win->widget()), old_gravity);
    }
  }
}

bool MathGtkDocumentWindow::on_configure(GdkEvent *e) {
  GdkEventConfigure *event = (GdkEventConfigure *)e;
  
  GdkRectangle client_rect = {event->x, event->y, event->width, event->height};
  {
    GdkGravity old_gravity = gtk_window_get_gravity(GTK_WINDOW(_widget));
    gtk_window_set_gravity(GTK_WINDOW(_widget), GDK_GRAVITY_STATIC);
    gtk_window_get_position(GTK_WINDOW(_widget), &client_rect.x, &client_rect.y);
    gtk_window_set_gravity(GTK_WINDOW(_widget), old_gravity);
  }
  
  bool is_move_only = event->width  == _previous_rect.width
                      && event->height == _previous_rect.height;
                      
  _previous_rect = client_rect;
  
  if(!is_move_only)
    Impl(*this).update_menu_bar_overflow_item(event->width);
  
  if(!is_move_only || !_working_area->is_mouse_down() || _snapped_documents.length() < 1)
    return false;
  
  invalidate_popup_window_positions();
  move_palettes();
  return false;
}

bool MathGtkDocumentWindow::on_delete(GdkEvent *e) {
  switch(document()->get_style(ClosingAction)) {
    case ClosingActionHide: {
        bool all_closed = true;
        for(auto other : CommonDocumentWindow::All) {
          if(auto other_win = dynamic_cast<MathGtkDocumentWindow*>(other)) {
            if(other_win != this && gtk_widget_get_visible(other_win->widget())) {
              all_closed = false;
              break;
            }
          }
        }
        
        if(all_closed)
          break;
        
        document()->style.set(Visible, false);
        invalidate_options();
      }
      return true;
    
    case ClosingActionDelete:
    default:
      break;
  }
  
  if(_has_unsaved_changes && document()->get_style(Saveable)) {
    YesNoCancel answer = ask_save(document());
    
    switch(answer) {
      case YesNoCancel::Yes:
        if(Application::save(document()) == richmath_System_DollarFailed)
          return true;
        break;
      
      case YesNoCancel::No:
        break;
        
      case YesNoCancel::Cancel:
        return true;
    }
  }
  
  if(auto styles = _working_area->stylesheet_document())
    styles->native()->close();
  
  deregister_self();
  return false;
}

bool MathGtkDocumentWindow::on_focus_in(GdkEvent *e) {
//  move_palettes();
  _snapped_documents.length(0);
  _snapped_documents.add(DocumentPosition(FrontEndReference::None, _previous_rect.x, _previous_rect.y));
  
#if GTK_MAJOR_VERSION >= 3
  if(_style_provider) {
    if(gtk_widget_has_screen(_widget)) {
      GdkScreen *screen = gtk_widget_get_screen(_widget);
      
      pmath_debug_print("[remove_provider_for_screen(%p, %p)]\n", screen, _style_provider);
      gtk_style_context_remove_provider_for_screen(screen, _style_provider);
      
      pmath_debug_print("[add_provider_for_screen(%p, %p)]\n", screen, _style_provider);
      gtk_style_context_add_provider_for_screen(screen, _style_provider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    }
  }
#endif

#if GTK_MAJOR_VERSION < 3
  _active = true; // handled by on_window_state() in Gtk >= 3 
#endif

  return false;
}

bool MathGtkDocumentWindow::on_focus_out(GdkEvent *e) {
#if GTK_MAJOR_VERSION >= 3
  if(_style_provider) {
    if(gtk_widget_has_screen(_widget)) {
      GdkScreen *screen = gtk_widget_get_screen(_widget);
      
      pmath_debug_print("[remove_provider_for_screen(%p, %p)]\n", screen, _style_provider);
      gtk_style_context_remove_provider_for_screen(screen, _style_provider);
    }
  }
#endif

#if GTK_MAJOR_VERSION < 3
  _active = false; // handled by on_window_state() in Gtk >= 3 
#endif

  for(auto _win : CommonDocumentWindow::All) {
    if(auto main = dynamic_cast<MathGtkDocumentWindow*>(_win)) {
      main->_snapped_documents.length(0);
      main->_snapped_documents.add(DocumentPosition(FrontEndReference::None, main->_previous_rect.x, main->_previous_rect.y));
      
      if(!main->is_palette()) {
        GdkRectangle rect;
        main->get_outer_rect(&rect);
        
        for(auto _win2 : CommonDocumentWindow::All) {
          if(auto win = dynamic_cast<MathGtkDocumentWindow*>(_win2)) {
            if(win->is_palette()) {
              GdkRectangle other_rect;
              win->get_outer_rect(&other_rect);
              
              int dx = SnapDistance;
              int dy = SnapDistance;
              if(test_rects_touch(rect, other_rect, &dx, &dy, nullptr))
                main->_snapped_documents.add(DocumentPosition(win->document()->id(), other_rect.x, other_rect.y));
            }
          }
        }
      }
    }
  }
  return false;
}

bool MathGtkDocumentWindow::on_scroll(GdkEvent *e) {
  GdkEventScroll *event = (GdkEventScroll *)e;
  
  if(event->state & GDK_CONTROL_MASK) {
    if(event->direction == GDK_SCROLL_UP) {
      _working_area->scale_by(pow(2, 0.5));
    }
    else if(event->direction == GDK_SCROLL_DOWN) {
      _working_area->scale_by(pow(2, -0.5));
    }
    return true;
  }
  
  return false;
}

bool MathGtkDocumentWindow::on_map(GdkEvent *e) {
  Impl(*this).on_auto_hide_menu(true);
  document()->style.set(Visible, true);
  return false;
}

bool MathGtkDocumentWindow::on_unmap(GdkEvent *e) {
  document()->style.set(Visible, false);
  invalidate_popup_window_positions();
  return false;
}

bool MathGtkDocumentWindow::on_window_state(GdkEvent *e) {
  GdkEventWindowState *event = (GdkEventWindowState *)e;

#if GTK_MAJOR_VERSION >= 3
  if(event->changed_mask & GDK_WINDOW_STATE_FOCUSED) {
    if(event->new_window_state & GDK_WINDOW_STATE_FOCUSED) 
      _active = true;
    else
      _active = false;
  }
#endif
  
  return false;
}

//} ... class MathGtkDocumentWindow

//{ class MathGtkDocumentWindowImpl ...

#if GTK_MAJOR_VERSION >= 3
GtkStyleProvider *MathGtkDocumentWindowImpl::global_style_provider = nullptr;
#endif

void MathGtkDocumentWindowImpl::add_remove_window(int delta) {
  static int global_window_count = 0;
  if(global_window_count == 0) {
#if GTK_MAJOR_VERSION >= 3
#  define LN  "\n"
    if(!global_style_provider) {
      global_style_provider = GTK_STYLE_PROVIDER(gtk_css_provider_new());
      gtk_css_provider_load_from_data(
        GTK_CSS_PROVIDER(global_style_provider), 
        ( "menubar.hidden-menu {"                                         LN
          "  opacity: 0;"                                                 LN
          "  margin-bottom:-100px;"                                       LN
          "}"                                                             LN
          "menuitem.menubar-pin > label {"                                LN
          "  background-image: -gtk-icontheme('view-pin-symbolic');"      LN
          "  background-repeat: no-repeat;"                               LN
          "  background-position: center;"                                LN
          "  min-width: 20pt;"                                            LN
          "}"                                                             LN
          "menuitem.menubar-pin:checked > label {"                        LN
          "  background-image: -gtk-icontheme('go-up-symbolic');"         LN
          "}"                                                             LN
          "headerbar headerbar {" /* .pmath-inner-headerbar */            LN
          "  background: none;"                                           LN
          "  border: none;"                                               LN
          "  padding: 0;"                                                 LN
          "  box-shadow: none;"                                           LN
          "}"                                                             LN
          "menuitem entry {"                                              LN
          "  min-height: 0px;"                                            LN
          "  padding-top: 1px;"                                           LN
          "  padding-bottom: 1px;"                                        LN
          "}"
        ), -1,
        nullptr);
      
      // GTK_STYLE_PROVIDER_PRIORITY_APPLICATION does not suffcie for "headerbar"
      gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), global_style_provider, GTK_STYLE_PROVIDER_PRIORITY_USER);
    }
#  undef LN
#endif // GTK_MAJOR_VERSION
  }
  
  global_window_count += delta;
  
  if(global_window_count == 0) {
#if GTK_MAJOR_VERSION >= 3
    if(global_style_provider) {
      gtk_style_context_remove_provider_for_screen(gdk_screen_get_default(), global_style_provider);
      
      g_object_unref(global_style_provider);
      global_style_provider = nullptr;
    }
#endif // GTK_MAJOR_VERSION
  }
}

void MathGtkDocumentWindowImpl::update_scrollbar_overlay() {
  // TODO: check if any of the indicators changed ...
  gtk_widget_queue_draw(self._vscrollbar);
}

gboolean MathGtkDocumentWindowImpl::scrollbar_expose_callback(GtkWidget *scrollbar, GdkEvent *e, void *user_data) {
  GdkEventExpose *event = &e->expose;
  
  cairo_t *cr = gdk_cairo_create(event->window);
  
  cairo_translate(cr, event->area.x, event->area.y);
  
  cairo_move_to(cr, 0, 0);
  cairo_rel_line_to(cr, event->area.width, 0);
  cairo_rel_line_to(cr, 0, event->area.height);
  cairo_rel_line_to(cr, -event->area.width, 0);
  cairo_close_path(cr);
  cairo_clip(cr);
  
  gboolean result = scrollbar_draw_callback(scrollbar, cr, user_data);
  cairo_destroy(cr);
  
  return result;
}

gboolean MathGtkDocumentWindowImpl::scrollbar_draw_callback(GtkWidget *scrollbar, cairo_t *cr, void *user_data) {
  MathGtkDocumentWindow *self = (MathGtkDocumentWindow *)user_data;
  
  Canvas canvas(cr);
  Impl(*self).on_scrollbar_draw(scrollbar, canvas);
  
  return false;
}

void MathGtkDocumentWindowImpl::on_scrollbar_draw(GtkWidget *scrollbar, Canvas &canvas) {
  canvas.line_width(1);
  cairo_set_line_cap(canvas.cairo(), CAIRO_LINE_CAP_BUTT);
  
  gboolean has_back, has_secondary_forward, has_secondary_back, has_forward;
  gtk_widget_style_get(
    scrollbar,
    "has-backward-stepper",           &has_back,
    "has-secondary-forward-stepper",  &has_secondary_forward,
    "has-secondary-backward-stepper", &has_secondary_back,
    "has-forward-stepper",            &has_forward,
    nullptr);
  
  int stepper_size = 0;
  // TODO: use min-height CSS property for GTK >= 3.20
  gtk_widget_style_get(scrollbar, "stepper-size", &stepper_size, nullptr);
  
  int stepper_spacing = 0;
  // TODO: use margin CSS property for GTK >= 3.20
  gtk_widget_style_get(scrollbar, "stepper-spacing", &stepper_spacing, nullptr);
  
  GdkRectangle rect;
  //gtk_range_get_range_rect(GTK_RANGE(scrollbar), &rect);
  gtk_widget_get_allocation(scrollbar, &rect);
  
  int trough_start = 0;
  int trough_end = rect.height;
  
  if(has_back)              trough_start += stepper_size + stepper_spacing;
  if(has_secondary_forward) trough_start += stepper_size + stepper_spacing;
  
  if(has_forward)        trough_end -= stepper_size + stepper_spacing;
  if(has_secondary_back) trough_end -= stepper_size + stepper_spacing;
  
  RectangleF trough_rect {Point(0, trough_start), Point(rect.width, trough_end)};
  
  if(trough_rect.height > 0) {
    Document *doc = self.document();
    GtkAdjustment *adjustment = gtk_range_get_adjustment(GTK_RANGE(scrollbar));
    
    double doc_height = gtk_adjustment_get_upper(adjustment) / doc->native()->scale_factor();
    if(!(doc_height > 0.0))
      doc_height = 1.0;
    
    canvas.set_color(Color::from_rgb24(0x000080));
    if(auto sel = doc->selection_now()) {
      sel.add_path(canvas);
      RectangleF doc_sel_rect = canvas.path_extents();
      canvas.new_path();
      
      float doc_y = doc_sel_rect.y + 0.5 * doc_sel_rect.height;
      float y = trough_rect.y + trough_rect.height * doc_y / doc_height;
      
      RectangleF overlay_rect(Point(trough_rect.x, y), Vector2F(trough_rect.width, 2));
      overlay_rect.pixel_align(canvas, false, 0);
      overlay_rect.add_rect_path(canvas);
      canvas.fill();
    }
    
    canvas.set_color(Color::from_rgb24(0xFF8000));
    for(auto ref : doc->current_word_references()) {
      if(auto sel = ref.get_all()) {
        sel.add_path(canvas);
        RectangleF doc_sel_rect = canvas.path_extents();
        canvas.new_path();
        
        float doc_y = doc_sel_rect.y + 0.5 * doc_sel_rect.height;
        float y = trough_rect.y + trough_rect.height * doc_y / doc_height;
        
        float w = trough_rect.width/3;
        RectangleF overlay_rect(Point(trough_rect.x + trough_rect.width/2, y), Vector2F(0, 0));
        overlay_rect.grow(w/2, w/2);
        overlay_rect.pixel_align(canvas, false, 0);
        overlay_rect.add_rect_path(canvas);
        canvas.fill();
      }
    }
  }
}

void MathGtkDocumentWindowImpl::connect_scrollbar_overlay_signals() {
#if GTK_MAJOR_VERSION >= 3
  g_signal_connect_after(self._vscrollbar, "draw", G_CALLBACK(scrollbar_draw_callback), &self);
#else
  gtk_widget_add_events(self._vscrollbar, GDK_EXPOSURE_MASK);
  g_signal_connect_after(self._vscrollbar, "expose-event", G_CALLBACK(scrollbar_expose_callback), &self);
#endif
}

void MathGtkDocumentWindowImpl::connect_adjustment_changed_signals() {
  g_signal_connect(self._hadjustment, "changed", G_CALLBACK(adjustment_changed_callback), &self);
  g_signal_connect(self._vadjustment, "changed", G_CALLBACK(adjustment_changed_callback), &self);
}

void MathGtkDocumentWindowImpl::connect_menubar_signals() {
  gtk_container_forall(GTK_CONTAINER(self._menu_bar), [](GtkWidget *item, void *_self) {
      MathGtkDocumentWindow &self = *(MathGtkDocumentWindow*)_self;
      if(GTK_IS_MENU_ITEM(item)) {
        if(GtkWidget *menu = gtk_menu_item_get_submenu(GTK_MENU_ITEM(item))) {
          g_signal_connect(
            menu, 
            "map-event", 
            G_CALLBACK((gboolean(*)(GtkWidget*,GdkEvent*,void*))([](GtkWidget *sender, GdkEvent *ev, void *_self) -> gboolean {
              MathGtkDocumentWindow &self = *(MathGtkDocumentWindow*)_self;
              Impl(self).on_auto_hide_menu(false);
              return FALSE;
            })), &self);
          g_signal_connect(
            menu, 
            "unmap-event", 
            G_CALLBACK((gboolean(*)(GtkWidget*,GdkEvent*,void*))([](GtkWidget *sender, GdkEvent *ev, void *_self) -> gboolean {
              MathGtkDocumentWindow &self = *(MathGtkDocumentWindow*)_self;
              Impl(self).on_auto_hide_menu(true);
              return FALSE;
            })), &self);
        }
      }
    }, &self);
  g_signal_connect(
    self._widget, 
    "key-press-event", 
    G_CALLBACK((gboolean(*)(GtkWidget*,GdkEvent*,void*))([](GtkWidget *sender, GdkEvent *e, void *_self) -> gboolean {
      GdkEventKey *event = &e->key;
      MathGtkDocumentWindow &self = *(MathGtkDocumentWindow*)_self;
      
      switch(event->keyval) {
        case GDK_Alt_L:
        case GDK_Alt_R: 
          Impl(self).on_auto_hide_menu(!Impl(self).menu_is_auto_hidden());
          break;
        
        default:
          break;
      }
      return FALSE;
    })), &self);
}

void MathGtkDocumentWindowImpl::adjustment_changed_callback(GtkAdjustment *adjustment, void *user_data) {
  MathGtkDocumentWindow *self = (MathGtkDocumentWindow *)user_data;
  
  Impl(*self).on_adjustment_changed(adjustment);
}

void MathGtkDocumentWindowImpl::on_adjustment_changed(GtkAdjustment *adjustment) {
  GtkWidget *scrollbar = nullptr;
  if(adjustment == self._hadjustment)
    scrollbar = self._hscrollbar;
  else if(adjustment == self._vadjustment)
    scrollbar = self._vscrollbar;
    
  if(!scrollbar)
    return;
    
  double lo, page, hi;
  g_object_get(adjustment,
               "lower",     &lo,
               "page-size", &page,
               "upper",     &hi,
               nullptr);
               
  gtk_widget_set_visible(scrollbar, (self.window_frame() == WindowFrameNormal) && lo + page < hi);
}

void MathGtkDocumentWindowImpl::register_theme_observer() {
#if GTK_MAJOR_VERSION >= 3
  static bool already_registered = false;
  
  if(!already_registered) {
    GtkSettings *settings = gtk_settings_get_default();
    g_signal_connect_after(settings, "notify::gtk-theme-name", G_CALLBACK(on_theme_changed), nullptr);
    already_registered = true;
  }
#endif
}

void MathGtkDocumentWindowImpl::on_theme_changed(GObject*, GParamSpec*) {
  for(auto _win : CommonDocumentWindow::All) {
    if(auto win = dynamic_cast<MathGtkDocumentWindow*>(_win)) {
      win->update_dark_mode();
    }
  }
}

void MathGtkDocumentWindowImpl::append_overflow_menu_item() {
  self._overflow_menu_item = gtk_menu_item_new_with_label("\xC2\xBB"); // U+00BE = utf8: \xC2\xBB
  gtk_menu_shell_append(GTK_MENU_SHELL(self._menu_bar), self._overflow_menu_item);
  gtk_widget_set_visible(self._overflow_menu_item, false);
  
//  gtk_widget_set_events(self._menu_bar, 
//    gtk_widget_get_events(self._menu_bar) | GDK_STRUCTURE_MASK);
//  
//  g_signal_connect(
//    self._menu_bar,
//    "configure-event",
//    G_CALLBACK((gboolean(*)(GtkWidget*,GdkEventConfigure*,void*))[](GtkWidget *menubar, GdkEventConfigure *event, void *_self) -> gboolean {
//      MathGtkDocumentWindow &self = *(MathGtkDocumentWindow*)_self;
//      MathGtkDocumentWindowImpl(self).update_menu_bar_overflow_item(event->width);
//      return false;
//    }),
//    &self);
}

void MathGtkDocumentWindowImpl::update_menu_bar_overflow_item(int max_width) {
  static const char *OrigWidgetKey = "richmath-overflow-orig";
  
  Array<GtkWidget*> items;
  gtk_container_foreach(
    GTK_CONTAINER(self._menu_bar),
    [](GtkWidget *item, void *arr_ptr) {
      Array<GtkWidget*> *arr = (Array<GtkWidget*>*)arr_ptr;
      arr->add(item);
    },
    &items);
  //items = {};
  //return;
  int overflow_index = items.length() - 1;
  while(overflow_index >= 0 && items[overflow_index] != self._overflow_menu_item)
    --overflow_index;
  
  if(overflow_index <= 0)
    return;
  
  Array<int> total_widths;
  total_widths.length(overflow_index + 1);
  
  GtkRequisition size;
  int total_width = 0;
  for(int i = 0; i <= overflow_index; ++i) {
#if GTK_MAJOR_VERSION >= 3
    gtk_widget_get_preferred_size(items[i], nullptr, &size);
#else
    gtk_widget_size_request(items[i], &size);
#endif
    total_width+= size.width;
    total_widths[i] = total_width;
  }
  
  for(int i = overflow_index + 1; i < items.length(); ++i) {
#if GTK_MAJOR_VERSION >= 3
    gtk_widget_get_preferred_size(items[i], nullptr, &size);
#else
    gtk_widget_size_request(items[i], &size);
#endif
    max_width-= size.width;
  }
  
  int last_visible_index = overflow_index - 1;
  if(total_widths[overflow_index - 1] > max_width) {
    max_width-= total_widths[overflow_index] - total_widths[overflow_index - 1];
    while(last_visible_index >= 0 && total_widths[last_visible_index] > max_width)
      --last_visible_index;
  }
  
//  pmath_debug_print(
//    "[show %d items for width %d <= %d]\n", 
//    last_visible_index + 1,
//    total_widths[last_visible_index],
//    max_width);
  
  GtkWidget *overflow_menu = gtk_menu_item_get_submenu(GTK_MENU_ITEM(self._overflow_menu_item));
  if(!overflow_menu) {
    overflow_menu = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(self._overflow_menu_item), overflow_menu);
  }
  
  Array<GtkWidget*> overflow_items;
  gtk_container_foreach(
    GTK_CONTAINER(overflow_menu),
    [](GtkWidget *item, void *arr_ptr) {
      Array<GtkWidget*> *arr = (Array<GtkWidget*>*)arr_ptr;
      arr->add(item);
    },
    &overflow_items);
  
  for(auto overflow_item : overflow_items) {
    GtkWidget *orig = (GtkWidget*)g_object_get_data(G_OBJECT(overflow_item), OrigWidgetKey);
    if(orig) {
      GtkWidget *submenu = gtk_menu_item_get_submenu(GTK_MENU_ITEM(overflow_item));
      if(submenu) {
        g_object_ref(G_OBJECT(submenu));
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(overflow_item), nullptr);
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(orig), submenu);
        g_object_unref(G_OBJECT(submenu));
      }
      else {
        pmath_debug_print(
          "[no submenu found for overflow item %s]", 
          gtk_menu_item_get_label(GTK_MENU_ITEM(overflow_item)));
      }
      
      g_object_set_data(G_OBJECT(overflow_item), OrigWidgetKey, nullptr);
    }
  }
  
  for(int i = 0; i <= last_visible_index; ++i)
    gtk_widget_show(items[i]);
  
  int num_overflow = overflow_index - (last_visible_index + 1);
  for(int i = 0; i < num_overflow; ++i) {
    GtkWidget *orig_item     = items[last_visible_index + 1 + i];
    GtkWidget *overflow_item;
    
    if(i < overflow_items.length()) {
      overflow_item = overflow_items[i];
    }
    else {
      overflow_item = gtk_menu_item_new();
      gtk_menu_shell_append(GTK_MENU_SHELL(overflow_menu), overflow_item);
      gtk_menu_item_set_use_underline(GTK_MENU_ITEM(overflow_item), true);
    }
    
    g_object_set_data(G_OBJECT(overflow_item), OrigWidgetKey, orig_item);
    gtk_menu_item_set_label(
      GTK_MENU_ITEM(overflow_item), 
      gtk_menu_item_get_label(GTK_MENU_ITEM(orig_item)));
    
    GtkWidget *submenu = gtk_menu_item_get_submenu(GTK_MENU_ITEM(orig_item));
    if(submenu) {
      g_object_ref(G_OBJECT(submenu));
      gtk_menu_item_set_submenu(GTK_MENU_ITEM(orig_item), nullptr);
      gtk_menu_item_set_submenu(GTK_MENU_ITEM(overflow_item), submenu);
      g_object_unref(G_OBJECT(submenu));
    }
    else
      gtk_menu_item_set_submenu(GTK_MENU_ITEM(overflow_item), nullptr);
    
    gtk_widget_show(overflow_item);
    gtk_widget_hide(orig_item);
  }
  
  for(int i = num_overflow; i < overflow_items.length(); ++i) {
    gtk_widget_hide(overflow_items[i]);
  }
  
  gtk_widget_set_visible(self._overflow_menu_item, last_visible_index + 1 < overflow_index);
}

void MathGtkDocumentWindowImpl::append_menu_bar_pin() {
#if GTK_MAJOR_VERSION >= 3
  self._menu_bar_pin = gtk_menu_item_new_with_mnemonic("    "); // "_pin"
  gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(self._menu_bar_pin)), "menubar-pin");
  gtk_menu_item_set_right_justified(GTK_MENU_ITEM(self._menu_bar_pin), true);
  gtk_menu_shell_append(GTK_MENU_SHELL(self._menu_bar), self._menu_bar_pin);
  
  g_signal_connect(
    self._menu_bar_pin, 
    "activate", 
    G_CALLBACK(activate_pin_item_callback), 
    &self);
  g_signal_connect(
    self._menu_bar_pin, 
    "enter-notify-event", 
    G_CALLBACK((gboolean(*)(GtkWidget*,GdkEvent*,void*))[](GtkWidget *menu_item, GdkEvent *ev, void *_self) -> gboolean { 
      MathGtkDocumentWindow &self = *(MathGtkDocumentWindow*)_self;
      gtk_menu_shell_select_item(GTK_MENU_SHELL(self._menu_bar), menu_item);
      return false;
    }), 
    &self);
#endif
}

void MathGtkDocumentWindowImpl::activate_pin_item_callback(GtkMenuItem *menu_item, void *user_data) {
  MathGtkDocumentWindow *self = (MathGtkDocumentWindow *)user_data;
  
  Impl(*self).on_activate_pin_item(menu_item);
}

void MathGtkDocumentWindowImpl::on_activate_pin_item(GtkMenuItem *menu_item) {
#if GTK_MAJOR_VERSION >= 3
  GtkStateFlags flags = gtk_widget_get_state_flags(GTK_WIDGET(menu_item));
  
  if(flags & GTK_STATE_FLAG_CHECKED) {
    pmath_debug_print("[on_activate_pin_item: was checked]\n");
    
    gtk_widget_unset_state_flags(GTK_WIDGET(menu_item), GTK_STATE_FLAG_CHECKED);
    on_auto_hide_menu(true);
  }
  else {
    pmath_debug_print("[on_activate_pin_item: was unchecked]\n");
    
    gtk_widget_set_state_flags(GTK_WIDGET(menu_item), GTK_STATE_FLAG_CHECKED, false);
  }
#endif
}

bool MathGtkDocumentWindowImpl::menu_is_auto_hidden() {
#if GTK_MAJOR_VERSION >= 3
  GtkStyleContext *menu_bar_context = gtk_widget_get_style_context(self._menu_bar);
  return gtk_style_context_has_class(menu_bar_context, "hidden-menu");
#else
  return false;
#endif
}

void MathGtkDocumentWindowImpl::on_auto_hide_menu(bool hide) {
#if GTK_MAJOR_VERSION >= 3
  if(self._menu_bar_pin && (gtk_widget_get_state_flags(self._menu_bar_pin) & GTK_STATE_FLAG_CHECKED)) {
    hide = false;
  }

  GtkStyleContext *menu_bar_context = gtk_widget_get_style_context(self._menu_bar);
  if(gtk_style_context_has_class(menu_bar_context, "hidden-menu")) {
    if(!hide) {
      gtk_style_context_remove_class(menu_bar_context, "hidden-menu");
    }
  }
  else {
    if(hide) {
      gtk_style_context_add_class(menu_bar_context, "hidden-menu");
    }
  }
#endif
}

//} ... class MathGtkDocumentWindowImpl
