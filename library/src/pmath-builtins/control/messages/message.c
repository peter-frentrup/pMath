#include <pmath-core/expressions-private.h>

#include <pmath-util/concurrency/threads-private.h>
#include <pmath-util/debug.h>
#include <pmath-util/emit-and-gather.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>
#include <pmath-util/modules-private.h>

#include <pmath-builtins/all-symbols-private.h>

#include <limits.h>
#include <stdio.h>

#ifdef _MSC_VER
#  define snprintf sprintf_s
#endif


extern pmath_symbol_t pmath_System_DollarMessageCount;
extern pmath_symbol_t pmath_System_DollarMessagePrePrint;
extern pmath_symbol_t pmath_System_Automatic;
extern pmath_symbol_t pmath_System_ColonForm;
extern pmath_symbol_t pmath_System_General;
extern pmath_symbol_t pmath_System_HoldForm;
extern pmath_symbol_t pmath_System_Increment;
extern pmath_symbol_t pmath_System_Message;
extern pmath_symbol_t pmath_System_MessageName;
extern pmath_symbol_t pmath_System_Off;
extern pmath_symbol_t pmath_System_On;
extern pmath_symbol_t pmath_System_Row;
extern pmath_symbol_t pmath_System_SectionPrint;
extern pmath_symbol_t pmath_System_StringForm;
extern pmath_symbol_t pmath_Internal_DollarMessageFormatter;
extern pmath_symbol_t pmath_Internal_CriticalMessageTag;

static const int max_message_count = 3;

PMATH_PRIVATE pmath_bool_t _pmath_message_is_default_off(pmath_t msg) {
//  return pmath_equals(msg, _pmath_object_newsym_message);
  if(!pmath_is_expr_of_len(msg, pmath_System_MessageName, 2))
    return FALSE;
    
  return pmath_equals(msg, _pmath_object_loadlibrary_load_message) ||
         pmath_equals(msg, _pmath_object_get_load_message);
}

// msg won't be freed
static pmath_bool_t is_known_on_off(pmath_thread_t thread, pmath_t msg, pmath_bool_t *is_on) {
  pmath_t val;
  assert(is_on != NULL);
  
  val = _pmath_thread_local_load_with(msg, thread);
  pmath_unref(val);
  
  if(pmath_same(val, pmath_System_On)) {
    *is_on = TRUE;
    return TRUE;
  }
  
  if( pmath_same(val, pmath_System_Off) ||
      _pmath_message_is_default_off(msg))
  {
    *is_on = FALSE;
    return TRUE;
  }
  
  *is_on = TRUE;
  return FALSE;
}

PMATH_PRIVATE pmath_bool_t _pmath_message_is_on(pmath_t msg) {
  pmath_thread_t thread;
  pmath_t tmp;
  pmath_bool_t result;
  
  thread = pmath_thread_get_current();
  if(is_known_on_off(thread, msg, &result))
    return result;
    
  if(!pmath_is_expr_of_len(msg, pmath_System_MessageName, 2))
    return TRUE;
    
  tmp = pmath_expr_set_item(pmath_ref(msg), 2, PMATH_NULL);
  if(is_known_on_off(thread, tmp, &result)) {
    pmath_unref(tmp);
    return result;
  }
  pmath_unref(tmp);
  
  tmp = pmath_expr_set_item(pmath_ref(msg), 1, pmath_ref(pmath_System_General));
  if(is_known_on_off(thread, tmp, &result)) {
    pmath_unref(tmp);
    return result;
  }
  pmath_unref(tmp);
  
  return TRUE;
}

