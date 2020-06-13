#include <pmath-core/symbols-private.h>

#include <pmath-util/concurrency/atomic-private.h>
#include <pmath-util/dispatch-tables.h>
#include <pmath-util/emit-and-gather.h>
#include <pmath-util/helpers.h>
#include <pmath-util/memory.h>
#include <pmath-util/messages.h>
#include <pmath-util/symbol-values-private.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/control/definitions-private.h>
#include <pmath-builtins/control/messages-private.h>
#include <pmath-builtins/control-private.h>


PMATH_PRIVATE pmath_t builtin_assign_messages(pmath_expr_t expr){
  struct _pmath_symbol_rules_t  *rules;
  struct _pmath_object_entry_t  *entry;
  pmath_hashtable_t              messages;
  pmath_t                 tag;
  pmath_t                 lhs;
  pmath_t                 rhs;
  pmath_t                 sym;
  
  int kind = _pmath_is_assignment(expr, &tag, &lhs, &rhs);
  
  if(!kind)
    return expr;
  
  if(!pmath_is_expr_of_len(lhs, PMATH_SYMBOL_MESSAGES, 1)){
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
    pmath_unref(rhs);
    return pmath_ref(PMATH_SYMBOL_FAILED);
  }
  
  pmath_unref(tag);
  pmath_unref(expr);
  
  if(pmath_is_string(sym))
    sym = pmath_symbol_find(sym, FALSE);
  
  if(!pmath_is_symbol(sym)){
    pmath_message(PMATH_NULL, "fnsym", 1, lhs);
    
    pmath_unref(sym);
    pmath_unref(rhs);
    return pmath_ref(PMATH_SYMBOL_FAILED);
  }
  
  if( pmath_same(rhs, PMATH_UNDEFINED) || 
      pmath_is_expr_of_len(rhs, PMATH_SYMBOL_LIST, 0))
  {
    rules = _pmath_symbol_get_rules(sym, RULES_READ);
    
    if(rules){
      rules = _pmath_symbol_get_rules(sym, RULES_WRITEOPTIONS);
      
      if(rules){
        messages = (pmath_hashtable_t)_pmath_atomic_lock_ptr(&rules->_messages);
        _pmath_atomic_unlock_ptr(&rules->_messages, NULL);
        
        pmath_ht_destroy(messages);
      }
    }
    
    pmath_unref(lhs);
    pmath_unref(sym);
    
    if(kind > 0 && !pmath_same(rhs, PMATH_UNDEFINED))
      return rhs;
    
    pmath_unref(rhs);
    return PMATH_NULL;
  }
  
  if(_pmath_is_rule(rhs)){
    rhs = pmath_expr_new_extended(
      pmath_ref(PMATH_SYMBOL_LIST), 1,
      rhs);
  }
  
  if(pmath_is_list_of_rules(rhs)){
    size_t i;
    
    pmath_unref(lhs);
    messages = pmath_ht_create(
      &pmath_ht_obj_class, 
      (unsigned int)pmath_expr_length(rhs));
    
    for(i = 1;i <= pmath_expr_length(rhs);++i){
      pmath_t rule;
      pmath_t rule_rhs;
      
      rule = pmath_expr_get_item(rhs, i);
      lhs = _pmath_extract_holdpattern(pmath_expr_get_item(rule, 1));
      rule_rhs = pmath_expr_get_item(rule, 2);
      pmath_unref(rule);
      
      if(!_pmath_is_valid_messagename(lhs)){
        pmath_message(PMATH_NULL, "name", 1, lhs);
        
        pmath_ht_destroy(messages);
        pmath_unref(rule_rhs);
        pmath_unref(rhs);
        pmath_unref(sym);
        return pmath_ref(PMATH_SYMBOL_FAILED);
      }
      
      tag = pmath_expr_get_item(lhs, 1);
      if(!pmath_same(tag, sym)){
        pmath_message(PMATH_NULL, "tag", 3, tag, lhs, sym);
        
        pmath_ht_destroy(messages);
        pmath_unref(rule_rhs);
        pmath_unref(rhs);
        return pmath_ref(PMATH_SYMBOL_FAILED);
      }
      pmath_unref(tag);
      
      if(!pmath_is_string(rule_rhs)){
        pmath_message(PMATH_NULL, "str", 2, lhs, rule_rhs);
        
        pmath_ht_destroy(messages);
        pmath_unref(rhs);
        pmath_unref(sym);
        return pmath_ref(PMATH_SYMBOL_FAILED);
      }
      
      entry = pmath_mem_alloc(sizeof(struct _pmath_object_entry_t));
      if(!entry){
        pmath_unref(lhs);
        pmath_unref(rule_rhs);
        pmath_unref(rhs);
        pmath_unref(sym);
        return PMATH_NULL;
      }
      
      entry->key = lhs;
      entry->value = rule_rhs;
      
      entry = pmath_ht_insert(messages, entry);
      if(entry)
        pmath_ht_obj_class.entry_destructor(entry);
    }
    
    rules = _pmath_symbol_get_rules(sym, RULES_WRITEOPTIONS);
    if(rules){
      pmath_ht_destroy(
        (pmath_hashtable_t)_pmath_atomic_lock_ptr(&rules->_messages));
      _pmath_atomic_unlock_ptr(&rules->_messages, messages);
      
      pmath_unref(sym);
      if(kind > 0)
        return rhs;
      pmath_unref(rhs);
      return PMATH_NULL;
    }
    
    pmath_unref(sym);
    pmath_unref(rhs);
    return pmath_ref(PMATH_SYMBOL_FAILED);
  }
  
  pmath_unref(sym);
  pmath_message(PMATH_NULL, "vlist", 2, lhs, rhs);
  return pmath_ref(PMATH_SYMBOL_FAILED);
}

PMATH_PRIVATE pmath_t builtin_messages(pmath_expr_t expr){
  struct _pmath_symbol_rules_t *rules;
  pmath_hashtable_t             messages;
  pmath_t                sym;
  
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
  
  rules = _pmath_symbol_get_rules(sym, RULES_READ);
  pmath_unref(sym);
  
  if(rules){
    messages = (pmath_hashtable_t)_pmath_atomic_lock_ptr(&rules->_messages);
    
    _pmath_rule_table_emit(messages);
    
    _pmath_atomic_unlock_ptr(&rules->_messages, messages);
  }
    
  
  return pmath_gather_end();
}
