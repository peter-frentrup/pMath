#include <pmath-core/numbers-private.h>

#include <pmath-language/patterns-private.h>

#include <pmath-util/concurrency/threads.h>
#include <pmath-util/emit-and-gather.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>
#include <pmath-util/option-helpers.h>

#include <pmath-builtins/all-symbols-private.h>


extern pmath_symbol_t pmath_System_All;
extern pmath_symbol_t pmath_System_FixedPointList;
extern pmath_symbol_t pmath_System_Function;
extern pmath_symbol_t pmath_System_List;
extern pmath_symbol_t pmath_System_NestWhileList;
extern pmath_symbol_t pmath_System_Not;
extern pmath_symbol_t pmath_System_PureArgument;
extern pmath_symbol_t pmath_System_Range;
extern pmath_symbol_t pmath_System_SameTest;
extern pmath_symbol_t pmath_System_True;

static pmath_t nestwhile(
  pmath_t func,       // wont be freed
  pmath_t obj,        // will be freed
  pmath_t test,       // wont be freed
  intptr_t argnummin,
  intptr_t argnum,
  intptr_t maxiter,
  intptr_t extra,
  pmath_bool_t emit_results
) {
  intptr_t iter, memlen, mempos, i;
  pmath_expr_t mem;
  
  if(maxiter < 0 || argnum < argnummin || argnum < 0)
    return obj;
    
  if(argnum < INTPTR_MAX) {
    memlen = argnum;
    if(memlen < -extra)
      memlen = -extra;
      
    mem = pmath_expr_new(PMATH_NULL, memlen - 1);
  }
  else {
    memlen = INTPTR_MAX;
    mem = pmath_expr_new(PMATH_NULL, 0);
  }
  
  if(emit_results)
    pmath_emit(pmath_ref(obj), PMATH_NULL);
    
  mempos = 0;
  mem = pmath_expr_set_item(mem, mempos, pmath_ref(obj));
  
  iter = 1;
  while(iter < argnummin) {
    obj = pmath_evaluate(pmath_expr_new_extended(pmath_ref(func), 1, obj));
    
    if(pmath_aborting()) {
      pmath_unref(mem);
      return obj;
    }
    
    if(emit_results)
      pmath_emit(pmath_ref(obj), PMATH_NULL);
      
    ++iter;
    ++mempos;
    if(memlen < INTPTR_MAX)
      mem = pmath_expr_set_item(mem, mempos, pmath_ref(obj));
    else
      mem = pmath_expr_append(mem, 1, pmath_ref(obj));
  }
  
  while(iter <= maxiter) {
    pmath_t tmp;
    size_t tmplen = (size_t)(iter < argnum ? iter : argnum);
    
    if(memlen < INTPTR_MAX) {
      tmp = pmath_expr_new(pmath_ref(test), tmplen);
      for(i = tmplen; i > 0; --i)
        tmp = pmath_expr_set_item(tmp, i,
                                  pmath_expr_get_item(mem, (mempos + 1 - i) % memlen));
    }
    else {
      tmp = pmath_expr_get_item_range(
              mem,
              mempos - tmplen + 1,
              tmplen);
      tmp = pmath_expr_set_item(tmp, 0, pmath_ref(test));
    }
    
    tmp = pmath_evaluate(tmp);
    pmath_unref(tmp);
    
    if(pmath_aborting()) {
      pmath_unref(mem);
      return obj;
    }
    
    if(!pmath_same(tmp, pmath_System_True))
      break;
      
    obj = pmath_evaluate(pmath_expr_new_extended(pmath_ref(func), 1, obj));
    
    if(emit_results)
      pmath_emit(pmath_ref(obj), PMATH_NULL);
      
    ++iter;
    ++mempos;
    if(memlen < INTPTR_MAX)
      mem = pmath_expr_set_item(mem, mempos % memlen, pmath_ref(obj));
    else
      mem = pmath_expr_append(mem, 1, pmath_ref(obj));
  }
  
  if(extra < 0) {
    pmath_unref(obj);
    obj = pmath_expr_get_item(mem, (mempos + extra) % memlen);
    pmath_unref(mem);
    return obj;
  }
  
  pmath_unref(mem);
  while(extra > 0 && !pmath_aborting()) {
    obj = pmath_evaluate(pmath_expr_new_extended(pmath_ref(func), 1, obj));
    
    if(emit_results)
      pmath_emit(pmath_ref(obj), PMATH_NULL);
  }
  
  return obj;
}


