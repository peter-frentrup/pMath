#include <boxes/subsuperscriptbox.h>

#include <cmath>

#include <boxes/mathsequence.h>
#include <graphics/context.h>

using namespace richmath;

extern pmath_symbol_t richmath_System_SubscriptBox;
extern pmath_symbol_t richmath_System_SubsuperscriptBox;
extern pmath_symbol_t richmath_System_SuperscriptBox;

//{ class SubsuperscriptBox ...

SubsuperscriptBox::SubsuperscriptBox()
  : Box(),
    _subscript(  new MathSequence),
    _superscript(new MathSequence)
{
  adopt(_subscript,   0);
  adopt(_superscript, 0);
}


SubsuperscriptBox::SubsuperscriptBox(MathSequence *sub, MathSequence *super)
  : Box(),
    _subscript(sub),
    _superscript(super)
{
  assert(_subscript || _superscript);
  int i = 0;
  if(_subscript)
    adopt(_subscript, i++);
  if(_superscript)
    adopt(_superscript, i);
}

SubsuperscriptBox::~SubsuperscriptBox() {
  delete_owned(_subscript);
  delete_owned(_superscript);
}

bool SubsuperscriptBox::try_load_from_object(Expr expr, BoxInputFlags opts) {
  if(expr[0] == richmath_System_SubscriptBox) {
    if(expr.expr_length() != 1)
      return false;
      
    if(_superscript) {
      _superscript->safe_destroy();
      _superscript = nullptr;
    }
    
    if(!_subscript)
      _subscript = new MathSequence;
      
    adopt(_subscript, 0);
    
    _subscript->load_from_object(expr[1], opts);
    
    finish_load_from_object(std::move(expr));
    return true;
  }
  
  if(expr[0] == richmath_System_SuperscriptBox) {
    if(expr.expr_length() != 1)
      return false;
      
    if(_subscript) {
      _subscript->safe_destroy();
      _subscript = nullptr;
    }
    
    if(!_superscript)
      _superscript = new MathSequence;
      
    adopt(_superscript, 0);
    
    _superscript->load_from_object(expr[1], opts);
    
    finish_load_from_object(std::move(expr));
    return true;
  }
  
  if(expr[0] == richmath_System_SubsuperscriptBox) {
    if(expr.expr_length() != 2)
      return false;
      
    if(!_subscript)
      _subscript = new MathSequence;
      
    if(!_superscript)
      _superscript = new MathSequence;
      
    adopt(_subscript,   0);
    adopt(_superscript, 1);
    
    _subscript->load_from_object(  expr[1], opts);
    _superscript->load_from_object(expr[2], opts);
    
    finish_load_from_object(std::move(expr));
    return true;
  }
  
  return false;
}

Box *SubsuperscriptBox::item(int i) {
  if(i == 0 && _subscript)
    return _subscript;
  return _superscript;
}

int SubsuperscriptBox::count() {
  return (_subscript ? 1 : 0) + (_superscript ? 1 : 0);
}

void SubsuperscriptBox::resize(Context *context) {
  float old_w = context->width;
  float old_fs = context->canvas->get_font_size();
  int old_script_indent = context->script_indent;
  context->script_indent++;
  
  context->width = HUGE_VAL;
  
  em = context->get_script_size(old_fs);
  context->canvas->set_font_size(em);
  
  if(_subscript)
    _subscript->resize(context);
    
  if(_superscript)
    _superscript->resize(context);
    
  context->script_indent = old_script_indent;
  context->canvas->set_font_size(old_fs);
  context->width = old_w;
  
  static const BoxSize zero_size(0, 0, 0);
  stretch(context, zero_size);
}

void SubsuperscriptBox::stretch(Context *context, const BoxSize &base) {
  context->math_shaper->script_positions(
    context, base.ascent, base.descent, _subscript, _superscript,
    &sub_y, &super_y);
    
  sub_x = super_x = 0;
  _extents.ascent  = base.ascent;
  _extents.descent = base.descent;
  _extents.width   = 0;
  
  if(_extents.ascent < em * 0.5)
    _extents.ascent = em * 0.5;
    
  if(_extents.descent < 0)
    _extents.descent = 0;
    
  if(_subscript) {
    if(_extents.descent < sub_y + _subscript->extents().descent)
      _extents.descent = sub_y + _subscript->extents().descent;
      
    if(_extents.width < _subscript->extents().width)
      _extents.width = _subscript->extents().width;
  }
  
  if(_superscript) {
    if(_extents.ascent < -super_y + _superscript->extents().ascent)
      _extents.ascent = -super_y + _superscript->extents().ascent;
      
    if(_extents.width < _superscript->extents().width)
      _extents.width = _superscript->extents().width;
  }
}

