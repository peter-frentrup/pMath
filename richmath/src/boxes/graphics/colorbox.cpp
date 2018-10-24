#include <boxes/graphics/colorbox.h>

#include <graphics/context.h>
#include <util/style.h>


using namespace richmath;

//{ class ColorBox ...

ColorBox::ColorBox(int color)
  : GraphicsElement(),
  _color(color)
{
}

ColorBox::~ColorBox() {
}

bool ColorBox::try_load_from_object(Expr expr, BoxInputFlags opts) {
  int c = pmath_to_color(expr);
  if(c < 0)
    return false;
  
  _color = c;
  
  finish_load_from_object(std::move(expr));
  return true;
}

ColorBox *ColorBox::create(Expr expr, BoxInputFlags opts) {
  ColorBox *box = new ColorBox;
  
  if(!box->try_load_from_object(expr, opts)) {
    delete box;
    return nullptr;
  }
  
  return box;
}

void ColorBox::paint(GraphicsBoxContext *context) {
  context->ctx->canvas->set_color(_color);
}

Expr ColorBox::to_pmath(BoxOutputFlags flags) {
  return color_to_pmath(_color);
}

//} ... class LineBox
