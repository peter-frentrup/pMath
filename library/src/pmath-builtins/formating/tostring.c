#include <pmath-core/expressions.h>
#include <pmath-core/strings.h>

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
  
  result = PMATH_NULL;
  
  for(i = 1;i <= len;++i){
    pmath_t obj = pmath_expr_get_item(expr, i);
    pmath_write(obj, 0, (pmath_write_func_t)_pmath_write_to_string, &result);
    pmath_unref(obj);
  }
  pmath_unref(expr);

  return result;
}
