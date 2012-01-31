#ifndef __PJ_VM_H__
#define __PJ_VM_H__

#include <pmath.h>
#include <jni.h>
#include <jvmti.h>


extern pmath_t pjvm_dll_filename;
extern jthrowable pjvm_internal_exception;

extern pmath_bool_t pjvm_register_external(JavaVM *jvm);


extern pmath_bool_t pjvm_java_is_running(void);
extern pmath_t pjvm_try_get(void); // must be freed
extern JavaVM   *pjvm_get_java(pmath_t pjvm);
extern jvmtiEnv *pjvm_get_jvmti(pmath_t pjvm);

extern JNIEnv *pjvm_try_get_env(void);
extern JNIEnv *pjvm_get_env(void);

extern pmath_bool_t pj_exception_to_pmath(JNIEnv *env);
extern pmath_bool_t pj_exception_to_java(JNIEnv *env);


extern void pjvm_ensure_started(void);
extern pmath_t pj_builtin_startvm(pmath_expr_t expr);


extern pmath_t pjvm_auto_detach_key;

extern pmath_bool_t pjvm_init(void);
extern void         pjvm_done(void);

#endif // __PJ_VM_H__
