#include <pmath-util/debug.h>
#include <pmath-util/helpers.h>

#include <stdarg.h>
#include <stdio.h>


#ifdef pmath_debug_print

  #undef pmath_debug_print
  #undef pmath_debug_print_object
  #undef pmath_debug_print_stack

#endif

#ifdef PMATH_OS_WIN32
/* no flockfile()/funlockfile() on windows/mingw -> do it your self */
  #if PMATH_USE_PTHREAD

    #include <pthread.h>
    static pthread_mutex_t  debuglog_mutex;

    #define flockfile(  file)  ((void)pthread_mutex_lock(  &debuglog_mutex))
    #define funlockfile(file)  ((void)pthread_mutex_unlock(&debuglog_mutex))

    #elif PMATH_USE_WINDOWS_THREADS

    #define NOGDI
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    static CRITICAL_SECTION  debuglog_critical_section;

    #define flockfile(  file)  ((void)EnterCriticalSection(&debuglog_critical_section))
    #define funlockfile(file)  ((void)LeaveCriticalSection(&debuglog_critical_section))

  #else

    #error Either PThread or Windows Threads must be used

  #endif
  
#endif

static FILE *debuglog = NULL;

static pmath_bool_t debugging_output = TRUE;

PMATH_API void pmath_debug_print(const char *fmt, ...) {
  if(debugging_output) {
    va_list args;
    
    debugging_output = FALSE;
    
    va_start(args, fmt);
    flockfile(debuglog);
    vfprintf(debuglog, fmt, args);
    funlockfile(debuglog);
    va_end(args);
    fflush(debuglog);
    
    debugging_output = TRUE;
  }
}

static void write_data(FILE *file, const uint16_t *data, int len) {
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

PMATH_API void pmath_debug_print_object(
  const char *pre,
  pmath_t obj,
  const char *post
) {
  if(debugging_output) {
    debugging_output = FALSE;
    
    flockfile(debuglog);
    
    fputs(pre, debuglog);
    pmath_write(
      obj,
      PMATH_WRITE_OPTIONS_FULLSTR | PMATH_WRITE_OPTIONS_INPUTEXPR
      | PMATH_WRITE_OPTIONS_FULLNAME,
      (void(*)(void*, const uint16_t*, int))write_data,
      debuglog);
    fputs(post, debuglog);
    
    funlockfile(debuglog);
    
    debugging_output = TRUE;
  }
}

static pmath_bool_t stack_walker(pmath_t head, void *p) {
  pmath_debug_print_object("  in ", head, "\n");
  
  return TRUE;
}

PMATH_API void pmath_debug_print_stack(void) {
  pmath_debug_print("pMath stack:\n");
  pmath_walk_stack(stack_walker, NULL);
}

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
