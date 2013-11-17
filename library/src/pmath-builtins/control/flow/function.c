#include <pmath-core/expressions-private.h>

#include <pmath-language/patterns-private.h>

#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/control/definitions-private.h>
#include <pmath-builtins/lists-private.h>

// retains debug-info
static pmath_t replace_purearg(
  pmath_t      function,  // will be freed
  pmath_expr_t arguments  // wont be freed
) {
  pmath_bool_t do_flatten = FALSE;
  pmath_t head, debug_info;
  size_t i, len;
  
  if(!pmath_is_expr(function))
    return function;
    
  debug_info = _pmath_expr_get_debug_info(function);
  head       = pmath_expr_get_item(function, 0);
  len        = pmath_expr_length(function);
  
  if(pmath_same(head, PMATH_SYMBOL_FUNCTION)) {
    pmath_unref(head);
    if(len == 1) {
      pmath_unref(debug_info);
      return function;
    }
    
    head = pmath_expr_get_item(function, 1);
    pmath_unref(head);
    
    if(pmath_same(head, PMATH_NULL)) {
      pmath_unref(debug_info);
      return function;
    }
  }
  else if(len == 1 && pmath_same(head, PMATH_SYMBOL_PUREARGUMENT)) {
    pmath_bool_t reverse;
    pmath_t pos = pmath_expr_get_item(function, 1);
    size_t min = 1;
    size_t max = pmath_expr_length(arguments);
    
    pmath_unref(head);
    
    if(extract_range(pos, &min, &max, TRUE)) {
      pmath_unref(pos);
      pmath_unref(function);
      if(min == max) {
        pmath_unref(debug_info);
        return pmath_expr_get_item(arguments, min);
      }
      
      reverse = max < min;
      if(reverse) {
        len = min - max + 1;
        min = max;
      }
      else
        len = max - min + 1;
        
      function = pmath_expr_set_item(
                   pmath_expr_get_item_range(arguments, min, len),
                   0, PMATH_UNDEFINED);
                   
      if(reverse) {
        for(i = 1; i <= len / 2; i++) {
          pmath_t first = pmath_expr_get_item(function, i);
          pmath_t last  = pmath_expr_get_item(function, len - i + 1);
          function = pmath_expr_set_item(function, i,       last);
          function = pmath_expr_set_item(function, len - i + 1, first);
        }
      }
      
      function = _pmath_expr_set_debug_info(function, debug_info);
      return function;
    }
    
    pmath_unref(pos);
  }
  else {
    head = replace_purearg(head, arguments);
    
    function = pmath_expr_set_item(function, 0, head);
  }
  
  for(i = 1; i <= len; ++i) {
    pmath_t item = replace_purearg(
                     pmath_expr_get_item(function, i),
                     arguments);
                     
    if(!do_flatten && pmath_is_expr_of(item, PMATH_UNDEFINED))
      do_flatten = TRUE;
      
    function = pmath_expr_set_item(function, i, item);
  }
  
  if(do_flatten) {
    function = pmath_expr_flatten(
                 function,
                 PMATH_UNDEFINED,
                 1);
  }
  
  function = _pmath_expr_set_debug_info(function, debug_info);
  return function;
}

