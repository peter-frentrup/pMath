#include "pj-classes.h"
#include "pj-symbols.h"
#include "pj-values.h"
#include "pjvm.h"

#include <limits.h>
#include <string.h>


enum{
  PJ_MODIFIER_PUBLIC        = 1,
  PJ_MODIFIER_PRIVATE       = 2,
  PJ_MODIFIER_PROTECTED     = 4,
  PJ_MODIFIER_STATIC        = 8,
  PJ_MODIFIER_FINAL 	      = 16,
  PJ_MODIFIER_SYNCHRONIZED  = 32,
  PJ_MODIFIER_VOLATILE      = 64,
  PJ_MODIFIER_TRANSIENT     = 128,
  PJ_MODIFIER_NATIVE        = 256,
  PJ_MODIFIER_INTERFACE     = 512,
  PJ_MODIFIER_ABSTRACT      = 1024,
  PJ_MODIFIER_STRICT        = 2048
};

//{ cms2id Hashtable implementation ...
struct pmath2id_t{
  pmath_string_t class_method_signature;
  pmath_t        info;
  jmethodID      id;
  int            modifiers;
  char           type; // Z,B,C,S,I,J,F,D or sth else for an object
};

  static void cms2id_entry_destructor(void *p){
    struct pmath2id_t *e = (struct pmath2id_t*)p;
    
    if(e){
      pmath_unref(e->class_method_signature);
      pmath_unref(e->info);
      pmath_mem_free(e);
    }
  }
  
  static unsigned int cms2id_entry_hash(void *p){
    struct pmath2id_t *e = (struct pmath2id_t*)p;
    return pmath_hash(e->class_method_signature);
  }
  
  static pmath_bool_t cms2id_entry_keys_equal(void *p1, void *p2){
    struct pmath2id_t *e1 = (struct pmath2id_t*)p1;
    struct pmath2id_t *e2 = (struct pmath2id_t*)p2;
    return pmath_equals(e1->class_method_signature, e2->class_method_signature);
  }
  
  static unsigned int cms2id_key_hash(void *p){
    pmath_t k = (pmath_t)p;
    
    return pmath_hash(k);
  }
  
  static pmath_bool_t cms2id_entry_equals_key(void *pe, void *pk){
    struct pmath2id_t *e = (struct pmath2id_t*)pe;
    pmath_t k = (pmath_t)pk;
    
    return pmath_equals(e->class_method_signature, k);
  }
  
static pmath_ht_class_t cms2id_class = {
  cms2id_entry_destructor,
  cms2id_entry_hash,
  cms2id_entry_keys_equal,
  cms2id_key_hash,
  cms2id_entry_equals_key
};
//} ... cms2id Hashtable implementation
static PMATH_DECLARE_ATOMIC(cms2id_lock) = 0;
static pmath_hashtable_t cms2id;



pmath_string_t pj_class_get_nice_name(JNIEnv *env, jclass clazz){
  static jmethodID id = NULL;
  jclass cc;
  jstring jstr;
  pmath_string_t result = NULL;
  
  if(!env || !clazz || (*env)->EnsureLocalCapacity(env, 2) != 0)
    return NULL;
  
  if(!id){
    cc = (*env)->GetObjectClass(env, clazz);
    if(!cc)
      return NULL;
    
    id = (*env)->GetMethodID(env, cc, "getName", "()Ljava/lang/String;");
    (*env)->DeleteLocalRef(env, cc);
  }
  
  if(id){
    jstr = (*env)->CallObjectMethod(env, clazz, id);
    result = pj_string_from_java(env, jstr);
    (*env)->DeleteLocalRef(env, jstr);
  }
  
  return result;
}

