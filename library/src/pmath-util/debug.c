#include <pmath-core/objects-private.h>
#include <pmath-core/expressions-private.h>
#include <pmath-core/strings-private.h>

#include <pmath-util/debug.h>
#include <pmath-util/helpers.h>

#include <pmath-builtins/all-symbols-private.h>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>


#ifdef pmath_debug_print
#  undef pmath_debug_print
#  undef pmath_debug_print_object
#  undef pmath_debug_print_stack
#  undef pmath_debug_print_debug_info
#endif

#ifdef PMATH_OS_WIN32
#    include <windows.h>
#endif

#ifdef PMATH_OS_WIN32
/* no flockfile()/funlockfile() on windows/mingw -> do it your self */
#  if PMATH_USE_PTHREAD
#    include <pthread.h>
static pthread_mutex_t  debuglog_mutex;
#    define flockfile(  file)  ((void)pthread_mutex_lock(  &debuglog_mutex))
#    define funlockfile(file)  ((void)pthread_mutex_unlock(&debuglog_mutex))
#  elif PMATH_USE_WINDOWS_THREADS
//#    define NOGDI
//#    define WIN32_LEAN_AND_MEAN
//#    include <windows.h>
static CRITICAL_SECTION  debuglog_critical_section;
#    define flockfile(  file)  ((void)EnterCriticalSection(&debuglog_critical_section))
#    define funlockfile(file)  ((void)LeaveCriticalSection(&debuglog_critical_section))
#  else
#    error Either PThread or Windows Threads must be used
#  endif
#endif

#ifdef _MSC_VER
#  define snprintf sprintf_s
#endif


static FILE *debuglog = NULL;

static pmath_bool_t debugging_output = TRUE;


/* Redirect pmath_debug_print_object(...) and pmath_debug_print_stack(...) to OutputDebugString().
  Settable in WinDbg with

  ed pmath!pmath_debug_print_to_debugger 1
 */
PMATH_API int pmath_debug_print_to_debugger = 0;

PMATH_API void pmath_debug_print(const char *fmt, ...) {
  if(debugging_output) {
    va_list args;
    
    debugging_output = FALSE;
    va_start(args, fmt);
    
    if(pmath_debug_print_to_debugger) {
      char small_buffer[1024];
      
      // TODO: use larger buffer is there is some %s or similar in the format string.
      vsnprintf(small_buffer, sizeof(small_buffer), fmt, args);
      
#ifdef PMATH_OS_WIN32
      {
        OutputDebugStringA(small_buffer);
      }
#else
      {
        flockfile(debuglog);
      
        fprintf(debuglog, "%s", small_buffer);
      
        funlockfile(debuglog);
        fflush(debuglog);
      }
#endif
    }
    else {
      flockfile(debuglog);
      
      vfprintf(debuglog, fmt, args);
      
      funlockfile(debuglog);
      fflush(debuglog);
    }
    
    va_end(args);
    debugging_output = TRUE;
  }
}

static void write_data_to_file(void *_file, const uint16_t *data, int len) {
  FILE *file = _file;
  
  while(len-- > 0) {
    if(*data <= 0xFF) {
      unsigned char c = (unsigned char) * data;
      fwrite(&c, 1, 1, file);
    }
    else {
      char hex[16] = "0123456789ABCDEF";
      char out[6];
      out[0] = '\\';
      out[1] = '\\';
      out[2] = hex[(*data & 0xF000) >> 12];
      out[3] = hex[(*data & 0x0F00) >>  8];
      out[4] = hex[(*data & 0x00F0) >>  4];
      out[5] = hex[ *data & 0x000F];
      fwrite(out, 1, sizeof(out), file);
    }
    ++data;
  }
}

