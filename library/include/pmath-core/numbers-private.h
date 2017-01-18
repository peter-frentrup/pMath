#ifndef __PMATH_CORE__NUMBERS_PRIVATE_H__
#define __PMATH_CORE__NUMBERS_PRIVATE_H__

#ifndef BUILDING_PMATH
#  error This header file is not part of the public pMath API
#endif

#include <pmath-core/numbers.h>

#include <float.h>
#include <math.h>


#ifdef PMATH_OS_WIN32
// Needed by mpir/gmp to use dllimport declarations
#  define MSC_USE_DLL
#endif

#ifdef _MSC_VER
#  pragma warning(push)
// In mpz_get_ui(): Converting 'mp_limb_t' to 'unsigned long': possible data loss.
#    pragma warning(disable: 4244)
#    include <gmp.h>
#    include <arb.h>
#  pragma warning(pop)
#else
#  include <gmp.h>
#  include <arb.h>
#endif // _MSC_VER

#include <mpfr.h>


#ifdef PMATH_OS_WIN32
//#  define exp2(x)      pow(2, x)
//#  define log2(x)      (log(x)/M_LN2)
//#  define round(x)     floor((x) + 0.5)

#  ifndef isfinite
#    warning "isfinite not defined"
#    define isfinite(x)  _finite(x)
#  endif

#  ifndef isnan
#    warning "isnan not defined"
#    define isnan(x)  _isnan(x)
#  endif
#elif defined(sun) || defined(__sun)
#  ifdef isfinite
#    undef isfinite
#  endif

#  ifdef isnan
#    undef isnan
#  endif

#  include <ieeefp.h>

#  define isfinite(x)  finite(x)
#  define isnan(x)     isnand(x)
#endif

#ifndef M_PI
#  warning "M_PI not defined"
#  define M_PI    3.14159265358979323846
#endif

#ifndef M_E
#  warning "M_E not defined"
#  define M_E   2.7182818284590452354
#endif

#define LOGE_2    0.69314718055994530941723212145818 // consider using M_LN2 ?
#define LOG2_10   3.3219280948873623478703194294894
#define LOG10_2   0.30102999566398119521373889472449

#define PMATH_MP_PREC_MAX    1000000

struct _pmath_mp_int_t {
  struct _pmath_t  inherited;
  mpz_t            value;
};

/* We don't use mpq_t to store quotients, because this would involve a lot of
   memory copying during automatic quotient-to-integer conversion.

   When freeing a quotient, its numerator and denominator are freed seperately
   to be stored in the integer cache.
 */
struct _pmath_quotient_t {
  struct _pmath_t  inherited;
  pmath_integer_t  numerator;
  pmath_integer_t  denominator;
};

/* A number with absolute uncertainty "error". So this represents
   value +/- 1/2 error
 */
struct _pmath_mp_float_t {
  struct _pmath_t  inherited;
  mpfr_t           value_old; // deprecated
  arb_t            value_new;
  slong            working_precision;        
};

#define PMATH_QUOT_NUM(obj)       (((struct _pmath_quotient_t*)     PMATH_AS_PTR(obj))->numerator)
#define PMATH_QUOT_DEN(obj)       (((struct _pmath_quotient_t*)     PMATH_AS_PTR(obj))->denominator)

#define PMATH_AS_MPZ(obj)         (((struct _pmath_mp_int_t*)       PMATH_AS_PTR(obj))->value)
#define PMATH_AS_MP_VALUE(obj)    (((struct _pmath_mp_float_t*)     PMATH_AS_PTR(obj))->value_old)

#define PMATH_AS_ARB(obj)                (((struct _pmath_mp_float_t*)     PMATH_AS_PTR(obj))->value_new)
#define PMATH_AS_ARB_WORKING_PREC(obj)   (((struct _pmath_mp_float_t*)     PMATH_AS_PTR(obj))->working_precision)

PMATH_FORCE_INLINE mpfr_prec_t min_prec(mpfr_prec_t a, mpfr_prec_t b) {
  if(a < b)
    return a;
  return b;
}

/*============================================================================*/

extern PMATH_PRIVATE pmath_quotient_t _pmath_one_half; /* readonly */

// initialization in pmath.c:
extern PMATH_PRIVATE pmath_t _pmath_object_overflow;         /* readonly */
extern PMATH_PRIVATE pmath_t _pmath_object_underflow;        /* readonly */
extern PMATH_PRIVATE pmath_t _pmath_object_pos_infinity;     /* readonly */
extern PMATH_PRIVATE pmath_t _pmath_object_neg_infinity;     /* readonly */
extern PMATH_PRIVATE pmath_t _pmath_object_complex_infinity; /* readonly */

extern PMATH_PRIVATE gmp_randstate_t  _pmath_randstate;
extern PMATH_PRIVATE pmath_atomic_t   _pmath_rand_spinlock;

PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
// struct _pmath_integer_t_ *
pmath_mpint_t _pmath_create_mp_int(signed long value);

PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
// struct _pmath_quotient_t_ *
pmath_quotient_t _pmath_create_quotient(
  pmath_integer_t numerator,    // will be freed; must not be divisible by denominator and must not be 0
  pmath_integer_t denominator); // will be freed; must be > 1

PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
//struct _pmath_mp_float_t *
pmath_mpfloat_t _pmath_create_mp_float(mpfr_prec_t precision);

PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
//struct _pmath_mp_float_t *
pmath_mpfloat_t _pmath_create_mp_float_from_d(double value);

/** \brief Create an mp float from a rational number.
    \param value      An integer or quotient. It will be freed.
    \param precision  Working precision of the result (and precision of the approximation)
    \return A new _pmath_mp_float_t* or PMATH_NULL.
 */
PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
//struct _pmath_mp_float_t *
pmath_mpfloat_t _pmath_create_mp_float_from_q(pmath_rational_t value, slong precision);

PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
// struct _pmath_mp_float_t*
pmath_mpfloat_t _pmath_convert_to_mp_float(pmath_float_t n); // n will be freed

PMATH_PRIVATE
void _pmath_write_machine_float(struct pmath_write_ex_t *info, pmath_t f);

PMATH_PRIVATE
void _pmath_write_machine_int(struct pmath_write_ex_t *info, pmath_t integer);

PMATH_PRIVATE
int _pmath_numbers_compare(
  pmath_number_t numA,
  pmath_number_t numB);

PMATH_PRIVATE
pmath_bool_t _pmath_numbers_equal(
  pmath_number_t numA,
  pmath_number_t numB);

PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
pmath_t _pmath_float_exceptions(
  pmath_number_t x);  // will be freed.
  
PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
pmath_t _pmath_mpfloat_call(
  pmath_mpfloat_t   arg,  // will be freed
  int             (*func)(mpfr_ptr, mpfr_srcptr,mpfr_rnd_t));

// returns pmath_thread_current()->mp_rounding_mode; and MPFR_RNDN on error.
PMATH_PRIVATE
mpfr_rnd_t _pmath_current_rounding_mode(void);

PMATH_PRIVATE
pmath_integer_t _pmath_mp_int_normalize(pmath_mpint_t f);

PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
pmath_integer_t _mul_ii(
  pmath_integer_t intA,  // will be freed. not PMATH_NULL!
  pmath_integer_t intB); // will be freed. not PMATH_NULL!

PMATH_PRIVATE void         _pmath_numbers_memory_panic(void);
PMATH_PRIVATE pmath_bool_t _pmath_numbers_init(void);
PMATH_PRIVATE void         _pmath_numbers_done(void);

#endif /* __PMATH_CORE__NUMBERS_PRIVATE_H__ */
