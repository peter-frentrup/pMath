#include <pmath-util/data-types/byte-arrays-private.h>

#include <pmath-core/expressions-private.h>
#include <pmath-core/numbers.h>
#include <pmath-core/packed-arrays.h>
#include <pmath-core/strings-private.h>

#include <pmath-util/concurrency/threads-private.h>
#include <pmath-util/debug.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <stdio.h>


#ifdef _MSC_VER
#  define snprintf sprintf_s
#endif


// A ByteArray(...) is represented by a custom expression with internal reference to a 
// pmath_blob_t, a start index, and a length
struct _pmath_byte_array_extra_data_t {
  struct _pmath_custom_expr_data_t base;
  
  size_t start;
  size_t length;
};

#define BYTE_ARRAY_EXTRA(EXPR_PTR)      ((struct _pmath_byte_array_extra_data_t*)PMATH_CUSTOM_EXPR_DATA(EXPR_PTR))

extern pmath_symbol_t pmath_System_ByteArray;
extern pmath_symbol_t pmath_System_HoldForm;
extern pmath_symbol_t pmath_System_Interpretation;
extern pmath_symbol_t pmath_System_List;
extern pmath_symbol_t pmath_System_MakeBoxes;
extern pmath_symbol_t pmath_System_Panel;
extern pmath_symbol_t pmath_System_Skeleton;
extern pmath_symbol_t pmath_System_StringForm;


static const uint8_t *byte_array_read_data(struct _pmath_custom_expr_t *ba);

struct base64_block {
  char ch[4];
};

static const char    base64_padding;
static const char    base64_alphabet[64];
static const uint8_t base64_values[128];
static struct base64_block base64_encode_block_be(uint32_t three_bytes);

void base64_write_all(const uint8_t *p, size_t len, void (*write_ascii)(const char*, int, void*), void *ctx, int space_every_n_blocks);

//{ custom expr API for byte arrays ...

static void         byte_array_destroy_data(           struct _pmath_custom_expr_data_t *_data);
static size_t       byte_array_get_length(             struct _pmath_custom_expr_t *e);
static pmath_t      byte_array_get_item(               struct _pmath_custom_expr_t *e, size_t i);
static size_t       byte_array_get_extra_bytecount(    struct _pmath_custom_expr_t *e);  
static pmath_bool_t byte_array_try_get_item_range(     struct _pmath_custom_expr_t *e, size_t start, size_t length, pmath_expr_t *result);     // does not free e
static pmath_bool_t byte_array_try_new_empty_like(     struct _pmath_custom_expr_t *e, pmath_expr_t *result);                                  // does not free e
static pmath_bool_t byte_array_try_item_equals(        struct _pmath_custom_expr_t *e, size_t i, pmath_t expected_item, pmath_bool_t *result); // does not free e or expected_item
static pmath_bool_t byte_array_try_convert_to_normal(  struct _pmath_custom_expr_t *e, pmath_expr_t *result);
static pmath_bool_t byte_array_try_set_item_copy(      struct _pmath_custom_expr_t *e, size_t i, pmath_t new_item, pmath_expr_t *result);      // does not free e, but frees new_item (only if returning TRUE)
static pmath_bool_t byte_array_try_set_item_mutable(   struct _pmath_custom_expr_t *e, size_t i, pmath_t new_item);
static pmath_bool_t byte_array_try_copy_shallow(       struct _pmath_custom_expr_t *e, pmath_expr_t *result);                                  // does not free e
static pmath_bool_t byte_array_try_compare_equal(      struct _pmath_custom_expr_t *e, pmath_t other, pmath_bool_t *result);                   // does not free e or other
static pmath_bool_t byte_array_try_maybe_contains_item(struct _pmath_custom_expr_t *e, pmath_t item, pmath_bool_t *result);                    // does not free e or other
static pmath_bool_t byte_array_try_write_output(       struct _pmath_custom_expr_t *e, struct pmath_write_ex_t *info, int priority);           // does not free e
static pmath_bool_t byte_array_try_format_fullform(    struct _pmath_custom_expr_t *e, pmath_t *result);                                       // does not free e
static pmath_bool_t byte_array_try_make_boxes(         struct _pmath_custom_expr_t *e, pmath_thread_t thread, pmath_expr_t *result);           // does not free e

