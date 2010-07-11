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

#include <pmath-util/concurrency/atomic.h>
#include <pmath-util/memory.h>
#include <pmath-util/messages.h>

#include <pmath-core/objects-inline.h>

#include <pmath-builtins/all-symbols.h>
#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/formating-private.h>

PMATH_PRIVATE
void _pmath_write_to_string(
  pmath_string_t *result, 
  const uint16_t *data, 
  int             len
){
  *result = pmath_string_insert_ucs2(
    *result, pmath_string_length(*result), data, len);
}

PMATH_PRIVATE pmath_t builtin_tostring(pmath_expr_t expr){
/* ToString(object)
 */
  pmath_string_t result;
  size_t i, len;
  
  len = pmath_expr_length(expr);
  if(len == 0){
    pmath_unref(expr);
    return pmath_string_new(0);
  }
  
  result = NULL;
  
  for(i = 1;i <= len;++i){
    pmath_t obj = pmath_expr_get_item(expr, i);
    pmath_write(obj, 0, (pmath_write_func_t)_pmath_write_to_string, &result);
    pmath_unref(obj);
  }
  pmath_unref(expr);

  return result;
}
