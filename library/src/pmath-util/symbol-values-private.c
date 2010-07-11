#include <assert.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pmath-config.h>
#include <pmath-types.h>
#include <pmath-core/objects.h>
#include <pmath-core/expressions.h>
#include <pmath-core/numbers.h>
#include <pmath-core/strings.h>
#include <pmath-core/symbols.h>

#include <pmath-util/debug.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/hashtables-private.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>
#include <pmath-util/memory.h>
#include <pmath-util/symbol-values-private.h>

#include <pmath-util/concurrency/atomic.h>

#include <pmath-core/objects-inline.h>
#include <pmath-core/objects-private.h>

#include <pmath-builtins/control-private.h>
#include <pmath-builtins/lists-private.h>
#include <pmath-builtins/all-symbols.h>
#include <pmath-builtins/all-symbols-private.h>

#include <pmath-language/patterns-private.h>

#include <pmath-util/concurrency/atomic-private.h> // depends on pmath-objects-inline.h

struct _pmath_multirule_t{
  struct _pmath_t   inherited;
  
  pmath_t pattern; // access via _pmath_object_atomic_[read|write]()
  pmath_t body;    // access via _pmath_object_atomic_[read|write]()
  
  struct _pmath_multirule_t * volatile next;
};

/* struct _pmath_multirule_t * is also used as a spinlock, so it may only be
   accessed through _pmath_object_atomic_read*() [using with MULTIRULE_[READ|START|END]()],
   
   but NOT directly or with _pmath_object_atomic_write(), because
   _pmath_object_atomic_write() does not lock.
 */

#define REF_MULTIRULE(rule)     ((struct _pmath_multirule_t*)pmath_ref((pmath_t)rule))
#define UNREF_MULTIRULE(rule)   pmath_unref((pmath_t)rule)

#define MULTIRULE_READ(ptr)        ((struct _pmath_multirule_t*)_pmath_object_atomic_read((pmath_t*)ptr))
#define MULTIRULE_START(ptr)       ((struct _pmath_multirule_t*)_pmath_object_atomic_read_start((pmath_t*)ptr))
#define MULTIRULE_END(ptr, value)  _pmath_object_atomic_read_end((pmath_t*)ptr, (pmath_t)value)

/* In struct _pmath_rulecache_t, the table-member is its own lock:
   while using the hashtable, it is set to PMATH_INVALID_PTR.
   Using the following functions to get the actual table:
 */

#define rulecache_table_lock(rc)           ((pmath_hashtable_t)_pmath_atomic_lock_ptr(&((rc)->_table)))
#define rulecache_table_unlock(rc, table)  _pmath_atomic_unlock_ptr(&((rc)->_table), (table));

//{ constructors / destructors...

static struct _pmath_multirule_t *create_multirule(void){
  struct _pmath_multirule_t *result;
  
  result = (struct _pmath_multirule_t*)_pmath_create_stub(
    PMATH_TYPE_SHIFT_MULTIRULE, 
    sizeof(struct _pmath_multirule_t));
  
  result->pattern  = NULL;
  result->body     = NULL;
  result->next     = NULL;
  return result;
}

static struct _pmath_multirule_t *move_multirule(
  struct _pmath_multirule_t *src // will be freed
){
  struct _pmath_multirule_t *result;
  struct _pmath_multirule_t *current;
  struct _pmath_multirule_t *src_next;
  
  if(!src)
    return NULL;
  
  result = current = create_multirule();
  while(current && src){
    current->pattern  = _pmath_object_atomic_read(&src->pattern);
    current->body     = _pmath_object_atomic_read(&src->body);
    current->next     = NULL;
    
    src_next = MULTIRULE_READ(&src->next);
    UNREF_MULTIRULE(src);
    if(!src_next)
      break;
      
    current = current->next = create_multirule();
    src = src_next;
  }
  
  return result;
}

