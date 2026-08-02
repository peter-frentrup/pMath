#include <pmath-builtins/arithmetic-private.h>

#include <pmath-core/numbers.h>

#include <pmath-util/emit-and-gather.h>
#include <pmath-util/messages.h>

#include <fenv.h>
#include <float.h>

#pragma fenv_access (on)


extern pmath_symbol_t pmath_System_DollarFailed;
extern pmath_symbol_t pmath_System_List;
extern pmath_symbol_t pmath_System_MachinePrecision;
extern pmath_symbol_t pmath_System_Rule;

#define RULE(NAME, VALUE)     pmath_expr_new_extended(pmath_ref(pmath_System_Rule), 2, PMATH_C_STRING(NAME), VALUE)


static double calculate_machine_epsilon() {
  // Based on exactinit() from "Routines for Arbitrary Precision Floating-point Arithmetic and Fast Robust Geometric Predicates (predicates.c)"
  // by Jonathan Richard Shewchuk.
  // But gives twice the value of epsilon there (his 'epsilon' is the largest value s.t. 1.0 + epsilon = 1.0)
  
  double next_epsilon = 1.0;
  double epsilon;
  double half = 0.5;
  
  double check = 1.0;
  double last_check;
  do{
    epsilon = next_epsilon;
    last_check = check;
    
    next_epsilon *= half;
    check = 1.0 + next_epsilon;
  } while((check != 1.0) && (check != last_check));
  
  return epsilon;
}


PMATH_PRIVATE pmath_t builtin_internal_getmachinerealsettings(pmath_expr_t expr) {
// Internal`GetMachineRealSettings()
//
  if(pmath_expr_length(expr) != 0) {
    pmath_message_argxxx(pmath_expr_length(expr), 0, 0);
    return expr;
  }
  
  pmath_unref(expr);
  
//  fenv_t env;
//  if(fegetenv(&env)) {
//    return pmath_ref(pmath_System_DollarFailed);
//  }
  
  pmath_gather_begin(PMATH_NULL);
  
  { // "RoundingMode"
    int rounding_mode = fegetround();
    pmath_t rounding;
    switch(rounding_mode) {
      case FE_DOWNWARD:   rounding = PMATH_C_STRING("Downward"); break;
      case FE_TONEAREST:  rounding = PMATH_C_STRING("ToNearest"); break;
      case FE_TOWARDZERO: rounding = PMATH_C_STRING("TowardZero"); break;
      case FE_UPWARD:     rounding = PMATH_C_STRING("Upward"); break;
      default:            rounding = pmath_ref(pmath_System_DollarFailed); break;
    }
    pmath_emit(RULE("RoundingMode", rounding), PMATH_NULL);
  }
  
  { // "ActiveExceptions"
    int exc_flags = fetestexcept(FE_ALL_EXCEPT);
    pmath_gather_begin(PMATH_NULL);
    
    if(exc_flags & FE_DIVBYZERO) pmath_emit(PMATH_C_STRING("DivideByZero"), PMATH_NULL);
    if(exc_flags & FE_INEXACT)   pmath_emit(PMATH_C_STRING("Inexact"), PMATH_NULL);
    if(exc_flags & FE_INVALID)   pmath_emit(PMATH_C_STRING("Invalid"), PMATH_NULL);
    if(exc_flags & FE_OVERFLOW)  pmath_emit(PMATH_C_STRING("Overflow"), PMATH_NULL);
    if(exc_flags & FE_UNDERFLOW) pmath_emit(PMATH_C_STRING("Underflow"), PMATH_NULL);
    
    pmath_t active_exceptions = pmath_gather_end();
    pmath_emit(RULE("ActiveExceptions", active_exceptions), PMATH_NULL);
  }
  
  { // "MachineEpsilon"
    double epsilon = calculate_machine_epsilon();
    pmath_emit(RULE("MachineEpsilon", epsilon), PMATH_NULL);
  }
  
#if defined(PMATH_OS_WIN32)
  {
    unsigned fp_cw = _controlfp(0, 0);
    
    pmath_emit(RULE("FPUControlWord", pmath_integer_new_uint(fp_cw)), PMATH_NULL);
  }
#else
  {
    // Not yet implemented. Use glibc fpu_control() or direct inline assembly
  }
#endif
  
  expr = pmath_gather_end();
  
  return expr;
}

