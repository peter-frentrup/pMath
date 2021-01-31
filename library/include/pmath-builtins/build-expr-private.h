#ifndef PMATH_BUILTINS__BUILD_EXPR_PRIVATE_H__INCLUDED
#define PMATH_BUILTINS__BUILD_EXPR_PRIVATE_H__INCLUDED


#define INT(I)          PMATH_FROM_INT32(I)
#define CINFTY          pmath_ref(_pmath_object_complex_infinity)
#define QUOT(N,D)       pmath_rational_new(INT(N), INT(D))
#define ONE_HALF        pmath_ref(_pmath_one_half)
#define FUNC(F, X)      pmath_expr_new_extended(F, 1, X)
#define FUNC2(F, X, Y)  pmath_expr_new_extended(F, 2, X, Y)
#define COMPLEX(R, I)   FUNC2(pmath_ref(pmath_System_Complex), R, I)
#define PLUS(X, Y)      FUNC2(pmath_ref(pmath_System_Plus), X, Y)
#define PLUS3(X, Y, Z)  pmath_expr_new_extended(pmath_ref(pmath_System_Plus), 3, X, Y, Z)
#define TIMES(X, Y)     FUNC2(pmath_ref(pmath_System_Times), X, Y)
#define TIMES3(X, Y, Z) pmath_expr_new_extended(pmath_ref(pmath_System_Times), 3, X, Y, Z)
#define NEG(X)          TIMES(INT(-1), X)
#define INV(X)          POW(X, INT(-1))
#define MINUS(X, Y)     PLUS(X, NEG(Y))
#define DIV(X, Y)       TIMES(X, INV(Y))
#define POW(X, Y)       FUNC2(pmath_ref(pmath_System_Power), X, Y)
#define SQRT(X)         POW(X, ONE_HALF)
#define INVSQRT(X)      POW(X, pmath_number_neg(ONE_HALF))

#define ABS(X)          FUNC(pmath_ref(pmath_System_Abs), X)
#define ARCTAN(X)       FUNC(pmath_ref(pmath_System_ArcTan), X)
#define ARG(X)          FUNC(pmath_ref(pmath_System_Arg), X)
#define GAMMA(X)        FUNC(pmath_ref(pmath_System_Gamma), X)
#define LOG(X)          FUNC(pmath_ref(pmath_System_Log), X)

#define GREATER(X,Y)    FUNC2(pmath_ref(pmath_System_Greater), X, Y)

#endif // PMATH_BUILTINS__BUILD_EXPR_PRIVATE_H__INCLUDED
