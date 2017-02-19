#include <pmath-builtins/logic-private.h>

#include <pmath-core/intervals-private.h>

#include <pmath-util/approximate.h>
#include <pmath-util/concurrency/threads-private.h>
#include <pmath-util/debug.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/arithmetic-private.h>
#include <pmath-builtins/build-expr-private.h>

#ifdef MIN
#  undef MIN
#endif

#define MIN(a, b)  (((a) < (b)) ? (a) : (b))

#define TOLERANCE_EXPONENT   6
#define TOLERANCE_FACTOR     (1 << TOLERANCE_EXPONENT)

/* Two positive numbers X, Y are "almost equal" if
      Max(X,Y) / Min(X,Y) <= (1 + TOLERANCE_FACTOR * epsilon)
   where epsilon = DBL_EPSILON if one of X,Y is a machine float, and
   epsilon = Max(error(X), error(Y)) otherwise.

   A number X is "almost equal" zero if Abs(X) <= error(X).

   Two numbers with opposite sign are never "almost equal".

   To negative numbers are "almost equal" if their absolute values are so.

   "almost equal" is of course not transitive.
 */

// x and y must have the same sign
static pmath_bool_t slow_almost_equal_mpf(mpfr_srcptr x, mpfr_srcptr y) {
  double tol_x = 1 + TOLERANCE_FACTOR * pow(0.5, mpfr_get_prec(x));
  double tol_y = 1 + TOLERANCE_FACTOR * pow(0.5, mpfr_get_prec(y));
  double tol = tol_x > tol_y ? tol_x : tol_y;
  MPFR_DECL_INIT(rhs, DBL_MANT_DIG);
  
  if(mpfr_cmpabs(x, y) <= 0) {
    mpfr_mul_d(rhs, y, tol, GMP_RNDU);
    
    return mpfr_cmpabs(x, rhs) <= 0;
  }
  else {
    mpfr_mul_d(rhs, x, tol, GMP_RNDU);
    
    return mpfr_cmpabs(y, rhs) <= 0;
  }
}

// reference implementation without fast paths:
static pmath_bool_t slow_almost_equal_numbers(pmath_number_t x, pmath_number_t y) {
  int sign_x, sign_y;
  pmath_bool_t is_mp_x, is_mp_y;
  
  assert(pmath_is_number(x));
  assert(pmath_is_number(y));
  
  sign_x = pmath_number_sign(x);
  sign_y = pmath_number_sign(y);
  
  if(sign_x != sign_y)
    return FALSE;
    
  if(sign_x == 0)
    return TRUE;
    
  if(pmath_is_double(x) || pmath_is_double(y)) {
    double dx = fabs(pmath_number_get_d(x));
    double dy = fabs(pmath_number_get_d(y));
    
    if(dx < dy)
      return dy <= dx * (1 + TOLERANCE_FACTOR * DBL_EPSILON);
      
    return dx <= dy * (1 + TOLERANCE_FACTOR * DBL_EPSILON);
  }
  
  is_mp_x = pmath_is_mpfloat(x);
  is_mp_y = pmath_is_mpfloat(y);
  if(is_mp_x || is_mp_y) {
    x = pmath_ref(x);
    y = pmath_ref(y);
    
    if(!is_mp_x) {
      x = pmath_set_precision(x, mpfr_get_prec(PMATH_AS_MP_VALUE(y)));
      is_mp_x = pmath_is_mpfloat(x);
    }
    
    if(!is_mp_y) {
      y = pmath_set_precision(y, mpfr_get_prec(PMATH_AS_MP_VALUE(x)));
      is_mp_y = pmath_is_mpfloat(y);
    }
    
    if(is_mp_x && is_mp_y) {
      pmath_bool_t eq = slow_almost_equal_mpf(PMATH_AS_MP_VALUE(x), PMATH_AS_MP_VALUE(y));
      
      pmath_unref(x);
      pmath_unref(y);
      return eq;
    }
    
    if(!is_mp_x)
      pmath_debug_print_object("[pmath_set_precision gave no mp float, but ", x, "]");
    if(!is_mp_y)
      pmath_debug_print_object("[pmath_set_precision gave no mp float, but ", y, "]");
      
    pmath_unref(x);
    pmath_unref(y);
    return FALSE;
  }
  
  return pmath_equals(x, y);
}

