#ifndef __PJ_VALUES_H__
#define __PJ_VALUES_H__

#include <jni.h>
#include <pmath.h>


PMATH_PRIVATE pmath_string_t pj_string_from_java(JNIEnv *env, jstring jstr); 
PMATH_PRIVATE jstring        pj_string_to_java(  JNIEnv *env, pmath_string_t str); // str will be freed; returns local ref


// obj will be freed; type wont be freed
PMATH_PRIVATE pmath_bool_t pj_value_to_java(  JNIEnv *env, pmath_t obj, pmath_t type, jvalue *value);
PMATH_PRIVATE pmath_t      pj_value_from_java(JNIEnv *env, char type, const jvalue *value);

// args and types wont be freed
PMATH_PRIVATE pmath_bool_t pj_value_fill_args(JNIEnv *env, pmath_expr_t types, pmath_expr_t args, jvalue *jargs);


#endif // __PJ_VALUES_H__
