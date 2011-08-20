#include <pmath-core/numbers-private.h>

#include <pmath-language/regex-private.h>
#include <pmath-language/scanner.h>

#include <pmath-util/concurrency/threads.h>
#include <pmath-util/emit-and-gather.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/files.h>
#include <pmath-util/helpers.h>
#include <pmath-util/memory.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/control-private.h>
#include <pmath-builtins/io-private.h>

#define PCRE_STATIC
#include <pcre.h>

PMATH_PRIVATE pmath_t builtin_find(pmath_expr_t expr) {
  /* Find(file, regex)
  
     options
      IgnoreCase->False
   */
  pmath_expr_t options;
  pmath_t file, obj;
  int pcre_options;
  struct _regex_t   *regex;
  struct _capture_t  capture;
  
  if(pmath_expr_length(expr) < 2) {
    pmath_message_argxxx(pmath_expr_length(expr), 2, 2);
    return expr;
  }
  
  options = pmath_options_extract(expr, 2);
  if(pmath_is_null(options))
    return expr;
    
  pcre_options = 0;
  obj = pmath_option_value(PMATH_NULL, PMATH_SYMBOL_IGNORECASE, options);
  pmath_unref(options);
  if(pmath_same(obj, PMATH_SYMBOL_TRUE)) {
    pcre_options |= PCRE_CASELESS;
  }
  else if(!pmath_same(obj, PMATH_SYMBOL_FALSE)) {
    pmath_message(PMATH_NULL, "opttf", 2, pmath_ref(PMATH_SYMBOL_IGNORECASE), obj);
    return expr;
  }
  pmath_unref(obj);
  
  
  regex = _pmath_regex_compile(pmath_expr_get_item(expr, 2), pcre_options);
  if(!regex)
    return expr;
    
  file = pmath_expr_get_item(expr, 1);
  if(!_pmath_file_check(file, PMATH_FILE_PROP_READ | PMATH_FILE_PROP_TEXT)) {
    pmath_unref(file);
    pmath_unref(expr);
    _pmath_regex_unref(regex);
    return pmath_ref(PMATH_SYMBOL_FAILED);
  }
  
  pmath_unref(expr);
  
  obj = PMATH_NULL;
  if(_pmath_regex_init_capture(regex, &capture)) {
    while(!pmath_aborting()) {
      int length;
      char *utf8;
      
      if(pmath_file_status(file) != PMATH_FILE_OK) {
        obj = pmath_ref(PMATH_SYMBOL_ENDOFFILE);
        break;
      }
      
      obj = pmath_file_readline(file);
      utf8 = pmath_string_to_utf8(obj, &length);
      if(utf8) {
        if(_pmath_regex_match(regex, utf8, length, 0, 0, &capture, NULL)) {
          pmath_mem_free(utf8);
          break;
        }
        
        pmath_mem_free(utf8);
      }
      pmath_unref(obj);
    }
    
    _pmath_regex_free_capture(&capture);
  }
  
  _pmath_regex_unref(regex);
  pmath_unref(file);
  return obj;
}

PMATH_PRIVATE pmath_t builtin_findlist(pmath_expr_t expr) {
  /* FindList(file, regex, n)
     FindList(file, regex)   =   FindList(file, regex, Infinity)
  
     options
      IgnoreCase->False
   */
  pmath_expr_t options;
  pmath_t file, obj;
  size_t last_nonoption, count;
  int pcre_options;
  struct _regex_t   *regex;
  struct _capture_t  capture;
  
  last_nonoption = 2;
  count = SIZE_MAX;
  if(pmath_expr_length(expr) >= 3) {
    pmath_t n = pmath_expr_get_item(expr, 3);
    
    if(!_pmath_is_rule(n) && !_pmath_is_list_of_rules(n)) {
      last_nonoption = 3;
      if(pmath_is_int32(n) && PMATH_AS_INT32(n) >= 0) {
        count = (size_t)PMATH_AS_INT32(n);
      }
      else if(!pmath_equals(n, _pmath_object_infinity)) {
        pmath_message(PMATH_NULL, "intnm", 2, PMATH_FROM_INT32(3), pmath_ref(expr));
        
        pmath_unref(n);
        return expr;
      }
    }
    
    pmath_unref(n);
  }
  else if(pmath_expr_length(expr) < 2) {
    pmath_message_argxxx(pmath_expr_length(expr), 2, 3);
    return expr;
  }
  
  options = pmath_options_extract(expr, last_nonoption);
  if(pmath_is_null(options))
    return expr;
    
  pcre_options = 0;
  obj = pmath_option_value(PMATH_NULL, PMATH_SYMBOL_IGNORECASE, options);
  pmath_unref(options);
  if(pmath_same(obj, PMATH_SYMBOL_TRUE)) {
    pcre_options |= PCRE_CASELESS;
  }
  else if(!pmath_same(obj, PMATH_SYMBOL_FALSE)) {
    pmath_message(PMATH_NULL, "opttf", 2, pmath_ref(PMATH_SYMBOL_IGNORECASE), obj);
    return expr;
  }
  pmath_unref(obj);
  
  
  regex = _pmath_regex_compile(pmath_expr_get_item(expr, 2), pcre_options);
  if(!regex)
    return expr;
    
  file = pmath_expr_get_item(expr, 1);
  if(pmath_is_string(file)) {
    file = pmath_evaluate(pmath_parse_string_args(
                            "Try(OpenRead(`1`))", "(o)", file));
  }
  
  if(!_pmath_file_check(file, PMATH_FILE_PROP_READ | PMATH_FILE_PROP_TEXT)) {
    pmath_unref(file);
    pmath_unref(expr);
    _pmath_regex_unref(regex);
    return pmath_ref(PMATH_SYMBOL_FAILED);
  }
  
  pmath_unref(expr);
  
  pmath_gather_begin(PMATH_NULL);
  if(_pmath_regex_init_capture(regex, &capture)) {
    while(count > 0 && pmath_file_status(file) == PMATH_FILE_OK) {
      int length;
      char *utf8;
      
      obj = pmath_file_readline(file);
      utf8 = pmath_string_to_utf8(obj, &length);
      if(utf8) {
        if(_pmath_regex_match(regex, utf8, length, 0, 0, &capture, NULL)) {
          pmath_emit(obj, PMATH_NULL);
          obj = PMATH_NULL;
          --count;
        }
        
        pmath_mem_free(utf8);
      }
      pmath_unref(obj);
    }
    
    _pmath_regex_free_capture(&capture);
  }
  
  _pmath_regex_unref(regex);
  pmath_unref(file);
  return pmath_gather_end();
}
