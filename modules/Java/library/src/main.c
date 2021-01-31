#include "pjvm.h"
#include "pj-classes.h"
#include "pj-objects.h"
#include "pj-threads.h"


extern pmath_symbol_t pjsym_Java_Java;
extern pmath_symbol_t pjsym_System_Environment;

static pmath_bool_t pj_symbols_init(void);
static void pj_symbols_done(void);

JNIEXPORT jint JNICALL
JNI_OnLoad(JavaVM *vm, void *reserved) {
// todo: make this reentrant and thread-safe

  printf("[JNI_OnLoad]\n");
  if(pmath_is_null(pjvm_dll_filename)) {
    // todo: load pmath in a worker thread...
    printf("[pmath not yet loaded]\n");
    return JNI_VERSION_1_4;
  }
  
  return JNI_VERSION_1_4;
}

JNIEXPORT void JNICALL
JNI_OnUnload(JavaVM *vm, void *reserved) {
  printf("[JNI_OnUnload]\n");
}


PMATH_MODULE
pmath_bool_t pmath_module_init(pmath_string_t dll_filename) {

  printf("[pmath-java: pmath_module_init]\n");
  
  pjvm_auto_detach_key = PMATH_NULL;
  pjvm_dll_filename    = PMATH_NULL;
  
  if(!pjvm_init())         goto FAIL_PJVM;
  if(!pj_classes_init())   goto FAIL_PJ_CLASSES;
  if(!pj_objects_init())   goto FAIL_PJ_OBJECTS;
  if(!pj_symbols_init())   goto FAIL_PJ_SYMBOLS;
  if(!pj_threads_init())   goto FAIL_PJ_THREADS;
  
  pjvm_dll_filename = pmath_ref(dll_filename);
  
  pjvm_auto_detach_key = pmath_expr_new_extended(
                           pmath_ref(pjsym_Java_Java), 1,
                           pmath_ref(pjsym_System_Environment));
                           
  if(!pmath_is_null(pjvm_auto_detach_key))
    return TRUE;
    
  pmath_unref(pjvm_dll_filename);
  
  pj_threads_done();
FAIL_PJ_THREADS:     pj_symbols_done();
FAIL_PJ_SYMBOLS:     pj_objects_done();
FAIL_PJ_OBJECTS:     pj_classes_done();
FAIL_PJ_CLASSES:     pjvm_done();
FAIL_PJVM:
  return FALSE;
}

PMATH_MODULE
void pmath_module_done(void) {
  pmath_unref(pjvm_dll_filename);
  pjvm_dll_filename = PMATH_NULL;
  
  pj_threads_done();
  pj_symbols_done();
  pj_objects_done();
  pj_classes_done();
  pjvm_done();
  
  pmath_unref(pjvm_auto_detach_key);
  pjvm_auto_detach_key = PMATH_NULL;
  
  pmath_debug_print("[pmath-java: pmath_module_done]\n");
}

#define PJ_DECLARE_SYMBOL(SYM, NAME)  PMATH_PRIVATE pmath_symbol_t SYM = PMATH_STATIC_NULL;
#define PJ_SYMBOL_DOWNFUNC_IMPL(SYM, NAME, FUNC)   PMATH_PRIVATE pmath_t FUNC(pmath_expr_t expr);
#  include "symbols.inc"
#undef PJ_SYMBOL_DOWNFUNC_IMPL
#undef PJ_DECLARE_SYMBOL

static pmath_bool_t pj_symbols_init(void) {
#define VERIFY(x)             do{ if(pmath_is_null(x)) goto FAIL; }while(0);
#define NEW_SYMBOL(name)      pmath_symbol_get(PMATH_C_STRING((name)), TRUE)
  
#define BIND(sym, func, use)  do{ if(!pmath_register_code((sym), (func), (use))) goto FAIL; }while(0);
#define BIND_DOWN(sym, func)  BIND((sym), (func), PMATH_CODE_USAGE_DOWNCALL)
#define BIND_UP(sym, func)  BIND((sym), (func), PMATH_CODE_USAGE_UPCALL)
  
#define PROTECT(sym)   pmath_symbol_set_attributes((sym), pmath_symbol_get_attributes((sym)) | PMATH_SYMBOL_ATTRIBUTE_PROTECTED)
  
#define PJ_DECLARE_SYMBOL(SYM, NAME)               VERIFY( SYM = NEW_SYMBOL(NAME) )
#define PJ_RESET_SYMBOL_ATTRIBUTES(SYM, ATTR)      pmath_symbol_set_attributes( (SYM), (ATTR) );
#define PJ_SYMBOL_DOWNFUNC_IMPL(SYM, NAME, FUNC)   BIND_DOWN(SYM, FUNC);
#  include "symbols.inc"
#undef PJ_SYMBOL_DOWNFUNC_IMPL
#undef PJ_RESET_SYMBOL_ATTRIBUTES
#undef PJ_DECLARE_SYMBOL

  BIND_UP(pjsym_Java_JavaField, pj_eval_upcall_Java_JavaField)
  
  return TRUE;
  
FAIL:
#define PJ_DECLARE_SYMBOL(SYM, NAME)      pmath_unref( SYM ); SYM = PMATH_NULL;
#  include "symbols.inc"
#undef PJ_DECLARE_SYMBOL

  return FALSE;
  
#undef VERIFY
#undef NEW_SYMBOL
#undef BIND
#undef BIND_DOWN
#undef PROTECT
}

static void pj_symbols_done(void) {
#define PJ_DECLARE_SYMBOL(SYM, NAME)            pmath_unref( SYM ); SYM = PMATH_NULL;
#  include "symbols.inc"
#undef PJ_DECLARE_SYMBOL
}