PMATH_PRIVATE pmath_t builtin_nestwhile_and_nestwhilelist(pmath_expr_t expr) {
  /* NestWhile(f, x, test, mmin..m, max, n)
     NextWhile(f, x, test, m, max, n)  = NextWhile(f, x, test, m..m, max,      n)
     NextWhile(f, x, test, ms, max)    = NextWhile(f, x, test, ms,   max,      0)
     NextWhile(f, x, test, ms)         = NextWhile(f, x, test, ms,   Infinity, 0)
     NextWhile(f, x, test)             = NextWhile(f, x, test, 1,    Infinity, 0)
  
     NestWhileList(...)
   */
  pmath_t obj, func, test;
  intptr_t m, mmin, max, n;
  pmath_bool_t generate_list = pmath_is_expr_of(expr, pmath_System_NestWhileList);
  
  if(pmath_expr_length(expr) < 3 || pmath_expr_length(expr) > 6) {
    pmath_message_argxxx(pmath_expr_length(expr), 3, 6);
    return expr;
  }
  
  
  mmin = 1;
  m = 1;
  max = INTPTR_MAX;
  n = 0;
  
  if(pmath_expr_length(expr) >= 4) {
    obj = pmath_expr_get_item(expr, 4);
    
    if(pmath_is_int32(obj) && PMATH_AS_INT32(obj) > 0) {
      m = mmin = PMATH_AS_INT32(obj);
    }
    else if(pmath_same(obj, pmath_System_All)) {
      m = INTPTR_MAX;
    }
    else if(pmath_is_expr_of_len(obj, pmath_System_Range, 2)) {
      pmath_t tmp = pmath_expr_get_item(obj, 1);
      
      if(pmath_is_int32(tmp) && PMATH_AS_INT32(tmp) >= 0) {
        mmin = PMATH_AS_INT32(tmp);
      }
      else {
        // TODO: appropriate message
        pmath_unref(tmp);
        pmath_unref(obj);
        pmath_message(PMATH_NULL, "nwargs", 2, PMATH_FROM_INT32(4), pmath_ref(expr));
        return expr;
      }
      
      pmath_unref(tmp);
      tmp = pmath_expr_get_item(obj, 2);
      
      if(pmath_is_int32(tmp) && PMATH_AS_INT32(tmp) >= 0) {
        m = PMATH_AS_INT32(tmp);
      }
      else if(!pmath_equals(tmp, _pmath_object_pos_infinity)) {
        // TODO: appropriate message
        pmath_unref(tmp);
        pmath_unref(obj);
        pmath_message(PMATH_NULL, "nwargs", 2, PMATH_FROM_INT32(4), pmath_ref(expr));
        return expr;
      }
      
      pmath_unref(tmp);
    }
    else {
      // TODO: appropriate message
      pmath_unref(obj);
      pmath_message(PMATH_NULL, "nwargs", 2, PMATH_FROM_INT32(4), pmath_ref(expr));
      return expr;
    }
    
    pmath_unref(obj);
  }
  
  if(pmath_expr_length(expr) >= 5) {
    obj = pmath_expr_get_item(expr, 5);
    
    if(pmath_is_int32(obj) && PMATH_AS_INT32(obj) > 0) {
      mmin = m = PMATH_AS_INT32(obj);
    }
    else if(!pmath_equals(obj, _pmath_object_pos_infinity)) {
      // TODO: appropriate message
      pmath_unref(obj);
      pmath_message(PMATH_NULL, "intnm", 2, PMATH_FROM_INT32(5), pmath_ref(expr));
      return expr;
    }
    
    pmath_unref(obj);
  }
  
  if(pmath_expr_length(expr) >= 6) {
    obj = pmath_expr_get_item(expr, 6);
    
    if(pmath_is_int32(obj)) {
      n = PMATH_AS_INT32(obj);
    }
    else {
      pmath_unref(obj);
      pmath_message(PMATH_NULL, "intm", 2, PMATH_FROM_INT32(6), pmath_ref(expr));
      return expr;
    }
    
    pmath_unref(obj);
  }
  
  func = pmath_expr_get_item(expr, 1);
  obj  = pmath_expr_get_item(expr, 2);
  test = pmath_expr_get_item(expr, 3);
  pmath_unref(expr);
  
  if(generate_list)
    pmath_gather_begin(PMATH_NULL);
    
  obj = nestwhile(func, obj, test, mmin, m, max, n, generate_list);
  
  pmath_unref(func);
  pmath_unref(test);
  
  if(generate_list) {
    pmath_unref(obj);
    obj = pmath_gather_end();
    if(n < 0) {
      if(pmath_expr_length(obj) <= (size_t) - n) {
        obj = pmath_expr_resize(obj, pmath_expr_length(obj) + n);
      }
      else {
        pmath_unref(obj);
        obj = pmath_expr_new(pmath_ref(pmath_System_List), 0);
      }
    }
  }
  
  return obj;
}


