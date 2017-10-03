#ifndef __MODULE__PMATH_NUMERICS__TRIG_OF_TRIGINVERSE_H__
#define __MODULE__PMATH_NUMERICS__TRIG_OF_TRIGINVERSE_H__

/** \brief Evaluate Cos(ArcCos(u)), Cos(ArcTan(u)), etc.
    \param expr  Pointer to the Cos-expression. On success, this will be replaced by the evaluation result.
    \param x     The cosine argument. It won't be freed.
    \return Whether the evaluation succeeded. If TRUE is returned, \a expr will hold the result, otherwise it
            remains unchanged.
 */
PMATH_PRIVATE pmath_bool_t try_cos_of_triginverse(pmath_t *expr, pmath_t x);

/** \brief Evaluate Sin(ArcCos(u)), Sin(ArcTan(u)), etc.
    \param expr  Pointer to the Sin-expression. On success, this will be replaced by the evaluation result.
    \param x     The sine argument. It won't be freed.
    \return Whether the evaluation succeeded. If TRUE is returned, \a expr will hold the result, otherwise it
            remains unchanged.
 */
PMATH_PRIVATE pmath_bool_t try_sin_of_triginverse(pmath_t *expr, pmath_t x);

/** \brief Evaluate Tan(ArcCos(u)), Tan(ArcTan(u)), etc.
    \param expr  Pointer to the Tan-expression. On success, this will be replaced by the evaluation result.
    \param x     The tangent argument. It won't be freed.
    \return Whether the evaluation succeeded. If TRUE is returned, \a expr will hold the result, otherwise it
            remains unchanged.
 */
PMATH_PRIVATE pmath_bool_t try_tan_of_triginverse(pmath_t *expr, pmath_t x);

#endif // __MODULE__PMATH_NUMERICS__TRIG_OF_TRIGINVERSE_H__
