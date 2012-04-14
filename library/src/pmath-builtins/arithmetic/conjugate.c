#include <pmath-core/expressions-private.h>
#include <pmath-core/numbers-private.h>

#include <pmath-util/emit-and-gather.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/build-expr-private.h>
#include <pmath-builtins/number-theory-private.h>

static pmath_bool_t try_conjugate(pmath_t *z);

static pmath_t force_conjugate(pmath_t z){
  if(try_conjugate(&z))
    return z;
  
  return pmath_expr_new_extended(
    pmath_ref(PMATH_SYMBOL_CONJUGATE), 1,
    z);
}

static pmath_bool_t try_conjugate(pmath_t *z) {
  int z_class;
  
  if(pmath_is_number(*z)) // complex numbers are expressions
    return TRUE;
  
  z_class = _pmath_number_class(*z);
  
  if(z_class & (PMATH_CLASS_REAL | PMATH_CLASS_RINF))
    return TRUE;
    
  if(z_class & PMATH_CLASS_IMAGINARY) {
    *z = NEG(*z);
    return TRUE;
  }
  
  if( pmath_is_expr_of_len(*z, PMATH_SYMBOL_RE, 1) ||
      pmath_is_expr_of_len(*z, PMATH_SYMBOL_IM, 1) ||
      pmath_is_expr_of_len(*z, PMATH_SYMBOL_ABS, 1))
  {
    return TRUE;
  }
  
  if(pmath_is_expr_of_len(*z, PMATH_SYMBOL_COMPLEX, 2)) {
    pmath_t im = pmath_expr_get_item(*z, 2);
    *z = pmath_expr_set_item(*z, 2, NEG(im));
    return TRUE;
  }
  
  if( pmath_is_expr_of(*z, PMATH_SYMBOL_PLUS) ||
      pmath_is_expr_of(*z, PMATH_SYMBOL_TIMES))
  {
    size_t i;
    size_t len = pmath_expr_length(*z);
    
    for(i = len; i > 0; --i) {
      pmath_t zi = pmath_expr_get_item(*z, i);
      
      if(try_conjugate(&zi)) {
        pmath_t unevaluated;
        size_t stop = i;
        size_t last_unevaluated = 0;
        
        *z = pmath_expr_set_item(*z, i, zi);
        
        pmath_gather_begin(PMATH_NULL);
        
        for(i = 1; i < stop; ++i) {
          zi = pmath_expr_extract_item(*z, i);
          
          if(!try_conjugate(&zi)) {
            last_unevaluated = i;
            pmath_emit(zi, PMATH_NULL);
            zi = PMATH_UNDEFINED;
          }
          
          *z = pmath_expr_set_item(*z, i, zi);
        }
        
        for(i = stop + 1; i <= len; ++i) {
          zi = pmath_expr_extract_item(*z, i);
          pmath_emit(zi, PMATH_NULL);
          *z = pmath_expr_set_item(*z, i, PMATH_UNDEFINED);
        }
        
        if(stop < len)
          last_unevaluated = stop + 1;
          
        unevaluated = pmath_gather_end();
        if(pmath_expr_length(unevaluated) > 0) {
          if(pmath_expr_length(unevaluated) > 1) {
            unevaluated = pmath_expr_set_item(
                            unevaluated, 0,
                            pmath_expr_get_item(*z, 0));
                            
            unevaluated = pmath_expr_new_extended(
                            pmath_ref(PMATH_SYMBOL_CONJUGATE), 1,
                            unevaluated);
          }
          else
            unevaluated = pmath_expr_set_item(
                            unevaluated, 0,
                            pmath_ref(PMATH_SYMBOL_CONJUGATE));
                            
          *z = pmath_expr_set_item(*z, last_unevaluated, unevaluated);
          *z = _pmath_expr_shrink_associative(*z, PMATH_UNDEFINED);
        }
        else
          pmath_unref(unevaluated);
          
        return TRUE;
      }
      
      pmath_unref(zi);
    }
  }
  
  if(pmath_is_expr_of_len(*z, PMATH_SYMBOL_POWER, 2)){
    pmath_t exponent = pmath_expr_get_item(*z, 2);
    
    if(pmath_is_integer(exponent)){
      pmath_t base = pmath_expr_extract_item(*z, 1);
      
      base = force_conjugate(base);
      
      *z = pmath_expr_set_item(*z, 1, base);
      pmath_unref(exponent);
      return TRUE;
    }
    
    pmath_unref(exponent);
  }
  
  if(pmath_is_expr_of(*z, PMATH_SYMBOL_DIRECTEDINFINITY)) {
    if(pmath_expr_length(*z) == 1) {
      pmath_t dir = pmath_expr_extract_item(*z, 1);
      
      if(!try_conjugate(&dir)) {
        dir = pmath_expr_new_extended(
                pmath_ref(PMATH_SYMBOL_CONJUGATE), 1,
                dir);
      }
      
      *z = pmath_expr_set_item(*z, 1, dir);
      return TRUE;
    }
    else if(pmath_expr_length(*z) == 0)
      return TRUE;
  }
  
  if( pmath_equals(*z, _pmath_object_overflow) ||
      pmath_equals(*z, _pmath_object_underflow))
  {
    return TRUE;
  }
  
  return FALSE;
}


PMATH_PRIVATE pmath_t builtin_conjugate(pmath_expr_t expr) {
  pmath_t z;
  
  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  z = pmath_expr_get_item(expr, 1);
  if(try_conjugate(&z)) {
    pmath_unref(expr);
    return z;
  }
  
  pmath_unref(z);
  return expr;
}