PMATH_PRIVATE
void _pmath_rulecache_copy(
  struct _pmath_rulecache_t *dst,
  struct _pmath_rulecache_t *src
){
  assert(dst != NULL);
  
  if(!src){
    memset(dst, 0, sizeof(struct _pmath_rulecache_t));
    return;
  }
  
  dst->_more = move_multirule(MULTIRULE_READ(&src->_more));
  
  if(src->_table){
    pmath_hashtable_t table = rulecache_table_lock(src);
    
    dst->_table = pmath_ht_copy(table, _pmath_object_entry_copy_func);
    
    rulecache_table_unlock(src, table);
  }
  else
    dst->_table = NULL;
}

PMATH_PRIVATE
void _pmath_symbol_rules_copy(
  struct _pmath_symbol_rules_t *dst,
  struct _pmath_symbol_rules_t *src
){
  pmath_hashtable_t src_messages;
  assert(dst != NULL);
  
  if(!src){
    memset(dst, 0, sizeof(struct _pmath_symbol_rules_t));
    return;
  }
  
  _pmath_rulecache_copy(&dst->up_rules,      &src->up_rules);
  _pmath_rulecache_copy(&dst->down_rules,    &src->down_rules);
  _pmath_rulecache_copy(&dst->sub_rules,     &src->sub_rules);
  _pmath_rulecache_copy(&dst->approx_rules,  &src->approx_rules);
  _pmath_rulecache_copy(&dst->default_rules, &src->default_rules);
  
  src_messages = (pmath_hashtable_t)_pmath_atomic_lock_ptr(&src->_messages);
  
  dst->_messages = pmath_ht_copy(
    src_messages,
    _pmath_object_entry_copy_func);
  
  _pmath_atomic_unlock_ptr(&src->_messages, src_messages);
}
  
static void destroy_multirule(struct _pmath_multirule_t *s){
  pmath_unref(s->pattern);
  pmath_unref(s->body);
  UNREF_MULTIRULE(s->next);
  pmath_mem_free(s);
}

PMATH_PRIVATE
void _pmath_rulecache_done(struct _pmath_rulecache_t *rc){
  assert(rc != NULL);
  assert(rc->_table != PMATH_INVALID_PTR);
  
  pmath_ht_destroy((pmath_hashtable_t)rc->_table);
  UNREF_MULTIRULE(rc->_more);
}

PMATH_PRIVATE
void _pmath_symbol_rules_done(struct _pmath_symbol_rules_t *rules){
  assert(rules != NULL);
  
  _pmath_rulecache_done(&rules->up_rules);
  _pmath_rulecache_done(&rules->down_rules);
  _pmath_rulecache_done(&rules->sub_rules);
  _pmath_rulecache_done(&rules->approx_rules);
  _pmath_rulecache_done(&rules->default_rules);
    
  pmath_ht_destroy((pmath_hashtable_t)rules->_messages);
}

//} ============================================================================
//{ visiting all objects ...

PMATH_PRIVATE
pmath_bool_t _pmath_symbol_value_visit(
  pmath_t value, // will be freed
  pmath_bool_t (*callback)(pmath_t,void*),
  void *closure
){
  if(!callback(value, closure)){
    pmath_unref(value);
    return FALSE;
  }
  
  if(pmath_instance_of(value, PMATH_TYPE_EXPRESSION)){
    size_t i;
    
    for(i = 0;i <= pmath_expr_length(value);++i){
      if(!_pmath_symbol_value_visit(
            pmath_expr_get_item(value, i),
            callback,
            closure)
      ){
        pmath_unref(value);
        return FALSE;
      }
    }
  }
  else if(pmath_instance_of(value, PMATH_TYPE_MULTIRULE)){
    pmath_bool_t result = TRUE;
    struct _pmath_multirule_t *next;
    struct _pmath_multirule_t *rule;
    
    rule = (struct _pmath_multirule_t*)value;
    
    while(rule && result){
      result = _pmath_symbol_value_visit(
        _pmath_object_atomic_read(&rule->pattern),
        callback,
        closure);
      
      if(result){
        result = _pmath_symbol_value_visit(
          _pmath_object_atomic_read(&rule->body),
          callback,
          closure);
      }
      
      next = MULTIRULE_READ(&rule->next);
      UNREF_MULTIRULE(rule);
      rule = next;
    }
    
    UNREF_MULTIRULE(rule);
    
    return result;
  }
  
  pmath_unref(value);
  return TRUE;
}
  
  static pmath_bool_t object_table_visit(
    pmath_hashtable_t table,
    pmath_bool_t (*callback)(pmath_t,void*),
    void *closure
  ){
    struct _pmath_object_entry_t *entry;
    unsigned int i, cap;
    
    cap = pmath_ht_capacity(table);
    for(i = 0;i < cap;++i){
      entry = pmath_ht_entry(table, i);
      
      if(entry){
        if(!_pmath_symbol_value_visit(pmath_ref(entry->key),   callback, closure)) return FALSE;
        if(!_pmath_symbol_value_visit(pmath_ref(entry->value), callback, closure)) return FALSE;
      }
    }
    
    return TRUE;
  }

