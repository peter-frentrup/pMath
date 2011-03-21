#include <pmath-core/numbers.h>

#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>

PMATH_PRIVATE pmath_t builtin_riffle(pmath_expr_t expr){
/* Riffle({e1, e2, ...}, x)
   Riffle({e1, e2, ...}, {x1, x2, ...})
   Riffle(list, x, n)
   Riffle(list, x, imin..imax..n)
 */
  intptr_t start, end, step, listlen, extra, i;
  pmath_expr_t list, xs;
  
  if(pmath_expr_length(expr) < 2 || pmath_expr_length(expr) > 3){
    pmath_message_argxxx(pmath_expr_length(expr), 2, 3);
    return expr;
  }
  
  list = pmath_expr_get_item(expr, 1);
  
  if(!pmath_is_expr_of(list, PMATH_SYMBOL_LIST)){
    pmath_unref(list);
    return expr;
  }
  
  listlen = pmath_expr_length(list);
  start = 2;
  end   = -2;
  step  = 2;
  
  xs = pmath_expr_get_item(expr, 2);
  if(!pmath_is_expr_of(xs, PMATH_SYMBOL_LIST)){
    xs = pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_LIST), 1, xs);
  }
  else if(pmath_expr_length(xs) == 0){
    pmath_unref(expr);
    pmath_unref(xs);
    return list;
  }  
  else if(pmath_expr_length(xs) == pmath_expr_length(list))
    end = -1;
  
  if(pmath_expr_length(expr) >= 3){
    pmath_t obj = pmath_expr_get_item(expr, 3);
    end = -2;
    
    if(pmath_is_integer(obj) && pmath_integer_fits_siptr(obj)){
      step = pmath_integer_get_siptr(obj);
      if(step < 0){
        end = step; start = 2;
      }
      else{
        start = step; end = -2;
      }
    }
    else if(pmath_is_expr_of(obj, PMATH_SYMBOL_RANGE)){
      pmath_t start_obj, end_obj, step_obj;
      start_obj = pmath_expr_get_item(obj, 1);
      end_obj   = pmath_expr_get_item(obj, 2);
      step_obj  = pmath_expr_get_item(obj, 3);
      
      if(pmath_is_integer(start_obj) && pmath_integer_fits_siptr(start_obj)
      && pmath_is_integer(end_obj)   && pmath_integer_fits_siptr(end_obj)){
        start = pmath_integer_get_siptr(start_obj);
        end   = pmath_integer_get_siptr(end_obj);
      }
      else if(!pmath_same(obj, PMATH_SYMBOL_ALL)){
        pmath_unref(start_obj);
        pmath_unref(end_obj);
        pmath_unref(step_obj);
        pmath_unref(obj);
        pmath_unref(list);
        pmath_unref(xs);
        pmath_message(PMATH_NULL, "seqs", 2, PMATH_FROM_INT32(3), pmath_ref(expr));
        return expr;
      }
    
      if(pmath_expr_length(obj) == 3
      && pmath_is_integer(step_obj) && pmath_integer_fits_siptr(step_obj)){
        step = pmath_integer_get_siptr(step_obj);
      }
      else if(pmath_expr_length(obj) != 2){
        pmath_unref(start_obj);
        pmath_unref(end_obj);
        pmath_unref(step_obj);
        pmath_unref(obj);
        pmath_unref(list);
        pmath_unref(xs);
        pmath_message(PMATH_NULL, "seqs", 2, PMATH_FROM_INT32(3), pmath_ref(expr));
        return expr;
      }
      
      pmath_unref(start_obj);
      pmath_unref(end_obj);
      pmath_unref(step_obj);
    }
    else if(!pmath_same(obj, PMATH_SYMBOL_ALL)){
      pmath_unref(obj);
      pmath_unref(list);
      pmath_unref(xs);
      pmath_message(PMATH_NULL, "seqs", 2, PMATH_FROM_INT32(3), pmath_ref(expr));
      return expr;
    }
    
    pmath_unref(obj);
  }
  
  if(start < 0)
    start = listlen + 2 + start;
  
  if(end < 0)
    end = listlen + 2 + end;
  
  if((step < 0) != (start > end)){
    pmath_unref(expr);
    pmath_unref(xs);
    return list;
  }
  
  if(step < -1){
    step = -step;
    start = end - (step-1) * ((start - end) / (step-1));
  }
  else if(step > 1){
    end = start + (step-1) * ((end - start) / (step-1));
  }
  
  if(step < 2 || end < start || start < 1 || end > listlen + 1){
    pmath_unref(expr);
    pmath_unref(xs);
    return list;
  }
  
  if(step < 2 || end < start || start < 1 || end > listlen + 1){
    pmath_unref(expr);
    pmath_unref(xs);
    return list;
  }
  
  extra = (end - start) / (step-1) + 1;
  pmath_unref(expr);
  list = pmath_expr_resize(list, listlen + extra);
  for(i = 0;i < extra;++i){
    intptr_t j, k;
    j = end - 1 - (step-1) * i;
    
    for(k = listlen;k > j;--k){
      list = pmath_expr_set_item(list, k + extra - i, pmath_expr_get_item(list, k));
    }
    
    listlen = j;
    list = pmath_expr_set_item(list, j + extra - i, 
      pmath_expr_get_item(xs, (extra - i - 1) % pmath_expr_length(xs) + 1));
  }
  
  pmath_unref(xs);
  return list;
}
