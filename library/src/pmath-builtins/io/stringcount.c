#include <pmath-core/numbers.h>

#include <pmath-language/regex-private.h>

#include <pmath-util/concurrency/threads.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/memory.h>
#include <pmath-util/messages.h>
#include <pmath-util/option-helpers.h>

#include <pmath-builtins/all-symbols-private.h>

#include <pcre.h>


extern pmath_symbol_t pmath_System_False;
extern pmath_symbol_t pmath_System_IgnoreCase;
extern pmath_symbol_t pmath_System_List;
extern pmath_symbol_t pmath_System_Overlaps;
extern pmath_symbol_t pmath_System_True;

static pmath_t stringcount(
  pmath_t            obj,      // will be freed
  struct _regex_t   *regex,
  struct _capture_t *capture,
  pmath_bool_t       overlaps
) {
  if(pmath_is_string(obj)) {
    int offset = 0;
    size_t count = 0;
    
    while(!pmath_aborting()) {
      if(!_pmath_regex_match(
            regex,
            obj,
            offset,
            PCRE_NO_UTF16_CHECK,
            capture,
            NULL))
      {
        break;
      }
      
      ++count;
      
      if(overlaps || capture->ovector[0] == capture->ovector[1])
        offset = capture->ovector[0] + 1;
      else
        offset = capture->ovector[1];
    }
    
    pmath_unref(obj);
    return pmath_integer_new_uiptr(count);
  }
  
  if(pmath_is_expr_of(obj, pmath_System_List)) {
    size_t i;
    for(i = 1; i <= pmath_expr_length(obj); ++i) {
      pmath_t item = pmath_expr_extract_item(obj, i);
      
      item = stringcount(item, regex, capture, overlaps);
      
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


PMATH_PRIVATE pmath_t builtin_stringcount(pmath_expr_t expr) {
  /* StringCount("string", "sub")
     StringCount("string", patt)
     StringCount({s1, s2, ...}, p)
   */
  pmath_expr_t options;
  pmath_bool_t overlaps;
  pmath_t obj;
  struct _regex_t *regex;
  struct _capture_t capture;
  int regex_options;
  
  if(pmath_expr_length(expr) < 2) {
    pmath_message_argxxx(pmath_expr_length(expr), 2, 2);
    return expr;
  }
  
  
  options = pmath_options_extract(expr, 2);
  if(pmath_is_null(options))
    return expr;
    
  regex_options = 0;
  obj = pmath_evaluate(pmath_option_value(PMATH_NULL, pmath_System_IgnoreCase, options));
  if(pmath_same(obj, pmath_System_True)) {
    regex_options |= PCRE_CASELESS;
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
  
  overlaps = FALSE;
  obj = pmath_evaluate(pmath_option_value(PMATH_NULL, pmath_System_Overlaps, options));
  if(pmath_same(obj, pmath_System_True)) {
    overlaps = TRUE;
  }
  else if(!pmath_same(obj, pmath_System_False)) {
    pmath_message(
      PMATH_NULL, "opttf", 2,
      pmath_ref(pmath_System_Overlaps),
      obj);
    pmath_unref(options);
    return expr;
  }
  pmath_unref(obj);
  pmath_unref(options);
  
  
  obj = pmath_expr_get_item(expr, 2);
  regex = _pmath_regex_compile(obj, regex_options);
  
  if(!regex)
    return expr;
    
  obj = PMATH_NULL;
  _pmath_regex_init_capture(regex, &capture);
  if(capture.ovector) {
    obj = pmath_expr_get_item(expr, 1);
    obj = stringcount(obj, regex, &capture, overlaps);
  }
  
  _pmath_regex_free_capture(&capture);
  _pmath_regex_unref(regex);
  
  if(pmath_same(obj, PMATH_UNDEFINED)) {
    pmath_message(PMATH_NULL, "strse", 2, PMATH_FROM_INT32(1), pmath_ref(expr));
    return expr;
  }
  
  pmath_unref(expr);
  return obj;
}
