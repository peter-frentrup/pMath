#include "pjvm.h"
#include "pj-classes.h"
#include "pj-objects.h"
#include "pj-symbols.h"
#include "pj-values.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>


#ifdef PMATH_OS_WIN32
#  include <windows.h>
#else
#  include <dlfcn.h>
#  include <errno.h>
#endif


static pmath_messages_t jvm_main_mq = PMATH_STATIC_NULL;
pmath_t pjvm_auto_detach_key = PMATH_STATIC_NULL; // initialized/freed in main.c
pmath_t pjvm_dll_filename    = PMATH_STATIC_NULL; // dito.

static pmath_atomic_t vm_lock = PMATH_ATOMIC_STATIC_INIT;
static pmath_t vm = PMATH_STATIC_NULL;
struct pjvm_data_t {
  JavaVM   *jvm;
  jvmtiEnv *jvmti;
};

static pmath_atomic_t vm_quit = PMATH_ATOMIC_STATIC_INIT;

#ifdef PMATH_OS_WIN32
static HINSTANCE vm_library = NULL;
#else
static void     *vm_library = NULL;
#endif
static int vm_library_counter = 0;
static pmath_threadlock_t vm_library_lock = NULL;

struct pj_thread_list_t {
  jobject                  thread; // a global reference
  jmethodID                interrupted_method; // should be a global static?
  
  struct pj_thread_list_t *prev;
  struct pj_thread_list_t *next;
};

struct pj_thread_list_t *volatile all_running_threads = NULL;

static jint (JNICALL *_JNI_GetDefaultJavaVMInitArgs)(void *args) = NULL;
static jint (JNICALL *_JNI_CreateJavaVM)(JavaVM **pvm, void **penv, void *args) = NULL;
static jint (JNICALL *_JNI_GetCreatedJavaVMs)(JavaVM **, jsize, jsize *) = NULL;

// we should maybe protect this
jthrowable pjvm_internal_exception = NULL; // a global reference

