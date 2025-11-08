#include "pj-objects.h"
#include "pj-classes.h"
#include "pj-threads.h"
#include "pjvm.h"

#include <limits.h>


extern pmath_symbol_t pjsym_Java_Internal_JavaCall;
extern pmath_symbol_t pjsym_Java_Internal_JavaNew;
extern pmath_symbol_t pjsym_Java_Internal_Return;
extern pmath_symbol_t pjsym_Java_Java;
extern pmath_symbol_t pjsym_Java_JavaCall;
extern pmath_symbol_t pjsym_Java_JavaClass;
extern pmath_symbol_t pjsym_Java_JavaField;
extern pmath_symbol_t pjsym_Java_JavaObject;
extern pmath_symbol_t pjsym_Java_JavaNew;
extern pmath_symbol_t pjsym_Java_JavaNull;

extern pmath_symbol_t pjsym_System_DollarFailed;
extern pmath_symbol_t pjsym_System_Abort;
extern pmath_symbol_t pjsym_System_Assign;
extern pmath_symbol_t pjsym_System_False;
extern pmath_symbol_t pjsym_System_Finally;
extern pmath_symbol_t pjsym_System_Message;
extern pmath_symbol_t pjsym_System_MessageName;
extern pmath_symbol_t pjsym_System_Throw;
extern pmath_symbol_t pjsym_System_True;


//{ p2j_objects Hashtable implementation ...
struct obj_entry_t {
  pmath_string_t owner_name;
  jobject        jglobal;
};

static unsigned int java_hash(jobject jobj) {
  pmath_t pjvm = pjvm_try_get();
  jvmtiEnv *jvmti = pjvm_get_jvmti(pjvm);
  jint hash_code = 0;
  
  if(jvmti) {
    jvmtiError err = (*jvmti)->GetObjectHashCode(jvmti, jobj, &hash_code);
    if(err) {
      pmath_debug_print("[java_hash: GetObjectHashCode failed with error %d]\n", (int)err);
    }
  }
  else
    pmath_debug_print("[java_hash: pjvm_get_jvmti() failed]\n");
  
  pmath_unref(pjvm);
  return hash_code;
}

static void p2j_entry_destructor(void *p) {
  struct obj_entry_t *e = (struct obj_entry_t*)p;
  
  if(e) {
    JNIEnv *env = pjvm_get_env();
    
    pmath_t sym = pmath_symbol_get(e->owner_name, FALSE);
    if(!pmath_is_null(sym)) {
      pmath_symbol_set_value(sym, PMATH_NULL);
      pmath_unref(sym);
    }
    
    if(env) {
      (*env)->DeleteGlobalRef(env, e->jglobal);
    }
    else {
      pmath_debug_print("p2j_entry_destructor: pjvm_get_env() failed\n");
    }
    
    pmath_mem_free(e);
  }
}

static void j2p_dummy_entry_destructor(void *p) {
}

static unsigned int p2j_entry_hash(void *p) {
  struct obj_entry_t *e = (struct obj_entry_t*)p;
  return pmath_hash(e->owner_name);
}

static unsigned int j2p_entry_hash(void *p) {
  struct obj_entry_t *e = (struct obj_entry_t*)p;
  
  return java_hash(e->jglobal);
}

static pmath_bool_t p2j_entry_keys_equal(void *p1, void *p2) {
  struct obj_entry_t *e1 = (struct obj_entry_t*)p1;
  struct obj_entry_t *e2 = (struct obj_entry_t*)p2;
  return pmath_equals(e1->owner_name, e2->owner_name);
}

static pmath_bool_t j2p_entry_keys_equal(void *p1, void *p2) {
  struct obj_entry_t *e1 = (struct obj_entry_t*)p1;
  struct obj_entry_t *e2 = (struct obj_entry_t*)p2;
  
  JNIEnv *env = pjvm_get_env();
  if(env)
    return (*env)->IsSameObject(env, e1->jglobal, e2->jglobal);
    
  pmath_debug_print("j2p_entry_keys_equal: pjvm_get_env() failed\n");
  return e1->jglobal == e2->jglobal;
}

