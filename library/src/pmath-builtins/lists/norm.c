#include <pmath-util/helpers.h>
#include <pmath-core/numbers-private.h>

#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/build-expr-private.h>
#include <pmath-builtins/lists-private.h>
#include <pmath-builtins/number-theory-private.h>


extern pmath_symbol_t pmath_System_Abs;
extern pmath_symbol_t pmath_System_Map;
extern pmath_symbol_t pmath_System_Max;
extern pmath_symbol_t pmath_System_Power;
extern pmath_symbol_t pmath_System_Total;

PMATH_PRIVATE pmath_t builtin_norm(pmath_expr_t expr) {
  size_t exprlen = pmath_expr_length(expr);
  pmath_t matrix;
  pmath_t ptype;
  size_t rows, cols;
  int ptype_class;
  
  if(exprlen < 1 || exprlen > 2) {
    pmath_message_argxxx(exprlen, 1, 2);
    return expr;
  }
  
  if(exprlen == 2)
    ptype = pmath_expr_get_item(expr, 2);
  else
    ptype = PMATH_FROM_INT32(2);
    
  matrix = pmath_expr_get_item(expr, 1);
  
  if( pmath_is_string(ptype) &&
      pmath_string_equals_latin1(ptype, "Frobenius"))
  {
    pmath_unref(ptype);
    
    if(_pmath_is_vector(matrix)) {
      pmath_unref(expr);
      return SQRT(FUNC(pmath_ref(pmath_System_Total), POW(ABS(matrix), INT(2))));
    }
    
    if(_pmath_is_matrix(matrix, &rows, &cols, TRUE)) {
      pmath_unref(expr);
      return SQRT(FUNC(pmath_ref(pmath_System_Total),
                       FUNC(pmath_ref(pmath_System_Total), POW(ABS(matrix), INT(2)))));
    }
    
    pmath_unref(matrix);
    return expr;
  }
  
  ptype_class = _pmath_number_class(ptype);
  if((ptype_class & ~(PMATH_CLASS_POSONE | PMATH_CLASS_POSBIG | PMATH_CLASS_POSINF | PMATH_CLASS_UNKNOWN))) {
    pmath_unref(matrix);
    pmath_message(PMATH_NULL, "ptype", 1, ptype);
    return expr;
  }
  
  if(_pmath_is_matrix(matrix, &rows, &cols, TRUE)) {
    if(ptype_class == PMATH_CLASS_POSINF) {
      pmath_unref(ptype);
      pmath_unref(expr);
      
      return FUNC(pmath_ref(pmath_System_Max),
                  FUNC2(pmath_ref(pmath_System_Map),
                        ABS(matrix),
                        pmath_ref(pmath_System_Total)));
    }
    
    if(ptype_class == PMATH_CLASS_POSONE) {
      pmath_unref(ptype);
      pmath_unref(expr);
      
      return FUNC(pmath_ref(pmath_System_Max),
                  FUNC(pmath_ref(pmath_System_Total),
                       ABS(matrix)));
    }
    
    // todo: 2-Norm for matrices
    if(!pmath_equals(ptype, PMATH_FROM_INT32(2))) {
      pmath_message(PMATH_NULL, "ptype", 1, pmath_ref(ptype));
    }
    pmath_unref(ptype);
    pmath_unref(matrix);
    return expr;
  }
  
  if(!_pmath_is_vector(matrix)) {
    pmath_unref(matrix);
    pmath_unref(ptype);
    return expr;
  }
  
  if(ptype_class == PMATH_CLASS_POSINF) {
    pmath_unref(ptype);
    pmath_unref(expr);
    
    return FUNC(pmath_ref(pmath_System_Max), ABS(matrix));
  }
  
  if(ptype_class == PMATH_CLASS_POSONE) {
    pmath_unref(ptype);
    pmath_unref(expr);
    
    return FUNC(pmath_ref(pmath_System_Total), ABS(matrix));
  }
  
  // special casing Norm({a,b}) = Sqrt(a^2 + b^2) for double values
  // to circumvalent numerical underflow/owerflow
  if( pmath_equals(ptype, PMATH_FROM_INT32(2)) &&
      pmath_expr_length(matrix) == 2)
  {
    pmath_t a = pmath_expr_get_item(matrix, 1);
    pmath_t b = pmath_expr_get_item(matrix, 2);
    
    if(pmath_is_double(a) && pmath_is_double(b)) {
      double x = fabs(PMATH_AS_DOUBLE(a));
      double y = fabs(PMATH_AS_DOUBLE(b));
      double z;
      
      if(x > y) {
        z = y / x;
        z = x * sqrt(1 + z * z);
      }
      else {
        z = x / y;
        z = y * sqrt(1 + z * z);
      }
      
      if(isfinite(z)) {
        pmath_unref(ptype);
        pmath_unref(matrix);
        pmath_unref(expr);
        
        return PMATH_FROM_DOUBLE(z);
      }
    }
    
    pmath_unref(a);
    pmath_unref(b);
  }
  
  pmath_unref(expr);
  expr = POW(FUNC(pmath_ref(pmath_System_Total), POW(ABS(matrix), pmath_ref(ptype))), INV(pmath_ref(ptype)));
  
  pmath_unref(ptype);
  return expr;
}
