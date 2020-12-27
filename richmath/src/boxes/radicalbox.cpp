#include <boxes/radicalbox.h>

#include <boxes/mathsequence.h>
#include <graphics/buffer.h>
#include <graphics/context.h>

using namespace richmath;

extern pmath_symbol_t richmath_System_RadicalBox;
extern pmath_symbol_t richmath_System_SqrtBox;

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
  delete_owned(_radicand);
  delete_owned(_exponent);
}

bool RadicalBox::try_load_from_object(Expr expr, BoxInputFlags opts) {
  size_t last_non_opt;
  bool has_exponent;
  
  if(expr[0] == richmath_System_RadicalBox) {
    last_non_opt = 2;
    has_exponent = true;
  }
  else if(expr[0] == richmath_System_SqrtBox) {
    last_non_opt = 1;
    has_exponent = false;
  }
  else
    return false;
  
  if(expr.expr_length() < last_non_opt)
    return false;
 
  Expr options(pmath_options_extract_ex(expr.get(), last_non_opt, PMATH_OPTIONS_EXTRACT_UNKNOWN_WARNONLY));
  if(options.is_null())
    return false;
    
  if(style){
    reset_style();
    style->add_pmath(options);
  }
  else if(options != PMATH_UNDEFINED)
    style = new Style(options);
  
  _radicand->load_from_object(expr[1], opts);
  
  if(has_exponent) {
    if(!_exponent){
      _exponent = new MathSequence;
      adopt(_exponent, 1);
    }
    
    _exponent->load_from_object(expr[2], opts);
  }
  else if(_exponent) {
    _exponent->safe_destroy();
    _exponent = nullptr;
  }
  
  finish_load_from_object(std::move(expr));
  return true;
}

Box *RadicalBox::item(int i) {
  if(i == 0)
    return _radicand;
  return _exponent;
}

int RadicalBox::count() {
  return 1 + (_exponent ? 1 : 0);
}

void RadicalBox::resize(Context &context) {
  _radicand->resize(context);
  
  _extents = _radicand->extents();
  
  context.math_shaper->shape_radical(
    context,
    &_extents,
    &rx,
    &_exponent_offset,
    &info);
  
  info.surd_form = get_own_style(SurdForm, 0) ? 1 : 0;
    
  if(_exponent) {
    float old_fs = context.canvas().get_font_size();
    int old_script_indent = context.script_indent;
    
    /* http://www.ntg.nl/maps/38/04.pdf: LuaTeX sets the radical degree in 
       \scriptscriptstyle 
       Microsoft's Math Input Panel seems to do the same.
     */
    context.script_indent+= 2;
    
    small_em = context.get_script_size(old_fs);
    context.canvas().set_font_size(small_em);
    
    _exponent->resize(context);
    
    context.canvas().set_font_size(old_fs);
    context.script_indent = old_script_indent;
    
    if(_extents.ascent < _exponent->extents().height() - _exponent_offset.y)
      _extents.ascent = _exponent->extents().height() - _exponent_offset.y;
      
    if(_exponent_offset.x < _exponent->extents().width) {
      rx +=             _exponent->extents().width - _exponent_offset.x;
      _extents.width += _exponent->extents().width - _exponent_offset.x;
    }
  }
}

void RadicalBox::paint(Context &context) {
  update_dynamic_styles(context);
    
  float x, y;
  context.canvas().current_pos(&x, &y);
  
  context.canvas().move_to(x + rx, y);
  _radicand->paint(context);
  
  if(_exponent) {
    if(_exponent_offset.x < _exponent->extents().width)
      context.canvas().move_to(x + _exponent->extents().width - _exponent_offset.x, y);
    else
      context.canvas().move_to(x, y);
  }
  else
    context.canvas().move_to(x, y);
    
  context.math_shaper->show_radical(
    context,
    info);
    
  if(_exponent) {
    float old_fs = context.canvas().get_font_size();
    context.canvas().set_font_size(small_em);
    
    if(_exponent_offset.x < _exponent->extents().width) {
      context.canvas().move_to(
        x,
        y + _exponent_offset.y - _exponent->extents().descent);
    }
    else {
      context.canvas().move_to(
        x + _exponent_offset.x - _exponent->extents().width,
        y + _exponent_offset.y - _exponent->extents().descent);
    }
    _exponent->paint(context);
    
    context.canvas().set_font_size(old_fs);
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
  
  if(auto par = parent()) {
    *index = _index;
    if(auto seq = dynamic_cast<MathSequence*>(par)) {
      if(_exponent) {
        if(_radicand->length() > 0)
          return move_logical(LogicalDirection::Backward, false, index);
          
        seq->insert(_index + 1, _exponent, 0, _exponent->length());
      }
      else
        seq->insert(_index + 1, _radicand, 0, _radicand->length());
    }
    return par->remove(index);
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
    return Symbol(richmath_System_RadicalBox);
  
  return Symbol(richmath_System_SqrtBox);
}

Expr RadicalBox::to_pmath(BoxOutputFlags flags) {
  Gather g;
  
  Gather::emit(_radicand->to_pmath(flags));
  if(_exponent)
    Gather::emit(_exponent->to_pmath(flags));
  
  if(style)
    style->emit_to_pmath(false);
  
  Expr result = g.end();
  if(_exponent)
    result.set(0, Symbol(richmath_System_RadicalBox));
  else
    result.set(0, Symbol(richmath_System_SqrtBox));
  
  return result;
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
      if(_exponent_offset.x < _exponent->extents().width)
        er = _exponent->extents().width;
      else
        er = _exponent_offset.x;
        
      if(*index_rel_x < (er + rx) / 2)
        return _exponent->move_vertical(direction, index_rel_x, index, false);
        
    }
    
    *index_rel_x -= rx;
    return _radicand->move_vertical(direction, index_rel_x, index, false);
  }
  
  if(auto par = parent()) {
    if(*index == 0)
      *index_rel_x += rx;
      
    *index = _index;
    return par->move_vertical(direction, index_rel_x, index, true);
  }
  
  return this;
}

VolatileSelection RadicalBox::mouse_selection(Point pos, bool *was_inside_start) {
  if(pos.x > (_extents.width + rx + _radicand->extents().width) / 2) {
    if(auto par = parent()) {
      *was_inside_start = false;
      return { par, _index + 1, _index + 1 };
    }
  }
  
  if(_exponent) {
    float el = 0;
    if(_exponent_offset.x > _exponent->extents().width)
      el = _exponent_offset.x - _exponent->extents().width;
      
    if(el / 2 < pos.x && pos.x < (el + _exponent->extents().width + rx) / 2)
      return _exponent->mouse_selection(
               pos + Vector2F{-el, -_exponent_offset.y + _exponent->extents().descent},
               was_inside_start);
  }
  
  if(pos.x < rx / 2) {
    if(auto par = parent()) {
      *was_inside_start = true;
      return { par, _index, _index };
    }
  }
  
  
  return _radicand->mouse_selection(pos - Vector2F(rx, 0), was_inside_start);
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
    if(_exponent_offset.x > _exponent->extents().width)
      el = _exponent_offset.x - _exponent->extents().width;
      
    cairo_matrix_translate(matrix, el, _exponent_offset.y - _exponent->extents().descent);
  }
}

//} ... class RadicalBox
