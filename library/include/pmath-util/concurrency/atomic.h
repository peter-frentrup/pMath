#ifndef __PMATH_UTIL__CONCURRENCY__ATOMIC_H__
#define __PMATH_UTIL__CONCURRENCY__ATOMIC_H__

#include <pmath-config.h>
#include <pmath-types.h>

#include <assert.h>
#include <inttypes.h>

/**\defgroup atomic_ops Atomic Operations
   \brief Using atomic operations (independent of the rest of the library)

   pMath provides a collection of functions/macros to do atomic operations. This
   part of the library is completely independent of the rest of pMath. To use
   atomic opertions, \#include <pmath-util/concurrency/atomic.h>. You do not have to link to
   an additionl library.

   At the moment, supported compilers are GCC and Microsoft Visual C++.
   
   On some platforms, the atomic opertations are implemented as inline functions 
   with inline assembler (currently GCC older than 4.x). On other
   platforms, macros and compiler intrinsic functions (GCC 4.x, MSVC) are used.

   \section sect_atomic_ops_ref Nice to read:
   \li http://developers.sun.com/solaris/articles/atomic_sparc/
   
   \li http://lists.canonical.org/pipermail/kragen-tol/1999-August/000457.html

   \li http://www.angstrom-distribution.org/unstable/sources/libc_sources.redhat.com__20061019.tar.gz \n
     libc/sysdeps/i386/i486/bits/atomic.h

   \li Intel's cmpxchg8b and cmpxchg16b instructions
   
   \li http://www.cse.msu.edu/~sdf/private/szumoframe-0.3.tar.gz \n
     szumoframe-0.3/src/szumoframe/szumo_preamble.h

   \li http://www.tml.tkk.fi/~rakajast/uvsr_renderer.tar.gz \n
     uvsr_renderer/needed_externals/threadlib/src/fifo.c

   \li qprof -> atomic_ops library
   
   \li http://code.google.com/p/google-perftools/
 */

/*
   #defines:
     HAVE_ATOMIC_OPS
     PMATH_DECLARE_ALIGNED(type, name, alignment)
     PMATH_DECLARE_ATOMIC(name)
     PMATH_DECLARE_ATOMIC_2(name)
 */

#define PMATH_ATOMIC_FASTLOOP_COUNT  (1000)

#ifdef PMATH_OS_WIN32
  #include <windows.h>
  
  #define pmath_atomic_loop_yield()  (Sleep(0))
  #define pmath_atomic_loop_nop()    (Sleep(1))
#else
  #if defined (__SVR4) && defined (__sun)
    #include <thread.h>

    #define pmath_atomic_loop_yield()  (thr_yield())
  #else
    #include <sched.h> 
    
    #define pmath_atomic_loop_yield()  (sched_yield())
  #endif
  
  #include <time.h> 
  
  #define pmath_atomic_loop_nop() \
    do{ \
      struct timespec tm; \
      tm.tv_sec = 0; \
      tm.tv_nsec = 2000001; \
      nanosleep(&tm, NULL); \
    }while(0)
#endif


#if PMATH_BITSIZE == 64
  #define PMATH_DECLARE_ATOMIC(name)   PMATH_DECLARE_ALIGNED(volatile intptr_t, name,    8)
  #define PMATH_DECLARE_ATOMIC_2(name) PMATH_DECLARE_ALIGNED(volatile intptr_t, name[2], 16)
#elif PMATH_BITSIZE == 32
  #define PMATH_DECLARE_ATOMIC(name)   PMATH_DECLARE_ALIGNED(volatile intptr_t, name,    4)
  #define PMATH_DECLARE_ATOMIC_2(name) PMATH_DECLARE_ALIGNED(volatile intptr_t, name[2], 8)
#else
  #error invalid PMATH_BITSIZE
#endif

#ifdef PMATH_DOXYGEN
  
  #include <pmath-util/concurrency/atomic/non-atomic.h>
  
#elif defined(__GNUC__)

  #define PMATH_DECLARE_ALIGNED(TYPE, NAME, ALIGNMENT) \
    TYPE NAME __attribute__ ((aligned(ALIGNMENT)))

  #if PMATH_NEED_GNUC(4, 0)
    #include <pmath-util/concurrency/atomic/gcc/built_in_functions.h>
  #elif defined(PMATH_X86)
    #include <pmath-util/concurrency/atomic/gcc/x86.h>
  #elif defined(PMATH_AMD64)
    #include <pmath-util/concurrency/atomic/gcc/x86-64.h>
  #elif defined(sun) || defined(__sun)
    #include <pmath-util/concurrency/atomic/gcc/sun.h>
  #else
    #warning UPGRADE YOUR GCC to 4.x!
    #include <pmath-util/concurrency/atomic/non-atomic.h>
  #endif

#elif defined(_MSC_VER)

  #define PMATH_DECLARE_ALIGNED(type, name, byte_alignment) \
    __declspec(align(byte_alignment)) type name

  #include <pmath-util/concurrency/atomic/msvc/intrinsic_functions.h>

#else
  #include <pmath-util/concurrency/atomic/non-atomic.h>
#endif

#endif /* __PMATH_UTIL__CONCURRENCY__ATOMIC_H__ */
