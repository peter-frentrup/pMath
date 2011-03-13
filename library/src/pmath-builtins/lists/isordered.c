#include <pmath-core/numbers.h>

#include <pmath-util/evaluation.h>
#include <pmath-util/messages.h>

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

  list = pmath_expr_get_item(expr, 1);
  if(!pmath_is_expr(list)){
    pmath_unref(list);
    pmath_message(PMATH_NULL, "nexprat", 2, PMATH_FROM_INT32(1), pmath_ref(expr));
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
      
      if(!pmath_same(cmp, PMATH_SYMBOL_TRUE)){
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
