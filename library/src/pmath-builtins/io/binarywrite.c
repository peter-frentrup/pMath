#include <pmath-util/evaluation.h>
#include <pmath-util/approximate.h>
#include <pmath-util/files/abstract-file.h>
#include <pmath-util/helpers.h>
#include <pmath-util/io-varint-private.h>
#include <pmath-util/messages.h>
#include <pmath-util/serialize.h>
#include <pmath-util/option-helpers.h>

#include <pmath-builtins/arithmetic-private.h>
#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/io-private.h>

#include <string.h>


extern pmath_symbol_t pmath_System_DollarFailed;
extern pmath_symbol_t pmath_System_DirectedInfinity;
extern pmath_symbol_t pmath_System_Expression;
extern pmath_symbol_t pmath_System_Integer;
extern pmath_symbol_t pmath_System_List;
extern pmath_symbol_t pmath_System_Undefined;

static pmath_bool_t binary_write(
  pmath_t  file,         // wont be freed
  pmath_t  value,        // will be freed
  pmath_t  type,         // will be freed
  int      byte_ordering
) {
  if( pmath_same(type, pmath_System_Expression) ||
      (pmath_is_string(type) &&
       pmath_string_equals_latin1(type, "Expression")))
  {
    pmath_unref(type);
    pmath_serialize(file, value, 0);
    
    return TRUE;
  }
  
  if(pmath_same(type, pmath_System_Integer)) {
    if(!pmath_is_integer(value)) {
      pmath_message(PMATH_NULL, "nocoerce", 2, value, type);
      return FALSE;
    }
    
    _pmath_serialize_raw_integer(file, value);
    pmath_unref(type);
    return TRUE;
  }
  
  if(pmath_is_expr_of(value, pmath_System_List)) {
    size_t i;
    
    if(pmath_is_expr_of(type, pmath_System_List)) {
      size_t tmax = pmath_expr_length(type);
      
      if(tmax == 0) {
        pmath_unref(type);
        type = PMATH_NULL;
      }
      else if(tmax == 1) {
        pmath_t item = pmath_expr_get_item(type, 1);
        pmath_unref(type);
        type = item;
      }
      else {
        size_t j = 1;
        
        for(i = 1; i <= pmath_expr_length(value); ++i) {
          if(!binary_write(
                file,
                pmath_expr_get_item(value, i),
                pmath_expr_get_item(type, j),
                byte_ordering))
          {
            pmath_unref(value);
            pmath_unref(type);
            return FALSE;
          }
          
          if(j == tmax)
            j = 1;
          else
            ++j;
        }
        
        pmath_unref(value);
        pmath_unref(type);
        return TRUE;
      }
    }
    
    for(i = 1; i <= pmath_expr_length(value); ++i) {
      if(!binary_write(
            file,
            pmath_expr_get_item(value, i),
            pmath_ref(type),
            byte_ordering))
      {
        pmath_unref(value);
        pmath_unref(type);
        return FALSE;
      }
    }
    
    pmath_unref(value);
    pmath_unref(type);
    return TRUE;
  }
  else if(pmath_is_null(type) || pmath_is_string(type)) {
    if(pmath_is_string(value)) {
      const uint16_t *buf = pmath_string_buffer(&value);
      const int len = pmath_string_length(value);
      
      if( pmath_is_null(type)                            ||
          pmath_string_equals_latin1(type, "Character8") ||
          pmath_string_equals_latin1(type, "TerminatedString"))
      {
        uint8_t data[256];
        int datalen = sizeof(data);
        int i, j;
        
        for(i = 0; i < len; ++i) {
          if(buf[i] > 0xFF) {
            if(pmath_is_null(type))
              type = PMATH_C_STRING("Character8");
              
            pmath_message(PMATH_NULL, "nocoerce", 2, value, type);
            return FALSE;
          }
        }
        
        for(i = 0; i < len; i += sizeof(data)) {
          if(datalen > len)
            datalen = len;
            
          for(j = 0; j < datalen; ++j)
            data[j] = (uint8_t)buf[i + j];
            
          pmath_file_write(file, data, (size_t)datalen);
        }
        
        if(pmath_string_equals_latin1(type, "TerminatedString")) {
          data[0] = 0;
          pmath_file_write(file, data, 1);
        }
        
        pmath_unref(type);
        pmath_unref(value);
        return TRUE;
      }
      
      if(pmath_string_equals_latin1(type, "Character16")) {
        pmath_unref(type);
        pmath_unref(value);
        
        if(byte_ordering == PMATH_BYTE_ORDER) {
          pmath_file_write(
            file,
            pmath_string_buffer(&value),
            (size_t)pmath_string_length(value));
        }
        else {
          const uint16_t *str = pmath_string_buffer(&value);
          const int len = pmath_string_length(value);
          uint8_t data[256];
          int datalen = sizeof(data) / 2;
          int i, j;
          
          for(i = 0; i < len; ++i) {
            if(datalen > len)
              datalen = len;
              
            memcpy(data, &str[i], (size_t)datalen * 2);
            for(j = 0; j < datalen; ++j) {
              uint8_t tmp = data[2 * j];
              data[2 * j] = data[2 * j + 1];
              data[2 * j + 1] = tmp;
            }
            
            pmath_file_write(
              file,
              data,
              (size_t)datalen * 2);
          }
        }
        
        return TRUE;
      }
    }
    else {
      uint8_t data[16];
      size_t size = 0;
      enum {
        NONE,
        UINT,
        INT,
        CHAR,
        REAL,
        COMPLEX
      } out_type = NONE;
      
      if( pmath_is_null(type)                      ||
          pmath_string_equals_latin1(type, "Byte") ||
          pmath_string_equals_latin1(type, "UnsignedInteger8"))
      {
        size = 1;
        out_type = UINT;
      }
      else if(pmath_string_equals_latin1(type, "Integer8")) {
        size = 1;
        out_type = INT;
      }
      else if(pmath_string_equals_latin1(type, "UnsignedInteger16")) {
        size = 2;
        out_type = UINT;
      }
      else if(pmath_string_equals_latin1(type, "Integer16")) {
        size = 2;
        out_type = INT;
      }
      else if(pmath_string_equals_latin1(type, "UnsignedInteger24")) {
        size = 3;
        out_type = UINT;
      }
      else if(pmath_string_equals_latin1(type, "Integer24")) {
        size = 3;
        out_type = INT;
      }
      else if(pmath_string_equals_latin1(type, "UnsignedInteger32")) {
        size = 4;
        out_type = UINT;
      }
      else if(pmath_string_equals_latin1(type, "Integer32")) {
        size = 4;
        out_type = INT;
      }
      else if(pmath_string_equals_latin1(type, "UnsignedInteger64")) {
        size = 8;
        out_type = UINT;
      }
      else if(pmath_string_equals_latin1(type, "Integer64")) {
        size = 8;
        out_type = INT;
      }
      else if(pmath_string_equals_latin1(type, "UnsignedInteger128")) {
        size = 16;
        out_type = UINT;
      }
      else if(pmath_string_equals_latin1(type, "Integer128")) {
        size = 16;
        out_type = INT;
      }
      else if(pmath_string_equals_latin1(type, "Character8")) {
        size = 1;
        out_type = CHAR;
      }
      else if(pmath_string_equals_latin1(type, "Character16")) {
        size = 2;
        out_type = CHAR;
      }
      else if(pmath_string_equals_latin1(type, "Real16")) {
        size = 2;
        out_type = REAL;
      }
      else if(pmath_string_equals_latin1(type, "Real32")) {
        size = 4;
        out_type = REAL;
      }
      else if(pmath_string_equals_latin1(type, "Real64")) {
        size = 8;
        out_type = REAL;
      }
      else if(pmath_string_equals_latin1(type, "Real128")) {
        size = 16;
        out_type = REAL;
      }
      else if(pmath_string_equals_latin1(type, "Complex32")) {
        size = 2;
        out_type = COMPLEX;
      }
      else if(pmath_string_equals_latin1(type, "Complex64")) {
        size = 4;
        out_type = COMPLEX;
      }
      else if(pmath_string_equals_latin1(type, "Complex128")) {
        size = 8;
        out_type = COMPLEX;
      }
      else if(pmath_string_equals_latin1(type, "Complex256")) {
        size = 16;
        out_type = COMPLEX;
      }
      
      if(size) {
        if(out_type == REAL || out_type == COMPLEX) {
          int re_dir = 2;
          int im_dir = 2;
          
          if( out_type == REAL &&
              pmath_equals(value, PMATH_FROM_INT32(0)))
          {
            pmath_unref(value);
            pmath_unref(type);
            
            memset(data, 0, size);
            pmath_file_write(file, data, size);
            return TRUE;
          }
          
          if(pmath_is_rational(value)) {
            switch(size) {
              case 2:  value = pmath_set_precision(value,  11); break;
              case 4:  value = pmath_set_precision(value,  24); break;
              case 8:  value = pmath_set_precision(value,  53); break;
              default: value = pmath_set_precision(value, 113); break;
            }
            
            value = pmath_evaluate(value);
          }
          
          if(out_type == COMPLEX) {
            pmath_t re = PMATH_NULL;
            pmath_t im = PMATH_NULL;
            
            if(_pmath_re_im(pmath_ref(value), &re, &im)) {
              pmath_unref(type);
              pmath_unref(value);
              
              switch(size) {
                case 2:  type = PMATH_C_STRING("Real16");  break;
                case 4:  type = PMATH_C_STRING("Real32");  break;
                case 8:  type = PMATH_C_STRING("Real64");  break;
                default: type = PMATH_C_STRING("Real128"); break;
              }
              
              binary_write(file, re, pmath_ref(type), byte_ordering);
              binary_write(file, im, type, byte_ordering);
              
              return TRUE;
            }
          }
          
          if(pmath_is_float(value) && out_type == REAL) {
            pmath_mpint_t mant = _pmath_create_mp_int(0);
            long bits = 10;
            
            pmath_unref(type);
            type = PMATH_NULL;
            
            switch(size) {
              case  2: bits =  10; break;
              case  4: bits =  23; break;
              case  8: bits =  52; break;
              case 16: bits = 112; break;
            }
            
            if(!pmath_is_null(mant)) {
              mp_exp_t exp;
              
              if(pmath_is_mpfloat(value)) {
                mpfr_t tmp_value;
                mpfr_init2(tmp_value, bits + 1);
                arf_get_mpfr(tmp_value, arb_midref(PMATH_AS_ARB(value)), MPFR_RNDN);
                if(mpfr_zero_p(tmp_value) || mpfr_number_p(tmp_value)) {
                  exp = bits + mpfr_get_z_exp(PMATH_AS_MPZ(mant), tmp_value);
                }
                else {
                  mpz_set_si(PMATH_AS_MPZ(mant), mpfr_sgn(tmp_value));
                  mpfr_clear(tmp_value);
                  exp = 0;
                  goto MAKE_INFINTE;
                }
              }
              else {
                mpfr_t tmp_value;
                mpfr_init2(tmp_value, bits + 1);
                mpfr_set_d(tmp_value, PMATH_AS_DOUBLE(value), MPFR_RNDN);
                exp = bits + mpfr_get_z_exp(PMATH_AS_MPZ(mant), tmp_value);
                mpfr_clear(tmp_value);
              }
              
              switch(size) {
                case 2: {
                    if(exp < -14) {
                      mpz_fdiv_q_2exp(PMATH_AS_MPZ(mant), PMATH_AS_MPZ(mant), - exp - 14);
                      exp = -15;
                    }
                    else
                      mpz_clrbit(PMATH_AS_MPZ(mant), bits);
                      
                    if( exp <= 15 &&
                        mpz_cmpabs_ui(PMATH_AS_MPZ(mant), 0x400) <= 0)
                    {
                      unsigned umant = (unsigned)mpz_get_ui(PMATH_AS_MPZ(mant));
                      unsigned uexp = (unsigned)(exp + 15);
                      
                      data[0] = (umant >> 8) & 0x03;
                      data[1] = umant & 0xFF;
                      
                      if(mpz_sgn(PMATH_AS_MPZ(mant)) < 0)
                        data[0] |= 0x80;
                        
                      data[0] |= uexp << 2;
                      
                      if(byte_ordering < 0) {
                        uint8_t tmp = data[0];
                        data[0] = data[1];
                        data[1] = tmp;
                      }
                    }
                    else
                      goto MAKE_INFINTE;
                  } break;
                  
                case 4: {
                    if(exp < -126) {
                      mpz_fdiv_q_2exp(PMATH_AS_MPZ(mant), PMATH_AS_MPZ(mant), - exp - 126);
                      exp = -127;
                    }
                    else
                      mpz_clrbit(PMATH_AS_MPZ(mant), bits);
                      
                    if( exp <= 127 &&
                        mpz_cmpabs_ui(PMATH_AS_MPZ(mant), 0x800000) <= 0)
                    {
                      unsigned umant = (unsigned)mpz_get_ui(PMATH_AS_MPZ(mant));
                      unsigned uexp = (unsigned)(exp + 127);
                      
                      data[0] = 0;
                      data[1] = (umant >> 16) & 0x7F;
                      data[2] = (umant >> 8) & 0xFF;
                      data[3] = umant & 0xFF;
                      
                      if(mpz_sgn(PMATH_AS_MPZ(mant)) < 0)
                        data[0] |= 0x80;
                        
                      data[0] |= uexp >> 1;
                      data[1] |= uexp << 7;
                      
                      if(byte_ordering < 0) {
                        uint8_t tmp = data[0];
                        data[0] = data[3];
                        data[3] = tmp;
                        
                        tmp = data[1];
                        data[1] = data[2];
                        data[2] = tmp;
                      }
                    }
                    else
                      goto MAKE_INFINTE;
                  } break;
                  
                case 8: {
                    if(exp < -1022) {
                      mpz_fdiv_q_2exp(PMATH_AS_MPZ(mant), PMATH_AS_MPZ(mant), - exp - 1022);
                      exp = -1023;
                    }
                    else
                      mpz_clrbit(PMATH_AS_MPZ(mant), bits);
                      
                    if(exp <= 1023) {
                      size_t count = (mpz_sizeinbase(PMATH_AS_MPZ(mant), 2) + 7) / 8;
                      unsigned uexp = (unsigned)(exp + 1023);
                      
                      if(count > 7)
                        goto MAKE_INFINTE;
                        
                      memset(data, 0, size);
                      mpz_export(data, NULL, -1, 1, PMATH_BYTE_ORDER, 0, PMATH_AS_MPZ(mant));
                      
                      if(data[6] & 0xF0)
                        goto MAKE_INFINTE;
                        
                      data[6] |= uexp << 4;
                      data[7] = (uexp >> 4) & 0x7F;
                      
                      if(mpz_sgn(PMATH_AS_MPZ(mant)) < 0)
                        data[7] |= 0x80;
                        
                      if(byte_ordering > 0) {
                        size_t i;
                        for(i = 0; i < size / 2; ++i) {
                          uint8_t tmp = data[i];
                          data[i] = data[size - i - 1];
                          data[size - i - 1] = tmp;
                        }
                      }
                    }
                    else
                      goto MAKE_INFINTE;
                  } break;
                  
                case 16: {
                    if(exp < -16382) {
                      mpz_fdiv_q_2exp(PMATH_AS_MPZ(mant), PMATH_AS_MPZ(mant), - exp - 16382);
                      exp = -16383;
                    }
                    else
                      mpz_clrbit(PMATH_AS_MPZ(mant), bits);
                      
                    if(exp <= 16383) {
                      size_t count = (mpz_sizeinbase(PMATH_AS_MPZ(mant), 2) + 7) / 8;
                      unsigned uexp = (unsigned)(exp + 16383);
                      
                      if(count > 14)
                        goto MAKE_INFINTE;
                        
                      memset(data, 0, size);
                      mpz_export(data, NULL, -1, 1, PMATH_BYTE_ORDER, 0, PMATH_AS_MPZ(mant));
                      
                      if(data[6] & 0xF0)
                        goto MAKE_INFINTE;
                        
                      data[14] |= uexp;
                      data[15] = (uexp >> 8) & 0x7F;
                      
                      if(mpz_sgn(PMATH_AS_MPZ(mant)) < 0)
                        data[15] |= 0x80;
                        
                      if(byte_ordering > 0) {
                        size_t i;
                        for(i = 0; i < size / 2; ++i) {
                          uint8_t tmp = data[i];
                          data[i] = data[size - i - 1];
                          data[size - i - 1] = tmp;
                        }
                      }
                    }
                    else
                      goto MAKE_INFINTE;
                  } break;
                  
              default: MAKE_INFINTE: {
                    im_dir = 0;
                    re_dir = mpz_sgn(PMATH_AS_MPZ(mant));
                    if(re_dir == 0)
                      re_dir = 1;
                      
                    pmath_unref(mant);
                  } goto INFINITE_IM_RE;
              }
              
              pmath_file_write(file, data, size);
            }
            
            pmath_unref(value);
            pmath_unref(mant);
            return TRUE;
          }
          
          {
            pmath_t infdir = _pmath_directed_infinity_direction(value);
            pmath_t re = PMATH_NULL;
            pmath_t im = PMATH_NULL;
            
            if(_pmath_re_im(infdir, &re, &im)) {
              re_dir = 2;
              im_dir = 2;
              
              if(!pmath_is_number(re) || !pmath_is_number(im)) {
                re_dir = im_dir = 0;
              }
              else {
                if(pmath_equals(re, PMATH_FROM_INT32(-1)))
                  re_dir = -1;
                else if(pmath_equals(re, PMATH_FROM_INT32(0)))
                  re_dir = 0;
                else if(pmath_equals(re, PMATH_FROM_INT32(1)))
                  re_dir = 1;
                  
                if(pmath_equals(im, PMATH_FROM_INT32(-1)))
                  im_dir = -1;
                else if(pmath_equals(im, PMATH_FROM_INT32(0)))
                  im_dir = 0;
                else if(pmath_equals(im, PMATH_FROM_INT32(1)))
                  im_dir = 1;
              }
            }
            else if(pmath_same(value, pmath_System_Undefined))
              re_dir = im_dir = 0;
              
            pmath_unref(re);
            pmath_unref(im);
          }
          
        INFINITE_IM_RE:
          if(re_dir != 2 && im_dir != 2) {
            if(out_type == COMPLEX) {
              pmath_unref(value);
              pmath_unref(type);
              
              switch(size) {
                case 2:  type = PMATH_C_STRING("Real16"); break;
                case 4:  type = PMATH_C_STRING("Real32"); break;
                case 8:  type = PMATH_C_STRING("Real64"); break;
                default: type = PMATH_C_STRING("Real128");
              }
              
              binary_write(
                file,
                pmath_expr_new_extended(
                  pmath_ref(pmath_System_DirectedInfinity), 1,
                  PMATH_FROM_INT32(re_dir)),
                pmath_ref(type),
                byte_ordering);
                
              binary_write(
                file,
                pmath_expr_new_extended(
                  pmath_ref(pmath_System_DirectedInfinity), 1,
                  PMATH_FROM_INT32(re_dir)),
                type,
                byte_ordering);
                
              return TRUE;
            }
            
            if(out_type == REAL && im_dir == 0) {
              pmath_unref(value);
              pmath_unref(type);
              
              memset(data, 0, size);
              
              switch(size) {
                case 2: {
                    data[0] = 0x7C;
                  } break;
                  
                case 4: {
                    data[0] = 0x7F;
                    data[1] = 0x80;
                  } break;
                  
                case 8: {
                    data[0] = 0x7F;
                    data[1] = 0xF0;
                  } break;
                  
                default: {
                    data[0] = 0x7F;
                    data[1] = 0xFF;
                  }
              }
              
              if(re_dir == 0) { // Undefined = nan => mantissa != 0, lets put PMATH there :)
                switch(size) {
                  case 2: {
                      data[1] = 'P';
                    } break;
                    
                  case 4: {
                      data[3] = 'P';
                      data[2] = 'M';
                    } break;
                    
                  default: {
                      data[size - 1] = 'P';
                      data[size - 2] = 'M';
                      data[size - 3] = 'A';
                      data[size - 4] = 'T';
                      data[size - 5] = 'H';
                    }
                }
              }
              else if(re_dir < 0)
                data[0] |= 0x80;
                
              if(byte_ordering < 0) {
                size_t i;
                for(i = 0; i < size / 2; ++i) {
                  uint8_t tmp = data[i];
                  data[i] = data[size - i - 1];
                  data[size - i - 1] = tmp;
                }
              }
              
              pmath_file_write(file, data, size);
              
              return TRUE;
            }
          }
        }
        
        if(pmath_is_int32(value)) {
          value = _pmath_create_mp_int(PMATH_AS_INT32(value));
        }
        
        if(pmath_is_mpint(value)) {
          size_t count;
          pmath_mpint_t pos = PMATH_NULL;
          
          if( out_type == INT &&
              mpz_sgn(PMATH_AS_MPZ(value)) < 0)
          {
            pos = _pmath_create_mp_int(1);
            
            if(pmath_is_null(pos)) {
              pmath_unref(value);
              pmath_unref(type);
              return FALSE;
            }
            
            mpz_mul_2exp(PMATH_AS_MPZ(pos), PMATH_AS_MPZ(pos), size * 8);
            mpz_add(
              PMATH_AS_MPZ(pos),
              PMATH_AS_MPZ(pos),
              PMATH_AS_MPZ(value));
          }
          else if( (out_type == INT || out_type == UINT) &&
                   mpz_sgn(PMATH_AS_MPZ(value)) >= 0)
          {
            pos = pmath_ref(value);
          }
          else {
            if(pmath_is_null(type))
              type = PMATH_C_STRING("Byte");
              
            pmath_message(PMATH_NULL, "nocoerce", 2, _pmath_mp_int_normalize(value), type);
            return FALSE;
          }
          
          count = (mpz_sizeinbase(PMATH_AS_MPZ(pos), 2) + 7) / 8;
          
          if(count > size) {
            if(pmath_is_null(type))
              type = PMATH_C_STRING("Byte");
              
            pmath_unref(pos);
            pmath_message(PMATH_NULL, "nocoerce", 2, _pmath_mp_int_normalize(value), type);
            return FALSE;
          }
          
          memset(data, 0, size);
          mpz_export(
            data,
            NULL,
            byte_ordering,
            1,
            PMATH_BYTE_ORDER,
            0,
            PMATH_AS_MPZ(pos));
            
          pmath_file_write(file, data, size);
          pmath_unref(pos);
          pmath_unref(value);
          pmath_unref(type);
          
          return TRUE;
        }
      }
      
      if(out_type != NONE) {
        if(pmath_is_null(type))
          type = PMATH_C_STRING("Byte");
          
        pmath_message(PMATH_NULL, "nocoerce", 2, value, type);
        return FALSE;
      }
    }
  }
  
  pmath_message(PMATH_NULL, "format", 1, type);
  pmath_unref(value);
  return FALSE;
}

