#include <pmath-util/concurrency/atomic-private.h>
#include <pmath-util/concurrency/threadpool-private.h>
#include <pmath-util/concurrency/threads-private.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/memory.h>

#include <pmath-builtins/all-symbols-private.h>

#include <errno.h>
#include <math.h>

/* TODO: use auto-reset events instead of semaphores?
 */
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
  #include <unistd.h>
  #include <semaphore.h>
  #include <time.h>
#endif

struct message_t{
  struct message_t *next;
  
  pmath_t   subject;
  pmath_symbol_t   result_symbol;
  pmath_messages_t sender;
};

struct msg_queue_t{
  struct msg_queue_t * volatile next;
  struct msg_queue_t * volatile prev;
  
  struct message_t * volatile head;
  struct message_t * volatile tail;
  
  PMATH_DECLARE_ATOMIC(head_spin);
  PMATH_DECLARE_ATOMIC(tail_spin);
  
  sem_t sleep_sem;
  pmath_t _sleep_result; // access with _pmath_object_atomic_[read|write]
  
  pmath_messages_t _child_messages; // access with _pmath_object_atomic_[read|write]
  
  volatile pmath_bool_t is_dead; // TRUE when thread is dead, but message queue not
};

static PMATH_DECLARE_ATOMIC(sleeplist_spin);
static struct msg_queue_t * volatile sleeplist = NULL;

/*============================================================================*/

static void wakeup_msg_queue(struct msg_queue_t *mq_data){
  pmath_messages_t child_mq;
  struct msg_queue_t *prev;
  struct msg_queue_t *next;
  
  for(;;){
    pmath_atomic_lock(&sleeplist_spin);
    
    prev = mq_data->prev;
    next = mq_data->next;
    prev->next = next;
    next->prev = prev;
    if(mq_data == sleeplist){
      if(mq_data->next == mq_data)
        sleeplist = NULL;
      else
        sleeplist = mq_data->next;
    }
    
    mq_data->prev = mq_data;
    mq_data->next = mq_data;
    
    pmath_atomic_unlock(&sleeplist_spin);
    
    pmath_atomic_barrier();
    sem_post(&mq_data->sleep_sem);
    
    child_mq = _pmath_object_atomic_read(&mq_data->_child_messages);
    if(child_mq){
      mq_data = pmath_custom_get_data(child_mq);
      pmath_unref(child_mq);
    }
    else
      break;
  }
}

// may only be called from the current thread / not reentrant
static void msg_queue_sleep(struct msg_queue_t *mq_data){
  struct msg_queue_t *sl;
  struct msg_queue_t *sl_next;
  struct msg_queue_t *mq_prev;
  
  assert(mq_data != NULL);
  assert(!mq_data->is_dead);
  assert(pmath_custom_get_data(pmath_thread_get_current()->message_queue) == mq_data);
  
  pmath_atomic_lock(&sleeplist_spin);
  
  sl = sleeplist;
  if(sl){
    sl_next = sl->next;
    mq_prev = mq_data->prev;
    
    sl_next->prev = mq_prev;
    mq_prev->next = sl_next;
    
    sl->next = mq_data;
    mq_data->prev = sl;
  }
  else{
    sleeplist = mq_data;
  }
  
  pmath_atomic_unlock(&sleeplist_spin);
  
  while(sem_wait(&mq_data->sleep_sem) == -1 && errno == EINTR)
    continue;
}

PMATH_PRIVATE
void _pmath_msq_queue_awake_all(void){
  struct msg_queue_t *sl;
  struct msg_queue_t *prev;
  struct msg_queue_t *next;
  
  for(;;){
    pmath_atomic_lock(&sleeplist_spin);
    
    sl = sleeplist;
    if(sl){
      prev = sl->prev;
      next = sl->next;
      prev->next = next;
      next->prev = prev;
      if(sl->next == sl)
        sleeplist = NULL;
      else
        sleeplist = sl->next;
      
      sl->prev = sl;
      sl->next = sl;
    }
    
    pmath_atomic_unlock(&sleeplist_spin);
    
    if(sl){
      sem_post(&sl->sleep_sem);
    }
    else break;
  }
}

/*============================================================================*/

