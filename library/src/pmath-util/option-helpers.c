#include <pmath-core/expressions-private.h>
#include <pmath-core/numbers.h>
#include <pmath-core/symbols-private.h>

#include <pmath-util/debug.h>
#include <pmath-util/dispatch-tables-private.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>
#include <pmath-util/option-helpers.h>
#include <pmath-util/symbol-values-private.h>

#include <pmath-builtins/all-symbols-private.h>


extern pmath_symbol_t pmath_System_List;
extern pmath_symbol_t pmath_System_OptionValue;
extern pmath_symbol_t pmath_System_Options;

static pmath_t get_default_options(pmath_t head) {
  struct _pmath_symbol_rules_t  *rules;
  
  if(!pmath_is_symbol(head))
    return pmath_ref(_pmath_object_emptylist);
    
  rules = _pmath_symbol_get_rules(head, RULES_READ);
  if(rules) {
    pmath_t result;
    result = pmath_ref(pmath_System_Options);
    
    if(_pmath_rulecache_find(&rules->default_rules, &result))
      return result;
      
//    pmath_unref(result);
//    result = pmath_expr_new_extended(
//                       pmath_ref(pmath_System_Options), 1, pmath_ref(head));
//                       
//    if(_pmath_rulecache_find(&rules->default_rules, &result))
//      return result;
      
    pmath_unref(result);
    return pmath_ref(_pmath_object_emptylist);
  }
  
  return pmath_ref(_pmath_object_emptylist);
}

PMATH_PRIVATE
pmath_expr_t _pmath_option_find_rule(pmath_t name, pmath_t option_set) {
  if(pmath_is_rule(option_set)) {
    pmath_t lhs = pmath_expr_get_item(option_set, 1);
    
    if(pmath_equals(name, lhs)) {
      pmath_unref(lhs);
      return pmath_ref(option_set);
    }
    
    pmath_unref(lhs);
    return PMATH_NULL;
  }
  
  if(pmath_is_list_of_rules(option_set)) 
    return _pmath_rules_find_rule(option_set, name, TRUE);
  
  if(pmath_is_expr_of(option_set, pmath_System_List)) {
    size_t i;
    size_t len = pmath_expr_length(option_set);
    const pmath_t *items = pmath_expr_read_item_data(option_set);
    
    if(!items) {
      if(pmath_is_packed_array(option_set))
        return PMATH_NULL;
      
      pmath_debug_print("[pmath_expr_read_item_data() gave NULL in %s:%d]\n", __FILE__, __LINE__);
      return PMATH_NULL;
    }
    
    for(i = 0; i < len; ++i) {
      pmath_t result = _pmath_option_find_rule(name, items[i]);
      
      if(!pmath_is_null(result))
        return result;
    }
  }
  
  return PMATH_NULL;
}

PMATH_PRIVATE
pmath_t _pmath_option_find_value(pmath_t name, pmath_t option_set) {
  pmath_t rule = _pmath_option_find_rule(name, option_set);
  pmath_t value;
  
  if(pmath_is_null(rule))
    return PMATH_UNDEFINED;
    
  value = pmath_expr_get_item(rule, 2);
  pmath_unref(rule);
  return value;
}

static pmath_bool_t is_option_name_of(pmath_t name, pmath_t option_set) {
  pmath_t value = _pmath_option_find_value(name, option_set);
  
  if(pmath_same(value, PMATH_UNDEFINED))
    return FALSE;
    
  pmath_unref(value);
  return TRUE;
}

PMATH_PRIVATE
pmath_bool_t _pmath_options_check_subset_of(
  pmath_t     set,
  pmath_t     default_options,
  pmath_t     msg_head, // won't be freed
  const char *msg_tag, // "optx" or NULL
  pmath_t     msg_arg
) {
  if(pmath_is_rule(set)) {
    pmath_t lhs = pmath_expr_get_item(set, 1);
    
    if(!is_option_name_of(lhs, default_options)) {
      if(msg_tag) {
        pmath_message(
          msg_head, msg_tag, 2,
          lhs,
          pmath_ref(msg_arg));
      }
      
      return FALSE;
    }
    
    pmath_unref(lhs);
    return TRUE;
  }
  
  if(pmath_is_expr_of(set, pmath_System_List)) {
    size_t i;
    size_t len = pmath_expr_length(set);
    const pmath_t *items = pmath_expr_read_item_data(set);
    
    if(!items) {
      if(pmath_is_packed_array(set))
        return FALSE;
      
      pmath_debug_print("[pmath_expr_read_item_data() gave NULL in %s:%d]\n", __FILE__, __LINE__);
      return FALSE;
    }
    
    for(i = 0; i < len; ++i) {
      if(!_pmath_options_check_subset_of(items[i], default_options, msg_head, msg_tag, msg_arg))
        return FALSE;
    }
    
    return TRUE;
  }
  
  return FALSE;
}

