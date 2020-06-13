#include <pmath-language/patterns-private.h>

#include <pmath-util/dispatch-tables.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/control-private.h>


static pmath_t prepare_pattern(pmath_t pattern){ // will be freed
  if(pmath_is_expr_of(pattern, PMATH_SYMBOL_LIST)){
    size_t i;
    pattern = pmath_expr_set_item(pattern, 0, pmath_ref(PMATH_SYMBOL_ALTERNATIVES));
    
    for(i = pmath_expr_length(pattern);i > 0;--i){
      pmath_t rule = pmath_expr_get_item(pattern, i);
      
      if(_pmath_is_rule(rule)){
        pattern = pmath_expr_set_item(pattern, i,
          pmath_expr_get_item(rule, 1));
      }
      
      pmath_unref(rule);
    }
    
    return pattern;
  }
  
  if(_pmath_is_rule(pattern)){
    pmath_t tmp = pmath_expr_get_item(pattern, 1);
    pmath_unref(pattern);
    return tmp;
  }
  
  if(pmath_is_expr_of_len(pattern, PMATH_SYMBOL_EXCEPT, 1)){
    pmath_t sub = pmath_expr_get_item(pattern, 1);
    pmath_unref(pattern);
    
    sub = prepare_pattern(sub);
    pattern = pmath_expr_new_extended(
      pmath_ref(PMATH_SYMBOL_EXCEPT), 1,
      sub);
    return pattern;
  }
  
  return pattern;
}

PMATH_PRIVATE pmath_t builtin_filterrules(pmath_expr_t expr){
  pmath_t rules, pattern;
  size_t i;
  
  if(pmath_expr_length(expr) != 2){
    pmath_message_argxxx(pmath_expr_length(expr), 2, 2);
    return expr;
  }
  
  rules = pmath_expr_get_item(expr, 1);
  if(_pmath_is_rule(rules)){
    rules = pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_LIST), 1, rules);
  }
  else if(!pmath_is_list_of_rules(rules)){
    pmath_message(PMATH_NULL, "reps", 1, rules);
    return expr;
  }
  
  pattern = pmath_expr_get_item(expr, 2);
  if(!_pmath_pattern_validate(pattern)) {
    pmath_unref(pattern);
    pmath_unref(rules);
    return expr;
  }
  pmath_unref(expr);
  
  pattern = prepare_pattern(pattern);
  
  for(i = pmath_expr_length(rules);i > 0;--i){
    pmath_t rule = pmath_expr_get_item(rules, i);
    pmath_t lhs  = pmath_expr_get_item(rule, 1);
    pmath_unref(rule);
    
    if(!_pmath_pattern_match(lhs, pmath_ref(pattern), NULL)){
      rules = pmath_expr_set_item(rules, i, PMATH_UNDEFINED);
    }
    
    pmath_unref(lhs);
  }
  
  pmath_unref(pattern);
  return pmath_expr_remove_all(rules, PMATH_UNDEFINED);
}
