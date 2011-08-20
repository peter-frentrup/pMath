#include <pmath-core/numbers.h>

#include <pmath-util/concurrency/threads-private.h>
#include <pmath-util/dynamic-private.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/control-private.h>


PMATH_PRIVATE pmath_t builtin_internal_dynamicevaluate(pmath_expr_t expr) {
  /* Internal`DynamicEvaluate(expr, id)
   */
  pmath_t id_obj, dyn_expr;
  pmath_thread_t thread = pmath_thread_get_current();
  
  if(!thread) {
    return expr;
  }
  
  if(pmath_expr_length(expr) != 2) {
    pmath_message_argxxx(pmath_expr_length(expr), 2, 2);
    return expr;
  }
  
  id_obj = pmath_evaluate(pmath_expr_get_item(expr, 2));
  if(pmath_is_int32(id_obj)) {
    intptr_t old_id;
    long id = PMATH_AS_INT32(id_obj);
    
    dyn_expr = pmath_expr_get_item(expr, 1);
    
    pmath_unref(expr);
    
    pmath_atomic_fetch_add(&_pmath_dynamic_trackers, 1);
    old_id = thread->current_dynamic_id;
    thread->current_dynamic_id = id;
    
    dyn_expr = pmath_evaluate(dyn_expr);
    
    thread->current_dynamic_id = old_id;
    pmath_atomic_fetch_add(&_pmath_dynamic_trackers, -1);
    
    return dyn_expr;
  }
  
  pmath_unref(id_obj);
  pmath_message(PMATH_NULL, "intm", 2, pmath_ref(expr), PMATH_FROM_INT32(2));
  return expr;
}

static pmath_t find_tracked_symbols(pmath_t dynamic, size_t start) {
  size_t i;
  
  assert(start > 0);
  
  for(i = pmath_expr_length(dynamic); i >= start; --i) {
    pmath_t item = pmath_expr_get_item(dynamic, i);
    
    if(_pmath_is_rule(item)) {
      pmath_t lhs = pmath_expr_get_item(item, 1);
      pmath_unref(lhs);
      
      if(pmath_same(lhs, PMATH_SYMBOL_TRACKEDSYMBOLS)) {
        pmath_t ts = pmath_expr_get_item(item, 2);
        
        pmath_unref(item);
        
        if(pmath_same(ts, PMATH_SYMBOL_AUTOMATIC)) {
          pmath_unref(ts);
          return PMATH_UNDEFINED;
        }
        
        if(pmath_same(ts, PMATH_SYMBOL_NONE)) {
          pmath_unref(ts);
          return PMATH_NULL;
        }
        
        return ts;
      }
    }
    else if(pmath_is_expr_of(item, PMATH_SYMBOL_LIST)) {
      pmath_t ts = find_tracked_symbols(item, 1);
      
      if(!pmath_same(ts, PMATH_UNDEFINED)) {
        pmath_unref(item);
        return ts;
      }
    }
    
    pmath_unref(item);
  }
  
  return PMATH_UNDEFINED;
}

static pmath_t replace_dynamic(
  pmath_t expr,   // will be freed
  pmath_t id_obj  // wont be freed
) {
  if(pmath_is_expr(expr)) {
    pmath_t head = pmath_expr_get_item(expr, 0);
    size_t len = pmath_expr_length(expr);
    size_t i;
    
    if(len >= 1 && pmath_same(head, PMATH_SYMBOL_DYNAMIC)) {
      pmath_t dyn_expr = pmath_expr_get_item(expr, 1);
      pmath_t ts       = find_tracked_symbols(expr, 2);
      pmath_unref(head);
      pmath_unref(expr);
      
      if(!pmath_same(ts, PMATH_UNDEFINED)) {
        return pmath_expr_new_extended(
                 pmath_ref(PMATH_SYMBOL_EVALUATIONSEQUENCE), 2,
                 pmath_expr_new_extended(
                   pmath_ref(PMATH_SYMBOL_INTERNAL_DYNAMICEVALUATE), 2,
                   ts,
                   pmath_ref(id_obj)),
                 dyn_expr);
      }
      
      return pmath_expr_new_extended(
               pmath_ref(PMATH_SYMBOL_INTERNAL_DYNAMICEVALUATE), 2,
               dyn_expr,
               pmath_ref(id_obj));
    }
    
    expr = pmath_expr_set_item(expr, 0, PMATH_NULL);
    head = replace_dynamic(head, id_obj);
    expr = pmath_expr_set_item(expr, 0, head);
    for(i = len; i > 0; --i) {
      pmath_t item = pmath_expr_get_item(expr, i);
      expr = pmath_expr_set_item(expr, i, PMATH_NULL);
      
      item = replace_dynamic(item, id_obj);
      
      expr = pmath_expr_set_item(expr, i, item);
    }
  }
  
  return expr;
}

PMATH_PRIVATE pmath_t builtin_internal_dynamicevaluatemultiple(pmath_expr_t expr) {
  /* Internal`DynamicEvaluateMultiple(expr, id)
   */
  pmath_t id_obj, dyn_expr;
  pmath_thread_t thread = pmath_thread_get_current();
  
  if(!thread) {
    return expr;
  }
  
  if(pmath_expr_length(expr) != 2) {
    pmath_message_argxxx(pmath_expr_length(expr), 2, 2);
    return expr;
  }
  
  id_obj = pmath_evaluate(pmath_expr_get_item(expr, 2));
  if(pmath_is_int32(id_obj)) {
    dyn_expr = pmath_expr_get_item(expr, 1);
    pmath_unref(expr);
    
    return replace_dynamic(dyn_expr, id_obj);
  }
  
  pmath_unref(id_obj);
  pmath_message(PMATH_NULL, "intm", 2, pmath_ref(expr), PMATH_FROM_INT32(2));
  return expr;
}

PMATH_PRIVATE pmath_t builtin_internal_dynamicremove(pmath_expr_t expr) {
  /* Internal`DynamicRemove(id1, id2, ...)
   */
  size_t len = pmath_expr_length(expr);
  size_t i;
  
  for(i = len; i > 0; --i) {
    pmath_t id_obj = pmath_evaluate(pmath_expr_get_item(expr, i));
    
    if(pmath_is_int32(id_obj)) {
      _pmath_dynamic_remove(PMATH_AS_INT32(id_obj));
      continue;
    }
    
    if(pmath_is_expr_of(id_obj, PMATH_SYMBOL_LIST)) {
      pmath_unref(builtin_internal_dynamicremove(id_obj));
      continue;
    }
    
    pmath_unref(id_obj);
  }
  
  pmath_unref(expr);
  return PMATH_NULL;
}
