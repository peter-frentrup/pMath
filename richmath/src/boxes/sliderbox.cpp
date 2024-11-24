#include <boxes/sliderbox.h>
#include <eval/application.h>
#include <eval/cubic-bezier-easing-function.h>
#include <eval/eval-contexts.h>
#include <gui/document.h>
#include <gui/native-widget.h>

#include <algorithm>
#include <cmath>
#include <limits>

using namespace richmath;
using namespace std;

#ifdef _MSC_VER
namespace std {
  static bool isnan(double d) {return _isnan(d);}
}
#endif

#ifdef min
#  undef min
#endif

#ifdef max
#  undef max
#endif

#ifndef NAN
#  define NAN  (std::numeric_limits<double>::quiet_NaN())
#endif

namespace richmath {
  namespace strings {
    extern String DollarContext_namespace;
    extern String DownArrow;
    extern String Horizontal;
    extern String LeftArrow;
    extern String RightArrow;
    extern String Slider;
    extern String ToggleSwitchChecked;
    extern String ToggleSwitchUnchecked;
    extern String UpArrow;
    extern String Vertical;
  }
  
  class SliderBox::Impl {
    private:
      SliderBox &self;
    public:
      Impl(SliderBox &self) : self(self) {}
      
    public:
      bool is_vertical() { return is_vertical(self.type); }
      static bool is_vertical(ContainerType type);
      
      double mouse_to_val(Point p);
      Point calc_thumb_pos(double val);
      Vector2F thumb_size();
      
    private:
      double mouse_to_val_scalar(double x, double min_x, double max_x);
      float calc_thumb_pos_scalar(double val, double max_pos);
      
    public:
      bool approximately_equals(double val1, double val2);
      
      Expr position_to_value(double d, bool evaluate);
      
      void assign_dynamic_value(double d, bool pre, bool middle, bool post);
      
      void finish_update_value();
      double translate_range_value(Expr val);
      void set_range_value(double value);
      
      void paint_error_indicator_if_necessary(Canvas &canvas, Point pos);
      
      Expr to_literal();
      
    private:
      void paint_error_indicator(Canvas &canvas, Point pos);
      void paint_underflow_indicator(Canvas &canvas, Point pos);
      void paint_overflow_indicator(Canvas &canvas, Point pos);
    
    public:
      void paint_channel(Canvas &canvas, Point pos, ControlState state);
      void animate_thumb(Context &context, Point pos, double old_value, ControlState new_state);
      
      static ContainerType parse_thumb_appearance(Expr appearance);
      static ContainerType channel_for_thumb(ContainerType thumb);
      
    private:
      SharedPtr<BoxAnimation> create_thumb_animation(Canvas &canvas, Point pos, ControlState state1, ControlState state2);
  };
}

extern pmath_symbol_t richmath_System_Automatic;
extern pmath_symbol_t richmath_System_List;
extern pmath_symbol_t richmath_System_Range;
extern pmath_symbol_t richmath_System_SliderBox;

//{ class SliderBox ...

SliderBox::SliderBox()
  : EmptyWidgetBox(ContainerType::HorizontalSliderThumb),
    _range_interval(0.0, 1.0),
    _range_step(0.0),
    _range_value(0.5),
    _animation_start_value(0.5),
    _animation_start_time(0.0),
    _thumb_width(1),
    _channel_width(1)
{
  use_double_values(true);
  _dynamic.init(this, Expr());
}

SliderBox::~SliderBox() {
}

