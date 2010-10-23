#include "pj-objects.h"
#include "pj-classes.h"
#include "pjvm.h"

#include <limits.h>


static jmethodID midHashCode = NULL;

//{ p2j_objects Hashtable implementation ...
struct obj_entry_t{
  pmath_string_t owner_name;
  jobject        jglobal;
};
  
  static unsigned int java_hash(jobject jobj){
    JNIEnv *env = pjvm_try_get_env();
    
    if(env){
      if(!midHashCode && (*env)->EnsureLocalCapacity(env, 1)){
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
    
    pmath_debug_print("java_hash: pjvm_try_get_env() failed\n");
    return 0;
  }
  
  static void p2j_entry_destructor(void *p){
    struct obj_entry_t *e = (struct obj_entry_t*)p;
    
    if(e){
      JNIEnv *env = pjvm_try_get_env();
      
      pmath_t sym = pmath_symbol_get(e->owner_name, FALSE);
      if(sym){
        pmath_symbol_set_value(sym, NULL);
      }
      
      if(env){
        (*env)->DeleteGlobalRef(env, e->jglobal);
      }
      else{
        pmath_debug_print("p2j_entry_destructor: pjvm_try_get_env() failed\n");
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
    
    JNIEnv *env = pjvm_try_get_env();
    if(env)
      return (*env)->IsSameObject(env, e1->jglobal, e2->jglobal);
    
    pmath_debug_print("j2p_entry_keys_equal: pjvm_try_get_env() failed\n");
    return e1->jglobal == e2->jglobal;
  }
  
  static unsigned int p2j_key_hash(void *p){
    pmath_t k = (pmath_t)p;
    
    return pmath_hash(k);
  }
  
  static unsigned int j2p_key_hash(void *p){
    jobject k = (jobject)p;
    
    return java_hash(k);
  }
  
  static pmath_bool_t p2j_entry_equals_key(void *pe, void *pk){
    struct obj_entry_t *e = (struct obj_entry_t*)pe;
    pmath_t k = (pmath_t)pk;
    
    return pmath_equals(e->owner_name, k);
  }
  
  static pmath_bool_t j2p_entry_equals_key(void *pe, void *pk){
    struct obj_entry_t *e = (struct obj_entry_t*)pe;
    jobject k = (jobject)pk;
    
    JNIEnv *env = pjvm_try_get_env();
    if(env)
      return (*env)->IsSameObject(env, e->jglobal, k);
    
    pmath_debug_print("j2p_entry_equals_key: pjvm_try_get_env() failed\n");
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
static PMATH_DECLARE_ATOMIC(p2j_lock) = 0;
static pmath_hashtable_t p2j_objects;
static pmath_hashtable_t j2p_objects; // does not own the entries.
  
  static void java_destructor(void *e){
    struct obj_entry_t *p2j_entry;
    pmath_string_t name = (pmath_string_t)e;
    
    pmath_atomic_lock(&p2j_lock);
    {
      p2j_entry = (struct obj_entry_t*)pmath_ht_remove(p2j_objects, name);
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
  pmath_symbol_t symbol = NULL;
  
  pmath_atomic_lock(&p2j_lock);
  {
    struct obj_entry_t *entry = (struct obj_entry_t*)pmath_ht_search(j2p_objects, jobj);
    
    if(entry){
      symbol = pmath_symbol_get(pmath_ref(entry->owner_name), FALSE);
    }
  }
  pmath_atomic_unlock(&p2j_lock);
  
  if(symbol)
    return symbol;
  
  symbol = pmath_symbol_create_temporary(PMATH_C_STRING("Java`Objects`javaObject"), TRUE);
  if(symbol){
    struct obj_entry_t *entry;
    
    entry = (struct obj_entry_t*)pmath_mem_alloc(sizeof(struct obj_entry_t));
    if(entry){
      struct obj_entry_t  *old_entry;
      struct obj_entry_t  *test_entry;
      pmath_string_t       name;
      pmath_custom_t       value;
      
      name  = pmath_symbol_name(symbol);
      value = pmath_custom_new(pmath_ref(name), java_destructor);
      if(value){
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
                entry = (struct obj_entry_t*)pmath_ht_remove(p2j_objects, name);
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
            pmath_t str = pj_class_get_nice_name(env, clazz);
            (*env)->DeleteLocalRef(env, clazz);
            
            str = pmath_string_insert_latin1(str, INT_MAX, " \xBB", 2);
            str = pmath_string_insert_latin1(str, 0,       "\xAB ", 2);
            
            PMATH_RUN_ARGS(
                "MakeBoxes(`1`)::= InterpretationBox(`2`,"
                    "`1`)", 
              "(oo)", 
              pmath_ref(symbol), 
              str);
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
  
  if(pmath_instance_of(obj, PMATH_TYPE_SYMBOL)){
    pmath_t value = pmath_symbol_get_value(obj);
    pmath_unref(obj);
    obj = value;
  }
  
  if(pmath_instance_of(obj, PMATH_TYPE_CUSTOM)
  && pmath_custom_has_destructor(obj, java_destructor)){
    pmath_string_t name = pmath_ref((pmath_string_t)pmath_custom_get_data(obj));
    jobject result = NULL;
    
    pmath_atomic_lock(&p2j_lock);
    {
      struct obj_entry_t *e = pmath_ht_search(p2j_objects, name);
      
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
  
  if(pmath_instance_of(obj, PMATH_TYPE_SYMBOL)){
    value = pmath_symbol_get_value(obj);
  }
  else
    value = pmath_ref(obj);
  
  if(pmath_instance_of(value, PMATH_TYPE_CUSTOM)
  && pmath_custom_has_destructor(value, java_destructor)){
    pmath_unref(value);
    return TRUE;
  }
  
  pmath_unref(value);
  return FALSE;
}


void pj_objects_clear_cache(void){
  pmath_hashtable_t new_p2j = pmath_ht_create(&p2j_class, 0);
  pmath_hashtable_t new_j2p = pmath_ht_create(&j2p_class, 0);
  
  if(new_p2j && new_j2p){
    pmath_hashtable_t old_p2j;
    pmath_hashtable_t old_j2p;
    
    pmath_atomic_lock(&p2j_lock)
    {
      old_p2j = p2j_objects;
      old_j2p = j2p_objects;
      p2j_objects = new_p2j;
      j2p_objects = new_j2p;
    }
    pmath_atomic_unlock(&p2j_lock);
    
    pmath_ht_destroy(old_j2p);
    pmath_ht_destroy(old_p2j);
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
