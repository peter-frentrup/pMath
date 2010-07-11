#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

#include <pmath-config.h>
#include <pmath-types.h>
#include <pmath-core/objects.h>
#include <pmath-core/expressions.h>
#include <pmath-core/numbers.h>
#include <pmath-core/strings.h>
#include <pmath-core/symbols.h>

#include <pmath-util/messages.h>

#include <pmath-util/concurrency/atomic.h>

#include <pmath-core/objects-inline.h>
#include <pmath-core/numbers-private.h>

#include <pmath-builtins/arithmetic-private.h>
#include <pmath-builtins/all-symbols.h>
#include <pmath-builtins/all-symbols-private.h>

static pmath_t chop(
  pmath_t obj,   // will be freed
  pmath_number_t ntol,  // wont be freed
  pmath_number_t ptol   // wont be freed
){
  if(pmath_instance_of(obj, PMATH_TYPE_FLOAT)){
    if(pmath_number_sign(obj) == 0){
      pmath_unref(obj);
      return pmath_integer_new_si(0);
    }
    
    if(pmath_number_sign(obj) < 0){
      if(pmath_compare(ntol, obj) < 0){
        pmath_unref(obj);
        return pmath_integer_new_si(0);
      }
    }
    else if(pmath_compare(obj, ptol) < 0){
      pmath_unref(obj);
      return pmath_integer_new_si(0);
    }
    
    return obj;
  }
  
  if(pmath_instance_of(obj, PMATH_TYPE_EXPRESSION)){
    size_t i;
    
    for(i = 0;i <= pmath_expr_length(obj);++i){
      obj = pmath_expr_set_item(
        obj, i, 
        chop(
          pmath_expr_get_item(obj, i),
          ntol,
          ptol));
    }
  }
  
  return obj;
}

PMATH_PRIVATE
pmath_t builtin_chop(pmath_expr_t expr){
  pmath_t obj;
  pmath_number_t ntol, ptol;
  size_t exprlen = pmath_expr_length(expr);

  if(exprlen < 1 || exprlen > 2){
    pmath_message_argxxx(exprlen, 1, 2);
    return expr;
  }
  
  if(exprlen == 2){
    ptol = (pmath_number_t)pmath_expr_get_item(expr, 2);
    
    if(!pmath_instance_of(ptol, PMATH_TYPE_NUMBER)
    || pmath_number_sign(ptol) < 0){
      pmath_unref(ptol);
      
      pmath_message(
        NULL, "numn", 2,
        pmath_integer_new_si(2),
        pmath_ref(expr));
      
      return expr;
    }
  }
  else
    ptol = pmath_float_new_d(1e-10);
  
  ntol = pmath_number_neg(pmath_ref(ptol));
  
  obj = pmath_expr_get_item(expr, 1);
  pmath_unref(expr);
  
  obj = chop(obj, ntol, ptol);
  
  pmath_unref(ntol);
  pmath_unref(ptol);
  
  return obj;
}
