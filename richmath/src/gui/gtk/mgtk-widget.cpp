#include <gui/gtk/mgtk-widget.h>

#include <eval/binding.h>
#include <gui/gtk/mgtk-clipboard.h>
#include <gui/gtk/mgtk-menu-builder.h>
#include <gui/gtk/mgtk-tooltip-window.h>

#include <glib.h>
#include <cmath>

#if GTK_MAJOR_VERSION >= 3
#  include <gdk/gdkkeysyms-compat.h>
#else
#  include <gdk/gdkkeysyms.h>
#endif

#ifdef GDK_WINDOWING_X11
#  include <gdk/gdkx.h>
#  include <X11/XKBlib.h>
#endif


#if !GTK_CHECK_VERSION(2,22,0)
static GdkDragAction gdk_drag_context_get_selected_action(GdkDragContext *context) {
  return context->action;
}

static GdkDragAction gdk_drag_context_get_suggested_action(GdkDragContext *context) {
  return context->action;
}

static GdkDragAction gdk_drag_context_get_actions(GdkDragContext *context) {
  return context->actions;
}
#endif


using namespace richmath;

#define ANIMATION_DELAY  (50)
static bool animation_running = false;
Hashtable<SharedPtr<TimedEvent>, Void> animations;

static gboolean animation_timeout(gpointer data) {
  animation_running = false;
  
  unsigned int count, i;
  for(count = 0, i = 0; count < animations.size(); ++i) {
    Entry<SharedPtr<TimedEvent>, Void> *e = animations.entry(i);
    
    if(e) {
      ++count;
      
      SharedPtr<TimedEvent> te = e->key;
      if(te->min_wait_seconds <= te->timer()) {
        animations.remove(te);
        
        te->execute_event();
      }
      else
        animation_running = true;
    }
  }
  
  return animation_running; // continue ?
}

static Array<const char *> drag_mime_types; // index is the info parameter
static GtkTargetList *drop_targets;

static void add_remove_widget(int delta) {
  static int widget_count = 0;
  
  if(widget_count == 0) {
    drag_mime_types.add(Clipboard::BoxesText);
    drag_mime_types.add(Clipboard::PlainText);
    
    drop_targets = gtk_target_list_new(NULL, 0);
    for(int i = 0; i < drag_mime_types.length(); ++i) {
      MathGtkClipboard::add_to_target_list(drop_targets, drag_mime_types[i], i);
    }
  }
  
  widget_count += delta;
  
  if(widget_count == 0) {
    drag_mime_types.length(0);
    gtk_target_list_unref(drop_targets);
    drop_targets = 0;
  }
}

//{ class MathGtkWidget ...

MathGtkWidget::MathGtkWidget(Document *doc)
  : NativeWidget(doc),
    BasicGtkWidget(),
    _autohide_vertical_scrollbar(false),
    _mouse_down_button(0),
    is_painting(false),
    is_blinking(false),
    ignore_key_release(true),
    old_width(0),
    _hadjustment(0),
    _vadjustment(0),
    _im_context(gtk_im_multicontext_new())
{
  add_remove_widget(+1);
  g_signal_connect(_im_context, "commit",          G_CALLBACK(&MathGtkWidget::im_commit_callback),          this);
  g_signal_connect(_im_context, "preedit_changed", G_CALLBACK(&MathGtkWidget::im_preedit_changed_callback), this);
  
  gtk_im_context_set_use_preedit(_im_context, FALSE);
}

MathGtkWidget::~MathGtkWidget() {
  all_document_ids.remove(document()->id());
  
  hadjustment(0);
  vadjustment(0);
  
  g_signal_handlers_disconnect_matched(
    _im_context,
    G_SIGNAL_MATCH_DATA,
    0, 0, 0, 0,
    this);
    
  g_object_unref(_im_context);
  _im_context = 0;
  
  add_remove_widget(-1);
}

