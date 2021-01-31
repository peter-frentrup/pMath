#include <pmath-core/expressions-private.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/language-private.h>


extern pmath_symbol_t pmath_System_DollarFailed;

PMATH_PRIVATE pmath_t builtin_developer_getdebuginfo(pmath_expr_t expr) {
  pmath_t obj;
  size_t exprlen = pmath_expr_length(expr);
  size_t i;
  
  if(exprlen < 1) {
    pmath_unref(expr);
    return pmath_ref(pmath_System_DollarFailed);
  }
  
  obj = pmath_expr_get_item(expr, 1);
  for(i = 2; i <= exprlen; ++i) {
    pmath_t index = pmath_expr_get_item(expr, i);
    
    if( pmath_is_int32(index) &&
        PMATH_AS_INT32(index) >= 0 &&
        pmath_is_expr(obj))
    {
      pmath_t tmp = pmath_expr_get_item(obj, (size_t)PMATH_AS_INT32(index));
      pmath_unref(obj);
      
      obj = tmp;
      continue;
    }
    
    pmath_unref(obj);
    pmath_unref(index);
    pmath_unref(expr);
    return pmath_ref(pmath_System_DollarFailed);
  }
  
  pmath_unref(expr);
  expr = pmath_get_debug_info(obj);
  pmath_unref(obj);
  return expr;
}

static pmath_t set_debuginfo_at(
  pmath_t obj,     // will be freed
  pmath_t info,    // will be freed
  pmath_t indices, // wont be freed
  size_t  i
) {
  if(i <= pmath_expr_length(indices)) {
    pmath_t index = pmath_expr_get_item(indices, i);
    
    if( pmath_is_int32(index) &&
        PMATH_AS_INT32(index) >= 0 &&
        pmath_is_expr(obj))
    {
      pmath_t sub = pmath_expr_extract_item(obj, (size_t)PMATH_AS_INT32(index));
      
      sub = set_debuginfo_at(sub, info, indices, i + 1);
      
      info = _pmath_expr_get_debug_info(obj);
      obj = pmath_expr_set_item(obj, (size_t)PMATH_AS_INT32(index), sub);
      obj = _pmath_expr_set_debug_info(obj, info);
      return obj;
    }
    
    pmath_unref(index);
    pmath_unref(info);
    return obj;
  }
  
  return pmath_try_set_debug_info(obj, info);
}

PMATH_PRIVATE pmath_t builtin_developer_setdebuginfoat(pmath_expr_t expr) {
  /* SetDebugInfo(obj, info, [i1, i2, ...])
   */
  pmath_t obj, info;
  size_t exprlen = pmath_expr_length(expr);
  
  if(exprlen < 2 || exprlen > 20) {
    pmath_unref(expr);
    return pmath_ref(pmath_System_DollarFailed);
  }
  
  obj = pmath_expr_extract_item(expr, 1);
  info = pmath_expr_get_item(expr, 2);
  
  obj = set_debuginfo_at(obj, info, expr, 3);
  pmath_unref(expr);
  
  return obj;
}
