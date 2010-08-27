#include <pmath-core/numbers-private.h>

#include <pmath-language/patterns-private.h>

#include <pmath-util/concurrency/threads.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/lists-private.h>

#include <limits.h>

typedef struct{
  pmath_bool_t with_heads; // currently not set (allways FALSE)
  long levelmin;
  long levelmax;
}replace_info_t;

static pmath_t apply_rule_list(
  const replace_info_t *info,
  pmath_t               obj,     // will be freed
  pmath_expr_t          rules,   // wont be freed; must have form {a->b, c->d, ...}
  long                  level
){
  int reldepth = _pmath_object_in_levelspec(
    obj, info->levelmin, info->levelmax, level);
  
  if(reldepth > 0)
    return obj;
  
  if(pmath_instance_of(obj, PMATH_TYPE_EXPRESSION)){
    size_t len = pmath_expr_length(obj);
    size_t i;
    
    for(i = info->with_heads ? 0 : 1;i <= len;i++){
      obj = pmath_expr_set_item(
        obj, i,
        apply_rule_list(
          info,
          pmath_expr_get_item(obj, i),
          rules,
          level + 1));
    }
  }
  
  if(reldepth == 0){
    size_t i;
    
    for(i = 1;i <= pmath_expr_length(rules);++i){
      pmath_expr_t rule_i = pmath_expr_get_item(rules, i);
      pmath_t pattern = pmath_expr_get_item(rule_i, 1);
      pmath_t rhs     = pmath_expr_get_item(rule_i, 2);
      
      if(_pmath_pattern_match(obj, pattern, &rhs)){
        pmath_unref(obj);
        pmath_unref(rule_i);
        return rhs;
      }
      pmath_unref(rule_i);
      pmath_unref(rhs);
    }
  }
  return obj;
}

PMATH_PRIVATE pmath_bool_t _pmath_is_rule(pmath_t rule){
  if(!pmath_instance_of(rule, PMATH_TYPE_EXPRESSION))
    return FALSE;
  else if(pmath_expr_length(rule) != 2)
    return FALSE;
  else{
    pmath_t head = pmath_expr_get_item(rule, 0);
    pmath_unref(head);
    return head == PMATH_SYMBOL_RULE || head == PMATH_SYMBOL_RULEDELAYED;
  }
}

PMATH_PRIVATE pmath_bool_t _pmath_is_list_of_rules(pmath_t rules){
  if(pmath_instance_of(rules, PMATH_TYPE_EXPRESSION)){
    size_t i;
    pmath_t head = pmath_expr_get_item(rules, 0);
    pmath_unref(head);
    
    if(head != PMATH_SYMBOL_LIST)
      return FALSE;
      
    for(i = pmath_expr_length(rules);i >= 1;--i){
      pmath_t rule = pmath_expr_get_item(rules, i);
      if(!pmath_instance_of(rule, PMATH_TYPE_EXPRESSION)
      || !_pmath_is_rule(rule)){
        pmath_unref(rule);
        return FALSE;
      }
      pmath_unref(rule);
    }
    return TRUE;
  }
  return FALSE;
}

