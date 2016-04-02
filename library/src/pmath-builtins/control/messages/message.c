#include <pmath-core/expressions-private.h>

#include <pmath-util/concurrency/threads-private.h>
#include <pmath-util/emit-and-gather.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>
#include <pmath-util/modules-private.h>

#include <pmath-builtins/all-symbols-private.h>

static const int max_message_count = 3;

PMATH_PRIVATE pmath_bool_t _pmath_message_is_default_off(pmath_t msg) {
//  return pmath_equals(msg, _pmath_object_newsym_message);
  return pmath_equals(msg, _pmath_object_loadlibrary_load_message) ||
         pmath_equals(msg, _pmath_object_get_load_message);
}

PMATH_PRIVATE pmath_bool_t _pmath_message_is_on(pmath_t msg) {
  pmath_thread_t thread;
  pmath_t is_off;
  
  thread = pmath_thread_get_current();
  is_off = _pmath_thread_local_load_with(msg, thread);
  pmath_unref(is_off);
  
  if(pmath_same(is_off, PMATH_SYMBOL_ON))
    return TRUE;
    
  if( pmath_same(is_off, PMATH_SYMBOL_OFF) ||
      _pmath_message_is_default_off(msg))
  {
    return FALSE;
  }
  
  is_off = PMATH_UNDEFINED;
  if(pmath_is_expr_of_len(msg, PMATH_SYMBOL_MESSAGENAME, 2)) {
    pmath_t varname = pmath_expr_set_item(pmath_ref(msg), 2, PMATH_NULL);
    
    is_off = _pmath_thread_local_load_with(varname, thread);
    pmath_unref(is_off);
    pmath_unref(varname);
  }
  
  if(pmath_same(is_off, PMATH_SYMBOL_OFF))
    return FALSE;
    
  return TRUE;
}

PMATH_PRIVATE pmath_t builtin_message(pmath_expr_t expr) {
  /* Message(symbol::tag, arg1, arg2, ...)
   */
  pmath_t name;
  pmath_string_t text;
  pmath_bool_t stop_msg = FALSE;
  pmath_thread_t thread;
  
  if(pmath_expr_length(expr) < 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, SIZE_MAX);
    return expr;
  }
  
  name = pmath_expr_get_item(expr, 1);
  
  if(!_pmath_message_is_on(name)) {
    pmath_unref(name);
    pmath_unref(expr);
    return PMATH_NULL;
  }
  
  if(!pmath_equals(_pmath_object_stop_message, name)) {
    pmath_t count = pmath_evaluate(
                      pmath_expr_new_extended(
                        pmath_ref(PMATH_SYMBOL_INCREMENT), 1,
                        pmath_expr_new_extended(
                          pmath_ref(PMATH_SYMBOL_MESSAGECOUNT), 1,
                          pmath_ref(name))));
                          
    if(pmath_is_int32(count)) {
      long cnt = PMATH_AS_INT32(count);
      
      stop_msg = cnt == max_message_count;
      
      if(cnt > max_message_count) {
        pmath_unref(count);
        pmath_unref(expr);
        pmath_unref(name);
        return PMATH_NULL;
      }
    }
    
    pmath_unref(count);
  }
  
  thread = pmath_thread_get_current();
  if(thread && thread->critical_messages) {
    pmath_t dothrow = pmath_evaluate(
                        pmath_expr_new_extended(
                          pmath_ref(PMATH_SYMBOL_INTERNAL_ISCRITICALMESSAGE), 1,
                          pmath_ref(name)));
    pmath_unref(dothrow);
    
    if(pmath_same(dothrow, PMATH_SYMBOL_TRUE)) {
      pmath_unref(expr);//pmath_unref(pmath_evaluate(expr));
      pmath_throw(name);
      return PMATH_NULL;
    }
  }
  
  text = pmath_message_find_text(pmath_ref(name));
  
  if(pmath_same(text, PMATH_UNDEFINED)) {
    pmath_unref(name);
    pmath_unref(expr);
    return PMATH_NULL;
  }
  
  pmath_gather_begin(PMATH_NULL);
  
//  pmath_emit(
//    pmath_expr_new_extended(
//      pmath_ref(PMATH_SYMBOL_HOLDFORM), 1,
//      pmath_ref(name)),
//    PMATH_NULL);
//
//  pmath_emit(PMATH_C_STRING(": "), PMATH_NULL);

  if(!pmath_is_null(text)) {
    expr = pmath_expr_set_item(
             expr, 0,
             pmath_ref(PMATH_SYMBOL_STRINGFORM));
             
    expr = pmath_expr_set_item(expr, 1, text);
    
    pmath_emit(expr, PMATH_NULL);
  }
  else {
    pmath_emit(PMATH_C_STRING("-- Message text not found --"), PMATH_NULL);
    
    if(pmath_expr_length(expr) > 1) {
      size_t i;
      
      pmath_emit(PMATH_C_STRING(" ("), PMATH_NULL);
      pmath_emit(pmath_expr_get_item(expr, 2), PMATH_NULL);
      for(i = 3; i <= pmath_expr_length(expr); ++i) {
        pmath_emit(PMATH_C_STRING(","), PMATH_NULL);
        pmath_emit(pmath_expr_get_item(expr, i), PMATH_NULL);
      }
      pmath_emit(PMATH_C_STRING(")"), PMATH_NULL);
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
           
  if(stop_msg) {
    pmath_unref(pmath_evaluate(expr));
    
    expr = pmath_expr_new_extended(
             pmath_ref(PMATH_SYMBOL_MESSAGE), 2,
             pmath_ref(_pmath_object_stop_message),
             pmath_expr_new_extended(
               pmath_ref(PMATH_SYMBOL_HOLDFORM), 1,
               name));
               
    name = PMATH_NULL;
  }
  
  pmath_unref(name);
  
  expr = pmath_evaluate(expr);
  
  return expr;
}

PMATH_PRIVATE pmath_t builtin_messagecount(pmath_expr_t expr) {
  pmath_unref(expr);
  return PMATH_FROM_INT32(0);
}
