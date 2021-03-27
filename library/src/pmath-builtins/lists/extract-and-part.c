#include <pmath-core/expressions-private.h>

#include <pmath-util/dispatch-tables-private.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>
#include <pmath-util/symbol-values-private.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/control/definitions-private.h>
#include <pmath-builtins/lists-private.h>


extern pmath_symbol_t pmath_System_DollarFailed;
extern pmath_symbol_t pmath_System_All;
extern pmath_symbol_t pmath_System_Key;
extern pmath_symbol_t pmath_System_List;
extern pmath_symbol_t pmath_System_Missing;
extern pmath_symbol_t pmath_System_Part;
extern pmath_symbol_t pmath_System_Range;
extern pmath_symbol_t pmath_System_Sequence;

struct assign_part_context_t {
  pmath_expr_t  position;
  size_t        position_start;
  pmath_t       new_value;
  pmath_bool_t *error;
};

static pmath_t assign_part(
  pmath_t       list,            // will be freed
  pmath_expr_t  position,        // wont be freed
  size_t        position_start,
  pmath_t       new_value,       // wont be freed
  pmath_bool_t *error);
static pmath_bool_t check_list_of_rules(pmath_expr_t list); // list won't be freed
static pmath_bool_t modify_rule_rhs(pmath_t *rhs, pmath_bool_t was_no_delay, void *_context); // _context is assign_part_context_t*
static pmath_bool_t part(pmath_expr_t *list, pmath_expr_t position, size_t position_start); // position won't be freed
static pmath_bool_t try_packed_part(pmath_expr_t *array, pmath_expr_t position, size_t position_start); // position won't be freed

PMATH_PRIVATE pmath_t builtin_extract(pmath_expr_t expr) {
  pmath_expr_t list, part_spec;
  size_t exprlen;
  
  exprlen = pmath_expr_length(expr);
  
  if(exprlen < 2 || exprlen > 3) {
    pmath_message_argxxx(exprlen, 2, 3);
    return expr;
  }
  
  part_spec = pmath_expr_get_item(expr, 2);
  
  if(!pmath_is_expr_of(part_spec, pmath_System_List))
    part_spec = pmath_build_value("(o)", part_spec);
    
  list = pmath_expr_get_item(expr, 1);
  if(!part(&list, part_spec, 1)) {
    pmath_unref(list);
    pmath_unref(part_spec);
    return expr;
  }
  
  pmath_unref(part_spec);
  
  if(exprlen == 3) {
    list = pmath_expr_new_extended(
             pmath_expr_get_item(expr, 3), 1,
             list);
  }
  
  pmath_unref(expr);
  return list;
}

PMATH_PRIVATE pmath_t builtin_part(pmath_expr_t expr) {
  pmath_expr_t list;
  
  if(pmath_expr_length(expr) < 1)
    return expr;
    
  list = pmath_expr_get_item(expr, 1);
  if(!part(&list, expr, 2)) {
    pmath_unref(list);
    return expr;
  }
  
  pmath_unref(expr);
  return list;
}

