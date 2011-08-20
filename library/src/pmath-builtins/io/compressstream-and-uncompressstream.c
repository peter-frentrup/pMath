#include <pmath-util/compression.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>


pmath_t builtin_compressstream(pmath_expr_t expr) {
  pmath_t stream;
  
  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  stream = pmath_expr_get_item(expr, 1);
  if(!pmath_file_test(stream, PMATH_FILE_PROP_WRITE | PMATH_FILE_PROP_BINARY)) {
    if(!pmath_file_test(stream, PMATH_FILE_PROP_BINARY))
      pmath_message(PMATH_NULL, "iob", 1, stream);
    else if(!pmath_file_test(stream, PMATH_FILE_PROP_WRITE))
      pmath_message(PMATH_NULL, "iow", 1, stream);
    else
      pmath_message(PMATH_NULL, "invio", 1, stream);
      
    return expr;
  }
  
  stream = pmath_file_create_compressor(stream);
  if(pmath_is_null(stream)) {
    pmath_message(PMATH_NULL, "invio", 1, pmath_expr_get_item(expr, 1));
    return expr;
  }
  
  pmath_unref(expr);
  return stream;
}

pmath_t builtin_uncompressstream(pmath_expr_t expr) {
  pmath_t stream;
  
  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  stream = pmath_expr_get_item(expr, 1);
  if(!pmath_file_test(stream, PMATH_FILE_PROP_READ | PMATH_FILE_PROP_BINARY)) {
    if(!pmath_file_test(stream, PMATH_FILE_PROP_BINARY))
      pmath_message(PMATH_NULL, "iob", 1, stream);
    else if(!pmath_file_test(stream, PMATH_FILE_PROP_READ))
      pmath_message(PMATH_NULL, "ior", 1, stream);
    else
      pmath_message(PMATH_NULL, "invio", 1, stream);
      
    return expr;
  }
  
  stream = pmath_file_create_uncompressor(stream);
  if(pmath_is_null(stream)) {
    pmath_message(PMATH_NULL, "invio", 1, pmath_expr_get_item(expr, 1));
    return expr;
  }
  
  pmath_unref(expr);
  return stream;
}
