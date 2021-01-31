#include "util.h"

#include <stdint.h>
#include <stdlib.h>
#include <pmath.h>

extern pmath_t windows_CommandLineToArgv(pmath_expr_t expr);
extern pmath_t windows_GetAllKnownFolders(pmath_expr_t expr);
extern pmath_t windows_RegGetValue(pmath_expr_t expr);
extern pmath_t windows_RegEnumKeys(pmath_expr_t expr);
extern pmath_t windows_RegEnumValues(pmath_expr_t expr);
extern pmath_t windows_ShellExecute(pmath_expr_t expr);
extern pmath_t windows_SHGetKnownFolderPath(pmath_expr_t expr);

#define P4WIN_DECLARE_SYMBOL(SYM, NAME_STR)  PMATH_PRIVATE pmath_symbol_t SYM = PMATH_STATIC_NULL;
#  include "symbols.inc"
#undef P4WIN_DECLARE_SYMBOL

static void uninit_com(void *dummy) {
  // TODO: RoUninitialize() if necessary
  CoUninitialize();
}

static void init_com(void) {
  // TODO: needs to be called on every thread once ...
  
  DWORD mode = COINIT_APARTMENTTHREADED;
  HRESULT res;
  
  pmath_t mode_obj = pmath_evaluate(pmath_ref(p4win_Windows_DollarComMultiThreadedAppartment));
  if(pmath_same(mode_obj, p4win_System_True))
    mode = COINIT_MULTITHREADED;
  else if(pmath_same(mode_obj, p4win_System_False))
    mode = COINIT_APARTMENTTHREADED;
  // else: automatic ...
  
  pmath_unref(mode_obj);
  
  // TODO: use RoInitialize() on Windows 8 and above. That is MTA. 
  // OleInitialize() requires STA, both do not seem to work together :(
  res = CoInitializeEx(NULL, mode | COINIT_DISABLE_OLE1DDE);
  
  if(check_succeeded(res)) {
    // uninitialize COM when this thread gets pmath_done()
    pmath_custom_t com_keepalive_value = pmath_custom_new(NULL, uninit_com);
    if(!pmath_is_null(com_keepalive_value)) {
      pmath_t com_keepalive_key = pmath_expr_new_extended(pmath_ref(p4win_Windows_Internal_ComInitializer), 0);
      pmath_unref(pmath_thread_local_save(com_keepalive_key, com_keepalive_value));
      pmath_unref(com_keepalive_key);
    }
  }
}

PMATH_MODULE
pmath_bool_t pmath_module_init(pmath_string_t filename) {
#define VERIFY(x)             do{ if(pmath_is_null(x)) goto FAIL; }while(0)
#define NEW_SYMBOL(name)      pmath_symbol_get(PMATH_C_STRING((name)), TRUE)

#define BIND(sym, func, use)  do{ if(!pmath_register_code((sym), (func), (use))) goto FAIL; }while(0)
#define BIND_DOWN(sym, func)  BIND((sym), (func), PMATH_CODE_USAGE_DOWNCALL)

#define P4WIN_DECLARE_SYMBOL(SYM, NAME_STR)  VERIFY( SYM = NEW_SYMBOL( NAME_STR ) );
#  include "symbols.inc"
#undef P4WIN_DECLARE_SYMBOL

  BIND_DOWN(p4win_Windows_CommandLineToArgv,          windows_CommandLineToArgv);
  BIND_DOWN(p4win_Windows_Private_GetAllKnownFolders, windows_GetAllKnownFolders);
  BIND_DOWN(p4win_Windows_RegGetValue,                windows_RegGetValue);
  BIND_DOWN(p4win_Windows_RegEnumKeys,                windows_RegEnumKeys);
  BIND_DOWN(p4win_Windows_RegEnumValues,              windows_RegEnumValues);
  BIND_DOWN(p4win_Windows_ShellExecute,               windows_ShellExecute);
  BIND_DOWN(p4win_Windows_SHGetKnownFolderPath,       windows_SHGetKnownFolderPath);
  
  PMATH_RUN("Windows`$KnownFolders::= Windows`Private`GetAllKnownFolders()");
  PMATH_RUN("Options(Windows`SHGetKnownFolderPath):= {CreateDirectory->False}");
  PMATH_RUN(
    "Options(Windows`RegGetValue):="
    "{Windows`Win64Version->Automatic,Expand->True}");
  PMATH_RUN(
    "Options(Windows`RegEnumKeys):="
    "Options(Windows`RegEnumValues):="
    "{Windows`Win64Version->Automatic}");
  
#define PROTECT(SYM)   pmath_symbol_set_attributes((SYM), pmath_symbol_get_attributes((SYM)) | PMATH_SYMBOL_ATTRIBUTE_PROTECTED);
  PROTECT( p4win_Windows_CommandLineToArgv )
  PROTECT( p4win_Windows_RegGetValue )
  PROTECT( p4win_Windows_RegEnumKeys )
  PROTECT( p4win_Windows_RegEnumValues )
  PROTECT( p4win_Windows_ShellExecute )
  PROTECT( p4win_Windows_SHGetKnownFolderPath )
  PROTECT( p4win_Windows_Win64Version )
#undef PROTECT
  
  init_com();
  
  return TRUE;
  
FAIL:
#define P4WIN_DECLARE_SYMBOL(SYM, NAME_STR)  pmath_unref(SYM);  SYM = PMATH_NULL;
#  include "symbols.inc"
#undef P4WIN_DECLARE_SYMBOL
  return FALSE;
  
#undef VERIFY
#undef NEW_SYMBOL
#undef BIND
#undef BIND_DOWN
}

PMATH_MODULE
void pmath_module_done(void) {
#define P4WIN_DECLARE_SYMBOL(SYM, NAME_STR)  pmath_unref(SYM);  SYM = PMATH_NULL;
#  include "symbols.inc"
#undef P4WIN_DECLARE_SYMBOL
}