PMATH_PRIVATE pmath_t builtin_assign_part(pmath_expr_t expr) {
  pmath_t tag;
  pmath_t lhs;
  pmath_t rhs;
  pmath_t sym;
  pmath_t list;
  int assignment;
  pmath_bool_t error;
  
  assignment = _pmath_is_assignment(expr, &tag, &lhs, &rhs);
  if(!assignment)
    return expr;
    
  if(!pmath_is_expr(lhs) || pmath_expr_length(lhs) <= 1) {
    pmath_unref(tag);
    pmath_unref(lhs);
    pmath_unref(rhs);
    return expr;
  }
  
  sym = pmath_expr_get_item(lhs, 0);
  pmath_unref(sym);
  
  if(pmath_same(sym, pmath_System_Part)) {
    size_t i;
    
    pmath_unref(expr);
    for(i = pmath_expr_length(lhs); i > 1; --i) {
      pmath_t item = pmath_expr_extract_item(lhs, i);
      item = pmath_evaluate(item);
      lhs = pmath_expr_set_item(lhs, i, item);
    }
  }
  else {
    pmath_unref(tag);
    pmath_unref(lhs);
    pmath_unref(rhs);
    return expr;
  }
  
  if(pmath_same(rhs, PMATH_UNDEFINED)) {
    pmath_t item = pmath_expr_get_item(lhs, pmath_expr_length(lhs));
    
    if(!pmath_is_string(item) && !pmath_is_expr_of(item, pmath_System_Key)) {
      pmath_message(PMATH_NULL, "keydel", 2, item, lhs);
      pmath_unref(tag);
      pmath_unref(lhs);
      return pmath_ref(pmath_System_DollarFailed);
    }
    
    pmath_unref(item);
  }
  
  sym = pmath_expr_get_item(lhs, 1);
  if(!pmath_is_symbol(sym)) {
    pmath_message(PMATH_NULL, "sym", 1, sym,
                  pmath_expr_new_extended(
                    pmath_ref(pmath_System_List), 2,
                    PMATH_FROM_INT32(pmath_same(tag, PMATH_UNDEFINED) ? 1 : 2),
                    PMATH_FROM_INT32(1)));
    pmath_unref(tag);
    pmath_unref(lhs);
    
    if(assignment < 0) {
      pmath_unref(rhs);
      return pmath_ref(pmath_System_DollarFailed);
    }
    
    return rhs;
  }
  
  if( !pmath_same(tag, PMATH_UNDEFINED) &&
      !pmath_same(tag, sym))
  {
    pmath_message(PMATH_NULL, "tag", 3, tag, lhs, sym);
    
    if(assignment < 0) {
      pmath_unref(rhs);
      return pmath_ref(pmath_System_DollarFailed);
    }
    
    return rhs;
  }
  
  pmath_unref(tag);
  list = pmath_symbol_get_value(sym);
  list = _pmath_symbol_value_prepare(sym, list);
  
  lhs = pmath_expr_flatten(
          lhs,
          pmath_ref(pmath_System_Sequence),
          PMATH_EXPRESSION_FLATTEN_MAX_DEPTH);
  error = FALSE;
  list = assign_part(list, lhs, 2, rhs, &error);
  pmath_unref(lhs);
  
  if(error) {
    pmath_unref(list);
    pmath_unref(sym);
    
    if(assignment < 0) {
      pmath_unref(rhs);
      return pmath_ref(pmath_System_DollarFailed);
    }
    
    return rhs;
  }
  
  if(!_pmath_assign(sym, pmath_ref(sym), list)) {
    pmath_unref(sym);
    
    if(assignment < 0) {
      pmath_unref(rhs);
      return pmath_ref(pmath_System_DollarFailed);
    }
    
    return rhs;
  }
  
  pmath_unref(sym);
  if(assignment < 0) {
    pmath_unref(rhs);
    return PMATH_NULL;
  }
  
  return rhs;
}