static unsigned int p2j_key_hash(void *p) {
  pmath_t k = *(pmath_t*)p;
  
  return pmath_hash(k);
}

static unsigned int j2p_key_hash(void *p) {
  jobject k = (jobject)p;
  
  return java_hash(k);
}

static pmath_bool_t p2j_entry_equals_key(void *pe, void *pk) {
  struct obj_entry_t *e = (struct obj_entry_t*)pe;
  pmath_t k = *(pmath_t*)pk;
  
  return pmath_equals(e->owner_name, k);
}

static pmath_bool_t j2p_entry_equals_key(void *pe, void *pk) {
  struct obj_entry_t *e = (struct obj_entry_t*)pe;
  jobject k = (jobject)pk;
  
  JNIEnv *env = pjvm_get_env();
  if(env)
    return (*env)->IsSameObject(env, e->jglobal, k);
    
  pmath_debug_print("j2p_entry_equals_key: pjvm_get_env() failed\n");
  return e->jglobal == k;
}

static pmath_ht_class_t p2j_class = {
  p2j_entry_destructor,
  p2j_entry_hash,
  p2j_entry_keys_equal,
  p2j_key_hash,
  p2j_entry_equals_key
};
static pmath_ht_class_t j2p_class = {
  j2p_dummy_entry_destructor,
  j2p_entry_hash,
  j2p_entry_keys_equal,
  j2p_key_hash,
  j2p_entry_equals_key
};
//}
static pmath_atomic_t p2j_lock = PMATH_ATOMIC_STATIC_INIT;
static pmath_hashtable_t p2j_objects;
static pmath_hashtable_t j2p_objects; // does not own the entries.

static void java_destructor(void *e) {
  struct obj_entry_t *p2j_entry;
  pmath_string_t name = PMATH_FROM_PTR(e);
  
  pmath_atomic_lock(&p2j_lock);
  {
    p2j_entry = (struct obj_entry_t*)pmath_ht_remove(p2j_objects, &name);
    if(p2j_entry) {
      struct obj_entry_t *j2p_entry;
      j2p_entry = pmath_ht_remove(j2p_objects, p2j_entry->jglobal);
    }
  }
  pmath_atomic_unlock(&p2j_lock);
  
  p2j_entry_destructor(p2j_entry);
  
  pmath_unref(name);
}

PMATH_PRIVATE pmath_t pj_object_from_java(JNIEnv *env, jobject jobj) {
  pmath_symbol_t symbol         = PMATH_NULL;
  pmath_string_t nice_classname = PMATH_NULL;
  
  if(!jobj)
    return PMATH_NULL;
  
  if(env && (*env)->EnsureLocalCapacity(env, 1) == JNI_OK) {
    jclass clazz = (*env)->GetObjectClass(env, jobj);
    
    pj_exception_to_pmath(env);
    if(clazz) {
      nice_classname = pj_class_get_nice_name(env, clazz);
      (*env)->DeleteLocalRef(env, clazz);
    }
    else
      return PMATH_NULL;
  }
  else
    return PMATH_NULL;
    
  pmath_atomic_lock(&p2j_lock);
  {
    struct obj_entry_t *entry = (struct obj_entry_t*)pmath_ht_search(j2p_objects, jobj);
    
    if(entry) {
      symbol = pmath_symbol_get(pmath_ref(entry->owner_name), FALSE);
    }
  }
  pmath_atomic_unlock(&p2j_lock);
  
  if(!pmath_is_null(symbol)) {
    return pmath_expr_new_extended(pmath_ref(pjsym_Java_JavaObject), 2,
                                   symbol,
                                   nice_classname);
  }
  
  symbol = pmath_symbol_create_temporary(PMATH_C_STRING("Java`Objects`javaObject"), TRUE);
  if(!pmath_is_null(symbol)) {
    struct obj_entry_t *entry;
    
    entry = (struct obj_entry_t*)pmath_mem_alloc(sizeof(struct obj_entry_t));
    if(entry) {
      struct obj_entry_t  *old_entry;
      struct obj_entry_t  *test_entry;
      pmath_string_t       name;
      pmath_custom_t       value;
      
      name  = pmath_symbol_name(symbol);
      value = pmath_custom_new(PMATH_AS_PTR(pmath_ref(name)), java_destructor);
      if(!pmath_is_null(value)) {
        entry->owner_name = pmath_ref(name);
        entry->jglobal    = (*env)->NewGlobalRef(env, jobj);
        
        pmath_atomic_lock(&p2j_lock);
        {
          old_entry = (struct obj_entry_t*)pmath_ht_search(j2p_objects, jobj);
          
          if(old_entry) {
            pmath_unref(symbol);
            symbol = pmath_symbol_get(pmath_ref(entry->owner_name), FALSE);
          }
          else {
            test_entry = (struct obj_entry_t*)pmath_ht_insert(p2j_objects, entry);
            
            if(test_entry) {
              assert(entry == test_entry);
            }
            else {
              test_entry = (struct obj_entry_t*)pmath_ht_insert(j2p_objects, entry);
              
              if(test_entry) {
                assert(entry == test_entry);
                entry = (struct obj_entry_t*)pmath_ht_remove(p2j_objects, &name);
              }
              else {
                entry = NULL;
              }
            }
          }
        }
        pmath_atomic_unlock(&p2j_lock);
        
        if(old_entry) {
          pmath_unref(value);
        }
        else {
          pmath_symbol_set_value(symbol, value);
          
          pmath_symbol_set_attributes(
            symbol,
            pmath_symbol_get_attributes(symbol)
            | PMATH_SYMBOL_ATTRIBUTE_PROTECTED);
        }
      }
      
      p2j_entry_destructor(entry); // entry may be NULL
      pmath_unref(name);
    }
  }
  
  if(!pmath_is_null(symbol)) {
    return pmath_expr_new_extended(pmath_ref(pjsym_Java_JavaObject), 2,
                                   symbol,
                                   nice_classname);
  }
  
  pmath_unref(nice_classname);
  return PMATH_NULL;
}

