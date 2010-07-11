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

RadicalBox::~RadicalBox(){
  delete _radicand;
  delete _exponent;
}

Box *RadicalBox::item(int i){
  if(i == 0)
    return _radicand;
  return _exponent;
}

int RadicalBox::count(){
  return 1 + (_exponent ? 1 : 0);
}

void RadicalBox::resize(Context *context){
  _radicand->resize(context);
  
  _extents = _radicand->extents();
  
  context->math_shaper->shape_radical(
    context,
    &_extents,
    &rx,
    &ex,
    &ey,
    &info);
  
  if(_exponent){
    float old_fs = context->canvas->get_font_size();
//    small_em = context->script_size_multiplier * old_fs;
//    if(small_em < context->script_size_min)
//      small_em = context->script_size_min;
//    context->canvas->set_font_size(small_em);
    context->script_indent++;
    small_em = context->get_script_size(old_fs);
    context->script_indent--;
    context->canvas->set_font_size(small_em);
    
    _exponent->resize(context);
    
    context->canvas->set_font_size(old_fs);
    
    if(_extents.ascent < _exponent->extents().height() - ey)
       _extents.ascent = _exponent->extents().height() - ey;
    
    if(ex < _exponent->extents().width){
      rx+=             _exponent->extents().width - ex;
      _extents.width+= _exponent->extents().width - ex;
    }
  }
}

void RadicalBox::paint(Context *context){
// test Buffer:
//  
//  Canvas *old_canvas = context->canvas;
//  SharedPtr<Buffer> buffer = new Buffer(old_canvas, CAIRO_FORMAT_ARGB32, _extents);
//  
//  if(buffer->canvas()){
//    context->canvas = buffer->canvas();
//    
//    context->canvas->set_color(0xFFFF00, 0.3);
//    context->canvas->paint();
//    
//    context->canvas->set_color(0x000000);
//  }
    
  {
//    context->canvas->save();
//    {
//      float x, y;
//      context->canvas->current_pos(&x, &y);
//      
//      context->canvas->align_point(&x, &y, true);
//      context->canvas->move_to(x, y);
//      context->canvas->line_to(x+6, y);
//      
//      int c = context->canvas->get_color();
//      context->canvas->set_color(0xFF0000);
//      context->canvas->hair_stroke();
//      context->canvas->set_color(c);
//    }
//    context->canvas->restore();
    
    float x, y;
    context->canvas->current_pos(&x, &y);
    
    context->canvas->move_to(x + rx, y);
    _radicand->paint(context);
    
    if(_exponent){
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
    
    if(_exponent){
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
  
//  buffer->paint(old_canvas);
//  context->canvas = old_canvas;
//  
//  context->canvas->save();
//  {
//    float x, y;
//    context->canvas->current_pos(&x, &y);
//    
//    context->canvas->align_point(&x, &y, true);
//    context->canvas->move_to(x, y);
//    context->canvas->line_to(x-6, y);
//    
//    int c = context->canvas->get_color();
//    context->canvas->set_color(0xFF0000);
//    context->canvas->hair_stroke();
//    context->canvas->set_color(c);
//  }
//  context->canvas->restore();
}

Box *RadicalBox::remove(int *index){
  if(_exponent && *index == 1){
    if(_exponent->length() == 0){
      delete _exponent;
      _exponent = 0;
      invalidate();
    }
    return move_logical(Backward, false, index);
  }
  
  if(_parent){
    MathSequence *seq = dynamic_cast<MathSequence*>(_parent);
    *index = _index;
    if(seq){
      if(_exponent){
        if(_radicand->length() > 0)
          return move_logical(Backward, false, index);
        
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

void RadicalBox::complete(){
  if(!_exponent){
    _exponent = new MathSequence;
    adopt(_exponent, 1);
  }
}

pmath_t RadicalBox::to_pmath(bool parseable){
  if(_exponent)
    return pmath_expr_new_extended(
      pmath_ref(PMATH_SYMBOL_RADICALBOX), 2,
      _radicand->to_pmath(parseable),
      _exponent->to_pmath(parseable));
    
  return pmath_expr_new_extended(
    pmath_ref(PMATH_SYMBOL_SQRTBOX), 1,
    _radicand->to_pmath(parseable));
}

Box *RadicalBox::move_vertical(
  LogicalDirection  direction, 
  float            *index_rel_x,
  int              *index
){
  if(*index < 0){
    if(_exponent){
      float er;
      if(ex < _exponent->extents().width)
        er = _exponent->extents().width;
      else
        er = ex;
        
      if(*index_rel_x < (er + rx) / 2)
        return _exponent->move_vertical(direction, index_rel_x, index);
      
    }
    
    *index_rel_x-= rx;
    return _radicand->move_vertical(direction, index_rel_x, index);
  }
  
  if(!_parent)
    return this;
    
  if(*index == 0)
    *index_rel_x+= rx;
    
  *index = _index;
  return _parent->move_vertical(direction, index_rel_x, index);
}
      
Box *RadicalBox::mouse_selection(
  float x,
  float y,
  int   *start,
  int   *end,
  bool  *eol
){
  if(_parent && x > (_extents.width + rx + _radicand->extents().width) / 2){
    *start = *end = _index + 1;
    *eol = true;
    return _parent;
  }
  
  if(_exponent){
    float el = 0;
    if(ex > _exponent->extents().width)
      el = ex - _exponent->extents().width;
    
    if(x > el / 2 && x < (el + _exponent->extents().width + rx) / 2)
      return _exponent->mouse_selection(
        x - el, 
        y - ey + _exponent->extents().descent, 
        start, end, eol);
  }
  
  if(_parent && x < rx / 2){
    *start = *end = _index;
    *eol = false;
    return _parent;
  }
  
  
  return _radicand->mouse_selection(x - rx, y, start, end, eol);
}

void RadicalBox::child_transformation(
  int             index,
  cairo_matrix_t *matrix
){
  if(index == 0){
    cairo_matrix_translate(matrix, rx, 0);
  }
  else{
    float el = 0;
    if(ex > _exponent->extents().width)
      el = ex - _exponent->extents().width;
    
    cairo_matrix_translate(matrix, el, ey - _exponent->extents().descent);
  }
}

//} ... class RadicalBox