PMATH_PRIVATE
PMATH_ATTRIBUTE_NONNULL(1)
pmath_bool_t _pmath_rulecache_visit(
  struct _pmath_rulecache_t *rc, 
  pmath_bool_t (*callback)(pmath_t,void*),
  void *closure
){
  pmath_hashtable_t table;
  pmath_bool_t result;
  
  if(!rc)
    return FALSE;
  
  table = rulecache_table_lock(rc);
  result = object_table_visit(table, callback, closure);
  rulecache_table_unlock(rc, table);
  
  if(result){
    return _pmath_symbol_value_visit(
      (pmath_t)MULTIRULE_READ(&rc->_more),
      callback,
      closure);
  }
  
  return FALSE;
}

PMATH_PRIVATE
PMATH_ATTRIBUTE_NONNULL(1)
pmath_bool_t _pmath_symbol_rules_visit(
  struct _pmath_symbol_rules_t *rules, 
  pmath_bool_t (*callback)(pmath_t,void*),
  void *closure
){
  pmath_hashtable_t table;
  pmath_bool_t result;
  
  if(!rules)
    return FALSE;
  
  if(!_pmath_rulecache_visit(&rules->up_rules,      callback, closure)) return FALSE;
  if(!_pmath_rulecache_visit(&rules->down_rules,    callback, closure)) return FALSE;
  if(!_pmath_rulecache_visit(&rules->sub_rules,     callback, closure)) return FALSE;
  if(!_pmath_rulecache_visit(&rules->approx_rules,  callback, closure)) return FALSE;
  if(!_pmath_rulecache_visit(&rules->default_rules, callback, closure)) return FALSE;
  
  table = (pmath_hashtable_t)_pmath_atomic_lock_ptr(&rules->_messages);
  result = object_table_visit(table, callback, closure);
  _pmath_atomic_unlock_ptr(&rules->_messages, table);
  
  return result;
}

//} ============================================================================
//{ removing a symbol ...

PMATH_PRIVATE
pmath_t _pmath_symbol_value_remove_all(
  pmath_t  value,           // will be freed
  pmath_t  to_be_removed,   // wont be freed
  pmath_t  replacement      // wont be freed
){
  if(value == to_be_removed){
    pmath_unref(value);
    return pmath_ref(replacement);
  }
  
  if(pmath_instance_of(value, PMATH_TYPE_EXPRESSION)){
    size_t i;
    
    for(i = 0;i <= pmath_expr_length(value);++i){
      value = pmath_expr_set_item(
        value, i,
        _pmath_symbol_value_remove_all(
          pmath_expr_get_item(value, i),
          to_be_removed,
          replacement));
    }
    
    return value;
  }
  
  if(pmath_instance_of(value, PMATH_TYPE_MULTIRULE)){
    struct _pmath_multirule_t *next;
    struct _pmath_multirule_t *rule;
    pmath_t obj;
    
    rule = (struct _pmath_multirule_t*)pmath_ref(value);
    
    while(rule){
      obj = _pmath_object_atomic_read(&rule->pattern);
      obj = _pmath_symbol_value_remove_all(obj, to_be_removed, replacement);
      _pmath_object_atomic_write(&rule->pattern, obj);
      
      obj = _pmath_object_atomic_read(&rule->body);
      obj = _pmath_symbol_value_remove_all(obj, to_be_removed, replacement);
      _pmath_object_atomic_write(&rule->body, obj);
      
      next = MULTIRULE_READ(&rule->next);
      UNREF_MULTIRULE(rule);
      rule = next;
    }
  }
  
  return value;
}

  static void object_table_remove_all(
    pmath_hashtable_t table,
    pmath_t           to_be_removed,   // wont be freed
    pmath_t           replacement      // wont be freed
  ){
    struct _pmath_object_entry_t *entry;
    unsigned int i, cap;
    
    cap = pmath_ht_capacity(table);
    for(i = 0;i < cap;++i){
      entry = pmath_ht_entry(table, i);
      
      if(entry){
        entry->key   = _pmath_symbol_value_remove_all(entry->key,   to_be_removed, replacement);
        entry->value = _pmath_symbol_value_remove_all(entry->value, to_be_removed, replacement);
      }
    }
  }

