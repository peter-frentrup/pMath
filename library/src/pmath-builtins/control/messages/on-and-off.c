#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <pmath-config.h>
#include <pmath-types.h>
#include <pmath-core/objects.h>
#include <pmath-core/expressions.h>
#include <pmath-core/numbers.h>
#include <pmath-core/strings.h>
#include <pmath-core/symbols.h>

#include <pmath-util/messages.h>

#include <pmath-util/concurrency/atomic.h>
#include <pmath-util/concurrency/threads.h>

#include <pmath-core/objects-inline.h>

#include <pmath-builtins/control/messages-private.h>
#include <pmath-builtins/all-symbols.h>
#include <pmath-builtins/all-symbols-private.h>

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
    if(!_pmath_is_valid_messagename(message)){
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
//    pmath_unref(pmath_thread_local_save(
//      message,
//      pmath_ref(on_off)));
    pmath_unref(message);
  }

  pmath_unref(head);
  pmath_unref(expr);
  return NULL;
}
