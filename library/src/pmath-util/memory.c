#include <assert.h>
#include <inttypes.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <pmath-config.h>
#include <pmath-types.h>
#include <pmath-core/objects.h>
#include <pmath-core/expressions.h>
#include <pmath-core/strings.h>
#include <pmath-core/symbols.h>
#include <pmath-core/numbers.h>

#include <pmath-util/concurrency/atomic.h>
#include <pmath-util/concurrency/threadlocks.h>
#include <pmath-util/concurrency/threadlocks-private.h>
#include <pmath-util/concurrency/threads.h>
#include <pmath-util/debug.h>
#include <pmath-util/hashtables-private.h>
#include <pmath-util/memory.h>

#include <pmath-language/regex-private.h>

#include <pmath-core/objects-inline.h>
#include <pmath-core/objects-private.h>
#include <pmath-core/numbers-private.h>
#include <pmath-core/symbols-private.h>

#ifdef PMATH_USE_DLMALLOC
  #include <pmath-util/dlmalloc.h>
#else
  #include <malloc.h>
#endif

#if defined(PMATH_DEBUG_MEMORY) && PMATH_USE_PTHREAD
  #include <pthread.h>
#endif

#ifdef PMATH_OS_WIN32
  #ifndef _WIN32_WINNT
    #define _WIN32_WINNT 0x0501 /* Windows XP for HEAP_INFORMATION_CLASS */
  #endif
  #define NOGDI
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
#endif

#define round_up_int(x, r) ((((x) + (r) - 1) / (r)) * (r))

#ifdef PMATH_USE_DLMALLOC
  #define memory_allocate(s)      dlmalloc((s))
  #define memory_reallocate(p,s)  dlrealloc((p), (s))
  #define memory_free(p)          dlfree((p))
  #define memory_size(p)          dlmalloc_usable_size((p))

  #define init_platform_memory_manager(o) ((void)0)
#elif defined(PMATH_OS_WIN32)
/* Using Low-fragmentation Heap on Windows (XP, Server 2003 or newer). 
   See http://msdn2.microsoft.com/en-us/library/aa366750.aspx
 */
  static HANDLE heap;

  #define memory_allocate(size)       HeapAlloc(heap, 0, (size))
  #define memory_reallocate(p, size)  HeapReAlloc(heap, 0, (p), (size))
  #define memory_free(p)              HeapFree(heap, 0, (p))
  #define memory_size(p)              HeapSize(heap, 0, (p))

  typedef BOOL (WINAPI *heap_set_information_func_t)(
    HANDLE,
    HEAP_INFORMATION_CLASS,
    void*,
    size_t);

  static void init_platform_memory_manager(void){
    HMODULE kernel32;
    ULONG low_fragmentation_heap = 2;
    heap_set_information_func_t hsi;
    
    heap = GetProcessHeap();

    kernel32 = GetModuleHandleA("Kernel32");
    
    if(!kernel32)
      return;

    hsi = (heap_set_information_func_t)
      GetProcAddress(kernel32, "HeapSetInformation");
    if(!hsi)
      return;

    hsi(
      heap,
      HeapCompatibilityInformation,
      &low_fragmentation_heap,
      sizeof(low_fragmentation_heap));
  }
#else
  #define memory_allocate(s)      malloc((s))
  #define memory_reallocate(p,s)  realloc((p), (s))
  #define memory_free(p)          free((p))
  #define memory_size(p)          malloc_usable_size((p))

  #define init_platform_memory_manager(o) ((void)0)
#endif