bool SliderBox::try_load_from_object(Expr expr, BoxInputFlags opts) {
  if(!expr.item_equals(0, richmath_System_SliderBox))
    return false;
    
  if(expr.expr_length() < 2)
    return false;
    
  Expr options = Expr(pmath_options_extract_ex(expr.get(), 2, PMATH_OPTIONS_EXTRACT_UNKNOWN_WARNONLY));
  
  if(options.is_null())
    return false;
    
  Interval<double> new_range_interval(NAN, NAN);
  Expr   new_range             = expr[2];
  double new_range_step        = 0.0;
  bool   new_use_double_values = true;
  
  if(new_range.item_equals(0, richmath_System_Range)) {
    if(new_range.expr_length() == 2) {
      new_range_interval.from = new_range[1].to_double(NAN);
      new_range_interval.to   = new_range[2].to_double(NAN);
      
      new_range_step = 0.0;
    }
    else if(new_range.expr_length() == 3) {
      new_range_interval.from  = new_range[1].to_double(NAN);
      new_range_interval.to    = new_range[2].to_double(NAN);
      new_range_step           = new_range[3].to_double(NAN);
      
      if(new_range_step != 0.0) {
        new_use_double_values = !Evaluate(
                                  Divide(
                                    Minus(new_range[2], new_range[1]),
                                    new_range[3])
                                ).is_rational();
      }
      
    }
    else
      return false;
  }
  else if(new_range.expr_length() > 0 && new_range.item_equals(0, richmath_System_List)) {
    new_range_interval.from = 1;
    new_range_interval.to   = new_range.expr_length();
    new_range_step          = 1;
  }
  else
    return false;
    
  if(std::isnan(new_range_interval.from))
    return false;
    
  if(std::isnan(new_range_interval.to))
    return false;
    
  if(std::isnan(new_range_step))
    return false;
    
  /* now success is guaranteed */
  
  if(_dynamic.expr() != expr[1] || has(opts, BoxInputFlags::ForceResetDynamic)) {
    _dynamic = expr[1];
    must_update(true);
    is_initialized(false);
  }
  
  reset_style();
  style.add_pmath(options);
    
  _range_expr       = new_range;
  _range_interval   = new_range_interval;
  _range_step       = new_range_step;
  use_double_values(new_use_double_values);
  
  finish_load_from_object(PMATH_CPP_MOVE(expr));
  return true;
}

ControlState SliderBox::calc_state(Context &context) {
  if(!enabled())
    return ControlState::Disabled;
  
  if(mouse_left_down())
    return ControlState::PressedHovered;
    
  if(mouse_inside()) {
    if(mouse_over_thumb())
      return ControlState::Hovered;
    return ControlState::Hot;
  }
  
  return ControlState::Normal;
}

bool SliderBox::expand(Context &context, const BoxSize &size) {
  base::expand(context, size);
  _extents.merge(size);
  return true;
}

void SliderBox::resize(Context &context) {
  float em = context.canvas().get_font_size();
  
  bool vertical = Impl(*this).is_vertical();
  type = Impl(*this).parse_thumb_appearance(get_own_style(Appearance));
  
  const LengthConversionFactors *length_factors;
  const LengthConversionFactors *thickness_factors;
  switch(type) {
    case ContainerType::ToggleSwitchThumbChecked:
    case ContainerType::ToggleSwitchThumbUnchecked:
      length_factors    = &LengthConversionFactors::ToggleSwitchLengthScale;
      thickness_factors = &LengthConversionFactors::ToggleSwitchThicknessScale;
      break;
    
    default:
      length_factors    = &LengthConversionFactors::SliderLengthScale;
      thickness_factors = &LengthConversionFactors::SliderThicknessScale;
      break;
  }
  
  Length wlen = get_own_style(ImageSizeHorizontal, SymbolicSize::Automatic);
  Length hlen = get_own_style(ImageSizeVertical,   SymbolicSize::Automatic);
  if(hlen == SymbolicSize::Automatic && wlen.is_symbolic())
    hlen = wlen;
  
  _extents.ascent  = em;//0.75 * em * 1.5;
  _extents.descent = 0;
  _extents.width   = em;
  
  BoxSize size = _extents;
  ControlPainter::std->calc_container_size(*this, context.canvas(), type, &size);
  
  if(vertical) {
    _extents.width = wlen.resolve(size.width, *thickness_factors, context.width);
    _thumb_width = size.height() * _extents.width / size.width;
  }
  else {
    _extents.ascent = hlen.resolve(size.height(), *thickness_factors, context.width);
    _thumb_width = size.width * _extents.ascent / size.height();
  }
  
  size = _extents;
  ControlPainter::std->calc_container_size(*this, context.canvas(), Impl::channel_for_thumb(type), &size);
  
  if(vertical) {
    _channel_width = min(size.width, _extents.width);
    _extents.ascent = hlen.resolve(size.height(), *length_factors, context.width);
    
    if(_thumb_width > 0.67 * _extents.ascent)
      _thumb_width = 0.67 * _extents.ascent;
  }
  else {
    _channel_width = min(size.height(), _extents.height());
    _extents.width = wlen.resolve(size.width, *length_factors, context.width);
    
    if(_thumb_width > 0.67 * _extents.width)
      _thumb_width = 0.67 * _extents.width;
  }
  
  float h = _extents.height();
  _extents.ascent = 0.25 * em + 0.5 * h;
  _extents.descent = h - _extents.ascent;
}

