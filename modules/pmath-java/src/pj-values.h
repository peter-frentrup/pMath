#ifndef __PJ_VALUES_H__
#define __PJ_VALUES_H__

#include <jni.h>
#include <pmath.h>


extern pmath_string_t pj_string_from_java(JNIEnv *env, jstring jstr); 
extern jstring        pj_string_to_java(  JNIEnv *env, pmath_string_t str); // str will be freed; returns local ref


// obj will be freed; type wont be freed
extern pmath_bool_t pj_value_to_java(JNIEnv *env, pmath_t obj, pmath_t type, jvalue *value);


#endif // __PJ_VALUES_H__
