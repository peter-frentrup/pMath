#include <gui/gtk/mgtk-attached-popup-window.h>

#include <gui/documents.h>
#include <gui/gtk/mgtk-menu-builder.h>

#include <cmath>


namespace richmath {
  namespace strings {
    extern String AttachedPopupWindow;
  }
  
  class MathGtkAttachedPopupWindow::Impl {
    public:
      Impl(MathGtkAttachedPopupWindow &self) : self{self} {}
      
      bool find_anchor_screen_position(int &x, int &y);
      
    private:
      MathGtkAttachedPopupWindow &self;
  };
  
  class MathGtkPopupContentArea final : public MathGtkWidget {
      using base = MathGtkWidget;
    public:
      MathGtkPopupContentArea(MathGtkAttachedPopupWindow *parent, Document *owner, Box *anchor);
      
      int best_width() { return _best_width; }
      int best_height() { return _best_height; }
      
      MathGtkWidget *owner_widget();
      
      virtual void close() override { _parent->close(); }
      virtual void invalidate_options() override;
      virtual void invalidate_source_location() override;
      
      virtual bool is_foreground_window() override { return _parent->is_foreground_window(); };
      virtual bool is_focused_widget() override { return _parent->is_focused_widget(); };
      virtual bool is_using_dark_mode() override { return _parent->is_using_dark_mode(); }
      virtual int dpi() override { return _parent->dpi(); }
      
    protected:
      ~MathGtkPopupContentArea();
      virtual void after_construction() override;
    
      virtual void paint_background(Canvas &canvas) override;
      virtual void paint_canvas(Canvas &canvas, bool resize_only) override;
      virtual void do_set_current_document() override;
      
    private:
      MathGtkAttachedPopupWindow *_parent;
      int                         _best_width;
      int                         _best_height;
  };
}

using namespace richmath;

#if GTK_MAJOR_VERSION < 3
static int gtk_widget_get_allocated_width(GtkWidget *widget);
static int gtk_widget_get_allocated_height(GtkWidget *widget);
#endif

//{ class MathGtkAttachedPopupWindow ...

MathGtkAttachedPopupWindow::MathGtkAttachedPopupWindow(Document *owner, Box *anchor) 
  : base(),
  _hadjustment{GTK_ADJUSTMENT(gtk_adjustment_new(0, 0, 0, 0, 0, 0))},
  _vadjustment{GTK_ADJUSTMENT(gtk_adjustment_new(0, 0, 0, 0, 0, 0))},
  _hscrollbar{nullptr},
  _vscrollbar{nullptr},
  _content_area{new MathGtkPopupContentArea(this, owner, anchor)},
  _appearance{ContainerType::None},
  _active{false}
{
}

MathGtkAttachedPopupWindow::~MathGtkAttachedPopupWindow() {
  g_object_unref(_hadjustment);
  g_object_unref(_vadjustment);
}

