#include <pmath-util/files.h>

#include <pmath-core/custom.h>

#include <pmath-language/scanner.h>

#include <pmath-util/concurrency/threads.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/memory.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/control-private.h>
#include <pmath-builtins/control/definitions-private.h>

#include <errno.h>
#include <iconv.h>
#include <limits.h>
#include <string.h>


struct _file_t{
  volatile intptr_t _lock;
  
  void  *extra;
  void (*extra_destructor)(void*);
  
  pmath_files_status_t (*status_function)(void *extra);
  void                 (*flush_function)( void *extra);
};

struct _pmath_binary_file_t{
  struct _file_t  inherited;
  
  size_t (*read_function)( void *extra,       void *buffer, size_t buffer_size);
  size_t (*write_function)(void *extra, const void *buffer, size_t buffer_size);
  
  size_t buffer_size;
  uint8_t *buffer;
  
  uint8_t *current_buffer_start;
  uint8_t *current_buffer_end;
};

struct _pmath_text_file_t{
  struct _file_t  inherited;
  
  pmath_string_t (*readln_function)(void *extra);
  pmath_bool_t (*write_function)(void *extra, const uint16_t *str, int len);
  
  pmath_string_t buffer;
};

static pmath_bool_t lock_file(struct _file_t *f){
  intptr_t me = (intptr_t)pmath_thread_get_current();
  
  if(f->_lock == me)
    return FALSE;
  
  pmath_atomic_loop_yield();
  
  while(!pmath_atomic_compare_and_set(&f->_lock, 0, me)){
    pmath_atomic_loop_nop();
  }
  
  return TRUE;
}

static void unlock_file(struct _file_t *f){
  pmath_atomic_unlock(&f->_lock);
}

static void destroy_binary_file(void *ptr){
  struct _pmath_binary_file_t *f = (struct _pmath_binary_file_t*)ptr;
  
  f->inherited.extra_destructor(f->inherited.extra);
  
  pmath_mem_free(f->buffer);
  pmath_mem_free(f);
}

static void destroy_text_file(void *ptr){
  struct _pmath_text_file_t *f = (struct _pmath_text_file_t*)ptr;
  
  f->inherited.extra_destructor(f->inherited.extra);
  
  pmath_unref(f->buffer);
  pmath_mem_free(f);
}

static pmath_t file_set_options(pmath_expr_t expr){
  size_t i;
  pmath_t file;
  
  if(!pmath_is_expr_of(expr, PMATH_SYMBOL_SETOPTIONS))
    return expr;
  
  file = pmath_expr_get_item(expr, 1);
  if(!pmath_file_test(file, 0)){
    pmath_unref(file);
    return expr;
  }
  
  for(i = 2;i <= pmath_expr_length(expr);++i){
    pmath_t rule = pmath_expr_get_item(expr, i);
    
    if(_pmath_is_rule(rule)){
      pmath_t lhs = pmath_expr_get_item(rule, 1);
      
      if(pmath_same(lhs, PMATH_SYMBOL_BINARYFORMAT)){
        pmath_message(
          PMATH_NULL, "changebf", 2,
          file,
          pmath_expr_get_item(rule, 2));
        
        pmath_unref(lhs);
        pmath_unref(rule);
        
        expr = pmath_expr_set_item(expr, i, PMATH_UNDEFINED);
        return pmath_expr_remove_all(expr, PMATH_UNDEFINED);
      }
      
      pmath_unref(lhs);
    }
    
    pmath_unref(rule);
  }
  
  pmath_unref(file);
  return expr;
}

//------------------------------------------------------------------------------

PMATH_API pmath_bool_t pmath_file_test(
  pmath_t file,
  int     properties
){
  if(pmath_is_symbol(file)){
    pmath_custom_t custom = pmath_symbol_get_value(file);
    
    if(pmath_is_custom(custom)){
      if(pmath_custom_has_destructor(custom, destroy_binary_file)){
        if((properties & PMATH_FILE_PROP_TEXT) == 0){
          pmath_bool_t result = TRUE;
          
          struct _pmath_binary_file_t *data = (struct _pmath_binary_file_t*)
            pmath_custom_get_data(custom);
          
          if(properties & PMATH_FILE_PROP_READ)
            result = result && data->read_function != NULL;
          
          if(properties & PMATH_FILE_PROP_WRITE)
            result = result && data->write_function != NULL;
          
          pmath_unref(custom);
          return result;
        }
      }
      else if(pmath_custom_has_destructor(custom, destroy_text_file)){
        if((properties & PMATH_FILE_PROP_BINARY) == 0){
          pmath_bool_t result = TRUE;
          
          struct _pmath_text_file_t *data = (struct _pmath_text_file_t*)
            pmath_custom_get_data(custom);
          
          if(properties & PMATH_FILE_PROP_READ)
            result = result && data->readln_function != NULL;
          
          if(properties & PMATH_FILE_PROP_WRITE)
            result = result && data->write_function != NULL;
          
          pmath_unref(custom);
          return result;
        }
      }
    }
    
    pmath_unref(custom);
  }
  else if(pmath_is_expr_of(file, PMATH_SYMBOL_LIST)){
    if((properties & PMATH_FILE_PROP_READ)  == 0
    && (properties & PMATH_FILE_PROP_WRITE) != 0){
      size_t i;
      
      for(i = pmath_expr_length(file);i > 0;--i){
        pmath_t item = pmath_expr_get_item(file, i);
        
        if(!pmath_file_test(item, properties)){
          pmath_unref(item);
          return FALSE;
        }
        
        pmath_unref(item);
      }
      
      return TRUE;
    }
  }
  
  return FALSE;
}

