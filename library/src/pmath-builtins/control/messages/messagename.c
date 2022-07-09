#include <pmath-core/symbols-private.h>

#include <pmath-util/concurrency/atomic-private.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/memory.h>
#include <pmath-util/messages.h>
#include <pmath-util/symbol-values-private.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/control/definitions-private.h>


extern pmath_t pmath_System_DollarFailed;
extern pmath_t pmath_System_DollarNewMessage;
extern pmath_t pmath_System_Message;
extern pmath_t pmath_System_MessageName;

PMATH_PRIVATE pmath_bool_t _pmath_is_valid_messagename(pmath_t msg){
  pmath_t obj;
  
  if(!pmath_is_expr(msg) || pmath_expr_length(msg) != 2)
    return FALSE;
  
  obj = pmath_expr_get_item(msg, 0);
  pmath_unref(obj);
  if(!pmath_same(obj, pmath_System_MessageName))
    return FALSE;
  
  obj = pmath_expr_get_item(msg, 1);
  if(!pmath_is_symbol(obj)){
    pmath_unref(obj);
    return FALSE;
  }
  pmath_unref(obj);
  
  obj = pmath_expr_get_item(msg, 2);
  if(!pmath_is_string(obj)){
    pmath_unref(obj);
    return FALSE;
  }
  pmath_unref(obj);
  return TRUE;
}

PMATH_PRIVATE pmath_t builtin_criticalmessagetag(pmath_expr_t expr){
  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  pmath_unref(expr);
  return pmath_ref(PMATH_NULL);
}

PMATH_PRIVATE pmath_t builtin_assign_messagename(pmath_expr_t expr){
  struct _pmath_symbol_rules_t  *rules;
  struct _pmath_object_entry_t  *entry;
  pmath_hashtable_t              messages;
  pmath_t                 tag;
  pmath_t                 lhs;
  pmath_t                 rhs;
  pmath_t                 sym;
  
  if(!_pmath_is_assignment(expr, &tag, &lhs, &rhs))
    return expr;
  
  if(!_pmath_is_valid_messagename(lhs)){
    pmath_unref(tag);
    pmath_unref(lhs);
    pmath_unref(rhs);
    return expr;
  }
  
  sym = pmath_expr_get_item(lhs, 1);
  assert(pmath_is_symbol(sym));
  
  if(!pmath_same(tag, PMATH_UNDEFINED) 
  && !pmath_same(tag, sym)){
    pmath_message(pmath_System_Message, "tag", 3, tag, lhs, sym);
    
    pmath_unref(expr);
    if(pmath_same(rhs, PMATH_UNDEFINED))
      return pmath_ref(pmath_System_DollarFailed);
    return rhs;
  }
  
  pmath_unref(tag);
  pmath_unref(expr);
  
  rules = _pmath_symbol_get_rules(sym, RULES_WRITEOPTIONS);
  
  pmath_unref(sym);
  if(!rules){
    pmath_unref(lhs);
    
    if(pmath_same(rhs, PMATH_UNDEFINED))
      return pmath_ref(pmath_System_DollarFailed);
    return rhs;
  }
  
  if(!pmath_same(rhs, PMATH_UNDEFINED)
  && !pmath_is_string(rhs)){
    pmath_message(pmath_System_Message, "str", 1, pmath_ref(rhs));
    
    pmath_unref(lhs);
    
    return rhs;
  }
  
  messages = (pmath_hashtable_t)_pmath_atomic_lock_ptr(&rules->_messages);
  
  if(!messages && !pmath_same(rhs, PMATH_UNDEFINED))
    messages = pmath_ht_create_ex(&pmath_ht_obj_class, 1);

  if(pmath_same(rhs, PMATH_UNDEFINED)){
    entry = pmath_ht_remove(messages, &lhs);
  }
  else{
    entry = pmath_ht_search(messages, &lhs);
      
    if(!entry){
      entry = pmath_mem_alloc(sizeof(struct _pmath_object_entry_t));
      
      entry->key   = pmath_ref(lhs);
      entry->value = pmath_ref(rhs);
      entry = pmath_ht_insert(messages, entry);
    }
    else{
      pmath_unref(entry->value);
      entry->value = pmath_ref(rhs);
      entry = NULL;
    }
  }
  
  _pmath_atomic_unlock_ptr(&rules->_messages, messages);
  
  if(entry)
    pmath_ht_obj_class.entry_destructor(NULL, entry);
    
  pmath_unref(lhs);
  if(pmath_same(rhs, PMATH_UNDEFINED))
    return PMATH_NULL;
  
  return rhs;
}

PMATH_PRIVATE pmath_t builtin_messagename(pmath_expr_t expr){
  struct _pmath_symbol_rules_t  *rules;
  struct _pmath_object_entry_t  *entry;
  pmath_hashtable_t              messages;
  pmath_symbol_t                 sym;
  int                            loop;
  
  if(pmath_expr_length(expr) != 2){
    pmath_message_argxxx(pmath_expr_length(expr), 2, 2);
    return expr;
  }
  
  sym = pmath_expr_get_item(expr, 1);
  if(!pmath_is_symbol(sym)){
    pmath_unref(sym);
    return expr;
  }
  
  if(!_pmath_is_valid_messagename(expr)){
    pmath_message(pmath_System_Message, "name", 1, pmath_ref(expr));
    pmath_unref(sym);
    return expr;
  }
  
  for(loop = 0;loop < 2;++loop){
    rules = _pmath_symbol_get_rules(sym, RULES_READ);
    
    if(rules){
      pmath_t obj = PMATH_NULL;
      
      messages = (pmath_hashtable_t)_pmath_atomic_lock_ptr(&rules->_messages);
      
      entry = pmath_ht_search(messages, &expr);
      if(entry)
        obj = pmath_ref(entry->value);
      
      _pmath_atomic_unlock_ptr(&rules->_messages, messages);
      
      if(!pmath_is_null(obj)){
        pmath_unref(expr);
        pmath_unref(sym);
        return obj;
      }
    }
    
    if(loop == 0){
      pmath_t value = pmath_symbol_get_value(pmath_System_DollarNewMessage);
      
      if(pmath_same(value, PMATH_UNDEFINED))
        break;
        
      pmath_unref(pmath_evaluate(
        pmath_expr_new_extended(
          value, 2,
          pmath_ref(sym),
          pmath_expr_get_item(expr, 2))));
    }
  }
  
  pmath_unref(sym);
  return expr;
}
