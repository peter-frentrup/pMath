#include <pmath-util/security-private.h>
#include <pmath-builtins/all-symbols-private.h>

#include <pmath-core/expressions.h>

#include <pmath-util/concurrency/threads-private.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/hash/incremental-hash-private.h>
#include <pmath-util/helpers.h>
#include <pmath-util/memory.h>


extern pmath_symbol_t pmath_System_HoldComplete;
extern pmath_symbol_t pmath_System_SecurityException;

static pmath_atomic_t _pmath_doorman_table;

#define LOCK_DOORMAN_TABLE()         (pmath_hashtable_t)_pmath_atomic_lock_ptr(&_pmath_doorman_table)
#define UNLOCK_DOORMAN_TABLE(TABLE)  _pmath_atomic_unlock_ptr(&_pmath_doorman_table, (TABLE))


static struct { pmath_security_level_t level; const char *name; } named_security_levels[] = {
  { PMATH_SECURITY_LEVEL_NOTHING_ALLOWED,             "NothingAllowed" },
  { PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED,  "PureDeterministicAllowed" },
  { PMATH_SECURITY_LEVEL_NON_DESTRUCTIVE_ALLOWED,     "NonDestructiveAllowed" },
  { PMATH_SECURITY_LEVEL_EVERYTHING_ALLOWED,          "EverythingAllowed" },
};

struct _pmath_doorman_entry_t {
  pmath_builtin_func_t           key;
  pmath_security_doorman_func_t  doorman;
  pmath_security_level_t         required_level;
};


static void destroy_doorman_entry(void *e) {
  struct _pmath_doorman_entry_t *entry = e;
  pmath_mem_free(entry);
}

static unsigned int hash_doorman_key(void *k) {
  pmath_builtin_func_t key = k;
  return _pmath_incremental_hash(&key, sizeof(pmath_builtin_func_t), 0);
}

static unsigned int hash_doorman_entry(void *e) {
  struct _pmath_doorman_entry_t *entry = e;
  return hash_doorman_key(entry->key);
}

static pmath_bool_t doorman_entry_keys_equal(
  void *e1,
  void *e2
) {
  struct _pmath_doorman_entry_t *entry1 = e1;
  struct _pmath_doorman_entry_t *entry2 = e2;
  return entry1->key == entry2->key;
}

static pmath_bool_t doorman_entry_equals_key(
  void *e,
  void *k
) {
  struct _pmath_doorman_entry_t *entry = e;
  pmath_builtin_func_t           key   = k;
  return entry->key == key;
}

static const pmath_ht_class_t doorman_table_class = {
  destroy_doorman_entry,
  hash_doorman_entry,
  doorman_entry_keys_equal,
  hash_doorman_key,
  doorman_entry_equals_key
};


PMATH_API
pmath_bool_t pmath_security_check(pmath_security_level_t required_level, pmath_t message_arg) {
  pmath_thread_t me = pmath_thread_get_current();
  pmath_t exception;
  
  if(!me)
    return FALSE;
  
  if(PMATH_SECURITY_REQUIREMENT_MATCHES_LEVEL(required_level, me->security_level))
    return TRUE;
  
  exception = pmath_catch();
  if(pmath_same(exception, PMATH_UNDEFINED)) {
    if(pmath_is_expr_of(message_arg, pmath_System_HoldComplete))
      message_arg = pmath_ref(message_arg);
    else
      message_arg = pmath_expr_new_extended(
                      pmath_ref(pmath_System_HoldComplete), 1,
                      pmath_ref(message_arg));
    
    // TODO: include (parts of) the Stack()
    exception = pmath_expr_new_extended(
                  pmath_ref(pmath_System_SecurityException), 2,
                  _pmath_security_level_to_expr(required_level),
                  message_arg);
  }
  
  pmath_throw(exception);
  return FALSE;
}

PMATH_API 
pmath_t pmath_evaluate_secured(pmath_t expr, pmath_security_level_t max_allowed_level) {
  pmath_thread_t me = pmath_thread_get_current();
  pmath_security_level_t old_level = me->security_level;
  
  if(old_level > max_allowed_level)
    me->security_level = max_allowed_level;
  
  expr = pmath_evaluate(expr);
  
  me->security_level = old_level;
  return expr;
}

