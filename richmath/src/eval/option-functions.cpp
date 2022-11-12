#include <boxes/box.h>


using namespace richmath;


extern pmath_symbol_t richmath_System_DollarFailed;
extern pmath_symbol_t richmath_System_Except;
extern pmath_symbol_t richmath_System_FilterRules;
extern pmath_symbol_t richmath_System_List;
extern pmath_symbol_t richmath_System_Options;
extern pmath_symbol_t richmath_System_Union;

Expr richmath_eval_FrontEnd_Options(Expr expr) {
  size_t len = expr.expr_length();
  if(len < 1 || len > 2)
    return Symbol(richmath_System_DollarFailed);
  
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
          default_options = Call(Symbol(richmath_System_Options), PMATH_CPP_MOVE(sym));
      }
      
      if(default_options) {
        need_filter = true;
        options = Call(Symbol(richmath_System_Union),
                    options,
                    Call(Symbol(richmath_System_FilterRules),
                         PMATH_CPP_MOVE(default_options),
                         Call(Symbol(richmath_System_Except),
                              options)));
      }
    }
    
    if(len == 2 && need_filter) {
      expr.set(0, Symbol(richmath_System_Options));
      expr.set(1, PMATH_CPP_MOVE(options));
      return expr;
    }
    
    return options;
  }
  
  return Symbol(richmath_System_DollarFailed);
}

Expr richmath_eval_FrontEnd_SetOptions(Expr expr) {
  if(expr.expr_length() < 1)
    return Symbol(richmath_System_DollarFailed);
  
  auto ref = FrontEndReference::from_pmath(expr[1]);
  auto obj = FrontEndObject::find_cast<ActiveStyledObject>(ref);
  
  if(obj) {
    Expr options = Expr(pmath_expr_get_item_range(expr.get(), 2, SIZE_MAX));
    options.set(0, Symbol(richmath_System_List));
    
    if(!obj->style)
      obj->style = new Style();
      
    obj->style->add_pmath(options, false);
    obj->on_style_changed(true); // TODO: check if only non-layout options were affected
    
    return options;
  }
  
  return Symbol(richmath_System_DollarFailed);
}