PMATH_API pmath_files_status_t pmath_file_status(pmath_t file){
  if(pmath_file_test(file, PMATH_FILE_PROP_READ)){
    pmath_custom_t custom = pmath_symbol_get_value(file);
    
    if(pmath_custom_has_destructor(custom, destroy_binary_file)){
      struct _pmath_binary_file_t *data = pmath_custom_get_data(custom);
      
      if(data->inherited.status_function){
        if(lock_file(&data->inherited)){
          pmath_files_status_t result;
          
          result = data->inherited.status_function(data->inherited.extra);
          
          if(result == PMATH_FILE_ENDOFFILE 
          && data->current_buffer_start < data->current_buffer_end)
            result = PMATH_FILE_OK;
          
          unlock_file(&data->inherited);
          
          pmath_unref(custom);
          return result;
        }
        
        pmath_unref(custom);
        return PMATH_FILE_RECURSIVE;
      }
    }
    else if(pmath_custom_has_destructor(custom, destroy_text_file)){
      struct _pmath_text_file_t *data = pmath_custom_get_data(custom);
      
      if(data->inherited.status_function){
        if(lock_file(&data->inherited)){
          pmath_files_status_t result;
          
          result = data->inherited.status_function(data->inherited.extra);
          
          if(result == PMATH_FILE_ENDOFFILE 
          && pmath_string_length(data->buffer) > 0)
            result = PMATH_FILE_OK;
          
          unlock_file(&data->inherited);
          
          pmath_unref(custom);
          return result;
        }
        
        pmath_unref(custom);
        return PMATH_FILE_RECURSIVE;
      }
    }
    
    pmath_unref(custom);
  }
  
  return PMATH_FILE_INVALID;
}

PMATH_API size_t pmath_file_read(
  pmath_t       file,
  void         *buffer,
  size_t        buffer_size,
  pmath_bool_t  preserve_internal_buffer
){
  if(pmath_file_test(file, PMATH_FILE_PROP_READ | PMATH_FILE_PROP_BINARY)){
    pmath_custom_t custom = pmath_symbol_get_value(file);
    
    if(pmath_custom_has_destructor(custom, destroy_binary_file)){
      struct _pmath_binary_file_t *data = pmath_custom_get_data(custom);
      
      if(data->read_function){
        if(lock_file(&data->inherited)){
          size_t bufmax = 0;
          
          if(data->current_buffer_start == data->current_buffer_end){
            bufmax = data->read_function(
              data->inherited.extra, data->buffer, data->buffer_size);
            
            data->current_buffer_start = data->buffer;
            data->current_buffer_end = data->buffer + bufmax;
          }
          else{
            bufmax = (size_t)data->current_buffer_end - (size_t)data->current_buffer_start;
            
            if(bufmax < buffer_size && bufmax < data->buffer_size){
              memmove(data->buffer, data->current_buffer_start, bufmax);
              
              data->current_buffer_start = data->buffer;
              
              bufmax+= data->read_function(
                data->inherited.extra, 
                data->current_buffer_start + bufmax, 
                data->buffer_size - bufmax);
              
              data->current_buffer_end = data->current_buffer_start + bufmax;
            }
          }
          
          if(preserve_internal_buffer){
            if(buffer_size > bufmax)
              buffer_size = bufmax;
            else
              bufmax = buffer_size;
              
            memcpy(buffer, data->current_buffer_start, buffer_size);
          }
          else if(buffer_size <= bufmax){
            memcpy(buffer, data->current_buffer_start, buffer_size);
            
            data->current_buffer_start+= buffer_size;
            bufmax = buffer_size;
          }
          else{
            memcpy(buffer, data->current_buffer_start, bufmax);
            
            data->current_buffer_start = data->current_buffer_end;
            
            bufmax+= data->read_function(
              data->inherited.extra, 
              (uint8_t*)buffer + bufmax, 
              buffer_size - bufmax);
          }
          
          unlock_file(&data->inherited);
          
          pmath_unref(custom);
          return bufmax;
        }
        
        pmath_unref(custom);
        return 0;
      }
    }
    
    pmath_unref(custom);
  }
  
  return 0;
}

