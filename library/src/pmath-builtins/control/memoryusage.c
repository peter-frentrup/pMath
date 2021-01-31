#include <pmath-core/numbers.h>

#include <pmath-util/memory.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>


extern pmath_symbol_t pmath_System_List;

PMATH_PRIVATE pmath_t builtin_memoryusage(pmath_expr_t expr){
  size_t current, max;;

  if(pmath_expr_length(expr) != 0){
    pmath_message_argxxx(pmath_expr_length(expr), 0, 0);
    return expr;
  }

  pmath_mem_usage(&current, &max);
  pmath_unref(expr);

  return pmath_expr_new_extended(
    pmath_ref(pmath_System_List), 2,
    pmath_integer_new_uiptr(current),
    pmath_integer_new_uiptr(max));
}
