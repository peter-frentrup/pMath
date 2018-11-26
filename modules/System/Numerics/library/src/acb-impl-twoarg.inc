/* Implement functions F(x, y) of a two complex arguments.
   First try ACB_FUNCTION(x, y) in C and then fall back to SIMPLIFY_SYMBOL(F, x, y)
   in pMath, returning unevaluated if that gives $Failed.
 */
#ifndef ACB_FUNCTION
#  error ACB_FUNCTION not defined
#endif
#ifndef SIMPLIFY_SYMBOL
#  error SIMPLIFY_SYMBOL not defined
#endif

pmath_t x;
pmath_t y;

if(pmath_expr_length(expr) != 2) {
  pmath_message_argxxx(pmath_expr_length(expr), 2, 2);
  return expr;
}

x = pmath_expr_get_item(expr, 1);
y = pmath_expr_get_item(expr, 2);
if(pmath_complex_try_evaluate_acb_2(&expr, x, y, ACB_FUNCTION)) {
  pmath_unref(x);
  pmath_unref(y);
  return expr;
}
  
x = pmath_expr_new_extended(
      pmath_ref(SIMPLIFY_SYMBOL), 3,
      pmath_expr_get_item(expr, 0),
      x,
      y);

x = pmath_evaluate(x);
if(!pmath_same(x, PMATH_SYMBOL_FAILED)) {
  pmath_unref(expr);
  return x;
}

pmath_unref(x);
return expr;