void MathGtkWidget::after_construction() {
  BasicGtkWidget::after_construction();
  
  _popup_menu = NULL;
  
  gtk_widget_set_events(_widget, GDK_ALL_EVENTS_MASK);
  gtk_widget_set_can_focus(_widget, TRUE);
  
  signal_connect<MathGtkWidget, &MathGtkWidget::on_popup_menu>("popup-menu");
  
#if GTK_MAJOR_VERSION >= 3
  signal_connect<MathGtkWidget, cairo_t *, &MathGtkWidget::on_draw>("draw");
#else
  signal_connect<MathGtkWidget, GdkEvent *, &MathGtkWidget::on_expose>("expose-event");
#endif
  
  signal_connect<MathGtkWidget, GdkEvent *, &MathGtkWidget::on_button_press>(  "button-press-event");
  signal_connect<MathGtkWidget, GdkEvent *, &MathGtkWidget::on_button_release>("button-release-event");
  signal_connect<MathGtkWidget, GdkEvent *, &MathGtkWidget::on_focus_in>(      "focus-in-event");
  signal_connect<MathGtkWidget, GdkEvent *, &MathGtkWidget::on_focus_out>(     "focus-out-event");
  signal_connect<MathGtkWidget, GdkEvent *, &MathGtkWidget::on_key_press>(     "key-press-event");
  signal_connect<MathGtkWidget, GdkEvent *, &MathGtkWidget::on_key_release>(   "key-release-event");
  signal_connect<MathGtkWidget, GdkEvent *, &MathGtkWidget::on_motion_notify>( "motion-notify-event");
  signal_connect<MathGtkWidget, GdkEvent *, &MathGtkWidget::on_leave_notify>(  "leave-notify-event");
  signal_connect<MathGtkWidget, GdkEvent *, &MathGtkWidget::on_scroll>(        "scroll-event");
  signal_connect<MathGtkWidget, GdkEvent *, &MathGtkWidget::on_map>(           "map-event");
  signal_connect<MathGtkWidget, GdkEvent *, &MathGtkWidget::on_unmap>(         "unmap-event");
  
  g_signal_connect(_widget, "drag-data-delete",   G_CALLBACK(drag_data_delete_callback),   this);
  g_signal_connect(_widget, "drag-data-get",      G_CALLBACK(drag_data_get_callback),      this);
  g_signal_connect(_widget, "drag-data-received", G_CALLBACK(drag_data_received_callback), this);
  g_signal_connect(_widget, "drag-end",           G_CALLBACK(drag_end_callback),           this);
  g_signal_connect(_widget, "drag-motion",        G_CALLBACK(drag_motion_callback),        this);
  g_signal_connect(_widget, "drag-drop",          G_CALLBACK(drag_drop_callback),          this);
  
  int len;
  GtkTargetEntry *table = gtk_target_table_new_from_list(drop_targets, &len);
  gtk_drag_dest_set(
    _widget,
    (GtkDestDefaults)0,
    table,
    len,
    (GdkDragAction)(GDK_ACTION_COPY | GDK_ACTION_MOVE));
  gtk_target_table_free(table, len);
}

void MathGtkWidget::window_size(float *w, float *h) {
  if(!_widget) {
    *w = *h = 0;
    return;
  }
  
  GtkAllocation rect;
  
  gtk_widget_get_allocation(_widget, &rect);
  
  *w = rect.width  / scale_factor();
  *h = rect.height / scale_factor();
}

void MathGtkWidget::scroll_pos(float *x, float *y) {
  *x = *y = 0;
  if(!is_scrollable())
    return;
    
  if(_hadjustment) {
    *x = gtk_adjustment_get_value(_hadjustment);
    *x /= scale_factor();
  }
  
  if(_vadjustment) {
    *y = gtk_adjustment_get_value(_vadjustment);
    *y /= scale_factor();
  }
}

void MathGtkWidget::scroll_to(float x, float y) {
  if(!is_scrollable())
    return;
    
  if(_hadjustment) {
    double oldx = gtk_adjustment_get_value(_hadjustment);
    double newx = x * scale_factor();
    
    double lo = gtk_adjustment_get_lower(_hadjustment);
    double hi = gtk_adjustment_get_upper(_hadjustment);
    hi -=        gtk_adjustment_get_page_size(_hadjustment);
    
    newx = CLAMP(newx, lo, hi);
    if(oldx != newx)
      gtk_adjustment_set_value(_hadjustment, newx);
  }
  
  if(_vadjustment) {
    double oldy = gtk_adjustment_get_value(_vadjustment);
    double newy = y * scale_factor();
    
    double lo = gtk_adjustment_get_lower(_vadjustment);
    double hi = gtk_adjustment_get_upper(_vadjustment);
    hi -=        gtk_adjustment_get_page_size(_vadjustment);
    
    newy = CLAMP(newy, lo, hi);
    if(oldy != newy)
      gtk_adjustment_set_value(_vadjustment, newy);
  }
}

void MathGtkWidget::show_tooltip(Expr boxes) {
  MathGtkTooltipWindow::show_global_tooltip(boxes);
}

void MathGtkWidget::hide_tooltip() {
  MathGtkTooltipWindow::hide_global_tooltip();
}

double MathGtkWidget::message_time() {
  guint32 timestamp = gtk_get_current_event_time();
  
  if(timestamp == GDK_CURRENT_TIME) {
    GTimeVal tv;
    g_get_current_time(&tv);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
  }
  
  return timestamp / 1000000.0;
}

double MathGtkWidget::double_click_time() {
  if(!_widget)
    return 1.0;
    
  GtkSettings *settings = gtk_widget_get_settings(_widget);
  gint t;
  
  g_object_get(
    settings,
    "gtk-double-click-time", &t,
    NULL);
    
  return t / 1000.0;
}

void MathGtkWidget::double_click_dist(float *dx, float *dy) {
  if(!_widget) {
    *dx = *dy = 0;
    return;
  }
  
  GtkSettings *settings = gtk_widget_get_settings(_widget);
  gint d;
  
  g_object_get(
    settings,
    "gtk-double-click-distance", &d,
    NULL);
    
  *dx = *dy = d / scale_factor();
}

