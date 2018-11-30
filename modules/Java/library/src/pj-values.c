#include "pj-values.h"
#include "pj-classes.h"
#include "pj-objects.h"
#include "pj-symbols.h"
#include "pjvm.h"

#include <math.h>
#include <string.h>


#ifndef NAN

static uint64_t nan_as_int = ((uint64_t)1 << 63) - 1;
#define NAN (*(double*)&nan_as_int)

#endif


pmath_string_t pj_string_from_java(JNIEnv *env, jstring jstr) {
  pmath_string_t s;
  const jchar *buf;
  int len;
  
  len = (int)(*env)->GetStringLength(env, jstr);
  if(len < 0) {
    (*env)->ExceptionDescribe(env);
    return PMATH_NULL;
  }
  
  buf = (*env)->GetStringCritical(env, jstr, NULL);
  s = pmath_string_insert_ucs2(PMATH_NULL, 0, buf, len);
  (*env)->ReleaseStringCritical(env, jstr, buf);
  
  return s;
}

jstring pj_string_to_java(JNIEnv *env, pmath_string_t str) {
  const jchar *buf = (const jchar*)pmath_string_buffer(&str);
  int len = pmath_string_length(str);
  
  jstring result = (*env)->NewString(env, buf, len);
  pmath_unref(str);
  return result;
}

// obj will be freed
static pmath_bool_t bool_to_java(pmath_t obj, jvalue *value) {
  pmath_unref(obj);
  
  if(pmath_same(obj, PMATH_SYMBOL_TRUE)) {
    value->z = JNI_TRUE;
    return TRUE;
  }
  
  if(pmath_same(obj, PMATH_SYMBOL_FALSE)) {
    value->z = JNI_FALSE;
    return TRUE;
  }
  
  return FALSE;
}

// obj will be freed
static pmath_bool_t char_to_java(pmath_t obj, jvalue *value) {
  if(pmath_is_string(obj) && pmath_string_length(obj) == 1) {
    value->c = (jchar)pmath_string_buffer(&obj)[0];
    pmath_unref(obj);
    return TRUE;
  }
  
  pmath_unref(obj);
  return FALSE;
}

// obj will be freed
static pmath_bool_t byte_to_java(pmath_t obj, jvalue *value) {
  int32_t i;
  
  if(!pmath_is_int32(obj)) {
    pmath_unref(obj);
    return FALSE;
  }
  
  i = PMATH_AS_INT32(obj);
  
  if(i < -128 || i > 127)
    return FALSE;
    
  value->b = (jbyte)i;
  return TRUE;
}

// obj will be freed
static pmath_bool_t short_to_java(pmath_t obj, jvalue *value) {
  int32_t i;
  
  if(!pmath_is_int32(obj)) {
    pmath_unref(obj);
    return FALSE;
  }
  
  i = PMATH_AS_INT32(obj);
  
  if(i < -32768 || i > 32767)
    return FALSE;
    
  value->s = (jshort)i;
  return TRUE;
}

// obj will be freed
static pmath_bool_t int_to_java(pmath_t obj, jvalue *value) {
  if(!pmath_is_int32(obj)) {
    pmath_unref(obj);
    return FALSE;
  }
  
  value->i = PMATH_AS_INT32(obj);
  return TRUE;
}

// obj will be freed
static pmath_bool_t long_to_java(pmath_t obj, jvalue *value) {
  int64_t j;
  
  if(!pmath_is_int32(obj) || !pmath_integer_fits_si64(obj)) {
    pmath_unref(obj);
    return FALSE;
  }
  
  j = pmath_integer_get_si64(obj);
  pmath_unref(obj);
  
  value->j = j;
  return TRUE;
}

// obj will be freed
static pmath_bool_t double_to_java(pmath_t obj, jvalue *value) {
  double d = 0;
  
  if(pmath_is_float(obj)) {
    d = pmath_number_get_d(obj);
  }
  else if(pmath_same(obj, PMATH_SYMBOL_UNDEFINED) || pmath_is_expr_of_len(obj, PMATH_SYMBOL_DIRECTEDINFINITY, 0)) {
    d = NAN;
  }
  else if(pmath_is_expr_of_len(obj, PMATH_SYMBOL_DIRECTEDINFINITY, 1)) {
    pmath_t dir = pmath_expr_get_item(obj, 1);
    
    if(pmath_is_number(dir)) {
      int sign = pmath_number_sign(dir);
      pmath_unref(dir);
      
      if(sign < 0)
        d = -HUGE_VAL;
      else if(sign > 0)
        d = HUGE_VAL;
      else
        d = NAN;
    }
    else {
      pmath_unref(dir);
      pmath_unref(obj);
      return FALSE;
    }
  }
  else {
    pmath_unref(obj);
    return FALSE;
  }
  
  value->d = d;
  
  pmath_unref(obj);
  return TRUE;
}

