#include <gui/gtk/mgtk-widget.h>

#include <glib.h>


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

static gboolean blink_caret(gpointer data){
//  KillTimer(_hwnd, TID_BLINKCURSOR);
//            
//  Context *ctx = document_context();
//  if(ctx->old_selection == ctx->selection 
//  || _hwnd != GetFocus()
//  || is_mouse_down())
//    ctx->old_selection.id = 0;
//  else
//    ctx->old_selection = ctx->selection;
//  
//  Box *box = ctx->selection.get();
//  if(box)
//    box->request_repaint_all();
  return FALSE;
}

//{ class MGtkWidget ...

MathGtkWidget::MathGtkWidget(Document *doc)
: NativeWidget(doc),
  BasicGtkWidget(),
  _autohide_vertical_scrollbar(false),
  is_painting(false)
{
}

MathGtkWidget::~MathGtkWidget(){
}

void MathGtkWidget::after_construction(){
  BasicGtkWidget::after_construction();
  
  gtk_widget_set_events(_widget, GDK_ALL_EVENTS_MASK);
}

void MathGtkWidget::window_size(float *w, float *h){
  if(!_widget)
    return;
  
  GtkAllocation rect;
  
  gtk_widget_get_allocation(_widget, &rect);
  
  *w = rect.width  / scale_factor();
  *h = rect.height / scale_factor();
}

void MathGtkWidget::scroll_pos(float *x, float *y){
  if(!_widget)
    return;
  
  // todo: implement gtk scrolling
  *x = *y = 0;
}

void MathGtkWidget::scroll_to(float x, float y){
  if(!_widget)
    return;
  
  // todo: implement gtk scrolling
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
  gint t;
  
  g_object_get(
    gtk_settings_get_default(),
    "gtk-double-click-time", &t,
    NULL);
  
  return t / 1000.0;
}

void MathGtkWidget::double_click_dist(float *dx, float *dy){
  gint d;
  
  g_object_get(
    gtk_settings_get_default(),
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
  
  return true;
}

void MathGtkWidget::invalidate(){
  if(!_widget)
    return;
  
  is_painting = false; // if inside "expose" event, invalidate at end of event
  
  GtkAllocation rect;
  gtk_widget_get_allocation(_widget, &rect);
  gtk_widget_queue_draw_area(_widget, 0, 0, rect.width, rect.height);
}

void MathGtkWidget::force_redraw(){
  invalidate();
}

void MathGtkWidget::set_cursor(CursorType type){
  if(!_widget)
    return;
  
  GdkWindow *w = gtk_widget_get_window(_widget);
  if(!w)
    return;
  
  if(mouse_moving){
    cursor = type;
    return;
  }
  
  switch(type){
    case FingerCursor: 
      gdk_window_set_cursor(w, gdk_cursor_new(GDK_HAND2));
      return;
    
    case DefaultCursor: 
      gdk_window_set_cursor(w, gdk_cursor_new(GDK_LEFT_PTR));
      return;
    
    case CurrentCursor:
      break;
    
//    TextSECursor   = 101,
//    TextECursor    = 102,
//    TextNECursor   = 103,
//    TextNCursor    = 104,
//    TextNWCursor   = 105,
//    TextWCursor    = 106,
//    TextSWCursor   = 107,
//    TextSCursor    = 108,
//    SectionCursor  = 109,
//    DocumentCursor = 110,
//    NoSelectCursor = 111
    default:
      gdk_window_set_cursor(w, gdk_cursor_new(GDK_XTERM));
//    default:
//      SetCursor(LoadCursor(GetModuleHandle(0), MAKEINTRESOURCE((int)type)));
  }
}

void MathGtkWidget::running_state_changed(){
}

bool MathGtkWidget::is_mouse_down(){
  if(!_widget)
    return false;
  
  GdkWindow *w = gtk_widget_get_window(_widget);
  if(!w)
    return false;
    
  GdkModifierType mod;
  
  gdk_window_get_pointer(w, NULL, NULL, &mod);
  return 0 != (mod & (GDK_BUTTON1_MASK | GDK_BUTTON2_MASK | GDK_BUTTON3_MASK | GDK_BUTTON4_MASK | GDK_BUTTON5_MASK));
}

void MathGtkWidget::beep(){
  gdk_beep();
}

bool MathGtkWidget::register_timed_event(SharedPtr<TimedEvent> event){
  animations.set(event, Void());
  if(!animation_running){
    animation_running = 0 < g_timeout_add(ANIMATION_DELAY, animation_timeout, NULL);
    
    if(!animation_running){
      animations.remove(event);
      return false;
    }
  }
  
  return true;
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
  
  if(_widget 
  && gtk_widget_has_focus(_widget)
  && document()->selection_box()
  && document()->selection_length() == 0){
    gboolean is_blining;
    gint     blink_time;
    
    g_object_get(
      gtk_settings_get_default(),
      "gtk-cursor-blink",      &is_blining,
      "gtk-cursor-blink-time", &blink_time,
      NULL);
    
    if(is_blining)
      g_timeout_add(blink_time, blink_caret, NULL);
  }
}

bool MathGtkWidget::on_paint(GdkEventExpose *event, bool from_normal_event){
  cairo_t *cr = gdk_cairo_create(event->window);
  {
    Canvas canvas(cr);
    
    if(!from_normal_event){
      canvas.clip();
    }
    else{
      canvas.move_to(event->area.x,                     event->area.y);
      canvas.line_to(event->area.x + event->area.width, event->area.y);
      canvas.line_to(event->area.x + event->area.width, event->area.y + event->area.height);
      canvas.line_to(event->area.x,                     event->area.y + event->area.height);
      canvas.close_path();
      canvas.clip();
    }
    
    paint_canvas(&canvas, !from_normal_event);
  }
  cairo_destroy(cr);
  
  return false;
}

bool MathGtkWidget::callback(GdkEvent *event){
  switch(event->type){
    case GDK_EXPOSE: {
      is_painting = true;
      
      bool result = on_paint(&event->expose, true);
      
      if(!is_painting)
        invalidate();
      is_painting = false;
      return result;
    }
    
    default: break;
  }
  
  return BasicGtkWidget::callback(event);
}

//} ... class MGtkWidget
