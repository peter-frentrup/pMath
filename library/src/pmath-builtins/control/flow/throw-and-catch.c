#include <pmath-util/evaluation.h>
#include <pmath-core/symbols.h>

#include <assert.h>
#include <string.h>
#include <time.h>

#include <pmath-util/hashtables-private.h>
#include <pmath-util/messages.h>

#include <pmath-util/concurrency/threadlocks.h>
#include <pmath-util/concurrency/threads.h>
#include <pmath-util/concurrency/threads-private.h>

#include <pmath-builtins/control-private.h>
#include <pmath-builtins/all-symbols.h>
#include <pmath-builtins/all-symbols-private.h>

#include <pmath-language/patterns-private.h>

PMATH_PRIVATE pmath_t builtin_throw(pmath_expr_t expr){
  if(pmath_expr_length(expr) != 1){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }

  pmath_throw(pmath_expr_get_item(expr, 1));
  pmath_unref(expr);
  return NULL;
}

PMATH_PRIVATE pmath_t builtin_catch(pmath_expr_t expr){
/* Catch(expr, pattern)  = Catch(expr, pattern)
   Catch(expr)           = Catch(expr, ~)
 */
  pmath_t result, exception;
  size_t len = pmath_expr_length(expr);

  if(len < 1 || len > 2){
    pmath_message_argxxx(len, 1, 2);
    return expr;
  }

  result = pmath_evaluate(pmath_expr_get_item(expr, 1));

  exception = pmath_catch();
  if(exception != PMATH_UNDEFINED){
    pmath_t rhs, pattern;

    if(len == 1){
      pmath_unref(result);
      pmath_unref(expr);
      return exception;
    }

    rhs = NULL;
    pattern = pmath_expr_get_item(expr, 2);
    if(_pmath_pattern_match(exception, pattern, &rhs)){
      pmath_unref(result);

      pmath_unref(expr);
      return exception;
    }

    pmath_throw(exception);
  }
  pmath_unref(expr);
  return result;
}
