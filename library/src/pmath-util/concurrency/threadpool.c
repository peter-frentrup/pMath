#include <pmath-util/concurrency/threadpool-private.h>

#include <pmath-core/objects-private.h>
#include <pmath-core/symbols-private.h>

#include <pmath-util/concurrency/threadmsg-private.h>
#include <pmath-util/concurrency/threads-private.h>
#include <pmath-util/debug.h>
#include <pmath-util/memory.h>
#include <pmath-util/messages.h>
#include <pmath-util/stacks-private.h>
#include <pmath-util/symbol-values-private.h>

#include <pmath-builtins/all-symbols-private.h>

#include <errno.h>
#include <math.h>


#if PMATH_USE_PTHREAD
  #include <pthread.h>
  #include <sched.h>
#endif

#ifdef PMATH_OS_WIN32
  #define NOGDI
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
  #include <process.h> // _beginthreadex

  typedef HANDLE sem_t;

  static int sem_init(sem_t *sem, int pshared, unsigned int value){
    *sem = CreateSemaphore(0, value, 0x7FFFFFFF, 0);
    return *sem == 0 ? -1 : 0;
  }

  static int sem_destroy(sem_t *sem){
    return CloseHandle(*sem) ? 0 : -1;
  }

  static int sem_wait(sem_t *sem){
    errno = 0;
    return WaitForSingleObject(*sem, INFINITE) == WAIT_OBJECT_0 ? 0 : -1;
  }

  static int sem_post(sem_t *sem){
    return ReleaseSemaphore(*sem, 1, 0) ? 0 : -1;
  }

#else

  #include <stdio.h>
  #include <string.h>
  
  #ifdef __APPLE__
  /* Mac OS X has no sem_timedwait. We use pthread_mutex_t with pthread_cond_t 
     instead. See Firebird: src/common/classes/semaphore.h for the idea.
   */
    
    typedef struct{
      pthread_mutex_t mutex;
      pthread_cond_t  cond;
    } sem_t;
    
    static int sem_init(sem_t *sem, int pshared, unsigned int value){
      int err;
      
      err = pthread_mutex_init(&sem->mutex, NULL);
      if(err)
        return -1;
      
      err = pthread_cond_init(&sem->cond, NULL);
      return err ? -1 : 0;
    }

    static int sem_destroy(sem_t *sem){
      int err;
      
      err = pthread_mutex_destroy(&sem->mutex);
      if(err)
        return -1;
      
      err = pthread_cond_destroy(&sem->cond);
      return err ? -1 : 0;
    }

    static int sem_wait(sem_t *sem){
      int err;
      errno = 0;
      
      err = pthread_mutex_lock(&sem->mutex);
      if(err)
        return -1;
      
      do{
        err = pthread_cond_wait(&sem->cond, &sem->mutex);
      }while(err == EINTR);
      
      pthread_mutex_unlock(&sem->mutex);
      
      errno = 0;
      return 0;
    }

    static int sem_timedwait(sem_t *sem, const struct timespec *abs_timeout){
      int err;
      errno = 0;
      
      err = pthread_mutex_lock(&sem->mutex);
      if(err)
        return -1;
      
      do{
        err = pthread_cond_timedwait(&sem->cond, &sem->mutex, abs_timeout);
      }while(err == EINTR);
      
      pthread_mutex_unlock(&sem->mutex);
      
      errno = 0;
      return 0;
    }
    
    static int sem_post(sem_t *sem){
      int err;
      errno = 0;
      
      err = pthread_mutex_lock(&sem->mutex);
      if(err)
        return -1;
      
      err = pthread_cond_broadcast(&sem->cond);
      
      pthread_mutex_unlock(&sem->mutex);
      return err ? -1 : 0;
    }
    
  #else

    #include <semaphore.h>
    #include <time.h>
    #include <unistd.h>
    
  #endif
#endif

struct _pmath_task_t{
  void *reserved;
  PMATH_DECLARE_ATOMIC(refcount);
  PMATH_DECLARE_ATOMIC(status);
    #define TASK_IDLE      0
    #define TASK_RUNNING   1
    #define TASK_DONE      2

  sem_t                 done;
  pmath_callback_t run;
  pmath_callback_t destroy;
  void *data;
  pmath_thread_t thread;
};

