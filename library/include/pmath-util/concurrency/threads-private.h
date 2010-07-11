#ifndef __PMATH_UTIL__CONCURRENCY__THREADS_PRIVATE_H__
#define __PMATH_UTIL__CONCURRENCY__THREADS_PRIVATE_H__

#ifndef BUILDING_PMATH
  #error This header file is not part of the public pMath API
#endif

struct _pmath_stack_info_t{
  struct _pmath_stack_info_t  *next;
  pmath_t               value;
};

struct _pmath_gather_info_t{
  struct _pmath_gather_info_t *next; // thread-local write, readable from child-threads
  pmath_t pattern;          // dito.

  PMATH_DECLARE_ATOMIC(value_count);
  union{
    struct _pmath_stack_info_t *ptr; // child threads may only push items => CAS2 operation not needed
    intptr_t                    intptr; // "Dereferencing type-punned pointers ..."
  } emitted_values;
};

PMATH_PRIVATE extern PMATH_DECLARE_ATOMIC(_pmath_abort_timer);
PMATH_PRIVATE extern PMATH_DECLARE_ATOMIC(_pmath_abort_reasons);

enum{
  BOXFORM_STANDARD,
  BOXFORM_STANDARDEXPONENT,
  BOXFORM_OUTPUT,
  BOXFORM_OUTPUTEXPONENT,
  BOXFORM_INPUT
};

struct _pmath_thread_t{
  pmath_thread_t      parent;
  pmath_threadlock_t  waiting_lock;
  intptr_t            ignore_older_aborts;
  
  // do not access from outside pmath_[gather_begin|gather_end|emit]:
  int                           gather_failed;
  struct _pmath_gather_info_t  *gather_info; 
  
  // this: read/write, children: read, other: none
  pmath_hashtable_t  local_values; // class: pmath_ht_obj_class
  pmath_hashtable_t  local_rules;  // class: _pmath_symbol_rules_ht_class
  
  // this: read/write, children: read, other: none
  struct _pmath_stack_info_t  *stack_info; 
  int                          evaldepth;
  
  // do not access from outside _pmath_thread_[throw|catch]():
  pmath_t exception; 
  
  pmath_t message_queue; // the pmath_messages_t (message queue)
  
  intptr_t current_dynamic_id;
  
  uint8_t boxform;           // BOXFORM_XXX
  uint8_t critical_messages; // TRUE / FALSE
  uint8_t longform;          // TRUE / FALSE
  uint8_t is_daemon;
};

PMATH_PRIVATE 
void _pmath_thread_throw(
  pmath_thread_t thread,     // not NULL
  pmath_t exception); // exception will be freed

PMATH_PRIVATE 
PMATH_ATTRIBUTE_USE_RESULT
pmath_t _pmath_thread_catch(
  pmath_thread_t thread);  // must be current thread

PMATH_PRIVATE void _pmath_thread_set_current(pmath_thread_t thread);

PMATH_PRIVATE 
PMATH_ATTRIBUTE_USE_RESULT
pmath_t _pmath_thread_local_load_with(
  pmath_t key,     // wont be freed
  pmath_thread_t thread); // must be current thread or parent of it

PMATH_PRIVATE 
PMATH_ATTRIBUTE_USE_RESULT
pmath_t _pmath_thread_local_save_with(
  pmath_t key,     // wont be freed
  pmath_t value,   // will be freed
  pmath_thread_t thread); // must be current thread or parent of it

PMATH_PRIVATE pmath_thread_t _pmath_thread_new(pmath_thread_t parent);
PMATH_PRIVATE void           _pmath_thread_clean(pmath_bool_t final);
PMATH_PRIVATE void           _pmath_thread_free(pmath_thread_t thread);

PMATH_PRIVATE pmath_bool_t _pmath_threads_init(void);
PMATH_PRIVATE void            _pmath_threads_done(void);

/* seconds since January 1, 1970 (UTC)
 */
PMATH_PRIVATE double _pmath_tickcount(void);

#endif /* __PMATH_UTIL__CONCURRENCY__THREADS_PRIVATE_H__ */