void SliderBox::paint(Context &context) {
  if(context.canvas().show_only_text)
    return;
    
  update_dynamic_styles(context);
  type = Impl(*this).parse_thumb_appearance(get_own_style(Appearance));
  
  double old_value = _range_value;
  
  have_drawn(true);
  Impl(*this).finish_update_value();
  
  Point pos = context.canvas().current_pos();
  pos.y -= _extents.ascent;
  
  ControlState new_state = calc_state(context);
  
  Impl(*this).paint_error_indicator_if_necessary(context.canvas(), pos);
  Impl(*this).paint_channel(context.canvas(), pos, new_state);
  Impl(*this).animate_thumb(context, pos, old_value, new_state);
  
  old_state = new_state;
}

Expr SliderBox::to_pmath_symbol() {
  return Symbol(richmath_System_SliderBox);
}

Expr SliderBox::to_pmath_impl(BoxOutputFlags flags) {
  Gather g;
  
  if(has(flags, BoxOutputFlags::Literal))
    Gather::emit(Impl(*this).to_literal());
  else
    Gather::emit(_dynamic.expr());
  
  Gather::emit(_range_expr);
  
  if(style) {
    bool with_inherited = true;
    
    String s;
    if(style.get(BaseStyleName, &s) && s == strings::Slider)
      with_inherited = false;
    
    style.emit_to_pmath(with_inherited);
  }
  
  Expr result = g.end();
  result.set(0, Symbol(richmath_System_SliderBox));
  return result;
}

void SliderBox::reset_style() {
  style.reset(strings::Slider);
}

VolatileSelection SliderBox::mouse_selection(Point pos, bool *was_inside_start) {
  *was_inside_start = true;
  return { this, 0, 0 };
}

void SliderBox::dynamic_updated() {
  if(must_update())
    return;
    
  must_update(true);
  
  style.flag_pending_dynamic();
  
  request_repaint_all();
}

void SliderBox::dynamic_finished(Expr info, Expr result) {
  double new_value = Impl(*this).translate_range_value(PMATH_CPP_MOVE(result));
  
  if(_range_value != new_value) {
    Impl(*this).set_range_value(new_value);
    request_repaint_all();
  }
}

VolatileSelection SliderBox::dynamic_to_literal(int start, int end) {
  _dynamic = Impl(*this).to_literal();
  return {this, start, end};
}

void SliderBox::on_mouse_exit() {
  base::on_mouse_exit();
}

void SliderBox::on_mouse_down(MouseEvent &event) {
  base::on_mouse_down(event);
  
  if(mouse_left_down() && enabled()) {
    event.set_origin(this);
    
    if(_dynamic.is_dynamic())
      Application::activated_control(this);
    
    double val = Impl(*this).mouse_to_val(event.position);
    
    bool has_pre = _dynamic.has_pre_or_post_assignment();
    if(has_pre || !Impl(*this).approximately_equals(val, _range_value)) {
      if(has_pre || get_own_style(ContinuousAction, true)) 
        Impl(*this).assign_dynamic_value(val, true, true, false);
      else
        Impl(*this).set_range_value(val);
    }
  }
}