pmath_string_t pj_class_get_name(JNIEnv *env, jclass clazz){
  pmath_string_t result = pj_class_get_nice_name(env, clazz);
  
  if(pmath_string_length(result) > 0){
    if(pmath_string_buffer(result)[0] == '[')
      return result;
    
    if(pmath_string_equals_latin1(result, "boolean")){
      pmath_unref(result);
      return PMATH_C_STRING("Z");
    }
    
    if(pmath_string_equals_latin1(result, "byte")){
      pmath_unref(result);
      return PMATH_C_STRING("B");
    }
    
    if(pmath_string_equals_latin1(result, "char")){
      pmath_unref(result);
      return PMATH_C_STRING("C");
    }
    
    if(pmath_string_equals_latin1(result, "short")){
      pmath_unref(result);
      return PMATH_C_STRING("S");
    }
    
    if(pmath_string_equals_latin1(result, "int")){
      pmath_unref(result);
      return PMATH_C_STRING("I");
    }
    
    if(pmath_string_equals_latin1(result, "long")){
      pmath_unref(result);
      return PMATH_C_STRING("J");
    }
    
    if(pmath_string_equals_latin1(result, "float")){
      pmath_unref(result);
      return PMATH_C_STRING("F");
    }
    
    if(pmath_string_equals_latin1(result, "double")){
      pmath_unref(result);
      return PMATH_C_STRING("D");
    }
    
    {
      int len = pmath_string_length(result);
      char *s = pmath_mem_alloc(len + 2);
      const uint16_t *buf = pmath_string_buffer(result);
      
      if(s && buf){
        int i;
        
        s[0] = 'L';
        for(i = 0;i < len;++i){
          if(buf[i] == '.')
            s[i+1] = '/';
          else
            s[i+1] = (char)buf[i];
        }
        s[len+1] = ';';
        
        pmath_unref(result);
        result = pmath_string_insert_latin1(NULL, 0, s, len+2);
        pmath_mem_free(s);
      }
    }
  }
  
  return result;
}
  
  static char *java_class_name(pmath_t obj){ // obj will be freed
    if(pmath_is_expr_of(obj, PJ_SYMBOL_JAVACLASS)){
      pmath_t name = pmath_expr_get_item(obj, 1);
      pmath_unref(obj);
      obj = name;
    }
    
    if(pmath_instance_of(obj, PMATH_TYPE_STRING)){
      int len;
      char *str = pmath_string_to_utf8(obj, &len);
      
      if(str){
        int i;
        for(i = 0;i < len;++i){
          if(str[i] == '.'){
            str[i] = '/';
          }
          else if(!str[i]){
            pmath_mem_free(str);
            str = NULL;
            break;
          }
        }
        
        pmath_unref(obj);
        return str;
      }
    }
    
    pmath_unref(obj);
    return NULL;
  }

jclass pj_class_get_java(JNIEnv *env, pmath_t obj){
  jclass result = NULL;
  
  if(env){
    char *str;
    
    if(pmath_instance_of(obj, PMATH_TYPE_SYMBOL)){
      if(obj == PJ_SYMBOL_TYPE_BOOLEAN){
        pmath_unref(obj);
        return (*env)->FindClass(env, "Z");
      }
      
      if(obj == PJ_SYMBOL_TYPE_BYTE){
        pmath_unref(obj);
        return (*env)->FindClass(env, "B");
      }
    
      if(obj == PJ_SYMBOL_TYPE_CHAR){
        pmath_unref(obj);
        return (*env)->FindClass(env, "C");
      }
    
      if(obj == PJ_SYMBOL_TYPE_SHORT){
        pmath_unref(obj);
        return (*env)->FindClass(env, "S");
      }
    
      if(obj == PJ_SYMBOL_TYPE_INT){
        pmath_unref(obj);
        return (*env)->FindClass(env, "I");
      }
    
      if(obj == PJ_SYMBOL_TYPE_LONG){
        pmath_unref(obj);
        return (*env)->FindClass(env, "J");
      }
    
      if(obj == PJ_SYMBOL_TYPE_FLOAT){
        pmath_unref(obj);
        return (*env)->FindClass(env, "F");
      }
    
      if(obj == PJ_SYMBOL_TYPE_DOUBLE){
        pmath_unref(obj);
        return (*env)->FindClass(env, "D");
      }
    }
    
    if(pmath_is_expr_of_len(obj, PJ_SYMBOL_TYPE_ARRAY, 1)){
      pmath_string_t prefix = NULL;
      char *str;
      
      do{
        pmath_t elem = pmath_expr_get_item(obj, 1);
        pmath_unref(obj);
        obj = elem;
        prefix = pmath_string_insert_latin1(prefix, INT_MAX, "[", 1);
      }while(pmath_is_expr_of_len(obj, PJ_SYMBOL_TYPE_ARRAY, 1));
      
      if(!pmath_aborting()){
        pmath_unref(obj);
        return NULL;
      }
      
      str = java_class_name(obj);
      if(str){
        int len = 0;
        
        prefix = pmath_string_concat(prefix, pmath_string_from_utf8(str, -1));
        pmath_mem_free(str);
        
        str = pmath_string_to_utf8(prefix, &len);
        if(str && len > 0){
          result = (*env)->FindClass(env, str);
          pmath_mem_free(str);
          return result;
        }
      }
      
      return NULL;
    }
    
    str = java_class_name(obj);
    if(str){
      result = (*env)->FindClass(env, str);
      pmath_mem_free(str);
    }
  }
  
  pmath_unref(obj);
  return result;
}


