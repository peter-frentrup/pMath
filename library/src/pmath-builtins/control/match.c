#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <pmath-config.h>
#include <pmath-types.h>
#include <pmath-core/objects.h>
#include <pmath-core/expressions.h>
#include <pmath-core/numbers.h>
#include <pmath-core/strings.h>
#include <pmath-core/symbols.h>

#include <pmath-util/concurrency/atomic.h>
#include <pmath-util/debug.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-core/objects-inline.h>
#include <pmath-core/objects-private.h>
#include <pmath-core/numbers-private.h>

#include <pmath-builtins/lists-private.h>
#include <pmath-builtins/all-symbols.h>
#include <pmath-builtins/all-symbols-private.h>

#include <pmath-language/patterns-private.h>

PMATH_PRIVATE pmath_t builtin_match(pmath_expr_t expr){
  /* Match(obj, pattern)
   */
  pmath_t obj, pat;
  
  if(pmath_expr_length(expr) != 2){
    pmath_message_argxxx(pmath_expr_length(expr), 2, 2);
    return expr;
  }
  
  obj = pmath_expr_get_item(expr, 1);
  pat = pmath_expr_get_item(expr, 2);
  pmath_unref(expr);
  
  if(_pmath_pattern_match(obj, pat, NULL)){
    pmath_unref(obj);
    return pmath_ref(PMATH_SYMBOL_TRUE);
  }

  pmath_unref(obj);
  return pmath_ref(PMATH_SYMBOL_FALSE);
}
