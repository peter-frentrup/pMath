#include <pmath-core/symbols-private.h>

#include <pmath-language/patterns-private.h>

#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>
#include <pmath-util/symbol-values-private.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/control/definitions-private.h>

PMATH_PRIVATE pmath_t builtin_assign_default(pmath_expr_t expr){
  struct _pmath_symbol_rules_t *rules;
  pmath_t tag;
  pmath_t lhs;
  pmath_t rhs;
  pmath_t sym;
  
  if(!_pmath_is_assignment(expr, &tag, &lhs, &rhs))
    return expr;
  
  if(!pmath_is_expr_of(lhs, PMATH_SYMBOL_DEFAULT)
  || pmath_expr_length(lhs) < 1
  || pmath_expr_length(lhs) > 3){
    pmath_unref(tag);
    pmath_unref(lhs);
    pmath_unref(rhs);
    return expr;
  }
  
  sym = pmath_expr_get_item(lhs, 1);
  
  if(tag != PMATH_UNDEFINED && tag != sym){
    pmath_message(NULL, "tag", 2, tag, lhs);
    
    pmath_unref(sym);
    pmath_unref(expr);
    if(rhs == PMATH_UNDEFINED)
      return pmath_ref(PMATH_SYMBOL_FAILED);
    return rhs;
  }
  
  pmath_unref(tag);
  pmath_unref(expr);
  
  if(pmath_instance_of(sym, PMATH_TYPE_STRING))
    sym = pmath_symbol_find(sym, FALSE);
  
  if(!pmath_instance_of(sym, PMATH_TYPE_SYMBOL)){
    pmath_message(NULL, "fnsym", 1, lhs);
    
    pmath_unref(sym);
    if(rhs == PMATH_UNDEFINED)
      return pmath_ref(PMATH_SYMBOL_FAILED);
    return rhs;
  }
  
  rules = _pmath_symbol_get_rules(sym, RULES_WRITE);
  pmath_unref(sym);
  
  if(!rules){
    pmath_unref(lhs);
    pmath_unref(rhs);
    return pmath_ref(PMATH_SYMBOL_FAILED);
  }
  
  _pmath_rulecache_change(&rules->default_rules, lhs, rhs);
  
  return NULL;
}

PMATH_PRIVATE pmath_t builtin_default(pmath_expr_t expr){
/* Default(f)
   Default(f, i)
   Default(f, i, n)
 */
  struct _pmath_symbol_rules_t *rules;
  pmath_symbol_t sym;
  size_t exprlen = pmath_expr_length(expr); 
  
  if(exprlen < 1 || exprlen > 3){
    pmath_message_argxxx(exprlen, 1, 3);
    return expr;
  }
  
  sym = pmath_expr_get_item(expr, 1);
  
  if(pmath_instance_of(sym, PMATH_TYPE_STRING))
    sym = pmath_symbol_find(sym, FALSE);
  
  if(!pmath_instance_of(sym, PMATH_TYPE_SYMBOL)){
    pmath_unref(sym);
    pmath_message(NULL, "fnsym", 1, pmath_ref(expr));
    return expr;
  }
  
  rules = _pmath_symbol_get_rules(sym, RULES_READ);
  pmath_unref(sym);
  
  if(rules){
    while(!_pmath_rulecache_find(&rules->default_rules, &expr)
    && pmath_expr_length(expr) > 1){
      pmath_t tmp = expr;
      expr = pmath_expr_get_item_range(tmp, 1, pmath_expr_length(expr) - 1);
      pmath_unref(tmp);
    }
  }
  
  return expr;
}
