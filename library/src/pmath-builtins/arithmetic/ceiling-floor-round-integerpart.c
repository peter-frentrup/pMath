#include <pmath-core/numbers-private.h>

#include <pmath-util/approximate.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/arithmetic-private.h>
#include <pmath-builtins/build-expr-private.h>


PMATH_PRIVATE pmath_t builtin_round_functions(pmath_expr_t expr) {
  /* Ceiling(x)           smallest integer greater than or equal to x
     Ceiling(x, a)        smallest multiple of a that is not less than x
     Ceiling(a + b*I)     = Ceil(a) + Ceil(b)*I
  
     Floor(x)             greatest integer less than or equal to x
     Floor(x, a)          greatest multiple of a that is not greater than x
     Floor(a + b*I)       = Floor(a) + Floor(b)*I
  
     Round(x)             integer closest to x, ties to even
     Round(x, a)          nearest multiple of a to x
     Round(a + b*I)       = Round(a) + Round(b)*I
     
     IntegerPart(x)       round to zero
     IntegerPart(a + b*I) = IntegerPart(a) + IntegerPart(b)*I
  
   */
  size_t len = pmath_expr_length(expr);
  pmath_t head, x;
  
  head = pmath_expr_get_item(expr, 0);
  x    = pmath_expr_get_item(expr, 1);
  if(len == 1) {
    if(_pmath_is_nonreal_complex(x)) {
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
      pmath_mpint_t num = pmath_ref(PMATH_QUOT_NUM(x));
      pmath_mpint_t den = pmath_ref(PMATH_QUOT_DEN(x));
      pmath_mpint_t result = _pmath_create_mp_int(0);
      pmath_unref(expr);
      
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
      
      if(pmath_same(head, PMATH_SYMBOL_CEILING)) {
        mpz_cdiv_q(
          PMATH_AS_MPZ(result),
          PMATH_AS_MPZ(num),
          PMATH_AS_MPZ(den));
      }
      else if(pmath_same(head, PMATH_SYMBOL_FLOOR)) {
        mpz_fdiv_q(
          PMATH_AS_MPZ(result),
          PMATH_AS_MPZ(num),
          PMATH_AS_MPZ(den));
      }
      else if(pmath_same(head, PMATH_SYMBOL_INTEGERPART)) {
        mpz_tdiv_q(
          PMATH_AS_MPZ(result),
          PMATH_AS_MPZ(num),
          PMATH_AS_MPZ(den));
      }
      else /*if(pmath_same(head, PMATH_SYMBOL_ROUND)) */{
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
    
    if(pmath_is_double(x)) {
      pmath_mpint_t result = _pmath_create_mp_int(0);
      pmath_unref(expr);
      if(pmath_is_null(result)) {
        pmath_unref(x);
        pmath_unref(head);
        return PMATH_NULL;
      }
      
      if(pmath_same(head, PMATH_SYMBOL_CEILING)) {
        // TODO: prevent overflow
        mpz_set_d(
          PMATH_AS_MPZ(result),
          ceil(PMATH_AS_DOUBLE(x)));
      }
      else if(pmath_same(head, PMATH_SYMBOL_FLOOR)) {
        // TODO: prevent overflow
        mpz_set_d(
          PMATH_AS_MPZ(result),
          floor(PMATH_AS_DOUBLE(x)));
      }
      else if(pmath_same(head, PMATH_SYMBOL_INTEGERPART)) {
        mpz_set_d(
          PMATH_AS_MPZ(result),
          PMATH_AS_DOUBLE(x)); // mpz_set_d truncates x
      }
      else /*if(pmath_same(head, PMATH_SYMBOL_ROUND)) */{
        double f = floor(PMATH_AS_DOUBLE(x));
        
        mpz_set_d(PMATH_AS_MPZ(result), f);
        f = PMATH_AS_DOUBLE(x) - f;
        
        if( f > 0.5 ||
            (f == 0.5 &&
             mpz_odd_p(PMATH_AS_MPZ(result))))
        {
          mpz_add_ui(PMATH_AS_MPZ(result), PMATH_AS_MPZ(result), 1);
        }
      }
      
      pmath_unref(x);
      pmath_unref(head);
      return _pmath_mp_int_normalize(result);
    }
    
    if(pmath_is_mpfloat(x)) {
      pmath_mpint_t result = _pmath_create_mp_int(0);
      pmath_unref(expr);
      if(pmath_is_null(result)) {
        pmath_unref(x);
        pmath_unref(head);
        return PMATH_NULL;
      }
      
      if(pmath_same(head, PMATH_SYMBOL_CEILING)) {
        mpfr_get_z(
          PMATH_AS_MPZ(result),
          PMATH_AS_MP_VALUE(x),
          MPFR_RNDU);
      }
      else if(pmath_same(head, PMATH_SYMBOL_FLOOR)) {
        mpfr_get_z(
          PMATH_AS_MPZ(result),
          PMATH_AS_MP_VALUE(x),
          MPFR_RNDD);
      }
      else if(pmath_same(head, PMATH_SYMBOL_INTEGERPART)) {
        mpfr_get_z(
          PMATH_AS_MPZ(result),
          PMATH_AS_MP_VALUE(x),
          MPFR_RNDZ);
      }
      else /*if(pmath_same(head, PMATH_SYMBOL_ROUND)) */{
        mpfr_get_z(
          PMATH_AS_MPZ(result),
          PMATH_AS_MP_VALUE(x),
          MPFR_RNDN);
      }
      
      pmath_unref(x);
      pmath_unref(head);
      return _pmath_mp_int_normalize(result);
    }
    
    { // TODO: use higher/adapted precision
      pmath_t re, im;
      pmath_unref(head);
      
      //expr = pmath_expr_set_item(expr, 1, PMATH_NULL);
      //x = pmath_approximate(x, HUGE_VAL, 2 * LOG2_10, NULL);
      x = pmath_approximate(x, -HUGE_VAL, NULL);
      
      if(pmath_is_float(x))
        return pmath_expr_set_item(expr, 1, x);
        
      if( _pmath_re_im(x, &re, &im) && // frees x
          pmath_is_float(re) &&
          pmath_is_float(im)) 
      {
        return pmath_expr_set_item(expr, 1, x);
      }
      
      pmath_unref(re);
      pmath_unref(im);
      return expr;
    }
    
    pmath_unref(head);
    pmath_unref(x);
    return expr;
  }
  
  if(pmath_same(head, PMATH_SYMBOL_INTEGERPART)) {
    pmath_unref(head);
    pmath_unref(x);
    pmath_message_argxxx(len, 1, 1);
    return expr;
  }
  
  if(len == 2) {
    pmath_t a, div;
    
    a = pmath_expr_get_item(expr, 2);
    
//    if(_pmath_is_nonreal_complex(x)){
//      pmath_t re = pmath_expr_get_item((pmath_expr_t)x, 1);
//      pmath_t im = pmath_expr_get_item((pmath_expr_t)x, 2);
//      pmath_unref(x);
//      pmath_unref(expr);
//      x = pmath_expr_new_extended(
//        pmath_ref(PMATH_SYMBOL_COMPLEX), 2,
//          pmath_expr_new_extended(pmath_ref(head), 2, re, pmath_ref(a)),
//          pmath_expr_new_extended(pmath_ref(head), 2, im, pmath_ref(a)));
//      pmath_unref(head);
//      pmath_unref(a);
//      return x;
//    }

    div = pmath_evaluate( // div = x/a
            pmath_expr_new_extended(
              pmath_ref(PMATH_SYMBOL_TIMES), 2,
              x,
              pmath_expr_new_extended(
                pmath_ref(PMATH_SYMBOL_POWER), 2,
                pmath_ref(a),
                PMATH_FROM_INT32(-1))));
                
    pmath_unref(expr);
    return pmath_expr_new_extended( // Ceil(x/a)*a  or  Floor(x/a)*a
             pmath_ref(PMATH_SYMBOL_TIMES), 2,
             pmath_expr_new_extended(head, 1, div),
             a);
  }
  
  pmath_unref(head);
  pmath_unref(x);
  pmath_message_argxxx(len, 1, 2);
  return expr;
}
