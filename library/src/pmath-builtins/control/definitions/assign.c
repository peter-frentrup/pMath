#include <pmath-core/symbols-private.h>

#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>
#include <pmath-util/symbol-values-private.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/control/definitions-private.h>

#include <pmath-private.h>


extern pmath_symbol_t pmath_System_DollarFailed;
extern pmath_symbol_t pmath_System_Alternatives;
extern pmath_symbol_t pmath_System_Assign;
extern pmath_symbol_t pmath_System_AssignDelayed;
extern pmath_symbol_t pmath_System_Condition;
extern pmath_symbol_t pmath_System_Except;
extern pmath_symbol_t pmath_System_HoldPattern;
extern pmath_symbol_t pmath_System_List;
extern pmath_symbol_t pmath_System_Literal;
extern pmath_symbol_t pmath_System_Longest;
extern pmath_symbol_t pmath_System_Optional;
extern pmath_symbol_t pmath_System_OptionsPattern;
extern pmath_symbol_t pmath_System_Pattern;
extern pmath_symbol_t pmath_System_PatternSequence;
extern pmath_symbol_t pmath_System_Repeated;
extern pmath_symbol_t pmath_System_Shortest;
extern pmath_symbol_t pmath_System_SingleMatch;
extern pmath_symbol_t pmath_System_TagAssign;
extern pmath_symbol_t pmath_System_TagAssignDelayed;
extern pmath_symbol_t pmath_System_TagUnassign;
extern pmath_symbol_t pmath_System_TestPattern;
extern pmath_symbol_t pmath_System_Unassign;

static pmath_bool_t assign_funcdef(
  pmath_symbol_t  sym,         // wont be freed
  int             kind_of_lhs, // XXX_RULES
  pmath_t         lhs,         // will be freed
  pmath_t         rhs          // will be freed
) {
  struct _pmath_symbol_rules_t *rules;
  rules = _pmath_symbol_get_rules(sym, RULES_WRITE);
  
  if(!rules) {
    pmath_unref(lhs);
    pmath_unref(rhs);
    return FALSE;
  }
  
  switch(kind_of_lhs) {
    case UP_RULES:
      _pmath_rulecache_change(&rules->up_rules, lhs, rhs);
      break;
      
    case DOWN_RULES:
      _pmath_rulecache_change(&rules->down_rules, lhs, rhs);
      break;
      
    case SUB_RULES:
      _pmath_rulecache_change(&rules->sub_rules, lhs, rhs);
      break;
      
    default:
      assert(0 && "invalid kind_of_lhs\n");
  }
  
  pmath_symbol_update(sym);
  return TRUE;
}