PMATH_PRIVATE jobject pj_object_to_java(JNIEnv *env, pmath_t obj) {
  if(!env) {
    pmath_unref(obj);
    return NULL;
  }
  
  if(pmath_is_expr_of(obj, pjsym_Java_JavaObject)) {
    pmath_t symbol = pmath_expr_get_item(obj, 1);
    pmath_unref(obj);
    obj = symbol;
  }
  else {
    pmath_unref(obj);
    return NULL;
  }
  
  if(pmath_is_symbol(obj)) {
    pmath_t value = pmath_symbol_get_value(obj);
    pmath_unref(obj);
    obj = value;
  }
  else {
    pmath_unref(obj);
    return NULL;
  }
  
  if( pmath_is_custom(obj) &&
      pmath_custom_has_destructor(obj, java_destructor))
  {
    pmath_string_t name = pmath_ref(PMATH_FROM_PTR(pmath_custom_get_data(obj)));
    jobject result = NULL;
    
    pmath_atomic_lock(&p2j_lock);
    {
      struct obj_entry_t *e = pmath_ht_search(p2j_objects, &name);
      
      if(e)
        result = (*env)->NewLocalRef(env, e->jglobal);
    }
    pmath_atomic_unlock(&p2j_lock);
    
    pmath_unref(name);
    pmath_unref(obj);
    return result;
  }
  
  pmath_unref(obj);
  return NULL;
}

PMATH_PRIVATE pmath_bool_t pj_object_is_java(JNIEnv *env, pmath_t obj) {
  pmath_t symbol;
  pmath_t value;
  
  if(!env)
    return FALSE;
    
  if(!pmath_is_expr_of(obj, pjsym_Java_JavaObject))
    return FALSE;
    
  symbol = pmath_expr_get_item(obj, 1);
  
  if(!pmath_is_symbol(symbol)) {
    pmath_unref(symbol);
    return FALSE;
  }
  
  value = pmath_symbol_get_value(symbol);
  pmath_unref(symbol);
  
  if(pmath_is_custom(value)
      && pmath_custom_has_destructor(value, java_destructor)) {
    pmath_unref(value);
    return TRUE;
  }
  
  pmath_unref(value);
  return FALSE;
}

