#include <boxes/box.h>


using namespace richmath;


Expr richmath_eval_FrontEnd_Options(Expr expr) {
  size_t len = expr.expr_length();
  if(len < 1 || len > 2)
    return Symbol(PMATH_SYMBOL_FAILED);
  
  auto ref = FrontEndReference::from_pmath(expr[1]);
  Box *box = FrontEndObject::find_cast<Box>(ref);
  
  if(box) {
    Gather gather;
    
    if(box->style)
      box->style->emit_to_pmath(true);
      
    Expr options = gather.end();
    if(!box->to_pmath_symbol().is_symbol())
      return options;
      
    Expr default_options =
      Call(Symbol(PMATH_SYMBOL_UNION),
           options,
           Call(Symbol(PMATH_SYMBOL_FILTERRULES),
                Call(Symbol(PMATH_SYMBOL_OPTIONS), box->to_pmath_symbol()),
                Call(Symbol(PMATH_SYMBOL_EXCEPT),
                     options)));
                     
    default_options = Expr(pmath_evaluate(default_options.release()));
    if(len == 2) {
      expr.set(0, Symbol(PMATH_SYMBOL_OPTIONS));
      expr.set(1, std::move(default_options));
      return expr;
    }
    
    return default_options;
  }
  
  return Symbol(PMATH_SYMBOL_FAILED);
}

Expr richmath_eval_FrontEnd_SetOptions(Expr expr) {
  if(expr.expr_length() < 1)
    return Symbol(PMATH_SYMBOL_FAILED);
  
  auto ref = FrontEndReference::from_pmath(expr[1]);
  Box *box = FrontEndObject::find_cast<Box>(ref);
  
  if(box) {
    Expr options = Expr(pmath_expr_get_item_range(expr.get(), 2, SIZE_MAX));
    options.set(0, Symbol(PMATH_SYMBOL_LIST));
    
    if(!box->style)
      box->style = new Style();
      
    box->style->add_pmath(options);
    box->invalidate_options();
    
    return options;
  }
  
  return Symbol(PMATH_SYMBOL_FAILED);
}
