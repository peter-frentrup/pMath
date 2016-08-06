#include <pmath-util/serialize.h>

#include <pmath-core/expressions.h>
#include <pmath-core/intervals.h>
#include <pmath-core/numbers-private.h>
#include <pmath-core/strings-private.h>

#include <pmath-util/debug.h>
#include <pmath-util/files/abstract-file.h>
#include <pmath-util/hashtables-private.h>
#include <pmath-util/io-varint-private.h>
#include <pmath-util/memory.h>


#define TAG_NULL          0
#define TAG_NEWREF        1
#define TAG_REF           2
#define TAG_MAGIC         3
#define TAG_STR8          4
#define TAG_STR16         5
#define TAG_SYMBOL        6
#define TAG_EXPR          7
#define TAG_INT32         8
#define TAG_MPINT         9
#define TAG_QUOT         10
#define TAG_DOUBLE       11
#define TAG_MPFLOAT      12
#define TAG_INTERVAL     13

/* Data format
  Integers are generally stored in variable-length encoding, like BinaryWrite(... ,Integer)

  - PMATH_NULL:         0

  - new reference:      1, refno, object
  - back references:    2, refno

  - Magic objects:      3, value   (value = PMATH_AS_INT32(object))

  - latin 1 strings:    4, length, C,C,...
                                   \_____/ = length * 1 bytes   (C = 8 Bit char)

  - strings:            5, length, C,C,...
                                   \_____/ = L * 2 bytes   (C = 16BitLE char)

  - symbols:            6, name      (name = symbol name as serialized string)

  - expressions:        7, length, o,o,...
                                   \_____/ = length + 1 serialized objects

  - int32:              8, value

  - big integers:       9, size, N,N,...
                                 \_____/ = abs(size) * 1 byte   (N,N,... = value in LE)
                                                                (sign(size) = sign)

   - quotients:         10, num, den         (num/den = serialized numerator/denomonator)

   - machine floats     11, D   (D = ieee 754 double bits, little endian)

   - multi floats       12, precision, exponent, mantissa
                               (precision and exponent are integers)
                               (mantissa = value's serialized integer mantissa including sign)

   - real intervals     13, interval_expr
 */

//{ hashtable_int_obj_class ...

struct int_object_entry_t {
  intptr_t       key;
  pmath_t value;
};

static void int_object_entry_destructor(struct int_object_entry_t *e) {
  pmath_unref(e->value);
  pmath_mem_free(e);
}

static pmath_bool_t int_object_entries_equal(
  struct int_object_entry_t *e1,
  struct int_object_entry_t *e2
) {
  return e1->key == e2->key;
}

static unsigned int int_object_entry_hash(struct int_object_entry_t *e) {
  return _pmath_hash_pointer((void *)e->key);
}

static pmath_bool_t int_object_entry_equals_key(
  struct int_object_entry_t *e,
  intptr_t                   k
) {
  return e->key == k;
}

static const pmath_ht_class_t hashtable_int_obj_class = {
  (pmath_callback_t)                    int_object_entry_destructor,
  (pmath_ht_entry_hash_func_t)        int_object_entry_hash,
  (pmath_ht_entry_equal_func_t)       int_object_entries_equal,
  (pmath_ht_key_hash_func_t)          _pmath_hash_pointer,
  (pmath_ht_entry_equals_key_func_t)  int_object_entry_equals_key
};

//}=============================================================================

static void write_tag(pmath_t file, unsigned tag) {
  _pmath_serialize_raw_integer_ui(file, tag);
}

static void write_ref(pmath_t file, intptr_t refno) {
  _pmath_serialize_raw_integer_si(file, refno);
}

static void write_size(pmath_t file, size_t size) {
  _pmath_serialize_raw_integer_ui(file, size);
}

static void write_prec(pmath_t file, mpfr_prec_t prec) {
  _pmath_serialize_raw_integer_si(file, prec);
}

