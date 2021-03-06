#include "pj-values.h"
#include "pj-classes.h"
#include "pj-objects.h"
#include "pjvm.h"

#include <math.h>
#include <limits.h>
#include <string.h>


#ifndef NAN
static uint64_t nan_as_int = ((uint64_t)1 << 63) - 1;
#define NAN (*(double*)&nan_as_int)
#endif


extern pmath_symbol_t pjsym_Java_JavaClass;
extern pmath_symbol_t pjsym_Java_JavaObject;

extern pmath_symbol_t pjsym_Java_Type_Array;
extern pmath_symbol_t pjsym_Java_Type_Boolean;
extern pmath_symbol_t pjsym_Java_Type_Byte;
extern pmath_symbol_t pjsym_Java_Type_Character;
extern pmath_symbol_t pjsym_Java_Type_Double;
extern pmath_symbol_t pjsym_Java_Type_Float;
extern pmath_symbol_t pjsym_Java_Type_Int;
extern pmath_symbol_t pjsym_Java_Type_Long;
extern pmath_symbol_t pjsym_Java_Type_Short;

extern pmath_symbol_t pjsym_System_BaseForm;
extern pmath_symbol_t pjsym_System_DirectedInfinity;
extern pmath_symbol_t pjsym_System_False;
extern pmath_symbol_t pjsym_System_List;
extern pmath_symbol_t pjsym_System_True;
extern pmath_symbol_t pjsym_System_Undefined;

// constants from pmath/util/Expr.java
#define PJ_EXPR_CONVERT_DEFAULT        0
#define PJ_EXPR_CONVERT_AS_JAVAOBJECT  1
#define PJ_EXPR_CONVERT_AS_SYMBOL      2
#define PJ_EXPR_CONVERT_AS_PARSED      3
#define PJ_EXPR_CONVERT_AS_EXPRESSION  4
#define PJ_EXPR_CONVERT_AS_MAGIC       5
#define PJ_EXPR_CONVERT_AS_QUOTIENT    6


