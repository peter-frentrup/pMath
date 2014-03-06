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
pmath_expr_t _pmath_expr_unpack_array(pmath_packed_array_t array, pmath_bool_t recursive);

PMATH_PRIVATE 
PMATH_ATTRIBUTE_USE_RESULT
pmath_expr_t _pmath_expr_pack_array(pmath_expr_t expr);


#define PMATH_ARRAYS_INCOMPATIBLE_CMP  (2)
PMATH_PRIVATE
int _pmath_packed_array_compare(pmath_packed_array_t a, pmath_packed_array_t b);

PMATH_PRIVATE 
pmath_bool_t _pmath_packed_array_equal(
  pmath_packed_array_t a,
  pmath_packed_array_t b);

PMATH_PRIVATE
size_t _pmath_packed_array_find_sorted(
  pmath_packed_array_t sorted_array, // wont be freed
  pmath_t              item);        // wont be freed
  
PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
pmath_packed_array_t _pmath_packed_array_resize(
  pmath_packed_array_t array,
  size_t               new_length);

PMATH_PRIVATE 
PMATH_ATTRIBUTE_USE_RESULT
pmath_expr_t _pmath_packed_array_sort(pmath_packed_array_t array); // will be freed

PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
pmath_expr_t _pmath_packed_array_map(
  pmath_packed_array_t  array, // will be freed
  size_t                start,
  size_t                end,
  pmath_t             (*func)(pmath_t, size_t, void*),
  void                 *context);

PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
pmath_packed_array_t _pmath_packed_array_flatten(
  pmath_packed_array_t  array,
  size_t                depth);

PMATH_PRIVATE
size_t _pmath_packed_array_bytecount(pmath_packed_array_t array);

PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
pmath_t _pmath_packed_array_form(pmath_packed_array_t array);
  
PMATH_PRIVATE pmath_bool_t _pmath_packed_arrays_init(void);
PMATH_PRIVATE void         _pmath_packed_arrays_done(void);

#endif // __PMATH_CORE__PACKED_ARRAYS_PRIVATE_H__
