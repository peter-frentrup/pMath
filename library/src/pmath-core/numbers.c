#include <pmath-core/numbers-private.h>
#include <pmath-core/objects-private.h>
#include <pmath-core/strings-private.h>

#include <pmath-language/number-parsing-private.h> // for _pmath_log2_of
#include <pmath-language/number-writing-private.h>
#include <pmath-language/tokens.h>

#include <pmath-util/approximate.h>
#include <pmath-util/concurrency/threadmsg.h>
#include <pmath-util/concurrency/threads-private.h>
#include <pmath-util/debug.h>
#include <pmath-util/hash/incremental-hash-private.h>
#include <pmath-util/memory.h>
#include <pmath-util/messages.h>
#include <pmath-util/strtod.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/number-theory-private.h>

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#ifdef PMATH_OS_WIN32
#  include <windows.h>
#elif defined(PMATH_OS_UNIX)
#  include <dlfcn.h>
#endif

#if __GNU_MP_VERSION < 4
#  error gmp version 4 or newer needed
#endif

#ifdef _MSC_VER
#  define snprintf sprintf_s
#endif

#define MPFR_MANT(x) ((x)->_mpfr_d)


extern pmath_symbol_t pmath_System_DirectedInfinity;
extern pmath_symbol_t pmath_System_Undefined;

PMATH_PRIVATE pmath_quotient_t _pmath_one_half; /* readonly */

PMATH_PRIVATE pmath_t _pmath_object_overflow;          /* readonly */
PMATH_PRIVATE pmath_t _pmath_object_underflow;         /* readonly */
PMATH_PRIVATE pmath_t _pmath_object_pos_infinity;      /* readonly */
PMATH_PRIVATE pmath_t _pmath_object_neg_infinity;      /* readonly */
PMATH_PRIVATE pmath_t _pmath_object_complex_infinity;  /* readonly */

PMATH_PRIVATE gmp_randstate_t  _pmath_randstate;
PMATH_PRIVATE pmath_atomic_t _pmath_rand_spinlock = PMATH_ATOMIC_STATIC_INIT;

#define CACHE_SIZE 256
#define CACHE_MASK (CACHE_SIZE-1)

/* Some implementation ideas:
   http://reference.wolfram.com/mathematica/note/SomeNotesOnInternalImplementation.html
 */

/*============================================================================*/
//{ caching unused integers ...

static pmath_atomic_t int_cache[CACHE_SIZE];
static pmath_atomic_t int_cache_pos = PMATH_ATOMIC_STATIC_INIT;

#ifdef PMATH_DEBUG_LOG
static pmath_atomic_t int_cache_hits   = PMATH_ATOMIC_STATIC_INIT;
static pmath_atomic_t int_cache_misses = PMATH_ATOMIC_STATIC_INIT;
#endif

static uintptr_t int_cache_inc(intptr_t delta) {
  return (uintptr_t)pmath_atomic_fetch_add(&int_cache_pos, delta);
}

static struct _pmath_mp_int_t *int_cache_swap(
  uintptr_t               i,
  struct _pmath_mp_int_t *value
) {
  i = i & CACHE_MASK;
  
  assert(!value || value->inherited.refcount._data == 0);
  
  return (struct _pmath_mp_int_t *)
         pmath_atomic_fetch_set(&int_cache[i], (intptr_t)value);
}

static void int_cache_clear(void) {
  uintptr_t i;
  
  for(i = 0; i < CACHE_SIZE; ++i) {
    struct _pmath_mp_int_t *integer = int_cache_swap(i, NULL);
    
    if(integer) {
      assert(integer->inherited.refcount._data == 0);
      
      mpz_clear(integer->value);
      pmath_mem_free(integer);
    }
  }
}

PMATH_PRIVATE pmath_mpint_t _pmath_create_mp_int(signed long value) {
  struct _pmath_mp_int_t *integer;
  
  uintptr_t i = int_cache_inc(-1);
  integer = int_cache_swap(i - 1, NULL);
  
  if(integer) {
#ifdef PMATH_DEBUG_LOG
    (void)pmath_atomic_fetch_add(&int_cache_hits, 1);
#endif
    
    assert(integer->inherited.refcount._data == 0);
    pmath_atomic_write_uint8_release( &integer->inherited.flags8,  0);
    pmath_atomic_write_uint16_release(&integer->inherited.flags16, 0);
    pmath_atomic_write_release(       &integer->inherited.refcount, 1);
    
    mpz_set_si(integer->value, value);
    
    return PMATH_FROM_PTR(integer);
  }
  else {
#ifdef PMATH_DEBUG_LOG
    (void)pmath_atomic_fetch_add(&int_cache_misses, 1);
#endif
  }
  
  integer = (void *)PMATH_AS_PTR(_pmath_create_stub(
                                   PMATH_TYPE_SHIFT_MP_INT,
                                   sizeof(struct _pmath_mp_int_t)));
                                   
  if(integer) {
    mpz_init_set_si(integer->value, value);
  }
  
  return PMATH_FROM_PTR(integer);
}

PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
// struct _pmath_quotient_t_ *
pmath_integer_t _pmath_integer_from_fmpz(const fmpz_t integer) {
  pmath_mpint_t result;
  
  if(fmpz_cmp_si(integer, INT32_MAX) <= 0 && fmpz_cmp_si(integer, INT32_MIN) >= 0) {
    slong value = fmpz_get_si(integer);
    return PMATH_FROM_INT32((int32_t)value);
  }
  
  result = _pmath_create_mp_int(0);
  if(!pmath_is_null(result))
    fmpz_get_mpz(PMATH_AS_MPZ(result), integer);
    
  return result;
}

//} ============================================================================
//{ creating quotients ...

PMATH_PRIVATE pmath_quotient_t _pmath_create_quotient(
  pmath_integer_t numerator,   // will be freed
  pmath_integer_t denominator  // will be freed
) {
  struct _pmath_quotient_t *quotient;
  
  if(pmath_is_null(numerator) || pmath_is_null(denominator)) {
    pmath_unref(numerator);
    pmath_unref(denominator);
    return PMATH_NULL;
  }
  
  quotient = (void *)PMATH_AS_PTR(_pmath_create_stub(
                                    PMATH_TYPE_SHIFT_QUOTIENT,
                                    sizeof(struct _pmath_quotient_t)));
                                    
  if(!quotient) {
    pmath_unref(numerator);
    pmath_unref(denominator);
    return PMATH_NULL;
  }
  
  quotient->numerator   = numerator;
  quotient->denominator = denominator;
  return PMATH_FROM_PTR(quotient);
}

PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
pmath_rational_t _pmath_rational_from_fmpq(const fmpq_t rational) {
  if(fmpz_is_one(fmpq_denref(rational)))
    return _pmath_integer_from_fmpz(fmpq_numref(rational));
    
  return _pmath_create_quotient(
           _pmath_integer_from_fmpz(fmpq_numref(rational)),
           _pmath_integer_from_fmpz(fmpq_denref(rational)));
}

//} ============================================================================
//{ caching unused mp floats ...

static pmath_atomic_t mp_cache[CACHE_SIZE];
static pmath_atomic_t mp_cache_pos = PMATH_ATOMIC_STATIC_INIT;

#ifdef PMATH_DEBUG_LOG
static pmath_atomic_t mp_cache_hits   = PMATH_ATOMIC_STATIC_INIT;
static pmath_atomic_t mp_cache_misses = PMATH_ATOMIC_STATIC_INIT;
#endif

static uintptr_t mp_cache_inc(intptr_t delta) {
  return (uintptr_t)pmath_atomic_fetch_add(&mp_cache_pos, delta);
}

static struct _pmath_mp_float_t *mp_cache_swap(
  uintptr_t                 i,
  struct _pmath_mp_float_t *f
) {
  i = i & CACHE_MASK;
  
  return (void *)pmath_atomic_fetch_set(&mp_cache[i], (intptr_t)f);
}

static void mp_cache_clear(void) {
  uintptr_t i;
  
  for(i = 0; i < CACHE_SIZE; ++i) {
    struct _pmath_mp_float_t *f = mp_cache_swap(i, NULL);
    
    if(f) {
      assert(f->inherited.refcount._data == 0);
      
      arb_clear(f->value_new);
      pmath_mem_free(f);
    }
  }
}

PMATH_PRIVATE pmath_float_t _pmath_create_mp_float(slong precision) {
  struct _pmath_mp_float_t *f;
  uintptr_t i;
  
  /*if(precision == 0)
    precision = mpfr_get_default_prec();
  else */if(precision < MPFR_PREC_MIN)
    precision = MPFR_PREC_MIN;
  else if(precision > PMATH_MP_PREC_MAX) // MPFR_PREC_MAX is too big! (not enough memory)
    precision =       PMATH_MP_PREC_MAX; // overflow error message?
    
  i = mp_cache_inc(-1);
  f = mp_cache_swap(i - 1, NULL);
  if(f) {
#ifdef PMATH_DEBUG_LOG
    (void)pmath_atomic_fetch_add(&mp_cache_hits, 1);
#endif
    
    assert(f->inherited.refcount._data == 0);
    pmath_atomic_write_uint8_release( &f->inherited.flags8,  0);
    pmath_atomic_write_uint16_release(&f->inherited.flags16, 0);
    pmath_atomic_write_release(       &f->inherited.refcount, 1);
    
    f->working_precision = precision;
    return PMATH_FROM_PTR(f);
  }
  else {
#ifdef PMATH_DEBUG_LOG
    (void)pmath_atomic_fetch_add(&mp_cache_misses, 1);
#endif
  }
  
  f = (void *)PMATH_AS_PTR(_pmath_create_stub(
                             PMATH_TYPE_SHIFT_MP_FLOAT,
                             sizeof(struct _pmath_mp_float_t)));
                             
  if(!f)
    return PMATH_NULL;
    
  arb_init(f->value_new);
  f->working_precision = (slong)precision;
  
  return PMATH_FROM_PTR(f);
}

PMATH_PRIVATE
pmath_float_t _pmath_create_mp_float_from_d(double value) {
  pmath_float_t result = _pmath_create_mp_float(DBL_MANT_DIG);
  
  if(PMATH_LIKELY(!pmath_is_null(result)))
    arb_set_d(PMATH_AS_ARB(result), value);
  
  return result;
}

