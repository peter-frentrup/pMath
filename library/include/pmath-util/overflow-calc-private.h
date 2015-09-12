#ifndef __PMATH_UTIL__OVERFLOW_CALC_PRIVATE_H__
#define __PMATH_UTIL__OVERFLOW_CALC_PRIVATE_H__

#ifndef BUILDING_PMATH
#  error This header file is not part of the public pMath API
#endif

#include <pmath-types.h>

/* See
   [1] INT32-C. Ensure that operations on signed integers do not result in overflow
       https://www.securecoding.cert.org/confluence/display/c/INT32-C.+Ensure+that+operations+on+signed+integers+do+not+result+in+overflow 
   
   [2] INT30-C. Ensure that unsigned integer operations do not wrap
       https://www.securecoding.cert.org/confluence/display/c/INT30-C.+Ensure+that+unsigned+integer+operations+do+not+wrap
 */

/* _pmath_add_si(a, b, &flag)
   Adds signed integers a and b if no overflow occurs.
   Otherwise, *error is set to TRUE and the result is an arbitrary integer.
   Note that *error is never cleared, so you can perform multiple calculations
   and check the flag only once at the end.
 */

#define IMPLEMENT_SIGNED_ADD(NAME, INT_TYPE, INT_TYPE_MIN, INT_TYPE_MAX)    \
  PMATH_FORCE_INLINE                                                        \
  INT_TYPE NAME(                                                            \
              INT_TYPE      a,                                              \
              INT_TYPE      b,                                              \
      _Inout_ pmath_bool_t *error                                           \
  ) {                                                                       \
    assert(error != NULL);                                                  \
    if(b > 0 && a > INT_TYPE_MAX - b) {                                     \
      *error = TRUE;                                                        \
      return 0;                                                             \
    }                                                                       \
    if (b < 0 && a < INT_TYPE_MIN - b) {                                    \
      *error = TRUE;                                                        \
      return 0;                                                             \
    }                                                                       \
    return a + b;                                                           \
  }

IMPLEMENT_SIGNED_ADD(_pmath_add_si,    int,      INT_MIN,    INT_MAX)
IMPLEMENT_SIGNED_ADD(_pmath_add_si32,  int32_t,  INT32_MIN,  INT32_MAX)
IMPLEMENT_SIGNED_ADD(_pmath_add_si64,  int64_t,  INT64_MIN,  INT64_MAX)
IMPLEMENT_SIGNED_ADD(_pmath_add_siptr, intptr_t, INTPTR_MIN, INTPTR_MAX)

#undef IMPLEMENT_SIGNED_ADD


#define IMPLEMENT_SIGNED_SUB(NAME, INT_TYPE, INT_TYPE_MIN, INT_TYPE_MAX)    \
  PMATH_FORCE_INLINE                                                        \
  INT_TYPE NAME(                                                            \
              INT_TYPE      a,                                              \
              INT_TYPE      b,                                              \
      _Inout_ pmath_bool_t *error                                           \
  ) {                                                                       \
    assert(error != NULL);                                                  \
    if(b > 0 && a < INT_TYPE_MIN + b) {                                     \
      *error = TRUE;                                                        \
      return 0;                                                             \
    }                                                                       \
    if(b < 0 && a > INT_TYPE_MAX + b) {                                     \
      *error = TRUE;                                                        \
      return 0;                                                             \
    }                                                                       \
    return a - b;                                                           \
  }

IMPLEMENT_SIGNED_SUB(_pmath_sub_si,    int,      INT_MIN,    INT_MAX)
IMPLEMENT_SIGNED_SUB(_pmath_sub_si32,  int32_t,  INT32_MIN,  INT32_MAX)
IMPLEMENT_SIGNED_SUB(_pmath_sub_si64,  int64_t,  INT64_MIN,  INT64_MAX)
IMPLEMENT_SIGNED_SUB(_pmath_sub_siptr, intptr_t, INTPTR_MIN, INTPTR_MAX)

#undef IMPLEMENT_SIGNED_SUB


#define IMPLEMENT_SIGNED_MUL(NAME, INT_TYPE, INT_TYPE_MIN, INT_TYPE_MAX)    \
  PMATH_FORCE_INLINE                                                        \
  INT_TYPE NAME(                                                            \
            INT_TYPE      a,                                                \
            INT_TYPE      b,                                                \
    _Inout_ pmath_bool_t *error                                             \
  ) {                                                                       \
    assert(error != NULL);                                                  \
    if(a > 0) {                                                             \
      if(b > 0) {                                                           \
        if(a > INT_TYPE_MAX / b) {                                          \
          *error = TRUE;                                                    \
          return 0;                                                         \
        }                                                                   \
      }                                                                     \
      else { /* a > 0, b <= 0 */                                            \
        if (b < INT_TYPE_MIN / a) {                                         \
          *error = TRUE;                                                    \
          return 0;                                                         \
        }                                                                   \
      }                                                                     \
    }                                                                       \
    else { /* a <= 0 */                                                     \
      if(b > 0) {                                                           \
        if (a < INT_TYPE_MIN / b) {                                         \
          *error = TRUE;                                                    \
          return 0;                                                         \
        }                                                                   \
      }                                                                     \
      else { /* a <= 0, b <= 0 */                                           \
        if(a != 0 && b < INT_TYPE_MAX / a) {                                \
          *error = TRUE;                                                    \
          return 0;                                                         \
        }                                                                   \
      }                                                                     \
    }                                                                       \
    return a * b;                                                           \
  }

