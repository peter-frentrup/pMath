#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pmath.h>

#ifdef PMATH_OS_WIN32
  #include <io.h>
  #define dup    _dup
  #ifdef fileno
    #undef fileno
  #endif
  #define fileno _fileno
  #define fdopen _fdopen
  
  #define NOGDI
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>

  typedef HANDLE sem_t;

  static int sem_init(sem_t *sem, int pshared, unsigned int value){
    *sem = CreateSemaphore(0, value, 0x7FFFFFFF, 0);
    return *sem == 0 ? -1 : 0;
  }

  static int sem_destroy(sem_t *sem){
    return CloseHandle(*sem) ? 0 : -1;
  }

  static int sem_wait(sem_t *sem){
    errno = 0;
    return WaitForSingleObject(*sem, INFINITE) == WAIT_OBJECT_0 ? 0 : -1;
  }

  static int sem_post(sem_t *sem){
    return ReleaseSemaphore(*sem, 1, 0) ? 0 : -1;
  }
#elif defined(PMATH_OS_UNIX)
  #include <unistd.h>
  #include <semaphore.h>
#endif

static volatile int dialog_depth = 0;
static volatile int quit_result = 0;
static volatile pmath_bool_t quitting = FALSE;
static volatile pmath_bool_t show_mem_stats = TRUE;

static sem_t interrupt_semaphore;
static volatile pmath_messages_t main_mq       = 0;
static PMATH_DECLARE_ATOMIC(     main_mq_lock) = 0;

static pmath_messages_t get_main_mq(void){
  pmath_messages_t mq;
  
  pmath_atomic_lock(&main_mq_lock);
  {
    mq = pmath_ref(main_mq);
  }
  pmath_atomic_unlock(&main_mq_lock);
  
  return main_mq;
}

// Reads a line from file without the ending "\n".
static pmath_string_t read_line(FILE *file){
  pmath_string_t result = NULL;
  char buf[512];
  
  while(fgets(buf, sizeof(buf), file) != NULL){
    int len = strlen(buf);
    
    if(buf[len-1] == '\n'){
      if(len == 1)
        return pmath_string_new(0);
      
      return pmath_string_concat(result, pmath_string_from_native(buf, len-1));
    }
    
    result = pmath_string_concat(result, pmath_string_from_native(buf, len));
  }
  
  return result;
}

static void write_cstr(FILE *file, const char *cstr){
  fwrite(cstr, 1, strlen(cstr), file);
}

  static pmath_threadlock_t print_lock = NULL;
  
  static void write_output_locked_callback(void *_obj){
    pmath_cstr_writer_info_t info;
    pmath_t obj = (pmath_t)_obj;
    int i;
    
    info.user = stdout;
    info.write_cstr = (void (*)(void*, const char*))write_cstr;
    
    i = dialog_depth;
    while(i-- > 0)
      printf(" ");
    printf("       "); 
    
    pmath_write(obj, 0, pmath_native_writer, &info);
    printf("\n");
    fflush(stdout);
  }

  static void write_line_locked_callback(void *s){
    printf("%s", (const char*)s);
    fflush(stdout);
  }

  static void indent_locked_callback(void *s){
    int i = dialog_depth;
    while(i-- > 0)
      printf(" ");
    printf("%s", (const char*)s);
    fflush(stdout);
  }

static void write_output(pmath_t obj){
  pmath_thread_call_locked(
    &print_lock,
    write_output_locked_callback,
    obj);
}

static void write_line(const char *s){
  pmath_thread_call_locked(
    &print_lock,
    write_line_locked_callback,
    (void*)s);
}

static void write_indent(const char *s){
  pmath_thread_call_locked(
    &print_lock,
    indent_locked_callback,
    (void*)s);
}

static void signal_dummy(int sig){
  signal(sig, signal_dummy);
}

static void signal_handler(int sig){
  int old_errno;
  
  signal(sig, signal_dummy);
  pmath_suspend_all_please();
  
  old_errno = errno;
  sem_post(&interrupt_semaphore);
  
  signal(sig, signal_handler);
  
  errno = old_errno;
  pmath_resume_all();
}

static void signal_term(int sig){
  quitting = TRUE;
  pmath_abort_please();
}

static void interrupt_daemon(void *dummy){
  pmath_messages_t mq;
  
  while(!quitting){
    sem_wait(&interrupt_semaphore);
    
    mq = get_main_mq();
    pmath_thread_send(
      mq, 
      pmath_expr_new(pmath_ref(PMATH_SYMBOL_INTERRUPT), 0));
    pmath_unref(mq);
  }
}

static void kill_interrupt_daemon(void *dummy){
  quitting = TRUE;
  sem_post(&interrupt_semaphore);
}

static pmath_string_t scanner_read(void *dummy){
  pmath_string_t result;
  
  if(pmath_aborting())
    return NULL;
  
  write_indent("     > ");
  
  result = read_line(stdin);
  if(pmath_string_length(result) == 0){
    pmath_unref(result);
    return NULL;
  }
  return result;
}

