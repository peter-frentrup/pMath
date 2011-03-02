#include <pmath-util/concurrency/threads-private.h>

#include <pmath-core/symbols-private.h>

#include <pmath-util/concurrency/atomic-private.h>
#include <pmath-util/concurrency/threadmsg-private.h>
#include <pmath-util/debug.h>
#include <pmath-util/hashtables-private.h>
#include <pmath-util/memory.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/control/definitions-private.h>

#include <pmath-private.h>


#if PMATH_USE_PTHREAD

  #include <errno.h>
  #include <pthread.h>
  
#elif PMATH_USE_WINDOWS_THREADS

  #define NOGDI
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
  
#else

  #error Either PThread or Windows Threads must be used
  
#endif

#if PMATH_USE_PTHREAD
  static pthread_key_t threadkey;
#elif PMATH_USE_WINDOWS_THREADS
  static DWORD threadkey;
#endif

PMATH_PRIVATE PMATH_DECLARE_ATOMIC(_pmath_abort_reasons) = 0;

static PMATH_DECLARE_ATOMIC(aborting) = FALSE;

/*============================================================================*/

PMATH_API pmath_thread_t pmath_thread_get_current(void){
  pmath_thread_t thread;
  
  #if PMATH_USE_PTHREAD
    thread = (pmath_thread_t)pthread_getspecific(threadkey);
  #elif PMATH_USE_WINDOWS_THREADS
    thread = (pmath_thread_t)TlsGetValue(threadkey);
  #endif

//  if(!thread)
//    pmath_debug_print("thread not initialized\n");

  return thread;
}

PMATH_API pmath_thread_t pmath_thread_get_parent(pmath_thread_t thread){
  if(!thread)
    return PMATH_NULL;
  return thread->parent;
}

PMATH_API pmath_bool_t pmath_thread_is_parent(pmath_thread_t parent, pmath_thread_t child){
  if(!parent)
    return TRUE;

  while(child && child != parent)
    child = child->parent;

  return child == parent;
}

PMATH_PRIVATE 
pmath_t _pmath_thread_local_save_with(
  pmath_t key,
  pmath_t value,
  pmath_thread_t thread
){
  struct _pmath_object_entry_t *entry;

  if(!thread){
    pmath_unref(value);
    return PMATH_UNDEFINED;
  }

  if(!thread->local_values){
    if(pmath_same(value, PMATH_UNDEFINED))
      return PMATH_UNDEFINED;

    thread->local_values = pmath_ht_create(&pmath_ht_obj_class, 1);
  }

  if(pmath_same(value, PMATH_UNDEFINED)){
    struct _pmath_object_entry_t *entry;
    
    entry = pmath_ht_remove(thread->local_values, key);
    if(entry){
      pmath_t result = pmath_ref(entry->value);
      pmath_ht_obj_class.entry_destructor(entry);
      return result;
    }
    return PMATH_UNDEFINED;
  }

  entry = pmath_ht_search(thread->local_values, key);

  if(entry){
    pmath_t result = entry->value;
    entry->value = value;
    return result;
  }

  entry = pmath_mem_alloc(sizeof(struct _pmath_object_entry_t));

  if(!entry){
    pmath_unref(value);
    return PMATH_UNDEFINED;
  }

  entry->key = pmath_ref(key);
  entry->value = value;

  entry = pmath_ht_insert(thread->local_values, entry);

  assert(entry == PMATH_NULL);

  return PMATH_UNDEFINED;
}

PMATH_PRIVATE 
pmath_t _pmath_thread_local_load_with(
  pmath_t key, 
  pmath_thread_t thread
){
  while(thread){
    struct _pmath_object_entry_t *entry;
    
    entry = pmath_ht_search(thread->local_values, key);
    if(entry)
      return pmath_ref(entry->value);

    thread = pmath_thread_get_parent(thread);
  }

  if(pmath_is_symbol(key))
    return _pmath_symbol_get_global_value(key);

  return PMATH_UNDEFINED;
}

