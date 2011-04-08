#include <pmath-core/numbers.h>

#include <pmath-language/regex-private.h>

#include <pmath-util/emit-and-gather.h>
#include <pmath-util/helpers.h>
#include <pmath-util/memory.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/control-private.h>

#define PCRE_STATIC
#include <pcre.h>

static int utf8_to_utf16_offset(
  const char     *utf8,
  const uint16_t *utf16,
  int             utf16len,
  int             offset
){
  int result = 0;
  
  while(offset > 0 && result < utf16len){
    if((*utf8 & 0xE0) == 0xC0){
      utf8+= 2;
      offset-= 2;
    }
    else if((*utf8 & 0xF0) == 0xE0){
      utf8+= 3;
      offset-= 3;
    }
    else if((*utf8 & 0xF8) == 0xF0){
      utf8+= 4;
      offset-= 4;
    }
    else{
      ++utf8;
      --offset;
    }
    
    if((*utf16 & 0xFC00) == 0xD800)
      result+=2;
    else
      ++result;
  }
  
  return result < utf16len ? result + 1 : utf16len + 1;
}

static pmath_t stringposition(
  pmath_t            obj,      // will be freed
  struct _regex_t   *regex,
  struct _capture_t *capture,
  pmath_bool_t       overlaps,
  size_t             max_matches
){
  if(pmath_is_string(obj)){
    int length, offset;
    const uint16_t *buf = pmath_string_buffer(&obj);
    int buflen          = pmath_string_length(obj);
    char *subject = pmath_string_to_utf8(obj, &length);
    
    if(!subject){
      pmath_unref(obj);
      return PMATH_UNDEFINED;
    }
    
    pmath_gather_begin(PMATH_NULL);
    offset = 0;
    
    while(max_matches > 0
    && _pmath_regex_match(
        regex, 
        subject, 
        length, 
        offset, 
        PCRE_NO_UTF8_CHECK, 
        capture, 
        NULL)
    ){
      int off1 = utf8_to_utf16_offset(subject, buf, buflen, capture->ovector[0]);
      int off2 = utf8_to_utf16_offset(subject, buf, buflen, capture->ovector[1]);
      
      pmath_emit(
        pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_RANGE), 2, 
          PMATH_FROM_INT32(off1), 
          PMATH_FROM_INT32(off2-1)), 
        PMATH_NULL);
      
      --max_matches;
      
      if(overlaps || capture->ovector[0] == capture->ovector[1])
        offset = capture->ovector[0] + 1;
      else
        offset = capture->ovector[1];
    }
    
    pmath_unref(obj);
    pmath_mem_free(subject);
    return pmath_gather_end();
  }
  
  if(pmath_is_expr_of(obj, PMATH_SYMBOL_LIST)){
    size_t i;
    for(i = 1;i <= pmath_expr_length(obj);++i){
      pmath_t item = pmath_expr_get_item(obj, i);
      obj = pmath_expr_set_item(obj, i, PMATH_NULL);
      
      item = stringposition(item, regex, capture, overlaps, max_matches);
      
      if(pmath_same(item, PMATH_UNDEFINED)){
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

PMATH_PRIVATE pmath_t builtin_stringposition(pmath_expr_t expr){
  /* StringPosition(stringOrList, pattern, n)
     StringPosition(s, p)  =  StringPosition(s, p, Infinity)
   */
  pmath_expr_t options;
  pmath_bool_t overlaps;
  pmath_t obj;
  struct _regex_t *regex;
  struct _capture_t capture;
  size_t max_matches;
  size_t last_nonoption;
  int regex_options;
  
  max_matches = SIZE_MAX;
  last_nonoption = 2;
  if(pmath_expr_length(expr) >= 3){
    obj = pmath_expr_get_item(expr, 3);
    
    if(pmath_is_int32(obj) && PMATH_AS_INT32(obj) >= 0){
      max_matches = (size_t)PMATH_AS_INT32(obj);
      pmath_unref(obj);
      last_nonoption = 3;
    }
    else if(!_pmath_is_rule(obj) && !_pmath_is_list_of_rules(obj)){
      pmath_unref(obj);
      pmath_message(PMATH_NULL, "intnm", 2, PMATH_FROM_INT32(3), pmath_ref(expr));
      return expr;
    }
  }
  else if(pmath_expr_length(expr) < 2){
    pmath_message_argxxx(pmath_expr_length(expr), 2, 3);
    return expr;
  }
  
  
  options = pmath_options_extract(expr, last_nonoption);
  if(pmath_is_null(options))
    return expr;
  
  regex_options = 0;
  obj = pmath_option_value(PMATH_NULL, PMATH_SYMBOL_IGNORECASE, options);
  if(pmath_same(obj, PMATH_SYMBOL_TRUE)){
    regex_options|= PCRE_CASELESS;
  }
  else if(!pmath_same(obj, PMATH_SYMBOL_FALSE)){
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
  if(pmath_same(obj, PMATH_SYMBOL_TRUE)){
    overlaps = TRUE;
  }
  else if(!pmath_same(obj, PMATH_SYMBOL_FALSE)){
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
  
  obj = PMATH_NULL;
  _pmath_regex_init_capture(regex, &capture);
  if(capture.ovector){
    obj = pmath_expr_get_item(expr, 1);
    obj = stringposition(obj, regex, &capture, overlaps, max_matches);
  }
  
  _pmath_regex_free_capture(&capture);
  _pmath_regex_unref(regex);
  
  if(pmath_same(obj, PMATH_UNDEFINED)){
    pmath_message(PMATH_NULL, "strse", 2, PMATH_FROM_INT32(1), pmath_ref(expr));
    return expr;
  }
  
  pmath_unref(expr);
  return obj;
}