PMATH_PRIVATE pmath_t builtin_replace(pmath_expr_t expr){
  /* Replace(obj, rules, levelspec)
     Replace(obj, rules, n)  =  Replace(obj, rules, 0..n)
     Replace(obj, rules)     =  Replace(obj, rules, 0..)
     
     same for ReplaceRepeated
     
     options:
       Heads->False
     
     messages:
       General::level
       General::opttf
       General::reps
   */
  replace_info_t info;
  size_t last_nonoption, len = pmath_expr_length(expr);
  pmath_expr_t options;
  pmath_t obj, rules;
  
  if(len < 2){
    pmath_message_argxxx(len, 2, 3);
    return expr;
  }
  
  last_nonoption = 2;
  info.with_heads = FALSE;
  info.levelmin = 0;
  info.levelmax = LONG_MAX;
  if(len > 2){
    pmath_t levels = pmath_expr_get_item(expr, 3);
    
    if(_pmath_extract_levels(levels, &info.levelmin, &info.levelmax)){
      last_nonoption = 3;
    }
    else if(!_pmath_is_rule(levels) && !_pmath_is_list_of_rules(levels)){
      pmath_message(NULL, "level", 1, levels);
      return expr;
    }
    
    pmath_unref(levels);
  }
  
  rules = pmath_expr_get_item(expr, 2);
  if(!_pmath_is_list_of_rules(rules)){
    rules = pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_LIST), 1, rules);
  
    if(!_pmath_is_list_of_rules(rules)){
      pmath_message(NULL, "reps", 1, rules);
      return expr;
    }
  }
  
  options = pmath_options_extract(expr, last_nonoption);
  if(!options){
    pmath_unref(rules);
    return expr;
  }
  
  obj = pmath_evaluate(pmath_option_value(NULL, PMATH_SYMBOL_HEADS, options));
  if(obj == PMATH_SYMBOL_TRUE){
    info.with_heads = TRUE;
  }
  else if(obj != PMATH_SYMBOL_FALSE){
    pmath_unref(rules);
    pmath_unref(options);
    pmath_message(
      NULL, "opttf", 2,
      pmath_ref(PMATH_SYMBOL_HEADS),
      obj);
    return expr;
  }
  pmath_unref(obj);
  pmath_unref(options);
  
  obj = pmath_expr_get_item(expr, 0);
  pmath_unref(obj);
  if(obj == PMATH_SYMBOL_REPLACE){
    obj = pmath_expr_get_item(expr, 1);
    pmath_unref(expr);
    
    obj = apply_rule_list(&info, obj, rules, 0);
    pmath_unref(rules);
    return obj;
  }
  else{ // ReplaceRepeated
    obj = pmath_expr_get_item(expr, 1);
    pmath_unref(expr);
    expr = obj; obj = NULL;
    
    do{
      pmath_unref(obj);
      obj = expr;
      expr = apply_rule_list(&info, pmath_ref(obj), rules, 0);
    }while(!pmath_aborting() && !pmath_equals(obj, expr));
    
    pmath_unref(rules);
    pmath_unref(obj);
    return expr;
  }
}
  
  // replace the  A -> B with A /? If(++i <= maxcount, Emit(B); False, True) -> /\/
  static pmath_expr_t add_maxcount_condition(
    pmath_expr_t rules, // will be freed
    pmath_t maxcount   // will be freed
  ){
    size_t i;
    pmath_symbol_t counter;
    pmath_t counter_ok;
    
    counter = pmath_symbol_create_temporary(
      PMATH_C_STRING("System`Private`replacelistcounter"), 
      TRUE);
    pmath_symbol_set_value(counter, pmath_integer_new_si(0));
    
    counter_ok = pmath_expr_new_extended(
      pmath_ref(PMATH_SYMBOL_LESSEQUAL), 2,
      pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_INCREMENT), 1,
        counter),
      maxcount);
    
    for(i = pmath_expr_length(rules);i > 0;--i){
      pmath_t rule = pmath_expr_get_item(rules, i);
      pmath_t lhs = pmath_expr_get_item(rule, 1);
      pmath_t rhs = pmath_expr_get_item(rule, 2);
      
      lhs = pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_CONDITION), 2,
        lhs,
        pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_IF), 3,
          pmath_ref(counter_ok),
          pmath_expr_new_extended(
            pmath_ref(PMATH_SYMBOL_EVALUATIONSEQUENCE), 2,
            pmath_expr_new_extended(
              pmath_ref(PMATH_SYMBOL_EMIT), 1,
              rhs),
            pmath_ref(PMATH_SYMBOL_FALSE)),
          pmath_ref(PMATH_SYMBOL_TRUE)));
      
      rule  = pmath_expr_set_item(rule, 1, lhs);
      rules = pmath_expr_set_item(rules, i, rule);
    }
    
    pmath_unref(counter_ok);
    
    return rules;
  }

PMATH_PRIVATE pmath_t builtin_replacelist(pmath_expr_t expr){
  /* ReplaceList(obj, rules, n)
     ReplaceList(obj, rules)     =  ReplaceList(obj, rules, Infinity)
     
     messages:
       General::innf
       General::opttf
       General::reps
   */
  replace_info_t info;
  size_t last_nonoption, len = pmath_expr_length(expr);
  pmath_expr_t options;
  pmath_t obj, rules, n;
  
  if(len < 2){
    pmath_message_argxxx(len, 2, 3);
    return expr;
  }
  
  last_nonoption = 2;
  info.with_heads = FALSE;
  info.levelmin = 0;
  info.levelmax = LONG_MAX;
  if(len > 3){
    n = pmath_expr_get_item(expr, 3);
    
    if(!_pmath_is_rule(n) && !_pmath_is_list_of_rules(n)){
      last_nonoption = 3;
    }
    else if((!pmath_instance_of(n, PMATH_TYPE_INTEGER)
     || pmath_number_sign(n) < 0)
    && !pmath_equals(n, _pmath_object_infinity)){
      pmath_message(NULL, "innf", 2, pmath_integer_new_si(3), pmath_ref(expr));
      pmath_unref(n);
      return expr;
    }
  }
  else
    n = pmath_ref(_pmath_object_infinity);
  
  rules = pmath_expr_get_item(expr, 2);
  if(!_pmath_is_list_of_rules(rules)){
    rules = pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_LIST), 1, rules);
  
    if(!_pmath_is_list_of_rules(rules)){
      pmath_message(NULL, "reps", 1, rules);
      pmath_unref(n);
      return expr;
    }
  }
  
  options = pmath_options_extract(expr, last_nonoption);
  if(!options){
    pmath_unref(rules);
    pmath_unref(n);
    return expr;
  }
  
  obj = pmath_evaluate(pmath_option_value(NULL, PMATH_SYMBOL_HEADS, options));
  if(obj == PMATH_SYMBOL_TRUE){
    info.with_heads = TRUE;
  }
  else if(obj != PMATH_SYMBOL_FALSE){
    pmath_unref(rules);
    pmath_unref(options);
    pmath_message(
      NULL, "opttf", 2,
      pmath_ref(PMATH_SYMBOL_HEADS),
      obj);
    return expr;
  }
  pmath_unref(obj);
  pmath_unref(options);

  obj = pmath_expr_get_item(expr, 1);
  pmath_unref(expr);
  
  rules = add_maxcount_condition(rules, n);
  
  pmath_gather_begin(NULL);
  pmath_unref(apply_rule_list(&info, obj, rules, 0));
  pmath_unref(rules);
  return pmath_gather_end();
}
