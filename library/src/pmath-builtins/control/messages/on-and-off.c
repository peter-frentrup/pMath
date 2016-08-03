#include <pmath-util/concurrency/threads.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/control/messages-private.h>


// message will be freed.
static void set_message_on_off(pmath_t message, pmath_bool_t on) {
  pmath_t head;
  pmath_bool_t default_off = _pmath_message_is_default_off(message);
  
  if(on) {
    if(default_off)
      head = pmath_ref(PMATH_SYMBOL_ON);
    else
      head = PMATH_UNDEFINED;
  }
  else {
    if(default_off)
      head = PMATH_UNDEFINED;
    else
      head = pmath_ref(PMATH_SYMBOL_OFF);
  }
  
  pmath_unref(
    pmath_thread_local_save(
      message,
      head));
      
  pmath_unref(message);
}

// messages will be freed.
static void set_all_messages_on_off(pmath_expr_t messages, pmath_bool_t on) {
  size_t len = pmath_expr_length(messages);
  size_t i;
  
  for(i = 1; i <= len; ++i) {
    pmath_t msg;
    if(pmath_aborting())
      break;
    
    msg = pmath_expr_get_item(messages, i);
    
    if(pmath_is_symbol(msg)) {
      msg = pmath_expr_new_extended(
              pmath_ref(PMATH_SYMBOL_MESSAGENAME), 2,
              msg,
              PMATH_NULL);
    }
    
    if(pmath_is_string(msg)) {
      // msg = Replace(Hold(msg), $MessageGroups, 1)
      msg = pmath_evaluate(
        pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_REPLACE), 3,
          pmath_expr_new_extended(
            pmath_ref(PMATH_SYMBOL_HOLD), 1,
            msg),
          pmath_ref(PMATH_SYMBOL_MESSAGEGROUPS),
          PMATH_FROM_INT32(1)));
        
      if(pmath_is_expr_of(msg, PMATH_SYMBOL_HOLD))
        msg = pmath_expr_set_item(msg, 0, pmath_ref(PMATH_SYMBOL_LIST));
    }
    
    if(pmath_is_expr_of(msg, PMATH_SYMBOL_LIST)) {
      set_all_messages_on_off(msg, on);
      continue;
    }
    
    if(_pmath_is_valid_messagename(msg)) {
      set_message_on_off(msg, on);
      continue;
    }
    
    pmath_message(PMATH_SYMBOL_MESSAGE, "name", 1, msg);
  }
  
  pmath_unref(messages);
}

PMATH_PRIVATE pmath_t builtin_on_or_off(pmath_expr_t expr) {
  /* On( sym1::tag1, sym2::tag2, ...)
     Off(sym1::tag1, sym2::tag2, ...)
   */
  size_t len;
  
  len = pmath_expr_length(expr);
  if(len < 1) {
    pmath_message_argxxx(len, 1, SIZE_MAX);
    return expr;
  }
  
  set_all_messages_on_off(expr, pmath_is_expr_of(expr, PMATH_SYMBOL_ON));
  return PMATH_NULL;
}
