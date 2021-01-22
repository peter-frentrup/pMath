#include <pmath-core/expressions-private.h>
#include <pmath-core/symbols-private.h>

#include <pmath-util/dispatch-tables.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>
#include <pmath-util/option-helpers-private.h>
#include <pmath-util/symbol-values-private.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/control/definitions-private.h>
#include <pmath-builtins/control-private.h>


PMATH_PRIVATE pmath_t builtin_assign_options(pmath_expr_t expr) {
  struct _pmath_symbol_rules_t *rules;
  pmath_t tag;
  pmath_t lhs;
  pmath_t rhs;
  pmath_t sym;
  int     assignment;
  
  assignment = _pmath_is_assignment(expr, &tag, &lhs, &rhs);
  
  if(!assignment)
    return expr;
    
  if(!pmath_is_expr_of_len(lhs, PMATH_SYMBOL_OPTIONS, 1)) {
    pmath_unref(tag);
    pmath_unref(lhs);
    pmath_unref(rhs);
    return expr;
  }
  
  lhs = pmath_evaluate_expression(lhs, FALSE);
  
  sym = pmath_expr_get_item(lhs, 1);
  
  if(!pmath_same(tag, PMATH_UNDEFINED)) {
    if(!pmath_is_symbol(sym)) {
      pmath_unref(tag);
      pmath_unref(lhs);
      pmath_unref(rhs);
      pmath_unref(sym);
      return expr;
    }
    
    if(!pmath_same(tag, sym)) {
      pmath_message(PMATH_NULL, "tag", 3, tag, lhs, sym);
      
      pmath_unref(expr);
      if(pmath_same(rhs, PMATH_UNDEFINED))
        return pmath_ref(PMATH_SYMBOL_FAILED);
        
      if(assignment < 0) {
        pmath_unref(rhs);
        return PMATH_NULL;
      }
      
      return rhs;
    }
  }
  
  pmath_unref(tag);
  pmath_unref(expr);
  
  if(!pmath_is_symbol(sym)) {
    pmath_message(PMATH_NULL, "fnsym", 1, lhs);
    
    pmath_unref(sym);
    if(pmath_same(rhs, PMATH_UNDEFINED))
      return pmath_ref(PMATH_SYMBOL_FAILED);
    return rhs;
  }
  
  rules = _pmath_symbol_get_rules(sym, RULES_WRITEOPTIONS);
  pmath_unref(sym);
  
  if(!rules) {
    pmath_unref(lhs);
    pmath_unref(rhs);
    return pmath_ref(PMATH_SYMBOL_FAILED);
  }
  
  if(assignment < 0) {
    _pmath_rulecache_change(&rules->default_rules, lhs, rhs);
    return PMATH_NULL;
  }
  
  _pmath_rulecache_change(&rules->default_rules, lhs, pmath_ref(rhs));
  
  return rhs;
}

PMATH_PRIVATE pmath_t builtin_isoption(pmath_expr_t expr) {
 /* IsOption(e)
  */
  pmath_t arg;
  
  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  arg = pmath_expr_get_item(expr, 1);
  pmath_unref(expr);
  
  if(pmath_is_set_of_options(arg))
    return pmath_ref(PMATH_SYMBOL_TRUE);
    
  return pmath_ref(PMATH_SYMBOL_FALSE);
}

static
pmath_bool_t check_set_of_options(pmath_t expr) {
  size_t i;
  
  if(!pmath_is_expr(expr)) {
    pmath_message(PMATH_NULL, "rep", 1, pmath_ref(expr));
    return FALSE;
  }
  
  if(_pmath_is_rule(expr))
    return TRUE;
  
  if(pmath_is_list_of_rules(expr))
    return TRUE;
    
  if(!pmath_is_expr_of(expr, PMATH_SYMBOL_LIST)) {
    pmath_message(PMATH_NULL, "rep", 1, pmath_ref(expr));
    return FALSE;
  }
  
  for(i = pmath_expr_length(expr); i > 0; --i) {
    pmath_t o = pmath_expr_get_item(expr, i);
    
    if(!check_set_of_options(o)) {
      pmath_unref(o);
      return FALSE;
    }
    
    pmath_unref(o);
  }
  
  return TRUE;
}