void MathGtkWidget::do_drag_drop(Box *src, int start, int end) {
  GdkEvent *event = gtk_get_current_event();
  
  if(!src || !_widget)
    return;
    
  drag_source_reference().set(src, start, end);
  
  GdkDragContext *context;
  unsigned actions = GDK_ACTION_COPY;
  if(src->get_style(Editable))
    actions |= GDK_ACTION_MOVE;
    
  context = gtk_drag_begin(
              _widget,
              drop_targets,
              (GdkDragAction)actions,
              _mouse_down_button,
              event);
              
  gtk_drag_set_icon_default(context);
}

bool MathGtkWidget::cursor_position(float *x, float *y) {
  if(!_widget)
    return false;
    
  gint ix, iy;
  
  gtk_widget_get_pointer(_widget, &ix, &iy);
  
  *x = ix / scale_factor();
  *y = iy / scale_factor();
  
  float sx, sy;
  scroll_pos(&sx, &sy);
  *x += sx;
  *y += sy;
  
  return true;
}

void MathGtkWidget::bring_to_front() {
  gtk_widget_grab_focus(_widget);
}

void MathGtkWidget::invalidate() {
  if(!_widget)
    return;
    
  is_painting = false; // if inside "expose" event, invalidate at end of event
  
  gtk_widget_queue_draw(_widget);
}

void MathGtkWidget::invalidate_options() {
}

void MathGtkWidget::invalidate_rect(float x, float y, float w, float h) {
  if(!_widget)
    return;
    
  is_painting = false; // if inside "expose" event, invalidate at end of event
  
  float sx, sy, sf;
  scroll_pos(&sx, &sy);
  sf = scale_factor();
  
  gtk_widget_queue_draw_area(_widget,
                             (int)floorf((x - sx) * sf) - 4,
                             (int)floorf((y - sy) * sf) - 4,
                             (int)ceilf(w * sf) + 8,
                             (int)ceilf(h * sf) + 8);
}

void MathGtkWidget::force_redraw() {
  invalidate();
}

void MathGtkWidget::set_cursor(CursorType type) {
  cursor = type;
  
  if(mouse_moving || !_widget)
    return;
    
  GdkWindow *win = gtk_widget_get_window(_widget);
  
  GdkCursor *cur = cursors.get_gdk_cursor(type);
  if(cur)
    gdk_window_set_cursor(win, cur);
}

void MathGtkWidget::running_state_changed() {
}

bool MathGtkWidget::is_mouse_down() {
  if(!_widget)
    return false;
    
  GdkWindow *w = gtk_widget_get_window(_widget);
  if(!w)
    return false;
    
  GdkModifierType mod = (GdkModifierType)0;
  
  gdk_window_get_pointer(w, NULL, NULL, &mod);
  return 0 != (mod & (GDK_BUTTON1_MASK | GDK_BUTTON2_MASK | GDK_BUTTON3_MASK | GDK_BUTTON4_MASK | GDK_BUTTON5_MASK));
}

void MathGtkWidget::beep() {
  gdk_beep();
}

bool MathGtkWidget::register_timed_event(SharedPtr<TimedEvent> event) {
  if(!_widget)
    return false;
    
  animations.set(event, Void());
  if(!animation_running) {
    animation_running = 0 < gdk_threads_add_timeout(ANIMATION_DELAY, animation_timeout, NULL);
    
    if(!animation_running) {
      animations.remove(event);
      return false;
    }
  }
  
  return true;
}

void MathGtkWidget::popup_detached(GtkWidget *attach_widget, GtkMenu *menu) {
  MathGtkWidget *wid = dynamic_cast<MathGtkWidget *>(BasicGtkWidget::from_widget(attach_widget));
  
  if(wid && wid->_popup_menu != NULL && GTK_MENU(wid->_popup_menu) == menu)
    wid->_popup_menu = NULL;
}

GtkMenu *MathGtkWidget::popup_menu() {
  if(!_popup_menu) {
    _popup_menu = gtk_menu_new();
    gtk_menu_attach_to_widget(GTK_MENU(_popup_menu), _widget, MathGtkWidget::popup_detached);
    
    GtkAccelGroup *accel_group = gtk_accel_group_new();
    MathGtkMenuBuilder::popup_menu.append_to(GTK_MENU_SHELL(_popup_menu), accel_group, document()->id());
    //MathGtkAccelerators::connect_all(accel_group, document()->id());
    g_object_unref(accel_group);
    
    gtk_widget_add_events(GTK_WIDGET(_popup_menu), GDK_STRUCTURE_MASK);
    
    g_signal_connect(GTK_WIDGET(_popup_menu), "map-event",   G_CALLBACK(MathGtkMenuBuilder::on_map_menu),   NULL);
    g_signal_connect(GTK_WIDGET(_popup_menu), "unmap-event", G_CALLBACK(MathGtkMenuBuilder::on_unmap_menu), NULL);
    
  }
  
  return GTK_MENU(_popup_menu);
}

static void adjustment_value_changed(
  GtkAdjustment *adjustment,
  void          *user_data
) {
  MathGtkWidget *self = (MathGtkWidget *)user_data;
  
  self->invalidate();
}

void MathGtkWidget::hadjustment(GtkAdjustment *ha) {
  if(_hadjustment) {
    g_signal_handlers_disconnect_matched(
      _hadjustment,
      G_SIGNAL_MATCH_DATA,
      0, 0, 0, 0,
      this);
      
    g_object_unref(_hadjustment);
  }
  
  _hadjustment = ha;
  if(_hadjustment) {
    g_signal_connect(_hadjustment, "value-changed", G_CALLBACK(adjustment_value_changed), this);
  }
  
  invalidate();
}

