#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pmath.h>

#ifdef pmath_debug_print_stack
#  undef pmath_debug_print_stack
#endif

static void os_init(void);

#ifdef PMATH_OS_WIN32
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

typedef HANDLE sem_t;

static int sem_init(sem_t *sem, int pshared, unsigned int value) {
  *sem = CreateSemaphore(0, value, 0x7FFFFFFF, 0);
  return *sem == 0 ? -1 : 0;
}

static int sem_destroy(sem_t *sem) {
  return CloseHandle(*sem) ? 0 : -1;
}

static int sem_wait(sem_t *sem) {
  errno = 0;
  return WaitForSingleObject(*sem, INFINITE) == WAIT_OBJECT_0 ? 0 : -1;
}

static int sem_post(sem_t *sem) {
  return ReleaseSemaphore(*sem, 1, 0) ? 0 : -1;
}

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
#elif defined(PMATH_OS_UNIX)
#  include <unistd.h>
#  include <semaphore.h>

static void os_init(void) {

}
#endif

static int console_width = 80;
static volatile int dialog_depth = 0;
static volatile int quit_result = 0;
static volatile pmath_bool_t quitting = FALSE;
static volatile pmath_bool_t show_mem_stats = TRUE;

static sem_t interrupt_semaphore;
static volatile pmath_messages_t main_mq;
static pmath_atomic_t main_mq_lock = PMATH_ATOMIC_STATIC_INIT;

static pmath_messages_t get_main_mq(void) {
  pmath_messages_t mq;
  
  pmath_atomic_lock(&main_mq_lock);
  {
    mq = pmath_ref(main_mq);
  }
  pmath_atomic_unlock(&main_mq_lock);
  
  return mq;
}

// Reads a line from file without the ending "\n".
static pmath_string_t read_line(FILE *file) {
  pmath_string_t result = PMATH_NULL;
  char buf[512];
  
  while(fgets(buf, sizeof(buf), file) != NULL) {
    int len = (int)strlen(buf);
    
    if(buf[len - 1] == '\n') {
      if(len == 1)
        return pmath_string_new(0);
        
      return pmath_string_concat(result, pmath_string_from_native(buf, len - 1));
    }
    
    result = pmath_string_concat(result, pmath_string_from_native(buf, len));
  }
  
  return result;
}


static size_t bytes_since_last_abortcheck = 0;

static void _pmath_write_cstr(FILE *file, const char *cstr) {
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
    
  fwrite(cstr, 1, len, file);
}

static pmath_threadlock_t print_lock = NULL;

struct write_output_t {
  pmath_t object;
  const char *indent;
};

static void write_output_locked_callback(void *_context) {
  pmath_cstr_writer_info_t info;
  struct write_output_t *context = _context;
  int indent_length;
  
  info.user = stdout;
  info._pmath_write_cstr = (void ( *)(void *, const char *))_pmath_write_cstr;
  
  indent_length = dialog_depth;
  while(indent_length-- > 0)
    printf(" ");
  printf("%s", context->indent);
  
  indent_length = 0;
  while(indent_length <= console_width / 2 && context->indent[indent_length])
    ++indent_length;
  
  indent_length+= dialog_depth;
  
  pmath_write_with_pagewidth(
    context->object,
    0,
    pmath_native_writer,
    &info,
    console_width - indent_length,
    indent_length);
  printf("\n");
  fflush(stdout);
}

static void write_line_locked_callback(void *s) {
  printf("%s", (const char *)s);
  fflush(stdout);
}

static void indent_locked_callback(void *s) {
  int i = dialog_depth;
  while(i-- > 0)
    printf(" ");
  printf("%s", (const char *)s);
  fflush(stdout);
}

static void write_output(const char *indent, pmath_t obj) {
  struct write_output_t context;
  
  context.object = obj;
  context.indent = indent ? indent : "";
  
  pmath_thread_call_locked(
    &print_lock,
    write_output_locked_callback,
    &context);
}

static void write_line(const char *s) {
  pmath_thread_call_locked(
    &print_lock,
    write_line_locked_callback,
    (void *)s);
}

static void write_indent(const char *s) {
  pmath_thread_call_locked(
    &print_lock,
    indent_locked_callback,
    (void *)s);
}