// obj will be freed
static pmath_bool_t float_to_java(pmath_t obj, jvalue *value) {
  jvalue tmp;
  if(double_to_java(obj, &tmp)) {
    value->f = (jfloat)tmp.d;
    return TRUE;
  }
  
  return FALSE;
}

// obj will be freed; type wont be freed
pmath_bool_t pj_value_to_java(JNIEnv *env, pmath_t obj, pmath_t type, jvalue *value) {
  if(!env || !value) {
    pmath_unref(obj);
    return FALSE;
  }
  
  memset(value, 0, sizeof(jvalue));
  
  if(pmath_is_symbol(type)) {
    if(pmath_same(type, PJ_SYMBOL_TYPE_BOOLEAN))
      return bool_to_java(obj, value);
      
    if(pmath_same(type, PJ_SYMBOL_TYPE_BYTE))
      return byte_to_java(obj, value);
      
    if(pmath_same(type, PJ_SYMBOL_TYPE_CHAR))
      return char_to_java(obj, value);
      
    if(pmath_same(type, PJ_SYMBOL_TYPE_SHORT))
      return short_to_java(obj, value);
      
    if(pmath_same(type, PJ_SYMBOL_TYPE_INT))
      return int_to_java(obj, value);
      
    if(pmath_same(type, PJ_SYMBOL_TYPE_LONG))
      return long_to_java(obj, value);
      
    if(pmath_same(type, PJ_SYMBOL_TYPE_FLOAT))
      return float_to_java(obj, value);
      
    if(pmath_same(type, PJ_SYMBOL_TYPE_DOUBLE))
      return double_to_java(obj, value);
  }
  
  if(pmath_is_expr_of_len(type, PJ_SYMBOL_TYPE_ARRAY, 1)) {
    if(pmath_is_null(obj)) {
      value->l = NULL;
      return TRUE;
    }
    
    if( pmath_is_packed_array(obj) &&
        pmath_packed_array_get_dimensions(obj) == 1)
    {
      pmath_t elem_type = pmath_expr_get_item(type, 1);
      jsize len = (jsize)pmath_expr_length(obj);
      
      switch(pmath_packed_array_get_element_type(obj)) {
        case PMATH_PACKED_DOUBLE:
          if(pmath_same(elem_type, PJ_SYMBOL_TYPE_DOUBLE)) {
            const double *data = pmath_packed_array_read(obj, NULL, 0);
            jdoubleArray arr = (*env)->NewDoubleArray(env, len);
            
            if(!arr) {
              pmath_unref(obj);
              return FALSE;
            }
            
            (*env)->SetDoubleArrayRegion(env, arr, 0, len, data);
            value->l = arr;
            
            pmath_unref(obj);
          }
          return TRUE;
          
        case PMATH_PACKED_INT32:
          if(pmath_same(elem_type, PJ_SYMBOL_TYPE_INT)) {
            const jint *data = pmath_packed_array_read(obj, NULL, 0);
            jintArray arr = (*env)->NewIntArray(env, len);
            
            assert(sizeof(int32_t) == sizeof(jint));
            
            if(!arr) {
              pmath_unref(obj);
              return FALSE;
            }
            
            (*env)->SetIntArrayRegion(env, arr, 0, len, data);
            value->l = arr;
            
            pmath_unref(obj);
          }
          return TRUE;
      }
    }
    
    if(pmath_is_expr_of(obj, PMATH_SYMBOL_LIST)) {
      pmath_t elem_type = pmath_expr_get_item(type, 1);
      jsize len = (jsize)pmath_expr_length(obj);
      jobject arr = NULL;
      size_t item_size = 0;
      pmath_bool_t (*simple_converter)(pmath_t, jvalue*) = NULL;
      
      if(pmath_same(elem_type, PJ_SYMBOL_TYPE_BOOLEAN)) {
        item_size = sizeof(jboolean);
        simple_converter = bool_to_java;
        arr = (*env)->NewBooleanArray(env, len);
      }
      else if(pmath_same(elem_type, PJ_SYMBOL_TYPE_BYTE)) {
        item_size = sizeof(jbyte);
        simple_converter = byte_to_java;
        arr = (*env)->NewByteArray(env, len);
      }
      else if(pmath_same(elem_type, PJ_SYMBOL_TYPE_CHAR)) {
        item_size = sizeof(jchar);
        simple_converter = char_to_java;
        arr = (*env)->NewCharArray(env, len);
      }
      else if(pmath_same(elem_type, PJ_SYMBOL_TYPE_SHORT)) {
        item_size = sizeof(jshort);
        simple_converter = short_to_java;
        arr = (*env)->NewShortArray(env, len);
      }
      else if(pmath_same(elem_type, PJ_SYMBOL_TYPE_INT)) {
        item_size = sizeof(jint);
        simple_converter = int_to_java;
        arr = (*env)->NewIntArray(env, len);
      }
      else if(pmath_same(elem_type, PJ_SYMBOL_TYPE_LONG)) {
        item_size = sizeof(jlong);
        simple_converter = long_to_java;
        arr = (*env)->NewLongArray(env, len);
      }
      else if(pmath_same(elem_type, PJ_SYMBOL_TYPE_FLOAT)) {
        item_size = sizeof(jfloat);
        simple_converter = float_to_java;
        arr = (*env)->NewFloatArray(env, len);
      }
      else if(pmath_same(elem_type, PJ_SYMBOL_TYPE_DOUBLE)) {
        item_size = sizeof(jdouble);
        simple_converter = double_to_java;
        arr = (*env)->NewDoubleArray(env, len);
      }
      else if((*env)->EnsureLocalCapacity(env, 2) == 0) {
        jclass item_class = pj_class_to_java(env, pmath_ref(elem_type));
        
        if(item_class) {
          arr = (*env)->NewObjectArray(env, len, item_class, NULL);
          
          (*env)->DeleteLocalRef(env, item_class);
        }
      }
      
      if(arr) {
        pmath_bool_t success = TRUE;
        value->l = arr;
        
        if(item_size == 0) {
          jsize i;
          
          assert(simple_converter == NULL);
          
          for(i = 0; i < len; ++i) {
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
        else {
          uint8_t *data = (*env)->GetPrimitiveArrayCritical(env, arr, NULL);
          
          assert(simple_converter != NULL);
          
          if(data) {
            jsize i;
            for(i = 0; i < len; ++i) {
              pmath_t item = pmath_expr_get_item(obj, (size_t)i + 1);
              
              if(!simple_converter(item, (jvalue*)(data + item_size * (size_t)i))) {
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
  
  if((*env)->EnsureLocalCapacity(env, 3) == 0) {
    pmath_bool_t success = FALSE;
    jclass dst_class = pj_class_to_java(env, pmath_ref(type));
    
    if(dst_class) {
      jobject val = NULL;
      
      if(pmath_is_int32(obj)) {
        jclass src_class = (*env)->FindClass(env, "java/lang/Integer");
        
        if(src_class) {
          jmethodID cid = (*env)->GetMethodID(env, src_class, "<init>", "(I)V");
          
          if(cid) {
            val = (*env)->NewObject(env, src_class, cid, (jint)PMATH_AS_INT32(obj));
          }
          
          (*env)->DeleteLocalRef(env, src_class);
        }
      }
      else if(pmath_is_double(obj)) {
        jclass src_class = (*env)->FindClass(env, "java/lang/Double");
        
        if(src_class) {
          jmethodID cid = (*env)->GetMethodID(env, src_class, "<init>", "(D)V");
          
          if(cid) {
            val = (*env)->NewObject(env, src_class, cid, (jdouble)PMATH_AS_DOUBLE(obj));
          }
          
          (*env)->DeleteLocalRef(env, src_class);
        }
      }
      else if(pmath_is_string(obj)) {
        val = (*env)->NewString(env, pmath_string_buffer(&obj), pmath_string_length(obj));
        pmath_unref(obj);
        obj = PMATH_NULL;
      }
      else if(pmath_same(obj, PMATH_SYMBOL_TRUE) || pmath_same(obj, PMATH_SYMBOL_FALSE)) {
        jclass src_class = (*env)->FindClass(env, "java/lang/Boolean");
        
        if(src_class) {
          jmethodID cid = (*env)->GetMethodID(env, src_class, "<init>", "(Z)V");
          
          if(cid) {
            val = (*env)->NewObject(env, src_class, cid, (jboolean)pmath_same(obj, PMATH_SYMBOL_TRUE));
          }
          
          (*env)->DeleteLocalRef(env, src_class);
        }
      }
      else if(pj_object_is_java(env, obj)) {
        val = pj_object_to_java(env, obj);
        obj = PMATH_NULL;
      }
      
      if(val) {
        jclass src_class = (*env)->GetObjectClass(env, val);
        
        if(src_class) {
          if((*env)->IsAssignableFrom(env, src_class, dst_class)) {
            value->l = val;
            val      = NULL;
            success  = TRUE;
          }
          
          (*env)->DeleteLocalRef(env, src_class);
        }
        
        if(val)
          (*env)->DeleteLocalRef(env, val);
      }
      else if(pmath_is_null(obj)) {
        value->l = NULL;
        success  = TRUE;
      }
      
      (*env)->DeleteLocalRef(env, dst_class);
    }
    
    pmath_unref(obj);
    return success;
  }
  
  pmath_unref(obj);
  return FALSE;
}


static pmath_bool_t starts_with(pmath_string_t str, const char *s) {
  int             len = pmath_string_length(str);
  const uint16_t *buf = pmath_string_buffer(&str);
  
  while(len-- > 0) {
    if(!*s)
      return TRUE;
      
    if(*buf++ != (uint16_t)(unsigned char)*s++)
      return FALSE;
  }
  
  return *s == '\0';
}


pmath_t pj_value_from_java(JNIEnv *env, char type, const jvalue *value) {
  switch(type) {
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
      return PMATH_NULL;
  }
  
  if(!value->l)
    return PMATH_NULL;
    
  if((*env)->EnsureLocalCapacity(env, 2) == 0) {
    jclass  clazz = (*env)->GetObjectClass(env, value->l);
    pmath_t class_name  = pj_class_get_name(env, clazz);
    int slen            = pmath_string_length(class_name);
    const uint16_t *buf = pmath_string_buffer(&class_name);
    
    if(starts_with(class_name, "Ljava/lang/")) {
      if(pmath_string_equals_latin1(class_name, "Ljava/lang/String;")) {
        pmath_unref(class_name);
        
        (*env)->DeleteLocalRef(env, clazz);
        return pj_string_from_java(env, value->l);
      }
      
      if(pmath_string_equals_latin1(class_name, "Ljava/lang/Boolean;")) {
        jmethodID mid = (*env)->GetMethodID(env, clazz, "booleanValue", "()Z");
        
        if(!pj_exception_to_pmath(env) && mid) {
          jboolean val;
          pmath_unref(class_name);
          (*env)->DeleteLocalRef(env, clazz);
          val = (*env)->CallBooleanMethod(env, value->l, mid);
          pj_exception_to_pmath(env);
          return pmath_build_value("b", (int)val);
        }
      }
      
      if(pmath_string_equals_latin1(class_name, "Ljava/lang/Byte;")) {
        jmethodID mid = (*env)->GetMethodID(env, clazz, "byteValue", "()B");
        
        if(!pj_exception_to_pmath(env) && mid) {
          jbyte val;
          pmath_unref(class_name);
          (*env)->DeleteLocalRef(env, clazz);
          val = (*env)->CallByteMethod(env, value->l, mid);
          pj_exception_to_pmath(env);
          return pmath_build_value("i", (int)val);
        }
      }
      
      if(pmath_string_equals_latin1(class_name, "Ljava/lang/Character;")) {
        jmethodID mid = (*env)->GetMethodID(env, clazz, "charValue", "()C");
        
        if(!pj_exception_to_pmath(env) && mid) {
          jchar val;
          pmath_unref(class_name);
          (*env)->DeleteLocalRef(env, clazz);
          val = (*env)->CallCharMethod(env, value->l, mid);
          pj_exception_to_pmath(env);
          return pmath_build_value("i", (int)val);
        }
      }
      
      if(pmath_string_equals_latin1(class_name, "Ljava/lang/Short;")) {
        jmethodID mid = (*env)->GetMethodID(env, clazz, "shortValue", "()S");
        
        if(!pj_exception_to_pmath(env) && mid) {
          jshort val;
          pmath_unref(class_name);
          (*env)->DeleteLocalRef(env, clazz);
          val = (*env)->CallShortMethod(env, value->l, mid);
          pj_exception_to_pmath(env);
          return pmath_build_value("i", (int)val);
        }
      }
      
      if(pmath_string_equals_latin1(class_name, "Ljava/lang/Integer;")) {
        jmethodID mid = (*env)->GetMethodID(env, clazz, "intValue", "()I");
        
        if(!pj_exception_to_pmath(env) && mid) {
          jint val;
          pmath_unref(class_name);
          (*env)->DeleteLocalRef(env, clazz);
          val = (*env)->CallIntMethod(env, value->l, mid);
          pj_exception_to_pmath(env);
          return pmath_build_value("i", (int)val);
        }
      }
      
      if(pmath_string_equals_latin1(class_name, "Ljava/lang/Long;")) {
        jmethodID mid = (*env)->GetMethodID(env, clazz, "longValue", "()J");
        
        if(!pj_exception_to_pmath(env) && mid) {
          jlong val;
          pmath_unref(class_name);
          (*env)->DeleteLocalRef(env, clazz);
          val = (*env)->CallLongMethod(env, value->l, mid);
          pj_exception_to_pmath(env);
          return pmath_build_value("k", (long long)val);
        }
      }
      
      if(pmath_string_equals_latin1(class_name, "Ljava/lang/Float;")) {
        jmethodID mid = (*env)->GetMethodID(env, clazz, "floatValue", "()F");
        
        if(!pj_exception_to_pmath(env) && mid) {
          jfloat val;
          pmath_unref(class_name);
          (*env)->DeleteLocalRef(env, clazz);
          val = (*env)->CallFloatMethod(env, value->l, mid);
          pj_exception_to_pmath(env);
          return pmath_build_value("f", (double)val);
        }
      }
      
      if(pmath_string_equals_latin1(class_name, "Ljava/lang/Double;")) {
        jmethodID mid = (*env)->GetMethodID(env, clazz, "doubleValue", "()D");
        
        if(!pj_exception_to_pmath(env) && mid) {
          jdouble val;
          pmath_unref(class_name);
          (*env)->DeleteLocalRef(env, clazz);
          val = (*env)->CallDoubleMethod(env, value->l, mid);
          pj_exception_to_pmath(env);
          return pmath_build_value("f", val);
        }
      }
    }
    
    (*env)->DeleteLocalRef(env, clazz);
    
    if(slen > 0 && buf[0] == '[') {
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

      if(is_simple_array) {
        jsize len = (*env)->GetArrayLength(env, value->l);
        pmath_t arr = pmath_expr_new(pmath_ref(PMATH_SYMBOL_LIST), (size_t)len);
        size_t item_size = 0;
        
        pj_exception_to_pmath(env);
        switch(buf[1]) {
          case 'Z': item_size = sizeof(value->z); break;
          case 'B': item_size = sizeof(value->b); break;
          case 'S': item_size = sizeof(value->s); break;
          case 'C': item_size = sizeof(value->c); break;
          case 'I': item_size = sizeof(value->i); break;
          case 'J': item_size = sizeof(value->j); break;
          case 'F': item_size = sizeof(value->f); break;
          case 'D': item_size = sizeof(value->d); break;
        }
        
        if(item_size == 0) {
          jsize i;
          jvalue v;
          
          for(i = len; i > 0; --i) {
            v.l = (*env)->GetObjectArrayElement(env, value->l, i - 1);
            pj_exception_to_pmath(env);
            
            arr = pmath_expr_set_item(arr, (size_t)i,
                                      pj_value_from_java(env, (char)buf[1], &v));
                                      
            (*env)->DeleteLocalRef(env, v.l);
          }
        }
        else {
          uint8_t *data = (*env)->GetPrimitiveArrayCritical(env, value->l, NULL);
          
          pj_exception_to_pmath(env);
          if(data) {
            size_t i;
            
            for(i = 0; i < (size_t)len; ++i) {
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
  
  return PMATH_NULL;
}


// args and types wont be freed
pmath_bool_t pj_value_fill_args(JNIEnv *env, pmath_expr_t types, pmath_expr_t args, jvalue *jargs) {
  size_t i;
  size_t len = pmath_expr_length(args);
  
  if(len != pmath_expr_length(types))
    return FALSE;
    
  for(i = 1; i <= len; ++i) {
    pmath_t arg  = pmath_expr_get_item(args, i);
    pmath_t type = pmath_expr_get_item(types, i);
    
    if(!pj_value_to_java(env, arg, type, &jargs[i - 1])) {
      pmath_unref(type);
      return FALSE;
    }
    
    pmath_unref(type);
  }
  
  return TRUE;
}