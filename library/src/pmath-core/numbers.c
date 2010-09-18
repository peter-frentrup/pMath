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

  static struct _pmath_integer_t *int_cache_swap(
    uintptr_t                i, 
    struct _pmath_integer_t *value
  ){
    i = i & CACHE_MASK;
    
    assert(!value || value->inherited.refcount == 0);
    
    return (struct _pmath_integer_t*)
      pmath_atomic_fetch_set(&int_cache[i], (intptr_t)value);
  }

  static void int_cache_clear(void){
    uintptr_t i;
    
    for(i = 0;i < CACHE_SIZE;++i){
      struct _pmath_integer_t *integer = int_cache_swap(i, NULL);
      
      if(integer){
        assert(integer->inherited.refcount == 0);
        
        mpz_clear(integer->value);
        pmath_mem_free(integer);
      }
    }
  }

  PMATH_PRIVATE struct _pmath_integer_t *_pmath_create_integer(void){
    struct _pmath_integer_t *integer;
    
    uintptr_t i = int_cache_inc(-1);
    integer = int_cache_swap(i-1, NULL);
    if(integer){
      #ifdef PMATH_DEBUG_LOG
        (void)pmath_atomic_fetch_add(&int_cache_hits, 1);
      #endif
      
      assert(integer->inherited.refcount == 0);
      integer->inherited.refcount = 1;
      
      return integer;
    }
    else{
      #ifdef PMATH_DEBUG_LOG
        (void)pmath_atomic_fetch_add(&int_cache_misses, 1);
      #endif
    }
    
    integer = (struct _pmath_integer_t*)_pmath_create_stub(
      PMATH_TYPE_SHIFT_INTEGER,
      sizeof(struct _pmath_integer_t));

    if(integer)
      mpz_init(integer->value);

    return integer;
  }

//} ============================================================================
//{ creating quotients ...

