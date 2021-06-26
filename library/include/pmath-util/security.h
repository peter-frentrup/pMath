#ifndef PMATH__UTIL__SECURITY_H__INCLUDED
#define PMATH__UTIL__SECURITY_H__INCLUDED


#include <pmath-core/objects-inline.h>
#include <pmath-builtins/all-symbols.h>

/**\defgroup security Secure evaluation
   \brief The security mechanism in pMath.
   
   The pMath security mechanism is based on white-listing built-in C functions.
   By default, all functions are allowed, but this can be restricted.
   
   @{
 */

/**\brief The available security levels.

    The default is \c PMATH_SECURITY_LEVEL_EVERYTHING_ALLOWED
 */
typedef enum {
  /**\brief No C code evaluations are allowed.
   */
  PMATH_SECURITY_LEVEL_NOTHING_ALLOWED            =     0,
  
  /**\brief Only deterministic function without any side-effects are allowed.
     
     In particular, file access, assignments RandomReal/... and timing functions are disallowed. 
   */
  PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED =   100,
  
  /**\brief Only deterministic function without any side-effects are allowed.
     
     Disallow assignments to global variables (TODO: but possibly assigning DynamicLocal and 
     definitely Local variables). Disallow all file-system access. Randomness and timing is allowed.
   */
  PMATH_SECURITY_LEVEL_NON_DESTRUCTIVE_ALLOWED    =   200,
  
  /**\brief No C code evaluations are allowed.
     
      Allow everything.
   */
  PMATH_SECURITY_LEVEL_EVERYTHING_ALLOWED         = 10000
} pmath_security_level_t;

/**\brief Test if a sequirity requirement is fullfilled by a (current) security level.
 */
#define PMATH_SECURITY_REQUIREMENT_MATCHES_LEVEL(REQUIREMENT, LEVEL)   ((REQUIREMENT) <= (LEVEL))

/**\brief A callback function to perform fine-grained security checks.
   \param func      The function to check (before it is callded).
   \param expr      The argument that will be given to \a func. This wont be freed.
   \param min_level The current security level.
   \return TRUE if it is safe to call \a func with argument \a expr and FALSE otherwise.
   
   This function should call and return \see pmath_security_check() with the required level
   and \a expr as second argument. In the case that 
   `PMATH_SECURITY_REQUIREMENT_MATCHES_LEVEL(required_level, min_level)` is TRUE, no call
   to pmath_security_check() is necessary and TRUE may be returned directly.
 */
typedef pmath_bool_t (*pmath_security_doorman_func_t)(
  void                   *func,
  pmath_expr_t            expr, 
  pmath_security_level_t  min_level);


/**\brief Check that a required security level is available.
   \param required_level The required level.
   \param message_arg    An additional argument for possible error messages/exceptions. It wont be freed.
   \return TRUE if required_level matches (<=) the current security level, and FALSE otherwise.
   
   When the \a required_level is too high, a `SecurityException(...)` will be thrown.
 */
PMATH_API
pmath_bool_t pmath_security_check(pmath_security_level_t required_level, pmath_t message_arg);


/**\brief Evaluate an expression in a security-restricted environment.
   \param expr The expression to evaluate. It will be freed.
   \param max_allowed_level The maximum allowed security level.
   \return The evaluation result.
   
    If \a max_allowed_level is larger than the current security level, the current level will remain intact,
    i.e. yiu cannot use this function to (temporarily) extent the security level, only restrict it.
    
    Uppon breach of security during evaluation, a `SecurityException(...)` is thrown.
    
    Even when no exception was thrown, you should wrap the result of this function in HoldComplete,
    because evaluating it further may breach security (e.g. if \a expr is of the form
    <i>if current security level is low, remain unevaluated, else run bad code</i>).
 */
PMATH_API
pmath_t pmath_evaluate_secured(pmath_t expr, pmath_security_level_t max_allowed_level);


/**\brief Register a security checker for a built-in C function.
   \param func      A function rgistered with pmath_register_code() or pmath_register_approx_code().
   \param min_level The required secuirity level to allow evaluating \a func.
   \param certifier An optional callback that checks whether \a func can be called without breaking security.
   \return Whether the registration succeeded.
   
   When the \a min_level requirement is \em not fullfilled, \a certifier will not even be called and 
   execution of \a func will be denied directly.
 */
PMATH_API
pmath_bool_t pmath_security_register_doorman(
  void                          *func, 
  pmath_security_level_t         min_level, 
  pmath_security_doorman_func_t  certifier);


/** @} */


#endif // PMATH__UTIL__SECURITY_H__INCLUDED
