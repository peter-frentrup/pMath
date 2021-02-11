#include <pmath-core/expressions.h>
#include <pmath-core/symbols-private.h>

#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/memory.h>
#include <pmath-util/messages.h>


extern pmath_symbol_t pmath_System_List;

static pmath_bool_t check_var_list(pmath_t vars);

PMATH_PRIVATE
PMATH_PRIVATE pmath_t builtin_internal_suppressdynamictrackingblock(pmath_expr_t expr) {
  /*  Internal`SuppressDynamicTrackingBlock({sym1, sym2, ...}, body)
      
      Temporarily resets the 'symN's dynamic tracker id to 0 while evaluating 'body'.
      In effect, changes to the 'symN' during evaluation of 'body' do not cause dynamic updates.
      
      However, if a current dynamic evaluation is in effect (pmath_thread_t::current_dynamic_id
      is non-zero, i.e. evaluating inside Internal`DynamicEvaluate), reading of 'symN' inside 
      'body' will again set the symbol's dynamic tracker id so that a subsequent change would 
      nontheless cause a dynamic update.
      Hence, it might be a good idea to wrap 'body' in Refresh(..., None) if necessary.
   */
  pmath_t vars;
  intptr_t *sym_ids;
  size_t len;
  
  if(pmath_expr_length(expr) != 2) {
    pmath_message_argxxx(pmath_expr_length(expr), 2, 2);
    return expr;
  }
  
  vars = pmath_expr_get_item(expr, 1);
  if(!check_var_list(vars)) {
    pmath_unref(vars);
    return expr;
  }
  
  len = pmath_expr_length(vars);
  sym_ids = pmath_mem_calloc(len, sizeof(intptr_t));
  
  if(sym_ids) {
    pmath_t body;
    size_t i;
    
    body = pmath_expr_get_item(expr, 2);
    pmath_unref(expr);
    
    for(i = 0; i < len; ++i) {
      pmath_symbol_t sym = pmath_expr_get_item(vars, i + 1);
      assert(pmath_is_symbol(sym));
      sym_ids[i] = _pmath_symbol_hard_reset_tracker(sym, 0);
      pmath_unref(sym);
    }
    
    expr = pmath_evaluate(body);
    
    for(i = 0; i < len; ++i) {
      pmath_symbol_t sym = pmath_expr_get_item(vars, i + 1);
      assert(pmath_is_symbol(sym));
      _pmath_symbol_lost_dynamic_tracker(sym, 0, sym_ids[i]);
      pmath_unref(sym);
    }
    
    pmath_mem_free(sym_ids);
  }
  
  pmath_unref(vars);
  return expr;
}

static pmath_bool_t check_var_list(pmath_t vars) {
  size_t i;
  size_t len;
  
  if(!pmath_is_expr_of(vars, pmath_System_List)) {
    pmath_message(PMATH_NULL, "vlist", 1, pmath_ref(vars));
    return FALSE;
  }
  
  len = pmath_expr_length(vars);
  for(i = 1; i <= len; ++i) {
    pmath_t sym = pmath_expr_get_item(vars, i);
    if(!pmath_is_symbol(sym)) {
      pmath_message(PMATH_NULL, "vsym", 2, pmath_ref(vars), sym);
      return FALSE;
    }
    pmath_unref(sym);
  }
  
  return TRUE;
}
