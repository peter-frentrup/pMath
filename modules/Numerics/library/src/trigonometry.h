#ifndef __MODULE__PMATH_NUMERICS__TRIGONOMETRY_H__
#define __MODULE__PMATH_NUMERICS__TRIGONOMETRY_H__

#ifndef __MODULE__PMATH_NUMERICS__STDAFX_H__
#  error include "stdafx.h" first
#endif

/** \brief Evaluate a function by converting degrees to radians.
    \param expr  Pointer to the function expression. On success, this will be replaced by the evaluation result.
    \param x     The function argument. It won't be freed.
    \return Whether the evaluation succeeded. If TRUE is returned, \a expr will hold the result, otherwise it
            remains unchanged.
 */
PMATH_PRIVATE
pmath_bool_t pnum_try_simplify_degree(pmath_t *expr, pmath_t x);

#endif // __MODULE__PMATH_NUMERICS__TRIGONOMETRY_H__
