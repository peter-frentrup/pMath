#ifndef __PJ_OBJECTS_H__
#define __PJ_OBJECTS_H__

#include <pmath.h>
#include <jni.h>


extern pmath_t pj_object_from_java(JNIEnv *env, jobject jobj);
extern jobject pj_object_to_java(  JNIEnv *env, pmath_t obj); // obj will be freed; returns new local reference

extern pmath_bool_t pj_object_is_java(JNIEnv *env, pmath_t obj); // obj wont be freed

extern void pj_objects_clear_cache(void);

extern pmath_t pj_builtin_isjavaobject(pmath_expr_t expr);
extern pmath_t pj_builtin_instanceof(pmath_expr_t expr);
extern pmath_t pj_builtin_parentclass(pmath_expr_t expr);

extern pmath_t pj_builtin_javacall(pmath_expr_t expr);
extern pmath_t pj_builtin_javanew(pmath_expr_t expr);

extern pmath_t pj_builtin_javafield(pmath_expr_t expr);
extern pmath_t pj_builtin_assign_javafield(pmath_expr_t expr);

extern pmath_bool_t pj_objects_init(void);
extern void         pj_objects_done(void);

#endif // __PJ_OBJECTS_H__
