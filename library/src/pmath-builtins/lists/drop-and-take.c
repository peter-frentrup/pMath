#include <pmath-core/numbers.h>
#include <pmath-core/packed-arrays.h>

#include <pmath-util/helpers.h>
#include <pmath-util/memory.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/lists-private.h>

#include <string.h>


extern pmath_symbol_t pmath_System_All;
extern pmath_symbol_t pmath_System_Drop;
extern pmath_symbol_t pmath_System_List;
extern pmath_symbol_t pmath_System_None;
extern pmath_symbol_t pmath_System_Take;

static pmath_bool_t convert_take_positions(pmath_expr_t expr, size_t exprstart, struct _pmath_range_t *pos);
static pmath_bool_t drop(pmath_t *obj, size_t depth, const struct _pmath_range_t *pos_array);
static pmath_expr_t drop_one_level(pmath_expr_t expr, const struct _pmath_urange_t *upos);
static pmath_expr_t make_similar(pmath_expr_t orig, size_t new_len);
static pmath_bool_t take(pmath_t *obj, size_t depth, const struct _pmath_range_t *pos_array);

static pmath_bool_t convert_take_positions(pmath_expr_t expr, size_t exprstart, struct _pmath_range_t *pos) { // expr wont be freed
  size_t i, exprlen;
  
  exprlen = pmath_expr_length(expr);
  
  for(i = exprstart; i <= exprlen; ++i) {
    pmath_t item = pmath_expr_get_item(expr, i);
    
    if(pmath_same(item, pmath_System_All)) {
      pos[i - exprstart].start = 1;
      pos[i - exprstart].end   = -1;
      pos[i - exprstart].step  = 1;
    }
    else if(pmath_same(item, pmath_System_None)) {
      pos[i - exprstart].start = 1;
      pos[i - exprstart].end   = 0;
      pos[i - exprstart].step  = 1;
    }
    else if(!_pmath_extract_longrange(item, &pos[i - exprstart])) {
      pmath_unref(item);
      pmath_message(PMATH_NULL, "seqs", 2, pmath_integer_new_uiptr(i), pmath_ref(expr));
      return FALSE;
    }
    
    pmath_unref(item);
  }
  
  return TRUE;
}

PMATH_PRIVATE
pmath_bool_t _pmath_convert_start_end_step(struct _pmath_urange_t *upos, const struct _pmath_range_t *pos, size_t length) {
  if(0 <= pos->start && (size_t)pos->start <= length + 1) {
    upos->start = (size_t)pos->start;
  }
  else if(pos->start <= 0 && length >= (size_t)(-pos->start)) {
    upos->start = length + 1 - (size_t) - pos->start;
  }
  else 
    return FALSE;
  
  if(0 <= pos->end && (size_t)pos->end <= length) {
    upos->end = (size_t)pos->end;
  }
  else if(pos->end <= 0 && length + 1 >= (size_t)(-pos->end)) {
    upos->end = length + 1 - (size_t) - pos->end;
  }
  else 
    return FALSE;
  
  if(0 < pos->step) {
    upos->step = (size_t)pos->step;
  }
  else {
    upos->step  = upos->start;
    upos->start = upos->end;
    upos->end   = upos->step;
    
    upos->step = (size_t)(-pos->step);
  }
  
  if(upos->step == 0)
    return FALSE;
  
  if(upos->end < upos->start) {
    if(upos->end + 1 == upos->start)
      return TRUE;
    
    return FALSE;
  }
  
  if(upos->step > 1) { // ensure that start/end are exactly met
    size_t dist = upos->end - upos->start;
    
    dist = (dist / upos->step) * upos->step;
    if(pos->step < 0)
      upos->start = upos->end - dist;
    else
      upos->end = upos->start + dist;
  }
    
  return TRUE;
}

