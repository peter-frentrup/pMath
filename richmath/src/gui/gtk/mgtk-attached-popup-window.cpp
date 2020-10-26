#include <gui/gtk/mgtk-attached-popup-window.h>

#include <cmath>


namespace richmath {
  class MathGtkAttachedPopupWindow::Impl {
    public:
      Impl(MathGtkAttachedPopupWindow &self) : self{self} {}
      
      MathGtkWidget *owner_widget();
      bool find_anchor_screen_position(int &x, int &y);
      
    private:
      MathGtkAttachedPopupWindow &self;
  };
}

using namespace richmath;

//{ class MathGtkAttachedPopupWindow ...

MathGtkAttachedPopupWindow::MathGtkAttachedPopupWindow(Document *owner, Box *anchor) 
  : base(new Document()),
  _active{false},
  _best_width{1},
  _best_height{1}
{
  owner_document(owner);
  source_box(anchor);
  
  _autohide_vertical_scrollbar = true;
}

MathGtkAttachedPopupWindow::~MathGtkAttachedPopupWindow() {
  if(Document *owner = owner_document()) 
    owner->popup_window_closed(document());
}

void MathGtkAttachedPopupWindow::after_construction() {
  if(!_widget)
    _widget = gtk_window_new(GTK_WINDOW_POPUP);
  
  //gtk_window_set_type_hint(GTK_WINDOW(_widget), GDK_WINDOW_TYPE_HINT_COMBO);
  gtk_window_set_type_hint(GTK_WINDOW(_widget), GDK_WINDOW_TYPE_HINT_POPUP_MENU);
  gtk_window_set_default_size(GTK_WINDOW(_widget), 1, 1);
  gtk_window_set_resizable(GTK_WINDOW(_widget), FALSE);
  
  if(MathGtkWidget *owner_wid = Impl(*this).owner_widget()) {
    GtkWindow *owner_win = GTK_WINDOW(gtk_widget_get_ancestor(owner_wid->widget(), GTK_TYPE_WINDOW));
    //gtk_window_set_transient_for(GTK_WINDOW(_widget), owner_win);
  }
  
  base::after_construction();
  
  signal_connect<MathGtkAttachedPopupWindow, GdkEvent *, &MathGtkAttachedPopupWindow::on_delete>("delete-event");
  signal_connect<MathGtkAttachedPopupWindow, GdkEvent *, &MathGtkAttachedPopupWindow::on_window_state>("window-state-event");

  Style::reset(document()->style, "AttachedPopupWindow");
  
  if(Document *owner = owner_document()) {
    document()->stylesheet(owner->stylesheet());
  }
  
  document()->style->set(Visible,                         true);
  document()->style->set(InternalHasModifiedWindowOption, true);
  document()->select(nullptr, 0, 0);

}

void MathGtkAttachedPopupWindow::anchor_location_changed() {
  MathGtkWidget *owner_wid = Impl(*this).owner_widget();
  if(!owner_wid) {
    close();
    return;
  }
  
  bool visible = gtk_widget_get_mapped(owner_wid->widget()) && document()->get_style(Visible, true);
  
  if(Box *anchor = source_box()) {
    int x, y;
    if(visible && Impl(*this).find_anchor_screen_position(x, y)) {
      int width  = _best_width;
      int height = _best_height;
      
      int old_width, old_height;
      gtk_window_get_size(GTK_WINDOW(_widget), &old_width, &old_height);
      
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
      
        width  = std::max(1, std::min(_best_width,  (int)(monitor_rect.x + monitor_rect.width  - x)));
        height = std::max(1, std::min(_best_height, (int)(monitor_rect.y + monitor_rect.height - y)));
      }
      
      if(!gtk_widget_get_mapped(_widget))
        gtk_window_set_transient_for(GTK_WINDOW(_widget), GTK_WINDOW(owner_win));
      
      if(width != old_width || height != old_height)
        gtk_widget_set_size_request(_widget, width, height);
    
      //gtk_window_set_gravity(GTK_WINDOW(_widget), GDK_GRAVITY_NORTH_WEST);
      gtk_window_move(GTK_WINDOW(_widget), x, y);
      
      gtk_widget_show(_widget);
    }
    else {
      gtk_widget_hide(_widget);
      gtk_window_set_transient_for(GTK_WINDOW(_widget), nullptr);
    }
  }
  else {
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
    
void MathGtkAttachedPopupWindow::invalidate_options() {
  base::invalidate_options();
  
  anchor_location_changed();
}

bool MathGtkAttachedPopupWindow::is_using_dark_mode() {
  static int recursion = 0;
  
  for(Box *src = source_box(); src; src = src->parent()) {
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
  
  if(Document *owner = owner_document()) {
    bool result = false;
    if(recursion < 2) {
      ++recursion;
      result = owner->native()->is_using_dark_mode();
      --recursion;
    }
    return result;
  }
  
  return base::is_using_dark_mode();
}

int MathGtkAttachedPopupWindow::dpi() {
  GdkScreen *screen = gtk_window_get_screen(GTK_WINDOW(_widget));
  double dpi = gdk_screen_get_resolution(screen);
  if(dpi <= 0)
    return 96;
  return (int)dpi;
}

void MathGtkAttachedPopupWindow::paint_canvas(Canvas &canvas, bool resize_only) {
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
    anchor_location_changed();
  }
}

bool MathGtkAttachedPopupWindow::on_delete(GdkEvent *e) {
  if(Document *owner = owner_document()) 
    owner->popup_window_closed(document());
  
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
//} ... class MathGtkAttachedPopupWindow

//{ class MathGtkAttachedPopupWindow::Impl ...

MathGtkWidget *MathGtkAttachedPopupWindow::Impl::owner_widget() {
  Document *owner = self.owner_document();
  if(Document *owner = self.owner_document())
    return dynamic_cast<MathGtkWidget*>(owner->native());
  
  return nullptr;
}

bool MathGtkAttachedPopupWindow::Impl::find_anchor_screen_position(int &x, int &y) {
  x = y = 0;
  
  MathGtkWidget *owner_wid = owner_widget();
  if(!owner_wid)
    return false;
  
  GtkWidget *owner_win = gtk_widget_get_ancestor(owner_wid->widget(), GTK_TYPE_WINDOW);
  if(!gtk_widget_get_visible(owner_win)) 
    return false;
  
  Box *anchor = self.source_box();
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
