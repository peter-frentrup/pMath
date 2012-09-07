#ifndef __PMATH_BUILTINS__CONTROL_PRIVATE_H__
#define __PMATH_BUILTINS__CONTROL_PRIVATE_H__

#ifndef BUILDING_PMATH
#error This header file is not part of the public pMath API
#endif

/* This header exports definitions of the sources in
   src/pmath-builtins/control/
 */

PMATH_PRIVATE pmath_bool_t _pmath_is_rule(pmath_t rule);
PMATH_PRIVATE pmath_bool_t _pmath_is_list_of_rules(pmath_t rules);

// gives _pmath_is_list_of_rules( Flatten({opts}) )
PMATH_PRIVATE pmath_bool_t _pmath_is_set_of_options(pmath_t opts);

#endif /* __PMATH_BUILTINS__CONTROL_PRIVATE_H__ */
