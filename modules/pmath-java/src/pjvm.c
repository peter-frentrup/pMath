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
#endif


static pmath_messages_t jvm_main_mq = NULL;
pmath_t pjvm_auto_detach_key; // initialized/freed in main.c

static PMATH_DECLARE_ATOMIC(vm_lock) = 0;
static pmath_t vm = NULL;

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
      
      // Maybe we should marshall java calls to different threads, so that the 
      // callers can still be interrupted by pmath and can stop the java thread
      // on their own?
      if(pmath_aborting()){
        JNIEnv *env = pjvm_try_get_env();
        
        if(env && 0 == (*env)->EnsureLocalCapacity(env, 1)){
          jclass thread_class = (*env)->FindClass(env, "Ljava/lang/Thread;");
          
          if(thread_class){
            jmethodID mid_interrupt = (*env)->GetMethodID(env, thread_class, "interrupt", "()V");
            
            if(mid_interrupt){
              pmath_atomic_lock(&vm_lock);
              {
                struct pj_thread_list_t *tl;
                
                tl = all_running_threads;
                if(tl){
                  pmath_debug_print("[interrupt java thread %p]\n", tl->thread);
                  (*env)->CallVoidMethod(env, tl->thread, mid_interrupt);
                  
                  tl = tl->next;
                  while(tl != all_running_threads){
                    pmath_debug_print("[interrupt java thread %p]\n", tl->thread);
                    (*env)->CallVoidMethod(env, tl->thread, mid_interrupt);
                    
                    tl = tl->next;
                  }
                }
              }
              pmath_atomic_unlock(&vm_lock);
            }
            
            (*env)->DeleteLocalRef(env, thread_class);
          }
        }
        else if(env)
          (*env)->ExceptionDescribe(env);
        
        // give the threads 3 seconds time to react to the interrupt...
        if(!vm_quit){
          double t1 = pmath_tickcount();
          do{
            pmath_thread_sleep_timeout(500);
          }while(!vm_quit && all_running_threads != 0 && pmath_tickcount() - t1 < 3.0);
        }
        // there could happen anything during pmath_thread_sleep_timeout()
        
        // Thread.stop() is deprecated bla bla bla
        env = pjvm_try_get_env();
        if(env && 0 == (*env)->EnsureLocalCapacity(env, 1)){
          jclass thread_class = (*env)->FindClass(env, "Ljava/lang/Thread;");
          if(thread_class){
            jmethodID mid_stop = (*env)->GetMethodID(env, thread_class, "stop", "()V");
            
            if(mid_stop){
              pmath_atomic_lock(&vm_lock);
              {
                struct pj_thread_list_t *tl;
                
                tl = all_running_threads;
                if(tl){
                  pmath_debug_print("[stop java thread %p]\n", tl->thread);
                  (*env)->CallVoidMethod(env, tl->thread, mid_stop);
                  
                  tl = tl->next;
                  while(tl != all_running_threads){
                    pmath_debug_print("[stop java thread %p]\n", tl->thread);
                    (*env)->CallVoidMethod(env, tl->thread, mid_stop);
                    
                    tl = tl->next;
                  }
                }
              }
              pmath_atomic_unlock(&vm_lock);
            }
            
            (*env)->DeleteLocalRef(env, thread_class);
          }
        }
        else if(env)
          (*env)->ExceptionDescribe(env);
      }
    }
  }

void *pjvm_enter_call(JNIEnv *env){
  struct pj_thread_list_t *tl = NULL;
  jclass    thread_class;
  jmethodID mid_currentThread;
  jmethodID mid_isInterrupted;
  jobject   current_thread_local;
  jobject   current_thread_global;
  
  if(!env || 0 != (*env)->EnsureLocalCapacity(env, 2))
    goto FAIL_ENV;
  
  thread_class = (*env)->FindClass(env, "Ljava/lang/Thread;");
  if(!thread_class)
    goto FAIL_CLASS;
  
  mid_currentThread = (*env)->GetStaticMethodID(env, thread_class, "currentThread", "()Ljava/lang/Thread;");
  mid_isInterrupted = (*env)->GetMethodID(      env, thread_class, "isInterrupted", "()Z");
  if(!mid_currentThread
  || !mid_isInterrupted)
    goto FAIL_MID;
  
  current_thread_local = (*env)->CallStaticObjectMethod(env, thread_class, mid_currentThread);
  if(!current_thread_local)
    goto FAIL_THREAD_LOCAL;
  
  if((*env)->CallBooleanMethod(env, current_thread_local, mid_isInterrupted)){
    jclass exception_class = (*env)->FindClass(env, "Ljava/lang/InterruptedException;");
    
    if(exception_class){
      (*env)->ThrowNew(env, exception_class, "Interrupt prevents pMath -> Java call.");
      (*env)->DeleteLocalRef(env, exception_class);
    }
    
    goto FAIL_INTERRUPT;
  }
  
  current_thread_global = (*env)->NewGlobalRef(env, current_thread_local);
  if(!current_thread_global)
    goto FAIL_THREAD_GLOBAL;
  
  tl = pmath_mem_alloc(sizeof(struct pj_thread_list_t));
  if(!tl)
    goto FAIL_MALLOC;
  
  tl->thread = current_thread_global; current_thread_global = NULL;
  tl->interrupted_method = (*env)->GetStaticMethodID(env, thread_class, "interrupted", "()Z");
  
  pmath_atomic_lock(&vm_lock);
  {
    if(all_running_threads){
      tl->next = all_running_threads;
      tl->prev = all_running_threads->prev;
      tl->next->prev = tl;
      tl->prev->next = tl;
    }
    else{
      all_running_threads = tl;
      tl->next = tl->prev = tl;
    }
  }
  pmath_atomic_unlock(&vm_lock);
  
 FAIL_MALLOC:          if(current_thread_global)
                         (*env)->DeleteGlobalRef(env, current_thread_global);
 FAIL_THREAD_GLOBAL:
 FAIL_INTERRUPT:       (*env)->DeleteLocalRef(env, current_thread_local);
 FAIL_THREAD_LOCAL:
 FAIL_MID:             (*env)->DeleteLocalRef(env, thread_class);
 FAIL_CLASS:
 FAIL_ENV:
  if(!tl){
    pmath_debug_print("pjvm_enter_call() failed\n");
  }
  return tl;
}

