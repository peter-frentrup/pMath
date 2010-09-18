#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/control-private.h>
#include <pmath-builtins/lists-private.h>

struct apply_info_t{
  pmath_bool_t with_heads;
  long levelmin;
  long levelmax;
};

static pmath_t apply(
  struct apply_info_t *info,
  pmath_t              f,    // wont be freed
  pmath_t              list, // will be freed
  long                 level
){
  if(pmath_instance_of(list, PMATH_TYPE_EXPRESSION)){
    int reldepth = _pmath_object_in_levelspec(
      list, info->levelmin, info->levelmax, level);
      
    if(reldepth == 0)
      list = pmath_expr_set_item(list, 0, pmath_ref(f));
    
    if(reldepth <= 0){
      size_t i, len;
      
      len = pmath_expr_length(list);
      
      for(i = info->with_heads ? 0 : 1;i <= len;--i){
        list = pmath_expr_set_item(
          list, i,
          apply(
            info,
            f, 
            pmath_expr_get_item(list, i),
            level + 1));
      }
    }
  }
  
  return list;
}

PMATH_PRIVATE pmath_t builtin_apply(pmath_expr_t expr){
/* Apply(list, f, startlevel..endlevel)
   Apply(list, f, n) = Apply(list, f, n..n)
   Apply(list, f)    = Apply(list, f, 0..0)
   
   f @ list = Apply(list, f)
   
   options:
     Heads->False
   
   messages:
     General::level
     General::opttf
 */
  pmath_t f, list;
  pmath_expr_t options;
  size_t exprlen, last_nonoption;
  struct apply_info_t info;
  
  exprlen = pmath_expr_length(expr);
  if(exprlen < 2 || exprlen > 3){
    pmath_message_argxxx(exprlen, 2, 3);
    return expr;
  }
  
  last_nonoption = 2;
  info.with_heads = FALSE;
  info.levelmin = 0;
  info.levelmax = 0;
  if(exprlen == 3){
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
  f = pmath_evaluate(pmath_option_value(NULL, PMATH_SYMBOL_HEADS, options));
  if(f == PMATH_SYMBOL_TRUE){
    info.with_heads = TRUE;
  }
  else if(f != PMATH_SYMBOL_FALSE){
    pmath_unref(options);
    pmath_message(
      NULL, "opttf", 2,
      pmath_ref(PMATH_SYMBOL_HEADS),
      f);
    return expr;
  }
  pmath_unref(f);
  pmath_unref(options);

  f =    pmath_expr_get_item(expr, 2);
  list = pmath_expr_get_item(expr, 1);
  pmath_unref(expr);
  
  list = apply(&info, f, list, 0);
  pmath_unref(f);
  return list;
}
