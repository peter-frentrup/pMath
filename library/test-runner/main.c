#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pmath.h>

#ifdef _MSC_VER
#  define snprintf sprintf_s
#endif

typedef struct Str {
  const char *s;
  size_t len;
} Str;
#define S(bytes)  (Str){ bytes "", sizeof(bytes) - 1}

typedef struct StringBuffer {
  size_t capacity;
  size_t len;
  char *s;
  pmath_bool_t err;
} StringBuffer;

typedef struct MappedFile {
  Str all;
  Str remaining;
} MappedFile;

struct CritSect; // Non-reentrant critical sections

static void os_init(void);

static pmath_bool_t MappedFile_open(MappedFile *f, const char *path);
static void MappedFile_close(MappedFile *f);

static void CritSect_init(struct CritSect *cs);
static void CritSect_destroy(struct CritSect *cs);
static void CritSect_enter(struct CritSect *cs);
static void CritSect_exit(struct CritSect *cs);


#define PMATH_SYSTEM_SYMBOL_X( sym )         X( pmath_System_ ## sym       , "System`" #sym )
#define PMATH_SYSTEM_DOLLAR_SYMBOL_X( sym )  X( pmath_System_Dollar ## sym , "System`$" #sym )
#define PMATH_LANGUAGE_SYMBOL_X( sym )       X( pmath_Language_ ## sym     , "Language`" #sym )
#define PMATH_SYMBOLS_X                      \
  PMATH_LANGUAGE_SYMBOL_X( SourceLocation  ) \
  PMATH_SYSTEM_SYMBOL_X( BoxData         ) \
  PMATH_SYSTEM_SYMBOL_X( HoldComplete    ) \
  PMATH_SYSTEM_SYMBOL_X( List            ) \
  PMATH_SYSTEM_SYMBOL_X( MakeExpression  ) \
  PMATH_SYSTEM_SYMBOL_X( OpenAppend      ) \
  PMATH_SYSTEM_SYMBOL_X( OpenWrite       ) \
  PMATH_SYSTEM_SYMBOL_X( Range           ) \
  PMATH_SYSTEM_SYMBOL_X( RawBoxes        ) \
  PMATH_SYSTEM_SYMBOL_X( Return          ) \
  PMATH_SYSTEM_SYMBOL_X( Row             ) \
  PMATH_SYSTEM_SYMBOL_X( Section         ) \
  PMATH_SYSTEM_SYMBOL_X( SectionPrint    ) \
  PMATH_SYSTEM_SYMBOL_X( Sequence        )

#define X( SYM, NAME )  static pmath_symbol_t SYM = PMATH_STATIC_NULL;
  PMATH_SYMBOLS_X
#undef X

#ifdef PMATH_OS_WIN32
//{ Windows ...
#  include <io.h>
#  define dup    _dup
#  ifdef fileno
#    undef fileno
#  endif
#  define fileno _fileno
#  define fdopen _fdopen

#  define NOGDI
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>

//{ Memory mapped files ...

static pmath_bool_t MappedFile_open(MappedFile *f, const char *path) {
  f->all       = S("");
  f->remaining = S("");
  
  // TODO: use CreateFileW
  HANDLE hFile = CreateFileA(
                    path,
                    GENERIC_READ,
                    FILE_SHARE_READ,
                    NULL,
                    OPEN_EXISTING,
                    FILE_ATTRIBUTE_NORMAL,
                    NULL);
  
  if(hFile == INVALID_HANDLE_VALUE) {
    return FALSE;
  }
  
  LARGE_INTEGER file_size;
  if (!GetFileSizeEx(hFile, &file_size)) {
    CloseHandle(hFile);
    return FALSE;
  }
  
  size_t size = (size_t)file_size.QuadPart;
  if((LONGLONG)size != file_size.QuadPart) {
    CloseHandle(hFile);
    return FALSE;
  }
  
  if(size == 0) {
    CloseHandle(hFile);
    return TRUE;
  }
  
  HANDLE hMap = CreateFileMappingW(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
  CloseHandle(hFile);
  
  void *data = MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0);
  CloseHandle(hMap);
  
  if(!data) {
    return FALSE;
  }
  
  f->all       = (Str){data, size};
  f->remaining = f->all;
  return TRUE;
}

