#include <boxes/sliderbox.h>
#include <eval/application.h>
#include <gui/document.h>
#include <gui/native-widget.h>

#include <cmath>
#include <limits>

using namespace richmath;
using namespace std;

extern pmath_symbol_t richmath_System_SliderBox;

#ifdef _MSC_VER
namespace std {
  static bool isnan(double d) {return _isnan(d);}
}
#endif

#ifndef NAN
#  define NAN  (std::numeric_limits<double>::quiet_NaN())
#endif

namespace richmath {
  class SliderBox::Impl {
    private:
      SliderBox &self;
    public:
      Impl(SliderBox &self) : self(self) {}
      
    public:
      double mouse_to_val(double mouse_x);
      
      float calc_thumb_pos(double val);
      
      bool approximately_equals(double val1, double val2);
      
      Expr position_to_value(double d, bool evaluate);
      
      void assign_dynamic_value(double d, bool pre, bool middle, bool post);
      
      void finish_update_value();
      
      void paint_error_indicator_if_necessary(Canvas &canvas, Point pos);
      
    private:
      void paint_error_indicator(Canvas &canvas, Point pos);
      void paint_underflow_indicator(Canvas &canvas, Point pos);
      void paint_overflow_indicator(Canvas &canvas, Point pos);
    
    public:
      void paint_channel(Canvas &canvas, Point pos);
      void animate_thumb(Context &context, Point pos, double old_value);
      
    private:
      SharedPtr<BoxAnimation> create_thumb_animation(Canvas &canvas, Point pos, ControlState state1, ControlState state2);
  };
}

//{ class SliderBox ...

SliderBox::SliderBox()
  : EmptyWidgetBox(SliderHorzThumb),
    range_min(0.0),
    range_max(1.0),
    range_step(0.0),
    range_value(0.5),
    thumb_width(1),
    channel_width(1),
    have_drawn(false),
    mouse_over_thumb(false),
    use_double_values(true)
{
  dynamic.init(this, Expr());
}

SliderBox::~SliderBox() {
}

