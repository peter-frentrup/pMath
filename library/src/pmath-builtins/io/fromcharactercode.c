#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>

#include <limits.h>


static pmath_string_t from_character_code(pmath_t code) { // code will be freed
  unsigned unichar;
  
  if(!pmath_is_int32(code) || PMATH_AS_INT32(code) < 0) {
    pmath_unref(code);
    return PMATH_NULL;
  }
  
  unichar = (unsigned)PMATH_AS_INT32(code);
  if(unichar <= 0xFFFF) {
    uint16_t u16 = unichar;
    
    return pmath_string_insert_ucs2(PMATH_NULL, 0, &u16, 1);
  }
  
  if(unichar <= 0x10FFFF) {
    uint16_t u16[2];
    
    unichar -= 0x10000;
    u16[0] = 0xD800 | (unichar >> 10);
    u16[1] = 0xDC00 | (unichar & 0x3FF);
    
    return pmath_string_insert_ucs2(PMATH_NULL, 0, u16, 2);
  }
  
  return PMATH_NULL;
}


PMATH_PRIVATE pmath_t builtin_fromcharactercode(pmath_expr_t expr) {
  pmath_t code;
  
  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  code = pmath_expr_get_item(expr, 1);
  if(pmath_is_expr_of(code, PMATH_SYMBOL_LIST)) {
    size_t len = pmath_expr_length(code);
    pmath_t item;
    
    if(len == 0) {
      pmath_unref(expr);
      pmath_unref(code);
      return pmath_string_new(0);
    }
    
    item = pmath_expr_get_item(code, 1);
    if(pmath_is_int32(item) && len < INT_MAX) {
      pmath_string_t str = pmath_string_new((int)len);
      size_t i;
      
      for(i = 1; i <= pmath_expr_length(code); ++i) {
        pmath_string_t sub = from_character_code(pmath_expr_get_item(code, i));
        
        if(pmath_is_null(sub)) {
          pmath_message(PMATH_NULL, "ilsmn", 2, PMATH_FROM_INT32(1), pmath_ref(expr));
          pmath_unref(str);
          pmath_unref(code);
          return expr;
        }
        
        str = pmath_string_concat(str, sub);
      }
      
      pmath_unref(code);
      pmath_unref(expr);
      return str;
    }
    
    if(pmath_is_expr_of(item, PMATH_SYMBOL_LIST)) {
      size_t i;
      
      pmath_unref(item);
      for(i = len; i > 0; --i) {
        item = pmath_expr_extract_item(code, i);
        expr = pmath_expr_set_item(expr, 1, item);
        
        item = pmath_evaluate(pmath_ref(expr));
        code = pmath_expr_set_item(code, i, item);
      }
      
      pmath_unref(expr);
      return code;
    }
    
    pmath_unref(item);
  }
  else {
    pmath_string_t str = from_character_code(code);
    if(pmath_is_null(str)) {
      pmath_message(PMATH_NULL, "ilsmn", 2, PMATH_FROM_INT32(1), pmath_ref(expr));
      return expr;
    }
    
    pmath_unref(expr);
    return str;
  }
  
  pmath_unref(code);
  return expr;
}
