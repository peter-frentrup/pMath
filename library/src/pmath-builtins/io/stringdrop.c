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

#include <pmath-util/concurrency/atomic.h>
#include <pmath-util/helpers.h>
#include <pmath-util/memory.h>
#include <pmath-util/messages.h>

#include <pmath-core/objects-inline.h>
#include <pmath-core/objects-private.h>
#include <pmath-core/strings-private.h>

#include <pmath-builtins/all-symbols.h>
#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/lists-private.h>

static pmath_t stringdrop(
  pmath_t str,   // will be freed
  pmath_t expr,  // wont be freed
  long           start, 
  long           end, 
  long           step
){
  if(pmath_instance_of(str, PMATH_TYPE_STRING)){
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
      if(end == len && step == 1){
        return pmath_string_part(str, start-1, -1);
      }
      else if(start == 1 && step == 1){
        return pmath_string_part(str, 0, end);
      }
      else{
        const uint16_t *buf = pmath_string_buffer(str);
        struct _pmath_string_t *s;
        long rem = (end - start)/step + 1;
        
        s = _pmath_new_string_buffer((int)(len - rem));
        if(s){
          uint16_t *sbuf = AFTER_STRING(s);
          
          if(start > 1)
            memcpy(sbuf, buf, 2 * (start - 1));
          
          if(step > 1){
            long i = start - 1;
            long j = start - 1;
            
            while(i < end){
              memcpy(sbuf + j, buf + i + 1, 2 * (step - 1));
              i+= step;
              j+= step-1;
            }
          }
          
          if(end < len){
            end = len - end;
            memcpy(sbuf + len - rem - end, buf + len - end, 2 * end);
          }
        }
        
        pmath_unref(str);
        return (pmath_string_t)s;
      }
    }
    
    pmath_message(NULL, "drop", 3,
      pmath_integer_new_si(start),
      pmath_integer_new_si(end),
      pmath_ref(str));
    
    return pmath_expr_set_item(pmath_ref(expr), 1, str);
  }
  
  if(pmath_is_expr_of(str, PMATH_SYMBOL_LIST)){
    size_t i;
    
    for(i = 1;i <= pmath_expr_length(str);++i){
      pmath_t item = pmath_expr_get_item(str, i);
      str = pmath_expr_set_item(str, i, NULL);
      
      item = stringdrop(str, expr, start, end, step);
      
      if(item == PMATH_UNDEFINED){
        pmath_unref(str);
        return PMATH_UNDEFINED;
      }
      
      str = pmath_expr_set_item(str, i, item);
    }
  }
  
  pmath_message(NULL, "strse", 2,
    pmath_integer_new_si(1),
    pmath_ref(expr));
  
  pmath_unref(str);
  return PMATH_UNDEFINED;
}

PMATH_PRIVATE pmath_t builtin_stringdrop(pmath_expr_t expr){
  pmath_t obj;
  long start, end, step;
  
  if(pmath_expr_length(expr) != 2){
    pmath_message_argxxx(pmath_expr_length(expr), 2, 2);
    return expr;
  }
  
  obj = pmath_expr_get_item(expr, 2);
  if(!_pmath_extract_longrange(obj, &start, &end, &step)){
    pmath_unref(obj);
    pmath_message(NULL, "seqs", 2, pmath_integer_new_si(2), pmath_ref(expr));
    return expr;
  }
  
  pmath_unref(obj);
  obj = pmath_expr_get_item(expr, 1);
  obj = stringdrop(obj, expr, start, end, step);
  
  if(obj == PMATH_UNDEFINED)
    return expr;
  
  pmath_unref(expr);
  return obj;
}
