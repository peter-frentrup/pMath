#include <gui/gtk/mgtk-widget.h>

#include <eval/binding.h>

#include <glib.h>
#include <gdk/gdkkeysyms.h>
#include <math.h>


using namespace richmath;

#define ANIMATION_DELAY  (50)
static bool animation_running = false;
Hashtable<SharedPtr<TimedEvent>,Void> animations;

static gboolean animation_timeout(gpointer data){
  animation_running = false;
            
  unsigned int count, i;
  for(count = 0,i = 0;count < animations.size();++i){
    Entry<SharedPtr<TimedEvent>,Void> *e = animations.entry(i);
    
    if(e){
      ++count;
      
      SharedPtr<TimedEvent> te = e->key;
      if(te->min_wait_seconds <= te->timer()){
        animations.remove(te);
        
        te->execute_event();
      }
      else
        animation_running = true;
    }
  }
  
  return animation_running; // continue ? 
}

//{ class MGtkWidget ...

MathGtkWidget::MathGtkWidget(Document *doc)
: NativeWidget(doc),
  BasicGtkWidget(),
  _autohide_vertical_scrollbar(false),
  is_painting(false),
  is_blinking(false),
  old_width(0),
  _hadjustment(0),
  _vadjustment(0),
  _im_context(gtk_im_multicontext_new())
{
  g_signal_connect(_im_context, "commit",          G_CALLBACK(&MathGtkWidget::im_commit_callback),          this);
  g_signal_connect(_im_context, "preedit_changed", G_CALLBACK(&MathGtkWidget::im_preedit_changed_callback), this);
  
  gtk_im_context_set_use_preedit(_im_context, FALSE);
}

MathGtkWidget::~MathGtkWidget(){
  hadjustment(0);
  vadjustment(0);
  
  g_signal_handlers_disconnect_matched(
    _im_context, 
    G_SIGNAL_MATCH_DATA,
    0, 0, 0, 0,
    this);
  
  g_signal_handlers_disconnect_matched(
    _widget, 
    G_SIGNAL_MATCH_DATA,
    0, 0, 0, 0,
    this);
  
  g_object_unref(_im_context);
  _im_context = 0;
}

void MathGtkWidget::after_construction(){
  BasicGtkWidget::after_construction();
  
  gtk_widget_set_events(_widget, GDK_ALL_EVENTS_MASK);
  gtk_widget_set_can_focus(_widget, TRUE);
  
  signal_connect<MathGtkWidget, &MathGtkWidget::on_button_press>(  "button-press-event");
  signal_connect<MathGtkWidget, &MathGtkWidget::on_button_release>("button-release-event");
  signal_connect<MathGtkWidget, &MathGtkWidget::on_expose>(        "expose-event");
  signal_connect<MathGtkWidget, &MathGtkWidget::on_focus_in>(      "focus-in-event");
  signal_connect<MathGtkWidget, &MathGtkWidget::on_focus_out>(     "focus-out-event");
  signal_connect<MathGtkWidget, &MathGtkWidget::on_key_press>(     "key-press-event");
  signal_connect<MathGtkWidget, &MathGtkWidget::on_key_release>(   "key-release-event");
  signal_connect<MathGtkWidget, &MathGtkWidget::on_motion_notify>( "motion-notify-event");
  signal_connect<MathGtkWidget, &MathGtkWidget::on_leave_notify>(  "leave-notify-event");
  signal_connect<MathGtkWidget, &MathGtkWidget::on_scroll>(        "scroll-event");
  signal_connect<MathGtkWidget, &MathGtkWidget::on_map>(           "map-event");
  signal_connect<MathGtkWidget, &MathGtkWidget::on_unmap>(         "unmap-event");
}

void MathGtkWidget::window_size(float *w, float *h){
  if(!_widget){
    *w = *h = 0;
    return;
  }
  
  GtkAllocation rect;
  
  gtk_widget_get_allocation(_widget, &rect);
  
  *w = rect.width  / scale_factor();
  *h = rect.height / scale_factor();
}