// TRUE, FALSE or PMATH_MAYBE_ORDERED
static int test_almost_equal(pmath_t a, pmath_t b) {
  if(pmath_is_number(a) && pmath_is_number(b))
    return slow_almost_equal_numbers(a, b) ? TRUE : FALSE;
    
  if( pmath_is_expr_of_len(a, PMATH_SYMBOL_COMPLEX, 2) ||
      pmath_is_expr_of_len(b, PMATH_SYMBOL_COMPLEX, 2))
  {
    pmath_t re_a, re_b, im_a, im_b;
    
    if( _pmath_re_im(pmath_ref(a), &re_a, &im_a) &&
        _pmath_re_im(pmath_ref(b), &re_b, &im_b) &&
        (!pmath_is_null(im_a) || !pmath_is_null(im_b)))
    {
      int eq_re, eq_im;
      eq_re = test_almost_equal(re_a, re_b);
      pmath_unref(re_a);
      pmath_unref(re_b);
      
      if(eq_re == FALSE) {
        pmath_unref(im_a);
        pmath_unref(im_b);
        return FALSE;
      }
      
      eq_im = test_almost_equal(im_a, im_b);
      pmath_unref(im_a);
      pmath_unref(im_b);
      
      if(eq_im == FALSE)
        return FALSE;
        
      if(eq_im == TRUE && eq_re == TRUE)
        return TRUE;
        
      return PMATH_MAYBE_ORDERED;
    }
    
    pmath_unref(re_a);
    pmath_unref(im_a);
    pmath_unref(re_b);
    pmath_unref(im_b);
  }
  
  return pmath_equals(a, b) ? TRUE : PMATH_MAYBE_ORDERED;
}

static pmath_bool_t almost_equal(pmath_t a, pmath_t b) {
  int test = test_almost_equal(a, b);
  if(test > 0)
    return TRUE;
  return FALSE;
}

static int pmath_fuzzy_compare(pmath_t a, pmath_t b) {
  if(almost_equal(a, b))
    return 0;
    
  return pmath_compare(a, b);
}

static pmath_bool_t check_complex_is_real(pmath_t z, pmath_t *out_re_only) {
  pmath_t im;
  
  assert(_pmath_is_nonreal_complex_number(z));
  
  im = pmath_expr_get_item(z, 2);
  
  assert(pmath_is_number(im));
  if(pmath_number_sign(im) == 0) {
    *out_re_only = pmath_expr_get_item(z, 1);
    pmath_unref(im);
    return TRUE;
  }
  
  pmath_unref(im);
  *out_re_only = PMATH_UNDEFINED;
  return FALSE;
}

enum known_direction_t {
  KNOWN_DIR_LESS,
  KNOWN_DIR_LESSEQUAL,
  KNOWN_DIR_EQUAL,
  KNOWN_DIR_GREATEREQUAL,
  KNOWN_DIR_GREATER,
  KNOWN_DIR_OVERLAP
};

#define MAYBE  PMATH_MAYBE_ORDERED
// first index is KNOWN_DIR_XXX, second index is PMATH_DIRECTION_YYY bitset
static const int8_t known_and_direction_result[6][8] = {
/*         0: !=   1: <   2: =   3: <=   4: >   5: ><   6: >=   7: >=< */
/* <  */ { TRUE,   TRUE,  FALSE, TRUE,   FALSE, TRUE,   FALSE,  TRUE   },
/* <= */ { MAYBE,  MAYBE, MAYBE, TRUE,   FALSE, MAYBE,  MAYBE,  TRUE   },
/*  = */ { FALSE,  FALSE, TRUE,  MAYBE,  FALSE, MAYBE,  MAYBE,  MAYBE  },
/* >= */ { MAYBE,  FALSE, MAYBE, MAYBE,  MAYBE, MAYBE,  TRUE,   TRUE   },
/* >  */ { TRUE,   FALSE, FALSE, FALSE,  TRUE,  TRUE,   TRUE,   TRUE   },
/* ?? */ { MAYBE,  MAYBE, MAYBE, MAYBE,  MAYBE, MAYBE,  MAYBE,  MAYBE  }
};
#undef MAYBE

static int to_comparison_result(enum known_direction_t known, int directions) {
  return known_and_direction_result[known][directions & (PMATH_DIRECTION_LESS | PMATH_DIRECTION_EQUAL | PMATH_DIRECTION_GREATER)];
}

static int8_t flip_direction_result[8] = {
  /* 0: !=  */  0,
  /* 1: <   */  4,
  /* 2: =   */  2,
  /* 3: <=  */  6,
  /* 4: >   */  1,
  /* 5: ><  */  5,
  /* 6: >=  */  3,
  /* 7: >=< */  7
};
static int flip_direction(int direction) {
  return flip_direction_result[direction & (PMATH_DIRECTION_LESS | PMATH_DIRECTION_EQUAL | PMATH_DIRECTION_GREATER)];
}

