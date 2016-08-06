#include <pmath-util/files/binary-buffer.h>
#include <pmath-util/memory.h>


struct binbuf_t {
  uint8_t *data;
  
  uint8_t *read_ptr;
  uint8_t *write_ptr;
  
  size_t capacity;
  pmath_bool_t error;
};

static void destroy_binbuf(void *p) {
  struct binbuf_t *bb = (struct binbuf_t *)p;
  
  pmath_mem_free(bb->data);
  pmath_mem_free(bb);
}

static struct binbuf_t *create_binbuf(size_t capacity) {
  struct binbuf_t *bb = pmath_mem_alloc(sizeof(struct binbuf_t));
  if(!bb)
    return NULL;
    
  bb->error    = FALSE;
  bb->capacity = capacity;
  bb->data     = pmath_mem_alloc(capacity);
  if(!bb->data) {
    pmath_mem_free(bb);
    return NULL;
  }
  
  bb->read_ptr = bb->write_ptr = bb->data;
  return bb;
}

static pmath_files_status_t binbuf_status(void *p) {
  struct binbuf_t *bb = (struct binbuf_t *)p;
  
  if(bb->error)
    return PMATH_FILE_OTHERERROR;
    
  assert((size_t)bb->data     <= (size_t)bb->read_ptr);
  assert((size_t)bb->read_ptr <= (size_t)bb->write_ptr);
  
  if(bb->read_ptr == bb->write_ptr)
    return PMATH_FILE_ENDOFFILE;
    
  return PMATH_FILE_OK;
}

static size_t binbuf_read(void *p, void *buffer, size_t buffer_size) {
  struct binbuf_t *bb = (struct binbuf_t *)p;
  
  if(bb->error)
    return 0;
    
  assert((size_t)bb->data     <= (size_t)bb->read_ptr);
  assert((size_t)bb->read_ptr <= (size_t)bb->write_ptr);
  
  if(buffer_size > (size_t)bb->write_ptr - (size_t)bb->read_ptr)
    buffer_size = (size_t)bb->write_ptr - (size_t)bb->read_ptr;
    
  memcpy(buffer, bb->read_ptr, buffer_size);
  bb->read_ptr += buffer_size;
  
  return buffer_size;
}

static size_t binbuf_write(void *p, const void *buffer, size_t buffer_size) {
  struct binbuf_t *bb = (struct binbuf_t *)p;
  size_t have_written;
  
  if(bb->error)
    return 0;
    
  assert((size_t)bb->data     <= (size_t)bb->read_ptr);
  assert((size_t)bb->read_ptr <= (size_t)bb->write_ptr);
  
  have_written = (size_t)bb->write_ptr - (size_t)bb->data;
  if(buffer_size + have_written > bb->capacity) {
    size_t have_read = (size_t)bb->read_ptr - (size_t)bb->data;
    
    if(buffer_size + have_written <= bb->capacity + have_read) {
      memmove(bb->data, bb->read_ptr, have_written - have_read);
    }
    else {
      size_t new_cap = bb->capacity ? bb->capacity : 0x100;
      
      while( new_cap >= bb->capacity &&
             new_cap <  0x1000       &&
             new_cap <  have_written + buffer_size)
      {
        new_cap *= 2;
      }
      
      if(new_cap < have_written + buffer_size) {
        new_cap = ((have_written + buffer_size - 1) / 0x1000 + 1) * 0x1000;
      }
      
      if(new_cap < bb->capacity) {
        bb->error = TRUE;
        pmath_mem_free(bb->data);
        bb->data = bb->read_ptr = bb->write_ptr = NULL;
        bb->capacity = 0;
        return 0;
      }
      
      bb->data = pmath_mem_realloc(bb->data, new_cap);
      if(!bb->data) {
        bb->error = TRUE;
        bb->data = bb->read_ptr = bb->write_ptr = NULL;
        bb->capacity = 0;
        return 0;
      }
      
      bb->read_ptr  = bb->data + have_read;
      bb->write_ptr = bb->data + have_written;
      bb->capacity  = new_cap;
    }
  }
  
  memcpy(bb->write_ptr, buffer, buffer_size);
  bb->write_ptr += buffer_size;
  return buffer_size;
}

static void binbuf_get_readablebytes(void *p, void *result) {
  struct binbuf_t *bb = (struct binbuf_t *)p;
  
  if(bb->error) {
    *(size_t *)result = 0;
  }
  else {
    assert((size_t)bb->data     <= (size_t)bb->read_ptr);
    assert((size_t)bb->read_ptr <= (size_t)bb->write_ptr);
    
    *(size_t *)result = (size_t)bb->write_ptr - (size_t)bb->read_ptr;
  }
}

PMATH_API
pmath_symbol_t pmath_file_create_binary_buffer(size_t mincapacity) {
  pmath_binary_file_api_t api;
  struct binbuf_t *bb;
  
  bb = create_binbuf(mincapacity);
  if(!bb)
    return PMATH_NULL;
    
  memset(&api, 0, sizeof(api));
  api.struct_size     = sizeof(api);
  api.status_function = binbuf_status;
  api.read_function   = binbuf_read;
  api.write_function  = binbuf_write;
  
  return pmath_file_create_binary(bb, destroy_binbuf, &api);
}

PMATH_API
size_t pmath_file_binary_buffer_size(
  pmath_t binfile
) {
  size_t result = 0;
  pmath_file_manipulate(
    binfile,
    destroy_binbuf,
    binbuf_get_readablebytes,
    &result);
    
  return result;
}

struct binbuf_manipulate_t {
  void (*callback)(uint8_t *, uint8_t **, const uint8_t *, void *);
  void *closure;
};

static void binbuf_manipulate(void *p, void *extra) {
  struct binbuf_t *bb = (struct binbuf_t *)p;
  struct binbuf_manipulate_t *info = (struct binbuf_manipulate_t *)extra;
  
  if(!bb->error) {
    assert((size_t)bb->data      <= (size_t)bb->read_ptr);
    assert((size_t)bb->read_ptr  <= (size_t)bb->write_ptr);
    assert((size_t)bb->write_ptr <= (size_t)bb->data + bb->capacity);
    
    info->callback(
      bb->read_ptr,
      &bb->write_ptr,
      bb->data + bb->capacity,
      info->closure);
      
    assert((size_t)bb->read_ptr  <= (size_t)bb->write_ptr);
    assert((size_t)bb->write_ptr <= (size_t)bb->data + bb->capacity);
  }
}

PMATH_API
void pmath_file_binary_buffer_manipulate(
  pmath_t   binfile,
  void    (*callback)(uint8_t *readable, uint8_t **writable, const uint8_t *end, void *closure),
  void     *closure
) {
  if(callback) {
    struct binbuf_manipulate_t info;
    
    info.callback = callback;
    info.closure = closure;
    
    pmath_file_manipulate(
      binfile,
      destroy_binbuf,
      binbuf_manipulate,
      &info);
  }
}
