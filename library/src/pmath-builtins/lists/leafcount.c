#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

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

#include <pmath-builtins/lists-private.h>
#include <pmath-builtins/all-symbols.h>
#include <pmath-builtins/all-symbols-private.h>

static size_t leafcount(pmath_t obj){ // obj will be freed
  if(pmath_instance_of(obj, PMATH_TYPE_EXPRESSION)){
    size_t result = 0;
    size_t i;
    
    for(i = 0;i <= pmath_expr_length(obj);++i)
      result+= leafcount(pmath_expr_get_item(obj, i));
    
    pmath_unref(obj);
    return result;
  }
  
  pmath_unref(obj);
  return 1;
}

PMATH_PRIVATE pmath_t builtin_leafcount(pmath_expr_t expr){
  pmath_t obj;
  
  if(pmath_expr_length(expr) != 1){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  obj = pmath_expr_get_item(expr, 1);
  pmath_unref(expr);
  
  return pmath_integer_new_size(leafcount(obj));
}
