#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <pmath-config.h>
#include <pmath-types.h>
#include <pmath-core/objects.h>

#include <pmath-util/debug.h>

#include <pmath-util/concurrency/atomic.h>

#include <pmath-core/objects-inline.h>

#include <pmath-util/concurrency/atomic-private.h> // depends on pmath-objects-inline.h

#undef pmath_ref
#undef pmath_unref
#undef pmath_instance_of

PMATH_API pmath_t pmath_ref(pmath_t obj){
  return pmath_fast_ref(obj);
}

PMATH_API void pmath_unref(pmath_t obj){
  pmath_fast_unref(obj);
}

PMATH_API pmath_bool_t pmath_instance_of(
  pmath_t obj,
  pmath_type_t type
){
  return pmath_fast_instance_of(obj, type);
}