void MathGtkWidget::vadjustment(GtkAdjustment *va) {
  if(_vadjustment) {
    g_signal_handlers_disconnect_matched(
      _vadjustment,
      G_SIGNAL_MATCH_DATA,
      0, 0, 0, 0,
      this);
      
    g_object_unref(_vadjustment);
  }
  
  _vadjustment = va;
  if(_vadjustment) {
    g_signal_connect(_vadjustment, "value-changed", G_CALLBACK(adjustment_value_changed), this);
  }
  
  
  invalidate();
}

void MathGtkWidget::update_im_cursor_location() {
  GdkRectangle area;
  
  float x1 = document_context()->last_cursor_x[0];
  float x2 = document_context()->last_cursor_x[1];
  float y1 = document_context()->last_cursor_y[0];
  float y2 = document_context()->last_cursor_y[1];
  
  if(x2 < x1) {
    float t = x2;
    x2 = x1;
    x1 = t;
  }
  
  if(y2 < y1) {
    float t = y2;
    y2 = y1;
    y1 = t;
  }
  
  float sx, sy;
  scroll_pos(&sx, &sy);
  
  area.x      = (int)((sx + x1) * scale_factor());
  area.y      = (int)((sy + y1) * scale_factor());
  area.width  = (int)ceilf((x2 - x1) * scale_factor());
  area.height = (int)ceilf((y2 - y1) * scale_factor());
  
  gtk_im_context_set_cursor_location(_im_context, &area);
}

void MathGtkWidget::on_im_commit(const char *str) {
  document()->insert_string(String::FromUtf8(str), false);
}

void MathGtkWidget::on_im_preedit_changed() {
}

void MathGtkWidget::on_drag_data_get(
  GdkDragContext   *context,
  GtkSelectionData *data,
  guint             info,
  guint             time
) {
  if(!_widget)
    return;
    
  Document *doc = document();
  Box *srcbox = drag_source_reference().get();
  
  if(info >= (unsigned)drag_mime_types.length())
    return;
    
  if(!srcbox)
    return;
    
  int  old_s = doc->selection_start();
  int  old_e = doc->selection_end();
  Box *old_b = doc->selection_box();
  
  doc->select(srcbox, drag_source_reference().start, drag_source_reference().end);
  String text = doc->copy_to_text(drag_mime_types[info]);
  
  int len;
  char *str = pmath_string_to_utf8(text.get(), &len);
  GdkAtom target = MathGtkClipboard::mimetype_to_atom(drag_mime_types[info]);
  gtk_selection_data_set(data, target, 8, (const guchar *)str, len);
  pmath_mem_free(str);
  
  doc->select(old_b, old_s, old_e);
}

void MathGtkWidget::on_drag_data_delete(GdkDragContext *context) {
  if(!_widget)
    return;
    
  Document *doc = document();
  Box *srcbox = drag_source_reference().get();
  
  if(!srcbox)
    return;
    
  int  old_s = doc->selection_start();
  int  old_e = doc->selection_end();
  Box *old_b = doc->selection_box();
  
  int s = drag_source_reference().start;
  int e = drag_source_reference().end;
  doc->select(srcbox, s, e);
  doc->remove_selection();
  
  if(srcbox == old_b && doc->selection_box() == srcbox) {
    if(old_s >= e)
      old_s -= e - s;
    if(old_e >= e)
      old_e -= e - s;
  }
  
  doc->select(old_b, old_s, old_e);
  
  drag_source_reference().reset();
}

void MathGtkWidget::on_drag_data_received(
  GdkDragContext   *context,
  int               x,
  int               y,
  GtkSelectionData *data,
  guint             info,
  guint             time
) {
  float fx = x / scale_factor();
  float fy = y / scale_factor();
  float sx, sy;
  scroll_pos(&sx, &sy);
  fx += sx;
  fy += sy;
  
  if(info >= (unsigned)drag_mime_types.length()) {
    gtk_drag_finish(context, FALSE, FALSE, time);
    return;
  }
  
  GtkWidget *source_widget = gtk_drag_get_source_widget(context);
  
  int start, end;
  bool was_inside_start;
  Box *dst = document()->mouse_selection(fx, fy, &start, &end, &was_inside_start);
  
  if(!may_drop_into(dst, start, end, source_widget == _widget)) {
    gtk_drag_finish(context, FALSE, FALSE, time);
    return;
  }
  
  String mimetype(drag_mime_types[info]);
  const char *raw_data = (const char *)gtk_selection_data_get_data(data);
  int         len      =              gtk_selection_data_get_length(data);
  if(!raw_data) {
    gtk_drag_finish(context, FALSE, FALSE, time);
    return;
  }
  String text = String::FromUtf8(raw_data, len);
  
  document()->select(dst, start, end);
  document()->paste_from_text(mimetype, text);
  
  Box *newbox = document()->selection_box();
  int newend  = document()->selection_start();
  
  if(dst == newbox) {
    document()->select(newbox, start, newend);
    
    Box *src = drag_source_reference().get();
    if(src == dst) {
      int s = drag_source_reference().start;
      int e = drag_source_reference().end;
      
      if(s >= end)
        s += newend - end;
      if(e >= end)
        e += newend - end;
        
      drag_source_reference().start = s;
      drag_source_reference().end   = e;
    }
  }
  
  gtk_drag_finish(context, TRUE, gdk_drag_context_get_selected_action(context) == GDK_ACTION_MOVE, time);
}

