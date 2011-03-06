#include <pmath-util/concurrency/atomic-private.h>
#include <pmath-util/concurrency/event-private.h>
#include <pmath-util/concurrency/threadpool-private.h>
#include <pmath-util/concurrency/threads-private.h>
#include <pmath-util/debug.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/memory.h>

#include <pmath-builtins/all-symbols-private.h>

#include <errno.h>
#include <math.h>
#include <string.h>

#ifdef PMATH_OS_UNIX
  
  #include <sys/time.h>
  
#endif

struct notifier_t{
  pmath_callback_t   func;
  void              *data;
  struct notifier_t *next;
};

struct message_t{
  struct message_t *next;
  
  pmath_t          subject;
  pmath_custom_t   result; // -> _pmath_abortable_message_t
  pmath_messages_t sender;
};

struct msg_queue_t{
  struct msg_queue_t * volatile next;
  struct msg_queue_t * volatile prev;
  
  struct message_t * volatile head;
  struct message_t * volatile tail;
  
  PMATH_DECLARE_ATOMIC(head_spin);
  PMATH_DECLARE_ATOMIC(tail_spin);
  PMATH_DECLARE_ATOMIC(notifier_spin);
  
  struct notifier_t *notifiers;
  
  pmath_event_t sleep_event;
  pmath_t _sleep_result; // access with _pmath_object_atomic_[read|write]
  
  pmath_messages_t _child_messages; // access with _pmath_object_atomic_[read|write]
  
  volatile pmath_bool_t is_dead; // TRUE when thread is dead, but message queue not
};

static PMATH_DECLARE_ATOMIC(sleeplist_spin);
static struct msg_queue_t * volatile sleeplist = NULL;

#ifdef PMATH_OS_WIN32
  static uint64_t win2unix_epoch;
#endif

/*============================================================================*/

