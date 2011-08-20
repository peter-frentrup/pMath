#include <pmath-util/files.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/io-private.h>


static const uint16_t newline = '\n';

PMATH_PRIVATE pmath_t builtin_write(pmath_expr_t expr) {
  /* Write(file, value1, value2, ...)
   */
  pmath_t file;
  size_t i, exprlen;
  
  exprlen = pmath_expr_length(expr);
  
  if(exprlen < 2) {
    pmath_message_argxxx(exprlen, 2, SIZE_MAX);
    return expr;
  }
  
  file = pmath_expr_get_item(expr, 1);
  if(!_pmath_file_check(file, PMATH_FILE_PROP_WRITE | PMATH_FILE_PROP_TEXT)) {
    pmath_unref(file);
    pmath_unref(expr);
    return pmath_ref(PMATH_SYMBOL_FAILED);
  }
  
  // locking?
  for(i = 2; i <= exprlen; ++i) {
    pmath_t item = pmath_expr_get_item(expr, i);
    
    pmath_file_write_object(file, item, 0);
    
    pmath_unref(item);
  }
  
  pmath_file_writetext(file, &newline, 1);
  pmath_file_flush(file);
  
  pmath_unref(expr);
  pmath_unref(file);
  return PMATH_NULL;
}

PMATH_PRIVATE pmath_t builtin_writestring(pmath_expr_t expr) {
  /* WriteString(file, string1, string2, ...)
   */
  pmath_t file;
  size_t i, exprlen;
  
  exprlen = pmath_expr_length(expr);
  
  if(exprlen < 2) {
    pmath_message_argxxx(exprlen, 2, SIZE_MAX);
    return expr;
  }
  
  file = pmath_expr_get_item(expr, 1);
  if(!_pmath_file_check(file, PMATH_FILE_PROP_WRITE | PMATH_FILE_PROP_TEXT)) {
    pmath_unref(file);
    pmath_unref(expr);
    return pmath_ref(PMATH_SYMBOL_FAILED);
  }
  
  // locking?
  for(i = 2; i <= exprlen; ++i) {
    pmath_t item = pmath_expr_get_item(expr, i);
    
    pmath_file_write_object(file, item, 0);
    
    pmath_unref(item);
  }
  
  pmath_file_flush(file);
  pmath_unref(expr);
  pmath_unref(file);
  return PMATH_NULL;
}
