#include <pmath-util/data-types/byte-arrays-private.h>

#include <pmath-util/debug.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>
#include <pmath-util/overflow-calc-private.h>


struct byte_array_size_accumulator {
  size_t total_bytes;
  uint8_t      depth;
  pmath_bool_t error;
  pmath_bool_t quiet;
};

struct byte_array_output_accumulator {
  uint8_t *next;
  uint8_t *end;
};

static void count_bytes_and_free(pmath_t item, struct byte_array_size_accumulator *acc); // item will be freed
static void count_raw_string_bytes(pmath_string_t str, struct byte_array_size_accumulator *acc); // str will not be freed
static void count_items_bytes(pmath_expr_t expr, struct byte_array_size_accumulator *acc); // expr will not be freed

static void write_raw_bytes(const uint8_t *data, size_t len, struct byte_array_output_accumulator *out);
static void write_raw_bytes_from_ucs2_lower(const uint16_t *data, size_t len, struct byte_array_output_accumulator *out);
static void write_raw_bytes_from_i32_lower(const int32_t *data, size_t len, struct byte_array_output_accumulator *out);
static void write_bytes_and_free(pmath_t item, struct byte_array_output_accumulator *out); // item will be freed
static void write_raw_string_bytes(pmath_string_t str, struct byte_array_output_accumulator *out); // str will not be freed
static void write_items_bytes(pmath_expr_t expr, struct byte_array_output_accumulator *out); // expr will not be freed

extern pmath_symbol_t pmath_System_ByteArray;
extern pmath_symbol_t pmath_System_List;


PMATH_PRIVATE pmath_t builtin_bytearray(pmath_expr_t expr) {
// ByteArray("base64string")   --- not yet supported
// ByteArray({b1, b2, ...})
// ByteArray({"latin1string"})
// ByteArray({b1, b2, "latin1", b9, ...})
//
// Examples:
//  pmath> ByteArray("")
//         ByteArray(<< 0 bytes >>)
//
//  pmath> ByteArray({ByteArray("AQIDBAUGBwgJCgsMDQ4PEA=="), ByteArray("ERITFA==")}) |> Normal
//         {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20}

  if(pmath_is_byte_array(expr)) {
    return expr;
  }
  
  size_t exprlen = pmath_expr_length(expr);
  
  if(exprlen != 1) {
    pmath_message_argxxx(exprlen, 1, 1);
    return expr;
  }
  
  pmath_t arg = pmath_expr_get_item(expr, 1);
  if(pmath_is_string(arg)) {
    pmath_byte_array_t result = pmath_byte_array_new_from_base64(arg);
    if(pmath_is_null(result)) {
      return expr;
    }
    
    pmath_unref(expr);
    return result;
  }
  pmath_unref(arg);
  
  struct byte_array_size_accumulator acc = {
    .total_bytes = 0,
    .depth = 0,
    .error = FALSE,
    .quiet = FALSE,
  };
  
  count_items_bytes(expr, &acc);
  if(acc.error)
    return expr;
  
  pmath_blob_t blob = pmath_blob_new(acc.total_bytes, FALSE);
  struct byte_array_output_accumulator write_acc = {
    .next = pmath_blob_try_write(blob),
  };
  
  if(!write_acc.next) {
    pmath_unref(blob);
    return expr;
  }
  
  write_acc.end = &write_acc.next[acc.total_bytes];
  write_items_bytes(expr, &write_acc);
  pmath_unref(expr);
  
  if(PMATH_UNLIKELY(write_acc.next != write_acc.end)) {
    pmath_debug_print("[unexpected: ByteBuffer not filled]\n");
  }
  
  return pmath_byte_array_new(blob, 0, acc.total_bytes);
}

//{ Counting final size ...

// item will be freed
static void count_bytes_and_free(pmath_t item, struct byte_array_size_accumulator *acc) {
  if(acc->error) {
    pmath_unref(item);
    return;
  }
  
  if(pmath_is_int32(item)) {
    uint32_t val = PMATH_AS_INT32(item);
    if(0 <= val && val <= 255) {
      acc->total_bytes = _pmath_add_size(acc->total_bytes, 1, &acc->error);
    }
    else {
      if(!acc->quiet) pmath_message(pmath_System_ByteArray, "nobytes", 1, pmath_ref(item));
      acc->error = TRUE;
    }
  }
  else if(pmath_is_string(item)) {
    count_raw_string_bytes(item, acc);
  }
  else if(pmath_is_byte_array(item)) {
    size_t len = pmath_expr_length(item);
    acc->total_bytes = _pmath_add_size(acc->total_bytes, len, &acc->error);
  }
  else if(pmath_is_expr_of(item, pmath_System_List)) {
    count_items_bytes(item, acc);
  }
  else {
    if(!acc->quiet) pmath_message(pmath_System_ByteArray, "nobytes", 1, pmath_ref(item));
    acc->error = TRUE;
  }
  
  pmath_unref(item);
}

static void count_raw_string_bytes(pmath_string_t str, struct byte_array_size_accumulator *acc) {
  int len = pmath_string_length(str);
  const uint16_t *buf = pmath_string_buffer(&str);
  
  int i = 0;
  while(i < len && buf[i] <= 255)
    ++i;
  
  if(i < len) {
    if(!acc->quiet) pmath_message(pmath_System_ByteArray, "nobytes", 1, pmath_ref(str));
    acc->error = TRUE;
  }
  else {
    acc->total_bytes = _pmath_add_size(acc->total_bytes, (size_t)len, &acc->error);
  }
}

