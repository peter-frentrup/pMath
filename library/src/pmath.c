#include <pmath.h>
#include <pmath-private.h>

#include <pmath-core/objects-private.h>
#include <pmath-core/strings-private.h>
#include <pmath-core/expressions-private.h>
#include <pmath-core/packed-arrays-private.h>
#include <pmath-core/custom-private.h>
#include <pmath-core/numbers-private.h>
#include <pmath-core/symbols-private.h>

#include <pmath-language/patterns-private.h>
#include <pmath-language/regex-private.h>

#include <pmath-util/concurrency/atomic-private.h>
#include <pmath-util/concurrency/threadlocks-private.h>
#include <pmath-util/concurrency/threadmsg-private.h>
#include <pmath-util/concurrency/threadpool-private.h>
#include <pmath-util/concurrency/threads-private.h>
#include <pmath-util/dynamic-private.h>
#include <pmath-util/hashtables-private.h>
#include <pmath-util/modules-private.h>
#include <pmath-util/stacks-private.h>
#include <pmath-util/symbol-values-private.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/io-private.h>
#include <pmath-builtins/number-theory-private.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

#ifdef PMATH_OS_WIN32
#  define NOGDI
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  include <shellapi.h>
#elif defined(__APPLE__)
#  include <CoreServices/CoreServices.h>
#else
#  include <dlfcn.h>
#  include <link.h>
#  include <unistd.h>
#endif

#if PMATH_USE_PTHREAD
#  include <pthread.h>
#endif

#include <time.h>

#include <pcre.h> // Only needed here to print its version number.

#include <zlib.h> // Only needed here to print its version number.


PMATH_STATIC_ASSERT(sizeof(intptr_t) == sizeof(void *));
PMATH_STATIC_ASSERT(PMATH_BITSIZE      == 8 * sizeof(void *));
PMATH_STATIC_ASSERT(PMATH_INT_BITSIZE  == 8 * sizeof(int));
PMATH_STATIC_ASSERT(PMATH_LONG_BITSIZE == 8 * sizeof(long));

PMATH_STATIC_ASSERT(sizeof(pmath_t) == 8);

#ifdef PMATH_OS_WIN32
PMATH_STATIC_ASSERT(sizeof(wchar_t) == sizeof(uint16_t));
#endif


static volatile enum {
  PMATH_STATUS_NONE,
  PMATH_STATUS_INITIALIZING,
  PMATH_STATUS_RUNNING,
  PMATH_STATUS_DESTROYING
} _pmath_status = PMATH_STATUS_NONE;

static pmath_atomic_t pmath_count = PMATH_ATOMIC_STATIC_INIT;

PMATH_PRIVATE
pmath_bool_t _pmath_is_running(void) {
  return _pmath_status == PMATH_STATUS_RUNNING;
}

static pmath_expr_t get_command_line(void) {
  pmath_gather_begin(PMATH_NULL);
#ifdef PMATH_OS_WIN32
  {
    int argc;
    wchar_t **argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    
    if(argv) {
      int i;
      for(i = 0; i < argc; ++i)
        pmath_emit(pmath_string_insert_ucs2(PMATH_NULL, 0, argv[i], -1), PMATH_NULL);
        
      LocalFree(argv);
    }
  }
#else
  {
    FILE *f = fopen("/proc/self/cmdline", "r");
  
    if(f) {
      pmath_string_t str = PMATH_NULL;
      char buf[2];
  
      while(fgets(buf, sizeof(buf), f)) {
        if(strlen(buf) == 0) {
          if(!pmath_is_null(str)) {
            pmath_emit(str, PMATH_NULL);
            str = PMATH_NULL;
          }
        }
        else
          str = pmath_string_concat(str, pmath_string_from_native(buf, -1));
      }
  
      fclose(f);
    }
    else {
      pmath_debug_print("cannot open /proc/self/cmdline\n");
    }
  }
#endif
  
  return pmath_gather_end();
}

static void init_pagewidth(void) {
  int width = 80;
  
#ifdef PMATH_OS_WIN32
  {
    CONSOLE_SCREEN_BUFFER_INFO info;
    memset(&info, 0, sizeof(info));
    
    if(GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info)) {
      width = info.dwSize.X;
    }
  }
#else
  {
    const char *col = getenv("COLUMNS");
  
    if(col)
      width = atoi(col);
  }
#endif
  
  if(width < 6)
    width = 6;
    
  PMATH_RUN_ARGS("$PageWidth:=`1`", "(i)", width);
}

static pmath_expr_t get_exe_name(void) {
#ifdef PMATH_OS_WIN32
  {
    pmath_string_t s;
    uint16_t *buf;
    int len;
    DWORD  needed;
    
    len = 128;
    s = PMATH_NULL;
    do{
      len *= 2;
      
      pmath_unref(s);
      s = pmath_string_new_raw(len);
      if(!pmath_string_begin_write(&s, &buf, NULL)) {
        pmath_unref(s);
        return PMATH_NULL;
      }
      
      needed = GetModuleFileNameW(NULL, buf, (DWORD)len);
      pmath_string_end_write(&s, &buf);
    } while(needed == (DWORD)len);
    
    return pmath_string_part(s, 0, (int)needed);
  }
#elif defined(__APPLE__)
  {
    ProcessSerialNumber *psn;
    FSRef               *fsr;
    char                *path;
  
    if( GetCurrentProcess(&psn)              == noErr &&
    GetProcessBundleLocation(&psn, &fsr) == noErr &&
    FSRefMakePath(&fsr, (UInt8 *)path, sizeof(path)))
    {
      return pmath_string_from_utf8(path, -1);
    }
  }
#else
  {
    void *exe = dlopen(NULL, RTLD_LAZY);
  
    if(exe) {
      void *main_sym;
      struct link_map *map = NULL;
  
      dlinfo(exe, RTLD_DI_LINKMAP, &map);
      if(map && map->l_name[0]) {
        pmath_string_t s = pmath_string_from_native(map->l_name, -1);
  
        dlclose(exe);
        return s;
      }
  
      // only works if executable was compiled with -export-dynamic
      main_sym = dlsym(exe, "main");
      if(main_sym) {
        Dl_info info;
  
        memset(&info, 0, sizeof(info));
        if(dladdr(main_sym, &info) && info.dli_fname) {
          pmath_string_t s = pmath_string_from_native(info.dli_fname, -1);
  
          dlclose(exe);
          return s;
        }
      }
  
      dlclose(exe);
    }
  
    {
      char filename[100];
      char buf[1024];
      ssize_t len;
  
      // Linux has a /proc/self/exe symlink
      if((len = readlink("/proc/self/exe", buf, sizeof(buf))) != -1) {
        return pmath_string_from_native(buf, len);
      }
  
      // Solaris has a /proc/<pid>/path/a.out symlink
      snprintf(filename, sizeof(filename), "/proc/%lu/path/a.out", (unsigned long)getpid());
      if((len = readlink(filename, buf, sizeof(buf))) != -1) {
        return pmath_string_from_native(buf, len);
      }
    }
  
//    {
//      Dl_info info;
//      if(dladdr(pmath_init, &info) && info.dli_fname){
//        return pmath_string_from_native(info.dli_fname, -1);
//      }
//    }
  
    return PMATH_NULL;
  }
#endif
}