static enum known_direction_t compare_interval_endpoints(mpfr_srcptr a_left, mpfr_srcptr a_right, mpfr_srcptr b_left, mpfr_srcptr b_right) {
  int cmp_aR_bL;
  int cmp_aL_bR;
  
  if(mpfr_equal_p(a_left, b_left) && mpfr_equal_p(a_right, b_right))
    return KNOWN_DIR_EQUAL;
  
  cmp_aR_bL = mpfr_cmp(a_right, b_left);
  if(cmp_aR_bL < 0)
    return KNOWN_DIR_LESS;
  if(cmp_aR_bL == 0)
    return KNOWN_DIR_LESSEQUAL;
    
  cmp_aL_bR = mpfr_cmp(a_left, b_right);
  if(cmp_aL_bR > 0)
    return KNOWN_DIR_GREATER;
  if(cmp_aL_bR == 0)
    return KNOWN_DIR_GREATEREQUAL;
  
  return KNOWN_DIR_OVERLAP;
}

static enum known_direction_t compare_interval_with_number(pmath_interval_t a, pmath_number_t b) {
  struct _pmath_mp_float_t  a_left_data;
  struct _pmath_mp_float_t  a_right_data;
  int cmp_left;
  int cmp_right;
  pmath_t a_left;
  pmath_t a_right;
  
  assert(pmath_is_number(b));
  
  a_left_data.inherited.type_shift = PMATH_TYPE_SHIFT_MP_FLOAT;
  a_left_data.inherited.refcount._data = 2;
  a_left_data.value_old[0] = PMATH_AS_MP_INTERVAL(a)->left;
  a_left = PMATH_FROM_PTR(&a_left_data);
  
  a_right_data.inherited.type_shift = PMATH_TYPE_SHIFT_MP_FLOAT;
  a_right_data.inherited.refcount._data = 2;
  a_right_data.value_old[0] = PMATH_AS_MP_INTERVAL(a)->right;
  a_right = PMATH_FROM_PTR(&a_right_data);
  
  cmp_left = _pmath_numbers_compare(a_left, b);
  cmp_right = _pmath_numbers_compare(a_right, b);
  
  if(cmp_left == 0 && cmp_right == 0)
    return KNOWN_DIR_EQUAL;
  
  if(cmp_right < 0)
    return KNOWN_DIR_LESS;
  if(cmp_right == 0)
    return KNOWN_DIR_LESSEQUAL;
  
  if(cmp_left > 0)
    return KNOWN_DIR_GREATER;
  if(cmp_left == 0)
    return KNOWN_DIR_GREATEREQUAL;
  
  return KNOWN_DIR_OVERLAP;
}

static int compare_interval(pmath_interval_t a, pmath_t b, int directions) {
  assert(pmath_is_interval(a));
  
  if(pmath_is_interval(b)) {
    mpfr_srcptr a_left = &PMATH_AS_MP_INTERVAL(a)->left;
    mpfr_srcptr a_right = &PMATH_AS_MP_INTERVAL(a)->right;
    mpfr_srcptr b_left = &PMATH_AS_MP_INTERVAL(b)->left;
    mpfr_srcptr b_right = &PMATH_AS_MP_INTERVAL(b)->right;
    enum known_direction_t known = compare_interval_endpoints(a_left, a_right, b_left, b_right);
    return to_comparison_result(known, directions);
  }
  
  if(pmath_is_number(b)) {
    enum known_direction_t known = compare_interval_with_number(a, b);
    return to_comparison_result(known, directions);
  }
  
  return PMATH_MAYBE_ORDERED;
}

