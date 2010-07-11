#ifndef __PMATH_CORE__CUSTOM_PRIVATE_H__
#define __PMATH_CORE__CUSTOM_PRIVATE_H__

#ifndef BUILDING_PMATH
  #error This header file is not part of the public pMath API
#endif

PMATH_PRIVATE pmath_bool_t _pmath_custom_objects_init(void);
PMATH_PRIVATE void            _pmath_custom_objects_done(void);
  
#endif /* __PMATH_CORE__CUSTOM_PRIVATE_H__ */