static void write_exp(pmath_t file, mpfr_exp_t exp) {
  _pmath_serialize_raw_integer_si(file, exp);
}

static void write_si32(pmath_t file, int32_t i) {
  _pmath_serialize_raw_integer_si(file, i);
}

static void write_int(pmath_t file, int i) {
  _pmath_serialize_raw_integer_si(file, i);
}

static void flip_endianness_64(void *ptr) {
  uint8_t *bytes = ptr;
  
  uint8_t tmp;
  tmp = bytes[0]; bytes[0] = bytes[7]; bytes[7] = tmp;
  tmp = bytes[1]; bytes[1] = bytes[6]; bytes[6] = tmp;
  tmp = bytes[2]; bytes[2] = bytes[5]; bytes[5] = tmp;
  tmp = bytes[3]; bytes[3] = bytes[4]; bytes[4] = tmp;
}

static void write_double(pmath_t file, double value) {
  assert(sizeof(double) == 8);
  
  if(PMATH_BYTE_ORDER > 0)
    flip_endianness_64(&value);
    
  pmath_file_write(file, &value, sizeof(double));
}

struct serializer_t {
  pmath_hashtable_t        object_to_int; // entries = struct _pmath_object_int_entry_t
  pmath_t                  file;
  pmath_serialize_error_t  error;
  intptr_t                 next_id;
};

static pmath_bool_t is_latin1(const uint16_t *str, int len) {
  while(len-- > 0) {
    if(*str++ > 255)
      return FALSE;
  }
  return TRUE;
}

// str will be freed
static void serialize_string_object(struct serializer_t *info, pmath_string_t str) {
  // malformed UTF-16 should be allowed. So we cannot use pmath_string_to_utf8() ...
  
  const uint16_t *orig_buf = pmath_string_buffer(&str);
  uint8_t tmp_buf[16];
  int len = pmath_string_length(str);
  int i;
  
  if(is_latin1(orig_buf, len)) {
    write_tag(info->file, TAG_STR8);
    write_int(info->file, len);
    
    for(; len > sizeof(tmp_buf); len -= sizeof(tmp_buf)) {
      for(i = 0; i < sizeof(tmp_buf); ++i) {
        tmp_buf[i] = (uint8_t) * orig_buf++;
      }
      
      pmath_file_write(info->file, tmp_buf, sizeof(tmp_buf));
    }
    
    for(i = 0; i < len; ++i) {
      tmp_buf[i] = (uint8_t) * orig_buf++;
    }
    
    pmath_file_write(info->file, tmp_buf, (size_t)len);
    pmath_unref(str);
    return;
  }
  
  write_tag(info->file, TAG_STR16);
  write_int(info->file, len);
  
  if(PMATH_BYTE_ORDER > 0) {
    const uint8_t *orig_bytes = (void*)orig_buf;
    for(; len > sizeof(tmp_buf) / 2; len -= sizeof(tmp_buf) / 2, orig_bytes += sizeof(tmp_buf) / 2) {
      for(i = 0; i < sizeof(tmp_buf) / 2; ++i) {
        tmp_buf[2 * i]     = orig_bytes[2 * i + 1];
        tmp_buf[2 * i + 1] = orig_bytes[2 * i];
      }
      
      pmath_file_write(info->file, tmp_buf, sizeof(tmp_buf));
    }
    
    for(i = 0; i < len; ++i) {
      tmp_buf[2 * i]     = orig_bytes[2 * i + 1];
      tmp_buf[2 * i + 1] = orig_bytes[2 * i];
    }
    
    pmath_file_write(info->file, tmp_buf, 2 * (size_t)len);
    pmath_unref(str);
    return;
  }
  
  pmath_file_write(info->file, orig_buf, 2 * (size_t)len);
  pmath_unref(str);
}

// object will be freed
static void serialize(struct serializer_t *info, pmath_t object);

