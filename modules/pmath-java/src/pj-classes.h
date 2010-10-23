#ifndef __PJ_CLASSES_H__
#define __PJ_CLASSES_H__

#include <jni.h>
#include <pmath.h>

extern pmath_string_t pj_class_get_nice_name(JNIEnv *env, jclass clazz);
extern pmath_string_t pj_class_get_name(JNIEnv *env, jclass clazz);

extern jclass pj_class_get_java(JNIEnv *env, pmath_t obj); // obj will be freed

extern void pj_cache_members(JNIEnv *env, jclass clazz);

extern pmath_bool_t pj_classes_init(void);
extern void         pj_classes_done(void);

#endif // __PJ_CLASSES_H__
