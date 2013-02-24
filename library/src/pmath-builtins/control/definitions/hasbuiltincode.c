#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/control/definitions-private.h>

static pmath_code_usage_t kind_to_usage(pmath_symbol_t kind) {
  if(pmath_same(kind, PMATH_SYMBOL_DOWNRULES))
    return PMATH_CODE_USAGE_DOWNCALL;
    
  if(pmath_same(kind, PMATH_SYMBOL_UPRULES))
    return PMATH_CODE_USAGE_UPCALL;
    
  if(pmath_same(kind, PMATH_SYMBOL_SUBRULES))
    return PMATH_CODE_USAGE_SUBCALL;
    
  if(pmath_same(kind, PMATH_SYMBOL_NRULES))
    return PMATH_CODE_USAGE_APPROX;
    
  return (pmath_code_usage_t)(-1);
}


PMATH_PRIVATE pmath_t builtin_developer_hasbuiltincode(pmath_expr_t expr) {
  /* Developer`HasBuiltinCode(symbol)
     Developer`HasBuiltinCode("name")
     Developer`HasBuiltinCode(sym, DownRules)
     Developer`HasBuiltinCode(sym, SubRules)
     Developer`HasBuiltinCode(sym, UpRules)
     Developer`HasBuiltinCode(sym, NRules)
  
     messages:
      Developer`HasBuiltinCode::norul
      General::fnsym
   */
  
  size_t exprlen;
  pmath_t sym, kind;
  pmath_code_usage_t usage;
  
  exprlen = pmath_expr_length(expr);
  
  if(exprlen < 1 || exprlen > 2) {
    pmath_message_argxxx(exprlen, 1, 2);
    return expr;
  }
  
  sym = pmath_expr_get_item(expr, 1);
  if(pmath_is_string(sym))
    sym = pmath_symbol_find(sym, FALSE);
    
  if(!pmath_is_symbol(sym)) {
    pmath_message(PMATH_NULL, "fnsym", 1, pmath_ref(expr));
    
    pmath_unref(sym);
    return expr;
  }
  
  if(exprlen == 1) {
    pmath_unref(expr);
    
    if( _pmath_have_code(sym, PMATH_CODE_USAGE_DOWNCALL) ||
        _pmath_have_code(sym, PMATH_CODE_USAGE_UPCALL) ||
        _pmath_have_code(sym, PMATH_CODE_USAGE_SUBCALL) ||
        _pmath_have_code(sym, PMATH_CODE_USAGE_APPROX))
    {
      pmath_unref(sym);
      return pmath_ref(PMATH_SYMBOL_TRUE);
    }
    
    pmath_unref(sym);
    return pmath_ref(PMATH_SYMBOL_FALSE);
  }
  
  kind = pmath_expr_get_item(expr, 2);
  pmath_unref(expr);
  
  if(pmath_is_expr_of(kind, PMATH_SYMBOL_LIST)) {
    size_t i;
    
    for(i = 1; i <= pmath_expr_length(kind); ++i) {
      pmath_t sub_kind = pmath_expr_get_item(kind, i);
      
      usage = kind_to_usage(sub_kind);
      
      if((int)usage < 0) {
        pmath_message(PMATH_NULL, "norul", 1, sub_kind);
        continue;
      }
      
      pmath_unref(sub_kind);
      if(_pmath_have_code(sym, usage)) {
        pmath_unref(kind);
        pmath_unref(sym);
        return pmath_ref(PMATH_SYMBOL_TRUE);
      }
    }
    
    pmath_unref(kind);
    pmath_unref(sym);
    return pmath_ref(PMATH_SYMBOL_FALSE);
  }
  
  usage = kind_to_usage(kind);
  if((int)usage < 0) {
    pmath_message(PMATH_NULL, "norul", 1, kind);
    pmath_unref(sym);
    return pmath_ref(PMATH_SYMBOL_FALSE);
  }
  
  pmath_unref(kind);
  if(_pmath_have_code(sym, usage)) {
    pmath_unref(sym);
    return pmath_ref(PMATH_SYMBOL_TRUE);
  }
  
  pmath_unref(sym);
  return pmath_ref(PMATH_SYMBOL_FALSE);
}
