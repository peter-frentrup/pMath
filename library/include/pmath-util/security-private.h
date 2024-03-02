#ifndef PMATH_UTIL__SECURITY_PRIVATE_H__INCLUDED
#define PMATH_UTIL__SECURITY_PRIVATE_H__INCLUDED


#ifndef BUILDING_PMATH
#  error This header file is not part of the public pMath API
#endif


#include <pmath-util/security.h>


PMATH_PRIVATE
pmath_bool_t _pmath_security_check_builtin(
  void                   *func,           // A pmath_builtin_func_t or pmath_approx_func_t 
  pmath_expr_t            expr, 
  pmath_symbol_t          src_sym,        // The symbol that defines `func`. Wont be freed.
  pmath_symbol_t          kind,           // How `func` is bound to `src_sym`, e.g. pmath_System_DownRules. Wont be freed.
  pmath_security_level_t  current_level); // must be pmath_thread_get_current()->security_level


PMATH_PRIVATE
pmath_t _pmath_security_level_to_expr(pmath_security_level_t level);


// expr wont be freed
PMATH_PRIVATE
pmath_bool_t _pmath_security_level_from_expr(pmath_security_level_t *level, pmath_expr_t expr);

PMATH_PRIVATE
pmath_bool_t _pmath_security_init(void);

PMATH_PRIVATE
void _pmath_security_done();


#endif // PMATH_UTIL__SECURITY_PRIVATE_H__INCLUDED
