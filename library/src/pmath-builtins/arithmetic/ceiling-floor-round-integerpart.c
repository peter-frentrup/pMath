#include <pmath-core/numbers-private.h>

#include <pmath-util/approximate.h>
#include <pmath-util/concurrency/threads-private.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/arithmetic-private.h>
#include <pmath-builtins/build-expr-private.h>


extern pmath_symbol_t pmath_System_Ceiling;
extern pmath_symbol_t pmath_System_Complex;
extern pmath_symbol_t pmath_System_Floor;
extern pmath_symbol_t pmath_System_IntegerPart;
extern pmath_symbol_t pmath_System_Power;
extern pmath_symbol_t pmath_System_Round;
extern pmath_symbol_t pmath_System_Times;

static void _pmath_round_arf_to_nearest(arf_t result, const arf_t x) {
  if(arf_is_int(x)) {
    arf_set(result, x);
    return;
  }
  
  if(result == x) {
    arf_t tmp;
    arf_init(tmp);
    arf_set(tmp, x);
    _pmath_round_arf_to_nearest(result, tmp);
    arf_clear(tmp);
  }
  
  if(arf_is_int_2exp_si(x, -1)) { // x is a half integer: round to even
    arf_floor(result, x);
    if(!arf_is_int_2exp_si(x, 1)) { // Floor(x) is odd
      arf_ceil(result, x);
    }
    return;
  }
  else {
    arf_t diff;
    arf_init(diff);
    
    arf_floor(result, x);
    arf_sub(diff, x, result, ARF_PREC_EXACT, ARF_RND_NEAR);
    if(arf_cmp_2exp_si(diff, -1) > 0)
      arf_ceil(result, x);
      
    arf_clear(diff);
  }
}

static void _pmath_integerpart_arf(arf_t result, const arf_t x) {
  if(arf_sgn(x) > 0)
    arf_floor(result, x);
  else
    arf_ceil(result, x);
}

static pmath_bool_t try_arf_to_integer(pmath_t *x, const arf_t value) {
  if(arf_cmpabs_2exp_si(value, 1000000) < 0) {
    fmpz_t result;
    fmpz_init(result);
    arf_get_fmpz(result, value, ARF_RND_NEAR);
    pmath_unref(*x);
    *x = _pmath_integer_from_fmpz(result);
    fmpz_clear(result);
    return TRUE;
  }
  return FALSE;
}

typedef void (*arf_func_t)(arf_t, const arf_t);

static pmath_bool_t try_arb_to_integer(pmath_t *x, const arb_t value, slong precision, arf_func_t round_func) {
  if(arb_is_exact(value)) {
    arf_t a;
    arf_init(a);
    
    round_func(a, arb_midref(value));
    if(try_arf_to_integer(x, a)) {
      arf_clear(a);
      return TRUE;
    }
    
    arf_clear(a);
  }
  else if(mag_cmp_2exp_si(arb_radref(value), -1) < 0) {
    arf_t a;
    arf_t b;
    
    arf_init(a);
    arf_init(b);
    _pmath_arb_bounds(a, b, value, precision);
    round_func(a, a);
    round_func(b, b);
    if(arf_equal(a, b)) {
      if(try_arf_to_integer(x, a)) {
        arf_clear(a);
        arf_clear(b);
        return TRUE;
      }
    }
    arf_clear(a);
    arf_clear(b);
  }
  return FALSE;
}

static pmath_bool_t try_round_inexact(pmath_t *x, arf_func_t round_func) {
  if(pmath_is_double(*x))
    *x = _pmath_create_mp_float_from_d(PMATH_AS_DOUBLE(*x));
    
  if(pmath_is_mpfloat(*x))
    return try_arb_to_integer(x, PMATH_AS_ARB(*x), PMATH_AS_ARB_WORKING_PREC(*x), round_func);
    
  if(pmath_is_expr_of_len(*x, pmath_System_Complex, 2)) {
    pmath_t re = pmath_expr_get_item(*x, 1);
    pmath_t im = pmath_expr_get_item(*x, 2);
    
    if(try_round_inexact(&re, round_func) && try_round_inexact(&im, round_func)) {
      *x = pmath_expr_set_item(*x, 1, re);
      *x = pmath_expr_set_item(*x, 2, im);
      return TRUE;
    }
    
    pmath_unref(re);
    pmath_unref(im);
  }
  
  return FALSE;
}

static slong arb_magnitude_as_precision(const arb_t x) {
  slong prec = arf_abs_bound_lt_2exp_si(arb_midref(x));
  return FLINT_MAX(prec, 0);
}

