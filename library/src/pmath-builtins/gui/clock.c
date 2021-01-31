#include <pmath-core/numbers.h>

#include <pmath-util/approximate.h>
#include <pmath-util/concurrency/threadmsg.h>
#include <pmath-util/concurrency/threads-private.h>
#include <pmath-util/dynamic-private.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/arithmetic-private.h>
#include <pmath-builtins/build-expr-private.h>

#include <math.h>
#include <limits.h>


extern pmath_symbol_t pmath_System_Floor;
extern pmath_symbol_t pmath_System_Greater;
extern pmath_symbol_t pmath_System_Plus;
extern pmath_symbol_t pmath_System_Power;
extern pmath_symbol_t pmath_System_Range;
extern pmath_symbol_t pmath_System_Times;
extern pmath_symbol_t pmath_System_True;
extern pmath_symbol_t pmath_Internal_DynamicUpdated;

static pmath_bool_t is_positive_infinity(pmath_t obj) {
  pmath_t dir = _pmath_directed_infinity_direction(obj);
  if(pmath_same(dir, PMATH_FROM_INT32(1)))
    return TRUE;
  
  pmath_unref(dir);
  return FALSE;
} 

PMATH_PRIVATE pmath_t builtin_clock(pmath_expr_t expr) {
/* Clock()                  = Clock(   0..1,    1,         Infinity)
   Clock(t)                 = Clock(   0..t,    t,         Infinity)
   Clock(xmin..xmax)        = Clock(xmin..xmax, xmax-xmin, Infinity)
   Clock(xmax, t)           = Clock(   0..xmax, t,         Infinity)
   Clock(xmin..xmax, t)     = Clock(xmin..xmax, t,         Infinity)
   
   Clock(xmin..xmax..dx)    = Clock(xmin..xmax..dx, dx, Infinity)  
   Clock(xmin..xmax..dx, t) = Clock(xmin..xmax..dx, t,  Infinity)
   Clock(values, t, n)
 */
  size_t exprlen = pmath_expr_length(expr);
  pmath_thread_t thread = pmath_thread_get_current();
  pmath_t obj;
  pmath_t duration, min, max, delta;
  double now;
  double duration_seconds;
  unsigned steps;
  pmath_bool_t backwards;
  unsigned max_cycles;

  if(!thread)
    return expr;

  if(thread->current_dynamic_id != 0) {
    now = _pmath_dynamic_first_eval(thread->current_dynamic_id);
    now = -now + pmath_tickcount();
  }
  else
    now = 0;

  if(exprlen == 0) {
    pmath_unref(expr);

    obj = pmath_expr_new_extended(
            pmath_ref(pmath_Internal_DynamicUpdated), 1,
            pmath_integer_new_slong((long)thread->current_dynamic_id));
    pmath_unref(pmath_evaluate(obj));

    return PMATH_FROM_DOUBLE(now - floor(now));
  }

  if(exprlen > 3) {
    pmath_message_argxxx(exprlen, 0, 3);
    return expr;
  }

  max_cycles = UINT_MAX;
  if(exprlen >= 3) {
    obj = pmath_expr_get_item(expr, 3);

    if(pmath_is_int32(obj) && PMATH_AS_INT32(obj) >= 0) {
      max_cycles = (unsigned)PMATH_AS_INT32(obj);
    }
    else if(!is_positive_infinity(obj)) {
      pmath_unref(obj);
      pmath_message(PMATH_NULL, "innf", 2, INT(3), pmath_ref(expr));
      return expr;
    }

    pmath_unref(obj);
  }

  duration = PMATH_UNDEFINED;
  if(exprlen >= 2) {
    duration = pmath_expr_get_item(expr, 2);
  }

  obj = pmath_expr_get_item(expr, 1);
  if(pmath_is_expr_of(obj, pmath_System_Range)) {
    size_t objlen = pmath_expr_length(obj);

    if(objlen == 2) {
      min = pmath_expr_get_item(obj, 1);
      max = pmath_expr_get_item(obj, 2);
      delta = PMATH_FROM_INT32(0);
    }
    else if(objlen == 3) {
      min   = pmath_expr_get_item(obj, 1);
      max   = pmath_expr_get_item(obj, 2);
      delta = pmath_expr_get_item(obj, 3);
    }
    else {
      pmath_unref(duration);
      pmath_message(PMATH_NULL, "vals", 1, obj);
      return expr;
    }

    if( pmath_is_null(min) ||
        pmath_is_null(max) ||
        pmath_is_null(delta))
    {
      pmath_unref(duration);
      pmath_unref(min);
      pmath_unref(max);
      pmath_unref(delta);
      pmath_message(PMATH_NULL, "vals", 1, obj);
      return expr;
    }

    pmath_unref(obj);
  }
  else {
    min   = INT(0);
    max   = obj;
    delta = PMATH_FROM_INT32(0);
  }

  backwards = FALSE;
  obj = pmath_evaluate(GREATER(pmath_ref(min), pmath_ref(max)));
  pmath_unref(obj);
  if(pmath_same(obj, pmath_System_True)) {
    backwards = TRUE;

    obj = min;
    min = max;
    max = obj;
    delta = NEG(delta);
  }

  obj = pmath_set_precision(pmath_ref(delta), -HUGE_VAL);
  if(!pmath_is_number(obj) || pmath_number_sign(obj) < 0) {
    pmath_unref(min);
    pmath_unref(max);
    pmath_unref(delta);
    pmath_unref(obj);
    pmath_unref(duration);
    pmath_message(PMATH_NULL, "length", 1, pmath_ref(expr));
    return expr;
  }

  if(pmath_number_sign(obj) == 0) {
    steps = 0;
    pmath_unref(obj);

    if(is_positive_infinity(max)) {
      steps = UINT_MAX;
      pmath_unref(delta);
      delta = PMATH_FROM_INT32(1);

      if(!pmath_same(duration, PMATH_UNDEFINED)) {
        pmath_unref(min);
        pmath_unref(max);
        pmath_unref(delta);
        pmath_unref(duration);
        pmath_message(PMATH_NULL, "inf", 0);
        return expr;
      }
    }
  }
  else {
    obj = PLUS(INT(1),
               FUNC(pmath_ref(pmath_System_Floor),
                    DIV(MINUS(pmath_ref(max), pmath_ref(min)), obj)));

    obj = pmath_evaluate(obj);

    if(pmath_is_int32(obj) && PMATH_AS_INT32(obj) >= 0) {
      steps = (unsigned)PMATH_AS_INT32(obj);
      pmath_unref(obj);
    }
    else if(is_positive_infinity(obj)) {
      steps = UINT_MAX;
      pmath_unref(obj);

      if(!pmath_same(duration, PMATH_UNDEFINED)) {
        pmath_unref(min);
        pmath_unref(max);
        pmath_unref(delta);
        pmath_unref(duration);
        pmath_message(PMATH_NULL, "inf", 0);
        return expr;
      }
    }
    else {
      pmath_unref(min);
      pmath_unref(max);
      pmath_unref(delta);
      pmath_unref(obj);
      pmath_unref(duration);
      pmath_message(PMATH_NULL, "length", 1, pmath_ref(expr));
      return expr;
    }
  }

  if(steps == UINT_MAX) {
    duration_seconds = HUGE_VAL;
  }
  else {
    if(pmath_same(duration, PMATH_UNDEFINED)) {
      if(steps + 1 > 1) // neither 0 not UINT_MAX
        duration = TIMES(INT(steps), pmath_ref(delta));
      else
        duration = MINUS(pmath_ref(max), pmath_ref(min));
    }

    obj = pmath_set_precision(pmath_ref(duration), -HUGE_VAL);
    if(!pmath_is_number(obj)) {
      pmath_unref(obj);
      pmath_unref(min);
      pmath_unref(max);
      pmath_unref(delta);
      pmath_message(PMATH_NULL, "period", 1, duration);
      return expr;
    }

    duration_seconds = pmath_number_get_d(obj);
    pmath_unref(obj);
  }
  pmath_unref(duration);
  pmath_unref(expr);

  if(backwards) {
    obj = min;
    min = max;
    max = obj;
  }

  if(duration_seconds < HUGE_VAL) {
    now /= duration_seconds;

    if(max_cycles != UINT_MAX && now > max_cycles) {
      pmath_unref(min);
      pmath_unref(delta);
      return max;
    }

    now = fmod(now, 1.0);
  }

  obj = pmath_expr_new_extended(
          pmath_ref(pmath_Internal_DynamicUpdated), 1,
          pmath_integer_new_slong((long)thread->current_dynamic_id));
  pmath_unref(pmath_evaluate(obj));

  if(steps + 1 > 1) { // neither 0 nor UINT_MAX
    steps = (unsigned)(floor(now * steps));

    pmath_unref(max);

    return PLUS(min, TIMES(PMATH_FROM_INT32(steps), delta));
  }

  if(steps < UINT_MAX) {
    pmath_unref(delta);
    delta = MINUS(max, pmath_ref(min));
  }
  else {
    pmath_unref(max);
  }

  return PLUS(min, TIMES(PMATH_FROM_DOUBLE(now), delta));
}
