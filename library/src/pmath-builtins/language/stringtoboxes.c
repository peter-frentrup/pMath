#include <pmath-language/scanner.h>

#include <pmath-util/evaluation.h>
#include <pmath-util/messages.h>
#include <pmath-util/option-helpers.h>

#include <pmath-builtins/all-symbols-private.h>


static void syntax_error(pmath_string_t code, int pos, void *flag, pmath_bool_t critical) {
  pmath_bool_t *have_critical = flag;
  
  if(!*have_critical)
    pmath_message_syntax_error(code, pos, PMATH_NULL, 0);
  
  if(critical)
    *have_critical = TRUE;
}

PMATH_PRIVATE pmath_t builtin_stringtoboxes(pmath_expr_t expr) {
  /* StringToBoxes("code", [options])
     
     Whitespace -> False
   */
  pmath_string_t                      code;
  pmath_bool_t                        error_flag;
  pmath_span_array_t                 *arr;
  pmath_t                             result;
  pmath_t                             options;
  struct pmath_boxes_from_spans_ex_t  settings;
  
  if(pmath_expr_length(expr) < 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    pmath_unref(expr);
    return pmath_ref(PMATH_SYMBOL_FAILED);
  }
  
  code = pmath_expr_get_item(expr, 1);
  if(!pmath_is_string(code)) {
    pmath_unref(code);
    return expr;
  }
  
  options = pmath_options_extract(expr, 1);
  if(pmath_is_null(options)) {
    pmath_unref(expr);
    pmath_unref(code);
    return pmath_ref(PMATH_SYMBOL_FAILED);
  }
  
  memset(&settings, 0, sizeof(settings));
  settings.size = sizeof(settings);
  
  // Whitespace->False ==> PMATH_BFS_PARSEABLE flag set
  result = pmath_evaluate(pmath_option_value(PMATH_NULL, PMATH_SYMBOL_WHITESPACE, options));
  if(pmath_same(result, PMATH_SYMBOL_FALSE)){
    settings.flags|= PMATH_BFS_PARSEABLE;
  }
  else if(!pmath_same(result, PMATH_SYMBOL_TRUE)){
    pmath_unref(options);
    pmath_unref(code);
    pmath_unref(expr);
    pmath_message(
      PMATH_NULL, "opttf", 2,
      pmath_ref(PMATH_SYMBOL_WHITESPACE),
      result);
    return pmath_ref(PMATH_SYMBOL_FAILED);
  }
  pmath_unref(result);
  
  
  pmath_unref(options);
  
  pmath_unref(expr);
  error_flag = FALSE;
  arr = pmath_spans_from_string(
          (pmath_string_t*)&code,
          NULL,
          NULL,
          NULL,
          syntax_error,
          &error_flag);
          
  if(error_flag) {
    pmath_unref(code);
    pmath_span_array_free(arr);
    return pmath_ref(PMATH_SYMBOL_FAILED);
  }
  
  result = pmath_boxes_from_spans_ex(arr, code, &settings);
  
  pmath_span_array_free(arr);
  pmath_unref(code);
  return result;
}
