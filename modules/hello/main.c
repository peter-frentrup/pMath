#include <stdint.h>
#include <stdlib.h>
#include <pmath.h>

static pmath_symbol_t HELLO_SYMBOL_GREET = PMATH_STATIC_NULL;

static pmath_t hello_func_greet(pmath_expr_t expr){
  pmath_unref(expr);
  return PMATH_C_STRING("Hi there.");
}

PMATH_MODULE
pmath_bool_t pmath_module_init(pmath_string_t filename){
#define VERIFY(x)             do{ if(pmath_is_null(x)) goto FAIL; }while(0)
#define NEW_SYMBOL(name)      pmath_symbol_get(PMATH_C_STRING((name)), TRUE)

#define BIND(sym, func, use)  do{ if(!pmath_register_code((sym), (func), (use))) goto FAIL; }while(0)
#define BIND_DOWN(sym, func)  BIND((sym), (func), PMATH_CODE_USAGE_DOWNCALL)

#define PROTECT(sym) pmath_symbol_set_attributes((sym), pmath_symbol_get_attributes((sym)) | PMATH_SYMBOL_ATTRIBUTE_PROTECTED)

  VERIFY(HELLO_SYMBOL_GREET = NEW_SYMBOL("Hello`Greet"));
  
  BIND_DOWN(HELLO_SYMBOL_GREET, hello_func_greet);
  
  PROTECT(HELLO_SYMBOL_GREET);
  
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
void pmath_module_done(void){
  pmath_unref(HELLO_SYMBOL_GREET);
}
