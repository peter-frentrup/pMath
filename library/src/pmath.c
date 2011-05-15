#include <pmath-builtins/io-private.h>
#include <pmath-util/stacks-private.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <pmath.h>
#include <pmath-private.h>

#include <pmath-core/objects-private.h>
#include <pmath-core/strings-private.h>
#include <pmath-core/expressions-private.h>
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
#include <pmath-util/symbol-values-private.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/number-theory-private.h>

#ifdef PMATH_OS_WIN32
  #define NOGDI
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
  #include <shellapi.h>
#elif defined(__APPLE__)
  #include <CoreServices/CoreServices.h>
#else
  #include <dlfcn.h>
  #include <link.h>
  #include <unistd.h>
#endif

#if PMATH_USE_PTHREAD
  #include <pthread.h>
#endif

#include <time.h>


PMATH_STATIC_ASSERT(sizeof(intptr_t) == sizeof(void*));
PMATH_STATIC_ASSERT(8*sizeof(void*) == PMATH_BITSIZE);
PMATH_STATIC_ASSERT(sizeof(pmath_t) == 8);

#ifdef PMATH_OS_WIN32
PMATH_STATIC_ASSERT(sizeof(wchar_t) == sizeof(uint16_t));
#endif


static volatile enum{
  PMATH_STATUS_NONE,
  PMATH_STATUS_INITIALIZING,
  PMATH_STATUS_RUNNING,
  PMATH_STATUS_DESTROYING
} _pmath_status = PMATH_STATUS_NONE;

static pmath_atomic_t pmath_count = PMATH_ATOMIC_STATIC_INIT;

PMATH_PRIVATE
pmath_bool_t _pmath_is_running(void){
  return _pmath_status == PMATH_STATUS_RUNNING;
}

static pmath_expr_t get_command_line(void){
  pmath_gather_begin(PMATH_NULL);
  #ifdef PMATH_OS_WIN32
  {
    int argc;
    wchar_t **argv = CommandLineToArgvW(GetCommandLineW(), &argc);

    if(argv){
      int i;
      for(i = 0;i < argc;++i)
        pmath_emit(pmath_string_insert_ucs2(PMATH_NULL, 0, argv[i], -1), PMATH_NULL);

      LocalFree(argv);
    }
  }
  #else
  {
    FILE *f = fopen("/proc/self/cmdline", "r");

    if(f){
      pmath_string_t str = PMATH_NULL;
      char buf[2];

      while(fgets(buf, sizeof(buf), f)){
        if(strlen(buf) == 0){
          if(!pmath_is_null(str)){
            pmath_emit(str, PMATH_NULL);
            str = PMATH_NULL;
          }
        }
        else
          str = pmath_string_concat(str, pmath_string_from_native(buf, -1));
      }

      fclose(f);
    }
    else{
      pmath_debug_print("cannot open /proc/self/cmdline\n");
    }
  }
  #endif

  return pmath_gather_end();
}