static
PMATH_ATTRIBUTE_USE_RESULT
pmath_t wrap_exception_result(pmath_symbol_t head, pmath_t exception) {
  if(pmath_same(exception, PMATH_ABORT_EXCEPTION)) {
    return pmath_expr_new(
             pmath_ref(pjsym_System_Abort), 0);
  }
  
  if(pmath_is_evaluatable(exception) || pmath_is_magic(exception)) {
    pmath_t do_throw = pmath_expr_new_extended(
                         pmath_ref(pjsym_System_Throw), 1,
                         pmath_ref(exception));
    pmath_t message_name = pmath_expr_new_extended(
                             pmath_ref(pjsym_System_MessageName), 2,
                             pmath_ref(head),
                             PMATH_C_STRING("ex"));
    pmath_t do_message = pmath_expr_new_extended(
                           pmath_ref(pjsym_System_Message), 2,
                           message_name,
                           exception);
    return pmath_expr_new_extended(
             pmath_ref(pjsym_System_Finally), 2,
             do_message, 
             do_throw);
  }
  
  pmath_unref(exception);
  return pmath_ref(pjsym_Java_JavaNull);
}

PMATH_PRIVATE pmath_t pj_eval_Java_IsJavaObject(pmath_expr_t expr) {
  JNIEnv *env;
  pmath_t obj;
  
  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(0, 1, 1);
    return expr;
  }
  
  pjvm_ensure_started();
  env = pjvm_get_env();
  if(!env) {
    pj_exception_to_pmath(env);
    
    return expr;
  }
  
  obj = pmath_expr_get_item(expr, 1);
  pmath_unref(expr);
  
  if(pj_object_is_java(env, obj)) {
    pmath_unref(obj);
    return pmath_ref(pjsym_System_True);
  }
  
  pmath_unref(obj);
  return pmath_ref(pjsym_System_False);
}

PMATH_PRIVATE pmath_t pj_eval_Java_InstanceOf(pmath_expr_t expr) {
  JNIEnv *env;
  pmath_t result;
  
  if(pmath_expr_length(expr) != 2) {
    pmath_message_argxxx(0, 2, 2);
    return expr;
  }
  
  result = pmath_ref(pjsym_System_DollarFailed);
  pjvm_ensure_started();
  env = pjvm_get_env();
  if(env && (*env)->EnsureLocalCapacity(env, 2) == 0) {
    jclass clazz = pj_class_to_java(env, pmath_expr_get_item(expr, 2));
    if(clazz) {
      jobject obj = pj_object_to_java(env, pmath_expr_get_item(expr, 1));
      if(obj) {
        pmath_unref(result);
        if((*env)->IsInstanceOf(env, obj, clazz))
          result = pmath_ref(pjsym_System_True);
        else
          result = pmath_ref(pjsym_System_False);
          
        (*env)->DeleteLocalRef(env, obj);
      }
      else {
        pmath_t item = pmath_expr_get_item(expr, 1);
        if(pmath_same(item, pjsym_Java_JavaNull)) {
          pmath_unref(item);
          pmath_unref(result);
          result = pmath_ref(pjsym_System_False);
        }
        else
          pmath_message(pjsym_Java_Java, "noobj", 1, item);
      }
      
      (*env)->DeleteLocalRef(env, clazz);
    }
    else {
      pmath_message(pjsym_Java_Java, "nocls", 1, pmath_expr_get_item(expr, 2));
    }
  }
  
  pj_exception_to_pmath(env);
  
  pmath_unref(expr);
  return result;
}

PMATH_PRIVATE pmath_t pj_eval_Java_ParentClass(pmath_expr_t expr) {
  JNIEnv *env;
  pmath_t result;
  
  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(0, 1, 1);
    return expr;
  }
  
  result = pmath_ref(pjsym_System_DollarFailed);
  pjvm_ensure_started();
  env = pjvm_get_env();
  if(env && (*env)->EnsureLocalCapacity(env, 2) == 0) {
    jobject clazz = pj_class_to_java(env, pmath_expr_get_item(expr, 1));
    
    if(clazz) {
      jclass super = (*env)->GetSuperclass(env, clazz);
      
      if(super) {
        pmath_unref(result);
        result = pmath_expr_new_extended(
                   pmath_ref(pjsym_Java_JavaClass), 1,
                   pj_class_get_nice_name(env, super));
                   
        (*env)->DeleteLocalRef(env, super);
      }
      
      (*env)->DeleteLocalRef(env, clazz);
    }
    else {
      pmath_message(pjsym_Java_Java, "nocls", 1, pmath_expr_get_item(expr, 1));
    }
  }
  
  pj_exception_to_pmath(env);
  
  pmath_unref(expr);
  return result;
}

