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

#include <pmath-util/evaluation.h>
#include <pmath-util/concurrency/atomic.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-core/objects-inline.h>
#include <pmath-core/objects-private.h>
#include <pmath-core/expressions-private.h>

#include <pmath-builtins/control/definitions-private.h>
#include <pmath-builtins/control/flow-private.h>
#include <pmath-builtins/all-symbols.h>
#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/lists-private.h>

#include <pmath-language/patterns-private.h>

static pmath_t replace_purearg(
  pmath_t     function,  // will be freed
  pmath_expr_t arguments  // wont be freed
){
  pmath_bool_t do_flatten = FALSE;
  pmath_t head;
  size_t i, len;
  
  if(!pmath_instance_of(function, PMATH_TYPE_EXPRESSION))
    return function;

  len = pmath_expr_length(function);
  
  head = replace_purearg(
    pmath_expr_get_item(function, 0),
    arguments);
    
  function = pmath_expr_set_item(function, 0, head);

  if(head == PMATH_SYMBOL_PUREARGUMENT && len == 1){
    pmath_bool_t reverse;
    pmath_t pos = pmath_expr_get_item(function, 1);
    size_t min = 1;
    size_t max = pmath_expr_length(arguments);
    
    if(extract_range(pos, &min, &max, TRUE)){
      pmath_unref(pos);
      pmath_unref(function);
      if(min == max)
        return pmath_expr_get_item(arguments, min);

      reverse = max < min;
      if(reverse){
        len = min - max + 1;
        min = max;
      }
      else
        len = max - min + 1;

      function = pmath_expr_set_item(
        pmath_expr_get_item_range(arguments, min, len),
        0, PMATH_UNDEFINED);
        
      if(reverse){
        for(i = 1;i <= len/2;i++){
          pmath_t first = pmath_expr_get_item(function, i);
          pmath_t last  = pmath_expr_get_item(function, len-i+1);
          function = pmath_expr_set_item(function, i,       last);
          function = pmath_expr_set_item(function, len-i+1, first);
        }
      }
      
      return function;
    }
    
    pmath_unref(pos);
  }

  for(i = 1;i <= len;++i){
    pmath_t item = replace_purearg(
      pmath_expr_get_item(function, i),
      arguments);
      
    if(!do_flatten && pmath_is_expr_of(item, PMATH_UNDEFINED))
      do_flatten = TRUE;
    
    function = pmath_expr_set_item(function, i, item);
  }
  
  if(do_flatten)
    return pmath_expr_flatten(
      function,
      PMATH_UNDEFINED,
      1);
      
  return function;
}

