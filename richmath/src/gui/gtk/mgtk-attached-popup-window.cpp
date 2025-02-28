#include <gui/gtk/mgtk-attached-popup-window.h>

#include <gui/common-tooltips.h>
#include <gui/documents.h>
#include <gui/gtk/mgtk-menu-builder.h>

#include <boxes/abstractsequence.h>

#include <cmath>


namespace richmath {
  namespace strings {
    extern String AttachedPopupWindow;
  }
  
  class MathGtkAttachedPopupWindow::Impl {
    public:
      Impl(MathGtkAttachedPopupWindow &self) : self{self} {}
      
      bool find_anchor_screen_position(RectangleF &target_rect);
      void adjust_target_rect(WindowFrameType wft, ControlPlacementKind cpk, const Vector2F &window_size, RectangleF &target_rect);
      
      int triangle_tip_size(int window_width, int window_height);
      int triangle_tip_size(const RectangleF    &rect) { return triangle_tip_size((int)rect.width, (int)rect.height); }
      int triangle_tip_size(const GtkAllocation &rect) { return triangle_tip_size(rect.width, rect.height); }
      void update_window_shape(WindowFrameType wft, ControlPlacementKind cpk, const RectangleF &window_rect, const RectangleF &target_rect);
      
    private:
      MathGtkAttachedPopupWindow &self;
  };
  
  class MathGtkPopupContentArea final : public MathGtkWidget {
      using base = MathGtkWidget;
    public:
      MathGtkPopupContentArea(MathGtkAttachedPopupWindow *parent, Document *owner, const SelectionReference &anchor);
      
      int best_width() { return _best_width; }
      int best_height() { return _best_height; }
      
      MathGtkWidget *owner_widget();
      
      virtual void bring_to_front() override {
        _parent->bring_to_front();
        base::bring_to_front();
      }
      virtual void close() override { _parent->close(); }
      virtual void invalidate_options() override;
      virtual void invalidate_source_location() override;
      
      virtual bool is_foreground_window() override { return _parent->is_foreground_window(); };
      virtual bool is_focused_widget() override { return _parent->is_foreground_window() && base::is_focused_widget(); }
      virtual bool is_using_dark_mode() override { return _parent->is_using_dark_mode(); }
      virtual int dpi() override { return _parent->dpi(); }
      
    protected:
      ~MathGtkPopupContentArea();
      virtual void after_construction() override;
    
      virtual void paint_background(Canvas &canvas) override;
      virtual void paint_canvas(Canvas &canvas, bool resize_only) override;
      virtual void do_set_selected_document() override;
      
    private:
      MathGtkAttachedPopupWindow *_parent;
      int                         _best_width;
      int                         _best_height;
  };
}

using namespace richmath;

GdkPoint richmath::discretize(const Point &p) {
  return { (int)(p.x + 0.5f), (int)(p.y + 0.5f) }; 
}

GdkRectangle richmath::discretize(const RectangleF &rect) {
  int x1 = (int)round_directed(rect.left(),  +1, false);
  int y1 = (int)round_directed(rect.top(),   +1, false);
  int x2 = (int)round_directed(rect.right(), -1, false);
  int y2 = (int)round_directed(rect.bottom(),-1, false);
  return { x1, y1, x2 - x1, y2 - y1 }; 
}

#if GTK_MAJOR_VERSION < 3
static int gtk_widget_get_allocated_width(GtkWidget *widget);
static int gtk_widget_get_allocated_height(GtkWidget *widget);
#endif

//{ class MathGtkAttachedPopupWindow ...

MathGtkAttachedPopupWindow::MathGtkAttachedPopupWindow(Document *owner, const SelectionReference &anchor) 
  : base(),
  _hadjustment{GTK_ADJUSTMENT(gtk_adjustment_new(0, 0, 0, 0, 0, 0))},
  _vadjustment{GTK_ADJUSTMENT(gtk_adjustment_new(0, 0, 0, 0, 0, 0))},
  _hscrollbar{nullptr},
  _vscrollbar{nullptr},
  _content_alignment{nullptr},
  _content_area{new MathGtkPopupContentArea(this, owner, anchor)},
  _appearance{ContainerType::None},
  _active{false},
  _callout_triangle{0.0f, 0.0f, 0.0f, 0.0f}
{
}