PMATH_API pmath_string_t pmath_file_readline(pmath_t file){
  if(pmath_file_test(file, PMATH_FILE_PROP_READ | PMATH_FILE_PROP_TEXT)){
    pmath_custom_t custom = pmath_symbol_get_value(file);
    
    if(pmath_custom_has_destructor(custom, destroy_text_file)){
      struct _pmath_text_file_t *data = pmath_custom_get_data(custom);
      
      if(data->readln_function){
        if(lock_file(&data->inherited)){
          pmath_string_t result;
          
          if(!pmath_is_null(data->buffer)){
            result = data->buffer;
            data->buffer = PMATH_NULL;
          }
          else
            result = data->readln_function(data->inherited.extra);
            
          unlock_file(&data->inherited);
          
          pmath_unref(custom);
          return result;
        }
      }
    }
    
    pmath_unref(custom);
  }
  
  return PMATH_NULL;
}

PMATH_API
pmath_bool_t pmath_file_set_binbuffer(
  pmath_t  file,
  size_t   size
){
  if(pmath_file_test(file, PMATH_FILE_PROP_READ | PMATH_FILE_PROP_BINARY)){
    pmath_custom_t custom = pmath_symbol_get_value(file);
    
    if(pmath_custom_has_destructor(custom, destroy_binary_file)){
      struct _pmath_binary_file_t *data = pmath_custom_get_data(custom);
      
      if(lock_file(&data->inherited)){
        uint8_t *newbuf = pmath_mem_realloc_no_failfree(data->buffer, size);
        
        if(newbuf || size == 0){
          data->buffer_size = size;
          
          data->buffer = newbuf;
          data->current_buffer_start = data->buffer;
          data->current_buffer_end   = data->buffer;
        }
        
        unlock_file(&data->inherited);
        
        pmath_unref(custom);
        return newbuf != NULL;
      }
    }
    
    pmath_unref(custom);
  }
  
  return FALSE;
}

PMATH_API
void pmath_file_set_textbuffer(pmath_t file, pmath_string_t buffer){
  if(pmath_file_test(file, PMATH_FILE_PROP_READ | PMATH_FILE_PROP_TEXT)){
    pmath_custom_t custom = pmath_symbol_get_value(file);
    
    if(pmath_custom_has_destructor(custom, destroy_text_file)){
      struct _pmath_text_file_t *data = pmath_custom_get_data(custom);
      
      if(data->readln_function){
        if(lock_file(&data->inherited)){
          pmath_string_t tmp = data->buffer;
          data->buffer = buffer;
          buffer = tmp;
          
          unlock_file(&data->inherited);
        }
      }
    }
    
    pmath_unref(custom);
  }
  
  pmath_unref(buffer);
}

PMATH_API size_t pmath_file_write(
  pmath_t          file,
  const void      *buffer,
  size_t           buffer_size
){
  if(pmath_is_symbol(file)){
    pmath_custom_t custom = pmath_symbol_get_value(file);
    
    if(pmath_is_custom(custom)
    && pmath_custom_has_destructor(custom, destroy_binary_file)){
      struct _pmath_binary_file_t *data = pmath_custom_get_data(custom);
    
      if(data->write_function){
        if(lock_file(&data->inherited)){
          size_t result = data->write_function(
            data->inherited.extra,
            buffer, 
            buffer_size);
          
          unlock_file(&data->inherited);
          
          pmath_unref(custom);
          return result;
        }
      }
    }
    
    pmath_unref(custom);
  }
  else if(pmath_is_expr_of(file, PMATH_SYMBOL_LIST)){
    size_t i;
    size_t result = buffer_size;
    
    for(i = 1;i <= pmath_expr_length(file);++i){
      pmath_t item = pmath_expr_get_item(file, i);
      
      size_t item_result = pmath_file_write(
        item,
        buffer,
        buffer_size);
      
      if(result > item_result)
         result = item_result;
      
      pmath_unref(item);
    }
    
    return result;
  }
  
  return 0;
}

PMATH_API pmath_bool_t pmath_file_writetext(
  pmath_t          file,
  const uint16_t  *str,
  int              len
){
  if(pmath_is_symbol(file)){
    pmath_custom_t custom = pmath_symbol_get_value(file);
    
    if(pmath_is_custom(custom)
    && pmath_custom_has_destructor(custom, destroy_text_file)){
      struct _pmath_text_file_t *data = pmath_custom_get_data(custom);
    
      if(data->write_function){
        if(lock_file(&data->inherited)){
          pmath_bool_t result;
          
          result = data->write_function(data->inherited.extra, str, len);
          
          unlock_file(&data->inherited);
          
          pmath_unref(custom);
          return result;
        }
      }
    }
    
    pmath_unref(custom);
  }
  else if(pmath_is_expr_of(file, PMATH_SYMBOL_LIST)){
    size_t i;
    pmath_bool_t result = TRUE;
    
    for(i = 1;i <= pmath_expr_length(file);++i){
      pmath_t item = pmath_expr_get_item(file, i);
      
      result = pmath_file_writetext(item, str, len) || result;
      
      pmath_unref(item);
    }
    
    return result;
  }
  
  return FALSE;
}

