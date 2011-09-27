#ifndef __PMATH_BUILTINS__LISTS_PRIVATE_H__
#define __PMATH_BUILTINS__LISTS_PRIVATE_H__

#ifndef BUILDING_PMATH
#error This header file is not part of the public pMath API
#endif

#include <pmath-core/expressions.h>
#include <pmath-core/symbols.h>

/* This header exports all definitions of the sources in
   src/pmath-builtins/lists/
 */

PMATH_PRIVATE pmath_t _pmath_object_head(pmath_t obj); // obj wont be freed

/* Infinity --> num=max
   n        --> num=n
   -n       --> num = max+1-n iff max != SIZE_MAX and max+1 >= n
                SIZE_MAX      otherwise
 */
PMATH_PRIVATE
pmath_bool_t extract_number(
  pmath_t  number,
  size_t   max,
  size_t  *num);

PMATH_PRIVATE long _pmath_object_depth(pmath_t obj);

// -1 = not deep enough
//  0 = in range
// +1 = too deep
PMATH_PRIVATE int _pmath_object_in_levelspec(
  pmath_t obj,
  long    levelmin,
  long    levelmax,
  long    level);

PMATH_PRIVATE
pmath_bool_t _pmath_extract_levels(
  pmath_t  levelspec,
  long    *levelmin,
  long    *levelmax);

/* n         --> max=n, min changed iff change_min_on_number, otherwise min unchanged
   a..b      --> min=a, max=b
   a..       --> min=a, max unchanged
   ..b       --> max=b, min unchanged
   ..        --> min, max unchanged
   Infinity  --> min, max unchanged

   negative n/a/b are treated as in extract_number
 */
PMATH_PRIVATE
pmath_bool_t extract_range(
  pmath_t       range,
  size_t       *min, // in/out
  size_t       *max, // in/out
  pmath_bool_t  change_min_on_number);

PMATH_PRIVATE
pmath_bool_t extract_delta_range(
  pmath_t  range,
  pmath_t *start,
  pmath_t *delta,
  size_t  *count);

PMATH_PRIVATE
pmath_bool_t _pmath_extract_longrange(
  pmath_t  range,
  long    *start,
  long    *end,
  long    *step);

PMATH_PRIVATE
pmath_symbol_t _pmath_topmost_symbol(pmath_t obj); // obj wont be freed

PMATH_PRIVATE
pmath_bool_t _pmath_is_matrix( // in dimensions.c
  pmath_t       m, // wont be freed
  size_t       *rows,
  size_t       *cols,
  pmath_bool_t  check_non_list_entries);

PMATH_PRIVATE
pmath_bool_t _pmath_is_vector( // in dimensions.c
  pmath_t v);

PMATH_PRIVATE
void _pmath_matrix_is_triangular( // in ludecomposition
  pmath_expr_t  m, // wont be freed, mjst be a square matrix!
  pmath_bool_t *lower_has_nonzeros,
  pmath_bool_t *diagonal_has_nonzeros,
  pmath_bool_t *diagonal_has_zeros,
  pmath_bool_t *upper_has_nonzeros);

PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
pmath_expr_t _pmath_dimensions(
  pmath_t obj,           // wont be freed
  size_t  maxdepth);

struct _pmath_map_info_t {
  pmath_bool_t with_heads;
  pmath_t      function;
  long         levelmin;
  long         levelmax;
};

struct _pmath_scan_info_t {
  pmath_bool_t with_heads;
  pmath_t      function;
  pmath_t      result;
  long         levelmin;
  long         levelmax;
};

PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
pmath_t _pmath_map(
  struct _pmath_map_info_t *info,
  pmath_t                   obj,   // will be freed
  long                      level);

PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
pmath_t _pmath_map_indexed(
  struct _pmath_map_info_t *info,
  pmath_t                   obj,    // will be freed
  pmath_expr_t              index); // will be freed

PMATH_PRIVATE
pmath_bool_t _pmath_scan(
  struct _pmath_scan_info_t *info,
  pmath_t                    obj, // will be freed
  long                       level);

PMATH_PRIVATE
pmath_bool_t _pmath_contains_symbol( // in length.c
  pmath_t        obj,  // wont be freed
  pmath_symbol_t sub); // wont be freed


PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
pmath_expr_t _pmath_expr_prepend(
  pmath_expr_t expr,  // will be freed
  pmath_t      item); // will be freed

/* matrix must point to a quadratic matrix,
   indx must be to sizeof(size_t) * pmath_expr_length(matrix) bytes,
   returns: *matrix contains lower and upper matrix, indx defines the
   permutation, return value is 0 iff matrix was singular and sign of
   permutation otherwise (#of row excahnges)

   The actual permutation is given by transitions
     (1 indx[1])(2 indx[2])...(n indx[n]), (from left to right)
   so indx = [3,3,3] stands for the permutation (1 3)(2 3)(3 3) = (3 1 2)

   if sing_fast_exit == TRUE and *matrix is singular, the decomposition stop
   immediately and the result is possibly not a valid LU-matrix
 */
PMATH_PRIVATE
int _pmath_matrix_ludecomp(
  pmath_expr_t *matrix,
  size_t       *indx,
  pmath_bool_t  sing_fast_exit);

/* Does the backsubstitution. To solve Dot(A,x) = b, do:
   *vector may be a matrix. You must apply the permutation yourself:

   ... alloc memory for indx ...
   d = _pmath_matrix_ludecomp(&A, indx, TRUE or FALSE) // changes A
   if(d != 0){
     for(i = 1..n)
       {b[i], b[indx[i]]} = {b[indx[i]], b[i]};

     _pmath_matrix_lubacksubst(A, b)
   }
   else
     ... error: matrix is singular ...

   ... free perm ...
   x = b;
 */
PMATH_PRIVATE
void _pmath_matrix_lubacksubst(
  pmath_expr_t  lumatrix,
  pmath_expr_t *vector);


PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
pmath_t _pmath_matrix_get(
  pmath_expr_t matrix,
  size_t       r,
  size_t       c);

PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
pmath_expr_t _pmath_matrix_set( // return: new matrix
  pmath_expr_t matrix, // will be freed
  size_t       r,
  size_t       c,
  pmath_t      value); // will be freed



PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
pmath_t _pmath_tensor_get(
  pmath_t       tensor,  // will be freed
  size_t        depth,
  const size_t *idx);

PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
pmath_t _pmath_tensor_set(
  pmath_t       tensor,  // will be freed
  size_t        depth,
  const size_t *idx,
  pmath_t       obj);    // will be freed

#endif /* __PMATH_BUILTINS__LISTS_PRIVATE_H__ */
