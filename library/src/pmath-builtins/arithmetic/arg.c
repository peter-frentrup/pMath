#include <pmath-core/expressions.h>
#include <pmath-core/numbers.h>
#include <pmath-core/symbols.h>

#include <assert.h>
#include <string.h>

#include <pmath-util/approximate.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/messages.h>

#include <pmath-core/numbers-private.h>

#include <pmath-builtins/arithmetic-private.h>
#include <pmath-builtins/build-expr-private.h>
#include <pmath-builtins/lists-private.h>
#include <pmath-builtins/number-theory-private.h>
#include <pmath-builtins/all-symbols.h>
#include <pmath-builtins/all-symbols-private.h>

PMATH_PRIVATE pmath_t builtin_arg(pmath_expr_t expr){
  pmath_t z, re, im;
  
  if(pmath_expr_length(expr) != 1){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  z = pmath_expr_get_item(expr, 1);
  
  if(!_pmath_is_numeric(z)){
    pmath_unref(z);
    return expr;
  }
  
  if(_pmath_re_im(z, &re, &im)){
    int reclass = _pmath_number_class(re);
    
    pmath_unref(expr);
      
    if(reclass & PMATH_CLASS_NEG){
      if(_pmath_number_class(im) & PMATH_CLASS_NEG)
        return MINUS(ARCTAN(DIV(im, re)), pmath_ref(PMATH_SYMBOL_PI));
      
      return PLUS(ARCTAN(DIV(im, re)), pmath_ref(PMATH_SYMBOL_PI));
    }
    
    if(reclass & PMATH_CLASS_ZERO){
      if(_pmath_number_class(im) & PMATH_CLASS_NEG)
        return TIMES(QUOT(-1, 2), pmath_ref(PMATH_SYMBOL_PI));
      
      return TIMES(ONE_HALF, pmath_ref(PMATH_SYMBOL_PI));
    }
    
    return ARCTAN(DIV(im, re));
  }
  
  pmath_unref(re);
  pmath_unref(im);
  return expr;
}