PMATH_API void pmath_file_flush(pmath_t file){
  if(pmath_is_symbol(file)){
    pmath_custom_t custom = pmath_symbol_get_value(file);
    
    if(pmath_is_custom(custom)
    && (pmath_custom_has_destructor(custom, destroy_binary_file)
     || pmath_custom_has_destructor(custom, destroy_text_file))){
      struct _file_t *f = pmath_custom_get_data(custom);
      
      if(f->flush_function){
        if(lock_file(f)){
          f->flush_function(f->extra);
          
          unlock_file(f);
        }
      }
    }
    
    pmath_unref(custom);
  }
}

typedef struct{
  pmath_t  file;
  pmath_bool_t success;
}_write_data_t;

static void write_data(_write_data_t *user, const uint16_t *data, int len){
  user->success = pmath_file_writetext(user->file, data, len) || user->success;
}

PMATH_API pmath_bool_t pmath_file_write_object(
  pmath_t                 file,
  pmath_t                 obj,
  pmath_write_options_t   options
){
  _write_data_t data;
  
  data.file = file;
  data.success = TRUE;
  
  pmath_write(obj, options, (pmath_write_func_t)write_data, &data);
  
  return data.success;
}

PMATH_API
void pmath_file_manipulate(
  pmath_t   file, 
  void    (*type)(void*),
  void    (*callback)(void*, void*),
  void     *data
){
  if(pmath_is_symbol(file)){
    pmath_custom_t custom = pmath_symbol_get_value(file);
    
    if(pmath_is_custom(custom)
    && (pmath_custom_has_destructor(custom, destroy_binary_file)
     || pmath_custom_has_destructor(custom, destroy_text_file))){
      struct _file_t *f = (struct _file_t*)pmath_custom_get_data(custom);
      
      if(f->extra_destructor == type){
        if(lock_file(f)){
          callback(f->extra, data);
          
          unlock_file(f);
        }
      }
    }
    
    pmath_unref(custom);
  }
}

PMATH_API
pmath_bool_t pmath_file_close(pmath_t file){
  if(pmath_is_symbol(file)){
    pmath_custom_t custom = pmath_symbol_get_value(file);
    
    if(pmath_is_custom(custom)
    && (pmath_custom_has_destructor(custom, destroy_binary_file)
     || pmath_custom_has_destructor(custom, destroy_text_file))){
      pmath_bool_t result;
      
      pmath_symbol_set_attributes(file, PMATH_SYMBOL_ATTRIBUTE_TEMPORARY);
      result = _pmath_clear(file, TRUE);
      pmath_unref(custom);
      pmath_unref(file);
      return result;
    }
    
    pmath_unref(custom);
  }
  else if(pmath_is_expr_of(file, PMATH_SYMBOL_LIST)){
    pmath_bool_t result = TRUE;
    size_t i;
    
    for(i = 1;i <= pmath_expr_length(file);++i){
      pmath_t item = pmath_expr_get_item(file, i);
      
      result = pmath_file_close(item) || result;
    }
    
    pmath_unref(file);
    return result;
  }
  
  pmath_unref(file);
  return FALSE;
}

//==============================================================================

PMATH_API 
pmath_symbol_t pmath_file_create_binary(
  void                     *extra,
  void                    (*extra_destructor)(void*),
  pmath_binary_file_api_t  *api
){
  struct _pmath_binary_file_t *data;
  pmath_custom_t custom;
  pmath_symbol_t file;
  
  if(api->struct_size < sizeof(size_t) + 3 * sizeof(void*)
  || !extra_destructor 
  || (!api->status_function && api->read_function))
    return PMATH_NULL;
  
  data = (struct _pmath_binary_file_t*)pmath_mem_alloc(sizeof(struct _pmath_binary_file_t));
  if(!data){
    extra_destructor(extra);
      
    return PMATH_NULL;
  }
  
  memset(data, 0, sizeof(struct _pmath_binary_file_t));
  
  data->inherited.extra            = extra;
  data->inherited.extra_destructor = extra_destructor;
  data->inherited.status_function  = api->status_function;
  data->read_function              = api->read_function;
  data->write_function             = api->write_function;
  
  if(api->struct_size >= (size_t)&api->flush_function - (size_t)api + sizeof(void*))
    data->inherited.flush_function = api->flush_function;
  
  if(data->read_function){
    data->buffer_size = 256;
    data->buffer = (uint8_t*)pmath_mem_alloc(data->buffer_size);
    if(!data->buffer){
      destroy_binary_file(data);
        
      return PMATH_NULL;
    }
    
    data->current_buffer_start = data->current_buffer_end = data->buffer;
  }
  
  custom = pmath_custom_new(data, destroy_binary_file);
  if(pmath_is_null(custom))
    return PMATH_NULL;
  
  file = pmath_symbol_create_temporary(
    PMATH_C_STRING("System`Private`io`object"), 
    TRUE);
  
  PMATH_RUN_ARGS(
      "Options(`1`):= {"
        "BinaryFormat->True}",
    "(o)",
    pmath_ref(file));
  
  pmath_register_code(file, file_set_options, PMATH_CODE_USAGE_UPCALL);
  
  pmath_symbol_set_value(file, custom);
  
  pmath_symbol_set_attributes(file, 
    PMATH_SYMBOL_ATTRIBUTE_TEMPORARY | 
    PMATH_SYMBOL_ATTRIBUTE_PROTECTED);
  
  return file;
}