static void write_data_to_debugger(void *dummy, const uint16_t *data, int len) {
#ifdef PMATH_OS_WIN32
  {
    wchar_t buffer[256];
    const int maxlen = sizeof(buffer) / sizeof(buffer[0]) - 1;
    
    while(len > maxlen) {
      memcpy(buffer, data, maxlen * sizeof(buffer[0]));
      buffer[maxlen] = 0;
      OutputDebugStringW(buffer);
      
      len -= maxlen;
      data += maxlen;
    }
    
    if(len > 0) {
      memcpy(buffer, data, len * sizeof(buffer[0]));
      buffer[len] = 0;
      OutputDebugStringW(buffer);
    }
  }
#else
  {
    write_data_to_file(debuglog, data, len);
  }
#endif
}

static const char *get_description(pmath_t obj) {
  if(pmath_is_int32(obj)) {
    return "int32";
  }
  else if(pmath_is_double(obj)) {
    return "double";
  }
  else if(pmath_is_str0(obj)) {
    return "str0";
  }
  else if(pmath_is_str1(obj)) {
    return "str1";
  }
  else if(pmath_is_str2(obj)) {
    return "str2";
  }
  else if(pmath_is_magic(obj)) {
    return "magic";
  }
  else if(pmath_is_pointer(obj)) {
    struct _pmath_t *pointer = PMATH_AS_PTR(obj);
    
    if(!pointer)
      return "null";
      
    switch(pointer->type_shift) {
    
      case PMATH_TYPE_SHIFT_MP_INT:
        return "mp-int";
        
      case PMATH_TYPE_SHIFT_MP_FLOAT:
        return "mp-float";
        
      case PMATH_TYPE_SHIFT_QUOTIENT:
        return "quotient";
        
      case PMATH_TYPE_SHIFT_BIGSTRING:
        return "string";
        
      case PMATH_TYPE_SHIFT_SYMBOL:
        return "symbol";
        
      case PMATH_TYPE_SHIFT_EXPRESSION_GENERAL:
        return "expr";
        
      case PMATH_TYPE_SHIFT_EXPRESSION_GENERAL_PART:
        return "expr-part";
        
      case PMATH_TYPE_SHIFT_MULTIRULE:
        return "multirule";
        
      case PMATH_TYPE_SHIFT_CUSTOM:
        return "custom";
        
      case PMATH_TYPE_SHIFT_BLOB:
        return "blob";
        
      case PMATH_TYPE_SHIFT_PACKED_ARRAY:
        return "packed-array";
        
      case PMATH_TYPE_SHIFT_INTERVAL:
        return "interval";
    }
  }
  
  return "?";
}

struct _debug_print_raw_info_t {
  void (*write)(void*, const uint16_t*, int);
  void *user;
  
  const char *link_begin_fmt;
  const char *link_end_fmt;
  
  char *index_info;
  size_t index_info_length;
  
  size_t maxlength;
  int maxdepth;
};

/* Setable by the debugger
   WinDbg:
     eza pmath!pmath_debug_print_begin_link_fmt "<?dml?><i>"
     eza pmath!pmath_debug_print_end_link_fmt "<?dml?></i>"

   Takes [1] an uint64_t (pmath_t::as_bits) and [2] a char* (info->index_info)
 */
PMATH_API char pmath_debug_print_raw_begin_link_fmt[1024] = ""; //"<?dml?><link cmd=\".call pmath!pmath_debug_print_raw(0x%" PRIx64 "); g\">";
PMATH_API char pmath_debug_print_raw_end_link_fmt[256] = ""; //"<?dml?></link>";

PMATH_API size_t pmath_debug_print_raw_maxlength = 5;
PMATH_API int pmath_debug_print_raw_maxdepth = 1;

static void debug_print_link(
  struct _debug_print_raw_info_t *info,
  const char                     *description,
  pmath_t                         obj
) {
  char buffer[256];
  
  snprintf(
    buffer,
    sizeof(buffer),
    info->link_begin_fmt,
    obj.as_bits,
    info->index_info);
  _pmath_write_cstr(buffer, info->write, info->user);
  
  snprintf(
    buffer,
    sizeof(buffer),
    "[%s %016" PRIx64 "]",
    description,
    obj.as_bits);
  _pmath_write_cstr(buffer, info->write, info->user);
  
  snprintf(
    buffer,
    sizeof(buffer),
    info->link_end_fmt,
    obj.as_bits,
    info->index_info);
  _pmath_write_cstr(buffer, info->write, info->user);
}

