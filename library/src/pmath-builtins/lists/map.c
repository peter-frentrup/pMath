#include <pmath-core/expressions-private.h>

#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>
#include <pmath-util/option-helpers.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/control/definitions-private.h>
#include <pmath-builtins/lists-private.h>


static pmath_bool_t can_evaluate_all_args_for(pmath_t head) {
  pmath_symbol_t            sym;
  pmath_symbol_attributes_t att;
  
  if(pmath_is_symbol(head)) {
    att = pmath_symbol_get_attributes(head);
    
    return (att & (PMATH_SYMBOL_ATTRIBUTE_HOLDALL | PMATH_SYMBOL_ATTRIBUTE_HOLDALLCOMPLETE)) == 0;
  }
  
  if(pmath_is_expr_of(head, PMATH_SYMBOL_FUNCTION)) {
    pmath_t att_spec;
    if(pmath_expr_length(head) < 3)
      return TRUE;
      
    att_spec = pmath_expr_get_item(head, 3);
    
    if(!_pmath_get_attributes(&att, att_spec))
      return FALSE;
      
    pmath_unref(att_spec);
    return (att & (PMATH_SYMBOL_ATTRIBUTE_HOLDALL | PMATH_SYMBOL_ATTRIBUTE_HOLDALLCOMPLETE)) == 0;
  }
  
  sym = _pmath_topmost_symbol(head);
  att = pmath_symbol_get_attributes(sym);
  pmath_unref(sym);
  
  return (att & PMATH_SYMBOL_ATTRIBUTE_DEEPHOLDALL) == 0;
}


struct map_impl_t {
  struct _pmath_map_info_t info;
  
  pmath_bool_t function_holds_args;
};

static pmath_t map_impl(
  struct map_impl_t *info,
  pmath_t            obj, // will be freed
  long               level,
  pmath_bool_t       evaluate_immediately);


struct map_impl_callback_t {
  struct map_impl_t *info;
  
  long         next_level;
  pmath_bool_t next_eval_imm;
  pmath_bool_t has_sequence;
};

static pmath_t map_impl_callback(pmath_t obj, size_t i, void *data) {
  struct map_impl_callback_t *context = data;
  
  obj = map_impl(context->info, obj, context->next_level, context->next_eval_imm);
  
  if(!context->has_sequence) {
    if(pmath_is_expr_of(obj, PMATH_SYMBOL_SEQUENCE))
      context->has_sequence = TRUE;
  }
  
  return obj;
}


static pmath_t map_impl(
  struct map_impl_t *info,
  pmath_t            obj, // will be freed
  long               level,
  pmath_bool_t       evaluate_immediately
) {
  struct map_impl_callback_t context;
  int reldepth = _pmath_object_in_levelspec(
                   obj, info->info.levelmin, info->info.levelmax, level);
                   
  if(reldepth > 0)
    return obj;
    
  context.info          = info;
  context.next_level    = level + 1;
  context.next_eval_imm = evaluate_immediately;
  context.has_sequence  = FALSE;
  
  if(reldepth == 0 && info->function_holds_args)
    context.next_eval_imm = FALSE;
    
  if(info->info.levelmax < 0 || level < info->info.levelmax) {
    if(pmath_is_expr(obj)) {
      pmath_bool_t update_only_result;
      
      if(info->info.with_heads) {
        pmath_t head = pmath_expr_extract_item(obj, 0);
        
        head = map_impl(info, head, level + 1, context.next_eval_imm);
        
        if(context.next_eval_imm)
          context.next_eval_imm = can_evaluate_all_args_for(head);
          
        update_only_result = pmath_same(head, PMATH_SYMBOL_LIST);
        obj = pmath_expr_set_item(obj, 0, head);
      }
      else {
        pmath_t head = pmath_expr_get_item(obj, 0);
        update_only_result = pmath_same(head, PMATH_SYMBOL_LIST);
        pmath_unref(head);
      }
      
      obj = _pmath_expr_map(
              obj,
              1,
              SIZE_MAX,
              map_impl_callback,
              &context);
              
      if(context.next_eval_imm) {
        if(!context.has_sequence && update_only_result)
          _pmath_expr_update(obj);
        else
          obj = pmath_evaluate(obj);
      }
    }
  }
  
  if(reldepth == 0) {
    obj = pmath_expr_new_extended(
            pmath_ref(info->info.function), 1,
            obj);
            
    if(evaluate_immediately)
      obj = pmath_evaluate(obj);
  }
  
  return obj;
}


PMATH_PRIVATE
pmath_t _pmath_map_eval(
  struct _pmath_map_info_t *info,
  pmath_t                   obj, // will be freed
  long                      level
) {
  struct map_impl_t impl_info;
  
  int reldepth = _pmath_object_in_levelspec(
                   obj, info->levelmin, info->levelmax, level);
                   
  if(reldepth > 0)
    return pmath_evaluate(obj);
    
  impl_info.info = *info;
  impl_info.function_holds_args = !can_evaluate_all_args_for(info->function);
  
  return map_impl(&impl_info, obj, level, TRUE);
}

PMATH_PRIVATE pmath_t builtin_map(pmath_expr_t expr) {
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
  
  if(len < 2) {
    pmath_message_argxxx(len, 2, 3);
    return expr;
  }
  
  info.with_heads = FALSE;
  info.levelmin   = 1;
  info.levelmax   = 1;
  last_nonoption  = 2;
  if(len >= 3) {
    pmath_t levels = pmath_expr_get_item(expr, 3);
    
    if(_pmath_extract_levels(levels, &info.levelmin, &info.levelmax)) {
      last_nonoption = 3;
    }
    else if(!pmath_is_set_of_options(levels)) {
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
  obj = _pmath_map_eval(&info, obj, 0);
  pmath_unref(info.function);
  return obj;
}