static pmath_t assign_part(
  pmath_t       list,            // will be freed
  pmath_expr_t  position,        // wont be freed
  size_t        position_start,
  pmath_t       new_value,       // wont be freed
  pmath_bool_t *error
) {
  size_t listlen;
  pmath_t index;
  
  if(position_start > pmath_expr_length(position)) {
    pmath_unref(list);
    return pmath_ref(new_value);
  }
  
  if(!pmath_is_expr(list)) {
    if(!*error)
      pmath_message(PMATH_NULL, "partd", 1, pmath_ref(position));
    *error = TRUE;
    return list;
  }
  
  index = pmath_expr_get_item(position, position_start);
  if(pmath_is_string(index) || pmath_is_expr_of_len(index, pmath_System_Key, 1)) {
    struct assign_part_context_t context;
    
    if(!check_list_of_rules(list)) {
      *error = TRUE;
      return list;
    }
    
    if(pmath_is_expr(index)) { // Key(k)
      pmath_t key = pmath_expr_get_item(index, 1);
      pmath_unref(index);
      index = key;
    }
    
    context.position = position;
    context.position_start = position_start;
    context.new_value = new_value;
    context.error = error;
    return pmath_rules_modify(list, index, modify_rule_rhs, &context);
  }
  
  listlen = pmath_expr_length(list);
  
  if(pmath_is_integer(index)) {
    size_t i = SIZE_MAX;
    
    if(!extract_number(index, listlen, &i)) {
      if(*error)
        pmath_unref(index);
      else
        pmath_message(PMATH_NULL, "pspec", 1, index);
      *error = TRUE;
      return list;
    }
    if(i > listlen) {
      if(*error)
        pmath_unref(index);
      else
        pmath_message(PMATH_NULL, "partw", 2, pmath_ref(list), index);
      *error = TRUE;
      return list;
    }
    
    pmath_unref(index);
    index = pmath_expr_get_item(list, i);
    list = pmath_expr_set_item(list, i, PMATH_NULL);
    return pmath_expr_set_item(list, i,
                               assign_part(index, position, position_start + 1, new_value, error));
  }
  
  if(pmath_is_expr_of(index, pmath_System_List)) {
    size_t i;
    size_t indexlen = pmath_expr_length(index);
    
    for(i = 1; i <= indexlen; ++i) {
      pmath_t subindex = pmath_expr_get_item(index, i);
      size_t list_i = SIZE_MAX;
      
      if(!extract_number(subindex, listlen, &list_i)) {
        if(*error)
          pmath_unref(subindex);
        else
          pmath_message(PMATH_NULL, "pspec", 1, subindex);
        *error = TRUE;
        pmath_unref(index);
        return list;
      }
      
      if(list_i > listlen) {
        if(*error)
          pmath_unref(subindex);
        else
          pmath_message(PMATH_NULL, "partw", 2,
                        pmath_ref(list),
                        subindex);
        *error = TRUE;
        pmath_unref(index);
        return list;
      }
      
      pmath_unref(subindex);
      
      if(pmath_is_expr_of_len(new_value, pmath_System_List, indexlen)) {
        pmath_t item     = pmath_expr_get_item(list,      list_i);
        pmath_t new_item = pmath_expr_get_item(new_value, i);
        
        list = pmath_expr_set_item(list, list_i, PMATH_NULL);
        list = pmath_expr_set_item(list, list_i,
                                   assign_part(item, position, position_start + 1, new_item, error));
                                   
        pmath_unref(new_item);
      }
      else {
        pmath_t item = pmath_expr_get_item(list, list_i);
        
        list = pmath_expr_set_item(list, list_i, PMATH_NULL);
        list = pmath_expr_set_item(list, list_i,
                                   assign_part(item, position, position_start + 1, new_value, error));
      }
    }
    
    pmath_unref(index);
    return list;
  }
  
  if(pmath_same(index, pmath_System_All) || pmath_is_expr_of(index, pmath_System_Range)) {
    pmath_expr_t overlay;
    struct _pmath_range_t range;
    
    // also handles All
    if(!_pmath_extract_longrange(index, &range)) {
      pmath_message(PMATH_NULL, "pspec", 1, index);
      *error = TRUE;
      return list;
    }
    
    if(position_start < pmath_expr_length(position)) {
      size_t i, len;
      
      overlay = pmath_ref(list);
      
      if(!_pmath_expr_try_take(&overlay, &range)) {
        pmath_message(PMATH_NULL, "partw", 2, overlay, index);
        *error = TRUE;
        return list;
      }
      
      len = pmath_expr_length(overlay);
      if(pmath_is_expr_of_len(new_value, pmath_System_List, len)) {
        for(i = 1; i <= len; ++i) {
          pmath_t item     = pmath_expr_extract_item(overlay,  i);
          pmath_t new_item = pmath_expr_get_item(    new_value, i);
          
          item = assign_part(item, position, position_start + 1, new_item, error);
          
          overlay = pmath_expr_set_item(overlay, i, item);
          
          pmath_unref(new_item);
        }
      }
      else {
        for(i = 1; i <= len; ++i) {
          pmath_t item = pmath_expr_extract_item(overlay, i);
          
          item = assign_part(item, position, position_start + 1, new_value, error);
          
          overlay = pmath_expr_set_item(overlay, i, item);
        }
      }
      
      overlay = pmath_expr_set_item(overlay, 0, pmath_ref(pmath_System_List));
    }
    else
      overlay = pmath_ref(new_value);
      
    if(!_pmath_expr_try_overlay(&list, overlay, &range)) {
      pmath_message(PMATH_NULL, "partw", 2, pmath_ref(list), index);
      *error = TRUE;
      pmath_unref(overlay);
      return list;
    }
    
    pmath_unref(overlay);
    pmath_unref(index);
    return list;
  }
  
  *error = TRUE;
  if(*error)
    pmath_unref(index);
  else
    pmath_message(PMATH_NULL, "pspec", 1, index);
    
  return list;
}

