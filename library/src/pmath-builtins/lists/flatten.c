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

#include <pmath-core/objects-inline.h>

#include <pmath-builtins/lists-private.h>
#include <pmath-builtins/all-symbols.h>
#include <pmath-builtins/all-symbols-private.h>

PMATH_PRIVATE pmath_t builtin_flatten(pmath_expr_t expr){
  /* Flatten(list)
     Flatten(list, tolevel)
     Flatten(list, tolevel, head)
   */
  pmath_expr_t list;
  pmath_t head;
  size_t depth, exprlen;
  
  exprlen = pmath_expr_length(expr);
  if(exprlen < 1 || exprlen > 3){
    pmath_message_argxxx(exprlen, 1, 3);
    return expr;
  }
  
  list = pmath_expr_get_item(expr, 1);
  if(!pmath_instance_of(list, PMATH_TYPE_EXPRESSION)){
    pmath_unref(list);
    return expr;
  }
  
  depth = SIZE_MAX;
  if(exprlen >= 2){
    pmath_t depth_arg = pmath_expr_get_item(expr, 2);
    if(!extract_number(depth_arg, SIZE_MAX, &depth)){
      pmath_unref(list);
      pmath_unref(depth_arg);
      return expr;
    }
    pmath_unref(depth_arg);
  }
  
  head = NULL;
  if(exprlen == 3)
    head = pmath_expr_get_item(expr, 3);
  else
    head = pmath_expr_get_item(list, 0);
    
  pmath_unref(expr);
  return pmath_expr_flatten(list, head, depth);
}
