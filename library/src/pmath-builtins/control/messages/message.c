#include <pmath-util/evaluation.h>
#include <pmath-core/numbers.h>
#include <pmath-core/symbols.h>

#include <assert.h>
#include <string.h>

#include <pmath-util/debug.h>
#include <pmath-util/hashtables-private.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>
#include <pmath-util/modules-private.h>
#include <pmath-util/symbol-values-private.h>

#include <pmath-util/concurrency/threadlocks.h>
#include <pmath-util/concurrency/threads.h>
#include <pmath-util/concurrency/threads-private.h>

#include <pmath-core/objects-private.h>
#include <pmath-core/expressions-private.h>
#include <pmath-core/symbols-private.h>

#include <pmath-builtins/all-symbols.h>
#include <pmath-builtins/all-symbols-private.h>

#include <pmath-util/concurrency/atomic-private.h>

static const int max_message_count = 3;

PMATH_PRIVATE pmath_bool_t _pmath_message_is_default_off(pmath_t msg){
//  return pmath_equals(msg, _pmath_object_newsym_message);
  return pmath_equals(msg, _pmath_object_loadlibrary_load_message)
      || pmath_equals(msg, _pmath_object_get_load_message);
}

PMATH_PRIVATE pmath_t builtin_message(pmath_expr_t expr){
 /* Message(symbol::tag, arg1, arg2, ...)
  */
  pmath_t name, off;
  pmath_string_t text;
  pmath_bool_t stop_msg = FALSE;
  pmath_thread_t thread;

  if(pmath_expr_length(expr) < 1){
    pmath_message_argxxx(pmath_expr_length(expr), 1, SIZE_MAX);
    return expr;
  }

  name = pmath_expr_get_item(expr, 1);
  
  thread = pmath_thread_get_current();
  off = _pmath_thread_local_load_with(name, thread);
  pmath_unref(off);
  
  if(off == PMATH_SYMBOL_OFF
  || (off != PMATH_SYMBOL_ON && _pmath_message_is_default_off(name))){
    pmath_unref(name);
    pmath_unref(expr);
    return NULL;
  }
  
  if(!pmath_equals(_pmath_object_stop_message, name)){
    off = pmath_evaluate(
      pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_INCREMENT), 1,
        pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_MESSAGECOUNT), 1,
          pmath_ref(name))));
    
    if(pmath_instance_of(off, PMATH_TYPE_INTEGER)
    && pmath_integer_fits_si((pmath_integer_t)off)){
      long cnt = pmath_integer_get_si((pmath_integer_t)off);
      
      stop_msg = cnt == max_message_count;
      
      if(cnt > max_message_count){
        pmath_unref(off);
        pmath_unref(expr);
        pmath_unref(name);
        return NULL;
      }
    }
    
    pmath_unref(off);
  }

  if(thread && thread->critical_messages){
    pmath_t dothrow = pmath_evaluate(
      pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_INTERNAL_ISCRITICALMESSAGE), 1,
        pmath_ref(name)));
    pmath_unref(dothrow);
    
    if(dothrow == PMATH_SYMBOL_TRUE){
      pmath_unref(expr);//pmath_unref(pmath_evaluate(expr));
      pmath_throw(name);
      return NULL;
    }
  }
  
  text = pmath_message_find_text(pmath_ref(name));

  if(text == PMATH_UNDEFINED){
    pmath_unref(name);
    pmath_unref(expr);
    return NULL;
  }
  
  pmath_gather_begin(NULL);

//  pmath_emit(
//    pmath_expr_new_extended(
//      pmath_ref(PMATH_SYMBOL_HOLDFORM), 1,
//      pmath_ref(name)),
//    NULL);
//
//  pmath_emit(PMATH_C_STRING(": "), NULL);
  
  if(text){
    expr = pmath_expr_set_item(
      expr, 0, 
      pmath_ref(PMATH_SYMBOL_STRINGFORM));
    
    expr = pmath_expr_set_item(expr, 1, text);
    
    pmath_emit(expr, NULL);
  }
  else{
    pmath_emit(PMATH_C_STRING("-- Message text not found --"), NULL);
    
    if(pmath_expr_length(expr) > 1){
      size_t i;
      
      pmath_emit(PMATH_C_STRING(" ("), NULL);
      pmath_emit(pmath_expr_get_item(expr, 2), NULL);
      for(i = 3;i <= pmath_expr_length(expr);++i){
        pmath_emit(PMATH_C_STRING(","), NULL);
        pmath_emit(pmath_expr_get_item(expr, i), NULL);
      }
      pmath_emit(PMATH_C_STRING(")"), NULL);
    }
    
    pmath_unref(expr);
  }
  
  expr = pmath_gather_end();
  expr = pmath_expr_new_extended(
    pmath_ref(PMATH_SYMBOL_ROW), 1,
    expr);
  expr = pmath_expr_new_extended(
    pmath_ref(PMATH_SYMBOL_COLON), 2,
    pmath_expr_new_extended(
      pmath_ref(PMATH_SYMBOL_HOLDFORM), 1,
      pmath_ref(name)),
    expr);
  
  expr = pmath_expr_new_extended(
    pmath_ref(PMATH_SYMBOL_SECTIONPRINT), 2,
    PMATH_C_STRING("Message"),
    expr);
  
  if(stop_msg){
    pmath_unref(pmath_evaluate(expr));
    
    expr = pmath_expr_new_extended(
      pmath_ref(PMATH_SYMBOL_MESSAGE), 2,
      pmath_ref(_pmath_object_stop_message),
      pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_HOLDFORM), 1,
        name));
      
    name = NULL;
  }
  
  pmath_unref(name);
  return expr;
}

PMATH_PRIVATE pmath_t builtin_messagecount(pmath_expr_t expr){
  pmath_unref(expr);
  return pmath_integer_new_ui(0);
}
