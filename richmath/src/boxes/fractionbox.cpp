#include <boxes/fractionbox.h>

#include <cmath>

#include <boxes/mathsequence.h>
#include <graphics/context.h>

using namespace richmath;

extern pmath_symbol_t richmath_System_FractionBox;

//{ class FractionBox ...

FractionBox::FractionBox()
  : Box(),
    _numerator(new MathSequence),
    _denominator(new MathSequence)
{
  adopt(_numerator, 0);
  adopt(_denominator, 1);
}

FractionBox::FractionBox(MathSequence *num, MathSequence *den)
  : Box(),
    _numerator(num),
    _denominator(den)
{
  if(!_numerator)
    _numerator = new MathSequence;
  if(!_denominator)
    _denominator = new MathSequence;
    
  adopt(_numerator,   0);
  adopt(_denominator, 1);
}

FractionBox::~FractionBox() {
  delete_owned(_numerator);
  delete_owned(_denominator);
}

bool FractionBox::try_load_from_object(Expr expr, BoxInputFlags opts) {
  if(expr[0] != richmath_System_FractionBox)
    return false;
    
  if(expr.expr_length() != 2)
    return false;
    
  /* now success is guaranteed */
  
  _numerator->load_from_object(  expr[1], opts);
  _denominator->load_from_object(expr[2], opts);
  
  finish_load_from_object(std::move(expr));
  return true;
}

Box *FractionBox::item(int i) {
  if(i == 0)
    return _numerator;
  return _denominator;
}

void FractionBox::resize(Context &context) {
  float old_width         = context.width;
  float old_fs            = context.canvas().get_font_size();
  int   old_script_indent = context.script_indent;
  
  if(context.smaller_fraction_parts) {
    context.script_indent++;
  }
  
  context.canvas().set_font_size(context.get_script_size(old_fs));
  
  context.width = HUGE_VAL;
  _numerator->resize(context);
  _denominator->resize(context);
  context.width = old_width;
  
  context.canvas().set_font_size(old_fs);
  
  context.math_shaper->shape_fraction(
    context,
    _numerator->extents(),
    _denominator->extents(),
    &num_y,
    &den_y,
    &_extents.width);
    
  _extents.ascent  = _numerator->extents().ascent    - num_y;
  _extents.descent = _denominator->extents().descent + den_y;
  
  context.script_indent = old_script_indent;
}

void FractionBox::paint(Context &context) {
  update_dynamic_styles(context);
  
  float old_fs = context.canvas().get_font_size();
  float x, y;
  context.canvas().current_pos(&x, &y);
  
  context.math_shaper->show_fraction(context, _extents.width);
  
  context.canvas().move_to(
    x + (_extents.width - _numerator->extents().width) / 2,
    y + num_y);
    
  context.canvas().set_font_size(_numerator->get_em());
  _numerator->paint(context);
  
  context.canvas().move_to(
    x + (_extents.width - _denominator->extents().width) / 2,
    y + den_y);
    
  context.canvas().set_font_size(_denominator->get_em());
  _denominator->paint(context);
  
  context.canvas().set_font_size(old_fs);
}

Box *FractionBox::remove(int *index) {
  if(MathSequence *seq = dynamic_cast<MathSequence*>(parent())) {
    if(*index == 0 && _numerator->length() == 0) {
      *index = _index;
      seq->insert(_index + 1, _denominator, 0, _denominator->length());
      return seq->remove(index);
    }
    
    if(*index == 1 && _denominator->length() == 0) {
      *index = _index + _numerator->length();
      seq->insert(_index + 1, _numerator, 0, _numerator->length());
      seq->remove(_index, _index + 1);
      return seq;
    }
  }
  
  return move_logical(LogicalDirection::Backward, false, index);
}

Expr FractionBox::to_pmath_symbol() {
  return Symbol(richmath_System_FractionBox);
}

Expr FractionBox::to_pmath(BoxOutputFlags flags) {
  return Call(
           Symbol(richmath_System_FractionBox),
           _numerator->to_pmath(flags),
           _denominator->to_pmath(flags));
}

Box *FractionBox::move_vertical(
  LogicalDirection  direction,
  float            *index_rel_x,
  int              *index,
  bool              called_from_child
) {
  MathSequence *dst = 0;
  
  if(*index < 0) {
    if(direction == LogicalDirection::Forward)
      dst = _numerator;
    else
      dst = _denominator;
  }
  else if(*index == 0) { // comming from numerator
    *index_rel_x += (_extents.width - _numerator->extents().width) / 2;
    
    if(direction == LogicalDirection::Forward)
      dst = _denominator;
  }
  else { // comming from denominator
    *index_rel_x += (_extents.width - _denominator->extents().width) / 2;
    
    if(direction == LogicalDirection::Backward)
      dst = _numerator;
  }
  
  if(!dst) {
    if(auto par = parent()) {
      *index = _index;
      return par->move_vertical(direction, index_rel_x, index, true);
    }
    
    return this;
  }
  
  *index_rel_x -= (_extents.width - dst->extents().width) / 2;
  *index = -1;
  return dst->move_vertical(direction, index_rel_x, index, false);
}

VolatileSelection FractionBox::mouse_selection(Point pos, bool *was_inside_start) {
  if(auto par = parent()) {
    float cw = _numerator->extents().width;
    if(cw < _denominator->extents().width)
      cw = _denominator->extents().width;
      
    if(pos.x < (_extents.width - cw) / 4) {
      *was_inside_start = false;
      return { par, _index, _index };
    }
    
    if(pos.x > (3 * _extents.width + cw) / 4) {
      *was_inside_start = false;
      return { par, _index + 1, _index + 1 };
    }
  }
  
  if(pos.y < num_y + _numerator->extents().descent + den_y - _denominator->extents().ascent) {
    pos.x -= (_extents.width - _numerator->extents().width) / 2;
    pos.y -= num_y;
    return _numerator->mouse_selection(pos, was_inside_start);
  }
  
  pos.x -= (_extents.width - _denominator->extents().width) / 2;
  pos.y -= den_y;
  return _denominator->mouse_selection(pos, was_inside_start);
}

void FractionBox::child_transformation(
  int             index,
  cairo_matrix_t *matrix
) {
  if(index == 0) {
    cairo_matrix_translate(matrix,
                           (_extents.width - _numerator->extents().width) / 2,
                           num_y);
  }
  else {
    cairo_matrix_translate(matrix,
                           (_extents.width - _denominator->extents().width) / 2,
                           den_y);
  }
}

//} ... class FractionBox
