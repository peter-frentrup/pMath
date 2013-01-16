#include <boxes/sliderbox.h>
#include <eval/application.h>
#include <gui/document.h>
#include <gui/native-widget.h>

#include <cmath>

using namespace richmath;
using namespace std;

#ifdef _MSC_VER

#define isnan  _isnan

#endif

#ifndef NAN

#define NAN  (make_nan())
static double make_nan() {
  union {
    uint64_t i;
    double   d;
  } u;
  
  u.i = 0x7ff8000000000000ULL;
  
  assert(isnan(u.d));
  assert(isnan((float)u.d));
  
  return u.d;
}

#endif

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

bool SliderBox::try_load_from_object(Expr expr, int opts) {
  if(expr[0] != PMATH_SYMBOL_SLIDERBOX)
    return false;
    
  if(expr.expr_length() < 2)
    return false;
    
  Expr options = Expr(pmath_options_extract(expr.get(), 2));
  
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
    
  if(isnan(new_range_min))
    return false;
    
  if(isnan(new_range_max))
    return false;
    
  if(isnan(new_range_step))
    return false;
    
  /* now success is guaranteed */
  
  if(dynamic.expr() != expr[1]) {
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
    
  if(style)
    style->update_dynamic(this);
    
  have_drawn = true;
  
  double old_value = range_value;
  
  if(must_update) {
    must_update = false;
    
    Expr val;
    if(dynamic.get_value(&val)) {
      if(range[0] == PMATH_SYMBOL_LIST) {
        range_value = range_min;
        
        size_t i;
        for(i = 1; i <= range.expr_length(); ++i)
          if(range[i] == val) {
            range_value = i;
            break;
          }
      }
      else {
        range_value = val.to_double(NAN);
      }
    }
  }
  
  float x, y;
  context->canvas->current_pos(&x, &y);
  
  y -= _extents.ascent;
  float h = _extents.height();
  
  float thumb_x = x + calc_thumb_pos(range_value);
  
  if(isnan(range_value)) {
    float rx = x + _extents.width / 2;
    
    int old_color = context->canvas->get_color();
    context->canvas->save();
    context->canvas->set_color(0xFF0000, 0.2);
    for(int i = -2; i <= 2; ++i) {
      context->canvas->arc(rx + i * h / 6, y + h / 2, h / 2, 0, 2 * M_PI, false);
      context->canvas->fill();
    }
    context->canvas->restore();
    context->canvas->set_color(old_color);
  }
  else if(range_value < range_min && range_value < range_max) {
    float rx = x + h / 2;
    
    int old_color = context->canvas->get_color();
    context->canvas->save();
    context->canvas->set_color(0xFF0000, 0.2);
    for(int i = 0; i <= 2; ++i) {
      context->canvas->arc(rx + i * h / 6, y + h / 2, h / 2, 0, 2 * M_PI, false);
      context->canvas->fill();
    }
    context->canvas->restore();
    context->canvas->set_color(old_color);
  }
  else if(range_value > range_max && range_value > range_min) {
    float rx = x + _extents.width - h / 2;
    
    int old_color = context->canvas->get_color();
    context->canvas->save();
    context->canvas->set_color(0xFF0000, 0.2);
    for(int i = -2; i <= 0; ++i) {
      context->canvas->arc(rx + i * h / 6, y + h / 2, h / 2, 0, 2 * M_PI, false);
      context->canvas->fill();
    }
    context->canvas->restore();
    context->canvas->set_color(old_color);
  }
  
  ControlPainter::std->draw_container(
    context->canvas,
    SliderHorzChannel,
    Normal,
    x,
    y + h / 2 - channel_width / 2,
    _extents.width,
    channel_width);
    
  ControlState new_state = calc_state(context);
  
  if(old_value == range_value) {
    if(new_state != old_state || !animation) {
      animation = ControlPainter::std->control_transition(
                    id(),
                    context->canvas,
                    SliderHorzThumb,
                    SliderHorzThumb,
                    old_state,
                    new_state,
                    thumb_x,
                    y,
                    thumb_width,
                    h);
                    
      old_state = new_state;
    }
    
    if(animation) {
      if(animation->paint(context->canvas))
        return;
        
      animation = ControlPainter::std->control_transition(
                    id(),
                    context->canvas,
                    SliderHorzThumb,
                    SliderHorzThumb,
                    new_state,
                    new_state,
                    thumb_x,
                    y,
                    thumb_width,
                    h);
    }
  }
  
  ControlPainter::std->draw_container(
    context->canvas,
    SliderHorzThumb,
    new_state,
    thumb_x,
    y,
    thumb_width,
    h);
}

float SliderBox::calc_thumb_pos(double val) {
  if(isnan(val))
    return _extents.width / 2 - thumb_width / 2;
    
  if(range_min < range_max) {
    if(val < range_min)
      return 0;
      
    if(val > range_max)
      return _extents.width - thumb_width;
      
    return (val - range_min) / (range_max - range_min) * (_extents.width - thumb_width);
  }
  
  if(range_min > range_max) {
    if(val < range_max)
      return 0;
      
    if(val > range_min)
      return _extents.width - thumb_width;
      
    return (val - range_min) / (range_max - range_min) * (_extents.width - thumb_width);
  }
  
  return _extents.width / 2 - thumb_width / 2;
}

double SliderBox::mouse_to_val(double mouse_x) {
  mouse_x -= thumb_width / 2;
  if(mouse_x < 0)
    mouse_x = 0;
  if(mouse_x > _extents.width - thumb_width)
    mouse_x = _extents.width - thumb_width;
    
  double val;
  
  if(range_min != range_max) {
    val = (mouse_x / (_extents.width - thumb_width)) * (range_max - range_min);
    
    if(range_step != 0) {
      val = range_min + floor(val / range_step + 0.5) * range_step;
      
      if(range_min < range_max) {
        if(val > range_max)
          val -= range_step;
      }
      else {
        if(val < range_max)
          val += range_step;
      }
    }
    else
      val += range_min;
  }
  else
    val = range_min;
    
  return val;
}

Expr SliderBox::to_pmath(int flags) {
  Expr val = dynamic.expr();
  
  if((flags & BoxFlagLiteral) && dynamic.is_dynamic())
    val = val[1];
    
  return Call(
           Symbol(PMATH_SYMBOL_SLIDERBOX),
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

void SliderBox::assign_dynamic_value(double d) {
  if(!have_drawn)
    return;
    
  have_drawn = false;
  if(range[0] == PMATH_SYMBOL_LIST) {
    dynamic.assign(range[(size_t)d]);
  }
  else if(use_double_values) {
    dynamic.assign(d);
  }
  else {
    dynamic.assign(Plus(range[1], Round(Minus(d, range[1]), range[3])));
  }
}

void SliderBox::on_mouse_exit() {
  EmptyWidgetBox::on_mouse_exit();
}

void SliderBox::on_mouse_down(MouseEvent &event) {
  EmptyWidgetBox::on_mouse_down(event);
  
  if(mouse_left_down) {
    event.set_source(this);
    
    if(dynamic.is_dynamic())
      Application::activated_control(this);
    
    double val = mouse_to_val(event.x);
    
    if(val != range_value && get_own_style(ContinuousAction, true))
      assign_dynamic_value(val);
    else
      range_value = val;
  }
}

void SliderBox::on_mouse_move(MouseEvent &event) {
  EmptyWidgetBox::on_mouse_move(event);
  
  event.set_source(this);
  
  if(mouse_left_down) {
    double val = mouse_to_val(event.x);
    
    if(val != range_value) {
      if(get_own_style(ContinuousAction, true))
        assign_dynamic_value(val);
      else
        range_value = val;
        
      request_repaint_all();
    }
  }
  else {
    float tx = calc_thumb_pos(range_value);
    
    bool old_mot = mouse_over_thumb;
    mouse_over_thumb = (tx <= event.x && event.x <= tx + thumb_width);
    
    if(old_mot != mouse_over_thumb)
      request_repaint_all();
  }
}

void SliderBox::on_mouse_up(MouseEvent &event) {
  if(event.left) {
    event.set_source(this);
    double val = mouse_to_val(event.x);
    if( val != range_value                  || 
        dynamic.synchronous_updating() == 2 || 
        !get_own_style(ContinuousAction, true))
    {
      assign_dynamic_value(val);
    }
    
    Application::deactivated_control(this);
  }
  
  EmptyWidgetBox::on_mouse_up(event);
}

/*void SliderBox::on_mouse_cancel() {
  Document *doc = find_parent<Document>(false);
  
  if(doc)
    doc->native()->beep();
  
  EmptyWidgetBox::on_mouse_cancel();
}*/

//} ... class SliderBox
