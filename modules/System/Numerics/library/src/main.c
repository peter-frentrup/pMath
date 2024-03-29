#include "stdafx.h"

#define PMATH_NUMERICS_SYMBOL(              sym, str_name)        PMATH_PRIVATE pmath_symbol_t sym = PMATH_STATIC_NULL;
#define PMATH_NUMERICS_SYMBOL_DOWNFUNC_IMPL(sym, str_name, func)  PMATH_PRIVATE pmath_t func(pmath_expr_t expr);
#  include "symbols.inc"
#undef PMATH_NUMERICS_SYMBOL_DOWNFUNC_IMPL
#undef PMATH_NUMERICS_SYMBOL

static pmath_bool_t pmath_numerics_init_security_doormen(void);

PMATH_MODULE
pmath_bool_t pmath_module_init(pmath_string_t filename) {
#define VERIFY(x)             do{ if(pmath_is_null(x)) goto FAIL; }while(0)
#define NEW_SYMBOL(name)      pmath_symbol_get(PMATH_C_STRING((name)), TRUE)

#define BIND(sym, func, use)  do{ if(!pmath_register_code((sym), (func), (use))) goto FAIL; }while(0)
#define BIND_DOWN(sym, func)  BIND((sym), (func), PMATH_CODE_USAGE_DOWNCALL)

#define PROTECT(sym) pmath_symbol_set_attributes((sym), pmath_symbol_get_attributes((sym)) | PMATH_SYMBOL_ATTRIBUTE_PROTECTED)

  // create the symbols
#  define PMATH_NUMERICS_SYMBOL(sym, str_name)    VERIFY( sym = NEW_SYMBOL(str_name) );
#    include "symbols.inc"
#  undef PMATH_NUMERICS_SYMBOL

  // bind the functions
#  define PMATH_NUMERICS_SYMBOL(              sym, str_name)
#  define PMATH_NUMERICS_SYMBOL_DOWNFUNC_IMPL(sym, str_name, func)   BIND_DOWN(sym, func);
#    include "symbols.inc"
#  undef PMATH_NUMERICS_SYMBOL_DOWNFUNC_IMPL
#  undef PMATH_NUMERICS_SYMBOL
  
  if(!pmath_numerics_init_security_doormen()) goto FAIL;
  
  return TRUE;
  
FAIL:
  // free the symbols
#  define PMATH_NUMERICS_SYMBOL(sym, str_name)    pmath_unref(sym); sym = PMATH_NULL;
#    include "symbols.inc"
#  undef PMATH_NUMERICS_SYMBOL
  return FALSE;
  
#undef VERIFY
#undef NEW_SYMBOL
#undef BIND
#undef BIND_DOWN
#undef PROTECT
}

PMATH_MODULE
void pmath_module_done(void) {
  // free the symbols
#  define PMATH_NUMERICS_SYMBOL(sym, str_name)    pmath_unref(sym); sym = PMATH_NULL;
#    include "symbols.inc"
#  undef PMATH_NUMERICS_SYMBOL
}


static pmath_bool_t pmath_numerics_init_security_doormen(void) {
#  define CHECK(x)  if(!(x)) return FALSE

#  define PMATH_NUMERICS_SYMBOL(sym, str_name)
#  define PMATH_NUMERICS_FUNC_SECURITY_IMPL(func, level)   CHECK( pmath_security_register_doorman(func, level, NULL) );
#    include "symbols.inc"
#  undef PMATH_NUMERICS_FUNC_SECURITY_IMPL
#  undef PMATH_NUMERICS_SYMBOL


  return TRUE;
}