void MathGtkAttachedPopupWindow::after_construction() {
  if(!_widget)
    _widget = gtk_window_new(GTK_WINDOW_TOPLEVEL); // GTK_WINDOW_TOPLEVEL to support keyboard focus
  
  /*  With GTK_WINDOW_TOPLEVEL, GTK3 (on WSL with Xming) will temporarily add window decorations
      whenever the window is shown.
      But GTK_WINDOW_TOPLEVEL is necessary to allow keyboard focus.
      
      TODO: maybe we should use GdkWindow directly? See also GtkPopover
   */
  
  //gtk_window_set_type_hint(GTK_WINDOW(_widget), GDK_WINDOW_TYPE_HINT_COMBO);
  gtk_window_set_type_hint(GTK_WINDOW(_widget), GDK_WINDOW_TYPE_HINT_POPUP_MENU);
  gtk_window_set_decorated(GTK_WINDOW(_widget), false);
  gtk_window_set_focus_on_map(GTK_WINDOW(_widget), false); // might be ignored by window manager
//  gtk_window_set_accept_focus(GTK_WINDOW(_widget), false);
//  gtk_widget_set_can_focus(_widget, FALSE);
  gtk_window_set_default_size(GTK_WINDOW(_widget), 1, 1);
//  gtk_window_set_resizable(GTK_WINDOW(_widget), false);
  
  base::after_construction();
  
  _content_area->init();
  
//#if GTK_MAJOR_VERSION >= 3
//  {
//    GtkStyleContext *style_context = gtk_widget_get_style_context(_widget);
//    gtk_style_context_
//    gtk_style_context_add_class(style_context, "popup");
//    //gtk_style_context_add_class(style_context, "frame");
//  }
//#endif

  if(MathGtkWidget *owner_wid = _content_area->owner_widget()) {
    GtkWindow *owner_win = GTK_WINDOW(gtk_widget_get_ancestor(owner_wid->widget(), GTK_TYPE_WINDOW));
    gtk_window_set_transient_for(GTK_WINDOW(_widget), owner_win);
    //gtk_window_set_attached_to(GTK_WINDOW(_widget), owner_wid->widget());
  }
  
  GtkAccelGroup *accel_group = gtk_accel_group_new();
  MathGtkAccelerators::connect_all(accel_group, content()->id());
  gtk_window_add_accel_group(GTK_WINDOW(_widget), accel_group);
  g_object_unref(accel_group);
  
  GtkWidget *table = gtk_table_new(2, 2, FALSE);
  gtk_container_add(GTK_CONTAINER(_widget), table);
  
  gtk_table_attach(
    GTK_TABLE(table), 
    _content_area->widget(), 
    0, 1, 0, 1, 
    (GtkAttachOptions)(GTK_EXPAND | GTK_FILL | GTK_SHRINK), 
    (GtkAttachOptions)(GTK_EXPAND | GTK_FILL | GTK_SHRINK), 
    0, 0);
  
  g_object_ref(_hadjustment);
  g_object_ref(_vadjustment);
  _hscrollbar = gtk_hscrollbar_new(_hadjustment);
  _vscrollbar = gtk_vscrollbar_new(_vadjustment);
  
  gtk_table_attach(
    GTK_TABLE(table), 
    _vscrollbar, 
    1, 2, 0, 1, 
    (GtkAttachOptions)0, 
    (GtkAttachOptions)(GTK_EXPAND | GTK_FILL | GTK_SHRINK), 
    0, 0);
  gtk_table_attach(
    GTK_TABLE(table), 
    _hscrollbar, 
    0, 1, 1, 2,
    (GtkAttachOptions)(GTK_EXPAND | GTK_FILL | GTK_SHRINK), 
    (GtkAttachOptions)0,
    0, 0);
  
  g_object_ref(_hadjustment);
  g_object_ref(_vadjustment);
  _content_area->hadjustment(_hadjustment);
  _content_area->vadjustment(_vadjustment);
  
//  gtk_widget_set_app_paintable(_widget, true);
#if GTK_MAJOR_VERSION >= 3
  signal_connect<MathGtkAttachedPopupWindow, cairo_t *, &MathGtkAttachedPopupWindow::on_draw>("draw");
#else
  signal_connect<MathGtkAttachedPopupWindow, GdkEvent *, &MathGtkAttachedPopupWindow::on_expose>("expose-event");
#endif

  signal_connect<MathGtkAttachedPopupWindow, GdkEvent *, &MathGtkAttachedPopupWindow::on_configure>("configure-event");
  signal_connect<MathGtkAttachedPopupWindow, GdkEvent *, &MathGtkAttachedPopupWindow::on_delete>("delete-event");
  signal_connect<MathGtkAttachedPopupWindow, GdkEvent *, &MathGtkAttachedPopupWindow::on_unmap>("unmap-event");
  signal_connect<MathGtkAttachedPopupWindow, GdkEvent *, &MathGtkAttachedPopupWindow::on_window_state>("window-state-event");

  gtk_widget_show_all(table);
  
  GList *focus_chain = nullptr;
  focus_chain = g_list_prepend(focus_chain, _content_area->widget());
  gtk_container_set_focus_chain(GTK_CONTAINER(table), focus_chain);
  g_list_free(focus_chain);
  
}

void MathGtkAttachedPopupWindow::invalidate_options() {
  switch(content()->get_own_style(WindowFrame)) {
    default: 
    case WindowFrameNone: 
      _appearance = ContainerType::None;
      gtk_widget_set_app_paintable(_widget, false);
      gtk_container_set_border_width(GTK_CONTAINER(_widget), 0);
      break;
      
    case WindowFrameThin:
      _appearance = ContainerType::PopupPanel;
      gtk_widget_set_app_paintable(_widget, true);
      gtk_container_set_border_width(GTK_CONTAINER(_widget), 1);
      break;
    
  }
//  if(_appearance != old_appearance) {
//    gtk_widget_queue_draw(_widget);
//  }
  
  invalidate_source_location();
}

