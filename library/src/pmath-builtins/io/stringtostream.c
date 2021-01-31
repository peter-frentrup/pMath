#include <pmath-language/scanner.h>

#include <pmath-util/evaluation.h>
#include <pmath-util/files/mixed-buffer.h>
#include <pmath-util/messages.h>
#include <pmath-util/option-helpers.h>

#include <pmath-builtins/io-private.h>
#include <pmath-builtins/all-symbols-private.h>


extern pmath_symbol_t pmath_System_DollarFailed;
extern pmath_symbol_t pmath_System_InputStream;
extern pmath_symbol_t pmath_System_PageWidth;

PMATH_PRIVATE pmath_t builtin_stringtostream(pmath_expr_t expr) {
  /* StringToStream(text)
    
      options:
        PageWidth -> Infinity
   */
  pmath_expr_t options;
  pmath_string_t text;
  pmath_symbol_t text_stream;
  pmath_symbol_t bin_stream;
  pmath_t page_width = PMATH_NULL;
  
  if(pmath_expr_length(expr) < 1) {
    pmath_message_argxxx(0, 1, 1);
    return expr;
  }
  
  options = pmath_options_extract(expr, 1);
  if(pmath_is_null(options))
    return expr;
  
  page_width = pmath_evaluate(
                 pmath_option_value(PMATH_NULL, pmath_System_PageWidth, options));
  pmath_unref(options);
  
  text = pmath_expr_get_item(expr, 1);
  if(!pmath_is_string(text)) {
    pmath_unref(page_width);
    pmath_message(PMATH_NULL, "str", 2, PMATH_FROM_INT32(1), pmath_ref(expr));
    pmath_unref(text);
    return expr;
  }
  
  pmath_file_create_mixed_buffer("latin1", &text_stream, &bin_stream);
  pmath_unref(bin_stream);
  
  pmath_unref(expr);
  if(pmath_is_null(text_stream)) {
    pmath_unref(page_width);
    pmath_unref(text);
    return pmath_ref(pmath_System_DollarFailed);
  }
  
  // TODO: do not copy content via write but reference text directly
  // maybe pmath_file_set_textbuffer() suffeces, because we threw away the binary file.
  pmath_file_writetext(text_stream, pmath_string_buffer(&text), pmath_string_length(text));
  pmath_unref(text);
  
  
  PMATH_RUN_ARGS(
    "Unprotect(`1`);"
    "Options(`1`):= Union("
    " Options(`1`),"
    " If(`2` =!= /\\/, {PageWidth->`2`}, {}));"
    "Protect(`1`)",
    "(oooo)",
    pmath_ref(text_stream),
    pmath_ref(page_width));
  
  pmath_unref(page_width);
  return pmath_expr_new_extended(pmath_ref(pmath_System_InputStream), 1, text_stream);
}