static const struct _pmath_custom_expr_api_t byte_array_expr_api = {
  .destroy_data            = byte_array_destroy_data,
  .get_length              = byte_array_get_length,
  .get_item                = byte_array_get_item,
  .get_extra_bytecount     = byte_array_get_extra_bytecount,
  .try_get_item_range      = byte_array_try_get_item_range,
  .try_convert_to_normal   = byte_array_try_convert_to_normal,
  .try_new_empty_like      = byte_array_try_new_empty_like,
  .try_item_equals         = byte_array_try_item_equals,
  .try_set_item_copy       = byte_array_try_set_item_copy,
  .try_set_item_mutable    = byte_array_try_set_item_mutable,
  .try_copy_shallow        = byte_array_try_copy_shallow,
  .try_compare_equal       = byte_array_try_compare_equal,
  .try_maybe_contains_item = byte_array_try_maybe_contains_item,
  .try_write_output        = byte_array_try_write_output,
  .try_format_fullform     = byte_array_try_format_fullform,
  .try_make_boxes          = byte_array_try_make_boxes,
};

//} ... custom expr API for byte arrays

PMATH_API pmath_bool_t pmath_is_byte_array(pmath_t obj) {
  return NULL != _pmath_as_custom_expr_by_api(obj, &byte_array_expr_api);
}

PMATH_API pmath_t pmath_byte_array_new(
  pmath_blob_t blob,
  size_t       start,
  size_t       length
) {
  struct _pmath_custom_expr_t *result;
  
  size_t blob_size = pmath_blob_get_size(blob);
  if(start > blob_size || length > blob_size || start > blob_size - length) {
    pmath_unref(blob);
    return PMATH_NULL;
  }
  
  result = _pmath_expr_new_custom(0, &byte_array_expr_api, sizeof(struct _pmath_byte_array_extra_data_t));
  if(PMATH_UNLIKELY(!result)) {
    pmath_unref(blob);
    return PMATH_NULL;
  }
  
  result->internals.items[0] = blob;
  
  struct _pmath_byte_array_extra_data_t *extra = BYTE_ARRAY_EXTRA(result);
  extra->start = start;
  extra->length = length;
  
  return PMATH_FROM_PTR(result);
}

