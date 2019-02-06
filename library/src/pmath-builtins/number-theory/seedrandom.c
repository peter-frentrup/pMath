#include <pmath-core/numbers-private.h>

#include <pmath-util/evaluation.h>
#include <pmath-util/messages.h>
#include <pmath-util/option-helpers-private.h>

#include <pmath-builtins/all-symbols-private.h>


extern pmath_symbol_t pmath_System_Method;

PMATH_PRIVATE pmath_t builtin_seedrandom(pmath_expr_t expr) {
  /* SeedRandom(n)
     SeedRandom()
  
     options:
       Method->Automatic  (Default, "MersenneTwister")
   */
  size_t exprlen = pmath_expr_length(expr);
  pmath_t seed;
  pmath_t options;
  pmath_t method;
  
  if(exprlen == 0) {
    seed = pmath_integer_new_sint(rand());
    options = PMATH_UNDEFINED;
  }
  else {
    seed = pmath_expr_get_item(expr, 1);
    if(pmath_is_integer(seed)) {
      options = pmath_options_extract(expr, 1);
    }
    else {
      pmath_unref(seed);
      seed = pmath_integer_new_sint(rand());
      options = pmath_options_extract(expr, 0);
    }
  }
  
  if(pmath_is_null(options))
    goto FAIL_OPTIONS;
    
  method = pmath_evaluate(pmath_option_value(PMATH_NULL, pmath_System_Method, options));
  if(pmath_same(method, PMATH_SYMBOL_DEFAULT)) {
    pmath_atomic_lock(&_pmath_rand_spinlock);
    {
      gmp_randclear(_pmath_randstate);
      gmp_randinit_default(_pmath_randstate);
    }
    pmath_atomic_unlock(&_pmath_rand_spinlock);
  }
  else if(pmath_is_string(method) && pmath_string_equals_latin1(method, "MersenneTwister")) {
    pmath_atomic_lock(&_pmath_rand_spinlock);
    {
      gmp_randclear(_pmath_randstate);
      gmp_randinit_mt(_pmath_randstate);
    }
    pmath_atomic_unlock(&_pmath_rand_spinlock);
  }
  else if(!pmath_same(method, PMATH_SYMBOL_AUTOMATIC)) {
    pmath_message(
      PMATH_NULL, "nogen", 2, 
      pmath_ref(method), 
      pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_LIST), 2,
        pmath_ref(PMATH_SYMBOL_DEFAULT),
        PMATH_C_STRING("MersenneTwister")));
    goto FAIL_METHOD;
  }
  
  if(pmath_is_int32(seed) && PMATH_AS_INT32(seed) < 0)
    seed = _pmath_create_mp_int(PMATH_AS_INT32(seed));
    
  pmath_atomic_lock(&_pmath_rand_spinlock);
  {
    if(pmath_is_int32(seed))
      gmp_randseed_ui(_pmath_randstate, (unsigned)PMATH_AS_INT32(seed));
      
    if(pmath_is_mpint(seed))
      gmp_randseed(_pmath_randstate, PMATH_AS_MPZ(seed));
  }
  pmath_atomic_unlock(&_pmath_rand_spinlock);
  
  pmath_unref(expr);
  expr = PMATH_NULL;
  
FAIL_METHOD:
  pmath_unref(method);
FAIL_OPTIONS:
  pmath_unref(options);
  pmath_unref(seed);
  return expr;
}
