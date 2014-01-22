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
#  include <pthread.h>
#  include <sched.h>
#endif

#ifdef PMATH_OS_WIN32
#  define NOGDI
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  include <process.h> // _beginthreadex

typedef HANDLE sem_t;

static int sem_init(sem_t *sem, int pshared, unsigned int value) {
  *sem = CreateSemaphore(0, value, 0x7FFFFFFF, 0);
  return *sem == 0 ? -1 : 0;
}

static int sem_destroy(sem_t *sem) {
  return CloseHandle(*sem) ? 0 : -1;
}

static int sem_wait(sem_t *sem) {
  errno = 0;
  return WaitForSingleObject(*sem, INFINITE) == WAIT_OBJECT_0 ? 0 : -1;
}

static int sem_post(sem_t *sem) {
  return ReleaseSemaphore(*sem, 1, 0) ? 0 : -1;
}

#else
#  include <stdio.h>
#  include <string.h>
#  include <semaphore.h>
#  include <time.h>
#  include <unistd.h>
#endif

struct _pmath_task_t {
  void             *reserved;
  pmath_atomic_t    refcount;
  pmath_atomic_t    status;
#define TASK_IDLE      0
#define TASK_RUNNING   1
#define TASK_DONE      2
  
  sem_t             done;
  pmath_callback_t  run;
  pmath_callback_t  destroy;
  void             *data;
  pmath_thread_t    thread;
};

static struct _pmath_task_t *create_idle_task(
  pmath_callback_t run,
  pmath_callback_t destroy,
  void *data,
  pmath_thread_t thread
) {
  struct _pmath_task_t *task;
  
  assert(run);
  assert(destroy);
  
  task = (struct _pmath_task_t *)pmath_mem_alloc(
           sizeof(struct _pmath_task_t));
           
  if(!task) {
    destroy(data);
    _pmath_thread_free(thread);
    return NULL;
  }
  
  if(sem_init(&task->done, 0, 0) != 0) {
    destroy(data);
    _pmath_thread_free(thread);
    pmath_mem_free(task);
    return NULL;
  }
  
  pmath_atomic_write_release(&task->refcount, 1);
  pmath_atomic_write_release(&task->status, TASK_IDLE);
  task->run      = run;
  task->destroy  = destroy;
  task->data     = data;
  task->thread   = thread;
  return task;
}

