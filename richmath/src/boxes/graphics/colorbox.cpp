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

ColorBox *ColorBox::create(Expr expr, int opts) {
  int c = pmath_to_color(expr);
  if(c < 0)
    return 0;
    
  return new ColorBox(c);
}

void ColorBox::paint(Context *context) {
  context->canvas->set_color(_color);
}

Expr ColorBox::to_pmath(int flags) {
  return color_to_pmath(_color);
}

//} ... class LineBox
