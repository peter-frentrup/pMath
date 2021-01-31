#define ICONV_CONST

#include <pmath-core/expressions-private.h>
#include <pmath-core/packed-arrays.h>

#include <pmath-util/concurrency/threads.h>
#include <pmath-util/debug.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/memory.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>

#include <errno.h>
#include <iconv.h>

#ifndef iconv_errno
#  define iconv_errno  errno
#endif


extern pmath_symbol_t pmath_System_List;
extern pmath_symbol_t pmath_System_None;

static pmath_expr_t string_to_utf16_codes(pmath_string_t str, iconv_t dummy) { // str will be freed
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

static pmath_expr_t string_to_utf32_codes(pmath_string_t str, iconv_t dummy) { // str will be freed
  const uint16_t *buf = pmath_string_buffer(&str);
  int             len = pmath_string_length(str);
  size_t size = (size_t)len;
  pmath_packed_array_t result = pmath_packed_array_new(PMATH_NULL, PMATH_PACKED_INT32, 1, &size, NULL, 0);
  int32_t *data_start = pmath_packed_array_begin_write(&result, NULL, 0);
  int32_t *data = data_start;
  pmath_t tmp;
  
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
  tmp = pmath_expr_get_item_range(result, 1, data - data_start);
  pmath_unref(result);
  return tmp;
}

static pmath_expr_t append_bytes(pmath_expr_t expr, char *bytes, size_t num_bytes) {
  size_t index;
  
  if(num_bytes == 0)
    return expr;
    
  if(pmath_expr_length(expr) == 0) {
    index = 1;
    pmath_unref(expr);
    expr = pmath_packed_array_new(PMATH_NULL, PMATH_PACKED_INT32, 1, &num_bytes, NULL, 0);
  }
  else {
    index = pmath_expr_length(expr);
    expr = pmath_expr_resize(expr, index + num_bytes);
    ++index;
  }
  
  // we created expr, so it is either an INT32 packed array of dimension 1 or a non-packed list
  if(pmath_is_packed_array(expr)) {
    int32_t *data = pmath_packed_array_begin_write(&expr, &index, 1);
    if(!data) { // out of memory
      pmath_unref(expr);
      return PMATH_NULL;
    }
    
    while(num_bytes-- > 0) {
      *data++ = (int32_t)(unsigned char)(*bytes++);
    }
  }
  else {
    for(; num_bytes > 0; --num_bytes, ++index, ++bytes) {
      expr = pmath_expr_set_item(expr, index, PMATH_FROM_INT32((int32_t)(unsigned char)(*bytes)));
    }
  }
  
  return expr;
}

static pmath_expr_t string_to_iconv_bytes(pmath_string_t str, iconv_t cd) {
  pmath_expr_t result = pmath_ref(_pmath_object_emptylist);
  int             len = pmath_string_length(str);
  size_t inbytesleft;
  char *inbuf;
  char buffer[128];
  
  inbytesleft = sizeof(uint16_t) * (size_t)len;
  inbuf  = (char *)pmath_string_buffer(&str);
  
  while(inbytesleft > 0) {
    char *outbuf = buffer;
    size_t outbytesleft = sizeof(buffer);
    
    size_t ret = iconv(cd, &inbuf, &inbytesleft, &outbuf, &outbytesleft);
    result = append_bytes(result, buffer, outbuf - buffer);
    
    if(ret == (size_t) - 1) {
      int err = iconv_errno;
      if(err != E2BIG && err != EILSEQ && err != EINVAL) {
        pmath_debug_print("[string_to_iconv_bytes: unknown errno %d from failed iconv()]\n", err);
      
        if(inbytesleft < 4)
          err = EINVAL;
        else if(outbytesleft < 4)
          err = E2BIG;
        else 
          err = EILSEQ;
      }
      
      if(err == EILSEQ) { // invalid input byte
        result = pmath_expr_append(result, 1, pmath_ref(pmath_System_None));
        
        ++inbuf;
        --inbytesleft;
      }
      else if(err == EINVAL) { // incomplete input
        result = pmath_expr_append(result, 1, pmath_ref(pmath_System_None));
        break;
      }
      else if(err == E2BIG) { // output buffer too small
        // already flushed buffer
        
        if(pmath_aborting())
          break;
      }
    }
  }
  
  pmath_unref(str);
  return result;
}

PMATH_PRIVATE pmath_t builtin_tocharactercode(pmath_expr_t expr) {
  /* ToCharacterCode(string)
     ToCharacterCode(string, encoding)
   */
  pmath_t code;
  size_t exprlen;
  pmath_expr_t (*converter)(pmath_string_t, iconv_t) = string_to_utf16_codes;
  iconv_t cd = NULL;
  
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
      else {
        char *to_code = pmath_string_to_utf8(code, NULL);
        cd = iconv_open(to_code, PMATH_BYTE_ORDER < 0 ? "UTF-16LE" : "UTF-16BE");
        if(cd)
          converter = string_to_iconv_bytes;
        
        pmath_mem_free(to_code);
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
    expr = converter(code, cd);
    if(cd)
      iconv_close(cd);
    return expr;
  }
  
  if(cd)
    iconv_close(cd);
    
  if(pmath_is_expr_of(code, pmath_System_List)) {
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
