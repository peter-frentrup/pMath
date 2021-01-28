#include <boxes/graphics/graphicsdirective.h>

#include <graphics/context.h>


using namespace richmath;

extern pmath_symbol_t richmath_System_Hue;

//{ class GraphicsDirective ...

GraphicsDirective::GraphicsDirective()
  : base()
{
}

GraphicsDirective::GraphicsDirective(Expr expr)
  : base(),
    _expr(expr)
{
}

bool GraphicsDirective::is_graphics_directive(Expr expr) {
  if(expr[0] == PMATH_SYMBOL_DIRECTIVE)
    return true;
  
  if(expr[0] == PMATH_SYMBOL_RGBCOLOR)
    return true;
  
  if(expr[0] == richmath_System_Hue)
    return true;
  
  if(expr[0] == PMATH_SYMBOL_GRAYLEVEL)
    return true;
  
  return false;
}

bool GraphicsDirective::try_load_from_object(Expr expr, BoxInputFlags opts) {
  if(!is_graphics_directive(expr))
    return false;
  
  _expr = expr;
  
  finish_load_from_object(std::move(expr));
  return true;
}

GraphicsDirective *GraphicsDirective::try_create(Expr expr, BoxInputFlags opts) {
  GraphicsDirective *box = new GraphicsDirective();
  
  if(!box->try_load_from_object(expr, opts)) {
    delete box;
    return nullptr;
  }
  
  return box;
}

void GraphicsDirective::paint(GraphicsBox *owner, Context &context) {
  apply(_expr, context);
}

void GraphicsDirective::apply(Expr directive, Context &context) {
  if(directive[0] == PMATH_SYMBOL_DIRECTIVE) {
    for(auto item : directive.items())
      apply(item, context);
    return;
  }
  
  if(directive[0] == PMATH_SYMBOL_RGBCOLOR || directive[0] == richmath_System_Hue || directive[0] == PMATH_SYMBOL_GRAYLEVEL) {
    if(Color c = Color::from_pmath(directive)) {
      context.canvas().set_color(c);
    }
    return;
  }
}

//} ... class GraphicsDirective