PMATH_API pmath_t pmath_thread_local_save(
  pmath_t key,
  pmath_t value
){
  return _pmath_thread_local_save_with(key, value, pmath_thread_get_current());
}

PMATH_API pmath_t pmath_thread_local_load(
  pmath_t key
){
  return _pmath_thread_local_load_with(key, pmath_thread_get_current());
}

/*----------------------------------------------------------------------------*/

PMATH_PRIVATE void _pmath_destroy_abortable_message(void *p){
  struct _pmath_abortable_message_t *data = (struct _pmath_abortable_message_t*)p;
  
  if(data){
    _pmath_object_atomic_write(&data->_value, PMATH_ABORT_EXCEPTION);
    pmath_unref(data->next);
    _pmath_object_atomic_write(&data->_pending_abort_request, PMATH_NULL);
    pmath_mem_free(data);
  }
}

PMATH_PRIVATE void _pmath_abort_message(pmath_t abortable){
  pmath_thread_t thread = pmath_thread_get_current();
  pmath_t current, pending;
  struct _pmath_abortable_message_t *current_data, *pending_data;
  
  if(!thread
  || !pmath_is_custom(abortable)
  || !pmath_custom_has_destructor(abortable, _pmath_destroy_abortable_message)){
    pmath_unref(abortable);
    return;
  }
  
  current = thread->abortable_messages;
  if(current == abortable){
    pmath_debug_print("[aborting %p]\n", abortable);
    _pmath_thread_throw(thread, abortable);
    return;
  }
  
  while(current){
    current_data = pmath_custom_get_data(current);
    
    pending = _pmath_object_atomic_read(&current_data->_pending_abort_request);
    pmath_unref(pending);
    if(pending == abortable){
      pmath_debug_print("[multible aborts %p]\n", abortable);
      break;
    }
    
    current = current_data->next;
    
    if(current == abortable){
      pmath_t next;
      pmath_t end = current;
      int end_depth = current_data->depth;
      
      current = thread->abortable_messages;
      while(current && current != end){
        current_data = pmath_custom_get_data(current);
        next = current_data->next;
        
        pending = _pmath_object_atomic_read(&current_data->_pending_abort_request);
        if(pending){
          if(pending == abortable){
            pmath_debug_print("[multible upper aborts %p]\n", abortable);
            pmath_unref(abortable);
            pmath_unref(pending);
            return;
          }
          
          pending_data = pmath_custom_get_data(pending);
          if(pending_data->depth < end_depth){
            pmath_debug_print("[switch abort requests %p <-> %p]\n", pending, abortable);
            _pmath_object_atomic_write(&current_data->_pending_abort_request, abortable);
            abortable = pending;
            end       = pending;
            end_depth = pending_data->depth;
          }
          else
            pmath_unref(pending);
        }
        else{
          pmath_debug_print("[request abort %p]\n", abortable);
          _pmath_object_atomic_write(&current_data->_pending_abort_request, abortable);
          return;
        }
        
        current = next;
      }
    }
  }
  
  pmath_debug_print("[abrotable not found %p]\n", abortable);
  pmath_unref(abortable);
}

/*----------------------------------------------------------------------------*/

//{ exception handling ...

static pmath_threadlock_t exception_handler_lock = PMATH_NULL;

struct _change_exception_data_t{
  pmath_thread_t thread;
  pmath_t exception;
  unsigned       prefer_new: 1;
};

static void change_exception(struct _change_exception_data_t *data){
  pmath_t old = data->thread->exception;
  
  if(pmath_same(old, PMATH_UNDEFINED)){
    if(!pmath_same(data->exception, PMATH_UNDEFINED)){
      (void)pmath_atomic_fetch_add(&_pmath_abort_reasons, 1);
      
      data->thread->exception = data->exception;
      data->exception = old;
      
      pmath_thread_wakeup(data->thread->message_queue);
    }
  }
  else if(data->prefer_new){
    if(pmath_same(data->exception, PMATH_UNDEFINED)){
      (void)pmath_atomic_fetch_add(&_pmath_abort_reasons, -1);
    }
    
    data->thread->exception = data->exception;
    
    if(!pmath_same(data->exception, PMATH_UNDEFINED))
      pmath_thread_wakeup(data->thread->message_queue);
      
    data->exception = old;
  }
}

