#include <pmath-util/approximate.h>

#include <pmath-util/concurrency/threads-private.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/arithmetic-private.h>
#include <pmath-builtins/number-theory-private.h>


#define DIRECTION_LESS      (1<<0)
#define DIRECTION_EQUAL     (1<<1)
#define DIRECTION_GREATER   (1<<2)

#define TOLERANCE_FACTOR 64

static pmath_bool_t almost_equal_machine(double a, double b) {
  if(a == b)
    return TRUE;
    
  if((a < 0) != (b < 0))
    return FALSE;
    
  if(a < 0) a = -a;
  if(b < 0) b = -b;
  
  if(a < b) {
    double tmp = a;
    a = b;
    b = tmp;
  }
  
  return a <= b * (1 + TOLERANCE_FACTOR * DBL_EPSILON);
}

static pmath_bool_t almost_zero_mp(pmath_float_t x) {
  assert(pmath_is_mpfloat(x));
  
  if(mpfr_cmp_abs(PMATH_AS_MP_VALUE(x), PMATH_AS_MP_ERROR(x)) <= 0)
    return TRUE;
  
  return FALSE;
}

static pmath_bool_t almost_equal_mp(
  pmath_float_t a,
  pmath_float_t b
) {
  pmath_number_t test;
  pmath_number_t err;
  
  assert(pmath_is_mpfloat(a));
  assert(pmath_is_mpfloat(b));
  
  if(mpfr_equal_p(PMATH_AS_MP_VALUE(a), PMATH_AS_MP_VALUE(b)))
    return TRUE;
    
  if(mpfr_zero_p(PMATH_AS_MP_VALUE(a)) || mpfr_zero_p(PMATH_AS_MP_VALUE(b)))
    return FALSE;
    
  if(mpfr_cmpabs(PMATH_AS_MP_VALUE(a), PMATH_AS_MP_VALUE(b)) < 0) {
    test = _mul_nn(
             pmath_ref(b),
             _pow_fi(
               pmath_ref(a),
               -1,
               TRUE));
  }
  else {
    test = _mul_nn(
             pmath_ref(a),
             _pow_fi(
               pmath_ref(b),
               -1,
               TRUE));
  }
  
  if(pmath_is_mpfloat(test) && pmath_number_sign(test) < 0) {
    pmath_unref(test);
    return FALSE;
  }
  
  err = pmath_ref(test);
  test = _mul_nn(
           pmath_rational_new(
             PMATH_FROM_INT32(1),
             PMATH_FROM_INT32(TOLERANCE_FACTOR)),
           _add_nn(test, PMATH_FROM_INT32(-1)));
           
  if(pmath_is_mpfloat(err) && pmath_is_mpfloat(test)) {
    // Max(a,b)/Min(a,b) <= 1 + TOLERANCE_FACTOR * epsilon
    if(mpfr_lessequal_p(PMATH_AS_MP_VALUE(test), PMATH_AS_MP_ERROR(err))) {
      pmath_unref(err);
      pmath_unref(test);
      return TRUE;
    }
  }
  
  pmath_unref(err);
  pmath_unref(test);
  return FALSE;
}

#define UNKNOWN (-1)

// TRUE, FALSE or UNKNOWN
static int test_almost_equal(pmath_t a, pmath_t b) {
  if(pmath_is_double(a)) {
    if(pmath_is_double(b))
      return almost_equal_machine(
               PMATH_AS_DOUBLE(a),
               PMATH_AS_DOUBLE(b)) ? TRUE : FALSE;
  }
  else if(pmath_is_mpfloat(a)) {
    if(pmath_is_mpfloat(b))
      return almost_equal_mp(a, b) ? TRUE : FALSE;
    
    if(pmath_same(b, PMATH_FROM_INT32(0)))
      return almost_zero_mp(a) ? TRUE : FALSE;
  }
  else if(pmath_is_mpfloat(b)) {
    if(pmath_same(a, PMATH_FROM_INT32(0)))
      return almost_zero_mp(b) ? TRUE : FALSE;
  }
  
  if(pmath_is_expr_of_len(a, PMATH_SYMBOL_COMPLEX, 2) ||
          pmath_is_expr_of_len(a, PMATH_SYMBOL_COMPLEX, 2))
  {
    pmath_t re_a, im_a, re_b, im_b;
    
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
      
      return UNKNOWN;
    }
    
    pmath_unref(re_a);
    pmath_unref(im_a);
    pmath_unref(re_b);
    pmath_unref(im_b);
  }
  
  return pmath_equals(a, b) ? TRUE : UNKNOWN;
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

