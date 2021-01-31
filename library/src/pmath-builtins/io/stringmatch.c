#include <pmath-core/expressions.h>
#include <pmath-core/numbers.h>

#include <pmath-language/regex-private.h>

#include <pmath-util/helpers.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/memory.h>
#include <pmath-util/messages.h>
#include <pmath-util/option-helpers.h>

#include <pmath-builtins/all-symbols-private.h>

#include <pcre.h>


extern pmath_symbol_t pmath_System_EndOfString;
extern pmath_symbol_t pmath_System_False;
extern pmath_symbol_t pmath_System_IgnoreCase;
extern pmath_symbol_t pmath_System_List;
extern pmath_symbol_t pmath_System_StartOfString;
extern pmath_symbol_t pmath_System_StringExpression;
extern pmath_symbol_t pmath_System_True;

static pmath_t stringmatch(
  pmath_t          obj,      // will be freed
  struct _regex_t *regex
) {
  if(pmath_is_string(obj)) {
    pmath_bool_t result = FALSE;
    struct _capture_t capture;
    
    _pmath_regex_init_capture(regex, &capture);
    
    if(capture.ovector) {
      result = _pmath_regex_match(
                 regex,
                 obj,
                 0,
                 PCRE_NO_UTF16_CHECK,
                 &capture,
                 NULL);
    }
    
    pmath_unref(obj);
    _pmath_regex_free_capture(&capture);
    return pmath_ref(result ? pmath_System_True : pmath_System_False);
  }
  
  if(pmath_is_expr_of(obj, pmath_System_List)) {
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
     
     The pattern is embraced in StartOfString ++ ... ++ EndOfString.
   */
  pmath_expr_t options;
  pmath_t obj;
  struct _regex_t *regex;
  int pcre_options;
  
  if(pmath_expr_length(expr) < 2) {
    pmath_message_argxxx(pmath_expr_length(expr), 2, 2);
    return expr;
  }
  
  options = pmath_options_extract(expr, 2);
  if(pmath_is_null(options))
    return expr;
    
  pcre_options = 0;
  obj = pmath_evaluate(pmath_option_value(PMATH_NULL, pmath_System_IgnoreCase, options));
  if(pmath_same(obj, pmath_System_True)) {
    pcre_options |= PCRE_CASELESS;
  }
  else if(!pmath_same(obj, pmath_System_False)) {
    pmath_message(
      PMATH_NULL, "opttf", 2,
      pmath_ref(pmath_System_IgnoreCase),
      obj);
    pmath_unref(options);
    return expr;
  }
  pmath_unref(obj);
  pmath_unref(options);
  
  obj = pmath_expr_get_item(expr, 2);
  obj = pmath_expr_new_extended(
          pmath_ref(pmath_System_StringExpression), 3,
          pmath_ref(pmath_System_StartOfString), 
          obj,
          pmath_ref(pmath_System_EndOfString));
  
  regex = _pmath_regex_compile(obj, pcre_options);
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
