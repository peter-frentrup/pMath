#include <pmath-core/numbers-private.h>

#include <pmath-util/evaluation.h>
#include <pmath-util/files.h>
#include <pmath-util/helpers.h>
#include <pmath-util/memory.h>
#include <pmath-util/messages.h>
#include <pmath-util/serialize.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/arithmetic-private.h>
#include <pmath-builtins/control-private.h>
#include <pmath-builtins/io-private.h>

#include <limits.h>


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

  if(pmath_is_number(im)) {
    if(pmath_number_sign(im) == 0) {
      pmath_unref(im);
      return re;
    }

    if(pmath_is_number(re)) {
      return pmath_expr_new_extended(
               pmath_ref(PMATH_SYMBOL_COMPLEX), 2, re, im);
    }
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

        size = pmath_file_read(file, buf, sizeof(buf), TRUE);
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
                            i);

            pmath_file_read(file, buf, i + 1, FALSE);
            return TRUE;
          }
        }

        *type_value = pmath_string_insert_latin1(
                        *type_value,
                        INT_MAX,
                        buf,
                        size);

        pmath_file_read(file, buf, size, FALSE);
      }
    }
    else {
      size_t size = 0;
      union {
        uint8_t  buf[16];
        uint32_t buf32[4];
        double d;
        float f;
      } data;

      enum {
        NONE,
        UINT,
        INT,
        CHAR,
        REAL,
        COMPLEX
      } type = NONE;

      if( pmath_is_null(*type_value)                      ||
          pmath_string_equals_latin1(*type_value, "Byte") ||
          pmath_string_equals_latin1(*type_value, "UnsignedInteger8"))
      {
        size = 1;
        type = UINT;
      }
      else if(pmath_string_equals_latin1(*type_value, "Integer8")) {
        size = 1;
        type = INT;
      }
      else if(pmath_string_equals_latin1(*type_value, "UnsignedInteger16")) {
        size = 2;
        type = UINT;
      }
      else if(pmath_string_equals_latin1(*type_value, "Integer16")) {
        size = 2;
        type = INT;
      }
      else if(pmath_string_equals_latin1(*type_value, "UnsignedInteger24")) {
        size = 3;
        type = UINT;
      }
      else if(pmath_string_equals_latin1(*type_value, "Integer24")) {
        size = 3;
        type = INT;
      }
      else if(pmath_string_equals_latin1(*type_value, "UnsignedInteger32")) {
        size = 4;
        type = UINT;
      }
      else if(pmath_string_equals_latin1(*type_value, "Integer32")) {
        size = 4;
        type = INT;
      }
      else if(pmath_string_equals_latin1(*type_value, "UnsignedInteger64")) {
        size = 8;
        type = UINT;
      }
      else if(pmath_string_equals_latin1(*type_value, "Integer64")) {
        size = 8;
        type = INT;
      }
      else if(pmath_string_equals_latin1(*type_value, "UnsignedInteger128")) {
        size = 16;
        type = UINT;
      }
      else if(pmath_string_equals_latin1(*type_value, "Integer128")) {
        size = 16;
        type = INT;
      }
      else if(pmath_string_equals_latin1(*type_value, "Character8")) {
        size = 1;
        type = CHAR;
      }
      else if(pmath_string_equals_latin1(*type_value, "Character16")) {
        size = 2;
        type = CHAR;
      }
      else if(pmath_string_equals_latin1(*type_value, "Real16")) {
        size = 2;
        type = REAL;
      }
      else if(pmath_string_equals_latin1(*type_value, "Real32")) {
        size = 4;
        type = REAL;
      }
      else if(pmath_string_equals_latin1(*type_value, "Real64")) {
        size = 8;
        type = REAL;
      }
      else if(pmath_string_equals_latin1(*type_value, "Real128")) {
        size = 16;
        type = REAL;
      }
      else if(pmath_string_equals_latin1(*type_value, "Real128")) {
        size = 16;
        type = REAL;
      }
      else if(pmath_string_equals_latin1(*type_value, "Complex32")) {
        size = 2;
        type = COMPLEX;
      }
      else if(pmath_string_equals_latin1(*type_value, "Complex64")) {
        size = 4;
        type = COMPLEX;
      }
      else if(pmath_string_equals_latin1(*type_value, "Complex128")) {
        size = 8;
        type = COMPLEX;
      }
      else if(pmath_string_equals_latin1(*type_value, "Complex256")) {
        size = 16;
        type = COMPLEX;
      }

      if(type == COMPLEX) {
        pmath_t re;
        pmath_t im;

        switch(size) {
          case 2:  re = PMATH_C_STRING("Real16"); break;
          case 4:  re = PMATH_C_STRING("Real32"); break;
          case 8:  re = PMATH_C_STRING("Real64"); break;
          default: re = PMATH_C_STRING("Real128");
        }

        im = pmath_ref(re);

        pmath_unref(*type_value);
        *type_value = PMATH_NULL;

        if( binary_read(file, &re, byte_ordering) &&
            binary_read(file, &im, byte_ordering))
        {
          *type_value = make_complex(re, im);
          return TRUE;
        }

        pmath_unref(re);
        pmath_unref(im);
        return TRUE;
      }

      if(size) {
        pmath_unref(*type_value);
        *type_value = PMATH_NULL;

        if(pmath_file_read(file, &data, size, FALSE) < size) {
          *type_value = pmath_ref(PMATH_SYMBOL_ENDOFFILE);
          return TRUE;
        }

        if(type == INT) {
          if( (byte_ordering < 0 && (int8_t)data.buf[size - 1] < 0) ||
              (byte_ordering > 0 && (int8_t)data.buf[0]        < 0))
          {
            size_t i;
            for(i = 0; i < size; ++i) {
              data.buf[i] = ~data.buf[i];
            }
          }
          else
            type = UINT;
        }

        if(type == UINT || type == INT) {
          *type_value = pmath_integer_new_data(
                          size,
                          byte_ordering,
                          1,
                          PMATH_BYTE_ORDER,
                          0,
                          &data);

          if(type == INT)
            *type_value = pmath_number_neg(*type_value);
          return TRUE;
        }

        if(type == CHAR) {
          if(size == 2) {
            uint16_t chr;

            if(byte_ordering < 0)
              chr = (uint16_t)data.buf[0] | ((uint16_t)data.buf[1] << 8);
            else
              chr = (uint16_t)data.buf[1] | ((uint16_t)data.buf[0] << 8);

            *type_value = pmath_string_insert_ucs2(PMATH_NULL, 0, &chr, 2);
          }
          else
            *type_value = pmath_string_insert_latin1(PMATH_NULL, 0, (const char *)&data, 1);

          return TRUE;
        }

        if(type == REAL) {
          if(size == 16) {
            pmath_mpfloat_t f  = _pmath_create_mp_float(113);
            pmath_mpint_t mant = pmath_integer_new_data(
                                   14, // 112 / 8
                                   byte_ordering,
                                   1,
                                   PMATH_BYTE_ORDER,
                                   0,
                                   byte_ordering < 0 ? &data.buf[0] : &data.buf[2]);

            if(pmath_is_int32(mant))
              mant = _pmath_create_mp_int(PMATH_AS_INT32(mant));

            if(!pmath_is_null(f) && !pmath_is_null(mant)) {
              uint16_t uexp;
              pmath_bool_t neg;

              if(byte_ordering > 0) {
                uexp = ((data.buf[0] & 0x7F) << 8) | (data.buf[1]);
              }
              else {
                uexp = ((data.buf[15] & 0x7F) << 8) | (data.buf[14]);
              }

              if(byte_ordering > 0) {
                neg = (data.buf[0] & 0x80) != 0;
              }
              else {
                neg = (data.buf[15] & 0x80) != 0;
              }

              if(uexp == 0) {
                if(mpz_sgn(PMATH_AS_MPZ(mant)) == 0) {
                  mpfr_set_ui(PMATH_AS_MP_VALUE(f), 0, MPFR_RNDN);

                  mpfr_set_ui_2exp(PMATH_AS_MP_ERROR(f), 1, -16382 - 112, MPFR_RNDU);

                  *type_value = f;
                  pmath_unref(mant);
                }
                else {
                  mpfr_set_ui_2exp(PMATH_AS_MP_VALUE(f), 1, -112, MPFR_RNDU);
                  mpfr_mul_z(
                    PMATH_AS_MP_VALUE(f),
                    PMATH_AS_MP_VALUE(f),
                    PMATH_AS_MPZ(mant),
                    MPFR_RNDN);

                  mpfr_set_ui_2exp(PMATH_AS_MP_ERROR(f), 1, -16382 - 112, MPFR_RNDU);
                  mpfr_mul(
                    PMATH_AS_MP_ERROR(f),
                    PMATH_AS_MP_ERROR(f),
                    PMATH_AS_MP_VALUE(f),
                    MPFR_RNDU);

                  if(neg)
                    mpfr_neg(PMATH_AS_MP_VALUE(f), PMATH_AS_MP_VALUE(f), MPFR_RNDN);
                  *type_value = f;
                  pmath_unref(mant);
                }
              }
              else if(uexp == 0x7FFF) {
                if(mpz_sgn(PMATH_AS_MPZ(mant)) == 0) {
                  pmath_unref(f);
                  pmath_unref(mant);

                  if(neg) {
                    *type_value = pmath_expr_new_extended(
                                    pmath_ref(PMATH_SYMBOL_DIRECTEDINFINITY), 1,
                                    PMATH_FROM_INT32(-1));
                  }
                  else
                    *type_value = pmath_ref(_pmath_object_infinity);
                }
                else {
                  pmath_unref(f);
                  pmath_unref(mant);

                  *type_value = pmath_ref(PMATH_SYMBOL_UNDEFINED);
                }
              }
              else {
                mpz_setbit(PMATH_AS_MPZ(mant), 112);

                mpfr_set_ui_2exp(PMATH_AS_MP_VALUE(f), 1, ((int)uexp) - 16383 - 112, MPFR_RNDU);
                mpfr_mul_z(
                  PMATH_AS_MP_VALUE(f),
                  PMATH_AS_MP_VALUE(f),
                  PMATH_AS_MPZ(mant),
                  MPFR_RNDN);

                mpfr_set_ui_2exp(PMATH_AS_MP_ERROR(f), 1, -112, MPFR_RNDU);
                mpfr_mul(
                  PMATH_AS_MP_ERROR(f),
                  PMATH_AS_MP_ERROR(f),
                  PMATH_AS_MP_VALUE(f),
                  MPFR_RNDU);

                if(neg)
                  mpfr_neg(PMATH_AS_MP_VALUE(f), PMATH_AS_MP_VALUE(f), MPFR_RNDN);

                *type_value = f;
                pmath_unref(mant);
              }
            }
            else {
              pmath_unref(mant);
              pmath_unref(f);
            }
          }
          else if(size == 2) { // works only if double is ieee
            pmath_bool_t neg;
            uint8_t uexp;
            uint16_t mant;

            if(byte_ordering > 0) {
              neg = (data.buf[0] & 0x80) != 0;
              uexp = (data.buf[0] & 0x7C) >> 2;
              mant = (((uint16_t)data.buf[0] & 0x03) << 8) | (uint16_t)data.buf[1];
            }
            else {
              neg = (data.buf[1] & 0x80) != 0;
              uexp = (data.buf[0] & 0x7C) >> 2;
              mant = (((uint16_t)data.buf[1] & 0x03) << 8) | (uint16_t)data.buf[0];
            }

            if(uexp == 0) {
              if(mant == 0) {
                *type_value = PMATH_FROM_DOUBLE(/*neg ? -0.0 : */0.0);
              }
              else {
                double val = mant * 2e-24;

                if(neg)
                  val = -val;

                *type_value = PMATH_FROM_DOUBLE(val);
              }
            }
            else if(uexp == 0x1F) {
              if(mant == 0) {
                if(neg) {
                  *type_value = pmath_expr_new_extended(
                                  pmath_ref(PMATH_SYMBOL_DIRECTEDINFINITY), 1,
                                  PMATH_FROM_INT32(-1));
                }
                else
                  *type_value = pmath_ref(_pmath_object_infinity);
              }
              else {
                *type_value = pmath_ref(PMATH_SYMBOL_UNDEFINED);
              }
            }
            else {
              double val = pow(2, (int)uexp - 25);

              mant |= 0x400;
              val *= mant;

              if(neg)
                val = -val;

              *type_value = PMATH_FROM_DOUBLE(val);
            }
          }
          else { // works only if float and double are ieee
            double d;
            if(size == 8)
              d = data.d;
            else
              d = data.f;

            if(d == HUGE_VAL) {
              *type_value = pmath_ref(_pmath_object_infinity);
            }
            else if(d == -HUGE_VAL) {
              *type_value = pmath_expr_new_extended(
                              pmath_ref(PMATH_SYMBOL_DIRECTEDINFINITY), 1,
                              PMATH_FROM_INT32(-1));
            }
            else if(isnan(d)) {
              *type_value = pmath_ref(PMATH_SYMBOL_UNDEFINED);
            }
            else {
              if(d == 0) // signed 0?
                d = 0;
              *type_value = PMATH_FROM_DOUBLE(d);
            }
          }

          return TRUE;
        }
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
  if(pmath_is_null(type) || _pmath_is_rule(type) || _pmath_is_list_of_rules(type)) {
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

  {
    byte_ordering = _pmath_get_byte_ordering(PMATH_NULL, options);

    if(!byte_ordering) {
      pmath_unref(expr);
      pmath_unref(type);
      pmath_unref(options);
      return pmath_ref(PMATH_SYMBOL_FAILED);
    }
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