IMPLEMENT_SIGNED_MUL(_pmath_mul_si,    int,      INT_MIN,    INT_MAX)
IMPLEMENT_SIGNED_MUL(_pmath_mul_si32,  int32_t,  INT32_MIN,  INT32_MAX)
IMPLEMENT_SIGNED_MUL(_pmath_mul_si64,  int64_t,  INT64_MIN,  INT64_MAX)
IMPLEMENT_SIGNED_MUL(_pmath_mul_siptr, intptr_t, INTPTR_MIN, INTPTR_MAX)

#undef IMPLEMENT_SIGNED_MUL


#define IMPLEMENT_SIGNED_DIV(NAME, INT_TYPE, INT_TYPE_MIN, INT_TYPE_MAX)    \
  PMATH_FORCE_INLINE                                                        \
  INT_TYPE NAME(                                                            \
            INT_TYPE      a,                                                \
            INT_TYPE      b,                                                \
    _Inout_ pmath_bool_t *error                                             \
  ) {                                                                       \
    assert(error != NULL);                                                  \
    if ((b == 0) || ((a == INT_TYPE_MIN) && (b == -1))) {                   \
      *error = TRUE;                                                        \
      return 0;                                                          \
    }                                                                       \
    return a / b;                                                           \
  }

IMPLEMENT_SIGNED_DIV(_pmath_div_si,    int,      INT_MIN,    INT_MAX)
IMPLEMENT_SIGNED_DIV(_pmath_div_si32,  int32_t,  INT32_MIN,  INT32_MAX)
IMPLEMENT_SIGNED_DIV(_pmath_div_si64,  int64_t,  INT64_MIN,  INT64_MAX)
IMPLEMENT_SIGNED_DIV(_pmath_div_siptr, intptr_t, INTPTR_MIN, INTPTR_MAX)

#undef IMPLEMENT_SIGNED_DIV


#define IMPLEMENT_SIGNED_MOD(NAME, INT_TYPE, INT_TYPE_MIN, INT_TYPE_MAX)    \
  PMATH_FORCE_INLINE                                                        \
  INT_TYPE NAME(                                                            \
            INT_TYPE      a,                                                \
            INT_TYPE      b,                                                \
    _Inout_ pmath_bool_t *error                                             \
  ) {                                                                       \
    assert(error != NULL);                                                  \
    if ((b == 0) || ((a == INT_TYPE_MIN) && (b == -1))) {                   \
      *error = TRUE;                                                        \
      return 0;                                                          \
    }                                                                       \
    return a % b;                                                           \
  }

IMPLEMENT_SIGNED_MOD(_pmath_mod_si,    int,      INT_MIN,    INT_MAX)
IMPLEMENT_SIGNED_MOD(_pmath_mod_si32,  int32_t,  INT32_MIN,  INT32_MAX)
IMPLEMENT_SIGNED_MOD(_pmath_mod_si64,  int64_t,  INT64_MIN,  INT64_MAX)
IMPLEMENT_SIGNED_MOD(_pmath_mod_siptr, intptr_t, INTPTR_MIN, INTPTR_MAX)

#undef IMPLEMENT_SIGNED_MOD



/* Unsigned arithmetic with overflow check */

#define IMPLEMENT_UNSIGNED_ADD(NAME, UINT_TYPE)    \
  PMATH_FORCE_INLINE                               \
  UINT_TYPE NAME(                                  \
            UINT_TYPE     a,                       \
            UINT_TYPE     b,                       \
    _Inout_ pmath_bool_t *error                    \
  ) {                                              \
    UINT_TYPE sum;                                 \
    assert(error != NULL);                         \
    sum = a + b;                                   \
    if (sum < a) {                                 \
      *error = TRUE;                               \
      return 0;                                    \
    }                                              \
    return sum;                                    \
  }

IMPLEMENT_UNSIGNED_ADD(_pmath_add_size, size_t)

#undef IMPLEMENT_UNSIGNED_ADD


#define IMPLEMENT_UNSIGNED_SUB(NAME, UINT_TYPE)    \
  PMATH_FORCE_INLINE                               \
  UINT_TYPE NAME(                                  \
            UINT_TYPE     a,                       \
            UINT_TYPE     b,                       \
    _Inout_ pmath_bool_t *error                    \
  ) {                                              \
    UINT_TYPE diff;                                \
    assert(error != NULL);                         \
    diff = a - b;                                  \
    if(diff > a) {                                 \
      *error = TRUE;                               \
      return 0;                                    \
    }                                              \
    return diff;                                   \
  }

IMPLEMENT_UNSIGNED_SUB(_pmath_sub_size, size_t)

#undef IMPLEMENT_UNSIGNED_SUB


#define IMPLEMENT_UNSIGNED_MUL(NAME, UINT_TYPE, UINT_TYPE_MAX)    \
  PMATH_FORCE_INLINE                                              \
  UINT_TYPE NAME(                                                 \
            UINT_TYPE     a,                                      \
            UINT_TYPE     b,                                      \
    _Inout_ pmath_bool_t *error                                   \
  ) {                                                             \
    assert(error != NULL);                                        \
    if(b != 0 && a > UINT_TYPE_MAX / b) {                         \
      *error = TRUE;                                              \
      return 0;                                                   \
    }                                                             \
    return a * b;                                                 \
  }

IMPLEMENT_UNSIGNED_MUL(_pmath_mul_size, size_t, SIZE_MAX)

#undef IMPLEMENT_UNSIGNED_MUL


#endif // __PMATH_UTIL__OVERFLOW_CALC_PRIVATE_H__