PMATH_PRIVATE pmath_t pj_eval_Java_JavaClassAsObject(pmath_expr_t expr) {
  JNIEnv *env;
  pmath_t result;
  
  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(0, 1, 1);
    return expr;
  }
  
  result = pmath_ref(pjsym_Java_JavaNull);
  pjvm_ensure_started();
  env = pjvm_get_env();
  if(env && (*env)->EnsureLocalCapacity(env, 2) == 0) {
    jobject clazz = pj_class_to_java(env, pmath_expr_get_item(expr, 1));
    
    if(clazz) {
      pmath_unref(result);
      
      result = pj_object_from_java(env, clazz);
      
      (*env)->DeleteLocalRef(env, clazz);
    }
    else {
      pmath_message(pjsym_Java_Java, "nocls", 1, pmath_expr_get_item(expr, 1));
    }
  }
  
  pj_exception_to_pmath(env);
  
  pmath_unref(expr);
  return result;
}

PMATH_PRIVATE pmath_t pj_eval_Java_Internal_Return(pmath_expr_t expr) {
  pmath_t result = pmath_expr_get_item(expr, 1);
  pmath_unref(expr);
  
  expr = pmath_expr_new(pmath_ref(pjsym_Java_Internal_Return), 0);
  
  pmath_debug_print("[Java`Internal`Return ...]\n");
  
  pmath_unref(pmath_thread_local_save(
                expr,
                result));
                
  pmath_unref(expr);
  return PMATH_NULL;
}

PMATH_PRIVATE pmath_t pj_eval_Java_Internal_JavaCall(pmath_expr_t expr) {
  JNIEnv *env;
  pmath_t result;
  pmath_t exception;
  pmath_t companion = pj_thread_get_companion(NULL);
  
  pmath_debug_print("[Java`Internal`JavaCall ...]\n");
  
  result = pmath_ref(pjsym_System_DollarFailed);
  pjvm_ensure_started();
  env = pjvm_get_env();
  if(env && (*env)->EnsureLocalCapacity(env, 2) == 0) {
    pmath_t obj;
    jobject jobj = NULL;
    pmath_bool_t is_static;
    
    obj = pmath_expr_get_item(expr, 1);
    if(pj_object_is_java(env, obj)) {
      jobj = pj_object_to_java(env, obj);
      is_static = FALSE;
    }
    else {
      jobj = pj_class_to_java(env, obj);
      is_static = TRUE;
      
      if(!jobj)
        pj_thread_message(companion,
                          pjsym_Java_Java, "nobcl", 1, pmath_expr_get_item(expr, 1));
    }
    
    if(jobj) {
      pmath_unref(result);
      result = pj_class_call_method(
                 env,
                 jobj,
                 is_static,
                 pmath_expr_get_item(expr, 2),
                 pmath_expr_get_item_range(expr, 3, SIZE_MAX),
                 companion);
                 
      (*env)->DeleteLocalRef(env, jobj);
    }
  }
  
  pj_exception_to_pmath(env);
  
  exception = pmath_catch();
  if(!pmath_same(exception, PMATH_UNDEFINED)) {
    pmath_unref(result);
    result = wrap_exception_result(pjsym_Java_JavaCall, exception);
  }
  
  pmath_unref(expr);
  expr = pmath_expr_new_extended(
           pmath_ref(pjsym_Java_Internal_Return), 1,
           result);
           
  pmath_thread_send(companion, expr);
  pmath_unref(companion);
  return PMATH_NULL;
}

