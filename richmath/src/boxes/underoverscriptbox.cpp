#include <boxes/underoverscriptbox.h>

#include <cmath>

#include <boxes/mathsequence.h>
#include <graphics/context.h>

using namespace richmath;

extern pmath_symbol_t richmath_System_OverscriptBox;
extern pmath_symbol_t richmath_System_UnderoverscriptBox;
extern pmath_symbol_t richmath_System_UnderscriptBox;

static inline bool char_is_vertical_paren(uint16_t ch) {
  return ch == 0x23B4
         || ch == 0x23B5
         || (ch >= 0x23DC && ch <= 0x23E1);
}

//{ class UnderoverscriptBox ...

UnderoverscriptBox::UnderoverscriptBox()
  : Box(),
  _base(       new MathSequence),
  _underscript(new MathSequence),
  _overscript( new MathSequence)
{
  adopt(_base,        0);
  adopt(_underscript, 1);
  adopt(_overscript,  2);
}

UnderoverscriptBox::UnderoverscriptBox(
  MathSequence *base,
  MathSequence *under,
  MathSequence *over)
  : Box(),
  _base(       base),
  _underscript(under),
  _overscript( over)
{
  assert(_base && (_underscript || _overscript));
  adopt(_base, 0);
  
  int i = 1;
  if(_underscript)
    adopt(_underscript, i++);
  if(_overscript)
    adopt(_overscript, i);
}

UnderoverscriptBox::~UnderoverscriptBox() {
  delete_owned(_base);
  delete_owned(_underscript);
  delete_owned(_overscript);
}

bool UnderoverscriptBox::try_load_from_object(Expr expr, BoxInputFlags opts) {
  if(expr[0] == richmath_System_OverscriptBox) {
    if(expr.expr_length() != 2)
      return false;
      
    if(_underscript) {
      _underscript->safe_destroy();
      _underscript = nullptr;
    }
    
    if(!_overscript)
      _overscript = new MathSequence;
      
    adopt(_overscript, 1);
    
    _base->load_from_object(      expr[1], opts);
    _overscript->load_from_object(expr[2], opts);
    
    finish_load_from_object(std::move(expr));
    return true;
  }
  
  if(expr[0] == richmath_System_UnderscriptBox) {
    if(expr.expr_length() != 2)
      return false;
      
    if(_overscript) {
      _overscript->safe_destroy();
      _overscript = nullptr;
    }
    
    if(!_underscript)
      _underscript = new MathSequence;
      
    adopt(_underscript, 1);
    
    _base->load_from_object(       expr[1], opts);
    _underscript->load_from_object(expr[2], opts);
    
    finish_load_from_object(std::move(expr));
    return true;
  }
  
  if(expr[0] == richmath_System_UnderoverscriptBox) {
    if(expr.expr_length() != 3)
      return false;
      
    if(!_underscript)
      _underscript = new MathSequence;
      
    if(!_overscript)
      _overscript = new MathSequence;
      
    adopt(_underscript, 1);
    adopt(_overscript, 2);
    
    _base->load_from_object(       expr[1], opts);
    _underscript->load_from_object(expr[2], opts);
    _overscript->load_from_object( expr[3], opts);
    
    finish_load_from_object(std::move(expr));
    return true;
  }
  
  return false;
}

Box *UnderoverscriptBox::item(int i) {
  if(i == 0)
    return _base;
    
  if(i == 1 && _underscript)
    return _underscript;
    
  return _overscript;
}

int UnderoverscriptBox::count() {
  return 1 + (_underscript ? 1 : 0) + (_overscript ? 1 : 0);
}

void UnderoverscriptBox::resize(Context &context) {
  float old_w = context.width;
  context.width = HUGE_VAL;
  
  _base->resize(context);
  
  int old_script_indent = context.script_indent;
  float old_fs = context.canvas().get_font_size();
  
  context.script_indent++;
  float em = context.get_script_size(old_fs);
  context.canvas().set_font_size(em);
  
  float w = 0;
  
  _underscript_is_stretched = _overscript_is_stretched = false;
  
  if(_underscript) {
    _underscript->resize(context);
    
    _underscript_is_stretched = _underscript->stretch_horizontal(
                                  context, _base->extents().width);
                                  
    w = _underscript->extents().width;
  }
  
  if(_overscript) {
    _overscript->resize(context);
    
    _overscript_is_stretched = _overscript->stretch_horizontal(
                                 context, _base->extents().width);
                                 
    if(w < _overscript->extents().width)
      w = _overscript->extents().width;
  }
  
  context.canvas().set_font_size(old_fs);
  context.width = old_w;
  
  if( !_underscript_is_stretched &&
      !_overscript_is_stretched &&
      _base->length() == 1)
  {
    if(_parent && _parent->length() == 1) {
      if(auto uo = dynamic_cast<UnderoverscriptBox*>(_parent->parent())) {
        for(int i = 0; i < uo->count(); ++i)
          if(i != _parent->index()) {
            float wi = uo->item(i)->extents().width;
            if(w < wi)
              w = wi;
          }
      }
    }
    
    _base->stretch_horizontal(context, w + 0.6f * em);
  }
  
  context.script_indent = old_script_indent;
  after_items_resize(context);
}

