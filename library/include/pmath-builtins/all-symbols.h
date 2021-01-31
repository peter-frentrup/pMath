#ifndef __PMATH_BUILTINS__SYMBOLS_H__
#define __PMATH_BUILTINS__SYMBOLS_H__

#include <pmath-core/expressions.h>
#include <pmath-core/symbols.h>

/**\addtogroup symbols

   @{
 */

/**\brief Prototype of a built-in function.
   \param expr An expression. The function must free it.
   \return Any pMath object.
 */
typedef pmath_t (*pmath_builtin_func_t)(pmath_expr_t expr);

/**\brief Constants determining when to call a binded function.
   Let f be the pMath symbol.

   - PMATH_CODE_USAGE_DOWNCALL Call binded function when evaluating `f(...)`

   - PMATH_CODE_USAGE_SUBCALL Call binded function when evaluating
     `f(...)(...)...`

   - PMATH_CODE_USAGE_UPCALL Call binded function when evaluating `g(...,f,...)`
     or `g(..., f(...), ...)`

   These use-cases correspond to the DownRules(), SubRules() and UpRules() of a
   symbol.

   \see pmath_register_code
  */
typedef enum {
  PMATH_CODE_USAGE_DOWNCALL   = 0,
  PMATH_CODE_USAGE_SUBCALL    = 1,
  PMATH_CODE_USAGE_UPCALL     = 2
} pmath_code_usage_t;

/**\brief Bind a pMath symbol to some native code
   \param symbol The symbol. It wont be freed.
   \param func A native function.
   \param usage When to call the native function.
   \return Whether the binding succeded.

   Any previous binding to the symbol with the same \a usage will be overwritten.
  */
PMATH_API
pmath_bool_t pmath_register_code(
  pmath_symbol_t         symbol,
  pmath_builtin_func_t   func,
  pmath_code_usage_t     usage);

/**\brief Bind a pMath symbol to some native code for approximation.
   \param symbol The symbol. It wont be freed.
   \param func A native function that replaces its first argument by an
               approximation (precision according to its second argument) and
               returns TRUE on success
   \return Whether the binding succeded.

   Any previous approximation-binding to the symbol will be overwritten.

   The native function will be called as <tt>func(obj, precision)</tt>
   when evaluating the pMath expression <tt>N(obj, precision)</tt>,
   where obj is either `symbol` or `symbol(...)`.
 */
PMATH_API
pmath_bool_t pmath_register_approx_code(
  pmath_symbol_t   symbol,
  pmath_bool_t   (*func)(pmath_t *, double));

/** @} */


#endif /* __PMATH_BUILTINS__SYMBOLS_H__ */