PMATH_PRIVATE pmath_t builtin_call_function(pmath_expr_t expr){
/* Function(body)(args)
   Function(x, body)(args)
   Function(x, body, attrib)(args)
   Function({xs}, body)(args)
   Function({xs}, body, attrib)(args)
 */
  pmath_expr_t head;
  pmath_t     head_head;
  size_t             exprlen;
  size_t             headlen;
  
  head = (pmath_expr_t)pmath_expr_get_item(expr, 0);
  
  assert(pmath_instance_of(head, PMATH_TYPE_EXPRESSION));
  
  head_head = pmath_expr_get_item(head, 0);
  pmath_unref(head_head);
  if(head_head != PMATH_SYMBOL_FUNCTION){
    pmath_unref(head);
    return expr;
  }
  
  exprlen = pmath_expr_length(expr);
  headlen = pmath_expr_length(head);

  if(headlen == 1){
    pmath_t body;
    size_t         i;
    
    body = pmath_expr_get_item(head, 1);
    pmath_unref(head);
    
    for(i = 1;i <= exprlen;++i){
      pmath_t item = pmath_evaluate(
        pmath_expr_get_item(expr, i));
        
      if(pmath_is_expr_of_len(item, PMATH_SYMBOL_UNEVALUATED, 1)){
        pmath_expr_t item_expr = (pmath_expr_t)item;
        item = pmath_expr_get_item(item_expr, 1);
        pmath_unref(item_expr);
      }
      
      expr = pmath_expr_set_item(expr, i, item);
    }
    
    body = replace_purearg(
      body,
      expr);
      
    pmath_unref(expr);
    return body;
  }
  
  if(headlen == 2 || headlen == 3){
    pmath_t params = pmath_expr_get_item(head, 1);
    pmath_t body   = pmath_expr_get_item(head, 2);
    
    if(exprlen > 0){
      pmath_bool_t eval_first   = TRUE;
      pmath_bool_t eval_rest    = TRUE;
      pmath_bool_t sort_args    = FALSE;
      pmath_bool_t thread_args  = FALSE;
      
      if(headlen == 3){
        pmath_t attrib_obj = pmath_expr_get_item(head, 3);
        pmath_symbol_attributes_t attrib;
        
        if(!_pmath_get_attributes(&attrib, attrib_obj)){
          pmath_unref(attrib_obj);
          pmath_unref(params);
          pmath_unref(body);
          pmath_unref(head);
          return expr;
        }
        
        pmath_unref(attrib_obj);
        
        if(attrib & PMATH_SYMBOL_ATTRIBUTE_HOLDALLCOMPLETE){
          eval_first = FALSE;
          eval_rest  = FALSE;
        }
        else{
          eval_first   = (attrib & PMATH_SYMBOL_ATTRIBUTE_HOLDFIRST)   == 0;
          eval_rest    = (attrib & PMATH_SYMBOL_ATTRIBUTE_HOLDREST)    == 0;
          sort_args    = (attrib & PMATH_SYMBOL_ATTRIBUTE_SYMMETRIC)   != 0;
          thread_args  = (attrib & PMATH_SYMBOL_ATTRIBUTE_LISTABLE)    != 0;
        }
      }
        
      if(eval_first){
        pmath_t item = pmath_evaluate(
          pmath_expr_get_item(expr, 1));
          
        if(pmath_is_expr_of_len(item, PMATH_SYMBOL_UNEVALUATED, 1)){
          pmath_expr_t item_expr = (pmath_expr_t)item;
          item = pmath_expr_get_item(item_expr, 1);
          pmath_unref(item_expr);
        }
        
        expr = pmath_expr_set_item(expr, 1, item);
      }
      
      if(eval_rest){
        size_t i;
        
        for(i = 2;i <= exprlen;++i){
          pmath_t item = pmath_evaluate(
            pmath_expr_get_item(expr, i));
            
          if(pmath_is_expr_of_len(item, PMATH_SYMBOL_UNEVALUATED, 1)){
            pmath_expr_t item_expr = (pmath_expr_t)item;
            item = pmath_expr_get_item(item_expr, 1);
            pmath_unref(item_expr);
          }
          
          expr = pmath_expr_set_item(expr, i, item);
        }
      }
      
      if(sort_args)
        expr = pmath_expr_sort(expr);

      if(thread_args){
        size_t i;
        
        for(i = 1;i <= exprlen;++i){
          pmath_t arg = pmath_expr_get_item(expr, i);
          
          if(pmath_is_expr_of(arg, PMATH_SYMBOL_LIST)){
            pmath_bool_t error_message = TRUE;
            pmath_unref(arg);
            pmath_unref(params);
            pmath_unref(body);
            pmath_unref(head);
            
            return _pmath_expr_thread(
              expr, PMATH_SYMBOL_LIST, 1, SIZE_MAX, &error_message);
          }
          
          pmath_unref(arg);
        }
      }
    }
    
    if(pmath_instance_of(params, PMATH_TYPE_SYMBOL)){
      pmath_unref(head);
      
      if(exprlen == 0){
        pmath_message(NULL, "cnt", 2, params, pmath_ref(expr));
        
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
    
    if(pmath_is_expr_of(params, PMATH_SYMBOL_LIST)){
      size_t i;
      
      if(pmath_expr_length(params) > exprlen){
        pmath_message(NULL, "cnt", 2, params, pmath_ref(expr));
        
        pmath_unref(body);
        pmath_unref(head);
        return expr;
      }
      
      for(i = 1;i <= pmath_expr_length(params);++i){
        pmath_t value = pmath_expr_get_item(expr, i);
        pmath_t p     = pmath_expr_get_item(params, i);
        
        if(!pmath_instance_of(p, PMATH_TYPE_SYMBOL)){
          pmath_unref(p);
          pmath_unref(value);
          pmath_unref(body);
          
          pmath_message(NULL, "par", 2, params, head);
            
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
    
    pmath_message(NULL, "par", 2, params, head);
      
    pmath_unref(body);
    return expr;
  }
  
  pmath_message_argxxx(headlen, 1, 3);
  pmath_unref(head);
  return expr;
}

PMATH_PRIVATE pmath_t builtin_function(pmath_expr_t expr){
/** Function(body)
    Function(x, body)
    Function(x, body, attrib)
    Function({xs}, body)
    Function({xs}, body, attrib)
 */
  size_t exprlen = pmath_expr_length(expr);
  
  if(exprlen < 1 || exprlen > 3){
    pmath_message_argxxx(exprlen, 1, 3);
    return expr;
  }
  
  if(exprlen > 1){
    pmath_t params = pmath_expr_get_item(expr, 1);
    
    if(pmath_instance_of(params, PMATH_TYPE_EXPRESSION)){
      size_t         i;
      pmath_t p;
      
      p = pmath_expr_get_item(params, 0);
      pmath_unref(p);
      
      if(p != PMATH_SYMBOL_LIST){
        pmath_message(NULL, "par", 2, params, pmath_ref(expr));
          
        return expr;
      }
      
      for(i = 1;i <= pmath_expr_length(params);++i){
        p = pmath_expr_get_item(params, i);
        
        if(!pmath_instance_of(p, PMATH_TYPE_SYMBOL)){
          pmath_unref(p);
          
          pmath_message(NULL, "par", 2, params, pmath_ref(expr));
            
          return expr;
        }
        
        pmath_unref(p);
      }
      
      pmath_unref(params);
      return expr;
    }
    
    if(!pmath_instance_of(params, PMATH_TYPE_SYMBOL)){
      pmath_message(NULL, "par", 2, params, pmath_ref(expr));
        
      return expr;
    }
    
    pmath_unref(params);
  }
  
  return expr;
}