PMATH_PRIVATE pmath_t builtin_call_function(pmath_expr_t expr) {
  /* Function(body)(args)
     Function(/\/, body)(args)
     Function(/\/, body, attrib)(args)
     Function(x, body)(args)
     Function(x, body, attrib)(args)
     Function({xs}, body)(args)
     Function({xs}, body, attrib)(args)
   */
  pmath_expr_t head;
  pmath_t      head_head;
  size_t       exprlen;
  size_t       headlen;
  
  head = pmath_expr_get_item(expr, 0);
  
  assert(pmath_is_expr(head));
  
  head_head = pmath_expr_get_item(head, 0);
  pmath_unref(head_head);
  if(!pmath_same(head_head, PMATH_SYMBOL_FUNCTION)) {
    pmath_unref(head);
    return expr;
  }
  
  exprlen = pmath_expr_length(expr);
  headlen = pmath_expr_length(head);
  
  if(headlen == 1) {
    pmath_t body = pmath_expr_get_item(head, 1);
    pmath_unref(head);
    
    body = replace_purearg(body, expr);
             
    pmath_unref(expr);
    return body;
  }
  
  if(headlen == 2 || headlen == 3) {
    pmath_t params = pmath_expr_get_item(head, 1);
    pmath_t body   = pmath_expr_get_item(head, 2);
    
    /* The evaluator has already handled function attributes for us, because it 
       has special knowledge about Function(...)(...).
       More precisely, it uses _pmath_get_function_attributes(), which
       extracts Function(...) attributes.
     */
    
    if(pmath_is_null(params)) {
      pmath_unref(head);
      
      body = replace_purearg(body, expr);
      
      pmath_unref(expr);
      return body;
    }
    
    if(pmath_is_symbol(params)) {
      pmath_unref(head);
      
      if(exprlen == 0) {
        pmath_message(PMATH_NULL, "cnt", 2, params, pmath_ref(expr));
        
        pmath_unref(body);
        pmath_unref(head);
        return expr;
      }
      
      head = pmath_expr_get_item(expr, 1);
      body = _pmath_replace_local(body, params, head);
      pmath_unref(head);
      pmath_unref(params);
      pmath_unref(expr);
      return body;
    }
    
    if(pmath_is_expr_of(params, PMATH_SYMBOL_LIST)) {
      size_t i;
      
      if(pmath_expr_length(params) > exprlen) {
        pmath_message(PMATH_NULL, "cnt", 2, params, pmath_ref(expr));
        
        pmath_unref(body);
        pmath_unref(head);
        return expr;
      }
      
      for(i = 1; i <= pmath_expr_length(params); ++i) {
        pmath_t value = pmath_expr_get_item(expr, i);
        pmath_t p     = pmath_expr_get_item(params, i);
        
        if(!pmath_is_symbol(p)) {
          pmath_unref(p);
          pmath_unref(value);
          pmath_unref(body);
          
          pmath_message(PMATH_NULL, "par", 2, params, head);
          
          return expr;
        }
        
        body = _pmath_replace_local(body, p, value);
        
        pmath_unref(p);
        pmath_unref(value);
      }
      
      pmath_unref(head);
      pmath_unref(params);
      pmath_unref(expr);
      return body;
    }
    
    pmath_message(PMATH_NULL, "par", 2, params, head);
    
    pmath_unref(body);
    return expr;
  }
  
  pmath_message_argxxx(headlen, 1, 3);
  pmath_unref(head);
  return expr;
}

PMATH_PRIVATE pmath_t builtin_function(pmath_expr_t expr) {
  /** Function(body)
      Function(/\/, body)
      Function(/\/, body, attrib)
      Function(x, body)
      Function(x, body, attrib)
      Function({xs}, body)
      Function({xs}, body, attrib)
   */
  size_t exprlen = pmath_expr_length(expr);
  
  if(exprlen < 1 || exprlen > 3) {
    pmath_message_argxxx(exprlen, 1, 3);
    return expr;
  }
  
  if(exprlen > 1) {
    pmath_t params = pmath_expr_get_item(expr, 1);
    
    if(pmath_is_expr(params)) {
      size_t  i;
      pmath_t p;
      
      p = pmath_expr_get_item(params, 0);
      pmath_unref(p);
      
      if(!pmath_same(p, PMATH_SYMBOL_LIST)) {
        pmath_message(PMATH_NULL, "par", 2, params, pmath_ref(expr));
        
        return expr;
      }
      
      for(i = 1; i <= pmath_expr_length(params); ++i) {
        p = pmath_expr_get_item(params, i);
        
        if(!pmath_is_symbol(p)) {
          pmath_unref(p);
          
          pmath_message(PMATH_NULL, "par", 2, params, pmath_ref(expr));
          
          return expr;
        }
        
        pmath_unref(p);
      }
      
      pmath_unref(params);
      return expr;
    }
    
    if(!pmath_is_null(params) && !pmath_is_symbol(params)) {
      pmath_message(PMATH_NULL, "par", 2, params, pmath_ref(expr));
      
      return expr;
    }
    
    pmath_unref(params);
  }
  
  return expr;
}
