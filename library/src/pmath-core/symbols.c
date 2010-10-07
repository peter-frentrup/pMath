#include <pmath-core/symbols-private.h>

#include <pmath-core/expressions-private.h>
#include <pmath-core/strings-private.h>

#include <pmath-util/concurrency/atomic-private.h>
#include <pmath-util/concurrency/threadmsg.h>
#include <pmath-util/concurrency/threads-private.h>
#include <pmath-util/debug.h>
#include <pmath-util/dynamic-private.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/memory.h>
#include <pmath-util/messages.h>
#include <pmath-util/stacks-private.h>
#include <pmath-util/symbol-values-private.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/control/definitions-private.h>

#include <pmath-private.h>

#include <limits.h>
#include <stdio.h>
#include <string.h>


#ifdef _MSC_VER
  #define snprintf sprintf_s
#endif

#ifdef __PMATH_DEBUG_H__

  #define PMATH_DEBUG_TIMING(CODE) \
    do{ \
      double PMATH_DEBUG_TIMING_START = pmath_tickcount(); \
      CODE \
      if(pmath_tickcount() - PMATH_DEBUG_TIMING_START > 1.0){ \
        pmath_debug_print("%s line %d: LONG WAIT (%f sec)\n", __FILE__, __LINE__, pmath_tickcount() - PMATH_DEBUG_TIMING_START); \
      } \
    }while(0)
  
#else
  
  #define PMATH_DEBUG_TIMING(CODE)  do{ CODE }while(0)
  
#endif

struct _pmath_symbol_t{
  struct _pmath_gc_t         inherited;
  
  struct _pmath_symbol_t    *prev;
  struct _pmath_symbol_t    *next;
  
  volatile pmath_string_t    name;
  pmath_threadlock_t         lock;
  pmath_symbol_attributes_t  attributes;
  pmath_t                    value;
  
  union{
    struct _pmath_symbol_rules_t  *rules;
    intptr_t                       rules_intptr;
  } u; // gcc warning: "Dereferencing type-punned pointers ..."
  
  PMATH_DECLARE_ATOMIC(current_dynamic_id);
};

//{ global symbol table ...
PMATH_DECLARE_ATOMIC(global_symbol_table_lock) = 0;

static struct _pmath_symbol_t *global_first;

static pmath_hashtable_t global_symbol_table;

// global_symbol_table_lock must be held:
static void pre_insert(struct _pmath_symbol_t *symbol){
  if(global_first){
    global_first->next->prev = symbol;
    symbol->next             = global_first->next;
    global_first->next       = symbol;
    symbol->prev             = global_first;
  }
  else{
    global_first = symbol->prev = symbol->next = symbol;
  }
}

// global_symbol_table_lock must be held:
static void post_remove(struct _pmath_symbol_t *symbol){
  if(global_first == symbol){
    if(symbol->next == symbol)
      global_first = NULL;
    else
      global_first = symbol->next;
  }
  
  symbol->next->prev = symbol->prev;
  symbol->prev->next = symbol->next;
}
//}
//{ caching unused symbols ...

static struct _pmath_stack_t  unused_symbols;

static void destroy_all_unused_symbols(void){
  void *item;
  while((item = pmath_stack_pop(&unused_symbols)) != NULL){
    pmath_mem_free(item);
  }
}

static struct _pmath_symbol_t *create_symbol(void){
  struct _pmath_symbol_t *symbol = pmath_stack_pop(&unused_symbols);
  
  if(symbol){
    symbol->inherited.inherited.inherited.type_shift = PMATH_TYPE_SHIFT_SYMBOL;
    symbol->inherited.inherited.inherited.refcount   = 1;
  }
  else{
    symbol = (struct _pmath_symbol_t*)_pmath_create_stub(
      PMATH_TYPE_SHIFT_SYMBOL,
      sizeof(struct _pmath_symbol_t));
  }
  
  if(symbol){
    symbol->inherited.inherited.last_change = _pmath_timer_get_next();
    symbol->inherited.gc_refcount = 0;
    symbol->prev = NULL;
    symbol->next = NULL;
    symbol->current_dynamic_id = 0;
  }
  
  return symbol;
}

//}
//{ hash table functions ...

static void symbol_entry_destructor(struct _pmath_symbol_t *symbol){
  pmath_unref(symbol->name);
  pmath_unref(symbol->value);
  
  if(symbol->u.rules){
    _pmath_symbol_rules_done(symbol->u.rules);
    
    pmath_mem_free(symbol->u.rules);
  }
  
  pmath_stack_push(&unused_symbols, symbol);
}

