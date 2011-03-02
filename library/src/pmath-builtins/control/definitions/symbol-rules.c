#include <pmath-core/symbols-private.h>

#include <pmath-util/emit-and-gather.h>
#include <pmath-util/symbol-values-private.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/control/definitions-private.h>
#include <pmath-builtins/control-private.h>

PMATH_PRIVATE
pmath_t _pmath_extract_holdpattern(pmath_t pat){
  if(pmath_is_expr_of_len(pat, PMATH_SYMBOL_HOLDPATTERN, 1)){
    pmath_t result = pmath_expr_get_item(pat, 1);
    pmath_unref(pat);
    return result;
  }
  
  return pat;
}

PMATH_PRIVATE pmath_t builtin_assign_ownrules(pmath_expr_t expr){
  pmath_t tag;
  pmath_t lhs;
  pmath_t rhs;
  pmath_t sym;
  
  if(!_pmath_is_assignment(expr, &tag, &lhs, &rhs))
    return expr;
  
  if(!pmath_is_expr_of_len(lhs, PMATH_SYMBOL_OWNRULES, 1)){
    pmath_unref(tag);
    pmath_unref(lhs);
    pmath_unref(rhs);
    return expr;
  }
  
  sym = pmath_expr_get_item(lhs, 1);
  
  if(!pmath_same(tag, PMATH_UNDEFINED)
  && !pmath_same(tag, sym)){
    pmath_message(PMATH_NULL, "tag", 3, tag, lhs, sym);
    
    pmath_unref(expr);
    if(pmath_same(rhs, PMATH_UNDEFINED))
      return pmath_ref(PMATH_SYMBOL_FAILED);
    return rhs;
  }
  
  pmath_unref(tag);
  pmath_unref(expr);
  
  if(pmath_is_string(sym))
    sym = pmath_symbol_find(sym, FALSE);
  
  if(!pmath_is_symbol(sym)){
    pmath_message(PMATH_NULL, "fnsym", 1, lhs);
    
    pmath_unref(sym);
    if(pmath_same(rhs, PMATH_UNDEFINED))
      return pmath_ref(PMATH_SYMBOL_FAILED);
    return rhs;
  }
  
  if(pmath_symbol_get_attributes(sym) & PMATH_SYMBOL_ATTRIBUTE_PROTECTED){
    pmath_message(PMATH_NULL, "wrsym", 1, sym);
    pmath_unref(lhs);
    pmath_unref(rhs);
    return pmath_ref(PMATH_SYMBOL_FAILED);
  }
  
  if(pmath_same(rhs, PMATH_UNDEFINED)){
    pmath_symbol_set_value(sym, PMATH_UNDEFINED);
    pmath_unref(sym);
    pmath_unref(lhs);
    return PMATH_NULL;
  }
  
  if(_pmath_is_rule(rhs)){
    pmath_t rule_rhs;
      
    pmath_unref(lhs);
    tag = PMATH_UNDEFINED;
    
    lhs = _pmath_extract_holdpattern(pmath_expr_get_item(rhs, 1));
    rule_rhs = pmath_expr_get_item(rhs, 2);
    
    _pmath_symbol_define_value_pos(&tag, lhs, rule_rhs);
    
    pmath_unref(rhs);
    
    pmath_symbol_set_value(sym, pmath_ref(tag));
    
    pmath_gather_begin(PMATH_NULL);
    _pmath_symbol_value_emit(sym, tag);
    pmath_unref(sym);
    return pmath_gather_end();
  }
  
  if(_pmath_is_list_of_rules(rhs)){
    size_t i;
    
    pmath_unref(lhs);
    tag = PMATH_UNDEFINED;
    for(i = 1;i <= pmath_expr_length(rhs);++i){
      pmath_t rule;
      pmath_t rule_rhs;
      
      rule = pmath_expr_get_item(rhs, i);
      lhs = _pmath_extract_holdpattern(pmath_expr_get_item(rule, 1));
      rule_rhs = pmath_expr_get_item(rule, 2);
      
      _pmath_symbol_define_value_pos(&tag, lhs, rule_rhs);
      
      pmath_unref(rule);
    }
    
    pmath_unref(rhs);
    
    pmath_symbol_set_value(sym, pmath_ref(tag));
      
    pmath_gather_begin(PMATH_NULL);
    _pmath_symbol_value_emit(sym, tag);
    pmath_unref(sym);
    return pmath_gather_end();
  }
  
  pmath_unref(sym);
  pmath_message(PMATH_NULL, "vlist", 2, lhs, rhs);
    
  return pmath_ref(PMATH_SYMBOL_FAILED);
}