PMATH_PRIVATE
void _pmath_rulecache_remove_all(
  struct _pmath_rulecache_t *rc,
  pmath_t                    to_be_removed, // wont be freed
  pmath_t                    replacement    // wont be freed
){
  pmath_hashtable_t table;
  pmath_t obj;
  
  table = rulecache_table_lock(rc);
  object_table_remove_all(table, to_be_removed, replacement);
  rulecache_table_unlock(rc, table);
  
  obj = (pmath_t)MULTIRULE_START(&rc->_more);
  obj = _pmath_symbol_value_remove_all(obj, to_be_removed, replacement);
  MULTIRULE_END(&rc->_more, obj);
}

PMATH_PRIVATE
void _pmath_symbol_rules_remove_all(
  struct _pmath_symbol_rules_t *rules,
  pmath_t                       to_be_removed, // wont be freed
  pmath_t                       replacement    // wont be freed
){
  pmath_hashtable_t table;
  
  _pmath_rulecache_remove_all(&rules->up_rules,      to_be_removed, replacement);
  _pmath_rulecache_remove_all(&rules->down_rules,    to_be_removed, replacement);
  _pmath_rulecache_remove_all(&rules->sub_rules,     to_be_removed, replacement);
  _pmath_rulecache_remove_all(&rules->approx_rules,  to_be_removed, replacement);
  _pmath_rulecache_remove_all(&rules->default_rules, to_be_removed, replacement);
  
  table = (pmath_hashtable_t)_pmath_atomic_lock_ptr(&rules->_messages);
  object_table_remove_all(table, to_be_removed, replacement);
  _pmath_atomic_unlock_ptr(&rules->_messages, table);
}

//} ============================================================================
//{ _pmath_symbol_rules_ht_class

static void destroy_symbol_rules_entry(
  struct _pmath_symbol_rules_entry_t *entry
){
  pmath_unref(entry->key);
  
  _pmath_symbol_rules_done(&entry->rules);
  
  pmath_mem_free(entry);
}

static unsigned int hash_symbol_rules_entry(
  struct _pmath_symbol_rules_entry_t *entry
){
  return _pmath_hash_pointer(entry->key);
}

static pmath_bool_t symbol_rules_entries_equal(
  struct _pmath_symbol_rules_entry_t *entry1,
  struct _pmath_symbol_rules_entry_t *entry2
){
  return entry1->key == entry2->key;
}

static pmath_bool_t symbol_rules_entry_equals_key(
  struct _pmath_symbol_rules_entry_t *entry,
  pmath_symbol_t                      key
){
  return entry->key == key;
}

PMATH_PRIVATE
const pmath_ht_class_t  _pmath_symbol_rules_ht_class = {
  (pmath_callback_t)                  destroy_symbol_rules_entry,
  (pmath_ht_entry_hash_func_t)        hash_symbol_rules_entry,
  (pmath_ht_entry_equal_func_t)       symbol_rules_entries_equal,
  (pmath_ht_key_hash_func_t)          _pmath_hash_pointer,
  (pmath_ht_entry_equals_key_func_t)  symbol_rules_entry_equals_key
};

//}
//{ pattern matching ...