// TRUE, FALSE or PMATH_MAYBE_ORDERED or PMATH_UNORDERED
PMATH_PRIVATE
int _pmath_numeric_order(pmath_t prev, pmath_t next, int directions) {
  if(pmath_is_interval(prev)) 
    return compare_interval(prev, next, directions);
  else if(pmath_is_interval(next)) 
    return compare_interval(next, prev, flip_direction(directions));
  
//  if(pmath_is_double(prev) && pmath_is_numeric(next)) {
//    pmath_t n = pmath_set_precision(pmath_ref(next), -HUGE_VAL);
//    
//    if(_pmath_is_nonreal_complex_number(n)) {
//      pmath_t re;
//      if(check_complex_is_real(n, &re)) {
//        pmath_unref(n);
//        n = re;
//      }
//      else if(directions & (PMATH_DIRECTION_LESS | PMATH_DIRECTION_GREATER)) {
//        pmath_message(PMATH_NULL, "nord", 1, n);
//        return PMATH_UNORDERED;
//      }
//    }
//    return compare_double_with_numeric(pmath_ref(prev), n, directions);
//  }
//  
//  if(pmath_is_double(next) && pmath_is_numeric(prev)) {
//    pmath_t p = pmath_set_precision(pmath_ref(prev), -HUGE_VAL);
//    
//    if(_pmath_is_nonreal_complex_number(p)) {
//      pmath_t re;
//      if(check_complex_is_real(p, &re)) {
//        pmath_unref(p);
//        p = re;
//      }
//      else if(directions & (PMATH_DIRECTION_LESS | PMATH_DIRECTION_GREATER)) {
//        pmath_message(PMATH_NULL, "nord", 1, p);
//        return PMATH_UNORDERED;
//      }
//    }
//    return compare_double_with_numeric(p, pmath_ref(next), directions);
//  }
  
  if(pmath_is_number(prev) && pmath_is_number(next)) {
    int c = pmath_fuzzy_compare(prev, next);
    
    if( (c <  0 && (directions & PMATH_DIRECTION_LESS)    == 0) ||
        (c == 0 && (directions & PMATH_DIRECTION_EQUAL)   == 0) ||
        (c >  0 && (directions & PMATH_DIRECTION_GREATER) == 0))
    {
      return FALSE;
    }
    
    return TRUE;
  }
  
  if(_pmath_is_nonreal_complex_number(prev)) {
    pmath_t re;
    if(check_complex_is_real(prev, &re)) {
      int result = _pmath_numeric_order(re, next, directions);
      pmath_unref(re);
      return result;
    }
    else if(directions & (PMATH_DIRECTION_LESS | PMATH_DIRECTION_GREATER)) {
      pmath_message(PMATH_NULL, "nord", 1, pmath_ref(prev));
      return PMATH_UNORDERED;
    }
  }
  else if(directions & (PMATH_DIRECTION_LESS | PMATH_DIRECTION_GREATER)) {
    if(pmath_equals(prev, _pmath_object_complex_infinity)) {
      pmath_message(PMATH_NULL, "nord", 1, pmath_ref(prev));
      return PMATH_UNORDERED;
    }
  }
  
  if(_pmath_is_nonreal_complex_number(next)) {
    pmath_t re;
    if(check_complex_is_real(prev, &re)) {
      int result = _pmath_numeric_order(re, next, directions);
      pmath_unref(re);
      return result;
    }
    else if(directions & (PMATH_DIRECTION_LESS | PMATH_DIRECTION_GREATER)) {
      pmath_message(PMATH_NULL, "nord", 1, pmath_ref(prev));
      return PMATH_UNORDERED;
    }
  }
  else if(directions & (PMATH_DIRECTION_LESS | PMATH_DIRECTION_GREATER)) {
    if(pmath_equals(next, _pmath_object_complex_infinity)) {
      pmath_message(PMATH_NULL, "nord", 1, pmath_ref(next));
      return PMATH_UNORDERED;
    }
  }
  
  if(pmath_equals(prev, next)) { // symbols, expressions
    if(directions & PMATH_DIRECTION_EQUAL)
      return TRUE;
      
    return FALSE;
  }
  
  { // +/- Infinity ...
    pmath_t prev_infdir = _pmath_directed_infinity_direction(prev);
    pmath_t next_infdir = _pmath_directed_infinity_direction(next);
    pmath_t p;
    pmath_t n;
    
    if(!pmath_is_null(prev_infdir) || !pmath_is_null(next_infdir)) {
      if(pmath_equals(prev_infdir, PMATH_FROM_INT32(-1))) {
        n = pmath_ref(next);
        
        if(pmath_is_null(next_infdir) && pmath_is_numeric(n))
          n = pmath_set_precision(n, -HUGE_VAL);
          
        if( pmath_is_number(n) ||
            pmath_equals(next_infdir, PMATH_FROM_INT32(1)))
        {
          pmath_unref(prev_infdir);
          pmath_unref(next_infdir);
          pmath_unref(n);
          
          if(directions & PMATH_DIRECTION_LESS)
            return TRUE;
            
          return FALSE;
        }
        
        pmath_unref(prev_infdir);
        pmath_unref(next_infdir);
        pmath_unref(n);
        return PMATH_MAYBE_ORDERED;
      }
      
      if(pmath_equals(prev_infdir, PMATH_FROM_INT32(1))) {
        n = pmath_ref(next);
        
        if(pmath_is_null(next_infdir) && pmath_is_numeric(n))
          n = pmath_set_precision(n, -HUGE_VAL);
          
        if( pmath_is_number(n) ||
            pmath_equals(next_infdir, PMATH_FROM_INT32(-1)))
        {
          pmath_unref(prev_infdir);
          pmath_unref(next_infdir);
          pmath_unref(n);
          
          if(directions & PMATH_DIRECTION_GREATER)
            return TRUE;
            
          return FALSE;
        }
        
        pmath_unref(prev_infdir);
        pmath_unref(next_infdir);
        pmath_unref(n);
        return PMATH_MAYBE_ORDERED;
      }
      
      if(pmath_equals(next_infdir, PMATH_FROM_INT32(-1))) {
        p = pmath_ref(prev);
        
        if(pmath_is_null(prev_infdir) && pmath_is_numeric(p))
          p = pmath_set_precision(p, -HUGE_VAL);
          
        if( pmath_is_number(p) ||
            pmath_equals(prev_infdir, PMATH_FROM_INT32(1)))
        {
          pmath_unref(prev_infdir);
          pmath_unref(next_infdir);
          pmath_unref(p);
          
          if(directions & PMATH_DIRECTION_GREATER)
            return TRUE;
            
          return FALSE;
        }
        
        pmath_unref(prev_infdir);
        pmath_unref(next_infdir);
        pmath_unref(p);
        return PMATH_MAYBE_ORDERED;
      }
      
      if(pmath_equals(next_infdir, PMATH_FROM_INT32(1))) {
        p = pmath_ref(prev);
        
        if(pmath_is_null(prev_infdir) && pmath_is_numeric(p))
          p = pmath_set_precision(p, -HUGE_VAL);
          
        if( pmath_is_number(p) ||
            pmath_equals(prev_infdir, PMATH_FROM_INT32(-1)))
        {
          pmath_unref(prev_infdir);
          pmath_unref(next_infdir);
          pmath_unref(p);
          
          if(directions & PMATH_DIRECTION_LESS)
            return TRUE;
            
          return FALSE;
        }
        
        pmath_unref(prev_infdir);
        pmath_unref(next_infdir);
        pmath_unref(p);
        return PMATH_MAYBE_ORDERED;
      }
      
      pmath_unref(prev_infdir);
      pmath_unref(next_infdir);
      return PMATH_MAYBE_ORDERED;
    }
  }
  
  if(pmath_is_numeric(prev) && pmath_is_numeric(next)) {
    pmath_thread_t me = pmath_thread_get_current();
    if(me) {
      pmath_t diff;
      double prec, maxprec;
      
      if(pmath_same(next, INT(0)))
        diff = pmath_ref(prev);
      else if(pmath_same(prev, INT(0)))
        diff = NEG(pmath_ref(next));
      else
        diff = MINUS(pmath_ref(prev), pmath_ref(next));
      
      prec = pmath_precision(pmath_ref(diff));
      maxprec = me->max_precision;
      
      if(!isfinite(prec))
        prec = DBL_MANT_DIG;
      else if(prec < DBL_MANT_DIG)
        prec = DBL_MANT_DIG;
      if(prec < me->min_precision)
        prec = me->min_precision;
      
      if(maxprec > prec + me->max_extra_precision)
        maxprec = prec + me->max_extra_precision;
      
      while(!pmath_thread_aborting(me)) {
        pmath_t n_diff = pmath_set_precision_interval(pmath_ref(diff), prec);
        int result;
        
        if(!pmath_is_interval(n_diff)) {
          pmath_unref(n_diff);
          break;
        }
        
        result = compare_interval(n_diff, INT(0), directions);
        pmath_unref(n_diff);
        if(result == TRUE || result == FALSE) {
          pmath_unref(diff);
          return result;
        }
          
        if(prec >= maxprec) {
          pmath_message(
            PMATH_SYMBOL_N, "meprec", 2, 
            _pmath_from_precision(me->max_extra_precision),
            pmath_ref(diff));
          break;
        }
        
        // TODO: adapt precision to interval diameters ...
        prec = 2 * prec;
        if(prec > maxprec)
          prec = maxprec;
      }
      
      pmath_unref(diff);
    }
  }
  
//  if(pmath_is_numeric(prev) && pmath_is_numeric(next)) {
//    double pprec = pmath_precision(pmath_ref(prev));
//    double nprec = pmath_precision(pmath_ref(next));
//    
//    if(pprec < HUGE_VAL && nprec < HUGE_VAL) {
//      pmath_t p = pmath_ref(prev);
//      pmath_t n = pmath_ref(next);
//      
//      if(!pmath_is_number(p))
//        p = pmath_set_precision(p, MIN(pprec, nprec));
//        
//      if(!pmath_is_number(n))
//        n = pmath_set_precision(n, MIN(pprec, nprec));
//        
//      if(pmath_is_number(p) && pmath_is_number(n)) {
//        int c = pmath_fuzzy_compare(p, n);
//        
//        pmath_unref(p);
//        pmath_unref(n);
//        
//        if( (c <  0 && (directions & PMATH_DIRECTION_LESS)    == 0) ||
//            (c == 0 && (directions & PMATH_DIRECTION_EQUAL)   == 0) ||
//            (c >  0 && (directions & PMATH_DIRECTION_GREATER) == 0))
//        {
//          return FALSE;
//        }
//        
//        return TRUE;
//      }
//      
//      pmath_unref(p);
//      pmath_unref(n);
//      
//      return PMATH_MAYBE_ORDERED;
//    }
//    else {
//      double prec, startprec;
//      int c = 0;
//      
//      pmath_thread_t me = pmath_thread_get_current();
//      if(me == NULL)
//        return PMATH_MAYBE_ORDERED;
//        
//      prec = startprec = DBL_MANT_DIG;
//      
//      for(;;) {
//        pmath_t p = pmath_set_precision(pmath_ref(prev), prec);
//        pmath_t n = pmath_set_precision(pmath_ref(next), prec);
//        
//        if(!pmath_is_number(p) || !pmath_is_number(n)) {
//          pmath_unref(p);
//          pmath_unref(n);
//          
//          return PMATH_MAYBE_ORDERED;
//        }
//        
//        c = pmath_fuzzy_compare(p, n);
//        pmath_unref(p);
//        pmath_unref(n);
//        
//        if(c != 0)
//          break;
//          
//        if(pmath_aborting())
//          return PMATH_MAYBE_ORDERED;
//          
//        if(prec >= startprec + me->max_extra_precision) {
//          pmath_t expr = pmath_expr_new_extended(
//                           pmath_current_head(), 2,
//                           pmath_ref(prev),
//                           pmath_ref(next));
//                           
//          pmath_message(PMATH_NULL, "meprec", 2,
//                        pmath_evaluate(pmath_ref(PMATH_SYMBOL_MAXEXTRAPRECISION)),
//                        expr);
//                        
//          return PMATH_MAYBE_ORDERED;
//        }
//        
//        prec *= 1.414;
//      }
//      
//      if( (c <  0 && (directions & PMATH_DIRECTION_LESS)    == 0) ||
//          (c == 0 && (directions & PMATH_DIRECTION_EQUAL)   == 0) || // c==0 should not happen
//          (c >  0 && (directions & PMATH_DIRECTION_GREATER) == 0))
//      {
//        return FALSE;
//      }
//      
//      return TRUE;
//    }
//  }
  
  return PMATH_MAYBE_ORDERED;
}

