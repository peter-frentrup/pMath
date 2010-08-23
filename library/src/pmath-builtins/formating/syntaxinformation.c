#include <pmath-util/evaluation.h>
#include <pmath-core/numbers.h>
#include <pmath-core/symbols.h>
#include <pmath-util/memory.h>

#include <assert.h>
#include <string.h>

#include <pmath-core/custom.h>

#include <pmath-util/debug.h>
#include <pmath-util/hashtables-private.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-core/objects-private.h>
#include <pmath-core/expressions-private.h>

#include <pmath-builtins/all-symbols.h>
#include <pmath-builtins/all-symbols-private.h>
#include <pmath-util/concurrency/atomic-private.h>
#include <pmath-builtins/control/definitions-private.h>
#include <pmath-builtins/formating-private.h>

PMATH_PRIVATE pmath_t builtin_assign_syntaxinformation(pmath_expr_t expr){
  pmath_t tag;
  pmath_t lhs;
  pmath_t rhs;
  pmath_t sym;
  int            assignment;
  
  assignment = _pmath_is_assignment(expr, &tag, &lhs, &rhs);
  
  if(!assignment)
    return expr;
  
  if(!pmath_is_expr_of_len(lhs, PMATH_SYMBOL_SYNTAXINFORMATION, 1)
  || tag != PMATH_UNDEFINED){
    pmath_unref(tag);
    pmath_unref(lhs);
    pmath_unref(rhs);
    return expr;
  }
  
  sym = pmath_expr_get_item(lhs, 1);
  
  if(!pmath_instance_of(sym, PMATH_TYPE_SYMBOL)){
    pmath_unref(tag);
    pmath_unref(lhs);
    pmath_unref(rhs);
    return expr;
  }
  
  if(assignment < 0)
    tag = pmath_ref(PMATH_SYMBOL_TAGASSIGNDELAYED);
  else
    tag = pmath_ref(PMATH_SYMBOL_TAGASSIGN);
  
  pmath_unref(expr);
  return pmath_expr_new_extended(
    tag, 3,
    sym,
    lhs,
    rhs);
}

PMATH_PRIVATE pmath_t builtin_syntaxinformation(pmath_expr_t expr){
  pmath_t sym;
  
  if(pmath_expr_length(expr) != 1){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  sym = pmath_expr_get_item(expr, 1);
  if(pmath_instance_of(sym, PMATH_TYPE_STRING)){
    sym = pmath_symbol_find(sym, FALSE);
    
    if(!sym){
      pmath_unref(expr);
      return pmath_ref(_pmath_object_emptylist);
    }
    
    expr = pmath_expr_set_item(expr, 1, sym);
  }
  else
    pmath_unref(sym);
  
  expr = pmath_expr_set_item(expr, 0, 
    pmath_ref(PMATH_SYMBOL_UTILITIES_GETSYSTEMSYNTAXINFORMATION));
  
  expr = pmath_evaluate(expr);
  if(expr == PMATH_SYMBOL_FAILED
  || pmath_is_expr_of(expr, PMATH_SYMBOL_UTILITIES_GETSYSTEMSYNTAXINFORMATION)){
    pmath_unref(expr);
    return pmath_ref(_pmath_object_emptylist);
  }
  
  return expr;
}
