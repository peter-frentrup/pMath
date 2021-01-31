#include <pmath-core/numbers-private.h>

#include <pmath-util/concurrency/threadmsg.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>


extern pmath_symbol_t pmath_System_List;

PMATH_PRIVATE pmath_t builtin_timing(pmath_expr_t expr) {
  pmath_t obj;
  double start, end;
  
  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  start = pmath_tickcount();
  
  obj = pmath_expr_get_item(expr, 1);
  pmath_unref(expr);
  obj = pmath_evaluate(obj);
  
  end = pmath_tickcount();
  
  return pmath_expr_new_extended(
           pmath_ref(pmath_System_List), 2,
           PMATH_FROM_DOUBLE(end - start),
           obj);
}
