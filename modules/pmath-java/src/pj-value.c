#include "pj-value.h"


pmath_string_t pj_string_from_java(JNIEnv *env, jstring jstr){
  pmath_string_t s;
  const jchar *buf;
  int len;
  
  len = (int)(*env)->GetStringLength(env, jstr);
  if(len < 0)
    return NULL;
  
  
  buf = (*env)->GetStringCritical(env, jstr, NULL);
  s = pmath_string_insert_ucs2(NULL, 0, buf, len);
  (*env)->ReleaseStringCritical(env, jstr, buf);
  
  return s;
}

jstring pj_string_to_java(JNIEnv *env, pmath_string_t str){
  const jchar *buf = (const jchar*)pmath_string_buffer(str);
  int len = pmath_string_length(str);
  
  return (*env)->NewString(env, buf, len);
}
