#include <pmath-util/concurrency/threadlocks-private.h>

#include <pmath-util/concurrency/atomic-private.h>
#include <pmath-util/concurrency/threads-private.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/memory.h>
#include <pmath-util/messages.h>
#include <pmath-util/stacks-private.h>

#include <pmath-builtins/all-symbols-private.h>


#if PMATH_USE_PTHREAD

#include <errno.h>
#include <pthread.h>
#include <string.h>

#elif PMATH_USE_WINDOWS_THREADS

#define NOGDI
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#else

#error Either PThread or Windows Threads must be used

#endif

typedef struct _threadlock_owners_t {
  struct _threadlock_owners_t *next;
  pmath_thread_t     owner; // children of owner wait for mutex
#if PMATH_USE_PTHREAD
  pthread_mutex_t   mutex;
#elif PMATH_USE_WINDOWS_THREADS
  CRITICAL_SECTION  mutex;
#endif
} threadlock_owners_t;

struct _pmath_threadlock_t {
  pmath_threadlock_t  reserved;
  
  threadlock_owners_t *owners;
  pmath_atomic_t       spin_lock;
  
#if PMATH_USE_PTHREAD
  pthread_mutex_t   fallback_wait_mutex;
#elif PMATH_USE_WINDOWS_THREADS
  CRITICAL_SECTION  fallback_wait_mutex;
#endif
  
  pmath_atomic_t      refcount;
};

extern pmath_symbol_t pmath_System_DollarThreadID;
extern pmath_symbol_t pmath_System_General;
extern pmath_symbol_t pmath_System_Stack;

static struct _pmath_stack_t  unused_threadlocks;

static __inline void destroy_all_unused_threadlocks(void) {
  pmath_threadlock_t threadlock;
  while((threadlock = pmath_stack_pop(&unused_threadlocks)) != NULL) {
    assert(threadlock->refcount._data == 0);
    assert(threadlock->owners == NULL);
#if PMATH_USE_PTHREAD
    pthread_mutex_destroy(&threadlock->fallback_wait_mutex);
#elif PMATH_USE_WINDOWS_THREADS
    DeleteCriticalSection(&threadlock->fallback_wait_mutex);
#endif
    pmath_mem_free(threadlock);
  }
}

static pmath_threadlock_t create_threadlock(void) {
  pmath_threadlock_t threadlock = pmath_stack_pop(&unused_threadlocks);
  if(threadlock) {
    assert(threadlock->spin_lock._data == 0);
    pmath_atomic_write_release(&threadlock->refcount, 1);
    return threadlock;
  }
  
  threadlock = pmath_mem_alloc(sizeof(struct _pmath_threadlock_t));
  if(!threadlock)
    return threadlock;
    
  threadlock->owners = NULL;
  pmath_atomic_write_release(&threadlock->refcount, 1);
  
  pmath_atomic_write_release(&threadlock->spin_lock, 0);
#if PMATH_USE_PTHREAD
  pthread_mutex_init(&threadlock->fallback_wait_mutex, NULL);
#elif PMATH_USE_WINDOWS_THREADS
  InitializeCriticalSectionAndSpinCount(&threadlock->fallback_wait_mutex, 4000);
#endif
  return threadlock;
}

static void ref_threadlock(pmath_threadlock_t threadlock) {
  if(threadlock)
    (void)pmath_atomic_fetch_add(&threadlock->refcount, 1);
}

static pmath_threadlock_t unref_threadlock(pmath_threadlock_t threadlock) {
  if(threadlock) {
    if(1 == pmath_atomic_fetch_add(&threadlock->refcount, -1)) {
      assert(threadlock->spin_lock._data == 0);
      assert(threadlock->owners == NULL);
      pmath_stack_push(&unused_threadlocks, threadlock);
      return NULL;
    }
  }
  return threadlock;
}

