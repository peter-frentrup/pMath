#include <pmath-core/numbers.h>

#include <pmath-util/debug.h>
#include <pmath-util/helpers.h>
#include <pmath-util/memory.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/lists-private.h>


struct sequence_t {
  long start;
  long end;
  long step;
};

static pmath_bool_t convert_take_positions(
  pmath_expr_t        expr, // wont be freed
  size_t              exprstart,
  struct sequence_t  *pos
) {
  size_t i, exprlen;
  
  exprlen = pmath_expr_length(expr);
  
  for(i = exprstart; i <= exprlen; ++i) {
    pmath_t item = pmath_expr_get_item(expr, i);
    
    if(pmath_same(item, PMATH_SYMBOL_ALL)) {
      pos[i - exprstart].start = 1;
      pos[i - exprstart].end   = -1;
      pos[i - exprstart].step  = 1;
    }
    else if(pmath_same(item, PMATH_SYMBOL_NONE)) {
      pos[i - exprstart].start = 1;
      pos[i - exprstart].end   = 0;
      pos[i - exprstart].step  = 1;
    }
    else if(!_pmath_extract_longrange(
              item,
              &pos[i - exprstart].start,
              &pos[i - exprstart].end,
              &pos[i - exprstart].step))
    {
      pmath_unref(item);
      pmath_message(PMATH_NULL, "seqs", 2, pmath_integer_new_uiptr(i), pmath_ref(expr));
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
) {
  size_t len = pmath_expr_length(obj);
  
  if(0 <= pos->start && (size_t)pos->start <= len + 1) {
    *start = (size_t)pos->start;
  }
  else if(pos->start <= 0 && len >= (size_t) - pos->start) {
    *start = len + 1 - (size_t) - pos->start;
  }
  else 
    return FALSE;
  
  if(0 <= pos->end && (size_t)pos->end <= len) {
    *end = (size_t)pos->end;
  }
  else if(pos->end <= 0 && len + 1 >= (size_t) - pos->end) {
    *end = len + 1 - (size_t) - pos->end;
  }
  else 
    return FALSE;
  
  if(0 < pos->step) {
    *step = (size_t)pos->step;
  }
  else {
    *step  = *start;
    *start = *end;
    *end   = *step;
    
    *step = (size_t) - pos->step;
  }
  
  if(*step == 1 && *start == *end + 1)
    return TRUE;
    
  return (*start <= *end) && *step > 0;
}

static pmath_bool_t drop(
  pmath_t            *obj,
  size_t              depth,
  struct sequence_t  *pos
) {
  size_t start, end, step;
  
  if(depth == 0)
    return TRUE;
    
  if( pmath_is_expr(*obj) &&
      convert_start_end_step(*obj, pos, &start, &end, &step))
  {
    size_t i;
    
    for(i = start; i <= end; i += step) {
      *obj = pmath_expr_set_item(*obj, i, PMATH_UNDEFINED);
    }
    
    *obj = pmath_expr_remove_all(*obj, PMATH_UNDEFINED);
    
    if(depth > 1) {
      size_t len = pmath_expr_length(*obj);
      
      for(i = 1; i <= len; ++i) {
        pmath_t item = pmath_expr_extract_item(*obj, i);
        
        if(!drop(&item, depth - 1, pos + 1)) {
          pmath_unref(item);
          return FALSE;
        }
        
        *obj = pmath_expr_set_item(*obj, i, item);
      }
    }
    
    return TRUE;
  }
  
  pmath_message(PMATH_SYMBOL_DROP, "drop", 3,
                pmath_integer_new_slong(pos->start),
                pmath_integer_new_slong(pos->end),
                pmath_ref(*obj));
                
  return FALSE;
}

static pmath_bool_t take(
  pmath_t            *obj,
  size_t              depth,
  struct sequence_t  *pos
) {
  if(depth == 0)
    return TRUE;
    
  if( pmath_is_expr(*obj) &&
      _pmath_expr_try_take(obj, pos->start, pos->end, pos->step))
  {
    size_t i;
    size_t len = pmath_expr_length(*obj);
    
    if(depth > 1) {
      for(i = 1; i <= len; ++i) {
        pmath_t item = pmath_expr_extract_item(*obj, i);
        
        if(!take(&item, depth - 1, pos + 1)) {
          pmath_unref(item);
          return FALSE;
        }
        
        *obj = pmath_expr_set_item(*obj, i, item);
      }
    }
    
    return TRUE;
  }
  
  pmath_message(PMATH_SYMBOL_TAKE, "take", 3,
                pmath_integer_new_slong(pos->start),
                pmath_integer_new_slong(pos->end),
                pmath_ref(*obj));
                
  return FALSE;
}

PMATH_PRIVATE
pmath_bool_t _pmath_expr_try_take(
  pmath_expr_t *list,
  long          start,
  long          end,
  long          step
) {
  struct sequence_t pos;
  size_t u_start;
  size_t u_end;
  size_t u_step;
  
  pos.start = start;
  pos.end   = end;
  pos.step  = step;
  
  if(convert_start_end_step(*list, &pos, &u_start, &u_end, &u_step)) {
    size_t len = (u_end - u_start + u_step) / u_step;
    size_t i;
    
    pmath_expr_t result;
    
    if(pos.step < 0) {
      result = pmath_expr_new(pmath_expr_get_item(*list, 0), len);
      
      for(i = len; u_start <= u_end; u_start += u_step, --i) {
        result = pmath_expr_set_item(result, i,
                                     pmath_expr_get_item(*list, u_start));
      }
    }
    else if(pos.step == 1 && u_start <= u_end && u_start > 0) {
      result = pmath_expr_get_item_range(*list, u_start, u_end - u_start + 1);
    }
    else {
      result = pmath_expr_new(pmath_expr_get_item(*list, 0), len);
      
      for(i = 1; u_start <= u_end; u_start += u_step, ++i) {
        result = pmath_expr_set_item(result, i,
                                     pmath_expr_get_item(*list, u_start));
      }
    }
    
    pmath_unref(*list);
    *list = result;
    
    return TRUE;
  }
  
  return FALSE;
}

PMATH_PRIVATE
pmath_bool_t _pmath_expr_try_overlay(
  pmath_expr_t *list,
  pmath_t       values, // wont be freed
  long          start,
  long          end,
  long          step
) {
  struct sequence_t pos;
  size_t u_start;
  size_t u_end;
  size_t u_step;
  
  pos.start = start;
  pos.end   = end;
  pos.step  = step;
  
  if(convert_start_end_step(*list, &pos, &u_start, &u_end, &u_step)) {
    size_t len = (u_end - u_start + u_step) / u_step;
    size_t i;
    
    if(!pmath_is_expr_of_len(values, PMATH_SYMBOL_LIST, len)) {
      for(i = 1; u_start <= u_end; u_start += u_step, ++i) {
        *list = pmath_expr_set_item(*list, u_start,
                                    pmath_ref(values));
      }
    }
    else if(pos.step < 0) {
      for(i = len; u_start <= u_end; u_start += u_step, --i) {
        *list = pmath_expr_set_item(*list, u_start,
                                    pmath_expr_get_item(values, i));
      }
    }
    else {
      for(i = 1; u_start <= u_end; u_start += u_step, ++i) {
        *list = pmath_expr_set_item(*list, u_start,
                                    pmath_expr_get_item(values, i));
      }
    }
    
    return TRUE;
  }
  
  return FALSE;
}

PMATH_PRIVATE pmath_t builtin_drop(pmath_expr_t expr) {
  pmath_t item;
  size_t depth;
  struct sequence_t *pos;
  
  depth = pmath_expr_length(expr);
  if(depth == 0) {
    pmath_message_argxxx(0, 1, SIZE_MAX);
    return expr;
  }
  
  --depth;
  pos = pmath_mem_alloc(depth * sizeof(struct sequence_t));
  
  item = pmath_expr_get_item(expr, 1);
  
  if( pos &&
      convert_take_positions(expr, 2, pos) &&
      drop(&item, depth, pos))
  {
    pmath_unref(expr);
    pmath_mem_free(pos);
    return item;
  }
  
  pmath_unref(item);
  pmath_mem_free(pos);
  return expr;
}

PMATH_PRIVATE pmath_t builtin_take(pmath_expr_t expr) {
  pmath_t item;
  size_t depth;
  struct sequence_t *pos;
  
  depth = pmath_expr_length(expr);
  if(depth == 0) {
    pmath_message_argxxx(0, 1, SIZE_MAX);
    return expr;
  }
  
  --depth;
  pos = pmath_mem_alloc(depth * sizeof(struct sequence_t));
  
  item = pmath_expr_get_item(expr, 1);
  
  if( pos &&
      convert_take_positions(expr, 2, pos) &&
      take(&item, depth, pos))
  {
    pmath_unref(expr);
    pmath_mem_free(pos);
    return item;
  }
  
  pmath_unref(item);
  pmath_mem_free(pos);
  return expr;
}