static unsigned int symbol_entry_hash(struct _pmath_symbol_t *symbol){
  return pmath_hash(symbol->name);
}

static pmath_bool_t symbol_entry_keys_equal(
  struct _pmath_symbol_t *symbol1, 
  struct _pmath_symbol_t *symbol2
){
  return pmath_equals(symbol1->name, symbol2->name);
}

static pmath_bool_t symbol_entry_equals_key(
  struct _pmath_symbol_t *symbol, 
  pmath_string_t          key
){
  return pmath_equals(symbol->name, key);
}

//}

static const pmath_ht_class_t symbol_table_class = {
  (pmath_callback_t)                  symbol_entry_destructor,
  (pmath_ht_entry_hash_func_t)        symbol_entry_hash,
  (pmath_ht_entry_equal_func_t)       symbol_entry_keys_equal,
  (pmath_ht_key_hash_func_t)          pmath_hash,
  (pmath_ht_entry_equals_key_func_t)  symbol_entry_equals_key
};

PMATH_API pmath_symbol_t pmath_symbol_iter_next(pmath_symbol_t old){
  pmath_symbol_t result;
  
  if(!old)
    return NULL;
  
  PMATH_DEBUG_TIMING(
    pmath_atomic_lock(&global_symbol_table_lock);
    {
      result = pmath_ref((pmath_symbol_t)((struct _pmath_symbol_t*)old)->next);
    }
    pmath_atomic_unlock(&global_symbol_table_lock);
  );
  
  pmath_unref(old);
  return result;
}

/*----------------------------------------------------------------------------*/

PMATH_API pmath_symbol_t pmath_symbol_get(
  pmath_string_t  name,   // will be freed
  pmath_bool_t    create
){
  pmath_symbol_attributes_t attr;
  pmath_symbol_t result = NULL;
  
  if(pmath_string_length(name) == 1){
    uint16_t ch = *pmath_string_buffer(name);
    
    switch(ch){
      case 0x03C0: 
        pmath_unref(name);
        return pmath_ref(PMATH_SYMBOL_PI);
      
      case 0x212F: 
      case 0x2147: 
        pmath_unref(name);
        return pmath_ref(PMATH_SYMBOL_E);
      
      case 0x2148: 
      case 0x2149: 
        pmath_unref(name);
        return pmath_ref(PMATH_SYMBOL_I);
      
      case 0x221E: 
        pmath_unref(name);
        return pmath_ref(PMATH_SYMBOL_INFINITY);
    }
  }
  
  PMATH_DEBUG_TIMING(
    pmath_atomic_lock(&global_symbol_table_lock);
    {
      result = pmath_ref(pmath_ht_search(global_symbol_table, name));
    }
    pmath_atomic_unlock(&global_symbol_table_lock);
  );
  
  if(result){
    attr = pmath_symbol_get_attributes(result);
    if(attr & PMATH_SYMBOL_ATTRIBUTE_REMOVED){
      if(create){
        pmath_symbol_set_attributes(result, attr & ~PMATH_SYMBOL_ATTRIBUTE_REMOVED);
        pmath_unref(name);
        return result;
      }
      else{
        pmath_unref(result);
        result = NULL;
      }
    }
    else{
      pmath_unref(name);
      return result;
    }
  }
  
  if(create){
    void *entry;
    struct _pmath_symbol_t *new_symbol = create_symbol();
    result = (pmath_symbol_t)new_symbol;
    if(!new_symbol){
      pmath_unref(name);
      return NULL;
    }
    
    new_symbol->name        = pmath_ref(name);
    new_symbol->lock        = NULL;
    new_symbol->attributes  = 0;
    new_symbol->value       = PMATH_UNDEFINED;
    new_symbol->u.rules     = NULL;
    
    PMATH_DEBUG_TIMING(
      pmath_atomic_lock(&global_symbol_table_lock);
      {
        pre_insert(new_symbol);
        entry = pmath_ht_insert(global_symbol_table, new_symbol);
        if(entry)
          post_remove(entry);
      }
      pmath_atomic_unlock(&global_symbol_table_lock);
    );
    
    if(entry){
      symbol_entry_destructor(entry);
      if(entry == new_symbol){
        pmath_unref(name);
        return NULL;
      }
    }
    
    if(_pmath_is_running()){
      pmath_t newsym = pmath_symbol_get_value(PMATH_SYMBOL_NEWSYMBOL);
      
      if(newsym){
        const uint16_t *buf = pmath_string_buffer(name);
        int             len = pmath_string_length(name);
        
        pmath_string_t ns, n;
        
        if(len > 0){
          do{
            --len;
          }while(len > 0 && buf[len] != '`');
        }
        
        ns = pmath_string_part(pmath_ref(name), 0, len);
        n  = pmath_string_part(pmath_ref(name), len + 1, -1);
        
        pmath_unref(pmath_evaluate(pmath_expr_new_extended(newsym, 2, n, ns)));
      }
    }
  }
  
  pmath_unref(name);
//  pmath_unref(data.name);

  return result;
}