void MathGtkWidget::on_drag_end(GdkDragContext *context) {
  drag_source_reference().reset();
}

bool MathGtkWidget::on_drag_motion(GdkDragContext *context, int x, int y, guint time) {
  if(!_widget)
    return false;
    
  MouseEvent me;
  me.left   = false;
  me.middle = false;
  me.right  = false;
  
  me.x = x;
  me.y = y;
  
  me.x /= scale_factor();
  me.y /= scale_factor();
  
  float sx, sy;
  scroll_pos(&sx, &sy);
  me.x += sx;
  me.y += sy;
  
  handle_mouse_move(me);
  
  GtkWidget *source_widget = gtk_drag_get_source_widget(context);
  
  int start, end;
  bool was_inside_start;
  Box *dst = document()->mouse_selection(me.x, me.y, &start, &end, &was_inside_start);
  
  document()->select(dst, start, end);
  if(!may_drop_into(dst, start, end, source_widget == _widget))
    return false;
    
  int action = 0;
  
  GdkAtom target = gtk_drag_dest_find_target(_widget, context, NULL);
  if(target != GDK_NONE) {
    GdkModifierType mask;
    gdk_window_get_pointer(gtk_widget_get_window(_widget), NULL, NULL, &mask);
    
    action = gdk_drag_context_get_suggested_action(context);
    if(mask & GDK_CONTROL_MASK)
      action = gdk_drag_context_get_actions(context) & GDK_ACTION_COPY;
    else if(mask & GDK_SHIFT_MASK)
      action = gdk_drag_context_get_actions(context) & GDK_ACTION_MOVE;
      
    GtkWidget *source_widget = gtk_drag_get_source_widget(context);
    if(source_widget == _widget && (gdk_drag_context_get_actions(context) & GDK_ACTION_MOVE) != 0)
      action = GDK_ACTION_MOVE;
  }
  
  gdk_drag_status(context, (GdkDragAction)action, time);
  
  return true;
}

bool MathGtkWidget::on_drag_drop(GdkDragContext *context, int x, int y, guint time) {
  if(!_widget)
    return false;
    
  float fx = x / scale_factor();
  float fy = y / scale_factor();
  
  float sx, sy;
  scroll_pos(&sx, &sy);
  fx += sx;
  fy += sy;
  
  GtkWidget *source_widget = gtk_drag_get_source_widget(context);
  
  int start, end;
  bool was_inside_start;
  Box *dst = document()->mouse_selection(fx, fy, &start, &end, &was_inside_start);
  
  if(!may_drop_into(dst, start, end, source_widget == _widget))
    return false;
    
  GdkAtom target = gtk_drag_dest_find_target(_widget, context, NULL);
  if(target != GDK_NONE)
    gtk_drag_get_data(_widget, context, target, time);
  else
    gtk_drag_finish(context, FALSE, FALSE, time);
    
  return true;
}

void MathGtkWidget::paint_background(Canvas *canvas) {
  canvas->set_color(0xffffff);
  canvas->paint();
}

void MathGtkWidget::paint_canvas(Canvas *canvas, bool resize_only) {
  cairo_set_line_width(canvas->cairo(), 1);
  cairo_set_line_cap(canvas->cairo(), CAIRO_LINE_CAP_SQUARE);
  canvas->set_font_size(10);// 10 * 4/3.
  
  if(!resize_only) {
    int color = document()->get_style(Background, -1);
    if(color >= 0) {
      canvas->set_color(color);
      canvas->paint();
    }
    else
      paint_background(canvas);
  }
  
  canvas->scale(scale_factor(), scale_factor());
  canvas->set_color(document()->get_style(FontColor, 0));
  
  document()->paint_resize(canvas, resize_only);
  
  if(gtk_widget_has_focus(_widget)
      && !is_blinking
      && document()->selection_box()
      && document()->selection_length() == 0) {
    GtkSettings *settings = gtk_widget_get_settings(_widget);
    gboolean may_blink;
    gint     blink_time;
    
    g_object_get(
      settings,
      "gtk-cursor-blink",      &may_blink,
      "gtk-cursor-blink-time", &blink_time,
      NULL);
      
    if(may_blink) {
      is_blinking = true;
      gdk_threads_add_timeout(blink_time / 2, blink_caret, (void *)(intptr_t)document()->id());
    }
  }
  
  
  if(is_scrollable()) {
    GtkAllocation rect;
    gtk_widget_get_allocation(_widget, &rect);
    
    double w_page = rect.width;
    double h_page = rect.height;
    
    double w_max = scale_factor() * document()->extents().width;
    double h_max;
    
    if(autohide_vertical_scrollbar())
      h_max = scale_factor() * document()->extents().height();
    else
      h_max = scale_factor() * document()->extents().height() + h_page * 0.8;
      
    w_max = floor(w_max + 0.5);
    h_max = floor(h_max + 0.5);
    
    if(rect.height >= h_max)
      h_page = h_max;
      
    if(rect.width >= w_max)
      w_page = w_max;
      
    if(_hadjustment) {
      g_object_set(_hadjustment,
                   "lower",     0.0,
                   "page-size", w_page,
                   "upper",     w_max,
                   NULL);
    }
    
    if(_vadjustment) {
      g_object_set(_vadjustment,
                   "lower",     0.0,
                   "page-size", h_page,
                   "upper",     h_max,
                   NULL);
    }
  }
}