static pmath_t ordered(
  pmath_expr_t expr,       // will be freed
  int          directions // DIRECTION_XXX bitset
) {
  size_t len = pmath_expr_length(expr);
  
  if(len > 1) {
    pmath_t prev;
    pmath_bool_t prev_was_true = TRUE;
    pmath_bool_t have_marker_after_start = FALSE;
    size_t start, i;
    
    start = 1;
    prev = pmath_expr_get_item(expr, 1);
    if(pmath_same(prev, PMATH_SYMBOL_UNDEFINED)) {
      pmath_unref(expr);
      return prev;
    }
    
    for(i = 2; i <= len; i++) {
      pmath_t next = pmath_expr_get_item(expr, i);
      
      int test = _pmath_numeric_order(prev, next, directions);
      
      if(test == FALSE) {
        pmath_unref(next);
        pmath_unref(prev);
        pmath_unref(expr);
        return pmath_ref(PMATH_SYMBOL_FALSE);
      }
      
      if(test == TRUE) {
        if(start == i - 1) {
          start++;
        }
        else if(prev_was_true) {
          have_marker_after_start = TRUE;
          expr = pmath_expr_set_item(expr, i - 1, PMATH_UNDEFINED);
        }
        
        prev_was_true = TRUE;
      }
      else
        prev_was_true = FALSE;
        
      pmath_unref(prev);
      prev = next;
      if(pmath_same(prev, PMATH_SYMBOL_UNDEFINED)) {
        pmath_unref(expr);
        return prev;
      }
    }
    
    pmath_unref(prev);
    if(start >= len) {
      pmath_unref(expr);
      return pmath_ref(PMATH_SYMBOL_TRUE);
    }
    
    if(prev_was_true) {
      have_marker_after_start = TRUE;
      expr = pmath_expr_set_item(expr, len, PMATH_UNDEFINED);
    }
    
    {
      pmath_t result;
      
      if(start > 1) {
        result = pmath_expr_get_item_range(expr, start, len);
        pmath_unref(expr);
      }
      else
        result = expr;
        
      if(have_marker_after_start)
        return pmath_expr_remove_all(result, PMATH_UNDEFINED);
      return result;
    }
  }
  
  pmath_unref(expr);
  return pmath_ref(PMATH_SYMBOL_TRUE);
}

