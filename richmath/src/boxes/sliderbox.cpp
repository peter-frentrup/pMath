#include <boxes/sliderbox.h>
#include <eval/client.h>
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
  static double make_nan(){
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
: Box(),
  range_min(0.0),
  range_max(1.0),
  range_step(0.0),
  range_value(0.5),
  old_thumb_state(Normal),
  new_thumb_state(Normal),
  thumb_width(1),
  channel_width(1),
  must_update(true),
  have_drawn(false),
  mouse_down(false),
  use_double_values(true)
{
  dynamic.init(this, Expr());
}

SliderBox::~SliderBox(){
}

SliderBox *SliderBox::create(Expr expr){
  if(expr.expr_length() < 2)
    return 0;
  
  Expr options = Expr(pmath_options_extract(expr.get(), 2));
    
  if(options.is_null())
    return 0;
  
  SliderBox *sb = new SliderBox();
  sb->dynamic = expr[1];
  
  sb->style = new Style(options);
  
  sb->range = expr[2];
  if(sb->range.expr_length() == 2
  && sb->range[0] == PMATH_SYMBOL_RANGE){
    sb->range_min = sb->range[1].to_double(NAN);
    sb->range_max = sb->range[2].to_double(NAN);
    
    if(isnan(sb->range_min) || isnan(sb->range_max)){
      delete sb;
      return 0;
    }
  }
  else if(sb->range.expr_length() == 3
  &&      sb->range[0] == PMATH_SYMBOL_RANGE){
    sb->range_min  = sb->range[1].to_double(NAN);
    sb->range_max  = sb->range[2].to_double(NAN);
    sb->range_step = sb->range[3].to_double(0);
    
    if(isnan(sb->range_min) || isnan(sb->range_max) || sb->range_step == 0){
      delete sb;
      return 0;
    }
    
    sb->use_double_values = !Evaluate(
      Divide(
        Minus(sb->range[2], sb->range[1]), 
        sb->range[3])
      ).instance_of(PMATH_TYPE_RATIONAL);
  }
  else if(sb->range.expr_length() > 0
  &&      sb->range[0] == PMATH_SYMBOL_LIST){
    sb->range_min = 1;
    sb->range_max = sb->range.expr_length();
    sb->range_step = 1;
  }
  else{
    delete sb;
    return 0;
  }
  
  return sb;
}

void SliderBox::resize(Context *context){
  float em = context->canvas->get_font_size();
  _extents.ascent  = 0.75 * em * 1.5;
  _extents.descent = 0.25 * em * 1.5;
  _extents.width   = 6 * em * 1.5;
  
  BoxSize size = _extents;
  ControlPainter::std->calc_container_size(context->canvas, SliderHorzThumb, &size);
  
  thumb_width = size.width;
  
  size = _extents;
  ControlPainter::std->calc_container_size(context->canvas, SliderHorzChannel, &size);
  channel_width = size.height();
}

void SliderBox::paint(Context *context){
  if(context->canvas->show_only_text)
    return;
  
  have_drawn = true;
  
  double old_value = range_value;
  
  if(must_update){
    must_update = false;
    
    Expr val;
    if(dynamic.get_value(&val)){
      if(range[0] == PMATH_SYMBOL_LIST){
        range_value = range_min;
        
        size_t i;
        for(i = 1;i <= range.expr_length();++i)
          if(range[i] == val){
            range_value = i;
            break;
          }
      }
      else{
        range_value = val.to_double(NAN);
      }
    }
  }
  
  float x, y;
  context->canvas->current_pos(&x, &y);
  
  y-= _extents.ascent;
  float h = _extents.height();
  
  float thumb_x = x + calc_thumb_pos(range_value);
  
  if(isnan(range_value)){
    float rx = x + _extents.width/2;
    
    context->canvas->save();
    context->canvas->set_color(0xFF0000, 0.2);
    for(int i = -2;i <= 2;++i){
      context->canvas->arc(rx + i * h/6, y + h/2, h/2, 0, 2 * M_PI, false);
      context->canvas->fill();
    }
    context->canvas->restore();
  }
  if(range_value < range_min && range_value < range_max){
    float rx = x + h/2;
    
    context->canvas->save();
    context->canvas->set_color(0xFF0000, 0.2);
    for(int i = 0;i <= 2;++i){
      context->canvas->arc(rx + i * h/6, y + h/2, h/2, 0, 2 * M_PI, false);
      context->canvas->fill();
    }
    context->canvas->restore();
  }
  else if(range_value > range_max && range_value > range_min){
    float rx = x + _extents.width - h/2;
    
    context->canvas->save();
    context->canvas->set_color(0xFF0000, 0.2);
    for(int i = -2;i <= 0;++i){
      context->canvas->arc(rx + i * h/6, y + h/2, h/2, 0, 2 * M_PI, false);
      context->canvas->fill();
    }
    context->canvas->restore();
  }
  
  ControlPainter::std->draw_container(
    context->canvas,
    SliderHorzChannel, 
    Normal, 
    x, 
    y + h/2 - channel_width/2, 
    _extents.width, 
    channel_width);
  
  if(old_value == range_value){
    if(new_thumb_state != old_thumb_state || !animation){
      animation = ControlPainter::std->control_transition(
        id(),
        context->canvas,
        SliderHorzThumb,
        old_thumb_state,
        new_thumb_state,
        thumb_x,
        y,
        thumb_width,
        h);
      
      old_thumb_state = new_thumb_state;
    }

    if(animation){
      if(animation->paint(context->canvas))
        return;
      
      animation = ControlPainter::std->control_transition(
        id(),
        context->canvas,
        SliderHorzThumb,
        new_thumb_state,
        new_thumb_state,
        thumb_x,
        y,
        thumb_width,
        h);
    }
  }
  
  ControlPainter::std->draw_container(
    context->canvas,
    SliderHorzThumb, 
    new_thumb_state, 
    thumb_x, 
    y, 
    thumb_width, 
    h);
}

float SliderBox::calc_thumb_pos(double val){
  if(isnan(val))
    return _extents.width/2 - thumb_width/2;
  
  if(range_min < range_max){
    if(val < range_min)
      return 0;
      
    if(val > range_max)
      return _extents.width - thumb_width;
      
    return (val - range_min) / (range_max - range_min) * (_extents.width - thumb_width);
  }
  
  if(range_min > range_max){
    if(val < range_max)
      return 0;
      
    if(val > range_min)
      return _extents.width - thumb_width;
      
    return (val - range_min) / (range_max - range_min) * (_extents.width - thumb_width);
  }
  
  return _extents.width/2 - thumb_width/2;
}

double SliderBox::mouse_to_val(double mouse_x){
  mouse_x-= thumb_width / 2;
  if(mouse_x < 0)
     mouse_x = 0;
  if(mouse_x > _extents.width - thumb_width)
     mouse_x = _extents.width - thumb_width;
  
  double val;
  
  if(range_min != range_max){
    val = (mouse_x / (_extents.width - thumb_width)) * (range_max - range_min);
    
    if(range_step != 0){
      val = range_min + floor(val / range_step + 0.5) * range_step;
      
      if(range_min < range_max){
        if(val > range_max)
          val-= range_step;
      }
      else{
        if(val < range_max)
          val+= range_step;
      }
    }
    else
      val+= range_min;
  }
  else
    val = range_min;
  
  return val;
}

Expr SliderBox::to_pmath(bool parseable){
  return Call(
    Symbol(PMATH_SYMBOL_SLIDERBOX),
    dynamic.expr(),
    range);
}

Box *SliderBox::mouse_selection(
  float  x,
  float  y,
  int   *start,
  int   *end,
  bool  *was_inside_start
){
  *was_inside_start = true;
  *start = *end = 0;
  return this;
}

void SliderBox::dynamic_updated(){
  if(must_update)
    return;
  
  must_update = true;
  request_repaint_all();
}

void SliderBox::dynamic_finished(Expr info, Expr result){
  double new_value = result.to_double(NAN);
  
  if(range_value != new_value)
    request_repaint_all();
}

void SliderBox::assign_dynamic_value(double d){
  if(!have_drawn)
    return;
  
  have_drawn = false;
  if(range[0] == PMATH_SYMBOL_LIST){
    dynamic.assign(range[(size_t)d]);
  }
  else if(use_double_values){
    dynamic.assign(d);
  }
  else{
    dynamic.assign(Plus(range[1], Round(Minus(d, range[1]), range[3])));
  }
}

void SliderBox::on_mouse_enter(){
}

void SliderBox::on_mouse_exit(){
  if(!mouse_down && new_thumb_state != Normal){
    new_thumb_state = Normal;
    request_repaint_all();
  }
}

void SliderBox::on_mouse_down(MouseEvent &event){
  if(event.left){
    mouse_down = true;
    event.set_source(this);
    
    double val = mouse_to_val(event.x);
    
    assign_dynamic_value(val);
    new_thumb_state = Pressed;
    request_repaint_all();
  }
}

void SliderBox::on_mouse_move(MouseEvent &event){
  Document *doc = find_parent<Document>(false);
  
  if(doc && doc->native()){
    doc->native()->set_cursor(DefaultCursor);
  }
  
  if(event.left){
    event.set_source(this);
    double val = mouse_to_val(event.x);
    if(val != range_value){
      assign_dynamic_value(val);
      request_repaint_all();
    }
  }
  
  if(!mouse_down){
    event.set_source(this);
    float tx = calc_thumb_pos(range_value);
    
    ControlState old_state = new_thumb_state;
    if(tx <= event.x && event.x <= tx + thumb_width)
      new_thumb_state = Hovered;
    else
      new_thumb_state = Normal;
    
    if(old_state != new_thumb_state)
      request_repaint_all();
  }
}

void SliderBox::on_mouse_up(MouseEvent &event){
  if(event.left){
    mouse_down = false;
    new_thumb_state = Normal;
    
    event.set_source(this);
    double val = mouse_to_val(event.x);
    if(val != range_value)
      assign_dynamic_value(val);
    
    request_repaint_all();
  }
}

//} ... class SliderBox
