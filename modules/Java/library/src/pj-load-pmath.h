#ifndef __PJ_LOAD_PMATH_H__
#define __PJ_LOAD_PMATH_H__


#include <pmath.h>
#include <jni.h>

PMATH_PRIVATE void pj_companion_run_init(void);

PMATH_PRIVATE pmath_bool_t pj_load_pmath(JNIEnv *env);


#endif // __PJ_LOAD_PMATH_H__