#ifdef PMATH_DEBUG_MEMORY
  static size_t stat_sizes[] = {4,8,12,16,20,24,28,32,48,56,64,72,80,96,128,192,256,512,1024};
  #define STAT_SIZES (sizeof(stat_sizes)/sizeof(stat_sizes[0]))
  static size_t alloc_stats[  1 + STAT_SIZES];
  static size_t realloc_stats[1 + STAT_SIZES];

  typedef struct _memory_header_t{
    struct _memory_header_t  *next;
    struct _memory_header_t  *prev;
    size_t                    size;
    intptr_t                  alloc_time;
  }memory_header_t;

  PMATH_PRIVATE PMATH_DECLARE_ATOMIC(_pmath_debug_global_time) = 0;

  #define DEBUG_UNDERFLOW       "UNDERFLOW"
  #define DEBUG_UNDERFLOW_SIZE  9
  #define DEBUG_OVERFLOW        "OVERFLOW"
  #define DEBUG_OVERFLOW_SIZE   8
  #define DEBUG_HEADER_SIZE  round_up_int(sizeof(memory_header_t) + DEBUG_UNDERFLOW_SIZE, 2*sizeof(/*(void*)*/size_t))

  #define DEBUG_MEM_TO_HEADER(  p)  ((memory_header_t*)(((char*)(p)) - DEBUG_HEADER_SIZE))
  #define DEBUG_MEM_FROM_HEADER(p)  (((char*)(p)) + DEBUG_HEADER_SIZE)

  #if PMATH_USE_PTHREAD
    static pthread_mutex_t   mem_list_mutex;
  #elif PMATH_USE_WINDOWS_THREADS
    static CRITICAL_SECTION  mem_list_mutex;
  #endif
  static memory_header_t  *mem_list;

  static size_t stat_size_class(size_t size){
    size_t i;
    for(i = 0;i < STAT_SIZES;i++)
      if(size <= stat_sizes[i])
        return i;
    return i;
  }

  static void debug_mem_show_extract(void *p){
    memory_header_t *header;
    #define EXTRACT_BYTE_COUNT 30
    char extract[1 + 3*EXTRACT_BYTE_COUNT +  2  + EXTRACT_BYTE_COUNT + 2];
    //          "["  "hh "                 "= "   "c"                 "]\0"
    char *out;
    char *in;
    size_t i;
    
    if(!p)
      return;
    
    header = DEBUG_MEM_TO_HEADER(p);
    
    out = extract;
    *out = '['; out++;

    in  = (char*)p;
    for(i = 0;i < EXTRACT_BYTE_COUNT;i++){
      if(i >= header->size){
        *out = '.'; out++;
        *out = '.'; out++;
      }
      else{
        *out = "0123456789abcdef"[(((unsigned char)in[i]) & 0xF0) >> 4];
        out++;
        *out = "0123456789abcdef"[ ((unsigned char)in[i]) & 0x0F];
        out++;
      }
      *out = ' '; out++;
    }
    *out = '='; out++;
    *out = ' '; out++;
    for(i = 0;i < EXTRACT_BYTE_COUNT;i++){
      if(i >= header->size){
        *out = '.';
        out++;
      }
      else{
        *out = ((unsigned char)in[i] >= ' ' ? in[i] : '.');
        out++;
      }
    }
    *out = ']'; out++;
    *out = '\0';

    pmath_debug_print("%s", extract);
  }

  static void debug_mem_init_range_check(memory_header_t *header){
    memcpy(
      DEBUG_MEM_FROM_HEADER(header) - DEBUG_UNDERFLOW_SIZE,
      DEBUG_UNDERFLOW,
      DEBUG_UNDERFLOW_SIZE);

    memcpy(
      DEBUG_MEM_FROM_HEADER(header) + header->size,
      DEBUG_OVERFLOW,
      DEBUG_OVERFLOW_SIZE);
  }

  static void debug_mem_delete_range_check(memory_header_t *header){
    memset(
      DEBUG_MEM_FROM_HEADER(header) - DEBUG_UNDERFLOW_SIZE,
      '?',
      DEBUG_UNDERFLOW_SIZE);

    memset(
      DEBUG_MEM_FROM_HEADER(header) + header->size,
      '?',
      DEBUG_OVERFLOW_SIZE);
  }

  static void debug_mem_check(void *p){
    memory_header_t *header;
    
    if(!p)
      return;
    
    header = DEBUG_MEM_TO_HEADER(p);
//    if(size != (memory_header_t*)p)->size){
//      pmath_debug_print("<%p> INVALID SIZE (%"PRIdPTR" != %"PRIdPTR") ",
//        p, (memory_header_t*)p)->size, size);
//      debug_mem_show_extract(p);
//      pmath_debug_print("\n\a");
//      return;
//    }
    if(0 != memcmp(DEBUG_MEM_FROM_HEADER(header) - DEBUG_UNDERFLOW_SIZE,
                   DEBUG_UNDERFLOW,
                   DEBUG_UNDERFLOW_SIZE))
    {
      pmath_debug_print("<%p, t=%"PRIuPTR"> UNDERFLOW at t=%"PRIuPTR" ",
        p, header->alloc_time, _pmath_debug_global_time);
      debug_mem_show_extract(p);
      pmath_debug_print("\n\a");
      return;
    }

    if(0 != memcmp(DEBUG_MEM_FROM_HEADER(header) + header->size,
                   DEBUG_OVERFLOW,
                   DEBUG_OVERFLOW_SIZE))
    {
      pmath_debug_print("<%p, t=%"PRIuPTR"> OVERFLOW at t=%"PRIuPTR" ",
        p, header->alloc_time, _pmath_debug_global_time);
      debug_mem_show_extract(p);
      pmath_debug_print("\n\a");
      return;
    }
  }

  static void debug_mem_mark_uninitialized(void *p, size_t size){
    memset(p, 0xAB, size);
  }

  static void *debug_mem_alloc(size_t size){
    memory_header_t *p;
    
    (void)pmath_atomic_fetch_add(
      (intptr_t*)&alloc_stats[stat_size_class(size)],
      1);

    p = (memory_header_t*)memory_allocate(DEBUG_HEADER_SIZE + size + DEBUG_OVERFLOW_SIZE);
    if(!p)
      return NULL;

    p->size = size;
    p->alloc_time = pmath_atomic_fetch_add(&_pmath_debug_global_time, 1);
    debug_mem_init_range_check(p);

    {
      #if PMATH_USE_PTHREAD
        pthread_mutex_lock(&mem_list_mutex);
      #elif PMATH_USE_WINDOWS_THREADS
        EnterCriticalSection(&mem_list_mutex);
      #endif

      p->prev = NULL;
      p->next = mem_list;
      if(mem_list)
        mem_list->prev = p;
      mem_list = p;

      #if PMATH_USE_PTHREAD
        pthread_mutex_unlock(&mem_list_mutex);
      #elif PMATH_USE_WINDOWS_THREADS
        LeaveCriticalSection(&mem_list_mutex);
      #endif
    }

    debug_mem_mark_uninitialized(DEBUG_MEM_FROM_HEADER(p), size);
    //pmath_debug_print("malloc() -> %p\n", DEBUG_MEM_FROM_HEADER(p));
    return DEBUG_MEM_FROM_HEADER(p);
  }

  static void *debug_mem_realloc(void *p, size_t size){
    memory_header_t *old_p;
    memory_header_t *new_p;
    size_t old_size;
    
    if(!p)
      return debug_mem_alloc(size);
    debug_mem_check(p);

    (void)pmath_atomic_fetch_add(
      (intptr_t*)&realloc_stats[stat_size_class(size)],
      1);

    old_p = DEBUG_MEM_TO_HEADER(p);
    old_size = old_p->size;
    debug_mem_delete_range_check(old_p);

    {
      #if PMATH_USE_PTHREAD
        pthread_mutex_lock(&mem_list_mutex);
      #elif PMATH_USE_WINDOWS_THREADS
        EnterCriticalSection(&mem_list_mutex);
      #endif

      if(old_p->prev)
        old_p->prev->next = old_p->next;
      if(old_p->next)
        old_p->next->prev = old_p->prev;
      if(old_p == mem_list)
        mem_list = old_p->next;

      #if PMATH_USE_PTHREAD
        pthread_mutex_unlock(&mem_list_mutex);
      #elif PMATH_USE_WINDOWS_THREADS
        LeaveCriticalSection(&mem_list_mutex);
      #endif
    }

    new_p = (memory_header_t*)memory_reallocate(
      old_p, 
      DEBUG_HEADER_SIZE + size + DEBUG_OVERFLOW_SIZE);
      
    if(!new_p){
      debug_mem_init_range_check(old_p);
      {
        #if PMATH_USE_PTHREAD
          pthread_mutex_lock(&mem_list_mutex);
        #elif PMATH_USE_WINDOWS_THREADS
          EnterCriticalSection(&mem_list_mutex);
        #endif

        old_p->prev = NULL;
        old_p->next = mem_list;
        if(mem_list)
          mem_list->prev = old_p;
        mem_list = old_p;

        #if PMATH_USE_PTHREAD
          pthread_mutex_unlock(&mem_list_mutex);
        #elif PMATH_USE_WINDOWS_THREADS
          LeaveCriticalSection(&mem_list_mutex);
        #endif
      }
      return NULL;
    }

    if(old_size < size){
      debug_mem_mark_uninitialized(
        DEBUG_MEM_FROM_HEADER(new_p) + old_size,
        size - old_size);
    }
    new_p->size = size;
    new_p->alloc_time = pmath_atomic_fetch_add(&_pmath_debug_global_time, 1);

    debug_mem_init_range_check(new_p);

    {
      #if PMATH_USE_PTHREAD
        pthread_mutex_lock(&mem_list_mutex);
      #elif PMATH_USE_WINDOWS_THREADS
        EnterCriticalSection(&mem_list_mutex);
      #endif

      new_p->prev = NULL;
      new_p->next = mem_list;
      if(mem_list)
        mem_list->prev = new_p;
      mem_list = new_p;

      #if PMATH_USE_PTHREAD
        pthread_mutex_unlock(&mem_list_mutex);
      #elif PMATH_USE_WINDOWS_THREADS
        LeaveCriticalSection(&mem_list_mutex);
      #endif
    }

    //pmath_debug_print("realloc(%p) -> %p\n", p, DEBUG_MEM_FROM_HEADER(new_p));
    return DEBUG_MEM_FROM_HEADER(new_p);
  }

  static void debug_mem_free(void *p){
    memory_header_t *old_p;
    
    if(!p)
      return;
    debug_mem_check(p);
    
    old_p = DEBUG_MEM_TO_HEADER(p);

    {
      #if PMATH_USE_PTHREAD
        pthread_mutex_lock(&mem_list_mutex);
      #elif PMATH_USE_WINDOWS_THREADS
        EnterCriticalSection(&mem_list_mutex);
      #endif

      if(old_p->prev)
        old_p->prev->next = old_p->next;
      if(old_p->next)
        old_p->next->prev = old_p->prev;
      if(old_p == mem_list)
        mem_list = old_p->next;

      #if PMATH_USE_PTHREAD
        pthread_mutex_unlock(&mem_list_mutex);
      #elif PMATH_USE_WINDOWS_THREADS
        LeaveCriticalSection(&mem_list_mutex);
      #endif
    }

    debug_mem_delete_range_check(old_p);

    //pmath_debug_print("free(%p)\n", p);
    memory_free(old_p);
  }
  
  static size_t debug_mem_size(void *p){
    memory_header_t *old_p;
    
    assert(p != NULL);
    
    old_p = DEBUG_MEM_TO_HEADER(p);
    
    memory_size(old_p); // not used, but called to ensure this call is tested+needed in debug mode
    return old_p->size;
  }
  
  static void check_all_freed(void){
    /* no locking needed, because this is called only within
       _pmath_memory_manager_done from the main thread */
    if(mem_list){
      int max = 20;
      pmath_debug_print("\a");
      
      do{
        pmath_debug_print("<%p, t=%"PRIuPTR"> NOT FREED %"PRIuPTR" BYTES ",
          DEBUG_MEM_FROM_HEADER(mem_list),
          mem_list->alloc_time,
          mem_list->size);
        debug_mem_show_extract(DEBUG_MEM_FROM_HEADER(mem_list));
        pmath_debug_print("\n");
        mem_list = mem_list->next;
      }while(mem_list && --max > 0);
      
      if(mem_list)
        pmath_debug_print("...and more\n");
    }
  }

  #ifdef memory_allocate
    #undef memory_allocate
    #undef memory_reallocate
    #undef memory_free
    #undef memory_size
  #endif

  #define memory_allocate(s)       debug_mem_alloc((s))
  #define memory_reallocate(p, s)  debug_mem_realloc((p), (s))
  #define memory_free(p)           debug_mem_free((p))
  #define memory_size(p)           debug_mem_size((p))