PMATH_PRIVATE
int _pmath_find_tag( // SYM_SEARCH_XXX
  pmath_t          lhs,         // wont be freed
  pmath_symbol_t   in_tag,      // wont be freed; PMATH_UNDEFINED = automatic
  pmath_symbol_t  *out_tag,     // set to PMATH_NULL before call!
  int             *kind_of_lhs, // XXX_RULES
  pmath_bool_t     literal
) {
  *kind_of_lhs = OWN_RULES;
  
  if(pmath_is_symbol(lhs)) {
    if( pmath_same(in_tag, lhs) ||
        pmath_same(in_tag, PMATH_UNDEFINED))
    {
      pmath_unref(*out_tag);
      *out_tag = pmath_ref(lhs);
      return SYM_SEARCH_OK;
    }
    
    return SYM_SEARCH_NOTFOUND;
  }
  
  if(pmath_is_expr(lhs)) {
    pmath_t item;
    size_t i;
    int error;
    
    item = pmath_expr_get_item(lhs, 0);
    
    if(!literal) {
      if(pmath_same(item, pmath_System_Alternatives)) {
        pmath_unref(item);
        return SYM_SEARCH_ALTERNATIVES;
      }
      
      if( pmath_same(item, pmath_System_Except)         ||
          pmath_same(item, pmath_System_Optional)       ||
          pmath_same(item, pmath_System_OptionsPattern) ||
          pmath_same(item, pmath_System_PatternSequence))
      {
        pmath_unref(item);
        return SYM_SEARCH_NOTFOUND;
      }
      
      if( pmath_same(item, pmath_System_Condition)   ||
          pmath_same(item, pmath_System_Repeated)    ||
          pmath_same(item, pmath_System_Longest)     ||
          pmath_same(item, pmath_System_Shortest)    ||
          pmath_same(item, pmath_System_HoldPattern) ||
          pmath_same(item, pmath_System_TestPattern))
      {
        pmath_unref(item);
        
        item = pmath_expr_get_item(lhs, 1);
        error = _pmath_find_tag(item, in_tag, out_tag, kind_of_lhs, literal);
        pmath_unref(item);
        
        return error;
      }
      
      if(pmath_same(item, pmath_System_Literal)) {
        pmath_unref(item);
        
        item = pmath_expr_get_item(lhs, 1);
        error = _pmath_find_tag(item, in_tag, out_tag, kind_of_lhs, TRUE);
        pmath_unref(item);
        
        return error;
      }
      
      if(pmath_same(item, pmath_System_Pattern)) {
        pmath_unref(item);
        
        item = pmath_expr_get_item(lhs, 2);
        error = _pmath_find_tag(item, in_tag, out_tag, kind_of_lhs, literal);
        pmath_unref(item);
        
        return error;
      }
      
      if(pmath_same(item, pmath_System_SingleMatch)) {
        pmath_unref(item);
        
        item = pmath_expr_get_item(lhs, 1);
        
        if( pmath_same(item, in_tag) ||
            (!pmath_is_null(item) && pmath_same(in_tag, PMATH_UNDEFINED)))
        {
          pmath_unref(*out_tag);
          *out_tag = item;
          *kind_of_lhs = DOWN_RULES;
          return SYM_SEARCH_OK;
        }
        
        pmath_unref(item);
        return SYM_SEARCH_NOTFOUND;
      }
    }
    
    error = _pmath_find_tag(item, in_tag, out_tag, kind_of_lhs, literal);
    pmath_unref(item);
    
    if(!error) {
      if(*kind_of_lhs == UP_RULES) {
        error = SYM_SEARCH_TOODEEP;
      }
      else {
        if(*kind_of_lhs == OWN_RULES)
          *kind_of_lhs = DOWN_RULES;
        else if(*kind_of_lhs == DOWN_RULES)
          *kind_of_lhs = SUB_RULES;
          
        return error;
      }
    }
    
    for(i = 1; error && i <= pmath_expr_length(lhs); ++i) {
      item = pmath_expr_get_item(lhs, i);
      error = _pmath_find_tag(item, in_tag, out_tag, kind_of_lhs, literal);
      pmath_unref(item);
      
      if(!error && *kind_of_lhs == UP_RULES) {
//        pmath_unref(*out_tag);
//        *out_tag = PMATH_NULL;
        error = SYM_SEARCH_TOODEEP;
      }
    }
    
    *kind_of_lhs = UP_RULES;
    
    return error;
  }
  
  return SYM_SEARCH_NOTFOUND;
}

