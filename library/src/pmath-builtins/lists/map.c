#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/control-private.h>
#include <pmath-builtins/lists-private.h>


PMATH_PRIVATE 
pmath_t _pmath_map(
  struct _pmath_map_info_t *info,
  pmath_t  obj, // will be freed
  long     level
){
  int reldepth = _pmath_object_in_levelspec(
    obj, info->levelmin, info->levelmax, level);
  
  if(reldepth <= 0 && pmath_is_expr(obj)){
    size_t len = pmath_expr_length(obj);
    size_t i;
    
    for(i = info->with_heads ? 0 : 1;i <= len;++i){
      obj = pmath_expr_set_item(obj, i,
        _pmath_map(
          info,
          pmath_expr_get_item(obj, i),
          level + 1));
    }
  }
  
  if(reldepth == 0){
    return pmath_expr_new_extended(
      pmath_ref(info->function), 1,
      obj);
  }
  
  return obj;
}

PMATH_PRIVATE pmath_t builtin_map(pmath_expr_t expr){
/* Map(list, f, startlevel..endlevel)
   Map(list, f)    = Map(list, f, 1..1)
   Map(list, f, n) = Map(list, f, n..n)
   
   options:
     Heads->False
   
   messages:
     General::level
     General::opttf
 */
  struct _pmath_map_info_t info;
  pmath_expr_t options;
  pmath_t obj;
  size_t last_nonoption;
  size_t len = pmath_expr_length(expr);
  
  if(len < 2){
    pmath_message_argxxx(len, 2, 3);
    return expr;
  }
  
  info.with_heads = FALSE;
  info.levelmin = 1;
  info.levelmax = 1;
  last_nonoption = 2;
  if(len >= 3){
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

  info.function = pmath_expr_get_item(expr, 2);
  
  obj = pmath_expr_get_item(expr, 1);
  pmath_unref(expr);
  obj = _pmath_map(&info, obj, 0);
  pmath_unref(info.function);
  return obj;
}