static void destroy_msg(struct message_t *msg){
  if(msg){
    if(msg->result_symbol){
      pmath_symbol_set_value(msg->result_symbol, PMATH_UNDEFINED);
      pmath_unref(msg->result_symbol);
    }
    
    if(msg->sender){
      wakeup_msg_queue(pmath_custom_get_data(msg->sender));
      pmath_unref(msg->sender);
    }
    
    pmath_unref(msg->subject);
    pmath_mem_free(msg);
  }
}

static void destroy_msg_queue(void *mq_data){
  struct message_t   *msg;
  struct message_t   *next;
  struct msg_queue_t *mq;
  
  mq = (struct msg_queue_t*)mq_data;
  
  assert(mq != NULL);
  
  wakeup_msg_queue(mq);
  
  msg = mq->head;
  while(msg != mq->tail){
    next = msg->next;
    
    (void)pmath_atomic_fetch_add(&_pmath_abort_reasons, -1);
    destroy_msg(msg);
    
    msg = next;
  }
  destroy_msg(mq->tail);
  
  sem_destroy(&mq->sleep_sem);
  _pmath_object_atomic_write(&mq->_sleep_result, NULL);
  
  pmath_mem_free(mq);
}

static void push_msg(
  struct msg_queue_t  *mq_data,  // wont be freed
  struct message_t    *msg       // will be freed
){
  struct message_t  *tail;
  
  if(!msg)
    return;
  
  msg->next = NULL;
  
  assert(mq_data != NULL);
  
  pmath_atomic_lock(&mq_data->tail_spin);
  
  if(!mq_data->is_dead){
    tail = mq_data->tail;
    tail->subject       = msg->subject;
    tail->result_symbol = msg->result_symbol;
    tail->sender        = msg->sender;
    msg->subject       = NULL;
    msg->result_symbol = NULL;
    msg->sender        = NULL;
    mq_data->tail = tail->next = msg;
    msg = NULL;
    
    (void)pmath_atomic_fetch_add(&_pmath_abort_reasons, +1);
  }
  
  pmath_atomic_unlock(&mq_data->tail_spin);
  
  if(!msg){
    wakeup_msg_queue(mq_data); // todo: wake up child threads!!!
  }
  else
    destroy_msg(msg);
}

static struct message_t *pop_msg(struct msg_queue_t *mq_data){
  struct message_t *msg;
  
  if(!mq_data)
    return NULL;
    
  msg = NULL;
  
  pmath_atomic_lock(&mq_data->head_spin);
  
  msg = mq_data->head;
  if(msg != mq_data->tail)
    mq_data->head = msg->next;
  else
    msg = NULL;
  
  pmath_atomic_unlock(&mq_data->head_spin);
  
  if(msg)
    (void)pmath_atomic_fetch_add(&_pmath_abort_reasons, -1);
  
  return msg;
}

/*============================================================================*/

PMATH_PRIVATE
pmath_messages_t _pmath_msg_queue_create(void){
  struct message_t    *dummy_msg;
  struct msg_queue_t  *mq;
  
  mq = pmath_mem_alloc(sizeof(struct msg_queue_t));
  if(!mq)
    return NULL;
  
  dummy_msg = pmath_mem_alloc(sizeof(struct message_t));
  if(!dummy_msg){
    pmath_mem_free(mq);
    return NULL;
  }
  
  memset(dummy_msg, 0, sizeof(struct message_t));
  
  memset(mq, 0, sizeof(struct msg_queue_t));
  if(sem_init(&mq->sleep_sem, 0, 0) != 0){
    pmath_mem_free(mq);
    pmath_mem_free(dummy_msg);
    return NULL;
  }
  
  mq->next = mq;
  mq->prev = mq;
  mq->tail = mq->head = dummy_msg;
  
  return pmath_custom_new(mq, destroy_msg_queue);
}

PMATH_PRIVATE
void _pmath_msg_queue_inform_death(pmath_messages_t mq){
  struct msg_queue_t  *mq_data;
  
  if(!pmath_instance_of(mq, PMATH_TYPE_CUSTOM)
  || pmath_custom_has_destructor(mq, destroy_msg_queue))
    return;
  
  mq_data = pmath_custom_get_data(mq);
  assert(mq_data != NULL);
  
  pmath_atomic_lock(&mq_data->tail_spin);
  mq_data->is_dead = TRUE;
  pmath_atomic_unlock(&mq_data->tail_spin);
}

