#include <pmath.h>


static pmath_symbol_t P4E_SYMBOL_TEST = PMATH_STATIC_NULL;

extern pmath_t p4e_builtin_test(pmath_expr_t _expr);


PMATH_MODULE
pmath_bool_t pmath_module_init(pmath_string_t filename) {
#define VERIFY(x)             do{ if(pmath_is_null(x)) goto FAIL; }while(0)
#define NEW_SYMBOL(name)      pmath_symbol_get(PMATH_C_STRING((name)), TRUE)

#define BIND(sym, func, use)  do{ if(!pmath_register_code((sym), (func), (use))) goto FAIL; }while(0)
#define BIND_DOWN(sym, func)  BIND((sym), (func), PMATH_CODE_USAGE_DOWNCALL)

#define PROTECT(sym) pmath_symbol_set_attributes((sym), pmath_symbol_get_attributes((sym)) | PMATH_SYMBOL_ATTRIBUTE_PROTECTED)


  VERIFY(P4E_SYMBOL_TEST = NEW_SYMBOL("Eigen`Test"));
  
  BIND_DOWN(P4E_SYMBOL_TEST, p4e_builtin_test);
  
  PROTECT(P4E_SYMBOL_TEST);
  
  return TRUE;
  
  
FAIL:
  return FALSE;
  
#undef VERIFY
#undef NEW_SYMBOL
#undef BIND
#undef BIND_DOWN
#undef PROTECT
}

PMATH_MODULE
void pmath_module_done(void) {
  pmath_unref(P4E_SYMBOL_TEST);
}