static pmath_expr_t get_system_information(void) {
#  define SETTINGS_RULE(name, value)  pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_RULE), 2, PMATH_C_STRING((name)), (value))
#  define LIST1(a)     pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_LIST), 1, (a))
#  define LIST2(a, b)  pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_LIST), 2, (a), (b))

  pmath_expr_t compiler_info, gmp_info;
  
  // http://sourceforge.net/p/predef/wiki/Compilers/
#  if defined( _MSC_VER )
  {
    char msvc_version[100];
    
    int version  = _MSC_VER / 100;
    int revision = _MSC_VER % 100;
    
#  ifdef _MSC_FULL_VER
#    if _MSC_VER >= 1400
    int patch = _MSC_FULL_VER % 100000;
#    else
    int patch = _MSC_FULL_VER % 10000;
#    endif
#  else
    int patch = 0;
#  endif
    
    sprintf(msvc_version, "%d.%d.%d", version, revision, patch);
    
    compiler_info = LIST2(
        SETTINGS_RULE("Name",    PMATH_C_STRING("Microsoft Visual C++")),
        SETTINGS_RULE("Version", PMATH_C_STRING(msvc_version)));
  }
#elif defined(__GNUC__)
  {
    char gcc_version[100];
  
    int version  = __GNUC__;
    int revision = __GNUC_MINOR__;
  
#  ifdef __GNUC_PATCHLEVEL__
    int patch = __GNUC_PATCHLEVEL__;
#  else
    int patch = 0;
#  endif
  
    sprintf(gcc_version, "%d.%d.%d", version, revision, patch);
  
    compiler_info = LIST2(
        SETTINGS_RULE("Name",    PMATH_C_STRING("GNU C/C++")),
        SETTINGS_RULE("Version", PMATH_C_STRING(gcc_version)));
  }
#else
  {
#  warning "No compiler info at runtime"
    compiler_info = LIST2(
        SETTINGS_RULE("Name",    "Unknown"),
        SETTINGS_RULE("Version", "Unknown"));
  }
#endif
  
#ifdef mpir_version
  {
    gmp_info = SETTINGS_RULE("mpir",
        LIST1(
            SETTINGS_RULE("Version", PMATH_C_STRING(mpir_version))
        ));
  }
#else
  {
    gmp_info = SETTINGS_RULE("gmp",
        LIST1(
            SETTINGS_RULE("Version", PMATH_C_STRING(gmp_version))
        ));
  }
#endif
  
  
  return LIST2(
      SETTINGS_RULE("Compiler", compiler_info),
      SETTINGS_RULE(
          "ThirdPartyLibraries",
          pmath_expr_new_extended(
              pmath_ref(PMATH_SYMBOL_LIST), 6,
              gmp_info,
              SETTINGS_RULE("mpfr",  LIST1( SETTINGS_RULE("Version", PMATH_C_STRING(mpfr_get_version())) )),
              SETTINGS_RULE("flint", LIST1( SETTINGS_RULE("Version", PMATH_C_STRING(FLINT_VERSION))      )),
              SETTINGS_RULE("arb",   LIST1( SETTINGS_RULE("Version", PMATH_C_STRING(arb_version))        )),
              SETTINGS_RULE("pcre",  LIST1( SETTINGS_RULE("Version", PMATH_C_STRING(pcre16_version()))   )),
              SETTINGS_RULE("zlib",  LIST1( SETTINGS_RULE("Version", PMATH_C_STRING(zlib_version))       ))
          ))
      );
      
#undef LIST1
#undef LIST2
#undef SETTINGS_RULE
}

#ifdef PMATH_DEBUG_TESTS
static void TEST_ATOMIC_OPS(void) {
  // This does not test for atomicity.
  pmath_atomic_t a;
  intptr_t b;
  pmath_atomic2_t c;
  pmath_bool_t test;
  
  pmath_atomic_write_release(&a, 5);
  assert(a._data == 5);
  
  b = pmath_atomic_fetch_add(&a, 2);
  assert(b == 5 && pmath_atomic_read_aquire(&a) == 7);
  assert(b == 5 && a._data                      == 7);
  
  b = pmath_atomic_fetch_set(&a, 27);
  assert(b == 7 && a._data == 27);
  
  b = pmath_atomic_fetch_compare_and_set(&a, 26, 11);
  assert(b == 27 && a._data == 27);
  b = pmath_atomic_fetch_compare_and_set(&a, 27, 11);
  assert(b == 27 && a._data == 11);
  
  test = pmath_atomic_compare_and_set(&a, 13, 22);
  assert(!test && a._data == 11);
  test = pmath_atomic_compare_and_set(&a, 11, 22);
  assert(test && a._data == 22);
  
  if(pmath_atomic_have_cas2()) {
    c._data[0] = 8;
    c._data[1] = 33;
    test = pmath_atomic_compare_and_set_2(&c, 5, 6, 1, 2);
    assert(!test && c._data[0] == 8 && c._data[1] == 33);
    test = pmath_atomic_compare_and_set_2(&c, 8, 33, 1, 2);
    assert(test && c._data[0] == 1 && c._data[1] == 2);
  }
  else
    fprintf(stderr, "no CAS2 available\n");
}

