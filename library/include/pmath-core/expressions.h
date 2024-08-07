#ifndef __PMATH_CORE__EXPRESSIONS_H__
#define __PMATH_CORE__EXPRESSIONS_H__

#include <pmath-core/objects-inline.h>

/**\defgroup expressions Expressions
   \brief Expression objects in pMath.
   
   Any pMath language-level expression (lists, terms, function calls, ...) is 
   stored in a pmath_expr_t -- an array of pMath objects. One special case
   of expressions are \ref packed_arrays.
   
   \see helpers
   @{
 */

/**\class pmath_expr_t
   \extends pmath_t
   \brief The Expression class.
   
   Because pmath_expr_t is derived from pmath_t, you can use 
   expressions wherever a pmath_t is accepted. E.g. you calculate an
   expression's hash value with pmath_hash().
   
   The pmath_type_t of strings is PMATH_TYPE_EXPRESSION.
   
   Expressions are arranged as array of objects. At index 0 is allways the head
   (function name). All arguments are at indices 1 to the length of the 
   expression (including). 
   
   At the pMath language level, any exprssion can be
   entered in the form `f(a,b,c,...)` or `a.f(b,c,...)`. For expressions with 
   only one argument, `a.f` can be used instead of `a.f()`.
   
 */
typedef pmath_t  pmath_expr_t;

/**\brief Create a new expression.
   \memberof pmath_expr_t
   \param head The expression's head (index 0). Do not use them after the call.
   \param length The number of additional items in the expression.
   \return PMATH_NULL or a new expression with head at index 0 and all other items 
           initialized to PMATH_NULL. You must destroy it with pmath_unref().
 */
PMATH_API 
PMATH_ATTRIBUTE_USE_RESULT
pmath_expr_t pmath_expr_new(
  pmath_t head,
  size_t  length);

/**\brief Create a new expression with all items given.
   \memberof pmath_expr_t
   \param head The expression's head (index 0). Do not use them after the call.
   \param length The number of additional items in the expression.
   \param ... exactly length pmath_ts. Do not use them after the call.
   \return PMATH_NULL or a new expression with head at index 0 all items at index 
           i = 1..length initialized to the i'th argument in `...`. You must 
           destroy it with pmath_unref().
 */
PMATH_API 
PMATH_ATTRIBUTE_USE_RESULT
pmath_expr_t pmath_expr_new_extended(
  pmath_t head,
  size_t  length,
  ...);

/**\brief Resize an expression.
   \memberof pmath_expr_t
   \param expr The old expression. It will be freed/invalid after the call.
   \param new_length The new length of the expression.
   \return PMATH_NULL or a new expression of length new_length. You must destroy it 
           with pmath_unref().
   
   If expr's length is less than or equals to new_length, all items at 
   1..(expr's length) are be copied and the rest is initialized with some 
   appropriate constant. 
   Otherwise, all items at 1..new_length are copied and those at 
   (new_length+1)..(expr's length) are freed.
 */
PMATH_API 
PMATH_ATTRIBUTE_USE_RESULT
pmath_expr_t pmath_expr_resize(
  pmath_expr_t expr,
  size_t       new_length);

/**\brief Append some items to an expression.
   \memberof pmath_expr_t
   \param expr The old expression. It will be freed/invalid after the call.
   \param count The number of items to append.
   \param ... exactly count pmath_ts. Do not use them after the call.
   \return PMATH_NULL or a new expression that contains all items of expr followed by
           the items in `...`. You must destroy it with pmath_unref().
   
   If expr == PMATH_NULL, the returns value is 
   pmath_expr_new_extended(PMATH_NULL,count,...).
 */
PMATH_API 
PMATH_ATTRIBUTE_USE_RESULT
pmath_expr_t pmath_expr_append(
  pmath_expr_t expr,
  size_t       count,
  ...);

/**\brief Get an expression's length.
   \memberof pmath_expr_t
   \param expr A pMath expression.
   \return The number of items in expr (not counting the head).
 */
PMATH_API
PMATH_ATTRIBUTE_PURE 
size_t pmath_expr_length(pmath_expr_t expr);

/**\brief Get an item from an expression.
   \memberof pmath_expr_t
   \param expr A pMath expression. It won't be freed.
   \param index The index of the item.
   \return A copy of the requested item, if index is not greater than the 
   length of expr and PMATH_NULL otherwise. You must destroy it with pmath_unref().
 */
PMATH_API 
PMATH_ATTRIBUTE_PURE 
PMATH_ATTRIBUTE_USE_RESULT
pmath_t pmath_expr_get_item(
  pmath_expr_t expr,
  size_t       index);

