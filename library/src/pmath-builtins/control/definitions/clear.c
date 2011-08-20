#include <pmath-core/symbols-private.h>

#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>
#include <pmath-util/symbol-values-private.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/control/definitions-private.h>


PMATH_PRIVATE
pmath_bool_t _pmath_clear(pmath_symbol_t sym, pmath_bool_t all) { // sym wont be freed
  struct _pmath_symbol_rules_t  *rules;
  
  if(!_pmath_symbol_assign_value(sym, pmath_ref(sym), PMATH_UNDEFINED))
    return FALSE;
    
  rules = _pmath_symbol_get_rules(sym, RULES_READ);
  
  // do nothing if there are no rules
  if(rules) {
    rules = _pmath_symbol_get_rules(sym, RULES_WRITE);
    if(rules) {
      if(all) {
        _pmath_symbol_rules_clear(rules);
      }
      else {
        // not clearing default_rules and messages
        
        _pmath_rulecache_clear(&rules->up_rules);
        _pmath_rulecache_clear(&rules->down_rules);
        _pmath_rulecache_clear(&rules->sub_rules);
        _pmath_rulecache_clear(&rules->approx_rules);
        _pmath_rulecache_clear(&rules->format_rules);
      }
    }
  }
  
  if(all) {
    // clear all attributes except Temporary and ThreadLocal, if they exist
    pmath_symbol_attributes_t attr = pmath_symbol_get_attributes(sym);
    pmath_symbol_set_attributes(sym, attr & (PMATH_SYMBOL_ATTRIBUTE_TEMPORARY | PMATH_SYMBOL_ATTRIBUTE_THREADLOCAL | PMATH_SYMBOL_ATTRIBUTE_REMOVED));
  }
  
  return TRUE;
}

PMATH_PRIVATE pmath_t builtin_clear(pmath_expr_t expr) {
  pmath_bool_t all = pmath_is_expr_of(expr, PMATH_SYMBOL_CLEARALL);
  size_t i;
  
  for(i = 1; i <= pmath_expr_length(expr); ++i) {
    pmath_t item = pmath_expr_get_item(expr, i);
    
    if(pmath_is_symbol(item)) {
      _pmath_clear(item, all);
      pmath_unref(item);
    }
    else if(pmath_is_string(item)) {
      pmath_t known = pmath_evaluate(
                        pmath_expr_new_extended(
                          pmath_ref(PMATH_SYMBOL_NAMES), 1,
                          item));
                          
      if(pmath_is_expr_of(known, PMATH_SYMBOL_LIST)
          && pmath_expr_length(known) > 0) {
        size_t j;
        for(j = pmath_expr_length(known); j > 0; --j) {
          pmath_t sym = pmath_symbol_get(
                          pmath_expr_get_item(known, j), FALSE);
          _pmath_clear(sym, all);
          pmath_unref(sym);
        }
        
        pmath_unref(known);
      }
      else {
        pmath_unref(known);
        pmath_message(PMATH_NULL, "notfound", 1, pmath_expr_get_item(expr, i));
      }
    }
    else
      pmath_message(PMATH_NULL, "ssym", 1, item);
  }
  
  pmath_unref(expr);
  return PMATH_NULL;
}
