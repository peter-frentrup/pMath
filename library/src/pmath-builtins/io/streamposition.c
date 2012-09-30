#include <pmath-core/numbers.h>

#include <pmath-util/messages.h>
#include <pmath-util/files.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/io-private.h>


PMATH_PRIVATE pmath_t builtin_streamposition(pmath_expr_t expr) {
  pmath_t file;
  int64_t pos;
  
  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  file = pmath_expr_get_item(expr, 1);
  if(!_pmath_file_check(file, 0)) {
    pmath_unref(file);
    return expr;
  }
  
  pmath_unref(expr);
  
  pos = pmath_file_get_position(file);
  pmath_unref(file);
  
  if(pos < 0)
    return pmath_ref(PMATH_SYMBOL_UNDEFINED);
  
  return pmath_integer_new_si64(pos);
}