#endif

static PMATH_DECLARE_ATOMIC(memory_in_use) = 0;

static PMATH_DECLARE_ATOMIC(max_memory_used) = 0;

  static void memory_panic(void){
    #ifdef PMATH_DEBUG_LOG
      intptr_t old_memory_in_use = memory_in_use;
    #endif
    pmath_debug_print("memory_panic() [memory_in_use = %"PRIuPTR"] ...\n",
      memory_in_use);

    _pmath_regex_memory_panic();
    _pmath_numbers_memory_panic();
    _pmath_symbols_memory_panic();
    _pmath_threadlocks_memory_panic();

    pmath_debug_print("...memory_panic() [safed memory: %"PRIdPTR"]\n",
      old_memory_in_use - memory_in_use);
  }

PMATH_API void *pmath_mem_alloc(size_t size){
  void *p = memory_allocate(size);
  size_t s;
  intptr_t max;
  
  if(PMATH_UNLIKELY(!p)){
    memory_panic();
    p = memory_allocate(size);
    if(!p){
      pmath_abort_please();
      return NULL;
    }
  }
  
  size = memory_size(p);
  s = size + (size_t)pmath_atomic_fetch_add(&memory_in_use, (intptr_t)size);
  while((size_t)(max = max_memory_used) < s){
    if(pmath_atomic_compare_and_set(
      &max_memory_used,
      max,
      (intptr_t)s))
        return p;
  }
  return p;
}