static void TEST_STACKS(void) {
  typedef struct {
    void *reserved;
    int value;
  } stack_test_item_t;
  stack_test_item_t a, b, c, *p;
  pmath_stack_t stack;
  
  a.value = 1;
  b.value = 2;
  c.value = 3;
  stack = pmath_stack_new();
  if(stack) {
    p = pmath_stack_pop(stack);
    assert(p == NULL);
    
    pmath_stack_push(stack, &a);
    pmath_stack_push(stack, &b);
    p = pmath_stack_pop(stack);
    assert(p && p->value == 2);
    
    pmath_stack_push(stack, &c);
    p = pmath_stack_pop(stack);
    assert(p && p->value == 3);
    
    p = pmath_stack_pop(stack);
    assert(p && p->value == 1);
    
    p = pmath_stack_pop(stack);
    assert(p == NULL);
    
    pmath_stack_free(stack);
  }
}
#else
#define TEST_ATOMIC_OPS()  ((void)0)
#define TEST_STACKS()      ((void)0)
#endif

PMATH_API pmath_bool_t pmath_init(void) {
  pmath_thread_t thread;
  
  if(pmath_atomic_fetch_add(&pmath_count, +1) == 0) {
    while(_pmath_status != PMATH_STATUS_NONE) {
    }
    
    _pmath_status = PMATH_STATUS_INITIALIZING;
    
//    #ifdef PMATH_OS_WIN32
//      SetErrorMode(SEM_NOOPENFILEERRORBOX);
//    #endif

    TEST_ATOMIC_OPS();
    
    _pmath_object_complex_infinity         = PMATH_NULL;
    _pmath_object_emptylist                = PMATH_NULL;
    _pmath_object_get_load_message         = PMATH_NULL;
    _pmath_object_pos_infinity             = PMATH_NULL;
    _pmath_object_neg_infinity             = PMATH_NULL;
    _pmath_object_loadlibrary_load_message = PMATH_NULL;
    _pmath_object_memory_exception         = PMATH_NULL;
    _pmath_object_multimatch               = PMATH_NULL;
//    _pmath_object_newsym_message           = PMATH_NULL;
    _pmath_object_overflow                 = PMATH_NULL;
    _pmath_object_range_from_one           = PMATH_NULL;
    _pmath_object_range_from_zero          = PMATH_NULL;
    _pmath_object_singlematch              = PMATH_NULL;
    _pmath_object_stop_message             = PMATH_NULL;
    _pmath_object_underflow                = PMATH_NULL;
    _pmath_object_zeromultimatch           = PMATH_NULL;
    _pmath_object_empty_pattern_sequence   = PMATH_NULL;
    
    if(!_pmath_debug_library_init())          goto FAIL_DEBUG_LIBRARY;
    if(!_pmath_stacks_init())                 goto FAIL_STACKS_LIBRARY;
    if(!_pmath_memory_manager_init())         goto FAIL_MEMORY_MANAGER;
    
    TEST_STACKS();
    PMATH_TEST_NEW_HASHTABLES();
    
    if(!_pmath_threads_init())                goto FAIL_THREADS;
    thread = _pmath_thread_new(NULL);
    _pmath_thread_set_current(thread);
    if(!thread)                               goto FAIL_THIS_THREAD;
    if(!_pmath_threadlocks_init())            goto FAIL_THREADLOCKS;
    if(!_pmath_charnames_init())              goto FAIL_CHARNAMES;
    if(!_pmath_objects_init())                goto FAIL_OBJECTS;
    if(!_pmath_strings_init())                goto FAIL_STRINGS;
    if(!_pmath_numbers_init())                goto FAIL_NUMBERS;
    if(!_pmath_expressions_init())            goto FAIL_EXPRESSIONS;
    if(!_pmath_packed_arrays_init())          goto FAIL_PACKED_ARRAYS;
    if(!_pmath_symbols_init())                goto FAIL_SYMBOLS;
    if(!_pmath_numeric_init())                goto FAIL_NUMERIC;
    if(!_pmath_symbol_values_init())          goto FAIL_SYMBOL_VALUES;
    if(!_pmath_symbol_builtins_init())        goto FAIL_BUILTINS;
    if(!_pmath_custom_objects_init())         goto FAIL_CUSTOM;
    if(!_pmath_threadmsg_init())              goto FAIL_THREADMSG;
    if(!_pmath_threadpool_init())             goto FAIL_THREADPOOL;
    if(!_pmath_regex_init())                  goto FAIL_REGEX;
    if(!_pmath_modules_init())                goto FAIL_MODULES;
    
    if(!_pmath_dynamic_init())                goto FAIL_DYNAMIC;
    
    pmath_debug_print("[zlib %s]\n", zlib_version);
    
    { // init static objects ...
      // SystemException("OutOfMemory")
      _pmath_object_memory_exception = pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_SYSTEMEXCEPTION), 1,
          PMATH_C_STRING("OutOfMemory"));
      _pmath_expr_update(_pmath_object_memory_exception);
      
      // Overflow()
      _pmath_object_overflow = pmath_expr_new(
          pmath_ref(PMATH_SYMBOL_OVERFLOW), 0);
      _pmath_expr_update(_pmath_object_overflow);
      
      // Underflow()
      _pmath_object_underflow = pmath_expr_new(
          pmath_ref(PMATH_SYMBOL_UNDERFLOW), 0);
      _pmath_expr_update(_pmath_object_underflow);
      
      // DirectedInfinity(1)
      _pmath_object_pos_infinity = pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_DIRECTEDINFINITY), 1,
          PMATH_FROM_INT32(1));
      _pmath_expr_update(_pmath_object_pos_infinity);
      
      // DirectedInfinity(-1)
      _pmath_object_neg_infinity = pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_DIRECTEDINFINITY), 1,
          PMATH_FROM_INT32(-1));
      _pmath_expr_update(_pmath_object_neg_infinity);
      
      // DirectedInfinity()
      _pmath_object_complex_infinity = pmath_expr_new(
          pmath_ref(PMATH_SYMBOL_DIRECTEDINFINITY), 0);
      _pmath_expr_update(_pmath_object_complex_infinity);
      
      // Range(1, Automatic)
      _pmath_object_range_from_one = pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_RANGE), 2,
          PMATH_FROM_INT32(1),
          pmath_ref(PMATH_SYMBOL_AUTOMATIC));
      _pmath_expr_update(_pmath_object_range_from_one);
      
      // Range(0, Automatic)
      _pmath_object_range_from_zero = pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_RANGE), 2,
          PMATH_FROM_INT32(0),
          pmath_ref(PMATH_SYMBOL_AUTOMATIC));
      _pmath_expr_update(_pmath_object_range_from_zero);
      
      // SingleMatch()
      _pmath_object_singlematch = pmath_expr_new(
          pmath_ref(PMATH_SYMBOL_SINGLEMATCH), 0);
      _pmath_expr_update(_pmath_object_singlematch);
      
      // Repeated(SingleMatch(), Range(1, /\/))
      _pmath_object_multimatch = pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_REPEATED), 2,
          pmath_ref(_pmath_object_singlematch),
          pmath_ref(_pmath_object_range_from_one));
      _pmath_expr_update(_pmath_object_multimatch);
      
      // Repeated(SingleMatch(), Range(0, /\/))
      _pmath_object_zeromultimatch = pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_REPEATED), 2,
          pmath_ref(_pmath_object_singlematch),
          pmath_ref(_pmath_object_range_from_zero));
      _pmath_expr_update(_pmath_object_zeromultimatch);
      
      // PMATH_MAGIC_PATTERN_SEQUENCE()
      _pmath_object_empty_pattern_sequence = pmath_expr_new(
          PMATH_MAGIC_PATTERN_SEQUENCE, 0);
      _pmath_expr_update(_pmath_object_empty_pattern_sequence);
      
      // List()
      _pmath_object_emptylist = pmath_expr_new(
          pmath_ref(PMATH_SYMBOL_LIST), 0);
      _pmath_expr_update(_pmath_object_emptylist);
      
      // MessageName(General, "stop")
      _pmath_object_stop_message = pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_MESSAGENAME), 2,
          pmath_ref(PMATH_SYMBOL_GENERAL),
          PMATH_C_STRING("stop"));
      //_pmath_expr_update(_pmath_object_stop_message);
      
