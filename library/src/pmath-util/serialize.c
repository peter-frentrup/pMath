#include <pmath-util/serialize.h>

#include <pmath-core/expressions.h>
#include <pmath-core/numbers-private.h>
#include <pmath-core/strings-private.h>

#include <pmath-util/debug.h>
#include <pmath-util/files.h>
#include <pmath-util/hashtables-private.h>
#include <pmath-util/memory.h>


#define TAG_NULL      0
#define TAG_NEWREF    1
#define TAG_REF       2
#define TAG_MAGIC     3
#define TAG_STR8      4
#define TAG_STR16     5
#define TAG_SYMBOL    6
#define TAG_EXPR      7
#define TAG_INT32     8
#define TAG_MPINT     9
#define TAG_QUOT     10
#define TAG_MPFLOAT  11
#define TAG_DOUBLE   12

/* Data format   (LE = little endian)

   - PMATH_NULL:         0     (1 byte)

   - new reference:      1,B,object  (B = 32BitLE number as new reference)
   - back references:    2,B         (B = 32BitLE number to previous reference)

   - Magic objects:      3,M   (5 bytes; M = PMATH_AS_INT32(object) = signed 32BitLE)

   - latin 1 strings:    4,L,C,C,...
                             \_____/ = L * 1 bytes   (L = 32BitLE length)
                                                     (C = 8 Bit char)

   - strings:            5,L,C,C,...
                             \_____/ = L * 2 bytes   (L = 32BitLE length)
                                                     (C = 16BitLE char)

   - symbols:            6,name      (name = symbol name as serialized string)

   - expressions:        7,L,o,o,...
                             \_____/ = L + 1 serialized objects (L = 32BitLE length)

   - int32:              8,I        (I = signed 32BitLE)

   - big integers:       9,L,N,N,...
                             \_____/ = abs(L) * 1 byte   (N,N,... = data in LE)
                                                         (sign(L) = sign)
                                                         (L       = 32BitLE length)

   - quotients:         10,n,d         (n/d = serialized numerator/denomonator)

   - multi floats       11,VP,VE,VM,EE,EM
                               (VP = value's unsigned 32BitLE precision in bits)
                               (VE = value's signed 32BitLE exponent)
                               (VM = value's serialized integer mantissa including sign)
                               (EE = error's signed 32BitLE exponent)
                               (EM = error's serialized integer mantissa)

   - machine floats     12,D   (D = ieee 754 double bits = unsigned 64BitLE)
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

static void write_ui8(pmath_t file, uint8_t i) {
  pmath_file_write(file, &i, 1);
}

static void write_ui16(pmath_t file, uint16_t i) {
#if PMATH_BYTE_ORDER > 0
  uint8_t data[2];
  
  data[0] = i & 0xFF;
  data[1] = (i & 0xFF00) >> 8;
  
  pmath_file_write(file, data, 2);
#else
  pmath_file_write(file, &i, 2);
#endif
}

static void write_ui32(pmath_t file, uint32_t i) {
#if PMATH_BYTE_ORDER > 0
  uint8_t data[4];
  
  data[0] = i & 0xFF;
  data[1] = (i & 0xFF00) >> 8;
  data[2] = (i & 0xFF0000) >> 16;
  data[3] = (i & 0xFF000000U) >> 24;
  
  pmath_file_write(file, data, 4);
#else
  pmath_file_write(file, &i, 4);
#endif
}

static void write_si32(pmath_t file, int32_t i) {
  write_ui32(file, (uint32_t)i);
}

static void write_ui64(pmath_t file, uint64_t i) {
  write_ui32(file, (uint32_t)(i & 0xFFFFFFFFU));
  write_ui32(file, (uint32_t)(i >> 32));
}

struct serializer_t {
  pmath_hashtable_t        object_to_int; // entries = struct _pmath_object_int_entry_t
  pmath_t                  file;
  pmath_serialize_error_t  error;
  intptr_t                 next_id;
};

static void serialize(
  struct serializer_t *info,
  pmath_t              object // will be freed
) {
  struct _pmath_object_int_entry_t *entry;
  
  if(pmath_is_magic(object)) {
    write_ui8( info->file, TAG_MAGIC);
    write_si32(info->file, PMATH_AS_INT32(object));
    return;
  }
  
  if(pmath_is_null(object)) {
    write_ui8(info->file, TAG_NULL);
    return;
  }
  
  entry = pmath_ht_search(info->object_to_int, &object);
  if(entry) {
    write_ui8( info->file, TAG_REF);
    write_ui32(info->file, (uint32_t)entry->value);
    pmath_unref(object);
    return;
  }
  
  if(pmath_is_int32(object)) {
    write_ui8( info->file, TAG_INT32);
    write_si32(info->file, PMATH_AS_INT32(object));
    return;
  }
  
  if(pmath_is_double(object)) {
    write_ui8( info->file, TAG_DOUBLE);
    write_ui64(info->file, object.as_bits);
    return;
  }
  
  if(pmath_is_ministr(object)) {
    if(pmath_is_str0(object)) {
      write_ui8( info->file, TAG_STR8);
      write_ui32(info->file, 0);
      return;
    }
    
    if(pmath_is_str1(object)) {
      if(object.s.u.as_chars[0] <= 0xFF) {
        write_ui8( info->file, TAG_STR8);
        write_ui32(info->file, 1);
        write_ui8(info->file, (uint8_t)object.s.u.as_chars[0]);
        return;
      }
      
      write_ui8( info->file, TAG_STR16);
      write_ui32(info->file, 1);
      write_ui16(info->file, object.s.u.as_chars[0]);
      return;
    }
    
    assert(pmath_is_str2(object));
    
    if( object.s.u.as_chars[0] <= 0xFF &&
        object.s.u.as_chars[1] <= 0xFF)
    {
      write_ui8( info->file, TAG_STR8);
      write_ui32(info->file, 2);
      write_ui8(info->file, (uint8_t)object.s.u.as_chars[0]);
      write_ui8(info->file, (uint8_t)object.s.u.as_chars[1]);
      return;
    }
    
    write_ui8( info->file, TAG_STR16);
    write_ui32(info->file, 2);
    write_ui16(info->file, object.s.u.as_chars[0]);
    write_ui16(info->file, object.s.u.as_chars[1]);
    return;
  }
  
  if(!pmath_is_pointer(object)) {
    pmath_unref(object);
    if(!info->error)
      info->error = PMATH_SERIALIZE_BAD_OBJECT;
    return;
  }
  
  {
    entry = pmath_mem_alloc(sizeof(*entry));
    if(!entry) {
      pmath_unref(object);
      serialize(info, PMATH_UNDEFINED);
      if(!info->error)
        info->error = PMATH_SERIALIZE_NO_MEMORY;
      return;
    }
    
    entry->key   = pmath_ref(object);
    entry->value = info->next_id++;
    write_ui8( info->file, TAG_NEWREF);
    write_ui32(info->file, entry->value);
    
    entry = pmath_ht_insert(info->object_to_int, entry);
    if(entry) {
      pmath_ht_obj_int_class.entry_destructor(entry);
      pmath_unref(object);
      serialize(info, PMATH_UNDEFINED);
      if(!info->error)
        info->error = PMATH_SERIALIZE_NO_MEMORY;
      return;
    }
  }
  
  switch(PMATH_AS_PTR(object)->type_shift) {
    case PMATH_TYPE_SHIFT_BIGSTRING: {
        const uint16_t *buf =    pmath_string_buffer(&object);
        uint32_t len = (uint32_t)pmath_string_length(object);
        unsigned int i;
        
        pmath_bool_t all_latin1 = TRUE;
        for(i = 0; i < len; ++i) {
          if(buf[i] > 0xFF) {
            all_latin1 = FALSE;
            break;
          }
        }
        
        if(all_latin1) {
          write_ui8( info->file, TAG_STR8);
          write_ui32(info->file, len);
          for(; len > 0; ++buf, --len)
            write_ui8(info->file, (uint8_t)*buf);
        }
        else {
          write_ui8( info->file, TAG_STR16);
          write_ui32(info->file, len);
          for(; len > 0; ++buf, --len)
            write_ui16(info->file, *buf);
        }
      } break;
      
    case PMATH_TYPE_SHIFT_SYMBOL: {
        write_ui8(info->file, TAG_SYMBOL);
        serialize(info, pmath_symbol_name(object));
      } break;
      
    case PMATH_TYPE_SHIFT_EXPRESSION_GENERAL:
    case PMATH_TYPE_SHIFT_EXPRESSION_GENERAL_PART: {
        size_t i, length = pmath_expr_length(object);
        
        if(length >= 0xFFFFFFFFU)
          goto DEFAULT;
          
        write_ui8( info->file, TAG_EXPR);
        write_ui32(info->file, (uint32_t)length);
        
        for(i = 0; i <= length; ++i)
          serialize(info, pmath_expr_get_item(object, i));
          
      } break;
      
    case PMATH_TYPE_SHIFT_MP_INT: {
        size_t count = (mpz_sizeinbase(PMATH_AS_MPZ(object), 2) + 7) / 8;
        void *data;
        
        if(count > 0x7FFFFFFFU)
          goto DEFAULT;
          
        data = pmath_mem_alloc(count);
        if(!data) {
          if(!info->error)
            info->error = PMATH_SERIALIZE_NO_MEMORY;
          serialize(info, PMATH_UNDEFINED);
          break;
        }
        
        write_ui8(info->file, TAG_MPINT);
        
        if(mpz_sgn(PMATH_AS_MPZ(object)) < 0)
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
          PMATH_AS_MPZ(object));
          
        pmath_file_write(info->file, data, count);
        
        pmath_mem_free(data);
      } break;
      
    case PMATH_TYPE_SHIFT_QUOTIENT: {
        write_ui8(info->file, TAG_QUOT);
        serialize(info, pmath_rational_numerator(object));
        serialize(info, pmath_rational_denominator(object));
      } break;
      
    case PMATH_TYPE_SHIFT_MP_FLOAT: {
        pmath_mpint_t mantissa = _pmath_create_mp_int(0);
        mpfr_prec_t prec;
        mp_exp_t exp;
        
        if(pmath_is_null(mantissa)) {
          if(!info->error)
            info->error = PMATH_SERIALIZE_NO_MEMORY;
          serialize(info, PMATH_UNDEFINED);
          break;
        }
        
        prec = mpfr_get_prec(PMATH_AS_MP_VALUE(object));
        
        if(mpfr_zero_p(PMATH_AS_MP_VALUE(object))) {
          exp = 0;
          mpz_set_ui(PMATH_AS_MPZ(mantissa), 0);
        }
        else {
          exp = mpfr_get_z_exp(
                  PMATH_AS_MPZ(mantissa),
                  PMATH_AS_MP_VALUE(object));
        }
        
        write_ui8( info->file, TAG_MPFLOAT);
        write_ui32(info->file, (uint32_t)prec);
        write_si32(info->file, (int32_t)exp);
        serialize(info, _pmath_mp_int_normalize(pmath_ref(mantissa)));
        
        if(mpfr_zero_p(PMATH_AS_MP_ERROR(object))) {
          exp = 0;
          mpz_set_ui(PMATH_AS_MPZ(mantissa), 0);
        }
        else {
          exp = mpfr_get_z_exp(
                  PMATH_AS_MPZ(mantissa),
                  PMATH_AS_MP_ERROR(object));
        }
        
        write_si32(info->file, (int32_t)exp);
        serialize(info, _pmath_mp_int_normalize(mantissa));
      } break;
      
  default: DEFAULT:
      if(!info->error)
        info->error = PMATH_SERIALIZE_BAD_OBJECT;
        
      serialize(info, PMATH_UNDEFINED);
  }
  
  pmath_unref(object);
}

/*============================================================================*/