void MathGtkAttachedPopupWindow::invalidate_source_location() {
  MathGtkWidget *owner_wid = _content_area->owner_widget();
  if(!owner_wid) {
    pmath_debug_print("[MathGtkAttachedPopupWindow: lost owner window]\n");
    document()->style->set(ClosingAction, ClosingActionDelete);
    close();
    return;
  }
  
  bool visible = gtk_widget_get_mapped(owner_wid->widget()) && content()->get_style(Visible, true);
  
  if(Box *anchor = _content_area->source_box()) {
    int x, y;
    if(visible && Impl(*this).find_anchor_screen_position(x, y)) {
      int content_width  = _content_area->best_width();
      int content_height = _content_area->best_height();
      
      int max_width  = content_width + 100;
      int max_height = content_height + 100;
      int border_extra = 0;
      
      for(GtkWidget *tmp = _widget; tmp; tmp = gtk_widget_get_parent(tmp)) {
        if(GTK_IS_CONTAINER(tmp)) {
          border_extra += 2 * gtk_container_get_border_width(GTK_CONTAINER(tmp));
        }
      }
      
      GtkWidget *owner_win = gtk_widget_get_ancestor(owner_wid->widget(), GTK_TYPE_WINDOW);
      {
        GdkScreen *screen = gtk_window_get_screen(GTK_WINDOW(owner_win));
        int monitor = gdk_screen_get_monitor_at_point(screen, x, y);
        
        GdkRectangle monitor_rect;
#if GTK_CHECK_VERSION(3, 4, 0)
        // TODO: use gdk_monitor_get_workarea() on GTK >= 3.22 ?
        gdk_screen_get_monitor_workarea(screen, monitor, &monitor_rect);
#else
        gdk_screen_get_monitor_geometry(screen, monitor, &monitor_rect);
#endif
        
        max_width  = monitor_rect.x + monitor_rect.width - x;
        max_height = monitor_rect.y + monitor_rect.height - y;
        
        content_width  = std::max(1, std::min(content_width,  max_width - border_extra));
        content_height = std::max(1, std::min(content_height, max_height - border_extra));
      }
      
      bool was_visible = gtk_widget_get_mapped(_widget);
      if(!was_visible) {
        gtk_window_set_transient_for(GTK_WINDOW(_widget), GTK_WINDOW(owner_win));
        //gtk_window_set_attached_to(GTK_WINDOW(_widget), owner_wid->widget());
      }
      
      int width = content_width + border_extra;
      int height = content_height + border_extra;
      
      if(content_height < _content_area->best_height()) {
        gtk_widget_show(_vscrollbar);
        
        width+= gtk_widget_get_allocated_width(_vscrollbar);
        if(width > max_width) {
          content_width+= max_width - width;
          width = max_width;
        }
      }
      else
        gtk_widget_hide(_vscrollbar);
      
      if(content_width < _content_area->best_width()) {
        gtk_widget_show(_hscrollbar);
        
        height+= gtk_widget_get_allocated_height(_hscrollbar);
        if(height > max_height) {
          content_height+= max_height - height;
          height = max_height;
          
          if(!gtk_widget_get_visible(_vscrollbar)) {
            gtk_widget_show(_vscrollbar);
            
            content_width-= gtk_widget_get_allocated_width(_vscrollbar);
          }
        }
      }
      else
        gtk_widget_hide(_hscrollbar);
      
      GtkAllocation old_rect;
      gtk_widget_get_allocation(_widget, &old_rect);
      
//      gtk_window_get_size(GTK_WINDOW(_widget), &old_rect.width, &old_rect.height);
//      gtk_window_get_position(GTK_WINDOW(_widget), &old_rect.x, &old_rect.y);
      
      if(old_rect.width != width || old_rect.height != height) {
        gtk_widget_set_size_request(_widget, width, height);
        gtk_window_resize(GTK_WINDOW(_widget), 1, 1);
      }
      
      //gtk_window_set_gravity(GTK_WINDOW(_widget), GDK_GRAVITY_NORTH_WEST);
      if(old_rect.x != x || old_rect.y != y)
        gtk_window_move(GTK_WINDOW(_widget), x, y);
      
      gtk_widget_show(_widget);
      
      if(old_rect.x != x || old_rect.y != y || old_rect.width != width || old_rect.height != height || !was_visible)
        content()->invalidate_popup_window_positions();
    }
    else {
      gtk_widget_hide(_widget);
      gtk_widget_set_size_request(_widget, 1, 1);
      gtk_window_set_default_size(GTK_WINDOW(_widget), 1, 1);
      gtk_window_set_transient_for(GTK_WINDOW(_widget), nullptr);
      //gtk_window_set_attached_to(GTK_WINDOW(_widget), nullptr);
    }
  }
  else {
    pmath_debug_print("[MathGtkAttachedPopupWindow: lost anchor]\n");
    document()->style->set(ClosingAction, ClosingActionDelete);
    close();
  }
}