void MathGtkWidget::scroll_pos(float *x, float *y){
  *x = *y = 0;
  if(!is_scrollable())
    return;
  
  if(_hadjustment){
    *x = gtk_adjustment_get_value(_hadjustment);
    *x/= scale_factor();
  }
  
  if(_vadjustment){
    *y = gtk_adjustment_get_value(_vadjustment);
    *y/= scale_factor();
  }
}

void MathGtkWidget::scroll_to(float x, float y){
  if(!is_scrollable())
    return;
  
  if(_hadjustment){
    double oldx = gtk_adjustment_get_value(_hadjustment);
    double newx = x * scale_factor();
    
    double lo = gtk_adjustment_get_lower(_hadjustment);
    double hi = gtk_adjustment_get_upper(_hadjustment);
    hi-=        gtk_adjustment_get_page_size(_hadjustment);
    
    newx = CLAMP(newx, lo, hi);
    if(oldx != newx)
      gtk_adjustment_set_value(_hadjustment, newx);
  }
  
  if(_vadjustment){
    double oldy = gtk_adjustment_get_value(_vadjustment);
    double newy = y * scale_factor();
    
    double lo = gtk_adjustment_get_lower(_vadjustment);
    double hi = gtk_adjustment_get_upper(_vadjustment);
    hi-=        gtk_adjustment_get_page_size(_vadjustment);
    
    newy = CLAMP(newy, lo, hi);
    if(oldy != newy)
      gtk_adjustment_set_value(_vadjustment, newy);
  }
}

void MathGtkWidget::show_tooltip(Expr boxes){
  if(!_widget)
    return;
  
  // todo: implement gtk tooltips
}

void MathGtkWidget::hide_tooltip(){
  if(!_widget)
    return;
  
  // todo: implement gtk tooltips
}

double MathGtkWidget::message_time(){
  guint32 timestamp = gtk_get_current_event_time();
  
  if(timestamp == GDK_CURRENT_TIME){
    GTimeVal tv;
    g_get_current_time(&tv);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
  }
  
  return timestamp / 1000000.0;
}

double MathGtkWidget::double_click_time(){
  GtkSettings *settings = gtk_widget_get_settings(_widget);
  gint t;
  
  g_object_get(
    settings,
    "gtk-double-click-time", &t,
    NULL);
  
  return t / 1000.0;
}

void MathGtkWidget::double_click_dist(float *dx, float *dy){
  GtkSettings *settings = gtk_widget_get_settings(_widget);
  gint d;
  
  g_object_get(
    settings,
    "gtk-double-click-distance", &d,
    NULL);
  
  *dx = *dy = d / scale_factor();
}

void MathGtkWidget::do_drag_drop(Box *src, int start, int end){
  if(!_widget)
    return;
  
  // todo: implement gtk drag&drop
}

bool MathGtkWidget::cursor_position(float *x, float *y){
  if(!_widget)
    return false;
  
  gint ix, iy;
  
  gtk_widget_get_pointer(_widget, &ix, &iy);
  
  *x = ix / scale_factor();
  *y = iy / scale_factor();
  
  float sx, sy;
  scroll_pos(&sx, &sy);
  *x+= sx;
  *y+= sy;
  
  return true;
}

void MathGtkWidget::invalidate(){
  is_painting = false; // if inside "expose" event, invalidate at end of event
  
  gtk_widget_queue_draw(_widget);
}

void MathGtkWidget::force_redraw(){
  invalidate();
}

void MathGtkWidget::set_cursor(CursorType type){
  cursor = type;
  
  if(mouse_moving)
    return;
  
  GdkWindow *win = gtk_widget_get_window(_widget);
  
  GdkCursor *cur = cursors.get_gdk_cursor(type);
  if(cur)
    gdk_window_set_cursor(win, cur);
}

void MathGtkWidget::running_state_changed(){
}

bool MathGtkWidget::is_mouse_down(){
  if(!_widget)
    return false;
  
  GdkWindow *w = gtk_widget_get_window(_widget);
  GdkModifierType mod = (GdkModifierType)0;
  
  gdk_window_get_pointer(w, NULL, NULL, &mod);
  return 0 != (mod & (GDK_BUTTON1_MASK | GDK_BUTTON2_MASK | GDK_BUTTON3_MASK | GDK_BUTTON4_MASK | GDK_BUTTON5_MASK));
}

