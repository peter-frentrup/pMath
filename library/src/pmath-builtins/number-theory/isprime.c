#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <pmath-config.h>
#include <pmath-types.h>
#include <pmath-core/objects.h>
#include <pmath-core/expressions.h>
#include <pmath-core/numbers.h>
#include <pmath-core/strings.h>
#include <pmath-core/symbols.h>

#include <pmath-util/debug.h>
#include <pmath-util/messages.h>

#include <pmath-util/concurrency/atomic.h>

#include <pmath-core/objects-inline.h>
#include <pmath-core/numbers-private.h>

#include <pmath-builtins/arithmetic-private.h>
#include <pmath-builtins/number-theory-private.h>
#include <pmath-builtins/all-symbols.h>
#include <pmath-builtins/all-symbols-private.h>

// product of all primes up to 97
#define PRODUCT_OF_SMALL_PRIMES_HEX \
  "1bc0946e5bb173bc25c4b8131ab1026"

// product of all primes between 101 and 997
#define PRODUCT_OF_MEDIUM_PRIMES_HEX \
  "6d931243d1f1eddad141d5ea95372e2024b39d47b1f033537098e40ae50a6a4cbe3e9" \
  "23fb64071c246d8afa57ca7122b3eb2e1be9ad97195ac0bc81993980b3e797948d91f" \
  "01716030f697cafe52f82567a833cb2b526d3bb412cd716f74cfccb11c391c2fa23a2" \
  "84f236ead6e8a19468a869de6b58a6f726320fdf00e897743054406eb9c56dfb00c7e" \
  "0a2018b90b5654670bb23bcc7aa10ec4e451ee9"

#define GREATEST_SMALL_PRIME  97

#define FIRST_MEDIUM_PRIME_POS  25
#define FIRST_BIG_PRIME_POS     168

// prime(FIRST_MEDIUM_PRIME_POS)^2  =  101^2
#define SQUARE_OF_FIRST_MEDIUM_PRIME   10201

// prime(FIRST_BIG_PRIME_POS)^2  =  1009^2
#define SQUARE_OF_FIRST_BIG_PRIME   1018081

static pmath_integer_t product_of_small_primes;
static pmath_integer_t product_of_medium_primes;

static pmath_bool_t is_small_prime(struct _pmath_integer_t *n){
  size_t i;
  for(i = 0;i < FIRST_MEDIUM_PRIME_POS; ++i){
    int cmp = mpz_cmp_ui(n->value, _pmath_primes16bit[i]);
    if(cmp == 0)
      return TRUE;
    if(cmp < 0)
      return FALSE;
  }
  return FALSE;
}

PMATH_PRIVATE pmath_bool_t _pmath_integer_is_prime(pmath_integer_t n){
  struct _pmath_integer_t *tmp;

  assert(n != NULL);

  if(mpz_cmp_ui(((struct _pmath_integer_t*)n)->value, 1) <= 0)
    return FALSE;

  if(mpz_cmp_ui(((struct _pmath_integer_t*)n)->value, GREATEST_SMALL_PRIME) <= 0)
    return is_small_prime((struct _pmath_integer_t*)n);

  tmp = _pmath_create_integer();
  if(!tmp)
    return TRUE; /* Not realy prime, but pmath_aborting() is TRUE, so we can
                    return whatever we want. */

  mpz_gcd(
    tmp->value,
    ((struct _pmath_integer_t*)n)->value,
    ((struct _pmath_integer_t*)product_of_small_primes)->value);

  if(mpz_cmp_ui(tmp->value, 1) != 0){
    pmath_unref((pmath_integer_t)tmp);
    return FALSE;
  }

  if(mpz_cmp_ui(
      ((struct _pmath_integer_t*)n)->value, 
      SQUARE_OF_FIRST_MEDIUM_PRIME) < 0)
  {
    pmath_unref((pmath_integer_t)tmp);
    return TRUE;
  }

  mpz_gcd(
    tmp->value,
    ((struct _pmath_integer_t*)n)->value,
    ((struct _pmath_integer_t*)product_of_medium_primes)->value);

  if(mpz_cmp_ui(tmp->value, 1) != 0){
    pmath_unref((pmath_integer_t)tmp);
    return FALSE;
  }

  if(mpz_cmp_ui(
      ((struct _pmath_integer_t*)n)->value, 
      SQUARE_OF_FIRST_BIG_PRIME) < 0)
  {
    pmath_unref((pmath_integer_t)tmp);
    return TRUE;
  }

  pmath_unref((pmath_integer_t)tmp);
  return mpz_probab_prime_p(((struct _pmath_integer_t*)n)->value, 5);
}

PMATH_PRIVATE pmath_t builtin_isprime(pmath_expr_t expr){
  pmath_t n;

  if(pmath_expr_length(expr) != 1){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }

  n = pmath_expr_get_item(expr, 1);

  if(pmath_instance_of(n, PMATH_TYPE_INTEGER)){
    pmath_bool_t result = _pmath_integer_is_prime((pmath_integer_t)n);
    pmath_unref(expr);
    pmath_unref(n);
    return pmath_ref(result ? PMATH_SYMBOL_TRUE : PMATH_SYMBOL_FALSE);
  }

  if(pmath_instance_of(n, PMATH_TYPE_NUMBER)
  || _pmath_is_nonreal_complex(n)
  || n == PMATH_SYMBOL_PI
  || n == PMATH_SYMBOL_E){
    pmath_unref(n);
    pmath_unref(expr);
    return pmath_ref(PMATH_SYMBOL_FALSE);
  }

  pmath_unref(n);
  return expr;
}

/*============================================================================*/

