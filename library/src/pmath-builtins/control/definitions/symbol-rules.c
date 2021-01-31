#include <pmath-core/symbols-private.h>

#include <pmath-util/concurrency/threads-private.h>
#include <pmath-util/dispatch-tables.h>
#include <pmath-util/emit-and-gather.h>
#include <pmath-util/dynamic-private.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>
#include <pmath-util/symbol-values-private.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/control/definitions-private.h>


extern pmath_symbol_t pmath_System_DollarFailed;
extern pmath_symbol_t pmath_System_DefaultRules;
extern pmath_symbol_t pmath_System_DownRules;
extern pmath_symbol_t pmath_System_FormatRules;
extern pmath_symbol_t pmath_System_HoldPattern;
extern pmath_symbol_t pmath_System_NRules;
extern pmath_symbol_t pmath_System_OwnRules;
extern pmath_symbol_t pmath_System_SubRules;
extern pmath_symbol_t pmath_System_UpRules;

PMATH_PRIVATE
pmath_t _pmath_extract_holdpattern(pmath_t pat) {
  if(pmath_is_expr_of_len(pat, pmath_System_HoldPattern, 1)) {
    pmath_t result = pmath_expr_get_item(pat, 1);
    pmath_unref(pat);
    return result;
  }
  
  return pat;
}

static void track_symbol(pmath_symbol_t sym) {
  if(pmath_atomic_read_aquire(&_pmath_dynamic_trackers)) {
    pmath_thread_t thread = pmath_thread_get_current();
    
    if(thread->current_dynamic_id) {
      _pmath_symbol_track_dynamic(sym, thread->current_dynamic_id);
    }
  }
}

PMATH_PRIVATE pmath_t builtin_assign_ownrules(pmath_expr_t expr) {
  pmath_t tag;
  pmath_t lhs;
  pmath_t rhs;
  pmath_t sym;
  
  if(!_pmath_is_assignment(expr, &tag, &lhs, &rhs))
    return expr;
    
  if(!pmath_is_expr_of_len(lhs, pmath_System_OwnRules, 1)) {
    pmath_unref(tag);
    pmath_unref(lhs);
    pmath_unref(rhs);
    return expr;
  }
  
  sym = pmath_expr_get_item(lhs, 1);
  
  if( !pmath_same(tag, PMATH_UNDEFINED) &&
      !pmath_same(tag, sym))
  {
    pmath_message(PMATH_NULL, "tag", 3, tag, lhs, sym);
    
    pmath_unref(expr);
    if(pmath_same(rhs, PMATH_UNDEFINED))
      return pmath_ref(pmath_System_DollarFailed);
    return rhs;
  }
  
  pmath_unref(tag);
  pmath_unref(expr);
  
  if(pmath_is_string(sym))
    sym = pmath_symbol_find(sym, FALSE);
    
  if(!pmath_is_symbol(sym)) {
    pmath_message(PMATH_NULL, "fnsym", 1, lhs);
    
    pmath_unref(sym);
    if(pmath_same(rhs, PMATH_UNDEFINED))
      return pmath_ref(pmath_System_DollarFailed);
    return rhs;
  }
  
  if(pmath_symbol_get_attributes(sym) & PMATH_SYMBOL_ATTRIBUTE_PROTECTED) {
    pmath_message(PMATH_NULL, "wrsym", 1, sym);
    pmath_unref(lhs);
    pmath_unref(rhs);
    return pmath_ref(pmath_System_DollarFailed);
  }
  
  if(pmath_same(rhs, PMATH_UNDEFINED)) {
    pmath_symbol_set_value(sym, PMATH_UNDEFINED);
    pmath_unref(sym);
    pmath_unref(lhs);
    return PMATH_NULL;
  }
  
  if(pmath_is_rule(rhs)) {
    pmath_t rule_rhs;
    pmath_locked_t value;
    
    pmath_unref(lhs);
    value._data = PMATH_UNDEFINED;
    
    lhs = _pmath_extract_holdpattern(pmath_expr_get_item(rhs, 1));
    rule_rhs = pmath_expr_get_item(rhs, 2);
    
    _pmath_symbol_define_value_pos(&value, lhs, rule_rhs);
    
    pmath_unref(rhs);
    
    pmath_symbol_set_value(sym, pmath_ref(value._data));
    
    pmath_gather_begin(PMATH_NULL);
    _pmath_symbol_value_emit(sym, value._data);
    pmath_unref(sym);
    return pmath_gather_end();
  }
  
  if(pmath_is_list_of_rules(rhs)) {
    pmath_locked_t value;
    size_t i;
    
    pmath_unref(lhs);
    value._data = PMATH_UNDEFINED;
    for(i = 1; i <= pmath_expr_length(rhs); ++i) {
      pmath_t rule;
      pmath_t rule_rhs;
      
      rule = pmath_expr_get_item(rhs, i);
      lhs = _pmath_extract_holdpattern(pmath_expr_get_item(rule, 1));
      rule_rhs = pmath_expr_get_item(rule, 2);
      
      _pmath_symbol_define_value_pos(&value, lhs, rule_rhs);
      
      pmath_unref(rule);
    }
    
    pmath_unref(rhs);
    
    pmath_symbol_set_value(sym, pmath_ref(value._data));
    
    pmath_gather_begin(PMATH_NULL);
    _pmath_symbol_value_emit(sym, value._data);
    pmath_unref(sym);
    return pmath_gather_end();
  }
  
  pmath_unref(sym);
  pmath_message(PMATH_NULL, "vlist", 2, lhs, rhs);
  
  return pmath_ref(pmath_System_DollarFailed);
}

