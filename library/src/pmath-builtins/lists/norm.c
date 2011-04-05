#include <pmath-util/helpers.h>
#include <pmath-core/numbers-private.h>

#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/build-expr-private.h>
#include <pmath-builtins/lists-private.h>
#include <pmath-builtins/number-theory-private.h>


PMATH_PRIVATE pmath_t builtin_norm(pmath_expr_t expr){
  size_t exprlen = pmath_expr_length(expr);
  pmath_t matrix;
  pmath_t ptype;
  size_t rows, cols;
  int ptype_class;
  
  if(exprlen < 1 || exprlen > 2){
    pmath_message_argxxx(exprlen, 1, 2);
    return expr;
  }
  
  if(exprlen == 2)
    ptype = pmath_expr_get_item(expr, 2);
  else
    ptype = PMATH_FROM_INT32(2);
  
  matrix = pmath_expr_get_item(expr, 1);
  
  if(pmath_is_string(ptype)
  && pmath_string_equals_latin1(ptype, "Frobenius")){
    pmath_unref(ptype);
      
    if(!pmath_is_expr_of(matrix, PMATH_SYMBOL_LIST)){
      pmath_unref(matrix);
      return expr;
    }
    
    pmath_unref(expr);
    
    if(_pmath_is_matrix(matrix, &rows, &cols)){
      return SQRT(FUNC(pmath_ref(PMATH_SYMBOL_TOTAL), 
        FUNC(pmath_ref(PMATH_SYMBOL_TOTAL), POW(ABS(matrix), INT(2)))));
    }
    
    return SQRT(FUNC(pmath_ref(PMATH_SYMBOL_TOTAL), POW(ABS(matrix), INT(2))));
  }
  
  ptype_class = _pmath_number_class(ptype);
  if((ptype_class & ~(PMATH_CLASS_POSONE | PMATH_CLASS_POSBIG | PMATH_CLASS_POSINF | PMATH_CLASS_UNKNOWN))){
    pmath_unref(matrix);
    pmath_message(PMATH_NULL, "ptype", 1, ptype);
    return expr;
  }
  
  if(!pmath_is_expr_of(matrix, PMATH_SYMBOL_LIST)){
    pmath_unref(matrix);
    pmath_unref(ptype);
    return expr;
  }
    
  if(ptype_class == PMATH_CLASS_POSINF){
    pmath_unref(ptype);
    pmath_unref(expr);
    
    if(_pmath_is_matrix(matrix, &rows, &cols)){
      return FUNC(pmath_ref(PMATH_SYMBOL_MAX),
        FUNC2(pmath_ref(PMATH_SYMBOL_MAP), 
          ABS(matrix),
          pmath_ref(PMATH_SYMBOL_TOTAL)));
    }
    
    return FUNC(pmath_ref(PMATH_SYMBOL_MAX), ABS(matrix));
  }
  
  if(ptype_class == PMATH_CLASS_POSONE){
    pmath_unref(ptype);
    pmath_unref(expr);
    
    if(_pmath_is_matrix(matrix, &rows, &cols)){
      return FUNC(pmath_ref(PMATH_SYMBOL_MAX),
        FUNC(pmath_ref(PMATH_SYMBOL_TOTAL),
          ABS(matrix)));
    }
    
    return FUNC(pmath_ref(PMATH_SYMBOL_TOTAL), ABS(matrix));
  }
  
  // todo: 2-Norm for matrices
  if(_pmath_is_matrix(matrix, &rows, &cols)){
    if(!pmath_equals(ptype, PMATH_FROM_INT32(2))){
      pmath_message(PMATH_NULL, "ptype", 1, pmath_ref(ptype));
    }
    pmath_unref(ptype);
    pmath_unref(matrix);
    return expr;
  }
  
  pmath_unref(expr);
  expr = POW(FUNC(pmath_ref(PMATH_SYMBOL_TOTAL), POW(ABS(matrix), pmath_ref(ptype))), INV(pmath_ref(ptype)));
  
  pmath_unref(ptype);
  return expr;
}
