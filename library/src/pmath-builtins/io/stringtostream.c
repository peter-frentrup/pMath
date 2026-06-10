#include <pmath-language/scanner.h>

#include <pmath-util/evaluation.h>
#include <pmath-util/files/mixed-buffer.h>
#include <pmath-util/messages.h>
#include <pmath-util/option-helpers.h>

#include <pmath-builtins/io-private.h>
#include <pmath-builtins/all-symbols-private.h>


extern pmath_symbol_t pmath_System_DollarFailed;
extern pmath_symbol_t pmath_System_InputStream;
extern pmath_symbol_t pmath_System_List;
extern pmath_symbol_t pmath_System_OutputStream;
extern pmath_symbol_t pmath_System_PageWidth;
extern pmath_symbol_t pmath_System_StringToStream;
extern pmath_symbol_t pmath_System_StringToInputOutputStreams;

PMATH_PRIVATE pmath_t builtin_stringtostream(pmath_expr_t expr) {
// StringToStream(text)
// StringToInputOutputStreams(text)
//
// Options:
//  PageWidth -> Infinity
//
// Examples:
//  pmath> f:= StringToStream("abc\[Alpha]\[Beta]\nMore lines\nlast")
//         InputStream(<<>>)
//
//  pmath> f.Read(String)          // InputForm
//         "abc\[Alpha]\[Beta]"
//  pmath> f.ReadList(String)      // InputForm
//         {"More lines", "last"}
//
//  pmath> {fin, fout} := StringToInputOutputStreams("ABC")
//         {InputStream(<<>>), OutputStream(<<>>)}
//  pmath> fin.Read(Character)
//         A
//  pmath> fin.Read(String)
//         BC
//  pmath> fin.Read(String)
//         
//  pmath> fout.WriteString("xyz")
//  pmath> fin.Read({Character, String})
//         {x, yz}
//
  pmath_expr_t options;
  pmath_t head;
  pmath_string_t text;
  pmath_symbol_t text_stream;
  pmath_symbol_t bin_stream;
  pmath_t page_width = PMATH_NULL;
  
  head = pmath_expr_get_item(expr, 0);
  pmath_unref(head);
  
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
  
  pmath_unref(pmath_file_mixed_buffer_swap_text(text_stream, text));
  
  PMATH_RUN_ARGS(
    "Unprotect(`1`);"
    "Options(`1`):= Union("
    " Options(`1`),"
    " If(`2` =!= /\\/, {PageWidth->`2`}, {}));"
    "Protect(`1`)",
    "(oo)",
    pmath_ref(text_stream),
    pmath_ref(page_width));
  
  pmath_unref(page_width);
  
  if(pmath_same(head, pmath_System_StringToStream)) {
    return pmath_expr_new_extended(pmath_ref(pmath_System_InputStream), 1, text_stream);
  }
  else {
    pmath_t copy_of_text_stream = pmath_ref(text_stream);
    return pmath_expr_new_extended(
      pmath_ref(pmath_System_List), 2,
      pmath_expr_new_extended(pmath_ref(pmath_System_InputStream), 1, text_stream),
      pmath_expr_new_extended(pmath_ref(pmath_System_OutputStream), 1, copy_of_text_stream));
  }
}