void SliderBox::on_mouse_move(MouseEvent &event) {
  EmptyWidgetBox::on_mouse_move(event);
  
  if(enabled()) {
    event.set_origin(this);
    
    if(mouse_left_down()) {
      double val = Impl(*this).mouse_to_val(event.position);
      
      if(!Impl(*this).approximately_equals(val, _range_value)) {
        if(_dynamic.has_pre_or_post_assignment() || get_own_style(ContinuousAction, true)) {
          Impl(*this).assign_dynamic_value(val, false, true, false);
          if(_dynamic.has_temporary_assignment())
            Impl(*this).set_range_value(val);
        }
        else
          Impl(*this).set_range_value(val);
          
        request_repaint_all();
      }
    }
    else {
      RectangleF thumb_rect(Impl(*this).calc_thumb_pos(_range_value), Impl(*this).thumb_size());
      thumb_rect.y-= _extents.ascent;
      
      bool old_mot = mouse_over_thumb();
      mouse_over_thumb(thumb_rect.contains(event.position));
      
      if(old_mot != mouse_over_thumb())
        request_repaint_all();
    }
  }
}

void SliderBox::on_mouse_up(MouseEvent &event) {
  if(event.left && enabled()) {
    event.set_origin(this);
    double val = Impl(*this).mouse_to_val(event.position);
    if( //!Impl(*this).approximately_equals(val, range_value) ||
        _dynamic.synchronous_updating() == AutoBoolAutomatic ||
        !get_own_style(ContinuousAction, true) ||
        _dynamic.has_pre_or_post_assignment())
    {
      Impl(*this).assign_dynamic_value(val, false, true, true);
      //Impl(*this).assign_dynamic_value(val, false, val != range_value || dynamic.synchronous_updating() == AutoBoolAutomatic || !get_own_style(ContinuousAction, true), true);
    }
    
    Application::deactivated_control(this);
  }
  
  base::on_mouse_up(event);
}

void SliderBox::on_mouse_cancel() {
  if(_dynamic.has_temporary_assignment()) {
    must_update(true);
    request_repaint_all();
  }
  
  base::on_mouse_cancel();
}

void SliderBox::range_value(double new_val) {
  Impl(*this).assign_dynamic_value(new_val, true, true, true);
}

//} ... class SliderBox

//{ class SliderBox::Impl ...

bool SliderBox::Impl::is_vertical(ContainerType type) {
  switch(type) {
    case ContainerType::VerticalSliderChannel:
    case ContainerType::VerticalSliderLeftArrowButton:
    case ContainerType::VerticalSliderRightArrowButton:
    case ContainerType::VerticalSliderThumb:
      return true;
    
    default:
      return false;
  }
}

double SliderBox::Impl::mouse_to_val(Point p) {
  float t_half = 0.5f * self._thumb_width;
  
  if(is_vertical()) 
    return mouse_to_val_scalar(self._extents.descent - p.y, t_half, self._extents.height() - t_half);
  else
    return mouse_to_val_scalar(p.x, t_half, self._extents.width - t_half);
}

double SliderBox::Impl::mouse_to_val_scalar(double x, double min_x, double max_x) {
  if(x < min_x)
    x = min_x;
  if(x > max_x)
    x = max_x;
    
  double val;
  if(self._range_interval.from != self._range_interval.to) {
    val = ((x - min_x) / (max_x - min_x)) * (self._range_interval.to - self._range_interval.from);
    
    if(self._range_step != 0) {
      val = self._range_interval.from + round(val / self._range_step) * self._range_step;
      
      if(self._range_interval.from < self._range_interval.to) {
        if(val > self._range_interval.to)
          val -= self._range_step;
      }
      else {
        if(val < self._range_interval.to)
          val += self._range_step;
      }
    }
    else
      val += self._range_interval.from;
  }
  else
    val = self._range_interval.from;
    
  return val;
}

