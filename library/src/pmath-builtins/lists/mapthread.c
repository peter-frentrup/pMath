#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/lists-private.h>


extern pmath_symbol_t pmath_System_List;

static pmath_t mapthread(pmath_t array, pmath_t f, unsigned level) { // array will be freed; f wont
  size_t i, j, args, count;
  pmath_t result;
  
  if(!pmath_is_expr_of(array, pmath_System_List)) {
    pmath_unref(array);
    return PMATH_NULL;
  }
  
  if(level == 0)
    return pmath_expr_set_item(array, 0, pmath_ref(f));
    
  if(pmath_expr_length(array) == 0)
    return array;
    
  args = pmath_expr_length(array);
  
  result = pmath_expr_get_item(array, 1);
  if(!pmath_is_expr_of(result, pmath_System_List)) {
    pmath_unref(array);
    pmath_unref(result);
    return PMATH_NULL;
  }
  
  count = pmath_expr_length(result);
  
  for(i = 1; i <= count; ++i) {
    pmath_t func = pmath_expr_new(pmath_ref(pmath_System_List), args);
    
    for(j = 1; j <= args; ++j) {
      pmath_t item = _pmath_matrix_get(array, j, i);
      func = pmath_expr_set_item(func, j, item);
    }
    
    result = pmath_expr_set_item(result, i, func);
  }
  
  --level;
  pmath_unref(array);
  for(i = 1; i <= count; ++i) {
    array = pmath_expr_extract_item(result, i);
    
    array = mapthread(array, f, level);
    if(pmath_is_null(array)) {
      pmath_unref(result);
      return PMATH_NULL;
    }
    
    result = pmath_expr_set_item(result, i, array);
  }
  
  return result;
}


PMATH_PRIVATE pmath_t builtin_mapthread(pmath_expr_t expr) {
  /* MapThread({{a1, a2, ...}, {b1, b2, ...}, ...}, f, level)
     MapThread(array, f)    = MapThread(array, f, 1)
  
     messages:
       General::intnm
       General::level
       General::list
       MapThread::mptd
   */
  size_t exprlen;
  unsigned level;
  pmath_t array, func;
  
  exprlen = pmath_expr_length(expr);
  if(exprlen < 2 || exprlen > 3) {
    pmath_message_argxxx(exprlen, 2, 3);
    return expr;
  }
  
  level = 1;
  if(exprlen == 3) {
    pmath_t lvl = pmath_expr_get_item(expr, 3);
    
    if(!pmath_is_int32(lvl) || PMATH_AS_INT32(lvl) < 0) {
      pmath_unref(lvl);
      pmath_message(PMATH_NULL, "intnm", 2, pmath_ref(expr), PMATH_FROM_INT32(3));
      return expr;
    }
    
    level = (unsigned)PMATH_AS_INT32(lvl);
  }
  
  array = pmath_expr_get_item(expr, 1);
  if(!pmath_is_expr_of(array, pmath_System_List)) {
    pmath_unref(array);
    pmath_message(PMATH_NULL, "list", 2, PMATH_FROM_INT32(1), pmath_ref(expr));
    return expr;
  }
  
  func = pmath_expr_get_item(expr, 2);
  array = mapthread(array, func, level);
  pmath_unref(func);
  
  if(pmath_is_null(array)) {
    long  depth;
    
    array = pmath_expr_get_item(expr, 1);
    depth = _pmath_object_depth(array);
    
    pmath_message(PMATH_NULL, "mptd", 5,
                  array,
                  PMATH_FROM_INT32(1),
                  pmath_ref(expr),
                  PMATH_FROM_INT32(depth - 1),
                  PMATH_FROM_INT32(level + 1));
                  
    return expr;
  }
  
  pmath_unref(expr);
  return array;
}
