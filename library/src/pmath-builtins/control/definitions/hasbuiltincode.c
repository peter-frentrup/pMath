#include <pmath-core/symbols-private.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/control/definitions-private.h>

#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>
#include <pmath-util/symbol-values-private.h>


extern pmath_symbol_t pmath_System_DownRules;
extern pmath_symbol_t pmath_System_False;
extern pmath_symbol_t pmath_System_List;
extern pmath_symbol_t pmath_System_NRules;
extern pmath_symbol_t pmath_System_SubRules;
extern pmath_symbol_t pmath_System_True;
extern pmath_symbol_t pmath_System_UpRules;

static pmath_code_usage_t kind_to_usage(pmath_symbol_t kind) {
  if(pmath_same(kind, pmath_System_DownRules))
    return PMATH_CODE_USAGE_DOWNCALL;
    
  if(pmath_same(kind, pmath_System_UpRules))
    return PMATH_CODE_USAGE_UPCALL;
    
  if(pmath_same(kind, pmath_System_SubRules))
    return PMATH_CODE_USAGE_SUBCALL;
    
  if(pmath_same(kind, pmath_System_NRules))
    return PMATH_CODE_USAGE_APPROX;
    
  return (pmath_code_usage_t)(-1);
}

static pmath_bool_t has_code(const struct _pmath_symbol_rules_t *rules, pmath_code_usage_t kind) {
  switch(kind) {
    case PMATH_CODE_USAGE_EARLYCALL:
    case PMATH_CODE_USAGE_DOWNCALL: return 0 != pmath_atomic_read_aquire(&rules->early_call) ||
                                           0 != pmath_atomic_read_aquire(&rules->down_call);
    case PMATH_CODE_USAGE_UPCALL:   return 0 != pmath_atomic_read_aquire(&rules->up_call);
    case PMATH_CODE_USAGE_SUBCALL:  return 0 != pmath_atomic_read_aquire(&rules->sub_call);
    case PMATH_CODE_USAGE_APPROX:   return 0 != pmath_atomic_read_aquire(&rules->approx_call);
  }
  return FALSE;
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
  
  struct _pmath_symbol_rules_t *rules;
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
  
  rules = _pmath_symbol_get_rules(sym, RULES_READ);
  if(!rules) {
    pmath_unref(expr);
    pmath_unref(sym);
    return pmath_ref(pmath_System_False);
  }
  
  if(exprlen == 1) {
    pmath_unref(expr);
    
    if( 0 != pmath_atomic_read_aquire(&rules->early_call) ||
        0 != pmath_atomic_read_aquire(&rules->up_call) ||
        0 != pmath_atomic_read_aquire(&rules->down_call) ||
        0 != pmath_atomic_read_aquire(&rules->sub_call) ||
        0 != pmath_atomic_read_aquire(&rules->approx_call))
    {
      pmath_unref(sym);
      return pmath_ref(pmath_System_True);
    }
    
    pmath_unref(sym);
    return pmath_ref(pmath_System_False);
  }
  
  kind = pmath_expr_get_item(expr, 2);
  pmath_unref(expr);
  
  if(pmath_is_expr_of(kind, pmath_System_List)) {
    size_t i;
    
    for(i = 1; i <= pmath_expr_length(kind); ++i) {
      pmath_t sub_kind = pmath_expr_get_item(kind, i);
      
      usage = kind_to_usage(sub_kind);
      
      if((int)usage < 0) {
        pmath_message(PMATH_NULL, "norul", 1, sub_kind);
        continue;
      }
      
      pmath_unref(sub_kind);
      if(has_code(rules, usage)) {
        pmath_unref(kind);
        pmath_unref(sym);
        return pmath_ref(pmath_System_True);
      }
    }
    
    pmath_unref(kind);
    pmath_unref(sym);
    return pmath_ref(pmath_System_False);
  }
  
  usage = kind_to_usage(kind);
  if((int)usage < 0) {
    pmath_message(PMATH_NULL, "norul", 1, kind);
    pmath_unref(sym);
    return pmath_ref(pmath_System_False);
  }
  
  pmath_unref(kind);
  if(has_code(rules, usage)) {
    pmath_unref(sym);
    return pmath_ref(pmath_System_True);
  }
  
  pmath_unref(sym);
  return pmath_ref(pmath_System_False);
}
