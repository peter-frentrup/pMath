#include <pmath-core/numbers-private.h>
#include <pmath-core/objects-private.h>
#include <pmath-core/strings-private.h>

#include <pmath-util/approximate.h>
#include <pmath-util/debug.h>
#include <pmath-util/incremental-hash-private.h>
#include <pmath-util/memory.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/number-theory-private.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>


#if __GNU_MP_VERSION < 4
  #error gmp version 4 or newer needed
#endif

#ifdef _MSC_VER
  #define snprintf sprintf_s
#endif

PMATH_PRIVATE pmath_quotient_t _pmath_one_half; /* readonly */

PMATH_PRIVATE pmath_t _pmath_object_overflow;          /* readonly */
PMATH_PRIVATE pmath_t _pmath_object_underflow;         /* readonly */
PMATH_PRIVATE pmath_t _pmath_object_infinity;          /* readonly */
PMATH_PRIVATE pmath_t _pmath_object_complex_infinity;  /* readonly */

PMATH_PRIVATE gmp_randstate_t  _pmath_randstate;
PMATH_PRIVATE PMATH_DECLARE_ATOMIC(_pmath_rand_spinlock);

#define CACHE_SIZE 256
#define CACHE_MASK (CACHE_SIZE-1)

/* Some implementation ideas:
   http://reference.wolfram.com/mathematica/note/SomeNotesOnInternalImplementation.html
 */

/*============================================================================*/
//{ caching unused integers ...

static intptr_t int_cache[CACHE_SIZE];
static PMATH_DECLARE_ATOMIC(int_cache_pos);

#ifdef PMATH_DEBUG_LOG
static PMATH_DECLARE_ATOMIC(int_cache_hits);
static PMATH_DECLARE_ATOMIC(int_cache_misses);
#endif

  static uintptr_t int_cache_inc(intptr_t delta){
    return (uintptr_t)pmath_atomic_fetch_add(&int_cache_pos, delta);
  }

  static struct _pmath_integer_t_ *int_cache_swap(
    uintptr_t                 i, 
    struct _pmath_integer_t_ *value
  ){
    i = i & CACHE_MASK;
    
    assert(!value || value->inherited.refcount == 0);
    
    return (struct _pmath_integer_t_*)
      pmath_atomic_fetch_set(&int_cache[i], (intptr_t)value);
  }

  static void int_cache_clear(void){
    uintptr_t i;
    
    for(i = 0;i < CACHE_SIZE;++i){
      struct _pmath_integer_t_ *integer = int_cache_swap(i, NULL);
      
      if(integer){
        assert(integer->inherited.refcount == 0);
        
        mpz_clear(integer->value);
        pmath_mem_free(integer);
      }
    }
  }

  PMATH_PRIVATE pmath_integer_t _pmath_create_integer(void){
    struct _pmath_integer_t_ *integer;
    
    uintptr_t i = int_cache_inc(-1);
    integer = int_cache_swap(i-1, NULL);
    if(integer){
      #ifdef PMATH_DEBUG_LOG
        (void)pmath_atomic_fetch_add(&int_cache_hits, 1);
      #endif
      
      assert(integer->inherited.refcount == 0);
      integer->inherited.refcount = 1;
      
      return PMATH_FROM_PTR(integer);
    }
    else{
      #ifdef PMATH_DEBUG_LOG
        (void)pmath_atomic_fetch_add(&int_cache_misses, 1);
      #endif
    }
    
    integer = (void*)PMATH_AS_PTR(_pmath_create_stub(
      PMATH_TYPE_SHIFT_INTEGER,
      sizeof(struct _pmath_integer_t_)));

    if(integer)
      mpz_init(integer->value);

    return PMATH_FROM_PTR(integer);
  }

