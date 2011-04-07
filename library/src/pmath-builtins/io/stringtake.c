#include <pmath-core/numbers.h>
#include <pmath-core/strings-private.h>

#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/lists-private.h>


static pmath_t stringtake(
  pmath_t str,   // will be freed
  pmath_t expr,  // wont be freed
  long    start, 
  long    end, 
  long    step
){
  if(pmath_is_string(str)){
    long len = pmath_string_length(str);
    
    if(start < 0)
      start = len + 1 + start;
    
    if(end < 0)
      end = len + 1 + end;
    
    if(step < 0){
      long s = start;
      start = end;
      end = s;
      step = -step;
    }
    
    if(start > 0 && end > 0 && step > 0 && start <= end && end <= len){
      if(step > 1){
        const uint16_t *buf = pmath_string_buffer(str);
        struct _pmath_string_t *s;
        long newlen = (end - start)/step + 1;
        
        s = _pmath_new_string_buffer((int)newlen);
        if(s){
          uint16_t *sbuf = AFTER_STRING(s);
          long i = 0;
          
          --start;
          while(start < end){
            sbuf[i] = buf[start];
            ++i;
            start+= step;
          }
        }
        
        pmath_unref(str);
        return _pmath_from_buffer(s);
      }
      else
        return pmath_string_part(str, start - 1, end - start + 1);
    }
    
    pmath_message(PMATH_NULL, "take", 3,
      pmath_integer_new_slong(start),
      pmath_integer_new_slong(end),
      pmath_ref(str));
    
    return pmath_expr_set_item(pmath_ref(expr), 1, str);
  }
  
  if(pmath_is_expr_of(str, PMATH_SYMBOL_LIST)){
    size_t i;
    
    for(i = 1;i <= pmath_expr_length(str);++i){
      pmath_t item = pmath_expr_get_item(str, i);
      str = pmath_expr_set_item(str, i, PMATH_NULL);
      
      item = stringtake(item, expr, start, end, step);
      
      if(pmath_same(item, PMATH_UNDEFINED)){
        pmath_unref(str);
        return PMATH_UNDEFINED;
      }
      
      str = pmath_expr_set_item(str, i, item);
    }
    
    return str;
  }
  
  pmath_message(PMATH_NULL, "strse", 2,
    PMATH_FROM_INT32(1),
    pmath_ref(expr));
  
  pmath_unref(str);
  return PMATH_UNDEFINED;
}

PMATH_PRIVATE pmath_t builtin_stringtake(pmath_expr_t expr){
  pmath_t obj;
  long start, end, step;
  
  if(pmath_expr_length(expr) != 2){
    pmath_message_argxxx(pmath_expr_length(expr), 2, 2);
    return expr;
  }
  
  obj = pmath_expr_get_item(expr, 2);
  if(!_pmath_extract_longrange(obj, &start, &end, &step)){
    pmath_unref(obj);
    pmath_message(PMATH_NULL, "seqs", 2, PMATH_FROM_INT32(2), pmath_ref(expr));
    return expr;
  }
  
  pmath_unref(obj);
  obj = pmath_expr_get_item(expr, 1);
  obj = stringtake(obj, expr, start, end, step);
  
  if(pmath_same(obj, PMATH_UNDEFINED))
    return expr;
  
  pmath_unref(expr);
  return obj;
}
