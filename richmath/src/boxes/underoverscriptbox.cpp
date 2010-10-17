#include <boxes/underoverscriptbox.h>

#include <cmath>

#include <boxes/mathsequence.h>
#include <graphics/context.h>

using namespace richmath;

static inline bool char_is_vertical_paren(uint16_t ch){
  return ch == 0x23B4 
      || ch == 0x23B5
      || (ch >= 0x23DC && ch <= 0x23E1);
}

//{ class UnderoverscriptBox ...

UnderoverscriptBox::UnderoverscriptBox(
  MathSequence *base, 
  MathSequence *under, 
  MathSequence *over)
: Box(),
  _base(base),
  _underscript(under),
  _overscript(over)
{
  assert(_base && (_underscript || _overscript));
  adopt(_base, 0);
  int i = 1;
  if(_underscript)
    adopt(_underscript, i++);
  if(_overscript)
    adopt(_overscript, i);
}

UnderoverscriptBox::~UnderoverscriptBox(){
  delete _base;
  delete _underscript;
  delete _overscript;
}

Box *UnderoverscriptBox::item(int i){
  if(i == 0)
    return _base;
  
  if(i == 1 && _underscript)
    return _underscript;
  
  return _overscript;
}

int UnderoverscriptBox::count(){
  return 1 + (_underscript ? 1 : 0) + (_overscript ? 1 : 0);
}

void UnderoverscriptBox::resize(Context *context){
  float old_w = context->width;
  context->width = HUGE_VAL;
  
  _base->resize(context);
  
  int old_script_indent = context->script_indent;
  float old_fs = context->canvas->get_font_size();
  
  context->script_indent++;
  float em = context->get_script_size(old_fs);
  context->canvas->set_font_size(em);
    
  float w = 0;
  
  u_stretched = o_stretched = false;
  
  if(_underscript){
    _underscript->resize(context);
    
    u_stretched = _underscript->stretch_horizontal(
      context, _base->extents().width);
    
    w = _underscript->extents().width;
  }
  
  if(_overscript){
    _overscript->resize(context);
    
    o_stretched = _overscript->stretch_horizontal(
      context, _base->extents().width);
    
    if(w < _overscript->extents().width)
       w = _overscript->extents().width;
  }
  
  context->canvas->set_font_size(old_fs);
  context->width = old_w;
  
  if(!u_stretched && !o_stretched 
  && _base->length() == 1){
    if(_parent && _parent->length() == 1){
      UnderoverscriptBox *uo = dynamic_cast<UnderoverscriptBox*>(_parent->parent());
      
      if(uo){
        for(int i = 0;i < uo->count();++i)
          if(i != _parent->index()){
            float wi = uo->item(i)->extents().width;
            if(w < wi)
              w = wi;
          }
      }
    }
    
    _base->stretch_horizontal(context, w + 0.6f * em);
  }
  
  context->script_indent = old_script_indent;
  after_items_resize(context);
}

void UnderoverscriptBox::after_items_resize(Context *context){
  context->math_shaper->accent_positions(
    context, _base, _underscript, _overscript,
    &base_x, &under_x, &under_y, &over_x, &over_y);
  
  _extents = _base->extents();
  if(base_x > 0)
    _extents.width+= base_x;
  
  if(_underscript){
    if(_extents.descent < under_y + _underscript->extents().descent)
       _extents.descent = under_y + _underscript->extents().descent;
    
    if(_extents.width < under_x + _underscript->extents().width)
       _extents.width = under_x + _underscript->extents().width;
  }
  
  if(_overscript){
    if(_extents.ascent < -over_y + _overscript->extents().ascent)
       _extents.ascent = -over_y + _overscript->extents().ascent;
    
    if(_extents.width < over_x + _overscript->extents().width)
       _extents.width = over_x + _overscript->extents().width;
  }
}

void UnderoverscriptBox::colorize_scope(SyntaxState *state){
  _base->colorize_scope(state);
  
  if(_underscript && !u_stretched)
    _underscript->colorize_scope(state);
  
  if(_overscript && !o_stretched)
    _overscript->colorize_scope(state);
}

void UnderoverscriptBox::paint(Context *context){
  float x, y;
  context->canvas->current_pos(&x, &y);
  
  context->canvas->move_to(x + base_x, y);
  _base->paint(context);
  
  float old_fs = context->canvas->get_font_size();
  
  if(_underscript){
    context->canvas->move_to(x + under_x, y + under_y);
    
    context->canvas->set_font_size(_underscript->font_size());
    _underscript->paint(context);
  }
  
  if(_overscript){
    context->canvas->move_to(x + over_x, y + over_y);
    
    context->canvas->set_font_size(_overscript->font_size());
    _overscript->paint(context);
  }
  
  context->canvas->set_font_size(old_fs);
}

