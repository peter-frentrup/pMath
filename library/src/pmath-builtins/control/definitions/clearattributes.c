#include <pmath-core/symbols-private.h>

#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/control/definitions-private.h>


extern pmath_symbol_t pmath_System_List;

// syms will be freed;
static pmath_bool_t clear_attributes(pmath_t syms, pmath_symbol_attributes_t mask) {

  if(pmath_is_string(syms))
    syms = pmath_symbol_find(syms, FALSE);
    
  if(pmath_is_symbol(syms)) {
    pmath_symbol_attributes_t atts = pmath_symbol_get_attributes(syms);
    
    atts &= mask;
    
    pmath_symbol_set_attributes(syms, atts);
    pmath_unref(syms);
    return TRUE;
  }
  
  if(pmath_is_expr_of(syms, pmath_System_List)) {
    size_t i;
    
    for(i = 1; i <= pmath_expr_length(syms); ++i) {
      pmath_t item = pmath_expr_get_item(syms, i);
      
      if(!clear_attributes(item, mask)) {
        pmath_unref(syms);
        return FALSE;
      }
    }
    
    pmath_unref(syms);
    return TRUE;
  }
  
  pmath_message(PMATH_NULL, "sym", 2, syms, PMATH_FROM_INT32(1));
  return FALSE;
}

PMATH_PRIVATE pmath_t builtin_clearattributes(pmath_expr_t expr) {
  pmath_t obj;
  pmath_symbol_attributes_t rem_atts;
  
  if(pmath_expr_length(expr) != 2) {
    pmath_message_argxxx(pmath_expr_length(expr), 2, 2);
    
    return expr;
  }
  
  obj = pmath_expr_get_item(expr, 2);
  if(!_pmath_get_attributes(&rem_atts, obj)) {
    pmath_unref(obj);
    return expr;
  }
  
  pmath_unref(obj);
  obj = pmath_expr_get_item(expr, 1);
  
  if(!clear_attributes(obj, ~rem_atts))
    return expr;
  
  pmath_unref(expr);
  return PMATH_NULL;
}
