#include <boxes/errorbox.h>

#include <graphics/context.h>

using namespace richmath;

//{ class ErrorBox ...
ErrorBox::ErrorBox(const Expr object)
: Box(),
  _object(object)
{
}

ErrorBox::~ErrorBox(){
}

void ErrorBox::resize(Context *context){
  float em = context->canvas->get_font_size();
  _extents.ascent = 0.75f * em;
  _extents.descent = 0.25f * em;
  _extents.width = 2 * em;
}

void ErrorBox::paint(Context *context){
  float x, y;
  context->canvas->current_pos(&x, &y);
  
  context->canvas->pixrect(
    x,
    y - _extents.ascent,
    x + _extents.width,
    y + _extents.descent,
    true);
  
  context->canvas->save();
  context->canvas->set_color(0xFFE6E6);
  context->canvas->fill_preserve();
  context->canvas->set_color(0xFF5454);
  context->canvas->hair_stroke();
  context->canvas->restore();
}

pmath_t ErrorBox::to_pmath(bool parseable){
  return pmath_ref(_object.get());
}

//} ... class ErrorBox