static void debug_print_indent(struct _debug_print_raw_info_t *info, int depth) {
  static const uint16_t space[2] = {' ', ' '};
  
  while(depth-- > 0)
    info->write(info->user, space, 2);
}

static void debug_print_raw_impl(
  struct _debug_print_raw_info_t *info,
  pmath_t                         obj,
  int                             maxdepth);

static void debug_print_item(
  struct _debug_print_raw_info_t *info,
  size_t                          index_info_start,
  pmath_t                         obj,
  size_t                          i,
  int                             depth
) {
  pmath_t item;
  
  snprintf(
    info->index_info + index_info_start,
    info->index_info_length - index_info_start,
    "[%" PRIxPTR "]",
    i);
    
  item = pmath_expr_get_item(obj, i);
  debug_print_raw_impl(info, item, depth);
  pmath_unref(item);
}

static void debug_print_skip_skeleton(
  struct _debug_print_raw_info_t *info,
  size_t                          skip_count
) {
  char buffer[64];
  
  snprintf(
    buffer,
    sizeof(buffer),
    "<< skipped %" PRIuPTR " elements >>",
    skip_count);
    
  _pmath_write_cstr(buffer, info->write, info->user);
}

static void debug_print_raw_pointer_impl(
  struct _debug_print_raw_info_t *info,
  struct _pmath_t                *pointer,
  int                             depth
) {
  pmath_t obj = PMATH_FROM_PTR(pointer);
  
  if(!pointer) {
    pmath_write(
      obj,
      PMATH_WRITE_OPTIONS_FULLSTR | PMATH_WRITE_OPTIONS_FULLNAME,
      info->write,
      info->user);
      
    return;
  }
  
  switch(pointer->type_shift) {
    case PMATH_TYPE_SHIFT_BIGSTRING:
    case PMATH_TYPE_SHIFT_SYMBOL:
      pmath_write(
        obj,
        PMATH_WRITE_OPTIONS_FULLSTR | PMATH_WRITE_OPTIONS_FULLNAME,
        info->write,
        info->user);
      break;
      
    case PMATH_TYPE_SHIFT_EXPRESSION_GENERAL:
    case PMATH_TYPE_SHIFT_EXPRESSION_GENERAL_PART:
    case PMATH_TYPE_SHIFT_PACKED_ARRAY:
      if(depth < info->maxdepth) {
        size_t length, i;
        pmath_t head;
        size_t index_info_start = strlen(info->index_info);
        
        ++depth;
        
        head = pmath_expr_get_item(obj, 0);
        if(!pmath_same(head, PMATH_SYMBOL_LIST)) {
          snprintf(
            info->index_info + index_info_start,
            info->index_info_length - index_info_start,
            "[0]");
            
          debug_print_raw_impl(info, head, depth);
        }
        pmath_unref(head);
        
        length = pmath_expr_length(obj);
        if(length == 0) {
          if(pmath_same(head, PMATH_SYMBOL_LIST))
            _pmath_write_cstr("{}", info->write, info->user);
          else
            _pmath_write_cstr("()", info->write, info->user);
        }
        else {
          size_t skip_index;
          size_t skip_count;
          
          if(length > info->maxlength) {
            skip_index = info->maxlength / 2 + 1;
            
            if(info->maxlength == 0)
              skip_count = length;
            else
              skip_count = length - info->maxlength + 1;
          }
          else {
            skip_index = length + 1;
            skip_count = 0;
          }
          
          if(pmath_same(head, PMATH_SYMBOL_LIST)) {
            _pmath_write_cstr("{ ", info->write, info->user);
          }
          else {
            _pmath_write_cstr("(\n", info->write, info->user);
            
            debug_print_indent(info, depth);
          }
          
          if(skip_index > 1) {
            debug_print_item(info, index_info_start, obj, 1, depth);
            
            for(i = 2; i < skip_index; ++i) {
              _pmath_write_cstr(",\n", info->write, info->user);
              debug_print_indent(info, depth);
              
              debug_print_item(info, index_info_start, obj, i, depth);
            }
            
            if(skip_index <= length) {
              _pmath_write_cstr(",\n", info->write, info->user);
              debug_print_indent(info, depth);
            }
          }
          
          if(skip_index <= length) {
            debug_print_skip_skeleton(info, skip_count);
            
            for(i = skip_index + skip_count; i <= length; ++i) {
              _pmath_write_cstr(",\n", info->write, info->user);
              debug_print_indent(info, depth);
              
              debug_print_item(info, index_info_start, obj, i, depth);
            }
          }
          
          if(pmath_same(head, PMATH_SYMBOL_LIST))
            _pmath_write_cstr(" }", info->write, info->user);
          else
            _pmath_write_cstr(")", info->write, info->user);
            
          info->index_info[index_info_start] = '\0';
        }
      }
      else
        debug_print_link(info, get_description(obj), obj);
        
      break;
      
    default:
      debug_print_link(info, get_description(obj), obj);
  }
}

