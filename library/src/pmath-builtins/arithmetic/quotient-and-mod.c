#include <pmath-core/numbers-private.h>

#include <pmath-util/evaluation.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/number-theory-private.h>

PMATH_PRIVATE pmath_t builtin_quotient(pmath_expr_t expr){
/* Quotient(m, n) = Floor(m/n)

   Quotient(m, n, d) = x   ==>   d <= m-nx < d+n
 */
  pmath_t m, n;
  size_t exprlen;
  
  exprlen = pmath_expr_length(expr);
  if(exprlen < 2 || exprlen > 3){
    pmath_message_argxxx(exprlen, 2, 3);
    return expr;
  }
  
  m = pmath_expr_get_item(expr, 1);
  n = pmath_expr_get_item(expr, 2);
  
  if(pmath_instance_of(n, PMATH_TYPE_NUMBER)
  && pmath_number_sign(n) == 0){
    pmath_message(NULL, "divz", 2, n, pmath_ref(expr));
    pmath_unref(m);
    return expr;
  }
  
  if(!_pmath_is_numeric(m)
  || !_pmath_is_numeric(n)){
    pmath_unref(m);
    pmath_unref(n);
    return expr;
  }
  
  if(exprlen == 3){
    pmath_t d = pmath_expr_get_item(expr, 3);
    
    if(_pmath_is_numeric(d)){
      pmath_unref(m);
      pmath_unref(n);
      pmath_unref(d);
      return expr;
    }
    
    m = pmath_evaluate( // m-d
      pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_PLUS), 2,
        m,
        pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_TIMES), 2,
          pmath_integer_new_si(-1),
          d)));
  }
  
  pmath_unref(expr);
//  if(pmath_instnace_of(m, PMATH_TYPE_INTEGER)
//  && pmath_instnace_of(n, PMATH_TYPE_INTEGER)){
//    struct _pmath_integer_t *result = _pmath_create_integer();
//    
//    if(result){
//      mpz_fdiv_q(
//        result->value,
//        ((struct _pmath_integer_t*)m)->value,
//        ((struct _pmath_integer_t*)n)->value);
//    }
//    
//    pmath_unref(m);
//    pmath_unref(n);
//    return (pmath_t)result;
//  }
  
  return pmath_expr_new_extended( // Floor(m/n)
    pmath_ref(PMATH_SYMBOL_FLOOR), 1,
    pmath_expr_new_extended(
      pmath_ref(PMATH_SYMBOL_TIMES), 2,
      m,
      pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_POWER), 2,
        n,
        pmath_integer_new_si(-1))));
}

PMATH_PRIVATE pmath_t builtin_mod(pmath_expr_t expr){
/* Mod(m, n)

   n * Quotient(m, n, d) + Mod(m, n, d) == m
 */
  pmath_t m, n, md;
  size_t exprlen;
  
  exprlen = pmath_expr_length(expr);
  if(exprlen < 2 || exprlen > 3){
    pmath_message_argxxx(exprlen, 2, 3);
    return expr;
  }
  
  m = pmath_expr_get_item(expr, 1);
  n = pmath_expr_get_item(expr, 2);
  
  if(pmath_instance_of(n, PMATH_TYPE_NUMBER)
  && pmath_number_sign(n) == 0){
    pmath_message(NULL, "divz", 2, n, pmath_ref(expr));
    pmath_unref(m);
    return expr;
  }
  
  if(!_pmath_is_numeric(m)
  || !_pmath_is_numeric(n)){
    pmath_unref(m);
    pmath_unref(n);
    return expr;
  }
  
  if(exprlen == 3){
    pmath_t d = pmath_expr_get_item(expr, 3);
    
    if(_pmath_is_numeric(d)){
      pmath_unref(m);
      pmath_unref(n);
      pmath_unref(d);
      return expr;
    }
    
    md = pmath_evaluate( // m-d
      pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_PLUS), 2,
        pmath_ref(m),
        pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_TIMES), 2,
          pmath_integer_new_si(-1),
          d)));
  }
  else
    md = pmath_ref(m);
  
  pmath_unref(expr);
  // Mod(m,n,d) = m - n * Quotient(m,n,d) = m - n * Floor((m-d)/n) = m - Floor(m-d, n)
  return pmath_expr_new_extended(
    pmath_ref(PMATH_SYMBOL_PLUS), 2,
    m,
    pmath_expr_new_extended(
      pmath_ref(PMATH_SYMBOL_TIMES), 2,
      pmath_integer_new_si(-1),
      pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_FLOOR), 2,
        md,
        n)));
}

