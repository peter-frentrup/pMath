#ifndef PMATH_MODULE_INOUT_STDAFX_H_INCLUDED
#define PMATH_MODULE_INOUT_STDAFX_H_INCLUDED

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__)
#  define _WIN32_WINNT 0x0600 /* Vista and above */
#endif

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>

#include <pmath.h>

#ifdef PMATH_OS_WIN32
#  include <Windows.h>
#else
#  include <sys/stat.h>
#  include <sys/types.h>
#  include <dirent.h>
#  include <errno.h>
#  include <stdio.h>
#  include <string.h>
#endif


#endif // PMATH_MODULE_INOUT_STDAFX_H_INCLUDED
