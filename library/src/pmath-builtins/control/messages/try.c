#include <pmath-core/numbers.h>

#include <pmath-language/patterns-private.h>

#include <pmath-util/concurrency/atomic.h>
#include <pmath-util/concurrency/threads-private.h>
#include <pmath-util/debug.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/control/messages-private.h>


extern pmath_symbol_t pmath_System_DollarFailed;
extern pmath_symbol_t pmath_System_Assign;
extern pmath_symbol_t pmath_System_AssignDelayed;
extern pmath_symbol_t pmath_System_DownRules;
extern pmath_symbol_t pmath_System_General;
extern pmath_symbol_t pmath_System_HoldPattern;
extern pmath_symbol_t pmath_System_List;
extern pmath_symbol_t pmath_System_MessageName;
extern pmath_symbol_t pmath_Internal_CriticalMessageTag;
extern pmath_symbol_t pmath_Internal_MessageThrown;

static void make_critical_message(pmath_t msg, pmath_t tag) { // msg will be freed, tag wont
  if(pmath_is_symbol(msg)) {
    msg = pmath_expr_new_extended(
            pmath_ref(pmath_System_MessageName), 2,
            msg,
            pmath_ref(_pmath_object_singlematch));
  }
  else {
    if(pmath_expr_item_equals(msg, 1, pmath_System_General))
      msg = pmath_expr_set_item(msg, 1, pmath_ref(_pmath_object_singlematch));
  }
  
  // Internal`CriticalMessageTag(HoldPattern(msg)):= tag
  msg = pmath_expr_new_extended(
          pmath_ref(pmath_System_HoldPattern), 1,
          msg);
          
  msg = pmath_expr_new_extended(
          pmath_ref(pmath_Internal_CriticalMessageTag), 1,
          msg);
          
  msg = pmath_expr_new_extended(
          pmath_ref(pmath_System_AssignDelayed), 2,
          msg,
          pmath_ref(tag));
          
  pmath_unref(pmath_evaluate(msg));
}

static void make_all_messages_critical(pmath_t messages, pmath_t tag) { // messages will be freed, tag wont
  if(pmath_is_symbol(messages) || _pmath_is_valid_messagename(messages)) {
    make_critical_message(messages, tag);
    return;
  }
  
  if(pmath_is_expr_of(messages, pmath_System_List)) {
    size_t i;
    size_t len = pmath_expr_length(messages);
    
    for(i = 1; i <= len; ++i) {
      pmath_t msg = pmath_expr_extract_item(messages, i);
      make_all_messages_critical(msg, tag);
    }
  }
  
  pmath_unref(messages);
}

static pmath_t check_messages(pmath_t messages) { // messages will be freed
  if(pmath_is_symbol(messages))
    return messages;
    
  if(pmath_is_string(messages))
    messages = _pmath_messages_in_group(messages);
    
  if(_pmath_is_valid_messagename(messages))
    return messages;
    
  if(pmath_is_expr_of(messages, pmath_System_List)) {
    size_t i;
    size_t len = pmath_expr_length(messages);
    
    for(i = 1; i <= len; ++i) {
      pmath_t msg = pmath_expr_extract_item(messages, i);
      msg = check_messages(msg);
      
      if(pmath_same(msg, PMATH_UNDEFINED)) {
        pmath_unref(messages);
        return PMATH_UNDEFINED;
      }
      
      messages = pmath_expr_set_item(messages, i, msg);
    }
    
    return messages;
  }
  
  pmath_unref(messages);
  return PMATH_UNDEFINED;
}

static pmath_atomic_t message_tag_value = PMATH_ATOMIC_STATIC_INIT;
static pmath_t generate_message_tag(void) {
  intptr_t val = pmath_atomic_fetch_add(&message_tag_value, 1);
  
  return pmath_expr_new_extended(
           pmath_ref(pmath_Internal_MessageThrown), 1,
           pmath_integer_new_siptr(val));
}

