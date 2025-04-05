#include <pmath-util/compression.h>

#include <pmath-util/concurrency/threads.h>
#include <pmath-util/debug.h>
#include <pmath-util/files/mixed-buffer.h>
#include <pmath-util/helpers.h>
#include <pmath-util/memory.h>
#include <pmath-util/messages.h>
#include <pmath-util/serialize.h>

#include <string.h>
#include <zlib.h>


extern pmath_symbol_t pmath_System_InputStream;
extern pmath_symbol_t pmath_System_OutputStream;


static void *alloc_for_zlib(void *opaque, uInt items, uInt size) {
  return pmath_mem_alloc(items * size);
}

static void free_for_zlib(void *opaque, void *address) {
  pmath_mem_free(address);
}

/*============================================================================*/

struct compressor_data_t {
  pmath_t              file;
  pmath_files_status_t status;
  z_stream             info;
  Bytef                outbuffer[64];
};

static pmath_bool_t compressor_flush_outbuffer(struct compressor_data_t *data) {
  size_t size = sizeof(data->outbuffer) - data->info.avail_out;
  
  data->info.next_out  = data->outbuffer;
  data->info.avail_out = sizeof(data->outbuffer);
  
  if(size != pmath_file_write(data->file, data->outbuffer, size)) {
    data->status = pmath_file_status(data->file);
    if(data->status == PMATH_FILE_OK)
      data->status = PMATH_FILE_OTHERERROR;
      
    return FALSE;
  }
  
  return TRUE;
}

static size_t compressor_deflate(
  struct compressor_data_t *data,
  const void               *buffer,
  size_t                    buffer_size,
  int                       zlib_flush_value
) {
  int ret = Z_OK;
  pmath_bool_t force_continue_deflate;
  uInt old_avail_out;
  if(data->status != PMATH_FILE_OK)
    return 0;
    
  data->info.next_in  = (void*)buffer;
  data->info.avail_in = (uInt)buffer_size;
  do {
    if(data->info.avail_out == 0) {
      if(!compressor_flush_outbuffer(data))
        return buffer_size - data->info.avail_in;
    }
    
    old_avail_out = data->info.avail_out;
    
    force_continue_deflate = FALSE;
    ret = deflate(&data->info, zlib_flush_value);
    if(ret < 0 && ret != Z_BUF_ERROR) {
      data->status = PMATH_FILE_OTHERERROR;
      return buffer_size - data->info.avail_in;
    }
    else if(ret == Z_OK && zlib_flush_value == Z_FINISH) {
      force_continue_deflate = !pmath_aborting();
    }
  } while(data->info.avail_in > 0 || old_avail_out != data->info.avail_out || force_continue_deflate);
  
  return buffer_size - data->info.avail_in;
}

static size_t compressor_inflate(
  struct compressor_data_t *data,
  void                     *buffer,
  size_t                    buffer_size,
  int                       zlib_flush_value
) {
  int ret = Z_OK;
  if(data->status != PMATH_FILE_OK)
    return 0;
    
  data->info.next_out  = buffer;
  data->info.avail_out = buffer_size;
  while(data->info.avail_out > 0) {
  
    if(data->info.avail_in == 0) {
      size_t size = sizeof(data->outbuffer);
      
      data->info.next_in  = data->outbuffer;
      data->info.avail_in = pmath_file_read(data->file, data->outbuffer, size, FALSE);
      if(data->info.avail_in == 0) {
        data->status = pmath_file_status(data->file);
        if(data->status == PMATH_FILE_OK)
          data->status = PMATH_FILE_ENDOFFILE;
          
        return buffer_size - data->info.avail_out;
      }
    }
    
    ret = inflate(&data->info, zlib_flush_value);
    if(ret < 0) {
      data->status = PMATH_FILE_OTHERERROR;
      return buffer_size - data->info.avail_out;
    }
    
    if(ret == Z_STREAM_END) {
      data->status = PMATH_FILE_ENDOFFILE;
      return buffer_size - data->info.avail_out;
    }
  }
  
  return buffer_size;
}

static pmath_files_status_t compressor_status(void *extra) {
  struct compressor_data_t *data = extra;
  if(data->status != PMATH_FILE_OK)
    return data->status;
    
  return data->status = pmath_file_status(data->file);
}

static size_t compressor_write(void *extra, const void *buffer, size_t buffer_size) {
  struct compressor_data_t *data = extra;
  
  return compressor_deflate(data, buffer, buffer_size, Z_NO_FLUSH);
}

static size_t compressor_read(void *extra, void *buffer, size_t buffer_size) {
  struct compressor_data_t *data = extra;
  
  return compressor_inflate(data, buffer, buffer_size, Z_NO_FLUSH);
}

