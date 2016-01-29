#include <pmath-core/numbers.h>

#include <pmath-language/regex-private.h>

#include <pmath-util/emit-and-gather.h>
#include <pmath-util/helpers.h>
#include <pmath-util/memory.h>
#include <pmath-util/messages.h>
#include <pmath-util/option-helpers.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/control-private.h>

#include <pcre.h>


static pmath_t stringcases(
  pmath_t            obj,      // will be freed
  struct _regex_t   *regex,
  struct _capture_t *capture,
  pmath_t            rhs,      // wont be freed
  pmath_bool_t       overlaps,
  size_t             max_matches
) {
  if(pmath_is_string(obj)) {
    int offset = 0;
    pmath_t tmprhs;
    
    pmath_gather_begin(PMATH_NULL);
    tmprhs = pmath_ref(rhs);
    
    while(max_matches > 0) {
      if(!_pmath_regex_match(
            regex,
            obj,
            offset,
            PCRE_NO_UTF16_CHECK,
            capture,
            &tmprhs))
      {
        break;
      }
      
      if(!pmath_same(tmprhs, PMATH_UNDEFINED)) {
        pmath_emit(tmprhs, PMATH_NULL);
        tmprhs = pmath_ref(rhs);
      }
      else {
        pmath_emit(
          pmath_string_part(
            pmath_ref(obj),
            capture->ovector[0],
            capture->ovector[1] - capture->ovector[0]),
          PMATH_NULL);
      }
      
      --max_matches;
      
      if(overlaps || capture->ovector[0] == capture->ovector[1])
        offset = capture->ovector[0] + 1;
      else
        offset = capture->ovector[1];
    }
    
    pmath_unref(tmprhs);
    pmath_unref(obj);
    return pmath_gather_end();
  }
  
  if(pmath_is_expr_of(obj, PMATH_SYMBOL_LIST)) {
    size_t i;
    for(i = 1; i <= pmath_expr_length(obj); ++i) {
      pmath_t item = pmath_expr_extract_item(obj, i);
      
      item = stringcases(item, regex, capture, rhs, overlaps, max_matches);
      
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

PMATH_PRIVATE pmath_t builtin_stringcases(pmath_expr_t expr) {
  /* StringCases(expr, pattern, n)
     StringCases(expr, lhs->rhs, n)
     StringCases(e, p)  =  StringCases(e, p, Infinity)
   */
  pmath_expr_t options;
  pmath_bool_t overlaps;
  pmath_t obj, rhs;
  struct _regex_t *regex;
  struct _capture_t capture;
  size_t max_matches;
  size_t last_nonoption;
  int regex_options;
  
  max_matches = SIZE_MAX;
  last_nonoption = 2;
  if(pmath_expr_length(expr) >= 3) {
    obj = pmath_expr_get_item(expr, 3);
    
    if(pmath_is_int32(obj) && PMATH_AS_INT32(obj) >= 0) {
      max_matches = (size_t)PMATH_AS_INT32(obj);
      pmath_unref(obj);
      last_nonoption = 3;
    }
    else if(!pmath_is_set_of_options(obj)) {
      pmath_unref(obj);
      pmath_message(PMATH_NULL, "intnm", 2, PMATH_FROM_INT32(3), pmath_ref(expr));
      return expr;
    }
  }
  else if(pmath_expr_length(expr) < 2) {
    pmath_message_argxxx(pmath_expr_length(expr), 2, 3);
    return expr;
  }
  
  
  options = pmath_options_extract(expr, last_nonoption);
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
  
  overlaps = FALSE;
  obj = pmath_option_value(PMATH_NULL, PMATH_SYMBOL_OVERLAPS, options);
  if(pmath_same(obj, PMATH_SYMBOL_TRUE)) {
    overlaps = TRUE;
  }
  else if(!pmath_same(obj, PMATH_SYMBOL_FALSE)) {
    pmath_message(
      PMATH_NULL, "opttf", 2,
      pmath_ref(PMATH_SYMBOL_OVERLAPS),
      obj);
    pmath_unref(options);
    return expr;
  }
  pmath_unref(obj);
  pmath_unref(options);
  
  
  obj = pmath_expr_get_item(expr, 2);
  if(_pmath_is_rule(obj)) {
    regex = _pmath_regex_compile(pmath_expr_get_item(obj, 1), regex_options);
    rhs = pmath_expr_get_item(obj, 2);
    pmath_unref(obj);
  }
  else {
    regex = _pmath_regex_compile(obj, regex_options);
    rhs = PMATH_UNDEFINED;
  }
  
  if(!regex) {
    pmath_unref(rhs);
    return expr;
  }
  
  obj = PMATH_NULL;
  _pmath_regex_init_capture(regex, &capture);
  if(capture.ovector) {
    obj = pmath_expr_get_item(expr, 1);
    obj = stringcases(obj, regex, &capture, rhs, overlaps, max_matches);
  }
  
  _pmath_regex_free_capture(&capture);
  _pmath_regex_unref(regex);
  pmath_unref(rhs);
  
  if(pmath_same(obj, PMATH_UNDEFINED)) {
    pmath_message(PMATH_NULL, "strse", 2, PMATH_FROM_INT32(1), pmath_ref(expr));
    return expr;
  }
  
  pmath_unref(expr);
  return obj;
}