PMATH_API void *pmath_mem_realloc(void *p, size_t new_size){
  void *new_p = pmath_mem_realloc_no_failfree(p, new_size);

  if(PMATH_UNLIKELY(!new_p))
    pmath_mem_free(p);

  return new_p;
}

PMATH_API void *pmath_mem_realloc_no_failfree(void *p, size_t new_size){
  void *new_p;
  intptr_t size, max;
  size_t s, old;

  if(!p)
    return pmath_mem_alloc(new_size);

  if(new_size == 0){
    pmath_mem_free(p);
    return NULL;
  }
  
  old = memory_size(p);
  new_p = memory_reallocate(p, new_size);
  if(PMATH_UNLIKELY(!new_p)){
    memory_panic();
    new_p = memory_reallocate(p, new_size);
    if(!new_p){
      pmath_abort_please();
      return NULL;
    }
  }
  
  size = (intptr_t)memory_size(new_p) - (intptr_t)old;
  if(size < 0){
    (void)pmath_atomic_fetch_add(&memory_in_use, size);
    return new_p;
  }
  
  s = (size_t)size + (size_t)pmath_atomic_fetch_add(&memory_in_use, size);
  while((size_t)(max = max_memory_used) < s){
    if(pmath_atomic_compare_and_set(
      &max_memory_used,
      max,
      (intptr_t)s))
        return new_p;
  }
  return new_p;
}

