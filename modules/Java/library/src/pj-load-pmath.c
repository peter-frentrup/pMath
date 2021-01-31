#include "pj-load-pmath.h"
#include "pj-threads.h"
#include "pjvm.h"

#include <math.h>


extern pmath_symbol_t pjsym_System_Get;

#ifndef PMATH_OS_WIN32
static char *JNU_GetStringNativeChars(JNIEnv *env, jstring jstr) {
  jbyteArray bytes = NULL;
  jthrowable exc;
  char *result = NULL;
  
  jclass string_class;
  jmethodID MID_String_getBytes;
  
  if((*env)->EnsureLocalCapacity(env, 2) < 0) 
    return NULL;
  
  string_class = (*env)->FindClass(env, "java/lang/String");
  if(!string_class)
    return NULL;
  
  MID_String_getBytes = (*env)->GetMethodID(env, string_class, "getBytes", "()[B");
  if(!MID_String_getBytes) {
    (*env)->DeleteLocalRef(env, string_class);
    return NULL;
  }
  
  (*env)->DeleteLocalRef(env, string_class);
  
  bytes = (*env)->CallObjectMethod(env, jstr, MID_String_getBytes);
  exc = (*env)->ExceptionOccurred(env);
  if(!exc) {
    jint len = (*env)->GetArrayLength(env, bytes);
    result = malloc(len + 1);
    
    if (result == 0) {
      (*env)->DeleteLocalRef(env, bytes);
      return NULL;
    }
    
    (*env)->GetByteArrayRegion(env, bytes, 0, len,
                               (jbyte *)result);
    result[len] = '\0';
  } 
  else {
    (*env)->DeleteLocalRef(env, exc);
  }
  
  (*env)->DeleteLocalRef(env, bytes);
  return result;
}
#endif

// set PMATH_BASEDIRECTORY environment variable from "pmath.core.base_directory" system property.
static void prepare_environment(JNIEnv *env) {
  if(JNI_OK == (*env)->EnsureLocalCapacity(env, 3)) {
    jclass system_class = (*env)->FindClass(env, "java/lang/System");
    
    if(system_class) {
      jmethodID mid_getProperty = (*env)->GetStaticMethodID(env, system_class, "getProperty", "(Ljava/lang/String;)Ljava/lang/String;");
      
      if(mid_getProperty) {
        jstring name = (*env)->NewStringUTF(env, "pmath.core.base_directory");
        
        if(name) {
          jstring value = (*env)->CallStaticObjectMethod(env, system_class, mid_getProperty, name);
          
          if(value) {
#           ifdef PMATH_OS_WIN32
            {
              jsize len = (*env)->GetStringLength(env, value);
              wchar_t *str = malloc(sizeof(wchar_t) * ((size_t)len + 1));
              const wchar_t *buf = (*env)->GetStringCritical(env, value, NULL);
              
              if(str && buf) {
                memcpy(str, buf, sizeof(wchar_t) * (size_t)len);
                str[len] = L'\0';
                
                SetEnvironmentVariableW(L"PMATH_BASEDIRECTORY", str);
              }
              
              (*env)->ReleaseStringCritical(env, value, buf);
              free(str);
            }
#           else
            {
              char *str = JNU_GetStringNativeChars(env, value);
            
              if(str) {
                setenv("PMATH_BASEDIRECTORY", str, 1);
                
                free(str);
              }
            }
#           endif
            (*env)->DeleteLocalRef(env, value);
          }
          
          (*env)->DeleteLocalRef(env, name);
        }
      }
      
      (*env)->DeleteLocalRef(env, system_class);
    }
  }
}

// calls pmath_init()
// pmath_done() will be called on thread exit.
static pmath_bool_t startup(JavaVM *jvm) {
  JNIEnv *env;
  
  if(!pmath_init())
    return FALSE;
    
  PMATH_RUN("<<Java`");
  
  if(pmath_is_null(pjvm_dll_filename)) {
    pmath_debug_print("[init_loader: pjvm_dll_filename == NULL]\n");
    pmath_done();
    return FALSE;
  }
  
  (*jvm)->AttachCurrentThreadAsDaemon(jvm, (void**)&env, NULL);
  pjvm_register_external(jvm);
  
  if(!pjvm_java_is_running()) {
    pmath_debug_print("[init_loader: pjvm_java_is_running() == FALSE]\n");
    pmath_done();
    return FALSE;
  }
  
  return TRUE;
}


// Run <<Java` in the companion thread to add it to the namespace path.
void pj_companion_run_init(void) {

  pmath_messages_t  companion;
  pmath_string_t    expr;
  
  companion = pj_thread_get_companion(NULL);
  expr = pmath_expr_new_extended(
           pmath_ref(pjsym_System_Get), 1,
           PMATH_C_STRING("Java`"));
  expr = pmath_thread_send_wait(companion, expr, HUGE_VAL, NULL, NULL);
  pmath_unref(companion);
  pmath_unref(expr);
}


pmath_bool_t pj_load_pmath(JNIEnv *env) {
  JavaVM           *jvm = NULL;
  
  if(JNI_OK != (*env)->GetJavaVM(env, &jvm))
    return FALSE;
    
  prepare_environment(env);
  
  if(!startup(jvm))
    return FALSE;
    
  pj_companion_run_init();
  
  return TRUE;
}

