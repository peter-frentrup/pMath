#include <pmath-core/expressions.h>

#include <pmath-util/messages.h>


PMATH_PRIVATE pmath_t builtin_through(pmath_expr_t expr) {
  /* Through(p(f1, f2)(x))  =  p(f1(x), f2(x))
     Through(expr, h) transforms expr if the head is h
   */
  pmath_t obj, head;
  size_t l;
  
  l = pmath_expr_length(expr);
  if(l < 1 || l > 2) {
    pmath_message_argxxx(l, 1, 2);
    return expr;
  }
  
  head = l == 1 ? PMATH_NULL : pmath_expr_get_item(expr, 2);
  
  obj = pmath_expr_get_item(expr, 1);
  pmath_unref(expr);
  
  if(!pmath_is_expr(obj)) {
    pmath_unref(head);
    return obj;
  }
  
  expr = pmath_expr_get_item(obj, 0);
  
  if(!pmath_is_expr(expr)) {
    pmath_unref(head);
    pmath_unref(expr);
    return obj;
  }
  
  if(l == 2) {
    pmath_t h = pmath_expr_get_item(expr, 0);
    if(!pmath_equals(head, h)) {
      pmath_unref(head);
      pmath_unref(h);
      pmath_unref(expr);
      return obj;
    }
    pmath_unref(h);
  }
  
  pmath_unref(head);
  
  obj = pmath_expr_set_item(obj, 0, PMATH_NULL);
  for(l = pmath_expr_length(expr); l > 0; --l) {
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