PMATH_PRIVATE pmath_string_t pj_string_from_java(JNIEnv *env, jstring jstr) {
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
static pmath_bool_t boolean_to_java(pmath_t obj, jvalue *value) {
  pmath_unref(obj);
  
  if(pmath_same(obj, pjsym_System_True)) {
    value->z = JNI_TRUE;
    return TRUE;
  }
  
  if(pmath_same(obj, pjsym_System_False)) {
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
  else if(pmath_same(obj, pjsym_System_Undefined) || pmath_is_expr_of_len(obj, pjsym_System_DirectedInfinity, 0)) {
    d = NAN;
  }
  else if(pmath_is_expr_of_len(obj, pjsym_System_DirectedInfinity, 1)) {
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

static void write_to_string(
  void           *user,
  const uint16_t *data,
  int             len
) {
  *(pmath_string_t *)user = pmath_string_insert_ucs2(
                              *(pmath_string_t *)user,
                              INT_MAX,
                              data,
                              len);
}

#define GENERATE_MAKE_BOXED(BOX_NAME, PRIM_NAME_STRING, PRIM_JNI_TYPE)                     \
  static jobject make_boxed_ ## BOX_NAME(JNIEnv *env, PRIM_JNI_TYPE value) {               \
    jobject obj = NULL;                                                                    \
    jclass type;                                                                           \
    if(!env || (*env)->EnsureLocalCapacity(env, 2) != 0)                                   \
      return NULL;                                                                         \
    type = (*env)->FindClass(env, "java/lang/" #BOX_NAME);                                 \
    if(type) {                                                                             \
      jmethodID cid = (*env)->GetMethodID(env, type, "<init>", "(" PRIM_NAME_STRING ")V"); \
      if(cid) {                                                                            \
        obj = (*env)->NewObject(env, type, cid, value);                                    \
      }                                                                                    \
      (*env)->DeleteLocalRef(env, type);                                                   \
    }                                                                                      \
    return obj;                                                                            \
  }

GENERATE_MAKE_BOXED(Boolean,   "Z", jboolean)
GENERATE_MAKE_BOXED(Byte,      "B", jbyte)
GENERATE_MAKE_BOXED(Character, "C", jchar)
GENERATE_MAKE_BOXED(Short,     "S", jshort)
GENERATE_MAKE_BOXED(Integer,   "I", jint)
GENERATE_MAKE_BOXED(Long,      "J", jlong)
GENERATE_MAKE_BOXED(Float,     "F", jfloat)
GENERATE_MAKE_BOXED(Double,    "D", jdouble)

static jobject make_BigInteger(JNIEnv *env, pmath_integer_t value) { // value will be freed
  const uint16_t *buf;
  int len;
  pmath_string_t str;
  pmath_t expr;
  jobject result = NULL;
  
  if(!env) {
    pmath_unref(value);
    return NULL;
  }
  
  expr = pmath_expr_new_extended(
           pmath_ref(pjsym_System_BaseForm), 2,
           value,
           PMATH_FROM_INT32(16));
  
  str = PMATH_NULL;
  pmath_write(expr, 0, write_to_string, &str);
  
  pmath_unref(expr); 
  expr = PMATH_NULL;
  value = PMATH_NULL;
  
  len = pmath_string_length(str);
  buf = pmath_string_buffer(&str);
  if(buf) {
    if(len > 5 && buf[0] == '-' && buf[1] == '1' && buf[2] == '6' && buf[3] == '^' && buf[4] == '^') {
      str = pmath_string_part(str, 5, -1);
      str = pmath_string_insert_latin1(str, 0, "-", 1);
    }
    else if(len > 4 && buf[0] == '1' && buf[1] == '6' && buf[2] == '^' && buf[3] == '^') {
      str = pmath_string_part(str, 4, -1);
    }
  }
  
  if((*env)->EnsureLocalCapacity(env, 3) == 0) {
    jstring hex_digits = pj_string_to_java(env, str); str = PMATH_NULL;
    if(hex_digits) {
      jclass java_math_BigInteger = (*env)->FindClass(env, "java/math/BigInteger");
      if(java_math_BigInteger) {
        jmethodID cid = (*env)->GetMethodID(env, java_math_BigInteger, "<init>", "(Ljava/lang/String;I)V"); 
        if(cid) {
          result = (*env)->NewObject(env, java_math_BigInteger, cid, hex_digits, (jint)16);
        }
        (*env)->DeleteLocalRef(env, java_math_BigInteger);
      }
      (*env)->DeleteLocalRef(env, hex_digits);
    }
  }
  
  pmath_unref(str);
  return result;
}

static jobject make_Expr_and_delete_local(JNIEnv *env, jobject object, int convert_options) {
  jobject result = NULL;
  jclass pmath_util_Expr;
  
  if(!env)
    return NULL;
  
  pmath_util_Expr = (*env)->FindClass(env, "pmath/util/Expr");
  if(pmath_util_Expr) {
    jmethodID cid = (*env)->GetMethodID(env, pmath_util_Expr, "<init>", "(Ljava/lang/Object;I)V");
    if(cid) {
      result = (*env)->NewObject(env, pmath_util_Expr, cid, object, (jint)convert_options);
    }
  
    (*env)->DeleteLocalRef(env, pmath_util_Expr);
  }
  
  if(object)
    (*env)->DeleteLocalRef(env, object);
  
  return result;
}

static void throw_java_lang_ClassCastException(JNIEnv *env, jclass destination_type) {
  jclass java_lang_ClassCastException;
  
  java_lang_ClassCastException = (*env)->FindClass(env, "java/lang/ClassCastException");
  if(java_lang_ClassCastException) {
    pmath_string_t str = pj_class_get_nice_name(env, destination_type);
    str = pmath_string_insert_latin1(str, 0, "Cannot cast expression to ", -1);
    pj_exception_throw_new(env, java_lang_ClassCastException, str);
    (*env)->DeleteLocalRef(env, java_lang_ClassCastException);
  }
}

static jobject make_object_from_int32(JNIEnv *env, jclass type, int32_t value) {
  pmath_string_t class_name;
  jobject result = NULL;
  
  if(!env || !type) 
    return NULL;
  
  class_name = pj_class_get_name(env, type);
  if( pmath_string_equals_latin1(class_name, "I") ||
      pmath_string_equals_latin1(class_name, "Ljava/lang/Integer;") ||
      pmath_string_equals_latin1(class_name, "Ljava/lang/Number;") ||
      pmath_string_equals_latin1(class_name, "Ljava/lang/Object;")) 
  {
    result = make_boxed_Integer(env, (jint)value);
  }
  else if(pmath_string_equals_latin1(class_name, "J") ||
      pmath_string_equals_latin1(class_name, "Ljava/lang/Long;")) 
  {
    result = make_boxed_Long(env, (jlong)value);
  }
  else if(pmath_string_equals_latin1(class_name, "S") ||
      pmath_string_equals_latin1(class_name, "Ljava/lang/Short;")) 
  {
    if(INT16_MIN <= value && value <= INT16_MAX)
      result = make_boxed_Short(env, (jshort)value);
  }
  else if(pmath_string_equals_latin1(class_name, "B") ||
      pmath_string_equals_latin1(class_name, "Ljava/lang/Byte;")) 
  {
    if(INT8_MIN <= value && value <= INT8_MAX)
      result = make_boxed_Byte(env, (jbyte)value);
  }
  else if(pmath_string_equals_latin1(class_name, "D") ||
      pmath_string_equals_latin1(class_name, "Ljava/lang/Double;")) 
  {
    result = make_boxed_Double(env, (jdouble)value);
  }
  else if(pmath_string_equals_latin1(class_name, "F") ||
      pmath_string_equals_latin1(class_name, "Ljava/lang/Float;")) 
  {
    result = make_boxed_Float(env, (jfloat)value);
  }
  else if(pmath_string_equals_latin1(class_name, "Ljava/math/BigInteger;")) {
    result = make_BigInteger(env, PMATH_FROM_INT32(value));
  }
  else if(pmath_string_equals_latin1(class_name, "Lpmath/util/Expr;")) {
    result = make_Expr_and_delete_local(env, make_boxed_Integer(env, (jint)value), PJ_EXPR_CONVERT_DEFAULT);
  }
  else if(!pmath_string_equals_latin1(class_name, "V") && 
      !pmath_string_equals_latin1(class_name, "Ljava/lang/Void;"))
  {
    throw_java_lang_ClassCastException(env, type);
  }
  
  pmath_unref(class_name);
  return result;
}

static jobject make_object_from_bigint(JNIEnv *env, jclass type, pmath_integer_t value) { // value will be freed
  pmath_string_t class_name;
  jobject result = NULL;
  
  if(!env || !type) {
    pmath_unref(value);
    return NULL;
  }
  
  class_name = pj_class_get_name(env, type);
  if( pmath_string_equals_latin1(class_name, "J") ||
      pmath_string_equals_latin1(class_name, "Ljava/lang/Long;")) {
    if(pmath_integer_fits_si64(value)) {
      result = make_boxed_Long(env, (jlong)pmath_integer_get_si64(value));
    }
  }
  else if( pmath_string_equals_latin1(class_name, "Ljava/lang/Number;") ||
           pmath_string_equals_latin1(class_name, "Ljava/lang/Object;")) 
  {
    if(pmath_integer_fits_si64(value)) {
      result = make_boxed_Long(env, (jlong)pmath_integer_get_si64(value));
    }
    else {
      result = make_BigInteger(env, value);
      value = PMATH_NULL;
    }
  }
  else if(pmath_string_equals_latin1(class_name, "D") ||
      pmath_string_equals_latin1(class_name, "Ljava/lang/Double;"))
  {
    result = make_boxed_Double(env, (jdouble)pmath_number_get_d(value));
  }
  else if(pmath_string_equals_latin1(class_name, "F") ||
      pmath_string_equals_latin1(class_name, "Ljava/lang/Float;")) 
  {
    result = make_boxed_Float(env, (jfloat)pmath_number_get_d(value));
  }
  else if(pmath_string_equals_latin1(class_name, "Ljava/math/BigInteger;")) {
    result = make_BigInteger(env, value);
    value = PMATH_NULL;
  }
  else if(pmath_string_equals_latin1(class_name, "Lpmath/util/Expr;")) {
    if(pmath_integer_fits_si64(value)) {
      result = make_boxed_Long(env, (jlong)pmath_integer_get_si64(value));
    }
    else {
      result = make_BigInteger(env, value);
      value = PMATH_NULL;
    }
    result = make_Expr_and_delete_local(env, result, PJ_EXPR_CONVERT_DEFAULT);
  }
  else if(!pmath_string_equals_latin1(class_name, "V") && 
      !pmath_string_equals_latin1(class_name, "Ljava/lang/Void;"))
  {
    throw_java_lang_ClassCastException(env, type);
  }
  
  pmath_unref(value);
  pmath_unref(class_name);
  return result;
}

static jobject make_object_from_quotient(JNIEnv *env, jclass type, pmath_quotient_t value) { // value will be freed
  jobject result = NULL;
  jclass pmath_util_Expr;
  
  if(!env || !type) {
    pmath_unref(value);
    return result;
  }
  
  pmath_util_Expr = (*env)->FindClass(env, "pmath/util/Expr");
  if(pmath_util_Expr) {
    jclass java_lang_Object = (*env)->FindClass(env, "java/lang/Object");
    if(java_lang_Object) {
      if( (*env)->IsSameObject(env, type, pmath_util_Expr) ||
          (*env)->IsSameObject(env, type, java_lang_Object))
      {
        pmath_t num_den = pmath_expr_new_extended(
                            pmath_ref(pjsym_System_List), 2,
                            pmath_rational_numerator(value),
                            pmath_rational_denominator(value));
        
        jobject data = pj_value_to_java_object(env, num_den, java_lang_Object);
        if(data) 
          result = make_Expr_and_delete_local(env, data, PJ_EXPR_CONVERT_AS_QUOTIENT);
      }
      
      (*env)->DeleteLocalRef(env, java_lang_Object);
    }
    (*env)->DeleteLocalRef(env, pmath_util_Expr);
  }
  
  
  pmath_unref(value);
  return result;
}

static jobject make_object_from_double(JNIEnv *env, jclass type, double value) {
  pmath_string_t class_name;
  jobject result = NULL;
  
  if(!env || !type) 
    return NULL;
  
  class_name = pj_class_get_name(env, type);
  if( pmath_string_equals_latin1(class_name, "D") ||
      pmath_string_equals_latin1(class_name, "Ljava/lang/Double;") ||
      pmath_string_equals_latin1(class_name, "Ljava/lang/Number;") ||
      pmath_string_equals_latin1(class_name, "Ljava/lang/Object;")) 
  {
    result = make_boxed_Double(env, (jdouble)value);
  }
  else if(pmath_string_equals_latin1(class_name, "F") ||
      pmath_string_equals_latin1(class_name, "Ljava/lang/Float;"))
  {
    result = make_boxed_Float(env, (jfloat)value);
  }
  else if(pmath_string_equals_latin1(class_name, "Lpmath/util/Expr;")) {
    result = make_Expr_and_delete_local(env, make_boxed_Double(env, (jdouble)value), PJ_EXPR_CONVERT_DEFAULT);
  }
  else if(!pmath_string_equals_latin1(class_name, "V") && 
      !pmath_string_equals_latin1(class_name, "Ljava/lang/Void;"))
  {
    throw_java_lang_ClassCastException(env, type);
  }
  
  pmath_unref(class_name);
  return result;
}

static jobject make_object_from_bool(JNIEnv *env, jclass type, pmath_bool_t value) {
  pmath_string_t class_name;
  jobject result = NULL;
  
  if(!env || !type)
    return NULL;
  
  class_name = pj_class_get_name(env, type);
  if( pmath_string_equals_latin1(class_name, "Z") ||
      pmath_string_equals_latin1(class_name, "Ljava/lang/Boolean;") ||
      pmath_string_equals_latin1(class_name, "Ljava/lang/Object;")) 
  {
    result = make_boxed_Boolean(env, (jboolean)value);
  }
  else if(pmath_string_equals_latin1(class_name, "Lpmath/util/Expr;")) {
    result = make_Expr_and_delete_local(env, make_boxed_Boolean(env, (jboolean)value), PJ_EXPR_CONVERT_DEFAULT);
  }
  else if(!pmath_string_equals_latin1(class_name, "V") && 
      !pmath_string_equals_latin1(class_name, "Ljava/lang/Void;")) 
  {
    throw_java_lang_ClassCastException(env, type);
  }
  
  pmath_unref(class_name);
  return result;
}


static jobject make_double_array_from_doubles(JNIEnv *env, const double *src, size_t len) {
  jdoubleArray arr;
  if(len > INT32_MAX)
    return NULL;
  
  arr = (*env)->NewDoubleArray(env, (jsize)len);
  
  PMATH_STATIC_ASSERT(sizeof(jdouble) == sizeof(double));
  
  if(arr) {
    (*env)->SetDoubleArrayRegion(env, arr, 0, (jsize)len, src);
  }
  
  return arr;
}

static jobject make_float_array_from_doubles(JNIEnv *env, const double *src, size_t len) {
  jfloatArray arr;
  if(len > INT32_MAX)
    return NULL;
  
  arr = (*env)->NewFloatArray(env, (jsize)len);
  
  if(arr) {
    jfloat *dst = (*env)->GetPrimitiveArrayCritical(env, arr, NULL);
    if(dst) {
      size_t i;
      for(i = 0; i < len; ++i) {
        dst[i] = (jfloat)src[i];
      }
      
      (*env)->ReleasePrimitiveArrayCritical(env, arr, dst, 0);
    }
  }
  
  return arr;
}

static jobject make_int_array_from_ints(JNIEnv *env, const int32_t *src, size_t len) {
  jintArray arr;
  if(len > INT32_MAX)
    return NULL;
  
  arr = (*env)->NewIntArray(env, (jsize)len);
  
  PMATH_STATIC_ASSERT(sizeof(jint) == sizeof(int32_t));
  
  if(arr) {
    (*env)->SetIntArrayRegion(env, arr, 0, (jsize)len, (const jint*)src);
  }
  
  return arr;
}

static jobject make_long_array_from_ints(JNIEnv *env, const int32_t *src, size_t len) {
  jlongArray arr;
  if(len > INT32_MAX)
    return NULL;
  
  arr = (*env)->NewShortArray(env, (jsize)len);
  
  if(arr) {
    jlong *dst = (*env)->GetPrimitiveArrayCritical(env, arr, NULL);
    if(dst) {
      size_t i;
      for(i = 0; i < len; ++i) {
        dst[i] = src[i];
      }
      
      (*env)->ReleasePrimitiveArrayCritical(env, arr, dst, 0);
    }
  }
  
  return arr;
}

static jobject make_short_array_from_ints(JNIEnv *env, const int32_t *src, size_t len) {
  jshortArray arr;
  if(len > INT32_MAX)
    return NULL;
  
  arr = (*env)->NewShortArray(env, (jsize)len);
  
  if(arr) {
    jshort *dst = (*env)->GetPrimitiveArrayCritical(env, arr, NULL);
    if(dst) {
      size_t i;
      for(i = 0; i < len; ++i) {
        if(src[i] < INT16_MIN || src[i] > INT16_MAX) {
          (*env)->ReleasePrimitiveArrayCritical(env, arr, dst, JNI_ABORT);
          (*env)->DeleteLocalRef(env, arr);
          return NULL;
        }
        dst[i] = (jshort)src[i];
      }
      
      (*env)->ReleasePrimitiveArrayCritical(env, arr, dst, 0);
    }
  }
  
  return arr;
}

static jobject make_byte_array_from_ints(JNIEnv *env, const int32_t *src, size_t len) {
  jbyteArray arr;
  if(len > INT32_MAX)
    return NULL;
  
  arr = (*env)->NewByteArray(env, (jsize)len);
  
  if(arr) {
    jbyte *dst = (*env)->GetPrimitiveArrayCritical(env, arr, NULL);
    if(dst) {
      size_t i;
      for(i = 0; i < len; ++i) {
        if(src[i] < INT8_MIN || src[i] > INT8_MAX) {
          (*env)->ReleasePrimitiveArrayCritical(env, arr, dst, JNI_ABORT);
          (*env)->DeleteLocalRef(env, arr);
          return NULL;
        }
        dst[i] = (jbyte)src[i];
      }
      
      (*env)->ReleasePrimitiveArrayCritical(env, arr, dst, 0);
    }
  }
  
  return arr;
}

static jobject fill_primitive_array_from_elements(
  JNIEnv *env, 
  jarray arr,                                          // will be freed
  size_t elem_size, 
  pmath_bool_t (*simple_converter)(pmath_t, jvalue*),  // frees first argument
  pmath_expr_t  list                                   // won't be freed
) {
  size_t len = pmath_expr_length(list);
  
  assert(elem_size > 0);
  assert(simple_converter != NULL);
  
  if(arr) {
    uint8_t *dst = (*env)->GetPrimitiveArrayCritical(env, arr, NULL);
    if(dst) {
      size_t i;
      for(i = 0; i < len; ++i) {
        pmath_t item = pmath_expr_get_item(list, (size_t)i + 1);
        
        if(!simple_converter(item, (jvalue*)(dst + elem_size * (size_t)i))) {
          (*env)->ReleasePrimitiveArrayCritical(env, arr, dst, JNI_ABORT);
          (*env)->DeleteLocalRef(env, arr);
          return NULL;
        }
      }
      (*env)->ReleasePrimitiveArrayCritical(env, arr, dst, 0);
    }
  }
  
  return arr;
}

#define GENERATE_MAKE_PRIMITIVE_ARRAY_FROM_ELEMENTS(TITLECASE, LOWERCASE) \
  static jobject make_ ## LOWERCASE ## _array_from_elements(JNIEnv *env, pmath_expr_t list) {                 \
    jarray arr;                                                                                               \
    size_t len = pmath_expr_length(list);                                                                     \
    if(len > INT32_MAX)                                                                                       \
      return NULL;                                                                                            \
    arr = (*env)->New ## TITLECASE ## Array(env, (jsize)len);                                                 \
    return fill_primitive_array_from_elements(env, arr, sizeof(j ## LOWERCASE), LOWERCASE ## _to_java, list); \
  }

GENERATE_MAKE_PRIMITIVE_ARRAY_FROM_ELEMENTS( Boolean, boolean )
GENERATE_MAKE_PRIMITIVE_ARRAY_FROM_ELEMENTS( Byte,    byte    )
GENERATE_MAKE_PRIMITIVE_ARRAY_FROM_ELEMENTS( Char,    char    )
GENERATE_MAKE_PRIMITIVE_ARRAY_FROM_ELEMENTS( Double,  double  )
GENERATE_MAKE_PRIMITIVE_ARRAY_FROM_ELEMENTS( Float,   float   )
GENERATE_MAKE_PRIMITIVE_ARRAY_FROM_ELEMENTS( Int,     int     )
GENERATE_MAKE_PRIMITIVE_ARRAY_FROM_ELEMENTS( Long,    long    )
GENERATE_MAKE_PRIMITIVE_ARRAY_FROM_ELEMENTS( Short,   short   )


static jobject make_object_array_from_elements(JNIEnv *env, jclass element_type, pmath_expr_t list) { // list won't be freed
  size_t len = pmath_expr_length(list); 
  size_t i;
  jobjectArray arr;
  if(len > INT32_MAX)
    return NULL;
  arr = (*env)->NewObjectArray(env, (jsize)len, element_type, NULL);
  
  if(!arr)
    return NULL;
  
  for(i = 0; i < len; ++i) {
    pmath_t item = pmath_expr_get_item(list, i + 1);
    if(!pmath_is_null(item)) {
      jobject val = pj_value_to_java_object(env, item, element_type);
      if(!val) {
        (*env)->DeleteLocalRef(env, arr);
        return NULL;
      }
      (*env)->SetObjectArrayElement(env, arr, (jsize)i, val);
      (*env)->DeleteLocalRef(env, val);
    }
  }
  
  return arr;
}

static jobject make_object_from_list(JNIEnv *env, jclass type, pmath_expr_t list) {
  pmath_string_t class_name = pj_class_get_name(env, type);
  const uint16_t *buf = pmath_string_buffer(&class_name);
  int class_name_len = pmath_string_length(class_name);
  jobject result = NULL;
  pmath_bool_t allow_any_object = FALSE;
  pmath_bool_t need_expr = FALSE;
  size_t list_len = pmath_expr_length(list);
  
  if(class_name_len == 0 || buf[0] != '[') {
    if(pmath_string_equals_latin1(class_name, "Ljava/lang/Object;")) {
      allow_any_object = TRUE;
    }
    else if(pmath_string_equals_latin1(class_name, "Lpmath/util/Expr;")) {
      allow_any_object = TRUE;
      need_expr = TRUE;
    }
    else {
      if( !pmath_string_equals_latin1(class_name, "V") && 
          !pmath_string_equals_latin1(class_name, "Ljava/lang/Void;"))
       {
        throw_java_lang_ClassCastException(env, type);
      }
      pmath_unref(list);
      pmath_unref(class_name);
      return NULL;
    }
  }

  if(pmath_is_packed_array(list) && pmath_packed_array_get_dimensions(list) == 1) {
    switch(pmath_packed_array_get_element_type(list)) {
      case PMATH_PACKED_DOUBLE: {
          const double *elements = (const double*)pmath_packed_array_read(list, NULL, 0);
          
          if(allow_any_object || (class_name_len == 2 && buf[0] == '[' && buf[1] == 'D')) {
            result = make_double_array_from_doubles(env, elements, list_len);
          }
          else if(class_name_len == 2 && buf[0] == '[' && buf[1] == 'F') {
            result = make_float_array_from_doubles(env, elements, list_len);
          }
        } break;
        
      case PMATH_PACKED_INT32: {
          const int32_t *elements = (const int32_t*)pmath_packed_array_read(list, NULL, 0);
          
          if(allow_any_object || (class_name_len == 2 && buf[0] == '[' && buf[1] == 'I')) {
            result = make_int_array_from_ints(env, elements, list_len);
          }
          else if(class_name_len == 2 && buf[0] == '[' && buf[1] == 'S') {
            result = make_short_array_from_ints(env, elements, list_len);
          }
          else if(class_name_len == 2 && buf[0] == '[' && buf[1] == 'B') {
            result = make_byte_array_from_ints(env, elements, list_len);
          }
          else if(class_name_len == 2 && buf[0] == '[' && buf[1] == 'J') {
            result = make_long_array_from_ints(env, elements, list_len);
          }
        } break;
    }
  }
  
  if(!result) {
    if(class_name_len >= 2 && buf[0] == '[') {
      switch(buf[1]) {
        case 'Z': result = make_boolean_array_from_elements(env, list); break;
        case 'B': result = make_byte_array_from_elements(env, list); break;
        case 'C': result = make_char_array_from_elements(env, list); break;
        case 'S': result = make_short_array_from_elements(env, list); break;
        case 'I': result = make_int_array_from_elements(env, list); break;
        case 'J': result = make_long_array_from_elements(env, list); break;
        case 'F': result = make_float_array_from_elements(env, list); break;
        case 'D': result = make_double_array_from_elements(env, list); break;
        default: {
            jclass elem_type = pj_class_get_component_type(env, type);
            if(elem_type) {
              result = make_object_array_from_elements(env, elem_type, list);
              (*env)->DeleteLocalRef(env, elem_type);
            }
          } break;
      }
    }
    else { // allow_any_object
      pmath_t first = pmath_expr_get_item(list, 1);
      if(pmath_is_double(first)) {
        result = make_double_array_from_elements(env, list);
      }
      else if(pmath_is_int32(first)) {
        result = make_int_array_from_elements(env, list);
        if(!result)
          result = make_long_array_from_elements(env, list);
      }
      else if(pmath_is_string(first) && pmath_string_length(first) == 1) {
        result = make_char_array_from_elements(env, list);
      }
      else if(pmath_same(first, pjsym_System_True) || pmath_same(first, pjsym_System_False)) {
        result = make_boolean_array_from_elements(env, list);
      }
      
      pmath_unref(first);
    }
    
    if(!result) {
      jclass java_lang_Object = (*env)->FindClass(env, "java/lang/Object");
      if(java_lang_Object) {
        result = make_object_array_from_elements(env, java_lang_Object, list);
        (*env)->DeleteLocalRef(env, java_lang_Object);
      }
    }
  }
  
  if(result && need_expr) {
    result = make_Expr_and_delete_local(env, result, PJ_EXPR_CONVERT_DEFAULT);
  }
  
  pmath_unref(list);
  pmath_unref(class_name);
  return result;
}

static jobject ensure_assignable_or_delete_local(JNIEnv *env, jclass type, jobject obj) {
  if(!env || !type || !obj)
    return obj;
  
  if((*env)->EnsureLocalCapacity(env, 1) == 0) {
    jclass obj_class = (*env)->GetObjectClass(env, obj);
    if(obj_class) {
      if(!(*env)->IsAssignableFrom(env, obj_class, type)) {
        pmath_t type_name = pj_class_get_name(env, type);
        if(pmath_string_equals_latin1(type_name, "Lpmath/util/Expr;")) {
          obj = make_Expr_and_delete_local(env, obj, PJ_EXPR_CONVERT_DEFAULT);
        }
        else {
          if( !pmath_string_equals_latin1(type_name, "V") && 
              !pmath_string_equals_latin1(type_name, "Ljava/lang/Void;"))
          {
            throw_java_lang_ClassCastException(env, type);
          }
          (*env)->DeleteLocalRef(env, obj);
          obj = NULL;
        }
        pmath_unref(type_name);
      }
      (*env)->DeleteLocalRef(env, obj_class);
    }
  }
  return obj;
}

static jobject make_object_from_string(JNIEnv *env, jclass type, pmath_string_t str) {
  jobject result = NULL;
  
  PMATH_STATIC_ASSERT(sizeof(jchar) == sizeof(uint16_t));
  
  if(pmath_string_length(str) == 1) {
    pmath_string_t type_name = pj_class_get_name(env, type);
    if( pmath_string_equals_latin1(type_name, "C") ||
       pmath_string_equals_latin1(type_name, "Ljava/lang/Character;"))
    {
      const uint16_t *buf = pmath_string_buffer(&str);
      result = make_boxed_Character(env, (jchar)*buf);
      pmath_unref(str);
      pmath_unref(type_name);
      return result;
    }
    pmath_unref(type_name);
  }
  
  return ensure_assignable_or_delete_local(env, type, pj_string_to_java(env, str));
}

static jobject make_object_from_expr(JNIEnv *env, jclass type, pmath_expr_t expr) {
  pmath_t item;
  jobject result = NULL;
  size_t len;
  jclass pmath_util_Expr;
  
  if(!env) {
    pmath_unref(expr);
    return NULL;
  }
  
  item = pmath_expr_get_item(expr, 0);
  if(pmath_same(item, pjsym_System_List)) {
    pmath_unref(item);
    return make_object_from_list(env, type, expr);
  }
  
  if(pmath_same(item, pjsym_Java_JavaObject)) {
    if(pj_object_is_java(env, expr)) {
      pmath_unref(item);
      return ensure_assignable_or_delete_local(env, type, pj_object_to_java(env, expr));
    }
  }
  
  if(pmath_same(item, pjsym_Java_JavaClass)) {
    jclass clazz = pj_class_to_java(env, pmath_ref(expr));
    if(clazz) {
      pmath_unref(item);
      pmath_unref(expr);
      return ensure_assignable_or_delete_local(env, type, clazz);
    }
  }
  
  len = pmath_expr_length(expr);
  if(len > INT_MAX/2) {
    pmath_unref(item);
    pmath_unref(expr);
    return NULL;
  }
  
  pmath_util_Expr = (*env)->FindClass(env, "pmath/util/Expr");
  if(pmath_util_Expr) {
    jclass java_lang_Object = (*env)->FindClass(env, "java/lang/Object");
    if(java_lang_Object) {
      if( (*env)->IsSameObject(env, type, pmath_util_Expr) ||
          (*env)->IsSameObject(env, type, java_lang_Object))
      {
        jobject head_and_args = (*env)->NewObjectArray(env, (jsize)len + 1, java_lang_Object, NULL);
        if(head_and_args) {
          size_t i = 0;
          for(;;) {
            jobject elem = pj_value_to_java_object(env, item, java_lang_Object);
            if(elem || pmath_is_null(item)) {
              (*env)->SetObjectArrayElement(env, head_and_args, (jsize)i, elem);
              (*env)->DeleteLocalRef(env, elem);
              item = PMATH_NULL;
            }
            else {
              (*env)->DeleteLocalRef(env, elem);
              (*env)->DeleteLocalRef(env, head_and_args);
              head_and_args = NULL;
              item = PMATH_NULL;
              break;
            }
            
            ++i;
            if(i > len)
              break;
            
            item = pmath_expr_get_item(expr, i);
          }
          
          if(head_and_args) {
            jmethodID cid = (*env)->GetMethodID(env, pmath_util_Expr, "<init>", "(Ljava/lang/Object;I)V");
            if(cid) {
              result = (*env)->NewObject(env, pmath_util_Expr, cid, head_and_args, (jint)PJ_EXPR_CONVERT_AS_EXPRESSION);
            }
          }
          
          (*env)->DeleteLocalRef(env, head_and_args);
        }
      }
      else {
        pmath_string_t type_name = pj_class_get_name(env, type);
        if( !pmath_string_equals_latin1(type_name, "V") && 
            !pmath_string_equals_latin1(type_name, "Ljava/lang/Void;"))
        {
          throw_java_lang_ClassCastException(env, type);
        }
        pmath_unref(type_name);
      }
      
      (*env)->DeleteLocalRef(env, java_lang_Object);
    }
    
    (*env)->DeleteLocalRef(env, pmath_util_Expr);
  }
  
  pmath_unref(item);
  pmath_unref(expr);
  return result;
}

static jobject make_object_from_symbol(JNIEnv *env, jclass type, pmath_symbol_t sym) {
  pmath_string_t str;
  
  if(pmath_same(sym, pjsym_System_True)) {
    pmath_unref(sym);
    return make_object_from_bool(env, type, TRUE);
  }
  
  if(pmath_same(sym, pjsym_System_False)) {
    pmath_unref(sym);
    return make_object_from_bool(env, type, FALSE);
  }
  
  str = pmath_symbol_name(sym);
  pmath_unref(sym);
  return ensure_assignable_or_delete_local(
           env, 
           type, 
           make_Expr_and_delete_local(
             env, 
             pj_string_to_java(env, str),
             PJ_EXPR_CONVERT_AS_SYMBOL));
}

static jobject make_object_from_magic(JNIEnv *env, jclass type, int32_t magic_value) {
  jobject result = NULL;
  jclass pmath_util_Expr;
  
  if(!env) 
    return NULL;
  
  pmath_util_Expr = (*env)->FindClass(env, "pmath/util/Expr");
  if(pmath_util_Expr) {
    if((*env)->IsAssignableFrom(env, pmath_util_Expr, type)) {
      jmethodID cid = (*env)->GetMethodID(env, pmath_util_Expr, "<init>", "(Ljava/lang/Object;I)V");
      if(cid) {
        jobject value = make_boxed_Integer(env, (jint)magic_value);
        
        result = (*env)->NewObject(env, pmath_util_Expr, cid, value, (jint)PJ_EXPR_CONVERT_AS_MAGIC);
        
        (*env)->DeleteLocalRef(env, value);
      }
    }
    
    (*env)->DeleteLocalRef(env, pmath_util_Expr);
  }
  
  return result;
}

static jobject make_object_from_other(JNIEnv *env, jclass type, pmath_t obj) {
  pmath_string_t str;
  
  str = PMATH_NULL;
  pmath_write(
    obj, 
    PMATH_WRITE_OPTIONS_FULLNAME_NONSYSTEM | PMATH_WRITE_OPTIONS_FULLEXPR | PMATH_WRITE_OPTIONS_FULLSTR, 
    write_to_string, 
    &str);
  
  pmath_unref(obj);
  return ensure_assignable_or_delete_local(
           env, 
           type, 
           make_Expr_and_delete_local(
             env, 
             pj_string_to_java(env, str),
             PJ_EXPR_CONVERT_AS_PARSED));
}

PMATH_PRIVATE
jobject pj_value_to_java_object(JNIEnv *env, pmath_t obj, jclass type) { // obj will be freed
  if(pmath_is_int32(obj))
    return make_object_from_int32(env, type, PMATH_AS_INT32(obj));
    
  if(pmath_is_double(obj))
    return make_object_from_double(env, type, PMATH_AS_DOUBLE(obj));
    
  if(pmath_is_null(obj))
    return NULL;
  
  if(pmath_is_integer(obj))
    return make_object_from_bigint(env, type, obj);
  
  if(pmath_is_quotient(obj))
    return make_object_from_quotient(env, type, obj);
  
  if(pmath_is_string(obj)) 
    return make_object_from_string(env, type, obj);
  
  // TODO: handle +/- infinity and Undefined
  if(pmath_is_expr(obj)) 
    return make_object_from_expr(env, type, obj);
    
  if(pmath_is_symbol(obj)) 
    return make_object_from_symbol(env, type, obj);
  
  if(pmath_is_magic(obj))
    return make_object_from_magic(env, type, PMATH_AS_INT32(obj));
  
  return make_object_from_other(env, type, obj);
}

// obj will be freed; type wont be freed
PMATH_PRIVATE
pmath_bool_t pj_value_to_java(JNIEnv *env, pmath_t obj, pmath_t type, jvalue *value) {
  if(!env || !value) {
    pmath_unref(obj);
    return FALSE;
  }
  
  memset(value, 0, sizeof(jvalue));
  
  if(pmath_is_symbol(type)) {
    if(pmath_same(type, pjsym_Java_Type_Boolean))
      return boolean_to_java(obj, value);
      
    if(pmath_same(type, pjsym_Java_Type_Byte))
      return byte_to_java(obj, value);
      
    if(pmath_same(type, pjsym_Java_Type_Character))
      return char_to_java(obj, value);
      
    if(pmath_same(type, pjsym_Java_Type_Short))
      return short_to_java(obj, value);
      
    if(pmath_same(type, pjsym_Java_Type_Int))
      return int_to_java(obj, value);
      
    if(pmath_same(type, pjsym_Java_Type_Long))
      return long_to_java(obj, value);
      
    if(pmath_same(type, pjsym_Java_Type_Float))
      return float_to_java(obj, value);
      
    if(pmath_same(type, pjsym_Java_Type_Double))
      return double_to_java(obj, value);
  }
  
  if((*env)->EnsureLocalCapacity(env, 3) == 0) {
    pmath_bool_t success = FALSE;
    jclass dst_class = pj_class_to_java(env, pmath_ref(type));
    
    if(dst_class) {
      if(pmath_is_null(obj)) {
        value->l = NULL;
        success = TRUE;
      }
      else {
        value->l = pj_value_to_java_object(env, obj, dst_class);
        obj = PMATH_NULL;
        success = value->l != NULL;
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

#define GENERATE_MAKE_VALUE_FROM_JAVA(LOWER_NAME, UPPER_NAME, UPPER_LONG_NAME, J_PRIM_SIG, PMATH_SIG_CAST, PMATH_SIG) \
  static pmath_t make_value_from_java_ ## UPPER_LONG_NAME ## _and_delete_class(            \
      JNIEnv *env,                                                                         \
      jclass  clazz,                                                                       \
      jobject obj                                                                          \
  ) {                                                                                      \
    jmethodID mid = (*env)->GetMethodID(env, clazz, #LOWER_NAME "Value", "()" J_PRIM_SIG); \
    if(!pj_exception_to_pmath(env) && mid) {                                               \
      j ## LOWER_NAME  val;                                                                \
      (*env)->DeleteLocalRef(env, clazz);                                                  \
      val = (*env)->Call ## UPPER_NAME ## Method(env, obj, mid);                           \
      pj_exception_to_pmath(env);                                                          \
      return pmath_build_value(PMATH_SIG, (PMATH_SIG_CAST)val);                            \
    }                                                                                      \
    (*env)->DeleteLocalRef(env, clazz);                                                    \
    return PMATH_NULL;                                                                     \
  }

GENERATE_MAKE_VALUE_FROM_JAVA( boolean, Boolean, Boolean,   "Z", int,       "b")
GENERATE_MAKE_VALUE_FROM_JAVA( byte,    Byte,    Byte,      "B", int,       "i")
GENERATE_MAKE_VALUE_FROM_JAVA( char,    Char,    Character, "C", int,       "c")
GENERATE_MAKE_VALUE_FROM_JAVA( short,   Short,   Short,     "S", int,       "i")
GENERATE_MAKE_VALUE_FROM_JAVA( int,     Int,     Integer,   "I", int,       "i")
GENERATE_MAKE_VALUE_FROM_JAVA( long,    Long,    Long,      "J", long long, "k")
GENERATE_MAKE_VALUE_FROM_JAVA( float,   Float,   Float,     "F", double,    "f")
GENERATE_MAKE_VALUE_FROM_JAVA( double,  Double,  Double,    "D", double,    "f")

static pmath_t make_value_from_java_BigInteger_and_delete_class(JNIEnv *env, jclass clazz, jobject obj) {
  pmath_t result = PMATH_NULL;
  
  jmethodID mid_toString_with_radix = (*env)->GetMethodID(env, clazz, "toString", "(I)Ljava/lang/String;");
  if(!pj_exception_to_pmath(env) && mid_toString_with_radix) {
    jstring s = (*env)->CallObjectMethod(env, obj, mid_toString_with_radix, 16);
    pj_exception_to_pmath(env);
    
    if(s) {
      pmath_string_t  str = pj_string_from_java(env, s);
      int             len = pmath_string_length(str);
      const uint16_t *buf = pmath_string_buffer(&str);
      
      if(buf && len >= 1 && buf[0] == '-')
        str = pmath_string_insert_latin1(str, 1, "16^^", 4);
      else
        str = pmath_string_insert_latin1(str, 0, "16^^", 4);
      
      result = pmath_parse_string(str);
      
      (*env)->DeleteLocalRef(env, s);
    }
  }
  (*env)->DeleteLocalRef(env, clazz);
  return result; 
}

static pmath_t make_value_from_java_Expr_and_delete_class(JNIEnv *env, jclass clazz, jobject obj) {
  pmath_t result = PMATH_NULL;
  
  jfieldID fid_object          = (*env)->GetFieldID(env, clazz, "object",          "Ljava/lang/Object;");
  jfieldID fid_convert_options = (*env)->GetFieldID(env, clazz, "convert_options", "I");
  if(fid_object && fid_convert_options) {
    int convert_options = (*env)->GetIntField(env, obj, fid_convert_options);
    jobject data = (*env)->GetObjectField(env, obj, fid_object);
    
    switch(convert_options) {
      case PJ_EXPR_CONVERT_DEFAULT: {
          jvalue val;
          val.l = data;
          result = pj_value_from_java(env, 'L', &val);
        } break;
      
      case PJ_EXPR_CONVERT_AS_JAVAOBJECT: {
          result = pj_object_from_java(env, data);
        } break;
      
      case PJ_EXPR_CONVERT_AS_SYMBOL: {
          jclass java_lang_String = (*env)->FindClass(env, "java/lang/String");
          if(java_lang_String) {
            if((*env)->IsInstanceOf(env, data, java_lang_String)) {
              result = pmath_symbol_get(pj_string_from_java(env, data), TRUE);
            }
            (*env)->DeleteLocalRef(env, java_lang_String);
          }
        } break;
      
      case PJ_EXPR_CONVERT_AS_PARSED: {
          jclass java_lang_String = (*env)->FindClass(env, "java/lang/String");
          if(java_lang_String) {
            if((*env)->IsInstanceOf(env, data, java_lang_String)) {
              result = pmath_parse_string(pj_string_from_java(env, data));
            }
            (*env)->DeleteLocalRef(env, java_lang_String);
          }
        } break;
      
      case PJ_EXPR_CONVERT_AS_EXPRESSION: {
          jclass array_of_java_lang_Object = (*env)->FindClass(env, "[Ljava/lang/Object;");
          if(array_of_java_lang_Object) {
            if((*env)->IsInstanceOf(env, data, array_of_java_lang_Object)) {
              size_t len_with_head = (size_t)(*env)->GetArrayLength(env, data);
              if(len_with_head >= 1) {
                size_t i;
                jvalue val;
                
                result = pmath_expr_new(PMATH_NULL, len_with_head - 1);
                for(i = 0; i < len_with_head; ++i) {
                  val.l = (*env)->GetObjectArrayElement(env, data, (jsize)i);
                  result = pmath_expr_set_item(result, i, pj_value_from_java(env, 'L', &val));
                  (*env)->DeleteLocalRef(env, val.l);
                }
              }
            }
            (*env)->DeleteLocalRef(env, array_of_java_lang_Object);
          }
        } break;
      
      case PJ_EXPR_CONVERT_AS_MAGIC: {
          jvalue val;
          val.l = data;
          result = pj_value_from_java(env, 'L', &val);
          if(pmath_is_int32(result)) {
            result = PMATH_FROM_TAG(PMATH_TAG_MAGIC, PMATH_AS_INT32(result));
          }
          else {
            pmath_unref(result);
            result = PMATH_NULL;
          }
        } break;
      
      case PJ_EXPR_CONVERT_AS_QUOTIENT: {
          jvalue val;
          pmath_t num_den;
          val.l = data;
          num_den = pj_value_from_java(env, 'L', &val);
          if(pmath_is_expr_of_len(num_den, pjsym_System_List, 2)) {
            pmath_t num = pmath_expr_get_item(num_den, 1);
            pmath_t den = pmath_expr_get_item(num_den, 2);
            
            if(pmath_is_integer(num) && pmath_is_integer(den)) {
              result = pmath_rational_new(num, den);
            }
            else {
              pmath_unref(num);
              pmath_unref(den);
            }
          }
          pmath_unref(num_den);
        } break;
      
      default:
        pmath_debug_print("[unsupported pmath.util.Expr.convert_options %d]\n", convert_options);
        result = pj_object_from_java(env, data);
        break;
    }
    
    if(data)
      (*env)->DeleteLocalRef(env, data);
  }
  
  (*env)->DeleteLocalRef(env, clazz);
  return result; 
}

PMATH_PRIVATE
pmath_t pj_value_from_java(JNIEnv *env, char type, const jvalue *value) {
  switch(type) {
    case 'Z':  return pmath_build_value("b", (int)value->z);
    case 'B':  return pmath_build_value("i", (int)value->b);
    case 'C':  return pmath_build_value("c", (int)value->c);
    case 'S':  return pmath_build_value("i", (int)value->s);
    case 'I':  return pmath_build_value("i", (int)value->i);
    case 'J':  return pmath_build_value("k", (long long)value->j);
    case 'F':  return pmath_build_value("f", (double)value->f);
    case 'D':  return pmath_build_value("f", (double)value->d);
    
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
      else if(pmath_string_equals_latin1(class_name, "Ljava/lang/Boolean;")) {
        pmath_unref(class_name);
        return make_value_from_java_Boolean_and_delete_class(env, clazz, value->l);
      }
      else if(pmath_string_equals_latin1(class_name, "Ljava/lang/Byte;")) {
        pmath_unref(class_name);
        return make_value_from_java_Byte_and_delete_class(env, clazz, value->l);
      }
      else if(pmath_string_equals_latin1(class_name, "Ljava/lang/Character;")) {
        pmath_unref(class_name);
        return make_value_from_java_Character_and_delete_class(env, clazz, value->l);
      }
      else if(pmath_string_equals_latin1(class_name, "Ljava/lang/Short;")) {
        pmath_unref(class_name);
        return make_value_from_java_Short_and_delete_class(env, clazz, value->l);
      }
      else if(pmath_string_equals_latin1(class_name, "Ljava/lang/Integer;")) {
        pmath_unref(class_name);
        return make_value_from_java_Integer_and_delete_class(env, clazz, value->l);
      }
      else if(pmath_string_equals_latin1(class_name, "Ljava/lang/Long;")) {
        pmath_unref(class_name);
        return make_value_from_java_Long_and_delete_class(env, clazz, value->l);
      }
      else if(pmath_string_equals_latin1(class_name, "Ljava/lang/Float;")) {
        pmath_unref(class_name);
        return make_value_from_java_Float_and_delete_class(env, clazz, value->l);
      }
      else if(pmath_string_equals_latin1(class_name, "Ljava/lang/Double;")) {
        pmath_unref(class_name);
        return make_value_from_java_Double_and_delete_class(env, clazz, value->l);
      }
    }
    else if(pmath_string_equals_latin1(class_name, "Ljava/math/BigInteger;")) {
      pmath_unref(class_name);
      return make_value_from_java_BigInteger_and_delete_class(env, clazz, value->l);
    }
    else if(pmath_string_equals_latin1(class_name, "Lpmath/util/Expr;")) {
      pmath_unref(class_name);
      return make_value_from_java_Expr_and_delete_class(env, clazz, value->l);
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
        pmath_t arr = pmath_expr_new(pmath_ref(pjsym_System_List), (size_t)len);
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
            
            (*env)->ReleasePrimitiveArrayCritical(env, value->l, data, JNI_ABORT);
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
PMATH_PRIVATE
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
