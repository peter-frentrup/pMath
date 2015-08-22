#include <pmath-core/expressions-private.h>
#include <pmath-core/numbers-private.h>

#include <pmath-util/debug.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/messages.h>
#include <pmath-util/option-helpers.h>

#include <pmath-builtins/all-symbols-private.h>


#ifdef MIN
#  undef MIN
#endif

#define MIN(a, b)  (((a) < (b)) ? (a) : (b))

pmath_expr_t expr_select(
  pmath_expr_t   list, // will be freed
  pmath_bool_t (*test)(pmath_t, void*), // does not free first argument
  void          *context,
  size_t         max
) {
  size_t i0, length, first_fail, resi1;
  const pmath_t *old_items;
  pmath_t result;
  struct _pmath_expr_t *new_list;
  
  if(pmath_is_null(list))
    return list;
  
  length    = pmath_expr_length(list);
  old_items = pmath_expr_read_item_data(list);
  
  if(!old_items) {
    pmath_debug_print("[pmath_expr_read_item_data() gave NULL in %s:%d]\n", __FILE__, __LINE__);
    pmath_unref(list);
    return pmath_ref(_pmath_object_emptylist);
  }
  
  if(max > length)
    max = length;
    
  i0 = 0;
  while(max > 0 && (*test)(old_items[i0], context)) {
    ++i0;
    --max;
  }
  
  if(i0 == length)
    return list;
  
  if(max == 0){
    result = pmath_expr_get_item_range(list, 1, i0);
    pmath_unref(list);
    return result;
  }
  
  first_fail = i0;
  ++i0;
  while(i0 < length && !(*test)(old_items[i0], context)) {
    ++i0;
  }
  
  if(i0 == length){
    result = pmath_expr_get_item_range(list, 1, first_fail);
    pmath_unref(list);
    return result;
  }
  
  if(pmath_refcount(list) == 1 && pmath_is_pointer_of(list, PMATH_TYPE_EXPRESSION_GENERAL)) {
    new_list = (void*)PMATH_AS_PTR(list);
    
    resi1 = first_fail + 1;
    for(++first_fail;first_fail <= i0;++first_fail) {
      pmath_unref(new_list->items[first_fail]);
      new_list->items[first_fail] = PMATH_UNDEFINED;
    }
    
    new_list->items[resi1] = new_list->items[i0 + 1];
    new_list->items[i0 + 1] = PMATH_UNDEFINED;
    ++resi1;
    --max;
    
    for(++i0; i0 < length && max > 0; ++i0) {
      if((*test)(new_list->items[i0 + 1], context)) {
        new_list->items[resi1] = new_list->items[i0 + 1];
        new_list->items[i0 + 1] = PMATH_UNDEFINED;
        ++resi1;
        --max;
      }
      else{
        pmath_unref(new_list->items[i0 + 1]);
        new_list->items[i0 + 1] = PMATH_UNDEFINED;
      }
    }
    
    return pmath_expr_resize(list, resi1 - 1);
  }
  
  new_list = _pmath_expr_new_noinit(first_fail + MIN(max, length - i0));
  if(!new_list){
    pmath_unref(list);
    return PMATH_NULL;
  }
  
  new_list->items[0] = pmath_expr_get_item(list, 0);
  for(resi1 = 1;resi1 <= first_fail;++resi1) 
    new_list->items[resi1] = pmath_ref(old_items[resi1 - 1]);
  
  new_list->items[resi1] = pmath_ref(old_items[i0]);
  ++resi1;
  --max;
  
  for(++i0; i0 < length && max > 0; ++i0) {
    if((*test)(old_items[i0], context)) {
      new_list->items[resi1] = pmath_ref(old_items[i0]);
      ++resi1;
      --max;
    }
  }
  
  pmath_unref(list);
  
  --resi1;
  // set rest to bytes 0 == (double) 0.0
  memset(&new_list->items[resi1 + 1], 0, (new_list->length - resi1) * sizeof(pmath_t));
  
  return pmath_expr_resize(PMATH_FROM_PTR(new_list), resi1);
}

static pmath_bool_t eval_test(pmath_t item, void *context) {
  pmath_t *crit = context;
  
  item = pmath_expr_new_extended(pmath_ref(*crit), 1, pmath_ref(item));
  item = pmath_evaluate(item);
  pmath_unref(item);
  
  return pmath_same(item, PMATH_SYMBOL_TRUE);
}

PMATH_PRIVATE pmath_t builtin_select(pmath_expr_t expr) {
  /* Select(list, crit, n)
     Select(list, crit)    = Select(list, crit, Infinity)
  
     messages:
       General::innf
       General::nexprat
   */
  pmath_t list, crit;
  size_t exprlen, count;
  
  exprlen = pmath_expr_length(expr);
  
  if(exprlen < 2 || exprlen > 3) {
    pmath_message_argxxx(exprlen, 2, 3);
    return expr;
  }
  
  list = pmath_expr_get_item(expr, 1);
  if(!pmath_is_expr(list)) {
    pmath_message(PMATH_NULL, "nexprat", 2, PMATH_FROM_INT32(1), pmath_ref(expr));
    return expr;
  }
  
  count = SIZE_MAX;
  if(exprlen == 3) {
    pmath_t n = pmath_expr_get_item(expr, 3);
    
    if(pmath_is_int32(n) && PMATH_AS_INT32(n) >= 0) {
      count = (unsigned)PMATH_AS_INT32(n);
    }
    else if( !pmath_equals(n, _pmath_object_pos_infinity) &&
             !pmath_is_set_of_options(n))
    {
      pmath_unref(n);
      pmath_unref(list);
      pmath_message(PMATH_NULL, "innf", 2, PMATH_FROM_INT32(4), pmath_ref(expr));
      return expr;
    }
    
    pmath_unref(n);
  }
  
  crit = pmath_expr_get_item(expr, 2);
  pmath_unref(expr);
  
//  exprlen = pmath_expr_length(list);
//  for(i = 1; i <= exprlen && count > 0; ++i) {
//    pmath_t obj = pmath_expr_new_extended(
//                    pmath_ref(crit), 1,
//                    pmath_expr_get_item(list, i));
//                    
//    obj = pmath_evaluate(obj);
//    pmath_unref(obj);
//    
//    if(!pmath_same(obj, PMATH_SYMBOL_TRUE))
//      list = pmath_expr_set_item(list, i, PMATH_UNDEFINED);
//    else
//      --count;
//  }
//  
//  for(; i <= exprlen; ++i)
//    list = pmath_expr_set_item(list, i, PMATH_UNDEFINED);
//    
//  pmath_unref(crit);
//  return pmath_expr_remove_all(list, PMATH_UNDEFINED);

  list = expr_select(list, eval_test, &crit, count);

  pmath_unref(crit);
  return list;
}