//    // MessageName(General, "newsym")
//    _pmath_object_newsym_message = pmath_expr_new_extended(
//      pmath_ref(PMATH_SYMBOL_MESSAGENAME), 2,
//      pmath_ref(PMATH_SYMBOL_GENERAL),
//      PMATH_C_STRING("newsym"));

      // MessageName(Get, "load")
      _pmath_object_loadlibrary_load_message = pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_MESSAGENAME), 2,
          pmath_ref(PMATH_SYMBOL_LOADLIBRARY),
          PMATH_C_STRING("load"));
      //_pmath_expr_update(_pmath_object_loadlibrary_load_message);
      
      // MessageName(LoadLibrary, "load")
      _pmath_object_get_load_message = pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_MESSAGENAME), 2,
          pmath_ref(PMATH_SYMBOL_GET),
          PMATH_C_STRING("load"));
      //_pmath_expr_update(_pmath_object_get_load_message);
      
      if( pmath_is_null(_pmath_object_complex_infinity)         ||
          pmath_is_null(_pmath_object_emptylist)                ||
          pmath_is_null(_pmath_object_get_load_message)         ||
          pmath_is_null(_pmath_object_pos_infinity)             ||
          pmath_is_null(_pmath_object_neg_infinity)             ||
          pmath_is_null(_pmath_object_loadlibrary_load_message) ||
          pmath_is_null(_pmath_object_memory_exception)         ||
          pmath_is_null(_pmath_object_multimatch)               ||
          // pmath_is_null(_pmath_object_newsym_message) ||
          pmath_is_null(_pmath_object_overflow)                 ||
          pmath_is_null(_pmath_object_range_from_one)           ||
          pmath_is_null(_pmath_object_range_from_zero)          ||
          pmath_is_null(_pmath_object_singlematch)              ||
          pmath_is_null(_pmath_object_stop_message)             ||
          pmath_is_null(_pmath_object_underflow)                ||
          pmath_is_null(_pmath_object_zeromultimatch)           ||
          pmath_is_null(_pmath_object_empty_pattern_sequence))
      {
        goto FAIL_STATIC_OBJECTS;
      }
    }
    
    _pmath_status = PMATH_STATUS_RUNNING;
    
    { // init threadlocal variables ...
      // $NamespacePath:= {"System`"}
      _pmath_symbol_set_global_value(
          PMATH_SYMBOL_NAMESPACEPATH,
          pmath_build_value("(s)", "System`"));
          
      // $Packages:= {"System`", "Global`"}
      _pmath_symbol_set_global_value(
          PMATH_SYMBOL_PACKAGES,
          pmath_build_value("(ss)", "System`", "Global`"));
          
      // $Namespace:= "Global`"
      _pmath_symbol_set_global_value(
          PMATH_SYMBOL_CURRENTNAMESPACE,
          PMATH_C_STRING("Global`"));
          
      // $DirectoryStack:= {}
      _pmath_symbol_set_global_value(
          PMATH_SYMBOL_DIRECTORYSTACK,
          pmath_ref(_pmath_object_emptylist));
          
      // $DirectoryStack:= {}
      _pmath_symbol_set_global_value(
          PMATH_SYMBOL_INPUT,
          pmath_string_new(0));
          
      // Internal`$NamespacePathStack:= {}
      _pmath_symbol_set_global_value(
          PMATH_SYMBOL_INTERNAL_NAMESPACEPATHSTACK,
          pmath_ref(_pmath_object_emptylist));
          
      // Internal`$NamespaceStack:= {}
      _pmath_symbol_set_global_value(
          PMATH_SYMBOL_INTERNAL_NAMESPACESTACK,
          pmath_ref(_pmath_object_emptylist));
          
      // System`BoxForm`$UseTextFormatting:= False
      _pmath_symbol_set_global_value(
          PMATH_SYMBOL_BOXFORM_USETEXTFORMATTING,
          pmath_ref(PMATH_SYMBOL_FALSE));
    }
    
    { // initialize runs ...
      PMATH_RUN_ARGS("$ApplicationFileName:= `1`", "(o)", get_exe_name());
      
      PMATH_RUN_ARGS("Developer`$SystemInformation:= `1`", "(o)", get_system_information());
      
      PMATH_RUN("$NewMessage:=Function({},,HoldFirst)");
      
      PMATH_RUN("ComplexInfinity:=DirectedInfinity()");
      PMATH_RUN("Infinity:=DirectedInfinity(1)");
      PMATH_RUN("I:=Complex(0,1)");
#ifdef PMATH_OS_WIN32
      PMATH_RUN("$PathListSeparator:=\";\"");
      PMATH_RUN("$PathnameSeparator:=\"\\\\\"");
#else
      PMATH_RUN("$PathListSeparator:=\":\"");
      PMATH_RUN("$PathnameSeparator:=\"/\"");
#endif
      
      PMATH_RUN("$InitialDirectory:=Directory()");
      
      PMATH_RUN_ARGS("$ByteOrdering:=`1`", "(i)", PMATH_BYTE_ORDER);
      PMATH_RUN_ARGS(
          "$CharacterEncoding:=$SystemCharacterEncoding:=`1`",
          "(s)", _pmath_native_encoding);
      PMATH_RUN_ARGS("$CommandLine:=`1`", "(o)", get_command_line());
      PMATH_RUN("$HistoryLength:=10");
      PMATH_RUN("$Line:=0");
      
      PMATH_RUN_ARGS("$MachineEpsilon:=`1`",   "(f)", (double)DBL_EPSILON);
      PMATH_RUN_ARGS("$MachinePrecision:=`1`", "(f)", (double)LOG10_2 * DBL_MANT_DIG);
      PMATH_RUN_ARGS("$MaxMachineNumber:=`1`", "(f)", (double)DBL_MAX);
      PMATH_RUN_ARGS("$MinMachineNumber:=`1`", "(f)", (double)DBL_MIN);
      PMATH_RUN("$MaxExtraPrecision:=50");
      
      PMATH_RUN("$MessageGroups:={\"Packing\" :> {General::punpack, General::punpack1}}");
      
      init_pagewidth();
      
      PMATH_RUN("$Path:={\".\"}");
      
#ifdef PMATH_OS_WIN32
      PMATH_RUN_ARGS("$ProcessId:=`1`", "(N)", (size_t)GetCurrentProcessId());
#else
      PMATH_RUN_ARGS("$ProcessId:=`1`", "(N)", (size_t)getpid());
#endif
      
      PMATH_RUN_ARGS("$ProcessorCount:=`1`", "(i)", _pmath_processor_count());
      
      PMATH_RUN("$ThreadId::=Internal`GetThreadId()");
      
#ifdef PMATH_OS_WIN32
      PMATH_RUN("$SystemId:=\"Windows\"");
#elif defined(linux) || defined(__linux) || defined(__linux__)
      PMATH_RUN("$SystemId:=\"Linux\"");
#elif defined(sun) || defined(__sun)
#if defined(__SVR4) || defined(__svr4__)
      PMATH_RUN("$SystemId:=\"Solaris\"");
#else
      PMATH_RUN("$SystemId:=\"SunOS\"");
#endif
#elif defined (__APPLE__)
      PMATH_RUN("$SystemId:=\"MacOSX\"");
#else
      unknown operating system
#endif
      
#ifdef PMATH_X86
      PMATH_RUN("$ProcessorType:=\"x86\"");
#elif defined(PMATH_AMD64)
      PMATH_RUN("$ProcessorType:=\"x86-64\"");
#elif defined(PMATH_SPARC32)
      PMATH_RUN("$ProcessorType:=\"Sparc\"");
#elif defined(PMATH_SPARC64)
      PMATH_RUN("$ProcessorType:=\"Sparc64\"");
#else
      unknown processor
#endif
      
      PMATH_RUN("$DialogLevel:=0");
      
      {
        int year, month, day, hour, minute, second;
        pmath_version_datetime(&year, &month, &day, &hour, &minute, &second);
        
        PMATH_RUN_ARGS("$CreationDate:= `1`", "((iiiiii))",
            year, month, day, hour, minute, second);
            
        PMATH_RUN_ARGS("$VersionList:= `1`", "((iiii))",
            pmath_version_number_part(1),
            pmath_version_number_part(2),
            pmath_version_number_part(3),
            pmath_version_number_part(4));
        PMATH_RUN_ARGS("$VersionNumber:= `1`", "(f)", pmath_version_number());
      }
      
      {
        time_t t = time(NULL);
        struct tm local_tm, global_tm;
        int loc_year_days, glob_year_days;
        double loc, glob;
        
        memcpy(&local_tm,  localtime(&t), sizeof(struct tm));
        memcpy(&global_tm, gmtime(&t),    sizeof(struct tm));
        
        local_tm.tm_year +=  1900;
        global_tm.tm_year += 1900;
        
        loc_year_days  = 365 + (local_tm.tm_year % 4 == 0 && (local_tm.tm_year % 100 != 0 || local_tm.tm_year % 400 == 0));
        glob_year_days = 365 + (global_tm.tm_year % 4 == 0 && (global_tm.tm_year % 100 != 0 || global_tm.tm_year % 400 == 0));
        
        if(local_tm.tm_year < global_tm.tm_year) {
          global_tm.tm_year++;
          global_tm.tm_yday += loc_year_days;
        }
        else if(local_tm.tm_year > global_tm.tm_year) {
          local_tm.tm_year++;
          local_tm.tm_yday += glob_year_days;
        }
        
        loc  = local_tm.tm_sec  + 60 * (local_tm.tm_min  + 60 * (local_tm.tm_hour  + 24 * local_tm.tm_yday));
        glob = global_tm.tm_sec + 60 * (global_tm.tm_min + 60 * (global_tm.tm_hour + 24 * global_tm.tm_yday));
        
        PMATH_RUN_ARGS("$TimeZone:=`1`", "(f)", (glob - loc) / (60 * 60.0));
      }
      
      PMATH_RUN("IsNumeric(Degree):=True");
      PMATH_RUN("IsNumeric(E):=True");
      PMATH_RUN("IsNumeric(EulerGamma):=True");
      PMATH_RUN("IsNumeric(GoldenRatio):=True");
      PMATH_RUN("IsNumeric(MachinePrecision):=True");
      PMATH_RUN("IsNumeric(Pi):=True");
      
      PMATH_RUN("SetPrecision(Degree,~)::= Pi/180");
      PMATH_RUN("MakeBoxes(Degree)::=\"\\[Degree]\"");
      
      PMATH_RUN("SetPrecision(GoldenRatio,~)::= (Sqrt(5)+1)/2");
      
      PMATH_RUN("Options(Internal`ParseRealBall):={\"MinPrecision\"->MachinePrecision}");
      PMATH_RUN("Options(Internal`RealBallFromMidpointRadius):={WorkingPrecision->Automatic}");
      PMATH_RUN("Options(Internal`WriteRealBall):={\"Base\"->10,\"MaxDigits\"->Automatic,\"AllowInexactDigits\"->False}");
      
      PMATH_RUN("Default(Ceiling,2):=1");
      PMATH_RUN("Default(Floor,2):=1");
      PMATH_RUN("Default(N,2):=MachinePrecision");
      PMATH_RUN("Default(Piecewise,2):=0");
      PMATH_RUN("Default(Plus):=0");
      PMATH_RUN("Default(PolyGamma,1):=0");
      PMATH_RUN("Default(Power,2):=1");
      PMATH_RUN("Default(Round,2):=1");
      PMATH_RUN("Default(Times):=1");
      
      PMATH_RUN(
          "Options(Apply):="
          "Options(Cases):="
          "Options(Count):="
          "Options(Level):="
          "Options(Map):="
          "Options(MapIndexed):="
          "Options(ParallelMap):="
          "Options(ParallelMapIndexed):="
          "Options(ParallelScan):="
          "Options(Replace):="
          "Options(ReplaceList):="
          "Options(ReplaceRepeated):="
          "Options(Scan):={Heads->False}");
          
      PMATH_RUN("Options(BinaryRead):=Options(BinaryReadList):=Options(BinaryWrite):={"
          "ByteOrdering:>$ByteOrdering}");
          
      PMATH_RUN("Options(Complement):={SameTest->Automatic}");
      
      PMATH_RUN("Options(CompressStream):={"
          "\"WindowBits\"->Automatic,"
          "\"RawDeflate\"->False,"
          "Level->Automatic}");
      PMATH_RUN("Options(UncompressStream):={"
          "\"WindowBits\"->Automatic,"
          "\"RawInflate\"->False}");
      
      PMATH_RUN("Options(DateList):={TimeZone:>$TimeZone}");
      
      PMATH_RUN("Options(DeleteDirectory):={DeleteContents->False}");
      
      PMATH_RUN("Options(FileNames):={IgnoreCase->Automatic}");
      
      PMATH_RUN(
          "Options(Find):="
          "Options(FindList):="
          "Options(Names):="
          "Options(StringMatch):="
          "Options(StringReplace):="
          "Options(StringSplit):={IgnoreCase->False}");
          
      PMATH_RUN(
          "Options(FixedPoint):="
          "Options(FixedPointList):={SameTest->Identical}");
          
      PMATH_RUN("Options(Get):={"
          "CharacterEncoding->Automatic,"
          "Head->Identity,"
          "Path:>$Path}");
          
      PMATH_RUN(
          "Options(IsFreeOf):="
          "Options(Position):={Heads->True}");
          
      PMATH_RUN("Options(MakeExpression):=Options(ToExpression):={"
          "ParserArguments->Automatic,"
          "ParseSymbols->Automatic}");
          
      PMATH_RUN("Options(OpenAppend):=Options(OpenWrite):={"
          "BinaryFormat->False,"
          "CharacterEncoding->Automatic,"
          "PageWidth:>80}");
          
      PMATH_RUN("Options(OpenRead):={"
          "BinaryFormat->False,"
          "CharacterEncoding->Automatic}");
          
      PMATH_RUN("Options(RandomReal):={WorkingPrecision->MachinePrecision}");
      
      PMATH_RUN("Options(ReadList):={RecordLists->False}");
      
      PMATH_RUN(
          "Options(Refresh):={"
          "TrackedSymbols->Automatic,"
          "UpdateInterval->Infinity"
          "}");
          
      PMATH_RUN("Options(ReplacePart):={Heads->Automatic}");
      
      PMATH_RUN("Options(RotationBox):={BoxRotation->0}");
      
      PMATH_RUN("Options(SeedRandom):={Method->Automatic}");
      
      PMATH_RUN("Options(StringCases):=Options(StringCount):={IgnoreCase->False,Overlaps->False}");
      PMATH_RUN("Options(StringPosition):={IgnoreCase->False,Overlaps->True}");
      
      PMATH_RUN("Options(StringToBoxes):={\"IgnoreSyntaxErrors\"->False,\"Tokens\"->String,Whitespace->False}");
    }
    
    flint_set_num_threads(_pmath_processor_count());
    _pmath_symbol_builtins_protect_all();
    
    { // loading maininit.pmath
      pmath_string_t exe = pmath_symbol_get_value(PMATH_SYMBOL_APPLICATIONFILENAME);
      
#ifdef PMATH_OS_UNIX
#define OS_SPECIFIC_PATH \
  "{\"/etc/pmath\"," \
  "\"/etc/local/pmath\"," \
  "\"/usr/share/pmath\"," \
  "\"/usr/local/share/pmath\"},"
#elif defined(PMATH_OS_WIN32)
#define OS_SPECIFIC_PATH \
  "(If(# =!= $Failed,{ToFileName(#,\"pmath\")},{})&)(Environment(\"CommonProgramFiles\"))," \
  "(If(# =!= $Failed,{ToFileName(#,\"pmath\")},{})&)(Environment(\"ProgramFiles\")),"
#else
#define OS_SPECIFIC_PATH ""
#endif
      
      if(pmath_is_string(exe)) {
        exe = pmath_expr_new_extended(
            pmath_ref(PMATH_SYMBOL_LIST), 1,
            pmath_expr_new_extended(
                pmath_ref(PMATH_SYMBOL_DIRECTORYNAME), 1,
                exe));
      }
      else {
        exe = pmath_expr_new(pmath_ref(PMATH_SYMBOL_LIST), 0);
      }
      
      PMATH_RUN_ARGS("Get(\"maininit`\","
          "Path->Join("
          "`1`,"
          //"If($CommandLine =!= {},{DirectoryName($CommandLine[1])},{}),"
          "If(# =!= $Failed,{#},{})&(Environment(\"PMATH_BASEDIRECTORY\")),"
          OS_SPECIFIC_PATH
          "{}))",
          "(o)", exe);
    }
    
    if(!pmath_aborting())
      return TRUE;
      
    _pmath_status = PMATH_STATUS_DESTROYING;
    
  FAIL_STATIC_OBJECTS:
    pmath_unref(_pmath_object_complex_infinity);         _pmath_object_complex_infinity =         PMATH_NULL;
    pmath_unref(_pmath_object_emptylist);                _pmath_object_emptylist =                PMATH_NULL;
    pmath_unref(_pmath_object_get_load_message);         _pmath_object_get_load_message =         PMATH_NULL;
    pmath_unref(_pmath_object_pos_infinity);             _pmath_object_pos_infinity =             PMATH_NULL;
    pmath_unref(_pmath_object_neg_infinity);             _pmath_object_neg_infinity =             PMATH_NULL;
    pmath_unref(_pmath_object_loadlibrary_load_message); _pmath_object_loadlibrary_load_message = PMATH_NULL;
    pmath_unref(_pmath_object_memory_exception);         _pmath_object_memory_exception =         PMATH_NULL;
    pmath_unref(_pmath_object_multimatch);               _pmath_object_multimatch =               PMATH_NULL;
