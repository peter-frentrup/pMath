#ifndef __PMATH_BUILTINS__CONTROL__MESSAGES_PRIVATE_H__
#define __PMATH_BUILTINS__CONTROL__MESSAGES_PRIVATE_H__

#ifndef BUILDING_PMATH
  #error This header file is not part of the public pMath API
#endif

/* This header exports all definitions of the sources in
   src/pmath-builtins/control/messages/
 */

PMATH_PRIVATE pmath_bool_t _pmath_is_valid_messagename(pmath_t msg);
PMATH_PRIVATE pmath_bool_t _pmath_message_is_default_off(pmath_t msg);

PMATH_PRIVATE pmath_bool_t _pmath_message_is_on(pmath_t msg);

#endif // __PMATH_BUILTINS__CONTROL__MESSAGES_PRIVATE_H__
