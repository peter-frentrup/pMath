#include <pmath-core/expressions-private.h>
#include <pmath-core/numbers.h>

#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>
#include <pmath-util/option-helpers.h>

#include <pmath-util/concurrency/threads.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/control-private.h>


static pmath_t extract_sametest_option(pmath_expr_t expr, size_t *len) {
  if(*len > 1) {
    pmath_t last = pmath_expr_get_item(expr, *len);
    
    if(_pmath_is_rule(last)) {
      pmath_t lhs = pmath_expr_get_item(last, 1);
      if(pmath_equals(lhs, PMATH_SYMBOL_SAMETEST)) {
        pmath_t rhs = pmath_expr_get_item(last, 2);
        
        --*len;
        pmath_unref(lhs);
        pmath_unref(last);
        return rhs;
      }
      
      pmath_unref(lhs);
    }
    else if(pmath_is_expr_of_len(last, PMATH_SYMBOL_LIST, 1)) {
      pmath_t item = pmath_expr_get_item(last, 1);
      
      if(_pmath_is_rule(item)) {
        pmath_t lhs = pmath_expr_get_item(item, 1);
        if(pmath_equals(lhs, PMATH_SYMBOL_SAMETEST)) {
          pmath_t rhs = pmath_expr_get_item(item, 2);
          
          --*len;
          pmath_unref(lhs);
          pmath_unref(item);
          pmath_unref(last);
          return rhs;
        }
        
        pmath_unref(lhs);
      }
      
      pmath_unref(item);
    }
    
    pmath_unref(last);
  }
  
  return pmath_option_value(PMATH_SYMBOL_COMPLEMENT, PMATH_SYMBOL_SAMETEST, PMATH_UNDEFINED);
}

PMATH_PRIVATE pmath_t builtin_complement(pmath_expr_t expr) {
  /* Complement(eall, e1, e2, ...)
   */
  size_t i, j, exprlen;
  pmath_t all, obj, sametest;
  
  exprlen = pmath_expr_length(expr);
  if(exprlen < 1) {
    pmath_message_argxxx(exprlen, 1, SIZE_MAX);
    return expr;
  }
  
  sametest = extract_sametest_option(expr, &exprlen);
  
  all = pmath_expr_get_item(expr, 1);
  if(!pmath_is_expr(all)) {
    pmath_unref(sametest);
    pmath_unref(all);
    pmath_message(PMATH_NULL, "nexprat", 2, PMATH_FROM_INT32(1), pmath_ref(expr));
    return expr;
  }
  
  obj = pmath_expr_get_item(all, 0);
  for(i = 2; i <= exprlen; ++i) {
    pmath_t item = pmath_expr_get_item(expr, i);
    pmath_t head;
    
    if(!pmath_is_expr(item)) {
      pmath_unref(sametest);
      pmath_unref(all);
      pmath_unref(obj);
      pmath_unref(item);
      pmath_message(PMATH_NULL, "nexprat", 2, pmath_integer_new_uiptr(i), pmath_ref(expr));
      return expr;
    }
    
    head = pmath_expr_get_item(item, 0);
    if(!pmath_equals(head, obj)) {
      pmath_unref(sametest);
      pmath_unref(all);
      pmath_unref(item);
      pmath_message(PMATH_NULL, "heads", 4,
                    obj, head, pmath_integer_new_uiptr(1), pmath_integer_new_uiptr(i));
      return expr;
    }
    
    pmath_unref(head);
    pmath_unref(item);
  }
  
  pmath_unref(obj);
  expr = pmath_expr_set_item(expr, 1, PMATH_NULL);
  all = pmath_expr_sort(all);
  
  if(pmath_same(sametest, PMATH_SYMBOL_AUTOMATIC)) {
    for(i = exprlen; i > 1; --i) {
      obj = pmath_expr_get_item(expr, i);
      expr = pmath_expr_set_item(expr, i, PMATH_NULL);
      
      obj = pmath_expr_sort(obj);
      expr = pmath_expr_set_item(expr, i, obj);
    }
    
    for(j = pmath_expr_length(all); j > 0; --j) {
      obj = pmath_expr_get_item(all, j);
      
      for(i = 2; i <= exprlen; ++i) {
        pmath_t e_i = pmath_expr_get_item(expr, i);
        
        if(_pmath_expr_find_sorted(e_i, obj) != 0) {
          pmath_unref(e_i);
          all = pmath_expr_set_item(all, j, PMATH_UNDEFINED);
          break;
        }
        
        pmath_unref(e_i);
        
        if(pmath_aborting()) {
          pmath_unref(obj);
          pmath_unref(all);
          pmath_unref(expr);
          pmath_unref(sametest);
          return PMATH_NULL;
        }
      }
      
      pmath_unref(obj);
    }
  }
  else {
    size_t k;
    pmath_t cmp;
    
    sametest = pmath_expr_new(sametest, 2);
    
    for(i = pmath_expr_length(all); i > 1; --i) {
      obj = pmath_expr_get_item(all, i);
      
      if(pmath_same(obj, PMATH_UNDEFINED))
        continue;
        
      sametest = pmath_expr_set_item(sametest, 1, obj);
      
      for(j = i - 1; j > 0; --j) {
        sametest = pmath_expr_set_item(
                     sametest, 2,
                     pmath_expr_get_item(all, j));
                     
        cmp = pmath_evaluate(pmath_ref(sametest));
        pmath_unref(cmp);
        
        if(pmath_same(cmp, PMATH_SYMBOL_TRUE)) {
          all = pmath_expr_set_item(all, j, PMATH_UNDEFINED);
        }
        
        if(pmath_aborting()) {
          pmath_unref(all);
          pmath_unref(expr);
          pmath_unref(sametest);
          return PMATH_NULL;
        }
      }
    }
    
    for(i = pmath_expr_length(all); i > 1; --i) {
      obj = pmath_expr_get_item(all, i);
      
      if(pmath_same(obj, PMATH_UNDEFINED))
        continue;
        
      sametest = pmath_expr_set_item(sametest, 1, obj);
      
      for(j = 2; j <= exprlen; ++j) {
        obj = pmath_expr_get_item(expr, j);
        
        for(k = pmath_expr_length(obj); k > 0; --k) {
          sametest = pmath_expr_set_item(
                       sametest, 2,
                       pmath_expr_get_item(obj, k));
                       
          cmp = pmath_evaluate(pmath_ref(sametest));
          pmath_unref(cmp);
          
          if(pmath_same(cmp, PMATH_SYMBOL_TRUE)) {
            all = pmath_expr_set_item(all, i, PMATH_UNDEFINED);
          }
          
          if(pmath_aborting()) {
            pmath_unref(all);
            pmath_unref(obj);
            pmath_unref(expr);
            pmath_unref(sametest);
            return PMATH_NULL;
          }
        }
        
        pmath_unref(obj);
      }
    }
  }
  
  pmath_unref(expr);
  pmath_unref(sametest);
  return pmath_expr_remove_all(all, PMATH_UNDEFINED);
}
