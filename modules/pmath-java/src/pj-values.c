#include "pj-values.h"
#include "pj-classes.h"
#include "pj-objects.h"
#include "pj-symbols.h"

#include <math.h>
#include <string.h>


#ifndef NAN
  
  static uint64_t nan_as_int = ((uint64_t)1 << 63) - 1;
  #define NAN (*(double*)&nan_as_int)
  
#endif


pmath_string_t pj_string_from_java(JNIEnv *env, jstring jstr){
  pmath_string_t s;
  const jchar *buf;
  int len;
  
  len = (int)(*env)->GetStringLength(env, jstr);
  if(len < 0){
    (*env)->ExceptionDescribe(env);
    return NULL;
  }
  
  buf = (*env)->GetStringCritical(env, jstr, NULL);
  s = pmath_string_insert_ucs2(NULL, 0, buf, len);
  (*env)->ReleaseStringCritical(env, jstr, buf);
  
  return s;
}

jstring pj_string_to_java(JNIEnv *env, pmath_string_t str){
  const jchar *buf = (const jchar*)pmath_string_buffer(str);
  int len = pmath_string_length(str);
  
  return (*env)->NewString(env, buf, len);
}


// obj will be freed; type wont be freed
pmath_bool_t pj_value_to_java(JNIEnv *env, pmath_t obj, pmath_t type, jvalue *value){
  if(!env || !value){
    pmath_unref(obj);
    return FALSE;
  }
  
  memset(value, 0, sizeof(jvalue));
  if(type == PJ_SYMBOL_TYPE_BOOLEAN){
    pmath_unref(obj);
    
    if(obj == PMATH_SYMBOL_TRUE){
      value->z = JNI_TRUE;
      return TRUE;
    }
    
    if(obj == PMATH_SYMBOL_FALSE){
      value->z = JNI_FALSE;
      return TRUE;
    }
    
    return FALSE;
  }
  
  if(type == PJ_SYMBOL_TYPE_BYTE){
    long i;
    
    if(!pmath_instance_of(obj, PMATH_TYPE_INTEGER)
    || !pmath_integer_fits_si(obj)){
      pmath_unref(obj);
      return FALSE;
    }
    
    i = pmath_integer_get_si(obj);
    pmath_unref(obj);
    
    if(i < -128 || i > 127)
      return FALSE;
    
    value->b = (jbyte)i;
    return TRUE;
  }
  
  if(type == PJ_SYMBOL_TYPE_CHAR){
    if(pmath_instance_of(obj, PMATH_TYPE_STRING)
    && pmath_string_length(obj) == 1){
      value->c = (jchar)pmath_string_buffer(obj)[0];
      pmath_unref(obj);
      return TRUE;
    }
    
    pmath_unref(obj);
    return FALSE;
  }
  
  if(type == PJ_SYMBOL_TYPE_SHORT){
    long i;
    
    if(!pmath_instance_of(obj, PMATH_TYPE_INTEGER)
    || !pmath_integer_fits_si(obj)){
      pmath_unref(obj);
      return FALSE;
    }
    
    i = pmath_integer_get_si(obj);
    pmath_unref(obj);
    
    if(i < -32768 || i > 32767)
      return FALSE;
    
    value->s = (jshort)i;
    return TRUE;
  }
  
  if(type == PJ_SYMBOL_TYPE_INT){
    long i;
    
    if(!pmath_instance_of(obj, PMATH_TYPE_INTEGER)
    || !pmath_integer_fits_si(obj)){
      pmath_unref(obj);
      return FALSE;
    }
    
    i = pmath_integer_get_si(obj);
    pmath_unref(obj);
    
    if(i < -2147483647-1 || i > 2147483647)
      return FALSE;
    
    value->i = (jint)i;
    return TRUE;
  }
  
  if(type == PJ_SYMBOL_TYPE_LONG){
    int64_t j;
    
    if(!pmath_instance_of(obj, PMATH_TYPE_INTEGER)
    || !pmath_integer_fits_si64(obj)){
      pmath_unref(obj);
      return FALSE;
    }
    
    j = pmath_integer_get_si64(obj);
    pmath_unref(obj);
    
    value->j = (jlong)j;
    return TRUE;
  }
  
  if(type == PJ_SYMBOL_TYPE_FLOAT
  || type == PJ_SYMBOL_TYPE_DOUBLE){
    double d = 0;
    
    if(pmath_instance_of(obj, PMATH_TYPE_NUMBER)){
      d = pmath_number_get_d(obj);
    }
    else if(obj == PMATH_SYMBOL_UNDEFINED
    || pmath_is_expr_of_len(obj, PMATH_SYMBOL_DIRECTEDINFINITY, 0)){
      d = NAN;
    }
    else if(pmath_is_expr_of_len(obj, PMATH_SYMBOL_DIRECTEDINFINITY, 1)){
      pmath_t dir = pmath_expr_get_item(obj, 1);
      
      if(pmath_instance_of(dir, PMATH_TYPE_NUMBER)){
        int sign = pmath_number_sign(dir);
        pmath_unref(dir);
        
        if(sign < 0)
          d = -HUGE_VAL;
        else if(sign > 0)
          d = HUGE_VAL;
        else
          d = NAN;
      }
      else{
        pmath_unref(dir);
        pmath_unref(obj);
        return FALSE;
      }
    }
    else{
      pmath_unref(obj);
      return FALSE;
    }
    
    if(type == PJ_SYMBOL_TYPE_FLOAT)
      value->f = (float)d;
    else
      value->d = d;
    
    pmath_unref(obj);
    return TRUE;
  }
  
  if(pmath_instance_of(obj, PMATH_TYPE_STRING)){
    if(pmath_instance_of(type, PMATH_TYPE_STRING)
    && pmath_string_equals_latin1(type, "Ljava/lang/String;")){
      value->l = (*env)->NewString(env, pmath_string_buffer(obj), pmath_string_length(obj));
      pmath_unref(obj);
      return TRUE;
    }
    
    pmath_unref(obj);
    return FALSE;
  }
  
  if(pmath_is_expr_of_len(type, PJ_SYMBOL_TYPE_ARRAY, 1)){
    if(!obj){
      value->l = NULL;
      return TRUE;
    }
    
    if(pmath_is_expr_of(obj, PMATH_SYMBOL_LIST)){
      pmath_t elem_type = pmath_expr_get_item(type, 1);
      jsize len = (jsize)pmath_expr_length(obj);
      jobject arr = NULL;
      size_t item_size = 0;
      
      if(elem_type == PJ_SYMBOL_TYPE_BOOLEAN){
        item_size = sizeof(jboolean);
        arr = (*env)->NewBooleanArray(env, len);
      }
      else if(elem_type == PJ_SYMBOL_TYPE_BYTE){
        item_size = sizeof(jbyte);
        arr = (*env)->NewByteArray(env, len);
      }
      else if(elem_type == PJ_SYMBOL_TYPE_CHAR){
        item_size = sizeof(jchar);
        arr = (*env)->NewCharArray(env, len);
      }
      else if(elem_type == PJ_SYMBOL_TYPE_SHORT){
        item_size = sizeof(jshort);
        arr = (*env)->NewShortArray(env, len);
      }
      else if(elem_type == PJ_SYMBOL_TYPE_INT){
        item_size = sizeof(jint);
        arr = (*env)->NewIntArray(env, len);
      }
      else if(elem_type == PJ_SYMBOL_TYPE_LONG){
        item_size = sizeof(jlong);
        arr = (*env)->NewLongArray(env, len);
      }
      else if(elem_type == PJ_SYMBOL_TYPE_FLOAT){
        item_size = sizeof(jfloat);
        arr = (*env)->NewFloatArray(env, len);
      }
      else if(elem_type == PJ_SYMBOL_TYPE_DOUBLE){
        item_size = sizeof(jdouble);
        arr = (*env)->NewDoubleArray(env, len);
      }
      else if((*env)->EnsureLocalCapacity(env, 2) == 0){
        jclass item_class = pj_class_to_java(env, pmath_ref(elem_type));
        
        if(item_class){
          arr = (*env)->NewObjectArray(env, len, item_class, NULL);
          
          (*env)->DeleteLocalRef(env, item_class);
        }
      }
      
      if(arr){
        pmath_bool_t success = TRUE;
        value->l = arr;
        
        if(item_size == 0){
          jsize i;
          for(i = 0;i < len;++i){
            pmath_t item = pmath_expr_get_item(obj, (size_t)i + 1);
            jvalue val;
            
            success = pj_value_to_java(env, item, elem_type, &val);
            
            (*env)->SetObjectArrayElement(env, arr, i, val.l);
            
            if(val.l)
              (*env)->DeleteLocalRef(env, val.l);
            
            if(!success)
              break;
          }
        }
        else{
          void *data = (*env)->GetPrimitiveArrayCritical(env, arr, NULL);
          if(data){
            jsize i;
            for(i = 0;i < len;++i){
              pmath_t item = pmath_expr_get_item(obj, (size_t)i + 1);
              
              if(!pj_value_to_java(env, item, elem_type, (jvalue*)(data + item_size * (size_t)i))){
                success = FALSE;
                break;
              }
            }
            
            (*env)->ReleasePrimitiveArrayCritical(env, arr, data, 0);
          }
          else
            success = FALSE;
        }
        
        pmath_unref(elem_type);
        pmath_unref(obj);
        return success;
      }
    
      pmath_unref(elem_type);
      pmath_unref(obj);
      return FALSE;
    }
  }
  
  if(pj_object_is_java(env, obj)
  && (*env)->EnsureLocalCapacity(env, 3) == 0){
    pmath_bool_t success = FALSE;
    jclass dst_class = pj_class_to_java(env, pmath_ref(type));
    
    if(dst_class){
      jobject val = pj_object_to_java(env, obj); obj = NULL;
      
      if(val){
        jclass src_class = (*env)->GetObjectClass(env, val);
        
        if(src_class){
          if((*env)->IsAssignableFrom(env, dst_class, src_class)){
            value->l = val;
            val = NULL;
          }
          
          (*env)->DeleteLocalRef(env, src_class);
        }
      }
      
      if(val)
        (*env)->DeleteLocalRef(env, val);
        
      (*env)->DeleteLocalRef(env, dst_class);
    }
    
    pmath_unref(obj);
    return success;
  }
  
  pmath_unref(obj);
  return FALSE;
}


