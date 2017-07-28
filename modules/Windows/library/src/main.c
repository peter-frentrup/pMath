#include <stdint.h>
#include <stdlib.h>
#include <pmath.h>

extern pmath_t windows_SHGetKnownFolderPath(pmath_expr_t expr);
extern pmath_t windows_GetAllKnownFolders(pmath_expr_t expr);
extern pmath_t windows_RegGetValue(pmath_expr_t expr);
extern pmath_t windows_RegEnumKeys(pmath_expr_t expr);
extern pmath_t windows_RegEnumValues(pmath_expr_t expr);

static void protect_all(pmath_symbol_t *start, size_t size) {
  pmath_symbol_t *end = start + size / sizeof(pmath_symbol_t);
  for(; start != end; ++start) {
    pmath_symbol_set_attributes(*start, pmath_symbol_get_attributes(*start) | PMATH_SYMBOL_ATTRIBUTE_PROTECTED);
  }
}

static void free_all(pmath_symbol_t *start, size_t size) {
  pmath_symbol_t *end = start + size / sizeof(pmath_symbol_t);
  while(start != end) {
    pmath_unref(*start++);
  }
}

static struct {
  pmath_symbol_t Windows_SHGetKnownFolderPath;
  pmath_symbol_t Windows_Private_GetAllKnownFolders;
  pmath_symbol_t Windows_RegGetValue;
  pmath_symbol_t Windows_RegEnumKeys;
  pmath_symbol_t Windows_RegEnumValues;
  pmath_symbol_t Windows_Win64Version;
} symbols;

const pmath_symbol_t *Windows_Win64Version = &symbols.Windows_Win64Version;

PMATH_MODULE
pmath_bool_t pmath_module_init(pmath_string_t filename) {
#define VERIFY(x)             do{ if(pmath_is_null(x)) goto FAIL; }while(0)
#define NEW_SYMBOL(name)      pmath_symbol_get(PMATH_C_STRING((name)), TRUE)

#define BIND(sym, func, use)  do{ if(!pmath_register_code((sym), (func), (use))) goto FAIL; }while(0)
#define BIND_DOWN(sym, func)  BIND((sym), (func), PMATH_CODE_USAGE_DOWNCALL)

  memset(&symbols, 0, sizeof(symbols));
  
  //if(FAILED(CoInitializeEx(NULL, ...)))
  
  VERIFY( symbols.Windows_SHGetKnownFolderPath        = NEW_SYMBOL("Windows`SHGetKnownFolderPath"));
  VERIFY( symbols.Windows_Private_GetAllKnownFolders  = NEW_SYMBOL("Windows`Private`GetAllKnownFolders"));
  VERIFY( symbols.Windows_RegGetValue                 = NEW_SYMBOL("Windows`RegGetValue"));
  VERIFY( symbols.Windows_RegEnumKeys                 = NEW_SYMBOL("Windows`RegEnumKeys"));
  VERIFY( symbols.Windows_RegEnumValues               = NEW_SYMBOL("Windows`RegEnumValues"));
  VERIFY( symbols.Windows_Win64Version                = NEW_SYMBOL("Windows`Win64Version"));
  
  BIND_DOWN(symbols.Windows_SHGetKnownFolderPath,       windows_SHGetKnownFolderPath);
  BIND_DOWN(symbols.Windows_Private_GetAllKnownFolders, windows_GetAllKnownFolders);
  BIND_DOWN(symbols.Windows_RegGetValue,                windows_RegGetValue);
  BIND_DOWN(symbols.Windows_RegEnumKeys,                windows_RegEnumKeys);
  BIND_DOWN(symbols.Windows_RegEnumValues,              windows_RegEnumValues);
  
  PMATH_RUN("Windows`$KnownFolders::= Windows`Private`GetAllKnownFolders()");
  PMATH_RUN("Options(Windows`SHGetKnownFolderPath):= {CreateDirectory->False}");
  PMATH_RUN(
    "Options(Windows`RegGetValue):="
    "{Windows`Win64Version->Automatic,Expand->True}");
  PMATH_RUN(
    "Options(Windows`RegEnumKeys):="
    "Options(Windows`RegEnumValues):="
    "{Windows`Win64Version->Automatic}");
  
  protect_all((pmath_symbol_t*)&symbols, sizeof(symbols));
  return TRUE;
  
FAIL:
  free_all((pmath_symbol_t*)&symbols, sizeof(symbols));
  return FALSE;
  
#undef VERIFY
#undef NEW_SYMBOL
#undef BIND
#undef BIND_DOWN
}

PMATH_MODULE
void pmath_module_done(void) {
  free_all((pmath_symbol_t*)&symbols, sizeof(symbols));
}