PMATH_API 
PMATH_ATTRIBUTE_USE_RESULT
PMATH_ATTRIBUTE_NONNULL(2)
pmath_symbol_t pmath_file_create_text(
  void                   *extra,
  void                  (*extra_destructor)(void*),
  pmath_text_file_api_t  *api
){
  struct _pmath_text_file_t *data;
  pmath_custom_t custom;
  pmath_symbol_t file;
  
  if(api->struct_size < sizeof(size_t) + 3 * sizeof(void*)
  || !extra_destructor 
  || (!api->status_function && api->readln_function))
    return PMATH_NULL;
  
  data = (struct _pmath_text_file_t*)pmath_mem_alloc(sizeof(struct _pmath_text_file_t));
  if(!data){
    extra_destructor(extra);
      
    return PMATH_NULL;
  }
  
  memset(data, 0, sizeof(struct _pmath_text_file_t));
  
  data->inherited.extra            = extra;
  data->inherited.extra_destructor = extra_destructor;
  data->inherited.status_function  = api->status_function;
  data->readln_function            = api->readln_function;
  data->write_function             = api->write_function;
  
  if(api->struct_size >= (size_t)&api->flush_function - (size_t)api + sizeof(void*))
    data->inherited.flush_function = api->flush_function;
  
  custom = pmath_custom_new(data, destroy_text_file);
  if(pmath_is_null(custom))
    return PMATH_NULL;
  
  file = pmath_symbol_create_temporary(
    PMATH_C_STRING("System`Private`io`object"), 
    TRUE);
  
  PMATH_RUN_ARGS(
      "Options(`1`):= {"
        "BinaryFormat->False}",
    "(o)",
    pmath_ref(file));
  
  pmath_register_code(file, file_set_options, PMATH_CODE_USAGE_UPCALL);
  
  pmath_symbol_set_value(file, custom);
  
  pmath_symbol_set_attributes(file, 
    PMATH_SYMBOL_ATTRIBUTE_TEMPORARY | 
    PMATH_SYMBOL_ATTRIBUTE_PROTECTED);
  
  return file;
}

//==============================================================================

struct _bintext_extra_t{
  pmath_t binfile;
  
  pmath_string_t rest; // input buffer
  pmath_bool_t skip_nl;
  
  iconv_t  in_cd;
  iconv_t  out_cd;
};

void destroy_bintext_extra(struct _bintext_extra_t *extra){
  pmath_unref(extra->binfile);
  pmath_unref(extra->rest);
  
  if(extra->in_cd != (iconv_t)-1)
    iconv_close(extra->in_cd);
  
  if(extra->out_cd != (iconv_t)-1)
    iconv_close(extra->out_cd);
  
  pmath_mem_free(extra);
}

pmath_files_status_t bintext_extra_status(struct _bintext_extra_t *extra){
  pmath_files_status_t result = pmath_file_status(extra->binfile);
  
  if(result == PMATH_FILE_ENDOFFILE
  && pmath_string_length(extra->rest) > 0)
    result = PMATH_FILE_OK;
  
  return result;
}