pmath_t pj_value_from_java(JNIEnv *env, char type, const jvalue *value){
  switch(type){
    case 'Z':  return pmath_build_value("b", (int)value->z);
    case 'B':  return pmath_build_value("i", (int)value->b);
    case 'C':  return pmath_build_value("c", (uint16_t)value->c);
    case 'S':  return pmath_build_value("i", (int)value->s);
    case 'I':  return pmath_build_value("i", (int)value->i);
    case 'J':  return pmath_build_value("k", value->j);
    case 'F':  return pmath_build_value("f", (double)value->f);
    case 'D':  return pmath_build_value("f", value->d);
    
    case 'L':
    case '[': break;
    
    default:
      return NULL;
  }
  
  if(!value->l)
    return NULL;
  
  if((*env)->EnsureLocalCapacity(env, 2) == 0){
    jclass  clazz = (*env)->GetObjectClass(env, value->l);
    pmath_t class_name  = pj_class_get_name(env, clazz);
    int slen            = pmath_string_length(class_name);
    const uint16_t *buf = pmath_string_buffer(class_name);
    (*env)->DeleteLocalRef(env, clazz);
    
    if(pmath_string_equals_latin1(class_name, "Ljava/lang/String;")){
      pmath_unref(class_name);
      return pj_string_from_java(env, value->l);
    }
    
    if(slen > 0 && buf[0] == '['){
      pmath_bool_t is_simple_array = FALSE;
      
      int i = 1;
      while(i < slen && buf[i] == '[')
        ++i;
      
      is_simple_array = TRUE;
//      if(i < slen && buf[i] != 'L'){
//        is_simple_array = TRUE;
//      }
//      else if(i + 18 == slen 
//      && buf[i + 11] == 'S'
//      && buf[i + 12] == 't'
//      && buf[i + 13] == 'r'
//      && buf[i + 14] == 'i'
//      && buf[i + 15] == 'n'
//      && buf[i + 16] == 'g'
//      && buf[i +  5] == '/'
//      && buf[i +  6] == 'l'
//      && buf[i +  7] == 'a'
//      && buf[i +  8] == 'n'
//      && buf[i +  9] == 'g'
//      && buf[i + 10] == '/'
//      && buf[i +  1] == 'j'
//      && buf[i +  2] == 'a'
//      && buf[i +  3] == 'v'
//      && buf[i +  4] == 'a'
//      && buf[i]      == 'L'
//      && buf[i + 17] == ';'){
//        is_simple_array = TRUE;
//      }
      
      if(is_simple_array){
        jsize len = (*env)->GetArrayLength(env, value->l);
        pmath_t arr = pmath_expr_new(pmath_ref(PMATH_SYMBOL_LIST), (size_t)len);
        size_t item_size = 0;
        
        switch(buf[1]){
          case 'Z': item_size = sizeof(value->z); break;
          case 'B': item_size = sizeof(value->b); break;
          case 'S': item_size = sizeof(value->s); break;
          case 'C': item_size = sizeof(value->c); break;
          case 'I': item_size = sizeof(value->i); break;
          case 'J': item_size = sizeof(value->j); break;
          case 'F': item_size = sizeof(value->f); break;
          case 'D': item_size = sizeof(value->d); break;
        }
        
        if(item_size == 0){
          jsize i;
          jvalue v;
          
          for(i = len;i > 0;--i){
            v.l = (*env)->GetObjectArrayElement(env, value->l, i-1);
            
            arr = pmath_expr_set_item(arr, (size_t)i, 
              pj_value_from_java(env, (char)buf[1], &v));
            
            (*env)->DeleteLocalRef(env, v.l);
          }
        }
        else{
          void *data = (*env)->GetPrimitiveArrayCritical(env, value->l, NULL);
          
          if(data){
            size_t i;
            
            for(i = 0;i < (size_t)len;++i){
              pmath_t item = pj_value_from_java(env, (char)buf[1], (const jvalue*)(data + i * item_size));
              
              arr = pmath_expr_set_item(arr, i + 1, item);
            }
            
            (*env)->ReleasePrimitiveArrayCritical(env, value->l, data, 0);
          }
        }
        
        pmath_unref(class_name);
        return arr;
      }
    }
    
    pmath_unref(class_name);
    return pj_object_from_java(env, value->l);
  }
  
  return NULL;
}


// args and types wont be freed
pmath_bool_t pj_value_fill_args(JNIEnv *env, pmath_expr_t types, pmath_expr_t args, jvalue *jargs){
  size_t i;
  size_t len = pmath_expr_length(args);
  
  if(len != pmath_expr_length(types))
    return FALSE;
  
  for(i = 1;i <= len;++i){
    pmath_t arg  = pmath_expr_get_item(args, i);
    pmath_t type = pmath_expr_get_item(types, i);
    
    if(!pj_value_to_java(env, arg, type, &jargs[i-1])){
      pmath_unref(type);
      return FALSE;
    }
    
    pmath_unref(type);
  }
  
  return TRUE;
}