MathGtkAttachedPopupWindow::~MathGtkAttachedPopupWindow() {
  pmath_debug_print("[MathGtkAttachedPopupWindow::~MathGtkAttachedPopupWindow]\n");
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
  
  _content_alignment = gtk_alignment_new(0, 0, 1, 1);
  gtk_container_add(GTK_CONTAINER(_widget), _content_alignment);
  
  GtkWidget *table = gtk_table_new(2, 2, FALSE);
  gtk_container_add(GTK_CONTAINER(_content_alignment), table);
  
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
  signal_connect<MathGtkAttachedPopupWindow, GdkEvent *, &MathGtkAttachedPopupWindow::on_map>("map-event");
  signal_connect<MathGtkAttachedPopupWindow, GdkEvent *, &MathGtkAttachedPopupWindow::on_unmap>("unmap-event");
  signal_connect<MathGtkAttachedPopupWindow, GdkEvent *, &MathGtkAttachedPopupWindow::on_window_state>("window-state-event");

  gtk_widget_show_all(_content_alignment);
  
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
      gtk_alignment_set_padding(GTK_ALIGNMENT(_content_alignment), 0, 0, 0, 0);
      gtk_container_set_border_width(GTK_CONTAINER(_widget), 0);
      break;
      
    case WindowFrameThin:
      _appearance = ContainerType::PopupPanel;
      gtk_widget_set_app_paintable(_widget, true);
      gtk_alignment_set_padding(GTK_ALIGNMENT(_content_alignment), 0, 0, 0, 0);
      gtk_container_set_border_width(GTK_CONTAINER(_widget), 1);
      break;
    
    case WindowFrameThinCallout: {
      _appearance = ContainerType::None;
      gtk_widget_set_app_paintable(_widget, true);
      
      GtkAllocation alloc;
      gtk_widget_get_allocation(_widget, &alloc);
      unsigned pad[4] = {0, 0, 0, 0}; //{2, 2, 2, 2};
      
      auto cpk = (ControlPlacementKind)content()->get_own_style(ControlPlacement, ControlPlacementKindBottom);
      Side side = opposite_side(control_placement_side(cpk));
      pad[(int)side] += Impl(*this).triangle_tip_size(alloc);
      
      gtk_alignment_set_padding(GTK_ALIGNMENT(_content_alignment), pad[(int)Side::Top], pad[(int)Side::Bottom], pad[(int)Side::Left], pad[(int)Side::Right]);
      gtk_container_set_border_width(GTK_CONTAINER(_widget), 2);
    } break;
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
    content()->style.set(ClosingAction, ClosingActionDelete);
    close();
    return;
  }
  
  bool visible = gtk_widget_get_mapped(owner_wid->widget()) && content()->get_style(Visible, true);
  
  if(Box *anchor = _content_area->source_box()) {
    auto cpk = (ControlPlacementKind)content()->get_own_style(ControlPlacement, ControlPlacementKindBottom);
    RectangleF target_rect;
    if(visible && Impl(*this).find_anchor_screen_position(target_rect)) {
      auto wft = (WindowFrameType)content()->get_own_style(WindowFrame);
      
      Vector2F size(_content_area->best_width(), _content_area->best_height());
      
      int border_extra_x = 0;
      int border_extra_y = 0;
      
      for(GtkWidget *tmp = _content_area->widget(); tmp; tmp = gtk_widget_get_parent(tmp)) {
        if(GTK_IS_CONTAINER(tmp)) {
          int delta = 2 * gtk_container_get_border_width(GTK_CONTAINER(tmp));
          border_extra_x += 2 * delta;
          border_extra_y += 2 * delta;
          
          if(GTK_IS_ALIGNMENT(tmp)) {
            unsigned padding_left   = 0;
            unsigned padding_right  = 0;
            unsigned padding_top    = 0;
            unsigned padding_bottom = 0;
            gtk_alignment_get_padding(GTK_ALIGNMENT(_content_alignment), &padding_top, &padding_bottom, &padding_left, &padding_right);
            
            border_extra_x += padding_left + padding_right;
            border_extra_y += padding_top  + padding_bottom;
          }
        }
      }
      
      size.x+= border_extra_x;
      size.y+= border_extra_y;
      
      Impl(*this).adjust_target_rect(wft, cpk, size, target_rect);
      
      RectangleF popup_rect;
      
      GtkWidget *owner_win = gtk_widget_get_ancestor(owner_wid->widget(), GTK_TYPE_WINDOW);
      {
        GdkScreen *screen = gtk_window_get_screen(GTK_WINDOW(owner_win));
        int monitor = gdk_screen_get_monitor_at_point(screen, target_rect.x + target_rect.width/2, target_rect.y + target_rect.height/2);
        
        GdkRectangle monitor_rect_int;
#if GTK_CHECK_VERSION(3, 4, 0)
        // TODO: use gdk_monitor_get_workarea() on GTK >= 3.22 ?
        gdk_screen_get_monitor_workarea(screen, monitor, &monitor_rect_int);
#else
        gdk_screen_get_monitor_geometry(screen, monitor, &monitor_rect_int);
#endif
        
        RectangleF monitor_rect(monitor_rect_int.x, monitor_rect_int.y, monitor_rect_int.width, monitor_rect_int.height);
        
        popup_rect = CommonTooltips::popup_placement(target_rect, size, cpk, monitor_rect);
        
        if(popup_rect.height < size.y) {
          gtk_widget_show(_vscrollbar);
          size.x+= gtk_widget_get_allocated_width(_vscrollbar);
          
          popup_rect = CommonTooltips::popup_placement(target_rect, size, cpk, monitor_rect);
        }
        else
          gtk_widget_hide(_vscrollbar);
        
        if(popup_rect.width < size.x) {
          gtk_widget_show(_hscrollbar);
          
          size.y+= gtk_widget_get_allocated_height(_hscrollbar);
          
          popup_rect = CommonTooltips::popup_placement(target_rect, size, cpk, monitor_rect);
          if(popup_rect.height < size.y) {
            gtk_widget_show(_vscrollbar);
          }
        }
        else
          gtk_widget_hide(_hscrollbar);
      }
      
      Impl(*this).update_window_shape(wft, cpk, popup_rect, target_rect);
      
      bool was_visible = gtk_widget_get_mapped(_widget);
      if(!was_visible) {
        gtk_window_set_transient_for(GTK_WINDOW(_widget), GTK_WINDOW(owner_win));
        //gtk_window_set_attached_to(GTK_WINDOW(_widget), owner_wid->widget());
      }
      
      GtkAllocation old_rect;
      gtk_widget_get_allocation(_widget, &old_rect);
      
//      gtk_window_get_size(GTK_WINDOW(_widget), &old_rect.width, &old_rect.height);
//      gtk_window_get_position(GTK_WINDOW(_widget), &old_rect.x, &old_rect.y);
      
      int width  = std::max(1, (int)round(popup_rect.width));
      int height = std::max(1, (int)round(popup_rect.height));
      
      if(old_rect.width != width || old_rect.height != height) {
        gtk_widget_set_size_request(_widget, width, height);
        gtk_window_resize(GTK_WINDOW(_widget), 1, 1);
      }
      
      GdkPoint pos = {(int)round(popup_rect.x), (int)round(popup_rect.y)};
      
      //gtk_window_set_gravity(GTK_WINDOW(_widget), GDK_GRAVITY_NORTH_WEST);
      if(old_rect.x != pos.x || old_rect.y != pos.y)
        gtk_window_move(GTK_WINDOW(_widget), pos.x, pos.y);
      
      gtk_widget_show(_widget);
      
      if(old_rect.x != pos.x || old_rect.y != pos.y || old_rect.width != width || old_rect.height != height || !was_visible)
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
    content()->style.set(ClosingAction, ClosingActionDelete);
    close();
  }
}