// expr will be freed
static void serialize_expr_general(struct serializer_t *info, pmath_expr_t expr) {
  size_t i, length = pmath_expr_length(expr);
  
  write_tag( info->file, TAG_EXPR);
  write_size(info->file, length);
  
  for(i = 0; i <= length; ++i)
    serialize(info, pmath_expr_get_item(expr, i));
    
  pmath_unref(expr);
}

// value will be freed
static void serialize_mp_int(struct serializer_t *info, pmath_mpint_t value) {
  size_t count = (mpz_sizeinbase(PMATH_AS_MPZ(value), 2) + 7) / 8;
  void *data;
  
  if(count > 0x7FFFFFFFU) {
    if(!info->error)
      info->error = PMATH_SERIALIZE_BAD_OBJECT;
    serialize(info, PMATH_UNDEFINED);
    return;
  }
  
  data = pmath_mem_alloc(count);
  if(!data) {
    if(!info->error)
      info->error = PMATH_SERIALIZE_NO_MEMORY;
    serialize(info, PMATH_UNDEFINED);
    return;
  }
  
  write_tag(info->file, TAG_MPINT);
  
  if(mpz_sgn(PMATH_AS_MPZ(value)) < 0)
    write_si32(info->file, -(int32_t)count);
  else
    write_si32(info->file, +(int32_t)count);
    
  mpz_export(
    data,
    NULL,
    -1,
    1,
    0,
    0,
    PMATH_AS_MPZ(value));
    
  pmath_file_write(info->file, data, count);
  
  pmath_mem_free(data);
  pmath_unref(value);
}

static void serialize_mp_float(struct serializer_t *info, pmath_mpfloat_t value) {
  pmath_mpint_t mantissa = _pmath_create_mp_int(0);
  mpfr_prec_t prec;
  mp_exp_t exp;
  
  if(pmath_is_null(mantissa)) {
    if(!info->error)
      info->error = PMATH_SERIALIZE_NO_MEMORY;
    serialize(info, PMATH_UNDEFINED);
    return;
  }
  
  prec = mpfr_get_prec(PMATH_AS_MP_VALUE(value));
  
  if(mpfr_zero_p(PMATH_AS_MP_VALUE(value))) {
    exp = 0;
    mpz_set_ui(PMATH_AS_MPZ(mantissa), 0);
  }
  else {
    exp = mpfr_get_z_exp(
            PMATH_AS_MPZ(mantissa),
            PMATH_AS_MP_VALUE(value));
  }
  
  write_tag( info->file, TAG_MPFLOAT);
  write_prec(info->file, prec);
  write_exp(info->file, exp);
  serialize(info, _pmath_mp_int_normalize(pmath_ref(mantissa)));
  
  pmath_unref(mantissa);
  pmath_unref(value);
}

// object wont be freed
static void write_new_cache_entry(struct serializer_t *info, pmath_t object) {
  struct _pmath_object_int_entry_t *entry;
  intptr_t refno;
  
  entry = pmath_mem_alloc(sizeof(*entry));
  if(!entry) {
    if(!info->error)
      info->error = PMATH_SERIALIZE_NO_MEMORY;
    return;
  }
  
  refno = info->next_id++;
  
  entry->key   = pmath_ref(object);
  entry->value = refno;
  
  entry = pmath_ht_insert(info->object_to_int, entry);
  if(entry) {
    pmath_ht_obj_int_class.entry_destructor(entry);
    if(!info->error)
      info->error = PMATH_SERIALIZE_NO_MEMORY;
    return;
  }
  
  write_tag(info->file, TAG_NEWREF);
  write_ref(info->file, refno);
}