PMATH_PRIVATE
pmath_bool_t _pmath_assign(
  pmath_symbol_t tag,   // wont be freed; PMATH_UNDEFINED = automatic
  pmath_t        lhs,   // will be freed
  pmath_t        rhs    // will be freed; PMATH_UNDEFINED = remove rule
) {
  pmath_bool_t    result;
  pmath_symbol_t  out_tag;
  int             error;
  int             kind_of_lhs;
  
  if(pmath_is_expr(lhs))
    lhs = pmath_evaluate_expression(lhs, FALSE);
    
  out_tag = PMATH_NULL;
  error = _pmath_find_tag(lhs, tag, &out_tag, &kind_of_lhs, FALSE);
  
  if(error) {
    if(!_pmath_is_running()) {
      pmath_unref(out_tag);
      pmath_unref(lhs);
      pmath_unref(rhs);
      return FALSE;
    }
    
    switch(error) {
      case SYM_SEARCH_NOTFOUND:
        if(pmath_same(tag, PMATH_UNDEFINED))
          pmath_message(PMATH_NULL, "notag", 1, lhs);
        else
          pmath_message(PMATH_NULL, "tagnf", 2, pmath_ref(tag), lhs);
        pmath_unref(rhs);
        pmath_unref(out_tag);
        return FALSE;
        
      case SYM_SEARCH_ALTERNATIVES:
        pmath_message(PMATH_NULL, "noalt", 1, lhs);
        pmath_unref(rhs);
        pmath_unref(out_tag);
        return FALSE;
        
      case SYM_SEARCH_TOODEEP:
        pmath_message(PMATH_NULL, "tagpos", 2, out_tag, lhs);
        pmath_unref(rhs);
        return FALSE;
    }
  }
  
  if(kind_of_lhs == OWN_RULES)
    result = _pmath_symbol_assign_value(out_tag, lhs, rhs);
  else
    result = assign_funcdef(out_tag, kind_of_lhs, lhs, rhs);
    
  pmath_unref(out_tag);
  return result;
}

PMATH_PRIVATE
int _pmath_is_assignment(
  pmath_expr_t  expr,  // wont be freed
  pmath_t      *tag,   // out, may be PMATH_UNDEFINED
  pmath_t      *lhs,   // out
  pmath_t      *rhs    // out, may be PMATH_UNDEFINED
) {
  pmath_t head;
  size_t  len;
  
  *tag = PMATH_UNDEFINED;
  *lhs = PMATH_NULL;
  *rhs = PMATH_UNDEFINED;
  
  head = pmath_expr_get_item(expr, 0);
  pmath_unref(head);
  
  len = pmath_expr_length(expr);
  
  if(len == 2 && pmath_same(head, pmath_System_Assign)) {
    *lhs = pmath_expr_get_item(expr, 1);
    *rhs = pmath_expr_get_item(expr, 2);
    return 1;
  }
  
  if(len == 2 && pmath_same(head, pmath_System_AssignDelayed)) {
    *lhs = pmath_expr_get_item(expr, 1);
    *rhs = pmath_expr_get_item(expr, 2);
    return -1;
  }
  
  if(len == 3 && pmath_same(head, pmath_System_TagAssign)) {
    *tag = pmath_expr_get_item(expr, 1);
    *lhs = pmath_expr_get_item(expr, 2);
    *rhs = pmath_expr_get_item(expr, 3);
    return 1;
  }
  
  if(len == 3 && pmath_same(head, pmath_System_TagAssignDelayed)) {
    *tag = pmath_expr_get_item(expr, 1);
    *lhs = pmath_expr_get_item(expr, 2);
    *rhs = pmath_expr_get_item(expr, 3);
    return -1;
  }
  
  if(len == 1 && pmath_same(head, pmath_System_Unassign)) {
    *lhs = pmath_expr_get_item(expr, 1);
    return -1;
  }
  
  if(len == 2 && pmath_same(head, pmath_System_TagUnassign)) {
    *tag = pmath_expr_get_item(expr, 1);
    *lhs = pmath_expr_get_item(expr, 2);
    return -1;
  }
  
  return 0;
}

PMATH_PRIVATE pmath_t builtin_assign(pmath_expr_t expr) {
  pmath_t  head, lhs, rhs;
  
  if(pmath_expr_length(expr) != 2) {
    pmath_message_argxxx(pmath_expr_length(expr), 2, 2);
    return expr;
  }
  
  lhs = pmath_expr_get_item(expr, 1);
  if(pmath_is_expr(lhs)) {
    pmath_t lhs2 = pmath_evaluate_expression(pmath_ref(lhs), FALSE);
    
    if(!pmath_equals(lhs2, lhs)) {
      pmath_unref(lhs);
      return pmath_expr_set_item(expr, 1, lhs2);
    }
    
    pmath_unref(lhs2);
  }
  
  rhs = pmath_expr_get_item(expr, 2);
  head = pmath_expr_get_item(expr, 0);
  pmath_unref(head);
  pmath_unref(expr);
  
  if(pmath_same(head, pmath_System_Assign)) {
    _pmath_assign(PMATH_UNDEFINED, lhs, pmath_ref(rhs));
    return rhs;
  }
  
  if(!_pmath_assign(PMATH_UNDEFINED, lhs, rhs))
    return pmath_ref(pmath_System_DollarFailed);
    
  return PMATH_NULL;
}

