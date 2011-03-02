#ifndef __PMATH_CORE__NUMBERS_PRIVATE_H__
#define __PMATH_CORE__NUMBERS_PRIVATE_H__

#ifndef BUILDING_PMATH
  #error This header file is not part of the public pMath API
#endif

#include <pmath-core/numbers.h>

#include <float.h>
#include <math.h>
#include <gmp.h>
#include <mpfr.h>

#ifdef PMATH_OS_WIN32
  #define exp2(x)      pow(2, x)
  #define log2(x)      (log(x)/M_LN2)
  #define round(x)     floor((x) + 0.5)
  
  #ifndef isfinite
    #define isfinite(x)  _finite(x)
  #endif
  
  #ifndef isnan
    #define isnan(x)  _isnan(x)
  #endif
#elif defined(sun) || defined(__sun)
  #ifdef isfinite
    #undef isfinite
  #endif
  
  #ifdef isnan
    #undef isnan
  #endif
  
  #include <ieeefp.h>
  
  #define isfinite(x)  finite(x)
  #define isnan(x)     isnand(x)
#endif

#ifndef M_PI
  #define M_PI		3.14159265358979323846
#endif

#ifndef M_E
  #define M_E		2.7182818284590452354
#endif

#define LOG2_10   3.3219280948873623478703194294894
#define LOG10_2   0.30102999566398119521373889472449

#define PMATH_MP_ERROR_PREC  DBL_MANT_DIG
#define PMATH_MP_PREC_MAX    1000000

struct _pmath_integer_t_{
  struct _pmath_t  inherited;
  mpz_t            value;
};

/* We don't use mpq_t to store quotients, because this would involve a lot of
   memory copying during automatic quotient-to-integer conversion. 
   
   When freeing a quotient, its numerator and denominator are freed seperately 
   to be stored in the integer cache.
 */
struct _pmath_quotient_t_{
  struct _pmath_t  inherited;
  pmath_integer_t  numerator;
  pmath_integer_t  denominator;
};

struct _pmath_mp_float_t{
  struct _pmath_t  inherited;
  mpfr_t           value;
  mpfr_t           error;
};

struct _pmath_machine_float_t{
  struct _pmath_t  inherited;
  double           value;
};

#define PMATH_AS_DOUBLE(objA)      (((struct _pmath_machine_float_t*)PMATH_AS_PTR(objA))->value)

#define PMATH_QUOT_NUM(objA)       (((struct _pmath_quotient_t_*)    PMATH_AS_PTR(objA))->numerator)
#define PMATH_QUOT_DEN(objA)       (((struct _pmath_quotient_t_*)    PMATH_AS_PTR(objA))->denominator)

#define PMATH_AS_MPZ(objA)         (((struct _pmath_integer_t_*)     PMATH_AS_PTR(objA))->value)
#define PMATH_AS_MP_VALUE(objA)    (((struct _pmath_mp_float_t*)     PMATH_AS_PTR(objA))->value)
#define PMATH_AS_MP_ERROR(objA)    (((struct _pmath_mp_float_t*)     PMATH_AS_PTR(objA))->error)

/*============================================================================*/

extern PMATH_PRIVATE pmath_integer_t *special_values; /* do not refer direcly */
#define PMATH_NUMBER_MINUSONE  (special_values[-1])  /* readonly */
#define PMATH_NUMBER_ZERO      (special_values[0])   /* readonly */
#define PMATH_NUMBER_ONE       (special_values[1])   /* readonly */
#define PMATH_NUMBER_TWO       (special_values[2])   /* readonly */

extern PMATH_PRIVATE pmath_quotient_t _pmath_one_half; /* readonly */

// initialization in pmath.c:
extern PMATH_PRIVATE pmath_t _pmath_object_overflow;         /* readonly */
extern PMATH_PRIVATE pmath_t _pmath_object_underflow;        /* readonly */
extern PMATH_PRIVATE pmath_t _pmath_object_infinity;         /* readonly */
extern PMATH_PRIVATE pmath_t _pmath_object_complex_infinity; /* readonly */

extern PMATH_PRIVATE gmp_randstate_t  _pmath_randstate;
extern PMATH_PRIVATE PMATH_DECLARE_ATOMIC(_pmath_rand_spinlock);

PMATH_PRIVATE 
PMATH_ATTRIBUTE_USE_RESULT
// struct _pmath_integer_t_ *
pmath_integer_t _pmath_create_integer(void);

PMATH_PRIVATE 
PMATH_ATTRIBUTE_USE_RESULT
// struct _pmath_quotient_t_ *
pmath_quotient_t _pmath_create_quotient(
  pmath_integer_t numerator,    // will be freed; must not be divisible by denominator and must not be 0
  pmath_integer_t denominator); // will be freed; must be > 1

PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT 
struct _pmath_mp_float_t *_pmath_create_mp_float(mp_prec_t precision);

PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT 
struct _pmath_mp_float_t *_pmath_create_mp_float_from_d(double value);

PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT 
// struct _pmath_mp_float_t*
pmath_float_t _pmath_convert_to_mp_float(pmath_float_t n); // n will be freed

PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT 
// struct _pmath_machine_float_t *
pmath_float_t _pmath_create_machine_float(double value);

PMATH_PRIVATE
mp_prec_t _pmath_float_precision( // 0 = MachinePrecision
  pmath_float_t x); // wont be freed

PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
pmath_t _pmath_float_exceptions(
  pmath_number_t x);  // will be freed.

PMATH_PRIVATE
void _pmath_mp_float_normalize(struct _pmath_mp_float_t *f);

PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
pmath_integer_t _mul_ii(
  pmath_integer_t intA,  // will be freed. not PMATH_NULL!
  pmath_integer_t intB); // will be freed. not PMATH_NULL!

PMATH_PRIVATE void         _pmath_numbers_memory_panic(void);
PMATH_PRIVATE pmath_bool_t _pmath_numbers_init(void);
PMATH_PRIVATE void         _pmath_numbers_done(void);

#endif /* __PMATH_CORE__NUMBERS_PRIVATE_H__ */
