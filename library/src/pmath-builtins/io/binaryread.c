#include <pmath-builtins/io-private.h>

#include <pmath-core/expressions-private.h>
#include <pmath-core/numbers-private.h>
#include <pmath-core/packed-arrays.h>

#include <pmath-language/scanner.h>

#include <pmath-util/concurrency/threads.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/files.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>
#include <pmath-util/option-helpers.h>
#include <pmath-util/serialize.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/arithmetic-private.h>
#include <pmath-builtins/control-private.h>


#define MIN(A, B)  ((A) < (B) ? (A) : (B))


PMATH_PRIVATE int _pmath_get_byte_ordering(pmath_t head, pmath_expr_t options) {
  pmath_t value = pmath_evaluate(pmath_option_value(head, PMATH_SYMBOL_BYTEORDERING, options));

  if(pmath_is_int32(value)) {
    int i = PMATH_AS_INT32(value);

    if(i == 1 || i == -1) {
      pmath_unref(value);
      return (int)i;
    }
  }

  pmath_message(PMATH_NULL, "byteord", 1, value);
  return 0;
}

static pmath_t make_complex(pmath_t re, pmath_t im) {
  pmath_t re_inf;
  pmath_t im_inf;

  if(pmath_same(re, PMATH_SYMBOL_UNDEFINED)) {
    pmath_unref(im);
    return re;
  }

  if(pmath_same(im, PMATH_SYMBOL_UNDEFINED)) {
    pmath_unref(re);
    return im;
  }

  if(pmath_is_number(im) && pmath_is_number(re)) {
    return pmath_expr_new_extended(
             pmath_ref(PMATH_SYMBOL_COMPLEX), 2, re, im);
  }

  re_inf = _pmath_directed_infinity_direction(re);
  im_inf = _pmath_directed_infinity_direction(im);

  pmath_unref(re);
  pmath_unref(im);

  if(pmath_is_null(re_inf))
    re_inf = PMATH_FROM_INT32(0);

  if(pmath_is_null(im_inf))
    im_inf = PMATH_FROM_INT32(0);

  return pmath_expr_new_extended(
           pmath_ref(PMATH_SYMBOL_DIRECTEDINFINITY), 1,
           pmath_expr_new_extended(
             pmath_ref(PMATH_SYMBOL_COMPLEX), 2,
             re_inf,
             im_inf));
}

enum simple_binary_type_t {
  TYPE_NONE,   // size: 0
  TYPE_UINT,   // size: 1, 2, 3, 4, 8, 16
  TYPE_INT,    // size: 1, 2, 3, 4, 8, 16
  TYPE_CHAR,   // size: 1, 2
  TYPE_REAL,   // size: 2, 4, 8, 16
  TYPE_COMPLEX // size: 4, 8, 16, 32
};

static enum simple_binary_type_t as_simple_binary_type(pmath_t name, size_t *size) {
  assert(size != NULL);
  
  if(pmath_is_null(name)) {
    *size = 1;
    return TYPE_UINT;
  }
  
