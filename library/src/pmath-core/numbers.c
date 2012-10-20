#include <pmath-core/numbers-private.h>
#include <pmath-core/objects-private.h>
#include <pmath-core/strings-private.h>

#include <pmath-language/tokens.h>

#include <pmath-util/approximate.h>
#include <pmath-util/concurrency/threads-private.h>
#include <pmath-util/debug.h>
#include <pmath-util/incremental-hash-private.h>
#include <pmath-util/memory.h>
#include <pmath-util/messages.h>
#include <pmath-util/strtod.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/number-theory-private.h>

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>


#if __GNU_MP_VERSION < 4
#  error gmp version 4 or newer needed
#endif

#ifdef _MSC_VER
#  define snprintf sprintf_s
#endif

PMATH_PRIVATE pmath_quotient_t _pmath_one_half; /* readonly */

PMATH_PRIVATE pmath_t _pmath_object_overflow;          /* readonly */
PMATH_PRIVATE pmath_t _pmath_object_underflow;         /* readonly */
PMATH_PRIVATE pmath_t _pmath_object_infinity;          /* readonly */
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
  i = i &CACHE_MASK;
  
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
    pmath_atomic_write_release(&integer->inherited.refcount, 1);
    
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
  i = i &CACHE_MASK;
  
  return (void *)pmath_atomic_fetch_set(&mp_cache[i], (intptr_t)f);
}

static void mp_cache_clear(void) {
  uintptr_t i;
  
  for(i = 0; i < CACHE_SIZE; ++i) {
    struct _pmath_mp_float_t *f = mp_cache_swap(i, NULL);
    
    if(f) {
      assert(f->inherited.refcount._data == 0);
      
      mpfr_clear(f->value);
      mpfr_clear(f->error);
      pmath_mem_free(f);
    }
  }
}

PMATH_PRIVATE pmath_float_t _pmath_create_mp_float(mpfr_prec_t precision) {
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
    pmath_atomic_write_release(&f->inherited.refcount, 1);
    
    mpfr_set_prec(f->value, precision);
    mpfr_set_ui_2exp(f->error, 1, -precision - 1, MPFR_RNDU);
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
    
  mpfr_init2(f->value, precision);
  mpfr_init2(f->error, PMATH_MP_ERROR_PREC);
  mpfr_set_ui_2exp(f->error, 1, -precision - 1, MPFR_RNDU);
  
  return PMATH_FROM_PTR(f);
}