PMATH_API void pmath_mem_free(void *p){
  if(p){
    (void)pmath_atomic_fetch_add(&memory_in_use, -(intptr_t)memory_size(p));
    memory_free(p);
  }
}

PMATH_API void pmath_mem_usage(size_t *current, size_t *max){
  if(current)
    *current = (size_t)memory_in_use;
    
  if(max)
    *max = (size_t)max_memory_used;
}

/*============================================================================*/

static void *pmath_gmp_realloc(void *p, size_t oldsize, size_t newsize){
  return pmath_mem_realloc(p, newsize);
}

static void pmath_gmp_free(void *p, size_t size){
  pmath_mem_free(p);
}

PMATH_PRIVATE pmath_bool_t _pmath_memory_manager_init(void){
  {
    max_memory_used = 0;
    init_platform_memory_manager();

    #ifdef PMATH_DEBUG_MEMORY
      mem_list = NULL;

        /* initialize mem_list_mutex ... */
      #if PMATH_USE_PTHREAD
        if(0 != pthread_mutex_init(&mem_list_mutex, NULL))
          goto FAIL_MEM_LIST_MUTEX;
      #elif PMATH_USE_WINDOWS_THREADS
        if(!InitializeCriticalSectionAndSpinCount(&mem_list_mutex, 4000))
          goto FAIL_MEM_LIST_MUTEX;
      #endif

      memset(alloc_stats,   0, sizeof(alloc_stats));
      memset(realloc_stats, 0, sizeof(realloc_stats));
    #endif
  }

/* GMP allocation functions must not return NULL. So we retry an allocation, if
   it failed, because caches are freed on memory failure, but
   pmath_mem_[re]alloc() return NULL then, even if cache-freeing was successful
   enough. Note that GMP will terminate the application, if the following
   functions return NULL.
 */
  mp_set_memory_functions(
    pmath_mem_alloc,
    pmath_gmp_realloc,
    pmath_gmp_free);

  return TRUE;
  #ifdef PMATH_DEBUG_MEMORY
   FAIL_MEM_LIST_MUTEX:
    return FALSE;
  #endif
}