static pmath_t type2pmath(pmath_string_t name, int start){ // name will be freed
  const uint16_t *buf = pmath_string_buffer(name);
  int             len = pmath_string_length(name);
  
  if(len <= start)
    return name;
  
  if(start + 1 == len){
    uint16_t ch = buf[start];
    pmath_unref(name);
    
    switch(ch){
      case 'Z': return pmath_ref(PJ_SYMBOL_TYPE_BOOLEAN);
      case 'B': return pmath_ref(PJ_SYMBOL_TYPE_BYTE);
      case 'C': return pmath_ref(PJ_SYMBOL_TYPE_CHAR);
      case 'S': return pmath_ref(PJ_SYMBOL_TYPE_SHORT);
      case 'I': return pmath_ref(PJ_SYMBOL_TYPE_INT);
      case 'J': return pmath_ref(PJ_SYMBOL_TYPE_LONG);
      case 'F': return pmath_ref(PJ_SYMBOL_TYPE_FLOAT);
      case 'D': return pmath_ref(PJ_SYMBOL_TYPE_DOUBLE);
    }
    
    return NULL;
  }
  
  if(buf[start] == '['){
    pmath_t sub = type2pmath(name, start + 1);
    return pmath_expr_new_extended(
      pmath_ref(PJ_SYMBOL_TYPE_ARRAY), 1,
      sub);
  }
  
  if(buf[start] == 'L' && buf[len-1] == ';'){
    return pmath_string_part(name, start + 1, len - start - 2);
  }
  
  return name;
}

  struct cache_info_t{
    pmath_string_t  class_name;
    jclass          jcClass;
    jclass          jcMethod;
    jmethodID       midGetFields;
    jmethodID       midGetName;
    jmethodID       midGetModifiers;
    jmethodID       midGetParameterTypes;
    jmethodID       midGetReturnType;
  };
  
  static pmath_bool_t init_cache_info(JNIEnv *env, jclass clazz, struct cache_info_t *info){
    memset(info, 0, sizeof(struct cache_info_t));
    
    if(!env || !clazz)
      return FALSE;
    
    info->class_name = pj_class_get_name(env, clazz);
    if(!info->class_name)
      return FALSE;
    
    info->jcClass = (*env)->FindClass(env, "Ljava/lang/Class;");
    if(!info->jcClass) 
      goto FAIL_CLASS;
    
    info->jcMethod = (*env)->FindClass(env, "Ljava/lang/reflect/Method;");
    if(!info->jcMethod) 
      goto FAIL_METHOD_CLASS;
    
    info->midGetFields = (*env)->GetMethodID(env, info->jcClass, "getMethods", "()[Ljava/lang/reflect/Method;");
    if(!info->midGetFields)
      goto FAIL_MID;
    
    info->midGetName = (*env)->GetMethodID(env, info->jcMethod, "getName", "()Ljava/lang/String;");
    if(!info->midGetName)
      goto FAIL_MID;
    
    info->midGetModifiers = (*env)->GetMethodID(env, info->jcMethod, "getModifiers", "()I");
    if(!info->midGetModifiers)
      goto FAIL_MID;
    
    info->midGetParameterTypes = (*env)->GetMethodID(env, info->jcMethod, "getParameterTypes", "()[Ljava/lang/Class;");
    if(!info->midGetParameterTypes)
      goto FAIL_MID;
    
    info->midGetReturnType = (*env)->GetMethodID(env, info->jcMethod, "getReturnType", "()Ljava/lang/Class;");
    if(!info->midGetReturnType)
      goto FAIL_MID;
    
    return TRUE;
    
    FAIL_MID:             (*env)->DeleteLocalRef(env, info->jcMethod);
    FAIL_METHOD_CLASS:    (*env)->DeleteLocalRef(env, info->jcClass);
    FAIL_CLASS:           pmath_unref(info->class_name);
    
    memset(info, 0, sizeof(struct cache_info_t));
    return FALSE;
  }
  
  static void free_cache_info(JNIEnv *env, struct cache_info_t *info){
    if(info->jcClass)
      (*env)->DeleteLocalRef(env, info->jcClass);
      
    if(info->jcMethod)
      (*env)->DeleteLocalRef(env, info->jcMethod);
    
    pmath_unref(info->class_name);
    
    memset(info, 0, sizeof(struct cache_info_t));
  }
  
  static pmath_expr_t cache_methods(
    JNIEnv *env, 
    struct cache_info_t *info,
    jobjectArray method_array
  ){
    jsize jlen, ji;
    
    pmath_gather_begin(NULL);
    
    jlen = (*env)->GetArrayLength(env, method_array);
    for(ji = 0;ji < jlen;++ji){
      pmath_string_t      name;
      pmath_t             params;
      int                 modifiers;
      char                type;
      jmethodID           id;
      jsize               jlen2, ji2;
      struct pmath2id_t  *cache_entry;
      jobject             method;
      jobject             jobj;
      jobjectArray        jtypes;
      
      method = (*env)->GetObjectArrayElement(env, method_array, ji);
      if(!method)
        goto FAIL_METHOD;
      
      
      modifiers = (*env)->CallIntMethod(env, method, info->midGetModifiers);
      if(!(modifiers & PJ_MODIFIER_PUBLIC)
      ||  (modifiers & PJ_MODIFIER_PRIVATE)
      ||  (modifiers & PJ_MODIFIER_PROTECTED))
        goto FAIL_PUBLIC;
      
      
      id = (*env)->FromReflectedMethod(env, method);
      if(!id)
        goto FAIL_ID;
      
      
      jobj = (*env)->CallObjectMethod(env, method, info->midGetReturnType);
      if(!jobj)
        goto FAIL_JRETURN;
        
      name = pj_class_get_name(env, (jstring)jobj);
      (*env)->DeleteLocalRef(env, jobj);
      if(pmath_string_length(name) == 0)
        goto FAIL_RETURN;
      
      type = (char)pmath_string_buffer(name)[0];
      pmath_unref(name);
      
      
      jobj = (*env)->CallObjectMethod(env, method, info->midGetName);
      if(!jobj)
        goto FAIL_JNAME;
      
      name = pj_string_from_java(env, (jstring)jobj);
      (*env)->DeleteLocalRef(env, jobj);
      if(!name)
        goto FAIL_NAME;
      
      
      jtypes = (jobjectArray)(*env)->CallObjectMethod(env, method, info->midGetParameterTypes);
      if(!jtypes)
        goto FAIL_JTYPES;
      
      jlen2 = (*env)->GetArrayLength(env, jtypes);
      params = pmath_expr_new(pmath_ref(PMATH_SYMBOL_LIST), (size_t)jlen2);
      
      for(ji2 = 0;ji2 < jlen2;++ji2){
        jobj = (*env)->GetObjectArrayElement(env, jtypes, ji2);
        
        if(jobj){
          pmath_t type = type2pmath(pj_class_get_name(env, (jclass)jobj), 0);
          (*env)->DeleteLocalRef(env, jobj);
          
          params = pmath_expr_set_item(params, 1 + (size_t)ji2, type);
        }
      }
      (*env)->DeleteLocalRef(env, jtypes);
      
      
      pj_exception_to_pmath(env);
      
      cache_entry = (struct pmath2id_t*)pmath_mem_alloc(sizeof(struct pmath2id_t));
      if(cache_entry){
        cache_entry->class_method_signature = pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_LIST), 3,
          pmath_ref(info->class_name),
          pmath_ref(name),
          pmath_ref(params));
        cache_entry->info                   = NULL;
        cache_entry->id                     = id;
        cache_entry->modifiers              = modifiers;
        cache_entry->type                   = type;
        
        if(!pmath_aborting()){
          pmath_atomic_lock(&cms2id_lock);
          {
            cache_entry = pmath_ht_insert(cms2id, cache_entry);
          }
          pmath_atomic_unlock(&cms2id_lock);
        }
        
        cms2id_entry_destructor(cache_entry);
      }
      
      if(!pmath_aborting()){
        pmath_t key = pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_LIST), 2,
          pmath_ref(info->class_name),
          pmath_ref(name));
        
        pmath_atomic_lock(&cms2id_lock);
        {
          cache_entry = pmath_ht_search(cms2id, key);
          
          if(cache_entry){
            cache_entry->info = pmath_expr_append(cache_entry->info, 1, pmath_ref(params));
          }
        }
        pmath_atomic_unlock(&cms2id_lock);
        
        if(!cache_entry){
          cache_entry = (struct pmath2id_t*)pmath_mem_alloc(sizeof(struct pmath2id_t));
          if(cache_entry){
            cache_entry->class_method_signature = pmath_ref(key);
            cache_entry->info                   = pmath_expr_new_extended(
              pmath_ref(PMATH_SYMBOL_LIST), 1,
              pmath_ref(params));
            cache_entry->id                     = 0;
            cache_entry->modifiers              = 0;
            cache_entry->type                   = '?';
            
            pmath_atomic_lock(&cms2id_lock);
            {
              cache_entry = pmath_ht_insert(cms2id, cache_entry);
            }
            pmath_atomic_unlock(&cms2id_lock);
            
            cms2id_entry_destructor(cache_entry);
          }
        }
        
        pmath_unref(key);
      }
      
      pmath_emit(pmath_ref(name), NULL);
      
                        pmath_unref(params);
      FAIL_JTYPES:      pmath_unref(name);
      FAIL_NAME:     
      FAIL_JNAME:
      FAIL_RETURN:
      FAIL_JRETURN:
      FAIL_ID:
      FAIL_PUBLIC:      (*env)->DeleteLocalRef(env, method);
      FAIL_METHOD:
      ;
    }
    
    return pmath_gather_end();
  }
  