PMATH_API void pmath_thread_call_locked(
  pmath_threadlock_t *threadlock_ptr,
  pmath_callback_t    callback,
  void               *data
) {
  pmath_thread_t me;
  pmath_threadlock_t threadlock;
  
#if PMATH_USE_PTHREAD
  pthread_mutex_t   *long_waiting_mutex = NULL;
#elif PMATH_USE_WINDOWS_THREADS
  CRITICAL_SECTION  *long_waiting_mutex = NULL;
#endif
  
  assert(threadlock_ptr != NULL);
  
  me = pmath_thread_get_current();
  if(!me)
    return;
    
  threadlock = _pmath_atomic_global_need(
                 (pmath_atomic_t*)threadlock_ptr,
                 (void * (*)(void)) create_threadlock,
                 (void(*)(void*)) ref_threadlock);
                 
  if(!threadlock)
    return;
    
  {
    threadlock_owners_t *owners;
    
    pmath_atomic_lock(&threadlock->spin_lock);
    
    owners = threadlock->owners;
    if(!owners || owners->owner != me) {
      while(owners && !pmath_thread_is_parent(owners->owner, me))
        owners = owners->next;
        
      if(!owners)
        long_waiting_mutex = &threadlock->fallback_wait_mutex;
      else
        long_waiting_mutex = &owners->mutex;
    }
    
    pmath_atomic_unlock(&threadlock->spin_lock);
  }
  
  pmath_atomic_barrier();
  
  if(long_waiting_mutex) {
    me->waiting_lock = threadlock;
    
    if(
#if PMATH_USE_PTHREAD
      pthread_mutex_trylock(long_waiting_mutex) == EBUSY
#elif PMATH_USE_WINDOWS_THREADS
      !TryEnterCriticalSection(long_waiting_mutex)
#endif
    ) {
      /* Detecting Possible Deadlocks
      
         Every thread knows, which threadlock it is waiting for and every
         threadlock knows by which thread it is held. A cycle in this linked
         chain equals a deadlock.
         An example would be two theads that execute
         Synchronize(A, Synchronize(B, ...)) and
         Synchronize(B, Synchronize(A, ...)) in parallel.
       */
      pmath_threadlock_t waiting_lock, next;
      waiting_lock = threadlock;
      while(waiting_lock) {
        pmath_atomic_lock(&waiting_lock->spin_lock);
        
        if(waiting_lock->owners == NULL) {
          assert(waiting_lock->spin_lock._data == 1);
          
          pmath_atomic_unlock(&waiting_lock->spin_lock);
          break;
        }
        
        if(waiting_lock->owners->owner == me) {
          assert(waiting_lock->spin_lock._data == 1);
          
          pmath_atomic_unlock(&waiting_lock->spin_lock);
          
          /* deadlock detected: The thread, that holds threadlock is waiting
           * for a lock that `me` blocks directly or indirectly.
           */
          //pmath_abort_please(me);
          me->waiting_lock = NULL;
          
          pmath_message(
            pmath_System_General, "deadlock", 2,
            pmath_evaluate(pmath_ref(pmath_System_DollarThreadID)),
            pmath_evaluate(pmath_expr_new(pmath_ref(pmath_System_Stack), 0)));
            
          //pthread_mutex_unlock(long_waiting_mutex);
          
          pmath_throw(PMATH_ABORT_EXCEPTION); // abort this thread
          
          goto CLEANUP;
        }
        
        if(waiting_lock->owners->owner == NULL) {
          assert(waiting_lock->spin_lock._data == 1);
          
          pmath_atomic_unlock(&waiting_lock->spin_lock);
          break;
        }
        
        next = waiting_lock->owners->owner->waiting_lock;
        
        assert(waiting_lock->spin_lock._data == 1);
        
        pmath_atomic_unlock(&waiting_lock->spin_lock);
        
        waiting_lock = next;
      }
      
#if PMATH_USE_PTHREAD
      pthread_mutex_lock(long_waiting_mutex);
#elif PMATH_USE_WINDOWS_THREADS
      EnterCriticalSection(long_waiting_mutex);
#endif
    }
    me->waiting_lock = NULL;
    
    pmath_atomic_barrier();
    
    /* I aquired the lock.
       Let all my (not yet existing) child threads use a new common mutex, while I
       hold long_waiting_mutex.
     */
    {
      threadlock_owners_t *new_owner;
      
      pmath_atomic_lock(&threadlock->spin_lock);
      
      assert(threadlock->spin_lock._data == 1);
      
      new_owner = pmath_mem_alloc(sizeof(threadlock_owners_t));
      if(!new_owner) {
        assert(threadlock->spin_lock._data == 1);
        
        pmath_atomic_unlock(&threadlock->spin_lock);
#if PMATH_USE_PTHREAD
//          pthread_mutex_unlock(&threadlock->tmp_mutex);
        pthread_mutex_unlock(long_waiting_mutex);
#elif PMATH_USE_WINDOWS_THREADS
//          LeaveCriticalSection(&threadlock->tmp_mutex);
        LeaveCriticalSection(long_waiting_mutex);
#endif
        goto CLEANUP;
      }
      
      new_owner->next = threadlock->owners;
      new_owner->owner = me;
#if PMATH_USE_PTHREAD
      pthread_mutex_init(&new_owner->mutex, NULL);
#elif PMATH_USE_WINDOWS_THREADS
      InitializeCriticalSectionAndSpinCount(&new_owner->mutex, 4000);
#endif
      threadlock->owners = new_owner;
      
      assert(threadlock->spin_lock._data == 1);
      
      pmath_atomic_unlock(&threadlock->spin_lock);
    }
  }
  
  pmath_atomic_barrier();
  
  callback(data);
  
  if(long_waiting_mutex) {
    { // Destroy the mutex for my (not any more existing) child threads.
      threadlock_owners_t *old_owner;
      pmath_atomic_lock(&threadlock->spin_lock);
      
      old_owner = threadlock->owners;
      assert(old_owner != NULL);
      assert(old_owner->owner == me);
      threadlock->owners = old_owner->next;
      
      assert(threadlock->spin_lock._data == 1);
      
      pmath_atomic_unlock(&threadlock->spin_lock);
      
#if PMATH_USE_PTHREAD
      pthread_mutex_destroy(&old_owner->mutex);
#elif PMATH_USE_WINDOWS_THREADS
      DeleteCriticalSection(&old_owner->mutex);
#endif
      pmath_mem_free(old_owner);
    }
    
#if PMATH_USE_PTHREAD
    pthread_mutex_unlock(long_waiting_mutex);
#elif PMATH_USE_WINDOWS_THREADS
    LeaveCriticalSection(long_waiting_mutex);
#endif
  }
  
CLEANUP:
  _pmath_atomic_global_done(
    (pmath_atomic_t*)threadlock_ptr,
    (void*)          threadlock,
    (void*(*)(void*))unref_threadlock);
}

PMATH_PRIVATE void _pmath_threadlocks_memory_panic(void) {
  destroy_all_unused_threadlocks();
}

PMATH_PRIVATE pmath_bool_t _pmath_threadlocks_init(void) {
  memset(&unused_threadlocks, 0, sizeof(unused_threadlocks));
  
  return TRUE;
}

PMATH_PRIVATE void _pmath_threadlocks_done(void) {
  destroy_all_unused_threadlocks();
}
