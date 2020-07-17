#ifndef __PJ_OBJECTS_H__
#define __PJ_OBJECTS_H__

#include <pmath.h>
#include <jni.h>


PMATH_PRIVATE 
PMATH_ATTRIBUTE_USE_RESULT
pmath_t pj_object_from_java(JNIEnv *env, jobject jobj);

PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
jobject pj_object_to_java(JNIEnv *env, pmath_t obj); // obj will be freed; returns new local reference

PMATH_PRIVATE pmath_bool_t pj_object_is_java(JNIEnv *env, pmath_t obj); // obj wont be freed

PMATH_PRIVATE void pj_objects_clear_cache(void);

PMATH_PRIVATE pmath_t pj_eval_upcall_Java_JavaField(pmath_expr_t expr);

PMATH_PRIVATE pmath_bool_t pj_objects_init(void);
PMATH_PRIVATE void         pj_objects_done(void);

#endif // __PJ_OBJECTS_H__