PMATH_PRIVATE pmath_t pj_eval_Java_JavaCall(pmath_expr_t expr) {
  pmath_t pjvm;
  pmath_messages_t companion;
  pmath_t result;
  pmath_t result_key;
  JNIEnv *env;
  jvmtiEnv *jvmti;
  jthread jthread_obj;
  
  if(pmath_expr_length(expr) < 2) {
    pmath_message_argxxx(pmath_expr_length(expr), 2, SIZE_MAX);
    return expr;
  }
  
  pjvm_ensure_started();
  pjvm = pjvm_try_get();
  jvmti = pjvm_get_jvmti(pjvm);
  env = pjvm_get_env();
  if(!jvmti || !env) {
    pmath_unref(pjvm);
    pmath_unref(expr);
    return pmath_ref(pjsym_System_DollarFailed);
  }
  
  companion = pj_thread_get_companion(&jthread_obj);
  if(pmath_is_null(companion) || !jthread_obj) {
    pmath_unref(companion);
    
    if(jthread_obj)
      (*env)->DeleteLocalRef(env, jthread_obj);
      
    pmath_unref(expr);
    pmath_unref(pjvm);
    return pmath_ref(pjsym_System_DollarFailed);
  }
  
  result_key = pmath_expr_new(pmath_ref(pjsym_Java_Internal_Return), 0);
  pmath_unref(pmath_thread_local_save(result_key, PMATH_UNDEFINED));
  
  result = PMATH_UNDEFINED;
  if(!pmath_aborting()) {
    expr = pmath_expr_set_item(expr, 0,
                               pmath_ref(pjsym_Java_Internal_JavaCall));
    pmath_thread_send(companion, expr);
    expr = PMATH_NULL;
    
    while(pmath_same(result, PMATH_UNDEFINED)) {
      pmath_thread_sleep();
      
      if(pmath_aborting()) {
        double start_time = pmath_tickcount();
        
        pmath_debug_print("[aborting javacall...]\n");
        // give the thread a 2 second chance to react for the interrupt
        (*jvmti)->InterruptThread(jvmti, jthread_obj);
        
        do {
          pmath_thread_sleep_timeout(0.5);
          
          result = pmath_thread_local_load(result_key);
          
        } while(pmath_tickcount() - start_time < 2.0
                && pmath_same(result, PMATH_UNDEFINED));
                
        if(!pmath_same(result, PMATH_UNDEFINED))
          break;
          
        if(pjvm_internal_exception)
          (*jvmti)->StopThread(jvmti, jthread_obj, pjvm_internal_exception);
          
        break;
      }
      
      result = pmath_thread_local_load(result_key);
    }
  }
  
  (*env)->DeleteLocalRef(env, jthread_obj);
  
  pmath_unref(pmath_thread_local_save(result_key, PMATH_UNDEFINED));
  
  pmath_unref(expr);
  pmath_unref(companion);
  pmath_unref(result_key);
  pmath_unref(pjvm);
  return result;
}

PMATH_PRIVATE pmath_t pj_eval_Java_Internal_JavaNew(pmath_expr_t expr) {
  JNIEnv *env;
  pmath_t result;
  pmath_t exception;
  pmath_t companion = pj_thread_get_companion(NULL);
  
  result = pmath_ref(pjsym_System_DollarFailed);
  pjvm_ensure_started();
  env = pjvm_get_env();
  if(env && (*env)->EnsureLocalCapacity(env, 2) == 0) {
    jclass clazz;
    
    clazz = pj_class_to_java(env, pmath_expr_get_item(expr, 1));
    if(clazz) {
      pmath_expr_t args;
      jobject jresult;
      
      args = pmath_expr_get_item_range(expr, 2, SIZE_MAX);
      
      jresult = pj_class_new_object(env, clazz, args, PMATH_NULL);
      if(jresult) {
        pmath_unref(result);
        result = pj_object_from_java(env, jresult);
        (*env)->DeleteLocalRef(env, jresult);
      }
      
      (*env)->DeleteLocalRef(env, clazz);
    }
    else {
      pj_thread_message(companion,
                        pjsym_Java_JavaNew, "nocls", 1, pmath_expr_get_item(expr, 1));
    }
  }
  
  pj_exception_to_pmath(env);
  
  exception = pmath_catch();
  if(!pmath_same(exception, PMATH_UNDEFINED)) {
    pmath_unref(result);
    result = wrap_exception_result(pjsym_Java_JavaNew, exception);
  }
  
  pmath_unref(expr);
  expr = pmath_expr_new_extended(
           pmath_ref(pjsym_Java_Internal_Return), 1,
           result);
           
  pmath_thread_send(companion, expr);
  pmath_unref(companion);
  return PMATH_NULL;
}

