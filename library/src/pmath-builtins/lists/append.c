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
#include <pmath-util/concurrency/threads.h>

#include <pmath-core/objects-inline.h>
#include <pmath-core/strings-private.h>

#include <pmath-builtins/lists-private.h>
#include <pmath-builtins/all-symbols.h>
#include <pmath-builtins/all-symbols-private.h>

PMATH_PRIVATE pmath_t builtin_append(pmath_expr_t expr){
  pmath_t list, elem;
  
  if(pmath_expr_length(expr) != 2){
    pmath_message_argxxx(pmath_expr_length(expr), 2, 2);
    return expr;
  }
  
  list = pmath_expr_get_item(expr, 1);
  if(!pmath_instance_of(list, PMATH_TYPE_EXPRESSION)){
    pmath_unref(list);
    pmath_message(NULL, "nexprat", 2, pmath_integer_new_si(1), pmath_ref(expr));
    return expr;
  }
  
  elem = pmath_expr_get_item(expr, 2);
  pmath_unref(expr);
  return pmath_expr_append(list, 1, elem);
}