static pmath_bool_t _pmath_multirule_find(
  struct _pmath_multirule_t *rule,   // will be freed
  pmath_t                   *inout
){
  struct _pmath_multirule_t *next;
  pmath_t cond;
  pmath_t rule_body;
  
  assert(inout != NULL);
  
  while(rule){
    rule_body = _pmath_object_atomic_read(&rule->body);
    
    if(_pmath_pattern_match(
        *inout, 
        _pmath_object_atomic_read(&rule->pattern), 
        &rule_body))
    {
      if(_pmath_rhs_condition(&rule_body, TRUE)){
        rule_body = pmath_evaluate(rule_body);
        
        if(pmath_is_expr_of_len(rule_body, PMATH_SYMBOL_INTERNAL_CONDITION, 2)){
          cond = pmath_expr_get_item(rule_body, 2);
          pmath_unref(cond);
          
          if(cond == PMATH_SYMBOL_TRUE){
            pmath_unref(*inout);
            *inout = pmath_expr_get_item(rule_body, 1);
            pmath_unref(rule_body);
            UNREF_MULTIRULE(rule);
            return TRUE;
          }
        }
      }
      else{
        pmath_unref(*inout);
        *inout = rule_body;
        UNREF_MULTIRULE(rule);
        return TRUE;
      }
    }
    
    pmath_unref(rule_body);
    
    next = MULTIRULE_READ(&rule->next);
    UNREF_MULTIRULE(rule);
    rule = next;
  }
  
  return FALSE;
}

PMATH_PRIVATE
pmath_bool_t _pmath_rulecache_find(
  struct _pmath_rulecache_t *rc,
  pmath_t                   *inout
){
  struct _pmath_object_entry_t *entry;
  pmath_hashtable_t table;
  
  assert(rc != NULL);
  assert(inout != NULL);
  
  table = rulecache_table_lock(rc);
  
  entry = pmath_ht_search(table, *inout);
  
  if(entry){
    pmath_unref(*inout);
    *inout = pmath_ref(entry->value);
  }
  
  rulecache_table_unlock(rc, table);
  
  if(entry)
    return TRUE;
  
  return _pmath_multirule_find(MULTIRULE_READ(&rc->_more), inout);
}

//} ============================================================================
//{ changing rules ...

/* result: whether the change succeeded 
           (otherwise caller must call the function again)
 */