PMATH_PRIVATE
pmath_float_t _pmath_create_mp_float_from_d(double value) {
  pmath_float_t result = _pmath_create_mp_float(DBL_MANT_DIG); // (0);
  
  if(!pmath_is_null(result)) {
    mpfr_set_d(PMATH_AS_MP_VALUE(result), value, MPFR_RNDN);
    mpfr_set_d(PMATH_AS_MP_ERROR(result), value, MPFR_RNDN);
    mpfr_abs(PMATH_AS_MP_ERROR(result), PMATH_AS_MP_ERROR(result), MPFR_RNDU);
    mpfr_div_2si(
      PMATH_AS_MP_ERROR(result),
      PMATH_AS_MP_ERROR(result),
      mpfr_get_prec(PMATH_AS_MP_VALUE(result)),
      MPFR_RNDU);
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

PMATH_API pmath_integer_t pmath_integer_new_slong(signed long int si) {
  pmath_mpint_t integer;
  if(si >= INT32_MIN && si <= INT32_MAX)
    return PMATH_FROM_INT32(si);
    
  integer = _pmath_create_mp_int(0);
  if(pmath_is_null(integer))
    return PMATH_NULL;
    
  mpz_set_si(PMATH_AS_MPZ(integer), si);
  return integer;
}

PMATH_API pmath_integer_t pmath_integer_new_ulong(unsigned long int ui) {
  pmath_mpint_t integer;
  if(ui <= INT32_MAX)
    return PMATH_FROM_INT32(ui);
    
  integer = _pmath_create_mp_int(0);
  if(pmath_is_null(integer))
    return PMATH_NULL;
    
  mpz_set_ui(PMATH_AS_MPZ(integer), ui);
  return integer;
}

PMATH_API pmath_integer_t pmath_integer_new_si64(int64_t si) {
  pmath_mpint_t integer;
  
  if(INT32_MIN <= si && si <= INT32_MAX)
    return PMATH_FROM_INT32((int32_t)si);
    
  integer = _pmath_create_mp_int(0);
  if(pmath_is_null(integer))
    return PMATH_NULL;
    
  if(si < 0) {
    si = -si;
    
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

PMATH_API pmath_integer_t pmath_integer_new_ui64(uint64_t ui) {
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

PMATH_API
pmath_number_t pmath_float_new_str(
  const char               *str, // digits.digits
  int                       base,
  pmath_precision_control_t precision_control,
  double                    base_precision_accuracy
) {
  int i, len, int_digits, frac_digits;
  double log2_base = log2(base);
  pmath_bool_t automatic = FALSE;
  pmath_mpfloat_t f;
  
  if(base < 2 || base > 36)
    return PMATH_NULL;
    
  len = strlen(str);
  
  int_digits = 0;
  for(i = 0; i < len && !pmath_char_is_basedigit(base, str[i]); ++i) {
  }
  
  for(; i < len && str[i] != '.'; ++i)
    ++int_digits;
    
  frac_digits = 0;
  if(i < len && str[i] == '.') {
    for(++i; i < len && pmath_char_is_basedigit(base, str[i]); ++i)
      ++frac_digits;
  }
  
  if(str[i] == '@' || str[i] == 'e' || str[i] == 'E') {
    long exp = strtol(str + (i + 1), 0, 10);
    
    if(exp >= INT_MAX - int_digits)
      return PMATH_NULL;
      
    if(exp <= frac_digits - INT_MAX)
      return PMATH_NULL;
      
    int_digits += exp;
    if(int_digits < 0)
      int_digits = 0;
      
    frac_digits -= exp;
    if(frac_digits < 0)
      frac_digits = 0;
  }
  
  if(precision_control == PMATH_PREC_CTRL_AUTO) {
    automatic = TRUE;
    if((int_digits + frac_digits) * log2_base <= DBL_MANT_DIG) {
      precision_control = PMATH_PREC_CTRL_MACHINE_PREC;
    }
    else {
      precision_control = PMATH_PREC_CTRL_GIVEN_PREC;
      base_precision_accuracy = int_digits + frac_digits;
    }
  }
  
  switch(precision_control) {
    case PMATH_PREC_CTRL_MACHINE_PREC: {
        double x;
        
//        if(base == 10) {
//          /* We use strtod() because the the formating of machine float numbers is
//             done based on strtod to print the smallest number of fractional digits
//             possible such that the output would produce the exact same value as
//             input (see _pmath_write_machine_float()).
//
//             Example -- Why we avoid mpfr_set_str():
//             The input 34643574574574947.427457` equals the double number
//                       34643574574574948 and is printed as "3.464357457457495`*^16".
//             When that string is given to MakeExpression(), this function will be
//             called with str = "3.464357457457495e16".
//
//             mpfr_set_str() would generate the number 34643574574574952, but
//             strtod() generates the value             34643574574574948, which we
//             want.
//           */
//          x = pmath_strtod(str, NULL);
//          if(isfinite(x))
//            return PMATH_FROM_DOUBLE(x);
//        }

        f = _pmath_create_mp_float(DBL_MANT_DIG);
        if(pmath_is_null(f))
          return PMATH_NULL;
          
        mpfr_set_str(PMATH_AS_MP_VALUE(f), str, base, MPFR_RNDN);
        
        x = mpfr_get_d(PMATH_AS_MP_VALUE(f), MPFR_RNDN);
        pmath_unref(f);
        return PMATH_FROM_DOUBLE(x);
      };
      
    case PMATH_PREC_CTRL_GIVEN_PREC: {
        pmath_float_t tmp_err;
        double bits = base_precision_accuracy * log2_base;
        mpfr_prec_t prec;
        
        if(bits >= PMATH_MP_PREC_MAX)
          return PMATH_NULL;
          
        if(bits < 0)
          bits = 0;
          
        prec = (mpfr_prec_t)ceil(bits);
        
        f = _pmath_create_mp_float(prec >= MPFR_PREC_MIN ? prec : MPFR_PREC_MIN);
        if(pmath_is_null(f))
          return PMATH_NULL;
          
        tmp_err = _pmath_create_mp_float(PMATH_MP_ERROR_PREC);
        if(pmath_is_null(tmp_err)) {
          pmath_unref(f);
          return PMATH_NULL;
        }
        
        mpfr_set_str(PMATH_AS_MP_VALUE(f), str, base, MPFR_RNDN);
        
        if(mpfr_zero_p(PMATH_AS_MP_VALUE(f))) {
          pmath_unref(f);
          if(automatic)
            return PMATH_FROM_DOUBLE(0.0);
          return PMATH_FROM_INT32(0);
        }
        
        // error = |value| * 2 ^ -bits
        mpfr_abs(PMATH_AS_MP_ERROR(f), PMATH_AS_MP_VALUE(f), MPFR_RNDU);
        mpfr_set_d(PMATH_AS_MP_VALUE(tmp_err), -bits, MPFR_RNDN);
        mpfr_ui_pow(PMATH_AS_MP_ERROR(tmp_err), 2, PMATH_AS_MP_VALUE(tmp_err), MPFR_RNDN);
        mpfr_mul(
          PMATH_AS_MP_ERROR(f),
          PMATH_AS_MP_ERROR(f),
          PMATH_AS_MP_ERROR(tmp_err),
          MPFR_RNDU);
          
        pmath_unref(tmp_err);
        _pmath_mp_float_normalize(f);
        return f;
      };
      
    case PMATH_PREC_CTRL_GIVEN_ACC: {
        double bits = (int_digits + base_precision_accuracy) * log2_base;
        mpfr_prec_t prec;
        
        if(bits >= PMATH_MP_PREC_MAX)
          return PMATH_NULL;
          
        if(bits < 0)
          bits = 0;
          
        prec = (mpfr_prec_t)ceil(bits);
        
        f = _pmath_create_mp_float(prec >= MPFR_PREC_MIN ? prec : MPFR_PREC_MIN);
        if(pmath_is_null(f))
          return PMATH_NULL;
          
        mpfr_set_str(PMATH_AS_MP_VALUE(f), str, base, MPFR_RNDN);
        
        // error = base ^ -accuracy
        mpfr_set_d(PMATH_AS_MP_ERROR(f), -base_precision_accuracy, MPFR_RNDD);
        mpfr_ui_pow(PMATH_AS_MP_ERROR(f), base, PMATH_AS_MP_ERROR(f), MPFR_RNDU);
        
        _pmath_mp_float_normalize(f);
        return f;
      };
      
    default: ;
  }
  
  return PMATH_NULL;
}

PMATH_PRIVATE
mpfr_prec_t _pmath_float_precision( // 0 = MachinePrecision
  pmath_float_t x // wont be freed
) {
  if(pmath_is_mpfloat(x))
    return mpfr_get_prec(PMATH_AS_MP_VALUE(x));
    
  return 0;
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
    
  /* MPFR flags "invalid" and "erange" are ignored.
   */
  
  if(PMATH_UNLIKELY(mpfr_underflow_p())) {
    pmath_message(PMATH_NULL, "unfl", 0);
    result = pmath_ref(_pmath_object_underflow);
  }
  else if (mpfr_overflow_p() ||
           mpfr_zero_p(PMATH_AS_MP_ERROR(x)))
  {
    pmath_message(PMATH_NULL, "ovfl", 0);
    result = pmath_ref(_pmath_object_overflow);
  }
  else if(mpfr_nan_p(PMATH_AS_MP_VALUE(x))) {
    result = pmath_ref(PMATH_SYMBOL_UNDEFINED);
    pmath_message(PMATH_NULL, "indet", 1, pmath_ref(result));
  }
  else if(mpfr_inf_p(PMATH_AS_MP_VALUE(x))) {
    result = pmath_expr_new_extended(
               pmath_ref(PMATH_SYMBOL_DIRECTEDINFINITY), 1,
               PMATH_FROM_INT32(mpfr_sgn(PMATH_AS_MP_VALUE(x))));
    pmath_message(PMATH_NULL, "infy", 1, pmath_ref(result));
  }
  else {
    mpfr_clear_flags();
    return x;
  }
  
  mpfr_clear_flags();
  pmath_unref(x);
  return result;
}

PMATH_PRIVATE
void _pmath_mp_float_include_error(pmath_mpfloat_t f, mpfr_t err_f) {
  MPFR_DECL_INIT(diff, PMATH_MP_ERROR_PREC);
  
  assert(pmath_is_mpfloat(f));
  assert(pmath_refcount(f) == 1);
  
  mpfr_sub(
    diff,
    err_f,
    PMATH_AS_MP_VALUE(f),
    MPFR_RNDA);
  
  mpfr_abs(diff, diff, MPFR_RNDA);
  mpfr_max(
    PMATH_AS_MP_ERROR(f),
    PMATH_AS_MP_ERROR(f),
    diff,
    MPFR_RNDU);
}

PMATH_PRIVATE
void _pmath_mp_float_normalize(pmath_mpfloat_t f) {
  assert(pmath_is_mpfloat(f));
  assert(pmath_refcount(f) == 1);
  
  if(mpfr_zero_p(PMATH_AS_MP_ERROR(f)) || mpfr_zero_p(PMATH_AS_MP_VALUE(f)))
    return;
    
  if(mpfr_get_exp(PMATH_AS_MP_ERROR(f)) > mpfr_get_exp(PMATH_AS_MP_VALUE(f))) {
    mpfr_sign_t sign = PMATH_AS_MP_VALUE(f)->_mpfr_sign;
    mpfr_set_ui(PMATH_AS_MP_VALUE(f), 0, MPFR_RNDN);
    PMATH_AS_MP_VALUE(f)->_mpfr_sign = sign;
  }
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
    
  return mpz_get_ui(PMATH_AS_MPZ(integer));
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
      return mpfr_get_d(PMATH_AS_MP_VALUE(number), MPFR_RNDN);
  }
  
  assert("invalid number type" && 0);
  return 0.0;
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
      return mpfr_sgn(PMATH_AS_MP_VALUE(num));
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
          result = _pmath_create_mp_float(mpfr_get_prec(PMATH_AS_MP_VALUE(num)));
          
        if(!pmath_is_null(result)) {
          mpfr_neg(
            PMATH_AS_MP_VALUE(result),
            PMATH_AS_MP_VALUE(num),
            MPFR_RNDN);
            
          mpfr_set(
            PMATH_AS_MP_ERROR(result),
            PMATH_AS_MP_ERROR(num),
            MPFR_RNDN);
        }
        
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
  int_ptr = int_cache_swap(i, int_ptr);
  if(int_ptr) {
    assert(int_ptr->inherited.refcount._data == 0);
    
    mpz_clear(int_ptr->value);
    pmath_mem_free(int_ptr);
  }
}

static unsigned int hash_init;

static unsigned int hash_mp_int(pmath_t integer) {
  return incremental_hash(
           PMATH_AS_MPZ(integer)[0]._mp_d,
           sizeof(PMATH_AS_MPZ(integer)[0]._mp_d[0]) * (size_t)abs(PMATH_AS_MPZ(integer)[0]._mp_size),
           hash_init);
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
      write_cstr(s, info->write, info->user);
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
    
  write_cstr(s, info->write, info->user);
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
    write_cstr("<<out-of-memory>>", info->write, info->user);
    return;
  }
//  if(base == 16)
//    write_cstr("16^^", write, user);
  mpz_get_str(str, base, PMATH_AS_MPZ(integer));
  
  if(base != 10) {
    char basestr[6];
    
    if(str[0] == '-') {
      snprintf(basestr, sizeof(basestr), "-%d^^", base);
      write_cstr(basestr, info->write, info->user);
      write_cstr(str + 1,   info->write, info->user);
    }
    else {
      snprintf(basestr, sizeof(basestr), "%d^^", base);
      write_cstr(basestr, info->write, info->user);
      write_cstr(str,     info->write, info->user);
    }
  }
  else
    write_cstr(str, info->write, info->user);
    
  pmath_mem_free(str);
}

//} ============================================================================
//{ pMath object functions for quotients ...

static void destroy_quotient(pmath_t quotient) {
  assert(pmath_refcount(quotient) == 0);
  
  pmath_unref(PMATH_QUOT_NUM(quotient));
  pmath_unref(PMATH_QUOT_DEN(quotient));
  
  pmath_mem_free(PMATH_AS_PTR(quotient));
}

static unsigned int hash_quotient(pmath_t quotient) {
  unsigned int next = 0;
  unsigned int h;
  
  h    = pmath_hash(PMATH_QUOT_NUM(quotient));
  next = incremental_hash(&h, sizeof(h), next);
  h    = pmath_hash(PMATH_QUOT_DEN(quotient));
  return incremental_hash(&h, sizeof(h), next);
}

static void write_quotient(struct pmath_write_ex_t *info, pmath_t quotient) {
  pmath_write_ex(info, PMATH_QUOT_NUM(quotient));
  write_cstr("/", info->write, info->user);
  pmath_write_ex(info, PMATH_QUOT_DEN(quotient));
}

//} ============================================================================
//{ pMath object functions for multi precision floats ...

static void destroy_mp_float(pmath_t f) {
  uintptr_t i = mp_cache_inc(+1);
  struct _pmath_mp_float_t *f_ptr;
  
  assert(pmath_refcount(f) == 0);
  
  f_ptr = (void *)PMATH_AS_PTR(f);
  f_ptr = mp_cache_swap(i, f_ptr);
  if(f_ptr) {
    assert(f_ptr->inherited.refcount._data == 0);
    
    mpfr_clear(f_ptr->value);
    mpfr_clear(f_ptr->error);
    pmath_mem_free(f_ptr);
  }
}

static unsigned int hash_mp_float(pmath_t f) {
  unsigned int h = 0;
  h = incremental_hash(&PMATH_AS_MP_VALUE(f)[0]._mpfr_prec, sizeof(mpfr_prec_t), h);
  h = incremental_hash(&PMATH_AS_MP_VALUE(f)[0]._mpfr_sign, sizeof(mpfr_sign_t), h);
  h = incremental_hash(&PMATH_AS_MP_VALUE(f)[0]._mpfr_exp,  sizeof(mp_exp_t), h);
  
  return incremental_hash(
           PMATH_AS_MP_VALUE(f)[0]._mpfr_d,
           sizeof(mp_limb_t) * (size_t)ceil(PMATH_AS_MP_VALUE(f)[0]._mpfr_prec / (double)mp_bits_per_limb),
           h);
}

static void write_short_double(
  double   d,
  void (*write)(void *, const uint16_t *, int),
  void    *user
) {
  char s[100];
  double test;
  int maxprec = 1 + (int)ceil(DBL_MANT_DIG * LOG10_2);
  int len, i;
  
  for(len = 1; len <= maxprec; ++len) {
    snprintf(s, sizeof(s), "%.*f", len, d);
    
    // not pmath_strtod() because sprintf gives locale specific result
    test = strtod(s, NULL);
    if(test == d)
      break;
  }
  
  for(i = 0; i < len; ++i)
    if(s[i] == ',')
      s[i] = '.';
      
  write_cstr(s, write, user);
}

static void delete_trailing_zeros(char *s) {
  char *s2 = s + strlen(s) - 1;
  
  while(s2 != s && *s2 == '0')
    --s2;
    
  s2[1] = '\0';
}

static void write_mp_float_ex(
  struct pmath_write_ex_t *info,
  pmath_t f,
  pmath_bool_t for_machine_float
) {
  pmath_thread_t thread = pmath_thread_get_current();
  int base = 10;
  char basestr[10];
  mp_exp_t exp;
  size_t digits, size;
  char *str;
  double prec10 = pmath_precision(pmath_ref(f));
  //double acc10  = pmath_accuracy(pmath_ref(f));
  double base_prec;
  //double base_acc;
  
  if(thread && thread->numberbase >= 2 && thread->numberbase <= 36)
    base = thread->numberbase;
    
  if(base == 2) {
    base_prec = prec10;
    //base_acc  = acc10;
  }
  else if(base == 10) {
    base_prec = LOG10_2 * prec10;
    //base_acc  = LOG10_2 * acc10;
  }
  else {
    base_prec = prec10 * LOGE_2 / log(base);
    //base_acc  = acc10  * LOGE_2 / log(base);
  }
  prec10 *= LOG10_2;
  //acc10 *= LOG10_2;
  
  if(mpfr_zero_p(PMATH_AS_MP_VALUE(f)) || prec10 == 0) {
    long exp;
    double d = mpfr_get_d_2exp(&exp, PMATH_AS_MP_ERROR(f), MPFR_RNDN);
    
    if(base != 10) {
      snprintf(basestr, sizeof(basestr), "%d^^", base);
      write_cstr(basestr, info->write, info->user);
    }
    
    if(for_machine_float) {
      if(info->options & PMATH_WRITE_OPTIONS_INPUTEXPR)
        write_cstr("0.0`", info->write, info->user);
      else
        write_cstr("0.0", info->write, info->user);
    }
    else if(info->options & PMATH_WRITE_OPTIONS_INPUTEXPR) {
      d = exp * LOG10_2 + log10(d);
      
      write_cstr("0``", info->write, info->user);
      write_short_double(-d, info->write, info->user);
    }
    else {
      char s[30];
      
      if(base == 10)
        d = exp * LOG10_2 + log10(d);
      else if(base == 2)
        d = exp + log(d) / LOGE_2;
      else
        d = (exp * LOGE_2 + log(d)) / log(base);
        
      snprintf(s, sizeof(s), "0.0*^%"PRIdMAX, (intmax_t)d);
      write_cstr(s, info->write, info->user);
    }
    
    return;
  }
  
  //digits = 1 + (size_t)(floor(base_prec - base_acc) + base_acc);
  digits = 1 + (size_t)ceil(base_prec);
  if(digits < 2)
    digits = 2;
    
  size = 7;
  if(digits > 5)
    size = digits + 2;
    
  str = (char *)pmath_mem_alloc(size);
  if(!str) {
    write_cstr("<<out-of-memory>>", info->write, info->user);
    return;
  }
  
  mpfr_get_str(str, &exp, base, digits, PMATH_AS_MP_VALUE(f), MPFR_RNDN);
  
  if(exp == 0) {
    if(*str == '-') {
      if(for_machine_float)
        delete_trailing_zeros(str + 1);
        
      if(base != 10) {
        snprintf(basestr, sizeof(basestr), "-%d^^0.", base);
        write_cstr(basestr, info->write, info->user);
      }
      else
        write_cstr("-0.", info->write, info->user);
        
      write_cstr(str + 1, info->write, info->user);
    }
    else {
      if(for_machine_float)
        delete_trailing_zeros(str);
        
      if(base != 10) {
        snprintf(basestr, sizeof(basestr), "%d^^0.", base);
        write_cstr(basestr, info->write, info->user);
      }
      else
        write_cstr("0.", info->write, info->user);
        
      write_cstr(str,    info->write, info->user);
    }
    
    if(info->options & PMATH_WRITE_OPTIONS_INPUTEXPR) {
      write_cstr("`", info->write, info->user);
      
      if(!for_machine_float)
        write_short_double(prec10, info->write, info->user);
    }
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
        write_cstr(basestr, info->write, info->user);
        write_cstr(str + 1,   info->write, info->user);
      }
      else {
        snprintf(basestr, sizeof(basestr), "%d^^", base);
        write_cstr(basestr, info->write, info->user);
        write_cstr(str,     info->write, info->user);
      }
    }
    else {
      write_cstr(str, info->write, info->user);
    }
    write_cstr(".", info->write, info->user);
    str[exp] = c;
    
    if(for_machine_float)
      delete_trailing_zeros(str + exp);
      
    write_cstr(str + exp, info->write, info->user);
    
    if(info->options & PMATH_WRITE_OPTIONS_INPUTEXPR) {
      write_cstr("`", info->write, info->user);
      
      if(!for_machine_float)
        write_short_double(prec10, info->write, info->user);
    }
  }
  else if(exp < 0 && exp > -5) {
    static const uint16_t zero_char = '0';
    
    int start;
    if(*str == '-') {
      if(base != 10) {
        snprintf(basestr, sizeof(basestr), "-%d^^0.", base);
        write_cstr(basestr, info->write, info->user);
      }
      else
        write_cstr("-0.", info->write, info->user);
      start = 1;
    }
    else {
      if(base != 10) {
        snprintf(basestr, sizeof(basestr), "%d^^0.", base);
        write_cstr(basestr, info->write, info->user);
      }
      else
        write_cstr("0.", info->write, info->user);
      start = 0;
    }
    
    do {
      info->write(info->user, &zero_char, 1);
    } while(++exp < 0);
    
    if(for_machine_float)
      delete_trailing_zeros(str + start);
      
    write_cstr(str + start, info->write, info->user);
    
    if(info->options & PMATH_WRITE_OPTIONS_INPUTEXPR) {
      write_cstr("`", info->write, info->user);
      
      if(!for_machine_float)
        write_short_double(prec10, info->write, info->user);
    }
  }
  else {
    int start;
    if(*str == '\0') { // 0.0
      if(base != 10) {
        snprintf(basestr, sizeof(basestr), "%d^^0.", base);
        write_cstr(basestr, info->write, info->user);
      }
      else
        write_cstr("0.", info->write, info->user);
      start = 0;
    }
    else if(*str == '-') {
      if(base != 10) {
        snprintf(basestr, sizeof(basestr), "-%d^^%c.", base, str[1]);
        write_cstr(basestr, info->write, info->user);
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
        write_cstr(basestr, info->write, info->user);
      }
      
      info->write(info->user, ustr, 2);
      start = 1;
      --exp;
    }
    
    if(for_machine_float)
      delete_trailing_zeros(str + start);
      
    write_cstr(str + start, info->write, info->user);
    
    if(info->options & PMATH_WRITE_OPTIONS_INPUTEXPR) {
      write_cstr("`", info->write, info->user);
      
      if(!for_machine_float)
        write_short_double(prec10, info->write, info->user);
    }
    
    if(exp != 0) {
      char s[30];
      snprintf(s, sizeof(s), "*^%"PRIdMAX, (intmax_t)exp);
      write_cstr(s, info->write, info->user);
    }
  }
  
  pmath_mem_free(str);
}

static void write_mp_float(struct pmath_write_ex_t *info, pmath_t f) {
  write_mp_float_ex(info, f, FALSE);
}

//} ============================================================================
//{ pMath object functions for machine floats ...

PMATH_PRIVATE
void _pmath_write_machine_float(struct pmath_write_ex_t *info, pmath_t f) {
  pmath_thread_t thread = pmath_thread_get_current();
  int base = 10;
  
  if(thread && thread->numberbase >= 2 && thread->numberbase <= 36)
    base = thread->numberbase;
    
  /*if(base == 10) {
    char s[100];
    double test;
    int minprec, midprec, maxprec, len, i;
    maxprec = 1 + (int)ceil(DBL_MANT_DIG * LOG10_2);
    midprec = 6;
  
    test = fabs(PMATH_AS_DOUBLE(f));
    if(test > 10000.0)
      minprec = 6;
    else if(test > 1000.0)
      minprec = 5;
    else if(test > 100.0)
      minprec = 4;
    else if(test > 10.0)
      minprec = 3;
    else if(test > 1.0)
      minprec = 2;
    else
      minprec = 1;
  
    if(test < 10.0)
      midprec = maxprec;
  
    for(len = minprec; len <= midprec; ++len) {
      snprintf(s, sizeof(s), "%.*g", len, PMATH_AS_DOUBLE(f));
  
      // not pmath_strtod() because sprintf gives locale specific result
      test = strtod(s, NULL);
      if(test == PMATH_AS_DOUBLE(f))
        goto FOUND;
    }
  
    for(len = midprec + 1; len <= maxprec; ++len) {
      snprintf(s, sizeof(s), "%.*e", len, PMATH_AS_DOUBLE(f));
  
      // not pmath_strtod() because sprintf gives locale specific result
      test = strtod(s, NULL);
      if(test == PMATH_AS_DOUBLE(f))
        goto FOUND;
    }
  
  FOUND:
  
    len = strlen(s);
    i = 0;
    while(i < len && s[i] != '.' && s[i] != ',' && s[i] != 'e')
      ++i;
  
    if(i < len && s[i] == ',')
      s[i] = '.';
  
    if(i == len) {
      write_cstr(s, info->write, info->user);
      if(info->options & PMATH_WRITE_OPTIONS_INPUTEXPR)
        write_cstr(".0`", info->write, info->user);
      else
        write_cstr(".0", info->write, info->user);
      return;
    }
  
    if(s[i] == 'e') {
      int exp = atoi(s + i + 1);
  
      s[i] = '\0';
      write_cstr(s, info->write, info->user);
  
      if(exp > 0 && exp < 6) {
        char zeros[] = "000000";
        zeros[exp] = '\0';
        write_cstr(zeros, info->write, info->user);
        if(info->options & PMATH_WRITE_OPTIONS_INPUTEXPR)
          write_cstr(".0`", info->write, info->user);
        else
          write_cstr(".0", info->write, info->user);
        return;
      }
      else {
        if(info->options & PMATH_WRITE_OPTIONS_INPUTEXPR)
          write_cstr(".0`*^", info->write, info->user);
        else
          write_cstr(".0*^", info->write, info->user);
  
        snprintf(s, sizeof(s), "%d", exp);
        write_cstr(s, info->write, info->user);
        return;
      }
    }
  
    while(i < len && s[i] != 'e')
      ++i;
  
    if(i < len) {
      int exp = atoi(s + i + 1);
  
      s[i] = '\0';
      write_cstr(s, info->write, info->user);
      if(info->options & PMATH_WRITE_OPTIONS_INPUTEXPR)
        write_cstr("`*^", info->write, info->user);
      else
        write_cstr("*^", info->write, info->user);
  
      snprintf(s, sizeof(s), "%d", exp);
      write_cstr(s, info->write, info->user);
      return;
    }
  
    write_cstr(s, info->write, info->user);
    if(info->options & PMATH_WRITE_OPTIONS_INPUTEXPR)
      write_cstr("`", info->write, info->user);
  
    return;
  }
  */
  
  {
    pmath_mpfloat_t mpf = _pmath_create_mp_float_from_d(PMATH_AS_DOUBLE(f));
    
    write_mp_float_ex(info, mpf, TRUE);
    
    pmath_unref(mpf);
    return;
  }
}

//} ============================================================================
//{ common pMath object functions for all number types ...

PMATH_PRIVATE
int _pmath_numbers_compare(
  pmath_number_t numA,
  pmath_number_t numB
) {
#define RETURN_SIMPLE_CMP \
  if(a < b)    \
    return -1; \
  if(a > b)    \
    return 1;  \
  return 0;
  
  if(pmath_is_int32(numA)) {
    int a = PMATH_AS_INT32(numA);
    
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
        return -mpfr_cmp_si(PMATH_AS_MP_VALUE(numB), a);
        
      case PMATH_TYPE_SHIFT_QUOTIENT: {
          double b = pmath_number_get_d(PMATH_QUOT_NUM(numB)) / pmath_number_get_d(PMATH_QUOT_DEN(numB));
          
          RETURN_SIMPLE_CMP
        } return 0;
    }
    
    assert("unknown number type" && 0);
    return 0;
  }
  
  if(pmath_is_double(numA)) {
    double a = PMATH_AS_DOUBLE(numA);
    
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
        return -mpz_cmp_d(PMATH_AS_MPZ(numB), a);
        
      case PMATH_TYPE_SHIFT_MP_FLOAT:
        return -mpfr_cmp_d(PMATH_AS_MP_VALUE(numB), a);
        
      case PMATH_TYPE_SHIFT_QUOTIENT: {
          double b = pmath_number_get_d(PMATH_QUOT_NUM(numB)) / pmath_number_get_d(PMATH_QUOT_DEN(numB));
          
          RETURN_SIMPLE_CMP
        } return 0;
    }
    
    assert("unknown number type" && 0);
    return 0;
  }
  
