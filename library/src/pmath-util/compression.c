#include <pmath-util/compression.h>
#include <pmath-util/debug.h>
#include <pmath-util/memory.h>


#include <string.h>
#include <zlib.h>


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
pmath_symbol_t pmath_file_create_compressor(pmath_t dstfile) {
  struct compressor_data_t *data;
  pmath_binary_file_api_t  api;
  int ret;
  memset(&api, 0, sizeof(api));
  api.struct_size = sizeof(api);
  
  api.status_function  = compressor_status;
  api.write_function   = compressor_write;
  api.flush_function   = compressor_flush;
  api.get_pos_function = compressor_get_position;
  
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
          Z_DEFAULT_COMPRESSION,
          Z_DEFLATED,
          -MAX_WBITS,
          8, // or MAX_MEM_LEVEL
          Z_DEFAULT_STRATEGY);
  if(ret != Z_OK) {
    pmath_unref(dstfile);
    return PMATH_NULL;
  }
  
  data->info.next_out  = data->outbuffer;
  data->info.avail_out = sizeof(data->outbuffer);
  
  return pmath_file_create_binary(data, compressor_deflate_destructor, &api);
}

PMATH_API
pmath_symbol_t pmath_file_create_uncompressor(pmath_t srcfile) {
  struct compressor_data_t *data;
  pmath_binary_file_api_t  api;
  int ret;
  memset(&api, 0, sizeof(api));
  api.struct_size = sizeof(api);
  
  api.status_function  = compressor_status;
  api.read_function    = compressor_read;
  api.get_pos_function = compressor_get_position;
  
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
  ret = inflateInit2(&data->info, -MAX_WBITS);
  if(ret != Z_OK) {
    pmath_unref(srcfile);
    return PMATH_NULL;
  }
  
  
  return pmath_file_create_binary(data, compressor_inflate_destructor, &api);
}
