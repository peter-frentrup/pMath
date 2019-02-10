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
  class SliderBoxImpl {
    private:
      SliderBox &self;
    public:
      SliderBoxImpl(SliderBox &_self)
        : self(_self)
      {
      }
      
    public:
      double mouse_to_val(double mouse_x) {
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
      
    public:
      float calc_thumb_pos(double val) {
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
    
    public:
      bool approximately_equals(double val1, double val2) {
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
      
    public:
      void assign_dynamic_value(double d) {
        if(!self.have_drawn)
          return;
          
        self.have_drawn = false;
        if(self.range[0] == PMATH_SYMBOL_LIST) {
          self.dynamic.assign(self.range[(size_t)d]);
        }
        else if(self.use_double_values) {
          self.dynamic.assign(d);
        }
        else {
          self.dynamic.assign(Plus(self.range[1], Round(Minus(d, self.range[1]), self.range[3])));
        }
      }
      
    public:
      void finish_update_value() {
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
      
    public:
      void paint_error_indicator_if_necessary(Canvas *canvas, float x, float y) {
        if(std::isnan(self.range_value)) {
          paint_error_indicator(canvas, x, y);
        }
        else if(self.range_value < self.range_min && self.range_value < self.range_max) {
          paint_underflow_indicator(canvas, x, y);
        }
        else if(self.range_value > self.range_max && self.range_value > self.range_min) {
          paint_overflow_indicator(canvas, x, y);
        }
      }
      
    private:
      void paint_error_indicator(Canvas *canvas, float x, float y) {
        float rx = x + self._extents.width / 2;
        float h = self._extents.height();
        
        int old_color = canvas->get_color();
        canvas->save();
        canvas->set_color(0xFF0000, 0.2);
        for(int i = -2; i <= 2; ++i) {
          canvas->arc(rx + i * h / 6, y + h / 2, h / 2, 0, 2 * M_PI, false);
          canvas->fill();
        }
        canvas->restore();
        canvas->set_color(old_color);
      }
      
      void paint_underflow_indicator(Canvas *canvas, float x, float y) {
        float h = self._extents.height();
        float rx = x + h / 2;
        
        int old_color = canvas->get_color();
        canvas->save();
        canvas->set_color(0xFF0000, 0.2);
        for(int i = 0; i <= 2; ++i) {
          canvas->arc(rx + i * h / 6, y + h / 2, h / 2, 0, 2 * M_PI, false);
          canvas->fill();
        }
        canvas->restore();
        canvas->set_color(old_color);
      }
      
      void paint_overflow_indicator(Canvas *canvas, float x, float y) {
        float h = self._extents.height();
        float rx = x + self._extents.width - h / 2;
        
        int old_color = canvas->get_color();
        canvas->save();
        canvas->set_color(0xFF0000, 0.2);
        for(int i = -2; i <= 0; ++i) {
          canvas->arc(rx + i * h / 6, y + h / 2, h / 2, 0, 2 * M_PI, false);
          canvas->fill();
        }
        canvas->restore();
        canvas->set_color(old_color);
      }
      
    public:
      void paint_channel(Canvas *canvas, float x, float y) {
        float h = self._extents.height();
        ControlPainter::std->draw_container(
          ControlContext::find(&self),
          canvas,
          SliderHorzChannel,
          Normal,
          x,
          y + h / 2 - self.channel_width / 2,
          self._extents.width,
          self.channel_width);
      }
      
      void animate_thumb(Context *context, float x, float y, double old_value) {
        ControlState new_state = self.calc_state(context);
        
        if(old_value == self.range_value) {
          if(new_state != self.old_state || !self.animation) {
            self.animation = SliderBoxImpl(*this).create_thumb_animation(context->canvas, x, y, self.old_state, new_state);
            self.old_state = new_state;
          }
          
          if(self.animation) {
            if(self.animation->paint(context->canvas))
              return;
              
            self.animation = SliderBoxImpl(*this).create_thumb_animation(context->canvas, x, y, new_state, new_state);
          }
        }
        
        float h = self._extents.height();
        float thumb_x = x + calc_thumb_pos(self.range_value);
        ControlPainter::std->draw_container(
          ControlContext::find(&self),
          context->canvas,
          SliderHorzThumb,
          new_state,
          thumb_x,
          y,
          self.thumb_width,
          h);
      }
      
    public:
      SharedPtr<BoxAnimation> create_thumb_animation(Canvas *canvas, float x, float y, ControlState state1, ControlState state2) {
        float h = self._extents.height();
        float thumb_x = x + calc_thumb_pos(self.range_value);
        
        return ControlPainter::std->control_transition(
                 self.id(),
                 canvas,
                 SliderHorzThumb,
                 SliderHorzThumb,
                 state1,
                 state2,
                 thumb_x,
                 y,
                 self.thumb_width,
                 h);
      }
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
  
  if(style) {
    reset_style();
    style->add_pmath(options);
  }
  else
    style = new Style(options);
    
  range             = new_range;
  range_min         = new_range_min;
  range_max         = new_range_max;
  range_step        = new_range_step;
  use_double_values = new_use_double_values;
  
  finish_load_from_object(std::move(expr));
  return true;
}

ControlState SliderBox::calc_state(Context *context) {
  if(mouse_left_down)
    return PressedHovered;
    
  if(mouse_inside && mouse_over_thumb)
    return Hovered;
    
  return Normal;
}

void SliderBox::resize(Context *context) {
  float em = context->canvas->get_font_size();
  _extents.ascent  = 0.75 * em * 1.5;
  _extents.descent = 0.25 * em * 1.5;
  _extents.width   = 6 * em * 1.5;
  
  BoxSize size = _extents;
  ControlPainter::std->calc_container_size(context->canvas, SliderHorzThumb, &size);
  
  thumb_width = size.width;
  _extents.ascent  = size.ascent;
  _extents.descent = size.descent;
  
  size = _extents;
  ControlPainter::std->calc_container_size(context->canvas, SliderHorzChannel, &size);
  channel_width = size.height();
}

void SliderBox::paint(Context *context) {
  if(context->canvas->show_only_text)
    return;
    
  update_dynamic_styles(context);
  
  double old_value = range_value;
  
  have_drawn = true;
  SliderBoxImpl(*this).finish_update_value();
  
  float x, y;
  context->canvas->current_pos(&x, &y);
  y -= _extents.ascent;
  
  SliderBoxImpl(*this).paint_error_indicator_if_necessary(context->canvas, x, y);
  SliderBoxImpl(*this).paint_channel(context->canvas, x, y);
  SliderBoxImpl(*this).animate_thumb(context, x, y, old_value);
}

Expr SliderBox::to_pmath_symbol() {
  return Symbol(richmath_System_SliderBox);
}

Expr SliderBox::to_pmath(BoxOutputFlags flags) {
  Expr val = dynamic.expr();
  
  if(has(flags, BoxOutputFlags::Literal) && dynamic.is_dynamic())
    val = val[1];
    
  return Call(
           Symbol(richmath_System_SliderBox),
           val,
           range);
}

Box *SliderBox::mouse_selection(
  float  x,
  float  y,
  int   *start,
  int   *end,
  bool  *was_inside_start
) {
  *was_inside_start = true;
  *start = *end = 0;
  return this;
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

Box *SliderBox::dynamic_to_literal(int *start, int *end) {
  if(dynamic.is_dynamic())
    dynamic = dynamic.expr()[1];
  return this;
}

void SliderBox::on_mouse_exit() {
  EmptyWidgetBox::on_mouse_exit();
}

void SliderBox::on_mouse_down(MouseEvent &event) {
  EmptyWidgetBox::on_mouse_down(event);
  
  if(mouse_left_down) {
    event.set_origin(this);
    
    if(dynamic.is_dynamic())
      Application::activated_control(this);
      
    double val = SliderBoxImpl(*this).mouse_to_val(event.x);
    
    if(!SliderBoxImpl(*this).approximately_equals(val, range_value)) {
      if(get_own_style(ContinuousAction, true)) 
        SliderBoxImpl(*this).assign_dynamic_value(val);
      else
        range_value = val;
    }
  }
}

void SliderBox::on_mouse_move(MouseEvent &event) {
  EmptyWidgetBox::on_mouse_move(event);
  
  event.set_origin(this);
  
  if(mouse_left_down) {
    double val = SliderBoxImpl(*this).mouse_to_val(event.x);
    
    if(!SliderBoxImpl(*this).approximately_equals(val, range_value)) {
      if(get_own_style(ContinuousAction, true)) 
        SliderBoxImpl(*this).assign_dynamic_value(val);
      else
        range_value = val;
        
      request_repaint_all();
    }
  }
  else {
    float tx = SliderBoxImpl(*this).calc_thumb_pos(range_value);
    
    bool old_mot = mouse_over_thumb;
    mouse_over_thumb = (tx <= event.x && event.x <= tx + thumb_width);
    
    if(old_mot != mouse_over_thumb)
      request_repaint_all();
  }
}

void SliderBox::on_mouse_up(MouseEvent &event) {
  if(event.left) {
    event.set_origin(this);
    double val = SliderBoxImpl(*this).mouse_to_val(event.x);
    if( !SliderBoxImpl(*this).approximately_equals(val, range_value) ||
        dynamic.synchronous_updating() == 2 ||
        !get_own_style(ContinuousAction, true))
    {
      SliderBoxImpl(*this).assign_dynamic_value(val);
    }
    
    Application::deactivated_control(this);
  }
  
  EmptyWidgetBox::on_mouse_up(event);
}

/*void SliderBox::on_mouse_cancel() {
  if(auto doc = find_parent<Document>(false))
    doc->native()->beep();

  EmptyWidgetBox::on_mouse_cancel();
}*/

//} ... class SliderBox