PMATH_PRIVATE pmath_t pj_eval_Java_JavaNew(pmath_expr_t expr) {
  pmath_t pjvm;
  pmath_messages_t companion;
  pmath_t result;
  pmath_t result_key;
  JNIEnv *env;
  jvmtiEnv *jvmti;
  jthread jthread_obj;
  
  if(pmath_expr_length(expr) < 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, SIZE_MAX);
    return expr;
  }
  
  pjvm_ensure_started();
  pjvm = pjvm_try_get();
  jvmti = pjvm_get_jvmti(pjvm);
  env = pjvm_get_env();
  if(!jvmti || !env) {
    pmath_unref(pjvm);
    pmath_unref(expr);
    return pmath_ref(pjsym_System_DollarFailed);
  }
  
  companion = pj_thread_get_companion(&jthread_obj);
  if(pmath_is_null(companion) || !jthread_obj) {
    pmath_unref(companion);
    
    if(jthread_obj)
      (*env)->DeleteLocalRef(env, jthread_obj);
      
    pmath_unref(expr);
    pmath_unref(pjvm);
    return pmath_ref(pjsym_System_DollarFailed);
  }
  
  result_key = pmath_expr_new(pmath_ref(pjsym_Java_Internal_Return), 0);
  pmath_unref(pmath_thread_local_save(result_key, PMATH_UNDEFINED));
  
  result = PMATH_UNDEFINED;
  if(!pmath_aborting()) {
    expr = pmath_expr_set_item(expr, 0, pmath_ref(pjsym_Java_Internal_JavaNew));
    pmath_thread_send(companion, expr);
    expr = PMATH_NULL;
    
    while(pmath_same(result, PMATH_UNDEFINED)) {
      pmath_thread_sleep();
      
      if(pmath_aborting()) {
        double start_time = pmath_tickcount();
        
        pmath_debug_print("[aborting javanew...]\n");
        // give the thread a 2 second chance to react for the interrupt
        (*jvmti)->InterruptThread(jvmti, jthread_obj);
        
        do {
          pmath_thread_sleep_timeout(0.5);
          
          result = pmath_thread_local_load(result_key);
          
        } while(pmath_tickcount() - start_time < 2.0
                && pmath_same(result, PMATH_UNDEFINED));
                
        if(!pmath_same(result, PMATH_UNDEFINED))
          break;
          
        if(pjvm_internal_exception)
          (*jvmti)->StopThread(jvmti, jthread_obj, pjvm_internal_exception);
          
        break;
      }
      
      result = pmath_thread_local_load(result_key);
    }
  }
  
  (*env)->DeleteLocalRef(env, jthread_obj);
  
  pmath_unref(pmath_thread_local_save(result_key, PMATH_UNDEFINED));
  
  pmath_unref(expr);
  pmath_unref(companion);
  pmath_unref(result_key);
  pmath_unref(pjvm);
  return result;
}

PMATH_PRIVATE pmath_t pj_eval_Java_JavaField(pmath_expr_t expr) {
  JNIEnv *env;
  pmath_t result;
  
  if(pmath_expr_length(expr) != 2) {
    pmath_message_argxxx(0, 2, 2);
    return expr;
  }
  
  result = pmath_ref(pjsym_System_DollarFailed);
  pjvm_ensure_started();
  env = pjvm_get_env();
  if(env && (*env)->EnsureLocalCapacity(env, 2) == 0) {
    pmath_t obj;
    jobject jobj = NULL;
    pmath_bool_t is_static;
    
    obj = pmath_expr_get_item(expr, 1);
    if(pj_object_is_java(env, obj)) {
      jobj = pj_object_to_java(env, obj);
      is_static = FALSE;
    }
    else {
      jobj = pj_class_to_java(env, obj);
      is_static = TRUE;
      
      if(!jobj)
        pmath_message(pjsym_Java_Java, "nobcl", 1, pmath_expr_get_item(expr, 1));
    }
    
    if(jobj) {
      pmath_unref(result);
      result = pj_class_get_field(
                 env,
                 jobj,
                 is_static,
                 pmath_expr_get_item(expr, 2));
                 
      (*env)->DeleteLocalRef(env, jobj);
    }
  }
  
  pj_exception_to_pmath(env);
  
  pmath_unref(expr);
  return result;
}

