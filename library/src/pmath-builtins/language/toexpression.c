#include <pmath-language/scanner.h>

#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/control-private.h>
#include <pmath-builtins/lists-private.h>


static pmath_t remove_whitespace_from_boxes(pmath_t boxes){
  pmath_t head;
  size_t max_boxes;
  
  if(pmath_is_string(boxes)){
    pmath_span_array_t *spans;
    pmath_string_t code = boxes;
    
    spans = pmath_spans_from_string(&code, NULL, NULL, NULL, NULL, NULL);
    boxes = pmath_boxes_from_spans(spans, code, TRUE, NULL, NULL);
    
    pmath_unref(code);
    pmath_span_array_free(spans);
    
    return boxes;
  }
  
  if(!pmath_is_expr(boxes))
    return boxes;
  
  head = pmath_expr_get_item(boxes, 0);
  pmath_unref(head);
  
  if(pmath_same(head, PMATH_SYMBOL_LIST)
  || pmath_is_null(head)){
    size_t i;
    pmath_t item;
    pmath_bool_t remove_empty = FALSE;
    
    item = pmath_expr_get_item(boxes, 1);
    if(pmath_is_string(item)){
      if(pmath_string_equals_latin1(item, "/*")){
        pmath_unref(item);
        pmath_unref(boxes);
        return pmath_expr_new(pmath_ref(PMATH_SYMBOL_LIST), 0);
      }
    
      if(pmath_string_equals_latin1(item, "<<")
      || pmath_string_equals_latin1(item, "??")){
        pmath_unref(item);
        return boxes;
      }
    
    }
    
    pmath_unref(item);
    for(i = pmath_expr_length(boxes);i > 0;--i){
      item = pmath_expr_get_item(boxes, i);
      boxes = pmath_expr_set_item(boxes, i, PMATH_NULL);
      
      item = remove_whitespace_from_boxes(item);
      if(pmath_is_expr_of_len(item, PMATH_SYMBOL_LIST, 0)
      || pmath_is_expr_of_len(item, PMATH_NULL, 0)){
        remove_empty = TRUE;
        pmath_unref(item);
        item = PMATH_UNDEFINED;
      }
      
      boxes = pmath_expr_set_item(boxes, i, item);
    }
    
    if(remove_empty)
      boxes = pmath_expr_remove_all(boxes, PMATH_UNDEFINED);
    
    return boxes;
  }
  
  if(pmath_same(head, PMATH_SYMBOL_RULE)
  || pmath_same(head, PMATH_SYMBOL_RULEDELAYED))
    return boxes;
  
  if(pmath_same(head, PMATH_SYMBOL_GRIDBOX)){
    size_t rows, cols;
    pmath_t matrix = pmath_expr_get_item(boxes, 1);
    boxes = pmath_expr_set_item(boxes, 1, PMATH_NULL);
    
    if(_pmath_is_matrix(matrix, &rows, &cols)){
      size_t i, j;
      
      for(i = rows;i > 0;--i){
        pmath_t row = pmath_expr_get_item(matrix, i);
        matrix = pmath_expr_set_item(matrix, i, PMATH_NULL);
        
        for(j = cols;j > 0;--j){
          pmath_t item = pmath_expr_get_item(row, j);
          row = pmath_expr_set_item(row, j, PMATH_NULL);
          
          item = remove_whitespace_from_boxes(item);
          
          row = pmath_expr_set_item(row, j, item);
        }
        
        matrix = pmath_expr_set_item(matrix, i, row);
      }
    }
    
    boxes = pmath_expr_set_item(boxes, 1, matrix);
    return boxes;
  }
  
  max_boxes  = 0;
  
  if(     pmath_same(head, PMATH_SYMBOL_FRAMEBOX))            max_boxes = 1;
  else if(pmath_same(head, PMATH_SYMBOL_FRACTIONBOX))         max_boxes = 2;
  else if(pmath_same(head, PMATH_SYMBOL_INTERPRETATIONBOX))   max_boxes = 1;
  else if(pmath_same(head, PMATH_SYMBOL_OVERSCRIPTBOX))       max_boxes = 2;
  else if(pmath_same(head, PMATH_SYMBOL_RADICALBOX))          max_boxes = 2;
  else if(pmath_same(head, PMATH_SYMBOL_ROTATIONBOX))         max_boxes = 1;
  else if(pmath_same(head, PMATH_SYMBOL_SQRTBOX))             max_boxes = 1;
  else if(pmath_same(head, PMATH_SYMBOL_STYLEBOX))            max_boxes = 1;
  else if(pmath_same(head, PMATH_SYMBOL_SUBSCRIPTBOX))        max_boxes = 1;
  else if(pmath_same(head, PMATH_SYMBOL_SUBSUPERSCRIPTBOX))   max_boxes = 2;
  else if(pmath_same(head, PMATH_SYMBOL_SUPERSCRIPTBOX))      max_boxes = 1;
  else if(pmath_same(head, PMATH_SYMBOL_TAGBOX))              max_boxes = 1;
  else if(pmath_same(head, PMATH_SYMBOL_TRANSFORMATIONBOX))   max_boxes = 1;
  else if(pmath_same(head, PMATH_SYMBOL_TOOLTIPBOX))          max_boxes = 2;
  else if(pmath_same(head, PMATH_SYMBOL_UNDERSCRIPTBOX))      max_boxes = 2;
  else if(pmath_same(head, PMATH_SYMBOL_UNDEROVERSCRIPTBOX))  max_boxes = 3;
  
  if(pmath_same(head, PMATH_SYMBOL_STYLEBOX)
  || pmath_same(head, PMATH_SYMBOL_TOOLTIPBOX)){
    pmath_expr_t options = pmath_options_extract(boxes, max_boxes);
    
    if(!pmath_is_null(options)){
      pmath_t strip = pmath_option_value(
        head, 
        PMATH_SYMBOL_STRIPONINPUT,
        options);
      
      pmath_unref(strip);
      if(pmath_same(strip, PMATH_SYMBOL_TRUE)){
        strip = pmath_expr_get_item(boxes, 1);
        pmath_unref(options);
        pmath_unref(boxes);
        return remove_whitespace_from_boxes(strip);
      }
      
      pmath_unref(options);
    }
  }
  
  if(max_boxes > 0){
    size_t i;
    for(i = max_boxes;i > 0;--i){
      pmath_t item = pmath_expr_get_item(boxes, i);
      boxes = pmath_expr_set_item(boxes, i, PMATH_NULL);
      
      item = remove_whitespace_from_boxes(item);
      
      boxes = pmath_expr_set_item(boxes, i, item);
    }
  }
  
  return boxes;
}