//    pmath_unref(_pmath_object_newsym_message);           _pmath_object_newsym_message =           PMATH_NULL;
    pmath_unref(_pmath_object_overflow);                 _pmath_object_overflow =                 PMATH_NULL;
    pmath_unref(_pmath_object_range_from_one);           _pmath_object_range_from_one =           PMATH_NULL;
    pmath_unref(_pmath_object_range_from_zero);          _pmath_object_range_from_zero =          PMATH_NULL;
    pmath_unref(_pmath_object_singlematch);              _pmath_object_singlematch =              PMATH_NULL;
    pmath_unref(_pmath_object_stop_message);             _pmath_object_stop_message =             PMATH_NULL;
    pmath_unref(_pmath_object_underflow);                _pmath_object_underflow =                PMATH_NULL;
    pmath_unref(_pmath_object_zeromultimatch);           _pmath_object_zeromultimatch =           PMATH_NULL;
    pmath_unref(_pmath_object_empty_pattern_sequence);   _pmath_object_empty_pattern_sequence =   PMATH_NULL;
    
    _pmath_thread_clean(TRUE);
    _pmath_symbols_almost_done();
    
    _pmath_dynamic_done();
  FAIL_DYNAMIC:           _pmath_modules_done();
  FAIL_MODULES:           _pmath_regex_done();
  FAIL_REGEX:             _pmath_threadpool_done();
  FAIL_THREADPOOL:        _pmath_threadmsg_done();
  FAIL_THREADMSG:         _pmath_custom_objects_done();
  FAIL_CUSTOM:            _pmath_symbol_builtins_done();
  FAIL_BUILTINS:          _pmath_symbol_values_done();
  FAIL_SYMBOL_VALUES:     _pmath_numeric_done();
  FAIL_NUMERIC:           _pmath_symbols_done();
  FAIL_SYMBOLS:           _pmath_packed_arrays_done();
  FAIL_PACKED_ARRAYS:     _pmath_expressions_done();
  FAIL_EXPRESSIONS:       _pmath_numbers_done();
  FAIL_NUMBERS:           _pmath_strings_done();
  FAIL_STRINGS:           _pmath_objects_done();
  FAIL_OBJECTS:           _pmath_charnames_done();
  FAIL_CHARNAMES:         _pmath_threadlocks_done();
  FAIL_THREADLOCKS:       _pmath_thread_set_current(NULL);
    _pmath_thread_free(thread);
  FAIL_THIS_THREAD:       _pmath_threads_done();
  FAIL_THREADS:           _pmath_memory_manager_done();
  FAIL_MEMORY_MANAGER:
  FAIL_STACKS_LIBRARY:    _pmath_debug_library_done();
  FAIL_DEBUG_LIBRARY:
    _pmath_status = PMATH_STATUS_NONE;
    pmath_atomic_fetch_add(&pmath_count, -1);
    return FALSE;
  }
  else {
    while(_pmath_status != PMATH_STATUS_RUNNING &&
        _pmath_status != PMATH_STATUS_INITIALIZING)
    {
    }
    
    thread = _pmath_thread_new(NULL);
    
    if(!thread) {
      pmath_atomic_fetch_add(&pmath_count, -1);
      return FALSE;
    }
    
    _pmath_thread_set_current(thread);
  }
  
  return TRUE;
}

