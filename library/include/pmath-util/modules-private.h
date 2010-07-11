#ifndef __PMATH_UTIL__MODULES_PRIVATE_H__
#define __PMATH_UTIL__MODULES_PRIVATE_H__

#ifndef BUILDING_PMATH
  #error This header file is not part of the public pMath API
#endif

// initialized in pmath_init():
PMATH_PRIVATE extern pmath_t _pmath_object_loadlibrary_load_message;
PMATH_PRIVATE extern pmath_t _pmath_object_get_load_message;

PMATH_PRIVATE pmath_bool_t _pmath_module_load(pmath_string_t filename); // filename will be freed

PMATH_PRIVATE pmath_bool_t _pmath_modules_init(void);
PMATH_PRIVATE void         _pmath_modules_done(void);

#endif /* __PMATH_UTIL__MODULES_PRIVATE_H__ */