void MathGtkWidget::handle_mouse_move(MouseEvent &event) {
  mouse_moving = true;
  cursor = DefaultCursor;
  
  MathGtkTooltipWindow::move_global_tooltip();
  document()->mouse_move(event);
  
  mouse_moving = false;
  set_cursor(cursor);
}

bool MathGtkWidget::on_popup_menu() {
  gtk_menu_popup(
    popup_menu(), NULL, NULL, NULL, NULL, 0, gtk_get_current_event_time());
  return true;
}

bool MathGtkWidget::on_map(GdkEvent *e) {
  gtk_im_context_set_client_window(_im_context, gtk_widget_get_window(_widget));
  return false;
}

bool MathGtkWidget::on_unmap(GdkEvent *e) {
  gtk_im_context_reset(_im_context);
  gtk_im_context_set_client_window(_im_context, 0);
  return false;
}

bool MathGtkWidget::on_draw(cairo_t *cr) {
  GtkAllocation rect;
  gtk_widget_get_allocation(_widget, &rect);
  if(old_width != rect.width) {
    old_width = rect.width;
    document()->invalidate_all();
  }
  
  is_painting = true;
  
  {
    Canvas canvas(cr);
    
    canvas.move_to(0, 0);
    canvas.line_to(rect.width, 0);
    canvas.line_to(rect.width, rect.height);
    canvas.line_to(0, rect.height);
    canvas.close_path();
    canvas.clip();
    
    paint_canvas(&canvas, false);
    
    if(_im_context_pos != document_context()->selection) {
      _im_context_pos = document_context()->selection;
      gtk_im_context_reset(_im_context);
    }
    else
      update_im_cursor_location();
  }
  
  if(!is_painting)
    invalidate();
    
//  set_cursor(cursor);
  is_painting = false;
  return true;
}