static pmath_bool_t check_list_of_rules(pmath_expr_t list) {
  if(!pmath_is_list_of_rules(list)) { // not a list of rules
    pmath_message(PMATH_NULL, "partw", 1, pmath_ref(list));
    return FALSE;
  }
  
  return TRUE;
}

static pmath_bool_t modify_rule_rhs(pmath_t *rhs, pmath_bool_t was_no_delay, void *_context) {
  struct assign_part_context_t *context = (struct assign_part_context_t*)_context;
  
  *rhs = assign_part(*rhs, context->position, context->position_start + 1, context->new_value, context->error);
  
  if(context->position_start == pmath_expr_length(context->position))
    return pmath_is_evaluated(*rhs);
  else
    return was_no_delay;
}

static pmath_bool_t part(
  pmath_expr_t  *list,
  pmath_expr_t   position,       // won't be freed
  size_t         position_start
) {
  pmath_t pos;
  size_t i, listlen, max_position_start;
  
  max_position_start = pmath_expr_length(position);
  
  for(;; ++position_start) {
    if(position_start > max_position_start)
      return TRUE;
      
    if(!pmath_is_expr(*list)) {
      pmath_message(PMATH_NULL, "partd", 1, pmath_ref(position));
      return FALSE;
    }
    
    listlen = pmath_expr_length(*list);
    
    pos = pmath_expr_get_item(position, position_start);
    if(pmath_is_string(pos) || pmath_is_expr_of_len(pos, pmath_System_Key, 1)) {
      pmath_t result;
      
      if(!check_list_of_rules(*list)) {
        pmath_unref(pos);
        return FALSE;
      }
      
      if(pmath_is_expr(pos)) { // Key(k)
        pmath_t key = pmath_expr_get_item(pos, 1);
        pmath_unref(pos);
        pos = key;
      }
      
      result = PMATH_UNDEFINED;
      if(!pmath_rules_lookup(*list, pmath_ref(pos), &result)) {
        pmath_unref(result);
        pmath_unref(*list);
        *list = pmath_expr_new_extended(
                  pmath_ref(pmath_System_Missing), 2,
                  pmath_ref(_pmath_string_keyabsent),
                  pos);
        return TRUE;
      }
      else
        pmath_unref(pos);
      
      pmath_unref(*list);
      *list = result;
      continue;
    }
    
    if(pmath_is_integer(pos)) {
      i = SIZE_MAX;
      if(!extract_number(pos, listlen, &i)) {
        pmath_message(PMATH_NULL, "pspec", 1, pos);
        return FALSE;
      }
      if(i > listlen) {
        pmath_message(PMATH_NULL, "partw", 2, pmath_ref(*list), pos);
        return FALSE;
      }
      
      pmath_unref(pos);
      {
        pmath_expr_t tmp = *list;
        *list = pmath_expr_get_item(tmp, i);
        pmath_unref(tmp);
      }
      
      continue;
    }
    
    break;
    // end-recursion: return part(list, position, position_start + 1);
  }
  
//  if(pmath_is_packed_array(*list)) {
//    if(try_packed_part(list, position, position_start))
//      return TRUE;
//  }
  
  if(pmath_is_expr_of(pos, pmath_System_List)) {
    size_t poslen = pmath_expr_length(pos);
    
    for(i = 1; i <= poslen; ++i) {
      pmath_t subpos = pmath_expr_get_item(pos, i);
      size_t index = SIZE_MAX;
      
      if(!extract_number(subpos, listlen, &index)) {
        pmath_message(PMATH_NULL, "pspec", 1, subpos);
        pmath_unref(pos);
        return FALSE;
      }
      
      if(index > listlen) {
        pmath_message(PMATH_NULL, "partw", 2,
                      pmath_ref(*list),
                      subpos);
        pmath_unref(pos);
        return FALSE;
      }
      
      pmath_unref(subpos);
      
      pos = pmath_expr_set_item(pos, i,
                                pmath_expr_get_item(*list, index));
    }
    pos = pmath_expr_set_item(pos, 0, pmath_expr_get_item(*list, 0));
    pmath_unref(*list);
    *list = pos;
    pos = PMATH_NULL;
  }
  else {
    struct _pmath_range_t range;
    
    // also handles ALL
    if(!_pmath_extract_longrange(pos, &range)) {
      pmath_message(PMATH_NULL, "pspec", 1, pos);
      return FALSE;
    }
    
    if(!_pmath_expr_try_take(list, &range)) {
      pmath_message(PMATH_NULL, "partw", 2, pmath_ref(*list), pos);
      return FALSE;
    }
  }
  
  pmath_unref(pos);
  
  if(position_start < pmath_expr_length(position)) {
    listlen = pmath_expr_length(*list);
    ++position_start;
    
    for(i = 1; i <= listlen; ++i) {
      pmath_expr_t item = pmath_expr_extract_item(*list, i);
      
      if(!part(&item, position, position_start)) {
        pmath_unref(item);
        return FALSE;
      }
      
      *list = pmath_expr_set_item(*list, i, item);
    }
  }
  
  return TRUE;
}

