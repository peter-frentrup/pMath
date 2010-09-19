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
  min(0.0),
  max(1.0),
  value(0.5),
  old_thumb_state(Normal),
  new_thumb_state(Normal),
  thumb_width(1),
  channel_width(1),
  must_update(true),
  have_drawn(false),
  mouse_down(false)
{
}

SliderBox::~SliderBox(){
}

SliderBox *SliderBox::create(Expr expr){
  if(expr.expr_length() < 2)
    return 0;
  
  Expr options = Expr(pmath_options_extract(expr.get(), 2));
    
  if(!options.is_valid())
    return 0;
  
  SliderBox *sb = new SliderBox();
  sb->dynamic_value = expr[1];
  
  sb->style = new Style(options);
  
  Expr range = expr[2];
  if(range.expr_length() == 2
  && range[0] == PMATH_SYMBOL_RANGE){
    sb->min = range[1].to_double(NAN);
    sb->max = range[2].to_double(NAN);
    
    if(isnan(sb->min) || isnan(sb->max)){
      delete sb;
      return 0;
    }
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
  
  double old_value = value;
  
  if(must_update){
    must_update = false;
    
    Expr val = dynamic_value;
    
    if(val[0] == PMATH_SYMBOL_DYNAMIC
    && val.expr_length() >= 1){
      Expr run = Call(
        Symbol(PMATH_SYMBOL_INTERNAL_DYNAMICEVALUATE),
        val[1],
        this->id());
      val = Client::interrupt(run, Client::dynamic_timeout);
    }
    
    value = val.to_double(NAN);
  }
  
  float x, y;
  context->canvas->current_pos(&x, &y);
  
  y-= _extents.ascent;
  float h = _extents.height();
  
  float thumb_x = x + calc_thumb_pos(value);
  
  if(isnan(value)){
    float rx = x + _extents.width/2;
    
    context->canvas->save();
    context->canvas->set_color(0xFF0000, 0.2);
    for(int i = -2;i <= 2;++i){
      context->canvas->arc(rx + i * h/6, y + h/2, h/2, 0, 2 * M_PI, false);
      context->canvas->fill();
    }
    context->canvas->restore();
  }
  if(value < min && value < max){
    float rx = x + h/2;
    
    context->canvas->save();
    context->canvas->set_color(0xFF0000, 0.2);
    for(int i = 0;i <= 2;++i){
      context->canvas->arc(rx + i * h/6, y + h/2, h/2, 0, 2 * M_PI, false);
      context->canvas->fill();
    }
    context->canvas->restore();
  }
  else if(value > max && value > min){
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
  
  if(old_value == value){
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
  
  if(min < max){
    if(val < min)
      return 0;
      
    if(val > max)
      return _extents.width - thumb_width;
      
    return (val - min) / (max - min) * (_extents.width - thumb_width);
  }
  
  if(min > max){
    if(val < max)
      return 0;
      
    if(val > min)
      return _extents.width - thumb_width;
      
    return (val - min) / (max - min) * (_extents.width - thumb_width);
  }
  
  return _extents.width/2 - thumb_width/2;
}

double SliderBox::mouse_to_val(double mouse_x){
  mouse_x-= thumb_width / 2;
  
  double val;
  if(min != max)
    val = min + (mouse_x / (_extents.width - thumb_width)) * (max - min);
  else
    val = min;
  
  if(min < max){
    if(val < min)
      return min;
    
    if(val > max)
      return max;
  }
  else{
    if(val < max)
      return max;
    
    if(val > min)
      return min;
  }
  
  return val;
}

pmath_t SliderBox::to_pmath(bool parseable){
  return pmath_expr_new_extended(
    pmath_ref(PMATH_SYMBOL_SLIDERBOX), 2,
    pmath_ref(dynamic_value.get()),
    pmath_expr_new_extended(
      pmath_ref(PMATH_SYMBOL_RANGE), 2,
      pmath_build_value("f", min),
      pmath_build_value("f", max)));
}

Box *SliderBox::mouse_selection(
  float x,
  float y,
  int   *start,
  int   *end,
  bool  *eol
){
  *eol = false;
  *start = *end = 0;
  return this;
}

void SliderBox::dynamic_updated(){
  if(must_update)
    return;
  
  must_update = true;
  request_repaint_all();
}

void SliderBox::assign_dynamic_value(double d){
  if(!have_drawn)
    return;
  
  have_drawn = false;
  if(dynamic_value[0] == PMATH_SYMBOL_DYNAMIC){
    Expr run;
    
    if(dynamic_value.expr_length() >= 2) 
      run = Call(dynamic_value[2], Expr(d));
    else
      run = Call(Symbol(PMATH_SYMBOL_ASSIGN), dynamic_value[1], Expr(d));
    
    Client::execute_for(run, this, Client::dynamic_timeout);
    
    return;
  }
  
  value = d;
  dynamic_value = Expr(d);
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
    if(val != value){
      assign_dynamic_value(val);
      request_repaint_all();
    }
  }
  
  if(!mouse_down){
    event.set_source(this);
    float tx = calc_thumb_pos(value);
    
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
    if(val != value)
      assign_dynamic_value(val);
    
    request_repaint_all();
  }
}

//} ... class SliderBox