void MathGtkWidget::beep(){
  gdk_beep();
}

bool MathGtkWidget::register_timed_event(SharedPtr<TimedEvent> event){
  animations.set(event, Void());
  if(!animation_running){
    animation_running = 0 < gdk_threads_add_timeout(ANIMATION_DELAY, animation_timeout, NULL);
    
    if(!animation_running){
      animations.remove(event);
      return false;
    }
  }
  
  return true;
}

  static void adjustment_value_changed(
    GtkAdjustment *adjustment,
    void          *user_data
  ){
    MathGtkWidget *self = (MathGtkWidget*)user_data;
    
    self->invalidate();
  }

void MathGtkWidget::hadjustment(GtkAdjustment *ha){
  if(_hadjustment){
    g_signal_handlers_disconnect_matched(
      _hadjustment, 
      G_SIGNAL_MATCH_DATA,
      0, 0, 0, 0,
      this);
      
    g_object_unref(_hadjustment);
  }
  
  _hadjustment = ha;
  if(_hadjustment){
    g_signal_connect(_hadjustment, "value-changed", G_CALLBACK(adjustment_value_changed), this);
  }
  
  invalidate();
}

void MathGtkWidget::vadjustment(GtkAdjustment *va){
  if(_vadjustment){
    g_signal_handlers_disconnect_matched(
      _vadjustment, 
      G_SIGNAL_MATCH_DATA,
      0, 0, 0, 0,
      this);
      
    g_object_unref(_vadjustment);
  }
  
  _vadjustment = va;
  if(_vadjustment){
    g_signal_connect(_vadjustment, "value-changed", G_CALLBACK(adjustment_value_changed), this);
  }
  
  
  invalidate();
}