/*----------------------------------------------------------------------------*/

static PMATH_DECLARE_ATOMIC(_pmath_tmp_name_counter) = 0;

PMATH_API pmath_symbol_t pmath_symbol_create_temporary(
  pmath_string_t name,
  pmath_bool_t   unique
){
  pmath_symbol_t result;
  const uint16_t *buf;
  int len;
  char val[20];
  
  buf = pmath_string_buffer(name);
  len = pmath_string_length(name);
  
  --len;
  while(len >= 0 && buf[len] >= '0' && buf[len] <= '9')
    --len;

  if(len > 0 && buf[len] == '$'){
    name = pmath_string_part(name, 0, len);
  }
  
  if(unique){
    snprintf(val, sizeof(val), "$%"PRIuPTR,
      pmath_atomic_fetch_add(&_pmath_tmp_name_counter, 1));
    
    name = pmath_string_insert_latin1(name, INT_MAX, val, -1);
  }
  else
    name = pmath_string_insert_latin1(name, INT_MAX, "$", 1);
  
  PMATH_DEBUG_TIMING(
    pmath_atomic_lock(&global_symbol_table_lock);
    {
      result = pmath_ref(pmath_ht_search(global_symbol_table, name));
    }
    pmath_atomic_unlock(&global_symbol_table_lock);
  );
  
  if(!result){
    void *entry;
    struct _pmath_symbol_t *new_symbol = create_symbol();
    result = (pmath_symbol_t)new_symbol;
    if(!new_symbol){
      pmath_unref(name);
      return NULL;
    }
    
    //new_symbol->last_update = (uintptr_t)global_update_counter;
    new_symbol->lock        = NULL;
    new_symbol->name        = name;
    new_symbol->attributes  = PMATH_SYMBOL_ATTRIBUTE_TEMPORARY;
    new_symbol->value       = PMATH_UNDEFINED;
    new_symbol->u.rules     = NULL;
    
    PMATH_DEBUG_TIMING(
      pmath_atomic_lock(&global_symbol_table_lock);
      {
        pre_insert(new_symbol);
        entry = pmath_ht_insert(global_symbol_table, new_symbol);
        if(entry)
          post_remove(entry);
      }
      pmath_atomic_unlock(&global_symbol_table_lock);
    );
    
    if(entry){
      symbol_entry_destructor(entry);
      if(entry == new_symbol)
        return NULL;
    }
  }
  else{
    pmath_unref(name);
    pmath_symbol_set_attributes(result, PMATH_SYMBOL_ATTRIBUTE_TEMPORARY);
  }
  
  return result;
}

/*----------------------------------------------------------------------------*/

  static pmath_symbol_t find_symbol_in_namespace(
    pmath_string_t  ns,      // will be freed
    pmath_string_t  name,    // wont be freed; does not contain "`"
    pmath_bool_t create
  ){
    int ns_len;
    
    if(!pmath_instance_of(ns, PMATH_TYPE_STRING)){
      pmath_unref(ns);
      return NULL;
    }

    ns_len = pmath_string_length(ns);
    if(ns_len > 0 && pmath_string_buffer(ns)[ns_len-1] != '`')
      return NULL;
      //ns = pmath_string_insert_latin1(ns, ns_len, "`", 1);
      
    ns = pmath_string_concat(ns, pmath_ref(name));

    return pmath_symbol_get(ns, create);
  }

  static pmath_symbol_t find_short_symbol(
    pmath_string_t  name,  // will be freed; does not contain `'`
    pmath_bool_t create
  ){
    pmath_expr_t namespaces;
    
    pmath_symbol_t symbol = pmath_symbol_get(pmath_ref(name), FALSE);

    if(symbol){
      pmath_unref(name);
      return symbol;
    }
    
    symbol = find_symbol_in_namespace(
      pmath_evaluate(pmath_ref(PMATH_SYMBOL_CURRENTNAMESPACE)),
      name,
      FALSE);
      
    if(symbol){
      pmath_unref(name);
      return symbol;
    }
      
    namespaces = pmath_evaluate(pmath_ref(PMATH_SYMBOL_NAMESPACEPATH));
    if(pmath_instance_of(namespaces, PMATH_TYPE_EXPRESSION)){
      size_t len = pmath_expr_length(namespaces);
      size_t i;
      
      for(i = 1;i <= len;++i){
        symbol = find_symbol_in_namespace(
          pmath_expr_get_item(namespaces, i),
          name,
          FALSE);

        if(symbol){
          pmath_unref(namespaces);
          pmath_unref(name);
          return symbol;
        }
      }
    }
    pmath_unref(namespaces);
    
    if(create){
      symbol = find_symbol_in_namespace(
        pmath_evaluate(pmath_ref(PMATH_SYMBOL_CURRENTNAMESPACE)),
        name,
        TRUE);
    }
    
    pmath_unref(name);
    return symbol;
  }

