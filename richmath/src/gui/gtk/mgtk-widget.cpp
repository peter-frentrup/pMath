#include <gui/gtk/mgtk-widget.h>

#include <boxes/gridbox.h>
#include <eval/binding.h>
#include <gui/gtk/mgtk-attached-popup-window.h>
#include <gui/gtk/mgtk-clipboard.h>
#include <gui/gtk/mgtk-dragdrophandler.h>
#include <gui/gtk/mgtk-icons.h>
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

#undef None

extern pmath_symbol_t richmath_System_List;

#if !GTK_CHECK_VERSION(2,22,0)
static GdkDragAction gdk_drag_context_get_selected_action(GdkDragContext *context) {
  return context.action;
}

static GdkDragAction gdk_drag_context_get_suggested_action(GdkDragContext *context) {
  return context.action;
}

static GdkDragAction gdk_drag_context_get_actions(GdkDragContext *context) {
  return context.actions;
}
#endif


namespace richmath { namespace strings {
  extern String Popup;
}}

extern pmath_symbol_t richmath_System_Menu;

using namespace richmath;

#define ANIMATION_DELAY  (16)
static bool animation_running = false;
Hashset<SharedPtr<TimedEvent>> animations;

static gboolean animation_timeout(gpointer data) {
  animation_running = false;
  
  for(auto e : animations.deletable_entries()) {
    if(e.key->min_wait_seconds <= e.key->timer()) {
      auto anim = e.key;
      e.delete_self();
      anim->execute_event();
    }
    else
      animation_running = true;
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
    
    drop_targets = gtk_target_list_new(nullptr, 0);
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

static DeviceKind get_pointer_device(GdkDevice *device) {
  if(!device)
    return DeviceKind::Mouse;
  
  switch(gdk_device_get_source(device)) {
    case GDK_SOURCE_MOUSE:  return DeviceKind::Mouse;
    
    case GDK_SOURCE_ERASER: return DeviceKind::Pen; // TODO add a DeviceKind::PenEraser ?
    case GDK_SOURCE_PEN:    return DeviceKind::Pen;
    
#if GTK_CHECK_VERSION(3,4,0)
    case GDK_SOURCE_TOUCHSCREEN: return DeviceKind::Touch;
    case GDK_SOURCE_TOUCHPAD:    return DeviceKind::Mouse;
#endif
    
    default: break;
  }
  
  return DeviceKind::Mouse;
}

static DeviceKind get_pointer_device_for_event(GdkEvent *e) {
#if GTK_CHECK_VERSION(3,22,0)
  if(GdkDeviceTool *tool = gdk_event_get_device_tool(e)) {
    switch(gdk_device_tool_get_tool_type(tool)) {
      case GDK_DEVICE_TOOL_TYPE_ERASER:   return DeviceKind::Pen; // TODO add a DeviceKind::PenEraser ?
      case GDK_DEVICE_TOOL_TYPE_PEN:      return DeviceKind::Pen;
      case GDK_DEVICE_TOOL_TYPE_BRUSH:    return DeviceKind::Pen;
      case GDK_DEVICE_TOOL_TYPE_PENCIL:   return DeviceKind::Pen;
      case GDK_DEVICE_TOOL_TYPE_AIRBRUSH: return DeviceKind::Pen;
      
      case GDK_DEVICE_TOOL_TYPE_MOUSE: return DeviceKind::Mouse;
      
      // TODO: how to detect touch input??
      
      default:
        break;
    }
  }
#endif
  
  switch(e->type) {
    case GDK_BUTTON_PRESS:
    case GDK_BUTTON_RELEASE:
      return get_pointer_device(((GdkEventButton*)e)->device);
    
    case GDK_MOTION_NOTIFY:
      return get_pointer_device(((GdkEventMotion*)e)->device);
    
    case GDK_SCROLL:
      return get_pointer_device(((GdkEventScroll*)e)->device);
    
    case GDK_PROXIMITY_IN:
    case GDK_PROXIMITY_OUT:
      return get_pointer_device(((GdkEventProximity*)e)->device);
    
    default:
      break;
  }
  
  return DeviceKind::Mouse;
}

//{ class MathGtkWidget ...

MathGtkWidget::MathGtkWidget(Document *doc)
  : NativeWidget(doc),
    BasicGtkWidget(),
    _autohide_vertical_scrollbar(false),
    _mouse_down_button(0),
    is_blinking(false),
    ignore_key_release(true),
    _focused(false),
    old_width(0),
    _hadjustment(nullptr),
    _vadjustment(nullptr),
    _im_context(gtk_im_multicontext_new())
{
  add_remove_widget(+1);
  g_signal_connect(_im_context, "commit",          G_CALLBACK(&MathGtkWidget::im_commit_callback),          this);
  g_signal_connect(_im_context, "preedit_changed", G_CALLBACK(&MathGtkWidget::im_preedit_changed_callback), this);
  
  gtk_im_context_set_use_preedit(_im_context, FALSE);
}

MathGtkWidget::~MathGtkWidget() {
  hadjustment(nullptr);
  vadjustment(nullptr);
  
  g_signal_handlers_disconnect_matched(
    _im_context,
    G_SIGNAL_MATCH_DATA,
    0, 0, 0, 0,
    this);
    
  g_object_unref(_im_context);
  _im_context = nullptr;
  
  add_remove_widget(-1);
}

void MathGtkWidget::after_construction() {
  BasicGtkWidget::after_construction();
  
  gtk_widget_set_events(_widget, GDK_ALL_EVENTS_MASK);
  gtk_widget_set_can_focus(_widget, TRUE);
  
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

Vector2F MathGtkWidget::window_size() {
  if(!_widget) 
    return Vector2F(0, 0);
  
  GtkAllocation rect;
  gtk_widget_get_allocation(_widget, &rect);
  old_width.register_observer();
  return Vector2F(rect.width, rect.height) / scale_factor();
}

Point MathGtkWidget::scroll_pos() {
  if(!is_scrollable())
    return Point(0, 0);
  
  Point sp(0, 0);
  if(_hadjustment) {
    sp.x = gtk_adjustment_get_value(_hadjustment);
    sp.x /= scale_factor();
  }
  
  if(_vadjustment) {
    sp.y = gtk_adjustment_get_value(_vadjustment);
    sp.y /= scale_factor();
  }
  
  return sp;
}

void MathGtkWidget::scroll_to(Point pos) {
  if(!is_scrollable())
    return;
    
  if(_hadjustment) {
    double oldx = gtk_adjustment_get_value(_hadjustment);
    double newx = pos.x * scale_factor();
    
    double lo = gtk_adjustment_get_lower(_hadjustment);
    double hi = gtk_adjustment_get_upper(_hadjustment);
    hi -=       gtk_adjustment_get_page_size(_hadjustment);
    
    newx = CLAMP(newx, lo, hi);
    if(oldx != newx)
      gtk_adjustment_set_value(_hadjustment, newx);
  }
  
  if(_vadjustment) {
    double oldy = gtk_adjustment_get_value(_vadjustment);
    double newy = pos.y * scale_factor();
    
    double lo = gtk_adjustment_get_lower(_vadjustment);
    double hi = gtk_adjustment_get_upper(_vadjustment);
    hi -=       gtk_adjustment_get_page_size(_vadjustment);
    
    newy = CLAMP(newy, lo, hi);
    if(oldy != newy)
      gtk_adjustment_set_value(_vadjustment, newy);
  }
}

void MathGtkWidget::show_tooltip(Box *source, Expr boxes) {
  MathGtkTooltipWindow::show_global_tooltip(source, boxes, document()->stylesheet());
}

void MathGtkWidget::hide_tooltip() {
  MathGtkTooltipWindow::hide_global_tooltip();
}

Document *MathGtkWidget::try_create_popup_window(const SelectionReference &anchor) {
  Box *anchor_box = FrontEndObject::find_cast<Box>(anchor.id);
  if(!document()->is_parent_of(anchor_box))
    return nullptr;
  
  auto *popup = new MathGtkAttachedPopupWindow(document(), anchor_box);
  popup->init();
  return popup->content();
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
    nullptr);
    
  return t / 1000.0;
}

Vector2F MathGtkWidget::double_click_dist() {
  if(!_widget) 
    return Vector2F(0, 0);
  
  GtkSettings *settings = gtk_widget_get_settings(_widget);
  gint d;
  
  g_object_get(
    settings,
    "gtk-double-click-distance", &d,
    nullptr);
    
  return Vector2F(d, d) / scale_factor();
}

void MathGtkWidget::do_drag_drop(const VolatileSelection &src, MouseEvent &event) {
  GdkEvent *g_event = gtk_get_current_event();
  
  if(!src || !_widget)
    return;
    
  int hot_x = 0;
  int hot_y = 0;
  GdkPixbuf *drag_image = nullptr;
  {
    Document *doc = document();
    AutoResetSelection ars(doc);
    
    cairo_surface_t *image = cairo_image_surface_create(CAIRO_FORMAT_RGB24, 1, 1);
    RectangleF rect = doc->prepare_copy_to_image(image);
    cairo_surface_destroy(image);
    
    int w = (int)ceil(rect.width - 0.001);
    int h = (int)ceil(rect.height - 0.001);
    if(w < 1) w = 1;
    if(h < 1) h = 1;
    
    /* hot_x, hot_y calculation below is correct, but with opaque drag image, we 
       should place the image below the cursor
     */
//    Point sp = scroll_pos();
//    event.set_origin(doc);
//    float px = (event.x - sp.x) * scale_factor();
//    float py = (event.y - sp.y) * scale_factor();
//    hot_x = (int)ceil(px - rect.x);
//    hot_y = (int)ceil(py - rect.y);
    
    image = cairo_image_surface_create(CAIRO_FORMAT_RGB24, w, h);
    doc->finish_copy_to_image(image, rect);
    
//    cairo_surface_t *alpha_image = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
//    cairo_t *cr = cairo_create(alpha_image);
//    cairo_set_source_surface(cr, image, 0, 0);
//    cairo_paint_with_alpha(cr, 0.5);
//    cairo_destroy(cr);
//    cairo_surface_destroy(image); image = alpha_image;
    
    cairo_surface_flush(image);
    GdkPixbuf *pixbuf = MathGtkIcons::new_pixbuf_from_image(image);
    cairo_surface_destroy(image);
    
    drag_image = gdk_pixbuf_add_alpha(pixbuf, false, 0, 0, 0);
    g_object_unref(pixbuf);
  }
  
  drag_source_reference().set(src);
  
  GdkDragContext *context;
  unsigned actions = GDK_ACTION_COPY;
  if(src.box->get_style(Editable))
    actions |= GDK_ACTION_MOVE;
    
  GridBox::selection_strategy = GridBox::best_selection_strategy_for_drag_source(src);
  context = gtk_drag_begin(
              _widget,
              drop_targets,
              (GdkDragAction)actions,
              _mouse_down_button,
              g_event);
  
  if(drag_image) {
    gtk_drag_set_icon_pixbuf(context, drag_image, hot_x, hot_y);
    g_object_unref(drag_image);
  }
  else
    gtk_drag_set_icon_default(context);
}

void MathGtkWidget::bring_to_front() {
  gtk_widget_grab_focus(_widget);
}

void MathGtkWidget::invalidate() {
  if(!_widget)
    return;
    
  gtk_widget_queue_draw(_widget);
}

void MathGtkWidget::invalidate_options() {
}

void MathGtkWidget::invalidate_rect(const RectangleF &rect) {
  if(!_widget)
    return;
    
  Point sp = scroll_pos();
  float sf = scale_factor();
  
  gtk_widget_queue_draw_area(_widget,
                             (int)floorf((rect.x - sp.x) * sf) - 4,
                             (int)floorf((rect.y - sp.y) * sf) - 4,
                             (int)ceilf(rect.width  * sf) + 8,
                             (int)ceilf(rect.height * sf) + 8);
}

void MathGtkWidget::force_redraw() {
  invalidate();
}

void MathGtkWidget::set_cursor(CursorType type) {
  if(mouse_moving) {
    cursor = type;
    return;
  }
  
  if(!_widget) 
    return;
  
  GdkWindow *win = gtk_widget_get_window(_widget);
  if(GdkCursor *cur = cursors.get_gdk_cursor(type)) {
    if(cur == gdk_window_get_cursor(win)) {
      /* Must not attempt to set the cursor again, because GTK 3 would otherwise choose the default 
         mouse cursor (at least on Win32).
       */
      gdk_cursor_unref(cur);
      return;
    }
    
    gdk_window_set_cursor(win, cur);
  }
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
  
  gdk_window_get_pointer(w, nullptr, nullptr, &mod);
  return 0 != (mod & (GDK_BUTTON1_MASK | GDK_BUTTON2_MASK | GDK_BUTTON3_MASK | GDK_BUTTON4_MASK | GDK_BUTTON5_MASK));
}

void MathGtkWidget::beep() {
  gdk_beep();
}

bool MathGtkWidget::register_timed_event(SharedPtr<TimedEvent> event) {
  if(!_widget)
    return false;
    
  animations.add(event);
  if(!animation_running) {
    animation_running = 0 < gdk_threads_add_timeout(ANIMATION_DELAY, animation_timeout, nullptr);
    
    if(!animation_running) {
      animations.remove(event);
      return false;
    }
  }
  
  return true;
}

GtkMenu *MathGtkWidget::create_popup_menu(VolatileSelection src, ObjectStyleOptionName style_name) {
  if(!src.box)
    src.box = document();
  
  Expr menu_expr = src.box->get_finished_flatlist_style(style_name);
  if(menu_expr[0] == richmath_System_List && menu_expr.expr_length() > 0) {
    menu_expr = Call(Symbol(richmath_System_Menu), strings::Popup, std::move(menu_expr));
  }
  else
    return nullptr;
  
  GtkMenu *new_popup_menu = GTK_MENU(gtk_menu_new());
  g_object_ref_sink(new_popup_menu);
  
  GtkAccelGroup *accel_group = gtk_accel_group_new();
  MathGtkMenuBuilder(menu_expr).append_to(GTK_MENU_SHELL(new_popup_menu), accel_group, src.box->id());
  //MathGtkAccelerators::connect_all(accel_group, src.box->id());
  g_object_unref(accel_group);
  
  MathGtkMenuBuilder::connect_events(new_popup_menu, document()->id());
  
  return new_popup_menu;
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
  RectangleF last_cursor_rect {
    document_context()->last_cursor_pos[0], 
    document_context()->last_cursor_pos[1] }; 
  
  last_cursor_rect.normalize();
  
  Point sp = scroll_pos();
  
  area.x      = (int)((sp.x + last_cursor_rect.x) * scale_factor());
  area.y      = (int)((sp.y + last_cursor_rect.y) * scale_factor());
  area.width  = (int)ceilf(last_cursor_rect.width * scale_factor());
  area.height = (int)ceilf(last_cursor_rect.height * scale_factor());
  
  gtk_im_context_set_cursor_location(_im_context, &area);
}

void MathGtkWidget::on_im_commit(const char *str) {
  String s = String::FromUtf8(str);
  if(s.length() == 1) {
    document()->key_press(s[0]);
    return;
  }
  document()->insert_string(s, false);
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
  VolatileSelection drag_src = drag_source_reference().get_all();
  
  if(info >= (unsigned)drag_mime_types.length())
    return;
    
  if(!drag_src)
    return;
    
  VolatileSelection old_sel = doc->selection_now();
  
  doc->select(drag_src);
  String text = doc->copy_to_text(drag_mime_types[info]);
  
  int len;
  char *str = pmath_string_to_utf8(text.get(), &len);
  GdkAtom target = MathGtkClipboard::mimetype_to_atom(drag_mime_types[info]);
  gtk_selection_data_set(data, target, 8, (const guchar *)str, len);
  pmath_mem_free(str);
  
  doc->select(old_sel);
}

void MathGtkWidget::on_drag_data_delete(GdkDragContext *context) {
  if(!_widget)
    return;
    
  Document *doc = document();
  SelectionReference drag_src = drag_source_reference();
  drag_source_reference().reset();
  
  doc->remove_selection(drag_src);
}

void MathGtkWidget::on_drag_data_received(
  GdkDragContext   *context,
  int               x,
  int               y,
  GtkSelectionData *data,
  guint             info,
  guint             time
) {
  Point pos = scroll_pos() + Vector2F(x,y) / scale_factor();
  
  if(info >= (unsigned)drag_mime_types.length()) {
    gtk_drag_finish(context, FALSE, FALSE, time);
    return;
  }
  
  GtkWidget *source_widget = gtk_drag_get_source_widget(context);
  
  bool was_inside_start;
  VolatileSelection dst = document()->mouse_selection(pos, &was_inside_start);
  
  if(!may_drop_into(dst, source_widget == _widget)) {
    gtk_drag_finish(context, FALSE, FALSE, time);
    return;
  }
  
  String mimetype(drag_mime_types[info]);
  const char *raw_data = (const char *)gtk_selection_data_get_data(data);
  int         len      =               gtk_selection_data_get_length(data);
  if(!raw_data) {
    gtk_drag_finish(context, FALSE, FALSE, time);
    return;
  }
  String text = String::FromUtf8(raw_data, len);
  
  document()->select(dst);
  if(gdk_drag_context_get_selected_action(context) == GDK_ACTION_MOVE) {
    if(SelectionReference drag_src = drag_source_reference()) {
      drag_source_reference().reset();
      
      document()->remove_selection(drag_src);
      dst = document()->selection_now();
    }
  }
  
  document()->paste_from_text(mimetype, text);
  
  Box *newbox = document()->selection_box();
  int newend  = document()->selection_start();
  
  if(dst.box == newbox) 
    document()->select(newbox, dst.start, newend);
  
  gtk_drag_finish(context, TRUE, gdk_drag_context_get_selected_action(context) == GDK_ACTION_MOVE, time);
}

void MathGtkWidget::on_drag_end(GdkDragContext *context) {
  GridBox::selection_strategy = GridSelectionStrategy::ContentsOnly;
  drag_source_reference().reset();
  
  gtk_widget_queue_draw(_widget);
}

bool MathGtkWidget::on_drag_motion(GdkDragContext *context, int x, int y, guint time) {
  if(!_widget)
    return false;
    
  MouseEvent me;

#if GTK_MAJOR_VERSION >= 3
  // TODO: maybe we should ace as a Mouse device if/when Touch move events without me.left are ignored?
  me.device = get_pointer_device(gdk_drag_context_get_device(context));
#endif
  
  me.left   = false;
  me.middle = false;
  me.right  = false;
  
  me.position = scroll_pos() + Vector2F(x, y) / scale_factor();
  
  handle_mouse_move(me);
  
  GtkWidget *source_widget = gtk_drag_get_source_widget(context);
  
  bool was_inside_start;
  VolatileSelection dst = document()->mouse_selection(me.position, &was_inside_start);
  
  document()->select(dst);
  bool self_is_source = source_widget == _widget;
  if(!may_drop_into(dst, self_is_source))
    return false;
    
  int action = 0;
  
  GdkAtom target = gtk_drag_dest_find_target(_widget, context, nullptr);
  if(target != GDK_NONE) {
    GdkModifierType mask;
    gdk_window_get_pointer(gtk_widget_get_window(_widget), nullptr, nullptr, &mask);
    
    if(mask & (GDK_MOD1_MASK | GDK_BUTTON2_MASK)) { // ALT+drag  or  middle mouse button drag
      action = GDK_ACTION_ASK;
    }
    else {
      GdkDragAction allowed_actions = gdk_drag_context_get_actions(context);
      
      if(self_is_source) {
        if(allowed_actions & GDK_ACTION_MOVE)
          action = GDK_ACTION_MOVE;
        else
          action = allowed_actions & GDK_ACTION_COPY;
      }
      else
        action = allowed_actions & GDK_ACTION_COPY;
      
      if(mask & GDK_CONTROL_MASK)
        action = allowed_actions & GDK_ACTION_COPY;
      else if(mask & GDK_SHIFT_MASK)
        action = allowed_actions & GDK_ACTION_MOVE;
    }
  }
  
  gdk_drag_status(context, (GdkDragAction)action, time);
  
  return true;
}

bool MathGtkWidget::on_drag_drop(GdkDragContext *context, int x, int y, guint time) {
  if(!_widget)
    return false;
    
  Point pos = scroll_pos() + Vector2F(x, y) / scale_factor();
  
  GtkWidget *source_widget = gtk_drag_get_source_widget(context);
  
  bool was_inside_start;
  VolatileSelection dst = document()->mouse_selection(pos, &was_inside_start);
  
  if(!may_drop_into(dst, source_widget == _widget))
    return false;
  
  GdkDragAction action = gdk_drag_context_get_selected_action(context);
  if(action == GDK_ACTION_ASK) {
    if(auto menu = create_popup_menu(dst, DragDropContextMenu)) {
      g_object_set_data_full(
        G_OBJECT(menu), 
        "richmath-drag-drop-handler", 
        new MathGtkDragDropHandler(context), 
        [](void *p) { if(p) static_cast<MathGtkDragDropHandler*>(p)->unref(); });
      
      // Note that gtk_menu_popup does not block.
      bring_to_front();
      gtk_menu_popup(menu, nullptr, nullptr, nullptr, nullptr, 0, time);
      g_object_unref(menu);
      return true;
    }
  }
    
  GdkAtom target = gtk_drag_dest_find_target(_widget, context, nullptr);
  if(target != GDK_NONE) {
    do_drop_data(context, action, target, time);
  }
  else
    gtk_drag_finish(context, FALSE, FALSE, time);
    
  bring_to_front();
  return true;
}

void MathGtkWidget::do_drop_data(GdkDragContext *context, GdkDragAction action, GdkAtom target, guint time) {
  GtkWidget *source_widget = gtk_drag_get_source_widget(context);
  
  bool need_data = true;
  if(MathGtkWidget *wid = dynamic_cast<MathGtkWidget*>(BasicGtkWidget::from_widget(source_widget))) {
    if(SelectionReference drag_src = wid->drag_source_reference()) {
      Expr boxes = drag_src.get_all().to_pmath(BoxOutputFlags::Default);
      
      if(action == GDK_ACTION_MOVE) {
        drag_source_reference().reset();
        document()->remove_selection(drag_src);
      }
    
      document()->paste_from_boxes(boxes);
      need_data = false;
      gtk_drag_finish(context, TRUE, action == GDK_ACTION_MOVE, time);
    }
  }
  
  if(need_data)
    gtk_drag_get_data(_widget, context, target, time);
}

void MathGtkWidget::paint_background(Canvas &canvas) {
//  cairo_set_operator(canvas.cairo(), CAIRO_OPERATOR_CLEAR);
//  canvas.set_color(Color::Black, 0.0);
//  canvas.paint();
//  cairo_set_operator(canvas.cairo(), CAIRO_OPERATOR_OVER);
}

void MathGtkWidget::paint_canvas(Canvas &canvas, bool resize_only) {
  cairo_set_line_width(canvas.cairo(), 1);
  cairo_set_line_cap(canvas.cairo(), CAIRO_LINE_CAP_SQUARE);
  canvas.set_font_size(10);// 10 * 4/3.
  
  if(!resize_only) {
    Color color = document()->get_style(Background);
    if(color.is_valid()) {
      canvas.set_color(color);
      canvas.paint();
    }
    else
      paint_background(canvas);
    
    bool old_has_dark_background = _has_dark_background;
    _has_dark_background = color.is_dark();
    if(old_has_dark_background != _has_dark_background)
      on_changed_dark_mode();
  }
  
  canvas.scale(scale_factor(), scale_factor());
  canvas.set_color(document()->get_style(FontColor, Color::Black));
  
  document()->paint_resize(canvas, resize_only);
  
  if( gtk_widget_has_focus(_widget) && 
      !is_blinking && 
      document()->selection_box() && 
      document()->selection_length() == 0)
  {
    GtkSettings *settings = gtk_widget_get_settings(_widget);
    gboolean may_blink;
    gint     blink_time;
    
    g_object_get(
      settings,
      "gtk-cursor-blink",      &may_blink,
      "gtk-cursor-blink-time", &blink_time,
      nullptr);
      
    if(may_blink) {
      is_blinking = true;
      gdk_threads_add_timeout(blink_time / 2, blink_caret, FrontEndReference::unsafe_cast_to_pointer(document()->id()));
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
      
    w_max = round(w_max);
    h_max = round(h_max);
    
    if(rect.height >= h_max)
      h_page = h_max;
      
    if(rect.width >= w_max)
      w_page = w_max;
      
    if(_hadjustment) {
      double old_lower = gtk_adjustment_get_lower(_hadjustment);
      double old_upper = gtk_adjustment_get_upper(_hadjustment);
      double old_page  = gtk_adjustment_get_page_size(_hadjustment);
      
      if(old_lower != 0.0 || old_upper != w_max || old_page != w_page) {
        g_object_set(_hadjustment,
                     "lower",     0.0,
                     "page-size", w_page,
                     "upper",     w_max,
                     nullptr);
      }
    }
    
    if(_vadjustment) {
      double old_lower = gtk_adjustment_get_lower(_vadjustment);
      double old_upper = gtk_adjustment_get_upper(_vadjustment);
      double old_page  = gtk_adjustment_get_page_size(_vadjustment);
      
      if(old_lower != 0.0 || old_upper != h_max || old_page != h_page) {
        g_object_set(_vadjustment,
                     "lower",     0.0,
                     "page-size", h_page,
                     "upper",     h_max,
                     nullptr);
      }
    }
  }
}

void MathGtkWidget::on_changed_dark_mode() {
}

void MathGtkWidget::handle_mouse_move(MouseEvent &event) {
  mouse_moving = true;
  cursor = CursorType::Default;
  
  MathGtkTooltipWindow::move_global_tooltip();
  document()->mouse_move(event);
  
  mouse_moving = false;
  set_cursor(cursor);
}

bool MathGtkWidget::on_map(GdkEvent *e) {
  gtk_im_context_set_client_window(_im_context, gtk_widget_get_window(_widget));
  return false;
}

bool MathGtkWidget::on_unmap(GdkEvent *e) {
  // FIXME: unmap-event is only sent to the top-level window, not its child widgets
  document()->invalidate_popup_window_positions();
  
  gtk_im_context_reset(_im_context);
  gtk_im_context_set_client_window(_im_context, nullptr);
  return false;
}

bool MathGtkWidget::on_draw(cairo_t *cr) {
  GtkAllocation rect;
  gtk_widget_get_allocation(_widget, &rect);
  if(!old_width.unobserved_equals(rect.width)) {
    old_width = rect.width;
    document()->invalidate_all();
  }
  
  {
    Canvas canvas(cr);
    
    canvas.move_to(0, 0);
    canvas.line_to(rect.width, 0);
    canvas.line_to(rect.width, rect.height);
    canvas.line_to(0, rect.height);
    canvas.close_path();
    canvas.clip();
    
    paint_canvas(canvas, false);
    
    if(_im_context_pos != document_context()->selection) {
      _im_context_pos = document_context()->selection;
      gtk_im_context_reset(_im_context);
    }
    else
      update_im_cursor_location();
  }
  
//  set_cursor(cursor);
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

static FrontEndReference focussed_document_id = FrontEndReference::None;

bool MathGtkWidget::on_focus_in(GdkEvent *e) {
  _focused = true;
  document()->focus_set();
  
  focussed_document_id = document()->id();
  
  if(document()->selectable())
    do_set_current_document();
  
  gtk_im_context_focus_in(_im_context);
  
  Box *sel_box = document()->selection_box();
  if(sel_box && document()->selection_length() == 0) {
    auto ctx = document_context();
    ctx->old_selection.id = FrontEndReference::None;
    
    sel_box->request_repaint_range(ctx->selection.start, ctx->selection.end);
  }
  
  return false;
}

bool MathGtkWidget::on_focus_out(GdkEvent *e) {
  g_idle_add_full(G_PRIORITY_DEFAULT, 
    [](void *_arg) -> gboolean {
      pmath_debug_print("[idle after focus-out-event %p]\n", _arg);
      if(auto wid = dynamic_cast<MathGtkWidget*>(BasicGtkWidget::from_widget((GtkWidget*)_arg))) {
        if(focussed_document_id == wid->document()->id())
          focussed_document_id = FrontEndReference::None;
        
        wid->document()->focus_killed(FrontEndObject::find_cast<Document>(focussed_document_id));
      }
      return false; 
    },
    g_object_ref(widget()),
    [](void *_arg) { g_object_unref(_arg); });
  
  _focused = false;
  
  if(_im_context)
    gtk_im_context_focus_out(_im_context);
    
  return false;
}

static SpecialKey keyval_to_special_key(guint keyval) {
  switch(keyval) {
    case GDK_Left:            return SpecialKey::Left;
    case GDK_Right:           return SpecialKey::Right;
    case GDK_Up:              return SpecialKey::Up;
    case GDK_Down:            return SpecialKey::Down;
    case GDK_Home:            return SpecialKey::Home;
    case GDK_End:             return SpecialKey::End;
    case GDK_Page_Up:         return SpecialKey::PageUp;
    case GDK_Page_Down:       return SpecialKey::PageDown;
    case GDK_BackSpace:       return SpecialKey::Backspace;
    case GDK_Delete:          return SpecialKey::Delete;
    case GDK_Linefeed:
    case GDK_Return:
    case GDK_KP_Enter:        return SpecialKey::Return;
    case GDK_ISO_Left_Tab: // shift+tab
    case GDK_Tab:             return SpecialKey::Tab;
    case GDK_Escape:          return SpecialKey::Escape;
    case GDK_F1:              return SpecialKey::F1;
    case GDK_F2:              return SpecialKey::F2;
    case GDK_F3:              return SpecialKey::F3;
    case GDK_F4:              return SpecialKey::F4;
    case GDK_F5:              return SpecialKey::F5;
    case GDK_F6:              return SpecialKey::F6;
    case GDK_F7:              return SpecialKey::F7;
    case GDK_F8:              return SpecialKey::F8;
    case GDK_F9:              return SpecialKey::F9;
    case GDK_F10:             return SpecialKey::F10;
    case GDK_F11:             return SpecialKey::F11;
    case GDK_F12:             return SpecialKey::F12;
    default:                  return SpecialKey::Unknown;
  }
}

bool MathGtkWidget::on_key_press(GdkEvent *e) {
  GdkEventKey *event = &e->key;
  
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
  
  SpecialKeyEvent ske;
  ske.key = keyval_to_special_key(event->keyval);
  ske.ctrl  = 0 != (event->state & GDK_CONTROL_MASK);
  ske.alt   = 0 != (event->state & GDK_MOD1_MASK);
  ske.shift = 0 != (event->state & GDK_SHIFT_MASK);
  if(ske.key != SpecialKey::Unknown) {
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
    
    Display *display = XOpenDisplay(nullptr);
    
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
  
  if(event->keyval == GDK_Menu || (event->keyval == GDK_F10 && (event->state & GDK_SHIFT_MASK))) {
    auto src = document()->selection_now();
    if(!src.box)
      src = VolatileSelection(document(), 0);
    
    if(auto menu = create_popup_menu(src)) {
      gtk_menu_popup(menu, nullptr, nullptr, nullptr, nullptr, 0, event->time);
      g_object_unref(menu);
    }
  }
  
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
  
  if(ignore_key_release)
    return true;
    
  if(gtk_im_context_filter_keypress(_im_context, event))
    return true;
    
  SpecialKeyEvent ske;
  ske.key = keyval_to_special_key(event->keyval);
  if(ske.key != SpecialKey::Unknown) {
    ske.ctrl  = 0 != (event->state & GDK_CONTROL_MASK);
    ske.alt   = 0 != (event->state & GDK_MOD1_MASK);
    ske.shift = 0 != (event->state & GDK_SHIFT_MASK);
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
  me.device = get_pointer_device_for_event(e);
  pmath_debug_print("[%s down]", me.device == DeviceKind::Pen ? "pen" : (me.device == DeviceKind::Touch ? "touch" : "mouse"));
  
  me.left     = event->button == 1;
  me.middle   = event->button == 2;
  me.right    = event->button == 3;
  me.position = scroll_pos() + Vector2F(event->x, event->y) / scale_factor();
  
  document()->mouse_down(me);
  
  if(document()->selectable()) {
    gtk_widget_grab_focus(_widget);
  }

  if(me.right) {
    bool dummy;
    auto src = document()->mouse_selection(me.position, &dummy);
    if(!src.box)
      src = VolatileSelection(document(), 0);
    
    if(auto menu = create_popup_menu(src)) {
      gtk_menu_popup(menu, nullptr, nullptr, nullptr, nullptr, event->button, event->time);
      g_object_unref(menu);
    }
  }
  
  return true;
}

bool MathGtkWidget::on_button_release(GdkEvent *e) {
  GdkEventButton *event = (GdkEventButton *)e;
  
  _mouse_down_button = 0;
  
  MouseEvent me;
  me.device = get_pointer_device_for_event(e);
  
  me.left     = event->button == 1;
  me.middle   = event->button == 2;
  me.right    = event->button == 3;
  me.position = scroll_pos() + Vector2F(event->x, event->y) / scale_factor();
  
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
  me.device = get_pointer_device_for_event(e);
  
  me.left     = 0 != (event->state & GDK_BUTTON1_MASK);
  me.middle   = 0 != (event->state & GDK_BUTTON2_MASK);
  me.right    = 0 != (event->state & GDK_BUTTON3_MASK);
  me.position = scroll_pos() + Vector2F(event->x, event->y) / scale_factor();
  
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
  
  GdkScrollDirection dir = event->direction;
  
  if(event->state & GDK_CONTROL_MASK) {
    switch(dir) {
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
  
  if(event->state & GDK_SHIFT_MASK) {
    switch(dir) {
      case GDK_SCROLL_UP:   dir = GDK_SCROLL_LEFT;  break;
      case GDK_SCROLL_DOWN: dir = GDK_SCROLL_RIGHT; break;
      default: break;
    }
  }
  
  Vector2F delta(0, 0);
  switch(dir) {
    case GDK_SCROLL_UP:    delta.y = - 60; break;
    case GDK_SCROLL_DOWN:  delta.y = + 60; break;
    case GDK_SCROLL_LEFT:  delta.x = - 60; break;
    case GDK_SCROLL_RIGHT: delta.x = + 60; break;
    
#if GTK_MAJOR_VERSION >= 3
    case GDK_SCROLL_SMOOTH:
      {
        double ddx, ddy;
        gdk_event_get_scroll_deltas(e, &ddx, &ddy);
        delta.x = 60 * ddx;
        delta.y = 60 * ddy;
      }
      break;
#endif
  }
  
  scroll_by(delta);
  
  return true;
}

gboolean MathGtkWidget::blink_caret(gpointer id_as_ptr) {
  FrontEndReference id = FrontEndReference::unsafe_cast_from_pointer(id_as_ptr);
  
  if(auto doc = FrontEndObject::find_cast<Document>(id)) {
    if(auto wid = dynamic_cast<MathGtkWidget *>(doc->native())) {
      Context *ctx = wid->document_context();
      
      if(!gtk_widget_is_focus(wid->widget()))
        ctx->old_selection = ctx->selection;
      else if(ctx->old_selection == ctx->selection || wid->is_mouse_down())
        ctx->old_selection.id = FrontEndReference::None;
      else
        ctx->old_selection = ctx->selection;
        
      if(Box *box = ctx->selection.get())
        box->request_repaint_range(ctx->selection.start, ctx->selection.end);
        
      wid->is_blinking = false;
    }
  }
  
  return FALSE;
}

//} ... class MathGtkWidget