static slong magnitude_as_precision(pmath_t obj) { // obj wont be freed
  if(pmath_is_double(obj)) {
    int exp = 0;
    frexp(PMATH_AS_DOUBLE(obj), &exp);
    slong prec = (slong)exp + 1;
    return FLINT_MAX(prec, 0);
  }
  
  if(pmath_is_mpfloat(obj)) 
    return arb_magnitude_as_precision(PMATH_AS_ARB(obj));
  
  if(pmath_is_expr_of_len(obj, pmath_System_Complex, 2)) {
    pmath_t re = pmath_expr_get_item(obj, 1);
    pmath_t im = pmath_expr_get_item(obj, 2);
    
    slong prec_re = magnitude_as_precision(re);
    slong prec_im = magnitude_as_precision(im);
    
    pmath_unref(re);
    pmath_unref(im);
    
    return FLINT_MAX(prec_re, prec_im);
  }
  
  return 0;
}

static pmath_bool_t try_round_numeric(pmath_t *x, arf_func_t round_func) {
  pmath_thread_t me = pmath_thread_get_current();
  double precision;
  
  if(!me)
    return FALSE;
  
  precision = FLINT_MIN(16, me->max_extra_precision);
  precision = FLINT_MAX(me->min_precision, precision);
  
  pmath_t approx = pmath_set_precision(pmath_ref(*x), precision);
  if(try_round_inexact(&approx, round_func)) {
    pmath_unref(*x);
    *x = approx;
    return TRUE;
  }
  
  slong necessary_prec = magnitude_as_precision(approx);
  pmath_unref(approx);
  
  precision = FLINT_MIN(FLINT_MAX(precision, necessary_prec) + 2, me->max_precision);
  double max_precision = FLINT_MIN(me->max_precision, precision + me->max_extra_precision);
  
  while(!pmath_aborting()) {
    approx = pmath_set_precision(pmath_ref(*x), precision);
    
    if(try_round_inexact(&approx, round_func)) {
      pmath_unref(*x);
      *x = approx;
      return TRUE;
    }
    
    if(!pmath_is_float(approx) && !_pmath_is_nonreal_complex_number(approx))  {
      pmath_unref(approx);
      break;
    }
    
    if(precision >= max_precision) {
      pmath_unref(approx);
      break;
    }
    
    necessary_prec = magnitude_as_precision(approx);
    double effective_prec = pmath_precision(approx); // frees approx
    
    if(precision >= max_precision)
      break;
    
    if(effective_prec <= necessary_prec) {
      double precision_delta = necessary_prec - effective_prec + 2;
      precision = FLINT_MIN(precision + precision_delta, max_precision);
    }
    else { // Unexpected. try_round_inexact() should have succeeded.
      precision = FLINT_MIN(2 * precision, max_precision);
    }
  }
  
  if(precision >= max_precision) {
    pmath_message(
      PMATH_NULL, "meprec", 2,
      _pmath_from_precision(me->max_extra_precision),
      pmath_ref(*x));
  }
  
  return FALSE;
}



static arf_func_t get_rounding_function(pmath_t head) {
  if(pmath_same(head, pmath_System_Ceiling))
    return arf_ceil;
    
  if(pmath_same(head, pmath_System_Floor))
    return arf_floor;
    
  if(pmath_same(head, pmath_System_IntegerPart))
    return _pmath_integerpart_arf;
    
  /*if(pmath_same(head, pmath_System_Round)) */
  return _pmath_round_arf_to_nearest;
}

