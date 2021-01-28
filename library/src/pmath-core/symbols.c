#include <pmath-core/symbols-private.h>

#include <pmath-core/expressions-private.h>
#include <pmath-core/strings-private.h>

#include <pmath-util/concurrency/atomic-private.h>
#include <pmath-util/concurrency/threadmsg.h>
#include <pmath-util/concurrency/threads-private.h>
#include <pmath-util/concurrency/threadpool-private.h>
#include <pmath-util/debug.h>
#include <pmath-util/dynamic-private.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/memory.h>
#include <pmath-util/messages.h>
#include <pmath-util/stacks-private.h>
#include <pmath-util/symbol-values-private.h>
#include <pmath-util/user-format-private.h>

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
      _on_long_wait(__LINE__, PMATH_DEBUG_TIMING_START, pmath_tickcount()); \
    } \
  }while(0)

static void _on_long_wait(int line, double start, double end) {
  pmath_debug_print("%s line %d: LONG WAIT (%f sec from %f to %f)\n", __FILE__, line, end - start, start, end);
}

#else

#define PMATH_DEBUG_TIMING(CODE)  do{ CODE }while(0)

#endif

struct _pmath_symbol_t {
  struct _pmath_gc_t         inherited;
  
  struct _pmath_symbol_t    *prev;
  struct _pmath_symbol_t    *next;
  
  volatile pmath_string_t    name;
  pmath_threadlock_t         lock;
  pmath_symbol_attributes_t  attributes;
  pmath_locked_t             value;
  
  /** The Up-, Down-, Sub-, ...-Rules as a  struct _pmath_symbol_rules_t *
   */
  pmath_atomic_t rules;
  
  /** Identifier of the last dynamic object that was interrested in this symbol but was not 
      yet informed about a change.
  
      A value of 0 means that no dynamic object is currently interrested in this symbol
      and thus no change notifications should be performed.
   */
  pmath_atomic_t current_dynamic_id;
  
  /** Identifier of a dynamic object that this symbol cannot be bound to.
      
      This is set by pmath_symbol_create_temporary() to allow for assignments to local symbols
      during a dynamic evaluation.
   */
  pmath_atomic_t ignore_dynamic_id;
};

extern pmath_symbol_t pmath_System_Degree;

//{ global symbol table ...
static pmath_atomic_t global_symbol_table_lock = PMATH_ATOMIC_STATIC_INIT;

static struct _pmath_symbol_t *global_first;

static pmath_hashtable_t global_symbol_table;

// global_symbol_table_lock must be held:
static void pre_insert(struct _pmath_symbol_t *symbol) {
  if(global_first) {
    global_first->next->prev = symbol;
    symbol->next             = global_first->next;
    global_first->next       = symbol;
    symbol->prev             = global_first;
  }
  else {
    global_first = symbol->prev = symbol->next = symbol;
  }
}