PMATH_PRIVATE
pmath_mpfloat_t _pmath_create_mp_float_from_q(pmath_rational_t value, slong precision) {
  pmath_mpfloat_t result = _pmath_create_mp_float(precision);
  
  if(!pmath_is_null(result)) {
    if(pmath_is_int32(value)) {
      arb_set_si(PMATH_AS_ARB(result), PMATH_AS_INT32(value));
      arb_set_round(PMATH_AS_ARB(result), PMATH_AS_ARB(result), precision);
    }
    else if(pmath_is_mpint(value)) {
      fmpz_t tmp;
      fmpz_init(tmp);
      fmpz_set_mpz(tmp, PMATH_AS_MPZ(value));
      
      arb_set_round_fmpz(PMATH_AS_ARB(result), tmp, precision);
      
      fmpz_clear(tmp);
    }
    else {
      fmpz_t tmp_num;
      fmpz_t tmp_den;
      fmpz_init(tmp_num);
      fmpz_init(tmp_den);
      
      assert(pmath_is_quotient(value));
      
      if(pmath_is_int32(PMATH_QUOT_NUM(value))) {
        fmpz_set_si(tmp_num, PMATH_AS_INT32(PMATH_QUOT_NUM(value)));
      }
      else {
        assert(pmath_is_mpint(PMATH_QUOT_NUM(value)));
        fmpz_set_mpz(tmp_num, PMATH_AS_MPZ(PMATH_QUOT_NUM(value)));
      }
      
      if(pmath_is_int32(PMATH_QUOT_DEN(value))) {
        fmpz_set_si(tmp_den, PMATH_AS_INT32(PMATH_QUOT_DEN(value)));
      }
      else {
        assert(pmath_is_mpint(PMATH_QUOT_DEN(value)));
        fmpz_set_mpz(tmp_den, PMATH_AS_MPZ(PMATH_QUOT_DEN(value)));
      }
      
      arb_fmpz_div_fmpz(PMATH_AS_ARB(result), tmp_num, tmp_den, precision);
      
      fmpz_clear(tmp_num);
      fmpz_clear(tmp_den);
    }
  }
  
  pmath_unref(value);
  return result;
}

PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
pmath_mpfloat_t _pmath_create_mp_float_from_midrad_arb(arb_t mid, arb_t rad, slong prec) {
  pmath_mpfloat_t result;
  
  result = _pmath_create_mp_float(prec);
  
  if(PMATH_LIKELY(!pmath_is_null(result))) {
    arb_set(PMATH_AS_ARB(result), mid);
    _pmath_arb_add_error_exact(PMATH_AS_ARB(result), rad);
  }
  return result;
}

PMATH_PRIVATE
pmath_float_t _pmath_convert_to_mp_float(pmath_float_t n) { // n will be freed
  pmath_float_t result;
  
  if(pmath_is_mpfloat(n))
    return n;
    
  result = _pmath_create_mp_float_from_d(pmath_number_get_d(n));
  pmath_unref(n);
  
  return result;
}

//} ============================================================================
//{ integer constructors ...

PMATH_PRIVATE
pmath_integer_t _pmath_mp_int_normalize(pmath_mpint_t integer) {
  assert(pmath_is_mpint(integer));
  
  if(mpz_fits_sint_p(PMATH_AS_MPZ(integer))) {
    int i = (int)mpz_get_si(PMATH_AS_MPZ(integer));
    
    if(i == (int32_t)i) { // in case of sizeof(int) > sizeof(int32_t)
      pmath_unref(integer);
      return PMATH_FROM_INT32((int32_t)i);
    }
  }
  
  return integer;
}

PMATH_API pmath_integer_t pmath_integer_new_ui32_slow(uint32_t ui) {
  pmath_mpint_t integer;
  if(ui <= INT32_MAX)
    return PMATH_FROM_INT32((int32_t)ui);
    
  integer = _pmath_create_mp_int(0);
  if(pmath_is_null(integer))
    return PMATH_NULL;
    
  mpz_import(
    PMATH_AS_MPZ(integer),
    1,
    -1,
    sizeof(uint32_t),
    0,
    0,
    &ui);
    
  return integer;
}

PMATH_API pmath_integer_t pmath_integer_new_si64_slow(int64_t si) {
  pmath_mpint_t integer;
  
  if(INT32_MIN <= si && si <= INT32_MAX)
    return PMATH_FROM_INT32((int32_t)si);
    
  integer = _pmath_create_mp_int(0);
  if(pmath_is_null(integer))
    return PMATH_NULL;
    
  if(si < 0) {
    if(si == INT64_MIN) { // -2^63
      mpz_set_si(PMATH_AS_MPZ(integer), -1);
      mpz_mul_2exp(PMATH_AS_MPZ(integer), PMATH_AS_MPZ(integer), 63);
    }
    else {
      si = -si;
      mpz_import(
        PMATH_AS_MPZ(integer),
        1,
        -1,
        sizeof(int64_t),
        0,
        0,
        &si);
      mpz_neg(PMATH_AS_MPZ(integer), PMATH_AS_MPZ(integer));
    }
  }
  else {
    mpz_import(
      PMATH_AS_MPZ(integer),
      1,
      -1,
      sizeof(int64_t),
      0,
      0,
      &si);
  }
  
  return integer;
}

PMATH_API pmath_integer_t pmath_integer_new_ui64_slow(uint64_t ui) {
  pmath_mpint_t integer;
  if(ui <= INT32_MAX)
    return PMATH_FROM_INT32((int32_t)ui);
    
  integer = _pmath_create_mp_int(0);
  if(pmath_is_null(integer))
    return PMATH_NULL;
    
  mpz_import(
    PMATH_AS_MPZ(integer),
    1,
    -1,
    sizeof(uint64_t),
    0,
    0,
    &ui);
    
  return integer;
}

PMATH_API
PMATH_ATTRIBUTE_USE_RESULT
pmath_integer_t pmath_integer_new_data(
  size_t       count,
  int          order,
  int          size,
  int          endian,
  size_t       nails,
  const void  *data
) {
  pmath_mpint_t integer = _pmath_create_mp_int(0);
  
  if(pmath_is_null(integer))
    return PMATH_NULL;
    
  mpz_import(
    PMATH_AS_MPZ(integer),
    count,
    order,
    size,
    endian,
    nails,
    data);
    
  return _pmath_mp_int_normalize(integer);
}

PMATH_API pmath_integer_t pmath_integer_new_str(const char *str, int base) {
  pmath_mpint_t integer = _pmath_create_mp_int(0);
  
  if(pmath_is_null(integer))
    return PMATH_NULL;
    
  if(mpz_set_str(PMATH_AS_MPZ(integer), str, base)) {
    pmath_unref(integer);
    return PMATH_NULL;
  }
  
  return _pmath_mp_int_normalize(integer);
}

//} ============================================================================
//{ quotient constructor and access functions ...

PMATH_API pmath_rational_t pmath_rational_new(
  pmath_integer_t  numerator,
  pmath_integer_t  denominator
) {
  if(pmath_is_null(numerator) || pmath_is_null(denominator)) {
    pmath_unref(numerator);
    pmath_unref(denominator);
    return PMATH_NULL;
  }
  
  assert(pmath_is_integer(numerator));
  assert(pmath_is_integer(denominator));
  
  if(pmath_number_sign(denominator) == 0) { // pmath_number_sign(PMATH_NULL) = 0
    pmath_unref(numerator);
    pmath_unref(denominator);
    return PMATH_NULL;
  }
  
  // fast check: n/1 -> n
  if(pmath_same(denominator, PMATH_FROM_INT32(1)))
    return numerator;
    
  //{ canonical form ...
  if(pmath_is_int32(numerator)) {
    numerator = _pmath_create_mp_int(PMATH_AS_INT32(numerator));
    
    if(pmath_is_null(numerator)) {
      pmath_unref(denominator);
      return PMATH_NULL;
    }
  }
  else if(pmath_refcount(numerator) > 1) {
    pmath_integer_t unique_num = _pmath_create_mp_int(0);
    
    if(pmath_is_null(unique_num)) {
      pmath_unref(numerator);
      pmath_unref(denominator);
      return PMATH_NULL;
    }
    
    mpz_set(PMATH_AS_MPZ(unique_num), PMATH_AS_MPZ(numerator));
    pmath_unref(numerator);
    numerator = unique_num;
  }
  
  if(pmath_is_int32(denominator)) {
    denominator = _pmath_create_mp_int(PMATH_AS_INT32(denominator));
    
    if(pmath_is_null(denominator)) {
      pmath_unref(numerator);
      return PMATH_NULL;
    }
  }
  else if(pmath_refcount(denominator) > 1) {
    pmath_mpint_t unique_den = _pmath_create_mp_int(0);
    
    if(pmath_is_null(unique_den)) {
      pmath_unref(numerator);
      pmath_unref(denominator);
      return PMATH_NULL;
    }
    
    mpz_set(PMATH_AS_MPZ(unique_den), PMATH_AS_MPZ(denominator));
    pmath_unref(denominator);
    denominator = unique_den;
  }
  
  {
    mpq_t  tmp_mpq;
    mpz_swap(mpq_numref(tmp_mpq), PMATH_AS_MPZ(numerator));
    mpz_swap(mpq_denref(tmp_mpq), PMATH_AS_MPZ(denominator));
    
    mpq_canonicalize(tmp_mpq);
    
    mpz_swap(mpq_numref(tmp_mpq), PMATH_AS_MPZ(numerator));
    mpz_swap(mpq_denref(tmp_mpq), PMATH_AS_MPZ(denominator));
  }
  //}
  
  // check again canonical form: n/1 -> n
  if(mpz_cmp_ui(PMATH_AS_MPZ(denominator), 1) == 0) {
    pmath_unref(denominator);
    return _pmath_mp_int_normalize(numerator);
  }
  
  return _pmath_create_quotient(
           _pmath_mp_int_normalize(numerator),
           _pmath_mp_int_normalize(denominator));
}

PMATH_API pmath_integer_t pmath_rational_numerator(
  pmath_rational_t rational
) {
  if(pmath_is_null(rational))
    return PMATH_NULL;
    
  if(pmath_is_integer(rational))
    return pmath_ref(rational);
    
  assert(pmath_is_quotient(rational));
  
  return pmath_ref(PMATH_QUOT_NUM(rational));
}

