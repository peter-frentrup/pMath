#include <boxes/graphics/graphicserrorbox.h>
#include <boxes/graphics/graphicsdrawingcontext.h>


namespace richmath { namespace strings {
  extern String Error_BadArgumentSomewhere;
  extern String Error_BadGraphicsElement;
  extern String Error_arg1;
  extern String Error_argmu;
  extern String Error_argm;
  extern String Error_argru;
  extern String Error_argr;
  extern String Error_argtu;
  extern String Error_argt;
  extern String Error_argxu;
  extern String Error_argx;
}}

using namespace richmath;

extern pmath_symbol_t richmath_System_StringForm;

//{ class GraphicsErrorBox ...

GraphicsErrorBox::GraphicsErrorBox(Expr expr, Expr error_message)
: base(),
  _expr(expr),
  _error_message(error_message)
{
}

bool GraphicsErrorBox::try_load_from_object(Expr expr, BoxInputFlags opts) {
  return (_expr == expr);
}

Expr GraphicsErrorBox::message_badhead(Expr expr) {
  return Call(Symbol(richmath_System_StringForm), 
    strings::Error_BadGraphicsElement, 
    expr.is_expr() ? expr[0] : expr);
}

Expr GraphicsErrorBox::message_badarg(Expr expr) {
  return Call(Symbol(richmath_System_StringForm), 
    strings::Error_BadArgumentSomewhere, 
    expr[0]);
}

Expr GraphicsErrorBox::message_argxxx(Expr expr, size_t min, size_t max) {
  size_t given = expr.expr_length();
  
  // better defer to pmath_message_argxxx() and capture Message output?
  
  if(given == 1) {
    if(min == max)
      return Call(Symbol(richmath_System_StringForm), strings::Error_argxu, expr[0], min);
      
    if(min + 1 == max)
      return Call(Symbol(richmath_System_StringForm), strings::Error_argtu, expr[0], min, max);
      
    if(max == SIZE_MAX)
      return Call(Symbol(richmath_System_StringForm), strings::Error_argmu, expr[0], min);
      
    return Call(Symbol(richmath_System_StringForm), strings::Error_argru, expr[0], min, max);
  }
  
  if(min == max) {
    if(min == 1)
      return Call(Symbol(richmath_System_StringForm), strings::Error_arg1, expr[0], given);
      
    return Call(Symbol(richmath_System_StringForm), strings::Error_arg1, expr[0], given, min);
  }
  
  if(min + 1 == max)
    return Call(Symbol(richmath_System_StringForm), strings::Error_argt, expr[0], given, min, max);
  
  if(max == SIZE_MAX)
    return Call(Symbol(richmath_System_StringForm), strings::Error_argm, expr[0], given, min);
    
  return Call(Symbol(richmath_System_StringForm), strings::Error_argr, expr[0], given, min, max);
}

void GraphicsErrorBox::find_extends(GraphicsBounds &bounds) {
}

void GraphicsErrorBox::paint(GraphicsDrawingContext &gc) {
  gc.add_paint_error(_error_message);
}

//} ... class GraphicsErrorBox
