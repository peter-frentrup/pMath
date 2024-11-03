#ifndef PMATH_UTIL__ASSOCIATION_LISTS_PRIVATE_H__INCLUDED
#define PMATH_UTIL__ASSOCIATION_LISTS_PRIVATE_H__INCLUDED

#ifndef BUILDING_PMATH
#  error This header file is not part of the public pMath API
#endif

#include <pmath-util/association-lists.h>

#include <pmath-core/expressions-private.h>


/**\addtogroup association_lists Association lists
  @{
 */

typedef struct _pmath_custom_expr_t  _pmath_association_list_t;

/** \brief Create an association list from its list of keys and list of values.
    \param keys    A list keys expression <tt>{k1, k2, ..., kN}</tt>. Often a \ref pmath_dispatch_table_t. It will be freed.
    \param values  A list of values expression <tt>{v1, v2, ..., vN}</tt> of the same length \a N as \a keys. It will be freed.
    \return A new association list <tt>{k1 -> v1, k2 -> v2, ... kN -> vN}</tt> or PMATH_NULL on error.
    This is essentially <tt>Thread(keys -> values)</tt>.
 */
PMATH_PRIVATE pmath_association_list_t _pmath_create_association_list(pmath_t keys, pmath_t values);


PMATH_PRIVATE pmath_bool_t _pmath_association_list_try_replace_keys_in_place(pmath_association_list_t assoc, pmath_t keys); // keys will be freed


PMATH_PRIVATE pmath_bool_t _pmath_association_lists_init(void);
PMATH_PRIVATE void _pmath_association_lists_done(void);

/** @} */

#endif // PMATH_UTIL__ASSOCIATION_LISTS_PRIVATE_H__INCLUDED
