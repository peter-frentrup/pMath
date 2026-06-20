#include <pmath-builtins/io-private.h>

#include <pmath-util/data-types/byte-arrays.h>
#include <pmath-util/files/binary-buffer.h>
#include <pmath-util/messages.h>

extern pmath_symbol_t pmath_System_DollarFailed;
extern pmath_symbol_t pmath_System_ByteArrayToStream;
extern pmath_symbol_t pmath_System_InputStream;
extern pmath_symbol_t pmath_System_List;
extern pmath_symbol_t pmath_System_OutputStream;


PMATH_PRIVATE pmath_t builtin_bytearraytostream(pmath_expr_t expr) {
// ByteArrayToStream(ba)
// ByteArrayToInputOutputStreams(ba)
//
// Examples:
//  pmath> fin:= ByteArrayToStream(ByteArray({1,2,3}))
//         InputStream(<<>>)
//
//  pmath> Table(fin.BinaryRead(), 4)
//         {1, 2, 3, EndOfFile}
//
//  pmath> {fin, fout} := ByteArrayToInputOutputStreams(ByteArray({}))
//         {InputStream(<<>>), OutputStream(<<>>)}
//
//  pmath> fout.BinaryWrite(f(x, g(y), h(g(y), z)), Expression)
//         OutputStream(<<>>)
//  pmath> bytes:= fin.BinaryReadList
//         {2, 0, 14, 6, 2, 2, 12, 2, 4, 8, 16, 71, 108, 111, 98, 97, 108, 96, 102, 2, 6, 12, 2, 8, 8, 
//          16, 71, 108, 111, 98, 97, 108, 96, 120, 2, 10, 14, 2, 2, 12, 12, 2, 14, 8, 16, 71, 108, 
//          111, 98, 97, 108, 96, 103, 2, 16, 12, 2, 18, 8, 16, 71, 108, 111, 98, 97, 108, 96, 121, 2, 
//          20, 14, 4, 2, 22, 12, 2, 24, 8, 16, 71, 108, 111, 98, 97, 108, 96, 104, 4, 10, 2, 26, 12, 
//          2, 28, 8, 16, 71, 108, 111, 98, 97, 108, 96, 122}
//  
//  pmath> Position(bytes, 121)
//         {{68}}
//  pmath> bytes[68] := 119
//         119
//  pmath> BinaryRead(ByteArrayToStream(ByteArray(bytes)), Expression)
//         f(x, g(w), h(g(w), z))
//
  pmath_t ba;
  pmath_t head = pmath_expr_get_item(expr, 0);
  pmath_unref(head);
  
  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  ba = pmath_expr_get_item(expr, 1);
  if(!pmath_is_byte_array(ba)) {
    pmath_message(PMATH_NULL, "bytearr", 2, PMATH_FROM_INT32(1), pmath_ref(expr));
    pmath_unref(ba);
    return expr;
  }
  
  pmath_unref(expr);
  
  size_t length = pmath_expr_length(ba);
  
  // TODO: get rid of inefficient copy, wrap original ByteArray instead if possible
  pmath_symbol_t bin_stream = pmath_file_create_binary_buffer(length);
  pmath_file_write(bin_stream, pmath_byte_array_read(ba), length);
  pmath_unref(ba);
  
  if(pmath_is_null(bin_stream))
    return pmath_ref(pmath_System_DollarFailed);
  
  if(pmath_same(head, pmath_System_ByteArrayToStream)) {
    return pmath_expr_new_extended(pmath_ref(pmath_System_InputStream), 1, bin_stream);
  }
  else {
    pmath_t copy_of_bin_stream = pmath_ref(bin_stream);
    return pmath_expr_new_extended(
      pmath_ref(pmath_System_List), 2,
      pmath_expr_new_extended(pmath_ref(pmath_System_InputStream), 1, bin_stream),
      pmath_expr_new_extended(pmath_ref(pmath_System_OutputStream), 1, copy_of_bin_stream));
  }
}