pmath_t builtin_toexpression(pmath_expr_t expr){
  pmath_t code, head;
  size_t exprlen = pmath_expr_length(expr);
  
  if(exprlen < 1){
    pmath_message_argxxx(exprlen, 1, 2);
    return expr;
  }
  
  head = PMATH_UNDEFINED;
  if(exprlen >= 2){
    head = pmath_expr_get_item(expr, 2);
    if(_pmath_is_rule(head) || _pmath_is_list_of_rules(head)){
      pmath_unref(head);
      head = PMATH_UNDEFINED;
    }
    else{
      size_t i;
      
      for(i = 2;i < exprlen;++i){
        expr = pmath_expr_set_item(expr, i, pmath_expr_get_item(expr, i+1));
      }
      
      expr = pmath_expr_resize(expr, exprlen - 1);
    }
  }
  
  code = pmath_expr_get_item(expr, 1);
  if(pmath_is_string(code)){
    code = pmath_expr_new_extended(
      pmath_ref(PMATH_SYMBOL_STRINGTOBOXES), 1,
      code);
    
    code = pmath_evaluate(code);
    if(pmath_same(code, PMATH_SYMBOL_FAILED)){
      pmath_unref(expr);
      pmath_unref(head);
      return code;
    }
    
    expr = pmath_expr_set_item(expr, 1, code);
    expr = pmath_expr_set_item(expr, 0, pmath_ref(PMATH_SYMBOL_MAKEEXPRESSION));
    
    expr = pmath_evaluate(expr);
  }
  else{
    expr = pmath_expr_set_item(expr, 1, PMATH_NULL);
    code = remove_whitespace_from_boxes(code);
    
    expr = pmath_expr_set_item(expr, 1, code);
    expr = pmath_expr_set_item(expr, 0, pmath_ref(PMATH_SYMBOL_MAKEEXPRESSION));
    
    expr = pmath_evaluate(expr);
  }
  
  if(pmath_is_expr_of(expr, PMATH_SYMBOL_HOLDCOMPLETE)){
    if(!pmath_same(head, PMATH_UNDEFINED))
      return pmath_expr_set_item(expr, 0, head);
    
    if(pmath_expr_length(expr) == 1){
      pmath_unref(head);
      head = pmath_expr_get_item(expr, 1);
      pmath_unref(expr);
      return head;
    }
    
    expr = pmath_expr_set_item(expr, 0, pmath_ref(PMATH_SYMBOL_SEQUENCE));
  }
  
  return expr;
}
