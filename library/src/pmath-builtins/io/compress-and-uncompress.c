#include <pmath-util/compression.h>
#include <pmath-util/messages.h>
#include <pmath-util/files/mixed-buffer.h>
#include <pmath-util/serialize.h>

#include <pmath-builtins/all-symbols-private.h>


PMATH_PRIVATE pmath_t builtin_compress(pmath_expr_t expr) {
  pmath_t obj, bfile, zfile, tfile;
  pmath_serialize_error_t err;
  
  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  obj = pmath_expr_get_item(expr, 1);
  
  pmath_file_create_mixed_buffer("base85", &tfile, &bfile);
  zfile = pmath_file_create_compressor(pmath_ref(bfile), NULL);
  err = pmath_serialize(zfile, obj, 0);
  pmath_file_close(zfile);
  pmath_file_close(bfile);
  
  if(err != PMATH_SERIALIZE_OK) {
    pmath_file_close(tfile);
    return expr;
  }
  
  pmath_unref(expr);
  
  expr = pmath_file_readline(tfile);
  pmath_file_close(tfile);
  return expr;
}

PMATH_PRIVATE pmath_t builtin_uncompress(pmath_expr_t expr) {
/* Uncompress("data")
   Uncompress("data", head)
 */
  pmath_t obj, str, bfile, tfile, zfile;
  pmath_serialize_error_t err;
  size_t exprlen = pmath_expr_length(expr);
  
  if(exprlen < 1 || exprlen > 2) {
    pmath_message_argxxx(exprlen, 1, 2);
    return expr;
  }
  
  str = pmath_expr_get_item(expr, 1);
  if(!pmath_is_string(str)) {
    pmath_message(PMATH_NULL, "str", 2, PMATH_FROM_INT32(1), pmath_ref(expr));
    pmath_unref(str);
    return expr;
  }
  
  pmath_file_create_mixed_buffer("base85", &tfile, &bfile);
  
  pmath_file_writetext(tfile, pmath_string_buffer(&str), pmath_string_length(str));
  pmath_file_close(tfile);
  
  zfile = pmath_file_create_decompressor(pmath_ref(bfile), NULL);
  obj = pmath_deserialize(zfile, &err);
  pmath_file_close(zfile);
  pmath_file_close(bfile);
  
  if(err != PMATH_SERIALIZE_OK) {
    if(err != PMATH_SERIALIZE_NO_MEMORY)
      pmath_message(PMATH_NULL, "corrupt", 1, str);
    pmath_unref(obj);
    return expr;
  }
  
  if(exprlen == 2){
    pmath_t head = pmath_expr_get_item(expr, 2);
    obj = pmath_expr_new_extended(head, 1, obj);
  }
  
  pmath_unref(str);
  pmath_unref(expr);
  return obj;
}
