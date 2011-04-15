#include "pj-threads.h"
#include "pj-symbols.h"
#include "pjvm.h"

#include <math.h>
#include <string.h>

#ifdef PMATH_OS_WIN32
  #define NOGDI
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>

  typedef HANDLE sem_t;

  static int sem_init(sem_t *sem, int pshared, unsigned int value){
    *sem = CreateSemaphore(0, value, 0x7FFFFFFF, 0);
    return *sem == 0 ? -1 : 0;
  }

  static int sem_destroy(sem_t *sem){
    return CloseHandle(*sem) ? 0 : -1;
  }

  static int sem_wait(sem_t *sem){
    errno = 0;
    return WaitForSingleObject(*sem, INFINITE) == WAIT_OBJECT_0 ? 0 : -1;
  }

  static int sem_post(sem_t *sem){
    return ReleaseSemaphore(*sem, 1, 0) ? 0 : -1;
  }
  
#else
  #include <semaphore.h>
#endif


static pmath_t cothread_key = PMATH_STATIC_NULL;
static pmath_atomic_t bye_companions = PMATH_ATOMIC_STATIC_INIT;

  struct cothread_t{
    pmath_messages_t pmath;
    jthread          java;   // global reference
  };

static void cothread_destructor(void *p){
  struct cothread_t *ct = (struct cothread_t*)p;
  JNIEnv *env = pjvm_try_get_env();
  
  pmath_debug_print("[cothread_destructor data=%p...]\n", p);
  
  if(!pmath_atomic_read_aquire(&bye_companions)){
    pmath_debug_print("[inform companion ...]\n");
    
    // inform our companion of our end
    pmath_thread_send(ct->pmath,
      pmath_expr_new_extended(
        pmath_ref(PJ_SYMBOL_INTERNAL_STOPPEDCOTHREAD), 1,
        pmath_ref(ct->pmath)));
  }
  
  pmath_debug_print("[cothread_destructor companion refcount = %d]\n", (int)pmath_refcount(ct->pmath));
  
  pmath_unref(ct->pmath);
  
  if(env){
    (*env)->DeleteGlobalRef(env, ct->java);
  }
  else{
    pmath_debug_print("[cothread_destructor: pjvm_try_get_env() failed]\n");
  }
  
  pmath_mem_free(ct);
}

static void kill_companion(void *dummy){
  pmath_atomic_write_release(&bye_companions, 1);
}
  
  struct companion_proc_info_t{
    pmath_messages_t  in_creator;
    jthread           in_jthread;
    
    jthread           out_jthread;
    sem_t             init_finished;
  };
  
static void companion_proc(void *p){
  struct companion_proc_info_t *info = (struct companion_proc_info_t*)p;
  pmath_t pjvm;
  jvmtiEnv *jvmti;
  JNIEnv   *env;
  
  info->out_jthread = NULL;
  
  pjvm_ensure_started();
  pjvm = pjvm_try_get();
  jvmti = pjvm_get_jvmti(pjvm);
  env = pjvm_get_env();
  
  if(jvmti && env){
    pmath_custom_t other = PMATH_STATIC_NULL;
    struct cothread_t *ct = pmath_mem_alloc(sizeof(struct cothread_t));
    
    if(ct){
      jobject thread = NULL;
      (*jvmti)->GetCurrentThread(jvmti, &thread);
      
      if(thread){
        info->out_jthread = (*env)->NewGlobalRef(env, thread);
        ct->java          = info->in_jthread; info->in_jthread = NULL;
        
        if(info->out_jthread && ct->java){
          pmath_debug_print("[in_creator = %p]\n", PMATH_AS_PTR(info->in_creator));
          ct->pmath = info->in_creator;
          info->in_creator = PMATH_NULL;
          
          pmath_debug_print("[new cothread data=%p]\n", ct);
          other = pmath_custom_new(ct, cothread_destructor);
        }
        else{
          if(ct->java)
            (*env)->DeleteGlobalRef(env, ct->java);
          
          pmath_mem_free(ct);
        }
        
        (*env)->DeleteLocalRef(env, thread);
      }
      else
        pmath_mem_free(ct);
    }
    
    pmath_debug_print("[new cothread %p]\n", PMATH_AS_PTR(other));
    
    pmath_unref(pmath_thread_local_save(
      cothread_key, 
      pmath_ref(other)));
    
    if(pmath_is_null(other) || pmath_refcount(other) == 1){
      if(info->out_jthread){
        (*env)->DeleteGlobalRef(env, info->out_jthread);
        info->out_jthread = NULL;
      }
    }
    
    pmath_unref(other);
  }
  
  pmath_unref(pjvm);
  sem_post(&info->init_finished);
  
  while(!pmath_atomic_read_aquire(&bye_companions)){
    pmath_t cc = pmath_thread_local_load(cothread_key);
    
    if(!pmath_is_custom(cc)
    || !pmath_custom_has_destructor(cc, cothread_destructor)){
      pmath_debug_print("[lost cothread]\n");
      pmath_unref(cc);
      break;
    }
    
    pmath_unref(cc);
    pmath_thread_sleep();
  }
  
  pmath_unref(pmath_thread_local_save(
    cothread_key, 
    PMATH_UNDEFINED));
  
  pmath_debug_print("[cothread bye]\n");
}

