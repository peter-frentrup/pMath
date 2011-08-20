#include <pmath-core/numbers-private.h>

#include <pmath-language/patterns-private.h>

#include <pmath-util/emit-and-gather.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/control-private.h>
#include <pmath-builtins/lists-private.h>


struct cases_info_t {
  pmath_bool_t with_heads;
  pmath_t lhs;
  pmath_t rhs;
  long levelmin;
  long levelmax;
  
  size_t count;
};

static pmath_bool_t cases(
  struct cases_info_t *info,
  pmath_t              obj,  // will be freed
  long                 level
) {
  int reldepth = _pmath_object_in_levelspec(
                   obj, info->levelmin, info->levelmax, level);
                   
  if(reldepth <= 0 && pmath_is_expr(obj)) {
    size_t len = pmath_expr_length(obj);
    size_t i;
    
    for(i = info->with_heads ? 0 : 1; i <= len; ++i) {
      if(!cases(info, pmath_expr_get_item(obj, i), level + 1)) {
        pmath_unref(obj);
        return FALSE;
      }
    }
  }
  
  if(reldepth == 0) {
    pmath_t rhs = pmath_ref(info->rhs);
    
    if(_pmath_pattern_match(obj, pmath_ref(info->lhs), &rhs)) {
      info->count--;
      
      if(info->count == 0) {
        pmath_unref(rhs);
        pmath_unref(obj);
        return FALSE;
      }
      
      if(pmath_same(rhs, PMATH_UNDEFINED)) {
        pmath_emit(obj, PMATH_NULL);
      }
      else {
        pmath_unref(obj);
        pmath_emit(rhs, PMATH_NULL);
      }
      
      return 0 < info->count;
    }
    
    pmath_unref(rhs);
  }
  
  pmath_unref(obj);
  return TRUE;
}

PMATH_PRIVATE pmath_t builtin_cases(pmath_expr_t expr) {
  /* Cases(list, pattern->rhs, levelspec, n)
     Cases(list, pattern, levelspec, n)
     Cases(list, patrule, levelspec)    = Cases(list, patrule, levelspec, Infinity)
     Cases(list, patrule)               = Cases(list, patrule, 1, Infinity)
  
     options:
       Heads->False
  
     messages:
       General::innf
       General::level
       General::opttf
   */
  struct cases_info_t info;
  pmath_t options, obj;
  size_t exprlen, last_nonoption;
  
  exprlen = pmath_expr_length(expr);
  
  if(exprlen < 2) {
    pmath_message_argxxx(exprlen, 2, 4);
    return expr;
  }
  
  info.with_heads = FALSE;
  info.levelmin = 1;
  info.levelmax = 1;
  info.count = SIZE_MAX;
  last_nonoption = 2;
  if(exprlen >= 3) {
    pmath_t levels = pmath_expr_get_item(expr, 3);
    
    if(_pmath_extract_levels(levels, &info.levelmin, &info.levelmax)) {
      last_nonoption = 3;
      if(exprlen >= 4) {
        pmath_t n = pmath_expr_get_item(expr, 4);
        
        if(pmath_is_integer(n) && pmath_number_sign(n) >= 0) {
          last_nonoption = 4;
          if(pmath_is_int32(n))
            info.count = (unsigned)PMATH_AS_INT32(n);
        }
        else if(!pmath_equals(n, _pmath_object_infinity)
                && !_pmath_is_rule(n)
                && !_pmath_is_list_of_rules(n)) {
          pmath_unref(n);
          pmath_unref(levels);
          pmath_message(PMATH_NULL, "innf", 2, PMATH_FROM_INT32(4), pmath_ref(expr));
          return expr;
        }
        
        pmath_unref(n);
      }
    }
    else if(!_pmath_is_rule(levels) && !_pmath_is_list_of_rules(levels)) {
      pmath_message(PMATH_NULL, "level", 1, levels);
      return expr;
    }
    
    pmath_unref(levels);
  }
  
  
  options = pmath_options_extract(expr, last_nonoption);
  if(pmath_is_null(options))
    return expr;
    
  obj = pmath_evaluate(pmath_option_value(PMATH_NULL, PMATH_SYMBOL_HEADS, options));
  if(pmath_same(obj, PMATH_SYMBOL_TRUE)) {
    info.with_heads = TRUE;
  }
  else if(!pmath_same(obj, PMATH_SYMBOL_FALSE)) {
    pmath_unref(options);
    pmath_message(
      PMATH_NULL, "opttf", 2,
      pmath_ref(PMATH_SYMBOL_HEADS),
      obj);
    return expr;
  }
  pmath_unref(obj);
  pmath_unref(options);
  
  
  info.lhs = pmath_expr_get_item(expr, 2);
  info.rhs = PMATH_UNDEFINED;
  if(_pmath_is_rule(info.lhs)) {
    info.rhs = pmath_expr_get_item(info.lhs, 2);
    obj = pmath_expr_get_item(info.lhs, 1);
    pmath_unref(info.lhs);
    info.lhs = obj;
  }
  
  obj = pmath_expr_get_item(expr, 1);
  pmath_unref(expr);
  pmath_gather_begin(PMATH_NULL);
  cases(&info, obj, 0);
  pmath_unref(info.lhs);
  pmath_unref(info.rhs);
  return pmath_gather_end();
}