PMATH_API pmath_integer_t pmath_rational_denominator(
  pmath_rational_t rational
) {
  if(pmath_is_null(rational))
    return PMATH_NULL;
    
  if(pmath_is_integer(rational))
    return PMATH_FROM_INT32(1);
    
  assert(pmath_is_quotient(rational));
  
  return pmath_ref(PMATH_QUOT_DEN(rational));
}

//} ============================================================================
//{ float constructors and exception handling ...

/**\brief Parse a floating point number string to integer mantissa and exponents.
   \param mantissa  Gets the mantissa.
   \param exponent  Gets the exponent.
   \param str       A string representing the number. The format is
                    `(+|-)?<basedigits>[.<basedigits>][<exp>(+|-)?<decimaldigits>]`
                    where the exponent prefix <exp> may be `*^`, `@` or, for bases
                    up to 10, `e` or `E`.
                    The exponent is always in decimal digits.
   \param base      The base, between 2 and 36 (inclusive).
   \return The total number of digits in the mantissa (ignoring a possible signle
           leading zero digit) or -1 on error.
 */
static slong parse_number_to_mantissa_and_exponent(fmpz_t mantissa, fmpz_t exponent, const char *str, int base) {
  const char *exp_mark;
  const char *exp_start;
  char *buffer;
  slong int_digits, frac_digits, i, digits;
  int frac_indicator;
  
  if(base < 2 || base > 36)
    return -1;
    
  if(*str == '+')
    return parse_number_to_mantissa_and_exponent(mantissa, exponent, str + 1, base);
    
  if(*str == '-') {
    digits = parse_number_to_mantissa_and_exponent(mantissa, exponent, str + 1, base);
    fmpz_neg(mantissa, mantissa);
    return digits;
  }
  
  buffer = pmath_mem_alloc(strlen(str) + 1);
  if(!buffer)
    return -1;
    
  if(base <= 10) {
    exp_mark = strchr(str, 'e');
    if(!exp_mark)
      exp_mark = strchr(str, 'E');
    if(!exp_mark)
      exp_mark = strchr(str, '@');
  }
  else
    exp_mark = strchr(str, '@');
    
  if(exp_mark) {
    exp_start = exp_mark + 1;
  }
  else {
    exp_mark = strstr(str, "*^");
    if(exp_mark)
      exp_start = exp_mark + 2;
  }
  
  if(exp_mark) {
    if(exp_start[0] == '+') {
      if(fmpz_set_str(exponent, exp_start + 1, base) != 0)
        goto FAIL;
    }
    else {
      if(fmpz_set_str(exponent, exp_start, base) != 0)
        goto FAIL;
    }
  }
  else
    fmpz_set_si(exponent, 0);
    
  int_digits = 0;
  frac_digits = 0;
  frac_indicator = 0;
  for(i = 0; str + i != exp_mark && str[i]; ++i) {
    if(str[i] == '.' && !frac_indicator) {
      frac_indicator = 1;
    }
    else if(pmath_char_is_basedigit(base, str[i])) {
      buffer[int_digits + frac_digits] = str[i];
      frac_digits += frac_indicator;
      int_digits += !frac_indicator; // !0 = 1
    }
    else
      goto FAIL;
  }
  
  digits = int_digits + frac_digits;
  if(str[0] == '0')
    --digits;
    
  buffer[int_digits + frac_digits] = '\0';
  while(int_digits + frac_digits > 1 && buffer[int_digits + frac_digits - 1] == '0') {
    buffer[int_digits + frac_digits - 1] = '\0';
    --frac_digits;
  }
  
  fmpz_sub_si(exponent, exponent, frac_digits);
  if(fmpz_set_str(mantissa, buffer, base) != 0)
    goto FAIL;
    
  pmath_mem_free(buffer);
  return digits;
FAIL:
  pmath_mem_free(buffer);
  return -1;
}

/**\brief Calculate an upper bound for  num_digits*log_2(base)
 */
static slong bits_for_basedigits(int base, arb_t num_digits) {
  slong bits;
  arb_t tmp;
  mag_t tmp2;
  arb_init(tmp);
  mag_init(tmp2);
  
  arb_set_si(tmp, base);
  arb_pow(tmp, tmp, num_digits, 10);
  arb_get_mag(tmp2, tmp);
  bits = 1 + fmpz_get_si(MAG_EXPREF(tmp2));
  
  arb_clear(tmp);
  mag_clear(tmp2);
  return bits;
}

static pmath_number_t parse_auto_prec(const char *str, int base, slong bit_prec) {
  pmath_number_t result = PMATH_NULL;
  fmpz_t mantissa;
  fmpz_t exponent;
  arb_t tmp;
  slong digits;
  
  fmpz_init(mantissa);
  fmpz_init(exponent);
  arb_init(tmp);
  
  digits = parse_number_to_mantissa_and_exponent(mantissa, exponent, str, base);
  if(digits < 0)
    goto CLEANUP;
    
  if(bit_prec < 0) {
    arb_set_si(tmp, digits);
    bit_prec = bits_for_basedigits(base, tmp);
    if(bit_prec <= DBL_MANT_DIG)
      bit_prec = DBL_MANT_DIG;
  }
  
  result = _pmath_create_mp_float(bit_prec);
  if(pmath_is_null(result))
    goto CLEANUP;
    
  if(fmpz_is_zero(mantissa)) {
    arb_zero(PMATH_AS_ARB(result));
  }
  else if(fmpz_is_zero(exponent)) {
    arb_set_round_fmpz(PMATH_AS_ARB(result), mantissa, bit_prec);
  }
  else {
    arb_set_si(tmp, base);
    arb_set_fmpz(PMATH_AS_ARB(result), mantissa);
    
    if(fmpz_sgn(exponent) > 0) {
      arb_pow_fmpz_binexp(tmp, tmp, exponent, bit_prec + 4);
      arb_mul(PMATH_AS_ARB(result), PMATH_AS_ARB(result), tmp, bit_prec);
    }
    else {
      fmpz_neg(exponent, exponent);
      arb_pow_fmpz_binexp(tmp, tmp, exponent, bit_prec + 4);
      arb_div(PMATH_AS_ARB(result), PMATH_AS_ARB(result), tmp, bit_prec);
    }
  }
  
CLEANUP:
  arb_clear(tmp);
  fmpz_clear(mantissa);
  fmpz_clear(exponent);
  return result;
}

static pmath_number_t to_double_if_low_prec(pmath_t number) {
  if(!pmath_is_mpfloat(number))
    return number;
    
  if(PMATH_AS_ARB_WORKING_PREC(number) <= DBL_MANT_DIG) {
    double d = arf_get_d(arb_midref(PMATH_AS_ARB(number)), ARF_RND_NEAR);
    if(isfinite(d)) {
      pmath_unref(number);
      return PMATH_FROM_DOUBLE(d);
    }
  }
  
  return number;
}

PMATH_API
pmath_number_t pmath_float_new_str(
  const char               *str, // digits.digits
  int                       base,
  pmath_precision_control_t precision_control,
  double                    base_precision_accuracy
) {
  if(base < 2 || base > 36)
    return PMATH_NULL;
    
  switch(precision_control) {
    case PMATH_PREC_CTRL_AUTO:
      return to_double_if_low_prec(parse_auto_prec(str, base, -1));
      
    case PMATH_PREC_CTRL_MACHINE_PREC:
      return to_double_if_low_prec(parse_auto_prec(str, base, DBL_MANT_DIG));
      
    case PMATH_PREC_CTRL_GIVEN_PREC: {
        slong bit_prec;
        pmath_mpfloat_t result;
        arb_t tmp;
        arb_init(tmp);
        
        arb_set_d(tmp, base_precision_accuracy);
        bit_prec = bits_for_basedigits(base, tmp);
        result = parse_auto_prec(str, base, bit_prec);
        
        arb_clear(tmp);
        return result;
      }
      
    // TODO: support creating an exact quotient for "infinite" precision
    
    case PMATH_PREC_CTRL_GIVEN_ACC: // TODO: PMATH_PREC_CTRL_GIVEN_ACC is not supported, delete it.
    default:
      return PMATH_NULL;
  }
}

static int digit_value(char c) {
  if(c >= '0' && c <= '9')
    return c - '0';
  if(c >= 'A' && c <= 'Z')
    return c - 'A' + 10;
  if(c >= 'a' && c <= 'z')
    return c - 'a' + 10;
  return -1;
}

PMATH_API
const char *pmath_rational_parse_floating_point(
  pmath_integer_t *out_mantissa,
  intptr_t        *out_factional_digits,
  const char      *str,
  int              base
) {
  fmpz_t mantissa;
  
  assert(out_mantissa         != NULL);
  assert(out_factional_digits != NULL);
  assert(str                  != NULL);
  assert(2 <= base);
  assert(base <= 36);
  
  fmpz_init(mantissa); // setting to zero
  
  while(*str) {
    int digit = digit_value(*str);
    if(digit < 0 || digit >= base)
      break;
      
    fmpz_mul_si(mantissa, mantissa, base);
    fmpz_add_si(mantissa, mantissa, digit);
    ++str;
  }
  
  *out_factional_digits = 0;
  if(*str == '.' && digit_value(*str + 1) >= 0 && digit_value(*str + 1) < base) {
    ++str;
    while(*str) {
      int digit = digit_value(*str);
      if(digit < 0 || digit >= base)
        break;
        
      ++*out_factional_digits;
      fmpz_mul_si(mantissa, mantissa, base);
      fmpz_add_si(mantissa, mantissa, digit);
      ++str;
    }
  }
  
  *out_mantissa = _pmath_integer_from_fmpz(mantissa);
  fmpz_clear(mantissa);
  return str;
}