void pjvm_exit_call(JNIEnv *env, void *enter_handle){
  if(env && enter_handle && JNI_OK == (*env)->EnsureLocalCapacity(env, 1)){
    struct pj_thread_list_t *tl = (struct pj_thread_list_t*)enter_handle;
    
    pmath_atomic_lock(&vm_lock);
    {
      tl->next->prev = tl->prev;
      tl->prev->next = tl->next;
      
      if(all_running_threads == tl){
        if(tl == tl->next)
          all_running_threads = NULL;
        else
          all_running_threads = tl->next;
      }
    }
    pmath_atomic_unlock(&vm_lock);
    
    // clear the interrupted flag
    if(tl->interrupted_method){
      jclass thread_class = (*env)->GetObjectClass(env, tl->thread);
      
      if(thread_class){
        (*env)->CallStaticBooleanMethod(env, thread_class, tl->interrupted_method);
        
        (*env)->DeleteLocalRef(env, thread_class);
      }
    }
    
    (*env)->DeleteGlobalRef(env, tl->thread);
    pmath_mem_free(tl);
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
          
          pmath_debug_print("[pmath-java: need new env]");
          
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
  if(!env)
    return FALSE;
  
  if((*env)->ExceptionCheck(env)){
    pmath_t    pex;
    jthrowable jex = (*env)->ExceptionOccurred(env);
    (*env)->ExceptionClear(env);
    
    pex = pmath_expr_new_extended(
      pmath_ref(PJ_SYMBOL_JAVAEXCEPTION), 2, 
      pj_object_from_java(env, jex),
      NULL);
    
    pmath_debug_print_object("[java exception: ", pex, "]\n");
    
    pmath_throw(pex);
    
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
          JNIEnv *env = NULL;
          JavaVMOption opt[2];
          jint nOptions = 0;
          char *classpath = NULL;
          
          memset(&vm_args, 0, sizeof(vm_args));
          memset(opt, 0, sizeof(opt));
          #ifdef PMATH_DEBUG_LOG
          {
            opt[0].optionString = "-verbose:jni";
            nOptions = 1;
          }
          #endif
          
          {
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
          
          vm_args.version = JNI_VERSION_1_4;
          vm_args.nOptions = nOptions;
          vm_args.options = opt;
          
          _JNI_CreateJavaVM(&jvm, (void**)&env, &vm_args); 
          vm = create_pjvm(jvm);
          
          pmath_mem_free(classpath);
          
          // todo: install custom security manager that disallows exit(), halt()
        }
      }
    }
    pmath_atomic_unlock(&vm_lock);
    
  }
  else{
    pmath_unref(pmath_thread_send_wait(jvm_main_mq, expr, HUGE_VAL, NULL, NULL));
    
//    #ifdef PMATH_DEBUG_LOG
//    {
//      JNIEnv *env = pjvm_get_env();
//      if(env){
//        jclass clazz = (*env)->FindClass(env, "Ljava/io/PrintStream;");
//        pmath_t name = pj_class_get_name(env, clazz);
//        
//        PMATH_RUN_ARGS("Print(`1`)", "(o)", name);
//        
//        pj_class_cache_members(env, clazz);
//        (*env)->DeleteLocalRef(env, clazz);
//        
//        pj_exception_to_pmath(env);
//      }
//    }
//    #endif
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
  pmath_unref(pmath_thread_local_save(pjvm_auto_detach_key, PMATH_UNDEFINED));
  
  pjvm_register_external(NULL);
  pmath_unref(jvm_main_mq); jvm_main_mq = NULL;
  
  if(vm_library_counter > 0){
    vm_library_counter = 1;
    unload_jvm_callback(NULL);
  }
}
