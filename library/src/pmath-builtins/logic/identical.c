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

#include <pmath-util/messages.h>

#include <pmath-util/concurrency/atomic.h>

#include <pmath-core/objects-inline.h>

#include <pmath-builtins/all-symbols.h>
#include <pmath-builtins/all-symbols-private.h>

PMATH_PRIVATE pmath_t builtin_identical(pmath_expr_t expr){
  pmath_t prev;
  size_t i, len;

  len = pmath_expr_length(expr);
  if(len <= 1){
    pmath_unref(expr);
    return pmath_ref(PMATH_SYMBOL_TRUE);
  }

  prev = pmath_expr_get_item(expr, 1);

  for(i = 2;i <= len;i++){
    pmath_t next = pmath_expr_get_item(expr, i);
    if(!pmath_equals(prev, next)){
      pmath_unref(prev);
      pmath_unref(next);
      pmath_unref(expr);
      return pmath_ref(PMATH_SYMBOL_FALSE);
    }

    pmath_unref(prev);
    prev = next;
  }
  pmath_unref(prev);
  pmath_unref(expr);

  return pmath_ref(PMATH_SYMBOL_TRUE);
}

PMATH_PRIVATE pmath_t builtin_unidentical(pmath_expr_t expr){
  size_t len = pmath_expr_length(expr);
  size_t i, j;
  for(i = 1;i < len;i++){
    pmath_t a = pmath_expr_get_item(expr, i);
    for(j = i+1;j <= len;j++){
      pmath_t b = pmath_expr_get_item(expr, j);
      if(pmath_equals(a, b)){
        pmath_unref(a);
        pmath_unref(b);
        pmath_unref(expr);
        return pmath_ref(PMATH_SYMBOL_FALSE);
      }
      pmath_unref(b);
    }
    pmath_unref(a);
  }
  pmath_unref(expr);

  return pmath_ref(PMATH_SYMBOL_TRUE);
}
