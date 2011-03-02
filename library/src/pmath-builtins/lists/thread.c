#include <pmath-core/expressions-private.h>

#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/lists-private.h>


PMATH_PRIVATE pmath_t builtin_thread(pmath_expr_t expr){
/* Thread(f(args), head, rangespec)
   Thread(f(args))              = Thread(f(args), List, 0..)
   Thread(f(args), head)        = Thread(f(args), List, 0..)
   Thread(f(args), head, n)     = Thread(f(args), List, n..n)
 */
  pmath_expr_t fst;
  pmath_t head;
  size_t exprlen, start, end;
  
  exprlen = pmath_expr_length(expr);
  
  if(exprlen < 1 || exprlen > 3){
    pmath_message_argxxx(exprlen, 1, 3);
    return expr;
  }
  
  fst = (pmath_expr_t)pmath_expr_get_item(expr, 1);
  if(!pmath_is_expr(fst)){
    pmath_unref(fst);
    return expr;
  }
  
  start = 1;
  end = pmath_expr_length(fst);
  if(exprlen == 3){
    pmath_t part = pmath_expr_get_item(expr, 3);
    if(pmath_same(part, PMATH_SYMBOL_ALL)){
      /* do nothing */
    }
    else if(pmath_same(part, PMATH_SYMBOL_NONE)){
      start = end = end + 1;
    }
    else if(!extract_range(part, &start, &end, TRUE)){
      pmath_message(PMATH_NULL, "pspec", 1, part);
      pmath_unref(fst);
      return expr;
    }
    pmath_unref(part);
  }
  
  if(exprlen >= 2)
    head = pmath_expr_get_item(expr, 2);
  else
    head = pmath_ref(PMATH_SYMBOL_LIST);
  
  pmath_unref(expr);
  fst = _pmath_expr_thread(fst, head, start, end, FALSE);
  pmath_unref(head);
  return fst;
}
