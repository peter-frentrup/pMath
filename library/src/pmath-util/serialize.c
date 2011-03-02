#include <pmath-util/serialize.h>

#include <pmath-core/expressions.h>
#include <pmath-core/numbers-private.h>
#include <pmath-core/strings-private.h>

#include <pmath-util/files.h>
#include <pmath-util/hashtables-private.h>
#include <pmath-util/memory.h>


/* Data format   (LE = little endian)

   - Magic objects:      
       object == PMATH_NULL:   0     (1 byte)
       object == b       1,b   (2 bytes)
   
   - new reference:      2,B,object  (B = 32BitLE number as new reference)
   - back references:    3,B         (B = 32BitLE number to previous reference)
   
   - strings:            4,L,C,C,...
                             \_____/ = L * 2 bytes   (L = 32BitLE length)
                                                     (C = 16BitLE char)
                             
   - symbols:            5,name      (name = symbol name as serialized string)
   
   - expressions:        6,L,I,I,...
                             \_____/ = L + 1 serialized objects (L = 32BitLE length)
   
   - integers:           7,L,N,N,...
                             \_____/ = abs(L) * 1 byte   (N,N,... = data in LE)
                                                         (sign(L) = sign)
                                                         (L       = 32BitLE length)
                             
   - quotients:          8,n,d         (n/d = serialized numerator/denomonator)
   
   - multi floats        9,VP,VE,VM,EE,EM  
                                 (VP = value's unsigned 32BitLE precision in bits)
                                 (VE = value's signed 32BitLE exponent)
                                 (VM = value's serialized integer mantissa)
                                 (EE = error's signed 32BitLE exponent)
                                 (EM = error's serialized integer mantissa)
   
   - machine floats     10,E,M   (E = signed 32BitLE exponent)
                                 (M = serialized integers mantissa)
   
   - latin 1 strings:   11,L,C,C,...
                             \_____/ = L * 1 bytes   (L = 32BitLE length)
                                                     (C = 8 Bit char)
 */

//{ hashtable_int_obj_class ...

struct int_object_entry_t{
  intptr_t       key;
  pmath_t value;
};

static void int_object_entry_destructor(struct int_object_entry_t *e){
  pmath_unref(e->value);
  pmath_mem_free(e);
}

static pmath_bool_t int_object_entries_equal(
  struct int_object_entry_t *e1,
  struct int_object_entry_t *e2
){
  return e1->key == e2->key;
}

static unsigned int int_object_entry_hash(struct int_object_entry_t *e){
  return _pmath_hash_pointer((void*)e->key);
}