struct _pmath_task_t *create_idle_task(
  pmath_callback_t run,
  pmath_callback_t destroy,
  void *data,
  pmath_thread_t thread
){
  struct _pmath_task_t *task;

  assert(run);
  assert(destroy);

  task = (struct _pmath_task_t*)pmath_mem_alloc(
    sizeof(struct _pmath_task_t));

  if(!task){
    destroy(data);
    _pmath_thread_free(thread);
    return NULL;
  }

  if(sem_init(&task->done, 0, 0) != 0){
    destroy(data);
    _pmath_thread_free(thread);
    pmath_mem_free(task);
    return NULL;
  }

  task->refcount = 1;
  task->status   = TASK_IDLE;
  task->run      = run;
  task->destroy  = destroy;
  task->data     = data;
  task->thread   = thread;
  return task;
}

static void run_task(struct _pmath_task_t *task){
  assert(task);

  if(PMATH_LIKELY(task->thread)){
    pmath_thread_t old_thread;

    old_thread = pmath_thread_get_current();
    _pmath_thread_set_current(task->thread);
    if(old_thread)
      task->thread->evaldepth = old_thread->evaldepth;

    task->run(task->data);

    if(task->thread->exception != PMATH_UNDEFINED){
      pmath_t exception = _pmath_thread_catch(task->thread);
      if(exception != PMATH_UNDEFINED
      && exception != PMATH_ABORT_EXCEPTION){
        if(task->thread->parent){
          _pmath_thread_throw(task->thread->parent, exception);
        }
        else{
          pmath_message(PMATH_SYMBOL_THROW, "nocatch", 1, exception);
        }
      }
    }

    _pmath_thread_set_current(old_thread);
  }
  else
    task->run(task->data);
}

/*============================================================================*/

PMATH_API
pmath_task_t pmath_task_ref(pmath_task_t task){
  if(PMATH_LIKELY(task))
    (void)pmath_atomic_fetch_add(&task->refcount, 1);

  return task;
}

PMATH_API
void pmath_task_unref(pmath_task_t task){
  if(PMATH_LIKELY(task)
  && 1 == pmath_atomic_fetch_add(&task->refcount, -1)){
    task->destroy(task->data);
    _pmath_thread_free(task->thread);
    sem_destroy(&task->done);
    pmath_mem_free(task);
  }
}

/*============================================================================*/

PMATH_API
void *pmath_task_get_data(pmath_task_t task){
  if(!task)
    return NULL;

  return task->data;
}

PMATH_API
pmath_bool_t pmath_task_has_destructor(
  pmath_task_t           task,
  pmath_callback_t  dtor
){
  if(!task)
    return FALSE;

  return task->destroy == dtor;
}

PMATH_PRIVATE
pmath_thread_t _pmath_task_get_thread(pmath_task_t task){
  if(!task)
    return NULL;

  return task->thread;
}

/*============================================================================*/

static pmath_stack_t idle_tasks;
static sem_t have_idle_tasks;

PMATH_PRIVATE
pmath_task_t _pmath_task_new_with_thread(
  pmath_callback_t run,
  pmath_callback_t destroy,
  void *data,
  pmath_thread_t thread
){
  pmath_task_t task = create_idle_task(run, destroy, data, thread);
  if(task){
    pmath_stack_push(idle_tasks, pmath_task_ref(task));
    sem_post(&have_idle_tasks);
  }

  return task;
}

PMATH_API
pmath_task_t pmath_task_new(
  pmath_callback_t run,
  pmath_callback_t destroy,
  void *data
){
  return _pmath_task_new_with_thread(
    run,
    destroy,
    data,
    _pmath_thread_new(NULL));
}

void pmath_task_wait(pmath_task_t task){
  if(PMATH_LIKELY(task)){
    intptr_t status = pmath_atomic_fetch_set(&task->status, TASK_RUNNING);

    if(status == TASK_RUNNING){ // allready running
      pmath_thread_t me = NULL;
      
      // ...._pmath_msq_queue_set_child();
      if(task->thread){
        me = pmath_thread_get_current();
        _pmath_msq_queue_set_child(me, task->thread);
      }
      
      while(sem_wait(&task->done) == -1 && errno == EINTR)
        continue;
      
      _pmath_msq_queue_set_child(me, NULL);
      
      return;
    }

    if(status == TASK_IDLE){
      run_task(task);
    }

    (void)pmath_atomic_fetch_set(&task->status, TASK_DONE);
    sem_post(&task->done);
  }
}