PMATH_PRIVATE pmath_t builtin_message(pmath_expr_t expr) {
  /* Message(symbol::tag, arg1, arg2, ...)
   */
  pmath_t name;
  pmath_t head;
  pmath_string_t text;
  pmath_bool_t stop_msg = FALSE;
  pmath_thread_t thread = pmath_thread_get_current();
  intptr_t old_dynamic_id;
  size_t exprlen = pmath_expr_length(expr);

  if(!thread)
    return expr;

  if(exprlen < 1) {
    pmath_message_argxxx(exprlen, 1, SIZE_MAX);
    return expr;
  }
  
  name = pmath_expr_get_item(expr, 1);
  
  if(!_pmath_message_is_on(name)) {
    pmath_unref(name);
    pmath_unref(expr);
    return PMATH_NULL;
  }
  
  old_dynamic_id = thread->current_dynamic_id;
  if(!pmath_equals(_pmath_object_stop_message, name)) {
    pmath_t count;
    
    thread->current_dynamic_id = 0;
    count = pmath_evaluate(
              pmath_expr_new_extended(
                pmath_ref(pmath_System_Increment), 1,
                pmath_expr_new_extended(
                  pmath_ref(pmath_System_DollarMessageCount), 1,
                  pmath_ref(name))));
    thread->current_dynamic_id = old_dynamic_id;

    if(pmath_is_int32(count) && _pmath_message_is_on(_pmath_object_stop_message)) {
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
  
  if(thread->critical_messages) {
    pmath_t throw_tag = pmath_evaluate(
                          pmath_expr_new_extended(
                            pmath_ref(pmath_Internal_CriticalMessageTag), 1,
                            pmath_ref(name)));
    
    if(!pmath_is_null(throw_tag)) {
      pmath_debug_print_object("[critical message ", expr, "]\n");
      pmath_unref(expr);//pmath_unref(pmath_evaluate(expr));
      pmath_unref(name);
      pmath_throw(throw_tag);
      return PMATH_NULL;
    }
  }
  
  text = pmath_message_find_text(pmath_ref(name));
  
  if(pmath_same(text, PMATH_UNDEFINED)) {
    pmath_unref(name);
    pmath_unref(expr);
    return PMATH_NULL;
  }
  
  if(pmath_is_null(text)) {
    text = PMATH_C_STRING("-- Message text not found --");
    
    if(exprlen > 1) {
      size_t i;
      
      text = pmath_string_insert_latin1(text, INT_MAX, " (`1`", 5);
      
      for(i = 3; i <= exprlen; ++i) {
        char buf[10];
        snprintf(buf, sizeof(buf), ", `%d`", (int)i - 1);
        text = pmath_string_insert_latin1(text, INT_MAX, buf, -1);
      }
      
      text = pmath_string_insert_latin1(text, INT_MAX, ")", 1);
    }
  }
  
  expr = pmath_expr_set_item(expr, 0, pmath_ref(pmath_System_StringForm));
  expr = pmath_expr_set_item(expr, 1, text);
  
  head = pmath_evaluate(pmath_ref(pmath_System_DollarMessagePrePrint));
  if(!pmath_same(head, pmath_System_Automatic) && !pmath_same(head, pmath_System_DollarMessagePrePrint)) {
    size_t i;
    for(i = 2; i <= exprlen; ++i) {
      expr = pmath_expr_set_item(
               expr, i, 
               pmath_expr_new_extended(
                 pmath_ref(head), 1, 
                 pmath_expr_extract_item(expr, i)));
    }
  }
  pmath_unref(head);
  
  head = pmath_evaluate(pmath_ref(pmath_Internal_DollarMessageFormatter));
  if(pmath_same(head, pmath_System_Automatic) || pmath_same(head, pmath_System_DollarMessagePrePrint)) {
    pmath_unref(head);
    head = pmath_ref(pmath_System_ColonForm);
  }
  
  expr = pmath_expr_new_extended(
           head, 2,
           pmath_expr_new_extended(
             pmath_ref(pmath_System_HoldForm), 1,
             pmath_ref(name)),
           expr);
           
  expr = pmath_expr_new_extended(
           pmath_ref(pmath_System_SectionPrint), 2,
           PMATH_C_STRING("Message"),
           expr);
           
  if(stop_msg) {
    pmath_unref(pmath_evaluate(expr));
    
    expr = pmath_expr_new_extended(
             pmath_ref(pmath_System_Message), 2,
             pmath_ref(_pmath_object_stop_message),
             pmath_expr_new_extended(
               pmath_ref(pmath_System_HoldForm), 1,
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
