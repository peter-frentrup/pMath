#include "pjvm.h"
#include "pj-symbols.h"


pmath_symbol_t _pjmv_symbols[1];


PMATH_MODULE
pmath_bool_t pmath_module_init(void){
  
  if(!pjvm_init())        goto FAIL_PJVM;
  if(!pj_symbols_init())  goto FAIL_PJ_SYMBOLS;
  
  return TRUE;
                      pj_symbols_done();
 FAIL_PJ_SYMBOLS:     pjvm_done();
 FAIL_PJVM:
  return FALSE;
}

PMATH_MODULE
void pmath_module_done(void){
  pj_symbols_done();
  pjvm_done();
}