PMATH_PRIVATE
void _pmath_msq_queue_handle_next(pmath_thread_t me){
  struct message_t *msg;
  
  if(!me)
    me = pmath_thread_get_current();
  
  while(me){
    msg = pop_msg(pmath_custom_get_data(me->message_queue));
    if(msg){
      
      if(msg->result_symbol){
        pmath_t ex;
        
        pmath_symbol_set_value(msg->result_symbol, pmath_evaluate(msg->subject));
        
        ex = pmath_catch();
        if(ex == msg->result_symbol){
          pmath_unref(ex);
        }
        else if(ex != PMATH_UNDEFINED)
          pmath_throw(ex);
        
        pmath_unref(msg->result_symbol);
      }
      else{
        pmath_unref(pmath_evaluate(msg->subject));
      }
      
      msg->subject = NULL;
      
      if(msg->sender){
        wakeup_msg_queue(pmath_custom_get_data(msg->sender));
        pmath_unref(msg->sender);
      }
      
      pmath_mem_free(msg);
      return;
    }
    
    me = me->parent;
  }
}

PMATH_PRIVATE
void _pmath_msq_queue_set_child(
  struct _pmath_thread_t *me, 
  struct _pmath_thread_t *child
){
  struct msg_queue_t  *mq_data;
  
  if(!me)
    return;
  
  mq_data = pmath_custom_get_data(me->message_queue);
  
  assert(mq_data != NULL);
  
  if(child)
    _pmath_object_atomic_write(&mq_data->_child_messages, pmath_ref(child->message_queue));
  else
    _pmath_object_atomic_write(&mq_data->_child_messages, NULL);
  
}

/*============================================================================*/

PMATH_API
PMATH_ATTRIBUTE_PURE
pmath_bool_t pmath_is_message_queue(pmath_t obj){
  return pmath_instance_of(obj, PMATH_TYPE_CUSTOM)
      && pmath_custom_has_destructor(obj, destroy_msg_queue);
}

PMATH_API
PMATH_ATTRIBUTE_USE_RESULT
pmath_messages_t pmath_thread_get_queue(void){
  pmath_thread_t me = pmath_thread_get_current();
  
  if(!me)
    return NULL;
  
  return pmath_ref(me->message_queue);
}

PMATH_API
void pmath_thread_sleep(void){
  struct msg_queue_t *mq_data;
  pmath_thread_t me = pmath_thread_get_current();
  
  if(!me || !me->message_queue)
    return;
  
  mq_data = pmath_custom_get_data(me->message_queue);
  assert(mq_data != NULL);
  msg_queue_sleep(mq_data);
  
  _pmath_msq_queue_handle_next(me);
}

PMATH_API
void pmath_thread_wakeup(pmath_messages_t mq){
  if(pmath_instance_of(mq, PMATH_TYPE_CUSTOM)
  && pmath_custom_has_destructor(mq, destroy_msg_queue)){
    wakeup_msg_queue(pmath_custom_get_data(mq));
  }
}

PMATH_API
void pmath_thread_send(pmath_messages_t mq, pmath_t msg){
  struct msg_queue_t  *mq_data;
  struct message_t    *msg_struct;
  
  if(pmath_instance_of(mq, PMATH_TYPE_CUSTOM)
  && pmath_custom_has_destructor(mq, destroy_msg_queue)){
    mq_data = pmath_custom_get_data(mq);
    assert(mq_data != NULL);
    
    msg_struct = pmath_mem_alloc(sizeof(struct message_t));
    if(!msg_struct){
      pmath_unref(msg);
      return;
    }
    
    memset(msg_struct, 0, sizeof(struct message_t));
    msg_struct->subject = msg;
    push_msg(mq_data, msg_struct);
  }
  else
    pmath_unref(msg);
}