void SubsuperscriptBox::adjust_x(
  Context           *context,
  uint16_t           base_char,
  const GlyphInfo   &base_info
) {
  context->math_shaper->script_corrections(
    context, base_char, base_info,
    _subscript, _superscript,
    sub_y, super_y,
    &sub_x, &super_x);
    
  if(_subscript) {
    if(_extents.width < sub_x + _subscript->extents().width)
      _extents.width = sub_x + _subscript->extents().width;
  }
  
  if(_superscript) {
    if(_extents.width < super_x + _superscript->extents().width)
      _extents.width = super_x + _superscript->extents().width;
  }
}

void SubsuperscriptBox::paint(Context *context) {
  update_dynamic_styles(context);
    
  float x, y;
  context->canvas->current_pos(&x, &y);
  
  float old_fs = context->canvas->get_font_size();
  context->canvas->set_font_size(em);
  
  if(_subscript) {
    context->canvas->move_to(x + sub_x, y + sub_y);
    _subscript->paint(context);
  }
  
  if(_superscript) {
    context->canvas->move_to(x + super_x, y + super_y);
    _superscript->paint(context);
  }
  
  context->canvas->set_font_size(old_fs);
}

Box *SubsuperscriptBox::remove(int *index) {
  if(_subscript && _superscript) {
    if(*index == 0) {
      if(_subscript->length() == 0) {
        _subscript->safe_destroy();
        _subscript = nullptr;
        adopt(_superscript, 1);
        invalidate();
      }
      return move_logical(LogicalDirection::Backward, false, index);
    }
    
    if(_superscript->length() == 0) {
      _superscript->safe_destroy();
      _superscript = nullptr;
      invalidate();
    }
    return move_logical(LogicalDirection::Backward, false, index);
  }
  
  if(_parent) {
    if( (_subscript   && _subscript->length()   == 0) ||
        (_superscript && _superscript->length() == 0))
    {
      *index = _index;
      return _parent->remove(index);
    }
  }
  
  return move_logical(LogicalDirection::Backward, false, index);
}

void SubsuperscriptBox::complete() {
  if(!_subscript) {
    _subscript = new MathSequence;
    adopt(_subscript, 0);
    adopt(_superscript, 1);
  }
  
  if(!_superscript) {
    _superscript = new MathSequence;
    adopt(_superscript, 1);
  }
}

Expr SubsuperscriptBox::to_pmath_symbol() {
  if(_subscript) {
    if(_superscript)
      return Symbol(richmath_System_SubsuperscriptBox);
    return Symbol(richmath_System_SubscriptBox);
  }
  
  return Symbol(richmath_System_SuperscriptBox);
}

Expr SubsuperscriptBox::to_pmath(BoxOutputFlags flags) {
  if(_subscript) {
    if(_superscript)
      return Call(
               Symbol(richmath_System_SubsuperscriptBox),
               _subscript->to_pmath(flags),
               _superscript->to_pmath(flags));
               
    return Call(
             Symbol(richmath_System_SubscriptBox),
             _subscript->to_pmath(flags));
  }
  
  return Call(
           Symbol(richmath_System_SuperscriptBox),
           _superscript->to_pmath(flags));
}

Box *SubsuperscriptBox::move_vertical(
  LogicalDirection  direction,
  float            *index_rel_x,
  int              *index,
  bool              called_from_child
) {
  MathSequence *dst = 0;
  
  if(*index < 0) {
    if(direction == LogicalDirection::Forward || !_subscript) {
      dst = _superscript;
      *index_rel_x -= super_x;
    }
    else {
      dst = _subscript;
      *index_rel_x -= sub_x;
//      *index_rel_x-= _base.width;
    }
  }
  else if(*index == 0 && _subscript) { // comming from subscript
//    *index_rel_x+= _base.width;
    *index_rel_x += sub_x;
    
    if(direction == LogicalDirection::Backward)
      dst = _superscript;
  }
  else { // comming from superscript
    if(direction == LogicalDirection::Forward && _subscript) {
      dst = _subscript;
      *index_rel_x -= sub_x;
//      *index_rel_x-= _base.width;
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
VolatileSelection SubsuperscriptBox::mouse_selection(float x, float y, bool *was_inside_start) {
  if(_subscript) {
    if(!_superscript || y >= sub_y - _subscript->extents().ascent + super_y + _superscript->extents().descent) {
      return _subscript->mouse_selection(
               x - sub_x, // x - _base.width,
               y - sub_y,
               was_inside_start);
    }
  }
          
  if(_superscript) {
    return _superscript->mouse_selection(
             x - super_x,
             y - super_y,
             was_inside_start);
  }
  
  *was_inside_start = true;
  return { _parent, _index, _index + 1 };
}

void SubsuperscriptBox::child_transformation(
  int             index,
  cairo_matrix_t *matrix
) {
  if(index == 0 && _subscript)
    cairo_matrix_translate(matrix, sub_x/*_base.width*/, sub_y);
  else
    cairo_matrix_translate(matrix, super_x, super_y);
}

//} ... class SubsuperscriptBox
