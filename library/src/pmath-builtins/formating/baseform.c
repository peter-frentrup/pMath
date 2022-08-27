#include <pmath-core/expressions.h>

#include <pmath-util/messages.h>


extern pmath_symbol_t pmath_System_Automatic;

PMATH_PRIVATE pmath_t builtin_baseform(pmath_expr_t expr) {
  pmath_t base;
  
  if(pmath_expr_length(expr) != 2) {
    pmath_message_argxxx(pmath_expr_length(expr), 2, 2);
    return expr;
  }
  
  base = pmath_expr_get_item(expr, 2);
  if(pmath_same(base, pmath_System_Automatic)) {
    pmath_unref(base);
  }
  else if( !pmath_is_int32(base)    ||
      PMATH_AS_INT32(base) < 2 ||
      PMATH_AS_INT32(base) > 36)
  {
    pmath_message(PMATH_NULL, "basf", 2, base, PMATH_FROM_INT32(36));
    return expr;
  }
  
  return expr;
}
