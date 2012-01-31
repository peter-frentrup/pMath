#include "pj-load-pmath.h"
#include "pj-threads.h"
#include "pjvm.h"

#include <math.h>


// set PMATH_BASEDIRECTORY environment variable from "pmath.core.base_directory" system property.
static void prepare_environment(JNIEnv *env) {
  if(JNI_OK == (*env)->EnsureLocalCapacity(env, 3)) {
    jclass system_class = (*env)->FindClass(env, "java/lang/System");
    
    if(system_class) {
      jmethodID mid_getProperty = (*env)->GetStaticMethodID(env, system_class, "getProperty", "(Ljava/lang/String;)Ljava/lang/String;");
      
      if(mid_getProperty) {
        jstring name = (*env)->NewStringUTF(env, "pmath.core.base_directory");
        
        if(name) {
          jobject value = (*env)->CallStaticObjectMethod(env, system_class, mid_getProperty, name);
          
          if(value) {
            const char *str = (*env)->GetStringUTFChars(env, value, NULL);
            
            if (str != NULL) {
#ifdef PMATH_OS_WIN32
              SetEnvironmentVariableA("PMATH_BASEDIRECTORY", str);
#else
              setenv("PMATH_BASEDIRECTORY", str, 1);
#endif
              
              (*env)->ReleaseStringUTFChars(env, value, str);
            }
            
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
static pmath_bool_t startup(void *arg) {
  JavaVM *jvm = (JavaVM*)arg;
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
void pj_companion_run_init(void){

  pmath_messages_t  companion;
  pmath_string_t    expr;
  
  companion = pj_thread_get_companion(NULL);
  expr = pmath_expr_new_extended(
           pmath_ref(PMATH_SYMBOL_GET), 1,
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

