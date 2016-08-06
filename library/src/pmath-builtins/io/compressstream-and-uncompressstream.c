#include <pmath-util/compression.h>

#include <pmath-builtins/all-symbols-private.h>

#include <pmath-util/messages.h>
#include <pmath-util/option-helpers.h>

#include <zlib.h>


static pmath_bool_t init_window_bits(int *window_bits, pmath_expr_t options) {
  pmath_t name = PMATH_C_STRING("WindowBits");
  pmath_t value = pmath_option_value(PMATH_NULL, name, options);
  pmath_unref(name);
  
  if(pmath_is_int32(value)) {
    *window_bits = PMATH_AS_INT32(value);
    
    if(*window_bits < 8 || *window_bits > MAX_WBITS) {
      pmath_message(PMATH_NULL, "wbits", 1, value);
      return FALSE;
    }
    
    return TRUE;
  }
  
  if(pmath_same(value, PMATH_SYMBOL_AUTOMATIC)) {
    *window_bits = MAX_WBITS;
    pmath_unref(value);
    return TRUE;
  }
  
  pmath_message(PMATH_NULL, "wbits", 1, value);
  return FALSE;
}

static pmath_bool_t init_skip_header(pmath_bool_t *skip_header, pmath_bool_t compress, pmath_expr_t options) {
  pmath_t name = PMATH_C_STRING(compress ? "RawDeflate" : "RawInflate");
  pmath_t value = pmath_option_value(PMATH_NULL, name, options);
  
  if(pmath_same(value, PMATH_SYMBOL_TRUE)) {
    pmath_unref(name);
    pmath_unref(value);
    *skip_header = TRUE;
    return TRUE;
  }
  
  if(pmath_same(value, PMATH_SYMBOL_FALSE)) {
    pmath_unref(name);
    pmath_unref(value);
    *skip_header = FALSE;
    return TRUE;
  }
  
  pmath_message(PMATH_NULL, "opttf", 2, name, value);
  return FALSE;
}

static pmath_bool_t init_level(struct pmath_compressor_settings_t *settings, pmath_expr_t options) {
  pmath_t value = pmath_option_value(PMATH_NULL, PMATH_SYMBOL_LEVEL, options);
  
  if(pmath_is_int32(value)) {
    settings->level = PMATH_AS_INT32(value);
    if(settings->level < Z_NO_COMPRESSION || settings->level > Z_BEST_COMPRESSION) {
      pmath_message(PMATH_NULL, "lvl", 1, value);
      return FALSE;
    }
    
    return TRUE;
  }
  
  if(pmath_same(value, PMATH_SYMBOL_AUTOMATIC)) {
    pmath_unref(value);
    settings->level = Z_DEFAULT_COMPRESSION;
    return TRUE;
  }
  
  pmath_message(PMATH_NULL, "lvl", 1, value);
  return FALSE;
}

static pmath_bool_t init_strategy(struct pmath_compressor_settings_t *settings, pmath_expr_t options) {
  settings->strategy = Z_DEFAULT_STRATEGY;
  return TRUE;
}

pmath_t builtin_compressstream(pmath_expr_t expr) {
  pmath_t stream;
  struct pmath_compressor_settings_t settings;
  pmath_expr_t options;
  
  if(pmath_expr_length(expr) < 1) {
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
  
  options = pmath_options_extract(expr, 1);
  if(pmath_is_null(options)) {
    pmath_unref(stream);
    return expr;
  }
  
  memset(&settings, 0, sizeof(settings));
  settings.size = sizeof(settings);
  if( !init_window_bits(&settings.window_bits, options) ||
      !init_skip_header(&settings.skip_header, TRUE, options) ||
      !init_level(&settings, options) ||
      !init_strategy(&settings, options))
  {
    pmath_unref(stream);
    pmath_unref(options);
    return expr;
  }
  pmath_unref(options);
  
  stream = pmath_file_create_compressor(stream, &settings);
  if(pmath_is_null(stream)) {
    pmath_message(PMATH_NULL, "invio", 1, pmath_expr_get_item(expr, 1));
    return expr;
  }
  
  pmath_unref(expr);
  return stream;
}

pmath_t builtin_uncompressstream(pmath_expr_t expr) {
  pmath_t stream;
  struct pmath_decompressor_settings_t settings;
  pmath_expr_t options;
  
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
  
  options = pmath_options_extract(expr, 1);
  if(pmath_is_null(options)) {
    pmath_unref(stream);
    return expr;
  }
  
  memset(&settings, 0, sizeof(settings));
  settings.size = sizeof(settings);
  if( !init_window_bits(&settings.window_bits, options) ||
      !init_skip_header(&settings.skip_header, FALSE, options))
  {
    pmath_unref(stream);
    pmath_unref(options);
    return expr;
  }
  pmath_unref(options);
  
  stream = pmath_file_create_decompressor(stream, &settings);
  if(pmath_is_null(stream)) {
    pmath_message(PMATH_NULL, "invio", 1, pmath_expr_get_item(expr, 1));
    return expr;
  }
  
  pmath_unref(expr);
  return stream;
}