#if defined(PMATH_DEBUG_LOG) && defined(PMATH_DEBUG_TESTS)
  static pmath_bool_t ui_is_prime(unsigned int n){
    static unsigned int primes55[] = {
      2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53, 59, 61, 
      67, 71, 73, 79, 83, 89, 97, 101, 103, 107, 109, 113, 127, 131, 137, 
      139, 149, 151, 157, 163, 167, 173, 179, 181, 191, 193, 197, 199, 211, 
      223, 227, 229, 233, 239, 241, 251, 257};
    
    unsigned int i, max;
    
    for(i = 0;i < 55;++i){
      if(n % primes55[i] == 0){
        if(n == primes55[i])
          return TRUE;
        
        return FALSE;
      }
    }
    
    max = n >> 4;
    for(i = 259;i < max;i+= 2){
      if(n % i == 0)
        return FALSE;
    }
    
    return TRUE;
  }
  
  /* verify that _pmath_primes16bit contains all the 6542 primes less than 2^16
   */
  void VERIFY_PRIMES16BIT_SANE(){
    int i;
    
    assert(_pmath_primes16bit_count == 6542);
    
    if(!ui_is_prime(_pmath_primes16bit[0])){
      pmath_debug_print("_pmath_primes16bit[0] = %d is not prime\n", 
        _pmath_primes16bit[0]);
      abort();
    }
    
    for(i = 1;i < _pmath_primes16bit_count;++i){
      if(!ui_is_prime(_pmath_primes16bit[i])){
        pmath_debug_print("_pmath_primes16bit[%d] = %d is not prime\n", 
          i, _pmath_primes16bit[i]);
        abort();
      }
    
      if(_pmath_primes16bit[i - 1] >= _pmath_primes16bit[i]){
        pmath_debug_print("prime at %d (=%d) is too small\n", 
          i, _pmath_primes16bit[i]);
        abort();
      }
    }
  }
  
  /* verify that product_of_small_primes is the product of all small primes
     (up to GREATEST_SMALL_PRIME)
     
     returns TRUE on success, FALSE on out-of-memory and does not return on 
     falsification
   */
  pmath_bool_t VERIFY_SMALL_PRIMES(){
    int i;
    struct _pmath_integer_t *tmp = _pmath_create_integer();
    
    if(!tmp)
      return FALSE;
    
    assert(_pmath_primes16bit[FIRST_MEDIUM_PRIME_POS - 1] == GREATEST_SMALL_PRIME);
    
    mpz_set_ui(tmp->value, 1);
    for(i = 0;i < FIRST_MEDIUM_PRIME_POS;++i){
      mpz_mul_ui(tmp->value, tmp->value, _pmath_primes16bit[i]);
    }
    
    if(!pmath_equals((pmath_integer_t)tmp, product_of_small_primes)){
      pmath_debug_print(
        "product_of_small_primes is not the product of all primes up to %d\n", 
        GREATEST_SMALL_PRIME);
      abort();
    }
    
    pmath_unref((pmath_integer_t)tmp);
    return TRUE;
  }
  
  /* verify that product_of_medium_primes is the product of all medium primes
     (primes with index FIRST_MEDIUM_PRIME_POS .. FIRST_BIG_PRIME_POS - 1,
     stating with 2 at index 0)
     
     returns TRUE on success, FALSE on out-of-memory and does not return on 
     falsification
   */
  pmath_bool_t VERIFY_MEDIUM_PRIMES(){
    int i;
    struct _pmath_integer_t *tmp = _pmath_create_integer();
    
    if(!tmp)
      return FALSE;
    
    assert(SQUARE_OF_FIRST_BIG_PRIME == _pmath_primes16bit[FIRST_BIG_PRIME_POS] * _pmath_primes16bit[FIRST_BIG_PRIME_POS]);
    
    mpz_set_ui(tmp->value, 1);
    for(i = FIRST_MEDIUM_PRIME_POS;i < FIRST_BIG_PRIME_POS;++i){
      mpz_mul_ui(tmp->value, tmp->value, _pmath_primes16bit[i]);
    }
    
    if(!pmath_equals((pmath_integer_t)tmp, product_of_medium_primes)){
      pmath_debug_print(
        "product_of_medium_primes is not the product of all primes from %d to %d\n", 
        _pmath_primes16bit[FIRST_MEDIUM_PRIME_POS],
        _pmath_primes16bit[FIRST_BIG_PRIME_POS]);
      abort();
    }
    
    pmath_unref((pmath_integer_t)tmp);
    return TRUE;
  }
#else
  #define VERIFY_PRIMES16BIT_SANE()  ((void)0)
  #define VERIFY_SMALL_PRIMES()      TRUE
  #define VERIFY_MEDIUM_PRIMES()     TRUE
#endif

PMATH_PRIVATE pmath_bool_t _pmath_primetest_init(void){
  product_of_small_primes  = pmath_integer_new_str(PRODUCT_OF_SMALL_PRIMES_HEX,  16);
  product_of_medium_primes = pmath_integer_new_str(PRODUCT_OF_MEDIUM_PRIMES_HEX, 16);
  
  if(!product_of_small_primes || !product_of_medium_primes){
    pmath_unref(product_of_small_primes);
    pmath_unref(product_of_medium_primes);
    return FALSE;
  }

  VERIFY_PRIMES16BIT_SANE();
  
  if(!VERIFY_SMALL_PRIMES()
  || !VERIFY_MEDIUM_PRIMES()){
    pmath_unref(product_of_small_primes);
    pmath_unref(product_of_medium_primes);
    return FALSE;
  }
  
  return TRUE;
}

PMATH_PRIVATE void _pmath_primetest_done(void){
  pmath_unref(product_of_small_primes);
  pmath_unref(product_of_medium_primes);
}
