#include <pmath-core/numbers.h>

#include <pmath-util/memory.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/lists-private.h>


struct sequence_t{
  long start;
  long end;
  long step;
};

static pmath_bool_t convert_take_positions(
  pmath_expr_t        expr, // wont be freed
  size_t              exprstart,
  struct sequence_t  *pos
){
  size_t i, exprlen;
  
  exprlen = pmath_expr_length(expr);
  
  for(i = exprstart;i <= exprlen;++i){
    pmath_t item = pmath_expr_get_item(expr, i);
    
    if(pmath_same(item, PMATH_SYMBOL_ALL)){
      pos[i - exprstart].start = 1;
      pos[i - exprstart].end   = -1;
      pos[i - exprstart].step  = 1;
    }
    else if(pmath_same(item, PMATH_SYMBOL_NONE)){
      pos[i - exprstart].start = 1;
      pos[i - exprstart].end   = 0;
      pos[i - exprstart].step  = 1;
    }
    else if(!_pmath_extract_longrange(
        item, 
        &pos[i - exprstart].start,
        &pos[i - exprstart].end,
        &pos[i - exprstart].step)
    ){
      pmath_unref(item);
      pmath_message(PMATH_NULL, "seqs", 2, pmath_integer_new_size(i), pmath_ref(expr));
      return FALSE;
    }
    
    pmath_unref(item);
  }
  
  return TRUE;
}

static pmath_bool_t convert_start_end_step(
  pmath_expr_t        obj, // wont be freed
  struct sequence_t  *pos,
  size_t             *start,
  size_t             *end,
  size_t             *step
){
  size_t len = pmath_expr_length(obj);
  
  if(0 <= pos->start && (size_t)pos->start <= len){
    *start = (size_t)pos->start;
  }
  else if(pos->start <= 0 && len >= (size_t)-pos->start){
    *start = len + 1 - (size_t)-pos->start;
  }
  else
    return FALSE;
  
  if(0 <= pos->end && (size_t)pos->end <= len){
    *end = (size_t)pos->end;
  }
  else if(pos->end <= 0 && len >= (size_t)-pos->end){
    *end = len + 1 - (size_t)-pos->end;
  }
  else
    return FALSE;
  
  if(0 < pos->step){
    *step = (size_t)pos->step;
  }
  else{
    *step  = *start;
    *start = *end;
    *end   = *step;
    
    *step = (size_t)-pos->step;
  }
  
  return (*start <= *end || *end == 0) && *step > 0;
}

static pmath_bool_t drop(
  pmath_t            *obj,
  size_t              depth,
  struct sequence_t  *pos
){
  size_t start, end, step;
  
  if(depth == 0)
    return TRUE;
  
  if(pmath_is_expr(*obj)
  && convert_start_end_step(*obj, pos, &start, &end, &step)){
    size_t i;
    
    for(i = 1;start <= end;start+= step,++i){
      *obj = pmath_expr_set_item(*obj, i, PMATH_UNDEFINED);
    }
    
    *obj = pmath_expr_remove_all(*obj, PMATH_UNDEFINED);
    
    if(depth > 1){
      size_t len = pmath_expr_length(*obj);
      
      for(i = 1;i <= len;++i){
        pmath_t item = pmath_expr_get_item(*obj, i);
        *obj = pmath_expr_set_item(*obj, i, PMATH_NULL);
        
        if(!drop(&item, depth - 1, pos + 1)){
          pmath_unref(item);
          return FALSE;
        }
        
        *obj = pmath_expr_set_item(*obj, i, item);
      }
    }
    
    return TRUE;
  }
  
  pmath_message(PMATH_SYMBOL_DROP, "drop", 3, 
    pmath_integer_new_size(pos->start),
    pmath_integer_new_size(pos->end),
    pmath_ref(*obj));
    
  return FALSE;
}

static pmath_bool_t take(
  pmath_t            *obj,
  size_t              depth,
  struct sequence_t  *pos
){
  size_t start, end, step;
  
  if(depth == 0)
    return TRUE;
  
  if(pmath_is_expr(*obj)
  && convert_start_end_step(*obj, pos, &start, &end, &step)){
    pmath_expr_t result;
    size_t len = (end - start + step) / step;
    size_t i;
    
    result = pmath_expr_new(pmath_expr_get_item(*obj, 0), len);
    
    for(i = 1;start <= end;start+= step,++i){
      result = pmath_expr_set_item(result, i, 
        pmath_expr_get_item(*obj, start));
    }
    
    pmath_unref(*obj);
    *obj = result;
    
    if(depth > 1){
      for(i = 1;i <= len;++i){
        pmath_t item = pmath_expr_get_item(result, i);
        result = pmath_expr_set_item(result, i, PMATH_NULL);
        
        if(!take(&item, depth - 1, pos + 1)){
          pmath_unref(item);
          return FALSE;
        }
        
        result = pmath_expr_set_item(result, i, item);
      }
    }
    
    return TRUE;
  }
  
  pmath_message(PMATH_SYMBOL_TAKE, "take", 3, 
    pmath_integer_new_si(pos->start),
    pmath_integer_new_si(pos->end),
    pmath_ref(*obj));
    
  return FALSE;
}

PMATH_PRIVATE pmath_t builtin_drop(pmath_expr_t expr){
  pmath_t item;
  size_t depth;
  struct sequence_t *pos;
  
  depth = pmath_expr_length(expr);
  if(depth == 0){
    pmath_message_argxxx(0, 1, SIZE_MAX);
    return expr;
  }
  
  --depth;
  pos = pmath_mem_alloc(depth * sizeof(struct sequence_t));
  
  item = pmath_expr_get_item(expr, 1);
  
  if(pos 
  && convert_take_positions(expr, 2, pos)
  && drop(&item, depth, pos)){
    pmath_unref(expr);
    pmath_mem_free(pos);
    return item;
  }
  
  pmath_unref(item);
  pmath_mem_free(pos);
  return expr;
}

PMATH_PRIVATE pmath_t builtin_take(pmath_expr_t expr){
  pmath_t item;
  size_t depth;
  struct sequence_t *pos;
  
  depth = pmath_expr_length(expr);
  if(depth == 0){
    pmath_message_argxxx(0, 1, SIZE_MAX);
    return expr;
  }
  
  --depth;
  pos = pmath_mem_alloc(depth * sizeof(struct sequence_t));
  
  item = pmath_expr_get_item(expr, 1);
  
  if(pos 
  && convert_take_positions(expr, 2, pos)
  && take(&item, depth, pos)){
    pmath_unref(expr);
    pmath_mem_free(pos);
    return item;
  }
  
  pmath_unref(item);
  pmath_mem_free(pos);
  return expr;
}