PMATH_PRIVATE pmath_t builtin_less(pmath_expr_t expr) {
  return ordered(expr, PMATH_DIRECTION_LESS);
}

PMATH_PRIVATE pmath_t builtin_lessequal(pmath_expr_t expr) {
  return ordered(expr, PMATH_DIRECTION_LESS | PMATH_DIRECTION_EQUAL);
}

PMATH_PRIVATE pmath_t builtin_greater(pmath_expr_t expr) {
  return ordered(expr, PMATH_DIRECTION_GREATER);
}

PMATH_PRIVATE pmath_t builtin_greaterequal(pmath_expr_t expr) {
  return ordered(expr, PMATH_DIRECTION_GREATER | PMATH_DIRECTION_EQUAL);
}

PMATH_PRIVATE pmath_t builtin_equal(pmath_expr_t expr) {
  return ordered(expr, PMATH_DIRECTION_EQUAL);
}

PMATH_PRIVATE pmath_t builtin_unequal(pmath_expr_t expr) {
  pmath_bool_t have_marker = FALSE;
  size_t i, len;
  pmath_t a;
  
  len = pmath_expr_length(expr);
  if(len <= 1) {
    pmath_unref(expr);
    return pmath_ref(PMATH_SYMBOL_TRUE);
  }
  
  for(i = 1; i < len; i++) {
    size_t j;
    
    a = pmath_expr_get_item(expr, i);
    if(pmath_same(a, PMATH_SYMBOL_UNDEFINED)) {
      pmath_unref(expr);
      return a;
    }
    
    for(j = i + 1; j <= len; j++) {
      pmath_t b = pmath_expr_get_item(expr, j);
      int unequal = _pmath_numeric_order(a, b, 0);
      if(unequal == FALSE) {
        pmath_unref(a);
        pmath_unref(b);
        pmath_unref(expr);
        return pmath_ref(PMATH_SYMBOL_FALSE);
      }
      if (unequal == TRUE) {
        have_marker = TRUE;
        expr = pmath_expr_set_item(expr, j, PMATH_UNDEFINED);
      }
      pmath_unref(b);
    }
    pmath_unref(a);
  }
  
  a = pmath_expr_get_item(expr, len);
  if(pmath_same(a, PMATH_SYMBOL_UNDEFINED)) {
    pmath_unref(expr);
    return a;
  }
  pmath_unref(a);
  
  if(have_marker)
    return pmath_expr_remove_all(expr, PMATH_UNDEFINED);
    
  return expr;
}

