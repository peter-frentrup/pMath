#include <pmath-language/scanner.h>

#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>


static void syntax_error(pmath_string_t code, int pos, void *flag, pmath_bool_t critical){
  if(critical)
    *(pmath_bool_t*)flag = TRUE;
  pmath_message_syntax_error(code, pos, NULL, 0);
}

PMATH_PRIVATE pmath_t builtin_stringtoboxes(pmath_expr_t expr){
/* StringToBoxes("code")
 */
  pmath_string_t       code;
  pmath_bool_t         error_flag;
  pmath_span_array_t  *arr;
  pmath_t              result;

  if(pmath_expr_length(expr) != 1){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    pmath_unref(expr);
    return pmath_ref(PMATH_SYMBOL_FAILED);
  }

  code = pmath_expr_get_item(expr, 1);
  if(!pmath_is_string(code)){
    pmath_unref(code);
    return expr;
  }

  pmath_unref(expr);
  error_flag = FALSE;
  arr = pmath_spans_from_string(
    (pmath_string_t*)&code,
    NULL,
    NULL,
    NULL,
    syntax_error,
    &error_flag);

  if(error_flag){
    pmath_unref(code);
    pmath_span_array_free(arr);
    return pmath_ref(PMATH_SYMBOL_FAILED);
  }

  result = pmath_boxes_from_spans(
    arr,
    (pmath_string_t)code,
    TRUE,
    NULL,
    NULL);

  pmath_span_array_free(arr);
  pmath_unref(code);
  return result;
}