struct deserializer_t {
  pmath_hashtable_t        int_to_object; // entries = struct int_object_entry_t
  pmath_t                  file;
  pmath_serialize_error_t  error;
};

static uint8_t read_ui8(struct deserializer_t *info) {
  uint8_t data[1];
  if(pmath_file_read(info->file, data, sizeof(data), FALSE) == sizeof(data))
    return data[0];
    
  if(!info->error)
    info->error = PMATH_SERIALIZE_EOF;
    
  return 0xFF;
}

static uint32_t read_ui32(struct deserializer_t *info) {
  uint8_t data[4];
  if(pmath_file_read(info->file, data, sizeof(data), FALSE) == sizeof(data)) {
    return data[0]
           | ((uint32_t)data[1] << 8)
           | ((uint32_t)data[2] << 16)
           | ((uint32_t)data[3] << 24);
  }
  
  if(!info->error)
    info->error = PMATH_SERIALIZE_EOF;
    
  return 0;
}

static int32_t read_si32(struct deserializer_t *info) {
  return (int32_t)read_ui32(info);
}

static uint64_t read_ui64(struct deserializer_t *info) {
  uint64_t result = read_ui32(info);
  result |= ((uint64_t)read_ui32(info)) << 32;
  return result;
}

static pmath_t deserialize(struct deserializer_t *info) {
  switch(read_ui8(info)) {
    case TAG_NULL: return PMATH_NULL;
    
    case TAG_NEWREF: {
        struct int_object_entry_t *entry;
        entry = pmath_mem_alloc(sizeof(struct int_object_entry_t));
        
        if(entry) {
          pmath_t result;
          
          entry->key = (intptr_t)read_ui32(info);
          entry->value = deserialize(info);
          
          result = pmath_ref(entry->value);
          
          entry = pmath_ht_insert(info->int_to_object, entry);
          if(entry)
            hashtable_int_obj_class.entry_destructor(entry);
            
          return result;
        }
        
        if(!info->error)
          info->error = PMATH_SERIALIZE_NO_MEMORY;
          
        read_ui32(info);
        return deserialize(info);
      }
      
    case TAG_REF: {
        struct int_object_entry_t *entry;
        entry = pmath_ht_search(info->int_to_object, (void *)(intptr_t)read_ui32(info));
        
        if(entry)
          return pmath_ref(entry->value);
          
        if(!info->error)
          info->error = PMATH_SERIALIZE_BAD_REF;
          
        return PMATH_UNDEFINED;
      }
      
    case TAG_MAGIC: return PMATH_FROM_TAG(PMATH_TAG_MAGIC, read_si32(info));
    
    case TAG_STR8: {
        int len = (int)read_ui32(info);
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
          uint16_t *buf       =           AFTER_STRING(result);
          const uint8_t *data = (uint8_t *)AFTER_STRING(result);
          
          --len;
          for(; len >= 0; --len)
            buf[len] = data[len];
        }
        
        return _pmath_from_buffer(result);
      }
      
    case TAG_STR16: {
        int len = (int)read_ui32(info);
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
      
    case TAG_SYMBOL: {
        pmath_string_t name = deserialize(info);
        
        if(pmath_is_string(name))
          return pmath_symbol_find(name, TRUE);
          
        if(!info->error)
          info->error = PMATH_SERIALIZE_BAD_BYTE;
          
        pmath_unref(name);
        return PMATH_UNDEFINED;
      }
      
    case TAG_EXPR: {
        pmath_expr_t result;
        size_t i, len = read_ui32(info);
        
        result = pmath_expr_new(deserialize(info), len);
        for(i = 0; i < len; ++i) {
          result = pmath_expr_set_item(result, i + 1, deserialize(info));
        }
        
        return result;
      }
      
    case TAG_INT32: return PMATH_FROM_INT32(read_si32(info));
    
    case TAG_MPINT: {
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
      
    case TAG_QUOT: {
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
      
    case TAG_MPFLOAT: {
        pmath_mpfloat_t result;
        pmath_mpint_t mant;
        mpfr_prec_t prec;
        mp_exp_t exp;
        
        prec = (mpfr_prec_t)read_ui32(info);
        if(prec < MPFR_PREC_MIN || prec > PMATH_MP_PREC_MAX) {
          if(!info->error)
            info->error = PMATH_SERIALIZE_BAD_BYTE;
            
          return PMATH_UNDEFINED;
        }
        
        exp = (mp_exp_t)read_si32(info);
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
        
        exp = (mp_exp_t)read_si32(info);
        mant = deserialize(info);
        
        if(pmath_is_int32(mant))
          mant = _pmath_create_mp_int(PMATH_AS_INT32(mant));
          
        if(!pmath_is_mpint(mant)) {
          pmath_unref(mant);
          pmath_unref(result);
          
          if(!info->error)
            info->error = PMATH_SERIALIZE_BAD_BYTE;
            
          return PMATH_UNDEFINED;
        }
        
        mpfr_set_z(  PMATH_AS_MP_ERROR(result), PMATH_AS_MPZ(mant), MPFR_RNDN);
        mpfr_mul_2si(PMATH_AS_MP_ERROR(result), PMATH_AS_MP_ERROR(result), exp, MPFR_RNDN);
        pmath_unref(mant);
        return result;
      }
      
    case TAG_DOUBLE: {
        pmath_t result;
        result.as_bits = read_ui64(info);
        
        if(pmath_is_double(result))
          return result;
          
        if(!info->error)
          info->error = PMATH_SERIALIZE_BAD_BYTE;
        return PMATH_UNDEFINED;
      }
      
    default:
      if(!info->error)
        info->error = PMATH_SERIALIZE_BAD_BYTE;
  }
  
  return PMATH_UNDEFINED;
}

/*============================================================================*/

PMATH_API
pmath_serialize_error_t pmath_serialize(
  pmath_t file,
  pmath_t object
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
