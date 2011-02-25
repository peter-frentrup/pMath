#include <pmath-core/numbers-private.h>

#include <pmath-util/concurrency/threads.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/number-theory-private.h>


PMATH_PRIVATE pmath_t builtin_nextprime(pmath_expr_t expr){
/* NextPrime(p)
 */
  pmath_t n;
  pmath_thread_t thread = pmath_thread_get_current();

  if(pmath_expr_length(expr) != 1){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }

  n = pmath_expr_get_item(expr, 1);

  if(!pmath_is_integer(n)){
    n = pmath_evaluate(
      pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_PLUS), 2,
        pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_CEILING), 1, n),
        pmath_integer_new_si(-1)));
  }

  if(pmath_is_integer(n)){
    struct _pmath_integer_t *result;
    size_t i;

    pmath_unref(expr);
    if(mpz_cmp_ui(((struct _pmath_integer_t*)n)->value, 2) < 0){
      pmath_unref(n);
      return pmath_integer_new_ui(2);
    }

    if(n->refcount == 1)
      result = (struct _pmath_integer_t*)n;
    else{
      result = _pmath_create_integer();
      if(!result){
        pmath_unref(n);
        return NULL;
      }
      mpz_set(result->value, ((struct _pmath_integer_t*)n)->value);
      pmath_unref(n);
    }
    if(mpz_even_p(result->value))
      mpz_add_ui(result->value, result->value, 1);
    else
      mpz_add_ui(result->value, result->value, 2);

    i = 0;
    while(!_pmath_integer_is_prime((pmath_integer_t)result)){
      mpz_add_ui(result->value, result->value, 2);
      if((i & 0xFF) == 0xFF && pmath_thread_aborting(thread))
        break;
    }

    return (pmath_integer_t)result;
  }
  
  pmath_unref(n);
  return expr;
}
