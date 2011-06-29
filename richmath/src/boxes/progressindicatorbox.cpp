#include <boxes/progressindicatorbox.h>
#include <eval/application.h>
#include <gui/document.h>
#include <gui/native-widget.h>

#include <cmath>
#include <limits>

using namespace richmath;
using namespace std;

#ifdef _MSC_VER
  
  #define isnan  _isnan
  
#endif

#ifndef NAN
  #define NAN numeric_limits<double>::quiet_NaN()
#endif

//{ class ProgressIndicatorBox ...

ProgressIndicatorBox::ProgressIndicatorBox()
: Box(),
  range_min(0.0),
  range_max(1.0),
  range_value(0.5),
  must_update(true),
  have_drawn(false)
{
  dynamic.init(this, Expr());
}

ProgressIndicatorBox::~ProgressIndicatorBox(){
}

ProgressIndicatorBox *ProgressIndicatorBox::create(Expr expr){
  if(expr.expr_length() < 2)
    return 0;
  
  Expr options = Expr(pmath_options_extract(expr.get(), 2));
    
  if(options.is_null())
    return 0;
  
  ProgressIndicatorBox *pi = new ProgressIndicatorBox();
  pi->dynamic = expr[1];
  
  pi->style = new Style(options);
  
  pi->range = expr[2];
  if(pi->range.expr_length() == 2
  && pi->range[0] == PMATH_SYMBOL_RANGE){
    pi->range_min = pi->range[1].to_double(NAN);
    pi->range_max = pi->range[2].to_double(NAN);
    
    if(isnan(pi->range_min) || isnan(pi->range_max)){
      delete pi;
      return 0;
    }
  }
  else{
    delete pi;
    return 0;
  }
  
  return pi;
}

bool ProgressIndicatorBox::expand(const BoxSize &size){
  _extents.width = size.width;
  
  return true;
}

void ProgressIndicatorBox::resize(Context *context){
  float em = context->canvas->get_font_size();
  _extents.ascent  = 0.75 * em * 1.5;
  _extents.descent = 0.25 * em * 1.5;
  _extents.width   = 6 * em * 1.5;
  
  BoxSize size = _extents;
  ControlPainter::std->calc_container_size(context->canvas, ProgressIndicatorBackground, &size);
  //ControlPainter::std->calc_container_size(context->canvas, ProgressIndicatorBar, &size);
}

// TODO: support progress bar animations
void ProgressIndicatorBox::paint(Context *context){
  if(context->canvas->show_only_text)
    return;
  
  have_drawn = true;
  
  if(must_update){
    must_update = false;
    
    Expr val;
    if(dynamic.get_value(&val)){
      range_value = val.to_double(NAN);
    }
  }
  
  float x, y;
  context->canvas->current_pos(&x, &y);
  
  y-= _extents.ascent;
  
  ControlPainter::std->draw_container(
    context->canvas,
    ProgressIndicatorBackground,
    Normal,
    x, y, 
    _extents.width, 
    _extents.height());
  
  double p = 0;
  ControlState state = Normal;
  
  if(range_min <= range_value && range_value <= range_max && range_max - range_min > 0){
    p = (range_value - range_min) / (range_max - range_min);
  }
  else if(range_value > range_max){
    p = 1;
  }
  else{
    p = 0;
  }
  
  BoxSize content_size = _extents;
  ControlPainter::std->calc_container_size(
    context->canvas, 
    ProgressIndicatorBar,
    &content_size);
  
  ControlPainter::std->draw_container(
    context->canvas,
    ProgressIndicatorBar,
    state,
    x + (_extents.width - content_size.width) / 2, 
    y + _extents.ascent - content_size.ascent, 
    content_size.width * p, 
    content_size.height());
}

Expr ProgressIndicatorBox::to_pmath(int flags){
  Expr val = dynamic.expr();
  
  if((flags & BoxFlagLiteral) && dynamic.is_dynamic())
    val = val[1];
  
  return Call(
    Symbol(PMATH_SYMBOL_PROGRESSINDICATORBOX),
    val,
    range);
}

Box *ProgressIndicatorBox::mouse_selection(
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

void ProgressIndicatorBox::dynamic_updated(){
  if(must_update)
    return;
  
  must_update = true;
  request_repaint_all();
}

void ProgressIndicatorBox::dynamic_finished(Expr info, Expr result){
  double new_value = result.to_double(NAN);
  
  if(range_value != new_value)
    request_repaint_all();
}

Box *ProgressIndicatorBox::dynamic_to_literal(int *start, int *end){
  if(dynamic.is_dynamic())
    dynamic = dynamic.expr()[1];
  return this;
}

void ProgressIndicatorBox::on_mouse_move(MouseEvent &event){
  Document *doc = find_parent<Document>(false);
  
  if(doc && doc->native()){
    doc->native()->set_cursor(DefaultCursor);
  }
}

//} ... class ProgressIndicatorBox