PMATH_API
void pmath_task_abort(pmath_task_t task){
  if(PMATH_LIKELY(task)){
    intptr_t status = pmath_atomic_fetch_set(&task->status, TASK_RUNNING);
    
    if(status == TASK_RUNNING){ // allready running
      pmath_thread_t me = NULL;
      pmath_thread_t child = _pmath_task_get_thread(task);
      
      _pmath_thread_throw(
        child,
        PMATH_ABORT_EXCEPTION);
        
      if(child){
        me = pmath_thread_get_current();
        _pmath_msq_queue_set_child(me, child);
      }
      
      while(sem_wait(&task->done) == -1 && errno == EINTR)
        continue;
      
      _pmath_msq_queue_set_child(me, NULL);
      
      return;
    }

    if(status == TASK_IDLE){
      // not yet running. do nothing
    }

    (void)pmath_atomic_fetch_set(&task->status, TASK_DONE);
    sem_post(&task->done);
  }
}

/*============================================================================*/

#ifdef PMATH_OS_WIN32
  #define THREAD_PROC(name,arg) unsigned __stdcall name(void *arg)
  typedef uintptr_t thread_handle_t;
#else
  #define THREAD_PROC(name,arg) void *name(void *arg)
  typedef pthread_t thread_handle_t;
#endif

struct daemon_t{
  thread_handle_t             handle;
  pmath_thread_t              thread;
  pmath_callback_t       callback;
  pmath_callback_t       kill;
  void                       *cb_data;
  
  pmath_messages_t          *message_queue_ptr;
  volatile pmath_bool_t  *init_ok;
  sem_t                     *init_sem;
  
  struct daemon_t * volatile  prev;
  struct daemon_t * volatile  next;
};

static struct daemon_t * volatile all_daemons = NULL; // ring buffer
static PMATH_DECLARE_ATOMIC(daemon_spin) = 0;
PMATH_PRIVATE PMATH_DECLARE_ATOMIC(_pmath_threadpool_deamon_count) = 0;

PMATH_API pmath_bool_t pmath_init(void);
PMATH_API void pmath_done(void);

static THREAD_PROC(daemon_proc, arg){
  struct daemon_t *me = (struct daemon_t*)arg;
  
  *me->init_ok = FALSE;
  
  if(!pmath_init()){
    sem_post(me->init_sem);
    return 0;
  }
  
  me->thread = pmath_thread_get_current();
  me->thread->is_daemon = TRUE;
  *me->message_queue_ptr = pmath_ref(me->thread->message_queue);
  me->message_queue_ptr = NULL;
  *me->init_ok = TRUE;
  pmath_atomic_barrier();
  sem_post(me->init_sem);
  
  me->init_sem = NULL;
  
  (void)pmath_atomic_fetch_add(&_pmath_threadpool_deamon_count, 1);
  
  pmath_atomic_lock(&daemon_spin);
  {
    struct daemon_t *all = all_daemons;
    
    if(all){
      all->next->prev = me->prev;
      me->prev->next = all->next;
      
      all->next = me;
      me->prev = all;
    }
    else
      all_daemons = me;
  }
  pmath_atomic_unlock(&daemon_spin);
  
  me->callback(me->cb_data);
  
  pmath_atomic_lock(&daemon_spin);
  {
    me->kill = NULL;
    
    if(me->next != me || (me != all_daemons && all_daemons != NULL)){
      me->next->prev = me->prev;
      me->prev->next = me->next;
      if(me == all_daemons){
        if(me->next == me)
          all_daemons = NULL;
        else
          all_daemons = me->next;
      }
      me->prev = me;
      me->next = me;
      
      me->thread = NULL;
    }
    else
      me = NULL;
  }
  pmath_atomic_unlock(&daemon_spin);
  
  (void)pmath_atomic_fetch_add(&_pmath_threadpool_deamon_count, -1);
  pmath_done();
  
  if(me){
    #ifdef PMATH_OS_WIN32
      CloseHandle((HANDLE)me->handle);
    #else
      pthread_detach(me->handle);
    #endif
    
    pmath_debug_print("[free deamon %p]", me);
    pmath_mem_free(me);
  }
  
  return 0;
}

