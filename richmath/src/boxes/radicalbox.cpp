#include <boxes/radicalbox.h>

#include <boxes/mathsequence.h>
#include <graphics/buffer.h>
#include <graphics/context.h>

using namespace richmath;

//{ class RadicalBox ...
RadicalBox::RadicalBox(MathSequence *radicand, MathSequence *exponent)
  : Box(),
  _radicand(radicand),
  _exponent(exponent)
{
  if(!_radicand)
    _radicand = new MathSequence;
  adopt(_radicand, 0);
  if(_exponent)
    adopt(_exponent, 1);
}

RadicalBox::~RadicalBox() {
  delete _radicand;
  delete _exponent;
}

bool RadicalBox::try_load_from_object(Expr expr, BoxInputFlags opts){
  if(expr[0] == PMATH_SYMBOL_RADICALBOX){
    if(expr.expr_length() != 2)
      return false;
    
    _radicand->load_from_object(expr[1], opts);
    
    if(!_exponent){
      _exponent = new MathSequence;
      adopt(_exponent, 1);
    }
    
    _exponent->load_from_object(expr[2], opts);
    
    return true;
  }
  
  if(expr[0] == PMATH_SYMBOL_SQRTBOX){
    if(expr.expr_length() != 1)
      return false;
    
    if(_exponent){
      _exponent->safe_destroy();
      _exponent = nullptr;
    }
    
    _radicand->load_from_object(expr[1], opts);
    
    return true;
  }
  
  return false;
}

Box *RadicalBox::item(int i) {
  if(i == 0)
    return _radicand;
  return _exponent;
}

int RadicalBox::count() {
  return 1 + (_exponent ? 1 : 0);
}

void RadicalBox::resize(Context *context) {
  _radicand->resize(context);
  
  _extents = _radicand->extents();
  
  context->math_shaper->shape_radical(
    context,
    &_extents,
    &rx,
    &ex,
    &ey,
    &info);
    
  if(_exponent) {
    float old_fs = context->canvas->get_font_size();
    int old_script_indent = context->script_indent;
    
    /* http://www.ntg.nl/maps/38/04.pdf: LuaTeX sets the radical degree in 
       \scriptscriptstyle 
       Microsoft's Math Input Panel seems to do the same.
     */
    context->script_indent+= 2;
    
    small_em = context->get_script_size(old_fs);
    context->canvas->set_font_size(small_em);
    
    _exponent->resize(context);
    
    context->canvas->set_font_size(old_fs);
    context->script_indent = old_script_indent;
    
    if(_extents.ascent < _exponent->extents().height() - ey)
      _extents.ascent = _exponent->extents().height() - ey;
      
    if(ex < _exponent->extents().width) {
      rx +=             _exponent->extents().width - ex;
      _extents.width += _exponent->extents().width - ex;
    }
  }
}

void RadicalBox::paint(Context *context) {
  if(style)
    style->update_dynamic(this);
    
  float x, y;
  context->canvas->current_pos(&x, &y);
  
  context->canvas->move_to(x + rx, y);
  _radicand->paint(context);
  
  if(_exponent) {
    if(ex < _exponent->extents().width)
      context->canvas->move_to(x + _exponent->extents().width - ex, y);
    else
      context->canvas->move_to(x, y);
  }
  else
    context->canvas->move_to(x, y);
    
  context->math_shaper->show_radical(
    context,
    info);
    
  if(_exponent) {
    float old_fs = context->canvas->get_font_size();
    context->canvas->set_font_size(small_em);
    
    if(ex < _exponent->extents().width)
      context->canvas->move_to(
        x,
        y + ey - _exponent->extents().descent);
    else
      context->canvas->move_to(
        x + ex - _exponent->extents().width,
        y + ey - _exponent->extents().descent);
    _exponent->paint(context);
    
    context->canvas->set_font_size(old_fs);
  }
}

Box *RadicalBox::remove(int *index) {
  if(_exponent && *index == 1) {
    if(_exponent->length() == 0) {
      _exponent->safe_destroy();
      _exponent = nullptr;
      invalidate();
    }
    return move_logical(LogicalDirection::Backward, false, index);
  }
  
  if(_parent) {
    *index = _index;
    if(auto seq = dynamic_cast<MathSequence*>(_parent)) {
      if(_exponent) {
        if(_radicand->length() > 0)
          return move_logical(LogicalDirection::Backward, false, index);
          
        seq->insert(_index + 1, _exponent, 0, _exponent->length());
      }
      else
        seq->insert(_index + 1, _radicand, 0, _radicand->length());
    }
    return _parent->remove(index);
  }
  
  *index = 0;
  return _radicand;
}

void RadicalBox::complete() {
  if(!_exponent) {
    _exponent = new MathSequence;
    adopt(_exponent, 1);
  }
}

Expr RadicalBox::to_pmath_symbol(){
  if(_exponent)
    return Symbol(PMATH_SYMBOL_RADICALBOX);
  
  return Symbol(PMATH_SYMBOL_SQRTBOX);
}

Expr RadicalBox::to_pmath(BoxOutputFlags flags) {
  if(_exponent)
    return Call(
             Symbol(PMATH_SYMBOL_RADICALBOX),
             _radicand->to_pmath(flags),
             _exponent->to_pmath(flags));
             
  return Call(
           Symbol(PMATH_SYMBOL_SQRTBOX),
           _radicand->to_pmath(flags));
}

Box *RadicalBox::move_vertical(
  LogicalDirection  direction,
  float            *index_rel_x,
  int              *index,
  bool              called_from_child
) {
  if(*index < 0) {
    if(_exponent) {
      float er;
      if(ex < _exponent->extents().width)
        er = _exponent->extents().width;
      else
        er = ex;
        
      if(*index_rel_x < (er + rx) / 2)
        return _exponent->move_vertical(direction, index_rel_x, index, false);
        
    }
    
    *index_rel_x -= rx;
    return _radicand->move_vertical(direction, index_rel_x, index, false);
  }
  
  if(!_parent)
    return this;
    
  if(*index == 0)
    *index_rel_x += rx;
    
  *index = _index;
  return _parent->move_vertical(direction, index_rel_x, index, true);
}

Box *RadicalBox::mouse_selection(
  float  x,
  float  y,
  int   *start,
  int   *end,
  bool  *was_inside_start
) {
  if(_parent && x > (_extents.width + rx + _radicand->extents().width) / 2) {
    *start = *end = _index + 1;
    *was_inside_start = false;
    return _parent;
  }
  
  if(_exponent) {
    float el = 0;
    if(ex > _exponent->extents().width)
      el = ex - _exponent->extents().width;
      
    if(x > el / 2 && x < (el + _exponent->extents().width + rx) / 2)
      return _exponent->mouse_selection(
               x - el,
               y - ey + _exponent->extents().descent,
               start, end, was_inside_start);
  }
  
  if(_parent && x < rx / 2) {
    *start = *end = _index;
    *was_inside_start = true;
    return _parent;
  }
  
  
  return _radicand->mouse_selection(x - rx, y, start, end, was_inside_start);
}

void RadicalBox::child_transformation(
  int             index,
  cairo_matrix_t *matrix
) {
  if(index == 0) {
    cairo_matrix_translate(matrix, rx, 0);
  }
  else {
    float el = 0;
    if(ex > _exponent->extents().width)
      el = ex - _exponent->extents().width;
      
    cairo_matrix_translate(matrix, el, ey - _exponent->extents().descent);
  }
}

//} ... class RadicalBox