PMATH_PRIVATE void _pmath_memory_manager_done(void){
  #ifdef PMATH_DEBUG_MEMORY
    size_t total_alloc, total_realloc, i;
    
    check_all_freed();

    #if PMATH_USE_PTHREAD
      pthread_mutex_destroy(&mem_list_mutex);
    #elif PMATH_USE_WINDOWS_THREADS
      DeleteCriticalSection(&mem_list_mutex);
    #endif

    total_alloc = 0;
    total_realloc = 0;
    printf("\n    size allocs resize    sum\n");
    for(i = 0;i < STAT_SIZES;i++){
      printf("<=%6"PRIuPTR" %6"PRIdPTR" %6"PRIdPTR" %6"PRIdPTR"\n",
        stat_sizes[i],
        alloc_stats[i],
        realloc_stats[i],
        alloc_stats[i] + realloc_stats[i]);
      total_alloc+=   alloc_stats[i];
      total_realloc+= realloc_stats[i];
    }
    printf("> %6"PRIuPTR" %6"PRIdPTR" %6"PRIdPTR" %6"PRIdPTR"\n\n",
      stat_sizes[STAT_SIZES-1],
      alloc_stats[STAT_SIZES],
      realloc_stats[STAT_SIZES],
      alloc_stats[STAT_SIZES] + realloc_stats[STAT_SIZES]);
    total_alloc+=   alloc_stats[STAT_SIZES];
    total_realloc+= realloc_stats[STAT_SIZES];
    printf("total:   %6"PRIdPTR" %6"PRIdPTR" %6"PRIdPTR"\n\n",
      total_alloc,
      total_realloc,
      total_alloc + total_realloc);
  #endif
}
