#ifndef __PMATH_CONFIG_H__
#define __PMATH_CONFIG_H__

#if defined(__GNUC__) && defined(__GNUC_MINOR__)
  #define PMATH_NEED_GNUC(maj, min) \
    ((__GNUC__ << 16) + __GNUC_MINOR__ >= ((maj) << 16) + (min))
#else
  #define PMATH_NEED_GNUC(maj, min)  0
#endif

/* PMATH_ATTRIBUTE_PURE: A function without side effects. */
#if PMATH_NEED_GNUC(2, 96)
  #define PMATH_ATTRIBUTE_PURE  __attribute__((__pure__))
#elif defined(_MSC_VER)
  #define PMATH_ATTRIBUTE_PURE  __declspec(noalias)
#else
  #define PMATH_ATTRIBUTE_PURE
#endif

#ifdef _MSC_VER
  #include <sal.h>
#endif

/* PMATH_ATTRIBUTE_USE_RESULT: You must use the result of a function with this 
   attribute. E.g. destroy the result.
   
   PMATH_ATTRIBUTE_NONNULL: the i-th arguments must not be PMATH_NULL 
 */
#if PMATH_NEED_GNUC(3, 4)
  #define PMATH_ATTRIBUTE_USE_RESULT   __attribute__((__warn_unused_result__))
  #define PMATH_ATTRIBUTE_NONNULL(...) __attribute__((__nonnull__(__VA_ARGS__)))
#else
  #define PMATH_ATTRIBUTE_USE_RESULT /*_Check_return_*/
  #define PMATH_ATTRIBUTE_NONNULL(...)
#endif

/* PMATH_DEPRECATED You should not use functions with this attribute.
 */
#if PMATH_NEED_GNUC(3, 1)
  #define PMATH_DEPRECATED  __attribute__((__deprecated__))
#elif defined(_MSC_VER)
  #define PMATH_DEPRECATED  __declspec(deprecated)
#else
  #define PMATH_DEPRECATED
#endif

/* PMATH_LIKELY(cond)   it is likely, that cond yields TRUE -> optimization.
   PMATH_UNLIKELY(cond) it is unlikely, that cond yields FALSE -> optimization.
 */
#if PMATH_NEED_GNUC(3, 0)
  #define PMATH_LIKELY(cond)    __builtin_expect((cond) != 0, 1)
  #define PMATH_UNLIKELY(cond)  __builtin_expect((cond) != 0, 0)
#else
  #define PMATH_LIKELY(cond)    (cond)
  #define PMATH_UNLIKELY(cond)  (cond)
#endif

/* PMATH_FORCE_INLINE  (static __inline __attribute__((__always_inline__)) or 
   static __forceinline)
   
   We cannot use extern inline (gcc) because that conflicts with PMATH_API
 */
#ifdef __GNUC__
  #define PMATH_FORCE_INLINE  static __inline __attribute__((__always_inline__))
#elif defined(_MSC_VER)
  #define PMATH_FORCE_INLINE  static __forceinline
#else
  #define PMATH_FORCE_INLINE  static __inline
#endif

/* PMATH_INLINE_NODEBUG
   
   The specified function should be handled as a macro for debugging (never jump 
   into the function) 
 */
#ifdef __GNUC__
  #define PMATH_INLINE_NODEBUG  __attribute__((artificial))
#else
  #define PMATH_INLINE_NODEBUG  
#endif

/* PMATH_INLINE  (__inline)
 */
#define PMATH_INLINE __inline

/* PMATH_OS_UNIX or PMATH_OS_WIN32
 */
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__)
  #define PMATH_OS_WIN32
#elif defined(linux) || defined(__linux) || defined(__linux__) \
   || defined(unix)  || defined(__unix)  || defined(__unix__) \
   || defined(__APPLE__)
  #define PMATH_OS_UNIX
#else
  #error Unknown Operating System.
#endif