static pmath_bool_t _pmath_multirule_change_ex(
  struct _pmath_multirule_t * volatile *rule_base, // accessed through _pmath_object_atomic_[read|write]
  struct _pmath_multirule_t            *rule,      // will be freed
  pmath_t pattern,                                 // wont be freed
  pmath_t body                                     // wont be freed, PMATH_UNDEFINED => remove rules
){
  //struct _pmath_multirule_t *rule;
  struct _pmath_multirule_t *prev;
  struct _pmath_multirule_t *tmp;
  struct _pmath_multirule_t *new_rule;
  pmath_t rule_member;
  int cmp;
  
  assert(rule_base != NULL);
  
  prev = NULL;
  while(rule){
    rule_member = _pmath_object_atomic_read(&rule->pattern);
    cmp = _pmath_pattern_compare(pattern, rule_member);
    pmath_unref(rule_member);
    
    if(cmp == 0){ // rhs coditions? (pattern :> value//condition)
      rule_member = _pmath_object_atomic_read(&rule->body);
      
      if(_pmath_rhs_condition(&rule_member, FALSE)){
        cmp = 1;
        
        if(body == PMATH_UNDEFINED){ // remove this rule and go on
          tmp = MULTIRULE_START(rule_base); // start atomic ...
          
          if(tmp != rule){ // another thread annoyed us
            MULTIRULE_END(rule_base, tmp);
            UNREF_MULTIRULE(prev);
            UNREF_MULTIRULE(rule);
            pmath_unref(rule_member);
            return FALSE;
          }
          
          UNREF_MULTIRULE(tmp);
          tmp = MULTIRULE_READ(&rule->next);
          pmath_atomic_barrier();
          UNREF_MULTIRULE(rule); 
          rule = REF_MULTIRULE(tmp);
          
          MULTIRULE_END(rule_base, tmp); // end atomic
          
          pmath_unref(rule_member);
          continue;
        }
      }
      else if(_pmath_rhs_condition(&body, FALSE))
        cmp = -1;
      
      pmath_unref(rule_member);
    }
    
    if(cmp == 0){ // replace current rule
      tmp = MULTIRULE_START(rule_base); // start atomic ...
      
      if(tmp != rule){ // another thread annoyed us
        MULTIRULE_END(rule_base, tmp);
        UNREF_MULTIRULE(prev);
        UNREF_MULTIRULE(rule);
        return FALSE;
      }
      
      if(body == PMATH_UNDEFINED){ // remove rule
        UNREF_MULTIRULE(tmp);
        tmp = MULTIRULE_READ(&rule->next);
      }
      else{ // change rule in place
        _pmath_object_atomic_write(&rule->pattern, pmath_ref(pattern));
        _pmath_object_atomic_write(&rule->body,    pmath_ref(body));
      }
      
      MULTIRULE_END(rule_base, tmp); // end atomic
      UNREF_MULTIRULE(prev);
      UNREF_MULTIRULE(rule);
      return TRUE;
    }
    
    if(cmp < 0){ // insert before curren rule
      if(body != PMATH_UNDEFINED){
        new_rule = create_multirule();
        
        if(!new_rule){ // no memory, but repurt success so caller does not stuck in an infinite loop
          UNREF_MULTIRULE(prev);
          UNREF_MULTIRULE(rule);
          return TRUE;
        }
        
        new_rule->pattern = pmath_ref(pattern);
        new_rule->body    = pmath_ref(body);
        
        tmp = MULTIRULE_START(rule_base); // start atomic ...
        
        if(tmp != rule){ // another thread annoyed us
          MULTIRULE_END(rule_base, tmp);
          UNREF_MULTIRULE(prev);
          UNREF_MULTIRULE(rule);
          UNREF_MULTIRULE(new_rule);
          return FALSE;
        }
        
        pmath_atomic_barrier();
        
        UNREF_MULTIRULE(tmp);
        tmp = new_rule;
        new_rule->next = rule;
        
        MULTIRULE_END(rule_base, tmp); // end atomic
        UNREF_MULTIRULE(prev);
        return TRUE;
      }
      
      UNREF_MULTIRULE(prev);
      UNREF_MULTIRULE(rule);
      return TRUE;
    }
    
    UNREF_MULTIRULE(prev);
    prev = rule;
    rule_base = &prev->next;
    rule = MULTIRULE_READ(rule_base);
  }
  
  if(body != PMATH_UNDEFINED){
    new_rule = create_multirule();
    
    if(new_rule){
      new_rule->pattern = pmath_ref(pattern);
      new_rule->body    = pmath_ref(body);
      
      tmp = MULTIRULE_START(rule_base); // start atomic ...
      
      if(tmp){ // tmp != rule, another thread annoyed us
        MULTIRULE_END(rule_base, tmp);
        UNREF_MULTIRULE(prev);
        UNREF_MULTIRULE(new_rule);
        return FALSE;
      }
      
      MULTIRULE_END(rule_base, new_rule); // end atomic
    }
  }
  
  UNREF_MULTIRULE(prev);
  return TRUE;
}

static pmath_bool_t _pmath_multirule_change(
  struct _pmath_multirule_t * volatile *rule_base, // accessed through _pmath_object_atomic_[read|write]
  pmath_t pattern,                                 // wont be freed
  pmath_t body                                     // wont be freed, PMATH_UNDEFINED => remove rules
){
  assert(rule_base != NULL);
  
  return _pmath_multirule_change_ex(
    rule_base,
    MULTIRULE_READ(rule_base),
    pattern,
    body);
}