PMATH_API pmath_byte_array_t pmath_byte_array_new_from_base64(pmath_string_t str) {
// pmath> ByteArray({"Peter"}) // InputForm
//        ByteArray("UGV0ZXI=")
// pmath> ByteArray("UGV0ZXI=") |> Normal
//        {80, 101, 116, 101, 114}
// pmath> ByteArray("UGV0ZX==") |> Normal
//        {80, 101, 116, 101}
// pmath> ByteArray("UGV0Z===") |> Normal
//        {80, 101, 116, 100}
// pmath> ByteArray("UGV0====") |> Normal
//        {80, 101, 116}
// pmath> ByteArray("UGV0") |> Normal
//        {80, 101, 116}
// pmath> ByteArray("UGV0Z") |> Normal
//        {80, 101, 116, 100}
// pmath> ByteArray("UGV0ZX") |> Normal
//        {80, 101, 116, 101}
// pmath> ByteArray("UGV0ZXI") |> Normal
//        {80, 101, 116, 101, 114}

  const uint16_t *buf = pmath_string_buffer(&str);
  int len = pmath_string_length(str);
  
  size_t max_byte_len = (((size_t)len + 3) / 4) * 3;
  pmath_blob_t blob = pmath_blob_new(max_byte_len, FALSE);
  uint8_t *start = pmath_blob_try_write(blob);
  if(PMATH_UNLIKELY(!start)) {
    pmath_unref(str);
    return PMATH_NULL;
  }
  
  unsigned shift = 0;
  uint32_t next_triple_be = 0;
  uint8_t *next = start;
  const uint8_t *end = start + max_byte_len;
  for(int i = 0; i < len; ++i) {
    if(buf[i] <= ' ')
      continue;
    
    if(buf[i] >= 128) {
      pmath_message(pmath_System_ByteArray, "base64", 3, pmath_string_part(str, i, 1), PMATH_FROM_INT32(i), pmath_ref(str));
      pmath_unref(str);
      pmath_unref(blob);
      return PMATH_NULL;
    }
    
    uint32_t val6 = base64_values[buf[i]];
    if(val6 == 0 && buf[i] != base64_alphabet[0]) {
      if(buf[i] == base64_padding) {
        int j;
        for(j = i + 1; j < len; ++j) {
          if(buf[j] != base64_padding && buf[j] > ' ') {
            // Padding is only allowed at the end.
            pmath_message(pmath_System_ByteArray, "base64", 3, pmath_string_part(str, i, 1), PMATH_FROM_INT32(i), pmath_ref(str));
            pmath_unref(str);
            pmath_unref(blob);
            return PMATH_NULL;
          }
        }
        break;
      }
      
      pmath_message(pmath_System_ByteArray, "base64", 3, pmath_string_part(str, i, 1), PMATH_FROM_INT32(i), pmath_ref(str));
      pmath_unref(str);
      pmath_unref(blob);
      return PMATH_NULL;
    }
    
    next_triple_be = (next_triple_be << 6) | val6;
    shift += 6;
    
    if(shift == 24) {
      if(PMATH_UNLIKELY(end - next < 3)) {
        pmath_debug_print("[Buffer too short]\n");
        pmath_unref(str);
        pmath_unref(blob);
        return PMATH_NULL;
      }
      else {
        *next++ = (next_triple_be >> 16) & 0xFF;
        *next++ = (next_triple_be >> 8) & 0xFF;
        *next++ = next_triple_be & 0xFF;
        shift = 0;
        next_triple_be = 0;
      }
    }
  }
  
  size_t needed = 0;
  switch(shift) {
    case 6:  needed = 1; break;
    case 12: needed = 1; break;
    case 18: needed = 2; break;
  }
  
  if(PMATH_UNLIKELY((size_t)(end - next) < needed)) {
    pmath_debug_print("[Buffer too short]\n");
    pmath_unref(str);
    pmath_unref(blob);
    return PMATH_NULL;
  }
  
  switch(shift) {
    case 6:
      next_triple_be <<= 18;
      *next++ = (next_triple_be >> 16) & 0xFF;
      break;
    
    case 12:
      next_triple_be <<= 12;
      *next++ = (next_triple_be >> 16) & 0xFF;
      break;
    
    case 18:
      next_triple_be <<= 6;
      *next++ = (next_triple_be >> 16) & 0xFF;
      *next++ = (next_triple_be >> 8) & 0xFF;
      break;
    
    default:
      pmath_debug_print("[Unepected shift]\n");
      break;
  }
  
  pmath_unref(str);
  return pmath_byte_array_new(blob, 0, (size_t)(next - start));
}

PMATH_API const uint8_t *pmath_byte_array_read(pmath_byte_array_t ba) {
  struct _pmath_custom_expr_t *_ba = _pmath_as_custom_expr_by_api(ba, &byte_array_expr_api);
  if(!_ba)
    return NULL;
  
  return byte_array_read_data(_ba);
}


PMATH_PRIVATE pmath_bool_t _pmath_byte_arrays_init(void) {
  return TRUE;
}

PMATH_PRIVATE void _pmath_byte_arrays_done(void) {
  // Nothing to do
}


static const uint8_t *byte_array_read_data(struct _pmath_custom_expr_t *ba) {
  struct _pmath_byte_array_extra_data_t *extra = BYTE_ARRAY_EXTRA(ba);

  const uint8_t *data = pmath_blob_read(ba->internals.items[0]);
  
  return &data[extra->start];
}

//{ custom expr API for byte arrays ...

static void byte_array_destroy_data(struct _pmath_custom_expr_data_t *_data) {
  // Nothing to do
}

static size_t byte_array_get_length(struct _pmath_custom_expr_t *e) {
  struct _pmath_byte_array_extra_data_t *extra = BYTE_ARRAY_EXTRA(e);
  
  return extra->length;
}