static void init_pagewidth(void){
  int width = 80;

  #ifdef PMATH_OS_WIN32
  {
    CONSOLE_SCREEN_BUFFER_INFO info;
    memset(&info, 0, sizeof(info));

    if(GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info)){
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

static pmath_expr_t get_exe_name(void){
  #ifdef PMATH_OS_WIN32
  {
    struct _pmath_string_t *s;
    uint16_t *buf;
    int len;
    DWORD  needed;

    len = 128;
    s = NULL;
    do{
      len*= 2;

      pmath_unref(PMATH_FROM_PTR(s));
      s = _pmath_new_string_buffer(len);
      if(!s)
        return PMATH_NULL;

      buf = AFTER_STRING(s);
      needed = GetModuleFileNameW(NULL, buf, (DWORD)len);
    }while(needed == (DWORD)len);

    s->length = (int)needed;
    return _pmath_from_buffer(s);
  }
  #elif defined(__APPLE__)
  {
    ProcessSerialNumber *psn;
    FSRef               *fsr;
    char                *path;

    if(GetCurrentProcess(&psn) == noErr
    && GetProcessBundleLocation(&psn, &fsr) == noErr
    && FSRefMakePath(&fsr, (UInt8*)path, sizeof(path))){
      return pmath_string_from_utf8(path, -1);
    }
  }
  #else
  {
    void *exe = dlopen(NULL, RTLD_LAZY);

    if(exe){
      void *main_sym;
      struct link_map *map = NULL;

      dlinfo(exe, RTLD_DI_LINKMAP, &map);
      if(map && map->l_name[0]){
        pmath_string_t s = pmath_string_from_native(map->l_name, -1);

        dlclose(exe);
        return s;
      }

      // only works if executable was compiled with -export-dynamic
      main_sym = dlsym(exe, "main");
      if(main_sym){
        Dl_info info;

        memset(&info, 0, sizeof(info));
        if(dladdr(main_sym, &info) && info.dli_fname){
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
      if((len = readlink("/proc/self/exe", buf, sizeof(buf))) != -1){
        return pmath_string_from_native(buf, len);
      }

      // Solaris has a /proc/<pid>/path/a.out symlink
      snprintf(filename, sizeof(filename), "/proc/%lu/path/a.out", (unsigned long)getpid());
      if((len = readlink(filename, buf, sizeof(buf))) != -1){
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

PMATH_API pmath_bool_t pmath_init(void){
  pmath_thread_t thread;

  if(pmath_atomic_fetch_add(&pmath_count, +1) == 0){
    while(_pmath_status != PMATH_STATUS_NONE){
    }

    _pmath_status = PMATH_STATUS_INITIALIZING;


//    #ifdef PMATH_OS_WIN32
//      SetErrorMode(SEM_NOOPENFILEERRORBOX);
//    #endif

    #ifdef PMATH_DEBUG_TESTS
    {
    // This does not test for atomicity.
      pmath_atomic_t a;
      intptr_t b;
      pmath_atomic2_t c;
      pmath_bool_t test;

      pmath_atomic_write_release(&a, 5);
      assert(a._data == 5);

      b = pmath_atomic_fetch_add(&a,2);
      assert(b == 5 && pmath_atomic_read_aquire(&a) == 7);
      assert(b == 5 && a._data                      == 7);

      b = pmath_atomic_fetch_set(&a,27);
      assert(b == 7 && a._data == 27);

      b = pmath_atomic_fetch_compare_and_set(&a, 26, 11);
      assert(b == 27 && a._data == 27);
      b = pmath_atomic_fetch_compare_and_set(&a, 27, 11);
      assert(b == 27 && a._data == 11);

      test = pmath_atomic_compare_and_set(&a, 13, 22);
      assert(!test && a._data == 11);
      test = pmath_atomic_compare_and_set(&a, 11, 22);
      assert(test && a._data == 22);

      if(pmath_atomic_have_cas2()){
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
    #endif

    _pmath_object_complex_infinity         = PMATH_NULL;
    _pmath_object_emptylist                = PMATH_NULL;
    _pmath_object_get_load_message         = PMATH_NULL;
    _pmath_object_infinity                 = PMATH_NULL;
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

    if(!_pmath_debug_library_init())          goto FAIL_DEBUG_LIBRARY;
    if(!_pmath_stacks_init())                 goto FAIL_STACKS_LIBRARY;
    if(!_pmath_memory_manager_init())         goto FAIL_MEMORY_MANAGER;

    #ifdef PMATH_DEBUG_TESTS
    {
      typedef struct{
        void *reserved;
        int value;
      }stack_test_item_t;
      stack_test_item_t a,b,c, *p;
      pmath_stack_t stack;

      a.value = 1;
      b.value = 2;
      c.value = 3;
      stack = pmath_stack_new();
      if(stack){
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
    #endif

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

    //{ init static objects ...
    // SystemException("OutOfMemory")
    _pmath_object_memory_exception = pmath_expr_new_extended(
      pmath_ref(PMATH_SYMBOL_SYSTEMEXCEPTION), 1,
      PMATH_C_STRING("OutOfMemory"));

    // Overflow()
    _pmath_object_overflow = pmath_expr_new(
      pmath_ref(PMATH_SYMBOL_OVERFLOW), 0);

    // Underflow()
    _pmath_object_underflow = pmath_expr_new(
      pmath_ref(PMATH_SYMBOL_UNDERFLOW), 0);

    // DirectedInfinity(1)
    _pmath_object_infinity = pmath_expr_new_extended(
      pmath_ref(PMATH_SYMBOL_DIRECTEDINFINITY), 1,
      PMATH_FROM_INT32(1));

    // DirectedInfinity()
    _pmath_object_complex_infinity = pmath_expr_new(
      pmath_ref(PMATH_SYMBOL_DIRECTEDINFINITY), 0);

    // Range(1, Automatic)
    _pmath_object_range_from_one = pmath_expr_new_extended(
      pmath_ref(PMATH_SYMBOL_RANGE), 2,
      PMATH_FROM_INT32(1),
      pmath_ref(PMATH_SYMBOL_AUTOMATIC));

    // Range(0, Automatic)
    _pmath_object_range_from_zero = pmath_expr_new_extended(
      pmath_ref(PMATH_SYMBOL_RANGE), 2,
      PMATH_FROM_INT32(0),
      pmath_ref(PMATH_SYMBOL_AUTOMATIC));

    // SingleMatch()
    _pmath_object_singlematch = pmath_expr_new(
      pmath_ref(PMATH_SYMBOL_SINGLEMATCH), 0);

    // Repeated(SingleMatch(), Range(1, /\/))
    _pmath_object_multimatch = pmath_expr_new_extended(
      pmath_ref(PMATH_SYMBOL_REPEATED), 2,
      pmath_ref(_pmath_object_singlematch),
      pmath_ref(_pmath_object_range_from_one));

    // Repeated(SingleMatch(), Range(0, /\/))
    _pmath_object_zeromultimatch = pmath_expr_new_extended(
      pmath_ref(PMATH_SYMBOL_REPEATED), 2,
      pmath_ref(_pmath_object_singlematch),
      pmath_ref(_pmath_object_range_from_zero));

    // List()
    _pmath_object_emptylist = pmath_expr_new(
      pmath_ref(PMATH_SYMBOL_LIST), 0);

    // MessageName(General, "stop")
    _pmath_object_stop_message = pmath_expr_new_extended(
      pmath_ref(PMATH_SYMBOL_MESSAGENAME), 2,
      pmath_ref(PMATH_SYMBOL_GENERAL),
      PMATH_C_STRING("stop"));

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

    // MessageName(LoadLibrary, "load")
    _pmath_object_get_load_message = pmath_expr_new_extended(
      pmath_ref(PMATH_SYMBOL_MESSAGENAME), 2,
      pmath_ref(PMATH_SYMBOL_GET),
      PMATH_C_STRING("load"));

    if(pmath_is_null(_pmath_object_complex_infinity)
    || pmath_is_null(_pmath_object_emptylist)
    || pmath_is_null(_pmath_object_get_load_message)
    || pmath_is_null(_pmath_object_infinity)
    || pmath_is_null(_pmath_object_loadlibrary_load_message)
    || pmath_is_null(_pmath_object_memory_exception)
    || pmath_is_null(_pmath_object_multimatch)
//    || pmath_is_null(_pmath_object_newsym_message)
    || pmath_is_null(_pmath_object_overflow)
    || pmath_is_null(_pmath_object_range_from_one)
    || pmath_is_null(_pmath_object_range_from_zero)
    || pmath_is_null(_pmath_object_singlematch)
    || pmath_is_null(_pmath_object_stop_message)
    || pmath_is_null(_pmath_object_underflow)
    || pmath_is_null(_pmath_object_zeromultimatch))
      goto FAIL_STATIC_OBJECTS;
    //}

    _pmath_status = PMATH_STATUS_RUNNING;

    //{ init threadlocal variables ...
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
    //} ... init threadlocal variables

    //{ initialize runs ...
    PMATH_RUN_ARGS("$ApplicationFileName:= `1`", "(o)", get_exe_name());
    
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

      local_tm.tm_year+=  1900;
      global_tm.tm_year+= 1900;

      loc_year_days  = 365 + (local_tm.tm_year % 4 == 0 && (local_tm.tm_year % 100 != 0 || local_tm.tm_year % 400 == 0));
      glob_year_days = 365 + (global_tm.tm_year % 4 == 0 && (global_tm.tm_year % 100 != 0 || global_tm.tm_year % 400 == 0));

      if(local_tm.tm_year < global_tm.tm_year){
        global_tm.tm_year++;
        global_tm.tm_yday+= loc_year_days;
      }
      else if(local_tm.tm_year > global_tm.tm_year){
        local_tm.tm_year++;
        local_tm.tm_yday+= glob_year_days;
      }

      loc  = local_tm.tm_sec  + 60*(local_tm.tm_min  + 60*(local_tm.tm_hour  + 24*local_tm.tm_yday));
      glob = global_tm.tm_sec + 60*(global_tm.tm_min + 60*(global_tm.tm_hour + 24*global_tm.tm_yday));

      PMATH_RUN_ARGS("$TimeZone:=`1`", "(f)", (glob - loc) / (60 * 60.0));
    }

    PMATH_RUN("IsNumeric(E):=True");
    PMATH_RUN("IsNumeric(EulerGamma):=True");
    PMATH_RUN("IsNumeric(Degree):=True");
    PMATH_RUN("IsNumeric(MachinePrecision):=True");
    PMATH_RUN("IsNumeric(Pi):=True");

    PMATH_RUN("N(Degree,~System`Private`x)::=N(Pi/180, System`Private`x)");
    PMATH_RUN("MakeBoxes(Degree)::=\"\\[Degree]\"");

    PMATH_RUN("Default(Ceiling,2):=1");
    PMATH_RUN("Default(Floor,2):=1");
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
      "Options(ParallelMap):="
      "Options(ParallelScan):="
      "Options(Replace):="
      "Options(ReplaceList):="
      "Options(ReplaceRepeated):="
      "Options(Scan):={Heads->False}");

    PMATH_RUN("Options(Complement):={SameTest->Automatic}");

    PMATH_RUN("Options(DateList):={TimeZone:>$TimeZone}");

    PMATH_RUN(
      "Options(Dynamic):="
      "Options(DynamicBox):={"
        "SynchronousUpdating->True,"
        "TrackedSymbols->Automatic"
        "}");

    PMATH_RUN("Options(BinaryRead):=Options(BinaryWrite):={"
      "ByteOrdering:>$ByteOrdering}");

    PMATH_RUN("Options(Button):={"
      "ButtonFrame->Automatic,"
      "Method->\"Preemptive\"}");

    PMATH_RUN("Options(ButtonBox):={"
      "ButtonFrame->Automatic,"
      "ButtonFunction->(/\\/ &),"
      "Method->\"Preemptive\"}");

    PMATH_RUN("Options(DeleteDirectory):={DeleteContents->False}");

    PMATH_RUN("Options(FileNames):={IgnoreCase->Automatic}");

    PMATH_RUN(
      "Options(Find):="
      "Options(FindList):="
      "Options(StringMatch):="
      "Options(StringReplace):="
      "Options(StringSplit):={IgnoreCase->False}");

    PMATH_RUN(
      "Options(FixedPoint):="
      "Options(FixedPointList):={SameTest->Identical}");

    PMATH_RUN("Options(Get):={Path:>$Path}");

    PMATH_RUN("Options(Grid):={"
      "ColumnSpacing->Inherited,"
      "RowSpacing->Inherited}");

    PMATH_RUN("Options(GridBox):={"
      "GridBoxColumnSpacing->Inherited,"
      "GridBoxRowSpacing->Inherited}");

    PMATH_RUN("Options(InterpretationBox):=Options(Interpretation):={"
      "AutoDelete->False,"
      "Editable->False"
      "}");

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
    PMATH_RUN("Options(TransformationBox):={BoxTransformation->{{1,0},{0,1}}}");

    PMATH_RUN("Options(StringCases):=Options(StringCount):={IgnoreCase->False,Overlaps->False}");
    PMATH_RUN("Options(StringPosition):={IgnoreCase->False,Overlaps->True}");

    PMATH_RUN("Options(Style):=Options(StyleBox):={"
      "Antialiasing->Inherited,"
      "AutoDelete->False,"
      "AutoNumberFormating->Inherited,"
      "AutoSpacing->Inherited,"
      "Background->None,"
      "BaseStyle->None,"
      "ButtonFrame->Inherited,"
      "ButtonFunction->Inherited,"
      "Editable->Inherited,"
      "FontColor->Inherited,"
      "FontFamily->Inherited,"
      "FontSize->Inherited,"
      "FontSlant->Inherited,"
      "FontWeight->Inherited,"
      "GridBoxColumnSpacing->Inherited,"
      "GridBoxRowSpacing->Inherited,"
      "LineBreakWithin->Inherited,"
      "Placeholder->Inherited,"
      "ScriptSizeMultipliers->Inherited,"
      "Selectable->Inherited,"
      "ShowAutoStyles->Inherited,"
      "ShowStringCharacters->Inherited,"
      "StripOnInput->True,"
      "TextShadow->Inherited"
      "}");

    PMATH_RUN("Options(Section):={"
      "Antialiasing->Automatic,"
      "AutoNumberFormating->Inherited,"
      "AutoSpacing->Inherited,"
      "Background->Automatic,"
      "BaseStyle->None,"
      "ButtonFrame->Automatic,"
      "ButtonFunction->(/\\/ &),"
      "Editable->True,"
      "Evaluatable->False,"
      "FontColor->GrayLevel(0),"
      "FontFamily->\"Times\","
      "FontSize->10,"
      "FontSlant->Plain,"
      "FontWeight->Plain,"
      "GridBoxColumnSpacing->0.5,"
      "GridBoxRowSpacing->0.5,"
      "LineBreakWithin->True,"
      "ScriptSizeMultipliers->Automatic,"
      "SectionFrame->False,"
      "SectionFrameColor->GrayLevel(0),"
      "SectionFrameMargins->8,"
      "SectionGenerated->False,"
      "SectionGroupPrecedence->0,"
      "SectionMargins->{{60, 30}, {4, 4}},"
      "SectionLabel->None,"
      "SectionLabelAutoDelete->True,"
      "Selectable->Inherited,"
      "ShowAutoStyles->True,"
      "ShowSectionBracket->True,"
      "ShowStringCharacters->True,"
      "TextShadow->None"
      "}");

    PMATH_RUN("Options(Slider):=Options(SliderBox):={ContinuousAction->True}");
    
    PMATH_RUN("Options(Tooltip):=Options(TooltipBox):={StripOnInput->True}");
    
    //} ... initialization runs

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

      if(pmath_is_string(exe)){
        exe = pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_LIST), 1,
          pmath_expr_new_extended(
            pmath_ref(PMATH_SYMBOL_DIRECTORYNAME), 1,
            exe));
      }
      else{
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
    pmath_unref(_pmath_object_infinity);                 _pmath_object_infinity =                 PMATH_NULL;
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
   FAIL_SYMBOLS:           _pmath_expressions_done();
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
  else{
    while(_pmath_status != PMATH_STATUS_RUNNING
    &&    _pmath_status != PMATH_STATUS_INITIALIZING){
    }

    thread = _pmath_thread_new(NULL);

    if(!thread){
      pmath_atomic_fetch_add(&pmath_count, -1);
      return FALSE;
    }

    _pmath_thread_set_current(thread);
  }

  return TRUE;
}

PMATH_API void pmath_done(void){
  intptr_t thread_count;
  pmath_thread_t thread;

  assert(pmath_count._data > 0);

  while(_pmath_status != PMATH_STATUS_RUNNING){
  }

  thread = pmath_thread_get_current();
  if(!thread)
    return;

  thread_count = pmath_atomic_read_aquire(&pmath_count);
  if(!thread->is_daemon
  && thread_count == pmath_atomic_read_aquire(&_pmath_threadpool_deamon_count) + 1){
    _pmath_threadpool_kill_daemons();
  }

  thread_count = pmath_atomic_fetch_add(&pmath_count, -1) - 1;
  if(thread_count == 0){
    _pmath_status = PMATH_STATUS_DESTROYING;

    pmath_unref(_pmath_object_complex_infinity);         _pmath_object_complex_infinity =         PMATH_NULL;
    pmath_unref(_pmath_object_emptylist);                _pmath_object_emptylist =                PMATH_NULL;
    pmath_unref(_pmath_object_get_load_message);         _pmath_object_get_load_message =         PMATH_NULL;
    pmath_unref(_pmath_object_infinity);                 _pmath_object_infinity =                 PMATH_NULL;
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
  else{
    _pmath_thread_set_current(NULL);
    _pmath_thread_free(thread);
    mpfr_free_cache();
  }
}

#ifdef PMATH_DEBUG_LOG
  #if defined(PMATH_OS_WIN32)
    BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved){
      switch (fdwReason){
        case DLL_PROCESS_ATTACH:
          fprintf(stderr, "[+ process]\n");
          break;

        case DLL_PROCESS_DETACH:
          fprintf(stderr, "[- process]\n");
          break;

        case DLL_THREAD_ATTACH:
          fprintf(stderr, "[+ thread]\n");
          break;

        case DLL_THREAD_DETACH:
          fprintf(stderr, "[- thread]\n");
          break;
      }
      return TRUE; // succesful
    }
  #elif defined(__GNUC__)
    static
    __attribute__((__constructor__))
    void shared_object_pmath_init(void){
      fprintf(stderr, "[+ process]\n");
    }

    static
    __attribute__((__destructor__))
    void shared_object_pmath_done(void){
      fprintf(stderr, "[- process]\n");
    }
  #endif
#endif