PMATH_PRIVATE pmath_t builtin_assign_symbol_rules(pmath_expr_t expr){
/* Note that these assignments are not atomic:
   While we assign e.g. DownRules(x):= {p1->v1, p2->v2}, another thread could
   interferre after p1->v1 is applied and before p2->v2 is applied.
 */
  struct _pmath_symbol_rules_t *rules;
  struct _pmath_rulecache_t    *rc;
  pmath_t tag;
  pmath_t lhs;
  pmath_t rhs;
  pmath_t kind;
  pmath_t sym;
  
  if(!_pmath_is_assignment(expr, &tag, &lhs, &rhs))
    return expr;
  
  if(pmath_is_expr(lhs)){
    kind = pmath_expr_get_item(lhs, 0);
    pmath_unref(kind);
  }
  else
    kind = PMATH_NULL;
  
  if(!pmath_same(kind, PMATH_SYMBOL_DEFAULTRULES)
  && !pmath_same(kind, PMATH_SYMBOL_DOWNRULES)
  && !pmath_same(kind, PMATH_SYMBOL_FORMATRULES)
  && !pmath_same(kind, PMATH_SYMBOL_NRULES)
  && !pmath_same(kind, PMATH_SYMBOL_SUBRULES)
  && !pmath_same(kind, PMATH_SYMBOL_UPRULES)){
    pmath_unref(tag);
    pmath_unref(lhs);
    pmath_unref(rhs);
    return expr;
  }
  
  if(pmath_expr_length(lhs) == 1){
    sym = pmath_expr_get_item(lhs, 1);
    
    if(!pmath_same(tag, PMATH_UNDEFINED)
    && !pmath_same(tag, sym)){
      pmath_message(PMATH_NULL, "tag", 3, tag, lhs, sym);
      
      pmath_unref(expr);
      if(pmath_same(rhs, PMATH_UNDEFINED))
        return pmath_ref(PMATH_SYMBOL_FAILED);
      return rhs;
    }
    
    pmath_unref(tag);
  }
  else{
    pmath_unref(tag);
    pmath_unref(lhs);
    pmath_unref(rhs);
    return expr;
  }
  
  pmath_unref(expr);
  
  if(pmath_is_string(sym))
    sym = pmath_symbol_find(sym, FALSE);
  
  if(!pmath_is_symbol(sym)){
    pmath_message(PMATH_NULL, "fnsym", 1, lhs);
    
    pmath_unref(sym);
    if(pmath_same(rhs, PMATH_UNDEFINED))
      return pmath_ref(PMATH_SYMBOL_FAILED);
    return rhs;
  }
  
  rules = _pmath_symbol_get_rules(sym, RULES_WRITE);
  
  if(!rules){
    pmath_unref(lhs);
    pmath_unref(rhs);
    pmath_unref(sym);
    return pmath_ref(PMATH_SYMBOL_FAILED);
  }
  
  if(     pmath_same(kind, PMATH_SYMBOL_DEFAULTRULES)) rc = &rules->default_rules;
  else if(pmath_same(kind, PMATH_SYMBOL_DOWNRULES))    rc = &rules->down_rules;
  else if(pmath_same(kind, PMATH_SYMBOL_FORMATRULES))  rc = &rules->format_rules;
  else if(pmath_same(kind, PMATH_SYMBOL_NRULES))       rc = &rules->approx_rules;
  else if(pmath_same(kind, PMATH_SYMBOL_SUBRULES))     rc = &rules->sub_rules;
  else if(pmath_same(kind, PMATH_SYMBOL_UPRULES))      rc = &rules->up_rules;
  else{
    rc = NULL;
    assert(0 && "unexpected kind of rule");
  }
  
  _pmath_rulecache_clear(rc);
  
  if(pmath_same(rhs, PMATH_UNDEFINED)){
    pmath_unref(lhs);
    pmath_unref(sym);
    return PMATH_NULL;
  }
  
  if(_pmath_is_rule(rhs)){
    pmath_t rule_rhs;
      
    pmath_unref(lhs);
    lhs = _pmath_extract_holdpattern(pmath_expr_get_item(rhs, 1));
    rule_rhs = pmath_expr_get_item(rhs, 2);
    
    pmath_unref(rhs);
    
    _pmath_rulecache_change(rc, lhs, rule_rhs);
    
    pmath_gather_begin(PMATH_NULL);
    _pmath_rulecache_emit(rc);
    pmath_symbol_update(sym);
    pmath_unref(sym);
    return pmath_gather_end();
  }
  
  if(_pmath_is_list_of_rules(rhs)){
    size_t i;
    
    pmath_unref(lhs);
    for(i = 1;i <= pmath_expr_length(rhs);++i){
      pmath_t rule;
      pmath_t rule_rhs;
      
      rule = pmath_expr_get_item(rhs, i);
      lhs = _pmath_extract_holdpattern(pmath_expr_get_item(rule, 1));
      rule_rhs = pmath_expr_get_item(rule, 2);
      
      _pmath_rulecache_change(rc, lhs, rule_rhs);
      
      pmath_unref(rule);
    }
    
    pmath_unref(rhs);
    
    pmath_gather_begin(PMATH_NULL);
    _pmath_rulecache_emit(rc);
    pmath_symbol_update(sym);
    pmath_unref(sym);
    return pmath_gather_end();
  }
  
  pmath_message(PMATH_NULL, "vlist", 2, lhs, rhs);
  pmath_unref(sym);
  return pmath_ref(PMATH_SYMBOL_FAILED);
}

