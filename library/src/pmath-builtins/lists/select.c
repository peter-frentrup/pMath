#include <pmath-core/numbers-private.h>

#include <pmath-util/evaluation.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/control-private.h>


PMATH_PRIVATE pmath_t builtin_select(pmath_expr_t expr){
/* Select(list, crit, n)
   Select(list, crit)    = Select(list, crit, Infinity)
   
   messages:
     General::innf
     General::nexprat
 */
  pmath_t list, crit;
  size_t exprlen, i, count;
  
  exprlen = pmath_expr_length(expr);
  
  if(exprlen < 2 || exprlen > 3){
    pmath_message_argxxx(exprlen, 2, 3);
    return expr;
  }
  
  list = pmath_expr_get_item(expr, 1);
  if(!pmath_instance_of(list, PMATH_TYPE_EXPRESSION)){
    pmath_message(NULL, "nexprat", 2, pmath_integer_new_si(1), pmath_ref(expr));
    return expr;
  }
  
  count = SIZE_MAX;
  if(exprlen == 3){
    pmath_t n = pmath_expr_get_item(expr, 3);

    if(pmath_instance_of(n, PMATH_TYPE_INTEGER)
    && pmath_number_sign(n) >= 0){
      if(pmath_integer_fits_ui(n))
        count = pmath_integer_get_ui(n);
    }
    else if(!pmath_equals(n, _pmath_object_infinity)
    && !_pmath_is_rule(n) 
    && !_pmath_is_list_of_rules(n)){
      pmath_unref(n);
      pmath_unref(list);
      pmath_message(NULL, "innf", 2, pmath_integer_new_si(4), pmath_ref(expr));
      return expr;
    }
    
    pmath_unref(n);
  }
  
  crit = pmath_expr_get_item(expr, 2);
  pmath_unref(expr);
  
  exprlen = pmath_expr_length(list);
  for(i = 1;i <= exprlen && count > 0;++i){
    pmath_t obj = pmath_expr_new_extended(
      pmath_ref(crit), 1, 
      pmath_expr_get_item(list, i));
    
    obj = pmath_evaluate(obj);
    pmath_unref(obj);
    
    if(obj != PMATH_SYMBOL_TRUE)
      list = pmath_expr_set_item(list, i, PMATH_UNDEFINED);
    else
      --count;
  }
  
  for(;i <= exprlen;++i)
    list = pmath_expr_set_item(list, i, PMATH_UNDEFINED);
  
  pmath_unref(crit);
  return pmath_expr_remove_all(list, PMATH_UNDEFINED);
}