static void debug_print_raw_impl(
  struct _debug_print_raw_info_t *info,
  pmath_t                         obj,
  int                             depth
) {
  if(!pmath_is_pointer(obj)) {
    pmath_write(
      obj,
      PMATH_WRITE_OPTIONS_FULLSTR | PMATH_WRITE_OPTIONS_FULLNAME,
      info->write,
      info->user);
  }
  else
    debug_print_raw_pointer_impl(info, PMATH_AS_PTR(obj), depth);
}

PMATH_API
void pmath_debug_print_raw(uint64_t obj_data) {
  struct _debug_print_raw_info_t info;
  pmath_t obj;
  char index_info_buffer[256] = "";
  
  obj.as_bits = obj_data;
  
  if(debugging_output) {
    debugging_output = FALSE;
    
    info.write = write_data_to_debugger;
    info.user = NULL;
    info.link_begin_fmt = pmath_debug_print_raw_begin_link_fmt;
    info.link_end_fmt = pmath_debug_print_raw_end_link_fmt;
    info.index_info = index_info_buffer;
    info.index_info_length = sizeof(index_info_buffer);
    info.maxlength = pmath_debug_print_raw_maxlength;
    info.maxdepth = pmath_debug_print_raw_maxdepth;
    
    debug_print_link(&info, get_description(obj), obj);
    _pmath_write_cstr(" = \n", info.write, info.user);
    //debug_print_indent(&info, 0);
    
    debug_print_raw_impl(&info, obj, 0);
    
    _pmath_write_cstr("\n", info.write, info.user);
    
    debugging_output = TRUE;
  }
}

PMATH_API void pmath_debug_print_object(
  const char *pre,
  pmath_t     obj,
  const char *post
) {
  if(debugging_output) {
    debugging_output = FALSE;
    
#ifdef PMATH_OS_WIN32
    if(pmath_debug_print_to_debugger) {
      OutputDebugStringA(pre);
      
      pmath_write(
        obj,
        PMATH_WRITE_OPTIONS_FULLSTR | PMATH_WRITE_OPTIONS_INPUTEXPR | PMATH_WRITE_OPTIONS_FULLNAME,
        write_data_to_debugger,
        NULL);
        
      OutputDebugStringA(post);
    }
    else
#endif
    {
      flockfile(debuglog);
      fputs(pre, debuglog);
      
      pmath_write(
        obj,
        PMATH_WRITE_OPTIONS_FULLSTR | PMATH_WRITE_OPTIONS_INPUTEXPR | PMATH_WRITE_OPTIONS_FULLNAME,
        write_data_to_file,
        debuglog);
        
      fputs(post, debuglog);
      fflush(debuglog);
      funlockfile(debuglog);
    }
    
    debugging_output = TRUE;
  }
}