PMATH_PRIVATE pmath_t builtin_ownrules(pmath_expr_t expr){
  pmath_t sym;
  
  if(pmath_expr_length(expr) != 1){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  sym = pmath_expr_get_item(expr, 1);
  
  if(pmath_is_string(sym))
    sym = pmath_symbol_find(sym, FALSE);
  
  if(!pmath_is_symbol(sym)){
    pmath_unref(sym);
    pmath_message(PMATH_NULL, "fnsym", 1, pmath_ref(expr));
    return expr;
  }
  
  pmath_unref(expr);
  pmath_gather_begin(PMATH_NULL);
  
  _pmath_symbol_value_emit(
    sym,
    pmath_symbol_get_value(sym));
  
  pmath_unref(sym);
  
  return pmath_gather_end();
}

PMATH_PRIVATE pmath_t builtin_symbol_rules(pmath_expr_t expr){
  struct _pmath_symbol_rules_t *rules;
  pmath_t head;
  pmath_t sym;
  
  if(pmath_expr_length(expr) != 1){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  sym = pmath_expr_get_item(expr, 1);
  
  if(pmath_is_string(sym))
    sym = pmath_symbol_find(sym, FALSE);
  
  if(!pmath_is_symbol(sym)){
    pmath_unref(sym);
    pmath_message(PMATH_NULL, "fnsym", 1, pmath_ref(expr));
    return expr;
  }
  
  head = pmath_expr_get_item(expr, 0);
  pmath_unref(head);
  
  pmath_unref(expr);
  pmath_gather_begin(PMATH_NULL);
  
  rules = _pmath_symbol_get_rules(sym, RULES_READ);
  
  if(rules){
    if(     pmath_same(head, PMATH_SYMBOL_DEFAULTRULES)) _pmath_rulecache_emit(&rules->default_rules);
    else if(pmath_same(head, PMATH_SYMBOL_DOWNRULES))    _pmath_rulecache_emit(&rules->down_rules);
    else if(pmath_same(head, PMATH_SYMBOL_FORMATRULES))  _pmath_rulecache_emit(&rules->format_rules);
    else if(pmath_same(head, PMATH_SYMBOL_NRULES))       _pmath_rulecache_emit(&rules->approx_rules);
    else if(pmath_same(head, PMATH_SYMBOL_SUBRULES))     _pmath_rulecache_emit(&rules->sub_rules);
    else if(pmath_same(head, PMATH_SYMBOL_UPRULES))      _pmath_rulecache_emit(&rules->up_rules);
  }
    
  pmath_unref(sym);
  
  return pmath_gather_end();
}