PMATH_PRIVATE
pmath_t _pmath_float_exceptions(
  pmath_number_t x  // will be freed.
) {
  pmath_t result = PMATH_NULL;
  
  if(pmath_is_double(x)) {
    if(!isfinite(PMATH_AS_DOUBLE(x))) {
      pmath_message(PMATH_NULL, "ovfl", 0);
      pmath_unref(x);
      return pmath_ref(_pmath_object_overflow);
    }
    
    return x;
  }
  
  if(!pmath_is_mpfloat(x))
    return x;
    
  if(arb_is_finite(PMATH_AS_ARB(x))) 
    return x;
  
  if(arf_is_nan(arb_midref(PMATH_AS_ARB(x)))) {
    result = pmath_ref(pmath_System_Undefined);
    pmath_message(PMATH_NULL, "indet", 1, pmath_ref(result));
  }
  else {
    int inf_sign;
    if(!mag_is_finite(arb_radref(PMATH_AS_ARB(x))))
      inf_sign = 0;
    else 
      inf_sign = arf_sgn(arb_midref(PMATH_AS_ARB(x)));
      
    result = pmath_expr_new_extended(
               pmath_ref(pmath_System_DirectedInfinity), 1,
               PMATH_FROM_INT32(inf_sign));
    pmath_message(PMATH_NULL, "infy", 1, pmath_ref(result));
  }
  
  pmath_unref(x);
  return result;
}

//}
//{ number conversion functions ...

PMATH_API pmath_bool_t pmath_integer_fits_ui32(pmath_integer_t integer) {
  if(pmath_is_int32(integer))
    return PMATH_AS_INT32(integer) >= 0;
    
  if(pmath_is_null(integer))
    return FALSE;
    
  return mpz_fits_ulong_p(PMATH_AS_MPZ(integer));
}

PMATH_API pmath_bool_t pmath_integer_fits_si64(pmath_integer_t integer) {
  size_t size;
  
  if(pmath_is_int32(integer))
    return TRUE;
    
  if(pmath_is_null(integer))
    return FALSE;
    
  size = mpz_sizeinbase(PMATH_AS_MPZ(integer), 2);
  if(size < 63)
    return TRUE;
    
  if(size == 63)
    return mpz_sgn(PMATH_AS_MPZ(integer)) < 0;
    
  return FALSE;
}

PMATH_API pmath_bool_t pmath_integer_fits_ui64(pmath_integer_t integer) {
  size_t size;
  
  if(pmath_is_int32(integer))
    return PMATH_AS_INT32(integer) >= 0;
    
  if(pmath_is_null(integer))
    return FALSE;
    
  size = mpz_sizeinbase(PMATH_AS_MPZ(integer), 2);
  return (size <= 64 && mpz_sgn(PMATH_AS_MPZ(integer)) >= 0);
}

PMATH_API int32_t pmath_integer_get_si32(pmath_integer_t integer) {
  if(pmath_is_int32(integer))
    return PMATH_AS_INT32(integer);
    
  return 0;
}

PMATH_API uint32_t pmath_integer_get_ui32(pmath_integer_t integer) {
  if(pmath_is_int32(integer)) {
    if(PMATH_AS_INT32(integer) >= 0)
      return PMATH_AS_INT32(integer);
      
    if(PMATH_AS_INT32(integer) == INT32_MIN)
      return 0;
      
    return - PMATH_AS_INT32(integer);
  }
  
  if(pmath_is_null(integer))
    return 0;
    
  return (uint32_t)mpz_get_ui(PMATH_AS_MPZ(integer));
}

PMATH_API
PMATH_ATTRIBUTE_PURE
int64_t pmath_integer_get_si64(pmath_integer_t integer) {
  size_t size;
  
  if(pmath_is_int32(integer))
    return PMATH_AS_INT32(integer);
    
  if(pmath_is_null(integer))
    return 0;
    
  size = mpz_sizeinbase(PMATH_AS_MPZ(integer), 8);
  if(size <= sizeof(uint64_t)) {
    uint64_t val = 0;
    mpz_export(&val, NULL, 1, sizeof(uint64_t), 0, 0, PMATH_AS_MPZ(integer));
    
    if(mpz_sgn(PMATH_AS_MPZ(integer)) < 0)
      return (int64_t)(-val);
    return (int64_t)val;
  }
  return 0;
}

PMATH_API
PMATH_ATTRIBUTE_PURE
uint64_t pmath_integer_get_ui64(pmath_integer_t integer) {
  size_t size;
  
  if(pmath_is_int32(integer)) {
    if(PMATH_AS_INT32(integer) >= 0)
      return PMATH_AS_INT32(integer);
      
    return (uint64_t)(-(int64_t)PMATH_AS_INT32(integer));
  }
  
  if(pmath_is_null(integer))
    return 0;
    
  size = mpz_sizeinbase(PMATH_AS_MPZ(integer), 8);
  if(size <= sizeof(uint64_t)) {
    uint64_t val = 0;
    mpz_export(&val, NULL, 1, sizeof(uint64_t), 0, 0, PMATH_AS_MPZ(integer));
    
    return val;
  }
  return 0;
}

PMATH_API double pmath_number_get_d(pmath_number_t number) {
  if(pmath_is_double(number))
    return PMATH_AS_DOUBLE(number);
    
  if(pmath_is_int32(number))
    return PMATH_AS_INT32(number);
    
  assert(pmath_is_pointer(number));
  
  if(pmath_is_null(number))
    return 0.0;
    
  switch(PMATH_AS_PTR(number)->type_shift) {
    case PMATH_TYPE_SHIFT_MP_INT:
      return mpz_get_d(PMATH_AS_MPZ(number));
      
    case PMATH_TYPE_SHIFT_QUOTIENT:
      return pmath_number_get_d(PMATH_QUOT_NUM(number))
             / pmath_number_get_d(PMATH_QUOT_DEN(number));
             
    case PMATH_TYPE_SHIFT_MP_FLOAT:
      return arf_get_d(arb_midref(PMATH_AS_ARB(number)), ARF_RND_NEAR);
  }
  
  assert("invalid number type" && 0);
  return 0.0;
}

PMATH_PRIVATE
void _pmath_integer_get_fmpz(fmpz_t result, pmath_integer_t integer) {
  if(pmath_is_int32(integer)) {
    fmpz_set_si(result, PMATH_AS_INT32(integer));
    return;
  }
  
  assert(pmath_is_mpint(integer));
  fmpz_set_mpz(result, PMATH_AS_MPZ(integer));
}

PMATH_PRIVATE
void _pmath_rational_get_fmpq(fmpq_t result, pmath_rational_t rational) {
  if(pmath_is_quotient(rational)) {
    _pmath_integer_get_fmpz(fmpq_numref(result), PMATH_QUOT_NUM(rational));
    _pmath_integer_get_fmpz(fmpq_denref(result), PMATH_QUOT_DEN(rational));
    return;
  }
  
  assert(pmath_is_integer(rational));
  _pmath_integer_get_fmpz(fmpq_numref(result), rational);
  fmpz_set_ui(fmpq_denref(result), 1);
}

PMATH_PRIVATE
void _pmath_number_get_arb(arb_t result, pmath_number_t real, slong precision) {
  if(pmath_is_int32(real)) {
    arb_set_si(result, PMATH_AS_INT32(real));
    return;
  }
  
  if(pmath_is_mpint(real)) {
    fmpz_t tmp;
    fmpz_init(tmp);
    fmpz_set_mpz(tmp, PMATH_AS_MPZ(real));
    arb_set_fmpz(result, tmp);
    fmpz_clear(tmp);
    return;
  }
  
  if(pmath_is_quotient(real)) {
    fmpq_t tmp;
    fmpq_init(tmp);
    _pmath_integer_get_fmpz(fmpq_numref(tmp), PMATH_QUOT_NUM(real));
    _pmath_integer_get_fmpz(fmpq_denref(tmp), PMATH_QUOT_DEN(real));
    arb_set_fmpq(result, tmp, precision);
    fmpq_clear(tmp);
    return;
  }
  
  if(pmath_is_double(real)) {
    arb_set_d(result, PMATH_AS_DOUBLE(real));
    return;
  }
  
  assert(pmath_is_mpfloat(real));
  arb_set(result, PMATH_AS_ARB(real));
}

//} ============================================================================
//{ general number functions ...

PMATH_API int pmath_number_sign(pmath_number_t num) {
  if(pmath_is_double(num)) {
    if(PMATH_AS_DOUBLE(num) < 0)
      return -1;
    if(PMATH_AS_DOUBLE(num) > 0)
      return 1;
    return 0;
  }
  
  if(pmath_is_int32(num)) {
    if(PMATH_AS_INT32(num) < 0)
      return -1;
    if(PMATH_AS_INT32(num) > 0)
      return 1;
    return 0;
  }
  
  assert(pmath_is_pointer(num));
  if(pmath_is_null(num))
    return 0;
    
  switch(PMATH_AS_PTR(num)->type_shift) {
    case PMATH_TYPE_SHIFT_MP_INT:
      return mpz_sgn(PMATH_AS_MPZ(num));
      
    case PMATH_TYPE_SHIFT_QUOTIENT:
      return pmath_number_sign(PMATH_QUOT_NUM(num));
      
    case PMATH_TYPE_SHIFT_MP_FLOAT:
      if(arb_is_positive(PMATH_AS_ARB(num)))
        return 1;
      if(arb_is_negative(PMATH_AS_ARB(num)))
        return -1;
      return 0;
  }
  
  assert("invalid number type" && 0);
  return 0;
}

static pmath_integer_t _neg_i(
  pmath_integer_t integer // will be freed. not PMATH_NULL!
) {
  pmath_mpint_t result;
  
  if(pmath_is_int32(integer)) {
    int32_t i = PMATH_AS_INT32(integer);
    
    if(i != INT32_MIN)
      return PMATH_FROM_INT32(-i);
      
    result = _pmath_create_mp_int(i);
    if(!pmath_is_null(result))
      mpz_neg(PMATH_AS_MPZ(result), PMATH_AS_MPZ(result));
      
    // _pmath_mp_int_normalize not necessary, because value cannot fit into
    // int32_t.
    return result;
  }
  
  assert(pmath_is_mpint(integer));
  
  if(pmath_refcount(integer) == 1)
    result = pmath_ref(integer);
  else
    result = _pmath_create_mp_int(0);
    
  if(!pmath_is_null(result))
    mpz_neg(PMATH_AS_MPZ(result), PMATH_AS_MPZ(integer));
    
  pmath_unref(integer);
  return result;
}