Point SliderBox::Impl::calc_thumb_pos(double val) {
  if(is_vertical()) {
    float h = self._extents.height() - self._thumb_width;
    return Point(0.0f, h - calc_thumb_pos_scalar(val, h));
  }
  else
    return Point(calc_thumb_pos_scalar(val, self._extents.width - self._thumb_width), 0.0f);
}

Vector2F SliderBox::Impl::thumb_size() {
  if(is_vertical())
    return Vector2F(self._extents.width, self._thumb_width);
  else
    return Vector2F(self._thumb_width, self._extents.height());
}

float SliderBox::Impl::calc_thumb_pos_scalar(double val, double max_pos) {
  if(std::isnan(val))
    return max_pos / 2;
    
  if(self._range_interval.from < self._range_interval.to) {
    if(val < self._range_interval.from)
      return 0;
      
    if(val > self._range_interval.to)
      return max_pos;
      
    return (val - self._range_interval.from) / (self._range_interval.to - self._range_interval.from) * max_pos;
  }
  
  if(self._range_interval.from > self._range_interval.to) {
    if(val < self._range_interval.to)
      return 0;
      
    if(val > self._range_interval.from)
      return max_pos;
      
    return (val - self._range_interval.from) / (self._range_interval.to - self._range_interval.from) * max_pos;
  }
  
  return max_pos / 2;
}

bool SliderBox::Impl::approximately_equals(double val1, double val2) {
  Vector2F delta = calc_thumb_pos(val1) - calc_thumb_pos(val2);
  
  if(delta.x == 0 && delta.y == 0)
    return true;
  
  cairo_matrix_t mat;
  cairo_matrix_init_identity(&mat);
  
  self.transformation(nullptr, &mat);
  
  double dx = delta.x;
  double dy = delta.y;
  cairo_matrix_transform_distance(&mat, &dx, &dy);
  
  return dx * dx + dy * dy < 0.5; // 0.75 is one pixel; TODO: use document's DPI
}

Expr SliderBox::Impl::position_to_value(double d, bool evaluate) {
  if(self._range_expr.item_equals(0, richmath_System_List))
    return self._range_expr[(size_t)d];
  
  if(self.use_double_values())
    return Expr(d);
  
  Expr result = Plus(self._range_expr[1], Call(Symbol(richmath_System_Round), Minus(d, self._range_expr[1]), self._range_expr[3]));
  if(evaluate)
    result = Application::interrupt_wait_cached(result, Application::dynamic_timeout);
  return result;
}

void SliderBox::Impl::assign_dynamic_value(double d, bool pre, bool middle, bool post) {
  if(!self.have_drawn())
    return;
    
  self.have_drawn(false);
  self._dynamic.assign(position_to_value(d, false), pre, middle, post);
}

void SliderBox::Impl::finish_update_value() {
  if(!self.must_update())
    return;
    
  self.must_update(false);
  
  bool was_initialized = self.is_initialized();
  self.is_initialized(true);
  
  Expr val;
  if(!self._dynamic.get_value(&val)) 
    return;
  
  if(!was_initialized && val.is_symbol() && self._dynamic.is_dynamic_of(val)) {
    set_range_value(self._range_interval.from);
    assign_dynamic_value(self._range_value, true, true, true);
    
    if(!self._dynamic.get_value(&val))
      return;
  }
    
  val = EvaluationContexts::replace_symbol_namespace(
          PMATH_CPP_MOVE(val), 
          EvaluationContexts::resolve_context(&self), 
          strings::DollarContext_namespace);
  
  set_range_value(translate_range_value(PMATH_CPP_MOVE(val)));
}

double SliderBox::Impl::translate_range_value(Expr val) {
  if(self._range_expr.item_equals(0, richmath_System_List)) {
    size_t i;
    for(i = 1; i <= self._range_expr.expr_length(); ++i) {
      if(self._range_expr.item_equals(i, val)) {
        return i;
      }
    }
    
    return self._range_interval.from;
  }
  
  return val.to_double(NAN);
}