//static struct _pmath_stack_t  unused_quotients;
//
//  static void destroy_all_unused_quotients(void){
//    void *item;
//    while((item = pmath_stack_pop(&unused_quotients)) != NULL){
//      pmath_mem_free(
//        STACK_ITEM_TO_NUMBER(item),
//        sizeof(struct _pmath_quotient_t));
//    }
//  }

  PMATH_PRIVATE struct _pmath_quotient_t *_pmath_create_quotient(
    pmath_integer_t numerator,   // will be freed
    pmath_integer_t denominator  // will be freed
  ){
    struct _pmath_quotient_t *quotient;

    if(!numerator || !denominator){
      pmath_unref(numerator);
      pmath_unref(denominator);
      return NULL;
    }

    quotient = (struct _pmath_quotient_t*)_pmath_create_stub(
      PMATH_TYPE_SHIFT_QUOTIENT,
      sizeof(struct _pmath_quotient_t));

    if(!quotient){
      pmath_unref(numerator);
      pmath_unref(denominator);
      return NULL;
    }

    quotient->numerator   = (struct _pmath_integer_t*)numerator;
    quotient->denominator = (struct _pmath_integer_t*)denominator;
    return quotient;

//    struct _pmath_quotient_t *quotient;
//    void *item;
//
//    if(!numerator || !denominator){
//      pmath_unref(numerator);
//      pmath_unref(denominator);
//      return NULL;
//    }
//
//    item = pmath_stack_pop(&unused_quotients);
//    if(item){
//      quotient = (struct _pmath_quotient_t*)pmath_ref(
//        (pmath_quotient_t)STACK_ITEM_TO_NUMBER(item));
//    }
//    else{
//      quotient = (struct _pmath_quotient_t*)_pmath_create_stub(
//        PMATH_TYPE_SHIFT_QUOTIENT,
//        sizeof(struct _pmath_quotient_t));
//
//      if(!quotient){
//        pmath_unref(numerator);
//        pmath_unref(denominator);
//        return NULL;
//      }
//    }
//
//    quotient->numerator   = (struct _pmath_integer_t*)numerator;
//    quotient->denominator = (struct _pmath_integer_t*)denominator;
//    return quotient;
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

  static struct _pmath_mp_float_t *mp_cache_swap(
    uintptr_t                 i, 
    struct _pmath_mp_float_t *f
  ){
    i = i & CACHE_MASK;
    
    return (struct _pmath_mp_float_t*)
      pmath_atomic_fetch_set(&mp_cache[i], (intptr_t)f);
  }

  static void mp_cache_clear(void){
    uintptr_t i;
    
    for(i = 0;i < CACHE_SIZE;++i){
      struct _pmath_mp_float_t *f = mp_cache_swap(i, NULL);
      
      if(f){
        assert(f->inherited.refcount == 0);
        
        mpfr_clear(f->value);
        mpfr_clear(f->error);
        pmath_mem_free(f);
      }
    }
  }

  PMATH_PRIVATE struct _pmath_mp_float_t *_pmath_create_mp_float(mp_prec_t precision){
    struct _pmath_mp_float_t *f;
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
      mpfr_set_ui_2exp(f->error, 1, -(mp_exp_t)precision-1, GMP_RNDU);
      return f;
    }
    else{
      #ifdef PMATH_DEBUG_LOG
        (void)pmath_atomic_fetch_add(&mp_cache_misses, 1);
      #endif
    }

    f = (struct _pmath_mp_float_t*)_pmath_create_stub(
      PMATH_TYPE_SHIFT_MP_FLOAT,
      sizeof(struct _pmath_mp_float_t));

    if(!f)
      return NULL;

    mpfr_init2(f->value, precision);
    mpfr_init2(f->error, PMATH_MP_ERROR_PREC);
    mpfr_set_ui_2exp(f->error, 1, -(mp_exp_t)precision-1, GMP_RNDU);

    return f;
  }

  PMATH_PRIVATE
  struct _pmath_mp_float_t *_pmath_create_mp_float_from_d(double value){
    struct _pmath_mp_float_t *result = _pmath_create_mp_float(0);

    if(result){
      mpfr_set_d(result->value, value, GMP_RNDN);
      mpfr_set_d(result->error, value, GMP_RNDN);
      mpfr_abs(result->error, result->error, GMP_RNDU);
      mpfr_div_2si(result->error, result->error, mpfr_get_prec(result->value), GMP_RNDU);
    }
    
    return result;
  }

  PMATH_PRIVATE
  struct _pmath_mp_float_t *_pmath_convert_to_mp_float(pmath_float_t n){ // n will be freed
    struct _pmath_mp_float_t *result;

    if(n && n->type_shift == PMATH_TYPE_SHIFT_MP_FLOAT)
      return (struct _pmath_mp_float_t*)n;

    result = _pmath_create_mp_float(0);

    if(result)
      mpfr_set_d(result->value, pmath_number_get_d(n), GMP_RNDN);

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

  static struct _pmath_machine_float_t *maf_cache_swap(
    uintptr_t                      i, 
    struct _pmath_machine_float_t *f
  ){
    i = i & CACHE_MASK;
    
    return (struct _pmath_machine_float_t*)
      pmath_atomic_fetch_set(&maf_cache[i], (intptr_t)f);
  }

  static void maf_cache_clear(void){
    uintptr_t i;
    
    for(i = 0;i < CACHE_SIZE;++i){
      pmath_mem_free(maf_cache_swap(i, NULL));
    }
  }

  PMATH_PRIVATE
  struct _pmath_machine_float_t *_pmath_create_machine_float(double value){
    struct _pmath_machine_float_t *f;
    
    uintptr_t i = maf_cache_inc(-1);
    f = maf_cache_swap(i-1, NULL);
    if(f){
      #ifdef PMATH_DEBUG_LOG
        (void)pmath_atomic_fetch_add(&maf_cache_hits, 1);
      #endif
  
      assert(f->inherited.refcount == 0);
      f->inherited.refcount = 1;
      
      f->value = value;
      
      return f;
    }
    else{
      #ifdef PMATH_DEBUG_LOG
        (void)pmath_atomic_fetch_add(&maf_cache_misses, 1);
      #endif
    }
    
    f = (struct _pmath_machine_float_t*)_pmath_create_stub(
      PMATH_TYPE_SHIFT_MACHINE_FLOAT,
      sizeof(struct _pmath_machine_float_t));
    
    if(f)
      f->value = value;

    return f;
  }

//} ============================================================================
//{ integer constructors ...

#define SPECIAL_MIN (-1)
#define SPECIAL_MAX (10)

static pmath_integer_t special_values_wrong_indices[SPECIAL_MAX - SPECIAL_MIN + 1];
PMATH_PRIVATE pmath_integer_t *special_values = special_values_wrong_indices - SPECIAL_MIN;

PMATH_API pmath_integer_t pmath_integer_new_si(signed long int si){
  struct _pmath_integer_t *integer;
  if(si >= SPECIAL_MIN && si <= SPECIAL_MAX)
    return pmath_ref(special_values[si]);

  integer = _pmath_create_integer();
  if(!integer)
    return NULL;

  mpz_set_si(integer->value, si);
  return (pmath_integer_t)integer;
}

PMATH_API pmath_integer_t pmath_integer_new_ui(unsigned long int ui){
  struct _pmath_integer_t *integer;
  if(ui <= SPECIAL_MAX)
    return pmath_ref(special_values[ui]);

  integer = _pmath_create_integer();
  if(!integer)
    return NULL;

  mpz_set_ui(integer->value, ui);
  return (pmath_integer_t)integer;
}

PMATH_API pmath_integer_t pmath_integer_new_size(size_t size){
  struct _pmath_integer_t *integer;
  if(size <= SPECIAL_MAX)
    return pmath_ref(special_values[size]);

  integer = _pmath_create_integer();
  if(!integer)
    return NULL;

  #if defined(PMATH_OS_WIN32) && PMATH_BITSIZE == 64
    mpz_import(
      integer->value,
      1,
      -1,
      sizeof(size_t),
      0,
      0,
      &size);
  #else
    mpz_set_ui(integer->value, (unsigned long)size);
  #endif
  return (pmath_integer_t)integer;
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
  struct _pmath_integer_t *integer = _pmath_create_integer();
  if(!integer)
    return NULL;

  mpz_import(
    integer->value,
    count,
    order,
    size,
    endian,
    nails,
    data);
    
  return (pmath_integer_t)integer;
}

PMATH_API pmath_integer_t pmath_integer_new_str(const char *str, int base){
  struct _pmath_integer_t *integer = _pmath_create_integer();
  if(!integer)
    return NULL;

  if(mpz_set_str(integer->value, str, base)){
    pmath_unref((pmath_integer_t)integer);
    return NULL;
  }

  return (pmath_integer_t)integer;
}

//} ============================================================================
//{ quotient constructor and access functions ...

PMATH_API pmath_rational_t pmath_rational_new(
  pmath_integer_t  numerator,
  pmath_integer_t  denominator
){
  if(!numerator || !denominator){
    pmath_unref(numerator);
    pmath_unref(denominator);
    return NULL;
  }
  if(pmath_number_sign(denominator) == 0){ // pmath_number_sign(NULL) = 0
    pmath_unref(numerator);
    pmath_unref(denominator);
    return NULL;
  }

  // fast check: n/1 -> n
  if(mpz_cmp_ui(((struct _pmath_integer_t*)denominator)->value, 1) == 0){
    pmath_unref(denominator);
    return numerator;
  }

  //{ canonical form ...
  if(numerator->refcount > 1){
    struct _pmath_integer_t *unique_num = _pmath_create_integer();
    if(!unique_num){
      pmath_unref(numerator);
      pmath_unref(denominator);
      return NULL;
    }
    mpz_set(unique_num->value, ((struct _pmath_integer_t*)numerator)->value);
    pmath_unref(numerator);
    numerator = (pmath_integer_t)unique_num;
  }

  if(denominator->refcount > 1){
    struct _pmath_integer_t *unique_den = _pmath_create_integer();
    if(!unique_den){
      pmath_unref(numerator);
      pmath_unref(denominator);
      return NULL;
    }
    mpz_set(unique_den->value, ((struct _pmath_integer_t*)denominator)->value);
    pmath_unref(denominator);
    denominator = (pmath_integer_t)unique_den;
  }

  {
    mpq_t  tmp_mpq;
    mpz_swap(mpq_numref(tmp_mpq), ((struct _pmath_integer_t*)numerator)->value);
    mpz_swap(mpq_denref(tmp_mpq), ((struct _pmath_integer_t*)denominator)->value);

    mpq_canonicalize(tmp_mpq);

    mpz_swap(mpq_numref(tmp_mpq), ((struct _pmath_integer_t*)numerator)->value);
    mpz_swap(mpq_denref(tmp_mpq), ((struct _pmath_integer_t*)denominator)->value);
  }
  //}

  // check again canonical form: n/1 -> n
  if(mpz_cmp_ui(((struct _pmath_integer_t*)denominator)->value, 1) == 0){
    pmath_unref(denominator);
    return numerator;
  }

  return (pmath_rational_t)_pmath_create_quotient(numerator, denominator);
}

PMATH_API pmath_integer_t pmath_rational_numerator(
  pmath_rational_t rational
){
  if(!rational)
    return NULL;

  if(rational->type_shift == PMATH_TYPE_SHIFT_INTEGER)
    return (pmath_integer_t)pmath_ref(rational);

  assert(rational->type_shift == PMATH_TYPE_SHIFT_QUOTIENT);
  return (pmath_integer_t)pmath_ref(
    (pmath_integer_t)((struct _pmath_quotient_t*)rational)->numerator);
}

PMATH_API pmath_integer_t pmath_rational_denominator(
  pmath_rational_t rational
){
  if(!rational)
    return NULL;

  if(rational->type_shift == PMATH_TYPE_SHIFT_INTEGER)
    return pmath_integer_new_ui(1);

  assert(rational->type_shift == PMATH_TYPE_SHIFT_QUOTIENT);
  return (pmath_integer_t)pmath_ref(
    (pmath_integer_t)((struct _pmath_quotient_t*)rational)->denominator);
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
  struct _pmath_mp_float_t *f;
  double log2_base = log2(base);
  pmath_bool_t automatic = FALSE;
  
  if(base < 2 || base > 36)
    return NULL;
  
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
         When that string is given to BoxesToExpression(), this function will be 
         called with str = "3.464357457457495e16".
         
         mpfr_set_str() would generate the number 34643574574574952, but 
         strtod() generates the value             34643574574574948, which we 
         want.
       */
        x = strtod(str, NULL);
        if(isfinite(x))
          return (pmath_float_t)_pmath_create_machine_float(x);
      }
      
      f = _pmath_create_mp_float(DBL_MANT_DIG);
      if(!f)
        return NULL;

      mpfr_set_str(f->value, str, base, GMP_RNDN);

      x = mpfr_get_d(f->value, GMP_RNDN);
      pmath_unref((pmath_float_t)f);
      return pmath_float_new_d(x);
    };
    
    case PMATH_PREC_CTRL_GIVEN_PREC: {
      struct _pmath_mp_float_t *tmp_err;
      double bits = base_precision_accuracy * log2_base;
      mp_prec_t prec;
      
      if(bits >= PMATH_MP_PREC_MAX)
        return NULL;
      
      if(bits < 0)
        bits = 0;
      
      prec = (mp_prec_t)ceil(bits);
      
      f = _pmath_create_mp_float(prec >= MPFR_PREC_MIN ? prec : MPFR_PREC_MIN);
      if(!f)
        return NULL;
      
      tmp_err = _pmath_create_mp_float(PMATH_MP_ERROR_PREC);
      if(!tmp_err){
        pmath_unref((pmath_float_t)f);
        return NULL;
      }
      
      mpfr_set_str(f->value, str, base, GMP_RNDN);
      
      if(mpfr_zero_p(f->value)){
        pmath_unref((pmath_float_t)f);
        if(automatic)
          return pmath_float_new_d(0.0);
        return pmath_integer_new_si(0);
      }
      
      // error = |value| * 2 ^ -bits
      mpfr_abs(f->error, f->value, GMP_RNDU);
      mpfr_set_d(tmp_err->value, -bits, GMP_RNDN);
      mpfr_ui_pow(tmp_err->error, 2, tmp_err->value, GMP_RNDN);
      mpfr_mul(f->error, f->error, tmp_err->error, GMP_RNDU);
      
      pmath_unref((pmath_float_t)tmp_err);
      _pmath_mp_float_normalize(f);
      return (pmath_float_t)f;
    };
    
    case PMATH_PREC_CTRL_GIVEN_ACC: {
      double bits = (int_digits + base_precision_accuracy) * log2_base;
      mp_prec_t prec;
      
      if(bits >= PMATH_MP_PREC_MAX)
        return NULL;
      
      if(bits < 0)
        bits = 0;
      
      prec = (mp_prec_t)ceil(bits);
      
      f = _pmath_create_mp_float(prec >= MPFR_PREC_MIN ? prec : MPFR_PREC_MIN);
      if(!f)
        return NULL;
      
      mpfr_set_str(f->value, str, base, GMP_RNDN);
      
      // error = base ^ -accuracy
      mpfr_set_d(f->error, -base_precision_accuracy, GMP_RNDD);
      mpfr_ui_pow(f->error, base, f->error, GMP_RNDU);

      _pmath_mp_float_normalize(f);
      return (pmath_float_t)f;
    };
  
    default: ;
  }
  
  return NULL;
}
  