PMATH_PRIVATE pmath_t builtin_assignwith(pmath_expr_t expr) {
  /*  AssithWith(x, f)    x //= f
   */
  size_t exprlen = pmath_expr_length(expr);
  pmath_t rhs, lhs, lhs_eval;
  
  if(exprlen != 2) {
    pmath_message_argxxx(exprlen, 2, 2);
    return expr;
  }
  
  lhs = pmath_expr_get_item(expr, 1);
  lhs_eval = pmath_evaluate(pmath_ref(lhs));
  if(pmath_equals(lhs, lhs_eval)) {
    pmath_message(PMATH_NULL, "rval", 1, lhs);
    pmath_unref(lhs_eval);
    return expr;
  }
  
  rhs = pmath_expr_get_item(expr, 2);
  pmath_unref(expr);
  
  expr = pmath_expr_new_extended(
           pmath_ref(pmath_System_Assign), 2,
           lhs,
           pmath_expr_new_extended(rhs, 1, lhs_eval));
             
  return expr;
}

PMATH_PRIVATE pmath_t builtin_unassign(pmath_expr_t expr) {
  pmath_t  head, lhs;
  
  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  lhs = pmath_expr_get_item(expr, 1);
  if(pmath_is_expr(lhs)) {
    pmath_t lhs2 = pmath_evaluate_expression(pmath_ref(lhs), FALSE);
    
    if(!pmath_equals(lhs2, lhs)) {
      pmath_unref(lhs);
      return pmath_expr_set_item(expr, 1, lhs2);
    }
    
    pmath_unref(lhs2);
  }
  
  head = pmath_expr_get_item(expr, 0);
  pmath_unref(head);
  pmath_unref(expr);
  
  if(!_pmath_assign(PMATH_UNDEFINED, lhs, PMATH_UNDEFINED))
    return pmath_ref(pmath_System_DollarFailed);
    
  return PMATH_NULL;
}

PMATH_PRIVATE pmath_t builtin_tagassign(pmath_expr_t expr) {
  pmath_t  head, lhs, rhs;
  pmath_symbol_t tag;
  
  if(pmath_expr_length(expr) != 3) {
    pmath_message_argxxx(pmath_expr_length(expr), 3, 3);
    return expr;
  }
  
  tag = pmath_expr_get_item(expr, 1);
  
  if(!pmath_is_symbol(tag)) {
    pmath_message(PMATH_NULL, "sym", 2, tag, PMATH_FROM_INT32(1));
    return expr;
  }
  
  lhs = pmath_expr_get_item(expr, 2);
  if(pmath_is_expr(lhs)) {
    pmath_t lhs2 = pmath_evaluate_expression(pmath_ref(lhs), FALSE);
    
    if(!pmath_equals(lhs2, lhs)) {
      pmath_unref(tag);
      pmath_unref(lhs);
      return pmath_expr_set_item(expr, 2, lhs2);
    }
    
    pmath_unref(lhs2);
  }
  
  rhs = pmath_expr_get_item(expr, 3);
  
  head = pmath_expr_get_item(expr, 0);
  pmath_unref(head);
  pmath_unref(expr);
  
  if(pmath_same(head, pmath_System_TagAssign)) {
    _pmath_assign(tag, lhs, pmath_ref(rhs));
    pmath_unref(tag);
    return rhs;
  }
  
  if(!_pmath_assign(tag, lhs, rhs)) {
    pmath_unref(tag);
    return pmath_ref(pmath_System_DollarFailed);
  }
  
  pmath_unref(tag);
  return PMATH_NULL;
}

