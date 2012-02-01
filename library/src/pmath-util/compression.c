#include <pmath-util/compression.h>
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

static size_t compressor_deflate(
  struct compressor_data_t *data,
  const void               *buffer,
  size_t                    buffer_size,
  int                       zlib_flush_value
) {
  int ret = Z_OK;
  uInt old_avail_out;
  if(data->status != PMATH_FILE_OK)
    return 0;
    
  data->info.next_in  = (void*)buffer;
  data->info.avail_in = (uInt)buffer_size;
  do {
    if(data->info.avail_out == 0) {
      size_t size = sizeof(data->outbuffer);
      
      data->info.next_out  = data->outbuffer;
      data->info.avail_out = size;
      if(size != pmath_file_write(data->file, data->outbuffer, size)) {
        data->status = pmath_file_status(data->file);
        if(data->status == PMATH_FILE_OK)
          data->status = PMATH_FILE_OTHERERROR;
          
        return buffer_size - data->info.avail_in;
      }
    }
    
    old_avail_out = data->info.avail_out;
    
    ret = deflate(&data->info, zlib_flush_value);
    if(ret < 0 && ret != Z_BUF_ERROR) {
      data->status = PMATH_FILE_OTHERERROR;
      return buffer_size - data->info.avail_in;
    }
  } while(data->info.avail_in > 0 || old_avail_out != data->info.avail_out);
  
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
}

static void compressor_deflate_destructor(void *extra) {
  struct compressor_data_t *data = extra;
  
  compressor_deflate(data, NULL, 0, Z_FINISH);
  pmath_file_write(data->file, data->outbuffer, sizeof(data->outbuffer) - data->info.avail_out);
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
  
  api.status_function = compressor_status;
  api.write_function  = compressor_write;
  api.flush_function  = compressor_flush;
  
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
  
  ret = deflateInit(&data->info, Z_DEFAULT_COMPRESSION);
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
  
  api.status_function = compressor_status;
  api.read_function   = compressor_read;
  
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
  data->info.zalloc = alloc_for_zlib;
  data->info.zfree  = free_for_zlib;
  
  ret = inflateInit(&data->info);
  if(ret != Z_OK) {
    pmath_unref(srcfile);
    return PMATH_NULL;
  }
  
  data->info.next_in  = data->outbuffer;
  data->info.avail_in = 0;
  
  return pmath_file_create_binary(data, compressor_inflate_destructor, &api);
}
