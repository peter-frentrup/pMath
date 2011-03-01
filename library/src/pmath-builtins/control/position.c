#include <pmath-core/numbers-private.h>

#include <pmath-language/patterns-private.h>

#include <pmath-util/emit-and-gather.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/control-private.h>
#include <pmath-builtins/lists-private.h>

#include <limits.h>

struct index_t{
  struct index_t *prev;
  size_t i;
};

struct position_info_t{
  pmath_bool_t with_heads; // currently not set (allways FALSE)
  long levelmin;
  long levelmax;
  size_t max;
};

static pmath_bool_t emit_pattern_position( // return = search more?
  struct index_t         *prev,
  struct position_info_t *info,
  pmath_t                 obj,     // will be freed
  pmath_t                 pattern, // wont be freed
  long                    level
){
  struct index_t index;
  pmath_bool_t more = TRUE;
  int reldepth;
  
  if(info->max == 0){
    pmath_unref(obj);
    return FALSE;
  }
  
  reldepth = _pmath_object_in_levelspec(
    obj, info->levelmin, info->levelmax, level);
  
  if(reldepth > 0){
    pmath_unref(obj);
    return TRUE;
  }
  
  index.prev = prev;
  
  if(pmath_is_expr(obj)){
    size_t len = pmath_expr_length(obj);
    
    for(index.i = info->with_heads ? 0 : 1;index.i <= len && more;index.i++){
      more = emit_pattern_position(
        &index,
        info,
        pmath_expr_get_item(obj, index.i),
        pattern,
        level + 1);
    }
  }
  
  if(more 
  && reldepth == 0
  && _pmath_pattern_match(obj, pmath_ref(pattern), NULL)){
    pmath_t pos;
    size_t len = 0;
    
    struct index_t *i = prev;
    while(i){
      ++len;
      i = i->prev;
    }
    
    pos = pmath_expr_new(pmath_ref(PMATH_SYMBOL_LIST), len);
    i = prev;
    while(i){
      pos = pmath_expr_set_item(
        pos, len,
        pmath_integer_new_size(i->i));
      
      --len;
      i = i->prev;
    }
    
    pmath_emit(pos, NULL);
    
    pmath_unref(obj);
    info->max--;
    return info->max > 0;
  }
  
  pmath_unref(obj);
  return TRUE;
}

PMATH_PRIVATE pmath_t builtin_position(pmath_expr_t expr){
  /* Position(obj, pattern, levelspec, n)
     Position(obj, pattern, levelspec)  = Position(obj, pattern, levelspec, Infinity) 
     Position(obj, pattern)             = Position(obj, pattern, {0, Infinity}, Infinity) 
     
     options:
       Heads->True
     
     messages:
       General::innf
       General::level
   */
  struct position_info_t info;
  size_t last_nonoption, len = pmath_expr_length(expr);
  pmath_expr_t options;
  pmath_t obj, pattern;
  
  if(len < 2){
    pmath_message_argxxx(len, 2, 4);
    return expr;
  }
  
  last_nonoption = 2;
  info.with_heads = TRUE;
  info.levelmin = 0;
  info.levelmax = LONG_MAX;
  info.max = SIZE_MAX;
  if(len > 2){
    pmath_t levels = pmath_expr_get_item(expr, 3);
    
    if(_pmath_extract_levels(levels, &info.levelmin, &info.levelmax)){
      last_nonoption = 3;
      
      if(len > 3){
        obj = pmath_expr_get_item(expr, 4);
        
        if(pmath_is_integer(obj)
        && pmath_number_sign(obj) >= 0){
          last_nonoption = 4;
          if(pmath_integer_fits_ui(obj))
            info.max = pmath_integer_get_ui(obj);
          else
            info.max = SIZE_MAX;
        }
        else if(pmath_equals(obj, _pmath_object_infinity)){
          last_nonoption = 4;
          info.max = SIZE_MAX;
        }
        else if(!_pmath_is_rule(obj) && !_pmath_is_list_of_rules(obj)){
          pmath_unref(obj);
          pmath_unref(levels);
          pmath_message(NULL, "innf", 2, pmath_integer_new_si(4), pmath_ref(expr));
          return expr;
        }
        
        pmath_unref(obj);
      }
    }
    else if(!_pmath_is_rule(levels) && !_pmath_is_list_of_rules(levels)){
      pmath_message(NULL, "level", 1, levels);
      return expr;
    }
    
    pmath_unref(levels);
  }
  
  pattern = pmath_expr_get_item(expr, 2);
  
  options = pmath_options_extract(expr, last_nonoption);
  if(!options){
    pmath_unref(pattern);
    return expr;
  }
  
  obj = pmath_evaluate(pmath_option_value(NULL, PMATH_SYMBOL_HEADS, options));
  if(pmath_same(obj, PMATH_SYMBOL_TRUE)){
    info.with_heads = TRUE;
  }
  else if(!pmath_same(obj, PMATH_SYMBOL_FALSE)){
    pmath_unref(pattern);
    pmath_unref(options);
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
  emit_pattern_position(NULL, &info, obj, pattern, 0);
  pmath_unref(pattern);
  return pmath_gather_end();
}