bool MathGtkWidget::on_expose(GdkEvent *e) {
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

bool MathGtkWidget::on_focus_in(GdkEvent *e) {
  Box *box = document()->selection_box();
  if(!box)
    box = document();
    
  if(box->selectable()) {
    set_current_document(document());
  }
  
  gtk_im_context_focus_in(_im_context);
  
  if(document()->selection_length() == 0) {
    invalidate();
  }
  
  return false;
}

bool MathGtkWidget::on_focus_out(GdkEvent *e) {
  if(_im_context)
    gtk_im_context_focus_out(_im_context);
    
  return false;
}

static SpecialKey keyval_to_special_key(guint keyval) {
  switch(keyval) {
    case GDK_Left:            return KeyLeft;
    case GDK_Right:           return KeyRight;
    case GDK_Up:              return KeyUp;
    case GDK_Down:            return KeyDown;
    case GDK_Home:            return KeyHome;
    case GDK_End:             return KeyEnd;
    case GDK_Page_Up:         return KeyPageUp;
    case GDK_Page_Down:       return KeyPageDown;
    case GDK_BackSpace:       return KeyBackspace;
    case GDK_Delete:          return KeyDelete;
    case GDK_Linefeed:
    case GDK_Return:
    case GDK_KP_Enter:        return KeyReturn;
    case GDK_Tab:             return KeyTab;
    case GDK_Escape:          return KeyEscape;
    case GDK_F1:              return KeyF1;
    case GDK_F2:              return KeyF2;
    case GDK_F3:              return KeyF3;
    case GDK_F4:              return KeyF4;
    case GDK_F5:              return KeyF5;
    case GDK_F6:              return KeyF6;
    case GDK_F7:              return KeyF7;
    case GDK_F8:              return KeyF8;
    case GDK_F9:              return KeyF9;
    case GDK_F10:             return KeyF10;
    case GDK_F11:             return KeyF11;
    case GDK_F12:             return KeyF12;
    default:                  return KeyUnknown;
  }
}

bool MathGtkWidget::on_key_press(GdkEvent *e) {
  GdkEventKey *event = &e->key;
  GdkModifierType mod = (GdkModifierType)0;
  
  ignore_key_release = false;
  
  GtkWidget *wid = _widget;
  while(wid && !GTK_IS_WINDOW(wid))
    wid = gtk_widget_get_parent(wid);
    
  if(wid) {
    guint keyval = 0;
    
    switch(event->keyval) {
      case GDK_dead_grave:      keyval = GDK_grave;       break;
      case GDK_dead_circumflex: keyval = GDK_asciicircum; break;
    }
    
    if(keyval && gtk_accel_groups_activate(G_OBJECT(wid), keyval, (GdkModifierType)event->state)) {
      pmath_debug_print("[accel %x %x]", keyval, event->state);
      ignore_key_release = true;
      return true;
    }
  }
  
  if(gtk_im_context_filter_keypress(_im_context, event)) {
    return true;
  }
  
  {
    GdkWindow *w = gtk_widget_get_window(_widget);
    
    gdk_window_get_pointer(w, NULL, NULL, &mod);
  }
  
  SpecialKeyEvent ske;
  ske.key = keyval_to_special_key(event->keyval);
  ske.ctrl  = 0 != (mod & GDK_CONTROL_MASK);
  ske.alt   = 0 != (mod & GDK_MOD1_MASK);
  ske.shift = 0 != (mod & GDK_SHIFT_MASK);
  if(ske.key) {
    document()->key_down(ske);
  }
  
  if(event->keyval == GDK_Caps_Lock || event->keyval == GDK_Shift_Lock) {
  
#ifdef GDK_WINDOWING_X11
  
    // Reset CAPS LOCK and input PMATH_CHAR_ALIASDELIMITER.
    //
    // If you type too fast, the next few character(s) are inserted before we turned off CAPS LOCK.
    //
    // Based on numlockx.c and
    // bugzilla.novell.com/show_bug.cgi?id=394949
    // (Bug 394949 - Simulated Caps lock key press via XTestFakeKeyEvent does not toggle the LED indicator)
    
    Display *display = XOpenDisplay(NULL);
    
    unsigned state;
    
    XkbGetIndicatorState(display, XkbUseCoreKbd, &state);
    bool capslock_active_before = (state & 0x01) == 1;
    
    if(capslock_active_before) {
      int capslock_mask = 0;
      
      KeyCode capslock_keycode = XKeysymToKeycode(display, XK_Caps_Lock);
      XModifierKeymap *map = XGetModifierMapping(display);
      for(int i = 0; i < 8; ++i) {
        if(map->modifiermap[map->max_keypermod * i] == capslock_keycode)
          capslock_mask = 1 << i;
      }
      XFreeModifiermap(map);
      
      if(capslock_mask != 0) {
        XkbLockModifiers(display, XkbUseCoreKbd, capslock_mask, 0);
        
        document()->key_press(PMATH_CHAR_ALIASDELIMITER);
      }
    }
    
    XCloseDisplay(display);
    
#endif
    
    return true;
  }
  
  if(event->keyval == GDK_Menu || (event->keyval == GDK_F10 && (mod & GDK_SHIFT_MASK)))
    gtk_menu_popup(
      popup_menu(), NULL, NULL, NULL, NULL, 0, event->time);
  
  if(ske.ctrl || (ske.alt && !ske.shift))
    return false;
    
  uint32_t unichar = gdk_keyval_to_unicode(event->keyval);
  if(event->keyval == GDK_Return || event->keyval == GDK_Linefeed)
    unichar = '\n';
    
  if(unichar) {
    if((unichar == ' ' || unichar == '\r' || unichar == '\n') &&
        (ske.ctrl || ske.alt || ske.shift))
    {
      return false;
    }
    if(unichar == '\t')
      return false;
      
    document()->key_press(unichar);
  }
  
  return true;
}

bool MathGtkWidget::on_key_release(GdkEvent *e) {
  GdkEventKey *event = &e->key;
  GdkModifierType mod = (GdkModifierType)0;
  
  if(ignore_key_release)
    return true;
    
  if(gtk_im_context_filter_keypress(_im_context, event))
    return true;
    
  {
    GdkWindow *w = gtk_widget_get_window(_widget);
    
    gdk_window_get_pointer(w, NULL, NULL, &mod);
  }
  
  SpecialKeyEvent ske;
  ske.key = keyval_to_special_key(event->keyval);
  if(ske.key) {
    ske.ctrl  = 0 != (mod & GDK_CONTROL_MASK);
    ske.alt   = 0 != (mod & GDK_MOD1_MASK);
    ske.shift = 0 != (mod & GDK_SHIFT_MASK);
    document()->key_up(ske);
  }
  
  return true;
}

bool MathGtkWidget::on_button_press(GdkEvent *e) {
  GdkEventButton *event = (GdkEventButton *)e;
  
  if(event->type != GDK_BUTTON_PRESS)
    return true;
    
  _mouse_down_button = event->button;
  
  MouseEvent me;
  me.left   = event->button == 1;
  me.middle = event->button == 2;
  me.right  = event->button == 3;
  
  me.x = event->x;
  me.y = event->y;
  
  me.x /= scale_factor();
  me.y /= scale_factor();
  
  float sx, sy;
  scroll_pos(&sx, &sy);
  me.x += sx;
  me.y += sy;
  
  document()->mouse_down(me);
  
  if(document()->selection_box()) {
    gtk_widget_grab_focus(_widget);
  }
//  else {
//    Document *cur = get_current_document();
//    if(cur && cur != document()) {
//      MathGtkWidget *w = dynamic_cast<MathGtkWidget*>(cur->native());
//
//      if(w && !gtk_widget_has_focus(w->widget())) {
//
//        GtkWidget *wid = w->widget();
//        GtkWidget *parent = gtk_widget_get_parent(wid);
//        while(parent) {
//          wid = parent;
//          parent = gtk_widget_get_parent(wid);
//        }
//        gtk_widget_grab_focus(wid);
//
//        gtk_widget_grab_focus(w->widget());
//      }
//    }
//  }

  if(me.right) {
    gtk_menu_popup(
      popup_menu(), NULL, NULL, NULL, NULL, event->button, event->time);
  }
  
  return true;
}

bool MathGtkWidget::on_button_release(GdkEvent *e) {
  GdkEventButton *event = (GdkEventButton *)e;
  
  _mouse_down_button = 0;
  
  MouseEvent me;
  me.left   = event->button == 1;
  me.middle = event->button == 2;
  me.right  = event->button == 3;
  
  me.x = event->x;
  me.y = event->y;
  
  me.x /= scale_factor();
  me.y /= scale_factor();
  
  float sx, sy;
  scroll_pos(&sx, &sy);
  me.x += sx;
  me.y += sy;
  
  document()->mouse_up(me);
  
  return true;
}

bool MathGtkWidget::on_motion_notify(GdkEvent *e) {
  GdkEventMotion *event = (GdkEventMotion *)e;
  
  /* This call is very important; it requests the next motion event.
   * If you don't call gdk_window_get_pointer() you'll only get
   * a single motion event. The reason is that we specified
   * GDK_POINTER_MOTION_HINT_MASK to gtk_widget_set_events().
   * If we hadn't specified that, we could just use event->x, event->y
   * as the pointer location. But we'd also get deluged in events.
   * By requesting the next event as we handle the current one,
   * we avoid getting a huge number of events faster than we
   * can cope.
   */
  int x, y;
  GdkModifierType state;
  gdk_window_get_pointer(event->window, &x, &y, &state);
  
  MouseEvent me;
  me.left   = 0 != (event->state & GDK_BUTTON1_MASK);
  me.middle = 0 != (event->state & GDK_BUTTON2_MASK);
  me.right  = 0 != (event->state & GDK_BUTTON3_MASK);
  
  me.x = event->x;
  me.y = event->y;
  
  me.x /= scale_factor();
  me.y /= scale_factor();
  
  float sx, sy;
  scroll_pos(&sx, &sy);
  me.x += sx;
  me.y += sy;
  
  handle_mouse_move(me);
  return true;
}

bool MathGtkWidget::on_leave_notify(GdkEvent *e) {
  //GdkEventCrossing *event = (GdkEventCrossing*)e;
  
  document()->mouse_exit();
  return false;
}

bool MathGtkWidget::on_scroll(GdkEvent *e) {
  GdkEventScroll *event = (GdkEventScroll *)e;
  
  if(event->state & GDK_CONTROL_MASK) {
    switch(event->direction) {
      case GDK_SCROLL_UP:
        scale_by(pow(2, 0.5));
        break;
        
      case GDK_SCROLL_DOWN:
        scale_by(pow(2, -0.5));
        break;
        
      case GDK_SCROLL_LEFT:  break;
      case GDK_SCROLL_RIGHT: break;
      
#if GTK_MAJOR_VERSION >= 3
      case GDK_SCROLL_SMOOTH:
        {
          double ddx, ddy;
          gdk_event_get_scroll_deltas(e, &ddx, &ddy);
          scale_by(pow(2, -0.5 * ddy));
        }
        break;
#endif
    }
    
    return true;
  }
  
  float dx = 0;
  float dy = 0;
  switch(event->direction) {
    case GDK_SCROLL_UP:    dy = - 60; break;
    case GDK_SCROLL_DOWN:  dy = + 60; break;
    case GDK_SCROLL_LEFT:  dx = - 60; break;
    case GDK_SCROLL_RIGHT: dx = + 60; break;
    
#if GTK_MAJOR_VERSION >= 3
    case GDK_SCROLL_SMOOTH:
      {
        double ddx, ddy;
        gdk_event_get_scroll_deltas(e, &ddx, &ddy);
        dx = 60 * ddx;
        dy = 60 * ddy;
      }
      break;
#endif
  }
  
  scroll_by(dx, dy);
  
  return true;
}

gboolean MathGtkWidget::blink_caret(gpointer id_as_ptr) {
  int id = (int)(intptr_t)id_as_ptr;
  
  Document *doc = dynamic_cast<Document *>(Box::find(id));
  if(doc) {
    MathGtkWidget *wid = dynamic_cast<MathGtkWidget *>(doc->native());
    
    if(wid) {
      Context *ctx = wid->document_context();
      
      if( ctx->old_selection == ctx->selection ||
          !gtk_widget_is_focus(wid->widget()) ||
          wid->is_mouse_down())
      {
        ctx->old_selection.id = 0;
      }
      else
        ctx->old_selection = ctx->selection;
        
      Box *box = ctx->selection.get();
      if(box)
        box->request_repaint_range(ctx->selection.start, ctx->selection.end);
        
        
      wid->is_blinking = false;
    }
  }
  
  return FALSE;
}

//} ... class MathGtkWidget