static void run_task(struct _pmath_task_t *task) {
  assert(task);
  
  if(PMATH_LIKELY(task->thread)) {
    pmath_thread_t old_thread;
    
    old_thread = pmath_thread_get_current();
    _pmath_thread_set_current(task->thread);
    if(old_thread)
      task->thread->evaldepth = old_thread->evaldepth;
      
    task->run(task->data);
    
    if(!pmath_same(task->thread->exception, PMATH_UNDEFINED)) {
      pmath_t exception = _pmath_thread_catch(task->thread);
      
      if( !pmath_same(exception, PMATH_UNDEFINED) &&
          !pmath_same(exception, PMATH_ABORT_EXCEPTION))
      {
        if(task->thread->parent) {
          _pmath_thread_throw(task->thread->parent, exception);
        }
        else {
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
pmath_task_t pmath_task_ref(pmath_task_t task) {
  if(PMATH_LIKELY(task))
    (void)pmath_atomic_fetch_add(&task->refcount, 1);
    
  return task;
}

PMATH_API
void pmath_task_unref(pmath_task_t task) {
  if( PMATH_LIKELY(task) &&
      1 == pmath_atomic_fetch_add(&task->refcount, -1))
  {
    task->destroy(task->data);
    _pmath_thread_free(task->thread);
    sem_destroy(&task->done);
    pmath_mem_free(task);
  }
}

/*============================================================================*/

PMATH_API
void *pmath_task_get_data(pmath_task_t task) {
  if(!task)
    return NULL;
    
  return task->data;
}

PMATH_API
pmath_bool_t pmath_task_has_destructor(
  pmath_task_t           task,
  pmath_callback_t  dtor
) {
  if(!task)
    return FALSE;
    
  return task->destroy == dtor;
}

PMATH_PRIVATE
pmath_thread_t _pmath_task_get_thread(pmath_task_t task) {
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
) {
  pmath_task_t task = create_idle_task(run, destroy, data, thread);
  if(task) {
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
) {
  return _pmath_task_new_with_thread(
           run,
           destroy,
           data,
           _pmath_thread_new(NULL));
}

void pmath_task_wait(pmath_task_t task) {
  if(PMATH_LIKELY(task)) {
    intptr_t status = pmath_atomic_fetch_set(&task->status, TASK_RUNNING);
    
    if(status == TASK_RUNNING) { // allready running
      pmath_thread_t me = NULL;
      
      // ...._pmath_msq_queue_set_child();
      if(task->thread) {
        me = pmath_thread_get_current();
        _pmath_msq_queue_set_child(me, task->thread);
      }
      
      while(sem_wait(&task->done) == -1 && errno == EINTR)
        continue;
        
      _pmath_msq_queue_set_child(me, NULL);
      
      return;
    }
    
    if(status == TASK_IDLE) {
      run_task(task);
    }
    
    pmath_atomic_write_release(&task->status, TASK_DONE);
    sem_post(&task->done);
  }
}

PMATH_API
void pmath_task_abort(pmath_task_t task) {
  if(PMATH_LIKELY(task)) {
    intptr_t status = pmath_atomic_fetch_set(&task->status, TASK_RUNNING);
    
    if(status == TASK_RUNNING) { // allready running
      pmath_thread_t me = NULL;
      pmath_thread_t child = _pmath_task_get_thread(task);
      
      _pmath_thread_throw(
        child,
        PMATH_ABORT_EXCEPTION);
        
      if(child) {
        me = pmath_thread_get_current();
        _pmath_msq_queue_set_child(me, child);
      }
      
      while(sem_wait(&task->done) == -1 && errno == EINTR)
        continue;
        
      _pmath_msq_queue_set_child(me, NULL);
      
      return;
    }
    
    if(status == TASK_IDLE) {
      // not yet running. do nothing
    }
    
    pmath_atomic_write_release(&task->status, TASK_DONE);
    sem_post(&task->done);
  }
}

/*============================================================================*/

#ifdef PMATH_OS_WIN32
#  define THREAD_PROC(name,arg) unsigned __stdcall name(void *arg)
typedef uintptr_t thread_handle_t;
#else
#  define THREAD_PROC(name,arg) void *name(void *arg)
typedef pthread_t thread_handle_t;
#endif

struct daemon_t {
  thread_handle_t    handle;
  pmath_thread_t     thread;
  pmath_callback_t   callback;
  pmath_callback_t   kill;
  void              *cb_data;
  
  pmath_messages_t  *message_queue_ptr;
  pmath_bool_t       alive;
  
  struct daemon_t   *prev;
  struct daemon_t   *next;
};

static struct daemon_t *all_daemons = NULL; // ring buffer
static pmath_atomic_t daemon_spin = PMATH_ATOMIC_STATIC_INIT;
PMATH_PRIVATE pmath_atomic_t _pmath_threadpool_deamon_count = PMATH_ATOMIC_STATIC_INIT;

PMATH_API pmath_bool_t pmath_init(void);
PMATH_API void pmath_done(void);

static pmath_bool_t daemon_init(void *arg) {
  struct daemon_t *me = (struct daemon_t *)arg;
  
  pmath_debug_print("[new deamon %p]\n", me);
  
  if(!pmath_init())
    return FALSE;
    
  me->thread            = pmath_thread_get_current();
  me->thread->is_daemon = TRUE;
  
  (void)pmath_atomic_fetch_add(&_pmath_threadpool_deamon_count, 1);
  
  pmath_atomic_lock(&daemon_spin);
  {
    struct daemon_t *all = all_daemons;
    
    if(all) {
      all->next->prev = me->prev;
      me->prev->next = all->next;
      
      all->next = me;
      me->prev = all;
    }
    else
      all_daemons = me;
  }
  pmath_atomic_unlock(&daemon_spin);
  
  *me->message_queue_ptr = pmath_ref(me->thread->message_queue);
  me->message_queue_ptr  = NULL;
  
  return TRUE;
}

static void daemon_proc(void *arg) {
  struct daemon_t *me = (struct daemon_t *)arg;
  
  me->callback(me->cb_data);
  
  pmath_atomic_lock(&daemon_spin);
  {
    me->kill = NULL;
    
    pmath_debug_print("[almost free deamon %p, next = %p, prev = %p, all_daemons = %p]\n", me, me->next, me->prev, all_daemons);
    
    if(me->next != me) {
      me->next->prev = me->prev;
      me->prev->next = me->next;
    }
    
    if(me == all_daemons) {
      if(me->next == me)
        all_daemons = NULL;
      else
        all_daemons = me->next;
    }
    me->prev = me;
    me->next = me;
    
    me->thread = NULL;
  }
  pmath_atomic_unlock(&daemon_spin);
  
  pmath_debug_print("[free deamon %p, all_daemons = %p]\n", me, all_daemons);
  pmath_mem_free(me);
  
  pmath_done();
  (void)pmath_atomic_fetch_add(&_pmath_threadpool_deamon_count, -1);
}

PMATH_PRIVATE void _pmath_threadpool_kill_daemons(void) {
#ifdef PMATH_DEBUG_LOG
  int loop_count = 0;
#endif
  
  while(pmath_atomic_read_aquire(&_pmath_threadpool_deamon_count) > 0) {
    pmath_atomic_lock(&daemon_spin);
    {
      struct daemon_t *all;
      struct daemon_t *daemon;
      
      all = all_daemons;
      
      if(all) {
        daemon = all;
        do {
          if(daemon->alive && daemon->kill) {
            daemon->alive = FALSE;
            pmath_debug_print("[killing deamon %p]\n", daemon);
            daemon->kill(daemon->cb_data);
          }
          
          daemon = daemon->next;
        } while(daemon != all);
      }
    }
    pmath_atomic_unlock(&daemon_spin);
    
    _pmath_msq_queue_awake_all();
    
    // Sleep 1 ms to give other thread chance to notice that they should stop.
    pmath_atomic_loop_nop();
    
#ifdef PMATH_DEBUG_LOG
    ++loop_count;
#endif
  }
  
#ifdef PMATH_DEBUG_LOG
  pmath_debug_print("[all deamons dead, %d iterations]\n", loop_count);
#endif
}

// CAUTION: kill(data) will be called while daemon_spin is held
PMATH_API
pmath_messages_t pmath_thread_fork_daemon(
  pmath_callback_t  callback,
  pmath_callback_t  kill,
  void             *data
) {
  pmath_messages_t message_queue = PMATH_NULL;
  struct daemon_t *daemon;
  
  daemon = pmath_mem_alloc(sizeof(struct daemon_t));
  if(!daemon)
    return PMATH_NULL;
    
  daemon->thread            = NULL;
  daemon->callback          = callback;
  daemon->kill              = kill;
  daemon->cb_data           = data;
  daemon->message_queue_ptr = &message_queue;
  daemon->alive             = TRUE;
  daemon->prev              = daemon;
  daemon->next              = daemon;
  
  if(!pmath_thread_fork_unmanaged(daemon_init, daemon_proc, daemon)) {
    pmath_mem_free(daemon);
    return PMATH_NULL;
  }
  
  return message_queue;
}

/*============================================================================*/

struct unmanaged_thread_t {
  sem_t                 *init_sem;
  volatile pmath_bool_t *init_ok;
  
  pmath_bool_t         (*init)(void *);
  void                 (*callback)(void *);
  void                  *data;
};


static THREAD_PROC(unmanaged_thread_proc, arg) {
  struct unmanaged_thread_t *me = (struct unmanaged_thread_t *)arg;
  
  pmath_bool_t init_ok = TRUE;
  
  if(me->init)
    init_ok = me->init(me->data);
    
  *me->init_ok = init_ok;
  
  pmath_atomic_barrier();
  sem_post(me->init_sem);
  me->init_sem = NULL;
  
  if(init_ok) {
    me->callback(me->data);
    free(me);
  }
  
  return 0;
}


PMATH_API
pmath_bool_t pmath_thread_fork_unmanaged(
  pmath_bool_t    (*init)(    void *),
  void            (*callback)(void *),
  void             *data
) {
  struct unmanaged_thread_t *arg;
  pmath_bool_t               error;
  sem_t                      init_sem;
  volatile pmath_bool_t      init_ok;
  
  arg = malloc(sizeof(struct unmanaged_thread_t));
  
  if(!arg)
    return FALSE;
    
  if(sem_init(&init_sem, 0, 0) == -1) {
    free(arg);
    return FALSE;
  }
  
  init_ok = FALSE;
  
  arg->init_sem = &init_sem;
  arg->init_ok  = &init_ok;
  
  arg->init     = init;
  arg->callback = callback;
  arg->data     = data;
  
#ifdef PMATH_OS_WIN32
  {
    HANDLE handle;
    handle = (HANDLE)_beginthreadex(
               NULL,     // default security
               0,        // default stack size
               unmanaged_thread_proc,
               arg,      // argument
               0,        // running
               NULL);    // do not need thread id
               
    error = handle == 0;
    
    if(handle)
      CloseHandle(handle);
  }
#else
  {
    pthread_t handle;
  
    error = pthread_create(
              &handle,
              NULL,
              unmanaged_thread_proc,
              arg);      // argument
  
    pthread_detach(handle);
  }
#endif
  
  if(error) {
    free(arg);
    sem_destroy(&init_sem);
    return FALSE;
  }
  
  while(sem_wait(&init_sem) == -1 && errno == EINTR)
    continue;
    
  sem_destroy(&init_sem);
  pmath_atomic_barrier();
  
  return init_ok;
}

/*============================================================================*/

static volatile pmath_bool_t stop_threadpool;
static pmath_atomic_t init_threads_counter = PMATH_ATOMIC_STATIC_INIT;

static struct worker_t {
  sem_t  finish_sem;
} *workers;

static THREAD_PROC(worker_thread_proc, arg) {
  uintptr_t worker_index = (uintptr_t)arg;
  
  (void)pmath_atomic_fetch_add(&init_threads_counter, -1);
  
  while(!stop_threadpool) {
    pmath_task_t task = (pmath_task_t)pmath_stack_pop(idle_tasks);
    
    if(PMATH_LIKELY(task)) {
      intptr_t status = pmath_atomic_fetch_set(&task->status, TASK_RUNNING);
      
      if(status != TASK_RUNNING) {
        if(status == TASK_IDLE) {
          run_task(task);
        }
        
        pmath_atomic_write_release(&task->status, TASK_DONE);
        sem_post(&task->done);
      }
      
      pmath_task_unref(task);
    }
    
    while(sem_wait(&have_idle_tasks) == -1 && errno == EINTR)
      continue;
  }
  
  sem_post(&workers[worker_index].finish_sem);
  
  return 0;
}

/*============================================================================*/

#define GC_PASS_BITS   1
#define GC_PASS_COUNT  (1 << GC_PASS_BITS)
#define GC_PASS_MASK   (GC_PASS_COUNT-1)

#define GC_WAIT_SEC 60 /* 1 minute */
static double last_gc_time;
static uintptr_t gc_pass = 0;
static pmath_bool_t gc_is_running = FALSE;

static pmath_messages_t       timer_thread_mq;
static struct _pmath_stack_t  unsorted_msgs;

#ifdef PMATH_DEBUG_LOG

PMATH_PRIVATE
pmath_atomic_t _pmath_debug_current_gc_symbol = PMATH_ATOMIC_STATIC_INIT;

#endif

PMATH_API void pmath_collect_temporary_symbols(void) {
  last_gc_time -= GC_WAIT_SEC;
  pmath_thread_wakeup(timer_thread_mq);
}

//#ifdef PMATH_DEBUG_LOG
//static pmath_bool_t gc_visit_check_ref(pmath_t obj, void *dummy) {
//  if(pmath_is_symbol(obj) || pmath_is_expr(obj)) {
//    struct _pmath_gc_t *gc_obj = (void *)PMATH_AS_PTR(obj);
//    
//    if( 0 != gc_obj->gc_refcount && 
//        (gc_obj->gc_refcount & GC_PASS_MASK) != ((gc_pass - 1) & GC_PASS_MASK)) 
//    {
//      pmath_debug_print("[not from prev. pass: %"PRIxPTR", ", gc_obj->gc_refcount);
//      pmath_debug_print_object("", obj , "]\n");
//      
//      return FALSE;
//    }
//  }
//  return TRUE;
//}
//#endif

static pmath_bool_t gc_visit_ref(pmath_t obj, void *dummy) {
  if(pmath_is_symbol(obj) || pmath_is_expr(obj)) {
    struct _pmath_gc_t *gc_obj = (void *)PMATH_AS_PTR(obj);
    
    if((gc_obj->gc_refcount & GC_PASS_MASK) == gc_pass) {
      gc_obj->gc_refcount += GC_PASS_COUNT;
    }
    else
      gc_obj->gc_refcount = gc_pass | GC_PASS_COUNT;
  }
  return TRUE;
}

static uintptr_t get_gc_refs(pmath_t obj) {
  struct _pmath_gc_t *gc_obj;
  
  assert(pmath_is_expr(obj) || pmath_is_symbol(obj));
  
  gc_obj = (void *)PMATH_AS_PTR(obj);
  
  if((gc_obj->gc_refcount & GC_PASS_MASK) == gc_pass) {
    return gc_obj->gc_refcount >> GC_PASS_BITS;
  }
  
  return 0;
}

static pmath_bool_t gc_all_expr_visited(pmath_t obj, void *dummy) {
  if(pmath_is_expr(obj)) {
    uintptr_t gc_refs = get_gc_refs(obj);
    
    // one reference is held by caller
    ++gc_refs;
    
    return gc_refs == (uintptr_t)pmath_refcount(obj);
  }
  
  return TRUE;
}

static void run_gc(void) {
  pmath_symbol_t sym;
#ifdef PMATH_DEBUG_LOG
  double mark_start, clear_start, end;
#endif
  
  assert(!gc_is_running);
  gc_is_running = TRUE;
  
  gc_pass = (gc_pass + 1) & GC_PASS_MASK;
  
#ifdef PMATH_DEBUG_LOG
  mark_start = pmath_tickcount();
#endif
  
//#ifdef PMATH_DEBUG_LOG
//  { // (debug) check that all gc_refs are set to the previous pass
//    sym = pmath_ref(PMATH_SYMBOL_LIST);
//    do {
//      if(pmath_symbol_get_attributes(sym) & PMATH_SYMBOL_ATTRIBUTE_TEMPORARY) {
//        struct _pmath_symbol_rules_t *rules;
//        
//        _pmath_symbol_value_visit(
//          _pmath_symbol_get_global_value(sym),
//          gc_visit_check_ref,
//          NULL);
//          
//        rules = _pmath_symbol_get_rules(sym, RULES_READ);
//        
//        if(rules)
//          _pmath_symbol_rules_visit(rules, gc_visit_check_ref, NULL);
//      }
//      
//      sym = pmath_symbol_iter_next(sym);
//    } while(!pmath_is_null(sym) && !pmath_same(sym, PMATH_SYMBOL_LIST));
//    pmath_unref(sym);
//  }
//#endif
  
  // mark/reference all temp. symbol values
  sym = pmath_ref(PMATH_SYMBOL_LIST);
  do {
    if(pmath_symbol_get_attributes(sym) & PMATH_SYMBOL_ATTRIBUTE_TEMPORARY) {
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
  } while(!pmath_is_null(sym) && !pmath_same(sym, PMATH_SYMBOL_LIST));
  pmath_unref(sym);
  
#ifdef PMATH_DEBUG_LOG
  clear_start = pmath_tickcount();
#endif
  
  // clear all temp. symbols that are only referenced by temp. symbols.
  sym = pmath_ref(PMATH_SYMBOL_LIST);
  do {
    if(pmath_symbol_get_attributes(sym) & PMATH_SYMBOL_ATTRIBUTE_TEMPORARY) {
      uintptr_t gc_refs = get_gc_refs(sym);
      uintptr_t actual_refs = (uintptr_t)pmath_refcount(sym);
      
      // one reference is held by sym
      ++gc_refs;
      
      if(gc_refs < actual_refs && actual_refs <= gc_refs + 3) {
        if(_pmath_have_code(sym, PMATH_CODE_USAGE_DOWNCALL))
          ++gc_refs;
          
        if(_pmath_have_code(sym, PMATH_CODE_USAGE_UPCALL))
          ++gc_refs;
          
        if(_pmath_have_code(sym, PMATH_CODE_USAGE_SUBCALL))
          ++gc_refs;
      }
      
      if(gc_refs == actual_refs) {
        pmath_bool_t all_visited;
        
        all_visited = _pmath_symbol_value_visit(
                        _pmath_symbol_get_global_value(sym),
                        gc_all_expr_visited,
                        NULL);
                        
        if(all_visited) {
          struct _pmath_symbol_rules_t *rules;
          rules = _pmath_symbol_get_rules(sym, RULES_READ);
          
          all_visited = _pmath_symbol_rules_visit(rules, gc_all_expr_visited, NULL);
          
          /* all_visited: the whole symbol value (expr tree) is only referenced
             by temp. symbols and so was visited by the gc in the previous loop.
           */
          if(all_visited) {
#          ifdef PMATH_DEBUG_LOG
            pmath_atomic_write_release(&_pmath_debug_current_gc_symbol, (intptr_t)PMATH_AS_PTR(sym));
#          endif            
            pmath_symbol_set_attributes(sym, PMATH_SYMBOL_ATTRIBUTE_TEMPORARY);
            _pmath_symbol_set_global_value(sym, PMATH_UNDEFINED);
            
            if(rules) {
              rules = _pmath_symbol_get_rules(sym, RULES_WRITEOPTIONS);
              if(rules)
                _pmath_symbol_rules_clear(rules);
            }
            
            pmath_register_code(sym, NULL, PMATH_CODE_USAGE_DOWNCALL);
            pmath_register_code(sym, NULL, PMATH_CODE_USAGE_UPCALL);
            pmath_register_code(sym, NULL, PMATH_CODE_USAGE_SUBCALL);

#          ifdef PMATH_DEBUG_LOG
            pmath_atomic_write_release(&_pmath_debug_current_gc_symbol, 0);
#          endif            
          }
        }
      }
    }
    
    sym = pmath_symbol_iter_next(sym);
  } while(!pmath_is_null(sym) && !pmath_same(sym, PMATH_SYMBOL_LIST));
  pmath_unref(sym);
  
  gc_is_running = FALSE;
  
#ifdef PMATH_DEBUG_LOG
  end = pmath_tickcount();
  
  if(end - mark_start > 1.0) {
    pmath_debug_print("[gc %f + %f = %f secs]\n",
                      clear_start - mark_start,
                      end - clear_start,
                      end - mark_start);
  }
#endif
  
}

PMATH_PRIVATE void _pmath_register_timed_msg(struct _pmath_timed_message_t *msg) {
  if(!msg)
    return;
    
  if(!(msg->fire_at_tick < HUGE_VAL) || stop_threadpool) {
    pmath_unref(msg->message_queue);
    pmath_unref(msg->subject);
    pmath_mem_free(msg);
    return;
  }
  
  pmath_stack_push(&unsorted_msgs, msg);
  pmath_thread_wakeup(timer_thread_mq);
}

static void discard_all_timed_msgs(void) {
  struct _pmath_timed_message_t *msg;
  
  msg = pmath_stack_pop(&unsorted_msgs);
  while(msg) {
    pmath_unref(msg->message_queue);
    pmath_unref(msg->subject);
    pmath_mem_free(msg);
    
    msg = pmath_stack_pop(&unsorted_msgs);
  }
}

static void kill_timer_thread(void *dummy) {
  stop_threadpool = TRUE;
}

static void timer_thread_proc(void *dummy) {
  struct _pmath_timed_message_t *msg;
  struct _pmath_timed_message_t *umsg;
  struct _pmath_timed_message_t *sorted_msgs = NULL;
  double now;
  double next_event;
  
  //(void)pmath_atomic_fetch_add(&init_threads_counter, -1);
  
#ifdef PMATH_OS_WIN32
  SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_IDLE);
#endif
  
  now = pmath_tickcount();
  last_gc_time = now;
  next_event = last_gc_time + GC_WAIT_SEC;
  while(!stop_threadpool) {
    pmath_thread_sleep_timeout(next_event);
    
    if(stop_threadpool)
      break;
      
    umsg = pmath_stack_pop(&unsorted_msgs);
    while(umsg) {
      struct _pmath_timed_message_t **prev = &sorted_msgs;
      msg = *prev;
      while(msg && msg->fire_at_tick < umsg->fire_at_tick) {
        prev = &msg->_reserved_next;
        msg = *prev;
      }
      
      *prev = umsg;
      umsg->_reserved_next = msg;
      
      umsg = pmath_stack_pop(&unsorted_msgs);
    }
    
    now = pmath_tickcount() + 0.001;
    while(sorted_msgs) {
      msg = sorted_msgs;
      
      if(msg->fire_at_tick <= now) {
        sorted_msgs = sorted_msgs->_reserved_next;
        
        pmath_thread_send(msg->message_queue, msg->subject);
        pmath_unref(msg->message_queue);
        pmath_mem_free(msg);
      }
      else
        break;
    }
    
    if(last_gc_time + GC_WAIT_SEC <= now) {
      run_gc();
      last_gc_time = now;
    }
    
    next_event = last_gc_time + GC_WAIT_SEC;
    if(sorted_msgs && sorted_msgs->fire_at_tick < next_event)
      next_event = sorted_msgs->fire_at_tick;
  }
  
  while(sorted_msgs) {
    msg = sorted_msgs;
    sorted_msgs = sorted_msgs->_reserved_next;
    
    pmath_unref(msg->message_queue);
    pmath_unref(msg->subject);
    pmath_mem_free(msg);
  }
}

/*============================================================================*/

static int worker_count;
static int processor_count;

PMATH_PRIVATE int _pmath_processor_count(void) {
  return processor_count;
}

PMATH_PRIVATE pmath_bool_t _pmath_threadpool_init(void) {
  int i;
  
  all_daemons = NULL;
  pmath_atomic_write_release(&daemon_spin, 0);
  pmath_atomic_write_release(&_pmath_threadpool_deamon_count, 0);
  
  memset(&unsorted_msgs, 0, sizeof(unsorted_msgs));
  
  if(sem_init(&have_idle_tasks, 0, 0) != 0)
    goto WORKER_SEM_FAIL;
    
  timer_thread_mq = PMATH_NULL;
  
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
    if(f) {
      while(fgets(line, sizeof(line), f)) {
        if(strncmp(line, "processor", 9) == 0)
          ++processor_count;
      }
  
      if(processor_count < 1)
        processor_count = 1;
  
      fclose(f);
    }
  
  
    if(processor_count < 1) {
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
  
  workers = (struct worker_t *)pmath_mem_alloc(worker_count * sizeof(struct worker_t));
  pmath_atomic_write_release(&init_threads_counter, worker_count);
  if(!workers)
    goto WORKERS_ARRAY_FAIL;
    
  for(i = 0; i < worker_count; ++i) {
    if(0 != sem_init(&workers[i].finish_sem, 0, 0)) {
      int j;
      
      for(j = 0; j < i; ++j)
        sem_destroy(&workers[i].finish_sem);
        
      goto WORKERS_FAIL;
    }
  }
  
  for(i = 0; i < worker_count; ++i) {
#ifdef PMATH_OS_WIN32
    {
      HANDLE thread = (HANDLE)_beginthreadex(
        NULL,                   // default security
        0,                      // default stack size
        worker_thread_proc,
        (void *)(uintptr_t)i, // argument
        0,                      // running
        NULL);                  // do not need thread id
        
      if(thread != 0) {
        CloseHandle(thread);
        continue;
      }
    }
#else
    {
      pthread_t thread;
      int err = pthread_create(
        &thread,
        NULL,                  // default attributes
        worker_thread_proc,
        (void *)(uintptr_t)i); // argument
    
      if(!err) {
        pthread_detach(thread);
        continue;
      }
    }
#endif
    
    {
      int j;
      
      sem_destroy(&workers[i].finish_sem);
      
      stop_threadpool = TRUE;
      for(j = 0; j < i; ++j)
        sem_post(&have_idle_tasks);
        
      for(j = 0; j < i; ++j) {
        sem_wait(   &workers[j].finish_sem);
        sem_destroy(&workers[j].finish_sem);
      }
      
      goto WORKERS_FAIL;
    }
  }
  
  while(pmath_atomic_read_aquire(&init_threads_counter) > 0) {
    pmath_atomic_loop_nop();
  }
  
  timer_thread_mq = pmath_thread_fork_daemon(
                      timer_thread_proc, kill_timer_thread, NULL);
                      
  if(pmath_is_null(timer_thread_mq))
    goto TIMER_FAIL;
    
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
  for(i = 0; i < worker_count; ++i) // stop all workers
    sem_post(&have_idle_tasks);
    
  for(i = 0; i < worker_count; ++i) {
    sem_wait(   &workers[i].finish_sem);
    sem_destroy(&workers[i].finish_sem);
  }
  
WORKERS_FAIL:         pmath_mem_free(workers);
WORKERS_ARRAY_FAIL:   pmath_stack_free(idle_tasks);
IDLE_TASKS_FAIL:      sem_destroy(&have_idle_tasks);
WORKER_SEM_FAIL:

  discard_all_timed_msgs();
  
  return FALSE;
}

PMATH_PRIVATE void _pmath_threadpool_done(void) {
  int i;
  pmath_task_t task;
  
  for(i = 0; i < worker_count; ++i) // stop all workers
    sem_post(&have_idle_tasks);
    
  for(i = 0; i < worker_count; ++i) {
    sem_wait(   &workers[i].finish_sem);
    sem_destroy(&workers[i].finish_sem);
  }
  
  pmath_mem_free(workers);
  
  task = (pmath_task_t)pmath_stack_pop(idle_tasks);
  while(task) {
    assert(pmath_atomic_read_aquire(&task->refcount) == 1);
    pmath_task_unref(task);
    task = (pmath_task_t)pmath_stack_pop(idle_tasks);
  }
  pmath_stack_free(idle_tasks);
  
  sem_destroy(&have_idle_tasks);
  
  discard_all_timed_msgs();
  pmath_unref(timer_thread_mq);
}