PMATH_PRIVATE void _pmath_threadpool_kill_daemons(void){
  struct daemon_t *all;
  struct daemon_t *daemon;
  
  all = (struct daemon_t*)pmath_atomic_fetch_set((intptr_t*)&all_daemons, 0);
  while(all){
    daemon = all;
    do{
      // TODO: is this safe???
      //       maybe we should send an Abort() command?
      //       maybe it is not neccessary, because pmath_aborting() might check
      //       for pmath_done() calls (i don't remember^^)
      //_pmath_thread_throw(daemon->thread, PMATH_ABORT_EXCEPTION);
      
      if(daemon->kill)
        daemon->kill(daemon->cb_data);
        
      daemon = daemon->next;
    }while(daemon != all);
    
    while(all){
      pmath_atomic_lock(&daemon_spin);
      
      daemon = all;
      if(daemon){
        daemon->next->prev = daemon->prev;
        daemon->prev->next = daemon->next;
        
        if(daemon->next == daemon)
          all = NULL;
        else
          all = daemon->next;
          
        daemon->next = daemon;
        daemon->prev = daemon;
      }
      
      pmath_atomic_unlock(&daemon_spin);
      
      if(daemon){
        pmath_debug_print("[kill deamon %p]", daemon);
        
        #ifdef PMATH_OS_WIN32
          WaitForSingleObject((HANDLE)daemon->handle, INFINITE);
          CloseHandle((HANDLE)daemon->handle);
        #else
          pthread_join(daemon->handle, 0);
          pthread_detach(daemon->handle);
        #endif
        
        pmath_mem_free(daemon);
      }
    }
    
    all = (struct daemon_t*)pmath_atomic_fetch_set((intptr_t*)&all_daemons, 0);
  }
}

PMATH_API
pmath_messages_t pmath_thread_fork_daemon(
  pmath_callback_t  callback,
  pmath_callback_t  kill,
  void                  *data
){
  pmath_messages_t message_queue = NULL;
  pmath_bool_t error;
  volatile pmath_bool_t init_ok;
  struct daemon_t *daemon;
  sem_t sem;
  
  if(sem_init(&sem, 0, 0) == -1)
    return NULL;
  
  daemon = pmath_mem_alloc(sizeof(struct daemon_t));
  if(!daemon)
    return NULL;
  
  daemon->thread            = NULL;
  daemon->callback          = callback;
  daemon->kill              = kill;
  daemon->cb_data           = data;
  daemon->message_queue_ptr = &message_queue;
  daemon->init_ok           = &init_ok;
  daemon->init_sem          = &sem;
  daemon->prev              = daemon;
  daemon->next              = daemon;
  
  init_ok = FALSE;
  error = TRUE;
  #ifdef PMATH_OS_WIN32
  {
    daemon->handle = _beginthreadex(
      NULL,     // default security
      0,        // default stack size
      daemon_proc,
      daemon,   // argument
      0,        // running
      NULL);    // do not need thread id
    
    error = daemon->handle == 0;
  }
  #else
  {
    error = pthread_create(
      &daemon->handle,
      NULL,
      daemon_proc,
      daemon);      // argument
  }
  #endif
  
  if(error){
    pmath_mem_free(daemon);
    sem_destroy(&sem);
    return NULL;
  }
  
  while(sem_wait(&sem) == -1 && errno == EINTR)
    continue;
  
  sem_destroy(&sem);
  pmath_atomic_barrier();
  if(!init_ok){
    assert(message_queue == NULL);
    
    #ifdef PMATH_OS_WIN32
      WaitForSingleObject((HANDLE)daemon->handle, INFINITE);
      CloseHandle((HANDLE)daemon->handle);
    #else
      pthread_join(daemon->handle, 0);
      pthread_detach(daemon->handle);
    #endif
    
    pmath_mem_free(daemon);
    return NULL;
  }
  
  return message_queue;
}

/*============================================================================*/

static volatile pmath_bool_t stop_threadpool;
static PMATH_DECLARE_ATOMIC(init_threads_counter);