  if(pmath_is_string(name)) {
    if( pmath_string_equals_latin1(name, "Byte") ||
        pmath_string_equals_latin1(name, "UnsignedInteger8"))
    {
      *size = 1;
      return TYPE_UINT;
    }
    
    if(pmath_string_equals_latin1(name, "Integer8")) {
      *size = 1;
      return TYPE_INT;
    }
    else if(pmath_string_equals_latin1(name, "UnsignedInteger16")) {
      *size = 2;
      return TYPE_UINT;
    }
    else if(pmath_string_equals_latin1(name, "Integer16")) {
      *size = 2;
      return TYPE_INT;
    }
    else if(pmath_string_equals_latin1(name, "UnsignedInteger24")) {
      *size = 3;
      return TYPE_UINT;
    }
    else if(pmath_string_equals_latin1(name, "Integer24")) {
      *size = 3;
      return TYPE_INT;
    }
    else if(pmath_string_equals_latin1(name, "UnsignedInteger32")) {
      *size = 4;
      return TYPE_UINT;
    }
    else if(pmath_string_equals_latin1(name, "Integer32")) {
      *size = 4;
      return TYPE_INT;
    }
    else if(pmath_string_equals_latin1(name, "UnsignedInteger64")) {
      *size = 8;
      return TYPE_UINT;
    }
    else if(pmath_string_equals_latin1(name, "Integer64")) {
      *size = 8;
      return TYPE_INT;
    }
    else if(pmath_string_equals_latin1(name, "UnsignedInteger128")) {
      *size = 16;
      return TYPE_UINT;
    }
    else if(pmath_string_equals_latin1(name, "Integer128")) {
      *size = 16;
      return TYPE_INT;
    }
    else if(pmath_string_equals_latin1(name, "Character8")) {
      *size = 1;
      return TYPE_CHAR;
    }
    else if(pmath_string_equals_latin1(name, "Character16")) {
      *size = 2;
      return TYPE_CHAR;
    }
    else if(pmath_string_equals_latin1(name, "Real16")) {
      *size = 2;
      return TYPE_REAL;
    }
    else if(pmath_string_equals_latin1(name, "Real32")) {
      *size = 4;
      return TYPE_REAL;
    }
    else if(pmath_string_equals_latin1(name, "Real64")) {
      *size = 8;
      return TYPE_REAL;
    }
    else if(pmath_string_equals_latin1(name, "Real128")) {
      *size = 16;
      return TYPE_REAL;
    }
    else if(pmath_string_equals_latin1(name, "Complex32")) {
      *size = 4;
      return TYPE_COMPLEX;
    }
    else if(pmath_string_equals_latin1(name, "Complex64")) {
      *size = 8;
      return TYPE_COMPLEX;
    }
    else if(pmath_string_equals_latin1(name, "Complex128")) {
      *size = 16;
      return TYPE_COMPLEX;
    }
    else if(pmath_string_equals_latin1(name, "Complex256")) {
      *size = 32;
      return TYPE_COMPLEX;
    }
  }
  
  *size = 0;
  return TYPE_NONE;
}

static void reverse_byte_order_32(uint8_t *buf) {
  uint8_t tmp0 = buf[0];
  uint8_t tmp1 = buf[1];
  uint8_t tmp2 = buf[2];
  uint8_t tmp3 = buf[3];
  
  buf[0] = tmp3;
  buf[1] = tmp2;
  buf[2] = tmp1;
  buf[3] = tmp0;
}

static pmath_t binary_read_real16(
  uint8_t *buf,
  int      byte_ordering
) { // works only if double is IEEE
  double val;
  
  pmath_bool_t neg;
  uint8_t uexp;
  uint16_t mant;

  if(byte_ordering > 0) {
    neg = (buf[0] & 0x80) != 0;
    uexp = (buf[0] & 0x7C) >> 2;
    mant = (((uint16_t)buf[0] & 0x03) << 8) | (uint16_t)buf[1];
  }
  else {
    neg = (buf[1] & 0x80) != 0;
    uexp = (buf[0] & 0x7C) >> 2;
    mant = (((uint16_t)buf[1] & 0x03) << 8) | (uint16_t)buf[0];
  }

  if(uexp == 0) {
    val = mant * 2e-24;
    if(neg)
      val = -val;

    return PMATH_FROM_DOUBLE(val);
  }
  
  if(uexp == 0x1F) {
    if(mant == 0) {
      if(neg) {
        return pmath_ref(_pmath_object_neg_infinity);
      }
      
      return pmath_ref(_pmath_object_pos_infinity);
    }
    
    return pmath_ref(PMATH_SYMBOL_UNDEFINED);
  }
  
  val = pow(2, (int)uexp - 25);
  mant |= 0x400;
  val *= mant;

  if(neg)
    val = -val;

  return PMATH_FROM_DOUBLE(val);
}