PMATH_PRIVATE
void _pmath_rulecache_change(
  struct _pmath_rulecache_t *rc,
  pmath_t                    pattern, // will be freed
  pmath_t                    body     // will be freed, PMATH_UNDEFINED => remove rules
){
  pmath_bool_t body_has_condition;
  struct _pmath_object_entry_t *entry;
  pmath_hashtable_t table;
  
  assert(rc != NULL);
  
  body_has_condition = _pmath_rhs_condition(&body, FALSE);
  
  table = rulecache_table_lock(rc);
  
  if(body == PMATH_UNDEFINED || body_has_condition){
    entry = pmath_ht_remove(table, pattern);
    
    if(entry){
      if(body_has_condition){
        while(!_pmath_multirule_change(&rc->_more, pattern, body)){
        }
        
        while(!_pmath_multirule_change(&rc->_more, entry->key, entry->value)){
        }
      }
      
      rulecache_table_unlock(rc, table);
      
      pmath_unref(pattern);
      pmath_unref(body);
      pmath_ht_obj_class.entry_destructor(entry);
      return;
    }
  }
  else{
    entry = pmath_ht_search(table, pattern);
    
    if(entry){
      pmath_unref(entry->key);
      pmath_unref(entry->value);
      entry->key   = pattern;
      entry->value = body;
      
      rulecache_table_unlock(rc, table);
      return;
    }
    
    if(_pmath_pattern_is_const(pattern)){
      if(!table)
        table = pmath_ht_create(&pmath_ht_obj_class, 1);
      
      entry = pmath_mem_alloc(sizeof(struct _pmath_object_entry_t));
      
      if(entry){
        entry->key = pattern;
        entry->value = body;
        
        entry = pmath_ht_insert(table, entry);
        assert(entry == NULL);
        
        rulecache_table_unlock(rc, table);
        return;
      }
    }
  }
  
  rulecache_table_unlock(rc, table);
  
  while(!_pmath_multirule_change(&rc->_more, pattern, body)){
  }
  
  pmath_unref(pattern);
  pmath_unref(body);
}

PMATH_PRIVATE
void _pmath_rulecache_clear(struct _pmath_rulecache_t *rc){
  pmath_hashtable_t          table;
  struct _pmath_multirule_t *more;
  
  assert(rc != NULL);
  
  more = MULTIRULE_START(&rc->_more);
  MULTIRULE_END(&rc->_more, NULL);
  
  table = rulecache_table_lock(rc);
  rulecache_table_unlock(rc, NULL);
  
  UNREF_MULTIRULE(more);
  pmath_ht_destroy(table);
}

PMATH_PRIVATE
void _pmath_symbol_rules_clear(struct _pmath_symbol_rules_t *rules){
  pmath_hashtable_t  messages;
  
  assert(rules != NULL);
  
  _pmath_rulecache_clear(&rules->up_rules);
  _pmath_rulecache_clear(&rules->down_rules);
  _pmath_rulecache_clear(&rules->sub_rules);
  _pmath_rulecache_clear(&rules->approx_rules);
  _pmath_rulecache_clear(&rules->default_rules);
  
  messages = (pmath_hashtable_t)_pmath_atomic_lock_ptr(&rules->_messages);
  _pmath_atomic_unlock_ptr(&rules->_messages, NULL);
  
  pmath_ht_destroy(messages);
}

//} ============================================================================
//{ symbol values ...

static void _pmath_multirules_emit(struct _pmath_multirule_t *rule){ // will be freed
  struct _pmath_multirule_t *next;
  
  while(rule){
    pmath_emit(
      pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_RULEDELAYED), 2,
        pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_HOLDPATTERN), 1,
          _pmath_object_atomic_read(&rule->pattern)),
        _pmath_object_atomic_read(&rule->body)),
      NULL);
    
    next = MULTIRULE_READ(&rule->next);
    UNREF_MULTIRULE(rule);
    rule = next;  
  }
}

PMATH_PRIVATE
void _pmath_symbol_value_emit(
  pmath_symbol_t sym,    // wont be freed
  pmath_t        value   // will be freed
){
  if(pmath_instance_of(value, PMATH_TYPE_MULTIRULE)){
    _pmath_multirules_emit((struct _pmath_multirule_t*)value);
  }
  else if(!value || pmath_instance_of(value, PMATH_TYPE_EVALUATABLE)){
    pmath_emit(
      pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_RULEDELAYED), 2,
        pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_HOLDPATTERN), 1,
            pmath_ref(sym)),
        value),
      NULL);
  }
  else
    pmath_unref(value);
}