/*
static pmath_bool_t try_packed_part(
  pmath_expr_t  *list,
  pmath_expr_t   position,       // won't be freed
  size_t         position_start
) {
  size_t max_out_dims = pmath_expr_length(position) - position_start + 1;
  size_t in_dims;
  size_t out_dims;
  const size_t *in_sizes;
  const size_t *in_steps;
  size_t *out_sizes;
  size_t i, j;
  
  assert(pmath_is_packed_array(*list));
  
  in_dims = pmath_packed_array_get_dimensions(*list);
  if(in_dims < max_out_dims)
    return FALSE;
  
  in_sizes = pmath_packed_array_get_sizes(*list);
  in_steps = pmath_packed_array_get_steps(*list);
  
  out_dims = 0;
  for(i = 0; i < max_out_dims; ++i) {
    pmath_t index = pmath_expr_get_item(position, position_start + i);
    
    if(pmath_is_integer(index)) {
      // no change
    }
    else if(pmath_same(index, pmath_System_All) || pmath_is_expr_of(index, pmath_System_List) || pmath_is_expr_of(index, pmath_System_Range)) {
      ++out_dims;
    }
    else {
      pmath_unref(index);
      return FALSE;
    }
    
    pmath_unref(index); 
  }
  
  if(out_dims == 0)
    return FALSE;
  
  if(in_dims < out_dims)
    return FALSE;
  
  out_sizes = pmath_mem_calloc(out_dims, sizeof(size_t));
  if(!out_sizes)
    return FALSE;
  
  for(i = 0, j = 0; j < out_dims; ++i) {
    pmath_t index = pmath_expr_get_item(position, position_start + i);
    
    if(pmath_is_integer(index)) {
      // no change
    }
    else if(pmath_same(index, pmath_System_All)) {
      out_sizes[j++] = in_sizes[i];
    }
    else if(pmath_is_expr_of(index, pmath_System_Range)) {
      pmath_unref(index);
      pmath_mem_free(out_sizes);
      return FALSE;
    }
    else {
      assert(pmath_is_expr_of(index, pmath_System_List)); // checked above
      
      out_sizes[j++] = pmath_expr_length(index);
    }
    
    pmath_unref(index); 
  }
  
  pmath_mem_free(out_sizes);
  return FALSE;
}
*/