static pmath_t byte_array_get_item(struct _pmath_custom_expr_t *e, size_t i) {
  struct _pmath_byte_array_extra_data_t *extra = BYTE_ARRAY_EXTRA(e);
  
  if(i == 0)
    return pmath_ref(pmath_System_ByteArray);
  
  if(i > extra->length)
    return PMATH_NULL;
  
  const uint8_t *bytes = byte_array_read_data(e);
  return PMATH_FROM_INT32(bytes[i - 1]);
}

static size_t byte_array_get_extra_bytecount( struct _pmath_custom_expr_t *e) {
  return sizeof(struct _pmath_byte_array_extra_data_t);
}

static pmath_bool_t byte_array_try_get_item_range(
  struct _pmath_custom_expr_t *e,      // does not free e
  size_t                       start, 
  size_t                       length,
  pmath_expr_t                *result
) {
  struct _pmath_byte_array_extra_data_t *extra = BYTE_ARRAY_EXTRA(e);
  
  if(start == 0) {
    return FALSE;
  }
  
  assert(1 <= start && length <= extra->length && start - 1 <= extra->length - length);
  
  *result = pmath_byte_array_new(pmath_ref(e->internals.items[0]), extra->start + start - 1, length);
  return TRUE;
}

static pmath_bool_t byte_array_try_new_empty_like(
  struct _pmath_custom_expr_t *e,      // does not free e
  pmath_expr_t                *result
) {
  *result = pmath_byte_array_new(pmath_blob_new(0, FALSE), 0, 0);
  return TRUE;
}

static pmath_bool_t byte_array_try_item_equals(
  struct _pmath_custom_expr_t *e, 
  size_t i,
  pmath_t expected_item,
  pmath_bool_t *result
) {
  struct _pmath_byte_array_extra_data_t *extra = BYTE_ARRAY_EXTRA(e);
  
  if(i == 0) {
    *result = pmath_same(expected_item, pmath_System_ByteArray);
    return TRUE;
  }
  
  if(PMATH_UNLIKELY(i > extra->length)) {
    *result = pmath_is_null(expected_item);
    return TRUE;
  }
  
  if(!pmath_is_int32(expected_item)) {
    *result = FALSE;
    return TRUE;
  }
  
  int32_t value = PMATH_AS_INT32(expected_item);
  if(value < 0 || value > 255) {
    *result = FALSE;
    return TRUE;
  }
  
  const uint8_t *bytes = byte_array_read_data(e);
  *result = (bytes[i - 1] == value);
  return TRUE;
}

static pmath_bool_t byte_array_try_convert_to_normal(
  struct _pmath_custom_expr_t *e,
  pmath_expr_t                *result
) {
  pmath_expr_t expr = _pmath_custom_expr_convert_to_normal_fallback(e);
  *result = pmath_expr_set_item(expr, 0, pmath_ref(pmath_System_List));
  return TRUE;
}

static pmath_bool_t byte_array_try_set_item_copy(
  struct _pmath_custom_expr_t *e,        // will not be freed or modified 
  size_t                       i, 
  pmath_t                      new_item, // will be freed iff returning TRUE
  pmath_expr_t                *result    // will be set iff returning TRUE
) {
  struct _pmath_byte_array_extra_data_t *extra = BYTE_ARRAY_EXTRA(e);
  
  if(PMATH_UNLIKELY(i == 0)) {
    if(pmath_same(new_item, pmath_System_ByteArray)) 
      pmath_unref(new_item);
    else
      pmath_message(pmath_System_ByteArray, "head", 1, new_item);

    *result = pmath_ref(PMATH_FROM_PTR(e));
    return TRUE;
  }
  
  if(PMATH_UNLIKELY(i > extra->length)) {
    pmath_message(pmath_System_ByteArray, "partw", 1, pmath_integer_new_uiptr(i));
    pmath_unref(new_item);
    
    *result = pmath_ref(PMATH_FROM_PTR(e));
    return TRUE;
  }
  
  if(!pmath_is_int32(new_item)) {
    pmath_message(pmath_System_ByteArray, "nobyte", 1, new_item);
    *result = pmath_ref(PMATH_FROM_PTR(e));
    return TRUE;
  }
  
  // No need to free `new_item`, since that is an INT32
  
  int32_t value = PMATH_AS_INT32(new_item);
  if(value < 0 || value > 255) {
    pmath_message(pmath_System_ByteArray, "nobyte", 1, new_item);
    *result = pmath_ref(PMATH_FROM_PTR(e));
    return TRUE;
  }
  
  const uint8_t *old_data = byte_array_read_data(e);
  if(old_data[i - 1] == value) {
    *result = pmath_ref(PMATH_FROM_PTR(e));
    return TRUE;
  }
  
  pmath_blob_t new_blob = pmath_blob_new(extra->length, FALSE);
  uint8_t *new_data = pmath_blob_try_write(new_blob);
  if(PMATH_UNLIKELY(!new_data)) {
    pmath_unref(new_blob);
    *result = PMATH_NULL;
    return TRUE;
  }
  
  memcpy(new_data, old_data, extra->length);
  new_data[i-1] = (uint8_t)value;
  *result = pmath_byte_array_new(new_blob, 0, extra->length);
  return TRUE;
}