PMATH_PRIVATE pmath_t builtin_optionvalue(pmath_expr_t expr) {
  /* OptionValue(fn, extrarules, names)
     OptionValue(fn, names)
     OptionValue(names)
  
     OptionValue(fn, extrarules, names, Hold)
     OptionValue(Automatic, Automatic, names, Hold)
   */
  pmath_t func, name, extra, result_head, default_opts;
  
  size_t exprlen = pmath_expr_length(expr);
  if(exprlen < 1 || exprlen > 4) {
    pmath_message_argxxx(exprlen, 1, 4);
    return expr;
  }
  
  if(exprlen == 1)
    return expr;
    
  func  = pmath_expr_get_item(expr, 1);
  extra = PMATH_UNDEFINED;
  
  if(exprlen >= 3)
    extra = pmath_expr_get_item(expr, 2);
    
  if(exprlen >= 4) {
    name        = pmath_expr_get_item(expr, 3);
    result_head = pmath_expr_get_item(expr, 4);
  }
  else {
    name        = pmath_expr_get_item(expr, exprlen);
    result_head = PMATH_UNDEFINED;
  }
  
  pmath_unref(expr);
  //default_opts = _pmath_options_from_expr(func);
  default_opts = pmath_evaluate(pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_OPTIONS), 1, pmath_ref(func)));
  
  if(!pmath_same(extra, PMATH_UNDEFINED)) {
    if(check_set_of_options(extra))
      _pmath_options_check_subset_of(extra, default_opts, "optnf", func);
  }
  
  if(pmath_is_expr_of(name, PMATH_SYMBOL_LIST)) {
    pmath_t name_list = name;
    size_t i;
    
    for(i = pmath_expr_length(name_list); i > 0; --i) {
      name = pmath_expr_get_item(name, i);
      
      expr = _pmath_option_find_value(name, extra);
      if(pmath_same(expr, PMATH_UNDEFINED))
        expr = _pmath_option_find_value(name, default_opts);
        
      if(pmath_same(expr, PMATH_UNDEFINED)) {
        pmath_message(
          PMATH_NULL, "optnf", 2,
          pmath_ref(name),
          func);
          
        expr = pmath_ref(name);
      }
      
      if(!pmath_same(result_head, PMATH_UNDEFINED))
        expr = pmath_expr_new_extended(pmath_ref(result_head), 1, expr);
        
      name_list = pmath_expr_set_item(
                    name_list, i,
                    expr);
                    
      pmath_unref(name);
    }
    
    pmath_unref(result_head);
    pmath_unref(extra);
    pmath_unref(func);
    return name_list;
  }
  
  expr = _pmath_option_find_value(name, extra);
  if(pmath_same(expr, PMATH_UNDEFINED))
    expr = _pmath_option_find_value(name, default_opts);
    
  if(pmath_same(expr, PMATH_UNDEFINED)) {
    pmath_message(
      PMATH_NULL, "optnf", 2,
      pmath_ref(name),
      func);
      
    expr = pmath_ref(name);
  }
  
  if(!pmath_same(result_head, PMATH_UNDEFINED))
    expr = pmath_expr_new_extended(result_head, 1, expr);
    
  pmath_unref(default_opts);
  pmath_unref(extra);
  pmath_unref(name);
  pmath_unref(func);
  return expr;
}

PMATH_PRIVATE pmath_t builtin_options(pmath_expr_t expr) {
  /* Options(f)
     Options(f, names)
   */
  pmath_expr_t options;
  pmath_t sym;
  size_t len;
  
  len = pmath_expr_length(expr);
  
  if(len < 1 || len > 2) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 2);
    return expr;
  }
  
  sym = pmath_expr_get_item(expr, 1);
  if(pmath_is_string(sym))
    sym = pmath_symbol_find(sym, FALSE);
    
  options = _pmath_options_from_expr(sym);
  
  if(len == 2) {
    size_t i;
    pmath_bool_t have_error;
    pmath_t names;
    
    names = pmath_expr_get_item(expr, 2);
    pmath_unref(expr);
    
    if(!pmath_is_expr_of(names, PMATH_SYMBOL_LIST)) {
      names = pmath_expr_new_extended(
                pmath_ref(PMATH_SYMBOL_LIST), 1,
                names);
    }
    
    have_error = FALSE;
    for(i = 1; i <= pmath_expr_length(names); ++i) {
      pmath_t name = pmath_expr_get_item(names, i);
      pmath_t rule = _pmath_option_find_rule(name, options);
      
      if(pmath_is_expr(rule)) {
        names = pmath_expr_set_item(names, i, rule);
      }
      else {
        pmath_message(
          PMATH_NULL, "optnf", 2,
          name,
          pmath_ref(sym));
          
        have_error = TRUE;
        names = pmath_expr_set_item(names, i, PMATH_UNDEFINED);
      }
//      found = FALSE;
//      for(j = 1; j <= pmath_expr_length(options); ++j) {
//        pmath_t rule = pmath_expr_get_item(options, j);
//
//        if(_pmath_is_rule(rule)) {
//          pmath_t lhs = pmath_expr_get_item(rule, 1);
//
//          if(pmath_equals(lhs, name)) {
//            pmath_unref(lhs);
//            names = pmath_expr_set_item(names, i, rule);
//
//            found = TRUE;
//            break;
//          }
//
//          pmath_unref(lhs);
//        }
//
//        pmath_unref(rule);
//      }
//
//      if(!found) {
//        pmath_message(
//          PMATH_NULL, "optnf", 2,
//          name,
//          pmath_ref(sym));
//
//        have_error = TRUE;
//        names = pmath_expr_set_item(names, i, PMATH_UNDEFINED);
//      }
//      else
//        pmath_unref(name);
    }
    
    if(have_error)
      names = pmath_expr_remove_all(names, PMATH_UNDEFINED);
      
    pmath_unref(sym);
    pmath_unref(options);
    return names;
  }
  
  pmath_unref(expr);
  pmath_unref(sym);
  
  if(pmath_is_expr(options))
    options = pmath_expr_flatten(options, pmath_ref(PMATH_SYMBOL_LIST), SIZE_MAX);
    
  return options;
}