void UnderoverscriptBox::after_items_resize(Context &context) {
  context.math_shaper->accent_positions(
    context,
    _base,
    _underscript,
    _overscript,
    &_base_offset_x,
    &_underscript_offset,
    &_overscript_offset);
    
  _extents = _base->extents();
  if(_base_offset_x > 0)
    _extents.width += _base_offset_x;
    
  if(_underscript) {
    if(_extents.descent < _underscript_offset.y + _underscript->extents().descent)
      _extents.descent = _underscript_offset.y + _underscript->extents().descent;
      
    if(_extents.width < _underscript_offset.x + _underscript->extents().width)
      _extents.width = _underscript_offset.x + _underscript->extents().width;
  }
  
  if(_overscript) {
    if(_extents.ascent < -_overscript_offset.y + _overscript->extents().ascent)
      _extents.ascent = -_overscript_offset.y + _overscript->extents().ascent;
      
    if(_extents.width < _overscript_offset.x + _overscript->extents().width)
      _extents.width = _overscript_offset.x + _overscript->extents().width;
  }
}

void UnderoverscriptBox::colorize_scope(SyntaxState &state) {
  _base->colorize_scope(state);
  
  if(_underscript && !_underscript_is_stretched)
    _underscript->colorize_scope(state);
    
  if(_overscript && !_overscript_is_stretched)
    _overscript->colorize_scope(state);
}

void UnderoverscriptBox::paint(Context &context) {
  update_dynamic_styles(context);
    
  Point pos = context.canvas().current_pos();
  
  context.canvas().move_to(pos.x + _base_offset_x, pos.y);
  _base->paint(context);
  
  float old_fs = context.canvas().get_font_size();
  
  if(_underscript) {
    context.canvas().move_to(pos + _underscript_offset);
    
    context.canvas().set_font_size(_underscript->font_size());
    _underscript->paint(context);
  }
  
  if(_overscript) {
    context.canvas().move_to(pos + _overscript_offset);
    
    context.canvas().set_font_size(_overscript->font_size());
    _overscript->paint(context);
  }
  
  context.canvas().set_font_size(old_fs);
}

Box *UnderoverscriptBox::remove(int *index) {
  if(*index == 0) {
    if(_base->length() == 0) {
      if(auto seq = dynamic_cast<MathSequence*>(_parent)) {
        if(_underscript && !_overscript) {
          seq->insert(_index + 1, _underscript, 0, _underscript->length());
          *index = _index;
          return seq->remove(index);
        }
        if(!_underscript && _overscript) {
          seq->insert(_index + 1, _overscript, 0, _overscript->length());
          *index = _index;
          return seq->remove(index);
        }
      }
    }
    
    if(_base->length() == 0)
      _base->insert(0, PMATH_CHAR_PLACEHOLDER);
      
    return move_logical(LogicalDirection::Backward, false, index);
  }
  
  if(_underscript && _overscript) {
    if(*index == 1) {
      if(_underscript->length() == 0) {
        _underscript->safe_destroy();
        _underscript = nullptr;
        adopt(_overscript, 1);
        invalidate();
      }
      
      return move_logical(LogicalDirection::Backward, false, index);
    }
    
    if(_overscript->length() == 0) {
      _overscript->safe_destroy();
      _overscript = nullptr;
      invalidate();
    }
    return move_logical(LogicalDirection::Backward, false, index);
  }
  
  auto seq = dynamic_cast<MathSequence*>(_parent);
  if( seq && 
      ((_underscript && _underscript->length() == 0) || 
       (_overscript  && _overscript->length()  == 0))) 
  {
    *index = _index + _base->length();
    seq->insert(_index + 1, _base, 0, _base->length());
    int i = _index;
    seq->remove(&i);
    return seq;
  }
  
  return move_logical(LogicalDirection::Backward, false, index);
}

void UnderoverscriptBox::complete() {
  if(!_underscript) {
    _underscript = new MathSequence;
    adopt(_underscript, 1);
    adopt(_overscript, 2);
  }
  
  if(!_overscript) {
    _overscript = new MathSequence;
    adopt(_overscript, 2);
  }
}