static int relation_direction(pmath_t rel) {
  if(pmath_same(rel, PMATH_SYMBOL_EQUAL))        return PMATH_DIRECTION_EQUAL;
  if(pmath_same(rel, PMATH_SYMBOL_LESS))         return PMATH_DIRECTION_LESS;
  if(pmath_same(rel, PMATH_SYMBOL_LESSEQUAL))    return PMATH_DIRECTION_LESS | PMATH_DIRECTION_EQUAL;
  if(pmath_same(rel, PMATH_SYMBOL_GREATER))      return PMATH_DIRECTION_GREATER;
  if(pmath_same(rel, PMATH_SYMBOL_GREATEREQUAL)) return PMATH_DIRECTION_GREATER | PMATH_DIRECTION_EQUAL;
  return 0;
}

static pmath_t combine_relations(pmath_t rel1, pmath_t rel2) {
  if(pmath_same(rel1, PMATH_SYMBOL_EQUAL))
    return rel2;
    
  if(pmath_same(rel2, PMATH_SYMBOL_EQUAL))
    return rel1;
    
  if( pmath_same(rel1, PMATH_SYMBOL_UNEQUAL) ||
      pmath_same(rel2, PMATH_SYMBOL_UNEQUAL))
  {
    return PMATH_NULL;
  }
  
  if(pmath_same(rel1, PMATH_SYMBOL_LESS)) {
    if( pmath_same(rel2, PMATH_SYMBOL_LESS) ||
        pmath_same(rel2, PMATH_SYMBOL_LESSEQUAL))
    {
      return rel1;
    }
  }
  
  if(pmath_same(rel1, PMATH_SYMBOL_LESSEQUAL)) {
    if( pmath_same(rel2, PMATH_SYMBOL_LESS) ||
        pmath_same(rel2, PMATH_SYMBOL_LESSEQUAL))
    {
      return rel2;
    }
  }
  
  if(pmath_same(rel1, PMATH_SYMBOL_GREATER)) {
    if( pmath_same(rel2, PMATH_SYMBOL_GREATER) ||
        pmath_same(rel2, PMATH_SYMBOL_GREATEREQUAL))
    {
      return rel1;
    }
  }
  
  if(pmath_same(rel1, PMATH_SYMBOL_GREATEREQUAL)) {
    if( pmath_same(rel2, PMATH_SYMBOL_GREATER) ||
        pmath_same(rel2, PMATH_SYMBOL_GREATEREQUAL))
    {
      return rel2;
    }
  }
  
  return PMATH_NULL;
}

