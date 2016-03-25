#include "pj-classes.h"
#include "pj-objects.h"
#include "pj-symbols.h"
#include "pj-threads.h"
#include "pj-values.h"
#include "pjvm.h"

#include <limits.h>
#include <stdarg.h>
#include <string.h>


enum {
  PJ_MODIFIER_PUBLIC        = 1,
  PJ_MODIFIER_PRIVATE       = 2,
  PJ_MODIFIER_PROTECTED     = 4,
  PJ_MODIFIER_STATIC        = 8,
  PJ_MODIFIER_FINAL         = 16,
  PJ_MODIFIER_SYNCHRONIZED  = 32,
  PJ_MODIFIER_VOLATILE      = 64,
  PJ_MODIFIER_TRANSIENT     = 128,
  PJ_MODIFIER_NATIVE        = 256,
  PJ_MODIFIER_INTERFACE     = 512,
  PJ_MODIFIER_ABSTRACT      = 1024,
  PJ_MODIFIER_STRICT        = 2048
};


//{ cms2id Hashtable implementation ...
struct pmath2id_t {
  pmath_string_t class_method_signature;
  pmath_t        info;
  jmethodID      mid;
  jfieldID       fid;
  int            modifiers;
  char           return_type; // Z,B,C,S,I,J,F,D or sth else for an object
};

/* "class"  ==>  info = {"method_or_field_1", "method_or_field_2", ...}

   {"class", "method_or_constructor"}  ==>  info = {{argtype11, argtype12, ...}, {argtype21, argtype22, ...}, ...}

   {"class", "method_or_constructor", {argtype1, argtyp2, ...}}  ==>
      info        = return type (or NULL for constructors)
      mid         = jni method id
      fid         = 0
      modifiers   = PJ_MODIFIER_XXX set
      return_type = first char of method return type signature

   {"class", "field"}  ==>
      info       = field type
      mid        = 0
      fid        = jni field id
      modifiers   = PJ_MODIFIER_XXX set
      return_type = first char of field type

   "<init>" is the name of the constructor. It is never a method.
 */

static void cms2id_entry_destructor(void *p) {
  struct pmath2id_t *e = (struct pmath2id_t*)p;
  
  if(e) {
    pmath_unref(e->class_method_signature);
    pmath_unref(e->info);
    pmath_mem_free(e);
  }
}

static unsigned int cms2id_entry_hash(void *p) {
  struct pmath2id_t *e = (struct pmath2id_t*)p;
  return pmath_hash(e->class_method_signature);
}

static pmath_bool_t cms2id_entry_keys_equal(void *p1, void *p2) {
  struct pmath2id_t *e1 = (struct pmath2id_t*)p1;
  struct pmath2id_t *e2 = (struct pmath2id_t*)p2;
  return pmath_equals(e1->class_method_signature, e2->class_method_signature);
}

static unsigned int cms2id_key_hash(void *p) {
  pmath_t k = *(pmath_t*)p;
  
  return pmath_hash(k);
}