PMATH_API pmath_number_t pmath_number_neg(pmath_number_t num) {
  if(pmath_is_double(num)) {
    num.as_double = -num.as_double;
    
    return num;
  }
  
  if(pmath_is_int32(num))
    return _neg_i(num);
    
  assert(pmath_is_pointer(num));
  
  if(pmath_is_null(num))
    return num;
    
  switch(PMATH_AS_PTR(num)->type_shift) {
    case PMATH_TYPE_SHIFT_MP_INT:
      return _neg_i(num);
      
    case PMATH_TYPE_SHIFT_QUOTIENT: {
        pmath_integer_t numerator   = pmath_ref(PMATH_QUOT_NUM(num));
        pmath_integer_t denominator = pmath_ref(PMATH_QUOT_DEN(num));
        pmath_unref(num);
        
        // already in canonical form -> using _pmath_create_quotient() directly
        return _pmath_create_quotient(
                 _neg_i(numerator), denominator);
      }
      
    case PMATH_TYPE_SHIFT_MP_FLOAT: {
        pmath_float_t result;
        
        if(pmath_refcount(num) == 1)
          result = pmath_ref(num);
        else
          result = _pmath_create_mp_float(PMATH_AS_ARB_WORKING_PREC(num));
          
        if(!pmath_is_null(result)) 
          arb_neg(PMATH_AS_ARB(result), PMATH_AS_ARB(num));
        
        pmath_unref(num);
        return result;
      }
  }
  
  assert("invalid number type" && 0);
  return PMATH_NULL;
}

//} ============================================================================
//{ basic integer functions

PMATH_PRIVATE pmath_integer_t _mul_ii(
  pmath_integer_t intA, // will be freed. not PMATH_NULL!
  pmath_integer_t intB  // will be freed. not PMATH_NULL!
) {
  pmath_mpint_t result;
  
  assert(pmath_is_integer(intA));
  assert(pmath_is_integer(intB));
  
  if(pmath_is_int32(intA)) {
    int a = PMATH_AS_INT32(intA);
    
    if(pmath_is_int32(intB)) {
      int b = PMATH_AS_INT32(intB);
      
      int64_t mul = (int64_t)a * b;
      if(mul == (int32_t)mul)
        return PMATH_FROM_INT32((int32_t)mul);
        
      intB = _pmath_create_mp_int(b);
      if(pmath_is_null(intB))
        return intB;
    }
    
    assert(pmath_is_mpint(intB));
    
    result = _pmath_create_mp_int(0);
    if(pmath_is_null(result)) {
      pmath_unref(intB);
      return result;
    }
    
    mpz_mul_si(
      PMATH_AS_MPZ(result),
      PMATH_AS_MPZ(intB),
      a);
      
    pmath_unref(intB);
    return _pmath_mp_int_normalize(result);
  }
  
  assert(pmath_is_mpint(intA));
  
  if(pmath_is_int32(intB)) {
    int b = PMATH_AS_INT32(intB);
    
    result = _pmath_create_mp_int(0);
    if(pmath_is_null(result)) {
      pmath_unref(intA);
      return result;
    }
    
    mpz_mul_si(
      PMATH_AS_MPZ(result),
      PMATH_AS_MPZ(intA),
      b);
      
    pmath_unref(intA);
    return _pmath_mp_int_normalize(result);
  }
  
  result = _pmath_create_mp_int(0);
  if(!pmath_is_null(result)) {
    mpz_mul(
      PMATH_AS_MPZ(result),
      PMATH_AS_MPZ(intA),
      PMATH_AS_MPZ(intB));
  }
  
  pmath_unref(intA);
  pmath_unref(intB);
  return result;
}

//} ============================================================================
//{ pMath object functions for integers ...

static void destroy_mp_int(pmath_t integer) {
  struct _pmath_mp_int_t *int_ptr;
  uintptr_t i = int_cache_inc(+1);
  
  assert(pmath_refcount(integer) == 0);
  
  int_ptr = (void *)PMATH_AS_PTR(integer);
  
  PMATH_OBJECT_MARK_DELETION_TRAP(&int_ptr->inherited);
  
  int_ptr = int_cache_swap(i, int_ptr);
  if(int_ptr) {
    assert(int_ptr->inherited.refcount._data == 0);
    
    mpz_clear(int_ptr->value);
    pmath_mem_free(int_ptr);
  }
}

static unsigned int hash_mpz(mpz_srcptr mpz, unsigned h) {
  return _pmath_incremental_hash(
           mpz[0]._mp_d,
           sizeof(mpz[0]._mp_d[0]) * (size_t)abs(mpz[0]._mp_size),
           h);
}

static unsigned int hash_init;

static unsigned int hash_mp_int(pmath_t integer) {
  /* Note that an mp integer never holds a value that fits int32, so the hash calculation does not 
     need to be compatible with that for pmath_is_int32()-values.
   */
  return hash_mpz(PMATH_AS_MPZ(integer), hash_init);
}

static char alphabet[] = "0123456789abcdefghijklmnopqrstuvwxyz";

PMATH_PRIVATE
void _pmath_write_machine_int(struct pmath_write_ex_t *info, pmath_t integer) {
  pmath_thread_t thread = pmath_thread_get_current();
  char s[40];
  
  if( thread                   &&
      thread->numberbase != 10 &&
      thread->numberbase >= 2  &&
      thread->numberbase <= 36)
  {
    unsigned val;
    int a, b;
    
    if(PMATH_AS_INT32(integer) == 0) {
      snprintf(s, sizeof(s), "%d^^0", (int)thread->numberbase);
      _pmath_write_cstr(s, info->write, info->user);
      return;
    }
    
    if(PMATH_AS_INT32(integer) < 0) {
      a = snprintf(s, sizeof(s), "-%d^^", (int)thread->numberbase);
      val = (unsigned)(-PMATH_AS_INT32(integer));
    }
    else {
      a = snprintf(s, sizeof(s),  "%d^^", (int)thread->numberbase);
      val = (unsigned)PMATH_AS_INT32(integer);
    }
    
    b = a - 1;
    while(val > 0) {
      unsigned mod = val % (unsigned)thread->numberbase;
      val /= (unsigned)thread->numberbase;
      
      s[++b] = alphabet[mod];
    }
    s[b + 1] = '\0';
    
    while(a < b) {
      char tmp = s[a];
      s[a] = s[b];
      s[b] = tmp;
      ++a;
      --b;
    }
  }
  else
    snprintf(s, sizeof(s), "%d", (int)PMATH_AS_INT32(integer));
    
  _pmath_write_cstr(s, info->write, info->user);
}

static void write_mp_int(struct pmath_write_ex_t *info, pmath_t integer) {
  pmath_thread_t thread = pmath_thread_get_current();
  char *str;
  int base = 10;
  size_t size;
  
  if(thread && thread->numberbase >= 2 && thread->numberbase <= 36)
    base = (int)thread->numberbase;
    
  size = mpz_sizeinbase(PMATH_AS_MPZ(integer), base) + 6;
  
  str = (char *)pmath_mem_alloc(size);
  if(!str) {
    _pmath_write_cstr("<<out-of-memory>>", info->write, info->user);
    return;
  }
//  if(base == 16)
//    _pmath_write_cstr("16^^", write, user);
  mpz_get_str(str, base, PMATH_AS_MPZ(integer));
  
  if(base != 10) {
    char basestr[6];
    
    if(str[0] == '-') {
      snprintf(basestr, sizeof(basestr), "-%d^^", base);
      _pmath_write_cstr(basestr, info->write, info->user);
      _pmath_write_cstr(str + 1,   info->write, info->user);
    }
    else {
      snprintf(basestr, sizeof(basestr), "%d^^", base);
      _pmath_write_cstr(basestr, info->write, info->user);
      _pmath_write_cstr(str,     info->write, info->user);
    }
  }
  else
    _pmath_write_cstr(str, info->write, info->user);
    
  pmath_mem_free(str);
}

//} ============================================================================
//{ pMath object functions for quotients ...

static void destroy_quotient(pmath_t quotient) {
  assert(pmath_refcount(quotient) == 0);
  
  PMATH_OBJECT_MARK_DELETION_TRAP(PMATH_AS_PTR(quotient));
  
  pmath_unref(PMATH_QUOT_NUM(quotient));
  pmath_unref(PMATH_QUOT_DEN(quotient));
  
  pmath_mem_free(PMATH_AS_PTR(quotient));
}

static unsigned int hash_quotient(pmath_t quotient) {
  unsigned int next = 0;
  unsigned int h;
  
  h    = pmath_hash(PMATH_QUOT_NUM(quotient));
  next = _pmath_incremental_hash(&h, sizeof(h), next);
  h    = pmath_hash(PMATH_QUOT_DEN(quotient));
  return _pmath_incremental_hash(&h, sizeof(h), next);
}

static void write_quotient(struct pmath_write_ex_t *info, pmath_t quotient) {
  _pmath_write_impl(info, PMATH_QUOT_NUM(quotient));
  _pmath_write_cstr("/", info->write, info->user);
  _pmath_write_impl(info, PMATH_QUOT_DEN(quotient));
}

//} ============================================================================
//{ pMath object functions for multi precision floats ...

static void destroy_mp_float(pmath_t f) {
  uintptr_t i = mp_cache_inc(+1);
  struct _pmath_mp_float_t *f_ptr;
  
  assert(pmath_refcount(f) == 0);
  
  f_ptr = (void *)PMATH_AS_PTR(f);
  
  PMATH_OBJECT_MARK_DELETION_TRAP(&f_ptr->inherited);
  
  f_ptr = mp_cache_swap(i, f_ptr);
  if(f_ptr) {
    assert(f_ptr->inherited.refcount._data == 0);
    
    arb_clear(f_ptr->value_new);
    pmath_mem_free(f_ptr);
  }
}

