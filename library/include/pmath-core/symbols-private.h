#ifndef __PMATH_CORE__SYMBOLS_PRIVATE_H__
#define __PMATH_CORE__SYMBOLS_PRIVATE_H__

#ifndef BUILDING_PMATH
#  error This header file is not part of the public pMath API
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

/** Replace a symbol's current tracker id and return the previous value.
    
    This can be used to temporarily disable tracking changes to the symbol. Its typical usage pattern is:
    <code>
    id = _pmath_symbol_hard_reset_tracker(symbol, 0);
    // ... evaluate, preferrably without tracking symbol changes ...
    _pmath_symbol_lost_dynamic_tracker(symbol, 0, id);
    </code>
 */
PMATH_PRIVATE
intptr_t _pmath_symbol_hard_reset_tracker(pmath_symbol_t symbol, intptr_t id);

/** Inform a symbol that one of it's trackers is removed.
    
    \param symbol A pMath symbol.
    \param oldid  The id of the tracker that was removed.
    \param other_tracker_id The id of an arbitray other tracker or 0 if no tracker remains.
 */
PMATH_PRIVATE
void _pmath_symbol_lost_dynamic_tracker(pmath_symbol_t symbol, intptr_t oldid, intptr_t other_tracker_id);

/** Gives an extimate for the number of references to the symbol directly held by itself (circular references).

    This function may overestimate the references when common sub-expressions are shared, e.g. in
    <tt>With({common:= f(x)}, x()::= {common, common})</tt>, x is counted twice, although this 
    definition only increases its refcount by 1.
 */
PMATH_PRIVATE
intptr_t _pmath_symbol_self_refcount(pmath_symbol_t symbol);

PMATH_PRIVATE void         _pmath_symbols_memory_panic(void);
PMATH_PRIVATE pmath_bool_t _pmath_symbols_init(void);
PMATH_PRIVATE void         _pmath_symbols_almost_done(void);
PMATH_PRIVATE void         _pmath_symbols_done(void);

#endif /* __PMATH_CORE__SYMBOLS_PRIVATE_H__ */