static void signal_dummy(int sig) {
  signal(sig, signal_dummy);
}

static void signal_handler(int sig) {
  int old_errno;
  
  signal(sig, signal_dummy);
  pmath_suspend_all_please();
  
  old_errno = errno;
  sem_post(&interrupt_semaphore);
  
  signal(sig, signal_handler);
  
  errno = old_errno;
  pmath_resume_all();
}

static void signal_term(int sig) {
  quitting = TRUE;
  pmath_abort_please();
}

static void interrupt_daemon(void *dummy) {
  pmath_messages_t mq;
  
  while(!quitting) {
    sem_wait(&interrupt_semaphore);
    
    if(quitting)
      break;
      
    mq = get_main_mq();
    pmath_thread_send(
      mq,
      pmath_expr_new(pmath_ref(PMATH_SYMBOL_INTERRUPT), 0));
    pmath_unref(mq);
  }
}

static void kill_interrupt_daemon(void *dummy) {
  quitting = TRUE;
  sem_post(&interrupt_semaphore);
}

struct parse_data_t {
  pmath_t code;
  pmath_t filename;
  int start_line;
  int numlines;
  unsigned error: 1;
};

static pmath_string_t scanner_read(void *_data) {
  struct parse_data_t *data = _data;
  pmath_string_t result;
  
  if(pmath_aborting())
    return PMATH_NULL;
    
  write_indent("     > ");
  
  result = read_line(stdin);
  if(pmath_string_length(result) == 0) {
    pmath_unref(result);
    return PMATH_NULL;
  }
  
  data->numlines++;
  return result;
}

static void scanner_error(pmath_string_t code, int pos, void *_data, pmath_bool_t critical) {
  struct parse_data_t *data = _data;
  
  if(!data->error)
    pmath_message_syntax_error(code, pos, PMATH_NULL, 0);
    
  if(critical)
    data->error = TRUE;
}

static void handle_options(int argc, const char **argv) {
  --argc;
  ++argv;
  while(argc > 0) {
    if(strcmp(*argv, "-q") == 0 || strcmp(*argv, "--quit") == 0) {
      quitting = TRUE;
      show_mem_stats = FALSE;
    }
    else if((strcmp(*argv, "-l") == 0 || strcmp(*argv, "--load") == 0) &&
            argc > 1)
    {
      --argc;
      ++argv;
      
      PMATH_RUN_ARGS("Get(`1`)", "(o)", pmath_string_from_native(*argv, -1));
    }
    else if((strcmp(*argv, "-x") == 0 || strcmp(*argv, "--exec") == 0) &&
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
        quit_result = 1;
        fprintf(stderr, "Unknown option %s\n", *argv);
      }
      
      fprintf(stderr, "\nPossible options are:\n"
              "    -l, --load FILENAME  Load a pMath script and execute it.\n"
              "    -q, --quit           Exit after processing the command line options.\n"
              "    -x, --exec CMD       Evaluate a pMath expression\n\n");
      quitting = TRUE;
      show_mem_stats = FALSE;
      return;
    }
    
    ++argv;
    --argc;
  }
}

static pmath_t check_dialog_return(pmath_t result) { // result wont be freed
  if( pmath_is_expr_of(result, PMATH_SYMBOL_RETURN) &&
      pmath_expr_length(result) <= 1)
  {
    return pmath_expr_get_item(result, 1);
  }
  
  return PMATH_UNDEFINED;
}

static pmath_t add_debug_info(
  pmath_t                             token_or_span, 
  const struct pmath_text_position_t *start, 
  const struct pmath_text_position_t *end, 
  void                               *_data
) {
  pmath_t debug_info;
  struct parse_data_t *data = _data;
  int start_line, end_line, start_column, end_column;
  
  assert(0 <= start->index);
  assert(start->index <= end->index);
  assert(end->index <= pmath_string_length(data->code));
  
  if(!pmath_is_expr(token_or_span))
    return token_or_span;
    
  start_line   = start->line + data->start_line;
  start_column = start->index - start->line_start_index;
  
  end_line   = end->line + data->start_line;
  end_column = end->index - end->line_start_index;
  
  debug_info = pmath_expr_new_extended(
                 pmath_ref(PMATH_SYMBOL_DEVELOPER_DEBUGINFOSOURCE), 2,
                 pmath_ref(data->filename),
                 pmath_expr_new_extended(
                   pmath_ref(PMATH_SYMBOL_RANGE), 2,
                   pmath_build_value("(ii)", start_line, start_column),
                   pmath_build_value("(ii)", end_line,   end_column)));
                   
  return pmath_try_set_debug_info(token_or_span, debug_info);
}