static pmath_t binary_read_real32(
  uint8_t *buf, 
  int      byte_ordering
) { // works only if double and float are IEEE
  union {
    uint8_t buf[4];
    float f;
  } data;
  double val;
  
  if(byte_ordering == PMATH_BYTE_ORDER) {
    data.buf[0] = buf[0];
    data.buf[1] = buf[1];
    data.buf[2] = buf[2];
    data.buf[3] = buf[3];
  }
  else {
    assert(byte_ordering == - PMATH_BYTE_ORDER);
    
    data.buf[3] = buf[0];
    data.buf[2] = buf[1];
    data.buf[1] = buf[2];
    data.buf[0] = buf[3];
  }
  
  val = data.f;
  
  if(val == HUGE_VAL) {
    return pmath_ref(_pmath_object_pos_infinity);
  }
  
  if(val == -HUGE_VAL) {
    return pmath_ref(_pmath_object_neg_infinity);
  }
  
  if(isnan(val)) {
    return pmath_ref(PMATH_SYMBOL_UNDEFINED);
  }
  
  return PMATH_FROM_DOUBLE(val);
}

static pmath_t binary_read_real64(
  uint8_t *buf, 
  int      byte_ordering
) { // works only if double is IEEE
  union {
    uint8_t buf[8];
    double d;
  } data;
  double val;
  
  if(byte_ordering == PMATH_BYTE_ORDER) {
    data.buf[0] = buf[0];
    data.buf[1] = buf[1];
    data.buf[2] = buf[2];
    data.buf[3] = buf[3];
    data.buf[4] = buf[4];
    data.buf[5] = buf[5];
    data.buf[6] = buf[6];
    data.buf[7] = buf[7];
  }
  else {
    assert(byte_ordering == - PMATH_BYTE_ORDER);
    
    data.buf[7] = buf[0];
    data.buf[6] = buf[1];
    data.buf[5] = buf[2];
    data.buf[4] = buf[3];
    data.buf[3] = buf[4];
    data.buf[2] = buf[5];
    data.buf[1] = buf[6];
    data.buf[0] = buf[7];
  }
  
  val = data.d;
  
  if(val == HUGE_VAL) {
    return pmath_ref(_pmath_object_pos_infinity);
  }
  
  if(val == -HUGE_VAL) {
    return pmath_ref(_pmath_object_neg_infinity);
  }
  
  if(isnan(val)) {
    return pmath_ref(PMATH_SYMBOL_UNDEFINED);
  }
  
  return PMATH_FROM_DOUBLE(val);
}

static pmath_t binary_read_real128(
  uint8_t *buf, 
  int      byte_ordering
) {
  pmath_mpfloat_t f  = _pmath_create_mp_float(113);
  pmath_mpint_t mant = pmath_integer_new_data(
                         14, // 112 / 8
                         byte_ordering,
                         1,
                         PMATH_BYTE_ORDER,
                         0,
                         byte_ordering < 0 ? &buf[0] : &buf[2]);

  if(pmath_is_int32(mant))
    mant = _pmath_create_mp_int(PMATH_AS_INT32(mant));

  if(!pmath_is_null(f) && !pmath_is_null(mant)) {
    uint16_t uexp;
    pmath_bool_t neg;

    if(byte_ordering > 0) {
      uexp = ((buf[0] & 0x7F) << 8) | (buf[1]);
    }
    else {
      uexp = ((buf[15] & 0x7F) << 8) | (buf[14]);
    }

    if(byte_ordering > 0) {
      neg = (buf[0] & 0x80) != 0;
    }
    else {
      neg = (buf[15] & 0x80) != 0;
    }

    if(uexp == 0) {
      if(mpz_sgn(PMATH_AS_MPZ(mant)) == 0) {
        mpfr_set_ui(PMATH_AS_MP_VALUE(f), 0, MPFR_RNDN);
      }
      else {
        mpfr_set_ui_2exp(PMATH_AS_MP_VALUE(f), 1, -112, MPFR_RNDU);
        mpfr_mul_z(
          PMATH_AS_MP_VALUE(f),
          PMATH_AS_MP_VALUE(f),
          PMATH_AS_MPZ(mant),
          MPFR_RNDN);
      }

      if(neg) {
        mpfr_neg(PMATH_AS_MP_VALUE(f), PMATH_AS_MP_VALUE(f), MPFR_RNDN);
      }
        
      pmath_unref(mant);
      return f;
    }
    
    if(uexp == 0x7FFF) {
      if(mpz_sgn(PMATH_AS_MPZ(mant)) == 0) {
        pmath_unref(f);
        pmath_unref(mant);

        if(neg) {
          return pmath_ref(_pmath_object_neg_infinity);
        }
        
        return pmath_ref(_pmath_object_pos_infinity);
      }
      
      pmath_unref(f);
      pmath_unref(mant);
      return pmath_ref(PMATH_SYMBOL_UNDEFINED);
    }
    else {
      mpz_setbit(PMATH_AS_MPZ(mant), 112);

      mpfr_set_ui_2exp(PMATH_AS_MP_VALUE(f), 1, ((int)uexp) - 16383 - 112, MPFR_RNDU);
      mpfr_mul_z(
        PMATH_AS_MP_VALUE(f),
        PMATH_AS_MP_VALUE(f),
        PMATH_AS_MPZ(mant),
        MPFR_RNDN);

      if(neg)
        mpfr_neg(PMATH_AS_MP_VALUE(f), PMATH_AS_MP_VALUE(f), MPFR_RNDN);

      pmath_unref(mant);
      return f;
    }
  }
  
  pmath_unref(mant);
  pmath_unref(f);
  return pmath_ref(PMATH_SYMBOL_FAILED);
}

