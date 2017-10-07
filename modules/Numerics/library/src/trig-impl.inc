#ifndef ACB_FUNCTION
#  error ACB_FUNCTION not defined
#endif

pmath_t x;

if(pmath_expr_length(expr) != 1) {
  pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
  return expr;
}

x = pmath_expr_get_item(expr, 1);
if(pmath_complex_try_evaluate_acb(&expr, x, ACB_FUNCTION)) {
  pmath_unref(x);
  return expr;
}
  
x = pmath_expr_new_extended(
      pmath_ref(pmath_System_Private_AutoSimplifyTrig), 2,
      pmath_expr_get_item(expr, 0),
      x);

x = pmath_evaluate(x);
if(!pmath_same(x, PMATH_SYMBOL_FAILED)) {
  pmath_unref(expr);
  return x;
}

pmath_unref(x);
return expr;
