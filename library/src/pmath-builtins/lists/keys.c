#include <pmath-core/expressions.h>

#include <pmath-util/dispatch-tables-private.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>


extern pmath_symbol_t pmath_System_List;

// list will be freed, wrap_head won't
pmath_t get_keys(pmath_t list, pmath_t wrap_head) {
  pmath_dispatch_table_t disp;
  size_t i;
  
  if(!pmath_is_expr_of(list, pmath_System_List)) {
    pmath_message(PMATH_NULL, "reps", 1, list); // Mma gives General::invrl
    return PMATH_NULL;
  }
  
  disp = _pmath_rules_need_dispatch_table(list);
  if(!pmath_is_null(disp)) {
    struct _pmath_dispatch_table_t *tab = (void*)PMATH_AS_PTR(disp);
    pmath_unref(list);
    list = pmath_ref(tab->all_keys);
    pmath_unref(disp);
    
    if(!pmath_same(wrap_head, PMATH_UNDEFINED)) {
      for(i = pmath_expr_length(list); i > 0; --i) {
        pmath_t key = pmath_expr_get_item(list, i);
        key = pmath_expr_new_extended(pmath_ref(wrap_head), 1, key);
        list = pmath_expr_set_item(list, i, key);
      }
    }
    return list;
  }
  
  for(i = pmath_expr_length(list); i > 0; --i) {
    pmath_t item = pmath_expr_get_item(list, i);
    item = get_keys(item, wrap_head);
    if(pmath_is_null(item)) {
      pmath_unref(list);
      return PMATH_NULL;
    }
    list = pmath_expr_set_item(list, i, item);
  }
  
  return list;
}

PMATH_PRIVATE pmath_t builtin_keys(pmath_expr_t expr) {
  /*  Keys({rule1, rule2, ...})
      Keys(rules, head)
  */
  size_t exprlen = pmath_expr_length(expr);
  pmath_t list;
  pmath_t wrap_head;
  
  if(exprlen < 1 || exprlen > 2) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 2);
    return expr;
  }
  
  if(exprlen == 2)
    wrap_head = pmath_expr_get_item(expr, 2);
  else
    wrap_head = PMATH_UNDEFINED;
  
  list = pmath_expr_get_item(expr, 1);
  list = get_keys(list, wrap_head);
  if(pmath_is_null(list)){
    pmath_unref(wrap_head);
    return expr;
  }
  
  pmath_unref(wrap_head);
  pmath_unref(expr);
  return list;
}
