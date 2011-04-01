#include "pmath_Expr.h"
#include "pj-values.h"


JNIEXPORT jobject JNICALL Java_pmath_Expr_execute(
  JNIEnv       *env, 
  jclass        j_clazz, 
  jstring       j_code, 
  jobjectArray  j_args
){
  pmath_string_t code = pj_string_from_java(env, j_code);
  pmath_unref(code);
  return NULL;
}