// TRUE, FALSE or UNKNOWN
static int ordered_pair(pmath_t prev, pmath_t next, int directions) {
  if(pmath_is_double(prev) && pmath_is_numeric(next)) {
    pmath_t n = pmath_approximate(pmath_ref(next), -HUGE_VAL, -HUGE_VAL, NULL);
    int c;
    
    if(!pmath_is_number(n)) {
      c = test_almost_equal(prev, n);
      pmath_unref(n);
      
      if(c == FALSE) {
        if(directions == DIRECTION_EQUAL)
          return FALSE;
          
        if(directions == 0) // no < or >
          return TRUE;
          
        return UNKNOWN;
      }
      
      if(c == TRUE) {
        if(directions == DIRECTION_EQUAL)
          return TRUE;
          
        if(directions == 0) // no < or >
          return FALSE;
      }
      
      return UNKNOWN;
    }
    
    c = pmath_fuzzy_compare(prev, n);
    pmath_unref(n);
    
    if( (c <  0 && (directions & DIRECTION_LESS)  == 0) ||
        (c == 0 && (directions & DIRECTION_EQUAL) == 0) ||
        (c >  0 && (directions & DIRECTION_GREATER) == 0))
    {
      return FALSE;
    }
    
    return TRUE;
  }
  
  if(pmath_is_double(next) && pmath_is_numeric(prev)) {
    pmath_t p = pmath_approximate(pmath_ref(prev), -HUGE_VAL, -HUGE_VAL, NULL);
    int c;
    
    if(!pmath_is_number(p)) {
      c = test_almost_equal(p, next);
      pmath_unref(p);
      
      if(c == FALSE) {
        if(directions == DIRECTION_EQUAL)
          return FALSE;
          
        if(directions == 0) // no < or >
          return TRUE;
          
        return UNKNOWN;
      }
      
      if(c == TRUE) {
        if(directions == DIRECTION_EQUAL)
          return TRUE;
          
        if(directions == 0) // no < or >
          return FALSE;
      }
      
      return UNKNOWN;
    }
    
    c = pmath_fuzzy_compare(p, next);
    pmath_unref(p);
    
    if( (c <  0 && (directions & DIRECTION_LESS)  == 0) ||
        (c == 0 && (directions & DIRECTION_EQUAL) == 0) ||
        (c >  0 && (directions & DIRECTION_GREATER) == 0))
    {
      return FALSE;
    }
    
    return TRUE;
  }
  
  if(pmath_is_number(prev) && pmath_is_number(next)) {
    int c = pmath_fuzzy_compare(prev, next);
    
    if( (c <  0 && (directions & DIRECTION_LESS)    == 0) ||
        (c == 0 && (directions & DIRECTION_EQUAL)   == 0) ||
        (c >  0 && (directions & DIRECTION_GREATER) == 0))
    {
      return FALSE;
    }
    
    return TRUE;
  }
  
  /*if(pmath_is_string(prev) && pmath_is_string(next)) {
    pmath_bool_t equal = pmath_equals(prev, next);
  
    if( ( equal && (directions & DIRECTION_EQUAL) == 0) ||
        (!equal && (directions & (DIRECTION_LESS |
                                  DIRECTION_EQUAL |
                                  DIRECTION_GREATER)) == DIRECTION_EQUAL))
    {
      pmath_unref(prev);
      pmath_unref(next);
      pmath_unref(expr);
      return pmath_ref(PMATH_SYMBOL_FALSE);
    }
  
    continue;
  }*/
  
  if(pmath_equals(prev, next)) { // symbols, expressions
    if(directions & DIRECTION_EQUAL)
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
          n = pmath_approximate(n, -HUGE_VAL, -HUGE_VAL, NULL);
          
        if( pmath_is_number(n) ||
            pmath_equals(next_infdir, PMATH_FROM_INT32(1)))
        {
          pmath_unref(prev_infdir);
          pmath_unref(next_infdir);
          pmath_unref(n);
          
          if(directions & DIRECTION_LESS)
            return TRUE;
            
          return FALSE;
        }
        
        pmath_unref(prev_infdir);
        pmath_unref(next_infdir);
        pmath_unref(n);
        return UNKNOWN;
      }
      
      if(pmath_equals(prev_infdir, PMATH_FROM_INT32(1))) {
        n = pmath_ref(next);
        
        if(pmath_is_null(next_infdir) && pmath_is_numeric(n))
          n = pmath_approximate(n, -HUGE_VAL, -HUGE_VAL, NULL);
          
        if( pmath_is_number(n) ||
            pmath_equals(next_infdir, PMATH_FROM_INT32(-1)))
        {
          pmath_unref(prev_infdir);
          pmath_unref(next_infdir);
          pmath_unref(n);
          
          if(directions & DIRECTION_GREATER)
            return TRUE;
            
          return FALSE;
        }
        
        pmath_unref(prev_infdir);
        pmath_unref(next_infdir);
        pmath_unref(n);
        return UNKNOWN;
      }
      
      if(pmath_equals(next_infdir, PMATH_FROM_INT32(-1))) {
        p = pmath_ref(prev);
        
        if(pmath_is_null(prev_infdir) && pmath_is_numeric(p))
          p = pmath_approximate(p, -HUGE_VAL, -HUGE_VAL, NULL);
          
        if( pmath_is_number(p) ||
            pmath_equals(prev_infdir, PMATH_FROM_INT32(1)))
        {
          pmath_unref(prev_infdir);
          pmath_unref(next_infdir);
          pmath_unref(p);
          
          if(directions & DIRECTION_GREATER)
            return TRUE;
            
          return FALSE;
        }
        
        pmath_unref(prev_infdir);
        pmath_unref(next_infdir);
        pmath_unref(p);
        return UNKNOWN;
      }
      
      if(pmath_equals(next_infdir, PMATH_FROM_INT32(1))) {
        p = pmath_ref(prev);
        
        if(pmath_is_null(prev_infdir) && pmath_is_numeric(p))
          p = pmath_approximate(p, -HUGE_VAL, -HUGE_VAL, NULL);
          
        if( pmath_is_number(p) ||
            pmath_equals(prev_infdir, PMATH_FROM_INT32(-1)))
        {
          pmath_unref(prev_infdir);
          pmath_unref(next_infdir);
          pmath_unref(p);
          
          if(directions & DIRECTION_LESS)
            return TRUE;
            
          return FALSE;
        }
        
        pmath_unref(prev_infdir);
        pmath_unref(next_infdir);
        pmath_unref(p);
        return UNKNOWN;
      }
      
      pmath_unref(prev_infdir);
      pmath_unref(next_infdir);
      return UNKNOWN;
    }
  }
  
  if(pmath_is_numeric(prev) && pmath_is_numeric(next)) {
    int c = 0;
    
    if(pmath_is_mpfloat(prev)) {
      double precacc = DBL_MANT_DIG + LOG10_2;
      pmath_t n;
      
      n = pmath_approximate(pmath_ref(next), precacc, HUGE_VAL, NULL);
      
      if(!pmath_is_number(n)) {
        pmath_unref(n);
        return UNKNOWN;
      }
      
      c = pmath_fuzzy_compare(prev, n);
      if(c == 0) {
        precacc = DBL_MANT_DIG + pmath_accuracy(n);
        n = pmath_approximate(pmath_ref(next), HUGE_VAL, precacc, NULL);
        
        if(!pmath_is_number(n)) {
          pmath_unref(n);
          return UNKNOWN;
        }
        
        c = pmath_fuzzy_compare(prev, n);
      }
      
      pmath_unref(n);
    }
    else if(pmath_is_mpfloat(next)) {
      double precacc = DBL_MANT_DIG + LOG10_2;
      pmath_t p;
      
      p = pmath_approximate(pmath_ref(prev), precacc, HUGE_VAL, NULL);
      
      if(!pmath_is_number(p)) {
        pmath_unref(p);
        return UNKNOWN;
      }
      
      c = pmath_fuzzy_compare(p, next);
      if(c == 0) {
        precacc = DBL_MANT_DIG + pmath_accuracy(p);
        p = pmath_approximate(pmath_ref(prev), HUGE_VAL, precacc, NULL);
        
        if(!pmath_is_number(p)) {
          pmath_unref(p);
          return UNKNOWN;
        }
        
        c = pmath_fuzzy_compare(p, next);
      }
      
      pmath_unref(p);
    }
    else {
      pmath_bool_t error1;
      pmath_bool_t error2;
      double pprec = DBL_MANT_DIG + LOG10_2;
      double nprec = DBL_MANT_DIG + LOG10_2;
      pmath_t p;
      pmath_t n;
      
      pmath_thread_t me = pmath_thread_get_current();
      
      if(!me)
        return UNKNOWN;
        
      p = pmath_approximate(pmath_ref(prev), pprec, HUGE_VAL, &error1);
      n = pmath_approximate(pmath_ref(next), nprec, HUGE_VAL, &error2);
      
      // TODO: check complex values for equality...
      if(!pmath_is_number(p) || !pmath_is_number(n)) {
        c = test_almost_equal(p, n);
        pmath_unref(p);
        pmath_unref(n);
        
        if(c == FALSE) {
          if(directions == DIRECTION_EQUAL)
            return FALSE;
            
          if(directions == 0) // no < or >
            return TRUE;
            
          return UNKNOWN;
        }
        
        if(c == TRUE) {
          if(directions == DIRECTION_EQUAL)
            return TRUE;
            
          if(directions == 0) // no < or >
            return FALSE;
        }
        
        return UNKNOWN;
      }
      
      c = pmath_fuzzy_compare(p, n);
      if(c == 0 && !pmath_aborting()) {
        nprec += 1;
        while(c == 0 && !pmath_aborting() && !error1 && !error2) {
          error1 = (pprec >= DBL_MANT_DIG + me->max_extra_precision);
          error2 = (nprec >= DBL_MANT_DIG + me->max_extra_precision);
          
          if(error1 || error2)
            break;
            
          pprec = 2 * pprec + 33;
          nprec = 2 * nprec + 33;
          
          if(pprec > DBL_MANT_DIG + me->max_extra_precision)
            pprec  = DBL_MANT_DIG + me->max_extra_precision;
          if(nprec > DBL_MANT_DIG + me->max_extra_precision)
            nprec  = DBL_MANT_DIG + me->max_extra_precision;
            
          pmath_unref(n);
          pmath_unref(p);
          
          p = pmath_approximate(pmath_ref(prev), pprec, HUGE_VAL, &error1);
          n = pmath_approximate(pmath_ref(next), nprec, HUGE_VAL, &error2);
          
          if(!pmath_is_number(p) || !pmath_is_number(n))
            break;
            
          c = pmath_fuzzy_compare(p, n);
        }
      }
      
      pmath_unref(n);
      pmath_unref(p);
      
      if(pmath_aborting())
        return UNKNOWN;
        
      if((directions & ~DIRECTION_EQUAL) == 0) {
        if(error1) {
          pmath_message(PMATH_SYMBOL_N, "meprec", 2,
                        pmath_evaluate(pmath_ref(PMATH_SYMBOL_MAXEXTRAPRECISION)),
                        pmath_ref(prev));
                        
          return UNKNOWN;
        }
        
        if(error2) {
          pmath_message(PMATH_SYMBOL_N, "meprec", 2,
                        pmath_evaluate(pmath_ref(PMATH_SYMBOL_MAXEXTRAPRECISION)),
                        pmath_ref(next));
                        
          return UNKNOWN;
        }
      }
    }
    
    if( (c <  0 && (directions & DIRECTION_LESS)    == 0) ||
        (c == 0 && (directions & DIRECTION_EQUAL)   == 0) ||
        (c >  0 && (directions & DIRECTION_GREATER) == 0))
    {
      return FALSE;
    }
    
    return TRUE;
  }
  
  return UNKNOWN;
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
      
      int test = ordered_pair(prev, next, directions);
      
      if(test == 0) {
        pmath_unref(next);
        pmath_unref(prev);
        pmath_unref(expr);
        return pmath_ref(PMATH_SYMBOL_FALSE);
      }
      
      if(test > 0) {
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
  return ordered(expr, DIRECTION_LESS);
}