void SliderBox::Impl::set_range_value(double value) {
  self._animation_start_value = self._range_value;
  self._animation_start_time  = pmath_tickcount();
  self._range_value           = value;
}

Expr SliderBox::Impl::to_literal() {
  if(!self._dynamic.is_dynamic())
    return self._dynamic.expr();
  
  return position_to_value(self._range_value, true);
}

void SliderBox::Impl::paint_error_indicator_if_necessary(Canvas &canvas, Point pos) {
  if(std::isnan(self._range_value)) {
    paint_error_indicator(canvas, pos);
  }
  else if(self._range_value < self._range_interval.from && self._range_value < self._range_interval.to) {
    paint_underflow_indicator(canvas, pos);
  }
  else if(self._range_value > self._range_interval.to && self._range_value > self._range_interval.from) {
    paint_overflow_indicator(canvas, pos);
  }
}

void SliderBox::Impl::paint_error_indicator(Canvas &canvas, Point pos) {
  Point center = pos + Vector2F(self._extents.width / 2, self._extents.height() / 2);
  float radius = std::min(self._extents.width, self._extents.height()) * 0.5f;
  Vector2F dir = is_vertical() ? Vector2F(0, -radius/3) : Vector2F(-radius/3, 0);
  
  Color old_color = canvas.get_color();
  canvas.save();
  canvas.set_color(Color::from_rgb24(0xFF0000), 0.2);
  for(int i = -2; i <= 2; ++i) {
    canvas.arc(center + i * dir, radius, 0, 2 * M_PI, false);
    canvas.fill();
  }
  canvas.restore();
  canvas.set_color(old_color);
}

void SliderBox::Impl::paint_underflow_indicator(Canvas &canvas, Point pos) {
  float radius = std::min(self._extents.width, self._extents.height()) * 0.5f;
  Point center;
  Vector2F dir;
  if(is_vertical()) {
    center = pos + Vector2F(self._extents.width/2, self._extents.height() - radius);
    dir = Vector2F(0, -radius/3);
  }
  else {
    center = pos + Vector2F(radius, self._extents.height()/2);
    dir = Vector2F(radius/3, 0);
  }
  
  Color old_color = canvas.get_color();
  canvas.save();
  canvas.set_color(Color::from_rgb24(0xFF0000), 0.2);
  for(int i = 0; i <= 2; ++i) {
    canvas.arc(center + i * dir, radius, 0, 2 * M_PI, false);
    canvas.fill();
  }
  canvas.restore();
  canvas.set_color(old_color);
}

void SliderBox::Impl::paint_overflow_indicator(Canvas &canvas, Point pos) {
  float radius = std::min(self._extents.width, self._extents.height()) * 0.5f;
  Point center;
  Vector2F dir;
  if(is_vertical()) {
    center = pos + Vector2F(self._extents.width/2, radius);
    dir = Vector2F(0, -radius/3);
  }
  else {
    center = pos + Vector2F(self._extents.width - radius, self._extents.height()/2);
    dir = Vector2F(radius/3, 0);
  }
  
  Color old_color = canvas.get_color();
  canvas.save();
  canvas.set_color(Color::from_rgb24(0xFF0000), 0.2);
  for(int i = -2; i <= 0; ++i) {
    canvas.arc(center + i * dir, radius, 0, 2 * M_PI, false);
    canvas.fill();
  }
  canvas.restore();
  canvas.set_color(old_color);
}

void SliderBox::Impl::paint_channel(Canvas &canvas, Point pos, ControlState state) {
  RectangleF rect;
  if(is_vertical()) {
    float w = self._extents.width;
    rect = {
      pos.x + w / 2 - self._channel_width / 2,
      pos.y,
      self._channel_width,
      self._extents.height()};
  }
  else {
    float h = self._extents.height();
    rect = {
      pos.x,
      pos.y + h / 2 - self._channel_width / 2,
      self._extents.width,
      self._channel_width};
  }
  ControlPainter::std->draw_container(
    ControlContext::find(&self),
    canvas,
    Impl::channel_for_thumb(self.type),
    state,
    rect);
}