bool SliderBox::try_load_from_object(Expr expr, BoxInputFlags opts) {
  if(expr[0] != richmath_System_SliderBox)
    return false;
    
  if(expr.expr_length() < 2)
    return false;
    
  Expr options = Expr(pmath_options_extract_ex(expr.get(), 2, PMATH_OPTIONS_EXTRACT_UNKNOWN_WARNONLY));
  
  if(options.is_null())
    return false;
    
  Expr   new_range             = expr[2];
  double new_range_min         = NAN;
  double new_range_max         = NAN;
  double new_range_step        = 0.0;
  bool   new_use_double_values = true;
  
  if(new_range[0] == PMATH_SYMBOL_RANGE) {
    if(new_range.expr_length() == 2) {
      new_range_min = new_range[1].to_double(NAN);
      new_range_max = new_range[2].to_double(NAN);
      
      new_range_step = 0.0;
    }
    else if(new_range.expr_length() == 3) {
      new_range_min  = new_range[1].to_double(NAN);
      new_range_max  = new_range[2].to_double(NAN);
      new_range_step = new_range[3].to_double(NAN);
      
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
  else if(new_range.expr_length() > 0 && new_range[0] == PMATH_SYMBOL_LIST) {
    new_range_min  = 1;
    new_range_max  = new_range.expr_length();
    new_range_step = 1;
  }
  else
    return false;
    
  if(std::isnan(new_range_min))
    return false;
    
  if(std::isnan(new_range_max))
    return false;
    
  if(std::isnan(new_range_step))
    return false;
    
  /* now success is guaranteed */
  
  if(dynamic.expr() != expr[1] || has(opts, BoxInputFlags::ForceResetDynamic)) {
    dynamic     = expr[1];
    must_update = true;
  }
  
  reset_style();
  style->add_pmath(options);
    
  range             = new_range;
  range_min         = new_range_min;
  range_max         = new_range_max;
  range_step        = new_range_step;
  use_double_values = new_use_double_values;
  
  finish_load_from_object(std::move(expr));
  return true;
}

ControlState SliderBox::calc_state(Context &context) {
  if(!enabled())
    return Disabled;
  
  if(mouse_left_down)
    return PressedHovered;
    
  if(mouse_inside && mouse_over_thumb)
    return Hovered;
    
  return Normal;
}

void SliderBox::resize(Context &context) {
  float em = context.canvas().get_font_size();
  _extents.ascent  = 0.75 * em * 1.5;
  _extents.descent = 0;
  _extents.width   = 6 * em * 1.5;
  
  BoxSize size = _extents;
  ControlPainter::std->calc_container_size(*this, context.canvas(), SliderHorzThumb, &size);
  
  thumb_width = size.width;
  _extents.ascent  = size.ascent;
  _extents.descent = size.descent;
  
  size = _extents;
  ControlPainter::std->calc_container_size(*this, context.canvas(), SliderHorzChannel, &size);
  channel_width = size.height();
  
  float h = _extents.height();
  _extents.ascent = 0.25 * em + 0.5 * h;
  _extents.descent = h - _extents.ascent;
}

void SliderBox::paint(Context &context) {
  if(context.canvas().show_only_text)
    return;
    
  update_dynamic_styles(context);
  
  double old_value = range_value;
  
  have_drawn = true;
  Impl(*this).finish_update_value();
  
  Point pos = context.canvas().current_pos();
  pos.y -= _extents.ascent;
  
  Impl(*this).paint_error_indicator_if_necessary(context.canvas(), pos);
  Impl(*this).paint_channel(context.canvas(), pos);
  Impl(*this).animate_thumb(context, pos, old_value);
}

Expr SliderBox::to_pmath_symbol() {
  return Symbol(richmath_System_SliderBox);
}

Expr SliderBox::to_pmath(BoxOutputFlags flags) {
  Gather g;
  
  if(has(flags, BoxOutputFlags::Literal))
    Gather::emit(to_literal());
  else
    Gather::emit(dynamic.expr());
  
  Gather::emit(range);
  
  if(style) {
    bool with_inherited = true;
    
    String s;
    if(style->get(BaseStyleName, &s) && s.equals("Slider"))
      with_inherited = false;
    
    style->emit_to_pmath(with_inherited);
  }
  
  Expr result = g.end();
  result.set(0, Symbol(richmath_System_SliderBox));
  return result;
}

void SliderBox::reset_style() {
  Style::reset(style, "Slider");
}

VolatileSelection SliderBox::mouse_selection(Point pos, bool *was_inside_start) {
  *was_inside_start = true;
  return { this, 0, 0 };
}

void SliderBox::dynamic_updated() {
  if(must_update)
    return;
    
  must_update = true;
  request_repaint_all();
}

void SliderBox::dynamic_finished(Expr info, Expr result) {
  double new_value = result.to_double(NAN);
  
  if(range_value != new_value)
    request_repaint_all();
}

Expr SliderBox::to_literal() {
  if(!dynamic.is_dynamic())
    return dynamic.expr();
  
  return Impl(*this).position_to_value(range_value, true);
}

VolatileSelection SliderBox::dynamic_to_literal(int start, int end) {
  dynamic = to_literal();
  return {this, start, end};
}

void SliderBox::on_mouse_exit() {
  base::on_mouse_exit();
}

void SliderBox::on_mouse_down(MouseEvent &event) {
  base::on_mouse_down(event);
  
  if(mouse_left_down && enabled()) {
    event.set_origin(this);
    
    if(dynamic.is_dynamic())
      Application::activated_control(this);
      
    double val = Impl(*this).mouse_to_val(event.position.x);
    
    bool has_pre = dynamic.has_pre_or_post_assignment();
    if(has_pre || !Impl(*this).approximately_equals(val, range_value)) {
      if(has_pre || get_own_style(ContinuousAction, true)) 
        Impl(*this).assign_dynamic_value(val, true, true, false);
      else
        range_value = val;
    }
  }
}

void SliderBox::on_mouse_move(MouseEvent &event) {
  EmptyWidgetBox::on_mouse_move(event);
  
  if(enabled()) {
    event.set_origin(this);
    
    if(mouse_left_down) {
      double val = Impl(*this).mouse_to_val(event.position.x);
      
      if(!Impl(*this).approximately_equals(val, range_value)) {
        if(dynamic.has_pre_or_post_assignment() || get_own_style(ContinuousAction, true)) {
          Impl(*this).assign_dynamic_value(val, false, true, false);
          if(dynamic.has_temporary_assignment())
            range_value = val;
        }
        else
          range_value = val;
          
        request_repaint_all();
      }
    }
    else {
      float tx = Impl(*this).calc_thumb_pos(range_value);
      
      bool old_mot = mouse_over_thumb;
      mouse_over_thumb = (tx <= event.position.x && event.position.x <= tx + thumb_width);
      
      if(old_mot != mouse_over_thumb)
        request_repaint_all();
    }
  }
}

void SliderBox::on_mouse_up(MouseEvent &event) {
  if(event.left && enabled()) {
    event.set_origin(this);
    double val = Impl(*this).mouse_to_val(event.position.x);
    if( //!Impl(*this).approximately_equals(val, range_value) ||
        dynamic.synchronous_updating() == AutoBoolAutomatic ||
        !get_own_style(ContinuousAction, true) ||
        dynamic.has_pre_or_post_assignment())
    {
      Impl(*this).assign_dynamic_value(val, false, true, true);
      //Impl(*this).assign_dynamic_value(val, false, val != range_value || dynamic.synchronous_updating() == AutoBoolAutomatic || !get_own_style(ContinuousAction, true), true);
    }
    
    Application::deactivated_control(this);
  }
  
  base::on_mouse_up(event);
}

void SliderBox::on_mouse_cancel() {
  if(dynamic.has_temporary_assignment()) {
    must_update = true;
    request_repaint_all();
  }
  
  base::on_mouse_cancel();
}

//} ... class SliderBox

//{ class SliderBox::Impl ...

double SliderBox::Impl::mouse_to_val(double mouse_x) {
  mouse_x -= self.thumb_width / 2;
  if(mouse_x < 0)
    mouse_x = 0;
  if(mouse_x > self._extents.width - self.thumb_width)
    mouse_x = self._extents.width - self.thumb_width;
    
  double val;
  if(self.range_min != self.range_max) {
    val = (mouse_x / (self._extents.width - self.thumb_width)) * (self.range_max - self.range_min);
    
    if(self.range_step != 0) {
      val = self.range_min + floor(val / self.range_step + 0.5) * self.range_step;
      
      if(self.range_min < self.range_max) {
        if(val > self.range_max)
          val -= self.range_step;
      }
      else {
        if(val < self.range_max)
          val += self.range_step;
      }
    }
    else
      val += self.range_min;
  }
  else
    val = self.range_min;
    
  return val;
}

float SliderBox::Impl::calc_thumb_pos(double val) {
  if(std::isnan(val))
    return self._extents.width / 2 - self.thumb_width / 2;
    
  if(self.range_min < self.range_max) {
    if(val < self.range_min)
      return 0;
      
    if(val > self.range_max)
      return self._extents.width - self.thumb_width;
      
    return (val - self.range_min) / (self.range_max - self.range_min) * (self._extents.width - self.thumb_width);
  }
  
  if(self.range_min > self.range_max) {
    if(val < self.range_max)
      return 0;
      
    if(val > self.range_min)
      return self._extents.width - self.thumb_width;
      
    return (val - self.range_min) / (self.range_max - self.range_min) * (self._extents.width - self.thumb_width);
  }
  
  return self._extents.width / 2 - self.thumb_width / 2;
}

bool SliderBox::Impl::approximately_equals(double val1, double val2) {
  double mouse_x_1 = calc_thumb_pos(val1);
  double mouse_x_2 = calc_thumb_pos(val2);
  double dx = mouse_x_1 - mouse_x_2;
  
  if(dx == 0)
    return true;
  
  cairo_matrix_t mat;
  cairo_matrix_init_identity(&mat);
  
  self.transformation(nullptr, &mat);
  
  double dy = 0.0;
  cairo_matrix_transform_distance(&mat, &dx, &dy);
  
  return dx * dx + dy * dy < 0.5; // 0.75 is one pixel; TODO: use document's DPI
}

Expr SliderBox::Impl::position_to_value(double d, bool evaluate) {
  if(self.range[0] == PMATH_SYMBOL_LIST)
    return self.range[(size_t)d];
  
  if(self.use_double_values)
    return Expr(d);
  
  Expr result = Plus(self.range[1], Round(Minus(d, self.range[1]), self.range[3]));
  if(evaluate)
    result = Application::interrupt_wait_cached(result, Application::dynamic_timeout);
  return result;
}

void SliderBox::Impl::assign_dynamic_value(double d, bool pre, bool middle, bool post) {
  if(!self.have_drawn)
    return;
    
  self.have_drawn = false;
  self.dynamic.assign(position_to_value(d, false), pre, middle, post);
}

void SliderBox::Impl::finish_update_value() {
  if(!self.must_update)
    return;
    
  self.must_update = false;
  
  Expr val;
  if(self.dynamic.get_value(&val)) {
    if(self.range[0] == PMATH_SYMBOL_LIST) {
      self.range_value = self.range_min;
      
      size_t i;
      for(i = 1; i <= self.range.expr_length(); ++i)
        if(self.range[i] == val) {
          self.range_value = i;
          break;
        }
    }
    else {
      self.range_value = val.to_double(NAN);
    }
  }
}

void SliderBox::Impl::paint_error_indicator_if_necessary(Canvas &canvas, Point pos) {
  if(std::isnan(self.range_value)) {
    paint_error_indicator(canvas, pos);
  }
  else if(self.range_value < self.range_min && self.range_value < self.range_max) {
    paint_underflow_indicator(canvas, pos);
  }
  else if(self.range_value > self.range_max && self.range_value > self.range_min) {
    paint_overflow_indicator(canvas, pos);
  }
}

void SliderBox::Impl::paint_error_indicator(Canvas &canvas, Point pos) {
  float rx = pos.x + self._extents.width / 2;
  float h = self._extents.height();
  
  Color old_color = canvas.get_color();
  canvas.save();
  canvas.set_color(Color::from_rgb24(0xFF0000), 0.2);
  for(int i = -2; i <= 2; ++i) {
    canvas.arc(rx + i * h / 6, pos.y + h / 2, h / 2, 0, 2 * M_PI, false);
    canvas.fill();
  }
  canvas.restore();
  canvas.set_color(old_color);
}

void SliderBox::Impl::paint_underflow_indicator(Canvas &canvas, Point pos) {
  float h = self._extents.height();
  float rx = pos.x + h / 2;
  
  Color old_color = canvas.get_color();
  canvas.save();
  canvas.set_color(Color::from_rgb24(0xFF0000), 0.2);
  for(int i = 0; i <= 2; ++i) {
    canvas.arc(rx + i * h / 6, pos.y + h / 2, h / 2, 0, 2 * M_PI, false);
    canvas.fill();
  }
  canvas.restore();
  canvas.set_color(old_color);
}

void SliderBox::Impl::paint_overflow_indicator(Canvas &canvas, Point pos) {
  float h = self._extents.height();
  float rx = pos.x + self._extents.width - h / 2;
  
  Color old_color = canvas.get_color();
  canvas.save();
  canvas.set_color(Color::from_rgb24(0xFF0000), 0.2);
  for(int i = -2; i <= 0; ++i) {
    canvas.arc(rx + i * h / 6, pos.y + h / 2, h / 2, 0, 2 * M_PI, false);
    canvas.fill();
  }
  canvas.restore();
  canvas.set_color(old_color);
}

void SliderBox::Impl::paint_channel(Canvas &canvas, Point pos) {
  float h = self._extents.height();
  ControlPainter::std->draw_container(
    ControlContext::find(&self),
    canvas,
    SliderHorzChannel,
    Normal,
    RectangleF(
      pos.x,
      pos.y + h / 2 - self.channel_width / 2,
      self._extents.width,
      self.channel_width));
}

void SliderBox::Impl::animate_thumb(Context &context, Point pos, double old_value) {
  ControlState new_state = self.calc_state(context);
  
  if(old_value == self.range_value) {
    if(new_state != self.old_state || !self.animation) {
      self.animation = create_thumb_animation(context.canvas(), pos, self.old_state, new_state);
      self.old_state = new_state;
    }
    
    if(self.animation) {
      if(self.animation->paint(context.canvas()))
        return;
        
      self.animation = create_thumb_animation(context.canvas(), pos, new_state, new_state);
    }
  }
  
  float h = self._extents.height();
  ControlPainter::std->draw_container(
    ControlContext::find(&self),
    context.canvas(),
    SliderHorzThumb,
    new_state,
    RectangleF(
      pos.x + calc_thumb_pos(self.range_value),
      pos.y,
      self.thumb_width,
      self._extents.height()));
}

SharedPtr<BoxAnimation> SliderBox::Impl::create_thumb_animation(Canvas &canvas, Point pos, ControlState state1, ControlState state2) {
  return ControlPainter::std->control_transition(
           self.id(),
           canvas,
           SliderHorzThumb,
           SliderHorzThumb,
           state1,
           state2,
           RectangleF{
             pos.x + calc_thumb_pos(self.range_value),
             pos.y,
             self.thumb_width,
             self._extents.height()});
}

//} ... class SliderBox::Impl
