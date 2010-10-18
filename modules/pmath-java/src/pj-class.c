#include "pj-class.h"
#include "pj-symbols.h"
#include "pj-value.h"

#include <limits.h>


pmath_string_t pj_class_get_name(JNIEnv *env, jclass clazz){
  jclass cc;
  jmethodID id;
  jstring jstr;
  pmath_string_t result = NULL;
  
  if(!env || !clazz)
    return NULL;
  
  cc = (*env)->GetObjectClass(env, clazz);
  if(!cc)
    goto FAIL_CC;
  
  id = (*env)->GetMethodID(env, cc, "getName", "()Ljava/lang/String;");
  if(!id)
    goto FAIL_ID;
  
  jstr = (*env)->CallObjectMethod(env, clazz, id);
  result = pj_string_from_java(env, jstr);
  
                (*env)->DeleteLocalRef(env, jstr);
  FAIL_ID:      (*env)->DeleteLocalRef(env, cc);
  FAIL_CC:
  
  return result;
}


jclass pj_class_get_java(JNIEnv *env, pmath_t obj){
  jclass result = NULL;
  
  if(env){
    pmath_t name = NULL;
    
    if(pmath_is_expr_of(obj, PJ_SYMBOL_JAVACLASS)){
      name = pmath_expr_get_item(obj, 1);
    }
    else
      name = pmath_ref(obj);
      
    if(pmath_instance_of(name, PMATH_TYPE_STRING)){
      int len;
      char *str = pmath_string_to_utf8(name, &len);
      
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
        
        if(str){
          result = (*env)->FindClass(env, str);
          pmath_mem_free(str);
        }
      }
    }
    
    pmath_unref(name);
  }
  
  pmath_unref(obj);
  return result;
}


pmath_bool_t pj_class_init_module(void){
  return TRUE;
}

void pj_class_done_module(void){
}
