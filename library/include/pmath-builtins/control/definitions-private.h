#ifndef __BMATH_BUILTINS__CONTROL__DEFINITIONS_PRIVATE_H__
#define __BMATH_BUILTINS__CONTROL__DEFINITIONS_PRIVATE_H__

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
  pmath_t             obj); // wont be freed

PMATH_PRIVATE
pmath_bool_t _pmath_clear(pmath_symbol_t sym); // wont be freed

PMATH_PRIVATE
pmath_bool_t _pmath_assign(
  pmath_symbol_t tag,   // wont be freed; PMATH_UNDEFINED = automatic
  pmath_t lhs,   // will be freed 
  pmath_t rhs);  // will be freed; PMATH_UNDEFINED = remove rule

/* return value: 
    -1 = deleayed assign (AssignDeleayed, TagAssignDeleayed, UnAssign, TagUnassign)
     0 = no assignment
     1 = direct assignment (Assign, TagAssign)
   
   if return value = 0: *lhs = NULL, *tag = *rhs = PMATH_UNDEFINED
 */
PMATH_PRIVATE
int _pmath_is_assignment(
  pmath_expr_t  expr,  // wont be freed
  pmath_t     *tag,   // out, may be PMATH_UNDEFINED
  pmath_t     *lhs,   // out
  pmath_t     *rhs);  // out, may be PMATH_UNDEFINED

#endif /* __BMATH_BUILTINS__CONTROL__DEFINITIONS_PRIVATE_H__ */