static pmath_bool_t drop(pmath_t *obj, size_t depth, const struct _pmath_range_t *pos_array) {
  struct _pmath_urange_t upos;
  
  if(depth == 0)
    return TRUE;
    
  if( pmath_is_expr(*obj) && _pmath_convert_start_end_step(&upos, pos_array, pmath_expr_length(*obj))) {
    *obj = drop_one_level(*obj, &upos);
    
    if(depth > 1) {
      size_t len = pmath_expr_length(*obj);
      size_t i;
      
      for(i = 1; i <= len; ++i) {
        pmath_t item = pmath_expr_extract_item(*obj, i);
        
        if(!drop(&item, depth - 1, pos_array + 1)) {
          pmath_unref(item);
          return FALSE;
        }
        
        *obj = pmath_expr_set_item(*obj, i, item);
      }
    }
    
    return TRUE;
  }
  
  pmath_message(pmath_System_Drop, "drop", 3,
                pmath_integer_new_slong(pos_array->start),
                pmath_integer_new_slong(pos_array->end),
                pmath_ref(*obj));
                
  return FALSE;
}

static pmath_expr_t drop_one_level(pmath_expr_t expr, const struct _pmath_urange_t *upos) {
  size_t i;
  size_t drop_count;
  size_t old_len = pmath_expr_length(expr);
  pmath_expr_t result;
  
  if(upos->step == 1) {
    if(upos->end == old_len) {
      result = pmath_expr_get_item_range(expr, 1, upos->start - 1);
      pmath_unref(expr);
      return result;
    }
    
    if(upos->start == 1) {
      result = pmath_expr_get_item_range(expr, upos->end + 1, SIZE_MAX);
      pmath_unref(expr);
      return result;
    }
  }
  
  if(upos->end < upos->start)
    return expr;
  
//  for(i = upos->start; i <= upos->end; i += upos->step) {
//    expr = pmath_expr_set_item(expr, i, PMATH_UNDEFINED);
//  }
//  
//  return pmath_expr_remove_all(expr, PMATH_UNDEFINED);
  
  drop_count = (upos->end - upos->start) / upos->step + 1;
  result = make_similar(expr, old_len - drop_count);
  for(i = 1; i < upos->start; ++i)
    result = pmath_expr_set_item(result, i, pmath_expr_get_item(expr, i));
  
  for(i = old_len; i > upos->end; --i)
    result = pmath_expr_set_item(result, i - drop_count, pmath_expr_get_item(expr, i));
  
  if(upos->step > 1) {
    size_t j = upos->start;
    for(i = upos->start; i <= upos->end; ++i) {
      if((i - upos->start) % upos->step) {
        result = pmath_expr_set_item(result, j++, pmath_expr_get_item(expr, i));
      }
    }
    
    assert(j == upos->end + 1 - drop_count);
  }
  
  pmath_unref(expr);
  return result;
}

static pmath_expr_t make_similar(pmath_expr_t orig, size_t new_len) {
  if(pmath_is_packed_array(orig)) {
    size_t        orig_dims  = pmath_packed_array_get_dimensions(orig);
    
    if(orig_dims > 0) {
      const size_t        *orig_sizes = pmath_packed_array_get_sizes(       orig);
      pmath_packed_type_t  elem_type  = pmath_packed_array_get_element_type(orig);
      size_t *sizes;
      pmath_packed_array_t result;
      
      if(new_len == orig_sizes[0]) {
        sizes = (size_t*)orig_sizes;
      }
      else {
        sizes = pmath_mem_alloc(orig_dims * sizeof(size_t));
        if(!sizes)
          return PMATH_NULL;
        
        sizes[0] = new_len;
        memcpy(&sizes[1], &orig_sizes[1], (orig_dims-1) * sizeof(size_t));
      }
      
      result = pmath_packed_array_new(PMATH_NULL, elem_type, orig_dims, sizes, NULL, 0);
      if(sizes != orig_sizes)
        pmath_mem_free(sizes);
      
      return result;
    }
  }
  
  return pmath_expr_new(pmath_expr_get_item(orig, 0), new_len);
}

