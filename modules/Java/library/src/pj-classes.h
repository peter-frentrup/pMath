#ifndef __PJ_CLASSES_H__
#define __PJ_CLASSES_H__

#include <jni.h>
#include <pmath.h>

PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
pmath_string_t pj_class_get_nice_name(JNIEnv *env, jclass clazz);

PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
pmath_string_t pj_class_get_name(JNIEnv *env, jclass clazz);

PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
jclass pj_class_to_java(JNIEnv *env, pmath_t obj); // obj will be freed

PMATH_PRIVATE void pj_class_cache_members(JNIEnv *env, jclass clazz);


PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
pmath_t pj_class_call_method(
  JNIEnv           *env, 
  jobject           obj, 
  pmath_bool_t      is_static, 
  pmath_string_t    name,         // will be freed; gives error message if no string 
  pmath_expr_t      args,         // will be freed
  pmath_messages_t  msg_thread);  // wont be freed

PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
jobject pj_class_new_object(
  JNIEnv           *env,
  jclass            clazz,
  pmath_expr_t      args,         // will be freed
  pmath_messages_t  msg_thread);  // wont be freed

PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
pmath_t pj_class_get_field(
  JNIEnv         *env,
  jobject         obj,
  pmath_bool_t    is_static,
  pmath_string_t  name);  // will be freed; gives error message if no string 

PMATH_PRIVATE
pmath_bool_t pj_class_set_field(
  JNIEnv         *env,
  jobject         obj,
  pmath_bool_t    is_static,
  pmath_string_t  name,   // will be freed; gives error message if no string 
  pmath_t         value); // will be freed

PMATH_PRIVATE pmath_bool_t pj_classes_init(void);
PMATH_PRIVATE void         pj_classes_done(void);

#endif // __PJ_CLASSES_H__