static unsigned int hash_fmpz(const fmpz_t x, unsigned int h) {
  fmpz coeff = *x;
    
  if(!COEFF_IS_MPZ(coeff)) {
    /** Give the same hash as if COEFF_IS_MPZ(x) was true with the same (small) value in an mpz.
    
        Is this compatibility really necessary?
        If Flint can guarantee that small values never appear in COEFF_IS_MPZ(x), then 
        we could do some totally different calculation for small values here.
     */
    PMATH_STATIC_ASSERT(GMP_NAIL_BITS == 0);
    PMATH_STATIC_ASSERT(sizeof(mp_limb_t) == sizeof(fmpz));
    
    mpz_t mpz;
    mp_limb_t limb = (coeff > 0 ? coeff : -coeff);
    int size = (limb != 0);
    
    mpz[0]._mp_d = &limb;
    mpz[0]._mp_alloc = 1;
    mpz[0]._mp_size = coeff >= 0 ? size : -size;
    
    return hash_mpz(mpz, h);
  }
  
   return hash_mpz(COEFF_TO_PTR(coeff), h);
}

static unsigned int hash_arf(const arf_t x, unsigned int h) {
  const mp_limb_t *mant;
  mp_size_t mant_len;
  
  h = hash_fmpz(ARF_EXPREF(x), h);
  h = _pmath_incremental_hash(&ARF_XSIZE(x), sizeof(mp_size_t), h);
  
  ARF_GET_MPN_READONLY(mant, mant_len, x);
  h = _pmath_incremental_hash(mant, sizeof(mant[0]) * mant_len, h);
  return h;
}

static unsigned int hash_mag(const mag_t x, unsigned int h) {
  const mp_limb_t *mant = &MAG_MAN(x);
  const mp_size_t mant_len = 1;
  
  h = hash_fmpz(MAG_EXPREF(x), h);
  h = _pmath_incremental_hash(mant, sizeof(mant[0]) * mant_len, h);
  return h;
}

static unsigned int hash_mp_float(pmath_t f) {
  unsigned int h = 0;
  /* Note that the hash calculation is incompatible with double-values, which could also be seen
     as real balls with 
     
     Also note that the working precision is ignored, because pmath_equal() also ignores it.
   */
  h = hash_arf(arb_midref(PMATH_AS_ARB(f)), h);
  h = hash_mag(arb_radref(PMATH_AS_ARB(f)), h);
  return h;
}

static void delete_trailing_zeros(char *s) {
  char *s2 = s + strlen(s) - 1;
  
  while(s2 != s && *s2 == '0')
    --s2;
    
  s2[1] = '\0';
}

static void write_raw_string(struct pmath_write_ex_t *info, pmath_string_t str) {
  const uint16_t *buf = pmath_string_buffer(&str);
  int len = pmath_string_length(str);
  info->write(info->user, buf, len);
}

static void write_as_machine_float(struct pmath_write_ex_t *info, mpfr_t f) {
  pmath_thread_t thread = pmath_thread_get_current();
  int base = 10;
  char basestr[10];
  mp_exp_t exp;
  size_t max_digits, size;
  char *str;
  double prec2 = DBL_MANT_DIG;
  double base_prec;
  pmath_bool_t allow_round_trip = 0 != (info->options & (PMATH_WRITE_OPTIONS_INPUTEXPR | PMATH_WRITE_OPTIONS_FULLEXPR));
  int exact_log2_base; // =0 if base is no power of 2, otherwise = log2(base)
  
  if(thread && thread->numberbase >= 2 && thread->numberbase <= 36)
    base = thread->numberbase;
    
  if(base == 10) {
    exact_log2_base = 0;
    base_prec = LOG10_2 * prec2;
  }
  else if(base == 2) {
    exact_log2_base = 1;
    base_prec = prec2 / exact_log2_base;
  }
  else if(base == 4) {
    exact_log2_base = 2;
    base_prec = prec2 / exact_log2_base;
  }
  else if(base == 8) {
    exact_log2_base = 3;
    base_prec = prec2 / exact_log2_base;
  }
  else if(base == 16) {
    exact_log2_base = 4;
    base_prec = prec2 / exact_log2_base;
  }
  else if(base == 32) {
    exact_log2_base = 5;
    base_prec = prec2 / exact_log2_base;
  }
  else {
    exact_log2_base = 0;
    base_prec = prec2 / log2(base);
  }
  
  if(allow_round_trip) {
    /* MPFR documentation says:
       To recover f, we need (in most cases ...) a representation with
       m = 1 + ceil(precbits * log(2) / log(base)) digits (with precbits
       replaced by  precbits-1  if base is a power of 2).
     */
    
    if(exact_log2_base > 0)
      max_digits = (size_t)(1 + ceil( (prec2 - 1) / exact_log2_base ));
    else
      max_digits = (size_t)(1 + ceil( base_prec ));
  }
  else {
    max_digits = (size_t)round(base_prec);
  }
  
  if(max_digits < 2)
    max_digits = 2;
    
  size = max_digits + 2;
  if(size < 7)
    size = 7;
    
  str = (char *)pmath_mem_alloc(size);
  if(!str) {
    _pmath_write_cstr("<<out-of-memory>>", info->write, info->user);
    return;
  }
  
  mpfr_get_str(str, &exp, base, max_digits, f, MPFR_RNDN);
  
  if(exp == 0) {
    if(*str == '-') {
      delete_trailing_zeros(str + 1);
      
      if(base != 10) {
        snprintf(basestr, sizeof(basestr), "-%d^^0.", base);
        _pmath_write_cstr(basestr, info->write, info->user);
      }
      else
        _pmath_write_cstr("-0.", info->write, info->user);
        
      _pmath_write_cstr(str + 1, info->write, info->user);
    }
    else {
      delete_trailing_zeros(str);
      
      if(base != 10) {
        snprintf(basestr, sizeof(basestr), "%d^^0.", base);
        _pmath_write_cstr(basestr, info->write, info->user);
      }
      else
        _pmath_write_cstr("0.", info->write, info->user);
        
      _pmath_write_cstr(str,    info->write, info->user);
    }
    
    if(allow_round_trip)
      _pmath_write_cstr("`", info->write, info->user);
  }
  else if(exp > 0 && exp <= 6 && (size_t)exp < strlen(str)) {
    char c;
    if(*str == '-')
      ++exp;
    c = str[exp];
    str[exp] = '\0';
    if(base != 10) {
      if(*str == '-') {
        snprintf(basestr, sizeof(basestr), "-%d^^", base);
        _pmath_write_cstr(basestr, info->write, info->user);
        _pmath_write_cstr(str + 1, info->write, info->user);
      }
      else {
        snprintf(basestr, sizeof(basestr), "%d^^", base);
        _pmath_write_cstr(basestr, info->write, info->user);
        _pmath_write_cstr(str,     info->write, info->user);
      }
    }
    else {
      _pmath_write_cstr(str, info->write, info->user);
    }
    _pmath_write_cstr(".", info->write, info->user);
    str[exp] = c;
    
    delete_trailing_zeros(str + exp);
    _pmath_write_cstr(str + exp, info->write, info->user);
    
    if(allow_round_trip)
      _pmath_write_cstr("`", info->write, info->user);
  }
  else if(exp < 0 && exp > -5) {
    static const uint16_t zero_char = '0';
    
    int start;
    if(*str == '-') {
      if(base != 10) {
        snprintf(basestr, sizeof(basestr), "-%d^^0.", base);
        _pmath_write_cstr(basestr, info->write, info->user);
      }
      else
        _pmath_write_cstr("-0.", info->write, info->user);
      start = 1;
    }
    else {
      if(base != 10) {
        snprintf(basestr, sizeof(basestr), "%d^^0.", base);
        _pmath_write_cstr(basestr, info->write, info->user);
      }
      else
        _pmath_write_cstr("0.", info->write, info->user);
      start = 0;
    }
    
    do {
      info->write(info->user, &zero_char, 1);
    } while(++exp < 0);
    
    delete_trailing_zeros(str + start);
    
    _pmath_write_cstr(str + start, info->write, info->user);
    
    if(allow_round_trip)
      _pmath_write_cstr("`", info->write, info->user);
  }
  else {
    int start;
    if(*str == '\0') { // 0.0
      if(base != 10) {
        snprintf(basestr, sizeof(basestr), "%d^^0.", base);
        _pmath_write_cstr(basestr, info->write, info->user);
      }
      else
        _pmath_write_cstr("0.", info->write, info->user);
      start = 0;
    }
    else if(*str == '-') {
      if(base != 10) {
        snprintf(basestr, sizeof(basestr), "-%d^^%c.", base, str[1]);
        _pmath_write_cstr(basestr, info->write, info->user);
      }
      else {
        uint16_t ustr[3] = {UCS2_CHAR('-'), UCS2_CHAR(str[1]), UCS2_CHAR('.')};
        info->write(info->user, ustr, 3);
      }
      
      start = 2;
      --exp;
    }
    else {
      uint16_t ustr[2] = {UCS2_CHAR(*str), UCS2_CHAR('.')};
      
      if(base != 10) {
        snprintf(basestr, sizeof(basestr), "%d^^", base);
        _pmath_write_cstr(basestr, info->write, info->user);
      }
      
      info->write(info->user, ustr, 2);
      start = 1;
      --exp;
    }
    
    delete_trailing_zeros(str + start);
    _pmath_write_cstr(str + start, info->write, info->user);
    if(allow_round_trip)
      _pmath_write_cstr("`", info->write, info->user);
      
    if(exp != 0) {
      char s[30];
      snprintf(s, sizeof(s), "*^%"PRIdMAX, (intmax_t)exp);
      _pmath_write_cstr(s, info->write, info->user);
    }
  }
  
  pmath_mem_free(str);
}

