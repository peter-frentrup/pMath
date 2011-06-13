#include <pmath-util/concurrency/threads.h>
#include <pmath-util/emit-and-gather.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/control/definitions-private.h>


static pmath_t get_definition_and_clear(pmath_symbol_t sym){
  pmath_t lhs, rhs;
  pmath_symbol_attributes_t att = pmath_symbol_get_attributes(sym);
  
  pmath_gather_begin(PMATH_NULL);
  
  lhs = pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_ATTRIBUTES), 1, pmath_ref(sym));
  rhs = pmath_evaluate(pmath_ref(lhs));
  pmath_emit(pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_ASSIGN), 2, lhs, rhs), PMATH_NULL);
  
  pmath_emit(
    pmath_expr_new_extended(
      pmath_ref(PMATH_SYMBOL_UNPROTECT), 1,
      pmath_ref(sym)), 
    PMATH_NULL);
  
  lhs = pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_DEFAULTRULES), 1, pmath_ref(sym));
  rhs = pmath_evaluate(pmath_ref(lhs));
  pmath_emit(pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_ASSIGN), 2, lhs, rhs), PMATH_NULL);
  
  lhs = pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_DOWNRULES), 1, pmath_ref(sym));
  rhs = pmath_evaluate(pmath_ref(lhs));
  pmath_emit(pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_ASSIGN), 2, lhs, rhs), PMATH_NULL);
  
  lhs = pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_FORMATRULES), 1, pmath_ref(sym));
  rhs = pmath_evaluate(pmath_ref(lhs));
  pmath_emit(pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_ASSIGN), 2, lhs, rhs), PMATH_NULL);
  
  lhs = pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_NRULES), 1, pmath_ref(sym));
  rhs = pmath_evaluate(pmath_ref(lhs));
  pmath_emit(pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_ASSIGN), 2, lhs, rhs), PMATH_NULL);
  
  lhs = pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_OWNRULES), 1, pmath_ref(sym));
  rhs = pmath_evaluate(pmath_ref(lhs));
  pmath_emit(pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_ASSIGN), 2, lhs, rhs), PMATH_NULL);
  
  lhs = pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_SUBRULES), 1, pmath_ref(sym));
  rhs = pmath_evaluate(pmath_ref(lhs));
  pmath_emit(pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_ASSIGN), 2, lhs, rhs), PMATH_NULL);
  
  lhs = pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_UPRULES), 1, pmath_ref(sym));
  rhs = pmath_evaluate(pmath_ref(lhs));
  pmath_emit(pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_ASSIGN), 2, lhs, rhs), PMATH_NULL);
  
  if(att & PMATH_SYMBOL_ATTRIBUTE_PROTECTED){
    pmath_emit(
      pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_PROTECT), 1,
        pmath_ref(sym)), 
      PMATH_NULL);
    
    pmath_symbol_set_attributes(sym, att & (~PMATH_SYMBOL_ATTRIBUTE_PROTECTED));
  }
  
  _pmath_clear(sym, TRUE);
  pmath_symbol_set_attributes(sym, att);
  
  return pmath_gather_end();
}


PMATH_PRIVATE pmath_t builtin_block(pmath_expr_t expr){
/* Block({x1:= v1, x2:= v2, x3, ...}, body)
 */
  pmath_t vars, body, oldvals, ex;
  size_t i;
  
  if(pmath_expr_length(expr) != 2){
    pmath_message_argxxx(pmath_expr_length(expr), 2, 2);
    return expr;
  }
  
  vars = pmath_expr_get_item(expr, 1);
  for(i = 1;i <= pmath_expr_length(vars);++i){
    pmath_t def = pmath_expr_get_item(vars, i);
    
    if(pmath_is_symbol(def)){
      pmath_unref(def);
      continue;
    }
    
    if(pmath_is_expr_of_len(def, PMATH_SYMBOL_ASSIGNDELAYED, 2)
    || pmath_is_expr_of_len(def, PMATH_SYMBOL_ASSIGN, 2)){
      pmath_t lhs = pmath_expr_get_item(def, 1);
      
      if(pmath_is_symbol(lhs)){
        pmath_unref(def);
        pmath_unref(lhs);
        continue;
      }
      
    }
      
    pmath_message(PMATH_NULL, "lvsym", 2, vars, def);
    return expr;
  }
  
  body = pmath_expr_get_item(expr, 2);
  pmath_unref(expr);
  
  for(i = 1;i <= pmath_expr_length(vars);++i){
    pmath_t def = pmath_expr_extract_item(vars, i);
    
    if(pmath_is_expr_of_len(def, PMATH_SYMBOL_ASSIGN, 2)){
      pmath_t val = pmath_expr_extract_item(def, 2);
      
      val = pmath_evaluate(val);
      
      def = pmath_expr_set_item(def, 2, val);
    }
    
    vars = pmath_expr_set_item(vars, i, def);
  }
  
  oldvals = pmath_ref(vars);
  for(i = pmath_expr_length(vars);i > 0;--i){
    pmath_t sym = pmath_expr_get_item(vars, i);
    
    if(!pmath_is_symbol(sym)){
      pmath_t tmp = sym;
      
      assert(pmath_is_expr(tmp));
      
      sym = pmath_expr_get_item(tmp, 1);
      pmath_unref(tmp);
      
      assert(pmath_is_symbol(sym));
    }
    
    oldvals = pmath_expr_set_item(oldvals, i, get_definition_and_clear(sym));
    pmath_unref(sym);
  }
  
  pmath_unref(pmath_evaluate(vars));
  body = pmath_evaluate(body);
  
  ex = pmath_catch();
  pmath_unref(pmath_evaluate(oldvals));
  if(!pmath_same(ex, PMATH_UNDEFINED))
    pmath_throw(ex);
  
  return body;
}
