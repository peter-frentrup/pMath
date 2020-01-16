#ifndef PMATH_UTIL__GARBAGE_COLLECTION_PRIVATE_H__INCLUDED
#define PMATH_UTIL__GARBAGE_COLLECTION_PRIVATE_H__INCLUDED

#ifndef BUILDING_PMATH
#  error This header file is not part of the public pMath API
#endif


#include <pmath-config.h>


/** Run the garbage collector.

    This function is not thread-safe. It must only be called from the garbage-collection thread.

    \see pmath_collect_temporary_symbols
 */
PMATH_PRIVATE
void _pmath_unsafe_run_gc(void);


#endif // PMATH_UTIL__GARBAGE_COLLECTION_PRIVATE_H__INCLUDED
