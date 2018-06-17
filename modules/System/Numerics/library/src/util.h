#ifndef __MODULE__PMATH_NUMERICS__UTIL_H__
#define __MODULE__PMATH_NUMERICS__UTIL_H__

#ifndef __MODULE__PMATH_NUMERICS__STDAFX_H__
#  error include "stdafx.h" first
#endif

// If *z == x * I => *z:= x
PMATH_PRIVATE pmath_bool_t pnum_is_imaginary(pmath_t *z);

PMATH_PRIVATE pmath_bool_t pnum_equals_quotient(pmath_t obj, int32_t num, int32_t den);
PMATH_PRIVATE pmath_bool_t pnum_get_small_rational(pmath_t obj, int32_t *num, int32_t *den);

// basically a copy of _pmath_contains_symbol
PMATH_PRIVATE
pmath_bool_t pnum_contains_nonhead_symbol(pmath_t obj, pmath_symbol_t sub);

//#include <pmath-builtins/build-expr-private.h>

#define INT(I)          PMATH_FROM_INT32(I)
//#define CINFTY          pmath_ref(_pmath_object_complex_infinity)
#define CINFTY          pmath_expr_new(pmath_ref(PMATH_SYMBOL_DIRECTEDINFINITY), 0)
#define QUOT(N,D)       pmath_rational_new(INT(N), INT(D))
//#define ONE_HALF        pmath_ref(_pmath_one_half)
#define ONE_HALF        QUOT(1, 2)
#define FUNC(F, X)      pmath_expr_new_extended(F, 1, X)
#define FUNC2(F, X, Y)  pmath_expr_new_extended(F, 2, X, Y)
#define COMPLEX(R, I)   FUNC2(pmath_ref(PMATH_SYMBOL_COMPLEX), R, I)
#define PLUS(X, Y)      FUNC2(pmath_ref(PMATH_SYMBOL_PLUS), X, Y)
#define PLUS3(X, Y, Z)  pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_PLUS), 3, X, Y, Z)
#define TIMES(X, Y)     FUNC2(pmath_ref(PMATH_SYMBOL_TIMES), X, Y)
#define TIMES3(X, Y, Z) pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_TIMES), 3, X, Y, Z)
#define NEG(X)          TIMES(INT(-1), X)
#define INV(X)          POW(X, INT(-1))
#define MINUS(X, Y)     PLUS(X, NEG(Y))
#define DIV(X, Y)       TIMES(X, INV(Y))
#define POW(X, Y)       FUNC2(pmath_ref(PMATH_SYMBOL_POWER), X, Y)
#define SQRT(X)         POW(X, ONE_HALF)
#define INVSQRT(X)      POW(X, pmath_number_neg(ONE_HALF))

#define ABS(X)          FUNC(pmath_ref(PMATH_SYMBOL_ABS), X)
#define ARCTAN(X)       FUNC(pmath_ref(PMATH_SYMBOL_ARCTAN), X)
#define ARG(X)          FUNC(pmath_ref(PMATH_SYMBOL_ARG), X)
#define COS(X)          FUNC(pmath_ref(PMATH_SYMBOL_COS), X)
#define EXP(X)          POW(pmath_ref(PMATH_SYMBOL_E), X)
#define GAMMA(X)        FUNC(pmath_ref(PMATH_SYMBOL_GAMMA), X)
#define LOG(X)          FUNC(pmath_ref(PMATH_SYMBOL_LOG), X)
#define SIN(X)          FUNC(pmath_ref(PMATH_SYMBOL_SIN), X)
#define SIGN(X)         FUNC(pmath_ref(PMATH_SYMBOL_SIGN), X)

#define GREATER(X,Y)    FUNC2(pmath_ref(PMATH_SYMBOL_GREATER), X, Y)


#endif // __MODULE__PMATH_NUMERICS__UTIL_H__