static pmath_bool_t cms2id_entry_equals_key(void *pe, void *pk) {
  struct pmath2id_t *e = (struct pmath2id_t*)pe;
  pmath_t k = *(pmath_t*)pk;
  
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
static pmath_atomic_t cms2id_lock = PMATH_ATOMIC_STATIC_INIT;
static pmath_hashtable_t cms2id;



pmath_string_t pj_class_get_nice_name(JNIEnv *env, jclass clazz) {
  pmath_string_t result = PMATH_NULL;
  pmath_t pjvm    = pjvm_try_get();
  jvmtiEnv *jvmti = pjvm_get_jvmti(pjvm);
  
  if(jvmti) {
    char *sig = NULL;
    (*jvmti)->GetClassSignature(jvmti, clazz, &sig, NULL);
    
    if(sig) {
      switch(*sig) {
        case 'Z': result = PMATH_C_STRING("boolean"); break;
        case 'B': result = PMATH_C_STRING("byte");    break;
        case 'C': result = PMATH_C_STRING("char");    break;
        case 'S': result = PMATH_C_STRING("short");   break;
        case 'I': result = PMATH_C_STRING("int");     break;
        case 'J': result = PMATH_C_STRING("long");    break;
        case 'F': result = PMATH_C_STRING("float");   break;
        case 'D': result = PMATH_C_STRING("double");  break;
        
        case 'L': {
            char *s = sig + 1;
            for(;;) {
              if(*s == '/') {
                *s = '.';
              }
              else if(*s == ';') {
                *s = '\0';
                break;
              }
              else if(*s == '\0')
                break;
                
              ++s;
            }
            
            result = pmath_string_from_utf8(sig + 1, -1);
          } break;
          
        default:
          result = pmath_string_from_utf8(sig, -1);
      }
    }
    
    if(sig)
      (*jvmti)->Deallocate(jvmti, (unsigned char*)sig);
  }
  
  pmath_unref(pjvm);
  return result;
}

pmath_string_t pj_class_get_name(JNIEnv *env, jclass clazz) {
  pmath_string_t result = PMATH_NULL;
  pmath_t pjvm    = pjvm_try_get();
  jvmtiEnv *jvmti = pjvm_get_jvmti(pjvm);
  
  if(jvmti) {
    char *sig = NULL;
    (*jvmti)->GetClassSignature(jvmti, clazz, &sig, NULL);
    
    if(sig)
      result = pmath_string_from_utf8(sig, -1);
      
    if(sig)
      (*jvmti)->Deallocate(jvmti, (unsigned char*)sig);
  }
  
  pmath_unref(pjvm);
  return result;
}

static char *java_class_name(pmath_t obj) { // obj will be freed
  if(pmath_is_expr_of(obj, PJ_SYMBOL_JAVACLASS)) {
    pmath_t name = pmath_expr_get_item(obj, 1);
    pmath_unref(obj);
    obj = name;
  }
  
  if(pmath_is_string(obj)) {
    int len;
    char *str = pmath_string_to_utf8(obj, &len);
    
    if(str) {
      int i;
      for(i = 0; i < len; ++i) {
        if(str[i] == '.') {
          str[i] = '/';
        }
        else if(!str[i]) {
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

jclass pj_class_to_java(JNIEnv *env, pmath_t obj) {
  jclass result = NULL;
  
  if(env && (*env)->EnsureLocalCapacity(env, 1) == 0) {
    char *str;
    
    /*if(pmath_is_symbol(obj)){
      if(pmath_same(obj, PJ_SYMBOL_TYPE_BOOLEAN)){
        pmath_unref(obj);
        return (*env)->FindClass(env, "Z");
      }
    
      if(pmath_same(obj, PJ_SYMBOL_TYPE_BYTE)){
        pmath_unref(obj);
        return (*env)->FindClass(env, "B");
      }
    
      if(pmath_same(obj, PJ_SYMBOL_TYPE_CHAR)){
        pmath_unref(obj);
        return (*env)->FindClass(env, "C");
      }
    
      if(pmath_same(obj, PJ_SYMBOL_TYPE_SHORT)){
        pmath_unref(obj);
        return (*env)->FindClass(env, "S");
      }
    
      if(pmath_same(obj, PJ_SYMBOL_TYPE_INT)){
        pmath_unref(obj);
        return (*env)->FindClass(env, "I");
      }
    
      if(pmath_same(obj, PJ_SYMBOL_TYPE_LONG)){
        pmath_unref(obj);
        return (*env)->FindClass(env, "J");
      }
    
      if(pmath_same(obj, PJ_SYMBOL_TYPE_FLOAT)){
        pmath_unref(obj);
        return (*env)->FindClass(env, "F");
      }
    
      if(pmath_same(obj, PJ_SYMBOL_TYPE_DOUBLE)){
        pmath_unref(obj);
        return (*env)->FindClass(env, "D");
      }
    }*/
    
    if(pmath_is_expr_of_len(obj, PJ_SYMBOL_TYPE_ARRAY, 1)) {
      pmath_string_t prefix = PMATH_NULL;
      char *str;
      
      do {
        pmath_t elem = pmath_expr_get_item(obj, 1);
        pmath_unref(obj);
        obj = elem;
        prefix = pmath_string_insert_latin1(prefix, INT_MAX, "[", 1);
      } while(pmath_is_expr_of_len(obj, PJ_SYMBOL_TYPE_ARRAY, 1));
      
      if(!pmath_aborting()) {
        pmath_unref(obj);
        return NULL;
      }
      
      str = java_class_name(obj); obj = PMATH_NULL;
      if(str) {
        int len = 0;
        
        prefix = pmath_string_concat(prefix, pmath_string_from_utf8(str, -1));
        pmath_mem_free(str);
        
        str = pmath_string_to_utf8(prefix, &len);
        if(str && len > 0) {
          result = (*env)->FindClass(env, str);
          pmath_mem_free(str);
          return result;
        }
      }
      
      return NULL;
    }
    
    if( pj_object_is_java(env, obj) &&
        JNI_OK == (*env)->EnsureLocalCapacity(env, 2))
    {
      jclass cc = (*env)->FindClass(env, "java/lang/Class");
      result = (jclass)pj_object_to_java(env, obj);
      
      if((*env)->IsInstanceOf(env, result, cc)) {
        (*env)->DeleteLocalRef(env, cc);
        return result;
      }
      
      (*env)->DeleteLocalRef(env, cc);
      (*env)->DeleteLocalRef(env, result);
      return NULL;
    }
    
    str = java_class_name(obj); obj = PMATH_NULL;
    if(str) {
      char *s = str;
      if(*s == 'L') {
        ++s;
        s[strlen(s)-1] = '\0';
      }
      result = (*env)->FindClass(env, s);
      pmath_mem_free(str);
    }
  }
  
  pmath_unref(obj);
  return result;
}


static pmath_t type2pmath(pmath_string_t name, int start) { // name will be freed
  const uint16_t *buf = pmath_string_buffer(&name);
  int             len = pmath_string_length(name);
  
  if(len <= start)
    return pmath_string_part(name, start, -1);
    
  if(start + 1 == len) {
    uint16_t ch = buf[start];
    pmath_unref(name);
    
    switch(ch) {
      case 'Z': return pmath_ref(PJ_SYMBOL_TYPE_BOOLEAN);
      case 'B': return pmath_ref(PJ_SYMBOL_TYPE_BYTE);
      case 'C': return pmath_ref(PJ_SYMBOL_TYPE_CHAR);
      case 'S': return pmath_ref(PJ_SYMBOL_TYPE_SHORT);
      case 'I': return pmath_ref(PJ_SYMBOL_TYPE_INT);
      case 'J': return pmath_ref(PJ_SYMBOL_TYPE_LONG);
      case 'F': return pmath_ref(PJ_SYMBOL_TYPE_FLOAT);
      case 'D': return pmath_ref(PJ_SYMBOL_TYPE_DOUBLE);
    }
    
    return PMATH_NULL;
  }
  
  if(buf[start] == '[') {
    pmath_t sub = type2pmath(name, start + 1);
    return pmath_expr_new_extended(
             pmath_ref(PJ_SYMBOL_TYPE_ARRAY), 1,
             sub);
  }
  
//  if(buf[start] == 'L' && buf[len-1] == ';'){
//    return pmath_string_part(name, start + 1, len - start - 2);
//  }

  return pmath_string_part(name, start, -1);
}

struct cache_info_t {
  pmath_string_t  class_name;
  jclass          jcClass;
  jclass          jcConstructor;
  jclass          jcMethod;
  jclass          jcField;
  jmethodID       midGetConstructors;
  jmethodID       midGetMethods;
  jmethodID       midGetFields;
  jmethodID       midConstructorGetModifiers;
  jmethodID       midConstructorGetParameterTypes;
  jmethodID       midMethodGetName;
  jmethodID       midMethodGetModifiers;
  jmethodID       midMethodGetParameterTypes;
  jmethodID       midMethodGetReturnType;
  jmethodID       midFieldGetName;
  jmethodID       midFieldGetModifiers;
  jmethodID       midFieldGetType;
};

static pmath_bool_t init_cache_info(JNIEnv *env, jclass clazz, struct cache_info_t *info) {
  pmath_t class_name = info->class_name;
  memset(info, 0, sizeof(struct cache_info_t));
  info->class_name = class_name;
  
  if(!env || !clazz)
    return FALSE;
    
  if(pmath_is_null(info->class_name))
    return FALSE;
    
  info->jcClass = (*env)->FindClass(env, "java/lang/Class");
  if(!info->jcClass)
    goto FAIL_CLASS;
    
  info->jcConstructor = (*env)->FindClass(env, "java/lang/reflect/Constructor");
  if(!info->jcConstructor)
    goto FAIL_CONSTRUCTOR_CLASS;
    
  info->jcMethod = (*env)->FindClass(env, "java/lang/reflect/Method");
  if(!info->jcMethod)
    goto FAIL_METHOD_CLASS;
    
  info->jcField = (*env)->FindClass(env, "java/lang/reflect/Field");
  if(!info->jcField)
    goto FAIL_FIELD_CLASS;
    
  info->midGetConstructors = (*env)->GetMethodID(env, info->jcClass, "getConstructors", "()[Ljava/lang/reflect/Constructor;");
  if(!info->midGetConstructors)
    goto FAIL_MID;
    
  info->midGetMethods = (*env)->GetMethodID(env, info->jcClass, "getMethods", "()[Ljava/lang/reflect/Method;");
  if(!info->midGetMethods)
    goto FAIL_MID;
    
  info->midGetFields = (*env)->GetMethodID(env, info->jcClass, "getFields", "()[Ljava/lang/reflect/Field;");
  if(!info->midGetFields)
    goto FAIL_MID;
    
  info->midConstructorGetModifiers = (*env)->GetMethodID(env, info->jcConstructor, "getModifiers", "()I");
  if(!info->midConstructorGetModifiers)
    goto FAIL_MID;
    
  info->midConstructorGetParameterTypes = (*env)->GetMethodID(env, info->jcConstructor, "getParameterTypes", "()[Ljava/lang/Class;");
  if(!info->midConstructorGetParameterTypes)
    goto FAIL_MID;
    
  info->midMethodGetName = (*env)->GetMethodID(env, info->jcMethod, "getName", "()Ljava/lang/String;");
  if(!info->midMethodGetName)
    goto FAIL_MID;
    
  info->midMethodGetModifiers = (*env)->GetMethodID(env, info->jcMethod, "getModifiers", "()I");
  if(!info->midMethodGetModifiers)
    goto FAIL_MID;
    
  info->midMethodGetParameterTypes = (*env)->GetMethodID(env, info->jcMethod, "getParameterTypes", "()[Ljava/lang/Class;");
  if(!info->midMethodGetParameterTypes)
    goto FAIL_MID;
    
  info->midMethodGetReturnType = (*env)->GetMethodID(env, info->jcMethod, "getReturnType", "()Ljava/lang/Class;");
  if(!info->midMethodGetReturnType)
    goto FAIL_MID;
    
  info->midFieldGetName = (*env)->GetMethodID(env, info->jcField, "getName", "()Ljava/lang/String;");
  if(!info->midFieldGetName)
    goto FAIL_MID;
    
  info->midFieldGetModifiers = (*env)->GetMethodID(env, info->jcField, "getModifiers", "()I");
  if(!info->midFieldGetModifiers)
    goto FAIL_MID;
    
  info->midFieldGetType = (*env)->GetMethodID(env, info->jcField, "getType", "()Ljava/lang/Class;");
  if(!info->midFieldGetType)
    goto FAIL_MID;
    
  return TRUE;
  
FAIL_MID:                 (*env)->DeleteLocalRef(env, info->jcField);
FAIL_FIELD_CLASS:         (*env)->DeleteLocalRef(env, info->jcMethod);
FAIL_METHOD_CLASS:        (*env)->DeleteLocalRef(env, info->jcConstructor);
FAIL_CONSTRUCTOR_CLASS:   (*env)->DeleteLocalRef(env, info->jcClass);
FAIL_CLASS:               pmath_unref(info->class_name);

  memset(info, 0, sizeof(struct cache_info_t));
  return FALSE;
}

static void free_cache_info(JNIEnv *env, struct cache_info_t *info) {
  if(info->jcClass)
    (*env)->DeleteLocalRef(env, info->jcClass);
    
  if(info->jcConstructor)
    (*env)->DeleteLocalRef(env, info->jcConstructor);
    
  if(info->jcMethod)
    (*env)->DeleteLocalRef(env, info->jcMethod);
    
  if(info->jcField)
    (*env)->DeleteLocalRef(env, info->jcField);
    
  pmath_unref(info->class_name);
  
  memset(info, 0, sizeof(struct cache_info_t));
}

// NULL is returned for constructors
static pmath_expr_t cache_methods(
  JNIEnv              *env,
  struct cache_info_t *info,
  jobjectArray         method_array,
  pmath_bool_t         constructors // whether method_array is [Ljava/lang/reflect/Constructor;
) {
  jsize jlen, ji;
  
  if(!constructors)
    pmath_gather_begin(PMATH_NULL);
    
  jlen = (*env)->GetArrayLength(env, method_array);
  for(ji = 0; ji < jlen; ++ji) {
    pmath_string_t      name;
    pmath_t             return_type;
    pmath_t             params;
    int                 modifiers;
    char                return_type_char;
    jmethodID           mid;
    jsize               jlen2, ji2;
    struct pmath2id_t  *cache_entry;
    jobject             method;
    jobject             jobj;
    jobjectArray        jtypes;
    
    method = (*env)->GetObjectArrayElement(env, method_array, ji);
    if(!method)
      goto FAIL_METHOD;
      
      
    modifiers = (*env)->CallIntMethod(
                  env, method,
                  constructors ? info->midConstructorGetModifiers : info->midMethodGetModifiers);
    if(!(modifiers & PJ_MODIFIER_PUBLIC)
        ||  (modifiers & PJ_MODIFIER_PRIVATE)
        ||  (modifiers & PJ_MODIFIER_PROTECTED))
      goto FAIL_PUBLIC;
      
      
    mid = (*env)->FromReflectedMethod(env, method);
    if(!mid)
      goto FAIL_MID;
      
      
    if(constructors) {
      return_type = PMATH_NULL;
      return_type_char = 'V';
      
      name = PMATH_C_STRING("<init>");
    }
    else {
      jobj = (*env)->CallObjectMethod(env, method, info->midMethodGetReturnType);
      if(!jobj)
        goto FAIL_JRETURN;
        
      name = pj_class_get_name(env, (jclass)jobj);
      (*env)->DeleteLocalRef(env, jobj);
      if(pmath_string_length(name) == 0) {
        pmath_unref(name);
        goto FAIL_RETURN;
      }
      
      return_type_char = (char)pmath_string_buffer(&name)[0];
      return_type = type2pmath(name, 0);
      name = PMATH_NULL;
      
      jobj = (*env)->CallObjectMethod(env, method, info->midMethodGetName);
      if(!jobj)
        goto FAIL_JNAME;
        
      name = pj_string_from_java(env, (jstring)jobj);
      (*env)->DeleteLocalRef(env, jobj);
      if(pmath_is_null(name))
        goto FAIL_NAME;
    }
    
    
    jtypes = (jobjectArray)(*env)->CallObjectMethod(
               env,
               method,
               constructors ? info->midConstructorGetParameterTypes : info->midMethodGetParameterTypes);
    if(!jtypes)
      goto FAIL_JTYPES;
      
    jlen2 = (*env)->GetArrayLength(env, jtypes);
    params = pmath_expr_new(pmath_ref(PMATH_SYMBOL_LIST), (size_t)jlen2);
    
    for(ji2 = 0; ji2 < jlen2; ++ji2) {
      jobj = (*env)->GetObjectArrayElement(env, jtypes, ji2);
      
      if(jobj) {
        pmath_t type = type2pmath(pj_class_get_name(env, (jclass)jobj), 0);
        (*env)->DeleteLocalRef(env, jobj);
        
        params = pmath_expr_set_item(params, 1 + (size_t)ji2, type);
      }
    }
    (*env)->DeleteLocalRef(env, jtypes);
    
    
    pj_exception_to_pmath(env);
    
    cache_entry = pmath_mem_alloc(sizeof(struct pmath2id_t));
    if(cache_entry) {
      cache_entry->class_method_signature = pmath_expr_new_extended(
                                              pmath_ref(PMATH_SYMBOL_LIST), 3,
                                              pmath_ref(info->class_name),
                                              pmath_ref(name),
                                              pmath_ref(params));
      cache_entry->info                   = return_type; return_type = PMATH_NULL;
      cache_entry->mid                    = mid;
      cache_entry->fid                    = 0;
      cache_entry->modifiers              = modifiers;
      cache_entry->return_type            = return_type_char;
      
      if(!pmath_aborting()) {
        pmath_atomic_lock(&cms2id_lock);
        {
          cache_entry = pmath_ht_insert(cms2id, cache_entry);
        }
        pmath_atomic_unlock(&cms2id_lock);
      }
      
      cms2id_entry_destructor(cache_entry);
    }
    
    if(!pmath_aborting()) {
      pmath_t key = pmath_expr_new_extended(
                      pmath_ref(PMATH_SYMBOL_LIST), 2,
                      pmath_ref(info->class_name),
                      pmath_ref(name));
                      
      pmath_atomic_lock(&cms2id_lock);
      {
        cache_entry = pmath_ht_search(cms2id, &key);
        
        if(cache_entry) {
          cache_entry->info = pmath_expr_append(cache_entry->info, 1, pmath_ref(params));
        }
      }
      pmath_atomic_unlock(&cms2id_lock);
      
      if(!cache_entry) {
        cache_entry = pmath_mem_alloc(sizeof(struct pmath2id_t));
        if(cache_entry) {
          cache_entry->class_method_signature = pmath_ref(key);
          cache_entry->info                   = pmath_expr_new_extended(
                                                  pmath_ref(PMATH_SYMBOL_LIST), 1,
                                                  pmath_ref(params));
          cache_entry->mid                    = 0;
          cache_entry->fid                    = 0;
          cache_entry->modifiers              = 0;
          cache_entry->return_type            = '?';
          
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
    
    if(!constructors)
      pmath_emit(pmath_ref(name), PMATH_NULL);
      
    pmath_unref(params);
  FAIL_JTYPES:      pmath_unref(name);
  FAIL_NAME:
  FAIL_JNAME:       pmath_unref(return_type);
  FAIL_RETURN:
  FAIL_JRETURN:
  FAIL_MID:
  FAIL_PUBLIC:      (*env)->DeleteLocalRef(env, method);
  FAIL_METHOD:
    ;
  }
  
  if(constructors)
    return PMATH_NULL;
    
  return pmath_gather_end();
}

static pmath_expr_t cache_fields(
  JNIEnv              *env,
  struct cache_info_t *info,
  jobjectArray         field_array
) {
  jsize jlen, ji;
  
  pmath_gather_begin(PMATH_NULL);
  
  jlen = (*env)->GetArrayLength(env, field_array);
  for(ji = 0; ji < jlen; ++ji) {
    struct pmath2id_t  *cache_entry;
    jfieldID            fid;
    jobject             field;
    jobject             jobj;
    int                 modifiers;
    pmath_string_t      name;
    pmath_string_t      type_name;
    pmath_t             type;
    
    field = (*env)->GetObjectArrayElement(env, field_array, ji);
    if(!field)
      goto FAIL_FIELD;
      
      
    modifiers = (*env)->CallIntMethod(env, field, info->midFieldGetModifiers);
    if(!(modifiers & PJ_MODIFIER_PUBLIC)
        ||  (modifiers & PJ_MODIFIER_PRIVATE)
        ||  (modifiers & PJ_MODIFIER_PROTECTED))
      goto FAIL_PUBLIC;
      
      
    fid = (*env)->FromReflectedField(env, field);
    if(!fid)
      goto FAIL_FID;
      
      
    { // field type
      jobj = (*env)->CallObjectMethod(env, field, info->midFieldGetType);
      if(!jobj)
        goto FAIL_JTYPE;
        
      type_name = pj_class_get_name(env, (jclass)jobj);
      (*env)->DeleteLocalRef(env, jobj);
      if(pmath_string_length(type_name) == 0) {
        pmath_unref(type_name);
        goto FAIL_TYPE;
      }
      
      type = type2pmath(pmath_ref(type_name), 0);
    }
    
    { // field name
      jobj = (*env)->CallObjectMethod(env, field, info->midFieldGetName);
      if(!jobj)
        goto FAIL_JNAME;
        
      name = pj_string_from_java(env, (jstring)jobj);
      (*env)->DeleteLocalRef(env, jobj);
      if(pmath_is_null(name))
        goto FAIL_NAME;
    }
    
    pj_exception_to_pmath(env);
    
    cache_entry = pmath_mem_alloc(sizeof(struct pmath2id_t));
    if(cache_entry) {
      cache_entry->class_method_signature = pmath_expr_new_extended(
                                              pmath_ref(PMATH_SYMBOL_LIST), 2,
                                              pmath_ref(info->class_name),
                                              pmath_ref(name));
      cache_entry->info                   = pmath_ref(type);
      cache_entry->mid                    = 0;
      cache_entry->fid                    = fid;
      cache_entry->modifiers              = modifiers;
      cache_entry->return_type            = (char)pmath_string_buffer(&type_name)[0];
      
      if(!pmath_aborting()) {
        pmath_atomic_lock(&cms2id_lock);
        {
          cache_entry = pmath_ht_insert(cms2id, cache_entry);
        }
        pmath_atomic_unlock(&cms2id_lock);
      }
      
      cms2id_entry_destructor(cache_entry);
    }
    
    pmath_emit(name, PMATH_NULL);
    
  FAIL_NAME:
  FAIL_JNAME:     pmath_unref(type); pmath_unref(type_name);
  FAIL_TYPE:
  FAIL_JTYPE:
  FAIL_FID:
  FAIL_PUBLIC:    (*env)->DeleteLocalRef(env, field);
  FAIL_FIELD:     ;
  }
  
  return pmath_gather_end();
}

void pj_class_cache_members(JNIEnv *env, jclass clazz) {
  struct pmath2id_t    *cache_entry;
  struct cache_info_t   info;
  pmath_expr_t          methods;
  pmath_expr_t          fields;
  pmath_t               members;
  jobjectArray          array;
  
  if(!env || !clazz)
    return;
    
  info.class_name = pj_class_get_name(env, clazz);
  pmath_atomic_lock(&cms2id_lock);
  {
    cache_entry = pmath_ht_search(cms2id, &info.class_name);
  }
  pmath_atomic_unlock(&cms2id_lock);
  
  if(cache_entry) {
    pmath_unref(info.class_name);
    return;
  }
  
  if((*env)->EnsureLocalCapacity(env, 8) != 0)
    return;
    
  if(!init_cache_info(env, clazz, &info))
    return;
    
  array = (jobjectArray)(*env)->CallObjectMethod(env, clazz, info.midGetConstructors);
  if(array) {
    methods = cache_methods(env, &info, array, TRUE);
    
    assert(pmath_is_null(methods));
    
    (*env)->DeleteLocalRef(env, array);
  }
  
  array = (jobjectArray)(*env)->CallObjectMethod(env, clazz, info.midGetMethods);
  if(array) {
    methods = cache_methods(env, &info, array, FALSE);
    
    (*env)->DeleteLocalRef(env, array);
  }
  else
    methods = PMATH_NULL;
    
  array = (jobjectArray)(*env)->CallObjectMethod(env, clazz, info.midGetFields);
  if(array) {
    fields = cache_fields(env, &info, array);
    
    (*env)->DeleteLocalRef(env, array);
  }
  else
    fields = PMATH_NULL;
    
  members = pmath_evaluate(
              pmath_expr_new_extended(
                pmath_ref(PMATH_SYMBOL_JOIN), 2,
                pmath_expr_new_extended(
                  pmath_ref(PMATH_SYMBOL_UNION), 1,
                  pmath_ref(methods)),
                pmath_ref(fields)));
                
  if(!pmath_aborting()
      && !(*env)->ExceptionCheck(env)
      && pmath_is_expr_of(members, PMATH_SYMBOL_LIST)) {
    cache_entry = pmath_mem_alloc(sizeof(struct pmath2id_t));
    
    if(cache_entry) {
      cache_entry->class_method_signature = pmath_ref(info.class_name);
      cache_entry->info                   = pmath_ref(members);
      cache_entry->mid                    = 0;
      cache_entry->fid                    = 0;
      cache_entry->modifiers              = 0;
      cache_entry->return_type            = '?';
      
      pmath_atomic_lock(&cms2id_lock);
      {
        cache_entry = pmath_ht_insert(cms2id, cache_entry);
      }
      pmath_atomic_unlock(&cms2id_lock);
      
      cms2id_entry_destructor(cache_entry);
    }
  }
  
//  PMATH_RUN_ARGS("Print(`1`, \": \", `2`)",
//    "(o(oo))",
//    pmath_ref(info.class_name),
//    pmath_ref(methods),
//    pmath_ref(fields));

  pmath_unref(fields);
  pmath_unref(methods);
  pmath_unref(members);
  
  free_cache_info(env, &info);
}

pmath_t pj_class_call_method(
  JNIEnv           *env,
  jobject           obj,
  pmath_bool_t      is_static,
  pmath_string_t    name,        // will be freed
  pmath_expr_t      args,        // will be freed
  pmath_messages_t  msg_thread   // wont be freed
) {
  pmath_t               result;
  jclass                clazz;
  jvalue               *jargs;
  pmath_string_t        class_name;
  pmath_t               key;
  pmath_expr_t          signatures;
  jint                  num_args;
  
  if(!env
      || !obj
      || (*env)->EnsureLocalCapacity(env, 1) != 0) {
    pj_exception_to_pmath(env);
    pmath_unref(name);
    pmath_unref(args);
    return pmath_ref(PMATH_SYMBOL_FAILED);
  }
  
  if(is_static)
    clazz = obj;
  else
    clazz = (*env)->GetObjectClass(env, obj);
    
  pj_class_cache_members(env, clazz);
  class_name = pj_class_get_name(env, clazz);
  num_args = (jint)pmath_expr_length(args);
  
  key = pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_LIST), 2,
          class_name,
          pmath_ref(name));
          
  class_name = PMATH_NULL;
  
  signatures = PMATH_NULL;
  pmath_atomic_lock(&cms2id_lock);
  {
    struct pmath2id_t *cache_entry = pmath_ht_search(cms2id, &key);
    if(cache_entry && cache_entry->fid == 0) {
      signatures = pmath_ref(cache_entry->info);
    }
  }
  pmath_atomic_unlock(&cms2id_lock);
  
  if(!pmath_is_expr_of(signatures, PMATH_SYMBOL_LIST)
      || !pmath_is_string(name)
      || pmath_string_equals_latin1(name, "<init>")) {
    pj_thread_message(msg_thread,
                      PJ_SYMBOL_JAVA, "nometh", 2,
                      name,
                      pj_class_get_nice_name(env, clazz));
    pmath_unref(args);
    pmath_unref(key);
    if(!is_static)
      (*env)->DeleteLocalRef(env, clazz);
      
    return pmath_ref(PMATH_SYMBOL_FAILED);
  }
  
  key = pmath_expr_append(key, 1, NULL);
  result = PMATH_UNDEFINED;
  jargs = pmath_mem_alloc(num_args * sizeof(jvalue));
  if(jargs) {
    size_t i;
    
    for(i = 1; i <= pmath_expr_length(signatures) && pmath_same(result, PMATH_UNDEFINED); ++i) {
      pmath_t types = pmath_expr_get_item(signatures, i);
      
      if((*env)->PushLocalFrame(env, num_args) == 0) {
        if(pj_value_fill_args(env, types, args, jargs)) {
          jmethodID  mid         = 0;
          int        modifiers   = 0;
          char       return_type = '?';
          
          key = pmath_expr_set_item(key, 3, pmath_ref(types));
          
          pmath_atomic_lock(&cms2id_lock);
          {
            struct pmath2id_t *cache_entry = pmath_ht_search(cms2id, &key);
            if(cache_entry && cache_entry->mid != 0) {
              mid         = cache_entry->mid;
              modifiers   = cache_entry->modifiers;
              return_type = cache_entry->return_type;
            }
          }
          pmath_atomic_unlock(&cms2id_lock);
          
          if(mid && !pmath_aborting()) {
            jvalue val;
            
            if(modifiers & PJ_MODIFIER_STATIC) {
              switch(return_type) {
                case 'Z':
                  val.z = (*env)->CallStaticBooleanMethodA(env, clazz, mid, jargs);
                  break;
                  
                case 'B':
                  val.b = (*env)->CallStaticByteMethodA(env, clazz, mid, jargs);
                  break;
                  
                case 'C':
                  val.c = (*env)->CallStaticCharMethodA(env, clazz, mid, jargs);
                  break;
                  
                case 'S':
                  val.s = (*env)->CallStaticShortMethodA(env, clazz, mid, jargs);
                  break;
                  
                case 'I':
                  val.i = (*env)->CallStaticIntMethodA(env, clazz, mid, jargs);
                  break;
                  
                case 'J':
                  val.j = (*env)->CallStaticLongMethodA(env, clazz, mid, jargs);
                  break;
                  
                case 'F':
                  val.f = (*env)->CallStaticFloatMethodA(env, clazz, mid, jargs);
                  break;
                  
                case 'D':
                  val.d = (*env)->CallStaticDoubleMethodA(env, clazz, mid, jargs);
                  break;
                  
                case 'L':
                case '[':
                  if((*env)->EnsureLocalCapacity(env, 1) != 0) {
                    pj_exception_to_pmath(env);
                    val.l = NULL;
                  }
                  else
                    val.l = (*env)->CallStaticObjectMethodA(env, clazz, mid, jargs);
                  break;
                  
                case 'V':
                  (*env)->CallStaticVoidMethodA(env, clazz, mid, jargs);
                  break;
                  
                default:
                  pmath_debug_print("\ainvalid java type `%c`\n", return_type);
                  assert("invalid java type" && 0);
              }
              
              result = pj_value_from_java(env, return_type, &val);
            }
            else if(is_static) {
              /* error: trying to call non-static method without an object */
            }
            else {
              switch(return_type) {
                case 'Z':
                  val.z = (*env)->CallBooleanMethodA(env, obj, mid, jargs);
                  break;
                  
                case 'B':
                  val.b = (*env)->CallByteMethodA(env, obj, mid, jargs);
                  break;
                  
                case 'C':
                  val.c = (*env)->CallCharMethodA(env, obj, mid, jargs);
                  break;
                  
                case 'S':
                  val.s = (*env)->CallShortMethodA(env, obj, mid, jargs);
                  break;
                  
                case 'I':
                  val.i = (*env)->CallIntMethodA(env, obj, mid, jargs);
                  break;
                  
                case 'J':
                  val.j = (*env)->CallLongMethodA(env, obj, mid, jargs);
                  break;
                  
                case 'F':
                  val.f = (*env)->CallFloatMethodA(env, obj, mid, jargs);
                  break;
                  
                case 'D':
                  val.d = (*env)->CallDoubleMethodA(env, obj, mid, jargs);
                  break;
                  
                case 'L':
                case '[':
                  if((*env)->EnsureLocalCapacity(env, 1) != 0) {
                    pj_exception_to_pmath(env);
                    val.l = NULL;
                  }
                  else
                    val.l = (*env)->CallObjectMethodA(env, obj, mid, jargs);
                  break;
                  
                case 'V':
                  (*env)->CallVoidMethodA(env, obj, mid, jargs);
                  break;
                  
                default:
                  pmath_debug_print("\ainvalid java type `%c`\n", return_type);
                  assert("invalid java type" && 0);
              }
              
              result = pj_value_from_java(env, return_type, &val);
            }
          }
        }
        
        (*env)->PopLocalFrame(env, NULL);
      }
      
      pj_exception_to_pmath(env);
      pmath_unref(types);
    }
    
    pmath_mem_free(jargs);
  }
  
  pmath_unref(key);
  pmath_unref(signatures);
    
  if(pmath_same(result, PMATH_UNDEFINED)) {
    if(num_args == 0) {
      pmath_unref(args);
      pj_thread_message(msg_thread,
                        PJ_SYMBOL_JAVA, "argx0", 2,
                        name,
                        pj_class_get_nice_name(env, clazz));
    }
    else {
      args = pmath_expr_set_item(args, 0, pmath_ref(PMATH_SYMBOL_LIST));
      
      pj_thread_message(msg_thread,
                        PJ_SYMBOL_JAVA, "argx", 3,
                        name,
                        pj_class_get_nice_name(env, clazz),
                        args);
    }
    
    name = PMATH_UNDEFINED;
    args = PMATH_UNDEFINED;
    result = pmath_ref(PMATH_SYMBOL_FAILED);
  }
  
  if(!is_static)
    (*env)->DeleteLocalRef(env, clazz);
  
  pmath_unref(name);
  pmath_unref(args);
  
  return result;
}

jobject pj_class_new_object(
  JNIEnv           *env,
  jclass            clazz,
  pmath_expr_t      args,        // will be freed
  pmath_messages_t  msg_thread   // wont be freed
) {
  jvalue         *jargs;
  pmath_string_t  class_name;
  pmath_t         key;
  pmath_expr_t    signatures;
  jint            num_args;
  jobject         result;
  
  pmath_unref(pj_thread_get_companion(NULL));
  
  if( !env || 
      !clazz || 
      (*env)->EnsureLocalCapacity(env, 1) != 0) 
  {
    pj_exception_to_pmath(env);
    pmath_unref(args);
    return NULL;
  }
  
  pj_class_cache_members(env, clazz);
  class_name = pj_class_get_name(env, clazz);
  num_args = (jint)pmath_expr_length(args);
  
  key = pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_LIST), 2,
          class_name,
          PMATH_C_STRING("<init>"));
          
  class_name = PMATH_NULL;
  signatures = PMATH_NULL;
  pmath_atomic_lock(&cms2id_lock);
  {
    struct pmath2id_t *cache_entry = pmath_ht_search(cms2id, &key);
    if(cache_entry && cache_entry->fid == 0) {
      signatures = pmath_ref(cache_entry->info);
    }
  }
  pmath_atomic_unlock(&cms2id_lock);
  
  if(!pmath_is_expr_of(signatures, PMATH_SYMBOL_LIST)) {
    // should not happen: every Java Class has a constructor <init>
    pj_thread_message(msg_thread,
                      PJ_SYMBOL_JAVANEW, "fail", 1,
                      class_name);
    pmath_unref(args);
    pmath_unref(key);
    
    return NULL;
  }
  
  result = NULL;
  key = pmath_expr_append(key, 1, NULL);
  jargs = pmath_mem_alloc(num_args * sizeof(jvalue));
  if(jargs) {
    size_t i;
    
    for(i = 1; i <= pmath_expr_length(signatures) && !result; ++i) {
      pmath_t types = pmath_expr_get_item(signatures, i);
      
      if((*env)->PushLocalFrame(env, num_args) == 0) {
        if(pj_value_fill_args(env, types, args, jargs)) {
          jmethodID mid       = 0;
          int       modifiers = 0;
          
          key = pmath_expr_set_item(key, 3, pmath_ref(types));
          
          pmath_atomic_lock(&cms2id_lock);
          {
            struct pmath2id_t *cache_entry = pmath_ht_search(cms2id, &key);
            if(cache_entry && cache_entry->mid != 0) {
              mid       = cache_entry->mid;
              modifiers = cache_entry->modifiers;
            }
          }
          pmath_atomic_unlock(&cms2id_lock);
          
          if(mid && !pmath_aborting()) {
            result = (*env)->NewObjectA(env, clazz, mid, jargs);
          }
        }
        
        result = (*env)->PopLocalFrame(env, result);
      }
      
      pj_exception_to_pmath(env);
      pmath_unref(types);
    }
    
    pmath_mem_free(jargs);
  }
  
  pmath_unref(key);
  pmath_unref(signatures);
  
  if(!result) {
    if(num_args == 0) {
      pmath_unref(args);
      pj_thread_message(msg_thread,
                        PJ_SYMBOL_JAVANEW, "argx0", 1,
                        pj_class_get_nice_name(env, clazz));
    }
    else {
      args = pmath_expr_set_item(args, 0, pmath_ref(PMATH_SYMBOL_LIST));
      pj_thread_message(msg_thread,
                        PJ_SYMBOL_JAVANEW, "argx", 2,
                        pj_class_get_nice_name(env, clazz),
                        args);
    }
    
    return NULL;
  }
  
  pmath_unref(args);
  return result;
}


pmath_t pj_class_get_field(
  JNIEnv         *env,
  jobject         obj,
  pmath_bool_t    is_static,
  pmath_string_t  name        // will be freed
) {
  jclass          clazz;
  pmath_string_t  class_name;
  pmath_t         key;
  jfieldID        fid;
  char            return_type;
  int             modifiers;
  jvalue          val;
  pmath_t         result;
  
  if(!env
      || !obj
      || (*env)->EnsureLocalCapacity(env, 1) != 0) {
    pj_exception_to_pmath(env);
    pmath_unref(name);
    return pmath_ref(PMATH_SYMBOL_FAILED);
  }
  
  if(is_static)
    clazz = obj;
  else
    clazz = (*env)->GetObjectClass(env, obj);
    
  pj_class_cache_members(env, clazz);
  class_name = pj_class_get_name(env, clazz);
  
  key = pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_LIST), 2,
          pmath_ref(class_name),
          pmath_ref(name));
          
  fid         = 0;
  return_type = '?';
  modifiers   = 0;
  pmath_atomic_lock(&cms2id_lock);
  {
    struct pmath2id_t *cache_entry = pmath_ht_search(cms2id, &key);
    if(cache_entry && cache_entry->mid == 0) {
      fid         = cache_entry->fid;
      return_type = cache_entry->return_type;
      modifiers   = cache_entry->modifiers;
    }
  }
  pmath_atomic_unlock(&cms2id_lock);
  pmath_unref(key); key = PMATH_NULL;
  
  if(!fid) {
    pmath_message(PJ_SYMBOL_JAVA, "nofld", 2,
                  name,
                  class_name);
    if(!is_static)
      (*env)->DeleteLocalRef(env, clazz);
    return pmath_ref(PMATH_SYMBOL_FAILED);
  }
  
  result = PMATH_NULL;
  if(modifiers & PJ_MODIFIER_STATIC) {
    switch(return_type) {
      case 'Z':
        val.z = (*env)->GetStaticBooleanField(env, clazz, fid);
        break;
        
      case 'B':
        val.b = (*env)->GetStaticByteField(env, clazz, fid);
        break;
        
      case 'C':
        val.c = (*env)->GetStaticCharField(env, clazz, fid);
        break;
        
      case 'S':
        val.s = (*env)->GetStaticShortField(env, clazz, fid);
        break;
        
      case 'I':
        val.i = (*env)->GetStaticIntField(env, clazz, fid);
        break;
        
      case 'J':
        val.j = (*env)->GetStaticLongField(env, clazz, fid);
        break;
        
      case 'F':
        val.f = (*env)->GetStaticFloatField(env, clazz, fid);
        break;
        
      case 'D':
        val.d = (*env)->GetStaticDoubleField(env, clazz, fid);
        break;
        
      case 'L':
      case '[':
        if((*env)->EnsureLocalCapacity(env, 1) != 0) {
          pj_exception_to_pmath(env);
          val.l = NULL;
        }
        else
          val.l = (*env)->GetStaticObjectField(env, clazz, fid);
        break;
        
      default:
        pmath_debug_print("\ainvalid java type `%c`\n", return_type);
        assert("invalid java type" && 0);
    }
    
    result = pj_value_from_java(env, return_type, &val);
    if(return_type == 'L' || return_type == '[')
      (*env)->DeleteLocalRef(env, val.l);
  }
  else if(is_static) {
    /* error: trying to get non-static field without an object */
    result = pmath_ref(PMATH_SYMBOL_FAILED);
  }
  else {
    switch(return_type) {
      case 'Z':
        val.z = (*env)->GetBooleanField(env, obj, fid);
        break;
        
      case 'B':
        val.b = (*env)->GetByteField(env, obj, fid);
        break;
        
      case 'C':
        val.c = (*env)->GetCharField(env, obj, fid);
        break;
        
      case 'S':
        val.s = (*env)->GetShortField(env, obj, fid);
        break;
        
      case 'I':
        val.i = (*env)->GetIntField(env, obj, fid);
        break;
        
      case 'J':
        val.j = (*env)->GetLongField(env, obj, fid);
        break;
        
      case 'F':
        val.f = (*env)->GetFloatField(env, obj, fid);
        break;
        
      case 'D':
        val.d = (*env)->GetDoubleField(env, obj, fid);
        break;
        
      case 'L':
      case '[':
        if((*env)->EnsureLocalCapacity(env, 1) != 0) {
          pj_exception_to_pmath(env);
          val.l = NULL;
        }
        else
          val.l = (*env)->GetObjectField(env, obj, fid);
        break;
        
      default:
        pmath_debug_print("\ainvalid java type `%c`\n", return_type);
        assert("invalid java type" && 0);
    }
    
    result = pj_value_from_java(env, return_type, &val);
    if(return_type == 'L' || return_type == '[')
      (*env)->DeleteLocalRef(env, val.l);
  }
  
  pmath_unref(name);
  pmath_unref(class_name);
  if(!is_static)
    (*env)->DeleteLocalRef(env, clazz);
  return result;
}

extern pmath_bool_t pj_class_set_field(
  JNIEnv         *env,
  jobject         obj,
  pmath_bool_t    is_static,
  pmath_string_t  name,       // will be freed
  pmath_t         value
) {
  jclass          clazz;
  pmath_string_t  class_name;
  pmath_t         key;
  jfieldID        fid;
  pmath_t         field_type;
  char            field_type_char;
  int             modifiers;
  jvalue          val;
  pmath_bool_t    result;
  
  if(!env
      || !obj
      || (*env)->EnsureLocalCapacity(env, 1) != 0) {
    pj_exception_to_pmath(env);
    pmath_unref(name);
    pmath_unref(value);
    return FALSE;
  }
  
  if(is_static)
    clazz = obj;
  else
    clazz = (*env)->GetObjectClass(env, obj);
    
  pj_class_cache_members(env, clazz);
  class_name = pj_class_get_name(env, clazz);
  
  key = pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_LIST), 2,
          pmath_ref(class_name),
          pmath_ref(name));
          
  fid             = 0;
  field_type      = PMATH_NULL;
  field_type_char = '?';
  modifiers       = 0;
  pmath_atomic_lock(&cms2id_lock);
  {
    struct pmath2id_t *cache_entry = pmath_ht_search(cms2id, &key);
    if(cache_entry && cache_entry->mid == 0) {
      fid             = cache_entry->fid;
      field_type      = pmath_ref(cache_entry->info);
      field_type_char = cache_entry->return_type;
      modifiers       = cache_entry->modifiers;
    }
  }
  pmath_atomic_unlock(&cms2id_lock);
  pmath_unref(key); key = PMATH_NULL;
  
  if(!fid) {
    pmath_message(PJ_SYMBOL_JAVA, "nofld", 2,
                  name,
                  class_name);
    pmath_unref(field_type);
    pmath_unref(value);
    if(!is_static)
      (*env)->DeleteLocalRef(env, clazz);
    return FALSE;
  }
  
  if(!pj_value_to_java(env, pmath_ref(value), field_type, &val)) {
    pmath_message(PJ_SYMBOL_JAVA, "fldx", 3,
                  name,
                  class_name,
                  value);
    pmath_unref(field_type);
    if(!is_static)
      (*env)->DeleteLocalRef(env, clazz);
    return FALSE;
  }
  pmath_unref(value);      value      = PMATH_NULL;
  pmath_unref(field_type); field_type = PMATH_NULL;
  
  result = TRUE;
  if(modifiers & PJ_MODIFIER_STATIC) {
    switch(field_type_char) {
      case 'Z':
        (*env)->SetStaticBooleanField(env, clazz, fid, val.z);
        break;
        
      case 'B':
        (*env)->SetStaticByteField(env, clazz, fid, val.b);
        break;
        
      case 'C':
        (*env)->SetStaticCharField(env, clazz, fid, val.c);
        break;
        
      case 'S':
        (*env)->SetStaticShortField(env, clazz, fid, val.s);
        break;
        
      case 'I':
        (*env)->SetStaticIntField(env, clazz, fid, val.i);
        break;
        
      case 'J':
        (*env)->SetStaticLongField(env, clazz, fid, val.j);
        break;
        
      case 'F':
        (*env)->SetStaticFloatField(env, clazz, fid, val.f);
        break;
        
      case 'D':
        (*env)->SetStaticDoubleField(env, clazz, fid, val.d);
        break;
        
      case 'L':
      case '[':
        (*env)->SetStaticObjectField(env, clazz, fid, val.l);
        (*env)->DeleteLocalRef(env, val.l);
        break;
        
      default:
        pmath_debug_print("\ainvalid java type `%c`\n", field_type_char);
        assert("invalid java type" && 0);
        result = FALSE;
    }
  }
  else if(is_static) {
    /* error: trying to set non-static field without an object */
    result = FALSE;
  }
  else {
    switch(field_type_char) {
      case 'Z':
        (*env)->SetBooleanField(env, obj, fid, val.z);
        break;
        
      case 'B':
        (*env)->SetByteField(env, obj, fid, val.b);
        break;
        
      case 'C':
        (*env)->SetCharField(env, obj, fid, val.c);
        break;
        
      case 'S':
        (*env)->SetShortField(env, obj, fid, val.s);
        break;
        
      case 'I':
        (*env)->SetIntField(env, obj, fid, val.i);
        break;
        
      case 'J':
        (*env)->SetLongField(env, obj, fid, val.j);
        break;
        
      case 'F':
        (*env)->SetFloatField(env, obj, fid, val.f);
        break;
        
      case 'D':
        (*env)->SetDoubleField(env, obj, fid, val.d);
        break;
        
      case 'L':
      case '[':
        (*env)->SetObjectField(env, obj, fid, val.l);
        (*env)->DeleteLocalRef(env, val.l);
        break;
        
      default:
        pmath_debug_print("\ainvalid java type `%c`\n", field_type_char);
        assert("invalid java type" && 0);
        result = FALSE;
    }
  }
  
  pmath_unref(name);
  pmath_unref(class_name);
  if(!is_static)
    (*env)->DeleteLocalRef(env, clazz);
  return result;
}


pmath_bool_t pj_classes_init(void) {
  cms2id = pmath_ht_create(&cms2id_class, 0);
  if(!cms2id)
    return FALSE;
    
  return TRUE;
}

void pj_classes_done(void) {
  pmath_ht_destroy(cms2id); cms2id = NULL;
}