PMATH_PRIVATE void _pmath_thread_throw(
  pmath_thread_t   thread,
  pmath_t exception
){
  struct _change_exception_data_t data;

  if(!thread){
    pmath_unref(exception);
    return;
  }

  data.thread = thread;
  data.exception = exception;
  data.prefer_new = 0;

  pmath_thread_call_locked(
    &exception_handler_lock,
    (pmath_callback_t)change_exception,
    &data);

  pmath_unref(data.exception);
}

PMATH_PRIVATE
pmath_t _pmath_thread_catch(
  pmath_thread_t thread
){
  struct _change_exception_data_t data;
  data.thread = thread;
  if(!thread)
    return PMATH_UNDEFINED;

  data.exception = PMATH_UNDEFINED;
  data.prefer_new = 1;

  pmath_thread_call_locked(
    &exception_handler_lock,
    (pmath_callback_t)change_exception,
    &data);

  return data.exception;
}

PMATH_API void pmath_throw(pmath_t exception){
  _pmath_thread_throw(pmath_thread_get_current(), exception);
}

PMATH_API pmath_t pmath_catch(void){
  return _pmath_thread_catch(pmath_thread_get_current());
}

//} ... exception handling

/*----------------------------------------------------------------------------*/

PMATH_PRIVATE PMATH_DECLARE_ATOMIC(_pmath_abort_timer) = 0;

static PMATH_DECLARE_ATOMIC(suspending_spin) = 0;
static PMATH_DECLARE_ATOMIC(suspending) = 0;
#if PMATH_USE_PTHREAD
  static volatile pthread_t  suspender;
#elif PMATH_USE_WINDOWS_THREADS
  static volatile DWORD      suspender;
#endif

static void do_suspend(void){
  pmath_atomic_lock(&suspending_spin);
  
  if(suspending){
    #if PMATH_USE_PTHREAD
      pthread_t  me = pthread_self();
    #elif PMATH_USE_WINDOWS_THREADS
      DWORD      me = GetCurrentThreadId();
    #endif
    
    #if PMATH_USE_PTHREAD
    if(!pthread_equal(suspender, me))
    #elif PMATH_USE_WINDOWS_THREADS
    if(suspender != me)
    #endif
    {
      pmath_atomic_unlock(&suspending_spin);
      
      while(suspending){ 
        pmath_atomic_loop_yield();
      }
      
      return;
    }
  }
  
  pmath_atomic_unlock(&suspending_spin);
}

PMATH_API pmath_bool_t pmath_aborting(void){
  if(_pmath_abort_reasons == 0)
    return FALSE;
  return pmath_thread_aborting(pmath_thread_get_current());
}

PMATH_API pmath_bool_t pmath_thread_aborting(pmath_thread_t thread){
  _pmath_msq_queue_handle_next(thread);
  
  if(_pmath_abort_reasons == 0)
    return FALSE;
  
  if(suspending)
    do_suspend();
  
  if(!thread)
    return TRUE;
    
  if(aborting 
  && (thread->ignore_older_aborts <= _pmath_abort_timer
   || !pmath_same(thread->exception, PMATH_UNDEFINED)))
  {
    return TRUE;
  }
  
  while(thread && thread->ignore_older_aborts <= _pmath_abort_timer){
    if(!pmath_same(thread->exception, PMATH_UNDEFINED))
      return TRUE;
    
    thread = thread->parent;
  }
    
  return FALSE;
}

PMATH_API void pmath_abort_please(void){
  if(!pmath_atomic_fetch_set(&aborting, 1)){
    (void)pmath_atomic_fetch_add(&_pmath_abort_reasons, 1);
  }
  _pmath_msq_queue_awake_all();
}

