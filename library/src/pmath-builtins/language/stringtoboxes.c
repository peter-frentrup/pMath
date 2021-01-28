#include <pmath-language/scanner.h>

#include <pmath-core/numbers.h>

#include <pmath-util/evaluation.h>
#include <pmath-util/messages.h>
#include <pmath-util/option-helpers.h>

#include <pmath-builtins/all-symbols-private.h>

#include <string.h>


extern pmath_symbol_t pmath_Language_SourceLocation;

static void syntax_error(pmath_string_t code, int pos, void *flag, pmath_bool_t critical) {
  pmath_bool_t *have_critical = flag;
  
  if(!*have_critical)
    pmath_message_syntax_error(code, pos, PMATH_NULL, 0);
    
  if(critical)
    *have_critical = TRUE;
}

struct _pmath_string_to_boxes_data_t {
  pmath_string_t                      debug_source;
  pmath_t                             token_decorator;
  struct pmath_boxes_from_spans_ex_t  settings;
};

static pmath_t add_string_debug_info(
  pmath_t                             token_or_span,
  const struct pmath_text_position_t *start,
  const struct pmath_text_position_t *end,
  void                               *_data
) {
  pmath_t debug_info;
  struct _pmath_string_to_boxes_data_t *data = _data;
  
  assert(0 <= start->index);
  assert(start->index <= end->index);
  
  if(!pmath_is_expr(token_or_span)) {
    if(pmath_same(data->token_decorator, PMATH_UNDEFINED)) {
      if(!pmath_is_string(token_or_span))
        return token_or_span;
    }
    else {
      token_or_span = pmath_expr_new_extended(
                        pmath_ref(data->token_decorator), 1,
                        token_or_span);
    }
  }
  
  debug_info = pmath_expr_new_extended(
                 pmath_ref(pmath_Language_SourceLocation), 2,
                 pmath_ref(data->debug_source),
                 pmath_expr_new_extended(
                   pmath_ref(PMATH_SYMBOL_RANGE), 2,
                   pmath_integer_new_si32(start->index + 1),
                   pmath_integer_new_si32(end->index)));
                   
  return pmath_try_set_debug_info(token_or_span, debug_info);
}

static pmath_bool_t handle_whitespace_option(
  struct _pmath_string_to_boxes_data_t *data,
  pmath_t                               options // wont be freed
) {
  // Whitespace->False ==> PMATH_BFS_PARSEABLE flag set
  pmath_t value = pmath_evaluate(pmath_option_value(PMATH_NULL, PMATH_SYMBOL_WHITESPACE, options));
  if(pmath_same(value, PMATH_SYMBOL_FALSE)) {
    data->settings.flags |= PMATH_BFS_PARSEABLE;
    pmath_unref(value);
    return TRUE;
  }
  
  if(pmath_same(value, PMATH_SYMBOL_TRUE)) {
    data->settings.flags &= ~PMATH_BFS_PARSEABLE;
    pmath_unref(value);
    return TRUE;
  }
  
  pmath_message(
    PMATH_NULL, "opttf", 2,
    pmath_ref(PMATH_SYMBOL_WHITESPACE),
    value);
  return FALSE;
}

static pmath_bool_t handle_ignoresyntaxerrors_option(
  pmath_bool_t *result,
  pmath_t       options // wont be freed
) {
  // "IgnoreSyntaxErrors"->False ==> return $Failed don error
  pmath_t name= PMATH_C_STRING("IgnoreSyntaxErrors");
  pmath_t value = pmath_evaluate(pmath_option_value(PMATH_NULL, name, options));
  pmath_unref(name);
  if(pmath_same(value, PMATH_SYMBOL_FALSE)) {
    *result = FALSE;
    pmath_unref(value);
    return TRUE;
  }
  
  if(pmath_same(value, PMATH_SYMBOL_TRUE)) {
    *result = TRUE;
    pmath_unref(value);
    return TRUE;
  }
  
  *result = FALSE;
  pmath_message(
    PMATH_NULL, "opttf", 2,
    pmath_ref(PMATH_SYMBOL_WHITESPACE),
    value);
  return FALSE;
}

static void handle_tokens_option(
  struct _pmath_string_to_boxes_data_t *data,
  pmath_t                               options // wont be freed
) {
  // "Tokens"->String ==> token_decorator = PMATH_UNDEFINED, else token_decorator = option value
  pmath_t name= PMATH_C_STRING("Tokens");
  pmath_t value = pmath_evaluate(pmath_option_value(PMATH_NULL, name, options));
  pmath_unref(name);
  if(pmath_same(value, PMATH_SYMBOL_STRING)) {
    data->token_decorator = PMATH_UNDEFINED;
    pmath_unref(value);
    return;
  }
  
  data->token_decorator = value;
}

PMATH_PRIVATE pmath_t builtin_stringtoboxes(pmath_expr_t expr) {
  /* StringToBoxes("code", [options])
  
     "Tokens" -> String       % return single tokens as a string instead of wrapped in an expression
     Whitespace -> False
   */
  struct _pmath_string_to_boxes_data_t  data;
  pmath_string_t                        code;
  pmath_bool_t                          error_flag;
  pmath_bool_t                          ignore_errors;
  pmath_span_array_t                   *arr;
  pmath_t                               result;
  pmath_t                               options;
  
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
  
  memset(&data, 0, sizeof(data));
  data.settings.size = sizeof(data.settings);
  data.settings.data = &data;
  data.settings.add_debug_info = add_string_debug_info;
  
  if(!handle_ignoresyntaxerrors_option(&ignore_errors, options)) {
    pmath_unref(options);
    pmath_unref(code);
    pmath_unref(data.token_decorator);
    pmath_unref(expr);
    return pmath_ref(PMATH_SYMBOL_FAILED);
  }
  
  handle_tokens_option(&data, options);
  
  if(!handle_whitespace_option(&data, options)) {
    pmath_unref(options);
    pmath_unref(code);
    pmath_unref(data.token_decorator);
    pmath_unref(expr);
    return pmath_ref(PMATH_SYMBOL_FAILED);
  }
  
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
          
  if(error_flag && !ignore_errors) {
    pmath_span_array_free(arr);
    pmath_unref(code);
    pmath_unref(data.token_decorator);
    return pmath_ref(PMATH_SYMBOL_FAILED);
  }
  
  
  data.debug_source = pmath_expr_new_extended(
                        pmath_ref(PMATH_SYMBOL_RULE), 2,
                        pmath_ref(PMATH_SYMBOL_STRING),
                        pmath_ref(code));
                        
  result = pmath_boxes_from_spans_ex(arr, code, &data.settings);
  
  pmath_span_array_free(arr);
  pmath_unref(code);
  pmath_unref(data.token_decorator);
  pmath_unref(data.debug_source);
  return result;
}
