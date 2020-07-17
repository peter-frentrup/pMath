#include "pmath_Core.h"
#include "pj-objects.h"
#include "pj-threads.h"
#include "pj-values.h"
#include "pj-load-pmath.h"
#include "pjvm.h"

#include <math.h>


extern pmath_symbol_t pjsym_Java_Internal_Failed; 
extern pmath_symbol_t pjsym_Java_Internal_CallFromJava; 
extern pmath_symbol_t pjsym_Java_Internal_Succeeded; 


pmath_t pj_eval_Java_Internal_CallFromJava(pmath_t expr) {
  pmath_messages_t mq   = pmath_expr_get_item(expr, 1);
  pmath_string_t code   = pmath_expr_get_item(expr, 2);
  pmath_t        args   = pmath_expr_get_item(expr, 3);
  pmath_t        result = PMATH_NULL;
  pmath_t        exception;
  
  pmath_unref(expr);
  
  if(!pmath_is_message_queue(mq)) {
    pmath_unref(code);
    pmath_unref(args);
    return PMATH_NULL;
  }
  
  if(pmath_is_string(code) && pmath_is_expr_of(args, PMATH_SYMBOL_LIST)) {
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
  if(pmath_is_evaluatable(exception)) {
    pmath_unref(result);
    
    result = pmath_expr_new_extended(
               pmath_ref(pjsym_Java_Internal_Failed), 1,
               exception);
               
//    pmath_thread_send(mq,
//      pmath_expr_new_extended(
//        pmath_ref(PMATH_SYMBOL_THROW), 1,
//        exception));
  }
  else {
    pmath_unref(exception);
    
    result = pmath_expr_new_extended(
               pmath_ref(pjsym_Java_Internal_Succeeded), 1,
               result);
  }
  
  pmath_unref(mq);
  return result;
}


JNIEXPORT jobject JNICALL Java_pmath_Core_execute(
  JNIEnv       *env,
  jclass        j_clazz,
  jstring       j_code,
  jobjectArray  j_args
) {
  pmath_messages_t companion;
  pmath_string_t code;
  pmath_t args, expr;
  
  jvalue  val;
  pmath_bool_t load_temporary;
  
  if(pmath_is_null(pjvm_dll_filename)) {
    if(!pj_load_pmath(env))
      return NULL;
      
    if(pmath_is_null(pjvm_dll_filename))
      return NULL;
  }
  
  load_temporary = pmath_thread_get_current() == NULL;
  if(load_temporary) {
    if(!pmath_init())
      return NULL;
      
    pj_companion_run_init();
  }
  
  code = pj_string_from_java(env, j_code);
  val.l = j_args;
  args = pj_value_from_java(env, '[', &val);
  
  companion = pj_thread_get_companion(NULL);
  expr = pmath_expr_new_extended(
           pmath_ref(pjsym_Java_Internal_CallFromJava), 3,
           pmath_thread_get_queue(),
           code,
           args);
  expr = pmath_thread_send_wait(companion, expr, HUGE_VAL, NULL, NULL);
  pmath_unref(companion);
  
  if(pmath_is_expr_of(expr, pjsym_Java_Internal_Failed)) {
    pmath_throw(pmath_expr_get_item(expr, 1));
    
    pj_exception_to_java(env);
    
    pmath_unref(expr);
    return NULL;
  }
  else if(pmath_is_expr_of(expr, pjsym_Java_Internal_Succeeded)) {
    pmath_t result = pmath_expr_get_item(expr, 1);
    pmath_unref(expr);
    expr = result;
  }
  else {
    pmath_debug_print_object("[java->pmath callback returned invalid code ", expr, "]\n");
    pmath_unref(expr);
    expr = PMATH_NULL;
  }
  
  val.l = NULL;
  code = PMATH_C_STRING("Ljava/lang/Object;");
  pj_value_to_java(env, expr, code, &val);
  pmath_unref(code);
  
  if(load_temporary)
    pmath_done();
    
  return val.l;
}
