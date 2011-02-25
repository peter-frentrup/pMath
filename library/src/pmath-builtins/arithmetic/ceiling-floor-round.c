#include <pmath-core/numbers-private.h>

#include <pmath-util/approximate.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/arithmetic-private.h>

PMATH_PRIVATE pmath_t builtin_ceiling_or_floor_or_round(
  pmath_expr_t expr
){
/* Ceiling(x)       smallest integer greater than or equal to x
   Ceiling(x, a)    smallest multiple of a that is not less than x
   Ceiling(a + b*I) = Ceil(a) + Ceil(b)*I
   
   Floor(x)       greatest integer less than or equal to x
   Floor(x, a)    greatest multiple of a that is not greater than x
   Floor(a + b*I) = Floor(a) + Floor(b)*I
   
   Round(x)       integer closest to x
   Round(x, a)    nearest multiple of a to x
   Round(a + b*I) = Round(a) + Round(b)*I

 */
  size_t len = pmath_expr_length(expr);
  pmath_t head, x;
  
  head = pmath_expr_get_item(expr, 0);
  x    = pmath_expr_get_item(expr, 1);
  if(len == 1){
    if(_pmath_is_nonreal_complex(x)){
      pmath_t re = pmath_expr_get_item(x, 1);
      pmath_t im = pmath_expr_get_item(x, 2);
      pmath_unref(x);
      pmath_unref(expr);
      x = pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_COMPLEX), 2,
          pmath_expr_new_extended(pmath_ref(head), 1, re),
          pmath_expr_new_extended(pmath_ref(head), 1, im));
      pmath_unref(head);
      return x;
    }

    if(pmath_is_integer(x)){
      pmath_unref(expr);
      pmath_unref(head);
      return x;
    }
    
    if(pmath_instance_of(x, PMATH_TYPE_QUOTIENT)){
      struct _pmath_integer_t *result = _pmath_create_integer();
      pmath_unref(expr);
      if(!result){
        pmath_unref(x);
        pmath_unref(head);
        return NULL;
      }

      if(head == PMATH_SYMBOL_CEILING){
        mpz_cdiv_q(result->value,
          ((struct _pmath_quotient_t*)x)->numerator->value,
          ((struct _pmath_quotient_t*)x)->denominator->value);
      }
      else if(head == PMATH_SYMBOL_FLOOR){
        mpz_fdiv_q(result->value,
          ((struct _pmath_quotient_t*)x)->numerator->value,
          ((struct _pmath_quotient_t*)x)->denominator->value);
      }
      else{
        pmath_bool_t even;
        int cmp;
        struct _pmath_integer_t *rem = _pmath_create_integer();
        struct _pmath_integer_t *half = _pmath_create_integer();
        
        if(rem && half){
          mpz_fdiv_qr(result->value, rem->value,
            ((struct _pmath_quotient_t*)x)->numerator->value,
            ((struct _pmath_quotient_t*)x)->denominator->value);
          
          even = mpz_even_p(((struct _pmath_quotient_t*)x)->denominator->value);
          
          mpz_fdiv_q_2exp(half->value, 
            ((struct _pmath_quotient_t*)x)->denominator->value,
            1);
          
          cmp = mpz_cmp(rem->value, half->value);
          if(cmp > 0
          || (cmp == 0 && even && mpz_odd_p(result->value))){
            mpz_add_ui(result->value, result->value, 1);
          }
        }
        else{
          pmath_unref((pmath_integer_t)result);
          result = NULL;
        }
        
        pmath_unref((pmath_integer_t)rem);
        pmath_unref((pmath_integer_t)half);
      }
      pmath_unref(x);
      pmath_unref(head);
      return (pmath_integer_t)result;
    }
    
    if(pmath_instance_of(x, PMATH_TYPE_MACHINE_FLOAT)){
      struct _pmath_integer_t *result = _pmath_create_integer();
      pmath_unref(expr);
      if(!result){
        pmath_unref(x);
        pmath_unref(head);
        return NULL;
      }

      if(head == PMATH_SYMBOL_CEILING){
        mpz_set_d(
          result->value,
          ceil(((struct _pmath_machine_float_t*)x)->value));
      }
      else if(head == PMATH_SYMBOL_FLOOR){
        mpz_set_d(
          result->value,
          ((struct _pmath_machine_float_t*)x)->value);
      }
      else{
        double f = floor(((struct _pmath_machine_float_t*)x)->value);
        
        mpz_set_d(result->value, f);
        f = ((struct _pmath_machine_float_t*)x)->value - f;
        
        if(f > 0.5
        || (f == 0.5 && mpz_odd_p(result->value))){
          mpz_add_ui(result->value, result->value, 1);
        }
      }
      
      pmath_unref(x);
      pmath_unref(head);
      return (pmath_t)result;
    }

    if(pmath_instance_of(x, PMATH_TYPE_MP_FLOAT)){
      struct _pmath_integer_t *result = _pmath_create_integer();
      pmath_unref(expr);
      if(!result){
        pmath_unref(x);
        pmath_unref(head);
        return NULL;
      }

      if(head == PMATH_SYMBOL_CEILING){
        mpfr_get_z(
          result->value,
          ((struct _pmath_mp_float_t*)x)->value,
          GMP_RNDU);
      }
      else if(head == PMATH_SYMBOL_FLOOR){
        mpfr_get_z(
          result->value,
          ((struct _pmath_mp_float_t*)x)->value,
          GMP_RNDD);
      }
      else{
        mpfr_get_z(
          result->value,
          ((struct _pmath_mp_float_t*)x)->value,
          GMP_RNDN);
      }
      
      pmath_unref(x);
      pmath_unref(head);
      return (pmath_t)result;
    }
    
    {
      expr = pmath_expr_set_item(expr, 1, NULL);
      x = pmath_approximate(x, HUGE_VAL, 2 * LOG2_10);
      
      if(pmath_instance_of(x, PMATH_TYPE_FLOAT)){
        pmath_unref(head);
        expr = pmath_expr_set_item(expr, 1, x);
        
        return expr;
      }
    }
  }
  else if(len == 2){
    pmath_t a = pmath_expr_get_item(expr, 2);
    pmath_t div;

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
          pmath_integer_new_si(-1))));

    pmath_unref(expr);
    return pmath_expr_new_extended( // Ceil(x/a)*a  or  Floor(x/a)*a
      pmath_ref(PMATH_SYMBOL_TIMES), 2,
      pmath_expr_new_extended(head, 1, div),
      a);
  }
  else{
    pmath_unref(head);
    pmath_unref(x);
    pmath_message_argxxx(len, 1, 2);
    return expr;
  }
  
  pmath_unref(head);
  pmath_unref(x);
  return expr;
}
