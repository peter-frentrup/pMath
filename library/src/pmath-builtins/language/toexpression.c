#include <pmath-language/scanner.h>

#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>
#include <pmath-util/option-helpers.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/lists-private.h>

extern pmath_symbol_t pmath_System_DollarFailed;
extern pmath_symbol_t pmath_System_DynamicLocalBox;
extern pmath_symbol_t pmath_System_FractionBox;
extern pmath_symbol_t pmath_System_FrameBox;
extern pmath_symbol_t pmath_System_GridBox;
extern pmath_symbol_t pmath_System_HoldComplete;
extern pmath_symbol_t pmath_System_InterpretationBox;
extern pmath_symbol_t pmath_System_List;
extern pmath_symbol_t pmath_System_MakeExpression;
extern pmath_symbol_t pmath_System_OverscriptBox;
extern pmath_symbol_t pmath_System_RadicalBox;
extern pmath_symbol_t pmath_System_RotationBox;
extern pmath_symbol_t pmath_System_Rule;
extern pmath_symbol_t pmath_System_RuleDelayed;
extern pmath_symbol_t pmath_System_Sequence;
extern pmath_symbol_t pmath_System_SqrtBox;
extern pmath_symbol_t pmath_System_SubscriptBox;
extern pmath_symbol_t pmath_System_SubsuperscriptBox;
extern pmath_symbol_t pmath_System_SuperscriptBox;
extern pmath_symbol_t pmath_System_StringToBoxes;
extern pmath_symbol_t pmath_System_StripOnInput;
extern pmath_symbol_t pmath_System_StyleBox;
extern pmath_symbol_t pmath_System_TagBox;
extern pmath_symbol_t pmath_System_TooltipBox;
extern pmath_symbol_t pmath_System_TransformationBox;
extern pmath_symbol_t pmath_System_True;
extern pmath_symbol_t pmath_System_UnderoverscriptBox;
extern pmath_symbol_t pmath_System_UnderscriptBox;

static pmath_t remove_whitespace_from_boxes_raw(pmath_t boxes);

static pmath_t remove_whitespace_from_boxes(pmath_t boxes) {
  pmath_t debug_info = pmath_get_debug_info(boxes);
  
  if(pmath_is_null(debug_info))
    return remove_whitespace_from_boxes_raw(boxes);
    
  boxes = remove_whitespace_from_boxes_raw(boxes);
  
  return pmath_try_set_debug_info(boxes, debug_info);
}