// global_symbol_table_lock must be held:
static void post_remove(struct _pmath_symbol_t *symbol) {
  if(global_first == symbol) {
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

static void destroy_all_unused_symbols(void) {
  void *item;
  while((item = pmath_stack_pop(&unused_symbols)) != NULL) {
    pmath_mem_free(item);
  }
}

static struct _pmath_symbol_t *create_symbol(void) {
  struct _pmath_symbol_t *symbol = pmath_stack_pop(&unused_symbols);
  
  if(symbol) {
    symbol->inherited.inherited.inherited.type_shift = PMATH_TYPE_SHIFT_SYMBOL;
    
    pmath_atomic_write_uint8_release( &symbol->inherited.inherited.inherited.flags8,  0);
    pmath_atomic_write_uint16_release(&symbol->inherited.inherited.inherited.flags16, 0);
    pmath_atomic_write_release(&symbol->inherited.inherited.inherited.refcount, 1);
  }
  else {
    symbol = (void *)PMATH_AS_PTR(_pmath_create_stub(
                                    PMATH_TYPE_SHIFT_SYMBOL,
                                    sizeof(struct _pmath_symbol_t)));
  }
  
  if(symbol) {
    symbol->inherited.inherited.last_change = _pmath_timer_get_next();
    symbol->inherited.gc_refcount           = 0;
    symbol->prev                            = NULL;
    symbol->next                            = NULL;
    pmath_atomic_write_uint32_release(&PMATH_GC_FLAGS32(&symbol->inherited), 0);
    pmath_atomic_write_release(&symbol->current_dynamic_id, 0);
    pmath_atomic_write_release(&symbol->ignore_dynamic_id,  0);
  }
  
  return symbol;
}

//}
//{ hash table functions ...

static void symbol_entry_destructor(void *p) {
  struct _pmath_symbol_t *symbol = p;
  struct _pmath_symbol_rules_t *rules;
  
  pmath_unref(symbol->name);
  pmath_unref(symbol->value._data);
  
  rules = (void *)pmath_atomic_read_aquire(&symbol->rules);
  if(rules) {
    _pmath_symbol_rules_done(rules);
    
    pmath_mem_free(rules);
  }
  
  pmath_stack_push(&unused_symbols, symbol);
}

static unsigned int symbol_entry_hash(void *p) {
  struct _pmath_symbol_t *symbol = p;
  return pmath_hash(symbol->name);
}

static pmath_bool_t symbol_entry_keys_equal(void *e1, void *e2) {
  struct _pmath_symbol_t *symbol1 = e1;
  struct _pmath_symbol_t *symbol2 = e2;
  
  if(e1 == e2)
    return TRUE;
  if(symbol1->attributes & PMATH_SYMBOL_ATTRIBUTE_REMOVED)
    return FALSE;
  if(symbol2->attributes & PMATH_SYMBOL_ATTRIBUTE_REMOVED)
    return FALSE;
  return pmath_equals(symbol1->name, symbol2->name);
}

static unsigned int symbol_entry_key_hash(void *key) {
  return pmath_hash(*(pmath_t *)key);
}

static pmath_bool_t symbol_entry_equals_key(void *e, void *key) {
  struct _pmath_symbol_t *symbol = e;
  
  /* Allow to remove the symbol with pmath_ht_remove(..., &symbol->name) 
     even if it already has the REMOVED attribute, which makes it invisible.
   */
  if(key == &symbol->name)
    return TRUE;
  if(symbol->attributes & PMATH_SYMBOL_ATTRIBUTE_REMOVED)
    return FALSE;
  return pmath_equals(symbol->name, *(pmath_t *)key);
}

//}

static const pmath_ht_class_t symbol_table_class = {
  symbol_entry_destructor,
  symbol_entry_hash,
  symbol_entry_keys_equal,
  symbol_entry_key_hash,
  symbol_entry_equals_key
};

PMATH_API pmath_symbol_t pmath_symbol_iter_next(pmath_symbol_t old) {
  pmath_symbol_t result;
  
  if(pmath_is_null(old))
    return PMATH_NULL;
    
  PMATH_DEBUG_TIMING( {
    pmath_atomic_lock(&global_symbol_table_lock);
    {
      result = pmath_ref(PMATH_FROM_PTR(
        ((struct _pmath_symbol_t *)PMATH_AS_PTR(old))->next));
    }
    pmath_atomic_unlock(&global_symbol_table_lock);
  });
  
  pmath_unref(old);
  return result;
}

/*----------------------------------------------------------------------------*/

struct symbol_refcounter_t {
  pmath_symbol_t symbol;
  intptr_t       count;
};

static enum pmath_visit_result_t count_symbol_callback(pmath_t obj, void *data) {
  struct symbol_refcounter_t *counter = data;
  
  if(pmath_same(counter->symbol, obj))
    ++counter->count;
    
  return PMATH_VISIT_NORMAL;
}

// does not yet check thread local storage
PMATH_PRIVATE
intptr_t _pmath_symbol_self_refcount(pmath_symbol_t symbol) {
  struct symbol_refcounter_t     counter;
  struct _pmath_symbol_rules_t  *rules;
  
  if(pmath_is_null(symbol))
    return 0;
    
  assert(pmath_is_symbol(symbol));
  
  counter.symbol = symbol;
  counter.count  = 0;
  
  _pmath_symbol_value_visit(
    _pmath_symbol_get_global_value(symbol),
    count_symbol_callback,
    &counter);
    
  rules = _pmath_symbol_get_rules(symbol, RULES_READ);
  
  if(rules)
    _pmath_symbol_rules_visit(rules, count_symbol_callback, &counter);
    
  if(_pmath_have_code(symbol, PMATH_CODE_USAGE_DOWNCALL))
    ++counter.count;
    
  if(_pmath_have_code(symbol, PMATH_CODE_USAGE_UPCALL))
    ++counter.count;
    
  if(_pmath_have_code(symbol, PMATH_CODE_USAGE_SUBCALL))
    ++counter.count;
    
  return counter.count;
}

/*----------------------------------------------------------------------------*/

PMATH_API pmath_symbol_t pmath_symbol_get(
  pmath_string_t  name,   // will be freed
  pmath_bool_t    create
) {
  pmath_symbol_attributes_t attr;
  pmath_symbol_t result = PMATH_NULL;
  
  if(pmath_string_length(name) == 1) {
    uint16_t ch = *pmath_string_buffer(&name);
    
    switch(ch) {
      case 0x00B0:
        pmath_unref(name);
        return pmath_ref(pmath_System_Degree);
        
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
    result = pmath_ref(PMATH_FROM_PTR(
                         pmath_ht_search(global_symbol_table, &name)));
  }
  pmath_atomic_unlock(&global_symbol_table_lock);
  );
  
  if(!pmath_is_null(result)) {
    attr = pmath_symbol_get_attributes(result);
    if(attr & PMATH_SYMBOL_ATTRIBUTE_REMOVED) {
      if(create) {
        pmath_symbol_set_attributes(result, attr & ~PMATH_SYMBOL_ATTRIBUTE_REMOVED);
        pmath_unref(name);
        return result;
      }
      else {
        pmath_unref(result);
        result = PMATH_NULL;
      }
    }
    else {
      pmath_unref(name);
      return result;
    }
  }
  
  if(create) {
    struct _pmath_symbol_t *entry;
    struct _pmath_symbol_t *new_symbol = create_symbol();
    result = PMATH_FROM_PTR(new_symbol);
    if(!new_symbol) {
      pmath_unref(name);
      return PMATH_NULL;
    }
    
    new_symbol->name        = _pmath_string_set_debug_info(pmath_ref(name), PMATH_NULL);
    new_symbol->lock        = NULL;
    new_symbol->attributes  = 0;
    new_symbol->value._data = PMATH_UNDEFINED;
    pmath_atomic_write_release(&new_symbol->rules, 0);
    
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
    
    if(entry) {
      symbol_entry_destructor(entry);
      if(entry == new_symbol) {
        pmath_unref(name);
        return PMATH_NULL;
      }
    }
    
    if(_pmath_is_running()) {
      pmath_t newsym = pmath_symbol_get_value(PMATH_SYMBOL_NEWSYMBOL);
      
      if(!pmath_is_null(newsym)) {
        const uint16_t *buf = pmath_string_buffer(&name);
        int             len = pmath_string_length(name);
        
        pmath_string_t ns, n;
        
        if(len > 0) {
          do {
            --len;
          } while(len > 0 && buf[len] != '`');
        }
        
        ns = pmath_string_part(pmath_ref(name), 0, len);
        n  = pmath_string_part(pmath_ref(name), len + 1, -1);
        
        pmath_unref(pmath_evaluate(pmath_expr_new_extended(newsym, 2, n, ns)));
      }
    }
  }
  
  pmath_unref(name);
  
  return result;
}

/*----------------------------------------------------------------------------*/

static pmath_atomic_t _pmath_tmp_name_counter = PMATH_ATOMIC_STATIC_INIT;

PMATH_API pmath_symbol_t pmath_symbol_create_temporary(
  pmath_string_t name,
  pmath_bool_t   unique
) {
  pmath_symbol_t result;
  const uint16_t *buf;
  int len;
  char val[20];
  
  buf = pmath_string_buffer(&name);
  len = pmath_string_length(name);
  
  --len;
  while(len >= 0 && buf[len] >= '0' && buf[len] <= '9')
    --len;
    
  if(len > 0 && buf[len] == '$') {
    name = pmath_string_part(name, 0, len);
  }
  
  if(unique) {
    snprintf(val, sizeof(val), "$%"PRIuPTR,
             pmath_atomic_fetch_add(&_pmath_tmp_name_counter, 1));
             
    name = pmath_string_insert_latin1(name, INT_MAX, val, -1);
  }
  else
    name = pmath_string_insert_latin1(name, INT_MAX, "$", 1);
    
  PMATH_DEBUG_TIMING(
    pmath_atomic_lock(&global_symbol_table_lock);
  {
    result = pmath_ref(PMATH_FROM_PTR(
                         pmath_ht_search(global_symbol_table, &name)));
  }
  pmath_atomic_unlock(&global_symbol_table_lock);
  );
  
  if(pmath_is_null(result)) {
    struct _pmath_symbol_t *entry;
    struct _pmath_symbol_t *new_symbol = create_symbol();
    pmath_thread_t thread = pmath_thread_get_current();
    
    result = PMATH_FROM_PTR(new_symbol);
    if(!new_symbol) {
      pmath_unref(name);
      return PMATH_NULL;
    }
    
    //new_symbol->last_update = (uintptr_t)global_update_counter;
    new_symbol->lock        = NULL;
    new_symbol->name        = _pmath_string_set_debug_info(name, PMATH_NULL);
    new_symbol->attributes  = PMATH_SYMBOL_ATTRIBUTE_TEMPORARY;
    new_symbol->value._data = PMATH_UNDEFINED;
    pmath_atomic_write_release(&new_symbol->rules, 0);
    if(thread)
      pmath_atomic_write_release(&new_symbol->ignore_dynamic_id, thread->current_dynamic_id);
      
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
    
    if(entry) {
      symbol_entry_destructor(entry);
      if(entry == new_symbol)
        return PMATH_NULL;
    }
  }
  else {
    pmath_unref(name);
    pmath_symbol_set_attributes(result, PMATH_SYMBOL_ATTRIBUTE_TEMPORARY);
  }
  
  return result;
}

/*----------------------------------------------------------------------------*/

static pmath_symbol_t find_symbol_in_namespace(
  pmath_string_t  ns,      // will be freed
  pmath_string_t  name,    // wont be freed; does not contain "`"
  pmath_bool_t    create
) {
  int ns_len;
  
  if(!pmath_is_string(ns)) {
    pmath_unref(ns);
    return PMATH_NULL;
  }
  
  ns_len = pmath_string_length(ns);
  if(ns_len > 0 && pmath_string_buffer(&ns)[ns_len - 1] != '`')
    return PMATH_NULL;
  //ns = pmath_string_insert_latin1(ns, ns_len, "`", 1);
  
  ns = pmath_string_concat(ns, pmath_ref(name));
  
  return pmath_symbol_get(ns, create);
}

static pmath_symbol_t find_short_symbol(
  pmath_string_t  name,  // will be freed; does not contain `'`
  pmath_bool_t    create
) {
  pmath_expr_t namespaces;
  
  pmath_symbol_t symbol = pmath_symbol_get(pmath_ref(name), FALSE);
  
  if(!pmath_is_null(symbol)) {
    pmath_unref(name);
    return symbol;
  }
  
  symbol = find_symbol_in_namespace(
             pmath_evaluate(pmath_ref(PMATH_SYMBOL_CURRENTNAMESPACE)),
             name,
             FALSE);
             
  if(!pmath_is_null(symbol)) {
    pmath_unref(name);
    return symbol;
  }
  
  namespaces = pmath_evaluate(pmath_ref(PMATH_SYMBOL_NAMESPACEPATH));
  if(pmath_is_expr(namespaces)) {
    size_t len = pmath_expr_length(namespaces);
    size_t i;
    
    for(i = 1; i <= len; ++i) {
      symbol = find_symbol_in_namespace(
                 pmath_expr_get_item(namespaces, i),
                 name,
                 FALSE);
                 
      if(!pmath_is_null(symbol)) {
        pmath_unref(namespaces);
        pmath_unref(name);
        return symbol;
      }
    }
  }
  pmath_unref(namespaces);
  
  if(create) {
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
) {
  pmath_symbol_t symbol = pmath_symbol_get(pmath_ref(name), FALSE);
  
  const uint16_t *str;
  int i, len;
  
  if(!pmath_is_null(symbol)) {
    pmath_unref(name);
    return symbol;
  }
  
  len = pmath_string_length(name);
  str = pmath_string_buffer(&name);
  
  if(len == 0) {
    pmath_unref(name);
    return PMATH_NULL;
  }
  
  i = 0;
  while(i < len && str[i] != '`')
    ++i;
    
  if(i == 0) {
    pmath_unref(name);
    return PMATH_NULL;
  }
  
  if(i == len)
    return find_short_symbol(name, create);
    
  return pmath_symbol_get(name, create);
}

/*----------------------------------------------------------------------------*/

PMATH_API pmath_string_t pmath_symbol_name(pmath_symbol_t symbol) {
  struct _pmath_symbol_t *_symbol = (struct _pmath_symbol_t *)PMATH_AS_PTR(symbol);
  
  if(pmath_is_null(symbol))
    return PMATH_NULL;
    
//  if(_symbol->attributes & PMATH_SYMBOL_ATTRIBUTE_REMOVED){
//    return pmath_string_concat(pmath_ref(_symbol->name), PMATH_C_STRING("/*REMOVED*/"));
//  }

  return pmath_ref(_symbol->name);
}

/*----------------------------------------------------------------------------*/

PMATH_API pmath_symbol_attributes_t pmath_symbol_get_attributes(
  pmath_symbol_t  symbol
) {
  if(pmath_is_null(symbol))
    return 0;
    
  return ((struct _pmath_symbol_t *)PMATH_AS_PTR(symbol))->attributes;
}

PMATH_API void pmath_symbol_set_attributes(
  pmath_symbol_t             symbol,
  pmath_symbol_attributes_t  attr
) {
  struct _pmath_symbol_t *sym_ptr;
  
  if(pmath_is_null(symbol))
    return;
    
  sym_ptr = (struct _pmath_symbol_t *)PMATH_AS_PTR(symbol);
  sym_ptr->inherited.inherited.last_change = _pmath_timer_get_next();
  
  sym_ptr->attributes = attr;
  
  if(pmath_atomic_fetch_set(&sym_ptr->current_dynamic_id, 0) != 0)
    _pmath_dynamic_update(symbol);
}

/*----------------------------------------------------------------------------*/

PMATH_PRIVATE
struct _pmath_symbol_rules_t *_pmath_symbol_get_rules(
  pmath_symbol_t  symbol,
  rule_access_t   access
) {
  struct _pmath_symbol_rules_t *rules;
  
  assert(pmath_is_symbol(symbol));
  
  if( access == RULES_WRITE &&
      (PMATH_SYMBOL_ATTRIBUTE_PROTECTED & ((struct _pmath_symbol_t *)PMATH_AS_PTR(symbol))->attributes))
  {
    if(_pmath_is_running())
      pmath_message(PMATH_NULL, "wrsym", 1, pmath_ref(symbol));
    return NULL;
  }
  
  if(PMATH_SYMBOL_ATTRIBUTE_THREADLOCAL & ((struct _pmath_symbol_t *)PMATH_AS_PTR(symbol))->attributes) {
    struct _pmath_symbol_rules_entry_t *entry;
    pmath_thread_t                      parent;
    pmath_thread_t                      me;
    
    me = pmath_thread_get_current();
    
    if(!me)
      return NULL;
      
    rules = NULL;
    for(parent = me; parent; parent = parent->parent) {
      entry = pmath_ht_search(parent->local_rules, &symbol);
      
      if(entry) {
        if(parent == me || access == RULES_READ)
          return &entry->rules;
          
        rules = &entry->rules;
        break;
      }
    }
    
    if(!rules) {
      rules = (void *)pmath_atomic_read_aquire(
                &((struct _pmath_symbol_t *)PMATH_AS_PTR(symbol))->rules);
                
      if(rules || access == RULES_READ)
        return rules;
    }
    
    if(!me->local_rules) {
      me->local_rules = pmath_ht_create(
                          &_pmath_symbol_rules_ht_class, 1);
                          
      if(!me->local_rules)
        return NULL;
    }
    
    entry = pmath_mem_alloc(sizeof(struct _pmath_symbol_rules_entry_t));
              
    if(!entry)
      return NULL;
      
    entry->key = pmath_ref(symbol);
    
    _pmath_symbol_rules_copy(&entry->rules, rules);
    
    rules = &entry->rules;
    
    entry = pmath_ht_insert(me->local_rules, entry);
    if(entry) { // Out Of Memory
      _pmath_symbol_rules_ht_class.entry_destructor(entry);
    }
    
    return rules;
  }
  else {
    struct _pmath_symbol_rules_t *new_rules;
    
    rules = (void *)pmath_atomic_read_aquire(
              &((struct _pmath_symbol_t *)PMATH_AS_PTR(symbol))->rules);
              
    if(rules || access == RULES_READ)
      return rules;
      
    new_rules = pmath_mem_alloc(sizeof(struct _pmath_symbol_rules_t));
    if(new_rules) {
      _pmath_symbol_rules_copy(new_rules, NULL);
      
      rules = (void *)pmath_atomic_fetch_compare_and_set(
                &((struct _pmath_symbol_t *)PMATH_AS_PTR(symbol))->rules,
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
  pmath_t         lhs,    // will be freed. typically pmath_ref(symbol)
  pmath_t         rhs     // will be freed
) {
  struct _pmath_symbol_t *sym_ptr = (void *)PMATH_AS_PTR(symbol);
  
  if(PMATH_UNLIKELY(!sym_ptr)) {
    pmath_unref(lhs);
    pmath_unref(rhs);
    return TRUE;
  }
  
  if(sym_ptr->attributes & PMATH_SYMBOL_ATTRIBUTE_PROTECTED) {
    if(_pmath_is_running())
      pmath_message(PMATH_NULL, "wrsym", 1, pmath_ref(symbol));
    pmath_unref(lhs);
    pmath_unref(rhs);
    return FALSE;
  }
  
  if(sym_ptr->attributes & PMATH_SYMBOL_ATTRIBUTE_THREADLOCAL) {
    struct _pmath_object_entry_t *entry;
    struct _pmath_object_entry_t *parent_entry;
    pmath_thread_t                me;
    pmath_thread_t                parent;
    
    me = pmath_thread_get_current();
    
    if(!me) {
      pmath_unref(lhs);
      pmath_unref(rhs);
      return FALSE;
    }
    
    if(!me->local_values) {
      me->local_values = pmath_ht_create(&pmath_ht_obj_class, 1);
      
      if(!me->local_values) {
        pmath_unref(lhs);
        pmath_unref(rhs);
        return FALSE;
      }
      
      entry = NULL;
    }
    else
      entry = pmath_ht_search(me->local_values, &symbol);
      
    if(!entry) {
      entry = pmath_mem_alloc(sizeof(struct _pmath_object_entry_t));
      
      if(!entry) {
        pmath_unref(lhs);
        pmath_unref(rhs);
        return FALSE;
      }
      
      entry->key   = pmath_ref(lhs);
      entry->value = PMATH_NULL;
      
      for(parent = me->parent; parent; parent = parent->parent) {
        parent_entry = pmath_ht_search(parent->local_values, &symbol);
        
        if(parent_entry) {
          entry->value = pmath_ref(parent_entry->value);
          break;
        }
      }
      
      parent_entry = pmath_ht_insert(me->local_values, entry);
      if(parent_entry)
        pmath_ht_obj_class.entry_destructor(parent_entry);
    }
    
    if(!_pmath_symbol_define_value_pos((pmath_locked_t *)&entry->value, lhs, rhs))
      return TRUE;
  }
  else if(!_pmath_symbol_define_value_pos(&sym_ptr->value, lhs, rhs))
    return TRUE;
  
  sym_ptr->inherited.inherited.last_change = _pmath_timer_get_next();
  if(pmath_atomic_fetch_set(&sym_ptr->current_dynamic_id, 0) != 0)
    _pmath_dynamic_update(symbol);
    
  return TRUE;
}

PMATH_PRIVATE
pmath_t _pmath_symbol_get_global_value(pmath_symbol_t symbol) {
  assert(pmath_is_symbol(symbol));
  
  return _pmath_object_atomic_read(
           &(((struct _pmath_symbol_t *)PMATH_AS_PTR(symbol))->value));
}

PMATH_API pmath_t pmath_symbol_get_value(pmath_symbol_t symbol) {
  struct _pmath_symbol_t *sym_ptr = (void *)PMATH_AS_PTR(symbol);
  
  if(!sym_ptr)
    return PMATH_NULL;
    
  if(sym_ptr->attributes & PMATH_SYMBOL_ATTRIBUTE_THREADLOCAL) {
    return pmath_thread_local_load(symbol);
  }
  
  return _pmath_object_atomic_read(&sym_ptr->value);
}

PMATH_PRIVATE
void _pmath_symbol_set_global_value(
  pmath_symbol_t symbol,
  pmath_t        value
) {
  struct _pmath_symbol_t *sym_ptr;
  
  assert(pmath_is_symbol(symbol));
  
  sym_ptr = (void *)PMATH_AS_PTR(symbol);
  
  sym_ptr->inherited.inherited.last_change = _pmath_timer_get_next();
  if(pmath_atomic_fetch_set(&sym_ptr->current_dynamic_id, 0) != 0)
    _pmath_dynamic_update(symbol);
    
  _pmath_object_atomic_write(
    &sym_ptr->value,
    value);
}

PMATH_API void pmath_symbol_set_value(
  pmath_symbol_t symbol,
  pmath_t        value
) {
  struct _pmath_symbol_t *sym_ptr = (void *)PMATH_AS_PTR(symbol);
  
  if(!sym_ptr) {
    pmath_unref(value);
    return;
  }
  
  sym_ptr->inherited.inherited.last_change = _pmath_timer_get_next();
  if(pmath_atomic_fetch_set(&sym_ptr->current_dynamic_id, 0) != 0)
    _pmath_dynamic_update(symbol);
    
  if(sym_ptr->attributes & PMATH_SYMBOL_ATTRIBUTE_THREADLOCAL) {
    pmath_unref(pmath_thread_local_save(symbol, value));
    return;
  }
  
  _pmath_object_atomic_write(&sym_ptr->value, value);
}

/*----------------------------------------------------------------------------*/

PMATH_API void pmath_symbol_synchronized(
  pmath_symbol_t     symbol,
  pmath_callback_t   callback,
  void              *data
) {
  if(!pmath_is_null(symbol)) {
    // ensure that the symbol wont be freed during synchronization:
    symbol = pmath_ref(symbol);
    pmath_thread_call_locked(
      &((struct _pmath_symbol_t *)PMATH_AS_PTR(symbol))->lock,
      callback,
      data);
    pmath_unref(symbol);
  }
}

/*----------------------------------------------------------------------------*/

PMATH_API void pmath_symbol_update(pmath_symbol_t symbol) {
  struct _pmath_symbol_t *sym_ptr = (struct _pmath_symbol_t *)PMATH_AS_PTR(symbol);
  
  if(PMATH_UNLIKELY(!sym_ptr))
    return;
    
  assert(pmath_is_symbol(symbol));
  
  sym_ptr->inherited.inherited.last_change = _pmath_timer_get_next();
  
  if(pmath_atomic_fetch_set(&sym_ptr->current_dynamic_id, 0) != 0)
    _pmath_dynamic_update(symbol);
}

PMATH_PRIVATE
void _pmath_symbol_track_dynamic(
  pmath_symbol_t symbol, // wont be freed
  intptr_t       id
) {
  struct _pmath_symbol_t *sym_ptr = (struct _pmath_symbol_t *)PMATH_AS_PTR(symbol);
  
  if(PMATH_UNLIKELY(!sym_ptr))
    return;
    
  assert(pmath_is_symbol(symbol));
  
  if(pmath_atomic_read_aquire(&sym_ptr->current_dynamic_id) == id)
    return;
    
  if(id && pmath_atomic_read_aquire(&sym_ptr->ignore_dynamic_id) == id)
    return;
    
  pmath_atomic_write_release(&sym_ptr->current_dynamic_id, id);
  _pmath_dynamic_bind(symbol, id);
}

PMATH_PRIVATE
void _pmath_symbol_lost_dynamic_tracker(pmath_symbol_t symbol, intptr_t oldid, intptr_t other_tracker_id) {
  struct _pmath_symbol_t *sym_ptr = (struct _pmath_symbol_t *)PMATH_AS_PTR(symbol);
  
  if(PMATH_UNLIKELY(!sym_ptr))
    return;

  assert(pmath_is_symbol(symbol));

  if(pmath_atomic_read_aquire(&sym_ptr->current_dynamic_id) == oldid) {
    pmath_atomic_write_release(&sym_ptr->current_dynamic_id, other_tracker_id);
    return;
  }
}

/*----------------------------------------------------------------------------*/

PMATH_API
void pmath_symbol_remove(pmath_symbol_t symbol) {
  if(!pmath_is_null(symbol)) {
    pmath_symbol_attributes_t attr;
    
    assert(pmath_is_symbol(symbol));
    
    attr = pmath_symbol_get_attributes(symbol);
    if(attr & PMATH_SYMBOL_ATTRIBUTE_PROTECTED) {
      pmath_message(PMATH_SYMBOL_REMOVE, "rmptc", 1, symbol);
      return;
    }
    
    pmath_symbol_set_attributes(
      symbol,
      attr | PMATH_SYMBOL_ATTRIBUTE_TEMPORARY | PMATH_SYMBOL_ATTRIBUTE_REMOVED);
    _pmath_clear(symbol, TRUE);
    
    pmath_unref(symbol);
  }
}

/*----------------------------------------------------------------------------*/

//{ pMath object functions ...

static unsigned hash_symbol(pmath_t symbol) {
  return _pmath_hash_pointer(PMATH_AS_PTR(symbol));
}

static void destroy_symbol(pmath_t s) {
  struct _pmath_symbol_t *symbol = (void *)PMATH_AS_PTR(s);
  
  if( (symbol->attributes & PMATH_SYMBOL_ATTRIBUTE_TEMPORARY) &&
      _pmath_is_running())
  {
    if(pmath_atomic_compare_and_set(&symbol->inherited.inherited.inherited.refcount, 0, 1)) {
      void *removed_entry;
      
#ifdef PMATH_DEBUG_LOG
      if(pmath_atomic_read_aquire(&_pmath_debug_current_gc_symbol) == (intptr_t)symbol){
        pmath_debug_print_object("\n[symbol still used by GC: ", s , "]\n");
      }
      
      if(_pmath_have_code(s, PMATH_CODE_USAGE_DOWNCALL)) {
        pmath_debug_print_object("\n[symbol still used in DOWNCALL: ", s , "]\n");
      }
      
      if(_pmath_have_code(s, PMATH_CODE_USAGE_SUBCALL)) {
        pmath_debug_print_object("\n[symbol still used in SUBCALL: ", s , "]\n");
      }
      
      if(_pmath_have_code(s, PMATH_CODE_USAGE_UPCALL)) {
        pmath_debug_print_object("\n[symbol still used in UPCALL: ", s , "]\n");
      }
#endif
      
      assert(pmath_is_string(symbol->name));
      assert(pmath_refcount(symbol->name) >= 1);
      
      {
        PMATH_DEBUG_TIMING( {
          pmath_atomic_lock(&global_symbol_table_lock);
          {
            removed_entry = pmath_ht_remove(
              global_symbol_table,
              (void *)&symbol->name);
            if(removed_entry)
              post_remove(removed_entry);
          }
          pmath_atomic_unlock(&global_symbol_table_lock);
        });
        
#ifdef PMATH_DEBUG_LOG
        if( removed_entry != symbol ||
            pmath_atomic_read_aquire(&symbol->inherited.inherited.inherited.refcount) != 1)
        {
          pmath_symbol_t entry;
          unsigned int count, cap, i;
          
          PMATH_DEBUG_TIMING( {
            pmath_atomic_lock(&global_symbol_table_lock);
            {
              count = pmath_ht_count(global_symbol_table);
              cap   = pmath_ht_capacity(global_symbol_table);
            }
            pmath_atomic_unlock(&global_symbol_table_lock);
          });
          
          pmath_debug_print_object("Hashtable corrupted?: ", PMATH_FROM_PTR(symbol), "");
          pmath_debug_print(" [hash= %u]\n", pmath_hash(PMATH_FROM_PTR(symbol)));
          
          pmath_debug_print_object("removed: ", PMATH_FROM_PTR(removed_entry), "");
          pmath_debug_print(" [hash= %u]\n", pmath_hash(PMATH_FROM_PTR(removed_entry)));
          
          pmath_debug_print(">-------------------------------\n");
          pmath_debug_print("\tcount=%u,cap=%u\n", count, cap);
          
          for(i = 0; i < cap; ++i) {
            pmath_debug_print("\t%u:\t", i);
            
            PMATH_DEBUG_TIMING( {
              pmath_atomic_lock(&global_symbol_table_lock);
              {
                entry = pmath_ref(PMATH_FROM_PTR(pmath_ht_entry(global_symbol_table, i)));
              }
              pmath_atomic_unlock(&global_symbol_table_lock);
            });
            
            if(pmath_is_null(entry)) {
              pmath_debug_print("NULL\n");
            }
            else {
              pmath_debug_print("[hash= %u]\t", pmath_hash(entry));
              pmath_debug_print_object("", entry, "\n");
              pmath_unref(entry);
            }
          }
          pmath_debug_print("<-------------------------------\n");
          
          PMATH_DEBUG_TIMING( {
            pmath_atomic_lock(&global_symbol_table_lock);
            {
              pre_insert(removed_entry);
              removed_entry = pmath_ht_insert(global_symbol_table, removed_entry);
              if(removed_entry)
                post_remove(removed_entry);
            }
            pmath_atomic_unlock(&global_symbol_table_lock);
          });
        }
#endif
      }
      
      if(removed_entry) {
        symbol_entry_destructor(removed_entry);
      }
    }
  }
}

static void write_symbol(struct pmath_write_ex_t *info, pmath_t symbol) {
  pmath_string_t name;
  const uint16_t *str;
  int len;
  
  if(_pmath_write_user_format(info, symbol))
    return;
  
  name = pmath_symbol_name(symbol);
  len = pmath_string_length(name);
  str = pmath_string_buffer(&name);
  
  if(pmath_symbol_get_attributes(symbol) & PMATH_SYMBOL_ATTRIBUTE_REMOVED) {
    _pmath_write_cstr("Removed(", info->write, info->user);
    _pmath_write_impl(info, name);
    _pmath_write_cstr(")", info->write, info->user);
    pmath_unref(name);
    return;
  }
  
  if((info->options & PMATH_WRITE_OPTIONS_FULLNAME) == 0) {
    int i = len;
    while(i > 0 && str[i - 1] != '`')
      --i;
      
    if(i > 0) {
      pmath_symbol_t found = PMATH_UNDEFINED;
      if(info->options & PMATH_WRITE_OPTIONS_FULLNAME_NONSYSTEM) {
        if( i == 7 &&
            str[0] == 'S' &&
            str[1] == 'y' &&
            str[2] == 's' &&
            str[3] == 't' &&
            str[4] == 'e' &&
            str[5] == 'm' &&
            str[6] == '`')
        {
          found = pmath_symbol_find(
                    pmath_string_part(pmath_ref(name), i, -1),
                    FALSE);
        }
      }
      else {
        found = pmath_symbol_find(
                  pmath_string_part(pmath_ref(name), i, -1),
                  FALSE);
      }
      
      pmath_unref(found);
      if(pmath_same(found, symbol)) {
        info->write(info->user, str + i, len - i);
        pmath_unref(name);
        return;
      }
    }
  }
  
  info->write(info->user, str, len);
  pmath_unref(name);
}

//}

PMATH_PRIVATE void _pmath_symbols_memory_panic(void) {
  destroy_all_unused_symbols();
}

PMATH_PRIVATE pmath_bool_t _pmath_symbols_init(void) {
  assert(pmath_atomic_read_aquire(&global_symbol_table_lock) == 0);
  
  memset(&unused_symbols, 0, sizeof(unused_symbols));
  
  global_symbol_table = pmath_ht_create(
                          &symbol_table_class,
                          PMATH_BUILTIN_SYMBOL_COUNT);
  if(!global_symbol_table)
    goto FAIL_GLOBAL_SYMBOL_TABLE;
    
  _pmath_init_special_type(
    PMATH_TYPE_SHIFT_SYMBOL,
    _pmath_compare_exprsym,
    hash_symbol,
    destroy_symbol,
    NULL,
    write_symbol);
    
  return TRUE;
  
FAIL_GLOBAL_SYMBOL_TABLE:
  return FALSE;
}

PMATH_PRIVATE void _pmath_symbols_almost_done(void) {
  unsigned int i, cap;
  
  cap = pmath_ht_capacity(global_symbol_table);
  for(i = 0; i < cap; ++i) {
    struct _pmath_symbol_t *symbol = pmath_ht_entry(global_symbol_table, i);
    
    if(symbol) {
      struct _pmath_symbol_rules_t *rules;
      _pmath_object_atomic_write(&symbol->value, PMATH_UNDEFINED);
      
      rules = (void *)pmath_atomic_fetch_set(&symbol->rules, 0);
      if(rules) {
        _pmath_symbol_rules_done(rules);
        
        pmath_mem_free(rules);
      }
    }
  }
}

PMATH_PRIVATE void _pmath_symbols_done(void) {

  _pmath_symbols_almost_done();
  
#ifdef PMATH_DEBUG_LOG
  {
    unsigned int i, cap;
    
    cap = pmath_ht_capacity(global_symbol_table);
    for(i = 0; i < cap; ++i) {
      struct _pmath_symbol_t *symbol = pmath_ht_entry(global_symbol_table, i);
      intptr_t refcount = 0;
      
      if(symbol)
        refcount = pmath_atomic_read_aquire(&symbol->inherited.inherited.inherited.refcount);
        
      if(refcount != 0) {
        if(pmath_ht_search(global_symbol_table, (void *)&symbol->name) != NULL) {
          pmath_debug_print_object("Symbol '", PMATH_FROM_PTR(symbol), "'");
          pmath_debug_print(" (%p) still has %"PRIuPTR" reference(s)\n",
                            symbol,
                            refcount);
        }
        else {
          pmath_debug_print_object("Hashtable corrupted: lost symbol '", PMATH_FROM_PTR(symbol), "'");
          pmath_debug_print(" (%p) still has %"PRIuPTR" reference(s)\n",
                            symbol,
                            refcount);
        }
      }
    }
  }
#endif
  
  pmath_ht_destroy(global_symbol_table);
  destroy_all_unused_symbols();
}