PMATH_API pmath_symbol_t pmath_symbol_find(
  pmath_string_t  name,  // will be freed; may contain "`"
  pmath_bool_t    create
){
  pmath_symbol_t symbol = pmath_symbol_get(pmath_ref(name), FALSE);
  
  const uint16_t *str;
  int i, len;
  
  if(symbol){
    pmath_unref(name);
    return symbol;
  }

  len = pmath_string_length(name);
  str = pmath_string_buffer(name);

  if(len == 0){
    pmath_unref(name);
    return NULL;
  }

  i = 0;
  while(i < len && str[i] != '`')
    ++i;

  if(i == 0){
    pmath_unref(name);
    return NULL;
  }

  if(i == len)
    return find_short_symbol(name, create);
  
  return pmath_symbol_get(name, create);
}

/*----------------------------------------------------------------------------*/

PMATH_API pmath_string_t pmath_symbol_name(pmath_symbol_t symbol){
  struct _pmath_symbol_t *_symbol = (struct _pmath_symbol_t*)symbol;
  
  if(!symbol)
    return NULL;
  
//  if(_symbol->attributes & PMATH_SYMBOL_ATTRIBUTE_REMOVED){
//    return pmath_string_concat(pmath_ref(_symbol->name), PMATH_C_STRING("/*REMOVED*/"));
//  }
  
  return (pmath_string_t)pmath_ref(_symbol->name);
}

/*----------------------------------------------------------------------------*/

PMATH_API pmath_symbol_attributes_t pmath_symbol_get_attributes(
  pmath_symbol_t  symbol
){
  if(!symbol)
    return 0;

  return ((struct _pmath_symbol_t*)symbol)->attributes;
}

PMATH_API void pmath_symbol_set_attributes(
  pmath_symbol_t             symbol,
  pmath_symbol_attributes_t  attr
){
  if(!symbol)
    return;
  
  ((struct _pmath_timed_t*)symbol)->last_change = _pmath_timer_get_next();
  
  ((struct _pmath_symbol_t*)symbol)->attributes = attr;
}

/*----------------------------------------------------------------------------*/

PMATH_PRIVATE
struct _pmath_symbol_rules_t *_pmath_symbol_get_rules(
  pmath_symbol_t  symbol, 
  rule_access_t   access
){
  struct _pmath_symbol_rules_t *rules;
  assert(pmath_instance_of(symbol, PMATH_TYPE_SYMBOL));
  
  if(access == RULES_WRITE
  && ((struct _pmath_symbol_t*)symbol)->attributes & PMATH_SYMBOL_ATTRIBUTE_PROTECTED){
    if(_pmath_is_running())
      pmath_message(NULL, "wrsym", 1, pmath_ref(symbol));
    return NULL;
  }
  
  if(((struct _pmath_symbol_t*)symbol)->attributes & PMATH_SYMBOL_ATTRIBUTE_THREADLOCAL){
    struct _pmath_symbol_rules_entry_t *entry;
    pmath_thread_t                      parent;
    pmath_thread_t                      me;
    
    me = pmath_thread_get_current();
    
    if(!me)
      return NULL;
    
    rules = NULL;
    for(parent = me;parent;parent = parent->parent){
      entry = (struct _pmath_symbol_rules_entry_t*)
        pmath_ht_search(parent->local_rules, symbol);
      
      if(entry){
        if(parent == me || access == RULES_READ)
          return &entry->rules;
        
        rules = &entry->rules;
        break;
      }
    }
    
    if(!rules){
      rules = ((struct _pmath_symbol_t*)symbol)->u.rules;
      
      pmath_atomic_barrier();
      
      if(rules || access == RULES_READ)
        return rules;
    }
    
    if(!me->local_rules){
      me->local_rules = pmath_ht_create(
        &_pmath_symbol_rules_ht_class, 1);
      
      if(!me->local_rules)
        return NULL;
    }
    
    entry = (struct _pmath_symbol_rules_entry_t*)pmath_mem_alloc(
      sizeof(struct _pmath_symbol_rules_entry_t));
    
    if(!entry)
      return NULL;
    
    entry->key = pmath_ref(symbol);
    
    _pmath_symbol_rules_copy(&entry->rules, rules);
    
    rules = &entry->rules;
    
    entry = pmath_ht_insert(me->local_rules, entry);
      
    assert(entry == NULL);
    
    return rules;
  }
  else{
    struct _pmath_symbol_rules_t *new_rules;
    
    rules = ((struct _pmath_symbol_t*)symbol)->u.rules;
    
    pmath_atomic_barrier();
    
    if(rules || access == RULES_READ)
      return rules;
    
    new_rules = (struct _pmath_symbol_rules_t*)pmath_mem_alloc(sizeof(struct _pmath_symbol_rules_t));
    if(new_rules){
      memset(new_rules, 0, sizeof(struct _pmath_symbol_rules_t));
      
      rules = (struct _pmath_symbol_rules_t*)pmath_atomic_fetch_compare_and_set(
        &((struct _pmath_symbol_t*)symbol)->u.rules_intptr, 
        0, 
        (intptr_t)new_rules);
      
      if(rules == NULL)
        return new_rules;
      
      pmath_mem_free(new_rules);
    }
    
    return rules;
  }
}

