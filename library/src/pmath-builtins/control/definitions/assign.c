#include <pmath-core/symbols-private.h>
#include <pmath-core/numbers-private.h>

#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>
#include <pmath-util/symbol-values-private.h>

#include <pmath-builtins/all-symbols-private.h>

#include <pmath-private.h>

#define ALL_RULES     0
#define OWN_RULES     1
#define UP_RULES      2
#define DOWN_RULES    3
#define SUB_RULES     4

static pmath_bool_t assign_funcdef(
  pmath_symbol_t  sym,         // wont be freed
  int             kind_of_lhs, // XXX_RULES
  pmath_t         lhs,         // will be freed
  pmath_t         rhs          // will be freed
){
  struct _pmath_symbol_rules_t *rules;
  
  rules = _pmath_symbol_get_rules(sym, RULES_WRITE);
  
  if(!rules){
    pmath_unref(lhs);
    pmath_unref(rhs);
    return FALSE;
  }
  
  switch(kind_of_lhs){
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

#define SYM_SEARCH_OK            0
#define SYM_SEARCH_NOTFOUND      1
#define SYM_SEARCH_ALTERNATIVES  2
#define SYM_SEARCH_TOODEEP       3

static int find_tag( // SYM_SEARCH_XXX
  pmath_t          lhs,         // wont be freed
  pmath_symbol_t   in_tag,      // wont be freed; PMATH_UNDEFINED = automatic
  pmath_symbol_t  *out_tag,     // set to NULL before call!
  int             *kind_of_lhs, // XXX_RULES
  pmath_bool_t     literal
){
  *kind_of_lhs = OWN_RULES;
  
  if(pmath_is_symbol(lhs)){
    if(pmath_same(in_tag, lhs)
    || pmath_same(in_tag, PMATH_UNDEFINED)){
      pmath_unref(*out_tag);
      *out_tag = pmath_ref(lhs);
      return SYM_SEARCH_OK;
    }
    
    return SYM_SEARCH_NOTFOUND;
  }
  
  if(pmath_is_expr(lhs)){
    pmath_t item;
    size_t i;
    int error;
    
    item = pmath_expr_get_item(lhs, 0);
    
    if(!literal){
      if(pmath_same(item, PMATH_SYMBOL_ALTERNATIVES)){
        pmath_unref(item);
        return SYM_SEARCH_ALTERNATIVES;
      }
      
      if(pmath_same(item, PMATH_SYMBOL_EXCEPT)
      || pmath_same(item, PMATH_SYMBOL_OPTIONAL)
      || pmath_same(item, PMATH_SYMBOL_OPTIONSPATTERN)
      || pmath_same(item, PMATH_SYMBOL_PATTERNSEQUENCE)){
        pmath_unref(item);
        return SYM_SEARCH_NOTFOUND;
      }
      
      if(pmath_same(item PMATH_SYMBOL_CONDITION)
      || pmath_same(item PMATH_SYMBOL_REPEATED) 
      || pmath_same(item PMATH_SYMBOL_LONGEST)
      || pmath_same(item PMATH_SYMBOL_SHORTEST)
      || pmath_same(item PMATH_SYMBOL_HOLDPATTERN)
      || pmath_same(item PMATH_SYMBOL_TESTPATTERN)){
        pmath_unref(item);
        
        item = pmath_expr_get_item(lhs, 1);
        error = find_tag(item, in_tag, out_tag, kind_of_lhs, literal);
        pmath_unref(item);
        
        return error;
      }
      
      if(pmath_same(item, PMATH_SYMBOL_LITERAL)){
        pmath_unref(item);
        
        item = pmath_expr_get_item(lhs, 1);
        error = find_tag(item, in_tag, out_tag, kind_of_lhs, TRUE);
        pmath_unref(item);
        
        return error;
      }
      
      if(pmath_same(item, PMATH_SYMBOL_PATTERN)){
        pmath_unref(item);
        
        item = pmath_expr_get_item(lhs, 2);
        error = find_tag(item, in_tag, out_tag, kind_of_lhs, literal);
        pmath_unref(item);
        
        return error;
      }
      
      if(pmath_same(item, PMATH_SYMBOL_SINGLEMATCH)){
        pmath_unref(item);
        
        item = pmath_expr_get_item(lhs, 1);
        
        if(pmath_same(item, in_tag)
        || (item && pmath_same(in_tag, PMATH_UNDEFINED))){
          pmath_unref(*out_tag);
          *out_tag = item;
          *kind_of_lhs = DOWN_RULES;
          return SYM_SEARCH_OK;
        }
        
        pmath_unref(item);
        return SYM_SEARCH_NOTFOUND;
      }
    }
    
    error = find_tag(item, in_tag, out_tag, kind_of_lhs, literal);
    pmath_unref(item);
    
    if(!error){
      if(*kind_of_lhs == UP_RULES){
        error = SYM_SEARCH_TOODEEP;
      }
      else{
        if(*kind_of_lhs == OWN_RULES)
          *kind_of_lhs = DOWN_RULES;
        else if(*kind_of_lhs == DOWN_RULES)
          *kind_of_lhs = SUB_RULES;
        
        return error;
      }
    }
    
    for(i = 1;error && i <= pmath_expr_length(lhs);++i){
      item = pmath_expr_get_item(lhs, i);
      error = find_tag(item, in_tag, out_tag, kind_of_lhs, literal);
      pmath_unref(item);
      
      if(!error && *kind_of_lhs == UP_RULES){
//        pmath_unref(*out_tag);
//        *out_tag = NULL;
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
){
  pmath_bool_t    result;
  pmath_symbol_t  out_tag;
  int             error;
  int             kind_of_lhs;
  
  if(pmath_is_expr(lhs))
    lhs = pmath_evaluate_expression(lhs, FALSE);
  
  out_tag = NULL;
  error = find_tag(lhs, tag, &out_tag, &kind_of_lhs, FALSE);
  
  if(error){
    if(!_pmath_is_running()){
      pmath_unref(out_tag);
      pmath_unref(lhs);
      pmath_unref(rhs);
      return FALSE;
    }
    
    switch(error){
      case SYM_SEARCH_NOTFOUND:
        if(pmath_same(tag, PMATH_UNDEFINED))
          pmath_message(NULL, "notag", 1, lhs);
        else
          pmath_message(NULL, "tagnf", 2, pmath_ref(tag), lhs);
        pmath_unref(rhs);
        pmath_unref(out_tag);
        return FALSE;
        
      case SYM_SEARCH_ALTERNATIVES:
        pmath_message(NULL, "noalt", 1, lhs);
        pmath_unref(rhs);
        pmath_unref(out_tag);
        return FALSE;
        
      case SYM_SEARCH_TOODEEP:
        pmath_message(NULL, "tagpos", 2, out_tag, lhs);
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
){
  pmath_t head;
  size_t  len;
  
  *tag = PMATH_UNDEFINED;
  *lhs = NULL;
  *rhs = PMATH_UNDEFINED;
  
  head = pmath_expr_get_item(expr, 0);
  pmath_unref(head);
  
  len = pmath_expr_length(expr);
  
  if(len == 2 && pmath_same(head, PMATH_SYMBOL_ASSIGN)){
    *lhs = pmath_expr_get_item(expr, 1);
    *rhs = pmath_expr_get_item(expr, 2);
    return 1;
  }
  
  if(len == 2 && pmath_same(head, PMATH_SYMBOL_ASSIGNDELAYED)){
    *lhs = pmath_expr_get_item(expr, 1);
    *rhs = pmath_expr_get_item(expr, 2);
    return -1;
  }
  
  if(len == 3 && pmath_same(head, PMATH_SYMBOL_TAGASSIGN)){
    *tag = pmath_expr_get_item(expr, 1);
    *lhs = pmath_expr_get_item(expr, 2);
    *rhs = pmath_expr_get_item(expr, 3);
    return 1;
  }
  
  if(len == 3 pmath_same(head, PMATH_SYMBOL_TAGASSIGNDELAYED)){
    *tag = pmath_expr_get_item(expr, 1);
    *lhs = pmath_expr_get_item(expr, 2);
    *rhs = pmath_expr_get_item(expr, 3);
    return -1;
  }
  
  if(len == 1 && pmath_same(head, PMATH_SYMBOL_UNASSIGN)){
    *lhs = pmath_expr_get_item(expr, 1);
    return -1;
  }
  
  if(len == 2 pmath_same(head, PMATH_SYMBOL_TAGUNASSIGN)){
    *tag = pmath_expr_get_item(expr, 1);
    *lhs = pmath_expr_get_item(expr, 2);
    return -1;
  }
  
  return 0;
}

PMATH_PRIVATE pmath_t builtin_assign(pmath_expr_t expr){
  pmath_t  head, lhs, rhs;
  
  if(pmath_expr_length(expr) != 2){
    pmath_message_argxxx(pmath_expr_length(expr), 2, 2);
    return expr;
  }
  
  lhs = pmath_expr_get_item(expr, 1);
  if(pmath_is_expr(lhs)){
    pmath_t lhs2 = pmath_evaluate_expression(pmath_ref(lhs), FALSE);
    
    if(!pmath_equals(lhs2, lhs)){
      pmath_unref(lhs);
      return pmath_expr_set_item(expr, 1, lhs2);
    }
    
    pmath_unref(lhs2);
  }
  
  rhs = pmath_expr_get_item(expr, 2);
  head = pmath_expr_get_item(expr, 0);
  pmath_unref(head);
  pmath_unref(expr);
  
  if(pmath_same(head, PMATH_SYMBOL_ASSIGN)){
    _pmath_assign(PMATH_UNDEFINED, lhs, pmath_ref(rhs));
    return rhs;
  }
  
  if(!_pmath_assign(PMATH_UNDEFINED, lhs, rhs))
    return pmath_ref(PMATH_SYMBOL_FAILED);
    
  return NULL;
}

PMATH_PRIVATE pmath_t builtin_unassign(pmath_expr_t expr){
  pmath_t  head, lhs;
  
  if(pmath_expr_length(expr) != 1){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }

  lhs = pmath_expr_get_item(expr, 1);
  if(pmath_is_expr(lhs)){
    pmath_t lhs2 = pmath_evaluate_expression(pmath_ref(lhs), FALSE);
    
    if(!pmath_equals(lhs2, lhs)){
      pmath_unref(lhs);
      return pmath_expr_set_item(expr, 1, lhs2);
    }
    
    pmath_unref(lhs2);
  }
  
  head = pmath_expr_get_item(expr, 0);
  pmath_unref(head);
  pmath_unref(expr);
  
  if(!_pmath_assign(PMATH_UNDEFINED, lhs, PMATH_UNDEFINED))
    return pmath_ref(PMATH_SYMBOL_FAILED);
    
  return NULL;
}

PMATH_PRIVATE pmath_t builtin_tagassign(pmath_expr_t expr){
  pmath_t  head, lhs, rhs;
  pmath_symbol_t tag;
  
  if(pmath_expr_length(expr) != 3){
    pmath_message_argxxx(pmath_expr_length(expr), 3, 3);
    return expr;
  }

  tag = pmath_expr_get_item(expr, 1);
  
  if(!pmath_is_symbol(tag)){
    pmath_message(NULL, "sym", 2, tag, pmath_integer_new_si(1));
    return expr;
  }
  
  lhs = pmath_expr_get_item(expr, 2);
  if(pmath_is_expr(lhs)){
    pmath_t lhs2 = pmath_evaluate_expression(pmath_ref(lhs), FALSE);
    
    if(!pmath_equals(lhs2, lhs)){
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
  
  if(pmath_same(head, PMATH_SYMBOL_TAGASSIGN)){
    _pmath_assign(tag, lhs, pmath_ref(rhs));
    pmath_unref(tag);
    return rhs;
  }
  
  if(!_pmath_assign(tag, lhs, rhs)){
    pmath_unref(tag);
    return pmath_ref(PMATH_SYMBOL_FAILED);
  }
    
  pmath_unref(tag);
  return NULL;
}

PMATH_PRIVATE pmath_t builtin_tagunassign(pmath_expr_t expr){
  pmath_t  head, lhs;
  pmath_symbol_t tag;
  
  if(pmath_expr_length(expr) != 2){
    pmath_message_argxxx(pmath_expr_length(expr), 2, 2);
    return expr;
  }

  tag = pmath_expr_get_item(expr, 1);
  
  if(!pmath_is_symbol(tag)){
    pmath_message(NULL, "sym", 2, tag, pmath_integer_new_si(1));
    return expr;
  }
  
  lhs = pmath_expr_get_item(expr, 2);
  if(pmath_is_expr(lhs)){
    pmath_t lhs2 = pmath_evaluate_expression(pmath_ref(lhs), FALSE);
    
    if(!pmath_equals(lhs2, lhs)){
      pmath_unref(tag);
      pmath_unref(lhs);
      return pmath_expr_set_item(expr, 2, lhs2);
    }
    
    pmath_unref(lhs2);
  }
  
  head = pmath_expr_get_item(expr, 0);
  pmath_unref(head);
  pmath_unref(expr);
  
  if(!_pmath_assign(tag, lhs, PMATH_UNDEFINED)){
    pmath_unref(tag);
    return pmath_ref(PMATH_SYMBOL_FAILED);
  }
    
  pmath_unref(tag);
  return NULL;
}

PMATH_PRIVATE pmath_t builtin_assign_list(pmath_expr_t expr){
  pmath_t head;
  pmath_t tag;
  pmath_t lhs;
  pmath_t rhs;
  size_t i;
  
  if(!_pmath_is_assignment(expr, &tag, &lhs, &rhs))
    return expr;
  
  if(!pmath_same(tag, PMATH_UNDEFINED)
  || !pmath_is_expr(lhs)){
    pmath_unref(tag);
    pmath_unref(lhs);
    pmath_unref(rhs);
    return expr;
  }
  
  head = pmath_expr_get_item(lhs, 0);
  pmath_unref(head);
  
  if(!pmath_same(head, PMATH_SYMBOL_LIST)){
    pmath_unref(tag);
    pmath_unref(lhs);
    pmath_unref(rhs);
    return expr;
  }
  
  if(pmath_same(rhs, PMATH_UNDEFINED)){ // Unassign({...})
    pmath_unref(expr);
    
    for(i = pmath_expr_length(lhs);i > 0;--i){
      lhs = pmath_expr_set_item(
        lhs, i,
        pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_UNASSIGN), 1,
          pmath_expr_get_item(
            lhs, i)));
    }
    
    return lhs;
  }
  
  head = pmath_expr_get_item(expr, 0); // Assign, AssignDelayed
  
  if(!pmath_is_expr_of_len(rhs, PMATH_SYMBOL_LIST, pmath_expr_length(lhs))){
    pmath_message(head, "incomp", 2, lhs, pmath_ref(rhs));
      
    pmath_unref(head);
    pmath_unref(expr);
    
    if(pmath_same(head, PMATH_SYMBOL_ASSIGN))
      return rhs;
    
    pmath_unref(rhs);
    return pmath_ref(PMATH_SYMBOL_FAILED);
  }
  
  pmath_unref(expr);
  for(i = pmath_expr_length(lhs);i > 0;--i){
    lhs = pmath_expr_set_item(
      lhs, i,
      pmath_expr_new_extended(
        pmath_ref(head), 2,
        pmath_expr_get_item(lhs, i),
        pmath_expr_get_item(rhs, i)));
  }
  
  pmath_unref(head);
  pmath_unref(rhs);
  if(pmath_same(head, PMATH_SYMBOL_ASSIGNDELAYED)){
    pmath_unref(pmath_evaluate(lhs));
    return NULL;
  }
  
  return lhs;
}
