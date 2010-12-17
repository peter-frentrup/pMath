#include "pjvm.h"
#include "pj-classes.h"
#include "pj-objects.h"
#include "pj-symbols.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>


#ifdef PMATH_OS_WIN32
  #include <windows.h>
#else
  #include <dlfcn.h>
  #include <errno.h>
#endif


static pmath_messages_t jvm_main_mq = NULL;
pmath_t pjvm_auto_detach_key; // initialized/freed in main.c

static PMATH_DECLARE_ATOMIC(vm_lock) = 0;
static pmath_t vm = NULL;
struct pjvm_data_t{
  JavaVM   *jvm;
  jvmtiEnv *jvmti;
};

static PMATH_DECLARE_ATOMIC(vm_quit) = 0;

#ifdef PMATH_OS_WIN32
  static HINSTANCE vm_library = NULL;
#else
  static void     *vm_library = NULL;
#endif
static int vm_library_counter = 0;
static pmath_threadlock_t vm_library_lock = NULL;

struct pj_thread_list_t{
  jobject                  thread; // a global reference
  jmethodID                interrupted_method; // should be a global static?

  struct pj_thread_list_t *prev;
  struct pj_thread_list_t *next;
};

struct pj_thread_list_t * volatile all_running_threads = NULL;

static jint (JNICALL *_JNI_GetDefaultJavaVMInitArgs)(void *args) = NULL;
static jint (JNICALL *_JNI_CreateJavaVM)(JavaVM **pvm, void **penv, void *args) = NULL;
static jint (JNICALL *_JNI_GetCreatedJavaVMs)(JavaVM **, jsize, jsize *) = NULL;
  
  // we should maybe protect this
  jthrowable pjvm_internal_exception = NULL; // a global reference
  
  static void unload_jvm_callback(void *dummy){
    if(--vm_library_counter == 0){
      _JNI_GetDefaultJavaVMInitArgs = NULL;
      _JNI_CreateJavaVM             = NULL;
      _JNI_GetCreatedJavaVMs        = NULL;
      
      #ifdef PMATH_OS_WIN32
        FreeLibrary(vm_library);
      #else
        dlclose(vm_library);
      #endif
      
      vm_library = NULL;
    }
  }
  
  struct load_jvm_callback_info_t{
    pmath_bool_t success;
  };
  
  static void load_jvm_callback(void *p){
    pmath_string_t vmlibfile;
    struct load_jvm_callback_info_t *info = (struct load_jvm_callback_info_t*)p;
    
    if(vm_library){
      ++vm_library_counter;
      info->success = TRUE;
      return;
    }
    info->success = FALSE;
    
    vmlibfile = pmath_evaluate(pmath_ref(PJ_SYMBOL_JAVAVMLIBRARYNAME));
    
    if(!pmath_instance_of(vmlibfile, PMATH_TYPE_STRING)){
      pmath_unref(vmlibfile);
      return;
    }
    
    #ifdef PMATH_OS_WIN32
    {
      #define CHECK_LOAD_FUNC(var, type, name) \
        if(!(var = (type)GetProcAddress(vm_library, name))) goto FAIL
      
      const uint16_t *buffer;
      
      vmlibfile = pmath_string_insert_latin1(vmlibfile, INT_MAX, "", 1);
      buffer = pmath_string_buffer(vmlibfile);
      
      if(buffer)
        vm_library = LoadLibraryW((const wchar_t*)buffer);
    }
    #else
    {
      #define CHECK_LOAD_FUNC(var, type, name) \
        if(!(var = (type)dlsym(vm_library, name))) goto FAIL
      
      char *str = pmath_string_to_native(vmlibfile, NULL);
      
      if(str){
        vm_library = dlopen(str, RTLD_LAZY);
        pmath_mem_free(str);
      }
    }
    #endif
    pmath_unref(vmlibfile);
    
    if(vm_library){
      ++vm_library_counter;
      
      CHECK_LOAD_FUNC(_JNI_GetDefaultJavaVMInitArgs, jint(JNICALL*)(void*),                 "JNI_GetDefaultJavaVMInitArgs");
      CHECK_LOAD_FUNC(_JNI_CreateJavaVM,             jint(JNICALL*)(JavaVM**,void**,void*), "JNI_CreateJavaVM");
      CHECK_LOAD_FUNC(_JNI_GetCreatedJavaVMs,        jint(JNICALL*)(JavaVM**,jsize,jsize*), "JNI_GetCreatedJavaVMs");
      
      info->success = TRUE;
      return;
     FAIL:
      unload_jvm_callback(NULL);
    }
  }
  
  static pmath_bool_t load_jvm_library(void){
    struct load_jvm_callback_info_t info;
    
    info.success = FALSE;
    
    pmath_thread_call_locked(&vm_library_lock, load_jvm_callback, &info);
    
    return info.success;
  }
  