PMATH_PRIVATE
pmath_bool_t _pmath_symbol_assign_value(
  pmath_symbol_t  symbol, // wont be freed
  pmath_t  lhs,    // will be freed. typically pmath_ref(symbol)
  pmath_t  rhs     // will be freed
){
  if(PMATH_UNLIKELY( !symbol )){
    pmath_unref(lhs);
    pmath_unref(rhs);
    return TRUE;
  }
  
  if(((struct _pmath_symbol_t*)symbol)->attributes & PMATH_SYMBOL_ATTRIBUTE_PROTECTED){
    if(_pmath_is_running())
      pmath_message(NULL, "wrsym", 1, pmath_ref(symbol));
    pmath_unref(lhs);
    pmath_unref(rhs);
    return FALSE;
  }
  
  if(((struct _pmath_symbol_t*)symbol)->attributes & PMATH_SYMBOL_ATTRIBUTE_THREADLOCAL){
    struct _pmath_object_entry_t *entry;
    struct _pmath_object_entry_t *parent_entry;
    pmath_thread_t                me;
    pmath_thread_t                parent;
    
    me = pmath_thread_get_current();
    
    if(!me){
      pmath_unref(lhs);
      pmath_unref(rhs);
      return FALSE;
    }
    
    if(!me->local_values){
      me->local_values = pmath_ht_create(&pmath_ht_obj_class, 1);
      
      if(!me->local_values){
        pmath_unref(lhs);
        pmath_unref(rhs);
        return FALSE;
      }
      
      entry = NULL;
    }
    else
      entry = pmath_ht_search(me->local_values, symbol);
    
    if(!entry){
      entry = pmath_mem_alloc(sizeof(struct _pmath_object_entry_t));
      
      if(!entry){
        pmath_unref(lhs);
        pmath_unref(rhs);
        return FALSE;
      }
      
      entry->key   = pmath_ref(lhs);
      entry->value = NULL;
      
      for(parent = me->parent;parent;parent = parent->parent){
        parent_entry = pmath_ht_search(parent->local_values, symbol);
        
        if(parent_entry){
          entry->value = pmath_ref(parent_entry->value);
          break;
        }
      }
      
      parent_entry = pmath_ht_insert(me->local_values, entry);
      assert(parent_entry == NULL);
    }
  
    _pmath_symbol_define_value_pos(
      &entry->value,
      lhs,
      rhs);
  }
  else{
    _pmath_symbol_define_value_pos(
      &((struct _pmath_symbol_t*)symbol)->value,
      lhs,
      rhs);
  }
  
  ((struct _pmath_timed_t*)symbol)->last_change = _pmath_timer_get_next();
  if(pmath_atomic_fetch_set(&((struct _pmath_symbol_t*)symbol)->current_dynamic_id, 0) != 0)
    _pmath_dynamic_update(symbol);
    
  return TRUE;
}

PMATH_PRIVATE
PMATH_ATTRIBUTE_NONNULL(1)
pmath_t *_pmath_symbol_get_value_pos(
  pmath_symbol_t  symbol
){
  return &((struct _pmath_symbol_t*)symbol)->value;
}

