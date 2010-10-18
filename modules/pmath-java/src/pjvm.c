#include "pjvm.h"
#include "pj-class.h"
#include "pj-symbols.h"

#include <string.h>
#include <stdio.h>
#include <math.h>


static pmath_messages_t jvm_main_mq = NULL;

static PMATH_DECLARE_ATOMIC(vm_lock) = 0;
static pmath_t vm = NULL;

static PMATH_DECLARE_ATOMIC(vm_quit) = 0;
  
  static void dummy_jvm_proc(void *p){
    #ifdef PMATH_DEBUG_LOG
      fprintf(stderr, "[dummy_jvm_proc]");
    #endif
  }
  
  static pmath_custom_t create_pjvm(JavaVM *jvm){
    if(!jvm)
      return NULL;
    
    return pmath_custom_new(jvm, dummy_jvm_proc);
  }

  static void auto_detach_proc(void *p){
    pmath_t pjvm = pjvm_try_get();
    
    #ifdef PMATH_DEBUG_LOG
      fprintf(stderr, "[auto_detach_proc]");
    #endif
    
    if(pjvm){
      JavaVM *jvm = pjvm_get_java(pjvm);
      
      if(jvm){
        (*jvm)->DetachCurrentThread(jvm);
      }
      
      pmath_unref(pjvm);
    }
  }

  static void jvm_main_kill(void *arg){
    vm_quit = TRUE;
    pmath_thread_wakeup(jvm_main_mq);
  }
  
  static void jvm_main(void *arg){
    while(!vm_quit){
      pmath_thread_sleep();
    }
  }

pmath_bool_t pjvm_register_external(JavaVM *jvm){
  pmath_t pjvm = create_pjvm(jvm);
  
  pmath_t _vm;
  pmath_atomic_lock(&vm_lock);
  {
    _vm = vm;
    vm = pjvm;
  }
  pmath_atomic_unlock(&vm_lock);
  
  pmath_unref(_vm);
  return pjvm != NULL;
}

pmath_t pjvm_try_get(void){
  pmath_t _vm;
  pmath_atomic_lock(&vm_lock);
  {
    _vm = pmath_ref(vm);
  }
  pmath_atomic_unlock(&vm_lock);
  
  return _vm;
}

JavaVM *pjvm_get_java(pmath_t pjvm){
  if(pmath_instance_of(pjvm, PMATH_TYPE_CUSTOM)
  && pmath_custom_has_destructor(pjvm, dummy_jvm_proc)){
    return (JavaVM*)pmath_custom_get_data(pjvm);
  }
  
  return NULL;
}

JNIEnv *pjvm_get_env(void){
  pmath_t pjvm = pjvm_try_get();
  JavaVM *jvm = NULL;
  
  if(pjvm){
    jvm = pjvm_get_java(pjvm);
    pmath_unref(pjvm);
  }
  
  if(jvm){
    JNIEnv *env = NULL;
    
    (*jvm)->GetEnv(jvm, (void**)&env, JNI_VERSION_1_4);
    if(!env){
      (*jvm)->AttachCurrentThreadAsDaemon(jvm, (void**)&env, NULL);
      
      if(env){
        pmath_t auto_detach = pmath_custom_new(NULL, auto_detach_proc);
        
        if(auto_detach)
          pmath_unref(pmath_thread_local_save(auto_detach, auto_detach));
      }
    }
    
    (*jvm)->GetEnv(jvm, (void**)&env, JNI_VERSION_1_4);
    
    return env;  
  }
  
  return NULL;
}


pmath_t pj_builtin_startvm(pmath_expr_t expr){
  pmath_messages_t msg;
  pmath_t pjvm;
  
  if(pmath_expr_length(expr) > 0){
    pmath_message_argxxx(pmath_expr_length(expr), 0, 0);
    return expr;
  }
  
  pjvm = pjvm_try_get();
  if(pjvm){
    pmath_unref(pjvm);
    pmath_unref(expr);
    return NULL;
  }
  
  msg = pmath_thread_get_queue();
  pmath_unref(msg);
  if(msg == jvm_main_mq){
    pmath_unref(expr);
    
    pmath_atomic_lock(&vm_lock);
    {
      if(!vm){
        JavaVMInitArgs vm_args;
        JavaVM *jvm = NULL;
        JNIEnv *env = NULL;
        
        memset(&vm_args, 0, sizeof(vm_args));
        vm_args.version = JNI_VERSION_1_2;
        JNI_GetDefaultJavaVMInitArgs(&vm_args);
        
        JNI_CreateJavaVM(&jvm, (void**)&env, &vm_args); 
        vm = create_pjvm(jvm);
      }
    }
    pmath_atomic_unlock(&vm_lock);
    
  }
  else{
    pmath_unref(pmath_thread_send_wait(jvm_main_mq, expr, HUGE_VAL, NULL, NULL));
    
    {
      JNIEnv *env = pjvm_get_env();
      if(env){
        jclass clazz = (*env)->FindClass(env, "Ljava/lang/String;");
        pmath_t name = pj_class_get_name(env, clazz);
        (*env)->DeleteLocalRef(env, clazz);
        
        PMATH_RUN_ARGS("Print(`1`)", "o", name);
      }
    }
  
  }
  
  pjvm = pjvm_try_get();
  pmath_unref(pjvm);
  return pjvm ? NULL : pmath_ref(PMATH_SYMBOL_FAILED);
}

pmath_t pj_builtin_killvm(pmath_expr_t expr){
  if(pmath_expr_length(expr) > 0){
    pmath_message_argxxx(pmath_expr_length(expr), 0, 0);
    return expr;
  }
  
  pmath_unref(expr);
  pjvm_register_external(NULL);
  return NULL;
}



pmath_bool_t pjvm_init(void){
  vm_lock = 0;
  jvm_main_mq = pmath_thread_fork_daemon(jvm_main, jvm_main_kill, NULL);
  return jvm_main_mq != NULL;
}

void pjvm_done(void){
  pjvm_register_external(NULL);
  pmath_unref(jvm_main_mq); jvm_main_mq = NULL;
}
