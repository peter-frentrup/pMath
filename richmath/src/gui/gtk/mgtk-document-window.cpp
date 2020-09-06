#include <gui/gtk/mgtk-document-window.h>

#include <boxes/section.h>

#include <eval/application.h>
#include <eval/binding.h>

#include <gui/gtk/mgtk-control-painter.h>
#include <gui/gtk/mgtk-menu-builder.h>
#include <gui/messagebox.h>

#include <cmath>

#if GTK_MAJOR_VERSION >= 3
#  include <gdk/gdkkeysyms-compat.h>
#else
#  include <gdk/gdkkeysyms.h>
#endif

#ifdef GDK_WINDOWING_X11
#  include <gdk/gdkx.h>
#  ifdef None
#    undef None
#  endif
#endif

using namespace richmath;

static const int SnapDistance = 4;

class MathGtkDocumentChildWidget: public MathGtkWidget {
    using base = MathGtkWidget;
    friend class MathGtkDocumentWindow;
  public:
    MathGtkDocumentChildWidget(MathGtkDocumentWindow *parent)
      : base(new Document()),
        _parent(parent)
    {
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
    
    virtual String window_title() override { return _parent->title(); }
    
    virtual void on_idle_after_edit() override { 
      base::on_idle_after_edit();
      _parent->on_idle_after_edit(this); 
    }
    virtual void on_saved() override { _parent->on_saved(); }
    
    virtual bool is_foreground_window() override { return _parent->is_foreground_window(); }
    virtual bool is_using_dark_mode() override { return _parent->is_using_dark_mode(); }
    
  private:
    MathGtkDocumentWindow *_parent;
    
  protected:
    virtual void do_set_current_document() override {
      set_current_document(_parent->document());
    }
};

class richmath::MathGtkWorkingArea: public MathGtkDocumentChildWidget {
    using base = MathGtkDocumentChildWidget;
    friend class MathGtkDocumentWindow;
  public:
    MathGtkWorkingArea(MathGtkDocumentWindow *parent)
      : base(parent)
    {
    }
    