PMATH_PRIVATE
pmath_t _pmath_symbol_get_global_value(pmath_symbol_t symbol){
  assert(pmath_instance_of(symbol, PMATH_TYPE_SYMBOL));
  
  return _pmath_object_atomic_read(&(((struct _pmath_symbol_t*)symbol)->value));
}

PMATH_API pmath_t pmath_symbol_get_value(pmath_symbol_t symbol){
  if(!symbol)
    return NULL;

  if(((struct _pmath_symbol_t*)symbol)->attributes & PMATH_SYMBOL_ATTRIBUTE_THREADLOCAL){
    return pmath_thread_local_load(symbol);
  }

  return _pmath_object_atomic_read(&(((struct _pmath_symbol_t*)symbol)->value));
}

PMATH_PRIVATE
void _pmath_symbol_set_global_value(
  pmath_symbol_t symbol,
  pmath_t value
){
  assert(pmath_instance_of(symbol, PMATH_TYPE_SYMBOL));
  
  ((struct _pmath_timed_t*)symbol)->last_change = _pmath_timer_get_next();
  if(pmath_atomic_fetch_set(&((struct _pmath_symbol_t*)symbol)->current_dynamic_id, 0) != 0)
    _pmath_dynamic_update(symbol);

  _pmath_object_atomic_write(
    &(((struct _pmath_symbol_t*)symbol)->value),
    value);
}

PMATH_API void pmath_symbol_set_value(
  pmath_symbol_t symbol,
  pmath_t value
){
  if(!symbol){
    pmath_unref(value);
    return;
  }
  
  ((struct _pmath_timed_t*)symbol)->last_change = _pmath_timer_get_next();
  if(pmath_atomic_fetch_set(&((struct _pmath_symbol_t*)symbol)->current_dynamic_id, 0) != 0)
    _pmath_dynamic_update(symbol);

  if(((struct _pmath_symbol_t*)symbol)->attributes & PMATH_SYMBOL_ATTRIBUTE_THREADLOCAL){
    pmath_unref(pmath_thread_local_save(symbol, value));
    return;
  }

  _pmath_object_atomic_write(
    &(((struct _pmath_symbol_t*)symbol)->value),
    value);
}

/*----------------------------------------------------------------------------*/

PMATH_API void pmath_symbol_synchronized(
  pmath_symbol_t          symbol,
  pmath_callback_t   callback,
  void                   *data
){
  if(symbol){
    // ensure that the symbol wont be freed during synchronization:
    symbol = pmath_ref(symbol);
    pmath_thread_call_locked(
      &((struct _pmath_symbol_t*)symbol)->lock,
      callback,
      data);
    pmath_unref(symbol);
  }
}

/*----------------------------------------------------------------------------*/

PMATH_API void pmath_symbol_update(pmath_symbol_t symbol){
  struct _pmath_symbol_t *_sym = (struct _pmath_symbol_t*)symbol;
  
  if(PMATH_UNLIKELY(!_sym))
    return;

  assert(pmath_instance_of(symbol, PMATH_TYPE_SYMBOL));
  
  _sym->inherited.inherited.last_change = _pmath_timer_get_next();
  
  if(pmath_atomic_fetch_set(&_sym->current_dynamic_id, 0) != 0)
    _pmath_dynamic_update(symbol);
}

PMATH_PRIVATE
void _pmath_symbol_track_dynamic(
  pmath_symbol_t symbol, // wont be freed
  intptr_t       id
){
  struct _pmath_symbol_t *_sym = (struct _pmath_symbol_t*)symbol;
  
  if(PMATH_UNLIKELY(!_sym))
    return;

  assert(pmath_instance_of(symbol, PMATH_TYPE_SYMBOL));
  
  if(_sym->current_dynamic_id != id){
    _sym->current_dynamic_id = id;
    
    _pmath_dynamic_bind(symbol, id);
  }
}

/*----------------------------------------------------------------------------*/