/**\brief Extract an item from an expression.
   \memberof pmath_expr_t
   \param expr A pMath expression. It won't be freed.
   \param index The index of the item.
   \return Same as pmath_expr_get_item() but if expr has refcount==1, the item 
           might be removed from expr to ensure that the item's refcount is 1.
   
   Normaly, you should use pmath_expr_get_item(). This function if for use in 
   loops which modify items of an expression.
 */
PMATH_API 
PMATH_ATTRIBUTE_PURE 
PMATH_ATTRIBUTE_USE_RESULT
pmath_t pmath_expr_extract_item(
  pmath_expr_t expr,
  size_t       index);

/**\brief Test whether an item from an expression equals a given value.
   \memberof pmath_expr_t
   \param expr A pMath expression. It won't be freed.
   \param index The index of the item.
   \param expected_item The value to compare with. It won't be freed.
   \return True, if the requested item and \a expected_item are equal. FALSE otherwise.
   
   This is essentially equivalent to
   
       item = pmath_expr_get_item(expr, index);
       eq = pmath_equals(item, expected_item);
       pmath_unref(item);
       return eq;
 */
PMATH_API 
PMATH_ATTRIBUTE_PURE 
pmath_bool_t pmath_expr_item_equals(
  pmath_expr_t expr,
  size_t       index,
  pmath_t      expected_item);

/**\brief Get multiple items from an expression.
   \memberof pmath_expr_t
   \param expr A pMath expression. It will *not* be destroyed.
   \param start The start index of the items.
   \param length The number of the items.
   \return A new expression with the same head as \a expr. Its length is 
           max(0, min(start + length, 1 + pmath_expr_length(expr)) - start)
           and it contains the items from expr beginning at index \a start.
 */
PMATH_API 
PMATH_ATTRIBUTE_PURE
PMATH_ATTRIBUTE_USE_RESULT
pmath_expr_t pmath_expr_get_item_range(
  pmath_expr_t expr,
  size_t       start,
  size_t       length);

/**\brief Get a pointer to the expression's internal items array.
   \memberof pmath_expr_t
   \param expr A pMath expression. It will *not* be destroyed.
   \return A 0-based array of pmath_t or NULL on error. This array is only valid
           while \a expr is valid and not changed.
   
   Note that this may return NULL even for valid expressions. E.g. for sparse
   arrays (not implemented yet).
   This function is for fast reading access to multiple items. You have to do 
   all the error checking alone. 
   Note that result[0] === pmath_expr_get_item(expr, 1), e.t.c.
 */
PMATH_API
const pmath_t *pmath_expr_read_item_data(pmath_expr_t expr);

  
/**\brief Set an item in an expression.
   \memberof pmath_expr_t
   \param expr A pMath expression. It will be destroyed, do not use it after the 
          call.
   \param index The index of the to-be-changed item.
   \param item The new value of the item. It will be destroyed, do not use it
          after the call.
   \return PMATH_NULL or a new expression with item at index. You must destroy it with 
           pmath_unref().
   
   If index is greater than expr's length, item will be destroyed and the 
   return value is expr.
 */
PMATH_API 
PMATH_ATTRIBUTE_USE_RESULT
pmath_expr_t pmath_expr_set_item(
  pmath_expr_t expr,
  size_t       index,
  pmath_t      item);

/**\brief Remove all occurencies of an object from an expression.
   \memberof pmath_expr_t
   \param expr A pMath expression. It will be destroyed, do not use it after the 
          call.
   \param rem The object to be removed. It will *not* be destroyed.
   \return PMATH_NULL or a new expression that contains no occurencies of \a rem 
           (except maybe the head). It is a shrinked version of \a expr.
 */
PMATH_API 
PMATH_ATTRIBUTE_USE_RESULT
pmath_expr_t pmath_expr_remove_all(
  pmath_expr_t expr,
  pmath_t      rem);

/**\brief Sort an expression.
   \memberof pmath_expr_t
   \param expr A pMath expression. It will be destroyed, do not use it after the 
          call.
   \return A new expression where all items from expr are sorted (except the 
           head, which remains unchanged).
 */
PMATH_API 
PMATH_ATTRIBUTE_USE_RESULT
pmath_expr_t pmath_expr_sort(pmath_expr_t  expr);

/**\brief Flatten an expression.
   \memberof pmath_expr_t
   \param expr A pMath expression. It will be destroyed, do not use it after the 
          call.
   \param head The head of items, that should be flattened out. It will be 
          destroyed, so you can use `pmath_expr_get_item(expr)` directly
          to use expr's head.
   \param depth The depth to which level flattening should be done. A value of 0
          means `no flattening`. 
   \return A new expression where all items, that have the same head as expr,
           will be flattened.
 */
PMATH_API 
PMATH_ATTRIBUTE_USE_RESULT
pmath_expr_t pmath_expr_flatten(
  pmath_expr_t  expr,
  pmath_t       head,
  size_t        depth);

/** @} */

#endif /* __PMATH_CORE__EXPRESSIONS_H__ */
