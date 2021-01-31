#include <pmath-util/files/abstract-file.h>

#include <pmath-core/custom.h>
#include <pmath-core/symbols-private.h>

#include <pmath-language/scanner.h>

#include <pmath-util/concurrency/threads.h>
#include <pmath-util/debug.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/line-writer.h>
#include <pmath-util/memory.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/control/definitions-private.h>

#include <limits.h>
#include <string.h>


struct _file_t {
  pmath_atomic_t _lock;
  
  void  *extra;
  void (*extra_destructor)(void *);
  
  pmath_files_status_t (*status_function)( void *extra);
  void                 (*flush_function)(  void *extra);
};

struct _pmath_binary_file_t {
  struct _file_t  inherited;
  
  size_t       (*read_function)(   void *extra,       void *buffer, size_t buffer_size);
  size_t       (*write_function)(  void *extra, const void *buffer, size_t buffer_size);
  int64_t      (*get_pos_function)(void *extra);
  pmath_bool_t (*set_pos_function)(void *extra, int64_t offset, int origin);
  
  
  size_t buffer_size;
  uint8_t *buffer;
  
  uint8_t *current_buffer_start;
  uint8_t *current_buffer_end;
};

struct _pmath_text_file_t {
  struct _file_t  inherited;
  
  pmath_string_t (*readln_function)(void *extra);
  pmath_bool_t (*write_function)(void *extra, const uint16_t *str, int len);
  
  pmath_string_t buffer;
};

extern pmath_symbol_t pmath_System_BinaryFormat;
extern pmath_symbol_t pmath_System_InputStream;
extern pmath_symbol_t pmath_System_List;
extern pmath_symbol_t pmath_System_OutputStream;
extern pmath_symbol_t pmath_System_SetOptions;

static pmath_bool_t lock_file(struct _file_t *f) {
  intptr_t me = (intptr_t)pmath_thread_get_current();
  
  if(pmath_atomic_read_aquire(&f->_lock) == me)
    return FALSE;
    
  pmath_atomic_loop_yield();
  
  while(!pmath_atomic_compare_and_set(&f->_lock, 0, me)) {
    pmath_atomic_loop_nop();
  }
  
  return TRUE;
}

static void unlock_file(struct _file_t *f) {
  pmath_atomic_unlock(&f->_lock);
}

static void destroy_binary_file(void *ptr) {
  struct _pmath_binary_file_t *f = ptr;
  
  f->inherited.extra_destructor(f->inherited.extra);
  
  pmath_mem_free(f->buffer);
  pmath_mem_free(f);
}

static void destroy_text_file(void *ptr) {
  struct _pmath_text_file_t *f = (struct _pmath_text_file_t *)ptr;
  
  f->inherited.extra_destructor(f->inherited.extra);
  
  pmath_unref(f->buffer);
  pmath_mem_free(f);
}