PMATH_PRIVATE pmath_t builtin_fixedpoint_and_fixedpointlist(pmath_expr_t expr) {
  /* FixedPoint(f, x, n) = NestWhile(f, expr, !Identical(##)&, 2, n)
     FixedPoint(f, x) = FixedPoint(f, x, Infinity)
  
     FixedPointList(...)
  
     Options:
       SameTest->Identical
   */
  pmath_expr_t options;
  pmath_t f, x, test;
  size_t last_nonoption = 2;
  intptr_t max = INTPTR_MAX;
  pmath_bool_t generate_list = pmath_is_expr_of(expr, pmath_System_FixedPointList);
  
  if(pmath_expr_length(expr) < 2) {
    pmath_message_argxxx(pmath_expr_length(expr), 2, 3);
    return expr;
  }
  
  if(pmath_expr_length(expr) >= 3) {
    x = pmath_expr_get_item(expr, 3);
    
    if(pmath_is_int32(x) && PMATH_AS_INT32(x) >= 0) {
      max = PMATH_AS_INT32(x);
      last_nonoption = 3;
    }
    else if(pmath_equals(x, _pmath_object_pos_infinity)) {
      last_nonoption = 3;
    }
    
    pmath_unref(x);
  }
  
  
  options = pmath_options_extract(expr, last_nonoption);
  if(pmath_is_null(options))
    return expr;
    
  test = pmath_evaluate(pmath_option_value(PMATH_NULL, pmath_System_SameTest, options));
  pmath_unref(options);
  
  test = pmath_expr_new_extended(
           pmath_ref(pmath_System_Function), 1,
           pmath_expr_new_extended(
             pmath_ref(pmath_System_Not), 1,
             pmath_expr_new_extended(
               test, 1,
               pmath_expr_new_extended(
                 pmath_ref(pmath_System_PureArgument), 1,
                 pmath_ref(_pmath_object_range_from_one)))));
                 
  f = pmath_expr_get_item(expr, 1);
  x = pmath_expr_get_item(expr, 2);
  pmath_unref(expr);
  
  if(generate_list)
    pmath_gather_begin(PMATH_NULL);
    
  x = nestwhile(f, x, test, 2, 2, max, 0, generate_list);
  
  pmath_unref(f);
  pmath_unref(test);
  
  if(generate_list) {
    pmath_unref(x);
    return pmath_gather_end();
  }
  
  return x;
}
