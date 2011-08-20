#include <pmath-core/numbers.h>

#include <pmath-util/concurrency/threads.h>
#include <pmath-util/emit-and-gather.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>


PMATH_PRIVATE pmath_t builtin_nestlist(pmath_expr_t expr) {
  /* NestList(f, x, n)
   */
  pmath_t obj, func;
  size_t n;
  
  if(pmath_expr_length(expr) != 3) {
    pmath_message_argxxx(pmath_expr_length(expr), 3, 3);
    return expr;
  }
  
  obj = pmath_expr_get_item(expr, 3);
  if(!pmath_is_int32(obj) || PMATH_AS_INT32(obj) < 0) {
    pmath_unref(obj);
    pmath_message(PMATH_NULL, "intnm", 2, PMATH_FROM_INT32(3), pmath_ref(expr));
    return expr;
  }
  
  n = (unsigned)PMATH_AS_INT32(obj);
  
  pmath_gather_begin(PMATH_NULL);
  func = pmath_expr_get_item(expr, 1);
  obj  = pmath_expr_get_item(expr, 2);
  pmath_unref(expr);
  while(n-- > 0 && !pmath_aborting()) {
    pmath_emit(pmath_ref(obj), PMATH_NULL);
    obj = pmath_evaluate(pmath_expr_new_extended(pmath_ref(func), 1, obj));
  }
  
  pmath_emit(obj, PMATH_NULL);
  pmath_unref(func);
  return pmath_gather_end();
}
