#include "stdafx.h"
#include "util.h"

extern pmath_symbol_t pmath_System_ArcCos;
extern pmath_symbol_t pmath_System_ArcCot;
extern pmath_symbol_t pmath_System_ArcCsc;
extern pmath_symbol_t pmath_System_ArcSec;
extern pmath_symbol_t pmath_System_ArcSin;
extern pmath_symbol_t pmath_System_ArcTan;

#define X pmath_ref(xx)

#define IMPL_do_Func_of_ArcFunc_of_X( func, arcfunc, term ) \
  static pmath_t do_ ## func ## _of_ ## arcfunc ## _of_x(pmath_t xx) { /* does not free xx */ \
    return term; \
  }

#define IMPL_Cos_Sin_Tan_of_ArcFunc_of_X( arcfunc, costerm, sinterm, tanterm ) \
  IMPL_do_Func_of_ArcFunc_of_X( Cos, arcfunc, costerm ) \
  IMPL_do_Func_of_ArcFunc_of_X( Sin, arcfunc, sinterm ) \
  IMPL_do_Func_of_ArcFunc_of_X( Tan, arcfunc, tanterm )

IMPL_Cos_Sin_Tan_of_ArcFunc_of_X(
  ArcCos,
  /* Cos */ X,                                           //               x
  /* Sin */     SQRT(MINUS(INT(1), POW(X, INT(2)))),     // Sqrt(1 - x^2)
  /* Tan */ DIV(SQRT(MINUS(INT(1), POW(X, INT(2)))), X)  // Sqrt(1 - x^2)/x
)
IMPL_Cos_Sin_Tan_of_ArcFunc_of_X( 
  ArcCot,
  /* Cos */               INVSQRT(PLUS(INT(1), POW(X, INT(-2)))),  //       1/Sqrt(1 + 1/x^2)
  /* Sin */ TIMES(INV(X), INVSQRT(PLUS(INT(1), POW(X, INT(-2))))), // 1/x * 1/Sqrt(1 + 1/x^2)
  /* Tan */       INV(X)                                           // 1/x
)
IMPL_Cos_Sin_Tan_of_ArcFunc_of_X(
  ArcCsc,
  /* Cos */                  SQRT(MINUS(INT(1), POW(X, INT(-2)))), //         Sqrt(1 - 1/x^2)
  /* Sin */       INV(X),                                          // 1/x
  /* Tan */ TIMES(INV(X), INVSQRT(MINUS(INT(1), POW(X, INT(-2))))) // 1/x * 1/Sqrt(1 - 1/x^2))
)
IMPL_Cos_Sin_Tan_of_ArcFunc_of_X(
  ArcSec,
  /* Cos */   INV(X),                                      // 1/x
  /* Sin */          SQRT(MINUS(INT(1), POW(X, INT(-2)))), //     Sqrt(1 - 1/x^2)
  /* Tan */ TIMES(X, SQRT(MINUS(INT(1), POW(X, INT(-2))))) //   x Sqrt(1 - 1/x^2)
)
IMPL_Cos_Sin_Tan_of_ArcFunc_of_X(
  ArcSin,
  /* Cos */        SQRT(MINUS(INT(1), POW(X, INT(2)))), //     Sqrt(1 - x^2)
  /* Sin */     X,                                      // x
  /* Tan */ DIV(X, SQRT(MINUS(INT(1), POW(X, INT(2))))) // x / Sqrt(1 - x^2)
)
IMPL_Cos_Sin_Tan_of_ArcFunc_of_X(
  ArcTan,
  /* Cos */          INVSQRT(PLUS(INT(1), POW(X, INT(2)))),  // 1 / Sqrt(1 + x^2)
  /* Sin */ TIMES(X, INVSQRT(PLUS(INT(1), POW(X, INT(2))))), // x / Sqrt(1 + x^2)
  /* Tan */       X                                          // x
)
  
#define TRY_Func_of_ArcFunc_of_u( func, arcfunc ) \
  if(pmath_same(head, pmath_System_ ## arcfunc )) { \
    pmath_unref(*expr); \
    *expr = do_ ## func ## _of_ ## arcfunc ## _of_x(u); \
    pmath_unref(u); \
    return TRUE; \
  }

#define IMPL_body_try_Func_of_triginverse( func ) \
  pmath_t head, u; \
  if(!pmath_is_expr(x) || pmath_expr_length(x) != 1) \
    return FALSE; \
  head = pmath_expr_get_item(x, 0); \
  pmath_unref(head); \
  u = pmath_expr_get_item(x, 1); \
  TRY_Func_of_ArcFunc_of_u( func, ArcCos ) \
  TRY_Func_of_ArcFunc_of_u( func, ArcCot ) \
  TRY_Func_of_ArcFunc_of_u( func, ArcCsc ) \
  TRY_Func_of_ArcFunc_of_u( func, ArcSec ) \
  TRY_Func_of_ArcFunc_of_u( func, ArcSin ) \
  TRY_Func_of_ArcFunc_of_u( func, ArcTan ) \
  pmath_unref(u); \
  return FALSE;

PMATH_PRIVATE pmath_bool_t try_cos_of_triginverse(pmath_t *expr, pmath_t x) {
  IMPL_body_try_Func_of_triginverse( Cos )
}

PMATH_PRIVATE pmath_bool_t try_sin_of_triginverse(pmath_t *expr, pmath_t x) {
  IMPL_body_try_Func_of_triginverse( Sin )
}

PMATH_PRIVATE pmath_bool_t try_tan_of_triginverse(pmath_t *expr, pmath_t x) {
  IMPL_body_try_Func_of_triginverse( Tan )
}