void MathGtkWidget::update_im_cursor_location(){
  GdkRectangle area;
  
  float x1 = document_context()->last_cursor_x[0];
  float x2 = document_context()->last_cursor_x[1];
  float y1 = document_context()->last_cursor_y[0];
  float y2 = document_context()->last_cursor_y[1];
  
  if(x2 < x1){
    float t = x2;
    x2 = x1;
    x1 = t;
  }
  
  if(y2 < y1){
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

void MathGtkWidget::on_im_commit(const char *str){
  document()->insert_string(String::FromUtf8(str), false);
}

void MathGtkWidget::on_im_preedit_changed(){
}


void MathGtkWidget::paint_background(Canvas *canvas){
  canvas->set_color(0xffffff);
  canvas->paint();
}

void MathGtkWidget::paint_canvas(Canvas *canvas, bool resize_only){
  cairo_set_line_width(canvas->cairo(), 1);
  cairo_set_line_cap(canvas->cairo(), CAIRO_LINE_CAP_SQUARE);
  canvas->set_font_size(10);// 10 * 4/3.
  
  if(!resize_only){
    int color = document()->get_style(Background, -1);
    if(color >= 0){
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
  && document()->selection_length() == 0){
    GtkSettings *settings = gtk_widget_get_settings(_widget);
    gboolean may_blink;
    gint     blink_time;
    
    g_object_get(
      settings,
      "gtk-cursor-blink",      &may_blink,
      "gtk-cursor-blink-time", &blink_time,
      NULL);
    
    if(may_blink){
      is_blinking = true;
      gdk_threads_add_timeout(blink_time / 2, blink_caret, (void*)document()->id());
    }
  }
  
  
  if(is_scrollable()){
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
    
    if(_hadjustment){
      g_object_set(_hadjustment,
        "lower",     0.0,
        "page-size", w_page,
        "upper",     w_max,
        NULL);
    }
    
    if(_vadjustment){
      g_object_set(_vadjustment,
        "lower",     0.0,
        "page-size", h_page,
        "upper",     h_max,
        NULL);
    }
  }
}

bool MathGtkWidget::on_map(GdkEvent *e){
  gtk_im_context_set_client_window(_im_context, gtk_widget_get_window(_widget));
  return false;
}

bool MathGtkWidget::on_unmap(GdkEvent *e){
  gtk_im_context_reset(_im_context);
  gtk_im_context_set_client_window(_im_context, 0);
  return false;
}

bool MathGtkWidget::on_expose(GdkEvent *e){
  GdkEventExpose *event = &e->expose;
  
  GtkAllocation rect;
  gtk_widget_get_allocation(_widget, &rect);
  if(old_width != rect.width){
    old_width = rect.width;
    document()->invalidate_all();
  }
  
  is_painting = true;
  
  cairo_t *cr = gdk_cairo_create(event->window);
  {
    Canvas canvas(cr);
    
    canvas.move_to(event->area.x,                     event->area.y);
    canvas.line_to(event->area.x + event->area.width, event->area.y);
    canvas.line_to(event->area.x + event->area.width, event->area.y + event->area.height);
    canvas.line_to(event->area.x,                     event->area.y + event->area.height);
    canvas.close_path();
    canvas.clip();
    
    paint_canvas(&canvas, false);
    
    if(_im_context_pos != document_context()->selection){
      _im_context_pos = document_context()->selection;
      gtk_im_context_reset(_im_context);
    }
    else
      update_im_cursor_location();
  }
  cairo_destroy(cr);
  
  if(!is_painting)
    invalidate();
    
  is_painting = false;
  
  return true;
}

bool MathGtkWidget::on_focus_in(GdkEvent *e){
  Box *box = document()->selection_box();
  if(!box)
    box = document();
  
  if(box->selectable()){
    set_current_document(document());
  }
  
  gtk_im_context_focus_in(_im_context);
  
  if(document()->selection_length() == 0){
    invalidate();
  }
  
  return false;
}

bool MathGtkWidget::on_focus_out(GdkEvent *e){
  gtk_im_context_focus_out(_im_context);
  
  return false;
}

  static SpecialKey keyval_to_special_key(guint keyval){
    switch(keyval){
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
      case GDK_Return:          return KeyReturn;
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

bool MathGtkWidget::on_key_press(GdkEvent *e){
  GdkEventKey *event = &e->key;
  GdkModifierType mod = (GdkModifierType)0;
  
  if(gtk_im_context_filter_keypress(_im_context, event)){
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
  if(ske.key){
    if(ske.key != KeyReturn || ske.ctrl || ske.alt || ske.shift){
      document()->key_down(ske);
      //return true;
    }
  }
  
  if(event->keyval == GDK_Caps_Lock || event->keyval == GDK_Shift_Lock){
    if(mod & GDK_LOCK_MASK){
      gdk_event_put(e);
      while(gtk_events_pending())
        gtk_main_iteration();
    }
    else
      document()->key_press(PMATH_CHAR_ALIASDELIMITER);
    
    return true;
  }
  
//  if(ske.ctrl || ske.alt)
//    return false;
  
  uint32_t unichar = gdk_keyval_to_unicode(event->keyval);
  if(event->keyval == GDK_Return)
    unichar = '\n';
  
  if(unichar){
    if((unichar == ' ' || unichar == '\r' || unichar == '\n')
    && (ske.ctrl || ske.alt || ske.shift))
      return false;
    
    if(unichar == '\t')
      return false;
    
    document()->key_press(unichar);
  }
  
  return true;
}

bool MathGtkWidget::on_key_release(GdkEvent *e){
  GdkEventKey *event = &e->key;
  GdkModifierType mod = (GdkModifierType)0;
  
  if(gtk_im_context_filter_keypress(_im_context, event)){
    return true;
  }
  
  {
    GdkWindow *w = gtk_widget_get_window(_widget);
    
    gdk_window_get_pointer(w, NULL, NULL, &mod);
  }
  
  SpecialKeyEvent ske;
  ske.key = keyval_to_special_key(event->keyval);
  if(ske.key){
    ske.ctrl  = 0 != (mod & GDK_CONTROL_MASK);
    ske.alt   = 0 != (mod & GDK_MOD1_MASK);
    ske.shift = 0 != (mod & GDK_SHIFT_MASK);
    document()->key_up(ske);
  }
        
  return true;
}

bool MathGtkWidget::on_button_press(GdkEvent *e){
  GdkEventButton *event = (GdkEventButton*)e;
  
  if(event->type != GDK_BUTTON_PRESS)
    return true;
  
  MouseEvent me;
  me.left   = event->button == 1;
  me.middle = event->button == 2;
  me.right  = event->button == 3;
  
  me.x = event->x;
  me.y = event->y;
  
  me.x/= scale_factor();
  me.y/= scale_factor();
  
  float sx, sy;
  scroll_pos(&sx, &sy);
  me.x+= sx;
  me.y+= sy;
  
  document()->mouse_down(me);
  
  if(document()->selection_box()){
    gtk_widget_grab_focus(_widget);
  }
  else{
    Document *cur = get_current_document();
    if(cur && cur != document()){
      MathGtkWidget *w = dynamic_cast<MathGtkWidget*>(cur->native());
      
      if(w && !gtk_widget_has_focus(w->widget())){
        
        GtkWidget *wid = w->widget();
        GtkWidget *parent = gtk_widget_get_parent(wid);
        while(parent){
          wid = parent;
          parent = gtk_widget_get_parent(wid);
        }
        gtk_widget_grab_focus(wid);
        
        gtk_widget_grab_focus(w->widget());
      }
    }
  }
  
  return true;
}

bool MathGtkWidget::on_button_release(GdkEvent *e){
  GdkEventButton *event = (GdkEventButton*)e;
  
  MouseEvent me;
  me.left   = event->button == 1;
  me.middle = event->button == 2;
  me.right  = event->button == 3;
  
  me.x = event->x;
  me.y = event->y;
  
  me.x/= scale_factor();
  me.y/= scale_factor();
  
  float sx, sy;
  scroll_pos(&sx, &sy);
  me.x+= sx;
  me.y+= sy;
  
  document()->mouse_up(me);
  
  return true;
}

bool MathGtkWidget::on_motion_notify(GdkEvent *e){
  GdkEventMotion *event = (GdkEventMotion*)e;
  
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
  
  me.x/= scale_factor();
  me.y/= scale_factor();
  
  float sx, sy;
  scroll_pos(&sx, &sy);
  me.x+= sx;
  me.y+= sy;
  
  mouse_moving = true;
  cursor = DefaultCursor;
  
  document()->mouse_move(me);
  
  mouse_moving = false;
  set_cursor(cursor);
  return true;
}

bool MathGtkWidget::on_leave_notify(GdkEvent *e){
  //GdkEventCrossing *event = (GdkEventCrossing*)e;
  
  document()->mouse_exit();
  return false;
}

bool MathGtkWidget::on_scroll(GdkEvent *e){
  GdkEventScroll *event = (GdkEventScroll*)e;
  
  if(event->state & GDK_CONTROL_MASK){
    if(event->direction == GDK_SCROLL_UP)
      scale_by(pow(2, 0.5));
    else if(event->direction == GDK_SCROLL_DOWN)
      scale_by(pow(2, -0.5));
    return true;
  }
  
  float dx = 0;
  float dy = 0;
  switch(event->direction){
    case GDK_SCROLL_UP:    dy = - 60; break;
    case GDK_SCROLL_DOWN:  dy = + 60; break;
    case GDK_SCROLL_LEFT:  dx = - 60; break;
    case GDK_SCROLL_RIGHT: dx = + 60; break;
  }
  
  scroll_by(dx, dy);
  
  return true;
}

gboolean MathGtkWidget::blink_caret(gpointer id_as_ptr){
  int id = (int)id_as_ptr;
  
  Document *doc = dynamic_cast<Document*>(Box::find(id));
  if(doc){
    MathGtkWidget *wid = dynamic_cast<MathGtkWidget*>(doc->native());
    
    if(wid){
      Context *ctx = wid->document_context();
      
      if(ctx->old_selection == ctx->selection 
      || !gtk_widget_is_focus(wid->widget())
      || wid->is_mouse_down()){
        ctx->old_selection.id = 0;
      }
      else
        ctx->old_selection = ctx->selection;
      
      Box *box = ctx->selection.get();
      if(box)
        box->request_repaint_all();
        
  
      wid->is_blinking = false;
    }
  }
  
  return FALSE;
}

//} ... class MGtkWidget