static pmath_t dialog(pmath_t first_eval) {
  pmath_t result = PMATH_NULL;
  pmath_t old_dialog = pmath_session_start();
  
  first_eval = pmath_evaluate(first_eval);
  result = check_dialog_return(first_eval);
  pmath_unref(first_eval);
  
  PMATH_RUN_ARGS("$PageWidth:=`1`", "(i)", console_width - (7 + dialog_depth));
  
  if(pmath_same(result, PMATH_UNDEFINED)) {
    struct pmath_boxes_from_spans_ex_t parse_settings;
    struct parse_data_t                parse_data;
    
    memset(&parse_settings, 0, sizeof(parse_settings));
    parse_settings.size           = sizeof(parse_settings);
    parse_settings.flags          = PMATH_BFS_PARSEABLE;
    parse_settings.data           = &parse_data;
    parse_settings.add_debug_info = add_debug_info;
    
    parse_data.filename = PMATH_C_STRING("<input>");
    parse_data.start_line = 1;
    parse_data.numlines = 0;
    
    result = PMATH_NULL;
    while(!quitting) {
      pmath_span_array_t *spans;
      
      pmath_continue_after_abort();
      
      write_line("\n");
      write_indent("pmath> ");
      
      parse_data.start_line+= parse_data.numlines;
      parse_data.numlines = 1;
      parse_data.error = FALSE;
      parse_data.code = read_line(stdin);
      
      if(dialog_depth > 0 && pmath_aborting()) {
        pmath_unref(parse_data.code);
        break;
      }
      
      spans = pmath_spans_from_string(
                &parse_data.code,
                scanner_read,
                NULL,
                NULL,
                scanner_error,
                &parse_data);
                
      if(!parse_data.error) {
        pmath_t debug_info;
        pmath_t obj = pmath_boxes_from_spans_ex(
                        spans,
                        parse_data.code,
                        &parse_settings);
                        
        debug_info = pmath_get_debug_info(obj);
        
        obj = pmath_evaluate(
                pmath_expr_new_extended(
                  pmath_ref(PMATH_SYMBOL_MAKEEXPRESSION), 1,
                  obj));
                  
        if(pmath_is_expr_of(obj, PMATH_SYMBOL_HOLDCOMPLETE)) {
          if(pmath_expr_length(obj) == 1) {
            pmath_t tmp = obj;
            obj = pmath_expr_get_item(tmp, 1);
            pmath_unref(tmp);
          }
          else {
            obj = pmath_expr_set_item(
                    obj, 0, pmath_ref(PMATH_SYMBOL_SEQUENCE));
          }
          
          obj = pmath_try_set_debug_info(obj, debug_info);
          obj = pmath_session_execute(obj, NULL);
        }
        else
          pmath_unref(debug_info);
          
        if(!quitting && !pmath_is_null(obj)) {
          if(dialog_depth > 0) {
            result = check_dialog_return(obj);
            
            if(!pmath_same(result, PMATH_UNDEFINED)) {
              pmath_unref(obj);
              pmath_unref(parse_data.code);
              pmath_span_array_free(spans);
              
              break;
            }
            
            result = PMATH_NULL;
          }
          
          write_output("       ", obj);
        }
        
        pmath_unref(obj);
      }
      
      pmath_unref(parse_data.code);
      pmath_span_array_free(spans);
    }
    
    pmath_unref(parse_data.filename);
  }
  
  PMATH_RUN_ARGS("$PageWidth:=`1`", "(i)", console_width - (7 + dialog_depth - 1));
  
  pmath_session_end(old_dialog);
  return result;
}

static pmath_threadlock_t dialog_lock = NULL;

struct dialog_callback_info_t {
  pmath_t result;
  pmath_t first_eval;
};