PMATH_PRIVATE pmath_t builtin_tagunassign(pmath_expr_t expr) {
  pmath_t  head, lhs;
  pmath_symbol_t tag;
  
  if(pmath_expr_length(expr) != 2) {
    pmath_message_argxxx(pmath_expr_length(expr), 2, 2);
    return expr;
  }
  
  tag = pmath_expr_get_item(expr, 1);
  
  if(!pmath_is_symbol(tag)) {
    pmath_message(PMATH_NULL, "sym", 2, tag, PMATH_FROM_INT32(1));
    return expr;
  }
  
  lhs = pmath_expr_get_item(expr, 2);
  if(pmath_is_expr(lhs)) {
    pmath_t lhs2 = pmath_evaluate_expression(pmath_ref(lhs), FALSE);
    
    if(!pmath_equals(lhs2, lhs)) {
      pmath_unref(tag);
      pmath_unref(lhs);
      return pmath_expr_set_item(expr, 2, lhs2);
    }
    
    pmath_unref(lhs2);
  }
  
  head = pmath_expr_get_item(expr, 0);
  pmath_unref(head);
  pmath_unref(expr);
  
  if(!_pmath_assign(tag, lhs, PMATH_UNDEFINED)) {
    pmath_unref(tag);
    return pmath_ref(pmath_System_DollarFailed);
  }
  
  pmath_unref(tag);
  return PMATH_NULL;
}

PMATH_PRIVATE pmath_t builtin_assign_list(pmath_expr_t expr) {
  pmath_t head;
  pmath_t tag;
  pmath_t lhs;
  pmath_t rhs;
  size_t i;
  
  if(!_pmath_is_assignment(expr, &tag, &lhs, &rhs))
    return expr;
    
  if( !pmath_same(tag, PMATH_UNDEFINED) ||
      !pmath_is_expr(lhs))
  {
    pmath_unref(tag);
    pmath_unref(lhs);
    pmath_unref(rhs);
    return expr;
  }
  
  head = pmath_expr_get_item(lhs, 0);
  pmath_unref(head);
  
  if(!pmath_same(head, pmath_System_List)) {
    pmath_unref(tag);
    pmath_unref(lhs);
    pmath_unref(rhs);
    return expr;
  }
  
  if(pmath_same(rhs, PMATH_UNDEFINED)) { // Unassign({...})
    pmath_unref(expr);
    
    for(i = pmath_expr_length(lhs); i > 0; --i) {
      lhs = pmath_expr_set_item(
              lhs, i,
              pmath_expr_new_extended(
                pmath_ref(pmath_System_Unassign), 1,
                pmath_expr_get_item(
                  lhs, i)));
    }
    
    return lhs;
  }
  
  head = pmath_expr_get_item(expr, 0); // Assign, AssignDelayed
  
  if(!pmath_is_expr_of_len(rhs, pmath_System_List, pmath_expr_length(lhs))) {
    pmath_message(head, "incomp", 2, lhs, pmath_ref(rhs));
    
    pmath_unref(head);
    pmath_unref(expr);
    
    if(pmath_same(head, pmath_System_Assign))
      return rhs;
      
    pmath_unref(rhs);
    return pmath_ref(pmath_System_DollarFailed);
  }
  
  pmath_unref(expr);
  for(i = pmath_expr_length(lhs); i > 0; --i) {
    lhs = pmath_expr_set_item(
            lhs, i,
            pmath_expr_new_extended(
              pmath_ref(head), 2,
              pmath_expr_get_item(lhs, i),
              pmath_expr_get_item(rhs, i)));
  }
  
  pmath_unref(head);
  pmath_unref(rhs);
  if(pmath_same(head, pmath_System_AssignDelayed)) {
    pmath_unref(pmath_evaluate(lhs));
    return PMATH_NULL;
  }
  
  return lhs;
}
