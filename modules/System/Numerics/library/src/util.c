#include "stdafx.h"
#include "util.h"


extern pmath_symbol_t pmath_System_Complex;
extern pmath_symbol_t pmath_System_Times;

PMATH_PRIVATE
pmath_bool_t pnum_is_imaginary(pmath_t *z) {
  if(pmath_is_expr(*z)) {
    size_t len = pmath_expr_length(*z);
    pmath_t head = pmath_expr_get_item(*z, 0);
    pmath_unref(head);
    
    if(pmath_same(head, pmath_System_Times)) {
      size_t i;
      pmath_t x = pmath_expr_get_item(*z, 1);
      
      if(pmath_is_number(x)) {
        pmath_unref(x);
        return FALSE;
      }
      
      i = 1;
      while(i <= len) {
        if(pmath_is_expr(x)) {
          if(pmath_expr_length(x) > 2) {
            pmath_unref(x);
            return FALSE;
          }
          
          if(pmath_expr_length(x) == 2) {
            head = pmath_expr_get_item(x, 0);
            pmath_unref(head);
            
            if(pmath_same(head, pmath_System_Complex)) {
              pmath_t xx = pmath_expr_get_item(x, 1);
              
              if(pmath_equals(xx, PMATH_FROM_INT32(0))) {
                pmath_unref(xx);
                
                xx = pmath_expr_get_item(x, 2);
                pmath_unref(x);
                *z = pmath_expr_set_item(*z, i, xx);
                
                return TRUE;
              }
              
              pmath_unref(xx);
              pmath_unref(x);
              return FALSE;
            }
          }
        }
        
        pmath_unref(x);
        ++i;
        x = pmath_expr_get_item(*z, i);
      }
      
      pmath_unref(x);
      return FALSE;
    }
    
    if(pmath_same(head, pmath_System_Complex) && len == 2) {
      pmath_t x = pmath_expr_get_item(*z, 1);
      
      if(pmath_equals(x, PMATH_FROM_INT32(0))) {
        pmath_unref(x);
        x = *z;
        *z = pmath_expr_get_item(x, 2);
        pmath_unref(x);
        return TRUE;
      }
      
      pmath_unref(x);
      return FALSE;
    }
  }
  
  return FALSE;
}

PMATH_PRIVATE 
pmath_bool_t pnum_equals_quotient(pmath_t obj, int32_t num, int32_t den) {
  pmath_t i;
  if(!pmath_is_quotient(obj))
    return FALSE;
  
  i = pmath_rational_numerator(obj);
  if(!pmath_is_int32(i)) {
    pmath_unref(i);
    return FALSE;
  }
  if(PMATH_AS_INT32(i) != num)
    return FALSE;
    
  i = pmath_rational_denominator(obj);
  if(!pmath_is_int32(i)) {
    pmath_unref(i);
    return FALSE;
  }
  if(PMATH_AS_INT32(i) != den)
    return FALSE;
  
  return TRUE;
}

PMATH_PRIVATE 
pmath_bool_t pnum_get_small_rational(pmath_t obj, int32_t *num, int32_t *den) {
  assert(num != NULL);
  assert(den != NULL);
  
  *num = 0;
  *den = 1;
  
  if(pmath_is_int32(obj)) {
    *num = PMATH_AS_INT32(obj);
    return TRUE;
  }
  
  if(pmath_is_quotient(obj)) {
    pmath_t num_obj = pmath_rational_numerator(obj);
    pmath_t den_obj = pmath_rational_denominator(obj);
    
    if(pmath_is_int32(num_obj) && pmath_is_int32(den_obj)) {
      *num = PMATH_AS_INT32(num_obj);
      *den = PMATH_AS_INT32(den_obj);
      return TRUE;
    }
    
    pmath_unref(num_obj);
    pmath_unref(den_obj);
  }
  
  return FALSE;
}

PMATH_PRIVATE
pmath_bool_t pnum_contains_nonhead_symbol(pmath_t obj, pmath_symbol_t sub) {
  size_t i;
  pmath_t item;
  pmath_bool_t test;
  
  if(pmath_same(obj, sub))
    return TRUE;
    
  if(pmath_is_packed_array(obj))
    return FALSE;
    
  if(!pmath_is_expr(obj))
    return FALSE;
    
  i = pmath_expr_length(obj);
  while(i > 0) {
    item = pmath_expr_get_item(obj, i);
    test = pnum_contains_nonhead_symbol(item, sub);
    pmath_unref(item);
    if(test)
      return TRUE;
    --i;
  }
  
  item = pmath_expr_get_item(obj, 0);
  test = pnum_contains_nonhead_symbol(item, sub);
  pmath_unref(item);
  
  return test;
}
