#include <gui/fltk/mfltk-widget.h>

#include <Fl/fl_ask.H>
#include <Fl/fl_draw.H>
#include <Fl/Fl_Cairo_Window.H>
#include <Fl/Fl_Scrollbar.H>

#ifdef PMATH_DEBUG_LOG
#  include <Fl/names.H>
#endif

#include <gui/document.h>
#include <math.h>


namespace richmath {
  class MathFltkWidget::Impl: public Fl_Cairo_Window {
      using base = Fl_Cairo_Window;
    public:
      Impl();
      
      virtual int handle(int event) override;
    
      MathFltkWidget *find_owner() { return dynamic_cast<MathFltkWidget*>(BasicFltkWidget::from_widget(this)); }
    
    protected:
      int handle_keydown();
      int handle_keyup();
      
      static SpecialKey as_special_key(int key);
      
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
  int sx = 0;
  int sy = 0; 
  
  if(Fl_Scrollbar *v_scrollbar = vscroll()) {
    sy = v_scrollbar->value();
  }
  if(Fl_Scrollbar *h_scrollbar = hscroll()) {
    sx = h_scrollbar->value();
  }
  
  return Point(Vector2F(sx, sy) / scale_factor());
}

bool MathFltkWidget::scroll_to(Point pos) {
  float scale = scale_factor();
  
  bool done = false;
  
  if(Fl_Scrollbar *v_scrollbar = vscroll()) {
    int sb_pos = round(pos.y * scale);
    if(sb_pos < 0) sb_pos = 0;
    v_scrollbar->value(sb_pos);
    done = true;
  }
  if(Fl_Scrollbar *h_scrollbar = hscroll()) {
    int sb_pos = round(pos.x * scale);
    if(sb_pos < 0) sb_pos = 0;
    h_scrollbar->value(sb_pos);
    done = true;
  }
  
  return done;
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
  
  if(is_focused_widget()) {
    RectangleF last_cursor_rect {
      document_context()->last_cursor_pos[0], 
      document_context()->last_cursor_pos[1] }; 
    
    last_cursor_rect.normalize();
    
    Point offset = scroll_pos();
    float scale  = scale_factor();
    
    fl_set_spot(
      _widget->labelfont(), 
      _widget->labelsize(), 
      (int)((last_cursor_rect.x - offset.x) * scale),
      (int)((last_cursor_rect.y - offset.y) * scale),
      (int)ceilf(last_cursor_rect.width  * scale),
      (int)ceilf(last_cursor_rect.height * scale),
      static_cast<Impl*>(_widget));
  }
}

bool MathFltkWidget::is_focused_widget() {
  return Fl::focus() == _widget;
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

int MathFltkWidget::Impl::handle(int event) {
  update_event_time();
  
  switch(event) {
//    case FL_MOUSEWHEEL:
//      pmath_debug_print("[handle FL_MOUSEWHEEL (%s) dx %d (%f) dy %d (%f)]\n",
//        Fl::event_ctrl() ? "ctrl" : "",
//        Fl::event_dx(), Fl::event_dx_f(), 
//        Fl::event_dy(), Fl::event_dy_f());
//      break;
    
    case FL_FOCUS:
      return 1;
    
    case FL_UNFOCUS:
      Fl::compose_reset();
      fl_reset_spot();
      return 1;
    
    case FL_KEYDOWN:
      return handle_keydown();
    
    case FL_KEYUP:
      return handle_keyup();
    
//    default:
//      pmath_debug_print("[handle %s]\n", fl_eventname_str(event).c_str());
//      break;
  }
  return base::handle(event);
}

int MathFltkWidget::Impl::handle_keydown() {
  MathFltkWidget *owner = find_owner();
  if(!owner)
    return 0;
  
  SpecialKeyEvent event {};
  event.key = as_special_key(Fl::event_key());
  
  owner->document()->ignores_next_key_press(false);
  if(event.key != SpecialKey::Unknown) {
    event.ctrl  = Fl::event_ctrl();
    event.alt   = Fl::event_alt();
    event.shift = Fl::event_shift();
    owner->document()->key_down(event);
  }
  
  pmath_debug_print("[handle_keydown key %d %d %s", Fl::event_key(), Fl::event_length(), Fl::event_text());
  int del = 0;
  if(Fl::compose(del)) {
    pmath_debug_print(" compose => del %d ins %d %s", del, Fl::event_length(), Fl::event_text());
    
    // TODO: replace `del` UTF8 bytes to the left.
    // TODO: mark
    Fl::compose_reset(); // TODO: only after moving cursor position via mouse/arrow keys; respect 'del'
    
    String ins = String::FromUtf8(Fl::event_text(), Fl::event_length());
    while(ins.length() > 0) {
      uint32_t ch = ins[0];
      ins = ins.part(1);
      if(is_utf16_high(ch) && ins.length() > 0) {
        uint16_t lo = ins[0];
        if(is_utf16_low(lo)) {
          ins = ins.part(1);
          ch = 0x10000 + (((ch & 0x03FF) << 10) | (lo & 0x03FF));
        }
      }
      owner->document()->key_press(ch);
    }
    
    //return 1;
  }
  else {
    pmath_debug_print(" no ins");
    
    switch(event.key) {
      case SpecialKey::Return: owner->document()->key_press('\r'); break;
      case SpecialKey::Tab:    owner->document()->key_press('\t'); break;
    }
  }
  
  pmath_debug_print("]\n");
  
  return 1;
}

int MathFltkWidget::Impl::handle_keyup() {
  MathFltkWidget *owner = find_owner();
  if(!owner)
    return 0;
  
  SpecialKeyEvent event {};
  event.key = as_special_key(Fl::event_key());
  if(event.key != SpecialKey::Unknown) {
    event.ctrl  = Fl::event_ctrl();
    event.alt   = Fl::event_alt();
    event.shift = Fl::event_shift();
    owner->document()->key_up(event);
  }
  
  return 1;
}

SpecialKey MathFltkWidget::Impl::as_special_key(int key) {
  switch(key) {
    case FL_Left:      return SpecialKey::Left;
    case FL_Right:     return SpecialKey::Right;
    case FL_Up:        return SpecialKey::Up;
    case FL_Down:      return SpecialKey::Down;
    case FL_Home:      return SpecialKey::Home;
    case FL_End:       return SpecialKey::End;
    case FL_Page_Up:   return SpecialKey::PageUp;
    case FL_Page_Down: return SpecialKey::PageDown;
    case FL_BackSpace: return SpecialKey::Backspace;
    case FL_Delete:    return SpecialKey::Delete;
    case FL_Enter:     return SpecialKey::Return;
    case FL_Tab:       return SpecialKey::Tab;
    case FL_Escape:    return SpecialKey::Escape;
    case FL_F + 1:     return SpecialKey::F1;
    case FL_F + 2:     return SpecialKey::F2;
    case FL_F + 3:     return SpecialKey::F3;
    case FL_F + 4:     return SpecialKey::F4;
    case FL_F + 5:     return SpecialKey::F5;
    case FL_F + 6:     return SpecialKey::F6;
    case FL_F + 7:     return SpecialKey::F7;
    case FL_F + 8:     return SpecialKey::F8;
    case FL_F + 9:     return SpecialKey::F9;
    case FL_F + 10:    return SpecialKey::F10;
    case FL_F + 11:    return SpecialKey::F11;
    case FL_F + 12:    return SpecialKey::F12;
  }
  return SpecialKey::Unknown;
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
