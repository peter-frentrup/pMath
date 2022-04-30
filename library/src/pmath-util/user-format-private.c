#include <pmath-util/user-format-private.h>

#include <pmath-core/objects-private.h>
#include <pmath-core/symbols-private.h>

#include <pmath-builtins/lists-private.h>
#include <pmath-builtins/all-symbols-private.h>

#include <pmath-util/concurrency/threads-private.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/messages.h>
#include <pmath-util/symbol-values-private.h>


extern pmath_symbol_t pmath_System_Format;

PMATH_PRIVATE
pmath_t _pmath_get_user_format(pmath_t obj) {
  if(pmath_is_symbol(obj) || pmath_is_expr(obj)) {
    pmath_symbol_t head = _pmath_topmost_symbol(obj);
    
    if(!pmath_is_null(head)) {
      struct _pmath_symbol_rules_t *rules;
      
      rules = _pmath_symbol_get_rules(head, RULES_READ);
      
      if(rules) {
        pmath_t debug_metadata = pmath_get_debug_metadata(obj);
        pmath_t result     = pmath_expr_new_extended(
                               pmath_ref(pmath_System_Format), 1,
                               pmath_ref(obj));
                               
        if(_pmath_rulecache_find(&rules->format_rules, &result)) {
          pmath_unref(head);
          result = pmath_evaluate(result);
          return pmath_try_set_debug_metadata(result, debug_metadata);
        }
        
        pmath_unref(result);
        pmath_unref(debug_metadata);
      }
      
      pmath_unref(head);
    }
  }
  
  return PMATH_UNDEFINED;
}


PMATH_PRIVATE
pmath_bool_t _pmath_write_user_format(struct pmath_write_ex_t *info, pmath_t obj) {
  pmath_thread_t me;
  pmath_t format;
  
  if(info->options & (PMATH_WRITE_OPTIONS_INPUTEXPR | PMATH_WRITE_OPTIONS_FULLEXPR))
    return FALSE;
  
  format = _pmath_get_user_format(obj);
  
  if(pmath_same(format, PMATH_UNDEFINED))
    return FALSE;
  
  me = pmath_thread_get_current();
    
  if(!me || me->evaldepth >= pmath_maxrecursion) {
    if(!me->critical_messages) {
      int old_evaldepth = me->evaldepth;
      me->evaldepth = 0;
      me->critical_messages = TRUE;
      
      pmath_message(pmath_System_Format, "forml", 0);
      
      me->critical_messages = FALSE;
      me->evaldepth = old_evaldepth;
    }
  
    pmath_unref(format);
    return FALSE;
  }
    
  if(me)
    me->evaldepth++;
    
  _pmath_write_impl(info, format);
  
  if(me)
    me->evaldepth--;
  
  pmath_unref(format);
  return TRUE;
}
