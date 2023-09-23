#include <pmath-core/symbols-private.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/control/definitions-private.h>

#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>
#include <pmath-util/symbol-values-private.h>


extern pmath_symbol_t pmath_System_DefaultRules;
extern pmath_symbol_t pmath_System_DownRules;
extern pmath_symbol_t pmath_System_False;
extern pmath_symbol_t pmath_System_List;
extern pmath_symbol_t pmath_System_FormatRules;
extern pmath_symbol_t pmath_System_NRules;
extern pmath_symbol_t pmath_System_SubRules;
extern pmath_symbol_t pmath_System_True;
extern pmath_symbol_t pmath_System_UpRules;

static pmath_bool_t has_rules(struct _pmath_symbol_rules_t *rules, pmath_symbol_t kind) {
  if(pmath_same(kind, pmath_System_DefaultRules))  return !_pmath_rulecache_is_empty(&rules->default_rules);
  if(pmath_same(kind, pmath_System_DownRules))     return !_pmath_rulecache_is_empty(&rules->down_rules);
  if(pmath_same(kind, pmath_System_FormatRules))   return !_pmath_rulecache_is_empty(&rules->format_rules);
  if(pmath_same(kind, pmath_System_NRules))        return !_pmath_rulecache_is_empty(&rules->approx_rules);
  if(pmath_same(kind, pmath_System_SubRules))      return !_pmath_rulecache_is_empty(&rules->sub_rules);
  if(pmath_same(kind, pmath_System_UpRules))       return !_pmath_rulecache_is_empty(&rules->up_rules);
  
  pmath_message(PMATH_NULL, "norul", 1, pmath_ref(kind));
  return FALSE;
}

PMATH_PRIVATE pmath_t builtin_developer_hasassignedrules(pmath_expr_t expr) {
  /* Developer`HasAssignedRules(symbol)
     Developer`HasAssignedRules("name")
     Developer`HasAssignedRules(sym, DefaultRules)
     Developer`HasAssignedRules(sym, DownRules)
     Developer`HasAssignedRules(sym, FormatRules)
     Developer`HasAssignedRules(sym, NRules)
     Developer`HasAssignedRules(sym, SubRules)
     Developer`HasAssignedRules(sym, UpRules)
     Developer`HasAssignedRules(sym, {kind, kind2, ...})
  
     messages:
      Developer`HasAssignedRules::norul
      General::fnsym
   */
  
  struct _pmath_symbol_rules_t *rules;
  
  size_t exprlen = pmath_expr_length(expr);
  if(exprlen < 1 || exprlen > 2) {
    pmath_message_argxxx(exprlen, 1, 2);
    return expr;
  }
  
  pmath_t sym = pmath_expr_get_item(expr, 1);
  if(pmath_is_string(sym))
    sym = pmath_symbol_find(sym, FALSE);
    
  if(!pmath_is_symbol(sym)) {
    pmath_message(PMATH_NULL, "fnsym", 1, pmath_ref(expr));
    
    pmath_unref(sym);
    return expr;
  }
  
  rules = _pmath_symbol_get_rules(sym, RULES_READ);
  if(!rules) {
    pmath_unref(expr);
    pmath_unref(sym);
    return pmath_ref(pmath_System_False);
  }
  
  if(exprlen == 1) {
    pmath_unref(expr);
    
    if( !_pmath_rulecache_is_empty(&rules->approx_rules) ||
        !_pmath_rulecache_is_empty(&rules->default_rules) ||
        !_pmath_rulecache_is_empty(&rules->down_rules) ||
        !_pmath_rulecache_is_empty(&rules->format_rules) ||
        !_pmath_rulecache_is_empty(&rules->sub_rules) ||
        !_pmath_rulecache_is_empty(&rules->up_rules))
    { // Messages(sym) currenlty get ignored
      pmath_unref(sym);
      return pmath_ref(pmath_System_True);
    }
    
    pmath_unref(sym);
    return pmath_ref(pmath_System_False);
  }
  
  pmath_t kind = pmath_expr_get_item(expr, 2);
  pmath_unref(expr);
  
  if(pmath_is_expr_of(kind, pmath_System_List)) {
    size_t len = pmath_expr_length(kind);
    
    for(size_t i = 1; i <= len; ++i) {
      pmath_t sub_kind = pmath_expr_get_item(kind, i);
      
      pmath_bool_t any_sub = has_rules(rules, sub_kind);
      
      pmath_unref(sub_kind);
      if(any_sub) {
        pmath_unref(sym);
        pmath_unref(kind);
        return pmath_ref(pmath_System_True); 
      }
    }
    
    pmath_unref(sym);
    pmath_unref(kind);
    return pmath_ref(pmath_System_False); 
  }
  
  pmath_bool_t any = has_rules(rules, kind);
  
  pmath_unref(sym);
  pmath_unref(kind);
  
  return pmath_ref(any ? pmath_System_True : pmath_System_False);
}