static void serialize(
  struct serializer_t *info,
  pmath_t              object // will be freed
) {
  struct _pmath_object_int_entry_t *entry;
  
  if(pmath_is_magic(object)) {
    write_tag( info->file, TAG_MAGIC);
    write_si32(info->file, PMATH_AS_INT32(object));
    return;
  }
  
  if(pmath_is_null(object)) {
    write_tag(info->file, TAG_NULL);
    return;
  }
  
  entry = pmath_ht_search(info->object_to_int, &object);
  if(entry) {
    write_tag( info->file, TAG_REF);
    write_ref(info->file, entry->value);
    pmath_unref(object);
    return;
  }
  
  if(pmath_is_int32(object)) {
    write_tag( info->file, TAG_INT32);
    write_si32(info->file, PMATH_AS_INT32(object));
    return;
  }
  
  if(pmath_is_double(object)) {
    write_tag( info->file, TAG_DOUBLE);
    write_double(info->file, object.as_double);
    return;
  }
  
  if(pmath_is_ministr(object)) {
    serialize_string_object(info, object);
    return;
  }
  
  if(!pmath_is_pointer(object)) {
    pmath_unref(object);
    if(!info->error)
      info->error = PMATH_SERIALIZE_BAD_OBJECT;
    return;
  }
  
  write_new_cache_entry(info, object);
  
  switch(PMATH_AS_PTR(object)->type_shift) {
    case PMATH_TYPE_SHIFT_BIGSTRING:
      serialize_string_object(info, object);
      return;
      
    case PMATH_TYPE_SHIFT_SYMBOL:
      write_tag(info->file, TAG_SYMBOL);
      serialize(info, pmath_symbol_name(object));
      pmath_unref(object);
      return;
      
    case PMATH_TYPE_SHIFT_MP_INT:
      serialize_mp_int(info, object);
      return;
      
    case PMATH_TYPE_SHIFT_QUOTIENT:
      write_tag(info->file, TAG_QUOT);
      serialize(info, pmath_rational_numerator(object));
      serialize(info, pmath_rational_denominator(object));
      pmath_unref(object);
      return;
      
    case PMATH_TYPE_SHIFT_MP_FLOAT:
      serialize_mp_float(info, object);
      return;
      
    case PMATH_TYPE_SHIFT_INTERVAL:
      write_tag(info->file, TAG_INTERVAL);
      serialize(info, pmath_interval_get_expr(object));
      pmath_unref(object);
      return;
  }
  
  if(pmath_is_expr(object)) {
    serialize_expr_general(info, object);
    return;
  }
  
  if(!info->error)
    info->error = PMATH_SERIALIZE_BAD_OBJECT;
    
  serialize(info, PMATH_UNDEFINED);
  pmath_unref(object);
}

/*============================================================================*/

struct deserializer_t {
  pmath_hashtable_t        int_to_object; // entries = struct int_object_entry_t
  pmath_t                  file;
  pmath_serialize_error_t  error;
};

static pmath_t deserialize(struct deserializer_t *info);

static uint8_t read_tag(struct deserializer_t *info) {
  uintptr_t ui;
  if(!_pmath_deserialize_raw_integer_ui(info->file, &ui) || ui > 255) {
    if(!info->error)
      info->error = PMATH_SERIALIZE_BAD_BYTE;
    return 0xFF;
  }
  
  return (uint8_t)ui;
}

static intptr_t read_ref(struct deserializer_t *info) {
  intptr_t si;
  if( !_pmath_deserialize_raw_integer_si(info->file, &si)) {
    return (SIZE_MAX >> 1);
  }
  
  return si;
}

static size_t read_size(struct deserializer_t *info) {
  uintptr_t ui;
  if(!_pmath_deserialize_raw_integer_ui(info->file, &ui)) {
    if(!info->error)
      info->error = PMATH_SERIALIZE_BAD_BYTE;
    return 0xFFFFFFFFu;
  }
  
  return ui;
}

static int32_t read_si32(struct deserializer_t *info) {
  intptr_t si;
  if( !_pmath_deserialize_raw_integer_si(info->file, &si) ||
      si < -0x7FFFFFFF - 1 || si > 0x7FFFFFFF)
  {
    if(!info->error)
      info->error = PMATH_SERIALIZE_BAD_BYTE;
    return 0x7FFFFFFF;
  }
  
  return (int32_t)si;
}

