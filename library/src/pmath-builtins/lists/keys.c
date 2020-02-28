#include <pmath-core/expressions.h>

#include <pmath-util/dispatch-table-private.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>


pmath_t get_keys(pmath_t list) {
  pmath_dispatch_table_t disp;
  size_t i;
  
  if(!pmath_is_expr_of(list, PMATH_SYMBOL_LIST)) {
    pmath_message(PMATH_NULL, "reps", 1, list); // Mma gives General::invrl
    return PMATH_NULL;
  }
  
  disp = _pmath_rules_need_dispatch_table(list);
  if(!pmath_is_null(disp)) {
    struct _pmath_dispatch_table_t *tab = (void*)PMATH_AS_PTR(disp);
    pmath_unref(list);
    list = pmath_ref(tab->all_keys);
    pmath_unref(disp);
    return list;
  }
  
  for(i = pmath_expr_length(list); i > 0; --i) {
    pmath_t item = pmath_expr_get_item(list, i);
    item = get_keys(item);
    if(pmath_is_null(item)) {
      pmath_unref(list);
      return PMATH_NULL;
    }
    list = pmath_expr_set_item(list, i, item);
  }
  
  return list;
}

PMATH_PRIVATE pmath_t builtin_keys(pmath_expr_t expr) {
  pmath_t list;
  
  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  list = pmath_expr_get_item(expr, 1);
  list = get_keys(list);
  if(pmath_is_null(list))
    return expr;
  
  pmath_unref(expr);
  return list;
}
