#include "pjvm.h"
#include "pj-classes.h"
#include "pj-objects.h"
#include "pj-symbols.h"


PMATH_MODULE
pmath_bool_t pmath_module_init(void){
  
  pjvm_auto_detach_key = NULL;
  
  if(!pjvm_init())        goto FAIL_PJVM;
  if(!pj_classes_init())  goto FAIL_PJ_CLASSES;
  if(!pj_objects_init())  goto FAIL_PJ_OBJECTS;
  if(!pj_symbols_init())  goto FAIL_PJ_SYMBOLS;
  
  pjvm_auto_detach_key = pmath_expr_new_extended(
    pmath_ref(PJ_SYMBOL_JAVA), 1,
    pmath_ref(PMATH_SYMBOL_ENVIRONMENT));
  
  if(pjvm_auto_detach_key)
    return TRUE;
                      pj_symbols_done();
 FAIL_PJ_SYMBOLS:     pj_objects_done();
 FAIL_PJ_OBJECTS:     pj_classes_done();
 FAIL_PJ_CLASSES:     pjvm_done();
 FAIL_PJVM:
  return FALSE;
}

PMATH_MODULE
void pmath_module_done(void){
  pj_symbols_done();
  pj_objects_done();
  pj_classes_done();
  pjvm_done();
  
  pmath_unref(pjvm_auto_detach_key);
  pjvm_auto_detach_key = NULL;
  
  pmath_debug_print("[java done]");
}
