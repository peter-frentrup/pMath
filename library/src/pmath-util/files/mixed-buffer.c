#include <pmath-core/custom.h>

#include <pmath-util/files/mixed-buffer.h>
#include <pmath-util/debug.h>
#include <pmath-util/memory.h>

#include <limits.h>
#include <string.h>


struct textbuffer_t {
  pmath_t               text;
  pmath_files_status_t  status;
  uint8_t              *out;
  uint8_t              *in;
  uint8_t               inbuffer[ 32];
  uint8_t               outbuffer[32];
  
  void (*flush_func)(void *extra);
  int blocklen;
  unsigned skip_newline_at_start : 1;
};

static void textbuffer_destroy_custom_data(void*);

static struct textbuffer_t *get_textbuffer(void *extra) {
  pmath_t tmp = PMATH_FROM_PTR(extra);
  
  assert(pmath_is_custom(tmp));
  assert(pmath_custom_has_destructor(tmp, textbuffer_destroy_custom_data));
  
  return pmath_custom_get_data(tmp);
}

static void textbuffer_destroy_custom_data(void *extra) {
  struct textbuffer_t *tb = extra;
  
  pmath_unref(tb->text);
  pmath_mem_free(tb);
}

static void textbuffer_destroy_custom(void *extra) {
  struct textbuffer_t *tb = get_textbuffer(extra);
  
  if(tb->flush_func)
    tb->flush_func(extra);
    
  pmath_unref(PMATH_FROM_PTR(extra));
}

static pmath_files_status_t textbuffer_text_status(void *extra) {
  struct textbuffer_t *tb = get_textbuffer(extra);
  
  if(tb->status != PMATH_FILE_OK)
    return tb->status;
    
  if(pmath_string_length(tb->text) == 0)
    return PMATH_FILE_ENDOFFILE;
    
  return PMATH_FILE_OK;
}

static pmath_files_status_t textbuffer_bin_status(void *extra) {
  struct textbuffer_t *tb = get_textbuffer(extra);
  
  if(tb->status != PMATH_FILE_OK)
    return tb->status;
    
  if(pmath_string_length(tb->text) < tb->blocklen)
    return PMATH_FILE_ENDOFFILE;
    
  return PMATH_FILE_OK;
}

static pmath_bool_t textbuffer_write(void *extra, const uint16_t *str, int len) {
  struct textbuffer_t *tb = get_textbuffer(extra);
  
  if(tb->status != PMATH_FILE_OK)
    return FALSE;
    
  tb->text = pmath_string_insert_ucs2(tb->text, INT_MAX, str, len);
  return TRUE;
}

static pmath_bool_t textbuffer_write_skipping_whitespace(void *extra, const uint16_t *str, int len) {
  struct textbuffer_t *tb = get_textbuffer(extra);
  int i;
  
  if(tb->status != PMATH_FILE_OK)
    return FALSE;
    
  for(i = 0; i < len;) {
    if(str[i] <= ' ') {
      if(i > 0)
        tb->text = pmath_string_insert_ucs2(tb->text, INT_MAX, str, i);
        
      ++i;
      while(i < len && str[i] <= ' ')
        ++i;
        
      str += i;
      len -= i;
      i = 0;
    }
    else
      ++i;
  }
  
  if(len > 0)
    tb->text = pmath_string_insert_ucs2(tb->text, INT_MAX, str, len);
    
  return TRUE;
}

static pmath_string_t textbuffer_readln(void *extra) {
  struct textbuffer_t *tb = get_textbuffer(extra);
  pmath_string_t result;
  int i;
  int len;
  const uint16_t *buf;
  
  if(tb->status != PMATH_FILE_OK)
    return PMATH_NULL;
  
  buf = pmath_string_buffer(&tb->text);
  len = pmath_string_length(tb->text);
  
  i = 0;
  if(tb->skip_newline_at_start && len > 0 && buf[0] == '\n') {
    i++;
  }
  tb->skip_newline_at_start = FALSE;
  for(; i < len; ++i) {
    if(buf[i] == '\n') {
      result = pmath_string_part(pmath_ref(tb->text), 0, i);
      tb->text = pmath_string_part(tb->text, i + 1, INT_MAX);
      return result;
    }
    
    if(buf[i] == '\r') {
      result = pmath_string_part(pmath_ref(tb->text), 0, i);
      ++i;
      if(i < len) {
        if(buf[i] == '\n') 
          ++i;
      }
      else
        tb->skip_newline_at_start = TRUE;
      
      tb->text = pmath_string_part(tb->text, i, INT_MAX);
      return result;
    }
  }
  
  result = tb->text;
  tb->text = pmath_string_new(0);
  return result;
}

