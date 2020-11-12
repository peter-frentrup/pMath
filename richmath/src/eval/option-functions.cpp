#include <boxes/box.h>


using namespace richmath;


Expr richmath_eval_FrontEnd_Options(Expr expr) {
  size_t len = expr.expr_length();
  if(len < 1 || len > 2)
    return Symbol(PMATH_SYMBOL_FAILED);
  
  auto ref = FrontEndReference::from_pmath(expr[1]);
  auto obj = FrontEndObject::find_cast<StyledObject>(ref);
  
  if(obj) {
    Gather gather;
    bool add_defaults = true;
    bool need_filter = true;
    
    if(auto s = obj->own_style()) {
      bool emit_all = true;
      if(len == 2) {
        StyleOptionName key = Style::get_key(expr[2]);
        if(key.is_valid()) {
          s->emit_pmath(key);
          emit_all = false;
          add_defaults = false;
          need_filter = false;
        }
      }
      
      if(emit_all)
        s->emit_to_pmath(true);
    }
      
    Expr options = gather.end();
    if(add_defaults || options.expr_length() == 0) {
      Expr default_options;
      if(auto box = dynamic_cast<Box*>(obj)) {
        Expr sym = box->to_pmath_symbol();
        if(sym.is_symbol())
          default_options = Call(Symbol(PMATH_SYMBOL_OPTIONS), std::move(sym));
      }
      
      if(default_options) {
        need_filter = true;
        options = Call(Symbol(PMATH_SYMBOL_UNION),
                    options,
                    Call(Symbol(PMATH_SYMBOL_FILTERRULES),
                         std::move(default_options),
                         Call(Symbol(PMATH_SYMBOL_EXCEPT),
                              options)));
      }
    }
    
    if(len == 2 && need_filter) {
      expr.set(0, Symbol(PMATH_SYMBOL_OPTIONS));
      expr.set(1, std::move(options));
      return expr;
    }
    
    return options;
  }
  
  return Symbol(PMATH_SYMBOL_FAILED);
}

Expr richmath_eval_FrontEnd_SetOptions(Expr expr) {
  if(expr.expr_length() < 1)
    return Symbol(PMATH_SYMBOL_FAILED);
  
  auto ref = FrontEndReference::from_pmath(expr[1]);
  auto obj = FrontEndObject::find_cast<ActiveStyledObject>(ref);
  
  if(obj) {
    Expr options = Expr(pmath_expr_get_item_range(expr.get(), 2, SIZE_MAX));
    options.set(0, Symbol(PMATH_SYMBOL_LIST));
    
    if(!obj->style)
      obj->style = new Style();
      
    obj->style->add_pmath(options);
    obj->invalidate_options();
    
    return options;
  }
  
  return Symbol(PMATH_SYMBOL_FAILED);
}
