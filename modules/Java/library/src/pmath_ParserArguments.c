#include "pmath_ParserArguments.h"

#include "pj-objects.h"
#include "pj-classes.h"
#include "pj-load-pmath.h"
#include "pj-threads.h"
#include "pj-values.h"
#include "pjvm.h"

#include <math.h>


extern pmath_symbol_t pjsym_Java_Internal_CallFromJavaWithContext;
extern pmath_symbol_t pjsym_Java_Internal_Failed;
extern pmath_symbol_t pjsym_Java_Internal_Succeeded;

PMATH_PRIVATE
pmath_t pj_eval_Java_Internal_CallFromJavaWithContext(pmath_t expr) {
  /*  Java`Internal`CallFromJavaWithContext(namespace, namespacePath, code, arguments, simplePostProcessor)
      
      returns:
        Java`Internal`Succeeded(result, namespace, namespacePath)
      or:
        Java`Internal`Failed(exception, namespace, namespacePath)
   */
  pmath_t old_ns;
  pmath_t old_ns_path;
  pmath_t ns             = pmath_expr_get_item(expr, 1);
  pmath_t ns_path        = pmath_expr_get_item(expr, 2);
  pmath_t code           = pmath_expr_get_item(expr, 3);
  pmath_t args           = pmath_expr_get_item(expr, 4);
  pmath_t post_processor = pmath_expr_get_item(expr, 5);
  pmath_t exception;
  
  pmath_unref(expr);
  expr = PMATH_NULL;
  
  old_ns = pmath_symbol_get_value(PMATH_SYMBOL_NAMESPACE);
  if(pmath_is_string(ns) && !pmath_equals(old_ns, ns)) 
    pmath_symbol_set_value(PMATH_SYMBOL_NAMESPACE, ns);
  else
    pmath_unref(ns);
  
  old_ns_path = pmath_symbol_get_value(PMATH_SYMBOL_NAMESPACEPATH);
  if(pmath_is_string(ns_path) && !pmath_equals(old_ns_path, ns_path)) 
    pmath_symbol_set_value(PMATH_SYMBOL_NAMESPACEPATH, ns_path);
  else
    pmath_unref(ns_path);
  
  if(pmath_is_null(args)) {
    expr = pmath_expr_new_extended(
             pmath_ref(PMATH_SYMBOL_TOEXPRESSION), 1,
             code);
  }
  else {
    expr = pmath_expr_new_extended(
             pmath_ref(PMATH_SYMBOL_TOEXPRESSION), 2,
             code,
             pmath_expr_new_extended(
               pmath_ref(PMATH_SYMBOL_RULE), 2,
               pmath_ref(PMATH_SYMBOL_PARSERARGUMENTS),
               args));
  }
  
  expr = pmath_evaluate(expr);
  
  if(pmath_is_string(post_processor) && pmath_string_length(post_processor) > 0) {
    post_processor = pmath_parse_string(post_processor);
    expr = pmath_expr_new_extended(post_processor, 1, expr);
    expr = pmath_evaluate(expr);
  }
  else
    pmath_unref(post_processor);
  
  exception = pmath_catch();
  
  ns = pmath_symbol_get_value(PMATH_SYMBOL_NAMESPACE);
  pmath_symbol_set_value(PMATH_SYMBOL_NAMESPACE, old_ns);
  
  ns_path = pmath_symbol_get_value(PMATH_SYMBOL_NAMESPACEPATH);
  pmath_symbol_set_value(PMATH_SYMBOL_NAMESPACEPATH, old_ns_path);
  
  if(pmath_is_evaluatable(exception)) {
    pmath_unref(expr);
    return pmath_expr_new_extended(
             pmath_ref(pjsym_Java_Internal_Failed), 3,
             exception,
             ns,
             ns_path);
    
  }
  
  pmath_unref(exception);
  return pmath_expr_new_extended(
           pmath_ref(pjsym_Java_Internal_Succeeded), 3,
           expr,
           ns,
           ns_path);
}

static jobject get_hidden_object_field(JNIEnv *env, jobject obj, const char *name, const char *type_signature) {
  jclass  clazz;
  jobject result = NULL;
  
  if(!obj)
    return NULL;
  
  if(*type_signature != '[' && *type_signature != 'L')
    return NULL;
  
  clazz = (*env)->GetObjectClass(env, obj);
  if(clazz) {
    jfieldID fid = (*env)->GetFieldID(env, clazz, name, type_signature);
    if(fid) {
      result = (*env)->GetObjectField(env, obj, fid);
    }
    
    (*env)->DeleteLocalRef(env, clazz);
  }
  
  return result;
}

static void set_hidden_object_field(JNIEnv *env, jobject obj, const char *name, const char *type_signature, jobject value) {
  jclass  clazz;
  
  if(!obj)
    return;
  
  if(*type_signature != '[' && *type_signature != 'L')
    return;
  
  clazz = (*env)->GetObjectClass(env, obj);
  if(clazz) {
    jfieldID fid = (*env)->GetFieldID(env, clazz, name, type_signature);
    if(fid) {
      (*env)->SetObjectField(env, obj, fid, value);
    }
    
    (*env)->DeleteLocalRef(env, clazz);
  }
}

static pmath_t get_hidden_object_field_as_pmath(JNIEnv *env, jobject obj, const char *name, const char *type_signature) {
  pmath_t result = PMATH_NULL;
  jvalue val;
  if(*type_signature == '[' || *type_signature == 'L') {
    val.l = get_hidden_object_field(env, obj, name, type_signature);
    result = pj_value_from_java(env, *type_signature, &val);
    (*env)->DeleteLocalRef(env, val.l);
  }
  return result;
}

