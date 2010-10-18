#ifndef __PJ_CLASS_H__
#define __PJ_CLASS_H__

#include <jni.h>
#include <pmath.h>

extern pmath_string_t pj_class_get_name(JNIEnv *env, jclass clazz);

extern jclass pj_class_get_java(JNIEnv *env, pmath_t obj); // obj will be freed

extern pmath_bool_t pj_class_init_module(void);
extern void         pj_class_done_module(void);

#endif // __PJ_CLASS_H__