PMATH_API pmath_float_t pmath_float_new_d(double dbl){
//  if(!isfinite(dbl))
//    return NULL;
//
  return (pmath_float_t)_pmath_create_machine_float(dbl);
//  struct _pmath_mp_float_t *f = _pmath_create_mp_float(PMATH_MACHINE_PRECISION);
//  if(!f)
//    return NULL;
//
//  mpfr_set_d(f->value, dbl, GMP_RNDN);
//  return (pmath_float_t)f;
}

PMATH_PRIVATE 
mp_prec_t _pmath_float_precision( // 0 = MachinePrecision
  pmath_float_t x // wont be freed
){
  if(pmath_instance_of(x, PMATH_TYPE_MP_FLOAT))
    return mpfr_get_prec(((struct _pmath_mp_float_t*)x)->value);

  return 0;
}

PMATH_PRIVATE
pmath_t _pmath_float_exceptions(
  pmath_number_t x  // will be freed.
){
  pmath_t result = NULL;

  if(pmath_instance_of(x, PMATH_TYPE_MACHINE_FLOAT)){
    if(!isfinite(((struct _pmath_machine_float_t*)x)->value)){
      pmath_message(NULL, "ovfl", 0);
      pmath_unref(x);
      return pmath_ref(_pmath_object_overflow);
    }

    return x;
  }

  if(!pmath_instance_of(x, PMATH_TYPE_MP_FLOAT))
    return x;

  /* MPFR flags "invalid" and "erange" are ignored.
   */

  if(PMATH_UNLIKELY(mpfr_underflow_p())){
    pmath_message(NULL, "unfl", 0);
    result = pmath_ref(_pmath_object_underflow);
  }
  else if(mpfr_overflow_p()
  || mpfr_zero_p(((struct _pmath_mp_float_t*)x)->error)){
    pmath_message(NULL, "ovfl", 0);
    result = pmath_ref(_pmath_object_overflow);
  }
  else if(mpfr_nan_p(((struct _pmath_mp_float_t*)x)->value)){
    result = pmath_ref(PMATH_SYMBOL_INDETERMINATE);
    pmath_message(NULL, "indet", 1, pmath_ref(result));
  }
  else if(mpfr_inf_p(((struct _pmath_mp_float_t*)x)->value)){
    result = pmath_expr_new_extended(
      pmath_ref(PMATH_SYMBOL_DIRECTEDINFINITY), 1,
      pmath_integer_new_si(mpfr_sgn(((struct _pmath_mp_float_t*)x)->value)));
    pmath_message(NULL, "infy", 1, pmath_ref(result));
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
void _pmath_mp_float_normalize(struct _pmath_mp_float_t *f){
  assert(f->inherited.refcount == 1);
  
  if(mpfr_zero_p(f->error) || mpfr_zero_p(f->value))
    return;
  
  if(mpfr_get_exp(f->error) > mpfr_get_exp(f->value)){
    mpfr_sign_t sign = f->value->_mpfr_sign;
    mpfr_set_ui(f->value, 0, GMP_RNDN);
    f->value->_mpfr_sign = sign;
  }
}

//}
//{ number conversion functions ...

PMATH_API pmath_bool_t pmath_integer_fits_si(pmath_integer_t integer){
  if(!integer)
    return FALSE;
  return mpz_fits_slong_p(((struct _pmath_integer_t*)integer)->value);
}

PMATH_API pmath_bool_t pmath_integer_fits_ui(pmath_integer_t integer){
  if(!integer)
    return FALSE;
  return mpz_fits_ulong_p(((struct _pmath_integer_t*)integer)->value);
}

PMATH_API signed long int pmath_integer_get_si(pmath_integer_t integer){
  if(!integer)
    return 0;
  return mpz_get_si(((struct _pmath_integer_t*)integer)->value);
}

PMATH_API unsigned long int pmath_integer_get_ui(pmath_integer_t integer){
  if(!integer)
    return 0;
  return mpz_get_ui(((struct _pmath_integer_t*)integer)->value);
}

PMATH_API double pmath_number_get_d(pmath_number_t number){
  if(!number)
    return 0.0;

  switch(number->type_shift){
    case PMATH_TYPE_SHIFT_INTEGER:
      return mpz_get_d(((struct _pmath_integer_t*)number)->value);

    case PMATH_TYPE_SHIFT_QUOTIENT:
      return mpz_get_d(((struct _pmath_quotient_t*)number)->numerator->value)
           / mpz_get_d(((struct _pmath_quotient_t*)number)->denominator->value);

    case PMATH_TYPE_SHIFT_MP_FLOAT:
      return mpfr_get_d(((struct _pmath_mp_float_t*)number)->value, GMP_RNDN);

    case PMATH_TYPE_SHIFT_MACHINE_FLOAT:
      return ((struct _pmath_machine_float_t*)number)->value;
  }

  assert("invalid number type" && 0);
  return 0.0;
}

//} ============================================================================
//{ general number functions ...

PMATH_API int pmath_number_sign(pmath_number_t num){
  if(!num)
    return 0;
  switch(num->type_shift){
    case PMATH_TYPE_SHIFT_INTEGER:
      return mpz_sgn(((struct _pmath_integer_t*)num)->value);

    case PMATH_TYPE_SHIFT_QUOTIENT:
      return mpz_sgn(((struct _pmath_quotient_t*)num)->numerator->value);

    case PMATH_TYPE_SHIFT_MP_FLOAT:
      return mpfr_sgn(((struct _pmath_mp_float_t*)num)->value);

    case PMATH_TYPE_SHIFT_MACHINE_FLOAT:
      if(((struct _pmath_machine_float_t*)num)->value < 0.0)
        return -1;
      if(((struct _pmath_machine_float_t*)num)->value > 0.0)
        return 1;
      return 0;
  }
  assert("invalid number type" && 0);
  return 0;
}

  static pmath_integer_t _neg_i(
    pmath_integer_t integer // will be freed. not NULL!
  ){
    struct _pmath_integer_t *result;

    assert(pmath_instance_of(integer, PMATH_TYPE_INTEGER));

    if(integer->refcount == 1)
      result = (struct _pmath_integer_t*)pmath_ref(integer);
    else
      result = _pmath_create_integer();

    if(result)
      mpz_neg(((struct _pmath_integer_t*)result)->value,
              ((struct _pmath_integer_t*)integer)->value);

    pmath_unref(integer);
    return (pmath_integer_t)result;
  }

PMATH_API pmath_number_t pmath_number_neg(pmath_number_t num){
  if(!num)
    return num;

  switch(num->type_shift){
    case PMATH_TYPE_SHIFT_INTEGER:
      return _neg_i((pmath_integer_t)num);

    case PMATH_TYPE_SHIFT_QUOTIENT: {
      pmath_integer_t numerator = pmath_ref(
        (pmath_integer_t)((struct _pmath_quotient_t*)num)->numerator);
      pmath_integer_t denominator = pmath_ref(
        (pmath_integer_t)((struct _pmath_quotient_t*)num)->denominator);
      pmath_unref(num);

      // already in canonical form -> using _pmath_create_quotient() directly
      return (pmath_rational_t)_pmath_create_quotient(
        _neg_i(numerator), denominator);
    }

    case PMATH_TYPE_SHIFT_MP_FLOAT: {
      struct _pmath_mp_float_t *result;
      if(num->refcount == 1){
        result = (struct _pmath_mp_float_t*)pmath_ref(num);
      }
      else
        result = _pmath_create_mp_float(
          mpfr_get_prec(((struct _pmath_mp_float_t*)num)->value));

      if(result){
        mpfr_neg(
          ((struct _pmath_mp_float_t*)result)->value,
          ((struct _pmath_mp_float_t*)num)->value,
          GMP_RNDN);
        
        mpfr_set(
          ((struct _pmath_mp_float_t*)result)->error,
          ((struct _pmath_mp_float_t*)num)->error,
          GMP_RNDN);
      }
      
      pmath_unref(num);
      return (pmath_float_t)result;
    }

    case PMATH_TYPE_SHIFT_MACHINE_FLOAT: {
      struct _pmath_machine_float_t *result =
        _pmath_create_machine_float(
          -((struct _pmath_machine_float_t*)num)->value);

      pmath_unref(num);
      return (pmath_float_t)result;
    }
  }
  assert("invalid number type" && 0);
  return NULL;
}

//} ============================================================================
//{ basic integer functions

PMATH_PRIVATE pmath_integer_t _mul_ii(
  pmath_integer_t intA, // will be freed. not NULL!
  pmath_integer_t intB  // will be freed. not NULL!
){
  struct _pmath_integer_t *result = _pmath_create_integer();

  assert(pmath_instance_of(intA, PMATH_TYPE_INTEGER));
  assert(pmath_instance_of(intB, PMATH_TYPE_INTEGER));

  if(result)
    mpz_mul(((struct _pmath_integer_t*)result)->value,
            ((struct _pmath_integer_t*)intA)->value,
            ((struct _pmath_integer_t*)intB)->value);

  pmath_unref(intA);
  pmath_unref(intB);
  return (pmath_number_t)result;
}

//} ============================================================================
//{ pMath object functions for integers ...

static void destroy_integer(struct _pmath_integer_t *integer){
  uintptr_t i = int_cache_inc(+1);
  
  assert(integer->inherited.refcount == 0);
  
  integer = int_cache_swap(i, integer);
  if(integer){
    assert(integer->inherited.refcount == 0);
    
    mpz_clear(integer->value);
    pmath_mem_free(integer);
  }
  //  pmath_stack_push(&unused_integers, NUMBER_TO_STACK_ITEM(integer));
}

static unsigned int hash_init;

static unsigned int hash_integer(struct _pmath_integer_t *integer){
  return incremental_hash(
    integer->value[0]._mp_d,
    sizeof(integer->value[0]._mp_d[0]) * (size_t)abs(integer->value[0]._mp_size),
    hash_init);
}

static void write_integer(
  struct _pmath_integer_t *integer,
  pmath_write_options_t    options,
  pmath_write_func_t       write,
  void                    *user
){
  char *str;
  int base = 10;
  size_t size = mpz_sizeinbase(integer->value, 16) + 2;

//  if(size > 1000)
//    /* if the number is that big, a decimal notation is realy useless and needs
//       time and more space.
//     */
//    base = 16;
//  else
    size = mpz_sizeinbase(integer->value, 10) + 2;

  str = (char*)pmath_mem_alloc(size);
  if(!str){
    write_cstr("<<out-of-memory>>", write, user);
    return;
  }
//  if(base == 16)
//    write_cstr("16^^", write, user);
  mpz_get_str(str, base, integer->value);

  /* mpz_sizeinbase returns 2 for integer->value = 8 or 9, but 1 should be the
     result, since its one digit */

  // calculate strlen faster fo very big values.
  // write(user, str, size - 3 + strlen(str + size - 3));
  write_cstr(str, write, user);

  pmath_mem_free(str);
}

//} ============================================================================
//{ pMath object functions for quotients ...

static void destroy_quotient(struct _pmath_quotient_t *quotient){
  pmath_unref((pmath_integer_t)quotient->numerator);
  pmath_unref((pmath_integer_t)quotient->denominator);
//  pmath_stack_push(&unused_quotients, NUMBER_TO_STACK_ITEM(quotient));
  pmath_mem_free(quotient);
}

static unsigned int hash_quotient(struct _pmath_quotient_t *quotient){
  unsigned int next = 0;
  unsigned int h;
  h = hash_integer(quotient->numerator);
  next = incremental_hash(&h, sizeof(h), next);
  h = hash_integer(quotient->denominator);
  return incremental_hash(&h, sizeof(h), next);
}

static void write_quotient(
  struct _pmath_quotient_t *quotient,
  pmath_write_options_t     options,
  pmath_write_func_t        write,
  void                     *user
){
  write_integer(quotient->numerator,   options, write, user);
  write_cstr("/", write, user);
  write_integer(quotient->denominator, options, write, user);
}

//} ============================================================================
//{ pMath object functions for multi precision floats ...

static void destroy_mp_float(struct _pmath_mp_float_t *f){
  uintptr_t i = mp_cache_inc(+1);
  
  assert(f->inherited.refcount == 0);
  
  f = mp_cache_swap(i, f);
  if(f){
    assert(f->inherited.refcount == 0);
    
    mpfr_clear(f->value);
    mpfr_clear(f->error);
    pmath_mem_free(f);
  }
}

static unsigned int hash_mp_float(struct _pmath_mp_float_t *f){
  unsigned int h = 0;
  h = incremental_hash(&f->value[0]._mpfr_prec, sizeof(mpfr_prec_t), h);
  h = incremental_hash(&f->value[0]._mpfr_sign, sizeof(mpfr_sign_t), h);
  h = incremental_hash(&f->value[0]._mpfr_exp,  sizeof(mp_exp_t), h);

  return incremental_hash(
    f->value[0]._mpfr_d,
    sizeof(mp_limb_t) * (size_t)ceil(f->value[0]._mpfr_prec/(double)mp_bits_per_limb),
    h);
}

static void write_mp_float(
  struct _pmath_mp_float_t *f,
  pmath_write_options_t  options,
  pmath_write_func_t     write,
  void                  *user
){
  mp_exp_t exp;
  size_t digits, size;
  char *str;
  char s[30];
  double prec10 = LOG10_2 * pmath_precision(pmath_ref((pmath_t)f));
  double acc10  = LOG10_2 * pmath_accuracy( pmath_ref((pmath_t)f));
  
  if(mpfr_zero_p(f->value) || prec10 == 0){
    char s[30];
    long exp;
    double d = mpfr_get_d_2exp(&exp, f->error, GMP_RNDN);
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

  mpfr_get_str(str, &exp, 10, digits, f->value, GMP_RNDN);

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

static void destroy_machine_float(struct _pmath_machine_float_t *f){
  uintptr_t i = maf_cache_inc(+1);
  
  assert(f->inherited.refcount == 0);
  
  f = maf_cache_swap(i, f);
  if(f){
    assert(f->inherited.refcount == 0);
    pmath_mem_free(f);
  }
}

static unsigned int hash_machine_float(struct _pmath_machine_float_t *f){
  return incremental_hash(&f->value, sizeof(f->value), 0);
}

static void write_machine_float(
  struct _pmath_machine_float_t *f,
  pmath_write_options_t  options,
  pmath_write_func_t     write,
  void                  *user
){
  char s[100];
  double test;
  int maxprec = 1 + (int)ceil(DBL_MANT_DIG * LOG10_2);
  int len, i;
  
  for(len = 1;len <= maxprec;++len){
    snprintf(s, sizeof(s), "%.*g", len, f->value);
    
    test = strtod(s, NULL);
    if(test == f->value)
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
    write_cstr(".0`*^", write, user);
    
    snprintf(s, sizeof(s), "%d", exp);
    write_cstr(s, write, user);
    return;
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
  /*double d, dexp;
  uint16_t s[100];
  int i, j, dot, exp;

  d = f->value;

  if(!isfinite(d)){
    write_cstr("0.0", write, user);
    return;
  }

  i = 0;
  if(d < 0){
    d = -d;
    s[i++] = '-';
  }

  d = frexp(d, &exp);
  // f->value = +/- d * 2 ^ exp

  //dexp = log10(exp2(exp));
  dexp = exp * LOG10_2;
  exp = (int)dexp;
  d*= pow(10, dexp - exp);

  if(d < 1){
    d*= 10;
    --exp;
  }
  dot = exp;

  if(dot >= 0){
    if(dot < 6){
      ++dot;
      exp = 0;
    }
    else
      dot = 1;
  }
  else if(dot > -6){
    s[i++] = '0';
    s[i++] = '.';
    while(++dot < 0)
      s[i++] = '0';

    dot = -1;
    exp = 0;
  }
  else
    dot = 1;

  j = 0;
  while(++j < 16){
    if(dot-- == 0)
      s[i++] = '.';

    s[i++] = '0' + (int)d;
    d-= (int)d;
    d*= 10;
  }

  s[i++] = '0' + (int)round(d);
  
  if(s[i-1] > '9'){
    --i;
    while(i > 0 && s[i-1] == '9')
      --i;
    
    if(i > 0 && s[i-1] == '.'){
      --i;
      
      dot = i;
      
      while(i > 0 && s[i-1] == '9')
        --i;
    }
    
    if(i > 0)
      s[i-1]++;
  }

  if(dot > 0){
    if(i == 0 || (i == 1 && s[0] == '-'))
      s[i++] = '0';
    s[i++] = '.';
    s[i++] = '0';
  }
  else{
    while(i > 0 && s[i-1] == '0')
      --i;
    if(i > 0 && s[i-1] == '.')
      s[i++] = '0';
  }

  write(user, s, i);
  
  write_cstr("`", write, user);

  if(exp != 0){
    char es[20];
    snprintf(es, sizeof(es), "*^%d", exp);
    write_cstr(es, write, user);
  }*/
}

//} ============================================================================
//{ common pMath object functions for all number types ...

  static int _cmp_ii(pmath_integer_t intA, pmath_integer_t intB){
    if(!intA || !intB)
      return pmath_compare(intA, intB);

    return mpz_cmp(
      ((struct _pmath_integer_t*)intA)->value,
      ((struct _pmath_integer_t*)intB)->value);
  }

static int compare_numbers(
  pmath_number_t numA,
  pmath_number_t numB
){
  if(numA->type_shift == PMATH_TYPE_SHIFT_INTEGER){
    if(numB->type_shift == PMATH_TYPE_SHIFT_INTEGER)
      return mpz_cmp(
        ((struct _pmath_integer_t*)numA)->value,
        ((struct _pmath_integer_t*)numB)->value);

    if(numB->type_shift == PMATH_TYPE_SHIFT_QUOTIENT){
      // cmp(u, w/x) = cmp(u*x,w)  because x > 0
      pmath_integer_t lhs = _mul_ii(
        (pmath_integer_t)pmath_ref(numA),
        pmath_rational_denominator((pmath_rational_t)numB));
      pmath_integer_t rhs = pmath_rational_numerator((pmath_rational_t)numB);
      int result = _cmp_ii(lhs, rhs);
      pmath_unref(lhs);
      pmath_unref(rhs);
      return result;
    }

    if(numB->type_shift == PMATH_TYPE_SHIFT_MP_FLOAT){
      return -mpfr_cmp_z(
        ((struct _pmath_mp_float_t*)numB)->value,
        ((struct _pmath_integer_t*) numA)->value);
    }

    assert(numB->type_shift == PMATH_TYPE_SHIFT_MACHINE_FLOAT);
    return mpz_cmp_d(
      ((struct _pmath_integer_t*)      numA)->value,
      ((struct _pmath_machine_float_t*)numB)->value);
  }

  if(numA->type_shift == PMATH_TYPE_SHIFT_QUOTIENT){
    if(numB->type_shift == PMATH_TYPE_SHIFT_QUOTIENT){
      // cmp(u/v, w/x) = cmp(u*x,v*w)  because v > 0 && x > 0
      pmath_integer_t lhs = _mul_ii(
        pmath_rational_numerator(  (pmath_rational_t)numA),
        pmath_rational_denominator((pmath_rational_t)numB));
      pmath_integer_t rhs = _mul_ii(
        pmath_rational_numerator(  (pmath_rational_t)numB),
        pmath_rational_denominator((pmath_rational_t)numA));
      int result = _cmp_ii(lhs, rhs);
      pmath_unref(lhs);
      pmath_unref(rhs);
      return result;
    }

    if(numB->type_shift == PMATH_TYPE_SHIFT_MP_FLOAT){
      mp_prec_t prec = mpfr_get_prec(((struct _pmath_mp_float_t*)numB)->value);
      struct _pmath_mp_float_t *tmp  = _pmath_create_mp_float(prec);
      struct _pmath_mp_float_t *tmp2 = _pmath_create_mp_float(prec);
      int result;

      if(!tmp || !tmp2){
        pmath_unref((pmath_float_t)tmp);
        pmath_unref((pmath_float_t)tmp2);
        return 1;
      }

      mpfr_set_z(
        tmp->value,
        ((struct _pmath_quotient_t*)numA)->numerator->value,
        GMP_RNDN);

      mpfr_div_z(
        tmp2->value,
        tmp->value,
        ((struct _pmath_quotient_t*)numA)->denominator->value,
        GMP_RNDN);

      result = mpfr_cmp(tmp2->value, ((struct _pmath_mp_float_t*)numB)->value);

      pmath_unref((pmath_float_t)tmp);
      pmath_unref((pmath_float_t)tmp2);

      return result;
    }

    if(numB->type_shift == PMATH_TYPE_SHIFT_MACHINE_FLOAT){
      double q;

      q = mpz_get_d(((struct _pmath_quotient_t*)numA)->numerator->value);
      q/= mpz_get_d(((struct _pmath_quotient_t*)numA)->denominator->value);

      if(q < ((struct _pmath_machine_float_t*)numB)->value)
        return -1;
      if(q > ((struct _pmath_machine_float_t*)numB)->value)
        return 1;
      return 0;
    }

    return -compare_numbers(numB, numA);
  }

  if(numA->type_shift == PMATH_TYPE_SHIFT_MP_FLOAT){
    if(numB->type_shift == PMATH_TYPE_SHIFT_MP_FLOAT){
      mp_prec_t precA = mpfr_get_prec(((struct _pmath_mp_float_t*)numA)->value);
      mp_prec_t precB = mpfr_get_prec(((struct _pmath_mp_float_t*)numB)->value);
      
      if(precA < precB){
        struct _pmath_mp_float_t *tmp = _pmath_create_mp_float(precA);
        int result;
        
        if(tmp){
          mpfr_set(tmp->value, ((struct _pmath_mp_float_t*)numB)->value, GMP_RNDN);
          
          result = mpfr_cmp(((struct _pmath_mp_float_t*)numA)->value, tmp->value);
          
          pmath_unref((pmath_float_t)tmp);
          return result;
        }
      }
      else if(precA > precB){
        struct _pmath_mp_float_t *tmp = _pmath_create_mp_float(precB);
        int result;
        
        if(tmp){
          mpfr_set(tmp->value, ((struct _pmath_mp_float_t*)numA)->value, GMP_RNDN);
          
          result = mpfr_cmp(((struct _pmath_mp_float_t*)numA)->value, tmp->value);
          
          pmath_unref((pmath_float_t)tmp);
          return result;
        }
      }
    
      return mpfr_cmp(
        ((struct _pmath_mp_float_t*)numA)->value,
        ((struct _pmath_mp_float_t*)numB)->value);
    }

    if(numB->type_shift == PMATH_TYPE_SHIFT_MACHINE_FLOAT){
      double d = mpfr_get_d(((struct _pmath_mp_float_t*)numA)->value, GMP_RNDN);
      
      if(d < ((struct _pmath_machine_float_t*)numB)->value)
        return -1;
        
      if(d > ((struct _pmath_machine_float_t*)numB)->value)
        return 1;
        
      return 0;
    }

    return -compare_numbers(numB, numA);
  }

  assert(numA->type_shift == PMATH_TYPE_SHIFT_MACHINE_FLOAT);

  if(numB->type_shift == PMATH_TYPE_SHIFT_MACHINE_FLOAT){
    if(((struct _pmath_machine_float_t*)numA)->value < ((struct _pmath_machine_float_t*)numB)->value)
      return -1;

    if(((struct _pmath_machine_float_t*)numA)->value > ((struct _pmath_machine_float_t*)numB)->value)
      return 1;

    return 0;
  }

  return -compare_numbers(numB, numA);
}

static pmath_bool_t equal_numbers(
  pmath_number_t numA,
  pmath_number_t numB
){
  if(numA->type_shift == PMATH_TYPE_SHIFT_INTEGER){
    if(numB->type_shift == PMATH_TYPE_SHIFT_INTEGER){
      return 0 == mpz_cmp(
        ((struct _pmath_integer_t*)numA)->value,
        ((struct _pmath_integer_t*)numB)->value);
    }
    
    return FALSE;
  }
  else if(numB->type_shift == PMATH_TYPE_SHIFT_INTEGER)
    return equal_numbers(numB, numA);

  if(numA->type_shift == PMATH_TYPE_SHIFT_QUOTIENT){
    if(numB->type_shift == PMATH_TYPE_SHIFT_QUOTIENT){
      return 0 == mpz_cmp(
                    ((struct _pmath_quotient_t*)numA)->numerator->value,
                    ((struct _pmath_quotient_t*)numB)->numerator->value)
          && 0 == mpz_cmp(
                    ((struct _pmath_quotient_t*)numA)->denominator->value,
                    ((struct _pmath_quotient_t*)numB)->denominator->value);
    }
    
    return FALSE;
  }
  else if(numB->type_shift == PMATH_TYPE_SHIFT_QUOTIENT)
    return equal_numbers(numB, numA);

  if(numA->type_shift == PMATH_TYPE_SHIFT_MP_FLOAT
  && numB->type_shift == PMATH_TYPE_SHIFT_MP_FLOAT){
    mp_prec_t precA = mpfr_get_prec(((struct _pmath_mp_float_t*)numA)->value);
    mp_prec_t precB = mpfr_get_prec(((struct _pmath_mp_float_t*)numB)->value);
    
    if(precA < precB){
      struct _pmath_mp_float_t *tmp = _pmath_create_mp_float(precA);
      pmath_bool_t result;
      
      if(tmp){
        mpfr_set(tmp->value, ((struct _pmath_mp_float_t*)numB)->value, GMP_RNDN);
        
        result = mpfr_equal_p(((struct _pmath_mp_float_t*)numA)->value, tmp->value);
        
        pmath_unref((pmath_float_t)tmp);
        return result;
      }
    }
    else if(precA > precB){
      struct _pmath_mp_float_t *tmp = _pmath_create_mp_float(precB);
      pmath_bool_t result;
      
      if(tmp){
        mpfr_set(tmp->value, ((struct _pmath_mp_float_t*)numA)->value, GMP_RNDN);
        
        result = mpfr_equal_p(tmp->value, ((struct _pmath_mp_float_t*)numA)->value);
        
        pmath_unref((pmath_float_t)tmp);
        return result;
      }
    }
    
    return mpfr_equal_p(
      ((struct _pmath_mp_float_t*)numA)->value,
      ((struct _pmath_mp_float_t*)numB)->value);
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

//  void dummy(void *user, const uint16_t *data, int len){
//    while(len--){
//      fputc(*data, stderr);
//      ++data;
//    }
//  }
//
//  void test(double d){
//    struct _pmath_machine_float_t f;
//    f.value = d;
//
//    fprintf(stderr, "%g -> ", d);
//    write_machine_float(&f, 0, dummy, NULL);
//    fprintf(stderr, "\n");
//  }


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
  
//  test(-1.6e10);
//  test(-0.1);
//  test(-0.0);
//  test(-1.7);
//  test(1/3.);
//  test(5.6e-5);
//  test(5.6e-6);
//  test(5.6e5);

  gmp_randinit_default(_pmath_randstate);
  _pmath_rand_spinlock = 0;

//  memset(&unused_quotients,      0, sizeof(unused_quotients));
  memset(&special_values_wrong_indices, 0, sizeof(special_values_wrong_indices));
  
  _pmath_init_special_type(
    PMATH_TYPE_SHIFT_INTEGER,
    (pmath_compare_func_t)        compare_numbers,
    (pmath_hash_func_t)           hash_integer,
    (pmath_proc_t)                destroy_integer,
    (pmath_equal_func_t)          equal_numbers,
    (_pmath_object_write_func_t)  write_integer);

  _pmath_init_special_type(
    PMATH_TYPE_SHIFT_QUOTIENT,
    (pmath_compare_func_t)        compare_numbers,
    (pmath_hash_func_t)           hash_quotient,
    (pmath_proc_t)                destroy_quotient,
    (pmath_equal_func_t)          equal_numbers,
    (_pmath_object_write_func_t)  write_quotient);

  _pmath_init_special_type(
    PMATH_TYPE_SHIFT_MP_FLOAT,
    (pmath_compare_func_t)        compare_numbers,
    (pmath_hash_func_t)           hash_mp_float,
    (pmath_proc_t)                destroy_mp_float,
    (pmath_equal_func_t)          equal_numbers,
    (_pmath_object_write_func_t)  write_mp_float);

  _pmath_init_special_type(
    PMATH_TYPE_SHIFT_MACHINE_FLOAT,
    (pmath_compare_func_t)        compare_numbers,
    (pmath_hash_func_t)           hash_machine_float,
    (pmath_proc_t)                destroy_machine_float,
    (pmath_equal_func_t)          equal_numbers,
    (_pmath_object_write_func_t)  write_machine_float);

  for(i = SPECIAL_MIN;i <= SPECIAL_MAX;i++){
    special_values[i] = (pmath_integer_t)_pmath_create_integer();
    if(!special_values[i])
      goto FAIL;

    mpz_set_si(((struct _pmath_integer_t*)special_values[i])->value, i);
  }

  _pmath_one_half = (pmath_quotient_t)_pmath_create_quotient(
    pmath_ref(special_values[1]),
    pmath_ref(special_values[2]));

  if(!_pmath_one_half)
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

//  destroy_all_unused_quotients();
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
