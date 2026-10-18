#include <gui/fltk/mfltk-widget.h>

#include <Fl/fl_ask.H>
#include <Fl/fl_draw.H>
#include <Fl/Fl_Cairo_Window.H>
#include <Fl/Fl_Menu_Item.H>
#include <Fl/Fl_Scrollbar.H>

#ifdef PMATH_DEBUG_LOG
#  include <Fl/names.H>
#endif

#include <gui/document.h>
#include <gui/documents.h>
#include <gui/fltk/mfltk-menu-items.h>
#include <math.h>


namespace richmath {
  class MathFltkWidget::Impl: public Fl_Cairo_Window {
      using base = Fl_Cairo_Window;
    public:
      Impl();
      
      virtual int handle(int event) override;
    
      MathFltkWidget *find_owner() { return dynamic_cast<MathFltkWidget*>(BasicFltkWidget::from_widget(this)); }
    
    protected:
      int handle_focus();
      int handle_unfocus();
      
      int handle_keydown();
      int handle_keyup();
      
      int handle_mousewheel();
      int handle_mouse_down();
      int handle_mouse_move();
      int handle_mouse_up();
      int handle_mouse_leave();
      
      MouseEvent prepare_mouse_event();
      static SpecialKey as_special_key(int key);
      
    public:
      static void static_cairo_draw_cb(Fl_Cairo_Window *window, cairo_t *cr);
      
      static void static_animation_timeout(void *_id);
      
    public:
      static Fl_Timestamp last_event_timestamp;
      static double last_event_time;
      static bool currently_drawing;
      static bool need_redraw;
      
      static void update_event_time();
  };
}

using namespace richmath;

namespace richmath { namespace strings {
  extern String Popup;
}}

extern pmath_symbol_t richmath_System_List;
extern pmath_symbol_t richmath_System_Menu;

//{ class MathFltkWidget ...

MathFltkWidget::MathFltkWidget(Document *doc)
: NativeWidget(doc),
  BasicFltkWidget(),
  _autohide_vertical_scrollbar(false),
  _did_scroll(false),
  _mouse_moving(false),
  _animation_running(false),
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
  
  bool did_scroll = false;
  
  if(Fl_Scrollbar *v_scrollbar = vscroll()) {
    int sb_pos = round(pos.y * scale);
    if(sb_pos < 0) sb_pos = 0;
    if(sb_pos > v_scrollbar->maximum())  sb_pos = (int)v_scrollbar->maximum();
    did_scroll |= !!v_scrollbar->value(sb_pos);
//    pmath_debug_print("[scroll_to v sb_pos= %d did_scroll:%d]\n", sb_pos, did_scroll);
  }
  if(Fl_Scrollbar *h_scrollbar = hscroll()) {
    int sb_pos = round(pos.x * scale);
    if(sb_pos < 0) sb_pos = 0;
    if(sb_pos > h_scrollbar->maximum())  sb_pos = (int)h_scrollbar->maximum();
    did_scroll |= !!h_scrollbar->value(sb_pos);
  }
  
  if(did_scroll) {
    _did_scroll = true;
    document()->invalidate();
  }
  return did_scroll;
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
  if(_widget) {
    uchar r, g, b;
    Fl::get_color(_widget->color(), r, g, b);
    canvas.set_color(Color::from_rgb(r / 255.0, g / 255.0, b/ 255.0));
    canvas.paint();
    
    Fl::get_color(_widget->labelcolor(), r, g, b);
    canvas.set_color(Color::from_rgb(r / 255.0, g / 255.0, b/ 255.0));
  }
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
  if(_widget) {
    _widget->take_focus();
    if(Fl_Window *win = _widget->top_window())
      win->show();
  }
}

void MathFltkWidget::invalidate() {
  Impl::need_redraw = true;
  if(_widget)
    _widget->damage(FL_DAMAGE_ALL);
}