static size_t textbuffer_write_latin1(void *extra, const void *buffer, size_t buffer_size) {
  struct textbuffer_t *tb = get_textbuffer(extra);
  
  if(tb->status != PMATH_FILE_OK)
    return 0;
    
  if(buffer_size > INT_MAX) {
    pmath_unref(tb->text);
    tb->text = PMATH_NULL;
    return 0;
  }
  
  tb->text = pmath_string_insert_latin1(tb->text, INT_MAX, buffer, (int)buffer_size);
  return buffer_size;
}

static size_t textbuffer_read_latin1(void *extra, void *buffer, size_t buffer_size) {
  struct textbuffer_t *tb = get_textbuffer(extra);
  const uint16_t *buf;
  int             len;
  uint8_t *buffer_as_ui8;
  
  if(tb->status != PMATH_FILE_OK)
    return 0;
    
  buf = pmath_string_buffer(&tb->text);
  len = pmath_string_length(tb->text);
  
  if(buffer_size < (size_t)len)
    len = (int)buffer_size;
  else
    buffer_size = (size_t)len;
    
  buffer_as_ui8 = buffer;
  while(len-- > 0)
    *buffer_as_ui8++ = (uint8_t) * buf++;
    
  return buffer_size;
}

static const char base85_char[85] = "0123456789"
                                    "ABCDEFGHIJ"
                                    "KLMNOPQRST"
                                    "UVWXYZabcd"
                                    "efghijklmn"
                                    "opqrstuvwx"
                                    "yz!#$%&()*"
                                    "+-;<=>?@^_"
                                    ",{}[]";
static const char base85_flush[] = "~~~";
static uint8_t base85_value[128] = { /* inverse of base85_char */
  /* 0_ */  0,  0,  0,  0,  0,  0,  0,  0,   0,  0,  0,  0,  0,  0,  0,  0,
  /* 1_ */  0,  0,  0,  0,  0,  0,  0,  0,   0,  0,  0,  0,  0,  0,  0,  0,
  /* 2_ */  0, 62,  0, 63, 64, 65, 66,  0,  67, 68, 69, 70, 80, 71,  0,  0,
  /* 3_ */  0,  1,  2,  3,  4,  5,  6,  7,   8,  9,  0, 72, 73, 74, 75, 76,
  /* 4_ */ 77, 10, 11, 12, 13, 14, 15, 16,  17, 18, 19, 20, 21, 22, 23, 24,
  /* 5_ */ 25, 26, 27, 28, 29, 30, 31, 32,  33, 34, 35, 83,  0, 84, 78, 79,
  /* 6_ */  0, 36, 37, 38, 39, 40, 41, 42,  43, 44, 45, 46, 47, 48, 49, 50,
  /* 7_ */ 51, 52, 53, 54, 55, 56, 57, 58,  59, 60, 61, 81,  0, 82,  0,  0
};

#ifdef PMATH_DEBUG_TESTS
static void debug_check_base85(void) {
  int i;
  for(i = 0; i < 85; ++i) {
    if(base85_value[0x7F & (uint8_t)base85_char[i]] != i) {
      pmath_debug_print("base85 db error at %d\n", i);
    }
  }
}
#endif

static void encode_base85(const uint8_t *in4, char *out5) {
  uint32_t ui = ((uint32_t)in4[0] << 24) | ((uint32_t)in4[1] << 16) | ((uint32_t)in4[2] << 8) | (uint32_t)in4[3];
  
  out5[4] = base85_char[ui % 85];
  ui /= 85;
  out5[3] = base85_char[ui % 85];
  ui /= 85;
  out5[2] = base85_char[ui % 85];
  ui /= 85;
  out5[1] = base85_char[ui % 85];
  ui /= 85;
  out5[0] = base85_char[ui % 85];
}