void pj_thread_message(
  pmath_messages_t mq, // wont be freed
  pmath_symbol_t sym,  // wont be freed
  const char *tag,
  size_t argcount,
  ...                  // pmath_t[argcount]   will all be freed
){
  pmath_t expr;
  va_list items;
  size_t i;
  
  va_start(items, argcount);
  expr = pmath_expr_new(pmath_ref(PMATH_SYMBOL_MESSAGE), argcount + 1);
  
  expr = pmath_expr_set_item(expr, 1,
    pmath_expr_new_extended(
      pmath_ref(PMATH_SYMBOL_MESSAGENAME), 2,
      pmath_ref(sym),
      PMATH_C_STRING(tag)));
  
  for(i = 0;i < argcount;++i){
    pmath_t item = va_arg(items, pmath_t);
    
    if(!pmath_is_evaluated(item))
      item = pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_HOLDFORM), 1,
        item);
        
    expr = pmath_expr_set_item(expr, 2+i, item);
  }
  
  va_end(items);
  
  if(pmath_is_null(mq))
    pmath_unref(pmath_evaluate(expr));
  else
    pmath_unref(pmath_thread_send_wait(mq, expr, HUGE_VAL, NULL, NULL));
}

pmath_messages_t pj_thread_get_companion(jthread *out_jthread){
  pmath_messages_t companion;
  pmath_t cc = pmath_thread_local_load(cothread_key);
  struct companion_proc_info_t info;
  pmath_t pjvm;
  jvmtiEnv *jvmti;
  JNIEnv   *env;
  
  if(out_jthread)
    *out_jthread = NULL;
  
  if(pmath_is_custom(cc)
  && pmath_custom_has_destructor(cc, cothread_destructor)){
    struct cothread_t *data = (struct cothread_t*)pmath_custom_get_data(cc);
    companion = pmath_ref(data->pmath);
    
    if(out_jthread){
      pjvm_ensure_started();
      env = pjvm_get_env();
      
      if(env)
        *out_jthread = (*env)->NewLocalRef(env, data->java);
    }
    
    pmath_unref(cc);
    return companion;
  }
  
  pmath_unref(cc);
  
  pjvm_ensure_started();
  pjvm  = pjvm_try_get();
  jvmti = pjvm_get_jvmti(pjvm);
  env = pjvm_get_env();
  if(!jvmti || !env){
    pmath_unref(pjvm);
    return PMATH_NULL;
  }
  
  memset(&info, 0, sizeof(info));
  info.in_creator = pmath_thread_get_queue();
  
  {
    jthread jt = NULL;
    (*jvmti)->GetCurrentThread(jvmti, &jt);
    
    if(jt){
      info.in_jthread = (*env)->NewGlobalRef(env, jt);
      
      (*env)->DeleteLocalRef(env, jt);
    }
  }
  if(!info.in_jthread){
    pmath_unref(info.in_creator);
    pmath_unref(pjvm);
    return PMATH_NULL;
  }
  
  if(sem_init(&info.init_finished, 0, 0) < 0){
    pmath_debug_print("sem_init failed\n");
    pmath_unref(info.in_creator);
    pmath_unref(pjvm);
    return PMATH_NULL;
  }
  
  companion = pmath_thread_fork_daemon(companion_proc, kill_companion, &info);
  sem_wait(&info.init_finished);
  sem_destroy(&info.init_finished);
  
  pmath_unref(info.in_creator);
  if(info.in_jthread)
    (*env)->DeleteGlobalRef(env, info.in_jthread);
  
  if(!info.out_jthread){
    pmath_unref(companion);
    pmath_unref(pjvm);
    return PMATH_NULL;
  }
  
  {
    struct cothread_t *data = pmath_mem_alloc(sizeof(struct cothread_t));
    
    if(!data){
      pmath_unref(companion);
      (*env)->DeleteGlobalRef(env, info.out_jthread);
      pmath_unref(pjvm);
      return PMATH_NULL;
    }
    
    data->pmath = companion;
    data->java  = info.out_jthread;
    
    cc = pmath_custom_new(data, cothread_destructor);
    pmath_debug_print("[register cothread %p, data=%p for companion %p]\n", 
      PMATH_AS_PTR(cc), data, PMATH_AS_PTR(companion));
      
    pmath_unref(pmath_thread_local_save(cothread_key, pmath_ref(cc)));
  }
  
  if(!pmath_is_null(cc) && pmath_refcount(cc) > 1){ // pmath_thread_local_save succeeded
    struct cothread_t *data = (struct cothread_t*)pmath_custom_get_data(cc);
    
    companion = pmath_ref(data->pmath);
    if(out_jthread)
      *out_jthread = (*env)->NewLocalRef(env, data->java);
    
    pmath_unref(cc);
    pmath_unref(pjvm);
    return companion;
  }
  
  pmath_unref(cc);
  pmath_unref(pjvm);
  return PMATH_NULL;
}

pmath_t pj_builtin_internal_stoppedcothread(pmath_expr_t expr){
  pmath_t cookie = pmath_expr_get_item(expr, 1);
  pmath_messages_t me = pmath_thread_get_queue();
  
  if(pmath_equals(me, cookie)){
    pmath_debug_print("[%p : cothread stopping]\n", PMATH_AS_PTR(me));
    pmath_unref(pmath_thread_local_save(cothread_key, PMATH_UNDEFINED));
  }
  else{
    pmath_debug_print("[stoppedcothread: cookie != me]\n");
  }
  
  pmath_unref(cookie);
  pmath_unref(me);
  pmath_unref(expr);
  return PMATH_NULL;
}



pmath_bool_t pj_threads_init(void){
  cothread_key = pmath_expr_new_extended(
    pmath_ref(PJ_SYMBOL_JAVA),1,
    PMATH_C_STRING("Cothread Key"));
  
  pmath_atomic_write_release(&bye_companions, 0);
  
  return !pmath_is_null(cothread_key);
}

void pj_threads_done(void){
  pmath_unref(cothread_key); cothread_key = PMATH_NULL;
}