Expr UnderoverscriptBox::to_pmath_symbol() {
  if(_underscript) {
    if(_overscript)
      return Symbol(richmath_System_UnderoverscriptBox);
    return Symbol(richmath_System_UnderscriptBox);
  }
  
  return Symbol(richmath_System_OverscriptBox);
}

Expr UnderoverscriptBox::to_pmath(BoxOutputFlags flags) {
  if(_underscript) {
    if(_overscript)
      return Call(
               Symbol(richmath_System_UnderoverscriptBox),
               _base->to_pmath(flags),
               _underscript->to_pmath(flags),
               _overscript->to_pmath(flags));
               
    return Call(
             Symbol(richmath_System_UnderscriptBox),
             _base->to_pmath(flags),
             _underscript->to_pmath(flags));
  }
  
  return Call(
           Symbol(richmath_System_OverscriptBox),
           _base->to_pmath(flags),
           _overscript->to_pmath(flags));
}

Box *UnderoverscriptBox::move_vertical(
  LogicalDirection  direction,
  float            *index_rel_x,
  int              *index,
  bool              called_from_child
) {
  MathSequence *dst = 0;
  
  if(*index < 0) {
    if(direction == LogicalDirection::Forward) {
      if(_overscript) {
        dst = _overscript;
        *index_rel_x -= _overscript_offset.x;
      }
      else {
        dst = _base;
        *index_rel_x -= _base_offset_x;
      }
    }
    else if(_underscript) {
      dst = _underscript;
      *index_rel_x -= _underscript_offset.x;
    }
    else {
      dst = _base;
      *index_rel_x -= _base_offset_x;
    }
  }
  else if(*index == 0) { // comming from base
    *index_rel_x += _base_offset_x;
    
    if(direction == LogicalDirection::Backward) {
      dst = _overscript;
      *index_rel_x -= _overscript_offset.x;
    }
    else {
      dst = _underscript;
      *index_rel_x -= _underscript_offset.x;
    }
  }
  else if(*index == 1 && _underscript) { // comming from underscript
    *index_rel_x += _underscript_offset.x;
    
    if(direction == LogicalDirection::Backward) {
      dst = _base;
      *index_rel_x -= _base_offset_x;
    }
  }
  else { // comming from overscript
    *index_rel_x += _overscript_offset.x;
    
    if(direction == LogicalDirection::Forward) {
      dst = _base;
      *index_rel_x -= _base_offset_x;
    }
  }
  
  if(!dst) {
    if(_parent) {
      *index = _index;
      return _parent->move_vertical(direction, index_rel_x, index, true);
    }
    
    return this;
  }
  
  *index = -1;
  return dst->move_vertical(direction, index_rel_x, index, false);
}

VolatileSelection UnderoverscriptBox::mouse_selection(float x, float y, bool *was_inside_start) {
  if(_underscript) {
    if(_underscript_offset.y - _underscript->extents().ascent > _base->extents().descent) {
      if(y > _base->extents().descent) {
        return _underscript->mouse_selection( x - _underscript_offset.x, y - _underscript_offset.y, was_inside_start);
      }
    }
    else if(x >= _underscript_offset.x) {
      if(!_overscript || y >= _underscript_offset.y - _underscript->extents().ascent + _overscript_offset.y + _overscript->extents().descent) {
        return _underscript->mouse_selection(x - _underscript_offset.x, y - _underscript_offset.y, was_inside_start);
      }
    }
  }
  
  if(_overscript) {
    if(-_overscript_offset.y - _overscript->extents().descent > _base->extents().ascent) {
      if(y < -_base->extents().ascent) 
        return _overscript->mouse_selection(x - _overscript_offset.x, y - _overscript_offset.y, was_inside_start);
    }
    else if(x >= _overscript_offset.x) 
      return _overscript->mouse_selection(x - _overscript_offset.x, y - _overscript_offset.y, was_inside_start);
  }
  
  return _base->mouse_selection(x - _base_offset_x, y, was_inside_start);
}

void UnderoverscriptBox::child_transformation(
  int             index,
  cairo_matrix_t *matrix
) {
  if(index == 0) {
    cairo_matrix_translate(
      matrix,
      _base_offset_x,
      0);
  }
  else if(index == 1 && _underscript) {
    cairo_matrix_translate(
      matrix,
      _underscript_offset.x,
      _underscript_offset.y);
  }
  else {
    cairo_matrix_translate(
      matrix,
      _overscript_offset.x,
      _overscript_offset.y);
  }
}

//} ... class UnderoverscriptBox