PMATH_PRIVATE pmath_t builtin_binarywrite(pmath_expr_t expr) {
  /* BinaryRead(file, value, type)
     BinaryRead(file, value)        = BinaryRead(file, value, "Byte")
   */
  pmath_expr_t options;
  pmath_t file, value, type;
  size_t last_nonoption;
  int byte_ordering = 0;
  
  if(pmath_expr_length(expr) < 2) {
    pmath_message_argxxx(0, 2, SIZE_MAX);
    return expr;
  }
  
  type = pmath_expr_get_item(expr, 3);
  if(pmath_is_null(type) ||pmath_is_set_of_options(type)) {
    pmath_unref(type);
    type = PMATH_NULL;
    last_nonoption = 2;
  }
  else
    last_nonoption = 3;
    
  options = pmath_options_extract(expr, last_nonoption);
  if(pmath_is_null(options)) {
    pmath_unref(type);
    return expr;
  }
  
  {
    byte_ordering = _pmath_get_byte_ordering(PMATH_NULL, options);
    
    if(!byte_ordering) {
      pmath_unref(expr);
      pmath_unref(type);
      pmath_unref(options);
      return pmath_ref(pmath_System_DollarFailed);
    }
  }
  
  file = pmath_expr_get_item(expr, 1);
  if(!_pmath_file_check(file, PMATH_FILE_PROP_WRITE | PMATH_FILE_PROP_BINARY)) {
    pmath_unref(file);
    pmath_unref(type);
    pmath_unref(expr);
    pmath_unref(options);
    return pmath_ref(pmath_System_DollarFailed);
  }
  
  value = pmath_expr_get_item(expr, 2);
  
  if(byte_ordering == 0)
    byte_ordering = PMATH_BYTE_ORDER;
    
  pmath_unref(expr);
  pmath_unref(options);
  
  // locking?
  if(!binary_write(file, value, type, byte_ordering)) {
    pmath_file_flush(file);
    pmath_unref(file);
    return pmath_ref(pmath_System_DollarFailed);
  }
  pmath_file_flush(file);
  
  return file;
}