/*PMATH_PRIVATE pmath_t builtin_div_or_mod(pmath_expr_t expr){
  pmath_t divident, divisor;
  if(pmath_expr_length(expr) != 2){
    pmath_message_argxxx(pmath_expr_length(expr), 2, 2);
    return expr;
  }

  divident = pmath_expr_get_item(expr, 1);
  divisor = pmath_expr_get_item(expr, 2);

  if(pmath_equals(divisor, PMATH_NUMBER_ZERO)){
    pmath_unref(divident);
    pmath_unref(divisor);
    pmath_message(NULL, "infy", 1, expr);
    return pmath_ref(_pmath_object_complex_infinity);
  }

  if(pmath_instance_of(divident, PMATH_TYPE_INTEGER)
  && pmath_instance_of(divisor,  PMATH_TYPE_INTEGER)){
    pmath_t head = pmath_expr_get_item(expr, 0);
    struct _pmath_integer_t *result = _pmath_create_integer();
    pmath_unref(head);
    pmath_unref(expr);
    if(!result){
      pmath_unref(divident);
      pmath_unref(divisor);
      return NULL;
    }

    if(head == PMATH_SYMBOL_QUOTIENT)
      mpz_fdiv_q(
        result->value,
        ((struct _pmath_integer_t*)divident)->value,
        ((struct _pmath_integer_t*)divisor)->value);
    else
      mpz_fdiv_r(
        result->value,
        ((struct _pmath_integer_t*)divident)->value,
        ((struct _pmath_integer_t*)divisor)->value);

    pmath_unref(divident);
    pmath_unref(divisor);
    return (pmath_integer_t)result;
  }
  pmath_unref(divident);
  pmath_unref(divisor);
  return expr;
}

PMATH_PRIVATE pmath_t builtin_divmod(pmath_expr_t expr){
  pmath_t divident, divisor;
  if(pmath_expr_length(expr) != 2){
    pmath_message_argxxx(pmath_expr_length(expr), 2, 2);
    return expr;
  }

  divident = pmath_expr_get_item(expr, 1);
  divisor = pmath_expr_get_item(expr, 2);

  if(pmath_equals(divisor, PMATH_NUMBER_ZERO)){
    pmath_unref(divident);
    pmath_unref(divisor);
    pmath_message(NULL, "infy", 1, expr);
    return pmath_ref(_pmath_object_complex_infinity);
  }

  if(pmath_instance_of(divident, PMATH_TYPE_INTEGER)
  && pmath_instance_of(divisor,  PMATH_TYPE_INTEGER)){
    pmath_t head = pmath_expr_get_item(expr, 0);
    struct _pmath_integer_t *quot = _pmath_create_integer();
    struct _pmath_integer_t *rem  = _pmath_create_integer();
    pmath_unref(head);
    pmath_unref(expr);
    if(!quot || !rem){
      pmath_unref((pmath_integer_t)quot);
      pmath_unref((pmath_integer_t)rem);
      pmath_unref(divident);
      pmath_unref(divisor);
      return NULL;
    }

    mpz_fdiv_qr(
      quot->value,
      rem->value,
      ((struct _pmath_integer_t*)divident)->value,
      ((struct _pmath_integer_t*)divisor)->value);

    pmath_unref(divident);
    pmath_unref(divisor);
    return pmath_expr_new_extended(
      pmath_ref(PMATH_SYMBOL_LIST), 2,
      (pmath_integer_t)quot,
      (pmath_integer_t)rem);
  }
  pmath_unref(divident);
  pmath_unref(divisor);
  return expr;
}*/
