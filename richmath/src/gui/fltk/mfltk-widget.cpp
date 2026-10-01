#include <gui/fltk/mfltk-widget.h>

#include <Fl/Fl_Cairo_Window.H>
#include <Fl/fl_ask.H>

#include <gui/document.h>
#include <math.h>


namespace richmath {
  class MathFltkWidget::Impl: public Fl_Cairo_Window {
      using base = Fl_Cairo_Window;
    public:
      Impl();
      
    public:
      static void static_cairo_draw_cb(Fl_Cairo_Window *window, cairo_t *cr);
    
    public:
      static Fl_Timestamp last_event_timestamp;
      static double last_event_time;
      
      static void update_event_time();
  };
}

using namespace richmath;

//{ class MathFltkWidget ...

MathFltkWidget::MathFltkWidget(Document *doc)
: NativeWidget(doc),
  BasicFltkWidget(),
  _autohide_vertical_scrollbar(false),
  cursor(CursorType::Default),
  last_width(0)
{
  SET_BASE_DEBUG_TAG(typeid(*this).name());
}

MathFltkWidget::~MathFltkWidget() {
}

void MathFltkWidget::after_construction() {
  RICHMATH_ASSERT(_widget == nullptr);
  
  Impl *wid = new Impl();
//  Fl_Cairo_Window *wid = new Fl_Cairo_Window(10, 10, nullptr);
//  wid->set_draw_cb(Impl::static_cairo_draw_cb);
  _widget = wid;
  
  BasicFltkWidget::after_construction();
}

Vector2F MathFltkWidget::window_size() {
  if(!_widget) 
    return Vector2F(0, 0);
  
  return Vector2F(_widget->w(), _widget->h()) / scale_factor();
}

Vector2F MathFltkWidget::monitor_size() {
  if(_widget) {
    if(Fl_Window *win = _widget->window()) {
      int screen_num = win->visible_r() ? win->screen_num() : 0;
      int wx = 0, wy = 0, ww = 0, wh = 0;
      Fl::screen_work_area(wx, wy, ww, wh, screen_num);
      return Vector2F(ww, wh) / scale_factor();
    }
  }
  
  return {500.0f, 500.0f};
}

Point MathFltkWidget::scroll_pos() {
  // TODO Fl_Scroll
  return Point(0, 0);
}

bool MathFltkWidget::scroll_to(Point pos) {
  // TODO Fl_Scroll
  return false;
}

void MathFltkWidget::show_tooltip(Box *source, Expr boxes) {
  // TODO
}

void MathFltkWidget::hide_tooltip() {
  // TODO
}

double MathFltkWidget::message_time() {
  // Note: on X11, FLTK has fl_event_time, on Windows use GetMessageTime, on Wayland wld_event_time
  // Those are only updated for mouse events on some platoforms
  return Impl::last_event_time;
}

double MathFltkWidget::double_click_time() {
  // on Windows often 500 ms
  return 0.5;
}

Vector2F MathFltkWidget::double_click_dist() {
  // on Windows often 4px 
  return Vector2F(4, 4) / scale_factor();
}

void MathFltkWidget::paint_background(Canvas &canvas) {
//  cairo_set_operator(canvas.cairo(), CAIRO_OPERATOR_CLEAR);
//  canvas.set_color(Color::Black, 0.0);
//  canvas.paint();
//  cairo_set_operator(canvas.cairo(), CAIRO_OPERATOR_OVER);
}

Document *MathFltkWidget::try_create_popup_window(const SelectionReference &anchor) {
  return nullptr;
}

void MathFltkWidget::do_drag_drop(const VolatileSelection &src, MouseEvent &event) {
  // TODO: Put content to clipboard and then call Fl::dnd()
}

void MathFltkWidget::bring_to_front() {
  if(_widget)
    _widget->take_focus();
}

void MathFltkWidget::invalidate() {
  if(_widget)
    _widget->damage(FL_DAMAGE_ALL);
}

void MathFltkWidget::invalidate_options() {
  float scale = document()->get_style(Magnification, custom_scale_factor());
  set_custom_scale(scale);
}

void MathFltkWidget::invalidate_rect(const RectangleF &rect) {
  if(_widget) {
    float scale = scale_factor();
    
    int x1 = (int)floorf(rect.left() * scale);
    int x2 = (int)ceilf(rect.right() * scale);
    int y1 = (int)floorf(rect.top() * scale);
    int y2 = (int)ceilf(rect.bottom() * scale);
    x1-= 4; x2+= 4; y1-= 4; y2 += 4;
    _widget->damage(FL_DAMAGE_ALL, x1, y1, x2 - x1, y2 - y1);
  }
}

