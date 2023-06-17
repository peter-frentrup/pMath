#ifndef __PMATH_BUILTINS__CONTROL__DEFINITIONS_PRIVATE_H__
#define __PMATH_BUILTINS__CONTROL__DEFINITIONS_PRIVATE_H__

#ifndef BUILDING_PMATH
#error This header file is not part of the public pMath API
#endif

#include <pmath-core/expressions.h>
#include <pmath-core/symbols.h>

/* This header exports all definitions of the sources in
   src/pmath-builtins/control/definitions/
 */

PMATH_PRIVATE
pmath_t _pmath_extract_holdpattern(pmath_t pat); // pat will be freed

PMATH_PRIVATE
pmath_bool_t _pmath_get_attributes(
  pmath_symbol_attributes_t *attr,
  pmath_t                    obj); // wont be freed

PMATH_PRIVATE
pmath_symbol_attributes_t _pmath_get_function_attributes(pmath_t head);

enum _pmath_clear_flags_t {
  PMATH_CLEAR_BASIC_RULES      = 1,
  PMATH_CLEAR_ALL_RULES        = 2,
  PMATH_CLEAR_BASIC_ATTRIBUTES = 4,
  PMATH_CLEAR_BUILTIN_CODE     = 8,
};

PMATH_PRIVATE
pmath_bool_t _pmath_clear(pmath_symbol_t sym, enum _pmath_clear_flags_t flags); // wont be freed


#define ALL_RULES     0
#define OWN_RULES     1
#define UP_RULES      2
#define DOWN_RULES    3
#define SUB_RULES     4

#define SYM_SEARCH_OK            0
#define SYM_SEARCH_NOTFOUND      1
#define SYM_SEARCH_ALTERNATIVES  2
#define SYM_SEARCH_TOODEEP       3

PMATH_PRIVATE
int _pmath_find_tag( // SYM_SEARCH_XXX
  pmath_t          lhs,         // wont be freed
  pmath_symbol_t   in_tag,      // wont be freed; PMATH_UNDEFINED = automatic
  pmath_symbol_t  *out_tag,     // set to PMATH_NULL before call!
  int             *kind_of_lhs, // XXX_RULES
  pmath_bool_t     literal);

PMATH_PRIVATE
pmath_bool_t _pmath_assign(
  pmath_symbol_t tag,   // wont be freed; PMATH_UNDEFINED = automatic
  pmath_t        lhs,   // will be freed
  pmath_t        rhs);  // will be freed; PMATH_UNDEFINED = remove rule

/* return value:
    -1 = delayed assign (AssignDelayed, TagAssignDelayed, Unassign, TagUnassign)
     0 = no assignment
     1 = direct assignment (Assign, TagAssign)

   if return value = 0: *lhs = PMATH_NULL, *tag = *rhs = PMATH_UNDEFINED
 */
PMATH_PRIVATE
int _pmath_is_assignment(
  pmath_expr_t  expr,  // wont be freed
  pmath_t      *tag,   // out, may be PMATH_UNDEFINED
  pmath_t      *lhs,   // out
  pmath_t      *rhs);  // out, may be PMATH_UNDEFINED

#endif /* __PMATH_BUILTINS__CONTROL__DEFINITIONS_PRIVATE_H__ */
