#ifndef __PMATH_BUILTINS__FORMATING_PRIVATE_H__
#define __PMATH_BUILTINS__FORMATING_PRIVATE_H__

#ifndef BUILDING_PMATH
  #error This header file is not part of the public pMath API
#endif

#include <pmath-core/expressions.h>
#include <pmath-core/strings.h>

/* This header exports all definitions of the sources in
   src/pmath-builtins/formating/
 */
 
PMATH_PRIVATE
void _pmath_write_to_string(
  pmath_string_t *result, 
  const uint16_t *data, 
  int             len);

PMATH_PRIVATE 
PMATH_ATTRIBUTE_USE_RESULT
PMATH_ATTRIBUTE_NONNULL(1)
pmath_bool_t _pmath_stringform_write(
  struct pmath_write_ex_t *info,
  pmath_expr_t              stringform); // wont be freed

PMATH_PRIVATE 
PMATH_ATTRIBUTE_USE_RESULT
pmath_t _pmath_stringform_to_boxes(
  pmath_expr_t  stringform); // stringform wont be freed

PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
pmath_string_t _pmath_string_escape(
  pmath_string_t  prefix,   // will be freed
  pmath_string_t  string,   // will be freed
  pmath_string_t  postfix,  // will be freed
  pmath_bool_t    two_times);
  
PMATH_PRIVATE 
PMATH_ATTRIBUTE_USE_RESULT
pmath_t _pmath_prepare_shallow(
  pmath_t obj,   // will be freed
  size_t  maxdepth,
  size_t  maxlength);

PMATH_PRIVATE
long _pmath_boxes_length(pmath_t boxes);  // boxes wont be freed

PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
pmath_t _pmath_shorten_boxes(pmath_t boxes, int length);

PMATH_PRIVATE
PMATH_ATTRIBUTE_NONNULL(1)
void _pmath_write_short(struct pmath_write_ex_t *info, pmath_t obj, int length);

#endif // __PMATH_BUILTINS__FORMATING_PRIVATE_H__