PMATH_PRIVATE
pmath_t _pmath_options_from_expr(pmath_t expr) {
  if(pmath_is_symbol(expr))
    return get_default_options(expr);
    
  if(pmath_is_expr(expr)) {
    pmath_t item, def;
    size_t i;
    size_t len = pmath_expr_length(expr);
    const pmath_t *items = pmath_expr_read_item_data(expr);
    
    if(!items) {
      if(pmath_is_packed_array(expr))
        return pmath_ref(_pmath_object_emptylist);
  
      pmath_debug_print("[pmath_expr_read_item_data() gave NULL in %s:%d]\n", __FILE__, __LINE__);
      return pmath_ref(_pmath_object_emptylist);
    }
    
    item = pmath_expr_get_item(expr, 0);
    if(pmath_same(item, pmath_System_List)) {
      if(pmath_is_set_of_options(expr)) {
        pmath_unref(item);
        return pmath_ref(expr);
      }
    }
    def = get_default_options(item);
    pmath_unref(item);
    
    for(i = len; i > 0; --i) {
      if(!_pmath_options_check_subset_of(items[i - 1], def, PMATH_NULL, NULL, PMATH_NULL))
        break;
    }
    ++i;
    
    pmath_unref(def);
    
    if(i > len)
      return pmath_ref(_pmath_object_emptylist);
      
    item = pmath_expr_get_item_range(expr, i, SIZE_MAX);
    item = pmath_expr_set_item(item, 0, pmath_ref(pmath_System_List));
    return item;
  }
  
  return pmath_ref(_pmath_object_emptylist);
}

PMATH_API
pmath_bool_t pmath_is_set_of_options(pmath_t expr) {
  size_t i;
  
  if(!pmath_is_expr(expr))
    return FALSE;
    
  if(pmath_is_rule(expr))
    return TRUE;
    
  if(!pmath_is_expr_of(expr, pmath_System_List))
    return FALSE;
    
  for(i = pmath_expr_length(expr); i > 0; --i) {
    pmath_t o = pmath_expr_get_item(expr, i);
    
    if(!pmath_is_set_of_options(o)) {
      pmath_unref(o);
      return FALSE;
    }
    
    pmath_unref(o);
  }
  
  return TRUE;
}

PMATH_API pmath_expr_t pmath_options_extract_ex(
  pmath_expr_t                  expr, 
  size_t                        last_nonoption, 
  pmath_options_extragt_flags_t flags
) {
  pmath_t option_set;
  size_t i, exprlen;
  const pmath_t *items;
  pmath_t head;
  
  exprlen = pmath_expr_length(expr);
  if(last_nonoption > exprlen) {
    pmath_message_argxxx(exprlen, last_nonoption, last_nonoption);
    return PMATH_NULL;
  }
  
  if(last_nonoption == exprlen)
    return pmath_ref(_pmath_object_emptylist);
    
  items = pmath_expr_read_item_data(expr);
  if(!items) {
    pmath_debug_print("[pmath_expr_read_item_data() gave NULL in %s:%d]\n", __FILE__, __LINE__);
    return pmath_ref(_pmath_object_emptylist);
  }
  
  head = pmath_expr_get_item(expr, 0);
  
  for(i = last_nonoption; i < exprlen; ++i) {
    if(!pmath_is_set_of_options(items[i])) {
      pmath_message(
        head, "nonopt", 3,
        pmath_ref(items[i]),
        pmath_integer_new_uiptr(last_nonoption),
        pmath_ref(expr));
      
      pmath_unref(head);
      return PMATH_NULL;
    }
  }
  
  if(!(flags & PMATH_OPTIONS_EXTRACT_UNKNOWN_QUIET)) {
    option_set = get_default_options(head);
    
    for(i = last_nonoption; i < exprlen; ++i) {
      if(!_pmath_options_check_subset_of(items[i], option_set, head, "optx", expr)) {
        if(flags & PMATH_OPTIONS_EXTRACT_UNKNOWN_WARNONLY)
          break;
        
        pmath_unref(option_set);
        pmath_unref(head);
        return PMATH_NULL;
      }
    }
    
    pmath_unref(option_set);
  }
  option_set = pmath_expr_get_item_range(expr, last_nonoption + 1, SIZE_MAX);
  option_set = pmath_expr_set_item(option_set, 0, pmath_ref(pmath_System_List));
  pmath_unref(head);
  return option_set;
}

PMATH_API pmath_expr_t pmath_options_extract(
  pmath_expr_t expr,
  size_t       last_nonoption
) {
  return pmath_options_extract_ex(expr, last_nonoption, PMATH_OPTIONS_EXTRACT_UNKNOWN_FAIL);
}

PMATH_API pmath_t pmath_option_value(
  pmath_t head,
  pmath_t name,
  pmath_t more
) {
  pmath_t value;
  
  value = _pmath_option_find_value(name, more);
  if(!pmath_same(value, PMATH_UNDEFINED))
    return value;
    
  if(pmath_is_null(head))
    head = pmath_current_head();
  else
    head = pmath_ref(head);
    
  if(pmath_is_symbol(head)) {
    pmath_t default_options = get_default_options(head);
    
    value = _pmath_option_find_value(name, default_options);
    if(!pmath_same(value, PMATH_UNDEFINED)) {
      pmath_unref(head);
      pmath_unref(default_options);
      return value;
    }
    
    pmath_unref(default_options);
    pmath_message(
      pmath_System_OptionValue, "optnf", 2,
      pmath_ref(name),
      head);
      
    return pmath_ref(name);
  }
  
  pmath_unref(head);
  
  if(pmath_same(more, PMATH_UNDEFINED))
    more = _pmath_object_emptylist;
    
  pmath_message(
    pmath_System_OptionValue, "optnf", 2,
    pmath_ref(name),
    pmath_ref(more));
    
  return pmath_ref(name);
}