PMATH_API
pmath_bool_t pmath_security_register_doorman(
  void                          *func, 
  pmath_security_level_t         min_level, 
  pmath_security_doorman_func_t  certifier
) {
  struct _pmath_doorman_entry_t *entry;
  pmath_hashtable_t              table;
  
  if(!func)
    return FALSE;
  
  if(min_level != PMATH_SECURITY_LEVEL_EVERYTHING_ALLOWED) {
    entry = pmath_mem_alloc(sizeof(struct _pmath_doorman_entry_t));
    if(!entry)
      return FALSE;
      
    entry->key            = func;
    entry->doorman        = certifier;
    entry->required_level = min_level;
  }
  else
    entry = NULL;
    
  table = LOCK_DOORMAN_TABLE();
  
  if(entry)
    entry = pmath_ht_insert(table, entry);
  else
    entry = pmath_ht_remove(table, func);
    
  UNLOCK_DOORMAN_TABLE(table);
  
  if(entry)
    destroy_doorman_entry(entry);
    
  return TRUE;
}

PMATH_PRIVATE
pmath_bool_t _pmath_security_check_builtin(
  void                   *func, 
  pmath_expr_t            expr, 
  pmath_symbol_t          src_sym,
  pmath_symbol_t          kind,
  pmath_security_level_t  current_level
) {
  pmath_hashtable_t table;
  pmath_security_level_t        required_level = PMATH_SECURITY_LEVEL_EVERYTHING_ALLOWED;
  pmath_security_doorman_func_t doorman = NULL;
  
  if(PMATH_SECURITY_REQUIREMENT_MATCHES_LEVEL(PMATH_SECURITY_LEVEL_EVERYTHING_ALLOWED, current_level))
    return TRUE;
  
  table = LOCK_DOORMAN_TABLE();
  {
    struct _pmath_doorman_entry_t *entry = pmath_ht_search(table, func);
    if(entry) {
      required_level = entry->required_level;
      doorman        = entry->doorman;
    }
  }
  UNLOCK_DOORMAN_TABLE(table);
  
  if(!PMATH_SECURITY_REQUIREMENT_MATCHES_LEVEL(required_level, current_level)) {
    pmath_expr_t msg_arg = pmath_expr_new_extended(
                             pmath_ref(pmath_System_HoldComplete), 3,
                             pmath_ref(src_sym),
                             pmath_ref(kind),
                             pmath_ref(expr));
    // throws a SecurityException and returns FALSE:
    pmath_bool_t result = pmath_security_check(required_level, msg_arg);
    pmath_unref(msg_arg);
    return result;
  }
  
  if(doorman) 
    return doorman(func, expr, current_level);
  
  return TRUE;
}

PMATH_PRIVATE
pmath_t _pmath_security_level_to_expr(pmath_security_level_t level) {
  for(size_t i = 0; i < sizeof(named_security_levels)/sizeof(named_security_levels[0]); ++i) {
    if(level == named_security_levels[i].level)
      return PMATH_C_STRING(named_security_levels[i].name);
  }
  
  return PMATH_FROM_INT32(level);
}

// expr wont be freed
PMATH_PRIVATE
pmath_bool_t _pmath_security_level_from_expr(pmath_security_level_t *level, pmath_expr_t expr) {
  if(pmath_is_string(expr)) {
    for(size_t i = 0; i < sizeof(named_security_levels)/sizeof(named_security_levels[0]); ++i) {
      if(pmath_string_equals_latin1(expr, named_security_levels[i].name)) {
        *level = named_security_levels[i].level;
        return TRUE;
      }
    }
  }
  
  *level = PMATH_SECURITY_LEVEL_NOTHING_ALLOWED;
  return FALSE;
}

PMATH_PRIVATE
pmath_bool_t _pmath_security_init(void) {
  pmath_hashtable_t doorman_table;
  
  memset(&_pmath_doorman_table, 0, sizeof(_pmath_doorman_table));
  
  doorman_table = pmath_ht_create(&doorman_table_class, 0);
  if(!doorman_table)
    goto FAIL_DOORMAN_TABLE;
    
  pmath_atomic_write_release(&_pmath_doorman_table, (intptr_t)doorman_table);
  
  return TRUE;
FAIL_DOORMAN_TABLE:
  return FALSE;
}

PMATH_PRIVATE
void _pmath_security_done() {
  pmath_ht_destroy((pmath_hashtable_t)pmath_atomic_read_aquire(&_pmath_doorman_table));
}