static int read_int(struct deserializer_t *info) {
  intptr_t si;
  if( !_pmath_deserialize_raw_integer_si(info->file, &si) ||
      si < INT_MIN || si > INT_MAX)
  {
    if(!info->error)
      info->error = PMATH_SERIALIZE_BAD_BYTE;
    return INT_MAX;
  }
  
  return (int)si;
}

static mpfr_prec_t read_prec(struct deserializer_t *info) {
  intptr_t si;
  if( !_pmath_deserialize_raw_integer_si(info->file, &si) ||
      si < MPFR_PREC_MIN || si > MPFR_PREC_MAX)
  {
    if(!info->error)
      info->error = PMATH_SERIALIZE_BAD_BYTE;
    return 0;
  }
  
  return (mpfr_prec_t)si;
}

static mpfr_exp_t read_exp(struct deserializer_t *info) {
  intptr_t si;
  if( !_pmath_deserialize_raw_integer_si(info->file, &si) ||
      si < MPFR_EMIN_DEFAULT || si > MPFR_EMAX_DEFAULT)
  {
    if(!info->error)
      info->error = PMATH_SERIALIZE_BAD_BYTE;
    return 0;
  }
  
  return (mpfr_exp_t)si;
}

static double read_double(struct deserializer_t *info) {
  double value;
  
  assert(sizeof(double) == 8);
  
  if(!pmath_file_read(info->file, &value, sizeof(double), FALSE)) {
    if(!info->error)
      info->error = PMATH_SERIALIZE_EOF;
    return NAN;
  }
  
  if(PMATH_BYTE_ORDER > 0)
    flip_endianness_64(&value);
    
  return value;
}

static pmath_t read_new_ref(struct deserializer_t *info) {
  struct int_object_entry_t *entry;
  entry = pmath_mem_alloc(sizeof(struct int_object_entry_t));
  
  if(entry) {
    pmath_t result;
    
    entry->key = read_ref(info);
    entry->value = deserialize(info);
    
    result = pmath_ref(entry->value);
    
    entry = pmath_ht_insert(info->int_to_object, entry);
    if(entry)
      hashtable_int_obj_class.entry_destructor(entry);
      
    return result;
  }
  
  if(!info->error)
    info->error = PMATH_SERIALIZE_NO_MEMORY;
    
  read_ref(info);
  return deserialize(info);
}

static pmath_t read_back_ref(struct deserializer_t *info) {
  struct int_object_entry_t *entry;
  entry = pmath_ht_search(info->int_to_object, (void *)read_ref(info));
  
  if(entry)
    return pmath_ref(entry->value);
    
  if(!info->error)
    info->error = PMATH_SERIALIZE_BAD_REF;
    
  return PMATH_UNDEFINED;
}

static pmath_string_t read_latin1_string(struct deserializer_t *info) {
  int len = read_int(info);
  struct _pmath_string_t *result;
  
  if(len < 0) {
    if(!info->error)
      info->error = PMATH_SERIALIZE_BAD_BYTE;
      
    return PMATH_UNDEFINED;
  }
  
  result = _pmath_new_string_buffer(len);
  if(!result) {
    if(!info->error)
      info->error = PMATH_SERIALIZE_NO_MEMORY;
      
    return PMATH_UNDEFINED;
  }
  
  if(pmath_file_read(info->file, AFTER_STRING(result), len, FALSE) != (size_t)len) {
    if(!info->error)
      info->error = PMATH_SERIALIZE_EOF;
      
    pmath_unref(PMATH_FROM_PTR(result));
    return PMATH_UNDEFINED;
  }
  
  {
    uint16_t *buf       =            AFTER_STRING(result);
    const uint8_t *data = (uint8_t *)AFTER_STRING(result);
    
    --len;
    for(; len >= 0; --len)
      buf[len] = data[len];
  }
  
  return _pmath_from_buffer(result);
}

