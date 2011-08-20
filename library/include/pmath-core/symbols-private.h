#ifndef __PMATH_CORE__SYMBOLS_PRIVATE_H__
#define __PMATH_CORE__SYMBOLS_PRIVATE_H__

#ifndef BUILDING_PMATH
#error This header file is not part of the public pMath API
#endif

#include <pmath-core/symbols.h>

typedef enum {
  RULES_READ,
  RULES_WRITEOPTIONS, // writeable even if symbol is Protected
  RULES_WRITE
} rule_access_t;

PMATH_PRIVATE
struct _pmath_symbol_rules_t *_pmath_symbol_get_rules(
  pmath_symbol_t  symbol, // NOT NULL!  wont be freed
  rule_access_t   access);

PMATH_PRIVATE
pmath_bool_t _pmath_symbol_assign_value(
  pmath_symbol_t  symbol, // wont be freed
  pmath_t         lhs,    // will be freed. typically pmath_ref(sym)
  pmath_t         rhs);   // will be freed

PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
pmath_t _pmath_symbol_get_global_value(
  pmath_symbol_t symbol); // wont be freed

PMATH_PRIVATE
void _pmath_symbol_set_global_value( // used in init
  pmath_symbol_t symbol, // wont be freed
  pmath_t        value); // will be freed

PMATH_PRIVATE
void _pmath_symbol_track_dynamic(
  pmath_symbol_t symbol, // wont be freed
  intptr_t       id);

PMATH_PRIVATE void         _pmath_symbols_memory_panic(void);
PMATH_PRIVATE pmath_bool_t _pmath_symbols_init(void);
PMATH_PRIVATE void         _pmath_symbols_almost_done(void);
PMATH_PRIVATE void         _pmath_symbols_done(void);

#endif /* __PMATH_CORE__SYMBOLS_PRIVATE_H__ */