PMATH_PRIVATE pmath_t builtin_lessequal(pmath_expr_t expr) {
  return ordered(expr, DIRECTION_LESS | DIRECTION_EQUAL);
}

PMATH_PRIVATE pmath_t builtin_greater(pmath_expr_t expr) {
  return ordered(expr, DIRECTION_GREATER);
}

PMATH_PRIVATE pmath_t builtin_greaterequal(pmath_expr_t expr) {
  return ordered(expr, DIRECTION_GREATER | DIRECTION_EQUAL);
}

PMATH_PRIVATE pmath_t builtin_equal(pmath_expr_t expr) {
  return ordered(expr, DIRECTION_EQUAL);
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
      if(pmath_equals(a, b)) {
        pmath_unref(a);
        pmath_unref(b);
        pmath_unref(expr);
        return pmath_ref(PMATH_SYMBOL_FALSE);
      }
      else if(!pmath_is_symbol(a) && !pmath_is_expr(a)) {
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
  if(pmath_same(rel, PMATH_SYMBOL_EQUAL))        return DIRECTION_EQUAL;
  if(pmath_same(rel, PMATH_SYMBOL_LESS))         return DIRECTION_LESS;
  if(pmath_same(rel, PMATH_SYMBOL_LESSEQUAL))    return DIRECTION_LESS | DIRECTION_EQUAL;
  if(pmath_same(rel, PMATH_SYMBOL_GREATER))      return DIRECTION_GREATER;
  if(pmath_same(rel, PMATH_SYMBOL_GREATEREQUAL)) return DIRECTION_GREATER | DIRECTION_EQUAL;
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
               ((new_direction & DIRECTION_LESS)    != 0 &&
                (direction     & DIRECTION_GREATER) != 0)   ||
               ((new_direction & DIRECTION_GREATER) != 0 &&
                (direction     & DIRECTION_LESS)    != 0))
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