static struct worker_t{
  thread_handle_t thread;
}*workers;

static THREAD_PROC(worker_thread_proc, arg){
  (void)pmath_atomic_fetch_add(&init_threads_counter, -1);

  while(!stop_threadpool){
    pmath_task_t task = (pmath_task_t)pmath_stack_pop(idle_tasks);

    if(PMATH_LIKELY(task)){
      intptr_t status = pmath_atomic_fetch_set(&task->status, TASK_RUNNING);

      if(status != TASK_RUNNING){
        if(status == TASK_IDLE){
          run_task(task);
        }

        (void)pmath_atomic_fetch_set(&task->status, TASK_DONE);
        sem_post(&task->done);
      }

      pmath_task_unref(task);
    }

    while(sem_wait(&have_idle_tasks) == -1 && errno == EINTR)
      continue;
  }

  return 0;
}

/*============================================================================*/

#define GC_PASS_BITS   1
#define GC_PASS_COUNT  (1 << GC_PASS_BITS)
#define GC_PASS_MASK   (GC_PASS_COUNT-1)

#define GC_WAIT_SEC 60 /* 1 minute */
static double last_gc_time;
static uintptr_t gc_pass = 0;

static sem_t                  timer_thread_sem;
static thread_handle_t        timer_thread;
static struct _pmath_stack_t  unsorted_msgs;

PMATH_API void pmath_collect_temporary_symbols(void){
  last_gc_time-= GC_WAIT_SEC;
  sem_post(&timer_thread_sem);
}

  static pmath_bool_t gc_visit_ref(pmath_t obj, void *dummy){
    if(pmath_instance_of(obj, PMATH_TYPE_EXPRESSION | PMATH_TYPE_SYMBOL)){
      if((((struct _pmath_gc_t*)obj)->gc_refcount & GC_PASS_MASK) == gc_pass){
        ((struct _pmath_gc_t*)obj)->gc_refcount+= GC_PASS_COUNT;
      }
      else
        ((struct _pmath_gc_t*)obj)->gc_refcount = gc_pass | GC_PASS_COUNT;
    }
    return TRUE;
  }
  
  static uintptr_t get_gc_refs(pmath_t obj){
    if((((struct _pmath_gc_t*)obj)->gc_refcount & GC_PASS_MASK) == gc_pass){
      return ((struct _pmath_gc_t*)obj)->gc_refcount >> GC_PASS_BITS;
    }
    
    return 0;
  }

  static pmath_bool_t gc_all_expr_visited(pmath_t obj, void *dummy){
    if(pmath_instance_of(obj, PMATH_TYPE_EXPRESSION)){
      uintptr_t gc_refs = get_gc_refs(obj);
      
      // one reference is held by caller
      ++gc_refs;
      
      return gc_refs == (uintptr_t)obj->refcount;
    }
    
    return TRUE;
  }