static void unload_jvm_callback(void *dummy) {
  if(--vm_library_counter == 0) {
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

struct load_jvm_callback_info_t {
  pmath_bool_t success;
};

static void load_jvm_callback(void *p) {
  struct load_jvm_callback_info_t *info = (struct load_jvm_callback_info_t*)p;
  
  if(vm_library) {
    ++vm_library_counter;
    info->success = TRUE;
    return;
  }
  info->success = FALSE;
  
#ifdef PMATH_OS_WIN32
  {
    vm_library = GetModuleHandle("jvm.dll");
  }
#else
  {
    vm_library = dlopen("libjvm.so", RTLD_LAZY | RTLD_NOLOAD);
  }
#endif
  
  if(!vm_library) {
    pmath_string_t vmlibfile;
    vmlibfile = pmath_evaluate(pmath_ref(PJ_SYMBOL_JAVAVMLIBRARYNAME));
    
    if(!pmath_is_string(vmlibfile)) {
      pmath_unref(vmlibfile);
      return;
    }
    
#ifdef PMATH_OS_WIN32
    {
#define CHECK_LOAD_FUNC(var, type, name) \
  if(!(var = (type)GetProcAddress(vm_library, name))) goto FAIL
    
      const uint16_t *buffer;
      
      vmlibfile = pmath_string_insert_latin1(vmlibfile, INT_MAX, "", 1);
      buffer = pmath_string_buffer(&vmlibfile);
      
      if(buffer)
        vm_library = LoadLibraryW((const wchar_t*)buffer);
    }
#else
    {
#define CHECK_LOAD_FUNC(var, type, name) \
  if(!(var = (type)dlsym(vm_library, name))) goto FAIL
    
      char *str = pmath_string_to_native(vmlibfile, NULL);
    
      if(str) {
        vm_library = dlopen(str, RTLD_LAZY);
        pmath_mem_free(str);
      }
    }
#endif
    pmath_unref(vmlibfile);
  }
  
  if(vm_library) {
    ++vm_library_counter;
    
    CHECK_LOAD_FUNC(_JNI_GetDefaultJavaVMInitArgs, jint(JNICALL*)(void*),                     "JNI_GetDefaultJavaVMInitArgs");
    CHECK_LOAD_FUNC(_JNI_CreateJavaVM,             jint(JNICALL*)(JavaVM **, void **, void*), "JNI_CreateJavaVM");
    CHECK_LOAD_FUNC(_JNI_GetCreatedJavaVMs,        jint(JNICALL*)(JavaVM **, jsize, jsize*),  "JNI_GetCreatedJavaVMs");
    
    info->success = TRUE;
    return;
  FAIL:
    unload_jvm_callback(NULL);
  }
}

static pmath_bool_t load_jvm_library(void) {
  struct load_jvm_callback_info_t info;
  
  info.success = FALSE;
  
  pmath_thread_call_locked(&vm_library_lock, load_jvm_callback, &info);
  
  return info.success;
}

//  static void unload_jvm_library(void){
//    pmath_thread_call_locked(&vm_library_lock, unload_jvm_callback, NULL);
//  }

static void pjvm_destructor(void *p) {
  struct pjvm_data_t *d = (struct pjvm_data_t*)p;
  
#ifdef PMATH_DEBUG_LOG
  fprintf(stderr, "[pjvm_destructor]\n");
#endif
  
  if(pjvm_internal_exception) {
    JNIEnv *env = NULL;
    (*d->jvm)->GetEnv(d->jvm, (void**)&env, JNI_VERSION_1_4);
    
    if(env) {
      (*env)->DeleteGlobalRef(env, pjvm_internal_exception);
    }
    
    pjvm_internal_exception = NULL;
  }
  
  (*d->jvmti)->DisposeEnvironment(d->jvmti);
  
  pmath_mem_free(d);
}

static pmath_custom_t create_pjvm(JavaVM *jvm) {
  struct pjvm_data_t *d;
  
  if(!jvm)
    return PMATH_NULL;
    
  d = pmath_mem_alloc(sizeof(struct pjvm_data_t));
  if(!d)
    return PMATH_NULL;
    
  d->jvm   = jvm;
  d->jvmti = NULL;
  (*d->jvm)->GetEnv(d->jvm, (void**)&d->jvmti, JVMTI_VERSION_1_0);
  if(!d->jvmti) {
    pmath_mem_free(d);
    return PMATH_NULL;
  }
  
  return pmath_custom_new(d, pjvm_destructor);
}

static void auto_detach_proc(void *p) {
  pmath_t pjvm = pjvm_try_get();
  
#ifdef PMATH_DEBUG_LOG
  fprintf(stderr, "[pmath-java: auto_detach_proc]\n");
#endif
  
  if(!pmath_is_null(pjvm)) {
    JavaVM *jvm = pjvm_get_java(pjvm);
    
    if(jvm) {
      JNIEnv *env = NULL;
      
      (*jvm)->GetEnv(jvm, (void**)&env, JNI_VERSION_1_4);
      if(env){
        (*env)->ExceptionClear(env);
        (*jvm)->DetachCurrentThread(jvm);
      }
    }
    
    pmath_unref(pjvm);
  }
}

static void jvm_main_kill(void *arg) {
  pmath_atomic_write_release(&vm_quit, TRUE);
  pmath_thread_wakeup(jvm_main_mq);
}

static void jvm_main(void *arg) {
  while(!pmath_atomic_read_aquire(&vm_quit)) {
    pmath_thread_sleep();
  }
  
  pmath_debug_print("[bye jvm_main]\n");
}

pmath_bool_t pjvm_register_external(JavaVM *jvm) {
  pmath_t pjvm = create_pjvm(jvm);
  
  pmath_t _vm;
  pmath_atomic_lock(&vm_lock);
  {
    _vm = vm;
    vm = pjvm;
  }
  pmath_atomic_unlock(&vm_lock);
  
  pmath_unref(_vm);
  return !pmath_is_null(pjvm);
}

pmath_bool_t pjvm_java_is_running(void) {
  JavaVM   *jvm     = NULL;
  jsize     num_vms = 0;
  
  if(!load_jvm_library()) {
    pmath_debug_print("[pjvm_java_is_running: load_jvm_library() failed]\n");
    return FALSE;
  }
  
  if(JNI_OK != _JNI_GetCreatedJavaVMs(&jvm, 1, &num_vms)) {
    pmath_debug_print("[pjvm_java_is_running: _JNI_GetCreatedJavaVMs() failed]\n");
    return FALSE;
  }
  
  return jvm != NULL && num_vms > 0;
}

pmath_t pjvm_try_get(void) {
  pmath_t _vm;
  pmath_atomic_lock(&vm_lock);
  {
    _vm = pmath_ref(vm);
  }
  pmath_atomic_unlock(&vm_lock);
  
  return _vm;
}

JavaVM *pjvm_get_java(pmath_t pjvm) {
  if( pmath_is_custom(pjvm) &&
      pmath_custom_has_destructor(pjvm, pjvm_destructor))
  {
    return ((struct pjvm_data_t*)pmath_custom_get_data(pjvm))->jvm;
  }
  
  return NULL;
}

jvmtiEnv *pjvm_get_jvmti(pmath_t pjvm) {
  if( pmath_is_custom(pjvm) &&
      pmath_custom_has_destructor(pjvm, pjvm_destructor))
  {
    return ((struct pjvm_data_t*)pmath_custom_get_data(pjvm))->jvmti;
  }
  
  return NULL;
}

static JNIEnv *get_env(pmath_bool_t may_fail) {
  pmath_t pjvm = pjvm_try_get();
  JavaVM *jvm = NULL;
  
  if(!pmath_is_null(pjvm)) {
    jvm = pjvm_get_java(pjvm);
    pmath_unref(pjvm);
  }
  
  if(jvm) {
    JNIEnv *env = NULL;
    
    (*jvm)->GetEnv(jvm, (void**)&env, JNI_VERSION_1_4);
    if(!env && !may_fail) {
      (*jvm)->AttachCurrentThreadAsDaemon(jvm, (void**)&env, NULL);
      
      if(env) {
        pmath_t auto_detach = pmath_custom_new(NULL, auto_detach_proc);
        
        pmath_debug_print("[pmath-java: need new env]\n");
        
        if(!pmath_is_null(auto_detach)) {
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

JNIEnv *pjvm_try_get_env(void) {
  return get_env(TRUE);
}

JNIEnv *pjvm_get_env(void) {
  return get_env(FALSE);
}


pmath_bool_t pj_exception_to_pmath(JNIEnv *env) {
  jthrowable jex;
  if(!env)
    return FALSE;
    
  jex = (*env)->ExceptionOccurred(env);
  if(jex) {
#ifdef PMATH_DEBUG_LOG
    (*env)->ExceptionDescribe(env);
#endif
    (*env)->ExceptionClear(env);
    
    if(!(*env)->IsSameObject(env, jex, pjvm_internal_exception)) {
      pmath_t pex = PMATH_UNDEFINED;
      
      if(JNI_OK == (*env)->EnsureLocalCapacity(env, 2)) {
        jclass wrapped_ex_class = (*env)->FindClass(env, "pmath/WrappedException");
        
        if(wrapped_ex_class &&
            (*env)->IsInstanceOf(env, jex, wrapped_ex_class))
        {
          jmethodID mid = (*env)->GetMethodID(env, wrapped_ex_class, "getCode", "()Ljava/lang/String;");
          
          if(mid) {
            jobject jobj = (*env)->CallObjectMethod(env, jex, mid);
            
            pmath_string_t code = pj_string_from_java(env, jobj);
            pex = pmath_parse_string(code);
            
            (*env)->DeleteLocalRef(env, jobj);
          }
          
          (*env)->DeleteLocalRef(env, wrapped_ex_class);
        }
      }
      
      if(pmath_same(pex, PMATH_UNDEFINED)) {
        pex = pmath_expr_new_extended(
                pmath_ref(PJ_SYMBOL_JAVAEXCEPTION), 3,
                pj_object_from_java(env, jex),
                PMATH_C_STRING(""),
                pmath_expr_new(pmath_ref(PMATH_SYMBOL_LIST), 0));
                
        if(JNI_OK == (*env)->EnsureLocalCapacity(env, 5)) {
          jclass ex_class = (*env)->GetObjectClass(env, jex);
          
          if(ex_class) {
            jclass object_class = (*env)->FindClass(env, "java/lang/Object");
            
            if(object_class) {
              jmethodID mid_tostring      = (*env)->GetMethodID(env, object_class, "toString",      "()Ljava/lang/String;");
              jmethodID mig_getstacktrace = (*env)->GetMethodID(env, ex_class,     "getStackTrace", "()[Ljava/lang/StackTraceElement;");
              
              if(mid_tostring && mig_getstacktrace) {
                jobjectArray jarr;
                jobject jstr = (*env)->CallObjectMethod(env, jex, mid_tostring);
                
                if(jstr) {
                  pex = pmath_expr_set_item(pex, 2, pj_string_from_java(env, jstr));
                  
                  (*env)->DeleteLocalRef(env, jstr);
                  
                  jarr = (*env)->CallObjectMethod(env, jex, mig_getstacktrace);
                  if(jarr) {
                    jsize len = (*env)->GetArrayLength(env, jarr);
                    jsize i;
                    pmath_t stack = pmath_expr_new(pmath_ref(PMATH_SYMBOL_LIST), (size_t)len);
                    
                    for(i = 0; i < len; ++i) {
                      jobject jobj = (*env)->GetObjectArrayElement(env, jarr, i);
                      if(!jobj)
                        break;
                        
                      jstr = (*env)->CallObjectMethod(env, jobj, mid_tostring);
                      (*env)->DeleteLocalRef(env, jobj);
                      
                      if(!jstr)
                        break;
                        
                      stack = pmath_expr_set_item(stack, (size_t)i + 1, pj_string_from_java(env, jstr));
                      (*env)->DeleteLocalRef(env, jstr);
                    }
                    
                    pex = pmath_expr_set_item(pex, 3, stack);
                    
                    (*env)->DeleteLocalRef(env, jarr);
                  }
                }
              }
              
              (*env)->DeleteLocalRef(env, object_class);
            }
            (*env)->DeleteLocalRef(env, ex_class);
          }
        }
      }
      
#ifdef PMATH_DEBUG_LOG
      (*env)->ExceptionDescribe(env);
#endif
      (*env)->ExceptionClear(env);
      pmath_debug_print_object("[java exception: ", pex, "]\n");
      
      pmath_throw(pex);
    }
    
    (*env)->DeleteLocalRef(env, jex);
    return TRUE;
  }
  
  return FALSE;
}


pmath_bool_t pj_exception_to_java(JNIEnv *env) {
  jclass throwable_class;
  jclass ex_class;
  jthrowable jex;
  jobject str;
  
  pmath_t ex = pmath_catch();
  
  if(pmath_same(ex, PMATH_UNDEFINED))
    return FALSE;
    
  if(pmath_same(ex, PMATH_ABORT_EXCEPTION)) {
    (*env)->Throw(env, pjvm_internal_exception);
    return TRUE;
  }
  
  if(!pmath_is_evaluatable(ex)) {
    pmath_debug_print_object("[uncatchable exception ", ex, " passing java code]\n");
    pmath_throw(ex);
    (*env)->Throw(env, pjvm_internal_exception);
    return TRUE;
  }
  
  if(JNI_OK != (*env)->EnsureLocalCapacity(env, 4)) {
    pmath_unref(ex);
    return TRUE;
  }
  
  throwable_class = (*env)->FindClass(env, "java/lang/Throwable");
  if(!throwable_class) {
    pmath_unref(ex);
    return TRUE;
  }
  
  jex = NULL;
  
  if(pmath_is_expr_of(ex, PJ_SYMBOL_JAVAEXCEPTION)) {
    pmath_t inner = pmath_expr_get_item(ex, 1);
    
    if(pj_object_is_java(env, inner)) {
      jex = pj_object_to_java(env, inner);
      
      if(jex && !(*env)->IsInstanceOf(env, jex, throwable_class)) {
        (*env)->DeleteLocalRef(env, jex);
        jex = NULL;
      }
    }
    
    pmath_unref(inner);
  }
  
  if(!jex) {
    ex_class = (*env)->FindClass(env, "pmath/WrappedException");
    
    if(ex_class) {
      jmethodID cid = (*env)->GetMethodID(env, ex_class, "<init>", "(Ljava/lang/String;)V");
      
      if(cid) {
        ex = pmath_evaluate(
               pmath_expr_new_extended(
                 pmath_ref(PMATH_SYMBOL_TOSTRING), 1,
                 pmath_expr_new_extended(
                   pmath_ref(PMATH_SYMBOL_INPUTFORM), 1,
                   ex)));
                   
        str = pj_string_to_java(env, ex);
        
        if(str) {
          jvalue val[1];
          val[0].l = str;
          
          jex = (*env)->NewObjectA(env, ex_class, cid, val);
          
          (*env)->DeleteLocalRef(env, str);
        }
      }
      
      (*env)->DeleteLocalRef(env, ex_class);
    }
  }
  
  if(jex) {
    (*env)->Throw(env, jex);
    (*env)->DeleteLocalRef(env, jex);
  }
  
  (*env)->DeleteLocalRef(env, throwable_class);
  pmath_unref(ex);
  return TRUE;
}


void pjvm_ensure_started(void) {
  pmath_t pjvm = pjvm_try_get();
  
  if(!pmath_is_null(pjvm)) {
    pmath_unref(pjvm);
    return;
  }
  
  pmath_unref(pmath_evaluate(pmath_expr_new(
                               pmath_ref(PJ_SYMBOL_JAVASTARTVM), 0)));
}

pmath_t pj_builtin_startvm(pmath_expr_t expr) {
  pmath_messages_t msg;
  pmath_t pjvm;
  
  if(pmath_expr_length(expr) > 0) {
    pmath_message_argxxx(pmath_expr_length(expr), 0, 0);
    return expr;
  }
  
  pjvm = pjvm_try_get();
  if(!pmath_is_null(pjvm)) {
    pmath_unref(pjvm);
    pmath_unref(expr);
    return PMATH_NULL;
  }
  
  msg = pmath_thread_get_queue();
  pmath_unref(msg);
  if(pmath_same(msg, jvm_main_mq)) {
    pmath_unref(expr);
    
    pmath_atomic_lock(&vm_lock);
    {
      if(pmath_is_null(vm)) {
        if(load_jvm_library()) {
          jint      err;
          JavaVM   *jvm = NULL;
          JNIEnv   *env = NULL;
          jvmtiEnv *jvmti = NULL;
          jsize     num_vms;
          
          if(_JNI_GetCreatedJavaVMs(&jvm, 1, &num_vms) == JNI_OK && jvm) {
            pmath_debug_print("[attaching to running jvm]\n");
            (*jvm)->AttachCurrentThreadAsDaemon(jvm, (void**)&env, NULL);
            (*jvm)->GetEnv(jvm, (void**)&env, JNI_VERSION_1_4);
          }
          else {
            JavaVMInitArgs vm_args;
            JavaVMOption opt[6];
            jint nOptions = 0;
            char *classpath = NULL;
            
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
              
              if(pmath_is_string(cp))
                cp = pmath_build_value("(o)", cp);
                
              if( pmath_is_expr_of(cp, PMATH_SYMBOL_LIST) &&
                  pmath_expr_length(cp) > 0)
              {
                pmath_string_t s = PMATH_C_STRING("-Djava.class.path=");
                size_t i;
                
                for(i = 1; i <= pmath_expr_length(cp); ++i) {
                  pmath_t item = pmath_expr_get_item(cp, i);
                  
                  if(!pmath_is_string(item)) {
                    pmath_unref(item);
                    pmath_unref(s);
                    s = PMATH_NULL;
                    break;
                  }
                  
                  if(i > 1) {
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
                
                if(!pmath_is_null(s))
                  classpath = pmath_string_to_utf8(s, NULL);
                  
                pmath_unref(s);
              }
              
              pmath_unref(cp);
            }
            
            if(classpath) {
              opt[nOptions].optionString = classpath;
              ++nOptions;
            }
            
            
            vm_args.version  = JNI_VERSION_1_4;
            vm_args.nOptions = nOptions;
            vm_args.options  = opt;
            
            err = _JNI_CreateJavaVM(&jvm, (void**)&env, &vm_args);
            if(err != JNI_OK) {
              pmath_debug_print("JNI_CreateJavaVM failed with %d\n", (int)err);
            }
            
            pmath_mem_free(classpath);
          }
          
          if(env) {
            jclass exclass = (*env)->FindClass(env, "pmath/InternalException");
            
            if(!exclass) {
              pmath_debug_print("[cannot find pmath.InternalException class]\n");
              
              exclass = (*env)->FindClass(env, "java/lang/ThreadDeath");
            }
            
            if(exclass) {
              jmethodID mid = (*env)->GetMethodID(env, exclass, "<init>", "()V");
              
              if(mid) {
                jobject ex = (*env)->NewObject(env, exclass, mid);
                
                if(ex) {
                  pjvm_internal_exception = (*env)->NewGlobalRef(env, ex);
                  
                  (*env)->DeleteLocalRef(env, ex);
                }
              }
              
              (*env)->DeleteLocalRef(env, exclass);
            }
          }
          
          vm = create_pjvm(jvm);
          
          jvmti = pjvm_get_jvmti(vm);
          if(jvmti) {
            jvmtiCapabilities cap;
            jvmtiError err;
            
            memset(&cap, 0, sizeof(cap));
            cap.can_signal_thread = 1;
            err = (*jvmti)->AddCapabilities(jvmti, &cap);
            
            if(err != JVMTI_ERROR_NONE) {
              pmath_debug_print("[cannot possess can_signal_thread: err = %d]\n", (int)err);
            }
          }
          
          
          // setting the security manager
          if(env && JNI_OK == (*env)->EnsureLocalCapacity(env, 4)) {
            jclass system = (*env)->FindClass(env, "java/lang/System");
            
            if(system) {
              jmethodID mid = (*env)->GetStaticMethodID(env, system, "setSecurityManager", "(Ljava/lang/SecurityManager;)V");
              
              if(mid) {
                jobject sm_class = (*env)->FindClass(env, "pmath/NoExitSecurityManager");
                
                if(sm_class) {
                  jmethodID ctor = (*env)->GetMethodID(env, sm_class, "<init>", "()V");
                  
                  if(ctor) {
                    jobject sm = (*env)->NewObject(env, sm_class, ctor);
                    
                    if(sm) {
                      (*env)->CallStaticVoidMethod(env, system, mid, sm);
                      
                      (*env)->DeleteLocalRef(env, sm);
                    }
                  }
                  
                  (*env)->DeleteLocalRef(env, sm_class);
                }
              }
              
              mid = (*env)->GetStaticMethodID(env, system, "setProperty", "(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;");
              if(mid) {
                jvalue args[2];
                args[0].l = (*env)->NewStringUTF(env, "pmath.binding.dll");
                
                if(args[0].l) {
                  args[1].l = pj_string_to_java(env, pmath_ref(pjvm_dll_filename));
                  
                  if(args[1].l) {
                    jobject result = (*env)->CallStaticObjectMethodA(env, system, mid, args);
                    
                    if(result)
                      (*env)->DeleteLocalRef(env, result);
                    (*env)->DeleteLocalRef(env, args[1].l);
                  }
                  
                  (*env)->DeleteLocalRef(env, args[0].l);
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
  else {
    pmath_unref(pmath_thread_send_wait(jvm_main_mq, expr, HUGE_VAL, NULL, NULL));
  }
  
  pjvm = pjvm_try_get();
  pmath_unref(pjvm);
  return pmath_is_null(pjvm) ? pmath_ref(PMATH_SYMBOL_FAILED) : PMATH_NULL;
}


pmath_bool_t pjvm_init(void) {
  pmath_atomic_write_release(&vm_lock, 0);
  jvm_main_mq = pmath_thread_fork_daemon(jvm_main, jvm_main_kill, NULL);
  return !pmath_is_null(jvm_main_mq);
}

void pjvm_done(void) {
  pmath_unref(pmath_thread_local_save(pjvm_auto_detach_key, PMATH_UNDEFINED));
  
  pjvm_register_external(NULL);
  pmath_unref(jvm_main_mq); jvm_main_mq = PMATH_NULL;
  
  if(vm_library_counter > 0) {
    vm_library_counter = 1;
    unload_jvm_callback(NULL);
  }
}
