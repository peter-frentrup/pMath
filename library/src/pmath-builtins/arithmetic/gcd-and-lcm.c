#include <pmath-core/expressions.h>
#include <pmath-core/numbers.h>
#include <pmath-core/symbols.h>

#include <assert.h>
#include <string.h>

#include <pmath-util/messages.h>

#include <pmath-core/numbers-private.h>

#include <pmath-builtins/arithmetic-private.h>
#include <pmath-builtins/all-symbols.h>
#include <pmath-builtins/all-symbols-private.h>

/* {1,2,3,4}, f --> f(f(f(1,2),3),4)
 */
static pmath_t nest_integer(
  pmath_expr_t expr, // will be freed
  void (*mpz_function)(mpz_ptr, mpz_srcptr, mpz_srcptr)
){
  size_t i;
  size_t len = pmath_expr_length(expr);
  pmath_integer_t result;
  if(len == 0)
    return expr;

  for(i = 1;i <= len;i++){
    pmath_t item = pmath_expr_get_item(expr, i);
    if(!pmath_instance_of(item, PMATH_TYPE_INTEGER)){
      if(pmath_equals(item, _pmath_object_overflow)
      || pmath_equals(item, _pmath_object_underflow)){
        pmath_unref(expr);
        return item;
      }
      pmath_unref(item);
      return expr;
    }
    pmath_unref(item);
  }

  result = (pmath_integer_t)pmath_expr_get_item(expr, 1);
  for(i = 2;i <= len;i++){
    pmath_integer_t item;
    struct _pmath_integer_t *tmp = _pmath_create_integer();
    if(!tmp){
      pmath_unref(result);
      pmath_unref(expr);
      return NULL;
    }

    item = (pmath_integer_t)pmath_expr_get_item(expr, i);

    mpz_function(
      tmp->value,
      ((struct _pmath_integer_t*)result)->value,
      ((struct _pmath_integer_t*)item)->value);

    pmath_unref(item);
    pmath_unref(result);
    result = (pmath_integer_t)tmp;
  }
  pmath_unref(expr);
  return result;
}

PMATH_PRIVATE pmath_t builtin_gcd(pmath_expr_t expr){
  size_t len = pmath_expr_length(expr);
  if(len == 0){
    pmath_unref(expr);
    return pmath_integer_new_ui(0);
  }

  return nest_integer(expr, mpz_gcd);
}

PMATH_PRIVATE pmath_t builtin_lcm(pmath_expr_t expr){
  return nest_integer(expr, mpz_lcm);
}