static pmath_bool_t byte_array_try_set_item_mutable(
  struct _pmath_custom_expr_t *e,        // will be modified only if returning TRUE
  size_t                       i,
  pmath_t                      new_item  // will be freed iff returning TRUE
) {
  struct _pmath_byte_array_extra_data_t *extra = BYTE_ARRAY_EXTRA(e);
  
  if(PMATH_UNLIKELY(i == 0)) {
    if(pmath_same(new_item, pmath_System_ByteArray)) 
      pmath_unref(new_item);
    else
      pmath_message(pmath_System_ByteArray, "head", 1, new_item);

    return TRUE;
  }
  
  if(PMATH_UNLIKELY(i > extra->length)) {
    pmath_message(pmath_System_ByteArray, "partw", 1, pmath_integer_new_uiptr(i));
    pmath_unref(new_item);
    return TRUE;
  }
  
  if(!pmath_is_int32(new_item)) {
    pmath_message(pmath_System_ByteArray, "nobyte", 1, new_item);
    return TRUE;
  }
  
  // No need to free `new_item`, since that is an INT32
  
  int32_t value = PMATH_AS_INT32(new_item);
  if(value < 0 || value > 255) {
    pmath_message(pmath_System_ByteArray, "nobyte", 1, new_item);
    return TRUE;
  }
  
  const uint8_t *old_data = byte_array_read_data(e);
  if(old_data[i - 1] == value) {
    return TRUE;
  }
  
  uint8_t *raw_data = pmath_blob_try_write(e->internals.items[0]);
  if(PMATH_UNLIKELY(!raw_data)) {
    pmath_blob_t new_blob = pmath_blob_new(extra->length, FALSE);
    uint8_t *new_data = pmath_blob_try_write(new_blob);
    if(PMATH_UNLIKELY(!new_data)) {
      pmath_unref(new_blob);
      return TRUE;
    }
    
    memcpy(new_data, old_data, extra->length);
    new_data[i - 1] = (uint8_t)value;
    pmath_unref(e->internals.items[0]);
    e->internals.items[0] = new_blob;
    extra->start           = 0;
  }
  else {
    uint8_t *new_data = &raw_data[extra->start];
    new_data[i - 1] = (uint8_t)value;
  }
  
  return TRUE;
}

static pmath_bool_t byte_array_try_copy_shallow(
  struct _pmath_custom_expr_t *e,     // does not free e
  pmath_expr_t                *result
) {
  struct _pmath_byte_array_extra_data_t *extra = BYTE_ARRAY_EXTRA(e);
  
  *result = pmath_byte_array_new(e->internals.items[0], extra->start, extra->length);
  return TRUE;
}                                  

static pmath_bool_t byte_array_try_compare_equal(
  struct _pmath_custom_expr_t *e,
  pmath_t                      other,
  pmath_bool_t                *result
) {
  struct _pmath_byte_array_extra_data_t *extra = BYTE_ARRAY_EXTRA(e);
  struct _pmath_custom_expr_t *other_ba = _pmath_as_custom_expr_by_api(other, &byte_array_expr_api);
  if(other_ba) {
    struct _pmath_byte_array_extra_data_t *other_extra = BYTE_ARRAY_EXTRA(other_ba);
    
    if(extra->length != other_extra->length) {
      *result = FALSE;
      return TRUE;
    }
    
    const uint8_t *bytes       = byte_array_read_data(e);
    const uint8_t *other_bytes = byte_array_read_data(other_ba);
    
    *result = (0 == memcmp(bytes, other_bytes, extra->length));
    return TRUE;
  }
  
  return FALSE;
}

