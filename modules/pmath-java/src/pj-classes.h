#ifndef __PJ_CLASSES_H__
#define __PJ_CLASSES_H__

#include <jni.h>
#include <pmath.h>

extern pmath_string_t pj_class_get_nice_name(JNIEnv *env, jclass clazz);
extern pmath_string_t pj_class_get_name(JNIEnv *env, jclass clazz);

extern jclass pj_class_to_java(JNIEnv *env, pmath_t obj); // obj will be freed

extern void pj_class_cache_members(JNIEnv *env, jclass clazz);


extern pmath_t pj_class_call_method(
  JNIEnv           *env, 
  jobject           obj, 
  pmath_bool_t      is_static, 
  pmath_string_t    name,         // will be freed; gives error message if no string 
  pmath_expr_t      args,         // will be freed
  pmath_messages_t  msg_thread);  // wont be freed

extern jobject pj_class_new_object(
  JNIEnv           *env,
  jclass            clazz,
  pmath_expr_t      args,         // will be freed
  pmath_messages_t  msg_thread);  // wont be freed

extern pmath_t pj_class_get_field(
  JNIEnv         *env,
  jobject         obj,
  pmath_bool_t    is_static,
  pmath_string_t  name);  // will be freed; gives error message if no string 

extern pmath_bool_t pj_class_set_field(
  JNIEnv         *env,
  jobject         obj,
  pmath_bool_t    is_static,
  pmath_string_t  name,   // will be freed; gives error message if no string 
  pmath_t         value); // will be freed


extern pmath_bool_t pj_classes_init(void);
extern void         pj_classes_done(void);

#endif // __PJ_CLASSES_H__