PMATH_PRIVATE pmath_t builtin_inequation(pmath_expr_t expr) {
  pmath_bool_t prev_was_true = TRUE;
  pmath_bool_t have_marker = FALSE;
  pmath_t prev;
  pmath_t prev_relation = pmath_ref(PMATH_SYMBOL_EQUAL);
  int direction;
  size_t i, len;
  
  len = pmath_expr_length(expr);
  if((len & 1) != 1)
    return expr;
    
  if(len == 1) {
    pmath_unref(expr);
    return pmath_ref(PMATH_SYMBOL_TRUE);
  }
  if(len == 3) {
    pmath_t result = pmath_expr_new_extended(
                       pmath_expr_get_item(expr, 2), 2,
                       pmath_expr_get_item(expr, 1),
                       pmath_expr_get_item(expr, 3));
    pmath_unref(expr);
    return result;
  }
  
  direction = 0;
  prev = pmath_expr_get_item(expr, 1);
  
  for(i = 2; i <= len; i += 2) {
    pmath_t relation = pmath_expr_get_item(expr, i);
    pmath_t next     = pmath_expr_get_item(expr, i + 1);
    
    pmath_t test = pmath_evaluate(
                     pmath_expr_new_extended(
                       pmath_ref(relation), 2,
                       pmath_ref(prev),
                       pmath_ref(next)));
                       
    if( pmath_same(test, PMATH_SYMBOL_FALSE) ||
        pmath_same(test, PMATH_SYMBOL_UNDEFINED))
    {
      pmath_unref(relation);
      pmath_unref(prev);
      pmath_unref(next);
      pmath_unref(expr);
      return test;
    }
    
    if(pmath_same(test, PMATH_SYMBOL_TRUE)) {
      if(prev_was_true) {
        pmath_t new_relation = combine_relations(prev_relation, relation);
        
        if(!pmath_is_null(new_relation)) {
          have_marker = TRUE;
          expr = pmath_expr_set_item(expr, i - 2, PMATH_UNDEFINED);
          expr = pmath_expr_set_item(expr, i - 1, PMATH_UNDEFINED);
          expr = pmath_expr_set_item(expr, i,   pmath_ref(new_relation));
        }
      }
      
      if(i + 1 == len) {
        have_marker = TRUE;
        expr = pmath_expr_set_item(expr, i,   PMATH_UNDEFINED); // relation
        expr = pmath_expr_set_item(expr, i + 1, PMATH_UNDEFINED); // next
      }
    }
    else {
      int new_direction = relation_direction(relation);
      
      if(i == 2) {
        direction = new_direction;
      }
      else if( direction == 0                               ||
               new_direction == 0                           ||
               ((new_direction & PMATH_DIRECTION_LESS)    != 0 &&
                (direction     & PMATH_DIRECTION_GREATER) != 0)   ||
               ((new_direction & PMATH_DIRECTION_GREATER) != 0 &&
                (direction     & PMATH_DIRECTION_LESS)    != 0))
      {
        pmath_expr_t until_here;
        pmath_t rest;
        
        until_here = pmath_expr_get_item_range(expr, 1, i - 1);
        
        if(have_marker)
          until_here = pmath_expr_remove_all(until_here, PMATH_UNDEFINED);
          
        rest = pmath_expr_get_item_range(expr, i - 1, SIZE_MAX);
        
        pmath_unref(test);
        pmath_unref(relation);
        pmath_unref(prev);
        pmath_unref(next);
        pmath_unref(expr);
        return pmath_expr_new_extended(
                 pmath_ref(PMATH_SYMBOL_AND), 2,
                 until_here,
                 rest);
      }
    }
    
    prev_was_true = pmath_same(test, PMATH_SYMBOL_TRUE);
    pmath_unref(test);
    pmath_unref(prev_relation);
    prev_relation = relation;
    pmath_unref(prev);
    prev = next;
  }
  
  pmath_unref(prev);
  if(have_marker)
    return pmath_expr_remove_all(expr, PMATH_UNDEFINED);
  return expr;
}