pmath_string_t bintext_extra_readln(struct _bintext_extra_t *extra){
  pmath_string_t result = PMATH_NULL;
  
  uint16_t out[100];
  char in[100];
  
  int len, i;
  
  len = pmath_string_length(extra->rest);
  
  if(len){
    const uint16_t *buf;
    
    buf = pmath_string_buffer(extra->rest);
    
    i = 0;
    while(i < len){
      if(buf[i] == '\n'){
        result = pmath_string_part(pmath_ref(extra->rest), 0, i);
        
        if(i == len - 1){
          pmath_unref(extra->rest);
          extra->rest = PMATH_NULL;
        }
        else
          extra->rest = pmath_string_part(extra->rest, i + 1, -1);
        
        return result;
      }
      
      if(buf[i] == '\r'){
        result = pmath_string_part(pmath_ref(extra->rest), 0, i);
        
        if(i + 1 < len){
          if(buf[i + 1] == '\n')
            ++i;
            
          if(i == len - 1){
            pmath_unref(extra->rest);
            extra->rest = PMATH_NULL;
          }
          else
            extra->rest = pmath_string_part(extra->rest, i + 1, -1);
        }
        else{
          pmath_unref(extra->rest);
          extra->rest = PMATH_NULL;
          extra->skip_nl = TRUE;
        }
        
        return result;
      }
      
      ++i;
    }
    
    result = extra->rest;
    extra->rest = PMATH_NULL;
  }
  
  for(;;){
    char *inbuf;
    char *outbuf;
    
    size_t inbytesleft;
    size_t outbytesleft;
    
    int chars_read;
    
    inbytesleft = pmath_file_read(extra->binfile, in, sizeof(in), FALSE);
    
    if(inbytesleft == 0)
      break;
    
    outbytesleft = sizeof(out);
    inbuf = in;
    outbuf = (char*)out;
    
    len = pmath_string_length(result);
    
    do{
      size_t ret = iconv(extra->in_cd, &inbuf, &inbytesleft, &outbuf, &outbytesleft);
      
      if(ret == (size_t)-1){
        if(errno == EILSEQ){ // invalid input byte
          if(outbytesleft > 1){
            *((uint16_t*)outbuf) = *(uint8_t*)inbuf;
            
            outbuf+= 2;
            outbytesleft-= 2;
          }
          
          ++inbuf;
          --inbytesleft;
        }
        else if(errno == EINVAL){ // incomplete input
          size_t more = 0;
          
          memmove(in, inbuf, inbytesleft);
          
          more = pmath_file_read(
            extra->binfile, 
            in + inbytesleft, 
            1/*sizeof(inbuf) - inbytesleft*/, 
            FALSE);
          
          inbuf = in;
          
          if(more == 0){
            in[inbytesleft] = '\0';
            more = 1;
//            while(inbytesleft > 0){
//              *((uint16_t*)outbuf) = *(uint8_t*)inbuf;
//              
//              outbuf+= 2;
//              outbytesleft-= 2;
//              
//              ++inbuf;
//              ++inbytesleft;
//            }
          }
          
          inbytesleft+= more;
        }
        else if(errno == E2BIG){ // output buffer too small
          chars_read = (int)(((size_t)outbuf - (size_t)out)/2);
          
          if(chars_read > 0){
            const uint16_t *str = out;
            
            if(extra->skip_nl){
              extra->skip_nl = FALSE;
              
              if(str[0] == '\n'){
                ++str;
                --chars_read;
              }
            }
            
            result = pmath_string_insert_ucs2(result, INT_MAX, str, chars_read);
          }
          
          outbuf = (char*)out;
          outbytesleft = sizeof(out);
        }
      }
    }while(inbytesleft > 0);
    
    chars_read = (int)(((size_t)outbuf - (size_t)out)/2);
    
    if(chars_read > 0){
      const uint16_t *str = out;
      
      if(extra->skip_nl){
        extra->skip_nl = FALSE;
        
        if(str[0] == '\n'){
          ++str;
          --chars_read;
        }
      }
      
      result = pmath_string_insert_ucs2(result, INT_MAX, str, chars_read);
      
      str = pmath_string_buffer(result);
      chars_read = pmath_string_length(result);
      
      for(i = len;i < chars_read;++i){
        if(str[i] == '\n'){
          chars_read-= i + 1;
          if(chars_read)
            extra->rest = pmath_string_part(pmath_ref(result), i + 1, chars_read);
          
          return pmath_string_part(result, 0, i);
        }
        
        if(str[i] == '\r'){
          chars_read-= i + 1;
          if(chars_read > 0){
            if(str[i + 1] == '\n'){
              if(chars_read > 1)
                extra->rest = pmath_string_part(pmath_ref(result), i + 2, chars_read - 1);
            }
            else
              extra->rest = pmath_string_part(pmath_ref(result), i + 1, chars_read);
          }
          else
            extra->skip_nl = TRUE;
          
          return pmath_string_part(result, 0, i);
        }
      }
    }
  }
  
  return result;
}

pmath_bool_t bintext_extra_write(
  struct _bintext_extra_t *extra, 
  const uint16_t          *str, 
  int                      len
){
  pmath_bool_t result = TRUE;
  char buf[100];
  
  if(len < 0){
    len = 0;
    while(str[len])
      ++len;
  }
  
  while(len > 0){
    int restlen = len;
    
    len = 0;
    while(len < restlen && str[len] != '\r' && str[len] != '\n')
      ++len;
    
    restlen-= len;
    
    {
      char *inbuf = (char*)str;
      char *outbuf = buf;
      
      size_t inbytesleft = sizeof(uint16_t) * (size_t)len;
      size_t outbytesleft = sizeof(buf);
      
      while(inbytesleft > 0){
        size_t ret = iconv(extra->out_cd, &inbuf, &inbytesleft, &outbuf, &outbytesleft);
        
        if(ret == (size_t)-1){
          if(errno == EILSEQ){ // invalid input byte
            if(outbytesleft > 1){
              *outbuf = '?';
              
              ++outbuf;
              --outbytesleft;
            }
            
            if(inbytesleft == 1){
              ++inbuf;
              --inbytesleft;
            }
            else{
              inbuf+= 2;
              inbytesleft-= 2;
            }
          }
          else if(errno == EINVAL){ // incomplete input
            if(outbytesleft > 1){
              *outbuf = '?';
              
              ++outbuf;
              --outbytesleft;
            }
            
            inbytesleft = 0;
          }
          else if(errno == E2BIG){ // output buffer too small
            result = pmath_file_write(
              extra->binfile,
              buf,
              sizeof(buf)) || result;
            
            outbuf = buf;
            outbytesleft = sizeof(buf);
          }
        }
      }
      
      if(outbytesleft < sizeof(buf)){
        result = pmath_file_write(
          extra->binfile,
          buf,
          sizeof(buf) - outbytesleft) || result;
      }
    }
    
    str+= len;
    len = restlen;
    if(len > 0){
      if(*str == '\r'){
        ++str;
        --len;
      }
      
      if(len > 0 && *str == '\n'){
        ++str;
        --len;
      }
      
      #ifdef PMATH_OS_WIN32
        result = pmath_file_write(extra->binfile, "\r\n", 2) || result;
      #else
        result = pmath_file_write(extra->binfile, "\n", 1) || result;
      #endif
    }
  }
  
  return result;
}