PMATH_API
void pmath_symbol_remove(pmath_symbol_t symbol){
  if(symbol){
    pmath_symbol_attributes_t attr;
    
    assert(pmath_instance_of(symbol, PMATH_TYPE_SYMBOL));
    
    attr = pmath_symbol_get_attributes(symbol);
    if(attr & PMATH_SYMBOL_ATTRIBUTE_PROTECTED){
      pmath_message(PMATH_SYMBOL_REMOVE, "rmptc", 1, symbol);
      return;
    }
    
    pmath_symbol_set_attributes(symbol, attr | PMATH_SYMBOL_ATTRIBUTE_TEMPORARY | PMATH_SYMBOL_ATTRIBUTE_REMOVED);
    _pmath_clear(symbol, TRUE);
    
    {
      unsigned int i, cap;
      pmath_t replacement;
      
      // We do not have to reevaluate pmath_ht_capacity() in the loop,
      // because a hashtable never shrinks.
      PMATH_DEBUG_TIMING(
        pmath_atomic_lock(&global_symbol_table_lock);
        {
          cap = pmath_ht_capacity(global_symbol_table);
        }
        pmath_atomic_unlock(&global_symbol_table_lock);
      );
      
      replacement = pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_SYMBOL), 1,
        pmath_symbol_name(symbol));
      
      for(i = 0;i < cap && symbol->refcount > 1;++i){
        pmath_symbol_t entry;
        
        PMATH_DEBUG_TIMING(
          pmath_atomic_lock(&global_symbol_table_lock);
          {
            entry = pmath_ref(pmath_ht_entry(global_symbol_table, i));
          }
          pmath_atomic_unlock(&global_symbol_table_lock);
        );
        
        if(entry){
          pmath_t value;
          struct _pmath_symbol_rules_t *rules;
          
          value = pmath_symbol_get_value(entry);
          value = _pmath_symbol_value_remove_all(value, symbol, replacement);
          pmath_symbol_set_value(entry, value);
          
          rules = _pmath_symbol_get_rules(entry, RULES_READ);
          if(rules)
            _pmath_symbol_rules_remove_all(rules, symbol, replacement);
          
          pmath_unref(entry);
        }
      }
      
      pmath_unref(replacement);
    }
    
//    pmath_thread_call_locked(
//      &global_symbol_table_threadlock,
//      (pmath_callback_t)symbol_remove_callback,
//      symbol);
    
    pmath_unref(symbol);
  }
}

/*----------------------------------------------------------------------------*/

//{ pMath object functions ...

static void destroy_symbol(struct _pmath_symbol_t *symbol){
  if((symbol->attributes & PMATH_SYMBOL_ATTRIBUTE_TEMPORARY) 
  && _pmath_is_running()){
    
    if(pmath_atomic_compare_and_set(&symbol->inherited.inherited.inherited.refcount, 0, 1)){
      void *removed_entry;
      
      assert(symbol->name->refcount >= 1);
      assert(symbol->name->type_shift == PMATH_TYPE_SHIFT_STRING);
      
      {
        PMATH_DEBUG_TIMING(
          pmath_atomic_lock(&global_symbol_table_lock);
          {
            removed_entry = pmath_ht_remove(
              global_symbol_table, 
              symbol->name);
            if(removed_entry)
              post_remove(removed_entry);
          }
          pmath_atomic_unlock(&global_symbol_table_lock);
        );
        
        #ifdef PMATH_DEBUG_LOG
        if(removed_entry != symbol 
        || symbol->inherited.inherited.inherited.refcount != 1){
          pmath_symbol_t entry;
          unsigned int count, cap, i;
          
          PMATH_DEBUG_TIMING(
            pmath_atomic_lock(&global_symbol_table_lock);
            {
              count = pmath_ht_count(   global_symbol_table);
              cap   = pmath_ht_capacity(global_symbol_table);
            }
            pmath_atomic_unlock(&global_symbol_table_lock);
          );
          
          pmath_debug_print_object("\aHashtable corrupted?: ",(pmath_symbol_t)symbol,"");
          pmath_debug_print(" [hash= %u]\n", pmath_hash((pmath_symbol_t)symbol));
          
          pmath_debug_print_object("\aremoved: ",(pmath_t)removed_entry,"");
          pmath_debug_print(" [hash= %u]\n", pmath_hash((pmath_symbol_t)removed_entry));
          
          pmath_debug_print(">-------------------------------\n");
          pmath_debug_print("\tcount=%u,cap=%u\n", count, cap);
          for(i = 0;i < cap;++i){
            pmath_debug_print("\t%u:\t", i);
            
            PMATH_DEBUG_TIMING(
              pmath_atomic_lock(&global_symbol_table_lock);
              {
                entry = pmath_ref(pmath_ht_entry(global_symbol_table, i));
              }
              pmath_atomic_unlock(&global_symbol_table_lock);
            );
            
            if(!entry){
              pmath_debug_print("NULL\n");
            }
            else{
              pmath_debug_print("[hash= %u]\t", pmath_hash(entry));
              pmath_debug_print_object("", entry, "\n");
              pmath_unref(entry);
            }
          }
          pmath_debug_print("<-------------------------------\n");
          
          PMATH_DEBUG_TIMING(
            pmath_atomic_lock(&global_symbol_table_lock);
            {
              pre_insert(removed_entry);
              removed_entry = pmath_ht_insert(global_symbol_table, removed_entry);
              if(removed_entry)
                post_remove(removed_entry);
            }
            pmath_atomic_unlock(&global_symbol_table_lock);
          );
        }
        #endif
      }
      
      if(removed_entry){
        symbol_entry_destructor(removed_entry);
      }
    }
  }
}