static void run_gc(void){
  pmath_symbol_t sym;
  
  gc_pass = (gc_pass + 1) & GC_PASS_MASK;
  
  // mark/reference all temp. symbol values
  sym = pmath_ref(PMATH_SYMBOL_LIST);
  do{
    if(pmath_symbol_get_attributes(sym) & PMATH_SYMBOL_ATTRIBUTE_TEMPORARY){
      struct _pmath_symbol_rules_t *rules;
      
      _pmath_symbol_value_visit(
        _pmath_symbol_get_global_value(sym), 
        gc_visit_ref, 
        NULL);
      
      rules = _pmath_symbol_get_rules(sym, RULES_READ);
      
      if(rules)
        _pmath_symbol_rules_visit(rules, gc_visit_ref, NULL);
    }
    
    sym = pmath_symbol_iter_next(sym);
  }while(sym && sym != PMATH_SYMBOL_LIST);
  pmath_unref(sym);
  
  // clear all temp. symbols that are only referenced by temp. symbols.
  sym = pmath_ref(PMATH_SYMBOL_LIST);
  do{
    if(pmath_symbol_get_attributes(sym) & PMATH_SYMBOL_ATTRIBUTE_TEMPORARY){
      uintptr_t gc_refs = get_gc_refs(sym);
      
      // one reference is hold by sym
      ++gc_refs;
      
      if(gc_refs     <  (uintptr_t)sym->refcount
      && gc_refs + 3 >= (uintptr_t)sym->refcount){
        if(_pmath_have_code(sym, PMATH_CODE_USAGE_DOWNCALL))
          ++gc_refs;
          
        if(_pmath_have_code(sym, PMATH_CODE_USAGE_UPCALL))
          ++gc_refs;
          
        if(_pmath_have_code(sym, PMATH_CODE_USAGE_SUBCALL))
          ++gc_refs;
      }
      
      if(gc_refs == (uintptr_t)sym->refcount){
        pmath_bool_t all_visited;
        
        all_visited = _pmath_symbol_value_visit(
          _pmath_symbol_get_global_value(sym), 
          gc_all_expr_visited, 
          NULL);
        
        if(all_visited){
          struct _pmath_symbol_rules_t *rules;
          rules = _pmath_symbol_get_rules(sym, RULES_READ);
          
          all_visited = _pmath_symbol_rules_visit(rules, gc_all_expr_visited, NULL);
          
          /* all_visited: the whole symbol value (expr tree) is only referenced 
             by temp. symbols and so was visited by the gc in the previous loop.
           */
          if(all_visited){
            pmath_symbol_set_attributes(sym, PMATH_SYMBOL_ATTRIBUTE_TEMPORARY);
            _pmath_symbol_set_global_value(sym, PMATH_UNDEFINED);
            
            if(rules){
              rules = _pmath_symbol_get_rules(sym, RULES_WRITEOPTIONS);
              if(rules)
                _pmath_symbol_rules_clear(rules);
            }
            
            pmath_register_code(sym, NULL, PMATH_CODE_USAGE_DOWNCALL);
            pmath_register_code(sym, NULL, PMATH_CODE_USAGE_UPCALL);
            pmath_register_code(sym, NULL, PMATH_CODE_USAGE_SUBCALL);
          }
        }
      }
    }
    
    sym = pmath_symbol_iter_next(sym);
  }while(sym && sym != PMATH_SYMBOL_LIST);
  pmath_unref(sym);
}

PMATH_PRIVATE void _pmath_register_timed_msg(struct _pmath_timed_message_t *msg){
  if(!msg)
    return;
  
  if(!(msg->absolute_time < HUGE_VAL) || stop_threadpool){
    pmath_unref(msg->message_queue);
    pmath_unref(msg->subject);
    pmath_mem_free(msg);
    return;
  }
  
  pmath_stack_push(&unsorted_msgs, msg);
  sem_post(&timer_thread_sem);
}

static void discard_all_timed_msgs(void){
  struct _pmath_timed_message_t *msg;
  
  msg = pmath_stack_pop(&unsorted_msgs);
  while(msg){
    pmath_unref(msg->message_queue);
    pmath_unref(msg->subject);
    pmath_mem_free(msg);
    
    msg = pmath_stack_pop(&unsorted_msgs);
  }
}