PMATH_PRIVATE pmath_t builtin_try(pmath_expr_t expr) {
  /* Try(body, failexpr, messages)
     Try(body, failexpr)
     Try(body)
   */
  pmath_t         body;
  pmath_t         failexpr;
  pmath_t         messages;
  pmath_t         old_downrules;
  pmath_t         exception;
  pmath_t         tag;
  pmath_thread_t  thread;
  uint8_t         old_critical_messages;
  intptr_t        old_dynamic_id;
  size_t          exprlen;
  
  exprlen = pmath_expr_length(expr);
  
  if(exprlen < 1 || exprlen > 3) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 3);
    return expr;
  }
  
  thread = pmath_thread_get_current();
  
  if(!thread)
    return expr;
    
  if(exprlen == 3) {
    messages = pmath_expr_get_item(expr, 3);
    
    messages = check_messages(messages);
    if(pmath_same(messages, PMATH_UNDEFINED)) {
      pmath_message(PMATH_NULL, "nomsgs", 1, pmath_expr_get_item(expr, 3));
      return expr;
    }
    
    failexpr = pmath_expr_get_item(expr, 2);
  }
  else if(exprlen == 2) {
    messages = PMATH_NULL;
    failexpr = pmath_expr_get_item(expr, 2);
  }
  else {
    messages = PMATH_NULL;
    failexpr = pmath_ref(pmath_System_DollarFailed);
  }
  
  body = pmath_expr_get_item(expr, 1);
#ifdef NDEBUG
  pmath_unref(expr); expr = PMATH_NULL;
#endif

  tag = generate_message_tag();
  
  old_dynamic_id = thread->current_dynamic_id;
  thread->current_dynamic_id = 0;

  old_downrules = pmath_evaluate(
                    pmath_expr_new_extended(
                      pmath_ref(pmath_System_DownRules), 1,
                      pmath_ref(pmath_Internal_CriticalMessageTag)));
                      
  if(exprlen == 3) {
    make_all_messages_critical(messages, tag);
    messages = PMATH_NULL;
  }
  else {
    // Internal`CriticalMessageTag(~):= tag
    pmath_t tmp;
    
    tmp = pmath_expr_new_extended(
            pmath_ref(pmath_Internal_CriticalMessageTag), 1,
            pmath_ref(_pmath_object_singlematch));
            
    tmp = pmath_expr_new_extended(
            pmath_ref(pmath_System_Assign), 2,
            tmp,
            pmath_ref(tag));
            
    pmath_unref(pmath_evaluate(tmp));
  }
  thread->current_dynamic_id = old_dynamic_id;

  old_critical_messages = thread->critical_messages;
  thread->critical_messages = TRUE;
  
  body = pmath_evaluate(body);
  
  exception = _pmath_thread_catch(thread);
  if(!pmath_same(exception, PMATH_UNDEFINED)) {
    if(pmath_equals(exception, tag)) {
      pmath_debug_print_object("[stopped ", expr, "]\n");
      pmath_unref(body);
      pmath_unref(exception);
      body = failexpr;
      failexpr = PMATH_NULL;
      exception = PMATH_UNDEFINED;
    }
    else {
      pmath_debug_print_object("[throw through ", expr, "]\n");
    }
  }
  
  thread->critical_messages = old_critical_messages;
  
  pmath_unref(messages);
  pmath_unref(failexpr);
  pmath_unref(tag);
  
  thread->current_dynamic_id = 0;
  { // DownRules(Internal`CriticalMessageTag):= old_downrules
    pmath_t tmp;
    
    tmp = pmath_expr_new_extended(
            pmath_ref(pmath_System_DownRules), 1,
            pmath_ref(pmath_Internal_CriticalMessageTag));
            
    tmp = pmath_expr_new_extended(
            pmath_ref(pmath_System_Assign), 2,
            tmp,
            old_downrules);
            
    pmath_unref(pmath_evaluate(tmp));
  }
  thread->current_dynamic_id = old_dynamic_id;
  
  if(!pmath_same(exception, PMATH_UNDEFINED))
    _pmath_thread_throw(thread, exception);
    
  pmath_unref(expr);
  return body;
}