static void compressor_flush(void *extra) {
  struct compressor_data_t *data = extra;
  
  compressor_deflate(data, NULL, 0, Z_SYNC_FLUSH);
  compressor_flush_outbuffer(data);
  pmath_file_flush(data->file);
}

static int64_t compressor_get_position(void *extra) {
  struct compressor_data_t *data = extra;
  
  int64_t pos = pmath_file_get_position(data->file);
  if(pos < 0)
    return -1;
    
  if(data->info.next_in == data->outbuffer) {
    pos -= data->info.avail_in;
    
    if(pos < 0) {
      pmath_debug_print("[decompressor: avail_in > pmath_file_get_position]\n");
    }
    
    return pos;
  }
  
  if(data->info.next_out == data->outbuffer) {
    pos -= data->info.avail_out;
    
    if(pos < 0) {
      pmath_debug_print("[compressor: avail_out > pmath_file_get_position]\n");
    }
    
    return pos;
  }
  
  return pos;
}

static void compressor_deflate_destructor(void *extra) {
  struct compressor_data_t *data = extra;
  
  compressor_deflate(data, NULL, 0, Z_FINISH);
  compressor_flush_outbuffer(data);
  
  deflateEnd(&data->info);
  pmath_file_close_if_unused(data->file);
  pmath_mem_free(data);
}

static void compressor_inflate_destructor(void *extra) {
  struct compressor_data_t *data = extra;
  
  //compressor_inflate(data, NULL, 0, Z_FINISH);
  inflateEnd(&data->info);
  pmath_file_close_if_unused(data->file);
  pmath_mem_free(data);
}


PMATH_API
pmath_symbol_t pmath_file_create_compressor(pmath_t dstfile, struct pmath_compressor_settings_t *options) {
  struct compressor_data_t *data;
  pmath_binary_file_api_t  api;
  int ret;
  int window_bits = MAX_WBITS;
  int level = Z_DEFAULT_COMPRESSION;
  int strategy = Z_DEFAULT_STRATEGY;
  memset(&api, 0, sizeof(api));
  api.struct_size = sizeof(api);
  
  api.status_function  = compressor_status;
  api.write_function   = compressor_write;
  api.flush_function   = compressor_flush;
  api.get_pos_function = compressor_get_position;
  
  if(options) {
    if(options->size != sizeof(struct pmath_compressor_settings_t)) {
      pmath_debug_print("[invalid pmath_compressor_settings_t size]\n");
      pmath_unref(dstfile);
      return PMATH_NULL;
    }
    
    if(options->level != Z_DEFAULT_COMPRESSION) {
      if(options->level < Z_NO_COMPRESSION || options->level > Z_BEST_COMPRESSION) {
        pmath_debug_print("[invalid compression level %d]\n", options->level);
        pmath_unref(dstfile);
        return PMATH_NULL;
      }
      
      level = options->level;
    }
    
    if(options->window_bits != 0) {
      if(options->window_bits < 8 || options->window_bits > MAX_WBITS) {
        pmath_debug_print("[invalid window_bits %d]\n", options->window_bits);
        pmath_unref(dstfile);
        return PMATH_NULL;
      }
      
      window_bits = options->window_bits;
    }
    
    if(options->skip_header)
      window_bits = -window_bits;
    
    strategy = options->strategy;
  }
  
  if(pmath_is_expr_of_len(dstfile, pmath_System_OutputStream, 1)) {
    pmath_t item = pmath_expr_get_item(dstfile, 1);
    pmath_unref(dstfile);
    dstfile = item;
  }
  
  if(!pmath_file_test(dstfile, PMATH_FILE_PROP_WRITE | PMATH_FILE_PROP_BINARY)) {
    pmath_unref(dstfile);
    return PMATH_NULL;
  }
  
  data = pmath_mem_alloc(sizeof(struct compressor_data_t));
  if(!data) {
    pmath_unref(dstfile);
    return PMATH_NULL;
  }
  
  data->file = dstfile;
  data->status  = PMATH_FILE_OK;
  
  memset(&data->info, 0, sizeof(data->info));
  data->info.zalloc = alloc_for_zlib;
  data->info.zfree  = free_for_zlib;
  
//  ret = deflateInit(&data->info, Z_DEFAULT_COMPRESSION);
  ret = deflateInit2(
          &data->info,
          level,
          Z_DEFLATED,
          window_bits,
          8, // or MAX_MEM_LEVEL
          strategy);
  if(ret != Z_OK) {
    pmath_debug_print("[deflateInit2() failed with error %d]\n", ret);
    pmath_unref(dstfile);
    return PMATH_NULL;
  }
  
  data->info.next_out  = data->outbuffer;
  data->info.avail_out = sizeof(data->outbuffer);
  
  dstfile = pmath_file_create_binary(data, compressor_deflate_destructor, &api);
  return pmath_expr_new_extended(pmath_ref(pmath_System_OutputStream), 1, dstfile);
}