PMATH_API void pmath_done(void) {
  intptr_t thread_count;
  pmath_thread_t thread;
  
  assert(pmath_count._data > 0);
  
  while(_pmath_status != PMATH_STATUS_RUNNING) {
  }
  
  thread = pmath_thread_get_current();
  if(!thread)
    return;
    
  thread_count = pmath_atomic_read_aquire(&pmath_count);
  if( !thread->is_daemon &&
      thread_count == pmath_atomic_read_aquire(&_pmath_threadpool_deamon_count) + 1)
  {
    _pmath_threadpool_kill_daemons();
  }
  
  thread_count = pmath_atomic_fetch_add(&pmath_count, -1) - 1;
  if(thread_count == 0) {
    _pmath_status = PMATH_STATUS_DESTROYING;
    
    pmath_unref(_pmath_object_complex_infinity);         _pmath_object_complex_infinity =         PMATH_NULL;
    pmath_unref(_pmath_object_emptylist);                _pmath_object_emptylist =                PMATH_NULL;
    pmath_unref(_pmath_object_get_load_message);         _pmath_object_get_load_message =         PMATH_NULL;
    pmath_unref(_pmath_object_pos_infinity);             _pmath_object_pos_infinity =             PMATH_NULL;
    pmath_unref(_pmath_object_neg_infinity);             _pmath_object_neg_infinity =             PMATH_NULL;
    pmath_unref(_pmath_object_loadlibrary_load_message); _pmath_object_loadlibrary_load_message = PMATH_NULL;
    pmath_unref(_pmath_object_memory_exception);         _pmath_object_memory_exception =         PMATH_NULL;
    pmath_unref(_pmath_object_multimatch);               _pmath_object_multimatch =               PMATH_NULL;
//    pmath_unref(_pmath_object_newsym_message);           _pmath_object_newsym_message =           PMATH_NULL;
    pmath_unref(_pmath_object_overflow);                 _pmath_object_overflow =                 PMATH_NULL;
    pmath_unref(_pmath_object_range_from_one);           _pmath_object_range_from_one =           PMATH_NULL;
    pmath_unref(_pmath_object_range_from_zero);          _pmath_object_range_from_zero =          PMATH_NULL;
    pmath_unref(_pmath_object_singlematch);              _pmath_object_singlematch =              PMATH_NULL;
    pmath_unref(_pmath_object_stop_message);             _pmath_object_stop_message =             PMATH_NULL;
    pmath_unref(_pmath_object_underflow);                _pmath_object_underflow =                PMATH_NULL;
    pmath_unref(_pmath_object_zeromultimatch);           _pmath_object_zeromultimatch =           PMATH_NULL;
    pmath_unref(_pmath_object_empty_pattern_sequence);   _pmath_object_empty_pattern_sequence =   PMATH_NULL;
    
    _pmath_thread_clean(TRUE);
    _pmath_symbols_almost_done();
    
    _pmath_dynamic_done();
    _pmath_modules_done();
    _pmath_regex_done();
    _pmath_threadpool_done();
    _pmath_threadmsg_done();
    _pmath_custom_objects_done();
    _pmath_symbol_builtins_done();
    _pmath_symbol_values_done();
    _pmath_numeric_done();
    _pmath_symbols_done();
    _pmath_packed_arrays_done();
    _pmath_expressions_done();
    _pmath_numbers_done();
    _pmath_strings_done();
    _pmath_objects_done();
    _pmath_charnames_done();
    _pmath_threadlocks_done();
    _pmath_thread_free(pmath_thread_get_current());
    _pmath_thread_set_current(NULL);
    _pmath_threads_done();
    _pmath_memory_manager_done();
    _pmath_debug_library_done();
    
    _pmath_status = PMATH_STATUS_NONE;
  }
  else {
    _pmath_thread_set_current(NULL);
    _pmath_thread_free(thread);
    mpfr_free_cache();
    flint_cleanup();
  }
}