static pmath_t remove_whitespace_from_boxes_raw(pmath_t boxes) {
  pmath_t head;
  size_t max_boxes;
  
  if(pmath_is_string(boxes)) {
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
  
  if( pmath_same(head, pmath_System_List) ||
      pmath_is_null(head))
  {
    size_t i;
    pmath_t item;
    pmath_bool_t remove_empty = FALSE;
    
    item = pmath_expr_get_item(boxes, 1);
    if(pmath_is_string(item)) {
      if(pmath_string_equals_latin1(item, "/*")) {
        pmath_unref(item);
        pmath_unref(boxes);
        return pmath_expr_new(pmath_ref(pmath_System_List), 0);
      }
      
      if( pmath_string_equals_latin1(item, "<<") ||
          pmath_string_equals_latin1(item, "??"))
      {
        pmath_unref(item);
        return boxes;
      }
      
    }
    
    pmath_unref(item);
    for(i = pmath_expr_length(boxes); i > 0; --i) {
      item = pmath_expr_get_item(boxes, i);
      boxes = pmath_expr_set_item(boxes, i, PMATH_NULL);
      
      item = remove_whitespace_from_boxes(item);
      if( pmath_is_expr_of_len(item, pmath_System_List, 0) ||
          pmath_is_expr_of_len(item, PMATH_NULL, 0))
      {
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
  
  if( pmath_same(head, pmath_System_Rule) ||
      pmath_same(head, pmath_System_RuleDelayed))
  {
    return boxes;
  }
  
  if(pmath_same(head, pmath_System_GridBox)) {
    size_t rows, cols;
    pmath_t matrix = pmath_expr_get_item(boxes, 1);
    boxes = pmath_expr_set_item(boxes, 1, PMATH_NULL);
    
    if(_pmath_is_matrix(matrix, &rows, &cols, FALSE)) {
      size_t i, j;
      
      for(i = rows; i > 0; --i) {
        pmath_t row = pmath_expr_get_item(matrix, i);
        matrix = pmath_expr_set_item(matrix, i, PMATH_NULL);
        
        for(j = cols; j > 0; --j) {
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
  
  if(pmath_same(head, pmath_System_DynamicLocalBox)) {
    // DynamicLocalBox({vars...}, boxes, options...)
    
    pmath_t item = pmath_expr_extract_item(boxes, 2);
    item = remove_whitespace_from_boxes(item);
    boxes = pmath_expr_set_item(boxes, 2, item);
    
    return boxes;
  }
  
  max_boxes  = 0;
  
  if(pmath_same(     head, pmath_System_FrameBox))            max_boxes = 1;
  else if(pmath_same(head, pmath_System_FractionBox))         max_boxes = 2;
  else if(pmath_same(head, pmath_System_InterpretationBox))   max_boxes = 1;
  else if(pmath_same(head, pmath_System_OverscriptBox))       max_boxes = 2;
  else if(pmath_same(head, pmath_System_RadicalBox))          max_boxes = 2;
  else if(pmath_same(head, pmath_System_RotationBox))         max_boxes = 1;
  else if(pmath_same(head, pmath_System_SqrtBox))             max_boxes = 1;
  else if(pmath_same(head, pmath_System_StyleBox))            max_boxes = 1;
  else if(pmath_same(head, pmath_System_SubscriptBox))        max_boxes = 1;
  else if(pmath_same(head, pmath_System_SubsuperscriptBox))   max_boxes = 2;
  else if(pmath_same(head, pmath_System_SuperscriptBox))      max_boxes = 1;
  else if(pmath_same(head, pmath_System_TagBox))              max_boxes = 1;
  else if(pmath_same(head, pmath_System_TransformationBox))   max_boxes = 1;
  else if(pmath_same(head, pmath_System_TooltipBox))          max_boxes = 2;
  else if(pmath_same(head, pmath_System_UnderoverscriptBox))  max_boxes = 3;
  else if(pmath_same(head, pmath_System_UnderscriptBox))      max_boxes = 2;
  
  if( pmath_same(head, pmath_System_StyleBox) ||
      pmath_same(head, pmath_System_TooltipBox))
  {
    pmath_expr_t options = pmath_options_extract(boxes, max_boxes);
    
    if(!pmath_is_null(options)) {
      pmath_t strip = pmath_evaluate(pmath_option_value(head, pmath_System_StripOnInput, options));
                        
      pmath_unref(strip);
      if(pmath_same(strip, pmath_System_True)) {
        strip = pmath_expr_get_item(boxes, 1);
        pmath_unref(options);
        pmath_unref(boxes);
        return remove_whitespace_from_boxes(strip);
      }
      
      pmath_unref(options);
    }
  }
  
  if(max_boxes > 0) {
    size_t i;
    for(i = max_boxes; i > 0; --i) {
      pmath_t item = pmath_expr_extract_item(boxes, i);
      
      item = remove_whitespace_from_boxes(item);
      
      boxes = pmath_expr_set_item(boxes, i, item);
    }
  }
  
  return boxes;
}


pmath_t builtin_toexpression(pmath_expr_t expr) {
  pmath_t code, head, debug_info;
  size_t exprlen = pmath_expr_length(expr);
  
  if(exprlen < 1) {
    pmath_message_argxxx(exprlen, 1, 2);
    return expr;
  }
  
  head = PMATH_UNDEFINED;
  if(exprlen >= 2) {
    head = pmath_expr_get_item(expr, 2);
    if(pmath_is_set_of_options(head)) {
      pmath_unref(head);
      head = PMATH_UNDEFINED;
    }
    else {
      size_t i;
      
      for(i = 2; i < exprlen; ++i) {
        expr = pmath_expr_set_item(expr, i, pmath_expr_get_item(expr, i + 1));
      }
      
      expr = pmath_expr_resize(expr, exprlen - 1);
    }
  }
  
  code = pmath_expr_get_item(expr, 1);
  debug_info = pmath_get_debug_info(code);
  if(pmath_is_string(code)) {
    code = pmath_expr_new_extended(
             pmath_ref(pmath_System_StringToBoxes), 1,
             code);
             
    code = pmath_evaluate(code);
    if(pmath_same(code, pmath_System_DollarFailed)) {
      pmath_unref(expr);
      pmath_unref(head);
      return code;
    }
    
    expr = pmath_expr_set_item(expr, 1, code);
    expr = pmath_expr_set_item(expr, 0, pmath_ref(pmath_System_MakeExpression));
    
    expr = pmath_evaluate(expr);
  }
  else {
    expr = pmath_expr_set_item(expr, 1, PMATH_NULL);
    code = remove_whitespace_from_boxes(code);
    
    expr = pmath_expr_set_item(expr, 1, code);
    expr = pmath_expr_set_item(expr, 0, pmath_ref(pmath_System_MakeExpression));
    
    expr = pmath_evaluate(expr);
  }
  
  if(pmath_is_expr_of(expr, pmath_System_HoldComplete)) {
    if(pmath_expr_length(expr) == 1) {
      pmath_t content = pmath_expr_get_item(expr, 1);
      pmath_unref(expr);
      
      content = pmath_try_set_debug_info(content, pmath_ref(debug_info));
      
      if(pmath_same(head, PMATH_UNDEFINED)) {
        pmath_unref(debug_info);
        return content;
      }
      
      expr = pmath_expr_new_extended(head, 1, content);
      expr = pmath_try_set_debug_info(expr, debug_info);
      return expr;
    }
    
    if(pmath_same(head, PMATH_UNDEFINED))
      head = pmath_ref(pmath_System_Sequence);
      
    expr = pmath_expr_set_item(expr, 0, head);
  }
  else
    pmath_unref(head);
  
  expr = pmath_try_set_debug_info(expr, debug_info);
  return expr;
}
