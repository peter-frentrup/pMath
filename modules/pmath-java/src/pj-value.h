#ifndef __PJ_VALUE_H__
#define __PJ_VALUE_H__

#include <jni.h>
#include <pmath.h>


extern pmath_string_t pj_string_from_java(JNIEnv *env, jstring jstr); 
extern jstring        pj_string_to_java(  JNIEnv *env, pmath_string_t str); // str will be freed


#endif // __PJ_VALUE_H__