void SliderBox::Impl::animate_thumb(Context &context, Point pos, double old_value, ControlState new_state) {
  double current_value = self._range_value;
  
  if(current_value != self._animation_start_value && isfinite(self._animation_start_value)) {
    double now      = pmath_tickcount();
    double duration = now - self._animation_start_time;
    double max_duration = ControlPainter::std->enable_animations() ? 0.1 : 0.0;
    if(0.0 <= duration && duration < max_duration) {
      
      SharedPtr<BoxRepaintEvent> ev = new BoxRepaintEvent(self.id(), 0.0);
      if(ev->register_event()) {
        current_value = self._animation_start_value + CubicBezierEasingFunction::EaseOut(duration / max_duration) * (current_value - self._animation_start_value);
      }
      else {
        self._animation_start_value = current_value;
      }
    }
    else {
      self._animation_start_value = current_value;
    }
  }
  
  if(old_value == current_value) {
    if(new_state != self.old_state || !self.animation) {
      self.animation = create_thumb_animation(context.canvas(), pos, self.old_state, new_state);
    }
    
    if(self.animation) {
      if(self.animation->paint(context.canvas()))
        return;
        
      self.animation = create_thumb_animation(context.canvas(), pos, new_state, new_state);
    }
  }
  
  ControlPainter::std->draw_container(
    ControlContext::find(&self),
    context.canvas(),
    self.type,
    new_state,
    RectangleF(
      Vector2F(pos) + calc_thumb_pos(current_value),
      thumb_size()));
}

SharedPtr<BoxAnimation> SliderBox::Impl::create_thumb_animation(Canvas &canvas, Point pos, ControlState state1, ControlState state2) {
  return ControlPainter::std->control_transition(
           self.id(),
           canvas,
           self.type,
           self.type,
           state1,
           state2,
           RectangleF{
             Vector2F(pos) + calc_thumb_pos(self._range_value),
             thumb_size()});
}

ContainerType SliderBox::Impl::parse_thumb_appearance(Expr appearance) {
  if(appearance == richmath_System_Automatic || appearance == strings::Slider || appearance == strings::Horizontal)
    return ContainerType::HorizontalSliderThumb;

  if(appearance == strings::DownArrow)
    return ContainerType::HorizontalSliderDownArrowButton;
  
  if(appearance == strings::ToggleSwitchChecked)
    return ContainerType::ToggleSwitchThumbChecked;

  if(appearance == strings::ToggleSwitchUnchecked)
    return ContainerType::ToggleSwitchThumbUnchecked;

  if(appearance == strings::UpArrow)
    return ContainerType::HorizontalSliderUpArrowButton;
    
  if(appearance == strings::Vertical)
    return ContainerType::VerticalSliderThumb;

  if(appearance == strings::LeftArrow)
    return ContainerType::VerticalSliderLeftArrowButton;

  if(appearance == strings::RightArrow)
    return ContainerType::VerticalSliderRightArrowButton;

  return ContainerType::HorizontalSliderThumb;
}

ContainerType SliderBox::Impl::channel_for_thumb(ContainerType thumb) {
  switch(thumb) {
    case ContainerType::HorizontalSliderDownArrowButton:
    case ContainerType::HorizontalSliderThumb:
    case ContainerType::HorizontalSliderUpArrowButton:  return ContainerType::HorizontalSliderChannel;
    case ContainerType::VerticalSliderLeftArrowButton:
    case ContainerType::VerticalSliderThumb:
    case ContainerType::VerticalSliderRightArrowButton: return ContainerType::VerticalSliderChannel;
    case ContainerType::ToggleSwitchThumbChecked:       return ContainerType::ToggleSwitchChannelChecked;
    case ContainerType::ToggleSwitchThumbUnchecked:     return ContainerType::ToggleSwitchChannelUnchecked;
    
    default: return ContainerType::None;
  }
}

//} ... class SliderBox::Impl