static pmath_bool_t byte_array_try_maybe_contains_item( // excluding index 0
  struct _pmath_custom_expr_t *e, 
  pmath_t                      item,
  pmath_bool_t                *result
) {
  if(pmath_is_int32(item)) {
    uint32_t value = PMATH_AS_INT32(item);
    *result = (0 <= value && value <= 255);
    return TRUE;
  }
  else {
    *result = FALSE;
    return TRUE;
  }
}

static void write_ascii(const char *str, int len, void *ctx) {
  struct pmath_write_ex_t *info = ctx;
  _pmath_write_latin1(str, len, info->write, info->user);
}

static pmath_bool_t byte_array_try_write_output(
  struct _pmath_custom_expr_t *e,       // does not free e
  struct pmath_write_ex_t     *info,
  int                          priority
) {
// pmath> ByteArray({1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 9, 8, 7, 6, 5, 4, 3, 2, 1})
//        ByteArray(<< 19 bytes >>)
// pmath> ByteArray({1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 9, 8, 7, 6, 5, 4, 3, 2, 1}) // InputForm
//        ByteArray("AQIDBAUGBwgJAAkIBwYFBAMCAQ==")
// pmath> ByteArray({1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 9, 8, 7, 6, 5, 4, 3, 2, 1}) // FullForm
//        ByteArray("AQIDBAUGBwgJAAkIBwYFBAMCAQ==")
// pmath> ByteArray(Table(PowerMod(i, 2, 255), i -> 70)) // FullForm
//        ByteArray("AQQJEBkkMUBRZHmQqcThASJFapG65RNCc6bb E0yHxARFiM0VXqn2RpfqQJfwTKkJas0zmgRv 
//          3Ey9MaYelxOQEJEVmiKrNw==")

  struct _pmath_byte_array_extra_data_t *extra = BYTE_ARRAY_EXTRA(e);
  
#define WRITE_CSTR(str) _pmath_write_cstr((str), info->write, info->user)

  pmath_write_ex(info, pmath_System_ByteArray);
  WRITE_CSTR("(");
  
  if(info->options & (PMATH_WRITE_OPTIONS_FULLEXPR | PMATH_WRITE_OPTIONS_INPUTEXPR)) {
    WRITE_CSTR("\"");
    
    const uint8_t *p = byte_array_read_data(e);
    size_t len = extra->length;
    base64_write_all(p, len, write_ascii, info, 8);
    
    WRITE_CSTR("\"");
  }
  else {
    char summary_buf[40]; // "<< 18446744073709551615 bytes >>"
    snprintf(summary_buf, sizeof(summary_buf), "<< %"PRIuPTR" bytes >>", (uintptr_t)extra->length);
    _pmath_write_cstr(summary_buf, info->write, info->user);
  }
  
  WRITE_CSTR(")");
  
  return TRUE;
#undef WRITE_CSTR
}

static void write_ascii_to_string(const char *ins, int len, void *_str) {
  pmath_string_t *s = _str;
  *s = pmath_string_insert_latin1(*s, INT_MAX, ins, len);
}