PMATH_PRIVATE
void _pmath_rule_table_emit(
  pmath_hashtable_t table
){
  struct _pmath_object_entry_t *entry;
  unsigned int i, cap;
  
  cap = pmath_ht_capacity(table);
  
  for(i = 0;i < cap;++i){
    entry = pmath_ht_entry(table, i);
    
    if(entry){
      pmath_emit(
        pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_RULEDELAYED), 2,
          pmath_expr_new_extended(
            pmath_ref(PMATH_SYMBOL_HOLDPATTERN), 1,
            pmath_ref(entry->key)),
          pmath_ref(entry->value)),
        NULL);
    }
  }
}

PMATH_PRIVATE
void _pmath_rulecache_emit(
  struct _pmath_rulecache_t *rc
){
  pmath_hashtable_t  table;
  
  assert(rc != NULL);
  
  table = rulecache_table_lock(rc);
  
  _pmath_rule_table_emit(table);
  
  rulecache_table_unlock(rc, table);
  
  _pmath_multirules_emit(MULTIRULE_READ(&rc->_more));
}

PMATH_PRIVATE
pmath_t _pmath_symbol_value_prepare(
  pmath_symbol_t sym,    // wont be freed
  pmath_t        value   // will be freed
){
  if(pmath_instance_of(value, PMATH_TYPE_MULTIRULE)){
    struct _pmath_multirule_t *rule = (struct _pmath_multirule_t*)value;
    value = pmath_ref(sym);
    
    if(!_pmath_multirule_find(rule, &value)){
      pmath_unref(value);
      return PMATH_UNDEFINED;
    }
  }
  
  return value;
}

PMATH_PRIVATE
pmath_t _pmath_symbol_find_value(pmath_symbol_t sym){
  return _pmath_symbol_value_prepare(sym, pmath_symbol_get_value(sym));
}

PMATH_PRIVATE
void _pmath_symbol_define_value_pos(
  pmath_t *value_position,
  pmath_t  pattern,
  pmath_t  body
){
  pmath_t value = _pmath_object_atomic_read(value_position);
  
  if(pmath_instance_of(value, PMATH_TYPE_MULTIRULE)){
    while(!_pmath_multirule_change_ex(
        (struct _pmath_multirule_t*volatile*)value_position, 
        (struct _pmath_multirule_t*)value,
        pattern, 
        body))
    {
    }
    
    if(!*value_position)
      _pmath_object_atomic_write(value_position, PMATH_UNDEFINED);
    
    pmath_unref(pattern);
    pmath_unref(body);
    return;
  }
  
  if(_pmath_rhs_condition(&body, FALSE)){
    _pmath_object_atomic_write(value_position, NULL);
    
    while(!_pmath_multirule_change_ex(
        (struct _pmath_multirule_t*volatile*)value_position, 
        (struct _pmath_multirule_t*)NULL,
        pattern, 
        body))
    {
    }
    
    pmath_unref(body);
    if(value == PMATH_UNDEFINED)
      pmath_unref(pattern);
    else
      _pmath_symbol_define_value_pos(value_position, pattern, value);
      
    return;
  }
  
  pmath_unref(pattern);
  pmath_unref(value);
  
  _pmath_object_atomic_write(value_position, body);
}

//} ============================================================================
//{ module init/done ...

  static int dummy_cmp(void *a, void *b){
    return 0;
  }

  static unsigned int dummy_hash(void *a){
    return 0;
  }

PMATH_PRIVATE pmath_bool_t _pmath_symbol_values_init(void){
  _pmath_init_special_type(
    PMATH_TYPE_SHIFT_MULTIRULE,
    (pmath_compare_func_t)        dummy_cmp,
    (pmath_hash_func_t)           dummy_hash,
    (pmath_proc_t)                destroy_multirule,
    (pmath_equal_func_t)          NULL,
    (_pmath_object_write_func_t)  NULL);
  
  return TRUE;
}

PMATH_PRIVATE void _pmath_symbol_values_done(void){
}

//}