static void count_items_bytes(pmath_expr_t expr, struct byte_array_size_accumulator *acc) {
  if(acc->depth >= 20) {
    if(!acc->quiet) pmath_message(pmath_System_ByteArray, "depth", 0);
    acc->error = TRUE;
    pmath_unref(expr);
    return;
  }
  
  if(pmath_is_packed_array(expr)) {
    pmath_packed_type_t elem_type = pmath_packed_array_get_element_type(expr);
    if(elem_type == PMATH_PACKED_INT32) {
      size_t noncont_dims = pmath_packed_array_get_non_continuous_dimensions(expr);
      if(noncont_dims == 0) {
        size_t length       = *pmath_packed_array_get_sizes(expr);
        size_t step_size    = *pmath_packed_array_get_steps(expr) / sizeof(int32_t);
        const int32_t *data = pmath_packed_array_read(expr, NULL, 0);
        
        for(size_t i = 0; i < length * step_size; ++i) {
          if(data[i] < 0 || data[i] > 255) {
            if(!acc->quiet) pmath_message(pmath_System_ByteArray, "nobytes", 1, pmath_ref(expr));
            acc->error = TRUE;
            return;
          }
        }
        
        acc->total_bytes = _pmath_add_size(acc->total_bytes, length * step_size, &acc->error);
        return;
      }
    }
  }
  
  acc->depth++;
  
  size_t list_len = pmath_expr_length(expr);
  for(size_t i = 1; i <= list_len && !acc->error; ++i) {
    count_bytes_and_free(pmath_expr_get_item(expr, i), acc);
  }
  
  acc->depth--;
}

//} ... Counting final size

//{ Writing bytes ...

static void write_raw_bytes(const uint8_t *data, size_t len, struct byte_array_output_accumulator *out) {
  if(PMATH_UNLIKELY(len > (size_t)(out->end - out->next))) {
    if(out->next < out->end) {
      pmath_debug_print("[write_raw_bytes too large len]\n");
      memset(out->next, 0, out->end - out->next);
      out->next = out->end;
      return;
    }
  }
  
  memmove(out->next, data, len);
  out->next += len;
}

static void write_raw_bytes_from_ucs2_lower(const uint16_t *data, size_t len, struct byte_array_output_accumulator *out) {
  if(PMATH_UNLIKELY(len > (size_t)(out->end - out->next))) {
    if(out->next < out->end) {
      pmath_debug_print("[write_raw_bytes too large len]\n");
      memset(out->next, 0, out->end - out->next);
      out->next = out->end;
      return;
    }
  }
  
  for(size_t i = 0; i < len; ++i) {
    out->next[i] = (uint8_t)data[i];
  }
  out->next += len;
}

static void write_raw_bytes_from_i32_lower(const int32_t *data, size_t len, struct byte_array_output_accumulator *out) {
  if(PMATH_UNLIKELY(len > (size_t)(out->end - out->next))) {
    if(out->next < out->end) {
      pmath_debug_print("[write_raw_bytes too large len]\n");
      memset(out->next, 0, out->end - out->next);
      out->next = out->end;
      return;
    }
  }
  
  for(size_t i = 0; i < len; ++i) {
    out->next[i] = (uint8_t)data[i];
  }
  out->next += len;
}

// item will be freed
static void write_bytes_and_free(pmath_t item, struct byte_array_output_accumulator *out) {
  if(pmath_is_int32(item)) {
    uint32_t val = PMATH_AS_INT32(item);
    uint8_t byte = (uint8_t)val;
    write_raw_bytes(&byte, 1, out);
  }
  else if(pmath_is_string(item)) {
    write_raw_string_bytes(item, out);
  }
  else if(pmath_is_byte_array(item)) {
    size_t len = pmath_expr_length(item);
    const uint8_t *bytes = pmath_byte_array_read(item);
    write_raw_bytes(bytes, len, out);
  }
  else if(pmath_is_expr_of(item, pmath_System_List)) {
    write_items_bytes(item, out);
  }
  
  pmath_unref(item);
}

static void write_raw_string_bytes(pmath_string_t str, struct byte_array_output_accumulator *out) {
  write_raw_bytes_from_ucs2_lower(pmath_string_buffer(&str), (size_t)pmath_string_length(str), out);
}

static void write_items_bytes(pmath_expr_t expr, struct byte_array_output_accumulator *out) {
  if(pmath_is_packed_array(expr)) {
    pmath_packed_type_t elem_type = pmath_packed_array_get_element_type(expr);
    if(elem_type == PMATH_PACKED_INT32) {
      size_t noncont_dims = pmath_packed_array_get_non_continuous_dimensions(expr);
      if(noncont_dims == 0) {
        size_t length       = *pmath_packed_array_get_sizes(expr);
        size_t step_size    = *pmath_packed_array_get_steps(expr) / sizeof(int32_t);
        const int32_t *data = pmath_packed_array_read(expr, NULL, 0);
        
        write_raw_bytes_from_i32_lower(data, length * step_size, out);
        return;
      }
    }
  }
  
  size_t list_len = pmath_expr_length(expr);
  for(size_t i = 1; i <= list_len; ++i) {
    write_bytes_and_free(pmath_expr_get_item(expr, i), out);
  }
}

//} ... Writing bytes