static pmath_bool_t byte_array_try_format_fullform(
  struct _pmath_custom_expr_t *e,      // does not free e
  pmath_t                     *result
) {
  struct _pmath_byte_array_extra_data_t *extra = BYTE_ARRAY_EXTRA(e);
  
  const uint8_t *p = byte_array_read_data(e);
  
  size_t block_size = 1024 * 1024;
  
  pmath_t arg = PMATH_NULL;
  if(extra->length <= block_size) {
    int base64_len = (((int)extra->length + 2) / 3) * 4;
    int spaces = ((int)extra->length + 3*8 - 1) / (3*8);
    
    arg = pmath_string_new(base64_len + spaces);
    
    base64_write_all(p, extra->length, write_ascii_to_string, &arg, 8);
  }
  if(pmath_is_null(arg)) {
    size_t num_blocks = extra->length / block_size + (extra->length % block_size ? 1 : 0);
    size_t len_remaining = extra->length;
    const uint8_t *next = p;
    
    pmath_t self = PMATH_FROM_PTR(&e->internals.inherited.inherited.inherited);
    
    arg = pmath_expr_new(pmath_ref(pmath_System_List), num_blocks);
    for(size_t i = 0; i < num_blocks; ++i) {
      size_t block_len = len_remaining;
      if(block_len > block_size)
        block_len = block_size;
      
      pmath_t item = pmath_expr_get_item_range(self, 1 + (size_t)(next - p), block_len);
      arg = pmath_expr_set_item(arg, i + 1, item);
      
      next          += block_len;
      len_remaining -= block_len;
    }
  }
  
  if(pmath_is_null(arg)) {
    _pmath_incref_impl(&e->internals.inherited.inherited.inherited);
    arg = _pmath_custom_expr_convert_to_normal(e);
  }
  
  *result = pmath_expr_new_extended(pmath_ref(pmath_System_ByteArray), 1, arg);
  return TRUE;
}

static pmath_bool_t byte_array_try_make_boxes(
  struct _pmath_custom_expr_t *e, 
  pmath_thread_t               thread, 
  pmath_expr_t                *result
) {
// pmath> ToBoxes(ByteArray({1,2,3,4}) // FullForm) // InputForm
//        StyleBox({"ByteArray", "(", {StringBox("\"AQIDBA==\"")}, ")"}, AutoDelete -> True, 
//          ShowStringCharacters -> True)
//
// pmath> ToBoxes(ByteArray({1,2,3,4}) // InputForm) // InputForm
//        StyleBox({"ByteArray", "(", {StringBox("\"AQIDBA==\"")}, ")"}, AutoDelete -> True, 
//          AutoNumberFormating -> False, ShowStringCharacters -> True)
//
// pmath> ToBoxes(ByteArray({1,2,3,4}) // OutputForm) // InputForm
//        InterpretationBox({"ByteArray", "(", {{"<<", StringBox("\"", {"4"}, " bytes\""), ">>"}}, 
//           ")"}, OutputForm(ByteArray("AQIDBA==")))
//
// pmath> ToBoxes(ByteArray({1,2,3,4})) // InputForm
//        InterpretationBox({"ByteArray", "(", {
//            {"Panel", "(", {StringBox("\"", {"4"}, " bytes\"")}, ")"}}, ")"}, ByteArray("AQIDBA=="))
//
  struct _pmath_byte_array_extra_data_t *extra = BYTE_ARRAY_EXTRA(e);
  
  if(thread->boxform >= BOXFORM_INPUT) {
    pmath_t format = PMATH_NULL;
    byte_array_try_format_fullform(e, &format);
    *result = pmath_evaluate(pmath_expr_new_extended(pmath_ref(pmath_System_MakeBoxes), 1, format));
    return TRUE;
  }
  
  pmath_t summary = pmath_expr_new_extended(
    pmath_ref(pmath_System_StringForm), 2,
    PMATH_C_STRING("`` bytes"),
    pmath_integer_new_uiptr(extra->length));
  
  if(thread->boxform >= BOXFORM_OUTPUT) {
    summary = pmath_expr_new_extended(pmath_ref(pmath_System_Skeleton), 1, summary);
  }
  else {
    summary = pmath_expr_new_extended(pmath_ref(pmath_System_Panel), 1, summary);
  }
  
  pmath_t head = pmath_expr_new_extended(pmath_ref(pmath_System_HoldForm), 1, pmath_ref(pmath_System_ByteArray));
  
  summary = pmath_expr_new_extended(head, 1, summary);
  summary = pmath_expr_new_extended(
    pmath_ref(pmath_System_Interpretation), 2,
    summary,
    pmath_ref(PMATH_FROM_PTR(&e->internals.inherited.inherited.inherited)));
  
  *result = pmath_evaluate(pmath_expr_new_extended(pmath_ref(pmath_System_MakeBoxes), 1, summary));
  
  return TRUE;
}

