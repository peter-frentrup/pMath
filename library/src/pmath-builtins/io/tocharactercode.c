#include <pmath-core/packed-arrays.h>

#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>


static pmath_expr_t string_to_utf16_codes(pmath_string_t str) { // str will be freed
  const uint16_t *buf = pmath_string_buffer(&str);
  int             len = pmath_string_length(str);
  int i;
  size_t size = (size_t)len;
  pmath_packed_array_t result = pmath_packed_array_new(PMATH_NULL, PMATH_PACKED_INT32, 1, &size, NULL, 0);
  int32_t *data = pmath_packed_array_begin_write(&result, NULL, 0);
  if(!data) {
    pmath_unref(str);
    pmath_unref(result);
    return PMATH_NULL;
  }
  
  for(i = 0; i < len; ++i) {
    data[i] = (int32_t)buf[i];
  }
  
  pmath_unref(str);
  return result;
}

static pmath_expr_t string_to_utf32_codes(pmath_string_t str) { // str will be freed
  const uint16_t *buf = pmath_string_buffer(&str);
  int             len = pmath_string_length(str);
  size_t size = (size_t)len;
  pmath_packed_array_t result = pmath_packed_array_new(PMATH_NULL, PMATH_PACKED_INT32, 1, &size, NULL, 0);
  int32_t *data_start = pmath_packed_array_begin_write(&result, NULL, 0);
  int32_t *data = data_start;
  if(!data_start) {
    pmath_unref(str);
    pmath_unref(result);
    return PMATH_NULL;
  }
  
  while(len-- > 0) {
    if( len > 0 &&
        (buf[0] & 0xFC00) == 0xD800 &&
        (buf[1] & 0xFC00) == 0xDC00)
    {
      *data++ = 0x10000 + ((((int32_t)buf[0] & 0x03FF) << 10) | (buf[1] & 0x03FF));
      buf += 2;
      --len;
    }
    else {
      *data++ = (int32_t)(*buf++);
    }
  }
  
  pmath_unref(str);
  return pmath_expr_get_item_range(result, 1, data - data_start);
}

PMATH_PRIVATE pmath_t builtin_tocharactercode(pmath_expr_t expr) {
  /* ToCharacterCode(string)
     ToCharacterCode(string, encoding)
   */
  pmath_t code;
  size_t exprlen;
  pmath_expr_t (*converter)(pmath_string_t) = string_to_utf16_codes;
  
  exprlen = pmath_expr_length(expr);
  if(exprlen < 1 || exprlen > 2) {
    pmath_message_argxxx(exprlen, 1, 2);
    return expr;
  }
  
  if(exprlen > 1) {
    converter = NULL;
    code = pmath_expr_get_item(expr, 2);
    if(pmath_is_string(code)) {
      if(pmath_string_equals_latin1(code, "UTF-16")) {
        converter = string_to_utf16_codes;
      }
      else if(pmath_string_equals_latin1(code, "UTF-32") ||
              pmath_string_equals_latin1(code, "Unicode"))
      {
        converter = string_to_utf32_codes;
      }
    }
    
    if(!converter) {
      pmath_message(PMATH_NULL, "charcode", 1, code);
      return expr;
    }
    pmath_unref(code);
  }
  
  code = pmath_expr_get_item(expr, 1);
  if(pmath_is_string(code)) {
    pmath_unref(expr);
    return converter(code);
  }
  
  if(pmath_is_expr_of(code, PMATH_SYMBOL_LIST)) {
    size_t i;
    for(i = pmath_expr_length(code); i > 0; --i) {
      pmath_t item = pmath_expr_extract_item(code, i);
      
      expr = pmath_expr_set_item(expr, 1, item);
      item = pmath_evaluate(pmath_ref(expr));
      
      code = pmath_expr_set_item(code, i, item);
    }
    
    pmath_unref(expr);
    return code;
  }
  
  pmath_message(PMATH_NULL, "strse", 2, PMATH_FROM_INT32(1), pmath_ref(expr));
  pmath_unref(code);
  return expr;
}
