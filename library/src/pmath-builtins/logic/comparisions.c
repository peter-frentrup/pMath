#include <pmath-builtins/logic-private.h>

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
/*
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
}*/

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

static const int8_t flip_direction_result[8] = {
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

static int pmath_arb_comparator_false(const arb_t x, const arb_t y) {
  return FALSE;
}

static int pmath_arb_comparator_true(const arb_t x, const arb_t y) {
  return TRUE;
}

typedef int (*pmath_arb_comparator_t)(const arb_t, const arb_t);
static const pmath_arb_comparator_t direction_arb_true[8] = {
  /* 0: !=  */ arb_ne,
  /* 1: <   */ arb_lt,
  /* 2: =   */ arb_eq,
  /* 3: <=  */ arb_le,
  /* 4: >   */ arb_gt,
  /* 5: ><  */ arb_ne,
  /* 6: >=  */ arb_ge,
  /* 7: >=< */ pmath_arb_comparator_true
};
static const pmath_arb_comparator_t direction_arb_false[8] = {
  /* 0: !=  */ arb_eq,
  /* 1: <   */ arb_ge,
  /* 2: =   */ arb_ne,
  /* 3: <=  */ arb_gt,
  /* 4: >   */ arb_le,
  /* 5: ><  */ arb_eq,
  /* 6: >=  */ arb_lt,
  /* 7: >=< */ pmath_arb_comparator_false
};

// TRUE, FALSE or PMATH_MAYBE_ORDERED
static int compare_arb_with_arb(const arb_t a, const arb_t b, int directions) {
  directions &= (PMATH_DIRECTION_LESS | PMATH_DIRECTION_EQUAL | PMATH_DIRECTION_GREATER);
  
  if(direction_arb_true[directions](a, b))
    return TRUE;
    
  if(direction_arb_false[directions](a, b))
    return FALSE;
    
  return PMATH_MAYBE_ORDERED;
}

// TRUE, FALSE or PMATH_MAYBE_ORDERED
static int compare_arb(pmath_mpfloat_t a, pmath_t b, int directions) {
  assert(pmath_is_mpfloat(a));
  
  if(pmath_is_mpfloat(b)) {
    return compare_arb_with_arb(PMATH_AS_ARB(a), PMATH_AS_ARB(b), directions);
  }
  else if(pmath_is_quotient(b)) {
    fmpq_t quot;
    arb_t tmp;
    arb_t num;
    int result;
    fmpq_init(quot);
    arb_init(tmp);
    arb_init(num);
    _pmath_rational_get_fmpq(quot, b);
    arb_mul_fmpz(tmp, PMATH_AS_ARB(a), fmpq_denref(quot), ARF_PREC_EXACT);
    arb_set_fmpz(num, fmpq_numref(quot));
    result = compare_arb_with_arb(tmp, num, directions);
    arb_clear(num);
    arb_clear(tmp);
    fmpq_clear(quot);
    return result;
  }
  else if(pmath_is_number(b)) {
    arb_t tmp;
    int result;
    arb_init(tmp);
    _pmath_number_get_arb(tmp, b, PMATH_AS_ARB_WORKING_PREC(a));
    result = compare_arb_with_arb(PMATH_AS_ARB(a), tmp, directions);
    arb_clear(tmp);
    return result;
  }
  
  return PMATH_MAYBE_ORDERED;
}

// TRUE, FALSE or PMATH_MAYBE_ORDERED
static int test_real_numbers_ordering(pmath_t prev, pmath_t next, int directions) {
  if(pmath_is_mpfloat(prev))
    return compare_arb(prev, next, directions);
  if(pmath_is_mpfloat(next))
    return compare_arb(next, prev, flip_direction(directions));
    
  if(pmath_is_number(prev) && pmath_is_number(next)) {
    int c = pmath_compare(prev, next);
    if( (c <  0 && (directions & PMATH_DIRECTION_LESS)    == 0) ||
        (c == 0 && (directions & PMATH_DIRECTION_EQUAL)   == 0) ||
        (c >  0 && (directions & PMATH_DIRECTION_GREATER) == 0))
    {
      return FALSE;
    }
    return TRUE;
  }
  return PMATH_UNORDERED;
}

// TRUE, FALSE or PMATH_MAYBE_ORDERED or PMATH_UNORDERED
PMATH_PRIVATE
int _pmath_numeric_order(pmath_t prev, pmath_t next, int directions) {
  int result = test_real_numbers_ordering(prev, next, directions);
  if(result == TRUE || result == FALSE)
    return result;
    
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
  
  if(!(pmath_is_number(prev) && pmath_is_number(next)) &&
      pmath_is_numeric(prev) &&
      pmath_is_numeric(next) &&
      directions != PMATH_DIRECTION_EQUAL)
  {
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
        pmath_t n_diff = pmath_set_precision(pmath_ref(diff), prec);
        result = test_real_numbers_ordering(prev, next, directions);
        if(result == TRUE || result == FALSE) {
          pmath_unref(n_diff);
          pmath_unref(diff);
          return result;
        }
        if(!pmath_is_mpfloat(n_diff)) {
          pmath_unref(n_diff);
          break;
        }
        pmath_unref(n_diff);
        if(prec >= maxprec) {
          pmath_message(
            PMATH_SYMBOL_N, "meprec", 2,
            _pmath_from_precision(me->max_extra_precision),
            pmath_ref(diff));
          break;
        }
        
        prec = 2 * prec;
        if(prec > maxprec)
          prec = maxprec;
      }
      
      pmath_unref(diff);
    }
  }
  
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