/* threading library: Posix or Win32 */
#ifdef BUILDING_PMATH
  #ifndef PMATH_USE_PTHREAD
    #define PMATH_USE_PTHREAD 0
  #endif

  #ifndef PMATH_USE_WINDOWS_THREADS
    #define PMATH_USE_WINDOWS_THREADS 0
  #endif

  #if (PMATH_USE_PTHREAD  &&  PMATH_USE_WINDOWS_THREADS) \
   || (!PMATH_USE_PTHREAD && !PMATH_USE_WINDOWS_THREADS)
    #error either PMATH_USE_PTHREAD or PMATH_USE_WINDOWS_THREADS must be TRUE.
  #elif PMATH_USE_WINDOWS_THREADS && !defined(PMATH_OS_WIN32)
    #error PMATH_USE_WINDOWS_THREADS can only be defined on Windows systems.
  #endif
#endif

#ifdef __cplusplus
  #define PMATH_EXTERN_C  extern "C"
#else
  #define PMATH_EXTERN_C
#endif

/* PMATH_API, PMATH_PRIVATE
 */
#ifdef PMATH_OS_WIN32
  #ifdef __GNUC__
    #define PMATH_MODULE     PMATH_EXTERN_C __attribute__((cdecl, dllexport))
    
    #ifdef BUILDING_PMATH
      #define PMATH_API      PMATH_EXTERN_C __attribute__((cdecl, dllexport))
      #define PMATH_PRIVATE  
    #else
      #define PMATH_API      PMATH_EXTERN_C __attribute__((cdecl, dllimport))
    #endif
  #else
    #define PMATH_MODULE     PMATH_EXTERN_C __declspec(dllexport)
    
    #ifdef BUILDING_PMATH
      #define PMATH_API      PMATH_EXTERN_C __declspec(dllexport)
      #define PMATH_PRIVATE
    #else
      #define PMATH_API      PMATH_EXTERN_C __declspec(dllimport)
    #endif
  #endif
#else
  #define PMATH_MODULE     PMATH_EXTERN_C __attribute__((__visibility__("default")))
  
  #define PMATH_API        PMATH_EXTERN_C __attribute__((__visibility__("default")))
  
  #ifdef BUILDING_PMATH
    #define PMATH_PRIVATE  PMATH_EXTERN_C __attribute__((__visibility__("hidden")))
  #endif
#endif

/* architecture     PMATH_X86  PMATH_AMD64  PMATH_SPARC32  PMATH_SPARC64
   endianness       PMATH_BYTE_ORDER = -1 (little endian)  or = +1 (big endian)
   bits per word    PMATH_32BIT or PMATH_64BIT
                    PMATH_BITSIZE stores value (32 or 64)
   
   bits per int     PMATH_INT_32BIT or PMATH_INT_64BIT
 */
#if  (defined(__amd64__) \
   || defined(__amd64) \
   || defined(__x86_64__) \
   || defined(__x86_64) \
   || defined(_M_X64) \
)
  #define PMATH_AMD64                  1
  #define PMATH_64BIT                  1
  #define PMATH_BITSIZE               64
  #define PMATH_BYTE_ORDER            (-1)
  #ifdef PMATH_OS_WIN32
    #define PMATH_INT_32BIT            1
  #else
    #define PMATH_INT_64BIT            1
  #endif
#elif (defined(i386) \
    || defined(__i386__) \
    || defined(_M_IX86) \
    || defined(_X86_) \
    || defined(__X86__) \
    || defined(__THW_INTEL__) \
    || defined(__I86__) \
    || defined(__INTEL__) \
)
  #define PMATH_X86                    1
  #define PMATH_32BIT                  1
  #define PMATH_BITSIZE               32
  #define PMATH_BYTE_ORDER            (-1)
  #define PMATH_INT_32BIT              1
#elif (defined(sparc) \
    || defined(__sparc)\
    || defined(__sparc__)\
    || defined(__sparc64__)\
)
  #define PMATH_BYTE_ORDER               1
  
  #if (defined(__arch64__) \
    || defined(__sparc64__)\
    || ((defined(__SUNPRO_C) || defined(__SUNPRO_CC)) && defined(__sparcv9)) \
  )
    #define PMATH_SPARC64                1
    #define PMATH_BITSIZE               64
    #define PMATH_64BIT                  1
    #define PMATH_INT_64BIT              1
  #else
    #define PMATH_SPARC32                1
    #define PMATH_BITSIZE               32
    #define PMATH_32BIT                  1
    #define PMATH_INT_32BIT              1
  #endif
#endif

#ifndef PMATH_BYTE_ORDER
  #define PMATH_BYTE_ORDER (1)
#endif

#endif /* __PMATH_CONFIG_H__ */
