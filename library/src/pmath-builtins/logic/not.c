#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>


extern pmath_symbol_t pmath_System_Equal;
extern pmath_symbol_t pmath_System_False;
extern pmath_symbol_t pmath_System_Greater;
extern pmath_symbol_t pmath_System_GreaterEqual;
extern pmath_symbol_t pmath_System_Less;
extern pmath_symbol_t pmath_System_LessEqual;
extern pmath_symbol_t pmath_System_Not;
extern pmath_symbol_t pmath_System_True;
extern pmath_symbol_t pmath_System_Undefined;
extern pmath_symbol_t pmath_System_Unequal;


static pmath_symbol_t opposite_inequality(pmath_symbol_t head);

PMATH_PRIVATE pmath_t builtin_not(pmath_expr_t expr) {
  pmath_t item;
  
  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  item = pmath_expr_get_item(expr, 1);
  if(pmath_same(item, pmath_System_True)) {
    pmath_unref(item);
    pmath_unref(expr);
    return pmath_ref(pmath_System_False);
  }
  
  if(pmath_same(item, pmath_System_False)) {
    pmath_unref(item);
    pmath_unref(expr);
    return pmath_ref(pmath_System_True);
  }
  
  if(pmath_same(item, pmath_System_Undefined)) {
    pmath_unref(expr);
    return item;
  }
  
  if(pmath_is_expr(item)) {
    pmath_t head = pmath_expr_get_item(item, 0);
    pmath_unref(head);
    
    size_t len = pmath_expr_length(item);
    
    if(len == 1 && pmath_same(head, pmath_System_Not)) {
      pmath_t result = pmath_expr_get_item(item, 1);
      pmath_unref(item);
      pmath_unref(expr);
      return result;
    }
    else if(len == 2) {
      head = opposite_inequality(head);
      if(!pmath_same(head, PMATH_UNDEFINED)) {
        pmath_unref(expr);
        item = pmath_expr_set_item(item, 0, pmath_ref(head));
        return item;
      }
    }
  }
  pmath_unref(item);
  
  return expr;
}


static pmath_symbol_t opposite_inequality(pmath_symbol_t head) { // won't be freed; must call pmath_ref() on result.
  if(pmath_same(head, pmath_System_Equal))   return pmath_System_Unequal;
  if(pmath_same(head, pmath_System_Unequal)) return pmath_System_Equal;
  
  // Note: Handle real inequalities only if they do not apply at all to complex numbers. 
  // E.g. we could define "2 < ImaginaryI" to be False, in which case "Not(x < y)" may *not* simplify to "x >= y".
  // In that case we might want to simplify using heads NotLess, NotLessEqual, NotGreater etc.
  
  if(pmath_same(head, pmath_System_Less))         return pmath_System_GreaterEqual;
  if(pmath_same(head, pmath_System_LessEqual))    return pmath_System_Greater;
  if(pmath_same(head, pmath_System_Greater))      return pmath_System_LessEqual;
  if(pmath_same(head, pmath_System_GreaterEqual)) return pmath_System_Less;
  
  return PMATH_UNDEFINED;
}
