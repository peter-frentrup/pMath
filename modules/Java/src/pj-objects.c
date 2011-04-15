#include "pj-objects.h"
#include "pj-classes.h"
#include "pj-symbols.h"
#include "pj-threads.h"
#include "pjvm.h"

#include <limits.h>


static jmethodID midHashCode = NULL;

//{ p2j_objects Hashtable implementation ...
struct obj_entry_t{
  pmath_string_t owner_name;
  jobject        jglobal;
};
  
  static unsigned int java_hash(jobject jobj){
    JNIEnv *env = pjvm_get_env();
    
    if(env){
      if(!midHashCode && (*env)->EnsureLocalCapacity(env, 1) == 0){
        jclass clazz = (*env)->GetObjectClass(env, jobj);
        
        if(clazz){
          midHashCode = (*env)->GetMethodID(env, clazz, "hashCode", "()I");
          (*env)->DeleteLocalRef(env, clazz);
        }
      }
      
      if(midHashCode){
        int hash = (*env)->CallIntMethod(env, jobj, midHashCode);
        
        return (unsigned int)hash;
      }
      else{
        pmath_debug_print("java_hash: midHashCode unknown\n");
        return 0;
      }
    }
    
    pmath_debug_print("java_hash: pjvm_get_env() failed\n");
    return 0;
  }
  
  static void p2j_entry_destructor(void *p){
    struct obj_entry_t *e = (struct obj_entry_t*)p;
    
    if(e){
      JNIEnv *env = pjvm_get_env();
      
      pmath_t sym = pmath_symbol_get(e->owner_name, FALSE);
      if(!pmath_is_null(sym)){
        pmath_symbol_set_value(sym, PMATH_NULL);
        pmath_unref(sym);
      }
      
      if(env){
        (*env)->DeleteGlobalRef(env, e->jglobal);
      }
      else{
        pmath_debug_print("p2j_entry_destructor: pjvm_get_env() failed\n");
      }
      
      pmath_mem_free(e);
    }
  }
  
  static void j2p_dummy_entry_destructor(void *p){
  }
  
  static unsigned int p2j_entry_hash(void *p){
    struct obj_entry_t *e = (struct obj_entry_t*)p;
    return pmath_hash(e->owner_name);
  }
  
  static unsigned int j2p_entry_hash(void *p){
    struct obj_entry_t *e = (struct obj_entry_t*)p;
    
    return java_hash(e->jglobal);
  }
  
  static pmath_bool_t p2j_entry_keys_equal(void *p1, void *p2){
    struct obj_entry_t *e1 = (struct obj_entry_t*)p1;
    struct obj_entry_t *e2 = (struct obj_entry_t*)p2;
    return pmath_equals(e1->owner_name, e2->owner_name);
  }
  
  static pmath_bool_t j2p_entry_keys_equal(void *p1, void *p2){
    struct obj_entry_t *e1 = (struct obj_entry_t*)p1;
    struct obj_entry_t *e2 = (struct obj_entry_t*)p2;
    
    JNIEnv *env = pjvm_get_env();
    if(env)
      return (*env)->IsSameObject(env, e1->jglobal, e2->jglobal);
    
    pmath_debug_print("j2p_entry_keys_equal: pjvm_get_env() failed\n");
    return e1->jglobal == e2->jglobal;
  }
  
  static unsigned int p2j_key_hash(void *p){
    pmath_t k = *(pmath_t*)p;
    
    return pmath_hash(k);
  }
  
  static unsigned int j2p_key_hash(void *p){
    jobject k = (jobject)p;
    
    return java_hash(k);
  }
  
  static pmath_bool_t p2j_entry_equals_key(void *pe, void *pk){
    struct obj_entry_t *e = (struct obj_entry_t*)pe;
    pmath_t k = *(pmath_t*)pk;
    
    return pmath_equals(e->owner_name, k);
  }
  
  static pmath_bool_t j2p_entry_equals_key(void *pe, void *pk){
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
  
  static void java_destructor(void *e){
    struct obj_entry_t *p2j_entry;
    pmath_string_t name = PMATH_FROM_PTR(e);
    
    pmath_atomic_lock(&p2j_lock);
    {
      p2j_entry = (struct obj_entry_t*)pmath_ht_remove(p2j_objects, &name);
      if(p2j_entry){
        struct obj_entry_t *j2p_entry;
        j2p_entry = pmath_ht_remove(j2p_objects, p2j_entry->jglobal);
      }
    }
    pmath_atomic_unlock(&p2j_lock);
    
    p2j_entry_destructor(p2j_entry);
    
    pmath_unref(name);
  }

pmath_t pj_object_from_java(JNIEnv *env, jobject jobj){
  pmath_symbol_t symbol = PMATH_NULL;
  
  pmath_atomic_lock(&p2j_lock);
  {
    struct obj_entry_t *entry = (struct obj_entry_t*)pmath_ht_search(j2p_objects, jobj);
    
    if(entry){
      symbol = pmath_symbol_get(pmath_ref(entry->owner_name), FALSE);
    }
  }
  pmath_atomic_unlock(&p2j_lock);
  
  if(!pmath_is_null(symbol))
    return symbol;
  
  if(!env || (*env)->EnsureLocalCapacity(env, 1) != 0){
    return PMATH_NULL;
  }
  
  symbol = pmath_symbol_create_temporary(PMATH_C_STRING("Java`Objects`javaObject"), TRUE);
  if(!pmath_is_null(symbol)){
    struct obj_entry_t *entry;
    
    entry = (struct obj_entry_t*)pmath_mem_alloc(sizeof(struct obj_entry_t));
    if(entry){
      struct obj_entry_t  *old_entry;
      struct obj_entry_t  *test_entry;
      pmath_string_t       name;
      pmath_custom_t       value;
      
      name  = pmath_symbol_name(symbol);
      value = pmath_custom_new(PMATH_AS_PTR(pmath_ref(name)), java_destructor);
      if(!pmath_is_null(value)){
        entry->owner_name = pmath_ref(name);
        entry->jglobal    = (*env)->NewGlobalRef(env, jobj);
        
        pmath_atomic_lock(&p2j_lock);
        {
          old_entry = (struct obj_entry_t*)pmath_ht_search(j2p_objects, jobj);
          
          if(old_entry){
            pmath_unref(symbol);
            symbol = pmath_symbol_get(pmath_ref(entry->owner_name), FALSE);
          }
          else{
            test_entry = (struct obj_entry_t*)pmath_ht_insert(p2j_objects, entry);
            
            if(test_entry){
              assert(entry == test_entry);
            }
            else{
              test_entry = (struct obj_entry_t*)pmath_ht_insert(j2p_objects, entry);
              
              if(test_entry){
                assert(entry == test_entry);
                entry = (struct obj_entry_t*)pmath_ht_remove(p2j_objects, &name);
              }
              else{
                entry = NULL;
              }
            }
          }
        }
        pmath_atomic_unlock(&p2j_lock);
        
        if(old_entry){
          pmath_unref(value);
        }
        else{
          jclass clazz;
          pmath_symbol_set_value(symbol, value);
          
          clazz = (*env)->GetObjectClass(env, jobj);
          if(clazz){
            pmath_string_t class_name  = pj_class_get_nice_name(env, clazz);
            pmath_t str = pmath_ref(class_name);
            (*env)->DeleteLocalRef(env, clazz);
            
            str = pmath_string_insert_latin1(str, INT_MAX, "\xA0\xBB", 2);
            str = pmath_string_insert_latin1(str, 0,       "\xAB\xA0", 2);
            
            PMATH_RUN_ARGS(
                "MakeBoxes(`1`)::= InterpretationBox(`2`, `1`)", 
              "(oo)", 
              pmath_ref(symbol),
              str);
            
            PMATH_RUN_ARGS(
                "`1`/: Java`ClassName(`1`)::= `2`", 
              "(oo)", 
              pmath_ref(symbol),
              pmath_ref(class_name));
            
            PMATH_RUN_ARGS(
                "`1`/: Java`GetClass(`1`)::= Java`JavaClass(`2`)", 
              "(oo)", 
              pmath_ref(symbol),
              class_name);
              
            PMATH_RUN_ARGS(
                "`1` @ (~f:String)::= Java`JavaField(`1`, f);"
                "`1` @ (~f:Symbol)::= Java`JavaField(`1`, SymbolName(f));"
                
                "`1` /: (`1` @ (~f:String):= ~rhs)::= Java`JavaField(`1`, f):= rhs;"
                "`1` /: (`1` @ (~f:Symbol):= ~rhs)::= With({s:= SymbolName(f)}, Java`JavaField(`1`, s):= rhs);"
                
                "`1` @ (~m:String)(~~~args)::= Java`JavaCall(`1`, m, args);"
                "`1` @ (~m:Symbol)(~~~args)::= Java`JavaCall(`1`, SymbolName(m), args);"
                "`1` @ (e1: ~(~~~))(~e2)::= (`1` @ e1) @ e2;",
              "(o)", 
              pmath_ref(symbol));
            
            pmath_symbol_set_attributes(symbol, 
              pmath_symbol_get_attributes(symbol) 
              | PMATH_SYMBOL_ATTRIBUTE_HOLDALL
              | PMATH_SYMBOL_ATTRIBUTE_PROTECTED);
            
          }
        }
      }
      
      p2j_entry_destructor(entry);
      pmath_unref(name);
    }
  }
  
  return symbol;
}

jobject pj_object_to_java(JNIEnv *env, pmath_t obj){
  if(!env){
    pmath_unref(obj);
    return NULL;
  }
  
  if(pmath_is_symbol(obj)){
    pmath_t value = pmath_symbol_get_value(obj);
    pmath_unref(obj);
    obj = value;
  }
  
  if(pmath_is_custom(obj)
  && pmath_custom_has_destructor(obj, java_destructor)){
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


pmath_bool_t pj_object_is_java(JNIEnv *env, pmath_t obj){
  pmath_t value;
  
  if(!env)
    return FALSE;
  
  if(pmath_is_symbol(obj)){
    value = pmath_symbol_get_value(obj);
  }
  else
    value = pmath_ref(obj);
  
  if(pmath_is_custom(value)
  && pmath_custom_has_destructor(value, java_destructor)){
    pmath_unref(value);
    return TRUE;
  }
  
  pmath_unref(value);
  return FALSE;
}


pmath_t pj_builtin_isjavaobject(pmath_expr_t expr){
  JNIEnv *env;
  pmath_t obj;
  
  if(pmath_expr_length(expr) != 1){
    pmath_message_argxxx(0, 1, 1);
    return expr;
  }
  
  pjvm_ensure_started();
  env = pjvm_get_env();
  if(!env){
    pj_exception_to_pmath(env);
    
    return expr;
  }
  
  obj = pmath_expr_get_item(expr, 1);
  pmath_unref(expr);
  
  if(pj_object_is_java(env, obj)){
    pmath_unref(obj);
    return pmath_ref(PMATH_SYMBOL_TRUE);
  }
  
  pmath_unref(obj);
  return pmath_ref(PMATH_SYMBOL_FALSE);
}


pmath_t pj_builtin_instanceof(pmath_expr_t expr){
  JNIEnv *env;
  pmath_t result;
  
  if(pmath_expr_length(expr) != 2){
    pmath_message_argxxx(0, 2, 2);
    return expr;
  }
  
  result = pmath_ref(PMATH_SYMBOL_FAILED);
  pjvm_ensure_started();
  env = pjvm_get_env();
  if(env && (*env)->EnsureLocalCapacity(env, 2) == 0){
    jobject obj = pj_object_to_java(env, pmath_expr_get_item(expr, 1));
    
    if(obj){
      jclass clazz = pj_class_to_java(env, pmath_expr_get_item(expr, 2));
      
      if(clazz){
        pmath_unref(result);
        if((*env)->IsInstanceOf(env, obj, clazz))
          result = pmath_ref(PMATH_SYMBOL_TRUE);
        else
          result = pmath_ref(PMATH_SYMBOL_FALSE);
        
        (*env)->DeleteLocalRef(env, clazz);
      }
      else{
        pmath_message(PJ_SYMBOL_JAVA, "nocls", 1, pmath_expr_get_item(expr, 2));
      }
        
      (*env)->DeleteLocalRef(env, obj);
    }
    else{
      pmath_message(PJ_SYMBOL_JAVA, "noobj", 1, pmath_expr_get_item(expr, 1));
    }
  }
  
  pj_exception_to_pmath(env);
  
  pmath_unref(expr);
  return result;
}


pmath_t pj_builtin_parentclass(pmath_expr_t expr){
  JNIEnv *env;
  pmath_t result;
  
  if(pmath_expr_length(expr) != 1){
    pmath_message_argxxx(0, 1, 1);
    return expr;
  }
  
  result = pmath_ref(PMATH_SYMBOL_FAILED);
  pjvm_ensure_started();
  env = pjvm_get_env();
  if(env && (*env)->EnsureLocalCapacity(env, 2) == 0){
    jobject clazz = pj_class_to_java(env, pmath_expr_get_item(expr, 1));
    
    if(clazz){
      jclass super = (*env)->GetSuperclass(env, clazz);
      
      if(super){
        pmath_unref(result);
        result = pmath_expr_new_extended(
          pmath_ref(PJ_SYMBOL_JAVACLASS), 1,
          pj_class_get_nice_name(env, super));
        
        (*env)->DeleteLocalRef(env, super);
      }
      
      (*env)->DeleteLocalRef(env, clazz);
    }
    else{
      pmath_message(PJ_SYMBOL_JAVA, "nocls", 1, pmath_expr_get_item(expr, 1));
    }
  }
  
  pj_exception_to_pmath(env);
  
  pmath_unref(expr);
  return result;
}


pmath_t pj_builtin_javaclassasobject(pmath_expr_t expr){
  JNIEnv *env;
  pmath_t result;
  
  if(pmath_expr_length(expr) != 1){
    pmath_message_argxxx(0, 1, 1);
    return expr;
  }
  
  result = pmath_ref(PMATH_SYMBOL_FAILED);
  pjvm_ensure_started();
  env = pjvm_get_env();
  if(env && (*env)->EnsureLocalCapacity(env, 2) == 0){
    jobject clazz = pj_class_to_java(env, pmath_expr_get_item(expr, 1));
    
    if(clazz){
      pmath_unref(result);
      
      result = pj_object_from_java(env, clazz);
      
      (*env)->DeleteLocalRef(env, clazz);
    }
    else{
      pmath_message(PJ_SYMBOL_JAVA, "nocls", 1, pmath_expr_get_item(expr, 1));
    }
  }
  
  pj_exception_to_pmath(env);
  
  pmath_unref(expr);
  return result;
}


pmath_t pj_builtin_internal_return(pmath_expr_t expr){
  pmath_t result = pmath_expr_get_item(expr, 1);
  pmath_unref(expr);
  
  expr = pmath_expr_new(pmath_ref(PJ_SYMBOL_INTERNAL_RETURN), 0);
  
  pmath_debug_print("[Java`Internal`Return ...]\n");
  
  pmath_unref(pmath_thread_local_save(
    expr,
    result));
  
  pmath_unref(expr);
  return PMATH_NULL;
}

pmath_t pj_builtin_internal_javacall(pmath_expr_t expr){
  JNIEnv *env;
  pmath_t result;
  pmath_t exception;
  pmath_t companion = pj_thread_get_companion(NULL);
  
  pmath_debug_print("[Java`Internal`JavaCall ...]\n");
  
  result = pmath_ref(PMATH_SYMBOL_FAILED);
  pjvm_ensure_started();
  env = pjvm_get_env();
  if(env && (*env)->EnsureLocalCapacity(env, 2) == 0){
    pmath_t obj;
    jobject jobj = NULL;
    pmath_bool_t is_static;
    
    obj = pmath_expr_get_item(expr, 1);
    if(pj_object_is_java(env, obj)){
      jobj = pj_object_to_java(env, obj);
      is_static = FALSE;
    }
    else{
      jobj = pj_class_to_java(env, obj);
      is_static = TRUE;
      
      if(!jobj)
        pj_thread_message(companion, 
          PJ_SYMBOL_JAVA, "nobcl", 1, pmath_expr_get_item(expr, 1));
    }
    
    if(jobj){
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
  if(!pmath_same(exception, PMATH_UNDEFINED)){
    
    if(pmath_same(exception, PMATH_ABORT_EXCEPTION)){
      pmath_unref(result);
      result = pmath_expr_new(
        pmath_ref(PMATH_SYMBOL_ABORT), 0);
    }
    else if(pmath_is_evaluatable(exception)){
      pmath_unref(result);
      result = pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_THROW), 1,
        exception);
    }
    else{
      pmath_unref(exception);
      pmath_unref(result);
      result = pmath_ref(PMATH_SYMBOL_FAILED);
    }
  }
  
  pmath_unref(expr);
  expr = pmath_expr_new_extended(
    pmath_ref(PJ_SYMBOL_INTERNAL_RETURN), 1,
    result);
  
  pmath_thread_send(companion, expr);
  pmath_unref(companion);
  return PMATH_NULL;
}

pmath_t pj_builtin_javacall(pmath_expr_t expr){
  pmath_t pjvm;
  pmath_messages_t companion;
  pmath_t result;
  pmath_t result_key;
  JNIEnv *env;
  jvmtiEnv *jvmti;
  jthread jthread_obj;
  
  if(pmath_expr_length(expr) < 2){
    pmath_message_argxxx(pmath_expr_length(expr), 2, SIZE_MAX);
    return expr;
  }
  
  pjvm_ensure_started();
  pjvm = pjvm_try_get();
  jvmti = pjvm_get_jvmti(pjvm);
  env = pjvm_get_env();
  if(!jvmti || !env){
    pmath_unref(pjvm);
    pmath_unref(expr);
    return pmath_ref(PMATH_SYMBOL_FAILED);
  }
  
  companion = pj_thread_get_companion(&jthread_obj);
  if(pmath_is_null(companion) || !jthread_obj){
    pmath_unref(companion);
    
    if(jthread_obj)
      (*env)->DeleteLocalRef(env, jthread_obj);
    
    pmath_unref(expr);
    pmath_unref(pjvm);
    return pmath_ref(PMATH_SYMBOL_FAILED);
  }
  
  result_key = pmath_expr_new(pmath_ref(PJ_SYMBOL_INTERNAL_RETURN), 0);
  pmath_unref(pmath_thread_local_save(result_key, PMATH_UNDEFINED));
  
  result = PMATH_UNDEFINED;
  if(!pmath_aborting()){
    expr = pmath_expr_set_item(expr, 0, 
      pmath_ref(PJ_SYMBOL_INTERNAL_JAVACALL));
    pmath_thread_send(companion, expr); 
    expr = PMATH_NULL;
    
    while(pmath_same(result, PMATH_UNDEFINED)){
      pmath_thread_sleep();
      
      if(pmath_aborting()){
        double start_time = pmath_tickcount();
        
        pmath_debug_print("[aborting javacall...]\n");
        // give the thread a 2 second chance to react for the interrupt
        (*jvmti)->InterruptThread(jvmti, jthread_obj);
        
        do{
          pmath_thread_sleep_timeout(0.5);
              
          result = pmath_thread_local_load(result_key);
          
        }while(pmath_tickcount() - start_time < 2.0 
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


pmath_t pj_builtin_internal_javanew(pmath_expr_t expr){
  JNIEnv *env;
  pmath_t result;
  pmath_t exception;
  pmath_t companion = pj_thread_get_companion(NULL);
  
  
  result = pmath_ref(PMATH_SYMBOL_FAILED);
  pjvm_ensure_started();
  env = pjvm_get_env();
  if(env && (*env)->EnsureLocalCapacity(env, 2) == 0){
    jclass clazz;
    
    clazz = pj_class_to_java(env, pmath_expr_get_item(expr, 1));
    if(clazz){
      pmath_expr_t args;
      jobject jresult;
      
      args = pmath_expr_get_item_range(expr, 2, SIZE_MAX);
      
      jresult = pj_class_new_object(env, clazz, args, PMATH_NULL);
      if(jresult){
        (*env)->DeleteLocalRef(env, clazz);
        
        pmath_unref(result);
        result = pj_object_from_java(env, jresult);
        (*env)->DeleteLocalRef(env, jresult);
      }
      
      (*env)->DeleteLocalRef(env, clazz);
    }
    else{
      pj_thread_message(companion, 
        PJ_SYMBOL_JAVANEW, "nocls", 1, pmath_expr_get_item(expr, 1));
    }
  }
  
  pj_exception_to_pmath(env);
  
  exception = pmath_catch();
  if(!pmath_same(exception, PMATH_UNDEFINED)){
    pmath_unref(result);
    
    if(pmath_same(exception, PMATH_ABORT_EXCEPTION)){
      result = pmath_expr_new(
        pmath_ref(PMATH_SYMBOL_ABORT), 0);
    }
    else{
      result = pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_THROW), 1,
        exception);
    }
  }
  
  pmath_unref(expr);
  expr = pmath_expr_new_extended(
    pmath_ref(PJ_SYMBOL_INTERNAL_RETURN), 1,
    result);
  
  pmath_thread_send(companion, expr);
  pmath_unref(companion);
  return PMATH_NULL;
}

pmath_t pj_builtin_javanew(pmath_expr_t expr){
  pmath_t pjvm;
  pmath_messages_t companion;
  pmath_t result;
  pmath_t result_key;
  JNIEnv *env;
  jvmtiEnv *jvmti;
  jthread jthread_obj;
  
  if(pmath_expr_length(expr) < 1){
    pmath_message_argxxx(pmath_expr_length(expr), 1, SIZE_MAX);
    return expr;
  }
  
  pjvm_ensure_started();
  pjvm = pjvm_try_get();
  jvmti = pjvm_get_jvmti(pjvm);
  env = pjvm_get_env();
  if(!jvmti || !env){
    pmath_unref(pjvm);
    pmath_unref(expr);
    return pmath_ref(PMATH_SYMBOL_FAILED);
  }
  
  companion = pj_thread_get_companion(&jthread_obj);
  if(pmath_is_null(companion) || !jthread_obj){
    pmath_unref(companion);
    
    if(jthread_obj)
      (*env)->DeleteLocalRef(env, jthread_obj);
    
    pmath_unref(expr);
    pmath_unref(pjvm);
    return pmath_ref(PMATH_SYMBOL_FAILED);
  }
  
  result_key = pmath_expr_new(pmath_ref(PJ_SYMBOL_INTERNAL_RETURN), 0);
  pmath_unref(pmath_thread_local_save(result_key, PMATH_UNDEFINED));
  
  result = PMATH_UNDEFINED;
  if(!pmath_aborting()){
    expr = pmath_expr_set_item(expr, 0, 
      pmath_ref(PJ_SYMBOL_INTERNAL_JAVANEW));
    pmath_thread_send(companion, expr); 
    expr = PMATH_NULL;
    
    while(pmath_same(result, PMATH_UNDEFINED)){
      pmath_thread_sleep();
      
      if(pmath_aborting()){
        double start_time = pmath_tickcount();
        
        pmath_debug_print("[aborting javanew...]\n");
        // give the thread a 2 second chance to react for the interrupt
        (*jvmti)->InterruptThread(jvmti, jthread_obj);
        
        do{
          pmath_thread_sleep_timeout(0.5);
              
          result = pmath_thread_local_load(result_key);
          
        }while(pmath_tickcount() - start_time < 2.0 
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


pmath_t pj_builtin_javafield(pmath_expr_t expr){
  JNIEnv *env;
  pmath_t result;

  if(pmath_expr_length(expr) != 2){
    pmath_message_argxxx(0, 2, 2);
    return expr;
  }
  
  result = pmath_ref(PMATH_SYMBOL_FAILED);
  pjvm_ensure_started();
  env = pjvm_get_env();
  if(env && (*env)->EnsureLocalCapacity(env, 2) == 0){
    pmath_t obj;
    jobject jobj = NULL;
    pmath_bool_t is_static;
    
    obj = pmath_expr_get_item(expr, 1);
    if(pj_object_is_java(env, obj)){
      jobj = pj_object_to_java(env, obj);
      is_static = FALSE;
    }
    else{
      jobj = pj_class_to_java(env, obj);
      is_static = TRUE;
      
      if(!jobj)
        pmath_message(PJ_SYMBOL_JAVA, "nobcl", 1, pmath_expr_get_item(expr, 1));
    }
    
    if(jobj){
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

pmath_t pj_builtin_assign_javafield(pmath_expr_t expr){
  JNIEnv *env;
  pmath_t lhs;
  pmath_bool_t success;
  
  if(!pmath_is_expr_of_len(expr, PMATH_SYMBOL_ASSIGN, 2))
    return expr;
  
  lhs = pmath_expr_get_item(expr, 1);
  if(!pmath_is_expr_of_len(lhs, PJ_SYMBOL_JAVAFIELD, 2)){
    pmath_unref(lhs);
    return expr;
  }
  
  success = FALSE;
  pjvm_ensure_started();
  env = pjvm_get_env();
  if(env && (*env)->EnsureLocalCapacity(env, 2) == 0){
    pmath_t obj;
    jobject jobj = NULL;
    pmath_bool_t is_static;
    
    obj = pmath_evaluate(pmath_expr_get_item(lhs, 1));
    if(pj_object_is_java(env, obj)){
      jobj = pj_object_to_java(env, obj);
      is_static = FALSE;
    }
    else{
      jobj = pj_class_to_java(env, obj);
      is_static = TRUE;
      
      if(!jobj)
        pmath_message(PJ_SYMBOL_JAVA, "nobcl", 1, pmath_expr_get_item(lhs, 1));
    }
    
    if(jobj){
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
  if(success){
    pmath_t result = pmath_expr_get_item(expr, 2);
    pmath_unref(expr);
    return result;
  }
  
  pmath_unref(expr);
  return pmath_ref(PMATH_SYMBOL_FAILED);
}


void pj_objects_clear_cache(void){
  pmath_hashtable_t new_p2j = pmath_ht_create(&p2j_class, 0);
  pmath_hashtable_t new_j2p = pmath_ht_create(&j2p_class, 0);
  
  if(new_p2j && new_j2p){
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

pmath_bool_t pj_objects_init(void){
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

void pj_objects_done(void){
  pmath_ht_destroy(j2p_objects); j2p_objects = NULL;
  pmath_ht_destroy(p2j_objects); p2j_objects = NULL;
}
