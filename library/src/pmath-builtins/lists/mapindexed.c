#include <pmath-core/numbers.h>

#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/control-private.h>
#include <pmath-builtins/lists-private.h>


PMATH_PRIVATE
pmath_t _pmath_map_indexed(
  struct _pmath_map_info_t *info,
  pmath_t                   obj,  // will be freed
  pmath_expr_t              index // will be freed
) {
  size_t level = pmath_expr_length(index);
  int reldepth = _pmath_object_in_levelspec(
                   obj, 
                   info->levelmin, 
                   info->levelmax, 
                   (long)level);
                   
  if(reldepth <= 0 && pmath_is_expr(obj)) {
    size_t len = pmath_expr_length(obj);
    size_t i;
    
    index = pmath_expr_resize(index, level + 1);
    
    for(i = info->with_heads ? 0 : 1; i <= len; ++i) {
      index = pmath_expr_set_item(index, level + 1, pmath_integer_new_uiptr(i));
      
      obj = pmath_expr_set_item(
        obj, i,
        _pmath_map_indexed(
          info,
          pmath_expr_get_item(obj, i),
          pmath_ref(index)));
    }
  }
  
  if(reldepth == 0) {
    index = pmath_expr_resize(index, level);
    
    return pmath_expr_new_extended(
             pmath_ref(info->function), 2,
             obj,
             index);
  }
  
  pmath_unref(index);
  return obj;
}

PMATH_PRIVATE pmath_t builtin_mapindexed(pmath_expr_t expr) {
  /* MapIndexed(list, f, startlevel..endlevel)
     MapIndexed(list, f)    = MapIndexed(list, f, 1..1)
     MapIndexed(list, f, n) = MapIndexed(list, f, n..n)
  
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
  
  if(len < 2) {
    pmath_message_argxxx(len, 2, 3);
    return expr;
  }
  
  info.with_heads = FALSE;
  info.levelmin = 1;
  info.levelmax = 1;
  last_nonoption = 2;
  if(len >= 3) {
    pmath_t levels = pmath_expr_get_item(expr, 3);
    
    if(_pmath_extract_levels(levels, &info.levelmin, &info.levelmax)) {
      last_nonoption = 3;
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
  
  info.function = pmath_expr_get_item(expr, 2);
  
  obj = pmath_expr_get_item(expr, 1);
  pmath_unref(expr);
  obj = _pmath_map_indexed(
    &info, 
    obj, 
    pmath_expr_new(pmath_ref(PMATH_SYMBOL_LIST), 0));
  pmath_unref(info.function);
  return obj;
}