static void wakeup_msg_queue(struct msg_queue_t *mq_data){
  pmath_messages_t child_mq;
  struct msg_queue_t *prev;
  struct msg_queue_t *next;
  
  for(;;){
    pmath_atomic_lock(&mq_data->notifier_spin);
    {
      struct notifier_t *notify = mq_data->notifiers;
      
      while(notify){
        notify->func(notify->data);
        notify = notify->next;
      }
    }
    pmath_atomic_unlock(&mq_data->notifier_spin);
    
    pmath_atomic_lock(&sleeplist_spin);
    {
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
    }
    pmath_atomic_unlock(&sleeplist_spin);
    
    pmath_atomic_lock(&mq_data->notifier_spin);
    {
      struct notifier_t *notify = mq_data->notifiers;
      
      while(notify){
        notify->func(notify->data);
        notify = notify->next;
      }
    }
    pmath_atomic_unlock(&mq_data->notifier_spin);
    
    _pmath_event_signal(&mq_data->sleep_event);
    
    child_mq = _pmath_object_atomic_read(&mq_data->_child_messages);
    if(!pmath_is_null(child_mq)){
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
  
  _pmath_event_wait(&mq_data->sleep_event);
}

// may only be called from the current thread / not reentrant
static void msg_queue_sleep_timeout(struct msg_queue_t *mq_data, double abs_timeout){
  struct msg_queue_t *sl;
  struct msg_queue_t *next;
  struct msg_queue_t *prev;
  
  assert(mq_data != NULL);
  assert(!mq_data->is_dead);
  assert(pmath_custom_get_data(pmath_thread_get_current()->message_queue) == mq_data);
  
  pmath_atomic_lock(&sleeplist_spin);
  {
    sl = sleeplist;
    if(sl){
      next = sl->next;
      prev = mq_data->prev;
      
      next->prev = prev;
      prev->next = next;
      
      sl->next = mq_data;
      mq_data->prev = sl;
    }
    else{
      sleeplist = mq_data;
    }
  }
  pmath_atomic_unlock(&sleeplist_spin);
  
  _pmath_event_timedwait(&mq_data->sleep_event, abs_timeout);
  
  pmath_atomic_lock(&sleeplist_spin);
  {
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
  }
  pmath_atomic_unlock(&sleeplist_spin);
  
}

PMATH_PRIVATE
void _pmath_msq_queue_awake_all(void){
  struct msg_queue_t *sl;
  struct msg_queue_t *prev;
  struct msg_queue_t *next;
  
  for(;;){
    pmath_atomic_lock(&sleeplist_spin);
    {
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
    }
    pmath_atomic_unlock(&sleeplist_spin);
    
    if(sl){
      pmath_atomic_lock(&sl->notifier_spin);
      {
        struct notifier_t *notify = sl->notifiers;
        
        while(notify){
          notify->func(notify->data);
          notify = notify->next;
        }
      }
      pmath_atomic_unlock(&sl->notifier_spin);
    
      _pmath_event_signal(&sl->sleep_event);
    }
    else break;
  }
}

/*============================================================================*/

static void destroy_msg(struct message_t *msg){
  if(msg){
    if(!pmath_is_null(msg->result)){
      struct _pmath_abortable_message_t *data;
      
      data = pmath_custom_get_data(msg->result);
      _pmath_object_atomic_write(&data->_value, PMATH_ABORT_EXCEPTION);
      
      pmath_unref(msg->result);
    }
    
    if(!pmath_is_null(msg->sender)){
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
  assert(mq->notifiers == NULL);
  
  wakeup_msg_queue(mq);
  
  msg = mq->head;
  while(msg != mq->tail){
    next = msg->next;
    
    pmath_debug_print_object("unhandled messaged ", msg->subject, "\n");
    
    (void)pmath_atomic_fetch_add(&_pmath_abort_reasons, -1);
    destroy_msg(msg);
    
    msg = next;
  }
  destroy_msg(mq->tail);
  
  _pmath_event_destroy(&mq->sleep_event);
  _pmath_object_atomic_write(&mq->_sleep_result, PMATH_NULL);
  
  pmath_mem_free(mq);
}

static void push_msg(
  struct msg_queue_t  *mq_data,  // wont be freed
  struct message_t    *msg       // will be freed
){
  struct message_t  *tail;
  pmath_messages_t child_mq;
  
  if(!msg)
    return;
  
  msg->next = NULL;
  
  assert(mq_data != NULL);
  
  child_mq = _pmath_object_atomic_read(&mq_data->_child_messages);
  if(!pmath_is_null(child_mq)){
    struct msg_queue_t *mq_child_data = pmath_custom_get_data(child_mq);
    assert(mq_child_data != NULL);
    
    push_msg(mq_child_data, msg);
    pmath_unref(child_mq);
    return;
  }
  
  pmath_atomic_lock(&mq_data->tail_spin);
  
  if(!mq_data->is_dead){
    tail = mq_data->tail;
    tail->subject = msg->subject;
    tail->result  = msg->result;
    tail->sender  = msg->sender;
    msg->subject  = PMATH_NULL;
    msg->result   = PMATH_NULL;
    msg->sender   = PMATH_NULL;
    mq_data->tail = tail->next = msg;
    msg           = NULL;
    
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
    return PMATH_NULL;
  
  dummy_msg = pmath_mem_alloc(sizeof(struct message_t));
  if(!dummy_msg){
    pmath_mem_free(mq);
    return PMATH_NULL;
  }
  
  memset(dummy_msg, 0, sizeof(struct message_t));
  
  memset(mq, 0, sizeof(struct msg_queue_t));
  if(!_pmath_event_init(&mq->sleep_event)){
    pmath_mem_free(mq);
    pmath_mem_free(dummy_msg);
    return PMATH_NULL;
  }
  
  mq->next = mq;
  mq->prev = mq;
  mq->tail = mq->head = dummy_msg;
  
  return pmath_custom_new(mq, destroy_msg_queue);
}

PMATH_PRIVATE
void _pmath_msg_queue_inform_death(pmath_messages_t mq){
  struct msg_queue_t  *mq_data;
  
  if(!pmath_is_custom(mq)
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
      
      if(!pmath_is_null(msg->result)){
        pmath_t ex;
        pmath_t val;
        
        struct _pmath_abortable_message_t *result_data;
        result_data = pmath_custom_get_data(msg->result);
        
        // Why we use a stack of _pmath_abortable_message_t:
        // (In this diagram, A and B stand for both, the messages being sent and
        //  the according _pmath_abortable_message_t's)
        //
        //  Other Thread                   This Thread
        //  |                              |
        //  v                              |
        //  pmath_send_wait(A) ---..__     v
        //      |                     ''--->eval(A)
        //      v                              |     
        //  .---timeout ---.._                 |        
        //  v                 '-._             v        
        //  pmath_send_wait(B) ---\----------->eval(B)                         
        //                         '-._           v
        //                             '--------->Internal`AbortMessage(A)
        //
        // Internal`AbortMessage(A) sees that B is the current message and not 
        // A. So it sets B's pending_abort_request to "A". When B finishs, it 
        // effectively calls Internal`AbortMessage(B.pending_abort_request)
        // which now sees that A is the current message again and so it throws
        // "A" to abort it.
        
        pmath_debug_print("[start abortable %p", PMATH_AS_PTR(msg->result));
        pmath_debug_print_object(", subject = ", msg->subject, "]\n");
        
        assert(pmath_is_null(result_data->next));
        result_data->next = pmath_ref(me->abortable_messages);
        me->abortable_messages = msg->result;
        
        if(!pmath_is_null(result_data->next)){
          struct _pmath_abortable_message_t *next_data;
          next_data = pmath_custom_get_data(result_data->next);
          
          result_data->depth = next_data->depth + 1;
        }
        else
          result_data->depth = 0;
        
        val = pmath_evaluate(msg->subject);
        
        if(pmath_aborting()){
          pmath_unref(val);
          val = pmath_ref(PMATH_SYMBOL_ABORTED);
        }
        else if(pmath_same(val, PMATH_UNDEFINED))
          val = pmath_ref(PMATH_SYMBOL_ABORTED);
        
        
        pmath_debug_print("[ending abortable %p", PMATH_AS_PTR(msg->result));
        pmath_debug_print_object(", value = ", val, "]\n");
        
        
        me->abortable_messages = result_data->next;
        pmath_atomic_barrier();
        _pmath_object_atomic_write(&result_data->_value, val);
        
        ex = pmath_catch();
        if(pmath_same(ex, msg->result)){
          pmath_unref(ex);
        }
        else if(!pmath_same(ex, PMATH_UNDEFINED)){
          pmath_throw(ex);
        }
        
        ex = _pmath_object_atomic_read(&result_data->_pending_abort_request);
        if(!pmath_is_null(ex)){
          _pmath_abort_message(ex);
        }
        
        pmath_debug_print("[ended abortable %p]\n", PMATH_AS_PTR(msg->result));
        
        pmath_unref(msg->result);
        msg->result = PMATH_NULL;
      }
      else{
        pmath_unref(pmath_evaluate(msg->subject));
      }
      
      msg->subject = PMATH_NULL;
      
      if(!pmath_is_null(msg->sender)){
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
    _pmath_object_atomic_write(&mq_data->_child_messages, PMATH_NULL);
  
}

/*============================================================================*/

PMATH_API
PMATH_ATTRIBUTE_PURE
pmath_bool_t pmath_is_message_queue(pmath_t obj){
  return pmath_is_custom(obj)
      && pmath_custom_has_destructor(obj, destroy_msg_queue);
}

PMATH_API
PMATH_ATTRIBUTE_USE_RESULT
pmath_messages_t pmath_thread_get_queue(void){
  pmath_thread_t me = pmath_thread_get_current();
  
  if(!me)
    return PMATH_NULL;
  
  return pmath_ref(me->message_queue);
}

PMATH_API
void pmath_thread_sleep(void){
  struct msg_queue_t *mq_data;
  pmath_thread_t me = pmath_thread_get_current();
  
  if(!me || pmath_is_null(me->message_queue))
    return;
  
  mq_data = pmath_custom_get_data(me->message_queue);
  assert(mq_data != NULL);
  msg_queue_sleep(mq_data);
  
  _pmath_msq_queue_handle_next(me);
}

PMATH_API
void pmath_thread_sleep_timeout(double abs_timeout){
  struct msg_queue_t *mq_data;
  pmath_thread_t me = pmath_thread_get_current();
  
  if(!me || pmath_is_null(me->message_queue))
    return;
  
  mq_data = pmath_custom_get_data(me->message_queue);
  assert(mq_data != NULL);
  msg_queue_sleep_timeout(mq_data, abs_timeout);
  
  _pmath_msq_queue_handle_next(me);
}

PMATH_API
double pmath_tickcount(void){
  #ifdef PMATH_OS_WIN32
  {
    uint64_t ft;
    GetSystemTimeAsFileTime((FILETIME*)&ft);
    return (ft - win2unix_epoch) * 1e-7;
  }
  #else
  {
    struct timeval tv;
    gettimeofday(&tv, NULL); // too slow?
    return (double)tv.tv_sec + tv.tv_usec * 1e-6;
  }
  #endif
}

PMATH_API
void pmath_thread_wakeup(pmath_messages_t mq){
  if(pmath_is_custom(mq)
  && pmath_custom_has_destructor(mq, destroy_msg_queue)){
    wakeup_msg_queue(pmath_custom_get_data(mq));
  }
}

PMATH_API
void pmath_thread_send(pmath_messages_t mq, pmath_t msg){
  struct msg_queue_t  *mq_data;
  struct message_t    *msg_struct;
  
  if(pmath_is_custom(mq)
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
  pmath_t          msg,
  double           timeout_seconds,
  void           (*idle_function)(void*),
  void            *idle_data
){
  struct msg_queue_t                *my_mq_data;
  struct msg_queue_t                *mq_data;
  struct message_t                  *msg_struct;
  pmath_thread_t                     me;
  pmath_t                            answer;
  pmath_t                            interrupt;
  pmath_custom_t                     result;
  struct _pmath_abortable_message_t *result_data;
  double                             end_time;
  
  answer = PMATH_UNDEFINED;
  me = pmath_thread_get_current();
  if(!me || pmath_thread_aborting(me)){
    pmath_unref(msg);
    return answer;
  }
  
  if(pmath_is_custom(mq)
  && pmath_custom_has_destructor(mq, destroy_msg_queue)){
    mq_data = pmath_custom_get_data(mq);
    assert(mq_data != NULL);
    
    msg_struct = pmath_mem_alloc(sizeof(struct message_t));
    if(!msg_struct){
      pmath_unref(msg);
      return answer;
    }
    
    result_data = pmath_mem_alloc(sizeof(struct _pmath_abortable_message_t));
    if(!result_data){
      pmath_mem_free(msg_struct);
      pmath_unref(msg);
      return answer;
    }
    
    memset(result_data, 0, sizeof(struct _pmath_abortable_message_t));
    
    result = pmath_custom_new(result_data, _pmath_destroy_abortable_message);
    interrupt = pmath_expr_new_extended(
      pmath_ref(PMATH_SYMBOL_INTERNAL_ABORTMESSAGE), 1,
      pmath_ref(result));
    if(pmath_is_null(result) || pmath_is_null(interrupt)){
      pmath_mem_free(msg_struct);
      pmath_unref(msg);
      pmath_unref(result);
      pmath_unref(interrupt);
      return answer;
    }
    
    _pmath_object_atomic_write(&result_data->_value, PMATH_UNDEFINED);
    
    end_time = pmath_tickcount() + timeout_seconds;
    
    memset(msg_struct, 0, sizeof(struct message_t));
    msg_struct->subject = msg;
    msg_struct->result = pmath_ref(result);
    msg_struct->sender = pmath_ref(me->message_queue);
    push_msg(mq_data, msg_struct);
    
    my_mq_data = pmath_custom_get_data(me->message_queue);
    assert(my_mq_data != NULL);
    
    while(!pmath_thread_aborting(me) && pmath_tickcount() < end_time){
      msg_queue_sleep_timeout(my_mq_data, end_time);
      
      pmath_unref(answer);
      answer = _pmath_object_atomic_read(&result_data->_value);
      
      if(!pmath_same(answer, PMATH_UNDEFINED))
        goto SUCCESS;
      
      if(idle_function)
        idle_function(idle_data);
    }
    
    if(pmath_tickcount() >= end_time)
      pmath_debug_print("[timeout %f %p]\n", timeout_seconds, PMATH_AS_PTR(result));
    
    pmath_thread_send(mq, pmath_ref(interrupt));
    
    pmath_unref(answer);
    answer = PMATH_UNDEFINED;
    
   SUCCESS:
    pmath_unref(interrupt);
    pmath_unref(result);
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
  
  if(!pmath_is_custom(mq)
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
  timed_msg->absolute_time = pmath_tickcount() + seconds;
  
  _pmath_register_timed_msg(timed_msg);
}

PMATH_API 
pmath_bool_t pmath_thread_queue_is_blocked_by(
  pmath_messages_t waiter_mq, // will be freed
  pmath_messages_t waitee_mq  // will be freed
){
  pmath_t child_mq;
  struct msg_queue_t *waiter_mq_data;
  
  while(!pmath_is_null(waiter_mq) && !pmath_same(waiter_mq, waitee_mq)){
    waiter_mq_data = pmath_custom_get_data(waiter_mq);
    assert(waiter_mq_data != NULL);
    
    child_mq = _pmath_object_atomic_read(&waiter_mq_data->_child_messages);
    pmath_unref(waiter_mq);
    
    if(pmath_is_null(child_mq)){
      pmath_unref(waitee_mq);
      return FALSE;
    }
    
    waiter_mq = child_mq;
  }
  
  pmath_unref(waiter_mq);
  pmath_unref(waitee_mq);
  return TRUE;
}

/*============================================================================*/

PMATH_API
void pmath_thread_run_with_interrupt_notifier(
  pmath_callback_t   callback,
  pmath_callback_t   notify,
  void              *callback_closure,
  void              *notify_closure
){
  pmath_messages_t mq = pmath_thread_get_queue();
  
  assert(callback != NULL);
  assert(notify   != NULL);
  
  if(!pmath_is_null(mq)){
    struct notifier_t notifier;
    struct msg_queue_t *mq_data;
    
    mq_data = pmath_custom_get_data(mq);
    assert(mq_data != NULL);
    
    notifier.func = notify;
    notifier.data = notify_closure;
    
    pmath_atomic_lock(&mq_data->notifier_spin);
    {
      notifier.next = mq_data->notifiers;
      mq_data->notifiers = &notifier;
    }
    pmath_atomic_unlock(&mq_data->notifier_spin);
    
    callback(callback_closure);
    
    pmath_atomic_lock(&mq_data->notifier_spin);
    {
      assert(mq_data->notifiers == &notifier);
      mq_data->notifiers = notifier.next;
    }
    pmath_atomic_unlock(&mq_data->notifier_spin);
    
    pmath_unref(mq);
  }
  else
    callback(callback_closure);
}
  
/*============================================================================*/

PMATH_PRIVATE pmath_bool_t _pmath_threadmsg_init(void){
  #ifdef PMATH_OS_WIN32
  {
    SYSTEMTIME unix_epoch;
    memset(&unix_epoch, 0, sizeof(unix_epoch));
    unix_epoch.wYear  = 1970;
    unix_epoch.wMonth = 1;
    unix_epoch.wDay   = 1;
    
    win2unix_epoch = 0;
    SystemTimeToFileTime(&unix_epoch, (FILETIME*)&win2unix_epoch);
  }
  #endif
  
  return TRUE;
}

PMATH_PRIVATE void _pmath_threadmsg_done(void){
  
}

