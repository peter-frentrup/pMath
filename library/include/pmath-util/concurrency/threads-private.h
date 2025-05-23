#ifndef __PMATH_UTIL__CONCURRENCY__THREADS_PRIVATE_H__
#define __PMATH_UTIL__CONCURRENCY__THREADS_PRIVATE_H__

#ifndef BUILDING_PMATH
#error This header file is not part of the public pMath API
#endif

#include <pmath-util/concurrency/atomic-private.h>
#include <pmath-util/concurrency/threadlocks.h>
#include <pmath-util/concurrency/threads.h>
#include <pmath-util/hash/hashtables.h>


struct _pmath_stack_info_t {
  struct _pmath_stack_info_t  *next;
  pmath_t                      head;
  pmath_t                      debug_metadata;
};

struct _pmath_gather_emit_info_t {
  struct _pmath_gather_emit_info_t  *next;
  pmath_t                            value;
};

struct _pmath_gather_info_t {
  struct _pmath_gather_info_t *next;    // thread-local write, readable from child-threads
  pmath_t                      pattern; // dito.
  
  pmath_atomic_t value_count;
  
  // struct _pmath_gather_emit_info_t
  pmath_atomic_t emitted_values;
};

struct _pmath_abortable_message_t {
  pmath_locked_t _value; // _pmath_object_atomic_[read|write]
  
  // thread-local write, thread-child-local read
  pmath_t        next; // pmath_custom_t -> _pmath_abortable_message_t
  int            depth; // = next -> depth + 1
  
  // thread-child-local read/write with _pmath_object_atomic_[read|write]
  pmath_locked_t _pending_abort_request; // dito
};

PMATH_PRIVATE extern pmath_atomic_t _pmath_abort_timer;
PMATH_PRIVATE extern pmath_atomic_t _pmath_abort_reasons;

enum {
  BOXFORM_STANDARD,
  BOXFORM_STANDARDEXPONENT,
  BOXFORM_OUTPUT,
  BOXFORM_OUTPUTEXPONENT,
  BOXFORM_INPUT
};

enum {
  PACKED_ARRAY_FORM_UNCOMPRESSED = 0,
  PACKED_ARRAY_FORM_SUMMARY,
  PACKED_ARRAY_FORM_COMPRESSED,
};

#define PMATH_BASE_FLAGS_BASE_MASK  0x3F
#define PMATH_BASE_FLAGS_AUTOMATIC  0x80

struct _pmath_thread_t {
  pmath_thread_t      parent;
  pmath_threadlock_t  waiting_lock;
  intptr_t            ignore_older_aborts;
  
  // do not access from outside pmath_[gather_begin|gather_end|emit]:
  int                           gather_failed;
  struct _pmath_gather_info_t  *gather_info;
  
  // this: read/write, children: not used (might be write), other: none
  pmath_hashtable_t  parser_cache;            // class: pmath_ht_obj_class
  
  // this: read/write, children: read, other: none
  pmath_hashtable_t  local_values; // class: pmath_ht_obj_class
  pmath_hashtable_t  local_rules;  // class: _pmath_symbol_rules_ht_class
  
  // this: read/write, children: read, other: none
  struct _pmath_stack_info_t  *stack_info;
  int                          evaldepth;
  
  // do not access from outside _pmath_thread_[throw|catch]():
  pmath_t exception;
  
  pmath_t message_queue;      // the pmath_messages_t (message queue)
  pmath_t abortable_messages; // pmath_custom_t -> _pmath_abortable_message_t
  
  double max_extra_precision; // in bits
  double min_precision;       // in bits
  double max_precision;       // in bits
  
  intptr_t current_dynamic_id;
  
  uint16_t security_level;   // actually a pmath_security_level_t
  uint8_t critical_messages; // TRUE / FALSE
  uint8_t is_daemon;         // TRUE / FALSE
  uint8_t boxform;           // BOXFORM_XXX
  uint8_t longform;          // TRUE / FALSE
  uint8_t base_flags;        // 2..36
  uint8_t packed_array_form; // PACKED_ARRAY_FORM_xxx
};

PMATH_PRIVATE void _pmath_destroy_abortable_message(void *p);
PMATH_PRIVATE void _pmath_abort_message(pmath_t abortable); // abortable will be freed

PMATH_PRIVATE
void _pmath_thread_throw(
  pmath_thread_t thread, // not PMATH_NULL
  pmath_t exception);    // exception will be freed

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

PMATH_PRIVATE void _pmath_thread_destructed(void);

PMATH_PRIVATE pmath_bool_t _pmath_threads_init(void);
PMATH_PRIVATE void         _pmath_threads_done(void);

#endif /* __PMATH_UTIL__CONCURRENCY__THREADS_PRIVATE_H__ */