PMATH_PRIVATE pmath_t pj_eval_upcall_Java_JavaField(pmath_expr_t expr) {
  JNIEnv *env;
  pmath_t lhs;
  pmath_bool_t success;
  
  if(!pmath_is_expr_of_len(expr, pjsym_System_Assign, 2))
    return expr;
    
  lhs = pmath_expr_get_item(expr, 1);
  if(!pmath_is_expr_of_len(lhs, pjsym_Java_JavaField, 2)) {
    pmath_unref(lhs);
    return expr;
  }
  
  success = FALSE;
  pjvm_ensure_started();
  env = pjvm_get_env();
  if(env && (*env)->EnsureLocalCapacity(env, 2) == 0) {
    pmath_t obj;
    jobject jobj = NULL;
    pmath_bool_t is_static;
    
    obj = pmath_evaluate(pmath_expr_get_item(lhs, 1));
    if(pj_object_is_java(env, obj)) {
      jobj = pj_object_to_java(env, obj);
      is_static = FALSE;
    }
    else {
      jobj = pj_class_to_java(env, obj);
      is_static = TRUE;
      
      if(!jobj)
        pmath_message(pjsym_Java_Java, "nobcl", 1, pmath_expr_get_item(lhs, 1));
    }
    
    if(jobj) {
      success = pj_class_set_field(
                  env,
                  jobj,
                  is_static,
                  pmath_expr_get_item(lhs, 2),
                  pmath_expr_get_item(expr, 2));
                  
      (*env)->DeleteLocalRef(env, jobj);
    }
  }
  
  pj_exception_to_pmath(env);
  
  pmath_unref(lhs);
  if(success) {
    pmath_t result = pmath_expr_get_item(expr, 2);
    pmath_unref(expr);
    return result;
  }
  
  pmath_unref(expr);
  return pmath_ref(pjsym_System_DollarFailed);
}

PMATH_PRIVATE void pj_objects_clear_cache(void) {
  pmath_hashtable_t new_p2j = pmath_ht_create(&p2j_class, 0);
  pmath_hashtable_t new_j2p = pmath_ht_create(&j2p_class, 0);
  
  if(new_p2j && new_j2p) {
    pmath_hashtable_t old_p2j;
    pmath_hashtable_t old_j2p;
    
    pmath_atomic_lock(&p2j_lock);
    {
      old_p2j = p2j_objects;
      old_j2p = j2p_objects;
      p2j_objects = new_p2j;
      j2p_objects = new_j2p;
    }
    pmath_atomic_unlock(&p2j_lock);
    
    pmath_ht_destroy(old_j2p);
    pmath_ht_destroy(old_p2j);
    return;
  }
  
  pmath_debug_print("pj_objects_clear_cache() failed\n");
  pmath_ht_destroy(new_p2j);
  pmath_ht_destroy(new_j2p);
}

PMATH_PRIVATE pmath_bool_t pj_objects_init(void) {
  p2j_objects = pmath_ht_create(&p2j_class, 0);
  if(!p2j_objects)
    goto FAIL_P2J;
    
  j2p_objects = pmath_ht_create(&j2p_class, 0);
  if(!j2p_objects)
    goto FAIL_J2P;
    
  return TRUE;
  
  pmath_ht_destroy(j2p_objects);
FAIL_J2P:   pmath_ht_destroy(p2j_objects);
FAIL_P2J:
  return FALSE;
}

PMATH_PRIVATE void pj_objects_done(void) {
  pmath_ht_destroy(j2p_objects); j2p_objects = NULL;
  pmath_ht_destroy(p2j_objects); p2j_objects = NULL;
}
