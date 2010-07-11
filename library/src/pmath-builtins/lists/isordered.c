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

#include <pmath-util/evaluation.h>
#include <pmath-util/messages.h>

#include <pmath-util/concurrency/atomic.h>

#include <pmath-core/objects-inline.h>

#include <pmath-builtins/control-private.h>
#include <pmath-builtins/all-symbols.h>
#include <pmath-builtins/all-symbols-private.h>

PMATH_PRIVATE pmath_t builtin_isordered(pmath_expr_t expr){
/* IsOrdered(f(a,b,...))
   IsOrdered(f(a,b,...), lessfn)
 */
  pmath_expr_t list;
  pmath_t last;
  size_t i, exprlen;
  
  exprlen = pmath_expr_length(expr);
  if(exprlen < 1 || exprlen > 2){
    pmath_message_argxxx(exprlen, 1, 2);
    return expr;
  }

  list = (pmath_expr_t)pmath_expr_get_item(expr, 1);
  if(!pmath_instance_of(list, PMATH_TYPE_EXPRESSION)){
    pmath_message(NULL, "noexpr", 1, list);
    return expr;
  }

  if(exprlen == 2){
    pmath_t lessfn = pmath_expr_get_item(expr, 2);
    pmath_t last = pmath_expr_get_item(list, 1);
    size_t i;
    for(i = 2;i <= pmath_expr_length(list);++i){
      pmath_t current = pmath_expr_get_item(list, i);

      pmath_t cmp = pmath_evaluate(pmath_expr_new_extended(
        pmath_ref(lessfn), 2,
        pmath_ref(last),
        pmath_ref(current)));
      pmath_unref(cmp);
      if(cmp != PMATH_SYMBOL_TRUE){
        pmath_unref(last);
        pmath_unref(current);
        pmath_unref(lessfn);
        pmath_unref(list);
        pmath_unref(expr);
        return pmath_ref(PMATH_SYMBOL_FALSE);
      }

      pmath_unref(last);
      last = current;
    }
    pmath_unref(last);
    pmath_unref(lessfn);
    pmath_unref(list);
    pmath_unref(expr);
    return pmath_ref(PMATH_SYMBOL_TRUE);
  }

  last = pmath_expr_get_item(list, 1);
  for(i = 2;i <= pmath_expr_length(list);++i){
    pmath_t current = pmath_expr_get_item(list, i);

    if(pmath_compare(last, current) > 0){
      pmath_unref(last);
      pmath_unref(current);
      pmath_unref(list);
      pmath_unref(expr);
      return pmath_ref(PMATH_SYMBOL_FALSE);
    }

    pmath_unref(last);
    last = current;
  }
  
  pmath_unref(last);
  pmath_unref(list);
  pmath_unref(expr);
  return pmath_ref(PMATH_SYMBOL_TRUE);
}