    virtual void page_size(float *w, float *h) override {
      base::page_size(w, h);
      
      if(parent()->window_frame() != WindowFrameNormal)
        *w = HUGE_VAL;
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
          
          gtk_window_resize(GTK_WINDOW(parent()->widget()), 1, 1);
          
          if(!was_resizable)
            gtk_window_set_resizable(GTK_WINDOW(parent()->widget()), false);
        }
      }
    }
    
    virtual bool on_draw(cairo_t *cr) override {
      bool result = base::on_draw(cr);
      rearrange();
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
      Style::reset(document()->style, "Docked");
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
    
    virtual void page_size(float *w, float *h) override {
      base::page_size(w, h);
      
      *w = HUGE_VAL;
    }
    
    virtual bool is_scrollable() override { return false; }
    
    virtual void invalidate() override {
      if(document()->length() > 0) {
        gtk_widget_set_size_request(_widget, 1, 1);
        if(!gtk_widget_get_visible(_widget))
          gtk_widget_set_visible(_widget, TRUE);
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
      
      document()->style->set(Editable,           false); // redirect Print() to console
      document()->style->set(Selectable,         AutoBoolFalse);
      document()->style->set(ShowSectionBracket, AutoBoolFalse);
      
      document()->select(0, 0, 0);
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

namespace richmath {
  class MathGtkDocumentWindow::Impl {
    public:
      Impl(MathGtkDocumentWindow &self) : self(self) {}
      
      static void add_remove_window(int delta);
      
      void connect_adjustment_changed_signals();
      void connect_menubar_signals();
      static void register_theme_observer();
      
      void append_menu_bar_pin();
      bool menu_is_auto_hidden();
      void on_auto_hide_menu(bool hide);
    
    private:
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
}

//{ class MathGtkDocumentWindow ...

MathGtkDocumentWindow::MathGtkDocumentWindow()
  : BasicGtkWidget(),
    _menu_bar(nullptr),
    _hadjustment(GTK_ADJUSTMENT(gtk_adjustment_new(0, 0, 0, 0, 0, 0))),
    _vadjustment(GTK_ADJUSTMENT(gtk_adjustment_new(0, 0, 0, 0, 0, 0))),
    _hscrollbar(nullptr),
    _vscrollbar(nullptr),
    _table(nullptr),
#if GTK_MAJOR_VERSION >= 3
    _style_provider(nullptr),
    _menu_bar_pin(nullptr),
#endif
    _window_frame(WindowFrameNormal),
    _active(true),
    _use_dark_mode(false)
{
  Impl::add_remove_window(+1);
  
  _previous_rect.x = 0;
  _previous_rect.y = 0;
  _previous_rect.width = 1;
  _previous_rect.height = 1;
  
  _working_area = new MathGtkWorkingArea(this);
  _top_area     = new MathGtkDock(this);
  _bottom_area  = new MathGtkDock(this);
  
  _content = _working_area->document();
}

void MathGtkDocumentWindow::after_construction() {
  if(!_widget)
    _widget = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    
  BasicGtkWidget::after_construction();
  
  gtk_window_set_icon_list(GTK_WINDOW(_widget), icons.get_app_icon_list());
  
  _working_area->init();
  _top_area->init();
  _bottom_area->init();
  
  top()->main_document    = document();
  bottom()->main_document = document();
  
  GtkAccelGroup *accel_group = gtk_accel_group_new();
  
  _menu_bar = gtk_menu_bar_new();
  MathGtkMenuBuilder::main_menu.append_to(GTK_MENU_SHELL(_menu_bar), accel_group, document()->id());
  Impl(*this).append_menu_bar_pin();
  MathGtkAccelerators::connect_all(accel_group, document()->id());
  
  gtk_window_add_accel_group(GTK_WINDOW(_widget), accel_group);
  g_object_unref(accel_group);
  
  _table = gtk_table_new(2, 5, FALSE);
  gtk_container_add(GTK_CONTAINER(_widget), _table);
  
  gtk_table_attach(GTK_TABLE(_table), _menu_bar,               0, 2, 0, 1, (GtkAttachOptions)(GTK_EXPAND | GTK_FILL | GTK_SHRINK), (GtkAttachOptions)0,                                    0, 0);
  gtk_table_attach(GTK_TABLE(_table), _top_area->widget(),     0, 2, 1, 2, (GtkAttachOptions)(GTK_EXPAND | GTK_FILL | GTK_SHRINK), (GtkAttachOptions)0,                                    0, 0);
  gtk_table_attach(GTK_TABLE(_table), _working_area->widget(), 0, 1, 2, 3, (GtkAttachOptions)(GTK_EXPAND | GTK_FILL | GTK_SHRINK), (GtkAttachOptions)(GTK_EXPAND | GTK_FILL | GTK_SHRINK), 0, 0);
  gtk_table_attach(GTK_TABLE(_table), _bottom_area->widget(),  0, 2, 4, 5, (GtkAttachOptions)(GTK_EXPAND | GTK_FILL | GTK_SHRINK), (GtkAttachOptions)0,                                    0, 0);
  
  g_object_ref(_hadjustment);
  g_object_ref(_vadjustment);
  _hscrollbar = gtk_hscrollbar_new(_hadjustment);
  _vscrollbar = gtk_vscrollbar_new(_vadjustment);
  gtk_table_attach(GTK_TABLE(_table), _vscrollbar, 1, 2, 2, 3, (GtkAttachOptions)0, (GtkAttachOptions)(GTK_EXPAND | GTK_FILL | GTK_SHRINK), 0, 0);
  gtk_table_attach(GTK_TABLE(_table), _hscrollbar, 0, 1, 3, 4, (GtkAttachOptions)(GTK_EXPAND | GTK_FILL | GTK_SHRINK), (GtkAttachOptions)0,                            0, 0);
  
  Impl(*this).connect_adjustment_changed_signals();
  Impl(*this).connect_menubar_signals();
  
  g_object_ref(_hadjustment);
  g_object_ref(_vadjustment);
  _working_area->hadjustment(_hadjustment);
  _working_area->vadjustment(_vadjustment);
  
  gtk_widget_show_all(_table);
  gtk_widget_set_can_focus(_widget, FALSE);
  
  GList *focus_chain = nullptr;
  focus_chain = g_list_prepend(focus_chain, _working_area->widget());
  gtk_container_set_focus_chain(GTK_CONTAINER(_table), focus_chain);
  g_list_free(focus_chain);
  
  gtk_window_set_default_size(GTK_WINDOW(_widget), 500, 550);
  
  top()->invalidate();
  bottom()->invalidate();
  
  if(get_current_document() == 0) 
    set_current_document(document());
  
  signal_connect<MathGtkDocumentWindow, GdkEvent *, &MathGtkDocumentWindow::on_configure>("configure-event");
  signal_connect<MathGtkDocumentWindow, GdkEvent *, &MathGtkDocumentWindow::on_delete>("delete-event");
  signal_connect<MathGtkDocumentWindow, GdkEvent *, &MathGtkDocumentWindow::on_focus_in>("focus-in-event");
  signal_connect<MathGtkDocumentWindow, GdkEvent *, &MathGtkDocumentWindow::on_focus_out>("focus-out-event");
  signal_connect<MathGtkDocumentWindow, GdkEvent *, &MathGtkDocumentWindow::on_scroll>("scroll-event");
  signal_connect<MathGtkDocumentWindow, GdkEvent *, &MathGtkDocumentWindow::on_window_state>("window-state-event");

  title(String());
  
  working_area()->document()->style->set(Visible,                         true);
  working_area()->document()->style->set(InternalHasModifiedWindowOption, true);
  update_dark_mode();
  Impl::register_theme_observer();
  Impl(*this).on_auto_hide_menu(true);
}

MathGtkDocumentWindow::~MathGtkDocumentWindow() {
  static bool deleting_all = false;
  if(!deleting_all) {
    bool have_only_palettes = true;
    for(auto _win : CommonDocumentWindow::All) {
      if(auto win = dynamic_cast<MathGtkDocumentWindow*>(_win)) {
        if(win == this)
          continue;
        
        if(!win->is_palette() && win->document()->get_style(Visible, true)) {
          have_only_palettes = false;
          break;
        }
      }
    }
    
    if(have_only_palettes) {
      deleting_all = true;
      
      CommonDocumentWindow *other = next_window();
      while(other && other != this) {
        CommonDocumentWindow *next = other->next_window();
        
        if(auto win = dynamic_cast<MathGtkDocumentWindow*>(other))
          win->destroy();
//        else {
//          //delete win ?
//        }
        
        other = next;
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

template<typename Func>
static void forall_widgets_recursive(GtkWidget *widget, Func func) {
//  static int debug_depth = 0;
//  
//  for(int i = debug_depth;i > 0;--i)
//    pmath_debug_print("  ");
//  
//  pmath_debug_print("%s\n", G_OBJECT_TYPE_NAME(widget));
  
  func(widget);
  if(GTK_IS_CONTAINER(widget)) {
//    ++debug_depth;
    gtk_container_forall(
      GTK_CONTAINER(widget),
      [](GtkWidget *child, void *func_ptr) {
        forall_widgets_recursive(child, *(Func*)func_ptr);
      },
      &func);
//    --debug_depth;
  }
  
//  // TODO: menus should probably be handled when they map/unmap only, menu item labels are not correctly styled
//  if(GTK_IS_MENU_ITEM(widget)) {
//    if(GtkWidget *menu = gtk_menu_item_get_submenu(GTK_MENU_ITEM(widget))) {
//      ++debug_depth;
//      forall_widgets_recursive(menu, func);
//      --debug_depth;
//    }
//  }
}

void MathGtkDocumentWindow::update_dark_mode() {
#if GTK_MAJOR_VERSION >= 3
  if(_style_provider) {
    forall_widgets_recursive(
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
  
  forall_widgets_recursive(
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
    _top_area->document()->invalidate_all();
    _bottom_area->document()->invalidate_all();
  }
  
  String s = doc->get_style(WindowTitle, _default_title);
  if(!_title.unobserved_equals(s))
    title(s);
  
  WindowFrameType f = (WindowFrameType)doc->get_style(WindowFrame, _window_frame);
  if(_window_frame != f)
    window_frame(f);
    
  Expr top          = SectionList::group(doc->get_style(DockedSectionsTop));
  Expr top_glass    = SectionList::group(doc->get_style(DockedSectionsTopGlass));
  Expr bottom       = SectionList::group(doc->get_style(DockedSectionsBottom));
  Expr bottom_glass = SectionList::group(doc->get_style(DockedSectionsBottomGlass));
  
  _top_area->document()->stylesheet(doc->stylesheet());
  _bottom_area->document()->stylesheet(doc->stylesheet());
    
  _top_area->reload(   SectionList::group(List(top_glass, top)));
  _bottom_area->reload(SectionList::group(List(bottom, bottom_glass)));
  
  if(doc->get_style(Visible, true)) {
    //gtk_window_present(GTK_WINDOW(_widget));
    
    if(!gtk_widget_get_visible(_widget))
      gtk_widget_show(_widget);
  }
  else {
    if(gtk_widget_get_visible(_widget))
      gtk_widget_hide(_widget);
  }
  
  float scale = doc->get_style(Magnification, _working_area->custom_scale_factor());
  _working_area->set_custom_scale(scale);
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

void MathGtkDocumentWindow::window_frame(WindowFrameType type) {
  if(!_widget) {
    _window_frame = type;
    return;
  }
  
  //gtk_window_set_resizable(   GTK_WINDOW(_widget), type == WindowFrameNormal);
  gtk_window_set_focus_on_map(GTK_WINDOW(_widget), type == WindowFrameNormal);
  
  gtk_widget_set_visible(_menu_bar, type == WindowFrameNormal);
  
  _working_area->_autohide_vertical_scrollbar = type == WindowFramePalette || type == WindowFrameDialog;
  
  if(_window_frame != type) {
//    GdkWindow *gdk = gtk_widget_get_window(_widget);
    
    _working_area->invalidate();
    
    bool was_mapped = gtk_widget_get_mapped(_widget);
    if(was_mapped) {
      gtk_widget_set_mapped(_widget, false);
    }
//    int x, y;
//    bool was_visible = gtk_widget_get_visible(_widget);
//    if(was_visible) {
//      gtk_window_get_position(GTK_WINDOW(_widget), &x, &y);
//      gtk_widget_hide(_widget);
//      //gdk_window_hide(gdk);
//    }
    
    switch(type) {
      case WindowFrameNormal:
        gtk_window_set_type_hint(        GTK_WINDOW(_widget), GDK_WINDOW_TYPE_HINT_NORMAL);
        gtk_window_set_skip_taskbar_hint(GTK_WINDOW(_widget), false);
        break;
        
      case WindowFramePalette:
        gtk_window_set_type_hint(        GTK_WINDOW(_widget), GDK_WINDOW_TYPE_HINT_UTILITY);
        gtk_window_set_skip_taskbar_hint(GTK_WINDOW(_widget), true);
        break;
        
      case WindowFrameDialog:
        gtk_window_set_type_hint(        GTK_WINDOW(_widget), GDK_WINDOW_TYPE_HINT_DIALOG);
        gtk_window_set_skip_taskbar_hint(GTK_WINDOW(_widget), true);
        break;
    }
    
    if(was_mapped) {
      gtk_widget_set_mapped(_widget, true);
    }
//    if(was_visible) {
//      //gdk_window_hide(gdk);
//      gtk_widget_show(_widget);
//      gtk_window_move(GTK_WINDOW(_widget), x, y);
//    }
  }
  
  _window_frame = type;
}

void MathGtkDocumentWindow::run_menucommand(Expr cmd) {
  Application::run_menucommand(std::move(cmd));
}

void MathGtkDocumentWindow::bring_to_front() {
  gtk_window_present(GTK_WINDOW(_widget));
}

void MathGtkDocumentWindow::close() {
  if(!_widget || destroying())
    return;
  
  GdkEvent *ev = gdk_event_new(GDK_DELETE);
  if(!gtk_widget_event(_widget, ev)) {
    destroy();
  }
  gdk_event_free(ev);
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
  
  if(!is_move_only || !_working_area->is_mouse_down() || _snapped_documents.length() < 1)
    return false;
    
  move_palettes();
  
  return false;
}

bool MathGtkDocumentWindow::on_delete(GdkEvent *e) {
  if(_has_unsaved_changes && document()->get_style(Saveable)) {
    YesNoCancel answer = ask_save(document());
    
    switch(answer) {
      case YesNoCancel::Yes:
        if(Application::save(document()) == PMATH_SYMBOL_FAILED)
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

//{ class MathGtkDocumentWindow::Impl ...

#if GTK_MAJOR_VERSION >= 3
GtkStyleProvider *MathGtkDocumentWindow::Impl::global_style_provider = nullptr;
#endif

void MathGtkDocumentWindow::Impl::add_remove_window(int delta) {
  static int global_window_count = 0;
  if(global_window_count == 0) {
#if GTK_MAJOR_VERSION >= 3
    if(!global_style_provider) {
      global_style_provider = GTK_STYLE_PROVIDER(gtk_css_provider_new());
      gtk_css_provider_load_from_data(
        GTK_CSS_PROVIDER(global_style_provider), 
        ( "menubar.hidden-menu {"
          "  opacity: 0;"
          "  margin-bottom:-100px;"
          "}"
          "menuitem.menubar-pin > label {"
          "  background-image: -gtk-icontheme('view-pin-symbolic');"
          "  background-repeat: no-repeat;"
          "  background-position: center;"
          "  min-width: 20pt;"
          "}"
          "menuitem.menubar-pin:checked > label {"
          "  background-image: -gtk-icontheme('go-up-symbolic');"
          "}"
          //"menuitem.menubar-pin > label {"
          //"  opacity:0.5;"
          //"}"
        ), -1,
        nullptr);
      
      gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), global_style_provider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    }
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

void MathGtkDocumentWindow::Impl::connect_adjustment_changed_signals() {
  g_signal_connect(self._hadjustment, "changed", G_CALLBACK(adjustment_changed_callback), &self);
  g_signal_connect(self._vadjustment, "changed", G_CALLBACK(adjustment_changed_callback), &self);
}

void MathGtkDocumentWindow::Impl::connect_menubar_signals() {
  gtk_container_forall(GTK_CONTAINER(self._menu_bar), [](GtkWidget *item, void *_self) {
      MathGtkDocumentWindow &self = *(MathGtkDocumentWindow*)_self;
      if(GTK_IS_MENU_ITEM(item)) {
        if(GtkWidget *menu = gtk_menu_item_get_submenu(GTK_MENU_ITEM(item))) {
          g_signal_connect(
            menu, 
            "map-event", 
            G_CALLBACK((gboolean(*)(GtkWidget*,GdkEvent*,void*))([](GtkWidget *sender, GdkEvent *ev, void *_self) -> gboolean {
              MathGtkDocumentWindow &self = *(MathGtkDocumentWindow*)_self;
              pmath_debug_print("[menu: map]\n");
              Impl(self).on_auto_hide_menu(false);
              return FALSE;
            })), &self);
          g_signal_connect(
            menu, 
            "unmap-event", 
            G_CALLBACK((gboolean(*)(GtkWidget*,GdkEvent*,void*))([](GtkWidget *sender, GdkEvent *ev, void *_self) -> gboolean {
              MathGtkDocumentWindow &self = *(MathGtkDocumentWindow*)_self;
              pmath_debug_print("[menu: unmap]\n");
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

void MathGtkDocumentWindow::Impl::adjustment_changed_callback(GtkAdjustment *adjustment, void *user_data) {
  MathGtkDocumentWindow *self = (MathGtkDocumentWindow *)user_data;
  
  Impl(*self).on_adjustment_changed(adjustment);
}

void MathGtkDocumentWindow::Impl::on_adjustment_changed(GtkAdjustment *adjustment) {
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

void MathGtkDocumentWindow::Impl::register_theme_observer() {
#if GTK_MAJOR_VERSION >= 3
  static bool already_registered = false;
  
  if(!already_registered) {
    GtkSettings *settings = gtk_settings_get_default();
    g_signal_connect_after(settings, "notify::gtk-theme-name", G_CALLBACK(on_theme_changed), nullptr);
    already_registered = true;
  }
#endif
}

void MathGtkDocumentWindow::Impl::on_theme_changed(GObject*, GParamSpec*) {
  for(auto _win : CommonDocumentWindow::All) {
    if(auto win = dynamic_cast<MathGtkDocumentWindow*>(_win)) {
      win->update_dark_mode();
    }
  }
}

void MathGtkDocumentWindow::Impl::append_menu_bar_pin() {
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
      return FALSE;
    }), 
    &self);
#endif
}

void MathGtkDocumentWindow::Impl::activate_pin_item_callback(GtkMenuItem *menu_item, void *user_data) {
  MathGtkDocumentWindow *self = (MathGtkDocumentWindow *)user_data;
  
  Impl(*self).on_activate_pin_item(menu_item);
}

void MathGtkDocumentWindow::Impl::on_activate_pin_item(GtkMenuItem *menu_item) {
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

bool MathGtkDocumentWindow::Impl::menu_is_auto_hidden() {
#if GTK_MAJOR_VERSION >= 3
  GtkStyleContext *menu_bar_context = gtk_widget_get_style_context(self._menu_bar);
  return gtk_style_context_has_class(menu_bar_context, "hidden-menu");
#else
  return false;
#endif
}

void MathGtkDocumentWindow::Impl::on_auto_hide_menu(bool hide) {
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

//} ... class MathGtkDocumentWindow::Impl
