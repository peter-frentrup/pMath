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

#include <pmath-util/memory.h>
#include <pmath-util/messages.h>

#include <pmath-util/concurrency/atomic.h>

#include <pmath-core/objects-inline.h>

#include <pmath-builtins/all-symbols.h>
#include <pmath-builtins/all-symbols-private.h>

PMATH_PRIVATE pmath_t builtin_memoryusage(pmath_expr_t expr){
  size_t current, max;;

  if(pmath_expr_length(expr) != 0){
    pmath_message_argxxx(pmath_expr_length(expr), 0, 0);
    return expr;
  }

  pmath_mem_usage(&current, &max);
  pmath_unref(expr);

  return pmath_expr_new_extended(
    pmath_ref(PMATH_SYMBOL_LIST), 2,
    pmath_integer_new_size(current),
    pmath_integer_new_size(max));
}
