#include <pmath-core/expressions.h>
#include <pmath-core/numbers.h>

#include <pmath-language/regex-private.h>

#include <pmath-util/helpers.h>
#include <pmath-util/memory.h>
#include <pmath-util/messages.h>
#include <pmath-util/option-helpers.h>

#include <pmath-builtins/all-symbols-private.h>

#define PCRE_STATIC
#include <pcre.h>


static pmath_t stringmatch(
  pmath_t          obj,      // will be freed
  struct _regex_t *regex
) {
  if(pmath_is_string(obj)) {
    pmath_bool_t result = FALSE;
    struct _capture_t capture;
    int length;
    char *subject = pmath_string_to_utf8(obj, &length);
    
    if(!subject) {
      pmath_unref(obj);
      return PMATH_UNDEFINED;
    }
    
    _pmath_regex_init_capture(regex, &capture);
    
    if(capture.ovector) {
      result = _pmath_regex_match(
                 regex,
                 subject,
                 length,
                 0,
                 PCRE_NO_UTF8_CHECK,
                 &capture,
                 NULL);
    }
    
    pmath_unref(obj);
    _pmath_regex_free_capture(&capture);
    pmath_mem_free(subject);
    return pmath_ref(result ? PMATH_SYMBOL_TRUE : PMATH_SYMBOL_FALSE);
  }
  
  if(pmath_is_expr_of(obj, PMATH_SYMBOL_LIST)) {
    size_t i;
    for(i = 1; i <= pmath_expr_length(obj); ++i) {
      pmath_t item = pmath_expr_get_item(obj, i);
      obj = pmath_expr_set_item(obj, i, PMATH_NULL);
      
      item = stringmatch(item, regex);
      
      if(pmath_same(item, PMATH_UNDEFINED)) {
        pmath_unref(obj);
        return PMATH_UNDEFINED;
      }
      
      obj = pmath_expr_set_item(obj, i, item);
    }
    
    return obj;
  }
  
  pmath_unref(obj);
  return PMATH_UNDEFINED;
}

PMATH_PRIVATE pmath_t builtin_stringmatch(pmath_expr_t expr) {
  /* StringMatch(string, pattern)
   */
  pmath_expr_t options;
  pmath_t obj;
  struct _regex_t *regex;
  int regex_options;
  
  if(pmath_expr_length(expr) < 2) {
    pmath_message_argxxx(pmath_expr_length(expr), 2, 2);
    return expr;
  }
  
  
  options = pmath_options_extract(expr, 2);
  if(pmath_is_null(options))
    return expr;
    
  regex_options = 0;
  obj = pmath_option_value(PMATH_NULL, PMATH_SYMBOL_IGNORECASE, options);
  if(pmath_same(obj, PMATH_SYMBOL_TRUE)) {
    regex_options |= PCRE_CASELESS;
  }
  else if(!pmath_same(obj, PMATH_SYMBOL_FALSE)) {
    pmath_message(
      PMATH_NULL, "opttf", 2,
      pmath_ref(PMATH_SYMBOL_IGNORECASE),
      obj);
    pmath_unref(options);
    return expr;
  }
  pmath_unref(obj);
  pmath_unref(options);
  
  
  regex = _pmath_regex_compile(pmath_expr_get_item(expr, 2), regex_options);
  if(!regex)
    return expr;
    
  obj = pmath_expr_get_item(expr, 1);
  obj = stringmatch(obj, regex);
  
  _pmath_regex_unref(regex);
  
  if(pmath_same(obj, PMATH_UNDEFINED)) {
    pmath_message(PMATH_NULL, "strse", 2, PMATH_FROM_INT32(1), pmath_ref(expr));
    return expr;
  }
  
  pmath_unref(expr);
  return obj;
}