PMATH_PRIVATE pmath_t builtin_assign_symbol_rules(pmath_expr_t expr) {
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
    
  if(pmath_is_expr(lhs)) {
    kind = pmath_expr_get_item(lhs, 0);
    pmath_unref(kind);
  }
  else
    kind = PMATH_NULL;
    
  if( !pmath_same(kind, pmath_System_DefaultRules) &&
      !pmath_same(kind, pmath_System_DownRules)    &&
      !pmath_same(kind, pmath_System_FormatRules)  &&
      !pmath_same(kind, pmath_System_NRules)       &&
      !pmath_same(kind, pmath_System_SubRules)     &&
      !pmath_same(kind, pmath_System_UpRules))
  {
    pmath_unref(tag);
    pmath_unref(lhs);
    pmath_unref(rhs);
    return expr;
  }
  
  if(pmath_expr_length(lhs) == 1) {
    sym = pmath_expr_get_item(lhs, 1);
    
    if( !pmath_same(tag, PMATH_UNDEFINED) &&
        !pmath_same(tag, sym))
    {
      pmath_message(PMATH_NULL, "tag", 3, tag, lhs, sym);
      
      pmath_unref(expr);
      if(pmath_same(rhs, PMATH_UNDEFINED))
        return pmath_ref(pmath_System_DollarFailed);
      return rhs;
    }
    
    pmath_unref(tag);
  }
  else {
    pmath_unref(tag);
    pmath_unref(lhs);
    pmath_unref(rhs);
    return expr;
  }
  
  pmath_unref(expr);
  
  if(pmath_is_string(sym))
    sym = pmath_symbol_find(sym, FALSE);
    
  if(!pmath_is_symbol(sym)) {
    pmath_message(PMATH_NULL, "fnsym", 1, lhs);
    
    pmath_unref(sym);
    if(pmath_same(rhs, PMATH_UNDEFINED))
      return pmath_ref(pmath_System_DollarFailed);
    return rhs;
  }
  
  rules = _pmath_symbol_get_rules(sym, RULES_WRITE);
  
  if(!rules) {
    pmath_unref(lhs);
    pmath_unref(rhs);
    pmath_unref(sym);
    return pmath_ref(pmath_System_DollarFailed);
  }
  
  if(     pmath_same(kind, pmath_System_DefaultRules)) rc = &rules->default_rules;
  else if(pmath_same(kind, pmath_System_DownRules))    rc = &rules->down_rules;
  else if(pmath_same(kind, pmath_System_FormatRules))  rc = &rules->format_rules;
  else if(pmath_same(kind, pmath_System_NRules))       rc = &rules->approx_rules;
  else if(pmath_same(kind, pmath_System_SubRules))     rc = &rules->sub_rules;
  else if(pmath_same(kind, pmath_System_UpRules))      rc = &rules->up_rules;
  else {
    rc = NULL;
    assert(0 && "unexpected kind of rule");
  }
  
  _pmath_rulecache_clear(rc);
  
  if(pmath_same(rhs, PMATH_UNDEFINED)) {
    pmath_unref(lhs);
    pmath_unref(sym);
    return PMATH_NULL;
  }
  
  if(pmath_is_rule(rhs)) {
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
  
  if(pmath_is_list_of_rules(rhs)) {
    size_t i;
    
    pmath_unref(lhs);
    for(i = 1; i <= pmath_expr_length(rhs); ++i) {
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
  return pmath_ref(pmath_System_DollarFailed);
}

PMATH_PRIVATE pmath_t builtin_ownrules(pmath_expr_t expr) {
  pmath_t sym;
  
  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  sym = pmath_expr_get_item(expr, 1);
  
  if(pmath_is_string(sym))
    sym = pmath_symbol_find(sym, FALSE);
    
  if(!pmath_is_symbol(sym)) {
    pmath_unref(sym);
    pmath_message(PMATH_NULL, "fnsym", 1, pmath_ref(expr));
    return expr;
  }
  
  pmath_unref(expr);
  pmath_gather_begin(PMATH_NULL);
  
  _pmath_symbol_value_emit(
    sym,
    pmath_symbol_get_value(sym));
    
  track_symbol(sym);
  pmath_unref(sym);
  
  return pmath_gather_end();
}

PMATH_PRIVATE pmath_t builtin_symbol_rules(pmath_expr_t expr) {
  struct _pmath_symbol_rules_t *rules;
  pmath_t head;
  pmath_t sym;
  
  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  sym = pmath_expr_get_item(expr, 1);
  
  if(pmath_is_string(sym))
    sym = pmath_symbol_find(sym, FALSE);
    
  if(!pmath_is_symbol(sym)) {
    pmath_unref(sym);
    pmath_message(PMATH_NULL, "fnsym", 1, pmath_ref(expr));
    return expr;
  }
  
  head = pmath_expr_get_item(expr, 0);
  pmath_unref(head);
  
  pmath_unref(expr);
  pmath_gather_begin(PMATH_NULL);
  
  rules = _pmath_symbol_get_rules(sym, RULES_READ);
  
  if(rules) {
    if(     pmath_same(head, pmath_System_DefaultRules)) _pmath_rulecache_emit(&rules->default_rules);
    else if(pmath_same(head, pmath_System_DownRules))    _pmath_rulecache_emit(&rules->down_rules);
    else if(pmath_same(head, pmath_System_FormatRules))  _pmath_rulecache_emit(&rules->format_rules);
    else if(pmath_same(head, pmath_System_NRules))       _pmath_rulecache_emit(&rules->approx_rules);
    else if(pmath_same(head, pmath_System_SubRules))     _pmath_rulecache_emit(&rules->sub_rules);
    else if(pmath_same(head, pmath_System_UpRules))      _pmath_rulecache_emit(&rules->up_rules);
  }
  
  track_symbol(sym);
  pmath_unref(sym);
  
  return pmath_gather_end();
}
