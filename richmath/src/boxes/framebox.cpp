#include <boxes/framebox.h>

#include <boxes/mathsequence.h>
#include <graphics/context.h>

using namespace richmath;

//{ class FrameBox ...

FrameBox::FrameBox(MathSequence *content)
: OwnerBox(content)
{
}

void FrameBox::resize(Context *context){
  em = context->canvas->get_font_size();
  
  float old_width = context->width;
  context->width-= em * 0.3f;
  
  OwnerBox::resize(context);
  
  _extents.width+= em * 0.5f;
  _extents.ascent+= em * 0.25f;
  _extents.descent+= em * 0.25f;
  if(_extents.ascent < em)
    _extents.ascent = em;
  if(_extents.descent < em * 0.5f)
    _extents.descent = em * 0.5f;
  
  context->width = old_width;
}

void FrameBox::paint(Context *context){
  float x,y;
  context->canvas->current_pos(&x, &y);
  
  context->canvas->pixframe(
    x,
    y - _extents.ascent,
    x + _extents.width,
    y + _extents.descent,
    0.1f * em);
  
  context->canvas->fill();
  
  context->canvas->move_to(x + 0.25f * em, y);
  
  _content->paint(context);
}

Expr FrameBox::to_pmath(bool parseable){
  return Call(
    Symbol(PMATH_SYMBOL_FRAMEBOX),
    _content->to_pmath(parseable));
}

//} ... class FrameBox