void MathGtkAttachedPopupWindow::bring_to_front() {
  gtk_window_present(GTK_WINDOW(_widget));
}

void MathGtkAttachedPopupWindow::close() {
  if(!_widget || destroying())
    return;
  
  pmath_debug_print("[MathGtkAttachedPopupWindow::close %p]\n", this);
  GdkEvent *ev = gdk_event_new(GDK_DELETE);
  if(!gtk_widget_event(_widget, ev)) {
    pmath_debug_print("[MathGtkAttachedPopupWindow::close %p ignored GDK_DELETE, calling destroy]\n", this);
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
  pmath_debug_print("[MathGtkAttachedPopupWindow::on_delete %p]\n", this);
  switch(content()->get_style(ClosingAction)) {
    case ClosingActionHide: {
        content()->style.set(Visible, false);
        invalidate_options();
      }
      return true;
    
    case ClosingActionDelete:
    default:
      break;
  }
  
  if(Document *owner = _content_area->owner_document()) 
    owner->popup_window_closed(content());
  
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
    
    auto wft = (WindowFrameType)content()->get_own_style(WindowFrame);
    switch(wft) {
      case WindowFrameThinCallout: {
        auto cpk = (ControlPlacementKind)content()->get_own_style(ControlPlacement, ControlPlacementKindBottom);
        Side side = opposite_side(control_placement_side(cpk));
        auto tip_size = Impl(*this).triangle_tip_size(alloc_rect);
        
        auto main_rect = rect;
        main_rect.grow(side, -tip_size);
        Point tri_points[3];
        _callout_triangle.get_triangle_points(tri_points, main_rect, side);
        
        Color bg = content()->get_style(Background, Color::None);
        if(!bg) {
          ControlPainter::std->draw_container(*this, canvas, ContainerType::PopupPanel, ControlState::Normal, rect);
        }
        
        canvas.move_to(main_rect.top_left());
        if(side == Side::Top) {
          canvas.line_to(tri_points[0]);
          canvas.line_to(tri_points[1]);
          canvas.line_to(tri_points[2]);
        }
        canvas.line_to(main_rect.top_right());
        if(side == Side::Right) {
          canvas.line_to(tri_points[0]);
          canvas.line_to(tri_points[1]);
          canvas.line_to(tri_points[2]);
        }
        canvas.line_to(main_rect.bottom_right());
        if(side == Side::Bottom) {
          canvas.line_to(tri_points[2]);
          canvas.line_to(tri_points[1]);
          canvas.line_to(tri_points[0]);
        }
        canvas.line_to(main_rect.bottom_left());
        if(side == Side::Left) {
          canvas.line_to(tri_points[2]);
          canvas.line_to(tri_points[1]);
          canvas.line_to(tri_points[0]);
        }
        canvas.line_to(main_rect.top_left());
        canvas.close_path();
        
        if(bg) {
          canvas.set_color(bg);
          canvas.fill_preserve();
        }
        
        canvas.set_color(Color::from_rgb24(0x808080));
        canvas.line_width(2.0f);
        canvas.stroke();
      } break;
      
      default:
        ControlPainter::std->draw_container(*this, canvas, _appearance, ControlState::Normal, rect);
        break;
    }
  }
  
  return false;
}

