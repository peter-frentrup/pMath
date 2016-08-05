#include <pmath-util/io-varint-private.h>
#include <pmath-util/files/abstract-file.h>

#include <limits.h>


PMATH_PRIVATE
void _pmath_serialize_raw_integer(pmath_t file, pmath_integer_t value) {
  pmath_mpint_t mp;
  uint8_t buf[16];
  size_t written;
  uint8_t lsb, msb;
  uint8_t rem;
  
  if(pmath_is_null(value))
    return;
    
  assert(pmath_is_integer(value));
  
  if(pmath_is_int32(value)) {
    mp = _pmath_create_mp_int(PMATH_AS_INT32(value));
    if(pmath_is_null(mp))
      return;
  }
  else if(pmath_refcount(value) > 1) {
    mp = _pmath_create_mp_int(0);
    if(pmath_is_null(mp))
      return;
    
    mpz_set(PMATH_AS_MPZ(mp), PMATH_AS_MPZ(value));
  }
  else
    mp = value;
  
  if(mpz_sgn(PMATH_AS_MPZ(mp)) < 0) {
    lsb = 1;
    mpz_neg(PMATH_AS_MPZ(mp), PMATH_AS_MPZ(mp));
    mpz_sub_ui(PMATH_AS_MPZ(mp), PMATH_AS_MPZ(mp), 1);
  }
  else {
    lsb = 0;
  }
  
  rem = (uint8_t)mpz_fdiv_q_ui(PMATH_AS_MPZ(mp), PMATH_AS_MPZ(mp), 1<<6);
  msb = mpz_sgn(PMATH_AS_MPZ(mp)) ? 0x80 : 0;
  buf[0] = lsb | (rem << 1) | msb;
  written = 1;
  
  while(msb) {
    if(written == sizeof(buf)) {
      pmath_file_write(file, buf, written);
      written = 0;
    }
    
    rem = (uint8_t)mpz_fdiv_q_ui(PMATH_AS_MPZ(mp), PMATH_AS_MPZ(mp), 1<<7);
    msb = mpz_sgn(PMATH_AS_MPZ(mp)) ? 0x80 : 0;
    buf[written++] = rem | msb;
  }
  
  pmath_file_write(file, buf, written);
  pmath_unref(mp);
}

static void _serialize_raw_integer_simple(pmath_t file, uintptr_t val, uint8_t lsb) {
  uint8_t buf[(sizeof(uintptr_t) * 8 + 7) / 7]; // Ceiling((bytes_per_ptr * 8 + 1)/7)
  size_t written;
  uint8_t rem, msb;
  
  rem = val & 0x3F; // val % (1<<6)
  val = val >> 6;
  msb = val ? 0x80 : 0;
  buf[0] = lsb | (rem << 1) | msb;
  written = 1;
  
  while(msb) {
    rem = val & 0x7F; // val % (1<<7)
    val = val >> 7;
    msb = val ? 0x80 : 0;
    buf[written++] = rem | msb;
  }
  
  assert(written <= sizeof(buf));
  pmath_file_write(file, buf, written);
}

PMATH_PRIVATE
void _pmath_serialize_raw_integer_ui(pmath_t file, uintptr_t value) {
  _serialize_raw_integer_simple(file, value, 0);
}

PMATH_PRIVATE
void _pmath_serialize_raw_integer_si(pmath_t file, intptr_t value) {
  if(value < 0)
    _serialize_raw_integer_simple(file, (uintptr_t)-(value + 1), 1);
  else
    _serialize_raw_integer_simple(file, (uintptr_t)value, 0);
}

PMATH_PRIVATE
pmath_integer_t _pmath_deserialize_raw_integer(pmath_t file) {
  pmath_mpint_t result = _pmath_create_mp_int(0);
  pmath_mpint_t shift = _pmath_create_mp_int(1);
  
  if(pmath_is_null(result) || pmath_is_null(shift)) {
    pmath_unref(result);
    pmath_unref(shift);
    return PMATH_NULL;
  }
  
  for(;;) {
    uint8_t buf[16];
    size_t size, i;
    
    size = (int)pmath_file_read(file, buf, sizeof(buf), TRUE); // don't move file pos
    if(size == 0)
      break;
      
    for(i = 0; i < size; ++i) {
      mpz_addmul_ui(
        PMATH_AS_MPZ(result),
        PMATH_AS_MPZ(shift),
        buf[i] & 0x7F);
        
      if((buf[i] & 0x80) == 0) {
        unsigned lsb;
        pmath_file_read(file, buf, i + 1, FALSE); // move file pos
        
        lsb = 1 & mpz_fdiv_q_ui(PMATH_AS_MPZ(result), PMATH_AS_MPZ(result), 2);
        if(lsb) {
          mpz_neg(PMATH_AS_MPZ(result), PMATH_AS_MPZ(result));
          mpz_sub_ui(PMATH_AS_MPZ(result), PMATH_AS_MPZ(result), 1);
        }
        pmath_unref(shift);
        return _pmath_mp_int_normalize(result);
      }
      
      mpz_mul_2exp(PMATH_AS_MPZ(shift), PMATH_AS_MPZ(shift), 7);
    }
    
    pmath_file_read(file, buf, size, FALSE); // move file pos
  }
  
  pmath_unref(result);
  pmath_unref(shift);
  return PMATH_NULL;
}

static pmath_bool_t _deserialize_raw_simple_integer(pmath_t file, uintptr_t *val, pmath_bool_t *lsb) {
  uint8_t buf[(sizeof(uintptr_t) * 8 + 7) / 7]; // Ceiling((bytes_per_ptr * 8 + 1)/7)
  size_t size, i;
  uintptr_t shift, tmp;
  
  *val = 0;
  *lsb = FALSE;
  
  size = pmath_file_read(file, buf, sizeof(buf), TRUE); // don't move file pos
  if(size == 0)
    return FALSE;
    
  for(i = 0;i < size;++i) {
    if(!(buf[i] & 0x80))
      break;
  }
  
  if(i == size)
    return FALSE;
  
  size = i + 1;
  
  if(size == sizeof(buf)) {
    int valid_bits_in_last_byte = ((sizeof(uintptr_t) * 8) % 7 + 1) % 8;
    int mask = (valid_bits_in_last_byte - 1) ^ 0xFF;
    if(buf[size-1] & mask)
      return FALSE; // overflow
  }
  
  *lsb = buf[0] & 1;
  *val = (buf[0] & 0x7F) >> 1;
  shift = 6;
  
  for(i = 1;i < size;++i) {
    tmp = (buf[i] & 0x7F) << shift;
    *val|= tmp;
    shift+= 7;
  }
  
  pmath_file_read(file, buf, size, FALSE); // move file pos
  return TRUE;
}

PMATH_PRIVATE
pmath_bool_t _pmath_deserialize_raw_integer_ui(pmath_t file, uintptr_t *value) {
  pmath_bool_t lsb;
  
  assert(value != NULL);
  
  return _deserialize_raw_simple_integer(file, value, &lsb) && !lsb;
}

PMATH_PRIVATE
pmath_bool_t _pmath_deserialize_raw_integer_si(pmath_t file, intptr_t *value) {
  pmath_bool_t lsb;
  uintptr_t ui;
  
  assert(value != NULL);
  *value = 0;
  
  if(!_deserialize_raw_simple_integer(file, &ui, &lsb))
    return FALSE;
  
  if(ui > (SIZE_MAX >> 1)) // INTPTR_MAX
    return FALSE;
    
  *value = (intptr_t)ui;
  if(lsb) 
    *value = -*value - 1;
  
  return TRUE;
}