static pmath_integer_t round_q(pmath_quotient_t x, pmath_t head) {
  pmath_mpint_t num, den, result;
  
  assert(pmath_is_quotient(x));
  
  num = pmath_ref(PMATH_QUOT_NUM(x));
  den = pmath_ref(PMATH_QUOT_DEN(x));
  result = _pmath_create_mp_int(0);
  
  if(pmath_is_int32(num))
    num = _pmath_create_mp_int(PMATH_AS_INT32(num));
    
  if(pmath_is_int32(den))
    den = _pmath_create_mp_int(PMATH_AS_INT32(den));
    
  if(pmath_is_null(result) || pmath_is_null(num) || pmath_is_null(den)) {
    pmath_unref(x);
    pmath_unref(num);
    pmath_unref(den);
    pmath_unref(head);
    return PMATH_NULL;
  }
  
  assert(pmath_is_mpint(num));
  assert(pmath_is_mpint(den));
  
  if(pmath_same(head, pmath_System_Ceiling)) {
    mpz_cdiv_q(
      PMATH_AS_MPZ(result),
      PMATH_AS_MPZ(num),
      PMATH_AS_MPZ(den));
  }
  else if(pmath_same(head, pmath_System_Floor)) {
    mpz_fdiv_q(
      PMATH_AS_MPZ(result),
      PMATH_AS_MPZ(num),
      PMATH_AS_MPZ(den));
  }
  else if(pmath_same(head, pmath_System_IntegerPart)) {
    mpz_tdiv_q(
      PMATH_AS_MPZ(result),
      PMATH_AS_MPZ(num),
      PMATH_AS_MPZ(den));
  }
  else { /*if(pmath_same(head, pmath_System_Round)) */
    pmath_bool_t even;
    int cmp;
    pmath_mpint_t rem  = _pmath_create_mp_int(0);
    pmath_mpint_t half = _pmath_create_mp_int(0);
    
    if(!pmath_is_null(rem) && !pmath_is_null(half)) {
      mpz_fdiv_qr(
        PMATH_AS_MPZ(result),
        PMATH_AS_MPZ(rem),
        PMATH_AS_MPZ(num),
        PMATH_AS_MPZ(den));
        
      even = mpz_even_p(PMATH_AS_MPZ(den));
      
      mpz_fdiv_q_2exp(
        PMATH_AS_MPZ(half),
        PMATH_AS_MPZ(den),
        1);
        
      cmp = mpz_cmp(PMATH_AS_MPZ(rem), PMATH_AS_MPZ(half));
      if( cmp > 0 ||
          (cmp == 0 &&
           even &&
           mpz_odd_p(PMATH_AS_MPZ(result))))
      {
        mpz_add_ui(PMATH_AS_MPZ(result), PMATH_AS_MPZ(result), 1);
      }
    }
    else {
      pmath_unref(result);
      result = PMATH_NULL;
    }
    
    pmath_unref(rem);
    pmath_unref(half);
  }
  
  pmath_unref(num);
  pmath_unref(den);
  pmath_unref(x);
  pmath_unref(head);
  return _pmath_mp_int_normalize(result);
}

PMATH_PRIVATE pmath_t builtin_round_functions(pmath_expr_t expr) {
  /* Ceiling(x)                     smallest integer greater than or equal to x
     Ceiling(x, a)                  smallest multiple of a that is not less than x
     Ceiling(a + b*ImaginaryI)      = Ceil(a) + Ceil(b)*ImaginaryI
  
     Floor(x)                       greatest integer less than or equal to x
     Floor(x, a)                    greatest multiple of a that is not greater than x
     Floor(a + b*ImaginaryI)        = Floor(a) + Floor(b)*ImaginaryI
  
     Round(x)                       integer closest to x, ties to even
     Round(x, a)                    nearest multiple of a to x
     Round(a + b*ImaginaryI)        = Round(a) + Round(b)*ImaginaryI
  
     IntegerPart(x)                 round to zero
     IntegerPart(a + b*ImaginaryI)  = IntegerPart(a) + IntegerPart(b)*ImaginaryI
  
   */
  size_t len = pmath_expr_length(expr);
  pmath_t head, x;
  
  head = pmath_expr_get_item(expr, 0);
  x    = pmath_expr_get_item(expr, 1);
  if(len == 1) {
    if(_pmath_is_nonreal_complex_number(x)) {
      pmath_t re = pmath_expr_get_item(x, 1);
      pmath_t im = pmath_expr_get_item(x, 2);
      pmath_unref(x);
      pmath_unref(expr);
      x = COMPLEX(
            FUNC(pmath_ref(head), re),
            FUNC(pmath_ref(head), im));
      pmath_unref(head);
      return x;
    }
    
    if(pmath_is_integer(x)) {
      pmath_unref(expr);
      pmath_unref(head);
      return x;
    }
    
    if(pmath_is_quotient(x)) {
      pmath_unref(expr);
      return round_q(x, head);
    }
    
    if(try_round_inexact(&x, get_rounding_function(head))) {
      pmath_unref(expr);
      pmath_unref(head);
      return x;
    }
    
    if(pmath_is_numeric(x)) {
      if(try_round_numeric(&x, get_rounding_function(head))) {
        pmath_unref(expr);
        pmath_unref(head);
        return x;
      }
    }
    
    pmath_unref(head);
    pmath_unref(x);
    return expr;
  }
  
  if(pmath_same(head, pmath_System_IntegerPart)) {
    pmath_unref(head);
    pmath_unref(x);
    pmath_message_argxxx(len, 1, 1);
    return expr;
  }
  
  if(len == 2) {
    pmath_t a, div;
    a = pmath_expr_get_item(expr, 2);
    div = pmath_evaluate(DIV(x, pmath_ref(a)));
    pmath_unref(expr);
    return TIMES(pmath_expr_new_extended(head, 1, div), a);
  }
  
  pmath_unref(head);
  pmath_unref(x);
  pmath_message_argxxx(len, 1, 2);
  return expr;
}