static size_t decode_base85(const uint16_t *in5, uint8_t *out4) {
  uint32_t ui;
  uint8_t full5[5];
  size_t result;
  
  full5[0] = in5[0] & 0x7F;
  full5[1] = in5[1] & 0x7F;
  full5[2] = in5[2] & 0x7F;
  full5[3] = in5[3] & 0x7F;
  full5[4] = in5[4] & 0x7F;
  
  result = 4;
  if(full5[4] == (unsigned char)'~') {
    full5[4] = base85_char[84];
    result = 3;
    if(full5[3] == (unsigned char)'~') {
      full5[3] = base85_char[84];
      result = 2;
      if(full5[2] == (unsigned char)'~') {
        full5[2] = base85_char[84];
        result = 1;
      }
    }
  }
  
  ui =           base85_value[full5[0]];
  ui = ui * 85 + base85_value[full5[1]];
  ui = ui * 85 + base85_value[full5[2]];
  ui = ui * 85 + base85_value[full5[3]];
  ui = ui * 85 + base85_value[full5[4]];
  
  out4[3] = ui & 0xFF;
  ui = ui >> 8;
  out4[2] = ui & 0xFF;
  ui = ui >> 8;
  out4[1] = ui & 0xFF;
  ui = ui >> 8;
  out4[0] = ui & 0xFF;
  return result;
}

static size_t textbuffer_write_base85(void *extra, const void *buffer, size_t buffer_size) {
  struct textbuffer_t *tb = get_textbuffer(extra);
  uint8_t *end = tb->outbuffer + 4;
  size_t written = 0;
  
  char block5[5];
  
  if(tb->status != PMATH_FILE_OK)
    return 0;
    
  if((size_t)tb->out > (size_t)tb->outbuffer) {
    size_t out_avail = (size_t)end - (size_t)tb->out;
    
    assert(out_avail > 0);
    assert(out_avail < 4);
    
    if(buffer_size < out_avail) {
      memcpy(tb->out, buffer, buffer_size);
      tb->out += buffer_size;
      return buffer_size;
    }
    
    memcpy(tb->out, buffer, out_avail);
    buffer = (uint8_t*)buffer + out_avail;
    buffer_size -= out_avail;
    written = out_avail;
    
    encode_base85(tb->outbuffer, block5);
    tb->text = pmath_string_insert_latin1(tb->text, INT_MAX, block5, 5);
    tb->out = tb->outbuffer;
  }
  
  while(buffer_size >= 4) {
    encode_base85(buffer, block5);
    tb->text = pmath_string_insert_latin1(tb->text, INT_MAX, block5, 5);
    
    buffer = (uint8_t*)buffer + 4;
    written += 4;
    buffer_size -= 4;
  }
  
  if(buffer_size > 0) {
    memcpy(tb->outbuffer, buffer, buffer_size);
    tb->out += buffer_size;
    written += buffer_size;
  }
  
  return written;
}

static size_t textbuffer_read_base85(void *extra, void *buffer, size_t buffer_size) {
  struct textbuffer_t *tb = get_textbuffer(extra);
  
  const uint16_t *buf = pmath_string_buffer(&tb->text);
  int             len = pmath_string_length(tb->text);
  
  size_t written = 0;
  uint8_t *end = tb->inbuffer + sizeof(tb->inbuffer);
  
  
  if(tb->status != PMATH_FILE_OK)
    return 0;
    
  if(tb->in != end) {
    size_t avail = (size_t)end - (size_t)tb->in;
    
    if(buffer_size < avail) {
      memcpy(buffer, tb->in, buffer_size);
      tb->in += buffer_size;
      return buffer_size;
    }
    
    memcpy(buffer, tb->in, avail);
    tb->in = end;
    buffer =      (uint8_t*)buffer + avail;
    buffer_size -= avail;
    written =     avail;
  }
  
  while(buffer_size >= 4 && len >= 5) {
    size_t size = decode_base85(buf, buffer);
    buf += 5;
    len -= 5;
    buffer =      (uint8_t*)buffer + size;
    buffer_size -= size;
    written +=     size;
  }
  
  if(len >= 5 && buffer_size > 0) {
    uint8_t out4[4];
    size_t size = decode_base85(buf, out4);
    len -= 5;
    
    if(buffer_size >= size) {
      memcpy(buffer, out4, size);
      written += size;
    }
    else {
      memcpy(buffer, out4, buffer_size);
      written += buffer_size;
      size -= buffer_size;
      
      tb->in = end - size;
      memcpy(tb->in, out4 + buffer_size, size);
    }
  }
  
  tb->text = pmath_string_part(tb->text, pmath_string_length(tb->text) - len, -1);
  return written;
}

