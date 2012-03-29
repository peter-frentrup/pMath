#include <pmath.h>


static pmath_symbol_t P4E_SYMBOL_TEST                 = PMATH_STATIC_NULL;
static pmath_symbol_t P4E_SYMBOL_FULLPIVLU            = PMATH_STATIC_NULL;
static pmath_symbol_t P4E_SYMBOL_PARTIALPIVLU         = PMATH_STATIC_NULL;
static pmath_symbol_t P4E_SYMBOL_HOUSEHOLDERQR        = PMATH_STATIC_NULL;
static pmath_symbol_t P4E_SYMBOL_COLPIVHOUSEHOLDERQR  = PMATH_STATIC_NULL;
static pmath_symbol_t P4E_SYMBOL_FULLPIVHOUSEHOLDERQR = PMATH_STATIC_NULL;

extern pmath_t p4e_builtin_test(                pmath_expr_t _expr);
extern pmath_t p4e_builtin_fullpivlu(           pmath_expr_t _expr);
extern pmath_t p4e_builtin_partialpivlu(        pmath_expr_t _expr);
extern pmath_t p4e_builtin_householderqr(       pmath_expr_t _expr);
extern pmath_t p4e_builtin_colpivhouseholderqr( pmath_expr_t _expr);
extern pmath_t p4e_builtin_fullpivhouseholderqr(pmath_expr_t _expr);


static void clear_symbols() {
  pmath_unref(P4E_SYMBOL_TEST);                 P4E_SYMBOL_TEST                 = PMATH_NULL;
  pmath_unref(P4E_SYMBOL_FULLPIVLU);            P4E_SYMBOL_FULLPIVLU            = PMATH_NULL;
  pmath_unref(P4E_SYMBOL_PARTIALPIVLU);         P4E_SYMBOL_PARTIALPIVLU         = PMATH_NULL;
  pmath_unref(P4E_SYMBOL_HOUSEHOLDERQR);        P4E_SYMBOL_HOUSEHOLDERQR        = PMATH_NULL;
  pmath_unref(P4E_SYMBOL_COLPIVHOUSEHOLDERQR);  P4E_SYMBOL_COLPIVHOUSEHOLDERQR  = PMATH_NULL;
  pmath_unref(P4E_SYMBOL_FULLPIVHOUSEHOLDERQR); P4E_SYMBOL_FULLPIVHOUSEHOLDERQR = PMATH_NULL;
}

PMATH_MODULE
pmath_bool_t pmath_module_init(pmath_string_t filename) {
#define VERIFY(x)             do{ if(pmath_is_null(x)) goto FAIL; }while(0)
#define NEW_SYMBOL(name)      pmath_symbol_get(PMATH_C_STRING((name)), TRUE)

#define BIND(sym, func, use)  do{ if(!pmath_register_code((sym), (func), (use))) goto FAIL; }while(0)
#define BIND_DOWN(sym, func)  BIND((sym), (func), PMATH_CODE_USAGE_DOWNCALL)

#define PROTECT(sym) pmath_symbol_set_attributes((sym), pmath_symbol_get_attributes((sym)) | PMATH_SYMBOL_ATTRIBUTE_PROTECTED)


  VERIFY( P4E_SYMBOL_TEST                 = NEW_SYMBOL("Eigen`Test"));
  VERIFY( P4E_SYMBOL_FULLPIVLU            = NEW_SYMBOL("Eigen`FullPivLU"));
  VERIFY( P4E_SYMBOL_PARTIALPIVLU         = NEW_SYMBOL("Eigen`PartialPivLU"));
  VERIFY( P4E_SYMBOL_HOUSEHOLDERQR        = NEW_SYMBOL("Eigen`HouseholderQR"));
  VERIFY( P4E_SYMBOL_COLPIVHOUSEHOLDERQR  = NEW_SYMBOL("Eigen`ColPivHouseholderQR"));
  VERIFY( P4E_SYMBOL_FULLPIVHOUSEHOLDERQR = NEW_SYMBOL("Eigen`FullPivHouseholderQR"));
  
  BIND_DOWN( P4E_SYMBOL_TEST,                 p4e_builtin_test);
  BIND_DOWN( P4E_SYMBOL_FULLPIVLU,            p4e_builtin_fullpivlu);
  BIND_DOWN( P4E_SYMBOL_PARTIALPIVLU,         p4e_builtin_partialpivlu);
  BIND_DOWN( P4E_SYMBOL_HOUSEHOLDERQR,        p4e_builtin_householderqr);
  BIND_DOWN( P4E_SYMBOL_COLPIVHOUSEHOLDERQR,  p4e_builtin_householderqr);
  BIND_DOWN( P4E_SYMBOL_FULLPIVHOUSEHOLDERQR, p4e_builtin_householderqr);
  
  PROTECT( P4E_SYMBOL_TEST);
  PROTECT( P4E_SYMBOL_FULLPIVLU);
  PROTECT( P4E_SYMBOL_PARTIALPIVLU);
  PROTECT( P4E_SYMBOL_HOUSEHOLDERQR);
  PROTECT( P4E_SYMBOL_COLPIVHOUSEHOLDERQR);
  PROTECT( P4E_SYMBOL_FULLPIVHOUSEHOLDERQR);
  
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