// value will be freed
static void set_hidden_object_field_from_pmath(JNIEnv *env, jobject obj, const char *name, const char *type_signature, pmath_t value) {
  if(*type_signature == '[' || *type_signature == 'L') {
    jclass type = (*env)->FindClass(env, type_signature);
    if(type) {
      jobject val = pj_value_to_java_object(env, value, type);
      if(val || pmath_is_null(value)) {
        set_hidden_object_field(env, obj, name, type_signature, val);
      }
      if(val)
        (*env)->DeleteLocalRef(env, val);
        
      value = PMATH_NULL;
      
      (*env)->DeleteLocalRef(env, type);
    }
  }
  pmath_unref(value);
}

static pmath_string_t get_hidden_string_field(JNIEnv *env, jobject obj, const char *name) {
  pmath_string_t result = PMATH_NULL;
  jobject str = get_hidden_object_field(env, obj, name, "Ljava/lang/String;");
  if(str) {
    result = pj_string_from_java(env, str);
    (*env)->DeleteLocalRef(env, str);
  }
  return result;
}

// value will be freed
static void set_hidden_string_field(JNIEnv *env, jobject obj, const char *name, pmath_string_t value) {
  jobject val = pj_string_to_java(env, value);
  if(val || pmath_is_null(value)) {
    set_hidden_object_field(env, obj, name, "Ljava/lang/String;", val);
  }
  if(val)
    (*env)->DeleteLocalRef(env, val);
}

JNIEXPORT jobject JNICALL Java_pmath_ParserArguments_execute(JNIEnv *env, jobject obj_ParserArguments_this) {
  pmath_messages_t companion;
  pmath_t expr;
  jobject obj_ParserArguments_context;
  pmath_string_t this_context_namespace;
  pmath_t        this_context_namespacePath;
  pmath_string_t this_code;
  pmath_t        this_arguments;
  pmath_string_t this_simplePostProcessor;
  
  jobject java_result = NULL;
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
  
  obj_ParserArguments_context = get_hidden_object_field(env, obj_ParserArguments_this, "context", "Lpmath/Context;");
  
  this_code                = get_hidden_string_field(         env, obj_ParserArguments_this, "code");
  this_arguments           = get_hidden_object_field_as_pmath(env, obj_ParserArguments_this, "arguments", "[Ljava/lang/Object;");
  this_simplePostProcessor = get_hidden_string_field(         env, obj_ParserArguments_this, "simplePostProcessor");
  
  if(obj_ParserArguments_context) {
    this_context_namespace     = get_hidden_string_field(         env, obj_ParserArguments_context, "namespace");
    this_context_namespacePath = get_hidden_object_field_as_pmath(env, obj_ParserArguments_context, "namespacePath", "[Ljava/lang/String;");
  }
  else {
    this_context_namespace     = PMATH_NULL;
    this_context_namespacePath = PMATH_NULL;
  }
  
  companion = pj_thread_get_companion(NULL);
  expr = pmath_expr_new_extended(
           pmath_ref(pjsym_Java_Internal_CallFromJavaWithContext), 5,
           this_context_namespace,
           this_context_namespacePath,
           this_code,
           this_arguments,
           this_simplePostProcessor);
  expr = pmath_thread_send_wait(companion, expr, HUGE_VAL, NULL, NULL);
  pmath_unref(companion);
  
  this_context_namespace     = PMATH_NULL;
  this_context_namespacePath = PMATH_NULL;
  if(pmath_is_expr_of(expr, pjsym_Java_Internal_Failed)) {
    this_context_namespace     = pmath_expr_get_item(expr, 2);
    this_context_namespacePath = pmath_expr_get_item(expr, 3);
    
    pmath_throw(pmath_expr_get_item(expr, 1));
    pmath_unref(expr);
    
    pj_exception_to_java(env);
  }
  else if(pmath_is_expr_of(expr, pjsym_Java_Internal_Succeeded)) {
    pmath_t result;
    jclass expected_type = get_hidden_object_field(env, obj_ParserArguments_this, "expectedType", "Ljava/lang/Class;");
    
    this_context_namespace     = pmath_expr_get_item(expr, 2);
    this_context_namespacePath = pmath_expr_get_item(expr, 3);
    
    result = pmath_expr_get_item(expr, 1);
    pmath_unref(expr);
    
    java_result = pj_value_to_java_object(env, result, expected_type);
    
    if(expected_type)
      (*env)->DeleteLocalRef(env, expected_type);
  }
  else {
    pmath_debug_print_object("[java->pmath callback returned invalid code ", expr, "]\n");
    pmath_unref(expr);
  }
  
  if(obj_ParserArguments_context) {
    if(pmath_is_string(this_context_namespace)) {
      set_hidden_string_field(env, obj_ParserArguments_context, "namespace", this_context_namespace);
      this_context_namespace = PMATH_NULL;
    }
    if(pmath_is_expr_of(this_context_namespacePath, PMATH_SYMBOL_LIST)) {
      set_hidden_object_field_from_pmath(env, obj_ParserArguments_context, "namespacePath", "[Ljava/lang/String;", this_context_namespacePath);
      this_context_namespacePath = PMATH_NULL;
    }
    
    (*env)->DeleteLocalRef(env, obj_ParserArguments_context);
  }
  
  pmath_unref(this_context_namespace);
  pmath_unref(this_context_namespacePath);
  if(load_temporary)
    pmath_done();
    
  return java_result;
}