static void dialog_callback(void *_info) {
  struct dialog_callback_info_t *info = (struct dialog_callback_info_t *)_info;
  
  ++dialog_depth;
  
  info->result = dialog(info->first_eval);
  info->first_eval = PMATH_NULL;
  
  --dialog_depth;
}

static pmath_t builtin_dialog(pmath_expr_t expr) {
  struct dialog_callback_info_t info;
  
  if(pmath_expr_length(expr) > 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 0, 0);
    return expr;
  }
  
  info.result = PMATH_NULL;
  info.first_eval = PMATH_NULL;
  if(pmath_expr_length(expr) == 1) {
    info.first_eval = pmath_expr_get_item(expr, 1);
  }
  
  pmath_unref(expr);
  
  pmath_thread_call_locked(
    &dialog_lock,
    dialog_callback,
    &info);
    
  pmath_unref(info.first_eval);
  return info.result;
}

static pmath_string_t next_word(pmath_string_t *line) {
  const uint16_t *buf = pmath_string_buffer(line);
  int             len = pmath_string_length(*line);
  pmath_string_t word;
  int i;
  
  for(i = 0; i < len && buf[i] > ' '; ++i)
    ++i;
    
  word = pmath_string_part(pmath_ref(*line), 0, i);
  
  for(i = 0; i < len && buf[i] <= ' '; ++i)
    ++i;
    
  *line = pmath_string_part(*line, i, INT_MAX);
  return word;
}

static pmath_threadlock_t interrupt_lock = NULL;

static void interrupt_callback(void *dummy) {
  pmath_string_t line = PMATH_NULL;
  pmath_string_t word = PMATH_NULL;
  
  pmath_suspend_all_please();
  
  while(!quitting/* && !pmath_aborting()*/) {
    pmath_unref(line);
    pmath_unref(word);
    
    write_line("\ninterrupt: ");
    
    line = read_line(stdin);
    word = next_word(&line);
    
    if( pmath_string_equals_latin1(word, "a") ||
        pmath_string_equals_latin1(word, "abort"))
    {
      write_line("aborting...\n");
      pmath_abort_please();
      break;
    }
    
    if( pmath_string_equals_latin1(word, "bt") ||
        pmath_string_equals_latin1(word, "backtrace"))
    {
      pmath_debug_print_stack();
      continue;
    }
    
    if( pmath_string_equals_latin1(word, "c") ||
        pmath_string_equals_latin1(word, "continue"))
    {
      write_line("continuing...\n");
      break;
    }
    
    if( pmath_string_equals_latin1(word, "i") ||
        pmath_string_equals_latin1(word, "inspect"))
    {
      pmath_messages_t mq ;
      
      write_line("entering interactive dialog (finish with `Return()`) ...\n");
      
      mq = get_main_mq();
      pmath_thread_send(
        mq,
        pmath_expr_new(pmath_ref(PMATH_SYMBOL_DIALOG), 0));
      pmath_unref(mq);
      break;
    }
    
    if(pmath_string_length(word) > 0) {
      write_output("unknown command: ", word);
    }
    
    write_line(
      "possible commands are:\n"
      "  a, abort       Abort the current evaluation.\n"
      "  bt, backtrace  Show the current evaluation stack.\n"
      "  c, continue    Continue the current evaluation.\n"
      "  i, inspect     Enter an interactive dialog and continue afterwards.\n");
  }
  
  pmath_unref(line);
  pmath_unref(word);
  
  write_line("\n");
  
  pmath_resume_all();
}

static pmath_t builtin_interrupt(pmath_expr_t expr) {
  if(pmath_expr_length(expr) > 0) {
    pmath_message_argxxx(pmath_expr_length(expr), 0, 0);
    return expr;
  }
  
  if(pmath_thread_queue_is_blocked_by(pmath_ref(main_mq), pmath_thread_get_queue())) {
    /* Already in main thread or the main thread is waiting on the current
       thread (e.g. through pmath_task_wait()) */
    
    pmath_unref(expr);
    
    pmath_thread_call_locked(
      &interrupt_lock,
      interrupt_callback,
      NULL);
  }
  else {
    // in another thread => send to main thread
    
    pmath_messages_t mq = get_main_mq();
    pmath_thread_send(mq, expr);
    pmath_unref(mq);
  }
  
  return PMATH_NULL;
}