void MathGtkAttachedPopupWindow::close() {
  if(!_widget || destroying())
    return;
  
  GdkEvent *ev = gdk_event_new(GDK_DELETE);
  if(!gtk_widget_event(_widget, ev)) {
    destroy();
  }
  gdk_event_free(ev);
}

bool MathGtkAttachedPopupWindow::is_using_dark_mode() {
  static int recursion = 0;
  
  for(Box *src = _content_area->source_box(); src; src = src->parent()) {
    ControlContext *ctx = dynamic_cast<ControlContext*>(src);
    if(!ctx) {
      if(Document *doc = dynamic_cast<Document*>(src))
        ctx = doc->native();
    }
    
    if(ctx) {
      bool result = false;
      if(recursion < 2) {
        ++recursion;
        result = ctx->is_using_dark_mode();
        --recursion;
      }
      return result;
    }
  }
  
  if(Document *owner = _content_area->owner_document()) {
    bool result = false;
    if(recursion < 2) {
      ++recursion;
      result = owner->native()->is_using_dark_mode();
      --recursion;
    }
    return result;
  }
  
  return false;
}

int MathGtkAttachedPopupWindow::dpi() {
  GdkScreen *screen = gtk_window_get_screen(GTK_WINDOW(_widget));
  double dpi = gdk_screen_get_resolution(screen);
  if(dpi <= 0)
    return 96;
  return (int)dpi;
}

bool MathGtkAttachedPopupWindow::on_configure(GdkEvent *e) {
  content()->invalidate_popup_window_positions();
  return false;
}

bool MathGtkAttachedPopupWindow::on_delete(GdkEvent *e) {
  if(Document *owner = _content_area->owner_document()) 
    owner->popup_window_closed(content());
  
  switch(content()->get_style(ClosingAction)) {
    case ClosingActionHide: {
        content()->style->set(Visible, false);
        invalidate_options();
      }
      return true;
    
    case ClosingActionDelete:
    default:
      break;
  }
  
  return false;
}

bool MathGtkAttachedPopupWindow::on_draw(cairo_t *cr) {
  GtkAllocation alloc_rect;
  gtk_widget_get_allocation(_widget, &alloc_rect);
  
  RectangleF rect(Point(alloc_rect.x, alloc_rect.y), Vector2F(alloc_rect.width, alloc_rect.height));
  
  {
    Canvas canvas(cr);
    
    rect.add_rect_path(canvas);
    canvas.clip();
    
    ControlPainter::std->draw_container(*this, canvas, _appearance, ControlState::Normal, rect);
  }
  
  return false;
}

bool MathGtkAttachedPopupWindow::on_unmap(GdkEvent *e) {
  content()->invalidate_popup_window_positions();
  return false;
}

