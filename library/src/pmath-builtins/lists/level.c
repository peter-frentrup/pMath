#include <pmath-core/symbols.h>
#include <pmath-core/numbers.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/memory.h>

#include <assert.h>
#include <limits.h>
#include <string.h>

#include <pmath-util/debug.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-core/objects-private.h>
#include <pmath-core/expressions-private.h>
#include <pmath-core/numbers-private.h>

#include <pmath-builtins/control-private.h>
#include <pmath-builtins/lists-private.h>
#include <pmath-builtins/all-symbols.h>
#include <pmath-builtins/all-symbols-private.h>

PMATH_PRIVATE int _pmath_object_in_levelspec(
  pmath_t obj,
  long levelmin,
  long levelmax,
  long level
){
  long depth;
  
  if(levelmin >= 0){
    if(level < levelmin)
      return -1;
      
    if(levelmax >= 0){
      if(level > levelmax)
        return +1;
      
      return 0;
    }
  }
  else if(levelmax >= 0){
    if(level > levelmax)
      return +1;
  }
  
  depth = _pmath_object_depth(obj);
  
  if(levelmin < 0 && depth > -levelmin)
    return -1;
  
  if(levelmax < 0 && depth < -levelmax)
    return +1;
  
  return 0;
}

PMATH_PRIVATE
pmath_bool_t _pmath_extract_levels(
  pmath_t levelspec,
  long *levelmin,
  long *levelmax
){
  if(pmath_instance_of(levelspec, PMATH_TYPE_INTEGER)
  && pmath_integer_fits_si(levelspec)){
    *levelmin = *levelmax = pmath_integer_get_si(levelspec);
    return TRUE;
  }
  
  if(pmath_is_expr_of_len(levelspec, PMATH_SYMBOL_RANGE, 2)){
    pmath_t obj = pmath_expr_get_item(levelspec, 1);
    
    if(pmath_instance_of(obj, PMATH_TYPE_INTEGER)
    && pmath_integer_fits_si(obj)){
      *levelmin = pmath_integer_get_si(obj);
    }
    else if(!obj){
      *levelmin = 1;
    }
    else{
      pmath_unref(obj);
      return FALSE;
    }
    
    pmath_unref(obj);
    obj = pmath_expr_get_item(levelspec, 2);
    
    if(pmath_instance_of(obj, PMATH_TYPE_INTEGER)
    && pmath_integer_fits_si(obj)){
      *levelmax = pmath_integer_get_si(obj);
    }
    else if(!obj || pmath_equals(obj, _pmath_object_infinity)){
      *levelmax = LONG_MAX;
    }
    else{
      pmath_unref(obj);
      return FALSE;
    }
    
    pmath_unref(obj);
    return TRUE;
  }
  
  return FALSE;
}

struct emit_level_info_t{
  long            levelmin;
  long            levelmax;
  pmath_bool_t with_heads;
};

static void emit_level(
  struct emit_level_info_t  *info,
  pmath_t             obj,      // will be freed
  long                       level
){
  int reldepth = _pmath_object_in_levelspec(
    obj, info->levelmin, info->levelmax, level);
  
  if(reldepth <= 0 && pmath_instance_of(obj, PMATH_TYPE_EXPRESSION)){
    size_t len = pmath_expr_length(obj);
    size_t i;
    
    for(i = info->with_heads ? 0 : 1;i <= len;++i){
      emit_level(
        info,
        pmath_expr_get_item(obj, i),
        level + 1);
    }
  }
  
  if(reldepth == 0)
    pmath_emit(obj, NULL);
  else
    pmath_unref(obj);
}

PMATH_PRIVATE pmath_t builtin_level(pmath_expr_t expr){
/* Level(expr, levelspec, f)
   Level(expr, levelspec)    = Level(expr, levelspec, List)
   
   options:
     Heads->False
    
   messages:
     General::level
     General::opttf
 */
  struct emit_level_info_t info;
  pmath_expr_t options;
  pmath_t obj, head;
  size_t last_nonoption;
  size_t len = pmath_expr_length(expr);
  
  if(len < 2){
    pmath_message_argxxx(len, 2, 3);
    return expr;
  }
  
  info.with_heads = FALSE;
  obj = pmath_expr_get_item(expr, 2);
  if(!_pmath_extract_levels(obj, &info.levelmin, &info.levelmax)){
    pmath_message(NULL, "level", 1, obj);
    return expr;
  }
  pmath_unref(obj);
  
  last_nonoption = 2;
  if(len >= 3){
    head = pmath_expr_get_item(expr, 3);
    
    if(_pmath_is_rule(head) || _pmath_is_list_of_rules(head)){
      pmath_unref(head);
      head = pmath_ref(PMATH_SYMBOL_LIST);
    }
    else
      last_nonoption = 3;
  }
  else
    head = pmath_ref(PMATH_SYMBOL_LIST);
  
  options = pmath_options_extract(expr, last_nonoption);
  if(!options){
    pmath_unref(head);
    return expr;
  }
  
  obj = pmath_evaluate(pmath_option_value(NULL, PMATH_SYMBOL_HEADS, options));
  if(obj == PMATH_SYMBOL_TRUE){
    info.with_heads = TRUE;
  }
  else if(obj != PMATH_SYMBOL_FALSE){
    pmath_unref(options);
    pmath_unref(head);
    pmath_message(
      NULL, "opttf", 2,
      pmath_ref(PMATH_SYMBOL_HEADS),
      obj);
    return expr;
  }
  pmath_unref(obj);
  pmath_unref(options);
  
  obj = pmath_expr_get_item(expr, 1);
  pmath_unref(expr);
  
  pmath_gather_begin(NULL);
  emit_level(&info, obj, 0);
  return pmath_expr_set_item(pmath_gather_end(), 0, head);
}
