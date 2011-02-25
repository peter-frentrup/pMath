#include <pmath-util/concurrency/threads.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/control/messages-private.h>

PMATH_PRIVATE pmath_t builtin_on_or_off(pmath_expr_t expr){
  /* On( sym1::tag1, sym2::tag2, ...)
     Off(sym1::tag1, sym2::tag2, ...)
   */
  pmath_t head;//, on_off;
  size_t i, len;

  len = pmath_expr_length(expr);
  if(len < 1){
    pmath_message_argxxx(len, 1, SIZE_MAX);
    return expr;
  }

  head = pmath_expr_get_item(expr, 0);
  
  for(i = 1;i <= len;++i){
    pmath_t message = pmath_expr_get_item(expr, i);
    
    if(pmath_is_symbol(message)){
      message = pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_MESSAGENAME), 2, 
        message,
        NULL);
    }
    else if(!_pmath_is_valid_messagename(message)){
      pmath_message(PMATH_SYMBOL_MESSAGE, "name", 1, message);
      pmath_unref(head);
      return expr;
    }
    
    if(_pmath_message_is_default_off(message)){
      if(head == PMATH_SYMBOL_OFF){
        pmath_unref(
          pmath_thread_local_save(
            message,
            PMATH_UNDEFINED));
      }
      else{
        pmath_unref(
          pmath_thread_local_save(
            message,
            pmath_ref(head)));
      }
    }
    else{
      if(head == PMATH_SYMBOL_ON){
        pmath_unref(
          pmath_thread_local_save(
            message,
            PMATH_UNDEFINED));
      }
      else{
        pmath_unref(
          pmath_thread_local_save(
            message,
            pmath_ref(head)));
      }
    }
    
    pmath_unref(message);
  }

  pmath_unref(head);
  pmath_unref(expr);
  return NULL;
}