static void write_mp_float(struct pmath_write_ex_t *info, pmath_t f) {
  pmath_thread_t thread = pmath_thread_get_current();
  int base = 10;
  slong max_digit_count;
  int base_flags;
  struct _pmath_number_string_parts_t parts;
  pmath_bool_t show_radius_and_precision = FALSE;
  
  if(thread && thread->numberbase >= 2 && thread->numberbase <= 36)
    base = thread->numberbase;
    
  max_digit_count = (int)(PMATH_AS_ARB_WORKING_PREC(f) / _pmath_log2_of(base) + 2);
  base_flags = base;
  
  if(info->options & PMATH_WRITE_OPTIONS_FULLEXPR) {
    // changing base
    base_flags = 16 | PMATH_BASE_FLAG_ALL_DIGITS;
    show_radius_and_precision = TRUE;
  }
  else if(info->options & PMATH_WRITE_OPTIONS_INPUTEXPR) {
    // not changing base: PMATH_BASE_FLAG_ALL_DIGITS only used if base was 16.
    base_flags = base | PMATH_BASE_FLAG_ALL_DIGITS;
    show_radius_and_precision = TRUE;
  }
  
  _pmath_mpfloat_get_string_parts(&parts, f, max_digit_count, base_flags);
  
  if(!show_radius_and_precision) {
    show_radius_and_precision =
      pmath_string_equals_latin1(parts.midpoint_fractional_mantissa_digits, "0.0") &&
      !pmath_string_equals_latin1(parts.radius_fractional_mantissa_digits, "0.0");
  }
  
  if(parts.is_negative)
    _pmath_write_cstr("-", info->write, info->user);
    
  if(parts.base != 10) {
    char buf[5];
    snprintf(buf, sizeof(buf), "%d^^", parts.base);
    _pmath_write_cstr(buf, info->write, info->user);
  }
  
  write_raw_string(info, parts.midpoint_fractional_mantissa_digits);
  if(show_radius_and_precision) {
    _pmath_write_cstr("[+/-", info->write, info->user);
    write_raw_string(info, parts.radius_fractional_mantissa_digits);
    if(pmath_string_length(parts.radius_exponent_part_decimal_digits) > 0) {
      _pmath_write_cstr("*^", info->write, info->user);
      write_raw_string(info, parts.radius_exponent_part_decimal_digits);
    }
    _pmath_write_cstr("]`", info->write, info->user);
    write_raw_string(info, parts.precision_decimal_digits);
  }
  
  if(pmath_string_length(parts.exponent_decimal_digits) > 0) {
    _pmath_write_cstr("*^", info->write, info->user);
    write_raw_string(info, parts.exponent_decimal_digits);
  }
  _pmath_number_string_parts_clear(&parts);
}

//} ============================================================================
//{ pMath object functions for machine floats ...

PMATH_PRIVATE
void _pmath_write_machine_float(struct pmath_write_ex_t *info, pmath_t f) {
  mpfr_t mpf;
  mpfr_init2(mpf, DBL_MANT_DIG);
  mpfr_set_d(mpf, PMATH_AS_DOUBLE(f), MPFR_RNDN);
  
  write_as_machine_float(info, mpf);
  
  mpfr_clear(mpf);
}

//} ============================================================================
//{ common pMath object functions for all number types ...

static int compare_int_to_number(int32_t a, pmath_number_t numB);
static int compare_double_to_number(double a, pmath_number_t numB);
static int compare_mpint_to_mp_number(pmath_mpint_t numA, pmath_number_t numB);
static int compare_quotient_to_number(pmath_quotient_t numA, pmath_number_t numB);
static int compare_arb_to_arb(const arb_t a, const arb_t b);
static int compare_arb_to_integer(const arb_t numA, pmath_integer_t numB);
static int compare_mpfloat_to_quotient(pmath_mpfloat_t numA, pmath_quotient_t numB);
static int compare_mpfloat_to_number(pmath_mpfloat_t numA, pmath_number_t numB);

#define RETURN_SIMPLE_CMP \
  if(a < b)    \
    return -1; \
  if(a > b)    \
    return 1;  \
  return 0;

static int compare_int_to_number(int32_t a, pmath_number_t numB) {
  if(pmath_is_int32(numB)) {
    int b = PMATH_AS_INT32(numB);
    
    RETURN_SIMPLE_CMP
  }
  
  if(pmath_is_double(numB)) {
    double b = PMATH_AS_DOUBLE(numB);
    
    RETURN_SIMPLE_CMP
  }
  
  switch(PMATH_AS_PTR(numB)->type_shift) {
    case PMATH_TYPE_SHIFT_MP_INT:
      return -mpz_cmp_si(PMATH_AS_MPZ(numB), a);
      
    case PMATH_TYPE_SHIFT_MP_FLOAT:
      return -compare_arb_to_integer(PMATH_AS_ARB(numB), PMATH_FROM_INT32(a));
      
    case PMATH_TYPE_SHIFT_QUOTIENT: {
        fmpq_t qa;
        fmpq_t qb;
        int res;
        
        fmpq_init(qa);
        fmpq_init(qb);
        
        fmpq_set_si(qa, a, 1);
        _pmath_rational_get_fmpq(qb, numB);
        
        res = fmpq_cmp(qa, qb);
        
        fmpq_clear(qb);
        fmpq_clear(qa);
        
        return res;
      }
  }
  
  assert("unknown number type" && 0);
  return 0;
}

static int compare_double_to_number(double a, pmath_number_t numB) {
  if(pmath_is_int32(numB)) {
    int b = PMATH_AS_INT32(numB);
    
    RETURN_SIMPLE_CMP
  }
  
  if(pmath_is_double(numB)) {
    double b = PMATH_AS_DOUBLE(numB);
    
    RETURN_SIMPLE_CMP
  }
  
  if(pmath_is_mpint(numB)) {
    return -mpz_cmp_d(PMATH_AS_MPZ(numB), a);
  }
  else {
    arb_t tmpA;
    int result;
    assert(pmath_is_mpfloat(numB) || pmath_is_quotient(numB));
    
    arb_init(tmpA);
    arb_set_d(tmpA, a);
    
    if(pmath_is_mpfloat(numB)) {
      result = compare_arb_to_arb(tmpA, PMATH_AS_ARB(numB));
    }
    else {
      assert(pmath_is_quotient(numB));
      if(pmath_is_int32(PMATH_QUOT_DEN(numB))) {
        arb_mul_si(tmpA, tmpA, PMATH_AS_INT32(PMATH_QUOT_DEN(numB)), ARF_PREC_EXACT);
      }
      else {
        fmpz_t den;
        assert(pmath_is_mpint(PMATH_QUOT_DEN(numB)));
        fmpz_init_set_readonly(den, PMATH_AS_MPZ(PMATH_QUOT_DEN(numB)));
        arb_mul_fmpz(tmpA, tmpA, den, ARF_PREC_EXACT);
      }
      result = compare_arb_to_integer(tmpA, PMATH_QUOT_NUM(numB));
    }
    
    arb_clear(tmpA);
    return result;
  }
}

static int compare_mpint_to_mp_number(pmath_mpint_t numA, pmath_number_t numB) {
  assert(pmath_is_mpint(numA));
  
  if(pmath_is_mpint(numB))
    return mpz_cmp(PMATH_AS_MPZ(numA), PMATH_AS_MPZ(numB));
    
  if(pmath_is_quotient(numB)) {
    fmpq_t qa;
    fmpq_t qb;
    int result;
    
    fmpq_init(qa);
    fmpq_init(qb);
    
    _pmath_integer_get_fmpz(fmpq_numref(qa), numA);
    fmpz_set_ui(fmpq_denref(qa), 1);
    
    _pmath_rational_get_fmpq(qa, numB);
    
    result = fmpq_cmp(qa, qb);
    
    fmpq_clear(qb);
    fmpq_clear(qa);
    return result;
  }
  
  assert(pmath_is_mpfloat(numB));
  return -compare_arb_to_integer(PMATH_AS_ARB(numB), numA);
}

static int compare_quotient_to_number(pmath_quotient_t numA, pmath_number_t numB) {
  assert(pmath_is_quotient(numA));
  
  if(pmath_is_quotient(numB)) {
    fmpq_t qa;
    fmpq_t qb;
    int result;
    
    fmpq_init(qa);
    fmpq_init(qb);
    
    _pmath_rational_get_fmpq(qa, numA);
    _pmath_rational_get_fmpq(qb, numB);
    result = fmpq_cmp(qa, qb);
    
    fmpq_clear(qb);
    fmpq_clear(qa);
    return result;
  }
  
  if(pmath_is_mpfloat(numB))
    return -compare_mpfloat_to_quotient(numB, numA);
    
  return -_pmath_numbers_compare(numB, numA);
}

static int compare_arb_to_arb(const arb_t a, const arb_t b) {
  if(arb_lt(a, b))
    return -1;
    
  if(arb_gt(a, b))
    return 1;
    
  return 0; // overlapping
}

static int compare_arb_to_integer(const arb_t numA, pmath_integer_t numB) {
  arb_t tmpB;
  int result;
  
  assert(pmath_is_integer(numB));
  
  arb_init(tmpB);
  _pmath_number_get_arb(tmpB, numB, 0); // precision is ignored for integers, supply 0
  
  result = compare_arb_to_arb(numA, tmpB);
  
  arb_clear(tmpB);
  return result;
}

static int compare_mpfloat_to_quotient(pmath_mpfloat_t numA, pmath_quotient_t numB) {
  arb_t tmpA;
  arb_t tmpB;
  int result;
  
  assert(pmath_is_mpfloat(numA));
  assert(pmath_is_quotient(numB));
  
  arb_init(tmpA);
  arb_init(tmpB);
  _pmath_number_get_arb(tmpA, PMATH_QUOT_DEN(numB), 0); // precision is ignored for integers, supply 0
  arb_mul(tmpA, PMATH_AS_ARB(numA), tmpA, ARF_PREC_EXACT);
  _pmath_number_get_arb(tmpB, PMATH_QUOT_NUM(numB), 0); // precision is ignored for integers, supply 0
  
  result = compare_arb_to_arb(tmpA, tmpB);
  
  arb_clear(tmpA);
  arb_clear(tmpB);
  return result;
}

static int compare_mpfloat_to_number(pmath_mpfloat_t numA, pmath_number_t numB) {
  assert(pmath_is_mpfloat(numA));
  
  if(pmath_is_mpfloat(numB))
    return compare_arb_to_arb(PMATH_AS_ARB(numA), PMATH_AS_ARB(numB));
    
  if(pmath_is_quotient(numB))
    return compare_mpfloat_to_quotient(numA, numB);
    
  if(pmath_is_integer(numB))
    return compare_arb_to_integer(PMATH_AS_ARB(numA), numB);
    
  assert(pmath_is_double(numB));
  return -compare_double_to_number(PMATH_AS_DOUBLE(numB), numA);
}

#undef RETURN_SIMPLE_CMP

