#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <pmath-config.h>
#include <pmath-types.h>
#include <pmath-core/objects.h>
#include <pmath-core/expressions.h>
#include <pmath-core/numbers.h>
#include <pmath-core/strings.h>
#include <pmath-core/symbols.h>

#include <pmath-util/concurrency/atomic.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-core/objects-inline.h>

#include <pmath-builtins/control/flow-private.h>
#include <pmath-builtins/control-private.h>
#include <pmath-builtins/lists-private.h>
#include <pmath-builtins/all-symbols.h>
#include <pmath-builtins/all-symbols-private.h>

PMATH_PRIVATE
pmath_bool_t _pmath_scan(
  struct _pmath_scan_info_t *info,
  pmath_t             obj, // will be freed
  long                       level
){
  int reldepth = _pmath_object_in_levelspec(
    obj, info->levelmin, info->levelmax, level);
    
  if(reldepth <= 0 && pmath_instance_of(obj, PMATH_TYPE_EXPRESSION)){
    size_t len = pmath_expr_length(obj);
    size_t i;
    
    for(i = info->with_heads ? 0 : 1;i <= len;++i){
      if(_pmath_scan(info, pmath_expr_get_item(obj, i), level + 1)){
        pmath_unref(obj);
        return TRUE;
      }
    }
  }
  
  if(reldepth == 0){
    pmath_unref(info->result);
    
    info->result = pmath_expr_new_extended(
      pmath_ref(info->function), 1,
      obj);
      
    return _pmath_run(&(info->result));
  }
  
  pmath_unref(obj);
  return FALSE;
}

PMATH_PRIVATE pmath_t builtin_scan(pmath_expr_t expr){
/* Scan(list, f, startlevel..endlevel)
   Scan(list, f)    = Scan(list, f, 1..1)
   Scan(list, f, n) = Scan(list, f, n..n)
 */
  struct _pmath_scan_info_t info;
  pmath_expr_t options;
  pmath_t obj;
  size_t last_nonoption;
  size_t len = pmath_expr_length(expr);
  
  if(len < 2 || len > 3){
    pmath_message_argxxx(len, 2, 3);
    return expr;
  }
  
  info.with_heads = FALSE;
  info.result = NULL;
  info.levelmin = 1;
  info.levelmax = 1;
  last_nonoption = 2;
  if(len >= 3){
    pmath_t levels = pmath_expr_get_item(expr, 3);
    
    if(_pmath_extract_levels(levels, &info.levelmin, &info.levelmax)){
      last_nonoption = 3;
    }
    else if(!_pmath_is_rule(levels) && !_pmath_is_list_of_rules(levels)){
      pmath_message(NULL, "level", 1, levels);
      return expr;
    }
    
    pmath_unref(levels);
  }
  
  options = pmath_options_extract(expr, last_nonoption);
  if(!options)
    return expr;
  
  obj = pmath_evaluate(pmath_option_value(NULL, PMATH_SYMBOL_HEADS, options));
  if(obj == PMATH_SYMBOL_TRUE){
    info.with_heads = TRUE;
  }
  else if(obj != PMATH_SYMBOL_FALSE){
    pmath_unref(options);
    pmath_message(
      NULL, "opttf", 2,
      pmath_ref(PMATH_SYMBOL_HEADS),
      obj);
    return expr;
  }
  pmath_unref(obj);
  pmath_unref(options);

  info.function = pmath_expr_get_item(expr, 2);
  
  obj = pmath_expr_get_item(expr, 1);
  pmath_unref(expr);
  if(_pmath_scan(&info, obj, 0)){
    pmath_unref(info.function);
    
    if(pmath_is_expr_of_len(info.result, PMATH_SYMBOL_RETURN, 1)){
      pmath_t r = pmath_expr_get_item(info.result, 1);
      
      pmath_unref(info.result);
      return r;
    }
    return info.result;
  }
  
  pmath_unref(info.function);
  return NULL;
}
