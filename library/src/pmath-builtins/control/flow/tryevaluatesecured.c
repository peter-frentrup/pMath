#include <pmath-util/evaluation.h>
#include <pmath-util/messages.h>
#include <pmath-util/security-private.h>

#include <pmath-builtins/all-symbols-private.h>


extern pmath_symbol_t pmath_System_HoldComplete;

PMATH_PRIVATE pmath_t builtin_internal_tryevaluatesecured(pmath_expr_t expr) {
// Internal`TryEvaluateSecured(expr, level)
//
// Examples:
//  Preparing the tests:
//  pmath> x:= 10; SeedRandom(1234);
//
//  pmath> Internal`TryEvaluateSecured(123, "NothingAllowed")
//         HoldComplete(123)
//  pmath> Internal`TryEvaluateSecured(x, "NothingAllowed")
//         HoldComplete(10)
//  pmath> Internal`TryEvaluateSecured(y, "NothingAllowed")
//         HoldComplete(y)
//  pmath> Internal`TryEvaluateSecured(1 + 1, "NothingAllowed")
//   ERR: Throw::nocatch: 
//         Uncaught exception SecurityException(PureDeterministicAllowed, 
//             HoldComplete(Plus, DownRules, 1 + 1)). [1]
//         $Aborted
//
//  pmath> Internal`TryEvaluateSecured(1 + 1, "PureDeterministicAllowed")
//         HoldComplete(2)
//  pmath> Internal`TryEvaluateSecured(x * 3, "PureDeterministicAllowed")
//         HoldComplete(30)
//  pmath> Internal`TryEvaluateSecured(RandomReal(), "PureDeterministicAllowed")
//   ERR: Throw::nocatch: 
//         Uncaught exception SecurityException(NonDestructiveAllowed, 
//             HoldComplete(RandomReal, DownRules, RandomReal())). [2]
//         $Aborted
//
//  pmath> Internal`TryEvaluateSecured(RandomReal(), "NonDestructiveAllowed")
//         HoldComplete(0.9708211914308248`)
//  pmath> Internal`TryEvaluateSecured(x:= 55, "NonDestructiveAllowed")
//   ERR: Throw::nocatch: 
//         Uncaught exception SecurityException(EverythingAllowed, 
//             HoldComplete(Assign, DownRules, x:= 55)). [3]
//         $Aborted
//  pmath> x
//         10
//
//  pmath> Internal`TryEvaluateSecured(x:= 55, "EverythingAllowed")
//         HoldComplete(55)
//  pmath> x
//         55
//
  pmath_t obj;
  pmath_security_level_t level;
  
  if(pmath_expr_length(expr) != 2) {
    pmath_message_argxxx(pmath_expr_length(expr), 2, 2);
    return expr;
  }
  
  obj = pmath_evaluate(pmath_expr_get_item(expr, 2));
  if(!_pmath_security_level_from_expr(&level, obj)) {
    pmath_message(PMATH_NULL, "seclvl", 1, obj);
    return expr;
  }
  pmath_unref(obj);
  
  obj = pmath_expr_extract_item(expr, 1);
  pmath_unref(expr);
  
  obj = pmath_evaluate_secured(obj, level);
  
  obj = pmath_expr_new_extended(pmath_ref(pmath_System_HoldComplete), 1, obj);
  return obj;
}
