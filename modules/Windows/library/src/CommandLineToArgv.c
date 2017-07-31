#include "util.h"

#include <pmath.h>


// the inverse of CommandLineToArgvW
pmath_t try_compose_arguments(pmath_expr_t list) { // frees list; return a string or PMATH_UNDEFINED
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
      int first_slash;
      int next_quote = start;
      while(next_quote < len && buf[next_quote] != '\"')
        ++next_quote;
        
      first_slash = next_quote;
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

pmath_t windows_CommandLineToArgv(pmath_expr_t expr) {
  /* CommandLineToArgv(string)
   */
  pmath_string_t str;
  int i;
  int argc;
  wchar_t **argv;
  
  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  str = pmath_expr_get_item(expr, 1);
  if(!pmath_is_string(str)) {
    pmath_message(PMATH_NULL, "str", 2, PMATH_FROM_INT32(1), pmath_ref(expr));
    pmath_unref(str);
    return expr;
  }
  
  pmath_unref(expr);
  str = pmath_string_insert_latin1(str, INT_MAX, "", 1); // zero-terminate
  argv = CommandLineToArgvW(pmath_string_buffer(&str), &argc);
  pmath_unref(str);
  
  if(!argv) {
    check_succeeded_win32(GetLastError());
    return pmath_ref(PMATH_SYMBOL_FAILED);
  }
  
  expr = pmath_expr_new(pmath_ref(PMATH_SYMBOL_LIST), (size_t)argc);
  for(i = 0;i < argc;++i) {
    str = pmath_string_insert_ucs2(PMATH_NULL, 0, argv[i], -1);
    expr = pmath_expr_set_item(expr, (size_t)i + 1, str);
  }
  
  LocalFree(argv);
  return expr;
}