static pmath_t builtin_quit(pmath_expr_t expr) {
  if(pmath_expr_length(expr) == 1) {
    pmath_t res = pmath_expr_get_item(expr, 1);
    
    if(!pmath_is_int32(res)) {
      pmath_unref(res);
      pmath_message(PMATH_NULL, "intm", 2, PMATH_FROM_INT32(1), pmath_ref(expr));
      return expr;
    }
    
    quit_result = PMATH_AS_INT32(res);
    pmath_unref(res);
  }
  else if(pmath_expr_length(expr) != 0) {
    pmath_message_argxxx(pmath_expr_length(expr), 0, 1);
    return expr;
  }
  
  quitting = TRUE;
  pmath_abort_please();
  
  pmath_unref(expr);
  return PMATH_NULL;
}

static pmath_t builtin_sectionprint(pmath_expr_t expr) {
  pmath_t style;
  const char *indent = "";
  
  if(pmath_expr_length(expr) < 2)
    return expr;
  
  style = pmath_expr_get_item(expr, 1);
  if(pmath_is_string(style) && pmath_string_equals_latin1(style, "Echo")) {
    indent = ">> ";
  }
  pmath_unref(style);
  
  if(pmath_expr_length(expr) == 2) {
    pmath_t item = pmath_expr_get_item(expr, 2);
    pmath_unref(expr);
    
    write_output(indent, item);
    pmath_unref(item);
  }
  else {
    pmath_t row = pmath_expr_get_item_range(expr, 2, SIZE_MAX);
    pmath_unref(expr);
    
    row = pmath_expr_set_item(row, 0, pmath_ref(PMATH_SYMBOL_LIST));
    row = pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_ROW), 1, row);
    
    write_output(indent, row);
    pmath_unref(row);
  }
  
  write_line("\n");
  return PMATH_NULL;
}

static void init_console_width(void) {
  pmath_t pw = pmath_evaluate(pmath_ref(PMATH_SYMBOL_PAGEWIDTHDEFAULT));
  
  if(pmath_is_int32(pw))
    console_width = PMATH_AS_INT32(pw) - 1;
    
  pmath_unref(pw);
}

int main(int argc, const char **argv) {
  main_mq = PMATH_NULL;
  
  os_init();
  
  if(sem_init(&interrupt_semaphore, 0, 0) < 0) {
    fprintf(stderr, "Out of System resoures (sem_init failed).\n");
    return 1;
  }
  
  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_term);
  
  if( !pmath_init() ||
      !pmath_register_code(PMATH_SYMBOL_DIALOG,       builtin_dialog,       0) ||
      !pmath_register_code(PMATH_SYMBOL_INTERRUPT,    builtin_interrupt,    0) ||
      !pmath_register_code(PMATH_SYMBOL_QUIT,         builtin_quit,         0) ||
      !pmath_register_code(PMATH_SYMBOL_SECTIONPRINT, builtin_sectionprint, 0))
  {
    fprintf(stderr, "Cannot initialize pMath.\n");
    return 1;
  }
  
  init_console_width();
  
  pmath_unref(
    pmath_thread_fork_daemon(
      interrupt_daemon,
      kill_interrupt_daemon,
      NULL));
      
  main_mq = pmath_thread_get_queue();
  
  handle_options(argc, argv);
  
  if(!quitting) {
    write_line("Welcome to pMath.\n"
               "Type `??symbol` to get Help about a symbol. Exit with `Quit()`.\n");
  }
  
  pmath_unref(dialog(PMATH_NULL));
  pmath_continue_after_abort();
  
  { // freeing main_mq
    pmath_t mq;
    
    pmath_atomic_lock(&main_mq_lock);
    {
      mq = main_mq;
      main_mq = PMATH_NULL;
    }
    pmath_atomic_unlock(&main_mq_lock);
    
    pmath_unref(mq);
  }
  
  pmath_done();
  
  {
    size_t current, max;
    pmath_mem_usage(&current, &max);
    if(show_mem_stats || current != 0) {
      printf("memory: %"PRIuPTR" (should be 0)\n", current);
      printf("max. used: %"PRIuPTR"\n", max);
    }
  }
  
  signal(SIGINT, signal_dummy);
  sem_destroy(&interrupt_semaphore);
  
  return quit_result;
}