Box *UnderoverscriptBox::remove(int *index){
  if(*index == 0){
    if(_base->length() == 0){
      MathSequence *seq = dynamic_cast<MathSequence*>(_parent);
      if(seq){
        if(_underscript && !_overscript){
          seq->insert(_index + 1, _underscript, 0, _underscript->length());
          *index = _index;
          return seq->remove(index);
        }
        if(!_underscript && _overscript){
          seq->insert(_index + 1, _overscript, 0, _overscript->length());
          *index = _index;
          return seq->remove(index);
        }
      }
    }
    
    if(_base->length() == 0)
      _base->insert(0, PMATH_CHAR_PLACEHOLDER);
      
    return move_logical(Backward, false, index);
  }
  
  if(_underscript && _overscript){
    if(*index == 1){
      if(_underscript->length() == 0){
        delete _underscript;
        _underscript = 0;
        adopt(_overscript, 1);
        invalidate();
      }
      
      return move_logical(Backward, false, index);
    }
    
    if(_overscript->length() == 0){
      delete _overscript;
      _overscript = 0;
      invalidate();
    }
    return move_logical(Backward, false, index);
  }
  
  MathSequence *seq = dynamic_cast<MathSequence*>(_parent);
  if(seq
  && ((_underscript && _underscript->length() == 0)
   || (_overscript  && _overscript->length()  == 0))){
    *index = _index + _base->length();
    seq->insert(_index + 1, _base, 0, _base->length());
    int i = _index;
    seq->remove(&i);
    return seq;
  }
  
  return move_logical(Backward, false, index);
}

void UnderoverscriptBox::complete(){
  if(!_underscript){
    _underscript = new MathSequence;
    adopt(_underscript, 1);
    adopt(_overscript, 2);
  }
  
  if(!_overscript){
    _overscript = new MathSequence;
    adopt(_overscript, 2);
  }
}

Expr UnderoverscriptBox::to_pmath(bool parseable){
  if(_underscript){
    if(_overscript)
      return Call(
        Symbol(PMATH_SYMBOL_UNDEROVERSCRIPTBOX),
        _base->to_pmath(parseable),
        _underscript->to_pmath(parseable),
        _overscript->to_pmath(parseable));
    
    return Call(
        Symbol(PMATH_SYMBOL_UNDERSCRIPTBOX),
        _base->to_pmath(parseable),
        _underscript->to_pmath(parseable));
  }
  
  return Call(
      Symbol(PMATH_SYMBOL_OVERSCRIPTBOX),
      _base->to_pmath(parseable),
      _overscript->to_pmath(parseable));
}

Box *UnderoverscriptBox::move_vertical(
  LogicalDirection  direction, 
  float            *index_rel_x,
  int              *index
){
  MathSequence *dst = 0;
  
  if(*index < 0){
    if(direction == Forward){
      if(_overscript){
        dst = _overscript;
        *index_rel_x-= over_x;
      }
      else{
        dst = _base;
        *index_rel_x-= base_x;
      }
    }
    else if(_underscript){
      dst = _underscript;
      *index_rel_x-= under_x;
    }
    else{
      dst = _base;
      *index_rel_x-= base_x;
    }
  }
  else if(*index == 0){ // comming from base
    *index_rel_x+= base_x;
    
    if(direction == Backward){
      dst = _overscript;
      *index_rel_x-= over_x;
    }
    else{
      dst = _underscript;
      *index_rel_x-= under_x;
    }
  }
  else if(*index == 1 && _underscript){ // comming from underscript
    *index_rel_x+= under_x;
    
    if(direction == Backward){
      dst = _base;
      *index_rel_x-= base_x;
    }
  }
  else{ // comming from overscript
    *index_rel_x+= over_x;
    
    if(direction == Forward){
      dst = _base;
      *index_rel_x-= base_x;
    }
  }
  
  if(!dst){
    if(_parent){
      *index = _index;
      return _parent->move_vertical(direction, index_rel_x, index);
    }
    
    return this;
  }
  
  *index = -1;
  return dst->move_vertical(direction, index_rel_x, index);
}

Box *UnderoverscriptBox::mouse_selection(
  float x,
  float y,
  int   *start,
  int   *end,
  bool  *eol
){
  if(_underscript){
    if(under_y - _underscript->extents().ascent > _base->extents().descent){
      if(y > _base->extents().descent){
        return _underscript->mouse_selection(
          x - under_x,
          y - under_y,
          start,
          end,
          eol);
      }
    }
    else if(x >= under_x){
      if(!_overscript 
      || y >= under_y - _underscript->extents().ascent + over_y + _overscript->extents().descent){
        return _underscript->mouse_selection(
          x - under_x,
          y - under_y,
          start,
          end,
          eol);
      }
    }
  }
  
  if(_overscript){
    if(-over_y - _overscript->extents().descent > _base->extents().ascent){
      if(y < -_base->extents().ascent){
        return _overscript->mouse_selection(
          x - over_x,
          y - over_y,
          start,
          end,
          eol);
      }
    }
    else if(x >= over_x){
      return _overscript->mouse_selection(
        x - over_x,
        y - over_y,
        start,
        end,
        eol);
    }
  }
  
  return _base->mouse_selection(
    x - base_x,
    y,
    start,
    end,
    eol);
}

void UnderoverscriptBox::child_transformation(
  int             index,
  cairo_matrix_t *matrix
){
  if(index == 0){
    cairo_matrix_translate(
      matrix, 
      base_x, 
      0);
  }
  else if(index == 1 && _underscript){
    cairo_matrix_translate(
      matrix, 
      under_x,
      under_y);
  }
  else{
    cairo_matrix_translate(
      matrix, 
      over_x, 
      over_y);
  }
}

//} ... class UnderoverscriptBox