static pmath_t file_set_options(pmath_expr_t expr) {
  size_t i;
  pmath_t file;
  
  if(!pmath_is_expr_of(expr, pmath_System_SetOptions))
    return expr;
    
  file = pmath_expr_get_item(expr, 1);
  if(!pmath_file_test(file, 0)) {
    pmath_unref(file);
    return expr;
  }
  
  for(i = 2; i <= pmath_expr_length(expr); ++i) {
    pmath_t rule = pmath_expr_get_item(expr, i);
    
    if(pmath_is_rule(rule)) {
      pmath_t lhs = pmath_expr_get_item(rule, 1);
      
      if(pmath_same(lhs, pmath_System_BinaryFormat)) {
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

static pmath_custom_t get_single_file_object(pmath_t file) {
  if(pmath_is_symbol(file)) {
    pmath_custom_t result = pmath_symbol_get_value(file);
    if(pmath_is_custom(result))
      return result;
    
    pmath_unref(result);
    return PMATH_NULL;
  }
  
  if(pmath_is_expr(file)) {
    pmath_t head = pmath_expr_get_item(file, 0);
    pmath_unref(head);
    
    if(pmath_same(head, pmath_System_List)) {
      if(pmath_expr_length(file) == 1) {
        pmath_t item = pmath_expr_get_item(file, 1);
        pmath_custom_t result = get_single_file_object(item);
        pmath_unref(item);
        return result;
      }
      
      return PMATH_NULL;
    }
    
    if(pmath_same(head, pmath_System_InputStream) || pmath_same(head, pmath_System_OutputStream)) {
      pmath_t item = pmath_expr_get_item(file, 1);
      pmath_custom_t result = get_single_file_object(item);
      pmath_unref(item);
      return result;
    }
  }
  
  return PMATH_NULL;
}

static pmath_bool_t foreach_file_object(pmath_t file, pmath_bool_t(*callback)(pmath_custom_t, void*), void *arg) {
  if(pmath_is_symbol(file)) {
    pmath_bool_t success = FALSE;
    pmath_custom_t custom = pmath_symbol_get_value(file);
    if(pmath_is_custom(custom)) {
      success = callback(custom, arg);
    }
    pmath_unref(custom);
    return success;
  }
  
  if(pmath_is_expr(file)) {
    pmath_t head = pmath_expr_get_item(file, 0);
    pmath_unref(head);
    
    if(pmath_same(head, pmath_System_List)) {
      pmath_bool_t success = TRUE;
      size_t i;
      for(i = pmath_expr_length(file); i > 0; --i) {
        pmath_t item = pmath_expr_get_item(file, i);
        success = foreach_file_object(item, callback, arg);
        pmath_unref(item);
        
        if(success) {
          if(!pmath_aborting()) continue;
          success = FALSE;
        }
        break;
      }
      return success;
    }
    
    if(pmath_same(head, pmath_System_InputStream) || pmath_same(head, pmath_System_OutputStream)) {
      pmath_t item = pmath_expr_get_item(file, 1);
      pmath_bool_t success = foreach_file_object(item, callback, arg);
      pmath_unref(item);
      return success;
    }
  }
  
  return FALSE;
} 

PMATH_API pmath_bool_t pmath_file_test(
  pmath_t file,
  int     properties
) {
  if(pmath_is_expr(file)) {
    pmath_t head = pmath_expr_get_item(file, 0);
    pmath_unref(head);
    
    if(pmath_same(head, pmath_System_List)) {
      size_t i;
      
      if(properties & PMATH_FILE_PROP_READ)
        return FALSE;
      
      for(i = pmath_expr_length(file); i > 0; --i) {
        pmath_t item = pmath_expr_get_item(file, i);
        
        if(!pmath_file_test(item, properties)) {
          pmath_unref(item);
          return FALSE;
        }
        
        pmath_unref(item);
      }
      
      return TRUE;
    }
    
    if(pmath_same(head, pmath_System_InputStream)) {
      pmath_t item;
      pmath_bool_t result;
      if(properties & PMATH_FILE_PROP_WRITE)
        return FALSE;
      
      if(pmath_expr_length(file) != 1) // TODO allow some options?
        return FALSE;
      
      item = pmath_expr_get_item(file, 1);
      result = pmath_file_test(item, properties);
      pmath_unref(item);
      return result;
    }
    
    if(pmath_same(head, pmath_System_OutputStream)) {
      pmath_t item;
      pmath_bool_t result;
      if(properties & PMATH_FILE_PROP_READ)
        return FALSE;
      
      if(pmath_expr_length(file) != 1) // TODO allow some options?
        return FALSE;
      
      item = pmath_expr_get_item(file, 1);
      result = pmath_file_test(item, properties);
      pmath_unref(item);
      return result;
    }
  }
  else if(pmath_is_symbol(file)) {
    pmath_custom_t custom = pmath_symbol_get_value(file);
    
    if(pmath_is_custom(custom)) {
      if(pmath_custom_has_destructor(custom, destroy_binary_file)) {
        if((properties & PMATH_FILE_PROP_TEXT) == 0) {
          pmath_bool_t result = TRUE;
          
          struct _pmath_binary_file_t *data = (struct _pmath_binary_file_t *)
                                              pmath_custom_get_data(custom);
                                              
          if(properties & PMATH_FILE_PROP_READ)
            result = result && data->read_function != NULL;
            
          if(properties & PMATH_FILE_PROP_WRITE)
            result = result && data->write_function != NULL;
            
          pmath_unref(custom);
          return result;
        }
      }
      else if(pmath_custom_has_destructor(custom, destroy_text_file)) {
        if((properties & PMATH_FILE_PROP_BINARY) == 0) {
          pmath_bool_t result = TRUE;
          
          struct _pmath_text_file_t *data = (struct _pmath_text_file_t *)
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
  
  return FALSE;
}

PMATH_API pmath_files_status_t pmath_file_status(pmath_t file) {
  if(pmath_file_test(file, PMATH_FILE_PROP_READ)) {
    pmath_custom_t custom = get_single_file_object(file);
    
    if(pmath_custom_has_destructor(custom, destroy_binary_file)) {
      struct _pmath_binary_file_t *data = pmath_custom_get_data(custom);
      
      if(data->inherited.status_function) {
        if(lock_file(&data->inherited)) {
          pmath_files_status_t result;
          
          result = data->inherited.status_function(data->inherited.extra);
          
          if( result == PMATH_FILE_ENDOFFILE &&
              data->current_buffer_start < data->current_buffer_end)
          {
            result = PMATH_FILE_OK;
          }
          
          unlock_file(&data->inherited);
          
          pmath_unref(custom);
          return result;
        }
        
        pmath_unref(custom);
        return PMATH_FILE_RECURSIVE;
      }
    }
    else if(pmath_custom_has_destructor(custom, destroy_text_file)) {
      struct _pmath_text_file_t *data = pmath_custom_get_data(custom);
      
      if(data->inherited.status_function) {
        if(lock_file(&data->inherited)) {
          pmath_files_status_t result;
          
          result = data->inherited.status_function(data->inherited.extra);
          
          if( result == PMATH_FILE_ENDOFFILE &&
              pmath_string_length(data->buffer) > 0)
          {
            result = PMATH_FILE_OK;
          }
          
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
) {
  if(pmath_file_test(file, PMATH_FILE_PROP_READ | PMATH_FILE_PROP_BINARY)) {
    pmath_custom_t custom = get_single_file_object(file);
    
    if(pmath_custom_has_destructor(custom, destroy_binary_file)) {
      struct _pmath_binary_file_t *data = pmath_custom_get_data(custom);
      
      if(data->read_function) {
        if(lock_file(&data->inherited)) {
          size_t bufmax = 0;
          
          if(data->current_buffer_start == data->current_buffer_end) {
            bufmax = data->read_function(
                       data->inherited.extra, data->buffer, data->buffer_size);
                       
            data->current_buffer_start = data->buffer;
            data->current_buffer_end = data->buffer + bufmax;
          }
          else {
            bufmax = (size_t)data->current_buffer_end - (size_t)data->current_buffer_start;
            
            if(bufmax < buffer_size && bufmax < data->buffer_size) {
              memmove(data->buffer, data->current_buffer_start, bufmax);
              
              data->current_buffer_start = data->buffer;
              
              bufmax += data->read_function(
                          data->inherited.extra,
                          data->current_buffer_start + bufmax,
                          data->buffer_size - bufmax);
                          
              data->current_buffer_end = data->current_buffer_start + bufmax;
            }
          }
          
          if(preserve_internal_buffer) {
            if(buffer_size > bufmax)
              buffer_size = bufmax;
            else
              bufmax = buffer_size;
              
            memcpy(buffer, data->current_buffer_start, buffer_size);
          }
          else if(buffer_size <= bufmax) {
            memcpy(buffer, data->current_buffer_start, buffer_size);
            
            data->current_buffer_start += buffer_size;
            bufmax = buffer_size;
          }
          else {
            memcpy(buffer, data->current_buffer_start, bufmax);
            
            data->current_buffer_start = data->current_buffer_end;
            
            bufmax += data->read_function(
                        data->inherited.extra,
                        (uint8_t *)buffer + bufmax,
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

PMATH_API pmath_string_t pmath_file_readline(pmath_t file) {
  if(pmath_file_test(file, PMATH_FILE_PROP_READ | PMATH_FILE_PROP_TEXT)) {
    pmath_custom_t custom = get_single_file_object(file);
    
    if(pmath_custom_has_destructor(custom, destroy_text_file)) {
      struct _pmath_text_file_t *data = pmath_custom_get_data(custom);
      
      if(data->readln_function) {
        if(lock_file(&data->inherited)) {
          pmath_string_t result;
          
          if(!pmath_is_null(data->buffer)) {
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
) {
  if(pmath_file_test(file, PMATH_FILE_PROP_READ | PMATH_FILE_PROP_BINARY)) {
    pmath_custom_t custom = get_single_file_object(file);
    
    if(pmath_custom_has_destructor(custom, destroy_binary_file)) {
      struct _pmath_binary_file_t *data = pmath_custom_get_data(custom);
      
      if(lock_file(&data->inherited)) {
        uint8_t *newbuf = pmath_mem_realloc_no_failfree(data->buffer, size);
        
        if(newbuf || size == 0) {
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
void pmath_file_set_textbuffer(pmath_t file, pmath_string_t buffer) {
  if(pmath_file_test(file, PMATH_FILE_PROP_READ | PMATH_FILE_PROP_TEXT)) {
    pmath_custom_t custom = get_single_file_object(file);
    
    if(pmath_custom_has_destructor(custom, destroy_text_file)) {
      struct _pmath_text_file_t *data = pmath_custom_get_data(custom);
      
      if(data->readln_function) {
        if(lock_file(&data->inherited)) {
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

struct file_write_arg_t {
  const void *buffer;
  size_t      buffer_size;
};

static pmath_bool_t fileobj_write_callback(pmath_custom_t custom, void *_args) {
  struct file_write_arg_t *args = _args;
  
  if(pmath_custom_has_destructor(custom, destroy_binary_file)) {
    struct _pmath_binary_file_t *data = pmath_custom_get_data(custom);
    if(data->write_function) {
      if(lock_file(&data->inherited)) {
        size_t result = data->write_function(
                          data->inherited.extra,
                          args->buffer,
                          args->buffer_size);
                          
        unlock_file(&data->inherited);
        
        if(result < args->buffer_size)
          args->buffer_size = result;
        
        return result > 0;
      }
    }
  }
  
  return FALSE;
}

PMATH_API size_t pmath_file_write(
  pmath_t          file,
  const void      *buffer,
  size_t           buffer_size
) {
  struct file_write_arg_t args;
  args.buffer      = buffer;
  args.buffer_size = buffer_size;
  
  if(!foreach_file_object(file, fileobj_write_callback, &args))
    return 0;
  
  return args.buffer_size;
}

struct file_writetext_arg_t {
  const uint16_t  *str;
  int              len;
};

static pmath_bool_t fileobj_writetext_callback(pmath_custom_t custom, void *_args) {
  struct file_writetext_arg_t *args = _args;
  
  if(pmath_custom_has_destructor(custom, destroy_text_file)) {
    struct _pmath_text_file_t *data = pmath_custom_get_data(custom);
    if(data->write_function) {
      if(lock_file(&data->inherited)) {
        pmath_bool_t success = data->write_function(data->inherited.extra, args->str, args->len);
        unlock_file(&data->inherited);
        return success;
      }
    }
  }
  
  return FALSE;
}

PMATH_API pmath_bool_t pmath_file_writetext(
  pmath_t          file,
  const uint16_t  *str,
  int              len
) {
  struct file_writetext_arg_t args;
  if(len < 0) {
    len = 0;
    while(len < INT_MAX && str[len])
      ++len;
  }
  args.str = str;
  args.len = len;
  
  return foreach_file_object(file, fileobj_writetext_callback, &args);
}

static pmath_bool_t fileobj_flush_callback(pmath_custom_t custom, void *_args) {
  if( pmath_custom_has_destructor(custom, destroy_binary_file) ||
       pmath_custom_has_destructor(custom, destroy_text_file))
  {
    struct _file_t *f = pmath_custom_get_data(custom);
    if(f->flush_function) {
      if(lock_file(f)) {
        f->flush_function(f->extra);
        
        unlock_file(f);
      }
    }
  }
  
  return TRUE;
}

PMATH_API void pmath_file_flush(pmath_t file) {
  foreach_file_object(file, fileobj_flush_callback, NULL);
}

/**\brief Get the stream's position, if possible
   \param file A file object. It wont be freed.
 */
PMATH_API int64_t pmath_file_get_position(pmath_t file) {
  int64_t result = -1;
  
  pmath_custom_t custom = get_single_file_object(file);
  if(pmath_is_null(custom))
    return -1;
  
  if(pmath_custom_has_destructor(custom, destroy_binary_file)) {
    struct _pmath_binary_file_t *f = pmath_custom_get_data(custom);
    
    if(f->get_pos_function) {
      if(lock_file(&f->inherited)) {
        result = f->get_pos_function(f->inherited.extra);
        
        if(f->current_buffer_start < f->current_buffer_end) {
          result -= (int64_t)(f->current_buffer_end - f->current_buffer_start);
        }
        
        unlock_file(&f->inherited);
      }
    }
  }
//  else if(pmath_custom_has_destructor(custom, destroy_text_file))
//  {
//    struct _file_t *f = pmath_custom_get_data(custom);
//
//    if(f->get_pos_function) {
//      if(lock_file(f)) {
//        result = f->get_pos_function(f->extra);
//
//        unlock_file(f);
//      }
//    }
//  }
  
  pmath_unref(custom);
  return result;
}

struct file_set_position_args_t {
  int64_t offset;
  int     origin;
};

static pmath_bool_t fileobj_set_position_callback(pmath_custom_t custom, void *_args) {
  struct file_set_position_args_t *args = _args;
  pmath_bool_t result = FALSE;
  
  if(pmath_custom_has_destructor(custom, destroy_binary_file)) {
    struct _pmath_binary_file_t *f = pmath_custom_get_data(custom);
    
    if(f->set_pos_function) {
      if(lock_file(&f->inherited)) {
        result = f->set_pos_function(f->inherited.extra, args->offset, args->origin);
        
        if(result)
          f->current_buffer_start = f->current_buffer_end;
          
        unlock_file(&f->inherited);
      }
    }
  }
//  else if(pmath_custom_has_destructor(custom, destroy_text_file))
//  {
//    struct _file_t *f = pmath_custom_get_data(custom);
//
//    if(f->get_pos_function) {
//      if(lock_file(f)) {
//        result = f->get_pos_function(f->extra);
//
//        unlock_file(f);
//      }
//    }
//  }
  
  return result;
}

PMATH_API pmath_bool_t pmath_file_set_position(
  pmath_t file,
  int64_t offset,
  int     origin
) {
  struct file_set_position_args_t args;
  args.offset = offset;
  args.origin = origin;
  return foreach_file_object(file, fileobj_set_position_callback, &args);
}

typedef struct {
  pmath_t      file;
  pmath_bool_t success;
} _write_data_t;

static void write_data(void *user, const uint16_t *data, int len) {
  _write_data_t *wd = user;
  
  if(wd->success)
    wd->success = pmath_file_writetext(wd->file, data, len);
}

PMATH_API pmath_bool_t pmath_file_write_object(
  pmath_t                 file,
  pmath_t                 obj,
  pmath_write_options_t   options
) {
  int page_width = -1;
  pmath_t pagewidth_obj;
  _write_data_t data;
  
  data.file    = file;
  data.success = TRUE;
  
  pagewidth_obj = pmath_evaluate(
                    pmath_parse_string_args("System`Try(System`OptionValue(`1`,System`PageWidth))", "(o)", pmath_ref(file)));
                    
  if(pmath_is_int32(pagewidth_obj))
    page_width = PMATH_AS_INT32(pagewidth_obj);
    
  pmath_unref(pagewidth_obj);
  
  if(page_width >= 6)
    pmath_write_with_pagewidth(obj, options, write_data, &data, page_width, 0);
  else
    pmath_write(obj, options, write_data, &data);
    
  return data.success;
}

PMATH_API
void pmath_file_manipulate(
  pmath_t   file,
  void    (*type)(void *),
  void    (*callback)(void *, void *),
  void     *data
) {
  pmath_custom_t custom = get_single_file_object(file);
  
  if( pmath_custom_has_destructor(custom, destroy_binary_file) ||
      pmath_custom_has_destructor(custom, destroy_text_file))
  {
    struct _file_t *f = (struct _file_t *)pmath_custom_get_data(custom);
    
    if(f->extra_destructor == type) {
      if(lock_file(f)) {
        callback(f->extra, data);
        
        unlock_file(f);
      }
    }
  }
  
  pmath_unref(custom);
}

PMATH_API
pmath_bool_t pmath_file_close(pmath_t file) {
  pmath_bool_t success = FALSE;
  
  if(pmath_is_symbol(file)) {
    pmath_custom_t custom = pmath_symbol_get_value(file);
    
    if( pmath_is_custom(custom) &&
        (pmath_custom_has_destructor(custom, destroy_binary_file) ||
         pmath_custom_has_destructor(custom, destroy_text_file)))
    {
      pmath_symbol_set_attributes(file, PMATH_SYMBOL_ATTRIBUTE_TEMPORARY);
      success = _pmath_clear(file, TRUE);
    }
    
    pmath_unref(custom);
  }
  else if(pmath_is_expr(file)) {
    pmath_t head = pmath_expr_get_item(file, 0);
    pmath_unref(head);
    
    if(pmath_same(head, pmath_System_InputStream) || pmath_same(head, pmath_System_OutputStream)) {
      success = pmath_file_close(pmath_expr_get_item(file, 1));
    }
    else if(pmath_same(head, pmath_System_List)) {
      size_t i;
      success = TRUE;
      for(i = pmath_expr_length(file); i > 0; --i) {
        success = pmath_file_close(pmath_expr_get_item(file, i)) && success;
      }
    }
  }
  
  pmath_unref(file);
  return success;
}

PMATH_API
void pmath_file_close_if_unused(pmath_t file) {
  if(pmath_is_symbol(file)) {
    if(pmath_file_test(file, 0)) {
      intptr_t file_refs = _pmath_symbol_self_refcount(file);
      
      if(file_refs + 1 == pmath_refcount(file)) {
        pmath_file_close(file);
        return;
      }
    }
  }
  else if(pmath_is_expr(file) && pmath_refcount(file) == 1) {
    pmath_t head = pmath_expr_get_item(file, 0);
    pmath_unref(head);
    
    if(pmath_same(head, pmath_System_InputStream) || pmath_same(head, pmath_System_OutputStream)) {
      pmath_file_close_if_unused(pmath_expr_extract_item(file, 1));
    }
    else if(pmath_same(head, pmath_System_List)) {
      size_t i;
      for(i = pmath_expr_length(file); i > 0; --i) {
        pmath_file_close_if_unused(
          pmath_expr_extract_item(file, i));
      }
    }
  }
  
  pmath_unref(file);
}

//==============================================================================

PMATH_API
pmath_symbol_t pmath_file_create_binary(
  void                     *extra,
  void                    (*extra_destructor)(void *),
  pmath_binary_file_api_t  *api
) {
  struct _pmath_binary_file_t *data;
  pmath_custom_t custom;
  pmath_symbol_t file;
  
  if( api->struct_size < sizeof(size_t) + 3 * sizeof(void *) ||
      !extra_destructor ||
      (!api->status_function && api->read_function))
  {
    return PMATH_NULL;
  }
  
  data = (struct _pmath_binary_file_t *)pmath_mem_alloc(sizeof(struct _pmath_binary_file_t));
  if(!data) {
    extra_destructor(extra);
    
    return PMATH_NULL;
  }
  
  memset(data, 0, sizeof(struct _pmath_binary_file_t));
  
  data->inherited.extra            = extra;
  data->inherited.extra_destructor = extra_destructor;
  data->inherited.status_function  = api->status_function;
  data->read_function              = api->read_function;
  data->write_function             = api->write_function;
  
  if(api->struct_size >= (size_t)&api->flush_function - (size_t)api + sizeof(void *))
    data->inherited.flush_function = api->flush_function;
    
  if(api->struct_size >= (size_t)&api->get_pos_function - (size_t)api + sizeof(void *))
    data->get_pos_function = api->get_pos_function;
    
  if(api->struct_size >= (size_t)&api->set_pos_function - (size_t)api + sizeof(void *))
    data->set_pos_function = api->set_pos_function;
    
  if(data->read_function) {
    data->buffer_size = 256;
    data->buffer = (uint8_t *)pmath_mem_alloc(data->buffer_size);
    if(!data->buffer) {
      destroy_binary_file(data);
      
      return PMATH_NULL;
    }
    
    data->current_buffer_start = data->current_buffer_end = data->buffer;
  }
  
  custom = pmath_custom_new(data, destroy_binary_file);
  if(pmath_is_null(custom))
    return PMATH_NULL;
    
  file = pmath_symbol_create_temporary(
           PMATH_C_STRING("System`Private`io`binStream"),
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
  void                  (*extra_destructor)(void *),
  pmath_text_file_api_t  *api
) {
  struct _pmath_text_file_t *data;
  pmath_custom_t custom;
  pmath_symbol_t file;
  
  if( api->struct_size < sizeof(size_t) + 3 * sizeof(void *) ||
      !extra_destructor || (!api->status_function && api->readln_function))
  {
    return PMATH_NULL;
  }
  
  data = (struct _pmath_text_file_t *)pmath_mem_alloc(sizeof(struct _pmath_text_file_t));
  if(!data) {
    extra_destructor(extra);
    
    return PMATH_NULL;
  }
  
  memset(data, 0, sizeof(struct _pmath_text_file_t));
  data->buffer = PMATH_NULL;
  
  data->inherited.extra            = extra;
  data->inherited.extra_destructor = extra_destructor;
  data->inherited.status_function  = api->status_function;
  data->readln_function            = api->readln_function;
  data->write_function             = api->write_function;
  
  if(api->struct_size >= (size_t)&api->flush_function - (size_t)api + sizeof(void *))
    data->inherited.flush_function = api->flush_function;
    
  custom = pmath_custom_new(data, destroy_text_file);
  if(pmath_is_null(custom))
    return PMATH_NULL;
    
  file = pmath_symbol_create_temporary(
           PMATH_C_STRING("System`Private`io`textStream"),
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