bool MathGtkAttachedPopupWindow::on_map(GdkEvent *e) {
  content()->invalidate_popup_window_positions();
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

bool MathGtkAttachedPopupWindow::Impl::find_anchor_screen_position(RectangleF &target_rect) {
  MathGtkWidget *owner_wid = self._content_area->owner_widget();
  if(!owner_wid)
    return false;
  
  GtkWidget *owner_win = gtk_widget_get_ancestor(owner_wid->widget(), GTK_TYPE_WINDOW);
  if(!gtk_widget_get_visible(owner_win)) 
    return false;
  
  VolatileSelection anchor = self._content_area->source_range().get_all();
  if(!anchor)
    return false;
  
  bool has_target_rect = false;
  if(true /* content padding */) {
    if(auto seq = dynamic_cast<AbstractSequence*>(anchor.box)) {
//      Array<RectangleF> rects;
//      anchor.add_rectangles(rects, SelectionDisplayFlags::BigCenterBlob | SelectionDisplayFlags::TightWidths, {0.0f, 0.0f});
//      for(const RectangleF &rect: rects) {
//        if(has_target_rect) {
//          target_rect = target_rect.union_hull(rect);
//        }
//        else {
//          target_rect = rect;
//          has_target_rect = true;
//        }
//      }
      LineRangeMeasurement measurement = seq->measure_range(anchor.start, anchor.start);
      
      target_rect = measurement.bounds;
      float em = seq->get_em();
      float min_accent  = 0.75 * em;
      float min_descent = 0.25 * em;
      if(measurement.first_line_ascent < min_accent)
        target_rect.grow(Side::Top, min_accent - measurement.first_line_ascent);
      
      if(measurement.last_line_descent < min_descent)
        target_rect.grow(Side::Bottom, min_descent - measurement.last_line_descent);
      
      has_target_rect = true;
    }
  }
  
  if(!has_target_rect) {
    target_rect = anchor.box->range_rect(anchor.start, anchor.end);
  }

  if(!anchor.box->visible_rect(target_rect))
    return false;
  
  auto scale_factor = owner_wid->scale_factor();
  target_rect.x      *= scale_factor;
  target_rect.y      *= scale_factor;
  target_rect.width  *= scale_factor;
  target_rect.height *= scale_factor;
  
  if(auto adj = owner_wid->hadjustment())
    target_rect.x -= gtk_adjustment_get_value(adj);
    
  if(auto adj = owner_wid->vadjustment())
    target_rect.y -= gtk_adjustment_get_value(adj);
  
//  if(!gtk_widget_translate_coordinates(owner_wid->widget(), owner_win, x, y, &x, &y))
//    return false;
  
  int root_x = 0;
  int root_y = 0;
  gdk_window_get_origin(gtk_widget_get_window(owner_wid->widget()), &root_x, &root_y);
  
  target_rect.x += root_x;
  target_rect.y += root_y;
  
  return true;
}

void MathGtkAttachedPopupWindow::Impl::adjust_target_rect(WindowFrameType wft, ControlPlacementKind cpk, const Vector2F &window_size, RectangleF &target_rect) {
  switch(wft) {
    case WindowFrameThinCallout: 
      target_rect.grow(control_placement_side(cpk), -triangle_tip_size((int)window_size.x, (int)window_size.y) / 4);
      break;
  }
}

int MathGtkAttachedPopupWindow::Impl::triangle_tip_size(int window_width, int window_height) {
  int size = 20;
  
  int max_size = std::min(window_width, window_height) / 2;
  if(size > max_size)
    size = max_size;
  
  return size;
}

void MathGtkAttachedPopupWindow::Impl::update_window_shape(WindowFrameType wft, ControlPlacementKind cpk, const RectangleF &window_rect, const RectangleF &target_rect) {
  if(!gtk_widget_get_realized(self.widget()))
    return;
  
  GdkWindow *gdk_win = gtk_widget_get_window(self.widget());
  if(!gdk_win)
    return;
  
  switch(wft) {
    case WindowFrameThinCallout: {
      auto side = opposite_side(control_placement_side(cpk));
      auto tip_size = triangle_tip_size(window_rect);
      self._callout_triangle = CalloutTriangle::ForSideOfBasePointingToTarget(window_rect, side, target_rect, tip_size, true);
      
      auto main_rect = window_rect - Vector2F{window_rect.top_left()};
      main_rect.grow(side, -tip_size);
      
      Point tri_points[3];
      self._callout_triangle.get_triangle_points(tri_points, main_rect, side);
      
      unsigned pad[4] = {0,0,0,0};
      pad[(int)side] += tip_size;
      gtk_alignment_set_padding(GTK_ALIGNMENT(self._content_alignment), pad[(int)Side::Top], pad[(int)Side::Bottom], pad[(int)Side::Left], pad[(int)Side::Right]);
      
#    if GTK_MAJOR_VERSION >= 3
      { // Sadly, gdk_region_polygon() was removed in GTK 3.
        cairo_surface_t *surface = gdk_window_create_similar_surface(gdk_win, CAIRO_CONTENT_COLOR_ALPHA, (int)window_rect.width, (int)window_rect.height);
        
        cairo_t *cr = cairo_create(surface);
        {
          Canvas canvas(cr);
          
          canvas.set_color(Color::White);
          canvas.move_to(tri_points[0]);
          canvas.line_to(tri_points[1]);
          canvas.line_to(tri_points[2]);
          canvas.fill();
          
          canvas.move_to(main_rect.top_left());
          canvas.line_to(main_rect.top_right());
          canvas.line_to(main_rect.bottom_right());
          canvas.line_to(main_rect.bottom_left());
          canvas.fill();
        }
        cairo_destroy(cr);
        
        cairo_region_t *reg = gdk_cairo_region_create_from_surface(surface);
        cairo_surface_destroy(surface);
        gtk_widget_shape_combine_region(self.widget(), reg);
        //gdk_window_shape_combine_region(gdk_win, reg, 0, 0);
        cairo_region_destroy(reg);
        //gdk_window_set_child_shapes(gtk_widget_get_parent_window(self.widget()));
      }
#    else
      {
        GdkPoint triangle_points[3] = { discretize(tri_points[0]), discretize(tri_points[1]), discretize(tri_points[2]) };
        GdkRegion *reg = gdk_region_polygon(triangle_points, 3, GDK_WINDING_RULE);
        
        GdkRectangle rect = discretize(main_rect);
        gdk_region_union_with_rect(reg, &rect);
        gdk_window_shape_combine_region(gdk_win, reg, 0, 0);
        gdk_region_destroy(reg);
      }
#    endif
    } break;
    
    default:
#    if GTK_MAJOR_VERSION >= 3
      gtk_widget_shape_combine_region(self.widget(), nullptr);
      //gdk_window_set_child_shapes(gtk_widget_get_parent_window(self.widget()));
#    else
      gdk_window_shape_combine_region(gdk_win, nullptr, 0, 0);
#    endif
      break;
  }
}

//} ... class MathGtkAttachedPopupWindow::Impl

//{ class MathGtkPopupContentArea ...

MathGtkPopupContentArea::MathGtkPopupContentArea(MathGtkAttachedPopupWindow *parent, Document *owner, const SelectionReference &anchor)
  : base(new Document()),
    _parent(parent),
    _best_width{1},
    _best_height{1}
{
  owner_document(owner);
  source_range(anchor);
  _autohide_vertical_scrollbar = true;
}

MathGtkPopupContentArea::~MathGtkPopupContentArea() {
  if(Document *owner = owner_document()) 
    owner->popup_window_closed(document());
}

void MathGtkPopupContentArea::after_construction() {
  base::after_construction();
  
  document()->style.reset(strings::AttachedPopupWindow);
  
  if(Document *owner = owner_document()) {
    document()->stylesheet(owner->stylesheet());
  }
  
  document()->style.set(Visible,                         true);
  document()->style.set(InternalHasModifiedWindowOption, true);
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

void MathGtkPopupContentArea::do_set_selected_document() {
  Documents::selected_document(document());
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
