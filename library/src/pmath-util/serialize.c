#include <pmath-util/serialize.h>

#include <pmath-core/expressions.h>
#include <pmath-core/numbers-private.h>
#include <pmath-core/packed-arrays.h>
#include <pmath-core/strings.h>

#include <pmath-util/debug.h>
#include <pmath-util/files/abstract-file.h>
#include <pmath-util/hash/hashtables-private.h>
#include <pmath-util/io-varint-private.h>
#include <pmath-util/memory.h>

#include <limits.h>


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
#define TAG_OLD_MPFLOAT  12
#define TAG_MPFLOAT_BALL 13
#define TAG_ARRAY        14

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

   - old multi floats   12, precision, exponent, mantissa
                               (precision and exponent are integers)
                               (mantissa = value's serialized integer mantissa *object* including sign)

   - multi float balls  13, midpoint, radius
                               midpoint and radius are "old multi floats"

   - packed arrays:     14, type, depth, size1, ... sizeN, V,V,...
                                                           \_____/ = size1*..*sizeN many values (double or int32_t: 8 or 4 bytes, little endian)
                                         \______________/ = depth many integers
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

static void write_slong(pmath_t file, slong i) {
  _pmath_serialize_raw_integer_si(file, i);
}

static void write_size(pmath_t file, size_t size) {
  _pmath_serialize_raw_integer_ui(file, size);
}

static void write_si32(pmath_t file, int32_t i) {
  _pmath_serialize_raw_integer_si(file, i);
}

static void write_int(pmath_t file, int i) {
  _pmath_serialize_raw_integer_si(file, i);
}

static void flip_endianness_32(void *ptr) {
  uint8_t *bytes = ptr;
  
  uint8_t tmp;
  tmp = bytes[0]; bytes[0] = bytes[3]; bytes[3] = tmp;
  tmp = bytes[1]; bytes[1] = bytes[2]; bytes[2] = tmp;
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
    
    assert(len <= sizeof(tmp_buf));
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
    pmath_unref(value);
    return;
  }
  
  data = pmath_mem_alloc(count);
  if(!data) {
    if(!info->error)
      info->error = PMATH_SERIALIZE_NO_MEMORY;
    serialize(info, PMATH_UNDEFINED);
    pmath_unref(value);
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

static void serialize_old_mpfloat_from_mant_exp(struct serializer_t *info, slong prec, const fmpz_t mant, const fmpz_t exp) {
  pmath_integer_t mant_obj;
  pmath_integer_t exp_obj;
  
  mant_obj = _pmath_integer_from_fmpz(mant);
  exp_obj = _pmath_integer_from_fmpz(exp);
  
  if(pmath_is_null(mant_obj) || pmath_is_null(exp_obj)) {
    pmath_unref(mant_obj);
    pmath_unref(exp_obj);
    if(!info->error)
      info->error = PMATH_SERIALIZE_NO_MEMORY;
    serialize(info, PMATH_UNDEFINED);
    return;
  }
  
  write_tag( info->file, TAG_OLD_MPFLOAT);
  write_slong(info->file, prec);
  _pmath_serialize_raw_integer(info->file, exp_obj); // frees exp_obj
  serialize(info, mant_obj); // frees mant_obj
}

static void serialize_old_mpfloat_from_arf(struct serializer_t *info, slong prec, const arf_t x) {
  fmpz_t mant;
  fmpz_t exp;
  
  fmpz_init(mant);
  fmpz_init(exp);
  arf_get_fmpz_2exp(mant, exp, x);
  serialize_old_mpfloat_from_mant_exp(info, prec, mant, exp);
  fmpz_clear(exp);
  fmpz_clear(mant);
}

// value will be freed
static void serialize_mp_float(struct serializer_t *info, pmath_mpfloat_t value) {
  slong prec = PMATH_AS_ARB_WORKING_PREC(value);
  if(mag_is_zero(arb_radref(PMATH_AS_ARB(value)))) {
    serialize_old_mpfloat_from_arf(info, prec, arb_midref(PMATH_AS_ARB(value)));
  }
  else {
    arf_t rad;
    arf_init_set_mag_shallow(rad, arb_radref(PMATH_AS_ARB(value)));
    
    write_tag( info->file, TAG_MPFLOAT_BALL);
    serialize_old_mpfloat_from_arf(info, prec, arb_midref(PMATH_AS_ARB(value)));
    serialize_old_mpfloat_from_arf(info, prec, rad);
  }
  
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
    pmath_ht_obj_int_class.entry_destructor(info->object_to_int, entry);
    if(!info->error)
      info->error = PMATH_SERIALIZE_NO_MEMORY;
    return;
  }
  
  write_tag(info->file, TAG_NEWREF);
  write_ref(info->file, refno);
}

static void write_array_data(
  struct serializer_t *info,
  const size_t *sizes,
  const size_t *steps,
  size_t depth,
  size_t non_cont_depth,
  const uint8_t *bytes
) {
  size_t i;
  assert(non_cont_depth < depth);
  
  if(non_cont_depth == 0) {
    size_t num_elems = 1;
    size_t elem_size = steps[depth - 1];
    
    for(i = 0; i < depth; ++i)
      num_elems *= sizes[i];
      
    if(PMATH_BYTE_ORDER > 0) {
      if(elem_size == 8) {
        const uint64_t *elems = (void*)bytes;
        uint64_t tmp;
        
        while(num_elems-- > 0) {
          tmp = *elems++;
          flip_endianness_64(&tmp);
          pmath_file_write(info->file, &tmp, sizeof(tmp));
        }
      }
      else {
        const uint32_t *elems = (void*)bytes;
        uint32_t tmp;
        
        assert(elem_size == 4);
        
        while(num_elems-- > 0) {
          tmp = *elems++;
          flip_endianness_32(&tmp);
          pmath_file_write(info->file, &tmp, sizeof(tmp));
        }
      }
    }
    else {
      pmath_file_write(info->file, bytes, num_elems * elem_size);
    }
    
    return;
  }
  
  for(i = 0; i < sizes[0]; ++i, bytes += steps[0]) {
    write_array_data(
      info,
      sizes + 1,
      steps + 1,
      depth - 1,
      non_cont_depth - 1,
      bytes);
  }
}

// array will be freed
static void serialize_packed_array(struct serializer_t *info, pmath_packed_array_t array) {
  pmath_packed_type_t elem_type;
  size_t i, depth, non_cont;
  const size_t *sizes;
  const size_t *steps;
  const uint8_t *bytes;
  
  elem_type = pmath_packed_array_get_element_type(array);
  depth = pmath_packed_array_get_dimensions(array);
  sizes = pmath_packed_array_get_sizes(array);
  steps = pmath_packed_array_get_steps(array);
  non_cont = pmath_packed_array_get_non_continuous_dimensions(array);
  bytes = pmath_packed_array_read(array, NULL, 0);
  
  write_tag(info->file, TAG_ARRAY);
  write_int(info->file, elem_type);
  write_size(info->file, depth);
  
  for(i = 0; i < depth; ++i)
    write_size(info->file, sizes[i]);
    
  write_array_data(info, sizes, steps, depth, non_cont, bytes);
  
  pmath_unref(array);
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
      
    case PMATH_TYPE_SHIFT_PACKED_ARRAY:
      serialize_packed_array(info, object);
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

static slong read_prec(struct deserializer_t *info) {
  intptr_t si;
  if( !_pmath_deserialize_raw_integer_si(info->file, &si) ||
      si < MPFR_PREC_MIN || si > MPFR_PREC_MAX)
  {
    if(!info->error)
      info->error = PMATH_SERIALIZE_BAD_BYTE;
    return 0;
  }
  
  return si;
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
  pmath_string_t result;
  uint16_t *buf;
  
  if(len < 0) {
    if(!info->error)
      info->error = PMATH_SERIALIZE_BAD_BYTE;
      
    return PMATH_UNDEFINED;
  }
  
  result = pmath_string_new_raw(len);
  if(!pmath_string_begin_write(&result, &buf, NULL)) {
    pmath_unref(result);
    if(!info->error)
      info->error = PMATH_SERIALIZE_NO_MEMORY;
      
    return PMATH_UNDEFINED;
  }
  
  if(pmath_file_read(info->file, buf, len, FALSE) != (size_t)len) {
    if(!info->error)
      info->error = PMATH_SERIALIZE_EOF;
      
    pmath_unref(result);
    return PMATH_UNDEFINED;
  }
  
  {
    const uint8_t *data = (uint8_t *)buf;
    
    --len;
    for(; len >= 0; --len)
      buf[len] = data[len];
  }
  
  pmath_string_end_write(&result, &buf);
  return result;
}

static pmath_string_t read_ucs2_string(struct deserializer_t *info) {
  int len = read_int(info);
  pmath_string_t result;
  uint16_t *buf;
  
  if(2 * len < 0) {
    if(!info->error)
      info->error = PMATH_SERIALIZE_BAD_BYTE;
      
    return PMATH_UNDEFINED;
  }
  
  result = pmath_string_new_raw(len);
  if(!pmath_string_begin_write(&result, &buf, NULL)) {
    pmath_unref(result);
    if(!info->error)
      info->error = PMATH_SERIALIZE_NO_MEMORY;
      
    return PMATH_UNDEFINED;
  }
  
  if(pmath_file_read(info->file, buf, 2 * len, FALSE) != 2 * (size_t)len) {
    if(!info->error)
      info->error = PMATH_SERIALIZE_EOF;
      
    pmath_unref(result);
    return PMATH_UNDEFINED;
  }
  
#if PMATH_BYTE_ORDER > 0
  {
    uint16_t *tmp = buf;
    
    for(; len > 0; --len, ++tmp)
      *tmp = ((*tmp & 0x00FF) << 8) | ((*tmp & 0xFF00) >> 8);
  }
#endif
  
  pmath_string_end_write(&result, &buf);
  return result;
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

static pmath_mpfloat_t read_simple_mp_float(struct deserializer_t *info) {
  pmath_mpfloat_t result;
  pmath_integer_t mant_obj;
  pmath_integer_t exp_obj;
  slong prec;
  fmpz_t mant;
  fmpz_t exp;
  
  prec = read_prec(info);
  if(prec < MPFR_PREC_MIN || prec > PMATH_MP_PREC_MAX) {
    if(!info->error)
      info->error = PMATH_SERIALIZE_BAD_BYTE;
      
    return PMATH_UNDEFINED;
  }
  
  exp_obj = _pmath_deserialize_raw_integer(info->file);
  if(!pmath_is_integer(exp_obj)) {
    pmath_unref(exp_obj);
    
    if(!info->error)
      info->error = PMATH_SERIALIZE_NO_MEMORY;
      
    return PMATH_UNDEFINED;
  }
  
  mant_obj = deserialize(info);
  
  if(!pmath_is_integer(mant_obj)) {
    pmath_unref(mant_obj);
    pmath_unref(exp_obj);
    
    if(!info->error)
      info->error = PMATH_SERIALIZE_BAD_BYTE;
      
    return PMATH_UNDEFINED;
  }
  
  result = _pmath_create_mp_float(prec);
  if(pmath_is_null(result)) {
    if(!info->error)
      info->error = PMATH_SERIALIZE_NO_MEMORY;
      
    pmath_unref(mant_obj);
    pmath_unref(exp_obj);
    return PMATH_UNDEFINED;
  }
  
  fmpz_init(mant);
  fmpz_init(exp);
  
  _pmath_integer_get_fmpz(mant, mant_obj);
  _pmath_integer_get_fmpz(exp, exp_obj);
  
  arf_set_fmpz_2exp(arb_midref(PMATH_AS_ARB(result)), mant, exp);
  mag_zero(arb_radref(PMATH_AS_ARB(result)));
  
  fmpz_clear(exp);
  fmpz_clear(mant);
  
  pmath_unref(exp_obj);
  pmath_unref(mant_obj);
  return result;
}

static pmath_t read_mp_float_ball(struct deserializer_t *info) {
  pmath_t mid = deserialize(info);
  pmath_t rad = deserialize(info);
  
  if( pmath_is_mpfloat(mid) &&
      pmath_is_mpfloat(rad) &&
      mag_is_zero(arb_radref(PMATH_AS_ARB(mid))) &&
      mag_is_zero(arb_radref(PMATH_AS_ARB(rad))) &&
      PMATH_AS_ARB_WORKING_PREC(mid) == PMATH_AS_ARB_WORKING_PREC(rad) &&
      arb_is_nonnegative(PMATH_AS_ARB(rad)))
  {
    pmath_t obj = _pmath_create_mp_float_from_midrad_arb(
                    PMATH_AS_ARB(mid),
                    PMATH_AS_ARB(rad),
                    (slong)PMATH_AS_ARB_WORKING_PREC(mid));
                    
    pmath_unref(mid);
    pmath_unref(rad);
    return obj;
  }
  
  pmath_unref(mid);
  pmath_unref(rad);
  
  if(!info->error)
    info->error = PMATH_SERIALIZE_BAD_BYTE;
    
  return PMATH_UNDEFINED;
}

static pmath_t read_packed_array(struct deserializer_t *info) {
  pmath_packed_type_t elem_type;
  size_t elem_size;
  pmath_packed_array_t array;
  size_t depth;
  size_t *sizes;
  size_t i;
  void *data;
  size_t total_elems;
  
  elem_type = read_int(info);
  elem_size = pmath_packed_element_size(elem_type);
  if(elem_size == 0) {
    if(!info->error)
      info->error = PMATH_SERIALIZE_BAD_BYTE;
    return PMATH_UNDEFINED;
  }
  
  depth = read_size(info);
  
  if(info->error)
    return PMATH_UNDEFINED;
    
  if(depth == 0 || depth > SIZE_MAX / sizeof(size_t)) {
    info->error = PMATH_SERIALIZE_BAD_BYTE;
    return PMATH_UNDEFINED;
  }
  
  sizes = pmath_mem_alloc(depth * sizeof(size_t));
  if(!sizes) {
    info->error = PMATH_SERIALIZE_NO_MEMORY;
    return PMATH_UNDEFINED;
  }
  
  for(i = 0; i < depth; ++i)
    sizes[i] = read_size(info);
    
  if(info->error) {
    pmath_mem_free(sizes);
    return PMATH_UNDEFINED;
  }
  
  array = pmath_packed_array_new(PMATH_NULL, elem_type, depth, sizes, NULL, 0);
  
  
  data = pmath_packed_array_begin_write(&array, NULL, 0);
  if(!data) {
    info->error = PMATH_SERIALIZE_NO_MEMORY;
    pmath_unref(array);
    pmath_mem_free(sizes);
    return PMATH_UNDEFINED;
  }
  
  total_elems = 1;
  for(i = 0; i < depth; ++i)
    total_elems *= sizes[i];
  
  pmath_mem_free(sizes);
  sizes = NULL;
  
  i = pmath_file_read(info->file, data, total_elems * elem_size, FALSE);
  if(i != total_elems * elem_size) {
    info->error = PMATH_SERIALIZE_EOF;
    pmath_unref(array);
    return PMATH_UNDEFINED;
  }
  
  if(PMATH_BYTE_ORDER > 0) {
    if(elem_size == 8) {
      uint64_t *elems = data;
      
      for(i = 0; i < total_elems; ++i)
        flip_endianness_64(elems++);
    }
    else {
      uint32_t *elems = data;
      
      assert(elem_size == 4);
      
      for(i = 0; i < total_elems; ++i)
        flip_endianness_32(elems++);
    }
  }
  
  return array;
}

static pmath_t deserialize(struct deserializer_t *info) {
  const uint8_t tag = read_tag(info);
  switch(tag) {
    case TAG_NULL:         return PMATH_NULL;
    case TAG_NEWREF:       return read_new_ref(info);
    case TAG_REF:          return read_back_ref(info);
    case TAG_MAGIC:        return PMATH_FROM_TAG(PMATH_TAG_MAGIC, read_si32(info));
    case TAG_STR8:         return read_latin1_string(info);
    case TAG_STR16:        return read_ucs2_string(info);
    case TAG_SYMBOL:       return read_symbol(info);
    case TAG_EXPR:         return read_general_expr(info);
    case TAG_INT32:        return PMATH_FROM_INT32(read_si32(info));
    case TAG_MPINT:        return read_mp_int(info);
    case TAG_QUOT:         return read_quotient(info);
    case TAG_DOUBLE:       return read_double_object(info);
    case TAG_OLD_MPFLOAT:  return read_simple_mp_float(info);
    case TAG_MPFLOAT_BALL: return read_mp_float_ball(info);
    case TAG_ARRAY:        return read_packed_array(info);
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
  
  info.object_to_int = pmath_ht_create_ex(&pmath_ht_obj_int_class, 0);
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