void bintext_extra_flush(struct _bintext_extra_t *extra){
  pmath_file_flush(extra->binfile);
}

//------------------------------------------------------------------------------

PMATH_API 
PMATH_ATTRIBUTE_USE_RESULT
PMATH_ATTRIBUTE_NONNULL(2)
pmath_symbol_t pmath_file_create_text_from_binary(
  pmath_t      binfile,
  const char  *encoding 
){
  struct _bintext_extra_t *extra;
  pmath_text_file_api_t api;
  
  extra = (struct _bintext_extra_t*)pmath_mem_alloc(sizeof(struct _bintext_extra_t));
  
  if(!extra){
    pmath_unref(binfile);
    return PMATH_NULL;
  }
  
  memset(extra, 0, sizeof(struct _bintext_extra_t));
  
  extra->binfile = binfile;
  extra->in_cd = (iconv_t)-1;
  extra->out_cd = (iconv_t)-1;
  
  memset(&api, 0, sizeof(api));
  api.struct_size = sizeof(api);
  
  if(pmath_file_test(binfile, PMATH_FILE_PROP_BINARY | PMATH_FILE_PROP_READ)){
    extra->in_cd = iconv_open(PMATH_BYTE_ORDER < 0 ? "UTF-16LE" : "UTF-16BE", encoding);
    
    if(extra->in_cd == (iconv_t)-1){
      pmath_unref(binfile);
      pmath_mem_free(extra);
      return PMATH_NULL;
    }
    
    api.status_function = (pmath_files_status_t(*)(void*))bintext_extra_status;
    api.readln_function = (pmath_string_t(*)(void*))bintext_extra_readln;
  }
  
  if(pmath_file_test(binfile, PMATH_FILE_PROP_BINARY | PMATH_FILE_PROP_WRITE)){
    extra->out_cd = iconv_open(encoding, PMATH_BYTE_ORDER < 0 ? "UTF-16LE" : "UTF-16BE");
    
    if(extra->out_cd == (iconv_t)-1){
      pmath_unref(binfile);
      
      if(extra->in_cd != (iconv_t)-1)
        iconv_close(extra->in_cd); 
      
      pmath_mem_free(extra);
      return PMATH_NULL;
    }
    else{
    /* For some strange reason, we have to call iconv once, otherwise writing
       the byte order mark (see below) will fail (if encoding is utf8/...).
     */
      static const uint16_t inbuf = 'a';
      char outbuf[10];
      char *in  = (char*)&inbuf;
      char *out = outbuf;
      size_t inleft  = sizeof(inbuf);
      size_t outleft = sizeof(outbuf);
      
      iconv(extra->out_cd, &in, &inleft, &out, &outleft);
    }
    
    api.write_function = (pmath_bool_t(*)(void*,const uint16_t*,int))bintext_extra_write;
    api.flush_function = (void(*)(void*))bintext_extra_flush;
  }
  
  binfile = pmath_file_create_text(
    extra, 
    (void(*)(void*))destroy_bintext_extra,
    &api);
  
  if(!pmath_is_null(binfile)){
    if(api.readln_function){ // skip byte order mark
      pmath_string_t line = pmath_file_readline(binfile);
      
      if(pmath_string_length(line) > 0
      && *pmath_string_buffer(line) == 0xFEFF)
        line = pmath_string_part(line, 1, -1);
      
      pmath_file_set_textbuffer(binfile, line);
    }
  }
  
  return binfile;
}

//==============================================================================

struct binbuf_t{
  uint8_t *data;
  
  uint8_t *read_ptr;
  uint8_t *write_ptr;
  
  size_t capacity;
  pmath_bool_t error;
};

static void destroy_binbuf(void *p){
  struct binbuf_t *bb = (struct binbuf_t*)p;
  
  pmath_mem_free(bb->data);
  pmath_mem_free(bb);
}

static struct binbuf_t *create_binbuf(size_t capacity){
  struct binbuf_t *bb = pmath_mem_alloc(sizeof(struct binbuf_t));
  if(!bb)
    return NULL;
  
  bb->error    = FALSE;
  bb->capacity = capacity;
  bb->data     = pmath_mem_alloc(capacity);
  if(!bb->data){
    pmath_mem_free(bb);
    return NULL;
  }
  
  bb->read_ptr = bb->write_ptr = bb->data;
  return bb;
}