static THREAD_PROC(timer_thread_proc, dummy){
  struct _pmath_timed_message_t *msg;
  struct _pmath_timed_message_t *umsg;
  struct _pmath_timed_message_t *sorted_msgs = NULL;
  double now;
  double next_event;
  
  pmath_bool_t noop = FALSE;
  
  (void)pmath_atomic_fetch_add(&init_threads_counter, -1);

  #ifdef PMATH_OS_WIN32
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_IDLE);
  #endif
  
  now = pmath_tickcount();
  last_gc_time = now;
  next_event = last_gc_time + GC_WAIT_SEC;
  while(!stop_threadpool){
    #ifdef PMATH_OS_WIN32
    {
      DWORD rel;
      
      rel = (DWORD)(1000 * (next_event - now));
      if((int)rel < 0)
        rel = 0;
        
      WaitForSingleObject(timer_thread_sem, rel);
    }
    #else
    {
      /* next_event = seconds since UNIX epoch, so conversion is easy */
      struct timespec ts;
      ts.tv_sec  = (time_t)next_event;
      ts.tv_nsec = (long)fmod(next_event * 1e9, 1e9);
      
      while(sem_timedwait(&timer_thread_sem, &ts) == -1 && errno == EINTR)
        continue;
    }
    #endif

    if(stop_threadpool)
      break;
    
    noop = TRUE;
    
    umsg = pmath_stack_pop(&unsorted_msgs);
    while(umsg){
      struct _pmath_timed_message_t **prev = &sorted_msgs;
      msg = *prev;
      while(msg && msg->absolute_time < umsg->absolute_time){
        prev = &msg->_reserved_next;
        msg = *prev;
      }
      
      *prev = umsg;
      umsg->_reserved_next = msg;
      
      umsg = pmath_stack_pop(&unsorted_msgs);
      
      noop = FALSE;
    }
    
    now = pmath_tickcount() + 0.001;
    while(sorted_msgs){
      msg = sorted_msgs;
      
      if(msg->absolute_time <= now){
        sorted_msgs = sorted_msgs->_reserved_next;
        
        pmath_thread_send(msg->message_queue, msg->subject);
        pmath_unref(msg->message_queue);
        pmath_mem_free(msg);
        
        noop = FALSE;
      }
      else
        break;
    }
    
    if(last_gc_time + GC_WAIT_SEC <= now){
      run_gc();
      last_gc_time = now;
      
      noop = FALSE;
    }
    
    next_event = last_gc_time + GC_WAIT_SEC;
    if(sorted_msgs && sorted_msgs->absolute_time < next_event)
      next_event = sorted_msgs->absolute_time;
  }
  
  while(sorted_msgs){
    msg = sorted_msgs;
    sorted_msgs = sorted_msgs->_reserved_next;
    
    pmath_unref(msg->message_queue);
    pmath_unref(msg->subject);
    pmath_mem_free(msg);
  }

  return 0;
}

/*============================================================================*/

static int worker_count;
static int processor_count;

PMATH_PRIVATE int _pmath_processor_count(void){
  return processor_count;
}

