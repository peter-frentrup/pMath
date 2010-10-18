#include "pj-symbols.h"
#include "pjvm.h"

#include <string.h>


pmath_symbol_t _pj_symbols[PJ_SYMBOLS_COUNT];


pmath_bool_t pj_symbols_init(void){
  memset(_pj_symbols, 0, sizeof(_pj_symbols));
  
  #define VERIFY(x)             do{ if(0 == (x)) goto FAIL; }while(0)
  #define NEW_SYMBOL(name)      pmath_symbol_get(PMATH_C_STRING((name)), TRUE)

  #define BIND(sym, func, use)  do{ if(!pmath_register_code((sym), (func), (use))) goto FAIL; }while(0)
  #define BIND_DOWN(sym, func)  BIND((sym), (func), PMATH_CODE_USAGE_DOWNCALL)

  #define PROTECT(sym) pmath_symbol_set_attributes((sym), pmath_symbol_get_attributes((sym)) | PMATH_SYMBOL_ATTRIBUTE_PROTECTED)

  VERIFY(PJ_SYMBOL_JAVA         = NEW_SYMBOL("Java`Java"));
  VERIFY(PJ_SYMBOL_JAVACLASS    = NEW_SYMBOL("Java`JavaClass"));
  VERIFY(PJ_SYMBOL_JAVAKILLVM   = NEW_SYMBOL("Java`JavaKillVM"));
  VERIFY(PJ_SYMBOL_JAVAOBJECT   = NEW_SYMBOL("Java`JavaObject"));
  VERIFY(PJ_SYMBOL_JAVASTARTVM  = NEW_SYMBOL("Java`JavaStartVM"));
  
  BIND_DOWN(PJ_SYMBOL_JAVASTARTVM, pj_builtin_startvm);
  BIND_DOWN(PJ_SYMBOL_JAVAKILLVM,  pj_builtin_killvm);
  
  {
    size_t i;
    for(i = 0;i < PJ_SYMBOLS_COUNT;++i){
      PROTECT(_pj_symbols[i]);
    }
  }
  
  return TRUE;
  
 FAIL:
  {
    size_t i;
    for(i = 0;i < PJ_SYMBOLS_COUNT;++i){
      pmath_unref(_pj_symbols[i]);
      _pj_symbols[i] = NULL;
    }
  }
  return FALSE;

#undef VERIFY
#undef NEW_SYMBOL
#undef BIND
#undef BIND_DOWN
#undef PROTECT
}

void pj_symbols_done(void){
  size_t i;
  for(i = 0;i < PJ_SYMBOLS_COUNT;++i){
    pmath_unref(_pj_symbols[i]);
    _pj_symbols[i] = NULL;
  }
}
