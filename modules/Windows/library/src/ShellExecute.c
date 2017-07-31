#define _WIN32_WINNT  0x0600

#include "util.h"

#include <pmath.h>
#include <objbase.h>
#include <shlobj.h>

// the inverse of CommandLineToArgvW
static pmath_t try_compose_arguments(pmath_expr_t list) { // frees list; return a string or PMATH_UNDEFINED
  pmath_string_t result = PMATH_NULL;
  size_t i;
  size_t listlen;
  
  listlen = pmath_expr_length(list);
  for(i = 1; i <= listlen; ++i) {
    pmath_string_t arg = pmath_expr_get_item(list, i);
    int len;
    const uint16_t *buf;
    int start;
    
    if(!pmath_is_string(arg)) {
      pmath_unref(arg);
      pmath_unref(result);
      pmath_unref(list);
      return PMATH_UNDEFINED;
    }
    
    result = pmath_string_insert_latin1(result, INT_MAX, "\"", -1);
      
    len = pmath_string_length(arg);
    buf = pmath_string_buffer(&arg);
    start = 0;
    while(start < len) {
      int next_quote = start;
      while(next_quote < len && buf[next_quote] != '\"')
        ++next_quote;
        
      int first_slash = next_quote;
      while(first_slash > start && buf[first_slash - 1] == '\\')
        --first_slash;
      
      result = pmath_string_insert_ucs2(result, INT_MAX, buf + start, first_slash - start);
      result = pmath_string_insert_ucs2(result, INT_MAX, buf + first_slash, next_quote - first_slash);
      result = pmath_string_insert_ucs2(result, INT_MAX, buf + first_slash, next_quote - first_slash);
      if(next_quote < len)
        result = pmath_string_insert_latin1(result, INT_MAX, "\\\"", -1);
      
      start = next_quote + 1;
    }
    
    result = pmath_string_insert_latin1(result, INT_MAX, "\" ", -1);
    pmath_unref(arg);
  }
  
  pmath_unref(list);
  return result;
}


pmath_t windows_ShellExecute(pmath_expr_t expr) {
  /* ShellExecute(verb, file)
     ShellExecute(verb, file, param)
   */
  pmath_string_t verb;
  pmath_string_t filename;
  pmath_string_t parameters;
  pmath_t options;
  SHELLEXECUTEINFOW info;
  
  size_t exprlen = pmath_expr_length(expr);
  if(exprlen < 2) {
    pmath_message_argxxx(exprlen, 2, 3);
  }
  
  verb = pmath_expr_get_item(expr, 1);
  if(pmath_same(verb, PMATH_SYMBOL_AUTOMATIC)) {
    pmath_unref(verb);
    verb = PMATH_NULL;
  }
  else if(!pmath_is_string(verb)) {
    pmath_message(PMATH_NULL, "str", 2, PMATH_FROM_INT32(1), pmath_ref(expr));
    goto FAIL_VERB;
  }
  
  filename = pmath_expr_get_item(expr, 2);
  if(!pmath_is_string(filename)) {
    pmath_message(PMATH_NULL, "str", 2, PMATH_FROM_INT32(2), pmath_ref(expr));
    goto FAIL_FILENAME;
  }
  
  if(exprlen > 2) {
    parameters = pmath_expr_get_item(expr, 3);
    if(pmath_is_string(parameters)) {
      options = pmath_options_extract(expr, 3);
    }
    else if(pmath_same(parameters, PMATH_SYMBOL_NONE)) {
      pmath_unref(parameters);
      parameters = PMATH_NULL;
      options = pmath_options_extract(expr, 3);
    }
    else if(pmath_is_expr_of(parameters, PMATH_SYMBOL_LIST)) {
      parameters = try_compose_arguments(parameters);
      if(pmath_is_string(parameters)) {
        options = pmath_options_extract(expr, 3);
      }
      else {
        pmath_unref(parameters);
        parameters = PMATH_NULL;
        options = pmath_options_extract(expr, 2);
      }
    }
    else {
      pmath_unref(parameters);
      parameters = PMATH_NULL;
      options = pmath_options_extract(expr, 2);
    }
    
    if(pmath_is_null(options))
      goto FAIL_OPTIONS;
  }
  else {
    parameters = PMATH_NULL;
    options = PMATH_UNDEFINED;
  }
  
  verb = pmath_string_insert_latin1(verb, INT_MAX, "", 1);
  filename = pmath_string_insert_latin1(filename, INT_MAX, "", 1);
  
  memset(&info, 0, sizeof(info));
  info.cbSize = sizeof(info);
  info.fMask = SEE_MASK_FLAG_NO_UI | SEE_MASK_NOASYNC;
  info.lpFile = pmath_string_buffer(&filename);
  info.nShow = SW_SHOWNORMAL;
  
  if(!pmath_is_null(verb)) {
    verb = pmath_string_insert_latin1(verb, INT_MAX, "", 1);
    info.lpVerb = pmath_string_buffer(&verb);
  }
  
  if(!pmath_is_null(parameters)) {
    parameters = pmath_string_insert_latin1(parameters, INT_MAX, "", 1);
    info.lpParameters = pmath_string_buffer(&parameters);
  }
  
  if(ShellExecuteExW(&info)) {
    pmath_unref(expr);
    expr = PMATH_NULL;
  }
  else {
    check_succeeded_win32(GetLastError());
    pmath_unref(expr);
    expr = pmath_ref(PMATH_SYMBOL_FAILED);
  }
  
  pmath_unref(options);
FAIL_OPTIONS:
  pmath_unref(parameters);
  pmath_unref(filename);
FAIL_FILENAME:
  pmath_unref(verb);
FAIL_VERB:
  return expr;
}