static pmath_t binary_read_real(
  uint8_t *buf, 
  size_t   size, 
  int      byte_ordering
) {
  assert(buf != NULL);
  assert(size > 0);
  assert(byte_ordering == +1 || byte_ordering == -1);
  
  switch(size) {
  case 2: 
    return binary_read_real16(buf, byte_ordering);
    
  case 4: 
    return binary_read_real32(buf, byte_ordering);
    
  case 8: 
    return binary_read_real64(buf, byte_ordering);
    
  case 16: 
    return binary_read_real128(buf, byte_ordering);
  }
  
  assert(0 && "invalid size");
  return pmath_ref(PMATH_SYMBOL_UNDEFINED);
}

static pmath_t binary_read_simple(
  uint8_t                   *buf,
  enum simple_binary_type_t  type,
  size_t                     size, 
  int                        byte_ordering
) {
  assert(buf != NULL);
  assert(size > 0);
  assert(type != TYPE_NONE);
  assert(byte_ordering == +1 || byte_ordering == -1);
  
  if(type == TYPE_INT) {
    if( (byte_ordering < 0 && (int8_t)buf[size - 1] < 0) ||
        (byte_ordering > 0 && (int8_t)buf[0]        < 0))
    {
      size_t i;
      for(i = 0; i < size; ++i) {
        buf[i] = ~buf[i];
      }
    }
    else
      type = TYPE_UINT;
  }

  if(type == TYPE_UINT || type == TYPE_INT) {
    pmath_t value = pmath_integer_new_data(
                      size,
                      byte_ordering,
                      1,
                      PMATH_BYTE_ORDER,
                      0,
                      buf);

    if(type == TYPE_INT) {
      value = _add_nn(value, PMATH_FROM_INT32(1));
      value = pmath_number_neg(value);
    }
    
    return value;
  }

  if(type == TYPE_CHAR) {
    if(size == 2) {
      uint16_t chr;

      if(byte_ordering < 0)
        chr = (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
      else
        chr = (uint16_t)buf[1] | ((uint16_t)buf[0] << 8);

      return pmath_string_insert_ucs2(PMATH_NULL, 0, &chr, 2);
    }
    
    assert(size == 1);
    assert(sizeof(char) == sizeof(uint8_t));
    return pmath_string_insert_latin1(PMATH_NULL, 0, (const char *)buf, 1);
  }

  if(type == TYPE_REAL) {
    return binary_read_real(&buf[0], size, byte_ordering);
  }
  
  if(type == TYPE_COMPLEX) {
    pmath_t re = binary_read_real(&buf[0],        size / 2, byte_ordering);
    pmath_t im = binary_read_real(&buf[size / 2], size / 2, byte_ordering);
    
    return make_complex(re, im);
  }
  
  assert(0 && "unknown type");
  return pmath_ref(PMATH_SYMBOL_FAILED);
}

static pmath_bool_t binary_read(
  pmath_t  file,        // wont be freed
  pmath_t *type_value,
  int      byte_ordering
) {
  if( pmath_same(*type_value, PMATH_SYMBOL_EXPRESSION) ||
      (pmath_is_string(*type_value) &&
       pmath_string_equals_latin1(*type_value, "Expression")))
  {
    pmath_serialize_error_t error;

    pmath_unref(*type_value);
    *type_value = pmath_deserialize(file, &error);
    if(error) {
      // todo: error message
      pmath_unref(*type_value);
      *type_value = pmath_ref(PMATH_SYMBOL_FAILED);
      return FALSE;
    }

    return TRUE;
  }

  if(pmath_is_null(*type_value) || pmath_is_string(*type_value)) {
    if(pmath_string_equals_latin1(*type_value, "TerminatedString")) {
      char buf[256];
      size_t size;

      pmath_unref(*type_value);
      *type_value = PMATH_NULL;

      for(;;) {
        size_t i;

        size = (int)pmath_file_read(file, buf, sizeof(buf), TRUE);
        if(size == 0) {
          pmath_unref(*type_value);
          *type_value = pmath_ref(PMATH_SYMBOL_ENDOFFILE);
          return TRUE;
        }

        for(i = 0; i < size; ++i) {
          if(buf[i] == '\0') {
            *type_value = pmath_string_insert_latin1(
                            *type_value,
                            INT_MAX,
                            buf,
                            (int)i);

            pmath_file_read(file, buf, i + 1, FALSE);
            return TRUE;
          }
        }

        *type_value = pmath_string_insert_latin1(
                        *type_value,
                        INT_MAX,
                        buf,
                        (int)size);

        pmath_file_read(file, buf, size, FALSE);
      }
    }
    else {
      size_t size = 0;
      enum simple_binary_type_t type = as_simple_binary_type(*type_value, &size);
      
      if(size) {
        uint8_t buf[32];
      
        assert(size <= sizeof(buf));
        
        pmath_unref(*type_value);
        if(pmath_file_read(file, buf, size, FALSE) < size) {
          *type_value = pmath_ref(PMATH_SYMBOL_ENDOFFILE);
          return TRUE;
        }
        
        *type_value = binary_read_simple(buf, type, size, byte_ordering);
        return TRUE;
      }
    }
  }
  else if( pmath_is_expr_of(*type_value, PMATH_SYMBOL_LIST) ||
           pmath_is_expr_of(*type_value, PMATH_SYMBOL_HOLDCOMPLETE))
  {
    size_t i;

    for(i = 1; i <= pmath_expr_length(*type_value); ++i) {
      pmath_t item = pmath_expr_get_item(*type_value, i);

      if(!binary_read(file, &item, byte_ordering)) {
        pmath_unref(*type_value);
        *type_value = item;
        return FALSE;
      }

      *type_value = pmath_expr_set_item(*type_value, i, item);
    }

    return TRUE;
  }

  pmath_message(PMATH_NULL, "format", 1, *type_value);
  *type_value = pmath_ref(PMATH_SYMBOL_FAILED);
  return FALSE;
}

PMATH_PRIVATE pmath_t builtin_binaryread(pmath_expr_t expr) {
  /* BinaryRead(file, type)
     BinaryRead(file)        = BinaryRead(file, "Byte")
     
     ByteOrdering :> $ByteOrdering
   */
  pmath_expr_t options;
  pmath_t file, type;
  size_t last_nonoption;
  int byte_ordering = 0;

  if(pmath_expr_length(expr) < 1) {
    pmath_message_argxxx(0, 1, SIZE_MAX);
    return expr;
  }

  type = pmath_expr_get_item(expr, 2);
  if(pmath_is_null(type) || pmath_is_set_of_options(type)) {
    pmath_unref(type);
    type = PMATH_NULL;
    last_nonoption = 1;
  }
  else
    last_nonoption = 2;

  options = pmath_options_extract(expr, last_nonoption);
  if(pmath_is_null(options)) {
    pmath_unref(type);
    return expr;
  }

  byte_ordering = _pmath_get_byte_ordering(PMATH_NULL, options);
  if(!byte_ordering) {
    pmath_unref(expr);
    pmath_unref(type);
    pmath_unref(options);
    return pmath_ref(PMATH_SYMBOL_FAILED);
  }

  file = pmath_expr_get_item(expr, 1);
  if(!_pmath_file_check(file, PMATH_FILE_PROP_READ | PMATH_FILE_PROP_BINARY)) {
    pmath_unref(file);
    pmath_unref(type);
    pmath_unref(expr);
    pmath_unref(options);
    return pmath_ref(PMATH_SYMBOL_FAILED);
  }

  if(byte_ordering == 0)
    byte_ordering = PMATH_BYTE_ORDER;

  pmath_unref(expr);
  pmath_unref(options);

  // locking?
  binary_read(file, &type, byte_ordering);

  pmath_unref(file);
  return type;
}

static pmath_t binary_read_int32_list(
  pmath_t file, // won't be freed
  size_t count, 
  int byte_ordering
) {
  pmath_blob_t blob = PMATH_NULL;
  pmath_bool_t append_eof_symbol = FALSE;
  pmath_packed_array_t array;
  void *buf = NULL;
  
  size_t buf_size = 0;
  size_t length = 0;
  
  while(length < count && pmath_file_status(file) == PMATH_FILE_OK && !pmath_aborting()) {
    pmath_blob_t new_blob;
    size_t read_bytes;
    size_t old_buf_size = buf_size;
    
    length = MIN(count, length + 128);
    if(SIZE_MAX / sizeof(int32_t) < length) {
      break;
    }
    
    buf_size = sizeof(int32_t) * length;
    new_blob = pmath_blob_new(buf_size, FALSE);
    if(PMATH_UNLIKELY(pmath_is_null(new_blob)))
      break;
    
    if(old_buf_size > 0) {
      int32_t *new_buf = pmath_blob_try_write(new_blob);
      memcpy(buf, new_buf, old_buf_size);
      
      pmath_unref(blob);
      
      buf = new_buf;
    }
    else{
      buf = pmath_blob_try_write(new_blob);
    }
    
    blob = new_blob;
    
    read_bytes = pmath_file_read(file, 
                                 (uint8_t*)buf + old_buf_size, 
                                 buf_size - old_buf_size, 
                                 FALSE);
    if(read_bytes < buf_size - old_buf_size) {
      length = (old_buf_size + read_bytes) / sizeof(int32_t);
      append_eof_symbol = (read_bytes % sizeof(int32_t)) != 0;
      break;
    }
  }
  
  if(byte_ordering != PMATH_BYTE_ORDER) {
    size_t i;
    uint8_t *p = buf;
    
    for(i = 0; i < length; ++i, p+= sizeof(int32_t)) {
      reverse_byte_order_32(p);
    }
  }
  
  array = pmath_packed_array_new(blob, PMATH_PACKED_INT32, 1, &length, NULL, 0);
  if(append_eof_symbol) {
    pmath_expr_append(array, 1, pmath_ref(PMATH_SYMBOL_ENDOFFILE));
  }
  
  return array;
}

static pmath_t binary_read_list(
  pmath_t file, // won't be freed
  pmath_t type, // won't be freed
  size_t count, 
  int byte_ordering
) {
  size_t item_size;
  enum simple_binary_type_t simple_type;
  pmath_expr_t result;
  pmath_t item;
  size_t i;
  size_t length;
  
  assert(byte_ordering == +1 || byte_ordering == -1);
  
  if(count == 0 || pmath_file_status(file) != PMATH_FILE_OK) {
    // TODO: check that `type` is valid anyways.
    return pmath_ref(_pmath_object_emptylist);
  }
  
  simple_type = as_simple_binary_type(type, &item_size);
  if(simple_type == TYPE_INT && item_size == sizeof(int32_t)) {
    return binary_read_int32_list(file, count, byte_ordering);
  }
  
  item = pmath_ref(type);
  if(!binary_read(file, &item, byte_ordering)) {
    return item; // $Failed
  }
  
  length = MIN(count, 128);
  result = pmath_expr_new(pmath_ref(PMATH_SYMBOL_LIST), length);
  result = pmath_expr_set_item(result, 1, item);
  
  i = 1;
  while(i < count && pmath_file_status(file) == PMATH_FILE_OK && !pmath_aborting()) {
    if(i == length) {
      length = MIN(count, length + 128);
      result = pmath_expr_resize(result, length);
    }
    ++i;
    
    item = pmath_ref(type);
    binary_read(file, &item, byte_ordering);
    result = pmath_expr_set_item(result, i, item);
  }
  
  result = pmath_expr_resize(result, i);
  return result;
}

PMATH_PRIVATE pmath_t builtin_binaryreadlist(pmath_expr_t expr) {
  /* BinaryReadList(file, type, n)
     BinaryReadList(file, type)    = BinaryReadList(file, type, Infinity)
     BinaryReadList(file)          = BinaryReadList(file, "Byte", Infinity)
     
     ByteOrdering :> $ByteOrdering
   */
  pmath_expr_t options;
  pmath_t file, type;
  size_t last_nonoption;
  int byte_ordering = 0;
  size_t count = SIZE_MAX;

  if(pmath_expr_length(expr) < 1) {
    pmath_message_argxxx(0, 1, 3);
    return expr;
  }

  type = pmath_expr_get_item(expr, 2);
  if(pmath_is_null(type) || pmath_is_set_of_options(type)) {
    pmath_unref(type);
    type = PMATH_NULL;
    last_nonoption = 1;
  }
  else {
    pmath_t n = pmath_expr_get_item(expr, 3);
    
    if(pmath_is_null(n) || pmath_is_set_of_options(n)) {
      last_nonoption = 2;
    }
    else {
      last_nonoption = 3;
      
      if(pmath_is_int32(n) && PMATH_AS_INT32(n) >= 0) {
        count = (size_t)PMATH_AS_INT32(n);
      }
      else if(!pmath_equals(n, _pmath_object_pos_infinity)) {
        pmath_message(PMATH_NULL, "intnm", 2, PMATH_FROM_INT32(3), pmath_ref(expr));
        
        pmath_unref(n);
        pmath_unref(type);
        return expr;
      }
    }
    
    pmath_unref(n);
  }
  
  options = pmath_options_extract(expr, last_nonoption);
  if(pmath_is_null(options)) {
    pmath_unref(type);
    return expr;
  }

  byte_ordering = _pmath_get_byte_ordering(PMATH_NULL, options);
  if(!byte_ordering) {
    pmath_unref(expr);
    pmath_unref(type);
    pmath_unref(options);
    return pmath_ref(PMATH_SYMBOL_FAILED);
  }

  file = pmath_expr_get_item(expr, 1);
  if(pmath_is_string(file)) {
    file = pmath_evaluate(pmath_parse_string_args(
                            "Try(OpenRead(`1`, BinaryFormat->True))", "(o)", file));
  }
  
  if(!_pmath_file_check(file, PMATH_FILE_PROP_READ | PMATH_FILE_PROP_BINARY)) {
    pmath_unref(file);
    pmath_unref(type);
    pmath_unref(expr);
    pmath_unref(options);
    return pmath_ref(PMATH_SYMBOL_FAILED);
  }

  if(byte_ordering == 0)
    byte_ordering = PMATH_BYTE_ORDER;

  pmath_unref(expr);
  pmath_unref(options);

  // locking?
  expr = binary_read_list(file, type, count, byte_ordering);

  pmath_unref(file);
  pmath_unref(type);
  return expr;
}