static void MappedFile_close(MappedFile *f) {
  if(f->all.len > 0) {
    UnmapViewOfFile(f->all.s);
  }
  f->all       = S("");
  f->remaining = S("");
}

//} ... Memory mapped files

//{ Critical Sections ...

typedef struct CritSect {
  CRITICAL_SECTION win32_cs;
} CritSect;

static void CritSect_init(struct CritSect *cs) {
  InitializeCriticalSection(&cs->win32_cs);
}

static void CritSect_destroy(struct CritSect *cs) {
  DeleteCriticalSection(&cs->win32_cs);
}

static void CritSect_enter(struct CritSect *cs) {
  EnterCriticalSection(&cs->win32_cs);
}

static void CritSect_exit(struct CritSect *cs) {
  LeaveCriticalSection(&cs->win32_cs);
}

//} ... Critical Sections

static void os_init(void) {
  HMODULE kernel32;
  
  // do not show message boxes on LoadLibrary errors:
  SetErrorMode(SEM_NOOPENFILEERRORBOX | SEM_FAILCRITICALERRORS);
  
  // remove current directory from dll search path:
  kernel32 = GetModuleHandleW(L"Kernel32");
  if(kernel32) {
    BOOL (WINAPI * SetDllDirectoryW_ptr)(const WCHAR *);
    SetDllDirectoryW_ptr = (BOOL (WINAPI *)(const WCHAR *))
                           GetProcAddress(kernel32, "SetDllDirectoryW");
                           
    if(SetDllDirectoryW_ptr)
      SetDllDirectoryW_ptr(L"");
  }
}

//} ... Windows
#elif defined(PMATH_OS_UNIX)
//{ Unix like ...

#  include <fcntl.h>
#  include <unistd.h>
#  include <pthread.h>
#  include <sys/mman.h>
#  include <sys/stat.h>

//{ Memory mapped files ...

static pmath_bool_t MappedFile_open(MappedFile *f, const char *path) {
  f->all       = S("");
  f->remaining = S("");
  
  int fd = open(path, O_RDONLY);
  if(fd < 0) {
    return FALSE;
  }
  
  struct stat file_statistics;
  if(fstat(fd, &file_statistics) != 0) {
    close(fd);
    return FALSE;
  }
  
  size_t size = file_statistics.st_size;
  if(size == 0) {
    return TRUE;
  }
  
  void *data = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
  close(fd);
  
  if(data == MAP_FAILED) {
    return FALSE;
  }
  
  f->all       = (Str){data, size};
  f->remaining = f->all;
  return TRUE;
}

static void MappedFile_close(MappedFile *f) {
  if(f->all.len > 0) {
    munmap((void*)f->all.s, f->all.len);
  }
  f->all       = S("");
  f->remaining = S("");
}

//} ... Memory mapped files

//{ Critical Sections ...

typedef struct CritSect {
  pthread_mutex_t mutex;
} CritSect;