static void textbuffer_flush_base85(void *extra) {
  struct textbuffer_t *tb = get_textbuffer(extra);
  uint8_t *end = tb->outbuffer + 4;
  
  if((size_t)tb->out > (size_t)tb->outbuffer) {
    size_t out_avail = (size_t)end - (size_t)tb->out;
    char block5[5];
    
    assert(out_avail > 0);
    assert(out_avail < 4);
    
    memset(tb->out, 0, out_avail);
    encode_base85(tb->outbuffer, block5);
    
    tb->text = pmath_string_insert_latin1(tb->text, INT_MAX, block5,       5 - (int)out_avail);
    tb->text = pmath_string_insert_latin1(tb->text, INT_MAX, base85_flush, (int)out_avail);
  }
}

PMATH_API
void pmath_file_create_mixed_buffer(
  const char *encoding,
  pmath_symbol_t *out_textfile,
  pmath_symbol_t *out_binfile
) {
  struct textbuffer_t     *tb;
  pmath_text_file_api_t    txtapi;
  pmath_binary_file_api_t  binapi;
  pmath_custom_t custom;
  
#ifdef PMATH_DEBUG_TESTS
  debug_check_base85();
#endif
  
  memset(&txtapi, 0, sizeof(txtapi));
  memset(&binapi, 0, sizeof(binapi));
  txtapi.struct_size = sizeof(txtapi);
  binapi.struct_size = sizeof(binapi);
  
  assert(out_textfile != NULL);
  assert(out_binfile != NULL);
  
  *out_textfile = PMATH_NULL;
  *out_binfile  = PMATH_NULL;
  
  tb = pmath_mem_alloc(sizeof(struct textbuffer_t));
  if(!tb)
    return;
    
  txtapi.status_function = textbuffer_text_status;
  txtapi.readln_function = textbuffer_readln;
  txtapi.write_function  = textbuffer_write;
  
  binapi.status_function = textbuffer_bin_status;
  if(0 == strcmp(encoding, "latin1")) {
    binapi.read_function  = textbuffer_read_latin1;
    binapi.write_function = textbuffer_write_latin1;
    tb->blocklen     = 0;
    tb->flush_func   = NULL;
  }
  else if(0 == strcmp(encoding, "base85")) {
    txtapi.write_function = textbuffer_write_skipping_whitespace;
    
    binapi.read_function  = textbuffer_read_base85;
    binapi.write_function = textbuffer_write_base85;
    binapi.flush_function = textbuffer_flush_base85;
    tb->blocklen     = 5;
    tb->flush_func   = textbuffer_flush_base85;
  }
  else {
    pmath_mem_free(tb);
    return;
  }
  
  tb->text   = pmath_string_new(16);
  tb->in     = tb->inbuffer + sizeof(tb->inbuffer);
  tb->out    = tb->outbuffer;
  tb->status = PMATH_FILE_OK;
  tb->skip_newline_at_start = 0;
  
  custom = pmath_custom_new(tb, textbuffer_destroy_custom_data);
  if(pmath_is_null(custom))
    return;
    
  custom = pmath_ref(custom);
  
  *out_textfile = pmath_file_create_text(
                    PMATH_AS_PTR(custom),
                    textbuffer_destroy_custom,
                    &txtapi);
                    
  *out_binfile = pmath_file_create_binary(
                   PMATH_AS_PTR(custom),
                   textbuffer_destroy_custom,
                   &binapi);
}
