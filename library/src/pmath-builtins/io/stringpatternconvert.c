#include <pmath-language/regex-private.h>

#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>
#include <pmath-util/option-helpers.h>

#include <pcre.h>


extern pmath_symbol_t pmath_System_False;
extern pmath_symbol_t pmath_System_IgnoreCase;
extern pmath_symbol_t pmath_System_True;

PMATH_PRIVATE pmath_t builtin_internal_stringpatternconvert(pmath_expr_t expr) {
  /*  StringPatternConvert(strpat)
      
      Options:
        IgnoreCase->False 
   */
  pmath_t obj;
  
  if(pmath_expr_length(expr) < 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  int regex_options = 0;
  pmath_expr_t options = pmath_options_extract(expr, 1);
  if(pmath_is_null(options))
    return expr;
  
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
  pmath_unref(options);
  
  struct _regex_t *regex = _pmath_regex_compile(pmath_expr_get_item(expr, 1), regex_options);
  if(!regex) 
    return expr;
  
  pmath_unref(expr);
  obj = _pmath_regex_decode(regex);
  
  _pmath_regex_unref(regex);
  return obj;
}