PMATH_PRIVATE pmath_bool_t _pmath_threadpool_init(void){
  int i;
  
  all_daemons = NULL;
  daemon_spin = 0;
  _pmath_threadpool_deamon_count = 0;
  
  memset(&unsorted_msgs, 0, sizeof(unsorted_msgs));
  
  if(sem_init(&have_idle_tasks, 0, 0) != 0)
    goto WORKER_SEM_FAIL;

  if(sem_init(&timer_thread_sem, 0, 0) != 0)
    goto TIMER_SEM_FAIL;

  idle_tasks = pmath_stack_new();
  if(!idle_tasks)
    goto IDLE_TASKS_FAIL;

  #ifdef PMATH_OS_WIN32
  {
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    processor_count = info.dwNumberOfProcessors;
  }
  #else /* unix */
  {
    FILE *f;
    char line[256];

    processor_count = 0;
    f = fopen("/proc/cpuinfo", "r");
    if(f){
      while(fgets(line, sizeof(line), f)){
        if(strncmp(line, "processor", 9) == 0)
          ++processor_count;
      }
      
      if(processor_count < 1)
        processor_count = 1;

      fclose(f);
    }
    
    
    if(processor_count < 1){
      #ifdef _SC_NPROCESSORS_ONLN
        processor_count = sysconf(_SC_NPROCESSORS_ONLN);
      #endif
    }
  }
  #endif

  if(processor_count < 1)
    processor_count = 1;
  
  worker_count = processor_count - 1;
  if(worker_count < 1)
    worker_count = 1;
  stop_threadpool = FALSE;

  workers = (struct worker_t*)pmath_mem_alloc(worker_count * sizeof(struct worker_t));
  init_threads_counter = worker_count + 1; // + GC thread;
  if(!workers)
    goto WORKERS_ARRAY_FAIL;
  
  for(i = 0;i < worker_count;++i){
    #ifdef PMATH_OS_WIN32
      workers[i].thread = _beginthreadex(
        NULL,     // default security
        0,        // default stack size
        worker_thread_proc,
        (void*)i, // argument
        0,        // running
        NULL);    // do not need thread id
      if(workers[i].thread == 0){
        int j;

        stop_threadpool = TRUE;
        for(j = 0;j < i;++j)
          sem_post(&have_idle_tasks);

        for(j = 0;j < i;++j){
          WaitForSingleObject((HANDLE)workers[j].thread, INFINITE);
          CloseHandle((HANDLE)workers[j].thread);
        }

        goto WORKERS_FAIL;
      }
    #else
      int err = pthread_create(
        &workers[i].thread,
        NULL,       // default attributes
        worker_thread_proc,
        (void*)(uintptr_t)i);  // argument

      if(err){
        int j;

        stop_threadpool = TRUE;
        for(j = 0;j < i;++j)
          sem_post(&have_idle_tasks);

        for(j = 0;j < i;++j){
          pthread_join(workers[j].thread, 0);
          pthread_detach(workers[j].thread);
        }

        goto WORKERS_FAIL;
      }
    #endif
  }

  #ifdef PMATH_OS_WIN32
    timer_thread = _beginthreadex(
      NULL,     // default security
      0,        // default stack size
      timer_thread_proc,
      NULL,     // argument
      0,        // running
      NULL);    // do not need thread id

    if(!timer_thread)
      goto TIMER_FAIL;
  #else
  {
    pthread_attr_t      timer_attr;
    struct sched_param  sp;
    int                 err;

    if(pthread_attr_init(&timer_attr))
      goto TIMER_FAIL;

    pthread_attr_getschedparam(&timer_attr, &sp);
    sp.sched_priority--;
    pthread_attr_setschedparam(&timer_attr, &sp);

    err = pthread_create(
      &timer_thread,
      &timer_attr,
      timer_thread_proc,
      NULL);      // argument

    pthread_attr_destroy(&timer_attr);

    if(err)
      goto TIMER_FAIL;
  }
  #endif

  while(init_threads_counter > 0){
  }

  return TRUE;

//    stop_threadpool = TRUE;
//    sem_post(&gc_semaphore);
//    #ifdef PMATH_OS_WIN32
//      WaitForSingleObject((HANDLE)gc_thread, INFINITE);
//      CloseHandle((HANDLE)gc_thread);
//    #else
//      pthread_join(gc_thread, 0);
//      pthread_detach(gc_thread);
//    #endif

 TIMER_FAIL:
    stop_threadpool = TRUE;
    for(i = 0;i < worker_count;++i) // stop all workers
      sem_post(&have_idle_tasks);

    for(i = 0;i < worker_count;++i){
      #ifdef PMATH_OS_WIN32
        WaitForSingleObject((HANDLE)workers[i].thread, INFINITE);
        CloseHandle((HANDLE)workers[i].thread);
      #else
        pthread_join(workers[i].thread, 0);
        pthread_detach(workers[i].thread);
      #endif
    }

 WORKERS_FAIL:         pmath_mem_free(workers);
 WORKERS_ARRAY_FAIL:   pmath_stack_free(idle_tasks);
 IDLE_TASKS_FAIL:      sem_destroy(&timer_thread_sem);
 TIMER_SEM_FAIL:       sem_destroy(&have_idle_tasks);
 WORKER_SEM_FAIL:
  
  discard_all_timed_msgs();
  
  return FALSE;
}

PMATH_PRIVATE void _pmath_threadpool_done(void){
  int i;
  pmath_task_t task;

  stop_threadpool = TRUE;

  sem_post(&timer_thread_sem);
  #ifdef PMATH_OS_WIN32
    WaitForSingleObject((HANDLE)timer_thread, INFINITE);
    CloseHandle((HANDLE)timer_thread);
  #else
    pthread_join(timer_thread, 0);
    pthread_detach(timer_thread);
  #endif

  for(i = 0;i < worker_count;++i) // stop all workers
    sem_post(&have_idle_tasks);

  for(i = 0;i < worker_count;++i){
    #ifdef PMATH_OS_WIN32
      WaitForSingleObject((HANDLE)workers[i].thread, INFINITE);
      CloseHandle((HANDLE)workers[i].thread);
    #else
      pthread_join(workers[i].thread, 0);
      pthread_detach(workers[i].thread);
    #endif
  }

  pmath_mem_free(workers);

  task = (pmath_task_t)pmath_stack_pop(idle_tasks);
  while(task){
    assert(task->refcount == 1);
    pmath_task_unref(task);
    task = (pmath_task_t)pmath_stack_pop(idle_tasks);
  }
  pmath_stack_free(idle_tasks);

  sem_destroy(&timer_thread_sem);
  sem_destroy(&have_idle_tasks);
  
  discard_all_timed_msgs();
}