//} ============================================================================
//{ creating quotients ...

  PMATH_PRIVATE pmath_quotient_t _pmath_create_quotient(
    pmath_integer_t numerator,   // will be freed
    pmath_integer_t denominator  // will be freed
  ){
    struct _pmath_quotient_t_ *quotient;

    if(pmath_is_null(numerator) || pmath_is_null(denominator)){
      pmath_unref(numerator);
      pmath_unref(denominator);
      return PMATH_NULL;
    }

    quotient = (void*)PMATH_AS_PTR(_pmath_create_stub(
      PMATH_TYPE_SHIFT_QUOTIENT,
      sizeof(struct _pmath_quotient_t_)));

    if(!quotient){
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

static intptr_t mp_cache[CACHE_SIZE];
static PMATH_DECLARE_ATOMIC(mp_cache_pos);

#ifdef PMATH_DEBUG_LOG
static PMATH_DECLARE_ATOMIC(mp_cache_hits);
static PMATH_DECLARE_ATOMIC(mp_cache_misses);
#endif

  static uintptr_t mp_cache_inc(intptr_t delta){
    return (uintptr_t)pmath_atomic_fetch_add(&mp_cache_pos, delta);
  }

  static struct _pmath_mp_float_t_ *mp_cache_swap(
    uintptr_t                  i, 
    struct _pmath_mp_float_t_ *f
  ){
    i = i & CACHE_MASK;
    
    return (struct _pmath_mp_float_t_*)
      pmath_atomic_fetch_set(&mp_cache[i], (intptr_t)f);
  }

  static void mp_cache_clear(void){
    uintptr_t i;
    
    for(i = 0;i < CACHE_SIZE;++i){
      struct _pmath_mp_float_t_ *f = mp_cache_swap(i, NULL);
      
      if(f){
        assert(f->inherited.refcount == 0);
        
        mpfr_clear(f->value);
        mpfr_clear(f->error);
        pmath_mem_free(f);
      }
    }
  }

  PMATH_PRIVATE pmath_float_t _pmath_create_mp_float(mp_prec_t precision){
    struct _pmath_mp_float_t_ *f;
    uintptr_t i;

    if(precision == 0)
      precision = mpfr_get_default_prec();
    else if(precision < MPFR_PREC_MIN)
      precision = MPFR_PREC_MIN;
    else if(precision > PMATH_MP_PREC_MAX) // MPFR_PREC_MAX is too big! (not enough memory)
      precision =       PMATH_MP_PREC_MAX; // overflow error message?
    
    i = mp_cache_inc(-1);
    f = mp_cache_swap(i-1, NULL);
    if(f){
      #ifdef PMATH_DEBUG_LOG
        (void)pmath_atomic_fetch_add(&mp_cache_hits, 1);
      #endif
  
      assert(f->inherited.refcount == 0);
      f->inherited.refcount = 1;
      
      mpfr_set_prec(f->value, precision);
      mpfr_set_ui_2exp(f->error, 1, -(mp_exp_t)precision-1, MPFR_RNDU);
      return PMATH_FROM_PTR(f);
    }
    else{
      #ifdef PMATH_DEBUG_LOG
        (void)pmath_atomic_fetch_add(&mp_cache_misses, 1);
      #endif
    }

    f = (void*)PMATH_AS_PTR(_pmath_create_stub(
      PMATH_TYPE_SHIFT_MP_FLOAT,
      sizeof(struct _pmath_mp_float_t_)));

    if(!f)
      return PMATH_NULL;

    mpfr_init2(f->value, precision);
    mpfr_init2(f->error, PMATH_MP_ERROR_PREC);
    mpfr_set_ui_2exp(f->error, 1, -(mp_exp_t)precision-1, MPFR_RNDU);

    return PMATH_FROM_PTR(f);
  }

  PMATH_PRIVATE
  pmath_float_t _pmath_create_mp_float_from_d(double value){
    pmath_float_t result = _pmath_create_mp_float(0);

    if(!pmath_is_null(result)){
      mpfr_set_d(PMATH_AS_MP_VALUE(result), value, MPFR_RNDN);
      mpfr_set_d(PMATH_AS_MP_ERROR(result), value, MPFR_RNDN);
      mpfr_abs(  PMATH_AS_MP_ERROR(result), PMATH_AS_MP_ERROR(result), MPFR_RNDU);
      mpfr_div_2si(
        PMATH_AS_MP_ERROR(result), 
        PMATH_AS_MP_ERROR(result), 
        mpfr_get_prec(PMATH_AS_MP_VALUE(result)), 
        MPFR_RNDU);
    }
    
    return result;
  }

  PMATH_PRIVATE
  pmath_float_t _pmath_convert_to_mp_float(pmath_float_t n){ // n will be freed
    pmath_float_t result;

    if(pmath_is_mpfloat(n))
      return n;
    
    result = _pmath_create_mp_float_from_d(pmath_number_get_d(n));
    pmath_unref(n);

    return result;
  }
  
//} ============================================================================
//{ creating machine floats ...

static intptr_t maf_cache[CACHE_SIZE];
static PMATH_DECLARE_ATOMIC(maf_cache_pos);

#ifdef PMATH_DEBUG_LOG
static PMATH_DECLARE_ATOMIC(maf_cache_hits);
static PMATH_DECLARE_ATOMIC(maf_cache_misses);
#endif

  static uintptr_t maf_cache_inc(intptr_t delta){
    return (uintptr_t)pmath_atomic_fetch_add(&maf_cache_pos, delta);
  }

  static struct _pmath_machine_float_t_ *maf_cache_swap(
    uintptr_t                       i, 
    struct _pmath_machine_float_t_ *f
  ){
    i = i & CACHE_MASK;
    
    return (void*)pmath_atomic_fetch_set(&maf_cache[i], (intptr_t)f);
  }

  static void maf_cache_clear(void){
    uintptr_t i;
    
    for(i = 0;i < CACHE_SIZE;++i){
      pmath_mem_free(maf_cache_swap(i, NULL));
    }
  }

  PMATH_PRIVATE
  pmath_float_t _pmath_create_machine_float(double value){
    struct _pmath_machine_float_t_ *f;
    
    uintptr_t i = maf_cache_inc(-1);
    f = maf_cache_swap(i-1, NULL);
    if(f){
      #ifdef PMATH_DEBUG_LOG
        (void)pmath_atomic_fetch_add(&maf_cache_hits, 1);
      #endif
  
      assert(f->inherited.refcount == 0);
      f->inherited.refcount = 1;
      
      f->value = value;
      
      return PMATH_FROM_PTR(f);
    }
    else{
      #ifdef PMATH_DEBUG_LOG
        (void)pmath_atomic_fetch_add(&maf_cache_misses, 1);
      #endif
    }
    
    f = (void*)PMATH_AS_PTR(_pmath_create_stub(
      _DEPRECATED_PMATH_TYPE_SHIFT_MACHINE_FLOAT,
      sizeof(struct _pmath_machine_float_t_)));
    
    if(f)
      f->value = value;

    return PMATH_FROM_PTR(f);
  }

//} ============================================================================
//{ integer constructors ...

#define SPECIAL_MIN (-1)
#define SPECIAL_MAX (10)

static pmath_integer_t special_values_wrong_indices[SPECIAL_MAX - SPECIAL_MIN + 1];
PMATH_PRIVATE pmath_integer_t *special_values = special_values_wrong_indices - SPECIAL_MIN;

PMATH_API pmath_integer_t pmath_integer_new_si(signed long int si){
  pmath_integer_t integer;
  if(si >= SPECIAL_MIN && si <= SPECIAL_MAX)
    return pmath_ref(special_values[si]);

  integer = _pmath_create_integer();
  if(pmath_is_null(integer))
    return PMATH_NULL;

  mpz_set_si(PMATH_AS_MPZ(integer), si);
  return integer;
}

PMATH_API pmath_integer_t pmath_integer_new_ui(unsigned long int ui){
  pmath_integer_t integer;
  if(ui <= SPECIAL_MAX)
    return pmath_ref(special_values[ui]);

  integer = _pmath_create_integer();
  if(pmath_is_null(integer))
    return PMATH_NULL;

  mpz_set_ui(PMATH_AS_MPZ(integer), ui);
  return integer;
}

PMATH_API pmath_integer_t pmath_integer_new_size(size_t size){
  pmath_integer_t integer;
  if(size <= SPECIAL_MAX)
    return pmath_ref(special_values[size]);

  integer = _pmath_create_integer();
  if(pmath_is_null(integer))
    return PMATH_NULL;

  #if defined(PMATH_OS_WIN32) && PMATH_BITSIZE == 64
    mpz_import(
      PMATH_AS_MPZ(integer),
      1,
      -1,
      sizeof(size_t),
      0,
      0,
      &size);
  #else
    mpz_set_ui(PMATH_AS_MPZ(integer), (unsigned long)size);
  #endif
  
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
){
  pmath_integer_t integer = _pmath_create_integer();
  
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
    
  return integer;
}

PMATH_API pmath_integer_t pmath_integer_new_str(const char *str, int base){
  pmath_integer_t integer = _pmath_create_integer();
  
  if(pmath_is_null(integer))
    return PMATH_NULL;

  if(mpz_set_str(PMATH_AS_MPZ(integer), str, base)){
    pmath_unref(integer);
    return PMATH_NULL;
  }

  return integer;
}

//} ============================================================================
//{ quotient constructor and access functions ...

PMATH_API pmath_rational_t pmath_rational_new(
  pmath_integer_t  numerator,
  pmath_integer_t  denominator
){
  if(pmath_is_null(numerator) || pmath_is_null(denominator)){
    pmath_unref(numerator);
    pmath_unref(denominator);
    return PMATH_NULL;
  }
  
  if(pmath_number_sign(denominator) == 0){ // pmath_number_sign(PMATH_NULL) = 0
    pmath_unref(numerator);
    pmath_unref(denominator);
    return PMATH_NULL;
  }

  // fast check: n/1 -> n
  if(mpz_cmp_ui(PMATH_AS_MPZ(denominator), 1) == 0){
    pmath_unref(denominator);
    return numerator;
  }

  //{ canonical form ...
  if(PMATH_AS_PTR(numerator)->refcount > 1){
    pmath_integer_t unique_num = _pmath_create_integer();
    
    if(pmath_is_null(unique_num)){
      pmath_unref(numerator);
      pmath_unref(denominator);
      return PMATH_NULL;
    }
    
    mpz_set(PMATH_AS_MPZ(unique_num), PMATH_AS_MPZ(numerator));
    pmath_unref(numerator);
    numerator = unique_num;
  }

  if(PMATH_AS_PTR(denominator)->refcount > 1){
    pmath_integer_t unique_den = _pmath_create_integer();
    
    if(pmath_is_null(unique_den)){
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
  if(mpz_cmp_ui(PMATH_AS_MPZ(denominator), 1) == 0){
    pmath_unref(denominator);
    return numerator;
  }

  return _pmath_create_quotient(numerator, denominator);
}

PMATH_API pmath_integer_t pmath_rational_numerator(
  pmath_rational_t rational
){
  if(pmath_is_null(rational))
    return PMATH_NULL;

  if(pmath_is_integer(rational))
    return pmath_ref(rational);

  assert(pmath_is_quotient(rational));
  
  return pmath_ref(PMATH_QUOT_NUM(rational));
}

PMATH_API pmath_integer_t pmath_rational_denominator(
  pmath_rational_t rational
){
  if(pmath_is_null(rational))
    return PMATH_NULL;

  if(pmath_is_integer(rational))
    return pmath_integer_new_ui(1);

  assert(pmath_is_quotient(rational));
  
  return pmath_ref(PMATH_QUOT_DEN(rational));
}

//} ============================================================================
//{ float constructors and exception handling ...

PMATH_API 
pmath_number_t pmath_float_new_str(
  const char *              str, // digits.digits
  int                       base,
  pmath_precision_control_t precision_control,
  double                    base_precision_accuracy
){
  int i, len, int_digits, frac_digits;
  double log2_base = log2(base);
  pmath_bool_t automatic = FALSE;
  pmath_float_t f;
  
  if(base < 2 || base > 36)
    return PMATH_NULL;
  
  len = strlen(str);
  
  int_digits = 0;
  for(i = 0;i < len && (str[i] < '1' || str[i] > '9');++i){
  }
  
  for(;i < len && str[i] != '.';++i)
    ++int_digits;
  
  frac_digits = 0;
  if(i < len && str[i] == '.'){
    for(++i;i < len && str[i] != '.';++i)
      ++frac_digits;
  }
  
  if(precision_control == PMATH_PREC_CTRL_AUTO){
    automatic = TRUE;
    if((int_digits + frac_digits) * log2_base <= DBL_MANT_DIG){
      precision_control = PMATH_PREC_CTRL_MACHINE_PREC;
    }
    else{
      precision_control = PMATH_PREC_CTRL_GIVEN_PREC;
      base_precision_accuracy = int_digits + frac_digits;
    }
  }
  
  switch(precision_control){
    case PMATH_PREC_CTRL_MACHINE_PREC: {
      double x;
      
      if(base == 10){
      /* We use strtod() because the the formating of machine float numbers is
         done based on strtod to print the smallest number of fractional digits
         possible such that the output would produce the exact same value as 
         input (see write_machine_float()).
         
         Example -- Why we avoid mpfr_set_str():
         The input 34643574574574947.427457` equals the double number 
                   34643574574574948 and is printed as "3.464357457457495`*^16". 
         When that string is given to MakeExpression(), this function will be 
         called with str = "3.464357457457495e16".
         
         mpfr_set_str() would generate the number 34643574574574952, but 
         strtod() generates the value             34643574574574948, which we 
         want.
       */
        x = strtod(str, NULL);
        if(isfinite(x))
          return _pmath_create_machine_float(x);
      }
      
      f = _pmath_create_mp_float(DBL_MANT_DIG);
      if(pmath_is_null(f))
        return PMATH_NULL;

      mpfr_set_str(PMATH_AS_MP_VALUE(f), str, base, MPFR_RNDN);

      x = mpfr_get_d(PMATH_AS_MP_VALUE(f), MPFR_RNDN);
      pmath_unref(f);
      return pmath_float_new_d(x);
    };
    
    case PMATH_PREC_CTRL_GIVEN_PREC: {
      pmath_float_t tmp_err;
      double bits = base_precision_accuracy * log2_base;
      mp_prec_t prec;
      
      if(bits >= PMATH_MP_PREC_MAX)
        return PMATH_NULL;
      
      if(bits < 0)
        bits = 0;
      
      prec = (mp_prec_t)ceil(bits);
      
      f = _pmath_create_mp_float(prec >= MPFR_PREC_MIN ? prec : MPFR_PREC_MIN);
      if(pmath_is_null(f))
        return PMATH_NULL;
      
      tmp_err = _pmath_create_mp_float(PMATH_MP_ERROR_PREC);
      if(pmath_is_null(tmp_err)){
        pmath_unref(f);
        return PMATH_NULL;
      }
      
      mpfr_set_str(PMATH_AS_MP_VALUE(f), str, base, MPFR_RNDN);
      
      if(mpfr_zero_p(PMATH_AS_MP_VALUE(f))){
        pmath_unref(f);
        if(automatic)
          return pmath_float_new_d(0.0);
        return pmath_integer_new_si(0);
      }
      
      // error = |value| * 2 ^ -bits
      mpfr_abs(   PMATH_AS_MP_ERROR(f), PMATH_AS_MP_VALUE(f), MPFR_RNDU);
      mpfr_set_d( PMATH_AS_MP_VALUE(tmp_err), -bits, MPFR_RNDN);
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
      mp_prec_t prec;
      
      if(bits >= PMATH_MP_PREC_MAX)
        return PMATH_NULL;
      
      if(bits < 0)
        bits = 0;
      
      prec = (mp_prec_t)ceil(bits);
      
      f = _pmath_create_mp_float(prec >= MPFR_PREC_MIN ? prec : MPFR_PREC_MIN);
      if(pmath_is_null(f))
        return PMATH_NULL;
      
      mpfr_set_str(PMATH_AS_MP_VALUE(f), str, base, MPFR_RNDN);
      
      // error = base ^ -accuracy
      mpfr_set_d( PMATH_AS_MP_ERROR(f), -base_precision_accuracy, MPFR_RNDD);
      mpfr_ui_pow(PMATH_AS_MP_ERROR(f), base, PMATH_AS_MP_ERROR(f), MPFR_RNDU);

      _pmath_mp_float_normalize(f);
      return f;
    };
  
    default: ;
  }
  
  return PMATH_NULL;
}
  
PMATH_API pmath_float_t pmath_float_new_d(double dbl){
//  if(!isfinite(dbl))
//    return PMATH_NULL;
//
  return _pmath_create_machine_float(dbl);
//  struct _pmath_mp_float_t *f = _pmath_create_mp_float(PMATH_MACHINE_PRECISION);
//  if(!f)
//    return PMATH_NULL;
//
//  mpfr_set_d(f->value, dbl, MPFR_RNDN);
//  return (pmath_float_t)f;
}

PMATH_PRIVATE 
mp_prec_t _pmath_float_precision( // 0 = MachinePrecision
  pmath_float_t x // wont be freed
){
  if(pmath_is_mpfloat(x))
    return mpfr_get_prec(PMATH_AS_MP_VALUE(x));

  return 0;
}

PMATH_PRIVATE
pmath_t _pmath_float_exceptions(
  pmath_number_t x  // will be freed.
){
  pmath_t result = PMATH_NULL;

  if(pmath_is_double(x)){
    if(!isfinite(PMATH_AS_DOUBLE(x))){
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

  if(PMATH_UNLIKELY(mpfr_underflow_p())){
    pmath_message(PMATH_NULL, "unfl", 0);
    result = pmath_ref(_pmath_object_underflow);
  }
  else if(mpfr_overflow_p()
  || mpfr_zero_p(PMATH_AS_MP_ERROR(x))){
    pmath_message(PMATH_NULL, "ovfl", 0);
    result = pmath_ref(_pmath_object_overflow);
  }
  else if(mpfr_nan_p(PMATH_AS_MP_VALUE(x))){
    result = pmath_ref(PMATH_SYMBOL_UNDEFINED);
    pmath_message(PMATH_NULL, "indet", 1, pmath_ref(result));
  }
  else if(mpfr_inf_p(PMATH_AS_MP_VALUE(x))){
    result = pmath_expr_new_extended(
      pmath_ref(PMATH_SYMBOL_DIRECTEDINFINITY), 1,
      pmath_integer_new_si(mpfr_sgn(PMATH_AS_MP_VALUE(x))));
    pmath_message(PMATH_NULL, "infy", 1, pmath_ref(result));
  }
  else{
    mpfr_clear_flags();
    return x;
  }

  mpfr_clear_flags();
  pmath_unref(x);
  return result;
}

PMATH_PRIVATE
void _pmath_mp_float_normalize(pmath_float_t f){
  assert(pmath_is_mpfloat(f));
  assert(PMATH_AS_PTR(f)->refcount == 1);
  
  if(mpfr_zero_p(PMATH_AS_MP_ERROR(f)) || mpfr_zero_p(PMATH_AS_MP_VALUE(f)))
    return;
  
  if(mpfr_get_exp(PMATH_AS_MP_ERROR(f)) > mpfr_get_exp(PMATH_AS_MP_VALUE(f))){
    mpfr_sign_t sign = PMATH_AS_MP_VALUE(f)->_mpfr_sign;
    mpfr_set_ui(PMATH_AS_MP_VALUE(f), 0, MPFR_RNDN);
    PMATH_AS_MP_VALUE(f)->_mpfr_sign = sign;
  }
}

//}
//{ number conversion functions ...

PMATH_API pmath_bool_t pmath_integer_fits_si(pmath_integer_t integer){
  if(pmath_is_null(integer))
    return FALSE;
    
  return mpz_fits_slong_p(PMATH_AS_MPZ(integer));
}

PMATH_API pmath_bool_t pmath_integer_fits_ui(pmath_integer_t integer){
  if(pmath_is_null(integer))
    return FALSE;
  
  return mpz_fits_ulong_p(PMATH_AS_MPZ(integer));
}

PMATH_API pmath_bool_t pmath_integer_fits_si64(pmath_integer_t integer){
  size_t size;
  
  if(pmath_is_null(integer))
    return FALSE;
  
  size = mpz_sizeinbase(PMATH_AS_MPZ(integer), 2);
  if(size < 63)
    return TRUE;
  
  if(size == 63)
    return mpz_sgn(PMATH_AS_MPZ(integer)) < 0;
  
  return FALSE;
}

PMATH_API pmath_bool_t pmath_integer_fits_ui64(pmath_integer_t integer){
  size_t size;
  
  if(pmath_is_null(integer))
    return FALSE;
  
  size = mpz_sizeinbase(PMATH_AS_MPZ(integer), 2);
  return (size <= 64 && mpz_sgn(PMATH_AS_MPZ(integer)) >= 0);
}

PMATH_API signed long int pmath_integer_get_si(pmath_integer_t integer){
  if(pmath_is_null(integer))
    return 0;
  
  return mpz_get_si(PMATH_AS_MPZ(integer));
}

PMATH_API unsigned long int pmath_integer_get_ui(pmath_integer_t integer){
  if(pmath_is_null(integer))
    return 0;
    
  return mpz_get_ui(PMATH_AS_MPZ(integer));
}

PMATH_API
PMATH_ATTRIBUTE_PURE
int64_t pmath_integer_get_si64(pmath_integer_t integer){
  size_t size;
  
  if(pmath_is_null(integer))
    return 0;
  
  size = mpz_sizeinbase(PMATH_AS_MPZ(integer), 8);
  if(size <= sizeof(uint64_t)){
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
uint64_t pmath_integer_get_ui64(pmath_integer_t integer){
  size_t size;
  
  if(pmath_is_null(integer))
    return 0;
  
  size = mpz_sizeinbase(PMATH_AS_MPZ(integer), 8);
  if(size <= sizeof(uint64_t)){
    uint64_t val = 0;
    mpz_export(&val, NULL, 1, sizeof(uint64_t), 0, 0, PMATH_AS_MPZ(integer));
    
    return val;
  }
  return 0;
}

PMATH_API double pmath_number_get_d(pmath_number_t number){
  if(pmath_is_double(number))
    return PMATH_AS_DOUBLE(number);
  
  assert(pmath_is_pointer(number));
  if(pmath_is_null(number))
    return 0.0;
  
  switch(PMATH_AS_PTR(number)->type_shift){
    case PMATH_TYPE_SHIFT_INTEGER:
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

PMATH_API int pmath_number_sign(pmath_number_t num){
  if(pmath_is_double(num)){
    if(PMATH_AS_DOUBLE(num) < 0)
      return -1;
    if(PMATH_AS_DOUBLE(num) > 0)
      return 1;
    return 0;
  }
  
  assert(pmath_is_pointer(num));
  if(pmath_is_null(num))
    return 0;
  
  switch(PMATH_AS_PTR(num)->type_shift){
    case PMATH_TYPE_SHIFT_INTEGER:
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
  ){
    pmath_integer_t result;

    assert(pmath_is_integer(integer));

    if(PMATH_AS_PTR(integer)->refcount == 1)
      result = pmath_ref(integer);
    else
      result = _pmath_create_integer();

    if(!pmath_is_null(result))
      mpz_neg(PMATH_AS_MPZ(result), PMATH_AS_MPZ(integer));

    pmath_unref(integer);
    return result;
  }

PMATH_API pmath_number_t pmath_number_neg(pmath_number_t num){
  if(pmath_is_double(num)){
    pmath_float_t result = _pmath_create_machine_float(-PMATH_AS_DOUBLE(num));

    pmath_unref(num);
    return result;
  }
  
  assert(pmath_is_pointer(num));
  if(pmath_is_null(num))
    return num;

  switch(PMATH_AS_PTR(num)->type_shift){
    case PMATH_TYPE_SHIFT_INTEGER:
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
      
      if(PMATH_AS_PTR(num)->refcount == 1)
        result = pmath_ref(num);
      else
        result = _pmath_create_mp_float(mpfr_get_prec(PMATH_AS_MP_VALUE(num)));

      if(!pmath_is_null(result)){
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
){
  pmath_integer_t result = _pmath_create_integer();

  assert(pmath_is_integer(intA));
  assert(pmath_is_integer(intB));

  if(!pmath_is_null(result)){
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

static void destroy_integer(pmath_t integer){
  struct _pmath_integer_t_ *int_ptr;
  uintptr_t i = int_cache_inc(+1);
  
  assert(PMATH_AS_PTR(integer)->refcount == 0);
  
  int_ptr = (void*)PMATH_AS_PTR(integer);
  int_ptr = int_cache_swap(i, int_ptr);
  if(int_ptr){
    assert(int_ptr->inherited.refcount == 0);
    
    mpz_clear(int_ptr->value);
    pmath_mem_free(int_ptr);
  }
}

static unsigned int hash_init;

static unsigned int hash_integer(pmath_t integer){
  return incremental_hash(
    PMATH_AS_MPZ(integer)[0]._mp_d,
    sizeof(PMATH_AS_MPZ(integer)[0]._mp_d[0]) * (size_t)abs(PMATH_AS_MPZ(integer)[0]._mp_size),
    hash_init);
}

static void write_integer(
  pmath_t                  integer,
  pmath_write_options_t    options,
  pmath_write_func_t       write,
  void                    *user
){
  char *str;
  int base = 10;
  size_t size = mpz_sizeinbase(PMATH_AS_MPZ(integer), 16) + 2;

//  if(size > 1000)
//    /* if the number is that big, a decimal notation is realy useless and needs
//       time and more space.
//     */
//    base = 16;
//  else
    size = mpz_sizeinbase(PMATH_AS_MPZ(integer), 10) + 2;

  str = (char*)pmath_mem_alloc(size);
  if(!str){
    write_cstr("<<out-of-memory>>", write, user);
    return;
  }
//  if(base == 16)
//    write_cstr("16^^", write, user);
  mpz_get_str(str, base, PMATH_AS_MPZ(integer));

  /* mpz_sizeinbase returns 2 for integer->value = 8 or 9, but 1 should be the
     result, since its one digit */

  // calculate strlen faster fo very big values.
  // write(user, str, size - 3 + strlen(str + size - 3));
  write_cstr(str, write, user);

  pmath_mem_free(str);
}

//} ============================================================================
//{ pMath object functions for quotients ...

static void destroy_quotient(pmath_t quotient){
  assert(PMATH_AS_PTR(quotient)->refcount == 0);
  
  pmath_unref(PMATH_QUOT_NUM(quotient));
  pmath_unref(PMATH_QUOT_DEN(quotient));
  
  pmath_mem_free(PMATH_AS_PTR(quotient));
}

static unsigned int hash_quotient(pmath_t quotient){
  unsigned int next = 0;
  unsigned int h;
  
  h = hash_integer(PMATH_QUOT_NUM(quotient));
  next = incremental_hash(&h, sizeof(h), next);
  h = hash_integer(PMATH_QUOT_DEN(quotient));
  return incremental_hash(&h, sizeof(h), next);
}

static void write_quotient(
  pmath_t                   quotient,
  pmath_write_options_t     options,
  pmath_write_func_t        write,
  void                     *user
){
  write_integer(PMATH_QUOT_NUM(quotient), options, write, user);
  write_cstr("/", write, user);
  write_integer(PMATH_QUOT_DEN(quotient), options, write, user);
}

//} ============================================================================
//{ pMath object functions for multi precision floats ...

static void destroy_mp_float(pmath_t f){
  uintptr_t i = mp_cache_inc(+1);
  struct _pmath_mp_float_t_ *f_ptr;
  
  assert(PMATH_AS_PTR(f)->refcount == 0);
  
  f_ptr = (void*)PMATH_AS_PTR(f);
  f_ptr = mp_cache_swap(i, f_ptr);
  if(f_ptr){
    assert(f_ptr->inherited.refcount == 0);
    
    mpfr_clear(f_ptr->value);
    mpfr_clear(f_ptr->error);
    pmath_mem_free(f_ptr);
  }
}

static unsigned int hash_mp_float(pmath_t f){
  unsigned int h = 0;
  h = incremental_hash(&PMATH_AS_MP_VALUE(f)[0]._mpfr_prec, sizeof(mpfr_prec_t), h);
  h = incremental_hash(&PMATH_AS_MP_VALUE(f)[0]._mpfr_sign, sizeof(mpfr_sign_t), h);
  h = incremental_hash(&PMATH_AS_MP_VALUE(f)[0]._mpfr_exp,  sizeof(mp_exp_t), h);

  return incremental_hash(
    PMATH_AS_MP_VALUE(f)[0]._mpfr_d,
    sizeof(mp_limb_t) * (size_t)ceil(PMATH_AS_MP_VALUE(f)[0]._mpfr_prec/(double)mp_bits_per_limb),
    h);
}

static void write_mp_float(
  pmath_t                f,
  pmath_write_options_t  options,
  pmath_write_func_t     write,
  void                  *user
){
  mp_exp_t exp;
  size_t digits, size;
  char *str;
  char s[30];
  double prec10 = LOG10_2 * pmath_precision(pmath_ref(f));
  double acc10  = LOG10_2 * pmath_accuracy( pmath_ref(f));
  
  if(mpfr_zero_p(PMATH_AS_MP_VALUE(f)) || prec10 == 0){
    char s[30];
    long exp;
    double d = mpfr_get_d_2exp(&exp, PMATH_AS_MP_ERROR(f), MPFR_RNDN);
    d = exp * LOG10_2 + log10(d);
    snprintf(s, sizeof(s), "0``%f", - d);
    write_cstr(s, write, user);
    return;
  }
  
  digits = 1+(size_t)(floor(prec10 - acc10) + acc10);
  if(digits < 2)
    digits = 2;

  size = 7;
  if(digits > 5)
    size = digits+2;

  str = (char*)pmath_mem_alloc(size);
  if(!str){
    write_cstr("<<out-of-memory>>", write, user);
    return;
  }

  mpfr_get_str(str, &exp, 10, digits, PMATH_AS_MP_VALUE(f), MPFR_RNDN);

  if(exp == 0){
    if(*str == '-'){
      write_cstr("-0.", write, user);

//      if(prec == DBL_MANT_DIG)
//        delete_trailing_zeros(str + 1);

      write_cstr(str + 1, write, user);
    }
    else{
      write_cstr("0.", write, user);

//      if(prec == DBL_MANT_DIG)
//        delete_trailing_zeros(str);

      write_cstr(str, write, user);
    }

    snprintf(s, sizeof(s), "`%f", prec10);
    write_cstr(s, write, user);
  }
  else if(exp > 0 && (size_t)exp < strlen(str)){
    char c;
    if(*str == '-')
      exp++;
    c = str[exp];
    str[exp] = '\0';
    write_cstr(str, write, user);
    write_cstr(".", write, user);
    str[exp] = c;

//    if(prec == DBL_MANT_DIG)
//      delete_trailing_zeros(str + exp + 1);
    write_cstr(str + exp, write, user);

    snprintf(s, sizeof(s), "`%f", prec10); // prec * LOG10_2
    write_cstr(s, write, user);
  }
  else if(exp < 0 && exp > -5){
    static const uint16_t zero_char = '0';

    int start;
    if(*str == '-'){
      write_cstr("-0.", write, user);
      start = 1;
    }
    else{
      write_cstr("0.", write, user);
      start = 0;
    }

    do{
      write(user, &zero_char, 1);
    }while(++exp < 0);

//    if(prec == DBL_MANT_DIG)
//      delete_trailing_zeros(str + start);

    write_cstr(str + start, write, user);

    snprintf(s, sizeof(s), "`%f", prec10);
    write_cstr(s, write, user);
  }
  else{
    int start;
    if(*str == '\0'){ // 0.0
      write_cstr("0.", write, user);
      start = 0;
    }
    else if(*str == '-'){
      uint16_t ustr[3] = {UCS2_CHAR('-'), UCS2_CHAR(str[1]), UCS2_CHAR('.')};
      write(user, ustr, 3);
      start = 2;
      --exp;
    }
    else{
      uint16_t ustr[2] = {UCS2_CHAR(*str), UCS2_CHAR('.')};
      write(user, ustr, 2);
      start = 1;
      --exp;
    }

//    if(prec == DBL_MANT_DIG)
//      delete_trailing_zeros(str + start);

    write_cstr(str + start, write, user);

    snprintf(s, sizeof(s), "`%f", prec10);
    write_cstr(s, write, user);

    if(exp != 0){
      char s[30];
      snprintf(s, sizeof(s), "*^%"PRIdMAX, (intmax_t)exp);
      write_cstr(s, write, user);
    }
  }

  pmath_mem_free(str);
}

//} ============================================================================
//{ pMath object functions for machine floats ...

static void destroy_machine_float(pmath_t f){
  uintptr_t i = maf_cache_inc(+1);
  struct _pmath_machine_float_t_ *f_ptr;
  
  assert(PMATH_AS_PTR(f)->refcount == 0);
  
  f_ptr = (void*)PMATH_AS_PTR(f);
  f_ptr = maf_cache_swap(i, f_ptr);
  if(f_ptr){
    assert(f_ptr->inherited.refcount == 0);
    pmath_mem_free(f_ptr);
  }
}

static unsigned int hash_machine_float(pmath_t f){
  return incremental_hash(&PMATH_AS_DOUBLE(f), sizeof(double), 0);
}

static void write_machine_float(
  pmath_t                f,
  pmath_write_options_t  options,
  pmath_write_func_t     write,
  void                  *user
){
  char s[100];
  double test;
  int maxprec = 1 + (int)ceil(DBL_MANT_DIG * LOG10_2);
  int len, i;
  
  for(len = 1;len <= maxprec;++len){
    snprintf(s, sizeof(s), "%.*g", len, PMATH_AS_DOUBLE(f));
    
    test = strtod(s, NULL);
    if(test == PMATH_AS_DOUBLE(f))
      break;
  }
  
  len = strlen(s);
  i = 0;
  while(i < len && s[i] != '.' && s[i] != 'e')
    ++i;
  
  if(i == len){
    write_cstr(s, write, user);
    write_cstr(".0`", write, user);
    return;
  }
  
  if(s[i] == 'e'){
    int exp = atoi(s + i + 1);
    
    s[i] = '\0';
    write_cstr(s, write, user);
    
    if(exp > 0 && exp < 6){
      char zeros[] = "000000";
      zeros[exp] = '\0';
      write_cstr(zeros, write, user);
      write_cstr(".0`", write, user);
      return;
    }
    else{
      write_cstr(".0`*^", write, user);
      
      snprintf(s, sizeof(s), "%d", exp);
      write_cstr(s, write, user);
      return;
    }
  }
  
  while(i < len && s[i] != 'e')
    ++i;
  
  if(i < len){
    int exp = atoi(s + i + 1);
    
    s[i] = '\0';
    write_cstr(s, write, user);
    write_cstr("`*^", write, user);
    
    snprintf(s, sizeof(s), "%d", exp);
    write_cstr(s, write, user);
    return;
  }
  
  write_cstr(s, write, user);
  write_cstr("`", write, user);
}

//} ============================================================================
//{ common pMath object functions for all number types ...

  static int _cmp_ii(pmath_integer_t intA, pmath_integer_t intB){
    if(pmath_is_null(intA) || pmath_is_null(intB))
      return pmath_compare(intA, intB);

    return mpz_cmp(PMATH_AS_MPZ(intA), PMATH_AS_MPZ(intB));
  }

static int compare_numbers(
  pmath_number_t numA,
  pmath_number_t numB
){
  if(pmath_is_integer(numA)){
    if(pmath_is_integer(numB))
      return mpz_cmp(PMATH_AS_MPZ(numA), PMATH_AS_MPZ(numB));
    
    if(pmath_is_double(numB))
      return mpz_cmp_d(PMATH_AS_MPZ(numA), PMATH_AS_DOUBLE(numB));
      
    if(pmath_is_quotient(numB)){
      // cmp(u, w/x) = cmp(u*x,w)  because x > 0
      pmath_integer_t lhs = _mul_ii(
        pmath_ref(numA),
        pmath_rational_denominator(numB));
      pmath_integer_t rhs = pmath_rational_numerator(numB);
      int result = _cmp_ii(lhs, rhs);
      pmath_unref(lhs);
      pmath_unref(rhs);
      return result;
    }

    assert(pmath_is_mpfloat(numB));
    
    return -mpfr_cmp_z(
      PMATH_AS_MP_VALUE(numB),
      PMATH_AS_MPZ(numA));
  }

  if(pmath_is_quotient(numA)){
    if(pmath_is_quotient(numB)){
      // cmp(u/v, w/x) = cmp(u*x,v*w)  because v > 0 && x > 0
      pmath_integer_t lhs = _mul_ii(
        pmath_rational_numerator(numA),
        pmath_rational_denominator(numB));
        
      pmath_integer_t rhs = _mul_ii(
        pmath_rational_numerator(numB),
        pmath_rational_denominator(numA));
        
      int result = _cmp_ii(lhs, rhs);
      
      pmath_unref(lhs);
      pmath_unref(rhs);
      return result;
    }

    if(pmath_is_double(numB)){
      double q;

      q = mpz_get_d(PMATH_AS_MPZ(PMATH_QUOT_NUM(numA)));
      q/= mpz_get_d(PMATH_AS_MPZ(PMATH_QUOT_DEN(numA)));

      if(q < PMATH_AS_DOUBLE(numB))
        return -1;
      if(q > PMATH_AS_DOUBLE(numB))
        return 1;
      return 0;
    }

    if(pmath_is_mpfloat(numB)){
      mp_prec_t prec = mpfr_get_prec(PMATH_AS_MP_VALUE(numB));
      pmath_float_t tmp  = _pmath_create_mp_float(prec);
      pmath_float_t tmp2 = _pmath_create_mp_float(prec);
      int result;

      if(pmath_is_null(tmp) || pmath_is_null(tmp2)){
        pmath_unref(tmp);
        pmath_unref(tmp2);
        return 1;
      }

      mpfr_set_z(
        PMATH_AS_MP_VALUE(tmp),
        PMATH_AS_MPZ(PMATH_QUOT_NUM(numA)),
        MPFR_RNDN);

      mpfr_div_z(
        PMATH_AS_MP_VALUE(tmp2),
        PMATH_AS_MP_VALUE(tmp),
        PMATH_AS_MPZ(PMATH_QUOT_DEN(numA)),
        MPFR_RNDN);

      result = mpfr_cmp(PMATH_AS_MP_VALUE(tmp2), PMATH_AS_MP_VALUE(numB));

      pmath_unref(tmp);
      pmath_unref(tmp2);

      return result;
    }

    return -compare_numbers(numB, numA);
  }

  if(pmath_is_mpfloat(numA)){
    if(pmath_is_mpfloat(numB)){
      mp_prec_t precA = mpfr_get_prec(PMATH_AS_MP_VALUE(numA));
      mp_prec_t precB = mpfr_get_prec(PMATH_AS_MP_VALUE(numB));
      
      if(precA < precB){
        pmath_float_t tmp = _pmath_create_mp_float(precA);
        int result;
        
        if(!pmath_is_null(tmp)){
          mpfr_set(PMATH_AS_MP_VALUE(tmp), PMATH_AS_MP_VALUE(numB), MPFR_RNDN);
          
          result = mpfr_cmp(PMATH_AS_MP_VALUE(numA), PMATH_AS_MP_VALUE(tmp));
          
          pmath_unref(tmp);
          return result;
        }
      }
      else if(precA > precB){
        pmath_float_t tmp = _pmath_create_mp_float(precB);
        int result;
        
        if(!pmath_is_null(tmp)){
          mpfr_set(PMATH_AS_MP_VALUE(tmp), PMATH_AS_MP_VALUE(numA), MPFR_RNDN);
          
          result = mpfr_cmp(PMATH_AS_MP_VALUE(numA), PMATH_AS_MP_VALUE(tmp));
          
          pmath_unref(tmp);
          return result;
        }
      }
    
      return mpfr_cmp(PMATH_AS_MP_VALUE(numA), PMATH_AS_MP_VALUE(numB));
    }

    if(pmath_is_double(numB)){
      double d = mpfr_get_d(PMATH_AS_MP_VALUE(numA), MPFR_RNDN);
      
      if(d < PMATH_AS_DOUBLE(numB))
        return -1;
        
      if(d > PMATH_AS_DOUBLE(numB))
        return 1;
        
      return 0;
    }

    return -compare_numbers(numB, numA);
  }

  assert(pmath_is_double(numA));

  if(pmath_is_double(numB)){
    if(PMATH_AS_DOUBLE(numA) < PMATH_AS_DOUBLE(numB))
      return -1;

    if(PMATH_AS_DOUBLE(numA) > PMATH_AS_DOUBLE(numB))
      return 1;

    return 0;
  }

  return -compare_numbers(numB, numA);
}

static pmath_bool_t equal_numbers(
  pmath_number_t numA,
  pmath_number_t numB
){
  if(pmath_is_integer(numA)){
    if(pmath_is_integer(numB)){
      return 0 == mpz_cmp(PMATH_AS_MPZ(numA), PMATH_AS_MPZ(numB));
    }
    
    return FALSE;
  }
  else if(pmath_is_integer(numB))
    return equal_numbers(numB, numA);

  if(pmath_is_quotient(numA)){
    if(pmath_is_quotient(numB)){
      return 0 == mpz_cmp(
                    PMATH_AS_MPZ(PMATH_QUOT_NUM(numA)),
                    PMATH_AS_MPZ(PMATH_QUOT_NUM(numB)))
          && 0 == mpz_cmp(
                    PMATH_AS_MPZ(PMATH_QUOT_DEN(numA)),
                    PMATH_AS_MPZ(PMATH_QUOT_DEN(numB)));
    }
    
    return FALSE;
  }
  else if(pmath_is_quotient(numB))
    return equal_numbers(numB, numA);

  if(pmath_is_mpfloat(numA)
  && pmath_is_mpfloat(numB)){
    mp_prec_t precA = mpfr_get_prec(PMATH_AS_MP_VALUE(numA));
    mp_prec_t precB = mpfr_get_prec(PMATH_AS_MP_VALUE(numB));
    
    if(precA < precB){
      pmath_float_t tmp = _pmath_create_mp_float(precA);
      pmath_bool_t result;
      
      if(!pmath_is_null(tmp)){
        mpfr_set(PMATH_AS_MP_VALUE(tmp), PMATH_AS_MP_VALUE(numB), MPFR_RNDN);
        
        result = mpfr_equal_p(PMATH_AS_MP_VALUE(numA), PMATH_AS_MP_VALUE(tmp));
        
        pmath_unref(tmp);
        return result;
      }
    }
    else if(precA > precB){
      pmath_float_t tmp = _pmath_create_mp_float(precB);
      pmath_bool_t result;
      
      if(!pmath_is_null(tmp)){
        mpfr_set(PMATH_AS_MP_VALUE(tmp), PMATH_AS_MP_VALUE(numA), MPFR_RNDN);
        
        result = mpfr_equal_p(PMATH_AS_MP_VALUE(tmp), PMATH_AS_MP_VALUE(numA));
        
        pmath_unref(tmp);
        return result;
      }
    }
    
    return mpfr_equal_p(PMATH_AS_MP_VALUE(numA), PMATH_AS_MP_VALUE(numB));
  }

  return 0 == compare_numbers(numA, numB);
}

//} ============================================================================
//{ module handling functions ...

PMATH_PRIVATE void _pmath_numbers_memory_panic(void){
//  destroy_all_unused_quotients();
  int_cache_clear();
  mp_cache_clear();
  maf_cache_clear();
  mpfr_free_cache();
}

PMATH_PRIVATE pmath_bool_t _pmath_numbers_init(void){
  int i;
  char c = 0;
  hash_init = incremental_hash(&c, 1, 0);

  memset(int_cache, 0, sizeof(int_cache));
  memset(mp_cache,  0, sizeof(mp_cache));
  memset(maf_cache, 0, sizeof(maf_cache));
  int_cache_pos = 0;
  mp_cache_pos  = 0;
  maf_cache_pos = 0;
  
  #ifdef PMATH_DEBUG_LOG
    int_cache_hits = int_cache_misses = 0;
    maf_cache_hits = maf_cache_misses = 0;
    mp_cache_hits  = mp_cache_misses  = 0;
  #endif
  
  gmp_randinit_default(_pmath_randstate);
  _pmath_rand_spinlock = 0;

//  memset(&unused_quotients,      0, sizeof(unused_quotients));
  memset(&special_values_wrong_indices, 0, sizeof(special_values_wrong_indices));
  
  _pmath_init_special_type(
    PMATH_TYPE_SHIFT_INTEGER,
    compare_numbers,
    hash_integer,
    destroy_integer,
    equal_numbers,
    write_integer);

  _pmath_init_special_type(
    PMATH_TYPE_SHIFT_QUOTIENT,
    compare_numbers,
    hash_quotient,
    destroy_quotient,
    equal_numbers,
    write_quotient);

  _pmath_init_special_type(
    PMATH_TYPE_SHIFT_MP_FLOAT,
    compare_numbers,
    hash_mp_float,
    destroy_mp_float,
    equal_numbers,
    write_mp_float);

  _pmath_init_special_type(
    _DEPRECATED_PMATH_TYPE_SHIFT_MACHINE_FLOAT,
    compare_numbers,
    hash_machine_float,
    destroy_machine_float,
    equal_numbers,
    write_machine_float);

  for(i = SPECIAL_MIN;i <= SPECIAL_MAX;i++){
    special_values[i] = _pmath_create_integer();
    if(pmath_is_null(special_values[i]))
      goto FAIL;

    mpz_set_si(PMATH_AS_MPZ(special_values[i]), i);
  }

  _pmath_one_half = _pmath_create_quotient(
    pmath_ref(special_values[1]),
    pmath_ref(special_values[2]));

  if(pmath_is_null(_pmath_one_half))
    goto FAIL;
    
  //pmath_debug_print("mpfr_get_default_prec() = %"PRIdMAX"\n", (intmax_t)mpfr_get_default_prec());

  if(!_pmath_primetest_init())
    goto FAIL_PRIME;

  return TRUE;
 
 //          _pmath_primetest_done();
 FAIL_PRIME:
 FAIL: pmath_unref(_pmath_one_half);

  for(i = SPECIAL_MIN;i <= SPECIAL_MAX;i++){
    pmath_unref(special_values[i]);
  }

//  destroy_all_unused_quotients();
  int_cache_clear();
  mp_cache_clear();
  maf_cache_clear();
  gmp_randclear(_pmath_randstate);
  return FALSE;
}

PMATH_PRIVATE void _pmath_numbers_done(void){
  int i;
  _pmath_primetest_done();
  pmath_unref(_pmath_one_half);

  for(i = SPECIAL_MIN;i <= SPECIAL_MAX;i++){
    pmath_unref(special_values[i]);
  }

  int_cache_clear();
  maf_cache_clear();
  mp_cache_clear();
  mpfr_free_cache();
  gmp_randclear(_pmath_randstate);
  
  #ifdef PMATH_DEBUG_LOG
  {
    pmath_debug_print("int cache hit rate:              %f (%d of %d)\n", 
      (double)int_cache_hits / (double)(int_cache_hits + int_cache_misses),
      (int) int_cache_hits,
      (int)(int_cache_hits + int_cache_misses));
    pmath_debug_print("machine float cache hit rate:    %f (%d of %d)\n", 
      (double)maf_cache_hits / (double)(maf_cache_hits + maf_cache_misses),
      (int) maf_cache_hits,
      (int)(maf_cache_hits + maf_cache_misses));
    pmath_debug_print("multi prec float cache hit rate: %f (%d of %d)\n", 
      (double)mp_cache_hits  / (double)(mp_cache_hits  + mp_cache_misses),
      (int) mp_cache_hits,
      (int)(mp_cache_hits + mp_cache_misses));
  }
  #endif
}

//}