void pj_cache_members(JNIEnv *env, jclass clazz){
  struct cache_info_t info;
  pmath_t             methods;
  jobjectArray        method_array;
  
  if(!env || !clazz)
    return;
  
  if((*env)->EnsureLocalCapacity(env, 6))
    return;
  
  if(!init_cache_info(env, clazz, &info))
    return;
  
  method_array = (jobjectArray)(*env)->CallObjectMethod(env, clazz, info.midGetFields);
  if(method_array){
    methods = cache_methods(env, &info, method_array);
    
    (*env)->DeleteLocalRef(env, method_array);
  }
  else
    methods = NULL;
  
  methods = pmath_evaluate(
    pmath_expr_new_extended(
      pmath_ref(PMATH_SYMBOL_UNION), 1,
      methods));
  
  if(!pmath_aborting() 
  && !(*env)->ExceptionCheck(env) 
  && pmath_is_expr_of(methods, PMATH_SYMBOL_LIST)){
    struct pmath2id_t *cache_entry;
    
    cache_entry = (struct pmath2id_t*)pmath_mem_alloc(sizeof(struct pmath2id_t));
    
    if(cache_entry){
      cache_entry->class_method_signature = pmath_ref(info.class_name);
      cache_entry->info                   = pmath_ref(methods);
      cache_entry->id                     = 0;
      cache_entry->modifiers              = 0;
      cache_entry->type                   = '?';
  
      pmath_atomic_lock(&cms2id_lock);
      {
        cache_entry = pmath_ht_insert(cms2id, cache_entry);
      }
      pmath_atomic_unlock(&cms2id_lock);
      
      cms2id_entry_destructor(cache_entry);
    }
  }
  
  PMATH_RUN_ARGS("Print(`1`, \": \", `2`)", 
    "(oo)", 
    pmath_ref(info.class_name), 
    pmath_ref(methods));
  
  pmath_unref(methods);
  
  free_cache_info(env, &info);
}


pmath_bool_t pj_classes_init(void){
  cms2id = pmath_ht_create(&cms2id_class, 0);
  if(!cms2id)
    return FALSE;
  
  return TRUE;
}

void pj_classes_done(void){
  pmath_ht_destroy(cms2id); cms2id = NULL;
}
