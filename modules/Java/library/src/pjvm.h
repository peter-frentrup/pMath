#ifndef PJVM_H__INCLUDED
#define PJVM_H__INCLUDED

#include <pmath.h>
#include <jni.h>
#include <jvmti.h>


extern pmath_t pjvm_dll_filename;
extern pmath_t pjvm_auto_detach_key;
extern jthrowable pjvm_internal_exception;

PMATH_PRIVATE pmath_bool_t pjvm_register_external(JavaVM *jvm);

PMATH_PRIVATE pmath_bool_t pjvm_java_is_running(void);

PMATH_PRIVATE 
PMATH_ATTRIBUTE_USE_RESULT
pmath_t pjvm_try_get(void); // must be freed

PMATH_PRIVATE JavaVM   *pjvm_get_java(pmath_t pjvm);
PMATH_PRIVATE jvmtiEnv *pjvm_get_jvmti(pmath_t pjvm);

PMATH_PRIVATE JNIEnv *pjvm_try_get_env(void);
PMATH_PRIVATE JNIEnv *pjvm_get_env(void);

PMATH_PRIVATE pmath_bool_t pj_exception_to_pmath(JNIEnv *env);
PMATH_PRIVATE pmath_bool_t pj_exception_to_java(JNIEnv *env);


PMATH_PRIVATE void pjvm_ensure_started(void);

PMATH_PRIVATE pmath_bool_t pjvm_init(void);
PMATH_PRIVATE void         pjvm_done(void);

#endif // PJVM_H__INCLUDED
