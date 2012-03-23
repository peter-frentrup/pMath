#include <pmath-core/numbers.h>
#include <pmath-core/strings-private.h>

#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/lists-private.h>

#include <string.h>

static pmath_t stringdrop(
  pmath_t str,   // will be freed
  pmath_t expr,  // wont be freed
  long    start,
  long    end,
  long    step
) {
  if(pmath_is_string(str)) {
    long len = pmath_string_length(str);
    
    if(start < 0)
      start = len + 1 + start;
      
    if(end < 0)
      end = len + 1 + end;
      
    if(step < 0) {
      long s = start;
      start = end;
      end = s;
      step = -step;
    }
    
    if(start > 0 && end > 0 && step > 0 && start <= end && end <= len) {
      if(end == len && step == 1) {
        return pmath_string_part(str, 0, start - 1);
      }
      else if(start == 1 && step == 1) {
        return pmath_string_part(str, end, -1);
      }
      else {
        const uint16_t *buf = pmath_string_buffer(&str);
        struct _pmath_string_t *s;
        long rem = (end - start) / step + 1;
        
        s = _pmath_new_string_buffer((int)(len - rem));
        if(s) {
          uint16_t *sbuf = AFTER_STRING(s);
          
          if(start > 1)
            memcpy(sbuf, buf, 2 *(start - 1));
            
          if(step > 1) {
            long i = start - 1;
            long j = start - 1;
            
            while(i < end) {
              memcpy(sbuf + j, buf + i + 1, 2 *(step - 1));
              i += step;
              j += step - 1;
            }
          }
          
          if(end < len) {
            end = len - end;
            memcpy(sbuf + len - rem - end, buf + len - end, 2 * end);
          }
        }
        
        pmath_unref(str);
        return _pmath_from_buffer(s);
      }
    }
    else if(start + 1 == end) {
      return str;
    }
    
    pmath_message(PMATH_NULL, "drop", 3,
                  pmath_integer_new_slong(start),
                  pmath_integer_new_slong(end),
                  pmath_ref(str));
                  
    return pmath_expr_set_item(pmath_ref(expr), 1, str);
  }
  
  if(pmath_is_expr_of(str, PMATH_SYMBOL_LIST)) {
    size_t i;
    
    for(i = 1; i <= pmath_expr_length(str); ++i) {
      pmath_t item = pmath_expr_get_item(str, i);
      str = pmath_expr_set_item(str, i, PMATH_NULL);
      
      item = stringdrop(item, expr, start, end, step);
      
      if(pmath_same(item, PMATH_UNDEFINED)) {
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

PMATH_PRIVATE pmath_t builtin_stringdrop(pmath_expr_t expr) {
  pmath_t obj;
  long start, end, step;
  
  if(pmath_expr_length(expr) != 2) {
    pmath_message_argxxx(pmath_expr_length(expr), 2, 2);
    return expr;
  }
  
  obj = pmath_expr_get_item(expr, 2);
  if(!_pmath_extract_longrange(obj, &start, &end, &step)) {
    pmath_unref(obj);
    pmath_message(PMATH_NULL, "seqs", 2, PMATH_FROM_INT32(2), pmath_ref(expr));
    return expr;
  }
  
  pmath_unref(obj);
  obj = pmath_expr_get_item(expr, 1);
  obj = stringdrop(obj, expr, start, end, step);
  
  if(pmath_same(obj, PMATH_UNDEFINED))
    return expr;
    
  pmath_unref(expr);
  return obj;
}
