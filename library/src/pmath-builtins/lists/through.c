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

PMATH_PRIVATE pmath_t builtin_through(pmath_expr_t expr){
  /* Through(p(f1, f2)(x))  =  p(f1(x), f2(x))
     Through(expr, h) transforms expr if the head is h
   */
  pmath_t obj, head;
  size_t l;
  
  l = pmath_expr_length(expr);
  if(l < 1 || l > 2){
    pmath_message_argxxx(l, 1, 2);
    return expr;
  }
  
  head = l == 1 ? NULL : pmath_expr_get_item(expr, 2);
  
  obj = pmath_expr_get_item(expr, 1);
  pmath_unref(expr);
  
  if(!pmath_instance_of(obj, PMATH_TYPE_EXPRESSION)){
    pmath_unref(head);
    return obj;
  }
  
  expr = pmath_expr_get_item(obj, 0);
  
  if(!pmath_instance_of(expr, PMATH_TYPE_EXPRESSION)){
    pmath_unref(head);
    pmath_unref(expr);
    return obj;
  }
  
  if(l == 2){
    pmath_t h = pmath_expr_get_item(expr, 0);
    if(!pmath_equals(head, h)){
      pmath_unref(head);
      pmath_unref(h);
      pmath_unref(expr);
      return obj;
    }
    pmath_unref(h);
  }
  
  pmath_unref(head);
  
  obj = pmath_expr_set_item(obj, 0, NULL);
  for(l = pmath_expr_length(expr);l > 0;--l){
    expr = pmath_expr_set_item(
      expr, l,
      pmath_expr_set_item(
        pmath_ref(obj), 0, 
        pmath_expr_get_item(
          expr, l)));
  }
  
  pmath_unref(obj);
  return expr;
}