static void scanner_error(pmath_string_t code, int pos, void *flag, pmath_bool_t critical){
  if(critical)
    *(pmath_bool_t*)flag = TRUE;
  pmath_message_syntax_error(code, pos, NULL, 0);
}

static void handle_options(int argc, const char **argv){
  --argc;
  ++argv;
  while(argc > 0){
    if(strcmp(*argv, "-q") == 0 || strcmp(*argv, "--quit") == 0){
      quitting = TRUE;
      show_mem_stats = FALSE;
    }
    else if((strcmp(*argv, "-l") == 0 || strcmp(*argv, "--load") == 0)
    && argc > 1){
      --argc;
      ++argv;
      
      PMATH_RUN_ARGS("Get(`1`)", "(o)", pmath_string_from_native(*argv, -1));
    }
    else if((strcmp(*argv, "-x") == 0 || strcmp(*argv, "--exec") == 0)
    && argc > 1){
      --argc;
      ++argv;
      
      pmath_unref(
        pmath_evaluate(
          pmath_parse_string(
            pmath_string_from_native(*argv, -1))));
    }
    else{
      if(strcmp(*argv, "--help") != 0){
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

static pmath_t check_dialog_return(pmath_t result){ // result wont be freed
  if(pmath_is_expr_of(result, PMATH_SYMBOL_RETURN)
  && pmath_expr_length(result) <= 1){
    return pmath_expr_get_item(result, 1);
  }
  
  return PMATH_UNDEFINED;
}

static pmath_t dialog(pmath_t first_eval){
  pmath_t result = NULL;
  pmath_t old_dialog = pmath_session_start();
  
  first_eval = pmath_evaluate(first_eval);
  result = check_dialog_return(first_eval);
  pmath_unref(first_eval);
  
  if(result == PMATH_UNDEFINED){
    result = NULL;
    while(!quitting){
      pmath_string_t code;
      pmath_bool_t err = FALSE;
      pmath_span_array_t *spans;
      
      pmath_continue_after_abort();
      
      write_line("\n");
      write_indent("pmath> ");
      
      code = read_line(stdin);
      
      if(dialog_depth > 0 && pmath_aborting()){
        pmath_unref(code);
        break;
      }
      
      spans = pmath_spans_from_string(
        &code, 
        scanner_read, 
        NULL, 
        NULL, 
        scanner_error,
        &err);
      
      if(!err){
        pmath_t obj = pmath_evaluate(
          pmath_expr_new_extended(
            pmath_ref(PMATH_SYMBOL_MAKEEXPRESSION), 1,
            pmath_boxes_from_spans(
              spans, 
              code, 
              TRUE, 
              NULL, 
              NULL)));
        
        if(pmath_instance_of(obj, PMATH_TYPE_EXPRESSION)){
          pmath_t head = pmath_expr_get_item(obj, 0);
          pmath_unref(head);
          
          if(head == PMATH_SYMBOL_HOLDCOMPLETE){
            if(pmath_expr_length(obj) == 1){
              head = obj;
              obj = pmath_expr_get_item(head, 1);
              pmath_unref(head);
            }
            else{
              obj = pmath_expr_set_item(
                obj, 0, pmath_ref(PMATH_SYMBOL_SEQUENCE));
            }
            
            obj = pmath_session_execute(obj, NULL);
          }
        } 
        
        if(obj && !quitting){
          if(dialog_depth > 0){
            result = check_dialog_return(obj);
            
            if(result != PMATH_UNDEFINED){
              pmath_unref(obj);
              pmath_unref(code);
              pmath_span_array_free(spans);
              
              break;
            }
            
            result = NULL;
          }
          
          write_output(obj);
        }
        
        pmath_unref(obj);
      }
    
      pmath_unref(code);
      pmath_span_array_free(spans);
    }
  }
  
  pmath_session_end(old_dialog);
  return result;
}

  static pmath_threadlock_t dialog_lock = NULL;
  
  struct dialog_callback_info_t{
    pmath_t result;
    pmath_t first_eval;
  };
  
  static void dialog_callback(void *_info){
    struct dialog_callback_info_t *info = (struct dialog_callback_info_t*)_info;
    
    ++dialog_depth;
  
    info->result = dialog(info->first_eval);
    info->first_eval = NULL;
    
    --dialog_depth;
  }

static pmath_t builtin_dialog(pmath_expr_t expr){
  struct dialog_callback_info_t info;
  
  if(pmath_expr_length(expr) > 1){
    pmath_message_argxxx(pmath_expr_length(expr), 0, 0);
    return expr;
  }
  
  info.result = NULL;
  info.first_eval = NULL;
  if(pmath_expr_length(expr) == 1){
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
  
  static pmath_string_t next_word(pmath_string_t *line){
    const uint16_t *buf = pmath_string_buffer(*line);
    int             len = pmath_string_length(*line);
    pmath_string_t word;
    int i;
    
    for(i = 0;i < len && buf[i] > ' ';++i)
      ++i;
    
    word = pmath_string_part(pmath_ref(*line), 0, i);
    
    for(i = 0;i < len && buf[i] <= ' ';++i)
      ++i;
    
    *line = pmath_string_part(*line, i, INT_MAX);
    return word;
  }
  
  static pmath_threadlock_t interrupt_lock = NULL;
  
  static void interrupt_callback(void *dummy){
    pmath_string_t line = 0;
    pmath_string_t word = 0;
    
    pmath_suspend_all_please();
    
    while(!quitting && !pmath_aborting()){
      pmath_unref(line);
      pmath_unref(word);
      
      write_line("\ninterrupt: ");
      
      line = read_line(stdin);
      word = next_word(&line);

      if(pmath_string_equals_latin1(word, "a")
      || pmath_string_equals_latin1(word, "abort")){
        write_line("aborting...\n");
        pmath_abort_please();
        break;
      }
      
      if(pmath_string_equals_latin1(word, "c")
      || pmath_string_equals_latin1(word, "continue")){
        write_line("continuing...\n");
        break;
      }
      
      if(pmath_string_equals_latin1(word, "i")
      || pmath_string_equals_latin1(word, "inspect")){
        pmath_messages_t mq ;
        
        write_line("entering interactive dialog (finish with `Return()`) ...\n");
        
        mq = get_main_mq();
        pmath_thread_send(
          mq, 
          pmath_expr_new(pmath_ref(PMATH_SYMBOL_DIALOG), 0));
        pmath_unref(mq);
        break;
      }
      
      if(pmath_string_length(word) > 0){
        write_line("unknown command: ");
        write_output(word);
      }
      
      write_line(
        "possible commands are:\n"
        "  a, abort     Abort the current evaluation.\n"
        "  c, continue  Continue the current evaluation.\n"
        "  i, inspect   Enter an interactive dialog.\n");
    }
    
    pmath_unref(line);
    pmath_unref(word);
    
    write_line("\n");
    
    pmath_resume_all();
  }

static pmath_t builtin_interrupt(pmath_expr_t expr){
  if(pmath_expr_length(expr) > 0){
    pmath_message_argxxx(pmath_expr_length(expr), 0, 0);
    return expr;
  }
  
  if(pmath_thread_queue_is_blocked_by(pmath_ref(main_mq), pmath_thread_get_queue())){
    /* Already in main thread or the main thread is waiting on the current 
       thread (e.g. through pmath_task_wait()) */
    
    pmath_unref(expr);
    
    pmath_thread_call_locked(
      &interrupt_lock,
      interrupt_callback,
      NULL);
  }
  else{
    // in another thread => send to main thread
    
    pmath_messages_t mq = get_main_mq();
    pmath_thread_send(mq, expr);
    pmath_unref(mq);
  }
  
  return NULL;
}

static pmath_t builtin_quit(pmath_expr_t expr){
  if(pmath_expr_length(expr) == 1){
    pmath_t res = pmath_expr_get_item(expr, 1);
    
    if(!pmath_instance_of(res, PMATH_TYPE_INTEGER)
    || !pmath_integer_fits_si(res)){
      pmath_unref(res);
      pmath_message(NULL, "intm", 2, pmath_integer_new_si(1), pmath_ref(expr));
      return expr;
    }
    
    quit_result = pmath_integer_get_si(res);
    pmath_unref(res);
  }
  else if(pmath_expr_length(expr) != 0){
    pmath_message_argxxx(pmath_expr_length(expr), 0, 1);
    return expr;
  }
  
  quitting = TRUE;
  pmath_abort_please();
  
  pmath_unref(expr);
  return NULL;
}

int main(int argc, const char **argv){
  sem_init(&interrupt_semaphore, 0, 0);
  
  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_term);

  if(!pmath_init()
  || !pmath_register_code(PMATH_SYMBOL_DIALOG,    builtin_dialog, 0)
  || !pmath_register_code(PMATH_SYMBOL_INTERRUPT, builtin_interrupt, 0)
  || !pmath_register_code(PMATH_SYMBOL_QUIT,      builtin_quit, 0)){
    fprintf(stderr, "Cannot initialize pMath.\n");
    return 1;
  }
  
  pmath_unref(
    pmath_thread_fork_daemon(
      interrupt_daemon, 
      kill_interrupt_daemon, 
      NULL));
  
  main_mq = pmath_thread_get_queue();
  
  handle_options(argc, argv);
  
  if(!quitting){
    write_line("Welcome to pMath.\n"
      "Type `??symbol` to get Help about a symbol. Exit with `Quit()`.\n");
  }
  
  pmath_unref(dialog(NULL));
  pmath_continue_after_abort();
  
  { // freeing main_mq
    pmath_t mq;
    
    pmath_atomic_lock(&main_mq_lock);
    {
      mq = main_mq;
      main_mq = 0;
    }
    pmath_atomic_unlock(&main_mq_lock);
    
    pmath_unref(mq);
  }
  
  pmath_done();
  
  {
    size_t current, max;
    pmath_mem_usage(&current, &max);
    if(show_mem_stats || current != 0){
      printf("memory: %"PRIuPTR" (should be 0)\n", current);
      printf("max. used: %"PRIuPTR"\n", max);
    }
  }
  
  signal(SIGINT, signal_dummy);
  sem_destroy(&interrupt_semaphore);
  
  return quit_result;
}
