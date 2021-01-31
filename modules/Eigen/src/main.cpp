#include <pmath.h>


#define P4E_DECLARE_SYMBOL(SYM, NAME_STR)  PMATH_PRIVATE pmath_symbol_t SYM = PMATH_STATIC_NULL;
#  include "symbols.inc"
#undef P4E_DECLARE_SYMBOL

extern pmath_symbol_t p4e_System_Complex;

extern pmath_t p4e_builtin_fullpivlu(           pmath_expr_t _expr);
extern pmath_t p4e_builtin_partialpivlu(        pmath_expr_t _expr);
extern pmath_t p4e_builtin_householderqr(       pmath_expr_t _expr);
extern pmath_t p4e_builtin_colpivhouseholderqr( pmath_expr_t _expr);
extern pmath_t p4e_builtin_fullpivhouseholderqr(pmath_expr_t _expr);
extern pmath_t p4e_builtin_llt(                 pmath_expr_t _expr);
extern pmath_t p4e_builtin_ldlt(                pmath_expr_t _expr);
extern pmath_t p4e_builtin_jacobisvd(           pmath_expr_t _expr);


static void clear_symbols() {
#define P4E_DECLARE_SYMBOL(SYM, NAME_STR)  pmath_unref(SYM); SYM = PMATH_STATIC_NULL;
#  include "symbols.inc"
#undef P4E_DECLARE_SYMBOL
}

PMATH_MODULE
pmath_bool_t pmath_module_init(pmath_string_t filename) {
#define VERIFY(x)             do{ if(pmath_is_null(x)) goto FAIL; }while(0)
#define NEW_SYMBOL(name)      pmath_symbol_get(PMATH_C_STRING((name)), TRUE)

#define BIND(sym, func, use)  do{ if(!pmath_register_code((sym), (func), (use))) goto FAIL; }while(0)
#define BIND_DOWN(sym, func)  BIND((sym), (func), PMATH_CODE_USAGE_DOWNCALL)

#define PROTECT(sym) pmath_symbol_set_attributes((sym), pmath_symbol_get_attributes((sym)) | PMATH_SYMBOL_ATTRIBUTE_PROTECTED)

#define P4E_DECLARE_SYMBOL(SYM, NAME_STR)  VERIFY( SYM = NEW_SYMBOL(NAME_STR) );
#  include "symbols.inc"
#undef P4E_DECLARE_SYMBOL

  BIND_DOWN( p4e_Eigen_FullPivLU,            p4e_builtin_fullpivlu);
  BIND_DOWN( p4e_Eigen_PartialPivLU,         p4e_builtin_partialpivlu);
  BIND_DOWN( p4e_Eigen_HouseholderQR,        p4e_builtin_householderqr);
  BIND_DOWN( p4e_Eigen_ColPivHouseholderQR,  p4e_builtin_colpivhouseholderqr);
  BIND_DOWN( p4e_Eigen_FullPivHouseholderQR, p4e_builtin_fullpivhouseholderqr);
  BIND_DOWN( p4e_Eigen_LLT,                  p4e_builtin_llt);
  BIND_DOWN( p4e_Eigen_LDLT,                 p4e_builtin_ldlt);
  BIND_DOWN( p4e_Eigen_JacobiSVD,            p4e_builtin_jacobisvd);
  
  PMATH_RUN("Eigen`LLT::posdef:= "
            "\"The matrix `1` is not sufficiently positive definite to "
            "complete the Cholesky decomposition to reasonable accuracy.\"");
  PMATH_RUN("Eigen`LLT::herm:= Eigen`LDLT::herm:= "
            "\"The matrix `1` is not Hermitian or real and symmetric.\"");
//  PMATH_RUN("Eigen`LDLT::semi:= "
//            "\"The matrix `1` is not positive or negative semidefinite.\"");
  
  PROTECT( p4e_Eigen_FullPivLU);
  PROTECT( p4e_Eigen_PartialPivLU);
  PROTECT( p4e_Eigen_HouseholderQR);
  PROTECT( p4e_Eigen_ColPivHouseholderQR);
  PROTECT( p4e_Eigen_FullPivHouseholderQR);
  PROTECT( p4e_Eigen_LLT);
  PROTECT( p4e_Eigen_LDLT);
  PROTECT( p4e_Eigen_JacobiSVD);
  
  return TRUE;
  
  
FAIL:
  clear_symbols();
  return FALSE;
  
#undef VERIFY
#undef NEW_SYMBOL
#undef BIND
#undef BIND_DOWN
#undef PROTECT
}

PMATH_MODULE
void pmath_module_done(void) {
  clear_symbols();
}