PMATH_API pmath_bool_t pmath_continue_after_abort(void){
  pmath_bool_t was_set = pmath_atomic_fetch_set(&aborting, 0) != 0;
  
  if(was_set){
    (void)pmath_atomic_fetch_add(&_pmath_abort_reasons, -1);
  }
  
  pmath_unref(pmath_catch());
  
  _pmath_clear(PMATH_SYMBOL_MESSAGECOUNT, FALSE);
  return was_set;
}

/*----------------------------------------------------------------------------*/

PMATH_PRIVATE void _pmath_thread_set_current(pmath_thread_t thread){
  #if PMATH_USE_PTHREAD
    pthread_setspecific(threadkey, thread);
  #elif PMATH_USE_WINDOWS_THREADS
    TlsSetValue(threadkey, thread);
  #endif
}

PMATH_API void pmath_suspend_all_please(void){
  pmath_atomic_lock(&suspending_spin);
  
  while(suspending){
    pmath_atomic_unlock(&suspending_spin);
    
    do_suspend();
    
    pmath_atomic_lock(&suspending_spin);
  }
  
  #if PMATH_USE_PTHREAD
    suspender = pthread_self();
  #elif PMATH_USE_WINDOWS_THREADS
    suspender = GetCurrentThreadId();
  #endif
  
  suspending = 1;
  (void)pmath_atomic_fetch_add(&_pmath_abort_reasons, 1);
  
  pmath_atomic_unlock(&suspending_spin);
}

PMATH_API void pmath_resume_all(void){
  pmath_atomic_lock(&suspending_spin);
  
  if(suspending)
    (void)pmath_atomic_fetch_add(&_pmath_abort_reasons, -1);
  
  suspending = 0;
    
  pmath_atomic_unlock(&suspending_spin);
}

/*============================================================================*/

PMATH_PRIVATE pmath_thread_t _pmath_thread_new(pmath_thread_t parent){
  pmath_thread_t thread = pmath_mem_alloc(sizeof(struct _pmath_thread_t));
    
  if(!thread)
    return PMATH_NULL;
  
  thread->parent                = parent;
  thread->waiting_lock          = PMATH_NULL;
  thread->ignore_older_aborts   = parent ? parent->ignore_older_aborts : 0;
  thread->gather_failed         = 0;
  thread->gather_info           = PMATH_NULL;
  thread->local_values          = PMATH_NULL;
  thread->local_rules           = PMATH_NULL;
  thread->stack_info            = PMATH_NULL;
  thread->evaldepth             = 0;
  thread->exception             = PMATH_UNDEFINED;
  thread->message_queue         = _pmath_msg_queue_create();
  thread->abortable_messages    = parent ? pmath_ref(parent->abortable_messages) : PMATH_NULL;
  thread->current_dynamic_id    = parent ? parent->current_dynamic_id            : 0;
  thread->boxform               = parent ? parent->boxform                       : BOXFORM_STANDARD;
  thread->critical_messages     = parent ? parent->critical_messages             : FALSE;
  thread->longform              = parent ? parent->longform                      : FALSE;
  thread->is_daemon             = FALSE;
  
  if(!thread->message_queue){
    _pmath_thread_free(thread);
    return PMATH_NULL;
  }
  
  return thread;
}

static void free_stack(struct _pmath_stack_info_t *s){
  while(s){
    struct _pmath_stack_info_t *n = s->next;
    
    pmath_unref(s->value);
    pmath_mem_free(s);
    
    s = n;
  }
}