void MathFltkWidget::invalidate_options() {
  float scale = document()->get_style(Magnification, custom_scale_factor());
  set_custom_scale(scale);
}

void MathFltkWidget::invalidate_rect(const RectangleF &rect) {
  Impl::need_redraw = true;
  if(_widget) {
    float scale = scale_factor();
    
    int x1 = (int)floorf(rect.left() * scale);
    int x2 = (int)ceilf(rect.right() * scale);
    int y1 = (int)floorf(rect.top() * scale);
    int y2 = (int)ceilf(rect.bottom() * scale);
    x1-= 4; x2+= 4; y1-= 4; y2 += 4;
    
    int sx = 0;
    int sy = 0; 
    
    if(Fl_Scrollbar *v_scrollbar = vscroll()) {
      sy = v_scrollbar->value();
    }
    if(Fl_Scrollbar *h_scrollbar = hscroll()) {
      sx = h_scrollbar->value();
    }
    
    _widget->damage(FL_DAMAGE_ALL, x1 - sx, y1 - sy, x2 - x1, y2 - y1);
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
  
  cursor = type;
  if(_mouse_moving)
    return;
  
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
  if(!_widget)
    return false;
  
  Document *doc = document();
  if(!doc)
    return false;
  
  animations.add(event);
  if(!_animation_running) {
    _animation_running = true;
    Fl::add_timeout(1.0 / 60, Impl::static_animation_timeout, FrontEndReference::unsafe_cast_to_pointer(doc->id()));
  }
  
  return true;
}

void MathFltkWidget::paint_canvas(Canvas &canvas, bool resize_only) {
  canvas.line_width(1);
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
bool MathFltkWidget::Impl::currently_drawing = false;
bool MathFltkWidget::Impl::need_redraw = false;

MathFltkWidget::Impl::Impl()
: base(0,0, nullptr)
{
  labelsize(10);
  set_draw_cb(Impl::static_cairo_draw_cb);
}

int MathFltkWidget::Impl::handle(int event) {
  AutoMemorySuspension ams;
  
  update_event_time();
  
  switch(event) {
    // TODO: global FL_APP_ACTIVATE, FL_APP_DEACTIVATE
    case FL_FOCUS:   return handle_focus();
    case FL_UNFOCUS: return handle_unfocus();
    
    case FL_KEYDOWN: return handle_keydown();
    case FL_KEYUP:   return handle_keyup();
    
    case FL_MOUSEWHEEL: return handle_mousewheel();
    
    case FL_ENTER:   return 1;
    case FL_PUSH:    return handle_mouse_down();
    case FL_DRAG:
    case FL_MOVE:    return handle_mouse_move();
    case FL_RELEASE: return handle_mouse_up();
    case FL_LEAVE:   return handle_mouse_leave();
    
//    default:
//      pmath_debug_print("[handle %s]\n", fl_eventname_str(event).c_str());
//      break;
  }
  return base::handle(event);
}

int MathFltkWidget::Impl::handle_focus() {
  MathFltkWidget *owner = find_owner();
  if(!owner)
    return 0;
  
  if(Document *doc = owner->document()) {
    Documents::focus_gained(doc);
    
    if(doc->selectable())
      owner->do_set_selected_document();
    
    Box *sel_box = doc->selection_box();
    if(sel_box && doc->selection_length() == 0) {
      auto ctx = owner->document_context();
      ctx->old_selection.id = FrontEndReference::None;
      
      //caret_blink_begin_location = SelectionReference();
      sel_box->request_repaint_range(ctx->selection.start, ctx->selection.end);
    }
  }
  
  return 1;
}

int MathFltkWidget::Impl::handle_unfocus() {
  MathFltkWidget *owner = find_owner();
  if(!owner)
    return 0;
  
  if(Document *doc = owner->document()) {
    Documents::focus_lost(doc);
    
    // TODO: defer until we have a newly focussed document to know whether focus was transferred 
    // to an attached popup. 
    doc->focus_killed(Documents::focused_document());
  }
  
  Fl::compose_reset();
  fl_reset_spot();
  return 1;
}

int MathFltkWidget::Impl::handle_keydown() {
  if(MathFltkMenuItems::handle_shortcut())
    return 1;
  
  MathFltkWidget *owner = find_owner();
  if(!owner)
    return 0;
  
  bool handled = false;
  
  SpecialKeyEvent event {};
  event.key = as_special_key(Fl::event_key());
  
  owner->document()->ignores_next_key_press(false);
  if(event.key != SpecialKey::Unknown) {
    event.ctrl  = Fl::event_ctrl();
    event.alt   = Fl::event_alt();
    event.shift = Fl::event_shift();
    owner->document()->key_down(event);
    
    if(event.key == SpecialKey::Unknown)
      handled = true;
  }
  
//  pmath_debug_print("[handle_keydown key %d %d %s]\n", Fl::event_key(), Fl::event_length(), Fl::event_text());
  int del = 0;
  if(Fl::compose(del)) {
//    pmath_debug_print("[compose => del %d ins %d %s]\n", del, Fl::event_length(), Fl::event_text());
    
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
    
    handled = true;
  }
  else {
    switch(event.key) {
      case SpecialKey::Return: owner->document()->key_press('\r'); handled = true; break;
      case SpecialKey::Tab:    owner->document()->key_press('\t'); handled = true; break;
    }
  }
  
  return handled ? 1 : 0;
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

int MathFltkWidget::Impl::handle_mousewheel() {
  MathFltkWidget *owner = find_owner();
  if(!owner)
    return 0;
  
  if(!owner->is_scrollable())
    return 0;
  
//  pmath_debug_print("[handle FL_MOUSEWHEEL (%s) dx %d (%f) dy %d (%f)]\n",
//    Fl::event_ctrl() ? "ctrl" : "",
//    Fl::event_dx(), Fl::event_dx_f(), 
//    Fl::event_dy(), Fl::event_dy_f());
    
  float dx = Fl::event_dx_f(); // Fl::event_dx();
  float dy = Fl::event_dy_f(); // Fl::event_dy();
  
  if(Fl::event_ctrl()) {
    if(!owner->is_scaleable())
      return 0;
    
    owner->scale_by(pow(2, -0.5 * dy));
    return 1;
  }
  
  if(Fl::event_shift() && dx == 0) {
    dy = dx; dy = 0;
  }
  
  dy *= 20 * 3; // 20 px per line, 3 lines by default on Win32, see SPI_GETWHEELSCROLLLINES
  dx *= 10 * 3; // 10 px per char, 3 chars by default on Win32, see SPI_GETWHEELSCROLLCHARS
  
  owner->scroll_by(dx, dy);
  
  return 1;
}

int MathFltkWidget::Impl::handle_mouse_down() {
  MathFltkWidget *owner = find_owner();
  if(!owner)
    return 0;
  
  MouseEvent event = prepare_mouse_event();
  
  owner->document()->mouse_down(event);
  
  return 1;
}

int MathFltkWidget::Impl::handle_mouse_up() {
  MathFltkWidget *owner = find_owner();
  if(!owner)
    return 0;
  
  MouseEvent event = prepare_mouse_event(); // TODO: depressed button
  
  Document *doc = owner->document();
  doc->mouse_up(event);
  
  if(event.right) {
    bool dummy;
    auto src = doc->mouse_selection(event.position, &dummy);
    if(!src.box)
      src = VolatileSelection(doc, 0);
    
    Expr menu_expr = src.box->get_finished_flatlist_style(ContextMenu);
    if(menu_expr.item_equals(0, richmath_System_List) && menu_expr.expr_length() > 0) {
      menu_expr = Call(Symbol(richmath_System_Menu), strings::Popup, PMATH_CPP_MOVE(menu_expr));
      
      MathFltkMenuItems menu;
      menu.reload(menu_expr);
      MathFltkMenuItems::update_states(menu.menutable(), src.box->id());
      
      const Fl_Menu_Item *picked = menu.menutable()->popup(Fl::event_x(), Fl::event_y());
      if(picked) {
        picked->do_callback(this, FL_REASON_SELECTED);
      }
    }
  }
  
  return 1;
}

int MathFltkWidget::Impl::handle_mouse_move() {
  MathFltkWidget *owner = find_owner();
  if(!owner)
    return 0;
  
  MouseEvent event = prepare_mouse_event();
  
  owner->_mouse_moving = true;
  owner->cursor = CursorType::Default;
  
  owner->document()->mouse_move(event);
  
  owner->_mouse_moving = false;
  owner->set_cursor(owner->cursor);
  
  return 1;
}

int MathFltkWidget::Impl::handle_mouse_leave() {
  MathFltkWidget *owner = find_owner();
  if(!owner)
    return 0;
  
  owner->document()->mouse_exit();
  cursor(FL_CURSOR_DEFAULT);
  return 1;
}

MouseEvent MathFltkWidget::Impl::prepare_mouse_event() {
  MathFltkWidget *owner = find_owner();
  if(!owner)
    return {};
  
  MouseEvent event {};
  
  event.device = DeviceKind::Mouse;
  
  event.left      = Fl::event_button1() || Fl::event_button() == FL_LEFT_MOUSE;
  event.middle    = Fl::event_button2() || Fl::event_button() == FL_MIDDLE_MOUSE;
  event.right     = Fl::event_button3() || Fl::event_button() == FL_RIGHT_MOUSE;
  event.ctrl_key  = Fl::event_ctrl();
  event.shift_key = Fl::event_shift();
  event.alt_key   = Fl::event_alt();
  
  float scale = owner->scale_factor();
  event.position = owner->scroll_pos() + Vector2F(Fl::event_x(), Fl::event_y()) / scale;
  
  return event;
}

void MathFltkWidget::Impl::static_cairo_draw_cb(Fl_Cairo_Window *window, cairo_t *cr) {
  MathFltkWidget *owner = dynamic_cast<MathFltkWidget*>(BasicFltkWidget::from_widget(window));
  if(!owner)
    return;
  
  AutoMemorySuspension ams;
  
  bool old_currently_drawing = currently_drawing;
  currently_drawing = true;
  
  need_redraw = false;
  
  {
    Canvas canvas(cr);
    
    canvas.save();
    owner->paint_canvas(canvas, false);
    canvas.restore();
    
    while(need_redraw) {
      need_redraw = false;
      pmath_debug_print("[redraw]");
      
      cairo_set_operator(canvas.cairo(), CAIRO_OPERATOR_CLEAR);
      canvas.set_color(Color::Black, 0.0);
      canvas.paint();
      cairo_set_operator(canvas.cairo(), CAIRO_OPERATOR_OVER);
      
      canvas.save();
      owner->paint_canvas(canvas, false);
      canvas.restore();
    }
  }
  
  currently_drawing = old_currently_drawing;
  
  cairo_surface_flush(cairo_get_target(cr));
}

void MathFltkWidget::Impl::static_animation_timeout(void *_id) {
  Document *doc = FrontEndObject::find_cast<Document>(FrontEndReference::unsafe_cast_from_pointer(_id));
  if(!doc)
    return;
  
  MathFltkWidget *wid = dynamic_cast<MathFltkWidget*>(doc->native());
  if(!wid)
    return;
  
  wid->_animation_running = false;
  for(auto e : wid->animations.deletable_entries()) {
    if(e.key->min_wait_seconds <= e.key->timer()) {
      auto anim = e.key;
      e.delete_self();
      anim->execute_event();
    }
    else if(!wid->_animation_running) {
      wid->_animation_running = true;
      Fl::repeat_timeout(1.0 / 60, static_animation_timeout, _id);
    }
  }
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