PMATH_API
pmath_symbol_t pmath_file_create_decompressor(pmath_t srcfile, struct pmath_decompressor_settings_t *options) {
  struct compressor_data_t *data;
  pmath_binary_file_api_t  api;
  int ret;
  int window_bits = MAX_WBITS;
  memset(&api, 0, sizeof(api));
  api.struct_size = sizeof(api);
  
  api.status_function  = compressor_status;
  api.read_function    = compressor_read;
  api.get_pos_function = compressor_get_position;
  
  if(options) {
    if(options->size != sizeof(struct pmath_decompressor_settings_t)) {
      pmath_debug_print("[invalid pmath_decompressor_settings_t size]\n");
      pmath_unref(srcfile);
      return PMATH_NULL;
    }
    
    if(options->window_bits != 0) {
      if(options->window_bits < 8 || options->window_bits > MAX_WBITS) {
        pmath_debug_print("[invalid window_bits %d]\n", options->window_bits);
        pmath_unref(srcfile);
        return PMATH_NULL;
      }
      
      window_bits = options->window_bits;
    }
    
    if(options->skip_header)
      window_bits = -window_bits;
  }
  
  if(pmath_is_expr_of_len(srcfile, pmath_System_InputStream, 1)) {
    pmath_t item = pmath_expr_get_item(srcfile, 1);
    pmath_unref(srcfile);
    srcfile = item;
  }
  
  if(!pmath_file_test(srcfile, PMATH_FILE_PROP_READ | PMATH_FILE_PROP_BINARY)) {
    pmath_unref(srcfile);
    return PMATH_NULL;
  }
  
  data = pmath_mem_alloc(sizeof(struct compressor_data_t));
  if(!data) {
    pmath_unref(srcfile);
    return PMATH_NULL;
  }
  
  data->file   = srcfile;
  data->status = PMATH_FILE_OK;
  
  memset(&data->info, 0, sizeof(data->info));
  data->info.zalloc   = alloc_for_zlib;
  data->info.zfree    = free_for_zlib;
  data->info.next_in  = data->outbuffer;
  data->info.avail_in = 0;
  
//  ret = inflateInit(&data->info);
  ret = inflateInit2(&data->info, window_bits);
  if(ret != Z_OK) {
    pmath_debug_print("[inflateInit2() failed with error %d]\n", ret);
    pmath_unref(srcfile);
    return PMATH_NULL;
  }
  
  srcfile = pmath_file_create_binary(data, compressor_inflate_destructor, &api);
  return pmath_expr_new_extended(pmath_ref(pmath_System_InputStream), 1, srcfile);
}

/* ========================================================================== */

PMATH_API
pmath_string_t pmath_compress_to_string(pmath_t obj) {
  pmath_t bfile, zfile, tfile;
  pmath_serialize_error_t err;
  
  pmath_file_create_mixed_buffer("base85", &tfile, &bfile);
  zfile = pmath_file_create_compressor(pmath_ref(bfile), NULL);
  err = pmath_serialize(zfile, obj, 0);
  pmath_file_close(zfile);
  pmath_file_close(bfile);
  
  if(err != PMATH_SERIALIZE_OK) {
    pmath_file_close(tfile);
    return PMATH_NULL;
  }
  
  obj = pmath_file_readline(tfile);
  obj = pmath_string_insert_latin1(obj, 0, "1:", 2);
  pmath_file_close(tfile);
  return obj;
}

PMATH_API
pmath_t pmath_decompress_from_string(pmath_string_t str) {
  pmath_t bfile, tfile, zfile;
  pmath_serialize_error_t err;
  pmath_t result;
  int len;
  const uint16_t *buf;
  
  if(pmath_is_null(str))
    return PMATH_UNDEFINED;
    
  assert(pmath_is_string(str));
  
  buf = pmath_string_buffer(&str);
  len = pmath_string_length(str);
  if(len > 2 && buf[1] == ':' && buf[0] == '1') {
    buf+= 2;
    len-= 2;
  }
  else {
    pmath_message(PMATH_NULL, "corrupt", 1, str);
    return PMATH_UNDEFINED;
  }
  
  pmath_file_create_mixed_buffer("base85", &tfile, &bfile);
  pmath_file_writetext(tfile, buf, len);
  pmath_file_close(tfile);
  
  zfile = pmath_file_create_decompressor(pmath_ref(bfile), NULL);
  result = pmath_deserialize(zfile, &err);
  pmath_file_close(zfile);
  pmath_file_close(bfile);
  
  if(err != PMATH_SERIALIZE_OK) {
    if(err != PMATH_SERIALIZE_NO_MEMORY)
      pmath_message(PMATH_NULL, "corrupt", 1, str);
    else
      pmath_unref(str);
    pmath_unref(result);
    return PMATH_UNDEFINED;
  }
  
  pmath_unref(str);
  return result;
}