PMATH_PRIVATE void _pmath_thread_clean(pmath_bool_t final){
  pmath_thread_t thread = pmath_thread_get_current();
  
  if(thread){
    if(!pmath_same(thread->exception, PMATH_UNDEFINED)){
      pmath_t exception = _pmath_thread_catch(thread);
      
      if(!pmath_same(exception, PMATH_UNDEFINED)
      && !pmath_same(exception, PMATH_ABORT_EXCEPTION)){
        if(thread->parent){
          _pmath_thread_throw(thread->parent, exception);
        }
        else if(_pmath_is_running()){
          pmath_message(PMATH_SYMBOL_THROW, "nocatch", 1, exception);
        }
      }
    }
  
    while(thread->gather_info){
      struct _pmath_gather_info_t *next_gather = thread->gather_info->next;
      
      pmath_unref(thread->gather_info->pattern);
      free_stack(thread->gather_info->emitted_values.ptr);
      pmath_mem_free(thread->gather_info);
      
      thread->gather_info = next_gather;
    }
    
    free_stack(thread->stack_info);
    thread->stack_info = PMATH_NULL;
    thread->evaldepth = 0;
    
    pmath_ht_destroy(thread->local_values);
    pmath_ht_destroy(thread->local_rules);
    thread->local_values = PMATH_NULL;
    thread->local_rules  = PMATH_NULL;
    
    pmath_unref(thread->abortable_messages);
    thread->abortable_messages = PMATH_NULL;
    
    if(final){
      _pmath_msg_queue_inform_death(thread->message_queue);
      pmath_unref(thread->message_queue);
      thread->message_queue = PMATH_NULL;
    }
  }
}

PMATH_PRIVATE void _pmath_thread_free(pmath_thread_t thread){
  if(!thread)
    return;
  
  pmath_ht_destroy(thread->local_values);
  pmath_ht_destroy(thread->local_rules);
    
  pmath_unref(thread->abortable_messages);
  thread->abortable_messages = PMATH_NULL;
  
  _pmath_msg_queue_inform_death(thread->message_queue);
  pmath_unref(thread->message_queue);
  thread->message_queue = PMATH_NULL;
  
  while(thread->gather_info){
    struct _pmath_gather_info_t *next_gather = thread->gather_info->next;
    
    pmath_unref(thread->gather_info->pattern);
    free_stack(thread->gather_info->emitted_values.ptr);
    pmath_mem_free(thread->gather_info);
    
    
    thread->gather_info = next_gather;
  }
  
  if(!pmath_same(thread->exception, PMATH_UNDEFINED)){
    (void)pmath_atomic_fetch_add(&_pmath_abort_reasons, -1);
  }
  
  free_stack(thread->stack_info);
//  pmath_unref(thread->messenger);
  pmath_mem_free(thread);
}

/*============================================================================*/

PMATH_PRIVATE pmath_bool_t _pmath_threads_init(void){
  {
    aborting = FALSE;
    _pmath_abort_reasons = 0;
    
    #if PMATH_USE_PTHREAD
    { /* create threadkey ... */
      int callcount = 2;
      int err;
      do{
        err = pthread_key_create(&threadkey, PMATH_NULL/*(void(*)(void*)) destroy_threadkey_data*/);
      }while(err == EAGAIN && --callcount >= 0);
      if(err)
        goto FAIL_THREAD_KEY;
    }
    #elif PMATH_USE_WINDOWS_THREADS
      threadkey = TlsAlloc();
      if(threadkey == TLS_OUT_OF_INDEXES)
        goto FAIL_THREAD_KEY;
    #endif
  }

  return TRUE;

 FAIL_THREAD_KEY:
  return FALSE;
}

PMATH_PRIVATE void _pmath_threads_done(void){
  /* destroy the main (=current) thread's thread data ... */
//  pmath_continue_after_abort();
  
  #if PMATH_USE_PTHREAD
    {
    #ifdef PMATH_DEBUG_LOG
      int err =
    #endif
    
    pthread_key_delete(threadkey);
    
    #ifdef PMATH_DEBUG_LOG
      if(err != 0)
        pmath_debug_print("\apthread_key_delete() returned %d\n", err);
    #endif
    }
  #elif PMATH_USE_WINDOWS_THREADS
    if(!TlsFree(threadkey))
      pmath_debug_print("\aTlsFree() failed.\n");
  #endif
  
  #ifdef PMATH_DEBUG_LOG
  if(pmath_atomic_fetch_set(&aborting, 0)){
    (void)pmath_atomic_fetch_add(&_pmath_abort_reasons, -1);
  }
  
  if(_pmath_abort_reasons != 0){
    pmath_debug_print("\a_pmath_abort_reasons = %d\n", (int)_pmath_abort_reasons);
  }
  #endif
}
