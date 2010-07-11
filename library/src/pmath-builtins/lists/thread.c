#include <assert.h>
#include <limits.h>
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
#include <pmath-core/objects-private.h>
#include <pmath-core/expressions-private.h>

#include <pmath-builtins/lists-private.h>
#include <pmath-builtins/all-symbols.h>
#include <pmath-builtins/all-symbols-private.h>

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
  if(!pmath_instance_of(fst, PMATH_TYPE_EXPRESSION)){
    pmath_unref(fst);
    return expr;
  }
  
  start = 1;
  end = pmath_expr_length(fst);
  if(exprlen == 3){
    pmath_t part = pmath_expr_get_item(expr, 3);
    if(!extract_range(part, &start, &end, TRUE)){
      pmath_message(NULL, "pspec", 1, part);
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