static pmath_files_status_t binbuf_status(void *p){
  struct binbuf_t *bb = (struct binbuf_t*)p;
  
  if(bb->error)
    return PMATH_FILE_OTHERERROR;
  
  assert((size_t)bb->data     <= (size_t)bb->read_ptr);
  assert((size_t)bb->read_ptr <= (size_t)bb->write_ptr);
  
  if(bb->read_ptr == bb->write_ptr)
    return PMATH_FILE_ENDOFFILE;
  
  return PMATH_FILE_OK;
}

static size_t binbuf_read(void *p, void *buffer, size_t buffer_size){
  struct binbuf_t *bb = (struct binbuf_t*)p;
  
  if(bb->error)
    return 0;
    
  assert((size_t)bb->data     <= (size_t)bb->read_ptr);
  assert((size_t)bb->read_ptr <= (size_t)bb->write_ptr);
  
  if(buffer_size > (size_t)bb->write_ptr - (size_t)bb->read_ptr)
     buffer_size = (size_t)bb->write_ptr - (size_t)bb->read_ptr;
  
  memcpy(buffer, bb->read_ptr, buffer_size);
  bb->read_ptr+= buffer_size;
  
  return buffer_size;
}

static size_t binbuf_write(void *p, const void *buffer, size_t buffer_size){
  struct binbuf_t *bb = (struct binbuf_t*)p;
  size_t have_written;
  
  if(bb->error)
    return 0;
    
  assert((size_t)bb->data     <= (size_t)bb->read_ptr);
  assert((size_t)bb->read_ptr <= (size_t)bb->write_ptr);
  
  have_written = (size_t)bb->write_ptr - (size_t)bb->data;
  if(buffer_size + have_written > bb->capacity){
    size_t have_read = (size_t)bb->read_ptr - (size_t)bb->data;
    
    if(buffer_size + have_written <= bb->capacity + have_read){
      memmove(bb->data, bb->read_ptr, have_written - have_read);
    }
    else{
      size_t new_cap = bb->capacity ? bb->capacity : 0x100;
      
      while(new_cap >= bb->capacity 
      && new_cap < 0x1000
      && new_cap < have_written + buffer_size){
        new_cap*= 2;
      }
      
      if(new_cap < have_written + buffer_size){
        new_cap = ((have_written + buffer_size - 1) / 0x1000 + 1) * 0x1000;
      }
      
      if(new_cap < bb->capacity){
        bb->error = TRUE;
        pmath_mem_free(bb->data);
        bb->data = bb->read_ptr = bb->write_ptr = NULL;
        bb->capacity = 0;
        return 0;
      }
      
      bb->data = pmath_mem_realloc(bb->data, new_cap);
      if(!bb->data){
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
  return buffer_size;
}

static void binbuf_get_readablebytes(void *p, void *result){
  struct binbuf_t *bb = (struct binbuf_t*)p;
  
  if(bb->error){
    *(size_t*)result = 0;
  }
  else{
    assert((size_t)bb->data     <= (size_t)bb->read_ptr);
    assert((size_t)bb->read_ptr <= (size_t)bb->write_ptr);
    
    *(size_t*)result = (size_t)bb->write_ptr - (size_t)bb->read_ptr;
  }
}

PMATH_API
pmath_symbol_t pmath_file_create_binary_buffer(size_t mincapacity){
  pmath_binary_file_api_t api;
  struct binbuf_t *bb;
  
  bb = create_binbuf(mincapacity);
  if(!bb)
    return PMATH_NULL;
  
  memset(&api, 0, sizeof(api));
  api.struct_size = sizeof(api);
  api.status_function = binbuf_status;
  api.read_function   = binbuf_read;
  api.write_function  = binbuf_write;
  
  return pmath_file_create_binary(bb, destroy_binbuf, &api);
}

PMATH_API
size_t pmath_file_binary_buffer_size(
  pmath_t binfile
){
  size_t result = 0;
  pmath_file_manipulate(
    binfile,
    destroy_binbuf,
    binbuf_get_readablebytes,
    &result);
  
  return result;
}

  struct binbuf_manipulate_t{
    void (*callback)(uint8_t*,uint8_t*,const uint8_t*,void*);
    void *closure;
  };
  
  static void binbuf_manipulate(void *p, void *extra){
    struct binbuf_t *bb = (struct binbuf_t*)p;
    struct binbuf_manipulate_t *info = (struct binbuf_manipulate_t*)extra;
    
    if(!bb->error){
      assert((size_t)bb->data     <= (size_t)bb->read_ptr);
      assert((size_t)bb->read_ptr <= (size_t)bb->write_ptr);
      
      info->callback(
        bb->read_ptr, 
        bb->write_ptr, 
        bb->data + bb->capacity,
        info->closure);
    }
  }

PMATH_API
void pmath_file_binary_buffer_manipulate(
  pmath_t   binfile,
  void    (*callback)(uint8_t *readable, uint8_t *writable, const uint8_t *end, void *closure),
  void     *closure
){
  if(callback){
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