//  static void unload_jvm_library(void){
//    pmath_thread_call_locked(&vm_library_lock, unload_jvm_callback, NULL);
//  }
  
  static void pjvm_destructor(void *p){
    struct pjvm_data_t *d = (struct pjvm_data_t*)p;
    
    #ifdef PMATH_DEBUG_LOG
      fprintf(stderr, "[pjvm_destructor]\n");
    #endif
    
    if(pjvm_internal_exception){
      JNIEnv *env = NULL;
      (*d->jvm)->GetEnv(d->jvm, (void**)&env, JNI_VERSION_1_4);
      
      if(env){
        (*env)->DeleteGlobalRef(env, pjvm_internal_exception);
      }
      
      pjvm_internal_exception = NULL;
    }
    
    (*d->jvmti)->DisposeEnvironment(d->jvmti);
    
    pmath_mem_free(d);
  }
  
  static pmath_custom_t create_pjvm(JavaVM *jvm){
    struct pjvm_data_t *d;
    
    if(!jvm)
      return NULL;
    
    d = pmath_mem_alloc(sizeof(struct pjvm_data_t));
    if(!d)
      return NULL;
    
    d->jvm   = jvm;
    d->jvmti = NULL;
    (*d->jvm)->GetEnv(d->jvm, (void**)&d->jvmti, JVMTI_VERSION_1_0);
    if(!d->jvmti){
      pmath_mem_free(d);
      return NULL;
    }
    
    return pmath_custom_new(d, pjvm_destructor);
  }

  static void auto_detach_proc(void *p){
    pmath_t pjvm = pjvm_try_get();
    
    #ifdef PMATH_DEBUG_LOG
      fprintf(stderr, "[auto_detach_proc]\n");
    #endif
    
    if(pjvm){
      JavaVM *jvm = pjvm_get_java(pjvm);
      
      if(jvm){
        JNIEnv *env = NULL;
        
        (*jvm)->GetEnv(jvm, (void**)&env, JNI_VERSION_1_4);
        if(env)
          (*env)->ExceptionClear(env);
        
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
    
    pmath_debug_print("[bye jvm_main]\n");
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
  && pmath_custom_has_destructor(pjvm, pjvm_destructor)){
    return ((struct pjvm_data_t*)pmath_custom_get_data(pjvm))->jvm;
  }
  
  return NULL;
}

jvmtiEnv *pjvm_get_jvmti(pmath_t pjvm){
  if(pmath_instance_of(pjvm, PMATH_TYPE_CUSTOM)
  && pmath_custom_has_destructor(pjvm, pjvm_destructor)){
    return ((struct pjvm_data_t*)pmath_custom_get_data(pjvm))->jvmti;
  }
  
  return NULL;
}

  static JNIEnv *get_env(pmath_bool_t may_fail){
    pmath_t pjvm = pjvm_try_get();
    JavaVM *jvm = NULL;
    
    if(pjvm){
      jvm = pjvm_get_java(pjvm);
      pmath_unref(pjvm);
    }
    
    if(jvm){
      JNIEnv *env = NULL;
      
      (*jvm)->GetEnv(jvm, (void**)&env, JNI_VERSION_1_4);
      if(!env && !may_fail){
        (*jvm)->AttachCurrentThreadAsDaemon(jvm, (void**)&env, NULL);
        
        if(env){
          pmath_t auto_detach = pmath_custom_new(NULL, auto_detach_proc);
          
          pmath_debug_print("[pmath-java: need new env]\n");
          
          if(auto_detach){
            pmath_unref(pmath_thread_local_save(pjvm_auto_detach_key, auto_detach));
          }
          
          // auto_detach will be freed in the context of this thread when it 
          // terminates.
        }
      
        (*jvm)->GetEnv(jvm, (void**)&env, JNI_VERSION_1_4);
      }
      
      return env;  
    }
    
    return NULL;
  }

JNIEnv *pjvm_try_get_env(void){
  return get_env(TRUE);
}

JNIEnv *pjvm_get_env(void){
  return get_env(FALSE);
}


pmath_bool_t pj_exception_to_pmath(JNIEnv *env){
  jthrowable jex;
  if(!env)
    return FALSE;
  
  jex = (*env)->ExceptionOccurred(env);
  if(jex){
    #ifdef PMATH_DEBUG_LOG
      (*env)->ExceptionDescribe(env);
    #endif
    (*env)->ExceptionClear(env);
    
    if(!(*env)->IsSameObject(env, jex, pjvm_internal_exception)){
      pmath_t pex;
      
      pex = pmath_expr_new_extended(
        pmath_ref(PJ_SYMBOL_JAVAEXCEPTION), 2, 
        pj_object_from_java(env, jex),
        NULL);
      
      pmath_debug_print_object("[java exception: ", pex, "]\n");
      
      pmath_throw(pex);
    }
    
    (*env)->DeleteLocalRef(env, jex);
    return TRUE;
  }
  
  return FALSE;
}


void pjvm_ensure_started(void){
  pmath_t pjvm = pjvm_try_get();
  
  if(pjvm){
    pmath_unref(pjvm);
    return;
  }
  
  pmath_unref(pmath_evaluate(pmath_expr_new(
    pmath_ref(PJ_SYMBOL_JAVASTARTVM), 0)));
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
        if(load_jvm_library()){
          JavaVMInitArgs vm_args;
          JavaVM *jvm = NULL;
          jvmtiEnv *jvmti = NULL;
          JNIEnv *env = NULL;
          JavaVMOption opt[6];
          jint nOptions = 0;
          char *classpath = NULL;
          jint err;
          
          memset(&vm_args, 0, sizeof(vm_args));
          memset(opt, 0, sizeof(opt));
          #ifdef PMATH_DEBUG_LOG
          {
            opt[nOptions++].optionString = "-verbose:jni";
            opt[nOptions++].optionString = "-verbose:class";
            opt[nOptions++].optionString = "-Xcheck:jni";
          }
          #endif
          
          opt[nOptions++].optionString = "-Xrs";
          
          
          { // setting up classpath
            pmath_t cp = pmath_evaluate(pmath_ref(PJ_SYMBOL_DEFAULTCLASSPATH));
            
            if(pmath_instance_of(cp, PMATH_TYPE_STRING))
              cp = pmath_build_value("(o)", cp);
            
            if(pmath_is_expr_of(cp, PMATH_SYMBOL_LIST)
            && pmath_expr_length(cp) > 0){
              pmath_string_t s = PMATH_C_STRING("-Djava.class.path=");
              size_t i;
              
              for(i = 1;i <= pmath_expr_length(cp);++i){
                pmath_t item = pmath_expr_get_item(cp, i);
                
                if(!pmath_instance_of(item, PMATH_TYPE_STRING)){
                  pmath_unref(item);
                  pmath_unref(s);
                  s = NULL;
                  break;
                }
                
                if(i > 1){
                  s = pmath_string_insert_latin1(s, INT_MAX, 
                    #ifdef PMATH_OS_WIN32
                      ";", 1
                    #else
                      ":", 1
                    #endif
                    );
                }
                
                s = pmath_string_concat(s, item);
              }
              
              if(s)
                classpath = pmath_string_to_utf8(s, NULL);
              
              pmath_unref(s);
            }
            
            pmath_unref(cp);
          }
          
          if(classpath){
            opt[nOptions].optionString = classpath;
            ++nOptions;
          }
          
          
          vm_args.version            = JNI_VERSION_1_4;
          vm_args.nOptions           = nOptions;
          vm_args.options            = opt;
          
          err = _JNI_CreateJavaVM(&jvm, (void**)&env, &vm_args); 
          if(err != JNI_OK){
            pmath_debug_print("JNI_CreateJavaVM failed with %d\n", (int)err);
          }
          
          if(env){
            jclass exclass = (*env)->FindClass(env, "pmath/InternalException");
            
            if(!exclass){
              pmath_debug_print("[cannot find pmath.InternalException class]\n");
              
              exclass = (*env)->FindClass(env, "java/lang/ThreadDeath");
            }
            
            if(exclass){
              jmethodID mid = (*env)->GetMethodID(env, exclass, "<init>","()V");
              
              if(mid){
                jobject ex = (*env)->NewObject(env, exclass, mid);
                
                if(ex){
                  pjvm_internal_exception = (*env)->NewGlobalRef(env, ex);
                
                  (*env)->DeleteLocalRef(env, ex);
                }
              }
              
              (*env)->DeleteLocalRef(env, exclass);
            }
          }
          
          vm = create_pjvm(jvm);
          
          pmath_mem_free(classpath);
          
          jvmti = pjvm_get_jvmti(vm);
          if(jvmti){
            jvmtiCapabilities cap;
            jvmtiError err;
            
            memset(&cap, 0, sizeof(cap));
            cap.can_signal_thread = 1;
            err = (*jvmti)->AddCapabilities(jvmti, &cap);
            
            if(err != JVMTI_ERROR_NONE){
              pmath_debug_print("[cannot possess can_signal_thread: err = %d]\n", (int)err);
            }
          }
          
          if(env && JNI_OK == (*env)->EnsureLocalCapacity(env, 4)){
            jclass system = (*env)->FindClass(env, "java/lang/System");
            
            if(system){
              jmethodID mid = (*env)->GetStaticMethodID(env, system, "setSecurityManager","(Ljava/lang/SecurityManager;)V");
              
              if(mid){
                jobject sm_class = (*env)->FindClass(env, "pmath/NoExitSecurityManager");
                
                if(sm_class){
                  jmethodID ctor = (*env)->GetMethodID(env, sm_class, "<init>","()V");
                  
                  if(ctor){
                    jobject sm = (*env)->NewObject(env, sm_class, ctor);
                    
                    if(sm){
                      (*env)->CallStaticVoidMethod(env, system, mid, sm);
                      
                      (*env)->DeleteLocalRef(env, sm);
                    }
                  }
                  
                  (*env)->DeleteLocalRef(env, sm_class);
                }
              }
              
              (*env)->DeleteLocalRef(env, system);
            }
          }
        }
      }
    }
    pmath_atomic_unlock(&vm_lock);
    
  }
  else{
    pmath_unref(pmath_thread_send_wait(jvm_main_mq, expr, HUGE_VAL, NULL, NULL));
  }
  
  pjvm = pjvm_try_get();
  pmath_unref(pjvm);
  return pjvm ? NULL : pmath_ref(PMATH_SYMBOL_FAILED);
}


pmath_bool_t pjvm_init(void){
  vm_lock = 0;
  jvm_main_mq = pmath_thread_fork_daemon(jvm_main, jvm_main_kill, NULL);
  return jvm_main_mq != NULL;
}

void pjvm_done(void){
  pmath_unref(pmath_thread_local_save(pjvm_auto_detach_key, PMATH_UNDEFINED));
  
  pjvm_register_external(NULL);
  pmath_unref(jvm_main_mq); jvm_main_mq = NULL;
  
  if(vm_library_counter > 0){
    vm_library_counter = 1;
    unload_jvm_callback(NULL);
  }
}