static pmath_expr_t set_option(
  pmath_expr_t options, // will be freed; changed options returned on su8ccess
  pmath_t     rule,    // will be freed
  pmath_t     sym      // wont be freed
) {
  size_t i;
  pmath_t lhs;
  
  if(!_pmath_is_rule(rule)) {
    pmath_message(PMATH_NULL, "reps", 1, rule);
    
    pmath_unref(options);
    return PMATH_NULL;
  }
  
  lhs = pmath_expr_get_item(rule, 1);
  
  for(i = 1; i <= pmath_expr_length(options); ++i) {
    pmath_t old_rule = pmath_expr_get_item(options, i);
    pmath_t old_lhs = pmath_expr_get_item(old_rule, 1);
    
    pmath_unref(old_rule);
    if(pmath_equals(lhs, old_lhs)) {
      pmath_unref(old_lhs);
      pmath_unref(lhs);
      return pmath_expr_set_item(options, i, rule);
    }
    
    pmath_unref(old_lhs);
  }
  
  pmath_message(
    PMATH_NULL, "optnf", 2,
    lhs,
    pmath_ref(sym));
    
  pmath_unref(rule);
  pmath_unref(options);
  return PMATH_NULL;
}

PMATH_PRIVATE pmath_t builtin_setoptions(pmath_expr_t expr) {
  /* SetOptions(f, name1->value1, ...)
   */
  pmath_expr_t options;
  pmath_symbol_t sym;
  size_t i, len;
  
  len = pmath_expr_length(expr);
  if(len < 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, SIZE_MAX);
    return expr;
  }
  
  sym = pmath_expr_get_item(expr, 1);
  if(!pmath_is_symbol(sym)) {
    pmath_message(PMATH_NULL, "sym", 2, sym, PMATH_FROM_INT32(1));
    return expr;
  }
  
  options = pmath_evaluate(
              pmath_expr_new_extended(
                pmath_ref(PMATH_SYMBOL_OPTIONS), 1,
                pmath_ref(sym)));
                
  if(!pmath_is_list_of_rules(options))
    options = pmath_ref(_pmath_object_emptylist);
    
  for(i = 2; i <= len; ++i) {
    pmath_t item = pmath_expr_get_item(expr, i);
    
    if(pmath_is_expr_of(item, PMATH_SYMBOL_LIST)) {
      size_t j;
      
      for(j = 1; j <= pmath_expr_length(item); ++j) {
        options = set_option(
                    options,
                    pmath_expr_get_item(item, j),
                    sym);
                    
        if(pmath_is_null(options)) {
          pmath_unref(item);
          pmath_unref(sym);
          return expr;
        }
      }
    }
    else {
      options = set_option(options, item, sym);
      
      if(pmath_is_null(options)) {
        pmath_unref(sym);
        return expr;
      }
    }
  }
  
  pmath_unref(expr);
  expr = pmath_expr_new_extended(
           pmath_ref(PMATH_SYMBOL_ASSIGN), 2,
           pmath_expr_new_extended(
             pmath_ref(PMATH_SYMBOL_OPTIONS), 1,
             sym),
           options);
           
  return builtin_assign_options(expr);
}
