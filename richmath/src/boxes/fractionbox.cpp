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
  if(!expr.item_equals(0, richmath_System_FractionBox))
    return false;
    
  if(expr.expr_length() < 2)
    return false;
  
  Expr options_expr(pmath_options_extract_ex(expr.get(), 2, PMATH_OPTIONS_EXTRACT_UNKNOWN_WARNONLY));
  if(options_expr.is_null())
    return false;
  
  /* now success is guaranteed */
  
  reset_style();
  style.add_pmath(options_expr);
    
  _numerator->load_from_object(  expr[1], opts);
  _denominator->load_from_object(expr[2], opts);
  
  finish_load_from_object(PMATH_CPP_MOVE(expr));
  return true;
}

Box *FractionBox::item(int i) {
  if(i == 0)
    return _numerator;
  return _denominator;
}

int FractionBox::child_script_level(int index, const int *opt_ambient_script_level) {
  int ambient_level = Box::child_script_level(0, opt_ambient_script_level);
  
  if(get_own_style(AllowScriptLevelChange, true)) {
    return 1 + ambient_level;
  }
  
  return ambient_level;
}

void FractionBox::resize(Context &context) {
  update_simple_dynamic_styles_on_resize(context);
  
  float old_width         = context.width;
  float old_fs            = context.canvas().get_font_size();
  int   old_script_level = context.script_level;
  
  context.script_level = child_script_level(0, &context.script_level);
  
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
  
  context.script_level = old_script_level;
}

void FractionBox::paint(Context &context) {
  update_dynamic_styles_on_paint(context);
  
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
  if(AbstractSequence *seq = dynamic_cast<AbstractSequence*>(parent())) {
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

Expr FractionBox::to_pmath_impl(BoxOutputFlags flags) {
  Gather g;
  
  Gather::emit(_numerator->to_pmath(flags));
  Gather::emit(_denominator->to_pmath(flags));

  bool with_inherited = true;
  style.emit_to_pmath(with_inherited);
  
  Expr e = g.end();
  e.set(0, Symbol(richmath_System_FractionBox));
  return e;
}

Box *FractionBox::move_vertical(
  LogicalDirection  direction,
  float            *index_rel_x,
  int              *index,
  bool              called_from_child
) {
  MathSequence *dst = nullptr;
  
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
