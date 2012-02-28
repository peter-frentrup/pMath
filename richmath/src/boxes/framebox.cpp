#include <boxes/framebox.h>

#include <boxes/mathsequence.h>
#include <graphics/context.h>

using namespace richmath;

//{ class FrameBox ...

FrameBox::FrameBox(MathSequence *content)
  : OwnerBox(content)
{
}

bool FrameBox::try_load_from_object(Expr expr, int options) {
  if(expr[0] != PMATH_SYMBOL_FRAMEBOX)
    return false;
  
  if(expr.expr_length() != 1)
    return false;
    
  /* now success is guaranteed */
  
  _content->load_from_object(expr[1], options);
  
  return true;
}

void FrameBox::resize(Context *context) {
  em = context->canvas->get_font_size();
  
  float old_width = context->width;
  context->width -= em * 0.5f;
  
  OwnerBox::resize(context);
  
  cx = em * 0.25f;
  
  _extents.width +=   2 * cx;
  _extents.ascent +=  cx;
  _extents.descent += cx;
  
  if(_extents.ascent < em)
    _extents.ascent = em;
    
  if(_extents.descent < em * 0.5f)
    _extents.descent = em * 0.5f;
    
  context->width = old_width;
}

void FrameBox::paint(Context *context) {
  float x, y;
  context->canvas->current_pos(&x, &y);
  
  context->canvas->pixframe(
    x,
    y - _extents.ascent,
    x + _extents.width,
    y + _extents.descent,
    0.1f * em);
    
  context->canvas->fill();
  
  context->canvas->move_to(x, y);
  OwnerBox::paint(context);
}

Expr FrameBox::to_pmath(int flags) {
  return Call(
           Symbol(PMATH_SYMBOL_FRAMEBOX),
           _content->to_pmath(flags));
}

//} ... class FrameBox