PMATH_API
pmath_t pmath_thread_send_wait(
  pmath_messages_t mq, 
  pmath_t   msg,
  double           timeout_seconds
){
  struct msg_queue_t  *my_mq_data;
  struct msg_queue_t  *mq_data;
  struct message_t    *msg_struct;
  pmath_thread_t       me;
  pmath_t       answer;
  pmath_t       interrupt;
  pmath_symbol_t       result_symbol;
  pmath_symbol_t       timeout_guard;
  
  answer = PMATH_UNDEFINED;
  me = pmath_thread_get_current();;
  if(!me || pmath_thread_aborting(me)){
    pmath_unref(msg);
    return answer;
  }
  
  if(pmath_instance_of(mq, PMATH_TYPE_CUSTOM)
  && pmath_custom_has_destructor(mq, destroy_msg_queue)){
    mq_data = pmath_custom_get_data(mq);
    assert(mq_data != NULL);
    
    msg_struct = pmath_mem_alloc(sizeof(struct message_t));
    if(!msg_struct){
      pmath_unref(msg);
      return answer;
    }
    
    result_symbol = pmath_symbol_create_temporary(PMATH_C_STRING("Internal`sendWait"), TRUE);
    
    if(timeout_seconds > 0 && timeout_seconds < HUGE_VAL){
      timeout_guard = pmath_symbol_create_temporary(
        PMATH_C_STRING("System`TimeConstrained`stop"), TRUE);
      
      pmath_symbol_set_value(timeout_guard, 
        pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_THROW), 1,
          pmath_expr_new_extended(
            pmath_ref(PMATH_SYMBOL_UNEVALUATED), 1,
            pmath_ref(timeout_guard))));
      
      if(timeout_guard){
        pmath_thread_send_delayed(
          me->message_queue, 
          pmath_ref(timeout_guard), 
          timeout_seconds);
      }
    }
    else
      timeout_guard = NULL;
    
    interrupt = pmath_expr_new_extended(
      pmath_ref(PMATH_SYMBOL_THROW), 1,
      pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_UNEVALUATED), 1,
        pmath_ref(result_symbol)));
        
    pmath_symbol_set_value(result_symbol, pmath_ref(interrupt));
    
    memset(msg_struct, 0, sizeof(struct message_t));
    msg_struct->subject = msg;
    msg_struct->result_symbol = pmath_ref(result_symbol);
    msg_struct->sender = pmath_ref(me->message_queue);
    push_msg(mq_data, msg_struct);
    
    my_mq_data = pmath_custom_get_data(me->message_queue);
    assert(my_mq_data != NULL);
    
    while(!pmath_thread_aborting(me)){
      msg_queue_sleep(my_mq_data);
      
      pmath_unref(answer);
      answer = pmath_symbol_get_value(result_symbol);
      
      if(!pmath_equals(answer, interrupt))
        break;
    }
    
    if(pmath_thread_aborting(me)){
      /* If the evaluation did not already end, result_symbol still holds 
         `interrupt` which is Throw(...). Sending result_symbol then causes the
         evalutation to stop.
         
         If the evaluation already finished when this `result_symbol` message
         is handled, result_symbol will containt the evaluations result and thus 
         will cause no harm.
       */
      pmath_thread_send(mq, pmath_ref(result_symbol));
    }
    
    if(pmath_equals(answer, interrupt)){
      pmath_unref(answer);
      answer = PMATH_UNDEFINED;
    }
    
    pmath_unref(interrupt);
    
    if(timeout_guard){
      pmath_symbol_set_value(timeout_guard, PMATH_UNDEFINED);
      
      interrupt = pmath_catch();
      if(interrupt == timeout_guard){
        pmath_unref(interrupt);
        pmath_unref(answer);
        answer = PMATH_UNDEFINED;
      }
      else if(interrupt != PMATH_UNDEFINED)
        pmath_throw(interrupt);
      
      pmath_unref(timeout_guard);
    }
    
    pmath_unref(result_symbol);
  }
  else
    pmath_unref(msg);
  
  return answer;
}

PMATH_API
void pmath_thread_send_delayed(
  pmath_messages_t mq, 
  pmath_t          msg,
  double           seconds
){
  struct _pmath_timed_message_t *timed_msg;
  
  if(!pmath_instance_of(mq, PMATH_TYPE_CUSTOM)
  || !pmath_custom_has_destructor(mq, destroy_msg_queue)){
    pmath_unref(msg);
    return;
  }
  
  timed_msg = pmath_mem_alloc(sizeof(struct _pmath_timed_message_t));
  if(!timed_msg){
    pmath_unref(msg);
    return;
  }
  
  memset(timed_msg, 0, sizeof(struct _pmath_timed_message_t));
  timed_msg->message_queue = pmath_ref(mq);
  timed_msg->subject = msg;
  timed_msg->absolute_time = _pmath_tickcount() + seconds;
  
  _pmath_register_timed_msg(timed_msg);
}