static void CritSect_init(struct CritSect *cs) {
  cs->mutex = (pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER;
}

static void CritSect_destroy(struct CritSect *cs) {
  pthread_mutex_destroy(&cs->mutex);
}

static void CritSect_enter(struct CritSect *cs) {
  pthread_mutex_lock(&cs->mutex);
}

static void CritSect_exit(struct CritSect *cs) {
  pthread_mutex_unlock(&cs->mutex);
}

//} ... Critical Sections

static void os_init(void) {

}

//} ... Unix like
#endif

static int console_width = 100;
static volatile pmath_bool_t quitting = FALSE;
static volatile pmath_bool_t show_mem_stats = TRUE;

static MappedFile input_file = { 0 };
static const char *input_file_path = NULL;
static pmath_t input_file_name = PMATH_STATIC_NULL;

static struct {
  CritSect      critical_section[1];
  StringBuffer  content[1];
  Str           current_prompt_prefix;
  Str           current_line_ending;
} output;


#define PROMPT        S("pmath> ")
#define PROMPT_MORE   S("     > ")
#define OUTPUT_INDENT S("       ")
#define PRINT_INDENT  S("    ")
#define ECHO_INDENT   S(" >> ")


//{ General string utility functions ...

static pmath_bool_t Str_eq(Str a, Str b) {
  return a.len == b.len && 0 == memcmp(a.s, b.s, a.len);
}

static pmath_bool_t Str_starts_with(Str text, Str prefix) {
  return text.len >= prefix.len && 0 == memcmp(text.s, prefix.s, prefix.len);
}

static Str Str_trim_spaces(Str text) {
  const char *s   = text.s;
  const char *end = text.s + text.len;
  
  while(s < end && (*s == ' ' || *s == '\t')) {
    ++s;
  }
  while(s < end && (end[-1] == ' ' || end[-1] == '\t')) {
    --end;
  }
  
  return (Str){ s, end - s };
}

static Str Str_take_ref(Str text, size_t max_len) {
  return (Str){ text.s, text.len < max_len ? text.len : max_len };
}

static Str Str_drop_ref(Str text, size_t max_len) {
  if(text.len <= max_len)
    return (Str){ text.s + text.len, 0};
  else
    return (Str){ text.s + max_len, text.len - max_len };
}

static Str Str_find_ref(Str text, Str sub) {
  if(sub.len == 0)
    return Str_take_ref(text, 0);
  
  if(text.len < sub.len)
    return Str_drop_ref(text, text.len);

  char last_ch = sub.s[sub.len - 1];
  const char *needle = text.s + sub.len - 1;
  const char *end = text.s + text.len;
  
  while(needle < end) {
    if(*needle++ == last_ch) {
      const char *candidate_start = needle - sub.len;
      if(Str_eq((Str){candidate_start, sub.len}, sub)) {
        return (Str){ candidate_start, sub.len};
      }
    }
  }
  
  return Str_drop_ref(text, text.len);
}

static Str Str_before_ref(Str text, Str sub_ref) {
  assert((uintptr_t)text.s <= (uintptr_t)sub_ref.s);
  assert((uintptr_t)(sub_ref.s + sub_ref.len) <= (uintptr_t)(text.s + text.len));
  return (Str){text.s, sub_ref.s - text.s};
}

static Str Str_after_ref(Str text, Str sub_ref) {
  assert((uintptr_t)text.s <= (uintptr_t)sub_ref.s);
  assert((uintptr_t)(sub_ref.s + sub_ref.len) <= (uintptr_t)(text.s + text.len));
  return (Str){sub_ref.s + sub_ref.len, (text.s + text.len) - (sub_ref.s + sub_ref.len)};
}

static Str Str_find_next_nl_ref(Str text) {
  const char *start = text.s;
  const char *end   = start + text.len;
  
  for(const char *s = start; s < end; ++s) {
    if(*s == '\n') 
      return (Str){s, 1};
    
    if(*s == '\r') 
      return (Str){s, (s[1] == '\n') ? 2 : 1};
  }
  
  return (Str){end, 0};
}

static Str Str_find_last_line_ref(Str text) {
  const char *start = text.s;
  const char *end   = start + text.len;
  
  const char *s = end;
  while(start < s && s[-1] != '\n' && s[-1] != '\r') {
    --s;
  }
  
  return (Str){s, end - s};
}

//} ... General string utility functions

//{ StringBuffer output ...

static pmath_bool_t StringBuffer_ensure_capacity(StringBuffer *buf, size_t min_capacity) {
  if(min_capacity <= buf->capacity)
    return TRUE;
  
  size_t new_capacity = buf->capacity + 100;
  while(new_capacity < min_capacity) {
    size_t next = new_capacity * 2;
    if(next < new_capacity) { // overflow
      buf->err = TRUE;
      return FALSE;
    }
    
    new_capacity = next;
  }
  char *new_s = realloc(buf->s, new_capacity);
  if(!new_s) {
    buf->err = TRUE;
    return FALSE;
  }
  
  buf->s = new_s;
  buf->capacity = new_capacity;
  return TRUE;
}

static void StringBuffer_append(StringBuffer *buf, Str str) {
  size_t new_len = buf->len + str.len;
  if(new_len < buf->len) { // overflow
    buf->err = TRUE;
    return;
  }
  
  if(!StringBuffer_ensure_capacity(buf, new_len)) 
    return;
  
  memmove(buf->s + buf->len, str.s, str.len);
  buf->len = new_len;
}

static void StringBuffer_free(StringBuffer *buf) {
  if(buf->capacity) {
    free(buf->s);
  }
  buf->len = 0;
  buf->capacity = 0;
  buf->err = TRUE;
}

// Only valid while buf is valid
static Str StringBuffer_as_Str(const StringBuffer *buf) {
  return (Str){ buf->s, buf->len };
}

static void Str_write(Str text) {
  CritSect_enter(output.critical_section);
  
  //fwrite(text.s, 1, text.len, stdout);
  StringBuffer_append(output.content, text);
  
  CritSect_exit(output.critical_section);
}

static void Str_write_prefixed_line(Str line) {
  CritSect_enter(output.critical_section);
  
  StringBuffer_append(output.content, output.current_prompt_prefix);
  StringBuffer_append(output.content, line);
  StringBuffer_append(output.content, output.current_line_ending);

  CritSect_exit(output.critical_section);
}

//} ... StringBuffer output

//{ Synchronized object output (line wise) ...

static pmath_threadlock_t print_lock = NULL;

static size_t bytes_since_last_abortcheck = 0;

typedef struct {
  pmath_t object;
  Str nl;
  Str indent_prefix;
  Str indent_more;
} WriteOutputCtx;

static void write_cstr(void *_context, const char *cstr) {
  WriteOutputCtx *context = _context;
  size_t len = strlen(cstr);
  
  if(bytes_since_last_abortcheck + len > 100) {
    if(pmath_aborting()) {
      bytes_since_last_abortcheck = 100;
      return;
    }
    else
      bytes_since_last_abortcheck = 0;
  }
  else
    bytes_since_last_abortcheck += len;
  
  const char *end = cstr + len;
  while(cstr < end) {
    const char *next = cstr;
    while(next < end && *next != '\n')
      ++next;
    
    Str_write((Str){cstr, next - cstr});
    if(next < end) {
      ++next;
      Str_write(context->nl);
      Str_write(context->indent_prefix);
    }
    cstr = next;
  }
}

static void write_output_locked_callback(void *_context) {
  WriteOutputCtx *context = _context;
  
  pmath_cstr_writer_info_t info = { ._pmath_write_cstr = write_cstr, .user = context };
  
  Str_write(context->indent_prefix);
  Str_write(context->indent_more);
  
  int indent_length = (context->indent_more.len < (size_t)console_width / 2)
    ? (int)context->indent_more.len
    : console_width / 2;
  
  if(indent_length > console_width / 2)
    indent_length = console_width / 2;
  
  pmath_write_with_pagewidth(
    context->object,
    0,
    pmath_native_writer,
    &info,
    console_width - indent_length,
    indent_length);
  
  Str_write(context->nl);
}

static void write_output(Str indent, pmath_t obj) {
  WriteOutputCtx context;
  
  CritSect_enter(output.critical_section);
  context.object        = obj;
  context.nl            = output.current_line_ending;
  context.indent_prefix = output.current_prompt_prefix;
  context.indent_more   = indent;
  CritSect_exit(output.critical_section);
  
  pmath_thread_call_locked(
    &print_lock,
    write_output_locked_callback,
    &context);
}

//} ... Synchronized output (line wise)

typedef struct ParseData {
  pmath_string_t code;
  pmath_string_t filename;
  int start_line;
  pmath_bool_t error;
} ParseData;

static void scanner_error(pmath_string_t code, int pos, void *_data, pmath_bool_t critical) {
  ParseData *data = _data;
  
  if(!data->error)
    pmath_message_syntax_error(code, pos, PMATH_NULL, 0);
    
  if(critical)
    data->error = TRUE;
}

static pmath_t add_debug_metadata(
  pmath_t                             token_or_span, 
  const struct pmath_text_position_t *start, 
  const struct pmath_text_position_t *end, 
  void                               *_data
) {
  pmath_t debug_metadata;
  ParseData *data = _data;
  
  assert(0 <= start->index);
  assert(start->index <= end->index);
  assert(end->index <= pmath_string_length(data->code));
  
  if(!pmath_is_expr(token_or_span) && !pmath_is_string(token_or_span))
    return token_or_span;
    
  int startline = start->line + data->start_line;
  int startcol  = start->index - start->line_start_index;
  
  int endline   = end->line + data->start_line;
  int endcol    = end->index - end->line_start_index;
  
  debug_metadata = pmath_language_new_file_location(
                     pmath_ref(data->filename), 
                     startline, startcol, 
                     endline, endcol);
                   
  return pmath_try_set_debug_metadata(token_or_span, debug_metadata);
}

static pmath_bool_t is_valid_prompt_prefix(Str text) {
  text = Str_trim_spaces(text);
  
  return Str_eq(text, S(""))
      || Str_eq(text, S("%"))
      || Str_eq(text, S("%%"))
      || Str_eq(text, S("%%%"))
      || Str_eq(text, S("//"))
      || Str_eq(text, S("///"))
      || Str_eq(text, S("//!"));
}

static pmath_string_t Str_to_pmath(Str more) {
  if(more.len > INT_MAX)
    return PMATH_NULL;
  
  return pmath_string_from_utf8(more.s, (int)more.len);
}

static int count_line_breaks(Str text) {
  int count = 0;
  Str nl    = Str_find_next_nl_ref(text);
  while(nl.len > 0) {
    count += 1;
    text   = Str_after_ref(text, nl);
    nl     = Str_find_next_nl_ref(text);
  }
  return count;
}

typedef struct {
  pmath_string_t filename;
  int            lines_before;
  Str            remaining;
} InputCtx;

static void run_next_prompt(InputCtx *_ctx) {
  InputCtx *ctx = _ctx;

  ParseData parse_data = {
    .code       = PMATH_STATIC_NULL,
    .filename   = ctx->filename,
    .start_line = ctx->lines_before,
    .error      = FALSE,
  };
  
  struct pmath_boxes_from_spans_ex_t parse_settings = {
    .size           = sizeof(parse_settings),
    .flags          = PMATH_BFS_PARSEABLE,
    .data           = &parse_data,
    .add_debug_metadata = add_debug_metadata,
  };
  
  Str prompt_ref        = Str_find_ref(  ctx->remaining, PROMPT);
  Str before_prompt_ref = Str_before_ref(ctx->remaining, prompt_ref);
  Str after_prompt_ref  = Str_after_ref( ctx->remaining, prompt_ref);
  
  Str prompt_prefix     = Str_find_last_line_ref(before_prompt_ref);
  
  Str nl_ref            = Str_find_next_nl_ref(after_prompt_ref);
  Str after_line_ref    = Str_after_ref(ctx->remaining, nl_ref);
  Str skipped           = Str_before_ref(ctx->remaining, after_line_ref);
  
  Str_write(skipped);
  ctx->remaining = after_line_ref;
  
  // TODO: use new output content instead?
  parse_data.start_line += count_line_breaks(skipped);
  
  if(prompt_ref.len > 0 && is_valid_prompt_prefix(prompt_prefix)) {
    CritSect_enter(output.critical_section);
    output.current_line_ending   = nl_ref;
    output.current_prompt_prefix = prompt_prefix;
    CritSect_exit(output.critical_section);
    
    parse_data.code = Str_to_pmath(Str_before_ref(after_prompt_ref, nl_ref));
    
    int extra_lines = 0;
    // collect continuation lines
    while(Str_starts_with(             ctx->remaining, prompt_prefix) 
    &&    Str_starts_with(Str_drop_ref(ctx->remaining, prompt_prefix.len), PROMPT_MORE)) {
      after_prompt_ref =  Str_drop_ref(ctx->remaining, prompt_prefix.len + PROMPT_MORE.len);
      nl_ref           = Str_find_next_nl_ref(after_prompt_ref);
      after_line_ref   = Str_after_ref(ctx->remaining, nl_ref);
      
      Str_write(Str_before_ref(ctx->remaining, after_line_ref));
      ctx->remaining = after_line_ref;
      
      ++extra_lines;
      parse_data.code = pmath_string_insert_latin1(parse_data.code, INT_MAX, "\n", 1);
      parse_data.code = pmath_string_concat(parse_data.code, Str_to_pmath(Str_before_ref(after_prompt_ref, nl_ref)));
    }
    
    int skipped_lines = 0;
    // skip old output lines
    while(Str_starts_with(ctx->remaining, prompt_prefix)) {
      Str after_prefix = Str_drop_ref(ctx->remaining, prompt_prefix.len);
      if(after_prefix.len < 1 || (after_prefix.s[0] != ' ' && after_prefix.s[0] != '\t'))
        break;
      
      nl_ref = Str_find_next_nl_ref(after_prefix);
      ctx->remaining = Str_after_ref(ctx->remaining, nl_ref);
      ++skipped_lines;
    }
    
    parse_data.error = FALSE;
    pmath_span_array_t *spans = pmath_spans_from_string(
      &parse_data.code, NULL, NULL, NULL, scanner_error, &parse_data);
    
    if(!parse_data.error) {
      pmath_t debug_metadata;
      pmath_t obj = pmath_boxes_from_spans_ex(
                      spans,
                      parse_data.code,
                      &parse_settings);
      
      debug_metadata = pmath_get_debug_metadata(obj);
      
      obj = pmath_evaluate(
              pmath_expr_new_extended(
                pmath_ref(pmath_System_MakeExpression), 1,
                obj));
      
      if(pmath_is_expr_of(obj, pmath_System_HoldComplete)) {
        if(pmath_expr_length(obj) == 1) {
          pmath_t tmp = obj;
          obj = pmath_expr_get_item(tmp, 1);
          pmath_unref(tmp);
        }
        else {
          obj = pmath_expr_set_item(
                  obj, 0, pmath_ref(pmath_System_Sequence));
        }
        
        obj = pmath_try_set_debug_metadata(obj, debug_metadata);
        obj = pmath_session_execute(obj, NULL);
      }
      else
        pmath_unref(debug_metadata);
        
      if(!quitting && !pmath_is_null(obj)) {
        write_output(OUTPUT_INDENT, obj);
      }
      
      pmath_unref(obj);
    }
    
    parse_data.start_line += extra_lines + skipped_lines;
    
    pmath_unref(parse_data.code); parse_data.code = PMATH_NULL;
    pmath_span_array_free(spans);
  }

  ctx->lines_before = parse_data.start_line;
}

static void run_all_input(void) {
  InputCtx ctx = { 
    .filename     = input_file_name,
    .lines_before = 0,
    .remaining    = input_file.remaining, 
  };
  
  while(ctx.remaining.len > 0) {
    Str old = ctx.remaining;
    run_next_prompt(&ctx);
    
    if(ctx.remaining.len >= old.len)
      break; // Ensure forward progress, should not happen
  }
  
  Str_write(ctx.remaining);
}

static pmath_bool_t handle_options(int argc, const char **argv) {
  pmath_bool_t success = TRUE;
  pmath_bool_t show_help = FALSE;
  pmath_bool_t need_file = TRUE;
  
  --argc;
  ++argv;
  while(argc > 0) {
    if(**argv == '-') {
      if((strcmp(*argv, "-x") == 0 || strcmp(*argv, "--exec") == 0) &&
              argc > 1)
      {
        --argc;
        ++argv;
        
        pmath_unref(
          pmath_evaluate(
            pmath_parse_string(
              pmath_string_from_native(*argv, -1))));
      }
      else {
        if(strcmp(*argv, "--help") != 0) {
          success = FALSE;
          fprintf(stderr, "Unknown option %s\n", *argv);
        }
        
        show_help = TRUE;
        quitting = TRUE;
        show_mem_stats = FALSE;
        break;
      }
    }
    else if(need_file) {
      need_file = FALSE;
      
      input_file_path = *argv;
    }
    else {
      fprintf(stderr, "Excessive parameter %s\n", *argv);
      quitting = TRUE;
      success = FALSE;
      show_mem_stats = FALSE;
      break;
    }
    
    ++argv;
    --argc;
  }
  
  if(need_file) {
    fprintf(stderr, "File name expected\n");
    success = FALSE;
    quitting = TRUE;
    show_mem_stats = FALSE;
  }
  
  if(show_help) {  
    fprintf(stderr, "\nPossible options are:\n"
            "    -q, --quit           Exit after processing the command line options.\n"
            "    -x, --exec CMD       Evaluate a pMath expression\n\n");
  }
  
  return success;
}

static void signal_term(int sig) {
  quitting = TRUE;
  pmath_abort_please();
}

// style will be freed
static Str convert_style_to_indent(pmath_t style) {
  if(pmath_string_equals_latin1(style, "Echo")) {
    pmath_unref(style);
    return ECHO_INDENT;
  }
  
  pmath_unref(style);
  return PRINT_INDENT;
}

static pmath_t builtin_sectionprint(pmath_expr_t expr) {
  size_t exprlen = pmath_expr_length(expr);
  
  if(exprlen == 0)
    return expr;
  
  if(exprlen == 1) {
    pmath_t sections = pmath_expr_get_item(expr, 1);
    size_t i;
    
    if(!pmath_is_expr_of(sections, pmath_System_List))
      sections = pmath_expr_new_extended(pmath_ref(pmath_System_List), 1, sections);
    
    for(i = 1; i <= pmath_expr_length(sections); ++i) {
      pmath_t item = pmath_expr_get_item(sections, i);
      Str indent = PRINT_INDENT;
      
      if(pmath_is_expr_of(item, pmath_System_Section)) {
        pmath_t boxes = pmath_expr_get_item(item, 1);
        indent = convert_style_to_indent(pmath_expr_get_item(item, 2));
        
        pmath_unref(item);
        if(pmath_is_expr_of(boxes, pmath_System_BoxData)) 
          item = pmath_expr_set_item(boxes, 0, pmath_ref(pmath_System_RawBoxes));
        else
          item = pmath_expr_new_extended(pmath_ref(pmath_System_RawBoxes), 1, boxes);
      }
      
      write_output(indent, item);
      
      pmath_unref(item);
      Str_write_prefixed_line(S(" "));
    }
    
    pmath_unref(sections);
    pmath_unref(expr);
    expr = PMATH_NULL;
  }
  else if(exprlen == 2) {
    Str indent = convert_style_to_indent(pmath_expr_get_item(expr, 1));
    pmath_t item = pmath_expr_get_item(expr, 2);
    
    pmath_unref(expr);
    expr = PMATH_NULL;
    
    write_output(indent, item);
    pmath_unref(item);
  }
  else {
    Str indent = convert_style_to_indent(pmath_expr_get_item(expr, 1));
    pmath_t row = pmath_expr_get_item_range(expr, 2, SIZE_MAX);
    
    pmath_unref(expr);
    expr = PMATH_NULL;
    
    row = pmath_expr_set_item(row, 0, pmath_ref(pmath_System_List));
    row = pmath_expr_new_extended(pmath_ref(pmath_System_Row), 1, row);
    
    write_output(indent, row);
    pmath_unref(row);
  }
  
  Str_write_prefixed_line(S(" "));
  return expr;
}

static pmath_t builtin_trap(pmath_expr_t expr) {
  write_output(S(" TRAP ON "), expr);
  pmath_unref(expr);
  pmath_abort_please();
  return PMATH_NULL;
}

static pmath_bool_t init_pmath_bindings() {
#define X( SYM, NAME )  SYM = pmath_symbol_get(PMATH_C_STRING( NAME ), FALSE);
  PMATH_SYMBOLS_X
#undef X
  
#define X( SYM, NAME )  !pmath_is_null( SYM ) &&
  return PMATH_SYMBOLS_X
         pmath_register_code(pmath_System_OpenAppend,   builtin_trap, 0) &&
         pmath_register_code(pmath_System_OpenWrite,    builtin_trap, 0) &&
         pmath_register_code(pmath_System_SectionPrint, builtin_sectionprint, 0);
#undef X
}

static void done_pmath_bindings(void) {
  // FIXME: race condition when another thread still runs
#define X( SYM, NAME )  pmath_unref( SYM ); SYM = PMATH_NULL;
  PMATH_SYMBOLS_X
#undef X
}

int main(int argc, const char **argv) {
  os_init();
  
  signal(SIGINT, signal_term);
  #ifdef SIGBREAK
  signal(SIGBREAK, signal_term);
  #endif
  signal(SIGTERM, signal_term);
  
  CritSect_init(output.critical_section);
  
  if(!pmath_init() || !init_pmath_bindings()) {
    fprintf(stderr, "Cannot initialize pMath.\n");
    CritSect_destroy(output.critical_section);
    return 1;
  }
  
  if(!handle_options(argc, argv)) {
    done_pmath_bindings();
    pmath_done();
    CritSect_destroy(output.critical_section);
    return 1;
  }
  
  int quit_result = 0;
  input_file_name = PMATH_NULL;
  if(input_file_path) {
    input_file_name = pmath_string_from_native(input_file_path, -1);
    
    if(!MappedFile_open(&input_file, input_file_path)) {
      fprintf(stderr, "Cannot read file %s\n", input_file_path);
      quit_result = 1;
      quitting = TRUE;
    }
  }
  
  PMATH_RUN_ARGS("$Input:=`1`", "(o)", pmath_ref(input_file_name));
  
#ifdef PMATH_OS_WIN32
  output.current_line_ending = S("\r\n");
#else
  output.current_line_ending = S("\n");
#endif
  output.current_prompt_prefix = S("");
  
  StringBuffer_ensure_capacity(output.content, input_file.all.len + 1000);
  
  if(!quitting) {
    run_all_input();
    pmath_continue_after_abort();
  }

  pmath_unref(input_file_name);
  done_pmath_bindings();
  pmath_done();
  
  { // Check memory usage
    CritSect_enter(output.critical_section);
    Str nl = output.current_line_ending;
    CritSect_exit(output.critical_section);
    
    size_t current, max;
    pmath_mem_usage(&current, &max);
    if(show_mem_stats || current != 0) {
      char line[100];
      if(current != 0) {
        snprintf(line, sizeof(line), "Memory: %"PRIuPTR" (should be 0)", current);
        fprintf(stderr, "\n%s\n", line);
        Str_write(nl);
        Str_write_prefixed_line((Str){line, strlen(line)});
      }
      
      snprintf(line, sizeof(line), "Maximum memory usage: %"PRIuPTR"", max);
      fprintf(stderr, "%s\n", line);
      if(current != 0) {
        Str_write_prefixed_line((Str){line, strlen(line)});
      }
    }
  }
  
  if(input_file_path) {
    CritSect_enter(output.critical_section);
    
    Str all_output = StringBuffer_as_Str(output.content);
    pmath_bool_t different = !Str_eq(all_output, input_file.all);
    
    MappedFile_close(&input_file);
    
    if(different && !quitting) {
      fprintf(stderr, "Different!\n");
      
      FILE *f = fopen(input_file_path, "wb");
      if(f) {
        fwrite(all_output.s, 1, all_output.len, f);
        fclose(f);
      }
      else {
        fprintf(stderr, "Failed to update %s\n", input_file_path);
      }
      
      quit_result = 1;
    }
    else {
      fprintf(stderr, "No changes.\n");
    }
    
    CritSect_exit(output.critical_section);
  }
  
  StringBuffer_free(output.content);
  CritSect_destroy(output.critical_section);
  
  // Self test:
  // pmath> 1+1
  //        2
  // Thats it!
  return quit_result;
}
