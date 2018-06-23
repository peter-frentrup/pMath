#include "stdafx.h"

#define PMATH_IO_SYMBOL(              sym, str_name)        PMATH_PRIVATE pmath_symbol_t sym = PMATH_STATIC_NULL;
#define PMATH_IO_SYMBOL_DOWNFUNC_IMPL(sym, str_name, func)  PMATH_PRIVATE pmath_t func(pmath_expr_t expr);
#  include "symbols.inc"
#undef PMATH_IO_SYMBOL_DOWNFUNC_IMPL
#undef PMATH_IO_SYMBOL

PMATH_MODULE
pmath_bool_t pmath_module_init(pmath_string_t filename) {
#define VERIFY(x)             do{ if(pmath_is_null(x)) goto FAIL; }while(0)
#define NEW_SYMBOL(name)      pmath_symbol_get(PMATH_C_STRING((name)), TRUE)

#define BIND(sym, func, use)  do{ if(!pmath_register_code((sym), (func), (use))) goto FAIL; }while(0)
#define BIND_DOWN(sym, func)  BIND((sym), (func), PMATH_CODE_USAGE_DOWNCALL)

#define PROTECT(sym) pmath_symbol_set_attributes((sym), pmath_symbol_get_attributes((sym)) | PMATH_SYMBOL_ATTRIBUTE_PROTECTED)

  // create the symbols
#  define PMATH_IO_SYMBOL(sym, str_name)    VERIFY( sym = NEW_SYMBOL(str_name) );
#    include "symbols.inc"
#  undef PMATH_IO_SYMBOL

  // bind the functions
#  define PMATH_IO_SYMBOL(              sym, str_name)
#  define PMATH_IO_SYMBOL_DOWNFUNC_IMPL(sym, str_name, func)   BIND_DOWN(sym, func);
#    include "symbols.inc"
#  undef PMATH_IO_SYMBOL_DOWNFUNC_IMPL
#  undef PMATH_IO_SYMBOL

  return TRUE;
  
FAIL:
  // free the symbols
#  define PMATH_IO_SYMBOL(sym, str_name)    pmath_unref(sym); sym = PMATH_NULL;
#    include "symbols.inc"
#  undef PMATH_IO_SYMBOL
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
#  define PMATH_IO_SYMBOL(sym, str_name)    pmath_unref(sym); sym = PMATH_NULL;
#    include "symbols.inc"
#  undef PMATH_IO_SYMBOL
}
