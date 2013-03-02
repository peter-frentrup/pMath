#include <pmath-core/numbers-private.h>

#include <pmath-util/approximate.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/arithmetic-private.h>
#include <pmath-builtins/build-expr-private.h>
#include <pmath-builtins/lists-private.h>
#include <pmath-builtins/number-theory-private.h>


PMATH_PRIVATE
pmath_bool_t _pmath_is_imaginary(
  pmath_t *z
) {
  if(pmath_is_expr(*z)) {
    size_t len = pmath_expr_length(*z);
    pmath_t head = pmath_expr_get_item(*z, 0);
    pmath_unref(head);
    
    if(pmath_same(head, PMATH_SYMBOL_TIMES)) {
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
            
            if(pmath_same(head, PMATH_SYMBOL_COMPLEX)) {
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
    
    if(pmath_same(head, PMATH_SYMBOL_COMPLEX) && len == 2) {
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

/*static pmath_expr_t extract_complex(
  pmath_expr_t *expr
) { // returns PMATH_NULL if there is no nonreal complex
  size_t i, len;

  len = pmath_expr_length(*expr);

  for(i = len; i > 0; --i) {
    pmath_t item = pmath_expr_get_item(*expr, i);

    if(_pmath_is_nonreal_complex(item)) {
      pmath_expr_t tmp = *expr;

      for(; i < len; ++i) {
        tmp = pmath_expr_set_item(tmp, i,
                                  pmath_expr_get_item(tmp, i + 1));
      }

      *expr = pmath_expr_get_item_range(tmp, 1, len - 1);

      pmath_unref(tmp);

      return item;
    }

    pmath_unref(item);
  }

  return PMATH_NULL;
}*/

PMATH_PRIVATE pmath_bool_t _pmath_re_im(
  pmath_t  z,   // will be freed
  pmath_t *re,  // optional output
  pmath_t *im   // optional output
) {
  pmath_t z2;
  
  if(re) *re = PMATH_NULL;
  if(im) *im = PMATH_NULL;
  
  if( pmath_same(z, PMATH_SYMBOL_UNDEFINED)    ||
      pmath_equals(z, _pmath_object_overflow)  ||
      pmath_equals(z, _pmath_object_underflow) ||
      pmath_is_number(z))
  {
    if(re) *re = z;
    if(im) *im = PMATH_FROM_INT32(0);
    return TRUE;
  }
  
  if(pmath_is_expr(z)) {
    pmath_t zhead = pmath_expr_get_item(z, 0);
    pmath_unref(zhead);
    
    if( pmath_expr_length(z) == 1 &&
        (pmath_same(zhead, PMATH_SYMBOL_RE) || pmath_same(zhead, PMATH_SYMBOL_IM)))
    {
      if(re) *re = z;
      if(im) *im = PMATH_FROM_INT32(0);
      return TRUE;
    }
    
    if( pmath_expr_length(z) == 2 &&
        pmath_same(zhead, PMATH_SYMBOL_COMPLEX))
    {
      if(re) *re = pmath_expr_get_item(z, 1);
      if(im) *im = pmath_expr_get_item(z, 2);
      pmath_unref(z);
      return TRUE;
    }
    
    if(pmath_same(zhead, PMATH_SYMBOL_PLUS)) {
      size_t i, j;
      
      for(j = 0, i = pmath_expr_length(z); i > 0; --i) {
        pmath_t re2, im2;
        
        if( _pmath_re_im(pmath_expr_get_item(z, i), &re2, &im2) &&
            !_pmath_contains_symbol(re2, PMATH_SYMBOL_RE) &&
            !_pmath_contains_symbol(re2, PMATH_SYMBOL_IM) &&
            !_pmath_contains_symbol(im2, PMATH_SYMBOL_RE) &&
            !_pmath_contains_symbol(im2, PMATH_SYMBOL_IM))
        {
          z = pmath_expr_set_item(z, i, PMATH_UNDEFINED);
          
          if(j == 0) {
            if(re) *re = pmath_expr_new(pmath_ref(PMATH_SYMBOL_PLUS), i);
            if(im) *im = pmath_expr_new(pmath_ref(PMATH_SYMBOL_PLUS), i);
          }
          
          ++j;
          
          if(re) *re = pmath_expr_set_item(*re, j, pmath_ref(re2));
          if(im) *im = pmath_expr_set_item(*im, j, pmath_ref(im2));
        }
        
        pmath_unref(re2);
        pmath_unref(im2);
      }
      
      if(j == pmath_expr_length(z)) {
        pmath_unref(z);
        return TRUE;
      }
      
      if(j > 0) {
        z = pmath_expr_remove_all(z, PMATH_UNDEFINED);
        
        if(re) {
          z2 = pmath_expr_get_item_range(*re, 1, j);
          
          pmath_unref(*re);
          
          *re = PLUS(z2, FUNC(pmath_ref(PMATH_SYMBOL_RE), pmath_ref(z)));
        }
        
        if(im) {
          z2 = pmath_expr_get_item_range(*im, 1, j);
          
          pmath_unref(*im);
          
          *im = PLUS(z2, FUNC(pmath_ref(PMATH_SYMBOL_IM), pmath_ref(z)));
        }
        
        pmath_unref(z);
        return TRUE;
      }
      
      if(re) { pmath_unref(*re); *re = PMATH_NULL; }
      if(im) { pmath_unref(*im); *im = PMATH_NULL; }
      
      pmath_unref(z);
      return FALSE;
    }
    
    if(pmath_same(zhead, PMATH_SYMBOL_TIMES) && pmath_expr_length(z) > 0) {
      pmath_t re2 = INT(1);
      pmath_t im2 = INT(0);
      size_t i;
      size_t zlen = pmath_expr_length(z);
      
      for(i = 1; i <= zlen; ++i) {
        pmath_t zi = pmath_expr_get_item(z, i);
        pmath_t re_zi, im_zi;
        pmath_t tmp_re, tmp_im;
        
        if(!_pmath_re_im(zi, &re_zi, &im_zi) ||
            _pmath_contains_symbol(re_zi, PMATH_SYMBOL_RE) ||
            _pmath_contains_symbol(re_zi, PMATH_SYMBOL_IM) ||
            _pmath_contains_symbol(im_zi, PMATH_SYMBOL_RE) ||
            _pmath_contains_symbol(im_zi, PMATH_SYMBOL_IM))
        {
          pmath_unref(re_zi);
          pmath_unref(im_zi);
          
          if(i > 1) {
            if(pmath_equals(re2, INT(0))) {
              // Re(a I * z) = -a Im(z)
              // Im(a I * z) =  a Re(z)
              
              if(re) {
                *re = TIMES3(
                        INT(-1),
                        pmath_ref(im2),
                        FUNC(pmath_ref(PMATH_SYMBOL_IM), pmath_expr_get_item_range(z, i, SIZE_MAX)));
              }
              
              if(im) {
                *im = TIMES(
                        pmath_ref(im2),
                        FUNC(pmath_ref(PMATH_SYMBOL_RE), pmath_expr_get_item_range(z, i, SIZE_MAX)));
              }
              
              pmath_unref(re2);
              pmath_unref(im2);
              pmath_unref(z);
              return TRUE;
            }
            
            if(pmath_equals(im2, INT(0))) {
              // Re(a * z) = a Re(z)
              // Im(a * z) = a Im(z)
              
              if(re) {
                *re = TIMES(
                        pmath_ref(re2),
                        FUNC(pmath_ref(PMATH_SYMBOL_RE), pmath_expr_get_item_range(z, i, SIZE_MAX)));
              }
              
              if(im) {
                *im = TIMES(
                        pmath_ref(re2),
                        FUNC(pmath_ref(PMATH_SYMBOL_IM), pmath_expr_get_item_range(z, i, SIZE_MAX)));
              }
              
              pmath_unref(re2);
              pmath_unref(im2);
              pmath_unref(z);
              return TRUE;
            }
          }
          
          pmath_unref(re2);
          pmath_unref(im2);
          pmath_unref(z);
          return FALSE;
        }
        
        tmp_re = MINUS(TIMES(pmath_ref(re2), pmath_ref(re_zi)), TIMES(pmath_ref(im2), pmath_ref(im_zi)));
        tmp_im = PLUS( TIMES(pmath_ref(re2), pmath_ref(im_zi)), TIMES(pmath_ref(im2), pmath_ref(re_zi)));
        
        pmath_unref(re2);
        pmath_unref(im2);
        pmath_unref(re_zi);
        pmath_unref(im_zi);
        
        re2 = pmath_evaluate(tmp_re);
        im2 = pmath_evaluate(tmp_im);
      }
      
      if(re) *re = pmath_ref(re2);
      if(im) *im = pmath_ref(im2);
      
      pmath_unref(re2);
      pmath_unref(im2);
      pmath_unref(z);
      return TRUE;
    }
    
    {
      pmath_t zinfdir = _pmath_directed_infinity_direction(z);
      if(!pmath_is_null(zinfdir)) {
        pmath_unref(z);
        if(re) *re = pmath_expr_new_extended(
                         pmath_ref(PMATH_SYMBOL_DIRECTEDINFINITY), 1,
                         pmath_expr_new_extended(
                           pmath_ref(PMATH_SYMBOL_RE), 1,
                           pmath_ref(zinfdir)));
                           
        if(im) *im = pmath_expr_new_extended(
                         pmath_ref(PMATH_SYMBOL_DIRECTEDINFINITY), 1,
                         pmath_expr_new_extended(
                           pmath_ref(PMATH_SYMBOL_IM), 1,
                           pmath_ref(zinfdir)));
                           
        pmath_unref(zinfdir);
        return TRUE;
      }
    }
  }
  
  if(!pmath_is_numeric(z)) {
    pmath_unref(z);
    return FALSE;
  }
  
  z2 = pmath_approximate(pmath_ref(z), -HUGE_VAL, -HUGE_VAL, NULL);
  
  if(pmath_is_number(z2)) {
    pmath_unref(z2);
    
    if(re) *re = pmath_ref(z);
    if(im) *im = PMATH_FROM_INT32(0);
    
    pmath_unref(z);
    return TRUE;
  }
  
  pmath_unref(z2);
  pmath_unref(z);
  return FALSE;
}

PMATH_PRIVATE pmath_bool_t _pmath_is_nonreal_complex(
  pmath_t z
) {
  pmath_t re, im;
  pmath_bool_t both_numbers;
  
  if(!pmath_is_expr_of_len(z, PMATH_SYMBOL_COMPLEX, 2))
    return FALSE;
    
  re = pmath_expr_get_item(z, 1);
  im = pmath_expr_get_item(z, 2);
  
  both_numbers = pmath_is_number(re) && pmath_is_number(im);
  
  pmath_unref(re);
  pmath_unref(im);
  return both_numbers;
}

PMATH_PRIVATE pmath_t builtin_complex(pmath_expr_t expr) {
  pmath_t x;
  if(pmath_expr_length(expr) != 2) {
    pmath_message_argxxx(pmath_expr_length(expr), 2, 2);
    return expr;
  }
  
  x = pmath_expr_get_item(expr, 2);
  if(pmath_equals(x, PMATH_FROM_INT32(0))) {
    pmath_unref(x);
    x = pmath_expr_get_item(expr, 1);
    pmath_unref(expr);
    return x;
  }
  
  pmath_unref(x);
  return expr;
}

PMATH_PRIVATE pmath_t builtin_re(pmath_expr_t expr) {
  pmath_t re, z;
  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  z = pmath_expr_get_item(expr, 1);
  if(!_pmath_re_im(z, &re, NULL))
    return expr;
    
  pmath_unref(expr);
  return re;
}

PMATH_PRIVATE pmath_t builtin_im(pmath_expr_t expr) {
  pmath_t im, z;
  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  z = pmath_expr_get_item(expr, 1);
  if(!_pmath_re_im(z, NULL, &im))
    return expr;
    
  pmath_unref(expr);
  return im;
}