#ifdef PMATH_OS_WIN32
static pmath_bool_t check_min_os_version(int major, int minor, int service_pack) {
  OSVERSIONINFOEXW versionInfoEx;
  
  const DWORDLONG dwlConditionMask =
      VerSetConditionMask(
          VerSetConditionMask(
              VerSetConditionMask(
                  0,
                  VER_MAJORVERSION,
                  VER_GREATER_EQUAL),
              VER_MINORVERSION,
              VER_GREATER_EQUAL),
          VER_SERVICEPACKMAJOR,
          VER_GREATER_EQUAL);
          
  memset(&versionInfoEx, 0, sizeof(versionInfoEx));
  versionInfoEx.dwOSVersionInfoSize = sizeof(versionInfoEx);
  
  versionInfoEx.dwMajorVersion = major;
  versionInfoEx.dwMinorVersion = minor;
  versionInfoEx.wServicePackMajor = service_pack;
  
  return VerifyVersionInfoW(
      &versionInfoEx,
      VER_MAJORVERSION | VER_MINORVERSION | VER_SERVICEPACKMAJOR,
      dwlConditionMask) != FALSE;
}

static pmath_bool_t is_vista_or_newer(void) {
  return check_min_os_version(
      HIBYTE(_WIN32_WINNT_VISTA),
      LOBYTE(_WIN32_WINNT_VISTA),
      0);
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {

// If pthreads are used, this function will be called elsewhere.
#ifdef PMATH_USE_WINDOWS_THREADS
  if(fdwReason == DLL_THREAD_DETACH)
    _pmath_thread_destructed();
#endif
    
  switch(fdwReason) {
    case DLL_PROCESS_ATTACH:
      {
        pmath_bool_t dynamic_loading = lpvReserved == NULL;
#ifdef PMATH_DEBUG_LOG
        fprintf(stderr, "[+ process threadid=%u %s]\n",
            (unsigned)GetCurrentThreadId(),
            dynamic_loading ? "dynamic load" : "static load");
#endif
        if(dynamic_loading && !is_vista_or_newer()) {
          /* __declspec(thread) variables do not work correctly in dynamically
             loaded DLLs under older Windows versions. This was fixed in Vista,
             see [1].
             The thread-safe build of MPFR uses __declspec(thread).
          
             [1] Thread Local Storage, part 7: Windows Vista support for __declspec(thread) in demand loaded DLLs
                 (http://www.nynaeve.net/?p=189)
           */
          return FALSE;
        }
      }
      break;
      
    case DLL_PROCESS_DETACH:
      {
#ifdef PMATH_DEBUG_LOG
        fprintf(stderr, "[- process threadid=%u %s]\n",
            (unsigned)GetCurrentThreadId(),
            lpvReserved ? "terminate" : "dynamic unload");
#endif
      }
      break;
  }
  
  return TRUE;
}
#elif defined(__GNUC__) && defined(PMATH_DEBUG_LOG)
static
__attribute__((__constructor__))
void shared_object_pmath_init(void) {
  fprintf(stderr, "[+ process]\n");
}

static
__attribute__((__destructor__))
void shared_object_pmath_done(void) {
  fprintf(stderr, "[- process]\n");
}
#endif