static pmath_bool_t stack_walker(pmath_t head, pmath_t debug_info, void *p) {
  pmath_debug_print_object("\n  in ", head, "");
  
  if(!pmath_is_null(debug_info)) {
    if(pmath_is_expr_of_len(debug_info, PMATH_SYMBOL_DEVELOPER_DEBUGINFOSOURCE, 2)) {
      pmath_t file = pmath_expr_get_item(debug_info, 1);
      pmath_t pos = pmath_expr_get_item(debug_info, 2);
      
      pmath_debug_print_object(" from ", file, ", ");
      pmath_debug_print_object("", pos, "");
      
      pmath_unref(file);
      pmath_unref(pos);
    }
    else
      pmath_debug_print_object(" from ", debug_info, "");
  }
  
  return TRUE;
}

PMATH_API void pmath_debug_print_stack(void) {
  pmath_debug_print("pMath stack:");
  pmath_walk_stack_2(stack_walker, NULL);
  pmath_debug_print("\n");
}

PMATH_API
void pmath_debug_print_debug_info(
  const char *pre,
  pmath_t     obj,
  const char *post
) {
  pmath_t info = pmath_get_debug_info(obj);
  
  pmath_debug_print_object(pre, obj, post);
  
  pmath_unref(info);
}

/*============================================================================*/

/* The following variables are used by the debugger visualizer (pmath.natvis) only.
 */

PMATH_PRIVATE
pmath_symbol_t *_pmath_DebugInfoSource_symbol = &PMATH_SYMBOL_DEVELOPER_DEBUGINFOSOURCE;
PMATH_PRIVATE
pmath_symbol_t *_pmath_List_symbol = &PMATH_SYMBOL_LIST;
PMATH_PRIVATE
pmath_symbol_t *_pmath_Range_symbol = &PMATH_SYMBOL_RANGE;
PMATH_PRIVATE
pmath_symbol_t *_pmath_Rule_symbol = &PMATH_SYMBOL_RULE;
PMATH_PRIVATE
pmath_symbol_t *_pmath_RuleDelayed_symbol = &PMATH_SYMBOL_RULEDELAYED;

PMATH_PRIVATE
struct _pmath_debug_symbol_attribute_t {
  pmath_symbol_attributes_t value;
} _pmath_debug_symbol_attribute_other_t_dummy_var;

PMATH_PRIVATE
struct _pmath_debug_span_t {
  uintptr_t value;
} _pmath_debug_span_t_dummy_var;

/*============================================================================*/

PMATH_PRIVATE pmath_bool_t _pmath_debug_library_init(void) {
#ifdef PMATH_OS_WIN32
#if PMATH_USE_PTHREAD
  { /* initialize debuglog_mutex ... */
    int err = pthread_mutex_init(&debuglog_mutex, NULL);
    if(err != 0)
      return FALSE;
  }
#elif PMATH_USE_WINDOWS_THREADS
  /* initialize debuglog_critical_section ... */
  if(!InitializeCriticalSectionAndSpinCount(&debuglog_critical_section, 4000))
    return FALSE;
#endif
#endif
  
  debuglog = stderr;
  
//  debuglog = fopen("pmath-debug.log", "w+");
//  char name[100];
//  int i = 0;
//  while(!debuglog && i < 100){
//    sprintf(name, "pmath-debug%2d.log",i);
//    debuglog = fopen(name, "w+");
//    i++;
//  }
//  if(!debuglog)
//    debuglog = stderr;
  return TRUE;
}

PMATH_PRIVATE void _pmath_debug_library_done(void) {
  if(debuglog && debuglog != stderr && debuglog != stdout) {
    fclose(debuglog);
    debuglog = NULL;
  }
  
#ifdef PMATH_OS_WIN32
#if PMATH_USE_PTHREAD
  pthread_mutex_destroy(&debuglog_mutex);
#elif PMATH_USE_WINDOWS_THREADS
  DeleteCriticalSection(&debuglog_critical_section);
#endif
#endif
}
