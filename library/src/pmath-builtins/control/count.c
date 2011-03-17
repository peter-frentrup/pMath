#include <pmath-core/numbers.h>

#include <pmath-language/patterns-private.h>

#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/control-private.h>
#include <pmath-builtins/lists-private.h>

struct count_info_t{
  pmath_bool_t with_heads;
  pmath_t lhs;
  long levelmin;
  long levelmax;
  
  size_t count;
};

static void count_matches(
  struct count_info_t *info, 
  pmath_t              obj,  // will be freed
  long                 level
){
  int reldepth = _pmath_object_in_levelspec(
    obj, info->levelmin, info->levelmax, level);
  
  if(reldepth <= 0 && pmath_is_expr(obj)){
    size_t len = pmath_expr_length(obj);
    size_t i;
    
    for(i = info->with_heads ? 0 : 1;i <= len;++i){
      count_matches(info, pmath_expr_get_item(obj, i), level + 1);
    }
  }
  
  if(reldepth == 0){
    pmath_t rhs = PMATH_NULL;
    
    if(_pmath_pattern_match(obj, pmath_ref(info->lhs), &rhs)){
      info->count++;
    }
    
    pmath_unref(rhs);
  }
  
  pmath_unref(obj);
}

PMATH_PRIVATE pmath_t builtin_count(pmath_expr_t expr){
/* Count(list, pattern, levelspec)    = Cases(list, pattern, levelspec)
   Count(list, pattern)               = Cases(list, pattern, 1)
   
   options:
     Heads->False
   
   messages:
     General::innf
     General::level
     General::opttf
 */
  struct count_info_t info;
  pmath_t options, obj;
  size_t exprlen, last_nonoption;
  
  exprlen = pmath_expr_length(expr);
  
  if(exprlen < 2){
    pmath_message_argxxx(exprlen, 2, 3);
    return expr;
  }
  
  info.with_heads = FALSE;
  info.levelmin = 1;
  info.levelmax = 1;
  info.count = 0;
  last_nonoption = 2;
  if(exprlen >= 3){
    pmath_t levels = pmath_expr_get_item(expr, 3);
    
    if(_pmath_extract_levels(levels, &info.levelmin, &info.levelmax)){
      last_nonoption = 3;
    }
    else if(!_pmath_is_rule(levels) && !_pmath_is_list_of_rules(levels)){
      pmath_message(PMATH_NULL, "level", 1, levels);
      return expr;
    }
    
    pmath_unref(levels);
  }
  
  
  options = pmath_options_extract(expr, last_nonoption);
  if(pmath_is_null(options))
    return expr;
  
  obj = pmath_evaluate(pmath_option_value(PMATH_NULL, PMATH_SYMBOL_HEADS, options));
  if(pmath_same(obj, PMATH_SYMBOL_TRUE)){
    info.with_heads = TRUE;
  }
  else if(!pmath_same(obj, PMATH_SYMBOL_FALSE)){
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
  
  obj = pmath_expr_get_item(expr, 1);
  pmath_unref(expr);
  count_matches(&info, obj, 0);
  pmath_unref(info.lhs);
  return pmath_integer_new_uiptr(info.count);
}