static pmath_bool_t equal_symbols(
  struct _pmath_symbol_t  *symA,
  struct _pmath_symbol_t  *symB
){
  return symA == symB;
}

static void write_symbol(
  pmath_symbol_t          symbol,
  pmath_write_options_t   options,
  pmath_write_func_t      write,
  void                   *user
){
  pmath_string_t name;
  const uint16_t *str;
  int len;
  
  name = pmath_symbol_name(symbol);
  len = pmath_string_length(name);
  str = pmath_string_buffer(name);
  
  if(pmath_symbol_get_attributes(symbol) & PMATH_SYMBOL_ATTRIBUTE_REMOVED){
    write_cstr("Symbol(", write, user);
    pmath_write(name, options, write, user);
    write_cstr(")", write, user);
    pmath_unref(name);
    return;
  }

  if((options & PMATH_WRITE_OPTIONS_FULLNAME) == 0){
    int i = len + 1;
    do{
      --i;
      while(i > 0 && str[i-1] != '`')
        --i;

      if(i > 0){
        pmath_symbol_t found = pmath_symbol_find(
          pmath_string_part(pmath_ref(name), i, -1),
          FALSE);
        pmath_unref(found);
        if(found == symbol){
          write(user, str + i, len - i);
          pmath_unref(name);
          return;
        }
      }
    }while(i > 0);
  }

  write(user, str, len);
  pmath_unref(name);
}

//}

PMATH_PRIVATE pmath_bool_t _pmath_symbols_init(void){
  assert(global_symbol_table_lock == 0);
  
  memset(&unused_symbols, 0, sizeof(unused_symbols));
  
  global_symbol_table = pmath_ht_create(&symbol_table_class,
    PMATH_BUILTIN_SYMBOL_COUNT);
  if(!global_symbol_table)
    goto FAIL_GLOBAL_SYMBOL_TABLE;

  _pmath_init_special_type(
    PMATH_TYPE_SHIFT_SYMBOL,
    (pmath_compare_func_t)        _pmath_compare_exprsym,
    (pmath_hash_func_t)           _pmath_hash_pointer,
    (pmath_proc_t)                destroy_symbol,
    (pmath_equal_func_t)          equal_symbols,
    (_pmath_object_write_func_t)  write_symbol);

  return TRUE;

 FAIL_GLOBAL_SYMBOL_TABLE:
  return FALSE;
}

PMATH_PRIVATE void _pmath_symbols_done(void){
  unsigned int i, cap;
  
  cap = pmath_ht_capacity(global_symbol_table);
  for(i = 0;i < cap;++i){
    struct _pmath_symbol_t *symbol = pmath_ht_entry(global_symbol_table, i);
    
    if(symbol){
      pmath_unref(symbol->value);
      symbol->value = PMATH_UNDEFINED;
      
      if(symbol->u.rules){
        _pmath_symbol_rules_done(symbol->u.rules);
      
        pmath_mem_free(symbol->u.rules);
        
        symbol->u.rules = NULL;
      }
    }
  }
  
  #ifdef PMATH_DEBUG_LOG
  for(i = 0;i < cap;++i){
    struct _pmath_symbol_t *symbol = pmath_ht_entry(global_symbol_table, i);
    
    if(symbol && symbol->inherited.inherited.inherited.refcount != 0){
      if(pmath_ht_search(global_symbol_table, symbol->name) != NULL){
        pmath_debug_print_object("\aSymbol '", (pmath_symbol_t)symbol, "'");
        pmath_debug_print(" (%p) still has %"PRIuPTR" reference(s)\n",
          symbol,
          symbol->inherited.inherited.inherited.refcount);
      }
      else{
        pmath_debug_print_object("\a\aHashtable corrupted: lost symbol '", (pmath_symbol_t)symbol, "'");
        pmath_debug_print(" (%p) still has %"PRIuPTR" reference(s)\n",
          symbol,
          symbol->inherited.inherited.inherited.refcount);
      }
    }
  }
  #endif
  
  pmath_ht_destroy(global_symbol_table);
  destroy_all_unused_symbols();
}

/*----------------------------------------------------------------------------*/

PMATH_PRIVATE void _pmath_symbols_memory_panic(void){
  destroy_all_unused_symbols();
}