void MathFltkWidget::force_redraw() {
  // TODO: not used?
}

void MathFltkWidget::set_cursor(CursorType type) {
  if(!_widget)
    return;
  
  Fl_Window *win = _widget->window();
  if(!win)
    return;
  
  if(cursor == type)
    return;
  
  cursor = type;
  
  switch(type) {
    case CursorType::Current: break;
    case CursorType::Finger: win->cursor(FL_CURSOR_HAND); return;
    case CursorType::Default: win->cursor(FL_CURSOR_DEFAULT); return;
    
    case CursorType::SizeN:
    case CursorType::SizeS: win->cursor(FL_CURSOR_NS); return;
      
    case CursorType::SizeNW:
    case CursorType::SizeSE: win->cursor(FL_CURSOR_NWSE); return;
      
    case CursorType::SizeE:
    case CursorType::SizeW: win->cursor(FL_CURSOR_WE); return;
      
    case CursorType::SizeNE:
    case CursorType::SizeSW: win->cursor(FL_CURSOR_NESW); return;
    
    case CursorType::TextSE:
    case CursorType::TextE:
    case CursorType::TextNE:
    case CursorType::TextN:
    case CursorType::TextNW:
    case CursorType::TextW:
    case CursorType::TextSW:
    case CursorType::TextS:
      win->cursor(FL_CURSOR_INSERT); // TODO: use custom rotated insert cursors
      return;
      
    case CursorType::Section:
      win->cursor(FL_CURSOR_ARROW); // TODO: use custom "|<-" cursor
      return;
      
    case CursorType::Document:
      win->cursor(FL_CURSOR_CROSS); // TODO: use custom "|----|" cursor
      return;
      
    case CursorType::NoSelect:
      win->cursor(FL_CURSOR_DEFAULT); // TODO: use custom "No" cursor
      return;
      
    case CursorType::Grab:
    case CursorType::Grabbing:
      win->cursor(FL_CURSOR_MOVE); // TODO: use custom full hand cursors
      return;
  }
}

bool MathFltkWidget::is_mouse_down() {
  return 0 != Fl::event_buttons(); // Fl::pushed() == _widget
}
      
void MathFltkWidget::beep() {
  fl_beep();
}

bool MathFltkWidget::register_timed_event(SharedPtr<TimedEvent> event) {
  // TODO
  return false;
}

void MathFltkWidget::paint_canvas(Canvas &canvas, bool resize_only) {
  canvas.line_width(1);
  cairo_set_line_cap(canvas.cairo(), CAIRO_LINE_CAP_SQUARE);
  canvas.set_font_size(_widget->labelsize()); // * 0.75 to convert from pixels?
  
  if(!resize_only) {
    Color color = document()->get_style(Background);
    if(color.is_valid()) {
      canvas.set_color(color);
      canvas.paint();
    }
    else
      paint_background(canvas);
  }
  
  canvas.scale(scale_factor(), scale_factor());
  canvas.set_color(document()->get_style(FontColor, Color::Black));
  
  if(last_width != _widget->w()) {
    last_width = _widget->w();
    document()->invalidate_all();
    _widget->clear_damage();
  }
  
  document()->paint_resize(canvas, resize_only);
}

//} ... class MathFltkWidget

//{ class MathFltkWidget::Impl ...

Fl_Timestamp MathFltkWidget::Impl::last_event_timestamp {};
double MathFltkWidget::Impl::last_event_time = 0.0;

MathFltkWidget::Impl::Impl()
: base(0,0, nullptr)
{
  set_draw_cb(Impl::static_cairo_draw_cb);
}

void MathFltkWidget::Impl::static_cairo_draw_cb(Fl_Cairo_Window *window, cairo_t *cr) {
  MathFltkWidget *owner = dynamic_cast<MathFltkWidget*>(BasicFltkWidget::from_widget(window));
  if(!owner)
    return;
  
  {
    Canvas canvas(cr);
    
    owner->paint_canvas(canvas, false);
  }
  
  cairo_surface_flush(cairo_get_target(cr));
}

void MathFltkWidget::Impl::update_event_time() {
  static bool has_last_event_timestamp = false;
  
  Fl_Timestamp now = Fl::now();
  
  if(!has_last_event_timestamp) {
    last_event_timestamp = now;
    has_last_event_timestamp = true;
  }
  
  last_event_time += Fl::seconds_between(now, last_event_timestamp);
  last_event_timestamp = now;
}

//} ... class MathFltkWidget::Impl
