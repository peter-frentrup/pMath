#include <pmath-core/numbers.h>
#include <pmath-core/packed-arrays.h>

#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>
#include <pmath-util/memory.h>

#include <pmath-builtins/all-symbols-private.h>

#include <limits.h>


// PMATH_UNDEFINED on error
static pmath_t try_from_unicode(const int32_t *data, size_t length) {
  pmath_string_t str;
  uint16_t tmpbuf[16];
  uint16_t *tmpbufmax = tmpbuf + sizeof(tmpbuf) / sizeof(tmpbuf[0]) - 1;
  uint16_t *tmp = tmpbuf;
  
  if(length >= INT_MAX) {
    pmath_mem_free(pmath_mem_alloc(SIZE_MAX)); // out of memory
    return PMATH_NULL;
  }
  
  str = pmath_string_new((int)length);
  while(length-- > 0) {
    int unichar = *data++;
    if(unichar < 0 || unichar > 0x10FFFF) {
      pmath_unref(str);
      return PMATH_UNDEFINED;
    }
    
    if(tmp >= tmpbufmax) {
      str = pmath_string_insert_ucs2(str, INT_MAX, tmpbuf, (int)(tmp - tmpbuf));
      tmp = tmpbuf;
    }
    
    if(unichar <= 0xFFFF) {
      *tmp++ = (uint16_t)unichar;
    }
    else {
      unichar -= 0x10000;
      *tmp++ = 0xD800 | (unichar >> 10);
      *tmp++ = 0xDC00 | (unichar & 0x3FF);
    }
  }
  
  str = pmath_string_insert_ucs2(str, INT_MAX, tmpbuf, (int)(tmp - tmpbuf));
  return str;
}

static void unicode_error(pmath_t codes) {
  size_t i, length;
  
  if(!pmath_is_expr_of(codes, PMATH_SYMBOL_LIST))
    codes = pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_LIST), 1, codes);
    
  length = pmath_expr_length(codes);
  for(i = 1; i <= length; ++i) {
    pmath_t ch = pmath_expr_get_item(codes, i);
    if(!pmath_is_int32(ch) || PMATH_AS_INT32(ch) < 0 || PMATH_AS_INT32(ch) > 0x10FFFF) {
      pmath_unref(ch);
      
      pmath_message(PMATH_NULL, "nonunicode", 2, pmath_integer_new_uiptr(i), pmath_ref(codes));
      return;
    }
  }
  
  pmath_unref(codes);
}

// PMATH_UNDEFINED on error, codes will be freed
static pmath_string_t try_from_packed_array(pmath_packed_array_t codes) {
  pmath_string_t result;
  
  if( !pmath_is_packed_array(codes) ||
      pmath_packed_array_get_element_type(codes) != PMATH_PACKED_INT32 ||
      pmath_packed_array_get_dimensions(codes) != 1)
  {
    unicode_error(codes);
    pmath_unref(codes);
    return PMATH_UNDEFINED;
  }
  
  result = try_from_unicode(pmath_packed_array_read(codes, NULL, 0), pmath_packed_array_get_sizes(codes)[0]);
  if(pmath_same(result, PMATH_UNDEFINED)) 
    unicode_error(codes);
  
  pmath_unref(codes);
  return result;
}

PMATH_PRIVATE pmath_t builtin_fromcharactercode(pmath_expr_t expr) {
  pmath_t codes;
  pmath_t tmp;
  pmath_bool_t give_list_of_strings;
  size_t i, count;
  
  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  codes = pmath_expr_get_item(expr, 1);
  if(!pmath_is_expr(codes))
    codes = pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_LIST), 1, codes);
    
  if(!pmath_is_expr_of(codes, PMATH_SYMBOL_LIST)) {
    pmath_unref(codes);
    pmath_message(PMATH_NULL, "ilsmn", 2, PMATH_FROM_INT32(1), pmath_ref(expr));
    return expr;
  }
  
  if(pmath_expr_length(codes) == 0) {
    pmath_unref(codes);
    pmath_unref(expr);
    return PMATH_C_STRING("");
  }
  
  tmp = pmath_expr_get_item(codes, 1);
  if(pmath_is_expr_of(tmp, PMATH_SYMBOL_LIST)) {
    give_list_of_strings = TRUE;
  }
  else {
    give_list_of_strings = FALSE;
    codes = pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_LIST), 1, codes);
  }
  pmath_unref(tmp);
  
  count = pmath_expr_length(codes);
  for(i = 1; i <= count; ++i) {
    tmp = pmath_expr_extract_item(codes, i);
    tmp = pmath_to_packed_array(tmp, PMATH_PACKED_INT32);
    tmp = try_from_packed_array(tmp);
    if(pmath_same(tmp, PMATH_UNDEFINED)) {
      pmath_unref(codes);
      return expr;
    }
    codes = pmath_expr_set_item(codes, i, tmp);
  }
  
  pmath_unref(expr);
  if(give_list_of_strings)
    return codes;
  
  tmp = pmath_expr_get_item(codes, 1);
  pmath_unref(codes);
  return tmp;
}
