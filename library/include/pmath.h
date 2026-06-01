#ifndef __PMATH_H__
#define __PMATH_H__

/**\mainpage The pMath Computer Algebra System Library
   \author Peter Frentrup
   \date 2005 \-- 2026

   \par Introduction

   pMath is a CAS for Windows and Unix like systems. The whole system consists
   of three projects:
    - The pMath library documented here, which implements the parser,
      interpreter, mathematical functionality and OS binding.
    - The RichMath graphical front-end.
    - Addon libraries/modules for the pMath library and language (e.g. a Java
      binding).

   This document does not cover the pMath \em language itself, but the underlying
   C library.

   You as a user (front-end or module programmer) of the pMath library just need
   to \#include <pmath.h> and link with the appropriate library file.
   The main operations are \ref parsing_code and evaluating expressions via
   pmath_evaluate(), see \ref objects.

   \par Links/Depencies

   pMath is build on top of several open source libraries:
    - Arb (http://www.arblib.org)
    - FLINT (http://www.flintlib.org)
    - MPIR/GMP (http://www.mpir.org or http://www.gmplib.org)
    - MPFR (http://www.mpfr.org)
    - PCRE (http://www.pcre.org)

 */

#include <pmath-core/custom.h>
#include <pmath-core/expressions.h>
#include <pmath-core/packed-arrays.h>
#include <pmath-core/numbers.h>
#include <pmath-core/symbols.h>

#include <pmath-language/charnames.h>
#include <pmath-language/tokens.h>
#include <pmath-language/scanner.h>
#include <pmath-language/source-location.h>

#include <pmath-util/approximate.h>
#include <pmath-util/compression.h>
#include <pmath-util/concurrency/threadlocks.h>
#include <pmath-util/concurrency/threadmsg.h>
#include <pmath-util/concurrency/threadpool.h>
#include <pmath-util/concurrency/threads.h>
#include <pmath-util/files/abstract-file.h>
#include <pmath-util/files/binary-buffer.h>
#include <pmath-util/files/filesystem.h>
#include <pmath-util/files/mixed-buffer.h>
#include <pmath-util/files/text-from-binary.h>
#include <pmath-util/debug.h>
#include <pmath-util/dispatch-tables.h>
#include <pmath-util/dynamic.h>
#include <pmath-util/emit-and-gather.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/hash/hashtables.h>
#include <pmath-util/helpers.h>
#include <pmath-util/line-writer.h>
#include <pmath-util/memory.h>
#include <pmath-util/messages.h>
#include <pmath-util/option-helpers.h>
#include <pmath-util/security.h>
#include <pmath-util/serialize.h>
#include <pmath-util/stacks.h>
#include <pmath-util/strtod.h>
#include <pmath-util/version.h>

#include <pmath-builtins/all-symbols.h>


/**\defgroup frontend Front-ends
   \brief Functions for use by front-ends.

   A front-end to pMath is an executable that initializes and finalizes the
   library and handles user input and output or otherwise invokes expression
   evaluation with the library. That is, what pMath does: parse and evaluate
   pMath code.
   
   The pMath system comes with several front-ends:
   - richmath.exe -- the graphical front-end for Windows and Linux
   - pmathc.exe -- a command line front-end with readline/editline support
   - console-win.exe -- a windows console front-end with advanced keyboard and 
      mouse support based on the hyper-console library
   - test.exe -- a *minimal* command line front-end
   - test-runner.exe -- the front-end that runs inline tests (comment lines in *.c or 
      *.pmath files which consist of "pmath>" prompts and their results)
   - TestApp.java -- a minimal command line front-end written in Java
  @{
 */

/**\brief Initialize the pMath CAS library.
   \return TRUE, if the initialization was successfull, FALSE otherwise.

   Any thread must call pmath_init() before using any other pMath function
   (exception: \ref atomic_ops). If the initialization fails, the thread should
   stop. Otherwise, pmath_done() must be called before the thread ends.

   When this function is called for the first time, the <tt>maininit.pmath</tt>
   file is executed. This file is searched in the following directories in
   order:
      -# The application directory
      -# The directory specified in the \c PMATH_BASEDIRECTORY environment
         variable
      -# Some operating system specific directories:
          - /etc/pmath, /etc/local/pmath, /usr/share/pmath,
            /usr/share/local/pmath on Unix like systems
          - The "pmath" subfolder in the "common program files" and
            "program files" directories on Microsoft Windows, if existent.
 */
PMATH_API
pmath_bool_t pmath_init(void);

/**\brief Free all resources used by the pMath CAS library and unload all
          modules.

   \see pmath_init
 */
PMATH_API
void pmath_done(void);

/** @} */

#endif /* __PMATH_H__ */