static pmath_bool_t int_object_entry_equals_key(
  struct int_object_entry_t *e,
  intptr_t                   k
){
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

static void write_ui8(pmath_t file, uint8_t i){
  pmath_file_write(file, &i, 1);
}

static void write_ui16(pmath_t file, uint16_t i){
  #if PMATH_BYTE_ORDER > 0
  uint8_t data[2];
  
  data[0] = i & 0xFF;
  data[1] = (i & 0xFF00) >> 8;
  
  pmath_file_write(file, data, 2);
  #else
  pmath_file_write(file, &i, 2);
  #endif
}

static void write_ui32(pmath_t file, uint32_t i){
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

static void write_si32(pmath_t file, int32_t i){
  write_ui32(file, (uint32_t)i);
}

struct serializer_t{
  pmath_hashtable_t            object_to_int; // entries = struct _pmath_object_int_entry_t
  pmath_t               file;
  pmath_serialize_error_t  error;
  intptr_t                     next_id;
};

static void serialize(
  struct serializer_t *info,
  pmath_t       object // will be freed
){
  if(PMATH_IS_MAGIC(object)){
    if(!object){
      write_ui8(info->file, 0);
    }
    else{
      write_ui8(info->file, 1);
      write_ui8(info->file, (uint8_t)(uintptr_t)object);
    }
  }
  else{
    struct _pmath_object_int_entry_t *entry;
    entry = pmath_ht_search(info->object_to_int, object);
    
    if(entry){
      write_ui8( info->file, 3);
      write_ui32(info->file, (uint32_t)entry->value);
      pmath_unref(object);
      return;
    }
    
    entry = pmath_mem_alloc(sizeof(struct _pmath_object_int_entry_t));
    if(!entry){
      pmath_unref(object);
      if(!info->error)
        info->error = PMATH_SERIALIZE_NO_MEMORY;
      return;
    }
    
    entry->key = object;
    entry->value = info->next_id++;
    
    write_ui8( info->file, 2);
    write_ui32(info->file, entry->value);
    
    entry = pmath_ht_insert(info->object_to_int, entry);
    
    switch(object->type_shift){
      case PMATH_TYPE_SHIFT_STRING: {
        const uint16_t *buf =    pmath_string_buffer(object);
        uint32_t len = (uint32_t)pmath_string_length(object);
        unsigned int i;
        
        pmath_bool_t all_latin1 = TRUE;
        for(i = 0;i < len;++i){
          if(buf[i] > 0xFF){
            all_latin1 = FALSE;
            break;
          }
        }
        
        if(all_latin1){
          write_ui8( info->file, 11);
          write_ui32(info->file, len);
          for(;len > 0;++buf,--len)
            write_ui8(info->file, (uint8_t)*buf);
        }
        else{
          write_ui8( info->file, 4);
          write_ui32(info->file, len);
          for(;len > 0;++buf,--len)
            write_ui16(info->file, *buf);
        }
      } break;
      
      case PMATH_TYPE_SHIFT_SYMBOL: {
        write_ui8(info->file, 5);
        serialize(info, pmath_symbol_name(object));
      } break;
      
      case PMATH_TYPE_SHIFT_EXPRESSION_GENERAL:
      case PMATH_TYPE_SHIFT_EXPRESSION_GENERAL_PART: {
        size_t i, length = pmath_expr_length(object);
        
        if(length >= 0xFFFFFFFFU)
          goto DEFAULT;
        
        write_ui8( info->file, 6);
        write_ui32(info->file, (uint32_t)length);
        
        for(i = 0;i <= length;++i)
          serialize(info, pmath_expr_get_item(object, i));
          
      } break;
      
      case PMATH_TYPE_SHIFT_INTEGER: {
        size_t count = (mpz_sizeinbase(((struct _pmath_integer_t*)object)->value, 2) + 7) / 8;
        void *data;
        
        if(count > 0x7FFFFFFFU)
          goto DEFAULT;
        
        data = pmath_mem_alloc(count);
        if(!data){
          if(!info->error)
            info->error = PMATH_SERIALIZE_NO_MEMORY;
          serialize(info, PMATH_UNDEFINED);
          break;
        }
        
        write_ui8(info->file, 7);
        
        if(mpz_sgn(((struct _pmath_integer_t*)object)->value) < 0)
          write_si32(info->file, -(int32_t)count);
        else
          write_si32(info->file, +(int32_t)count);
        
        mpz_export(
          data,
          PMATH_NULL,
          -1,
          1,
          0,
          0,
          ((struct _pmath_integer_t*)object)->value);
        
        pmath_file_write(info->file, data, count);
          
        pmath_mem_free(data);
      } break;
      
      case PMATH_TYPE_SHIFT_QUOTIENT: {
        write_ui8(info->file, 8);
        serialize(info, pmath_rational_numerator(object));
        serialize(info, pmath_rational_denominator(object));
      } break;
      
      case PMATH_TYPE_SHIFT_MP_FLOAT: {
        struct _pmath_integer_t *mantissa = _pmath_create_integer();
        mp_prec_t prec;
        mp_exp_t exp;
        
        if(!mantissa){
          if(!info->error)
            info->error = PMATH_SERIALIZE_NO_MEMORY;
          serialize(info, PMATH_UNDEFINED);
          break;
        }
        
        prec = mpfr_get_prec(((struct _pmath_mp_float_t*)object)->value);
        
        exp = mpfr_get_z_exp(
          mantissa->value,
          ((struct _pmath_mp_float_t*)object)->value);
        
        write_ui8(info->file, 9);
        write_ui32(info->file, (uint32_t)prec);
        write_si32(info->file, (int32_t)exp);
        serialize(info, pmath_ref((pmath_integer_t)mantissa));
        
        exp = mpfr_get_z_exp(
          mantissa->value,
          ((struct _pmath_mp_float_t*)object)->error);
        
        write_si32(info->file, (int32_t)exp);
        serialize(info, (pmath_integer_t)mantissa);
      } break;
      
      case PMATH_TYPE_SHIFT_MACHINE_FLOAT: {
        struct _pmath_mp_float_t *f = _pmath_create_mp_float(DBL_MANT_DIG);
        struct _pmath_integer_t *mantissa = _pmath_create_integer();
        mp_exp_t exp;
        
        if(!mantissa || !f){
          pmath_unref((pmath_integer_t)mantissa);
          pmath_unref((pmath_float_t)f);
          
          if(!info->error)
            info->error = PMATH_SERIALIZE_NO_MEMORY;
          serialize(info, PMATH_UNDEFINED);
          break;
        }
        
        mpfr_set_d(f->value, ((struct _pmath_machine_float_t*)object)->value, GMP_RNDN);
        exp = mpfr_get_z_exp(
          mantissa->value,
          f->value);
        
        write_ui8(info->file, 10);
        write_si32(info->file, (int32_t)exp);
        serialize(info, (pmath_integer_t)mantissa);
        pmath_unref((pmath_float_t)f);
      } break;
      
      default: DEFAULT:
        if(!info->error)
          info->error = PMATH_SERIALIZE_BAD_OBJECT;
        
        serialize(info, PMATH_UNDEFINED);
    }
    
    if(entry)
      pmath_ht_obj_int_class.entry_destructor(entry);
  }
}

/*============================================================================*/

struct deserializer_t{
  pmath_hashtable_t            int_to_object; // entries = struct int_object_entry_t
  pmath_t               file;
  pmath_serialize_error_t  error;
};

static uint8_t read_ui8(struct deserializer_t *info){
  uint8_t data[1];
  if(pmath_file_read(info->file, data, sizeof(data), FALSE) == sizeof(data))
    return data[0];
  
  if(!info->error)
    info->error = PMATH_SERIALIZE_EOF;  
  
  return 0xFF;
}

static uint32_t read_ui32(struct deserializer_t *info){
  uint8_t data[4];
  if(pmath_file_read(info->file, data, sizeof(data), FALSE) == sizeof(data)){
    return data[0] 
     | ((uint32_t)data[1] << 8)
     | ((uint32_t)data[2] << 16)
     | ((uint32_t)data[3] << 24);
  }
  
  if(!info->error)
    info->error = PMATH_SERIALIZE_EOF;  
  
  return 0;
}

static int32_t read_si32(struct deserializer_t *info){
  return (int32_t)read_ui32(info);
}

static pmath_t deserialize(struct deserializer_t *info){
  switch(read_ui8(info)){
    case 0: return PMATH_NULL;
    case 1: return (pmath_t)(uintptr_t)read_ui8(info);
    
    case 2: {
      struct int_object_entry_t *entry;
      entry = pmath_mem_alloc(sizeof(struct int_object_entry_t));
      
      if(entry){
        pmath_t result;
        
        entry->key = (intptr_t)read_ui32(info);
        entry->value = deserialize(info);
        
        result = pmath_ref(entry->value);
        
        entry = pmath_ht_insert(info->int_to_object, entry);
        if(entry)
          hashtable_int_obj_class.entry_destructor(entry);
        
        return result;
      }
      else{
        if(!info->error)
          info->error = PMATH_SERIALIZE_NO_MEMORY;
        
        read_ui32(info);
        return deserialize(info);
      }
    } break;
    
    case 3: {
      struct int_object_entry_t *entry;
      entry = pmath_ht_search(info->int_to_object, (void*)(intptr_t)read_ui32(info));
      
      if(entry)
        return pmath_ref(entry->value);
      
      if(!info->error)
        info->error = PMATH_SERIALIZE_BAD_REF;
    } break;
    
    case 4: {
      int len = (int)read_ui32(info);
      struct _pmath_string_t *result;
      
      if(2 * len < 0){
        if(!info->error)
          info->error = PMATH_SERIALIZE_BAD_BYTE;
        break;
      }
      
      result = _pmath_new_string_buffer(len);
      if(!result){
        if(!info->error)
          info->error = PMATH_SERIALIZE_NO_MEMORY;
        break;
      }
      
      if(pmath_file_read(info->file, AFTER_STRING(result), 2 * len, FALSE) != 2 * (size_t)len){
        if(!info->error)
          info->error = PMATH_SERIALIZE_EOF;  
        
        pmath_unref((pmath_string_t)result);
        break;
      }
      
      #if PMATH_BYTE_ORDER > 0
      {
        uint16_t *buf = (uint16_t*)AFTER_STRING(result);
        
        for(;len > 0;--len,++buf)
          *buf = ((*buf & 0x00FF) << 8) | ((*buf & 0xFF00) >> 8);
      }
      #endif
      
      return (pmath_string_t)result;
    } break;
    
    case 5: {
      pmath_string_t name = deserialize(info);
      
      if(pmath_is_string(name))
        return pmath_symbol_find(name, TRUE);
      
      if(!info->error)
        info->error = PMATH_SERIALIZE_BAD_BYTE;
        
      pmath_unref(name);
    } break;
    
    case 6: {
      pmath_expr_t result;
      size_t i, len = read_ui32(info);
      
      result = pmath_expr_new(deserialize(info), len);
      for(i = 0;i < len;++i){
        result = pmath_expr_set_item(result, i + 1, deserialize(info));
      }
      
      return result;
    } break;
    
    case 7: {
      struct _pmath_integer_t *result;
      int32_t slen = read_si32(info);
      uint32_t ulen = abs(slen);
      uint8_t *data;
      
      data = pmath_mem_alloc(ulen);
      if(!data && ulen){
        if(!info->error)
          info->error = PMATH_SERIALIZE_NO_MEMORY;
        break;
      }
      
      if(pmath_file_read(info->file, data, ulen, FALSE) != ulen){
        if(!info->error)
          info->error = PMATH_SERIALIZE_EOF;  
        
        pmath_mem_free(data);
        break;
      }
      
      result = _pmath_create_integer();
      if(!result){
        if(!info->error)
          info->error = PMATH_SERIALIZE_EOF;  
        
        pmath_mem_free(data);
        break;
      }
      
      mpz_import(
        result->value,
        ulen,
        -1,
        1,
        0,
        0,
        data);
      
      pmath_mem_free(data);
      if(slen < 0)
        mpz_neg(result->value, result->value);
        
      return (pmath_integer_t)PMATH_FROM_PTR(result);
    } break;
    
    case 8: {
      pmath_integer_t num = deserialize(info);
      pmath_integer_t den = deserialize(info);
      
      if(pmath_is_integer(num)
      && pmath_is_integer(den)
      && pmath_number_sign(den) > 0){
        return pmath_rational_new(num, den);
      }
      
      pmath_unref(num);
      pmath_unref(den);
      
      if(!info->error)
        info->error = PMATH_SERIALIZE_BAD_BYTE;
    } break;
    
    case 9: {
      struct _pmath_mp_float_t *result;
      pmath_integer_t mant;
      mp_prec_t prec;
      mp_exp_t exp;
      
      prec = (mp_prec_t)read_ui32(info);
      if(prec < MPFR_PREC_MIN || prec > PMATH_MP_PREC_MAX){
        if(!info->error)
          info->error = PMATH_SERIALIZE_BAD_BYTE;
        break;
      }
      
      exp = (mp_exp_t)read_si32(info);
      mant = deserialize(info);
      
      if(!pmath_is_integer(mant)){
        pmath_unref(mant);
        
        if(!info->error)
          info->error = PMATH_SERIALIZE_BAD_BYTE;
        break;
      }
    
      result = _pmath_create_mp_float(prec);
      if(!result){
        if(!info->error)
          info->error = PMATH_SERIALIZE_NO_MEMORY;
        
        pmath_unref(mant);
        break;
      }
      
      mpfr_set_z(result->value, ((struct _pmath_integer_t*)mant)->value, GMP_RNDN);
      mpfr_mul_2si(result->value, result->value, exp, GMP_RNDN);
      pmath_unref(mant);
      
      exp = (mp_exp_t)read_si32(info);
      mant = deserialize(info);
      
      if(!pmath_is_integer(mant)){
        pmath_unref(mant);
        pmath_unref((pmath_float_t)PMATH_FROM_PTR(result));;
        
        if(!info->error)
          info->error = PMATH_SERIALIZE_BAD_BYTE;
        break;
      }
      
      mpfr_set_z(result->error, ((struct _pmath_integer_t*)mant)->value, GMP_RNDN);
      mpfr_mul_2si(result->error, result->error, exp, GMP_RNDN);
      pmath_unref(mant);
      return (pmath_float_t)PMATH_FROM_PTR(result);
    } break;
    
    case 10: {
      struct _pmath_mp_float_t *result;
      pmath_integer_t mant;
      mp_exp_t exp;
      
      exp = (mp_exp_t)read_si32(info);
      mant = deserialize(info);
      
      if(!pmath_is_integer(mant)){
        pmath_unref(mant);
        
        if(!info->error)
          info->error = PMATH_SERIALIZE_BAD_BYTE;
        break;
      }
    
      result = _pmath_create_mp_float(DBL_MANT_DIG);
      if(!result){
        if(!info->error)
          info->error = PMATH_SERIALIZE_NO_MEMORY;
        
        pmath_unref(mant);
        break;
      }
      
      mpfr_set_z(result->value, ((struct _pmath_integer_t*)mant)->value, GMP_RNDN);
      mpfr_mul_2si(result->value, result->value, exp, GMP_RNDN);
      pmath_unref(mant);
      
      mant = pmath_float_new_d(mpfr_get_d(result->value, GMP_RNDN));
      pmath_unref((pmath_float_t)PMATH_FROM_PTR(result));;
      
      return mant;
    } break;
    
    case 11: {
      int len = (int)read_ui32(info);
      struct _pmath_string_t *result;
      
      if(len < 0){
        if(!info->error)
          info->error = PMATH_SERIALIZE_BAD_BYTE;
        break;
      }
      
      result = _pmath_new_string_buffer(len);
      if(!result){
        if(!info->error)
          info->error = PMATH_SERIALIZE_NO_MEMORY;
        break;
      }
      
      if(pmath_file_read(info->file, AFTER_STRING(result), len, FALSE) != (size_t)len){
        if(!info->error)
          info->error = PMATH_SERIALIZE_EOF;  
        
        pmath_unref((pmath_string_t)result);
        break;
      }
      
      {
        uint16_t *buf       =           AFTER_STRING(result);
        const uint8_t *data = (uint8_t*)AFTER_STRING(result);
        
        --len;
        for(;len >= 0;--len)
          buf[len] = data[len];
      }
      
      return (pmath_string_t)result;
    } break;

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
){
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
  pmath_t               file,
  pmath_serialize_error_t *error
){
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
