#include <boxes/graphics/colorbox.h>

#include <graphics/context.h>
#include <util/style.h>


using namespace richmath;

//{ class ColorBox ...

ColorBox::ColorBox()
  : GraphicsElement()
{
}

ColorBox::~ColorBox() {
}

bool ColorBox::try_load_from_object(Expr expr, BoxInputFlags opts) {
  Color c = Color::from_pmath(expr);
  if(!c.is_valid())
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
  return _color.to_pmath();
}

//} ... class LineBox