bool MathGtkAttachedPopupWindow::on_window_state(GdkEvent *e) {
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

bool MathGtkAttachedPopupWindow::on_expose(GdkEvent *e) {
  GdkEventExpose *event = &e->expose;
  
  cairo_t *cr = gdk_cairo_create(event->window);
  
  cairo_move_to(cr, event->area.x, event->area.y);
  cairo_line_to(cr, event->area.x + event->area.width, event->area.y);
  cairo_line_to(cr, event->area.x + event->area.width, event->area.y + event->area.height);
  cairo_line_to(cr, event->area.x,                     event->area.y + event->area.height);
  cairo_close_path(cr);
  cairo_clip(cr);
  
  bool result = on_draw(cr);
  cairo_destroy(cr);
  
  return result;
}

//} ... class MathGtkAttachedPopupWindow

//{ class MathGtkAttachedPopupWindow::Impl ...

bool MathGtkAttachedPopupWindow::Impl::find_anchor_screen_position(int &x, int &y) {
  x = y = 0;
  
  MathGtkWidget *owner_wid = self._content_area->owner_widget();
  if(!owner_wid)
    return false;
  
  GtkWidget *owner_win = gtk_widget_get_ancestor(owner_wid->widget(), GTK_TYPE_WINDOW);
  if(!gtk_widget_get_visible(owner_win)) 
    return false;
  
  Box *anchor = self._content_area->source_box();
  if(!anchor)
    return false;
  
  Point anchor_point = {0, anchor->extents().descent};
  RectangleF rect = anchor->extents().to_rectangle();
  if(!anchor->visible_rect(rect))
    return false;
  
  cairo_matrix_t mat;
  cairo_matrix_init_identity(&mat);
  anchor->transformation(nullptr, &mat);
  
  anchor_point = Canvas::transform_point(mat, anchor_point);
  anchor_point.x *= owner_wid->scale_factor();
  anchor_point.y *= owner_wid->scale_factor();
  
  x = (int)round(anchor_point.x);
  y = (int)round(anchor_point.y);
  
  if(auto adj = owner_wid->hadjustment())
    x -= gtk_adjustment_get_value(adj);
    
  if(auto adj = owner_wid->vadjustment())
    y -= gtk_adjustment_get_value(adj);
  
//  if(!gtk_widget_translate_coordinates(owner_wid->widget(), owner_win, x, y, &x, &y))
//    return false;
  
  int root_x = 0;
  int root_y = 0;
  gdk_window_get_origin(gtk_widget_get_window(owner_wid->widget()), &root_x, &root_y);
  
  x+= root_x;
  y+= root_y;
  
  return true;
}

//} ... class MathGtkAttachedPopupWindow::Impl

//{ class MathGtkPopupContentArea ...

MathGtkPopupContentArea::MathGtkPopupContentArea(MathGtkAttachedPopupWindow *parent, Document *owner, Box *anchor)
  : base(new Document()),
    _parent(parent),
    _best_width{1},
    _best_height{1}
{
  owner_document(owner);
  source_box(anchor);
  _autohide_vertical_scrollbar = true;
}

MathGtkPopupContentArea::~MathGtkPopupContentArea() {
  if(Document *owner = owner_document()) 
    owner->popup_window_closed(document());
}

void MathGtkPopupContentArea::after_construction() {
  base::after_construction();
  
  Style::reset(document()->style, strings::AttachedPopupWindow);
  
  if(Document *owner = owner_document()) {
    document()->stylesheet(owner->stylesheet());
  }
  
  document()->style->set(Visible,                         true);
  document()->style->set(InternalHasModifiedWindowOption, true);
  document()->select(nullptr, 0, 0);
}

MathGtkWidget *MathGtkPopupContentArea::owner_widget() {
  if(Document *owner = owner_document())
    return dynamic_cast<MathGtkWidget*>(owner->native());
  
  return nullptr;
}

void MathGtkPopupContentArea::invalidate_options() {
  base::invalidate_options();
  _parent->invalidate_options();
}

void MathGtkPopupContentArea::invalidate_source_location() {
  base::invalidate_source_location();
  _parent->invalidate_source_location();
}

void MathGtkPopupContentArea::paint_background(Canvas &canvas) {
  // TODO: use proper style
  //canvas.set_color(Color::from_rgb24(0xDCDCDC));
  //canvas.paint();
}

void MathGtkPopupContentArea::paint_canvas(Canvas &canvas, bool resize_only) {
  base::paint_canvas(canvas, resize_only);
  
  int old_bh = _best_height;
  int old_bw = _best_width;
  
  _best_height = (int)round(document()->extents().height() * scale_factor());
  _best_width  = (int)round(document()->unfilled_width     * scale_factor());
  
  if(_best_height < 1)
    _best_height = 1;
    
  if(_best_width < 1)
    _best_width = 1;
    
  if(old_bw != _best_width || old_bh != _best_height) {
    _parent->invalidate_source_location();
  }
}

void MathGtkPopupContentArea::do_set_current_document() {
  Documents::current(document());
}

//} ... class MathGtkPopupContentArea

#if GTK_MAJOR_VERSION < 3
static int gtk_widget_get_allocated_width(GtkWidget *widget) {
  GtkAllocation alloc;
  gtk_widget_get_allocation(widget, &alloc);
  return alloc.width;
}

static int gtk_widget_get_allocated_height(GtkWidget *widget) {
  GtkAllocation alloc;
  gtk_widget_get_allocation(widget, &alloc);
  return alloc.height;
}
#endif