PMATH_PRIVATE
int _pmath_numbers_compare(
  pmath_number_t numA,
  pmath_number_t numB
) {
  if(pmath_is_int32(numA))
    return compare_int_to_number(PMATH_AS_INT32(numA), numB);
    
  if(pmath_is_double(numA))
    return compare_double_to_number(PMATH_AS_DOUBLE(numA), numB);
    
  if(pmath_is_double(numB) || pmath_is_int32(numB))
    return - _pmath_numbers_compare(numB, numA);
    
  assert(pmath_is_pointer(numA));
  assert(pmath_is_pointer(numB));
  assert(!pmath_is_null(numA));
  assert(!pmath_is_null(numB));
  
  if(pmath_is_mpint(numA))
    return compare_mpint_to_mp_number(numA, numB);
    
  if(pmath_is_quotient(numA))
    return compare_quotient_to_number(numA, numB);
    
  if(pmath_is_mpfloat(numA))
    return compare_mpfloat_to_number(numA, numB);
    
  assert("unknown number type" && 0);
  return 0;
}

PMATH_PRIVATE
pmath_bool_t _pmath_numbers_equal(
  pmath_number_t numA,
  pmath_number_t numB
) {
  if(pmath_is_int32(numA)) {
    if(pmath_is_int32(numB))
      return PMATH_AS_INT32(numA) == PMATH_AS_INT32(numB);
      
    return FALSE;
  }
  
  if(pmath_is_int32(numB))
    return FALSE;
    
  if(pmath_is_mpint(numA)) {
    if(pmath_is_mpint(numB))
      return 0 == mpz_cmp(PMATH_AS_MPZ(numA), PMATH_AS_MPZ(numB));
      
    return FALSE;
  }
  
  if(pmath_is_mpint(numB))
    return FALSE;
    
  if(pmath_is_quotient(numA)) {
    if(pmath_is_quotient(numB)) {
      return _pmath_numbers_equal(PMATH_QUOT_NUM(numA), PMATH_QUOT_NUM(numB)) &&
             _pmath_numbers_equal(PMATH_QUOT_DEN(numA), PMATH_QUOT_DEN(numB));
    }
    
    return FALSE;
  }
  
  if(pmath_is_quotient(numB))
    return FALSE;
    
  if(pmath_is_double(numA)) {
    if(pmath_is_double(numB))
      return PMATH_AS_DOUBLE(numA) == PMATH_AS_DOUBLE(numB);
    
    return FALSE;
  }
    
  if(pmath_is_double(numB))
    return FALSE;
    
  assert(pmath_is_mpfloat(numA));
  assert(pmath_is_mpfloat(numB));
  
  if(!arf_equal(arb_midref(PMATH_AS_ARB(numA)), arb_midref(PMATH_AS_ARB(numB))))
    return FALSE;
    
  if(!mag_equal(arb_radref(PMATH_AS_ARB(numA)), arb_radref(PMATH_AS_ARB(numB))))
    return FALSE;
    
  return TRUE;
}

//} ============================================================================
//{ debug typedefs for *.natvis support

typedef struct {
  fmpz_t value;
} _pmath_fmpz_t;
PMATH_PRIVATE _pmath_fmpz_t _pmath_fmpz_t_dummy;

typedef struct {
  uint8_t value;
} _pmath_uint8_t;
PMATH_PRIVATE _pmath_uint8_t _pmath_uint8_t_dummy;

typedef struct {
#if PMATH_BYTE_ORDER < 0
  _pmath_uint8_t lo;
  _pmath_uint8_t hi;
#else
  _pmath_uint8_t hi;
  _pmath_uint8_t lo;
#endif
} _pmath_uint16_t;
PMATH_PRIVATE _pmath_uint16_t _pmath_uint16_t_dummy;

typedef struct {
#if PMATH_BYTE_ORDER < 0
  _pmath_uint16_t lo;
  _pmath_uint16_t hi;
#else
  _pmath_uint16_t hi;
  _pmath_uint16_t lo;
#endif
} _pmath_uint32_t;
PMATH_PRIVATE _pmath_uint32_t _pmath_uint32_t_dummy;

typedef struct {
#if PMATH_BYTE_ORDER < 0
  _pmath_uint32_t lo;
  _pmath_uint32_t hi;
#else
  _pmath_uint32_t hi;
  _pmath_uint32_t lo;
#endif
} _pmath_uint64_t;
PMATH_PRIVATE _pmath_uint64_t _pmath_uint64_t_dummy;

typedef union {
  mp_limb_t value;
#if GMP_LIMB_BITS == 64
  _pmath_uint64_t parts;
#elif GMP_LIMB_BITS == 32
  _pmath_uint32_t parts;
#endif
} _pmath_limb_t;
PMATH_PRIVATE _pmath_limb_t _pmath_limb_t_dummy;

PMATH_PRIVATE const char *_pmath_debug_hex = "0123456789abcdef";

//}
//{ module handling functions ...

PMATH_PRIVATE void _pmath_numbers_memory_panic(void) {
//  destroy_all_unused_quotients();
  int_cache_clear();
  mp_cache_clear();
  mpfr_free_cache();
  flint_cleanup();
}

PMATH_PRIVATE pmath_bool_t _pmath_numbers_init(void) {
  char c = 0;
  hash_init = _pmath_incremental_hash(&c, 1, 0);
  
#ifdef mpir_version
  pmath_debug_print("[mpir %s ~= gmp %s]\n", mpir_version, gmp_version);
#else
  pmath_debug_print("[gmp %s]\n", gmp_version);
#endif
  
  pmath_debug_print("[mpfr %s%s]\n",
                    mpfr_get_version(),
                    mpfr_buildopt_tls_p() ? "" : ", flags are not thrad safe");
                    
  pmath_debug_print("[flint %s]\n", FLINT_VERSION);
  pmath_debug_print("[arb %s]\n", arb_version);
  
  memset(int_cache, 0, sizeof(int_cache));
  memset(mp_cache,  0, sizeof(mp_cache));
  pmath_atomic_write_release(&int_cache_pos, 0);
  pmath_atomic_write_release(&mp_cache_pos,  0);
  
#ifdef PMATH_DEBUG_LOG
  pmath_atomic_write_release(&int_cache_hits,   0);
  pmath_atomic_write_release(&int_cache_misses, 0);
  
  pmath_atomic_write_release(&mp_cache_hits,    0);
  pmath_atomic_write_release(&mp_cache_misses,  0);
#endif
  
  gmp_randinit_default(_pmath_randstate);
  //gmp_randseed_ui(_pmath_randstate, (unsigned)rand());
  {
    mpz_t now;
    mpz_init_set_d(now, pmath_datetime() * 1000.0);
    gmp_randseed(_pmath_randstate, now);
    mpz_clear(now);
  }
  pmath_atomic_write_release(&_pmath_rand_spinlock, 0);
  
  _pmath_init_special_type(
    PMATH_TYPE_SHIFT_MP_INT,
    _pmath_numbers_compare,
    hash_mp_int,
    destroy_mp_int,
    _pmath_numbers_equal,
    write_mp_int);
    
  _pmath_init_special_type(
    PMATH_TYPE_SHIFT_QUOTIENT,
    _pmath_numbers_compare,
    hash_quotient,
    destroy_quotient,
    _pmath_numbers_equal,
    write_quotient);
    
  _pmath_init_special_type(
    PMATH_TYPE_SHIFT_MP_FLOAT,
    _pmath_numbers_compare,
    hash_mp_float,
    destroy_mp_float,
    _pmath_numbers_equal,
    write_mp_float);
    
  _pmath_one_half = _pmath_create_quotient(
                      PMATH_FROM_INT32(1),
                      PMATH_FROM_INT32(2));
                      
  if(pmath_is_null(_pmath_one_half))
    goto FAIL;
    
  if(!_pmath_primetest_init())
    goto FAIL_PRIME;
    
  return TRUE;
  
//          _pmath_primetest_done();
FAIL_PRIME:
FAIL: pmath_unref(_pmath_one_half);

  int_cache_clear();
  mp_cache_clear();
  gmp_randclear(_pmath_randstate);
  return FALSE;
}

static void unload_flint(void) {
  void (*cleanup_master_ptr)(void) = NULL;
  
  // Call flint_cleanup_master() if available to also free FLINT thread-pool. 
  // Introduced after Flint 2.5.3, but Flint 2.5.4 is not yet available. 
  
  #ifdef PMATH_OS_WIN32
  {
    HMODULE dll_flint = NULL;
    if(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (const wchar_t*)flint_cleanup, &dll_flint)) {
      cleanup_master_ptr = (void(*)(void))GetProcAddress(dll_flint, "flint_cleanup_master");
    }
  }
  #elif defined(PMATH_OS_UNIX)
  {
    Dl_info info_flint;
    if(dladdr(flint_cleanup, &info_flint)) {
      void *flint_lib_handle = dlopen(info_flint.dli_fname, RTLD_NOLOAD);
      if(flint_lib_handle) {
        cleanup_master_ptr = (void(*)(void))dlsym(flint_lib_handle, "flint_cleanup_master");
      }
    }
  }
  #endif
  
  if(cleanup_master_ptr)
    cleanup_master_ptr();
  else
    flint_cleanup();
}

PMATH_PRIVATE void _pmath_numbers_done(void) {
  _pmath_primetest_done();
  pmath_unref(_pmath_one_half);
  
  int_cache_clear();
  mp_cache_clear();
  mpfr_free_cache();
  unload_flint();
  gmp_randclear(_pmath_randstate);
  
#ifdef PMATH_DEBUG_LOG
  {
    intptr_t int_hits   = pmath_atomic_read_aquire(&int_cache_hits);
    intptr_t mp_hits    = pmath_atomic_read_aquire(&mp_cache_hits);
    intptr_t int_misses = pmath_atomic_read_aquire(&int_cache_misses);
    intptr_t mp_misses  = pmath_atomic_read_aquire(&mp_cache_misses);
    
    pmath_debug_print("int cache hit rate:              %f (%d of %d)\n",
                      int_hits / (double)(int_hits + int_misses),
                      (int) int_hits,
                      (int)(int_hits + int_misses));
    pmath_debug_print("multi prec float cache hit rate: %f (%d of %d)\n",
                      mp_hits  / (double)(mp_hits  + mp_misses),
                      (int) mp_hits,
                      (int)(mp_hits + mp_misses));
  }
#endif
}

//}