static pmath_string_t read_ucs2_string(struct deserializer_t *info) {
  int len = read_int(info);
  struct _pmath_string_t *result;
  
  if(2 * len < 0) {
    if(!info->error)
      info->error = PMATH_SERIALIZE_BAD_BYTE;
      
    return PMATH_UNDEFINED;
  }
  
  result = _pmath_new_string_buffer(len);
  if(!result) {
    if(!info->error)
      info->error = PMATH_SERIALIZE_NO_MEMORY;
      
    return PMATH_UNDEFINED;
  }
  
  if(pmath_file_read(info->file, AFTER_STRING(result), 2 * len, FALSE) != 2 * (size_t)len) {
    if(!info->error)
      info->error = PMATH_SERIALIZE_EOF;
      
    pmath_unref(PMATH_FROM_PTR(result));
    
    return PMATH_UNDEFINED;
  }
  
#if PMATH_BYTE_ORDER > 0
  {
    uint16_t *buf = (uint16_t *)AFTER_STRING(result);
    
    for(; len > 0; --len, ++buf)
      *buf = ((*buf & 0x00FF) << 8) | ((*buf & 0xFF00) >> 8);
  }
#endif
  
  return _pmath_from_buffer(result);
}

static pmath_t read_symbol(struct deserializer_t *info) {
  pmath_string_t name = deserialize(info);
  
  if(pmath_is_string(name))
    return pmath_symbol_find(name, TRUE);
    
  if(!info->error)
    info->error = PMATH_SERIALIZE_BAD_BYTE;
    
  pmath_unref(name);
  return PMATH_UNDEFINED;
}

static pmath_expr_t read_general_expr(struct deserializer_t *info) {
  pmath_expr_t result;
  size_t i;
  size_t len = read_size(info);
  
  result = pmath_expr_new(deserialize(info), len);
  for(i = 0; i < len; ++i) {
    result = pmath_expr_set_item(result, i + 1, deserialize(info));
  }
  
  return result;
}

static pmath_t read_mp_int(struct deserializer_t *info) {
  pmath_mpint_t result;
  int32_t slen = read_si32(info);
  uint32_t ulen = abs(slen);
  uint8_t *data;
  
  data = pmath_mem_alloc(ulen);
  if(!data && ulen) {
    if(!info->error)
      info->error = PMATH_SERIALIZE_NO_MEMORY;
      
    return PMATH_UNDEFINED;
  }
  
  if(pmath_file_read(info->file, data, ulen, FALSE) != ulen) {
    if(!info->error)
      info->error = PMATH_SERIALIZE_EOF;
      
    pmath_mem_free(data);
    return PMATH_UNDEFINED;
  }
  
  result = _pmath_create_mp_int(0);
  if(pmath_is_null(result)) {
    if(!info->error)
      info->error = PMATH_SERIALIZE_EOF;
      
    pmath_mem_free(data);
    return PMATH_UNDEFINED;
  }
  
  mpz_import(
    PMATH_AS_MPZ(result),
    ulen,
    -1,
    1,
    0,
    0,
    data);
    
  pmath_mem_free(data);
  if(slen < 0)
    mpz_neg(PMATH_AS_MPZ(result), PMATH_AS_MPZ(result));
    
  return _pmath_mp_int_normalize(result);
}

static pmath_t read_quotient(struct deserializer_t *info) {
  pmath_integer_t num = deserialize(info);
  pmath_integer_t den = deserialize(info);
  
  if( pmath_is_integer(num) &&
      pmath_is_integer(den) &&
      pmath_number_sign(den) > 0)
  {
    return pmath_rational_new(num, den);
  }
  
  pmath_unref(num);
  pmath_unref(den);
  
  if(!info->error)
    info->error = PMATH_SERIALIZE_BAD_BYTE;
    
  return PMATH_UNDEFINED;
}