#undef RETURN_SIMPLE_CMP
  
  if(pmath_is_double(numB) || pmath_is_int32(numB))
    return - _pmath_numbers_compare(numB, numA);
    
  assert(pmath_is_pointer(numA));
  assert(pmath_is_pointer(numB));
  assert(!pmath_is_null(numA));
  assert(!pmath_is_null(numB));
  
  if(pmath_is_mpint(numA)) {
    if(pmath_is_mpint(numB))
      return mpz_cmp(PMATH_AS_MPZ(numA), PMATH_AS_MPZ(numB));
      
    if(pmath_is_quotient(numB)) {
      // cmp(u, w/x) = cmp(u*x,w)  because x > 0
      pmath_integer_t lhs = _mul_ii(
                              pmath_ref(numA),
                              pmath_rational_denominator(numB));
      pmath_integer_t rhs = pmath_rational_numerator(numB);
      int result = pmath_compare(lhs, rhs);
      pmath_unref(lhs);
      pmath_unref(rhs);
      return result;
    }
    
    assert(pmath_is_mpfloat(numB));
    
    return -mpfr_cmp_z(
             PMATH_AS_MP_VALUE(numB),
             PMATH_AS_MPZ(numA));
  }
  
  if(pmath_is_quotient(numA)) {
    if(pmath_is_quotient(numB)) {
      // cmp(u/v, w/x) = cmp(u*x,v*w)  because v > 0 && x > 0
      pmath_integer_t lhs = _mul_ii(
                              pmath_rational_numerator(numA),
                              pmath_rational_denominator(numB));
                              
      pmath_integer_t rhs = _mul_ii(
                              pmath_rational_numerator(numB),
                              pmath_rational_denominator(numA));
                              
      int result = pmath_compare(lhs, rhs);
      
      pmath_unref(lhs);
      pmath_unref(rhs);
      return result;
    }
    
    if(pmath_is_mpfloat(numB)) {
      mpfr_prec_t prec = mpfr_get_prec(PMATH_AS_MP_VALUE(numB));
      pmath_float_t tmp  = _pmath_create_mp_float(prec);
      pmath_float_t tmp2 = _pmath_create_mp_float(prec);
      int result;
      
      if(pmath_is_null(tmp) || pmath_is_null(tmp2)) {
        pmath_unref(tmp);
        pmath_unref(tmp2);
        return 1;
      }
      
      if(pmath_is_int32(PMATH_QUOT_NUM(numA))) {
        mpfr_set_si(
          PMATH_AS_MP_VALUE(tmp),
          PMATH_AS_INT32(PMATH_QUOT_NUM(numA)),
          MPFR_RNDN);
      }
      else {
        assert(pmath_is_mpint(PMATH_QUOT_NUM(numA)));
        
        mpfr_set_z(
          PMATH_AS_MP_VALUE(tmp),
          PMATH_AS_MPZ(PMATH_QUOT_NUM(numA)),
          MPFR_RNDN);
      }
      
      if(pmath_is_int32(PMATH_QUOT_DEN(numA))) {
        mpfr_div_si(
          PMATH_AS_MP_VALUE(tmp2),
          PMATH_AS_MP_VALUE(tmp),
          PMATH_AS_INT32(PMATH_QUOT_DEN(numA)),
          MPFR_RNDN);
      }
      else {
        assert(pmath_is_mpint(PMATH_QUOT_DEN(numA)));
        
        mpfr_div_z(
          PMATH_AS_MP_VALUE(tmp2),
          PMATH_AS_MP_VALUE(tmp),
          PMATH_AS_MPZ(PMATH_QUOT_DEN(numA)),
          MPFR_RNDN);
      }
      
      result = mpfr_cmp(PMATH_AS_MP_VALUE(tmp2), PMATH_AS_MP_VALUE(numB));
      
      pmath_unref(tmp);
      pmath_unref(tmp2);
      
      return result;
    }
    
    return -_pmath_numbers_compare(numB, numA);
  }
  
  if(pmath_is_mpfloat(numA)) {
    if(pmath_is_mpfloat(numB)) {
      mpfr_prec_t precA = mpfr_get_prec(PMATH_AS_MP_VALUE(numA));
      mpfr_prec_t precB = mpfr_get_prec(PMATH_AS_MP_VALUE(numB));
      
      if(precA < precB) {
        pmath_float_t tmp = _pmath_create_mp_float(precA);
        int result;
        
        if(!pmath_is_null(tmp)) {
          mpfr_set(PMATH_AS_MP_VALUE(tmp), PMATH_AS_MP_VALUE(numB), MPFR_RNDN);
          
          result = mpfr_cmp(PMATH_AS_MP_VALUE(numA), PMATH_AS_MP_VALUE(tmp));
          
          pmath_unref(tmp);
          return result;
        }
      }
      else if(precA > precB) {
        pmath_float_t tmp = _pmath_create_mp_float(precB);
        int result;
        
        if(!pmath_is_null(tmp)) {
          mpfr_set(PMATH_AS_MP_VALUE(tmp), PMATH_AS_MP_VALUE(numA), MPFR_RNDN);
          
          result = mpfr_cmp(PMATH_AS_MP_VALUE(numA), PMATH_AS_MP_VALUE(tmp));
          
          pmath_unref(tmp);
          return result;
        }
      }
      
      return mpfr_cmp(PMATH_AS_MP_VALUE(numA), PMATH_AS_MP_VALUE(numB));
    }
    
    return -_pmath_numbers_compare(numB, numA);
  }
  
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
    return _pmath_numbers_equal(numB, numA);
    
  if( pmath_is_mpfloat(numA) &&
      pmath_is_mpfloat(numB))
  {
    mpfr_prec_t precA = mpfr_get_prec(PMATH_AS_MP_VALUE(numA));
    mpfr_prec_t precB = mpfr_get_prec(PMATH_AS_MP_VALUE(numB));
    
    if(precA < precB) {
      pmath_float_t tmp = _pmath_create_mp_float(precA);
      pmath_bool_t result;
      
      if(!pmath_is_null(tmp)) {
        mpfr_set(PMATH_AS_MP_VALUE(tmp), PMATH_AS_MP_VALUE(numB), MPFR_RNDN);
        
        result = mpfr_equal_p(PMATH_AS_MP_VALUE(numA), PMATH_AS_MP_VALUE(tmp));
        
        pmath_unref(tmp);
        return result;
      }
    }
    else if(precA > precB) {
      pmath_float_t tmp = _pmath_create_mp_float(precB);
      pmath_bool_t result;
      
      if(!pmath_is_null(tmp)) {
        mpfr_set(PMATH_AS_MP_VALUE(tmp), PMATH_AS_MP_VALUE(numA), MPFR_RNDN);
        
        result = mpfr_equal_p(PMATH_AS_MP_VALUE(tmp), PMATH_AS_MP_VALUE(numA));
        
        pmath_unref(tmp);
        return result;
      }
    }
    
    return mpfr_equal_p(PMATH_AS_MP_VALUE(numA), PMATH_AS_MP_VALUE(numB));
  }
  
  return 0 == _pmath_numbers_compare(numA, numB);
}

//} ============================================================================
//{ module handling functions ...

PMATH_PRIVATE void _pmath_numbers_memory_panic(void) {
//  destroy_all_unused_quotients();
  int_cache_clear();
  mp_cache_clear();
  mpfr_free_cache();
}

PMATH_PRIVATE pmath_bool_t _pmath_numbers_init(void) {
  char c = 0;
  hash_init = incremental_hash(&c, 1, 0);
  
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

PMATH_PRIVATE void _pmath_numbers_done(void) {
  _pmath_primetest_done();
  pmath_unref(_pmath_one_half);
  
  int_cache_clear();
  mp_cache_clear();
  mpfr_free_cache();
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