static pmath_bool_t take(pmath_t *obj, size_t depth, const struct _pmath_range_t *pos_array) {
  if(depth == 0)
    return TRUE;
    
  if(pmath_is_expr(*obj) && _pmath_expr_try_take(obj, pos_array)) {
    size_t i;
    size_t len = pmath_expr_length(*obj);
    
    if(depth > 1) {
      for(i = 1; i <= len; ++i) {
        pmath_t item = pmath_expr_extract_item(*obj, i);
        
        if(!take(&item, depth - 1, pos_array + 1)) {
          pmath_unref(item);
          return FALSE;
        }
        
        *obj = pmath_expr_set_item(*obj, i, item);
      }
    }
    
    return TRUE;
  }
  
  pmath_message(pmath_System_Take, "take", 3,
                pmath_integer_new_slong(pos_array->start),
                pmath_integer_new_slong(pos_array->end),
                pmath_ref(*obj));
                
  return FALSE;
}

PMATH_PRIVATE
pmath_bool_t _pmath_expr_try_take(pmath_expr_t *list, const struct _pmath_range_t *pos) {
  struct _pmath_urange_t upos;
  
  if(_pmath_convert_start_end_step(&upos, pos, pmath_expr_length(*list))) {
    size_t len = (upos.end - upos.start + upos.step) / upos.step;
    size_t i;
    
    pmath_expr_t result;
    
    if(pos->step < 0) {
      result = make_similar(*list, len);
      
      for(i = len; upos.start <= upos.end; upos.start += upos.step, --i) {
        result = pmath_expr_set_item(result, i,
                                     pmath_expr_get_item(*list, upos.start));
      }
    }
    else if(pos->step == 1 && upos.start <= upos.end && upos.start > 0) {
      result = pmath_expr_get_item_range(*list, upos.start, upos.end - upos.start + 1);
    }
    else {
      result = make_similar(*list, len);
      
      for(i = 1; upos.start <= upos.end; upos.start += upos.step, ++i) {
        result = pmath_expr_set_item(result, i,
                                     pmath_expr_get_item(*list, upos.start));
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
  pmath_expr_t                *list,
  pmath_t                      values, // wont be freed
  const struct _pmath_range_t *pos
) {
  struct _pmath_urange_t upos;
  
  if(_pmath_convert_start_end_step(&upos, pos, pmath_expr_length(*list))) {
    size_t len = (upos.end - upos.start + upos.step) / upos.step;
    size_t i;
    
    if(!pmath_is_expr_of_len(values, pmath_System_List, len)) {
      for(i = 1; upos.start <= upos.end; upos.start += upos.step, ++i) {
        *list = pmath_expr_set_item(*list, upos.start,
                                    pmath_ref(values));
      }
    }
    else if(pos->step < 0) {
      for(i = len; upos.start <= upos.end; upos.start += upos.step, --i) {
        *list = pmath_expr_set_item(*list, upos.start,
                                    pmath_expr_get_item(values, i));
      }
    }
    else {
      for(i = 1; upos.start <= upos.end; upos.start += upos.step, ++i) {
        *list = pmath_expr_set_item(*list, upos.start,
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
  struct _pmath_range_t *pos;
  
  depth = pmath_expr_length(expr);
  if(depth == 0) {
    pmath_message_argxxx(0, 1, SIZE_MAX);
    return expr;
  }
  
  --depth;
  pos = pmath_mem_calloc(depth, sizeof(struct _pmath_range_t));
  
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
  struct _pmath_range_t *pos;
  
  depth = pmath_expr_length(expr);
  if(depth == 0) {
    pmath_message_argxxx(0, 1, SIZE_MAX);
    return expr;
  }
  
  --depth;
  pos = pmath_mem_alloc(depth * sizeof(struct _pmath_range_t));
  
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