static pmath_t read_double_object(struct deserializer_t *info) {
  pmath_t result;
  result.as_double = read_double(info);
  
  if(pmath_is_double(result))
    return result;
    
  if(!info->error)
    info->error = PMATH_SERIALIZE_BAD_BYTE;
  return PMATH_UNDEFINED;
}

static pmath_t read_mp_float(struct deserializer_t *info) {
  pmath_mpfloat_t result;
  pmath_mpint_t mant;
  mpfr_prec_t prec;
  mp_exp_t exp;
  
  prec = read_prec(info);
  if(prec < MPFR_PREC_MIN || prec > PMATH_MP_PREC_MAX) {
    if(!info->error)
      info->error = PMATH_SERIALIZE_BAD_BYTE;
      
    return PMATH_UNDEFINED;
  }
  
  exp = read_exp(info);
  mant = deserialize(info);
  
  if(pmath_is_int32(mant))
    mant = _pmath_create_mp_int(PMATH_AS_INT32(mant));
    
  if(!pmath_is_mpint(mant)) {
    pmath_unref(mant);
    
    if(!info->error)
      info->error = PMATH_SERIALIZE_BAD_BYTE;
      
    return PMATH_UNDEFINED;
  }
  
  result = _pmath_create_mp_float(prec);
  if(pmath_is_null(result)) {
    if(!info->error)
      info->error = PMATH_SERIALIZE_NO_MEMORY;
      
    pmath_unref(mant);
    return PMATH_UNDEFINED;
  }
  
  mpfr_set_z(  PMATH_AS_MP_VALUE(result), PMATH_AS_MPZ(mant), MPFR_RNDN);
  mpfr_mul_2si(PMATH_AS_MP_VALUE(result), PMATH_AS_MP_VALUE(result), exp, MPFR_RNDN);
  pmath_unref(mant);
  return result;
}

static pmath_t deserialize(struct deserializer_t *info) {
  const uint8_t tag = read_tag(info);
  switch(tag) {
    case TAG_NULL:     return PMATH_NULL;
    case TAG_NEWREF:   return read_new_ref(info);
    case TAG_REF:      return read_back_ref(info);
    case TAG_MAGIC:    return PMATH_FROM_TAG(PMATH_TAG_MAGIC, read_si32(info));
    case TAG_STR8:     return read_latin1_string(info);
    case TAG_STR16:    return read_ucs2_string(info);
    case TAG_SYMBOL:   return read_symbol(info);
    case TAG_EXPR:     return read_general_expr(info);
    case TAG_INT32:    return PMATH_FROM_INT32(read_si32(info));
    case TAG_MPINT:    return read_mp_int(info);
    case TAG_QUOT:     return read_quotient(info);
    case TAG_DOUBLE:   return read_double_object(info);
    case TAG_MPFLOAT:  return read_mp_float(info);
    case TAG_INTERVAL: return pmath_interval_from_expr(deserialize(info));
  }
  
  if(!info->error)
    info->error = PMATH_SERIALIZE_BAD_BYTE;
  return PMATH_UNDEFINED;
}

/*============================================================================*/

PMATH_API
pmath_serialize_error_t pmath_serialize(
  pmath_t file,
  pmath_t object,
  int     flags
) {
  struct serializer_t info;
  
  info.object_to_int = pmath_ht_create(&pmath_ht_obj_int_class, 0);
  info.file          = file;
  info.error         = PMATH_SERIALIZE_OK;
  info.next_id       = 0;
  
  serialize(&info, object);
  
  pmath_ht_destroy(info.object_to_int);
  return info.error;
}

PMATH_API
pmath_t pmath_deserialize(
  pmath_t                  file,
  pmath_serialize_error_t *error
) {
  struct deserializer_t info;
  pmath_t result;
  
  info.int_to_object = pmath_ht_create(&hashtable_int_obj_class, 0);
  info.file          = file;
  info.error         = PMATH_SERIALIZE_OK;
  
  result = deserialize(&info);
  
  pmath_ht_destroy(info.int_to_object);
  if(error)
    *error = info.error;
  return result;
}
