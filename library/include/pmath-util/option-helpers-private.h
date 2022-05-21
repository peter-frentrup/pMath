#ifndef __PMATH_UTIL__OPTION_HELPERS_PRIVATE_H__
#define __PMATH_UTIL__OPTION_HELPERS_PRIVATE_H__

#ifndef BUILDING_PMATH
#  error This header file is not part of the public pMath API
#endif

#include <pmath-util/option-helpers.h>


/* name won't be freed
   set  won't be freed
   
   returns PMATH_NULL if no rule was found
 */
PMATH_PRIVATE 
pmath_expr_t _pmath_option_find_rule(pmath_t name, pmath_t set);


/* name won't be freed
   set  won't be freed
   
   returns PMATH_UNDEFINED if no value was found
 */
PMATH_PRIVATE 
pmath_t _pmath_option_find_value(pmath_t name, pmath_t set);


PMATH_PRIVATE
pmath_bool_t _pmath_options_check_subset_of(
  pmath_t     set, 
  pmath_t     default_options, 
  pmath_t     msg_head, // won't be freed
  const char *msg_tag, // "optx" or "optnf" or NULL
  pmath_t     msg_arg);

  
/* expr won't be freed.
   
   For expr = F(A,B->1,C->2,{D->3},E->4) with Options(F)={B->b,D->d,E->e}
   this gives              {{D->3},E->4}
 */
PMATH_PRIVATE
pmath_t _pmath_options_from_expr(pmath_t expr);

#endif // __PMATH_UTIL__OPTION_HELPERS_PRIVATE_H__
