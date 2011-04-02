#include "pmath_Expr.h"
#include "pj-objects.h"
#include "pj-symbols.h"
#include "pj-threads.h"
#include "pj-values.h"
#include "pjvm.h"

#include <math.h>


pmath_t pj_builtin__pmath_Expr_execute(pmath_t expr){
  pmath_messages_t mq   = pmath_expr_get_item(expr, 1);
  pmath_string_t code   = pmath_expr_get_item(expr, 2);
  pmath_t        args   = pmath_expr_get_item(expr, 3);
  pmath_t        result = PMATH_NULL;
  pmath_t        exception;
  
  pmath_unref(expr);
  
  if(!pmath_is_message_queue(mq)){
    pmath_unref(code);
    pmath_unref(args);
    return PMATH_NULL;
  }
  
  if(pmath_is_string(code)
  && pmath_is_expr_of(args, PMATH_SYMBOL_LIST)){
    pmath_t old_parser_arguments = pmath_thread_local_save(
      PMATH_THREAD_KEY_PARSERARGUMENTS, args);
    args = PMATH_NULL;
    
    result = pmath_parse_string(code);
    code = PMATH_NULL;
    
    pmath_unref(pmath_thread_local_save(
      PMATH_THREAD_KEY_PARSERARGUMENTS,
      old_parser_arguments));
  }
  
  pmath_unref(code);
  pmath_unref(args);
  
  result = pmath_evaluate(result);
  
  exception = pmath_catch();
  if(!pmath_same(exception, PMATH_UNDEFINED)){
    pmath_unref(result);
    result = PMATH_NULL;
    
    pmath_thread_send(mq, 
      pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_THROW), 1,
        exception));
  }
  
  pmath_unref(mq);
  return result;
}


JNIEXPORT jobject JNICALL Java_pmath_Expr_execute(
  JNIEnv       *env, 
  jclass        j_clazz, 
  jstring       j_code, 
  jobjectArray  j_args
){
  pmath_messages_t companion;
  pmath_string_t code;
  pmath_t args, expr, exception;
  
  jvalue  val;
  pmath_bool_t load_temporary;
  
  if(pmath_is_null(pjvm_dll_filename)){
    return NULL;
  }
  
  load_temporary = pmath_thread_get_current() == NULL;
  if(load_temporary){
    if(!pmath_init())
      return NULL;
  }
  
  code = pj_string_from_java(env, j_code);
  val.l = j_args;
  args = pj_value_from_java(env, '[', &val);
  
  companion = pj_thread_get_companion(NULL);
  expr = pmath_expr_new_extended(
    pmath_ref(PJ_SYMBOL_INTERNAL_CALLFROMJAVA), 3,
    pmath_thread_get_queue(),
    code,
    args);
  expr = pmath_thread_send_wait(companion, expr, HUGE_VAL, NULL, NULL);
  pmath_unref(companion);
  
  exception = pmath_catch();
  pj_pmath_to_exception(env, exception);
  
  val.l = NULL;
  code = PMATH_C_STRING("Ljava/lang/Object;");
  pj_value_to_java(env, expr, code, &val);
  pmath_unref(code);
  
  if(load_temporary)
    pmath_done();
    
  return val.l;
}