//} ... custom expr API for byte arrays

//{ Base64 encoding ...

static const char base64_padding = '=';
static const char base64_alphabet[64] =
/*
pmath> base64chars := "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"
       ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/
pmath> Print("*" ++ "/\n", InputForm(base64chars), ";")
    */
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// pmath> base64codes:= ToCharacterCode(base64chars);
// pmath> Length(base64codes)
//        64

static const uint8_t base64_values[128] = {
/*
pmath> Print("*" ++ "/");
     > Do(i->Length(base64codes)) {
     >   Print("['", FromCharacterCode(base64codes[i]), "'] = ", i-1, ",")
     > }
    */
    ['A'] = 0,
    ['B'] = 1,
    ['C'] = 2,
    ['D'] = 3,
    ['E'] = 4,
    ['F'] = 5,
    ['G'] = 6,
    ['H'] = 7,
    ['I'] = 8,
    ['J'] = 9,
    ['K'] = 10,
    ['L'] = 11,
    ['M'] = 12,
    ['N'] = 13,
    ['O'] = 14,
    ['P'] = 15,
    ['Q'] = 16,
    ['R'] = 17,
    ['S'] = 18,
    ['T'] = 19,
    ['U'] = 20,
    ['V'] = 21,
    ['W'] = 22,
    ['X'] = 23,
    ['Y'] = 24,
    ['Z'] = 25,
    ['a'] = 26,
    ['b'] = 27,
    ['c'] = 28,
    ['d'] = 29,
    ['e'] = 30,
    ['f'] = 31,
    ['g'] = 32,
    ['h'] = 33,
    ['i'] = 34,
    ['j'] = 35,
    ['k'] = 36,
    ['l'] = 37,
    ['m'] = 38,
    ['n'] = 39,
    ['o'] = 40,
    ['p'] = 41,
    ['q'] = 42,
    ['r'] = 43,
    ['s'] = 44,
    ['t'] = 45,
    ['u'] = 46,
    ['v'] = 47,
    ['w'] = 48,
    ['x'] = 49,
    ['y'] = 50,
    ['z'] = 51,
    ['0'] = 52,
    ['1'] = 53,
    ['2'] = 54,
    ['3'] = 55,
    ['4'] = 56,
    ['5'] = 57,
    ['6'] = 58,
    ['7'] = 59,
    ['8'] = 60,
    ['9'] = 61,
    ['+'] = 62,
    ['/'] = 63,
};

static struct base64_block base64_encode_block_be(uint32_t three_bytes) {
  struct base64_block ret;
  ret.ch[0] = base64_alphabet[(three_bytes >> 18) & 0x3F];
  ret.ch[1] = base64_alphabet[(three_bytes >> 12) & 0x3F];
  ret.ch[2] = base64_alphabet[(three_bytes >> 6)  & 0x3F];
  ret.ch[3] = base64_alphabet[ three_bytes        & 0x3F];
  return ret;
}

void base64_write_all(
  const uint8_t *p, 
  size_t len, 
  void (*put_ascii)(const char*, int, void*),
  void *ctx,
  int space_every_n_blocks
) {
  int next_space = space_every_n_blocks;
  while(len >= 3) {
    struct base64_block chars = base64_encode_block_be((p[0] << 16) | (p[1]<<8) | p[2]);
    put_ascii(chars.ch, 4, ctx);
    p+= 3;
    len -= 3;
    
    if(space_every_n_blocks >= 0 && next_space-- == 0) {
      next_space = space_every_n_blocks;
      put_ascii(" ", 1, ctx);
    }
  }
  
  if(len == 2) {
    struct base64_block chars = base64_encode_block_be((p[0] << 16) | (p[1]<<8));
    chars.ch[3] = base64_padding;
    put_ascii(chars.ch, 4, ctx);
  }
  else if(len == 1) {
    struct base64_block chars = base64_encode_block_be(p[0] << 16);
    chars.ch[2] = base64_padding;
    chars.ch[3] = base64_padding;
    put_ascii(chars.ch, 4, ctx);
  }
}

//} ... Base64 encoding
