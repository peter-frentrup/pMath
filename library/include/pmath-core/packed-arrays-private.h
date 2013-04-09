#ifndef __PMATH_CORE__PACKED_ARRAYS_PRIVATE_H__
#define __PMATH_CORE__PACKED_ARRAYS_PRIVATE_H__

#ifndef BUILDING_PMATH
#  error This header file is not part of the public pMath API
#endif

#include <pmath-core/objects-private.h>
#include <pmath-core/packed-arrays.h>


PMATH_PRIVATE
pmath_t _pmath_packed_element_unbox(const void *data, enum pmath_packed_type_t type);


PMATH_PRIVATE 
PMATH_ATTRIBUTE_PURE 
PMATH_ATTRIBUTE_USE_RESULT
pmath_t _pmath_packed_array_get_item(
  pmath_packed_array_t array,
  size_t               index);
  
PMATH_PRIVATE 
PMATH_ATTRIBUTE_PURE
PMATH_ATTRIBUTE_USE_RESULT
pmath_expr_t _pmath_packed_array_get_item_range(
  pmath_packed_array_t array,
  size_t               start,
  size_t               length);

PMATH_PRIVATE 
PMATH_ATTRIBUTE_USE_RESULT
pmath_expr_t _pmath_packed_array_set_item(
  pmath_packed_array_t array,
  size_t               index,
  pmath_t              item);
  
  
PMATH_PRIVATE 
PMATH_ATTRIBUTE_USE_RESULT
pmath_expr_t _pmath_expr_unpack_array(pmath_packed_array_t array);

PMATH_PRIVATE 
PMATH_ATTRIBUTE_USE_RESULT
pmath_expr_t _pmath_expr_pack_array(pmath_expr_t expr);

PMATH_PRIVATE 
void _pmath_packed_array_copy(
  void                         *dst,
  const size_t                 *dst_steps,
  struct _pmath_packed_array_t *src);


PMATH_PRIVATE pmath_bool_t _pmath_packed_arrays_init(void);
PMATH_PRIVATE void         _pmath_packed_arrays_done(void);

#endif // __PMATH_CORE__PACKED_ARRAYS_PRIVATE_H__
