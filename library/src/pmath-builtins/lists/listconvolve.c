#include <pmath-core/numbers.h>

#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>

static intptr_t umod(intptr_t i, intptr_t m) {
  assert(m > 0);
  
  if(i < 0)
    return (uintptr_t)((i % m) + m);
    
  return (uintptr_t)(i % m);
}

// Zero based index i
// Use pad == list  for cyclic access to list.
static pmath_t padded_list_get_0(pmath_expr_t list, intptr_t i, pmath_t pad) {

  intptr_t len;
  
  len = (intptr_t)pmath_expr_length(list);
  
  if(0 <= i && i < len)
    return pmath_expr_get_item(list, (size_t)i + 1);
  
  if(!pmath_is_expr_of(pad, PMATH_SYMBOL_LIST))
    return pmath_ref(pad);
  
  len = (intptr_t)pmath_expr_length(pad);
  if(len == 0)
    return PMATH_NULL;
    
  return pmath_expr_get_item(pad, umod(i, len) + 1);
}


PMATH_PRIVATE pmath_t builtin_listconvolve(pmath_expr_t expr) {
  /* ListConvolve(list, ker)            ==  ListConvolve(list, ker, {-1, 1})
     ListConvolve(list, ker, k)         ==  ListConvolve(list, ker, {k,  k})
     ListConvolve(list, ker, {kL, kR})  ==  ListConvolve(list, ker, {kL, kR})
  
     ListConvolve(list, ker, klist, padlist, g, h). All but the first two
     arguments are successively optional. g is used in place of Times, h in place
     of Plus.
  
  
     Let S:=Length(list), R:=Length(ker) . The result is a list of
  
          R-1
         .---.
          >    ker[r+1] * A[s-r+1]   for some integer indices s
         '---'
          r=0
  
     such that the result's start contains ker[kL]*A[1] and its end contains
     ker[kR]*A[-1], where A is an infinite sequence of repeated copies of padlist
     (ranging from -Infinity to +Infinity), superimposed with list at index 1.
  
     kL nd kR must be nonzero integers. -1 is treaded as R, -2 as R-1, ...,
     -R+1 as 1.
  
     At the moment, this function works in one dimension only.
  
  
     Messages:
      ListConvolve::kldims: The kernel `1` and list `2` are not both nonempty
                            lists.
                            [with the same tensor rank]
      ListConvolve::ohp:    Overhang parameters in `1` must be nonzero machine-
                            size integers.
                            [or lists of nonzero machine-size integers]
      ListConvolve::nlen:   The end conditions cannot be satisfied. A zero 
                            length list will be returned.
   */
  size_t exprlen;
  pmath_t list, ker, pad, times, plus;
  intptr_t kL, kR, start, end, S, R;
  
  exprlen = pmath_expr_length(expr);
  if(exprlen < 2 || exprlen > 6) {
    pmath_message_argxxx(exprlen, 2, 6);
    return expr;
  }
  
  list = pmath_expr_get_item(expr, 1);
  ker  = pmath_expr_get_item(expr, 2);
  
  if( !pmath_is_expr_of(list, PMATH_SYMBOL_LIST) ||
      !pmath_is_expr_of(ker,  PMATH_SYMBOL_LIST))
  {
    pmath_message(PMATH_NULL, "kldims", 2, ker, list);
    return expr;
  }
  
  if(exprlen >= 3) {
    pmath_t k_obj = pmath_expr_get_item(expr, 3);
    
    do { /* once */
    
      if(pmath_is_int32(k_obj)) {
        kL = kR = PMATH_AS_INT32(k_obj);
        
        if(kL != 0)
          break;
      }
      
      if(pmath_is_expr_of_len(k_obj, PMATH_SYMBOL_LIST, 2)) {
        pmath_t kL_obj = pmath_expr_get_item(k_obj, 1);
        pmath_t kR_obj = pmath_expr_get_item(k_obj, 2);
        
        if( pmath_is_int32(kL_obj) &&
            pmath_is_int32(kR_obj))
        {
          kL = PMATH_AS_INT32(kL_obj);
          kR = PMATH_AS_INT32(kR_obj);
          
          if(kL != 0 && kR != 0){
            pmath_unref(k_obj);
            break;
          }
        }
      }
      
      pmath_unref(list);
      pmath_unref(ker);
      pmath_message(PMATH_NULL, "ohp", 1, k_obj);
      return expr;
      
    } while(0);
  }
  else {
    kL = -1;
    kR = 1;
  }
  
  if(exprlen >= 4)
    pad = pmath_expr_get_item(expr, 4);
  else
    pad = pmath_ref(list);
  
  if(exprlen >= 5)
    times = pmath_expr_get_item(expr, 5);
  else
    times = pmath_ref(PMATH_SYMBOL_TIMES);
  
  if(exprlen >= 6)
    plus = pmath_expr_get_item(expr, 6);
  else
    plus = pmath_ref(PMATH_SYMBOL_PLUS);
  
  
  S = (intptr_t)pmath_expr_length(list);
  R = (intptr_t)pmath_expr_length(ker);
  
  if(kL > 0)
    start = kL - 1;
  else
    start = R + kL;
  
  if(kR > 0)
    end = S + kR - 2;
  else
    end = S + R + kR - 1;
  
  pmath_unref(expr);
  if(end - start + 1 >= 0){
    intptr_t s, r;
    
    expr = pmath_expr_new(pmath_ref(PMATH_SYMBOL_LIST), end - start + 1);
    
    for(s = start;s <= end;++s){
      pmath_t sum = pmath_expr_new(pmath_ref(plus), R);
      
      for(r = 0;r < R;++r) {
        pmath_t ker_factor  = pmath_expr_get_item(ker, (size_t)r + 1);
        pmath_t list_factor = padded_list_get_0(list, s - r, pad);
        
        pmath_t term = pmath_expr_new_extended(
          pmath_ref(times), 2,
          ker_factor,
          list_factor);
          
        term = pmath_evaluate(term);
        sum = pmath_expr_set_item(sum, r + 1, term);
      }
      
      sum = pmath_evaluate(sum);
      expr = pmath_expr_set_item(expr, (size_t)(s - start) + 1, sum);
    }
  }
  else{
    pmath_message(PMATH_NULL, "nlen", 0);
    expr = pmath_expr_new(pmath_ref(PMATH_SYMBOL_LIST), 0);
  }
  
  pmath_unref(list);
  pmath_unref(ker);
  pmath_unref(pad);
  pmath_unref(times);
  pmath_unref(plus);
  
  return expr;
}
