#include <pmath-core/expressions-private.h>
#include <pmath-core/numbers-private.h>

#include <pmath-builtins/build-expr-private.h>

#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>


extern pmath_symbol_t pmath_System_Exp;
extern pmath_symbol_t pmath_System_List;
extern pmath_symbol_t pmath_System_Plus;
extern pmath_symbol_t pmath_System_Power;
extern pmath_symbol_t pmath_System_Times;


struct num_den_t {
  pmath_t num;
  pmath_t den;
};

static pmath_bool_t extract_minus_sign(pmath_t *product) {
  if(pmath_is_expr_of(*product, pmath_System_Times)) {
    pmath_t factor = pmath_expr_get_item(*product, 1);
    if(pmath_is_number(factor) && pmath_number_sign(factor) < 0) {
      factor = pmath_number_neg(factor);
      *product = pmath_expr_set_item(*product, 1, factor);
      return TRUE;
    }
    
    pmath_unref(factor);
    return FALSE;
  }
  
  if(pmath_is_number(*product) && pmath_number_sign(*product) < 0) {
    *product = pmath_number_neg(*product);
    return TRUE;
  }
  
  return FALSE;
}

// obj will be freed
static struct num_den_t split_numerator_denominator(pmath_t obj) {
  if(pmath_is_expr(obj)) {
    pmath_t head = pmath_expr_get_item(obj, 0);
    pmath_unref(head);
    
    size_t len = pmath_expr_length(obj);
    
    if(pmath_same(head, pmath_System_Power) && len == 2) {
      pmath_t exp = pmath_expr_get_item(obj, 2);
      
      if(extract_minus_sign(&exp)) {
        obj = pmath_expr_set_item(obj, 2, exp);
        return (struct num_den_t){ PMATH_FROM_INT32(1), obj };
      }
      
      if(pmath_is_expr_of(exp, pmath_System_Plus)) {
        size_t exp_len = pmath_expr_length(exp);
        pmath_t den_exp = PMATH_UNDEFINED;
        
        size_t first_den = 0;
        for(size_t i = 1; i <= exp_len; ++i) {
          pmath_t summand = pmath_expr_get_item(exp, i);
          
          if(extract_minus_sign(&summand)) {
            exp = pmath_expr_set_item(exp, i, PMATH_UNDEFINED);
            
            if(first_den == 0) {
              first_den = i;
              den_exp = pmath_expr_new(pmath_ref(pmath_System_Plus), exp_len - first_den + 1);
            }
            den_exp = pmath_expr_set_item(den_exp, i - first_den + 1, summand);
          }
          else {
            pmath_unref(summand);
            
            if(first_den > 0)
              den_exp = pmath_expr_set_item(den_exp, i - first_den + 1, PMATH_UNDEFINED);
          }
        }
        
        if(first_den > 0) {
          pmath_t base = pmath_expr_get_item(obj, 1);
          pmath_unref(obj);
          
          exp     = pmath_expr_remove_all(exp,     PMATH_UNDEFINED);
          den_exp = pmath_expr_remove_all(den_exp, PMATH_UNDEFINED);
          
          struct num_den_t res = { POW(pmath_ref(base), exp), POW(pmath_ref(base), den_exp) };
          pmath_unref(base);
          return res;
        }
      }
      
      pmath_unref(exp);
      return (struct num_den_t){ obj, PMATH_FROM_INT32(1) };
    }
    else if(pmath_same(head, pmath_System_Exp) && len == 1) {
      pmath_t exp = pmath_expr_get_item(obj, 1);
      
      if(extract_minus_sign(&exp)) {
        obj = pmath_expr_set_item(obj, 1, exp);
        return (struct num_den_t){ PMATH_FROM_INT32(1), obj };
      }
      
      pmath_unref(exp);
      return (struct num_den_t){ obj, PMATH_FROM_INT32(1) };
    }
    else if(pmath_same(head, pmath_System_Times)) {
      pmath_t ONE = PMATH_FROM_INT32(1); // not reference counted
      
      pmath_t den_prod = ONE;
      
      size_t first_den = 0;
      for(size_t i = 1; i <= len; ++i) {
        struct num_den_t fac_nd = split_numerator_denominator(pmath_expr_get_item(obj, i));
        
        if(pmath_same(fac_nd.den, ONE) && first_den == 0) {
          pmath_unref(fac_nd.num);
        }
        else {
          obj = pmath_expr_set_item(obj, i, fac_nd.num);
          
          if(first_den == 0) {
            first_den = i;
            den_prod = pmath_expr_new(pmath_ref(pmath_System_Times), len - first_den + 1);
          }
          
          den_prod = pmath_expr_set_item(den_prod, i - first_den + 1, fac_nd.den);
        }
      }
      
      return (struct num_den_t){ obj, den_prod };
    }
  }
  else if(pmath_is_quotient(obj)) {
    struct num_den_t res = { pmath_rational_numerator(obj), pmath_rational_denominator(obj) };
    pmath_unref(obj);
    return res;
  }
  
  return (struct num_den_t){ obj, PMATH_FROM_INT32(1) };
}


PMATH_PRIVATE pmath_t builtin_numerator_denominator(pmath_expr_t expr) {
  // NumeratorDenominator(obj) = {Numerator(obj), Denominator(obj)}
  
  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  pmath_t obj = pmath_expr_get_item(expr, 1);
  pmath_unref(expr);
  
  struct num_den_t nd = split_numerator_denominator(obj);
  
  return pmath_expr_new_extended(pmath_ref(pmath_System_List), 2, nd.num, nd.den);
}

PMATH_PRIVATE pmath_t builtin_numerator(pmath_expr_t expr) {
  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  pmath_t obj = pmath_expr_get_item(expr, 1);
  pmath_unref(expr);
  
  struct num_den_t nd = split_numerator_denominator(obj);
  pmath_unref(nd.den);
  return nd.num;
}

PMATH_PRIVATE pmath_t builtin_denominator(pmath_expr_t expr) {
  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  pmath_t obj = pmath_expr_get_item(expr, 1);
  pmath_unref(expr);
  
  struct num_den_t nd = split_numerator_denominator(obj);
  pmath_unref(nd.num);
  return nd.den;
}
