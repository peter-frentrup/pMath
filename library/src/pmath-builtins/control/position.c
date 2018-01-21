#include <pmath-core/expressions-private.h>
#include <pmath-core/numbers-private.h>

#include <pmath-language/patterns-private.h>

#include <pmath-util/concurrency/threads.h>
#include <pmath-util/debug.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/memory.h>
#include <pmath-util/messages.h>
#include <pmath-util/option-helpers.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/lists-private.h>

#include <limits.h>


struct index_t {
  struct index_t *next;
  
  size_t i;
};

struct index_lists_t {
  struct index_lists_t *next;
  
  struct index_t       *list;
};

struct position_info_t {
  long levelmin;
  long levelmax;
  size_t max;
  pmath_bool_t with_heads;
  
  pmath_t pattern;
  unsigned pattern_is_const: 1;
};

static void destroy_index(struct index_t *idx) {
  while(idx) {
    struct index_t *next = idx->next;
    pmath_mem_free(idx);
    idx = next;
  }
}

static void destroy_index_lists(struct index_lists_t *ids) {
  while(ids) {
    struct index_lists_t *next = ids->next;
    destroy_index(ids->list);
    pmath_mem_free(ids);
    ids = next;
  }
}

// does not free obj
static pmath_bool_t test_pattern(struct position_info_t *info, pmath_t obj) {
  assert(info != NULL);
  
  if(info->pattern_is_const)
    return pmath_equals(obj, info->pattern);
  else
    return _pmath_pattern_match(obj, pmath_ref(info->pattern), NULL);
}

static pmath_bool_t prepend_index(struct index_lists_t *lists, size_t i) {
  struct index_t *new_ind;
  
  while(lists) {
    new_ind = pmath_mem_alloc(sizeof(struct index_t));
    if(!new_ind)
      return FALSE;
      
    new_ind->next = lists->list;
    new_ind->i    = i;
    lists->list   = new_ind;
    
    lists = lists->next;
  }
  
  return TRUE;
}

static struct index_lists_t **collect_position(
  struct position_info_t  *info,
  struct index_lists_t   **list_end,
  pmath_t                  expr, // wont be freed
  long                     level);

static struct index_lists_t **collect_expr_items_position_slow(
  struct position_info_t  *info,
  struct index_lists_t   **list_end,
  pmath_expr_t             expr, // wont be freed
  long                     level
) {
  size_t i, len;
  
  assert(pmath_is_expr(expr));
  assert(info->levelmax < 0 || level < info->levelmax);
  
  len = pmath_expr_length(expr);
  
  for(i = 1; i <= len; ++i) {
    struct index_lists_t **new_end;
    pmath_t item = pmath_expr_get_item(expr, i);
    
    new_end = collect_position(
                info,
                list_end,
                item,
                level + 1);
                
    pmath_unref(item);
    prepend_index(*list_end, i);
    list_end = new_end;
  }
  
  return list_end;
}

static struct index_lists_t **collect_general_expr_items_position(
  struct position_info_t  *info,
  struct index_lists_t   **list_end,
  const pmath_t           *expr_items,
  size_t                   length,
  long                     level
) {
  size_t i0;
  
  assert(expr_items);
  assert(info->levelmax < 0 || level < info->levelmax);
  
  for(i0 = 0; i0 < length; ++i0) {
    struct index_lists_t **new_end;
    
    new_end = collect_position(
                info,
                list_end,
                expr_items[i0],
                level + 1);
                
    prepend_index(*list_end, i0 + 1);
    list_end = new_end;
  }
  
  return list_end;
}

static struct index_lists_t **collect_position(
  struct position_info_t  *info,
  struct index_lists_t   **list_end,
  pmath_t                  expr, // wont be freed
  long                     level
) {
  int reldepth;
  
  if(info->max == 0 || pmath_aborting())
    return list_end;
    
  reldepth = _pmath_object_in_levelspec(
               expr, info->levelmin, info->levelmax, level);
               
  if(reldepth > 0)
    return list_end;
    
  if(pmath_is_expr(expr)) {
    if(info->levelmax < 0 || level < info->levelmax) {
      const pmath_t *items;
      struct index_lists_t **new_end;
      
      if(info->with_heads) {
        pmath_t head = pmath_expr_get_item(expr, 0);
        
        new_end = collect_position(
                    info,
                    list_end,
                    head,
                    level + 1);
                    
        pmath_unref(head);
        prepend_index(*list_end, 0);
        list_end = new_end;
      }
      
      items = pmath_expr_read_item_data(expr);
      if(items) {
        list_end = collect_general_expr_items_position(
                     info,
                     list_end,
                     items,
                     pmath_expr_length(expr),
                     level);
      }
      else {
        list_end = collect_expr_items_position_slow(
                     info,
                     list_end,
                     expr,
                     level);
      }
    }
  }
  
  if(info->max == 0)
    return list_end;
    
  if(reldepth < 0)
    return list_end;
    
  if(test_pattern(info, expr)) {
    struct index_lists_t *result;
    
    result = pmath_mem_alloc(sizeof(struct index_lists_t));
    if(!result)
      return list_end;
      
    info->max--;
    result->next = NULL;
    result->list = NULL;
    *list_end = result;
    return &result->next;
  }
  
  return list_end;
}

static size_t list_length(void **ind) {
  size_t len = 0;
  
  while(ind) {
    ++len;
    ind = *ind;
  }
  
  return len;
}

static pmath_expr_t index_to_expr(struct index_t *idx) {
  pmath_expr_t result;
  struct _pmath_expr_t *expr;
  size_t i;
  
  expr = _pmath_expr_new_noinit(list_length((void **)idx));
  if(!expr)
    return PMATH_NULL;
    
  expr->items[0] = pmath_ref(PMATH_SYMBOL_LIST);
  for(i = 1; idx != NULL; idx = idx->next, ++i) {
    expr->items[i] = pmath_integer_new_uiptr(idx->i);
  }
  
  assert(i == expr->length + 1);
  assert(idx == NULL);
  
  result = PMATH_FROM_PTR(expr);
  _pmath_expr_update(result);
  return result;
}

static pmath_expr_t index_lists_to_expr(struct index_lists_t *ids) {
  pmath_expr_t result;
  struct _pmath_expr_t *expr;
  size_t i;
  
  expr = _pmath_expr_new_noinit(list_length((void **)ids));
  if(!expr)
    return PMATH_NULL;
    
  expr->items[0] = pmath_ref(PMATH_SYMBOL_LIST);
  for(i = 1; ids != NULL; ids = ids->next, ++i) {
    expr->items[i] = index_to_expr(ids->list);
  }
  
  assert(i == expr->length + 1);
  assert(ids == NULL);
  
  result = PMATH_FROM_PTR(expr);
  _pmath_expr_update(result);
  return result;
}


static pmath_expr_t position(
  struct position_info_t *info, // will free .pattern member
  pmath_t                 expr  // will be freed
) {
  struct index_lists_t *lists = NULL;
  assert(info != NULL);
  
  info->pattern_is_const = _pmath_pattern_is_const(info->pattern);
  
  collect_position(
    info,
    &lists,
    expr,
    0);
    
  pmath_unref(expr);
  pmath_unref(info->pattern);
  info->pattern = PMATH_UNDEFINED;
  
  expr = index_lists_to_expr(lists);
  destroy_index_lists(lists);
  
  return expr;
}

PMATH_PRIVATE pmath_t builtin_position(pmath_expr_t expr) {
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
  size_t last_nonoption, exprlen = pmath_expr_length(expr);
  pmath_expr_t options;
  pmath_t obj;
  
  if(exprlen < 2) {
    pmath_message_argxxx(exprlen, 2, 4);
    return expr;
  }
  
  last_nonoption = 2;
  info.with_heads = TRUE;
  info.levelmin = 0;
  info.levelmax = LONG_MAX;
  info.max = SIZE_MAX;
  if(exprlen > 2) {
    pmath_t levels = pmath_expr_get_item(expr, 3);
    
    if(_pmath_extract_levels(levels, &info.levelmin, &info.levelmax)) {
      last_nonoption = 3;
      
      if(exprlen > 3) {
        obj = pmath_expr_get_item(expr, 4);
        
        if( pmath_is_integer(obj) &&
            pmath_number_sign(obj) >= 0)
        {
          last_nonoption = 4;
          if(pmath_is_int32(obj) && PMATH_AS_INT32(obj) >= 0)
            info.max = PMATH_AS_INT32(obj);
          else
            info.max = SIZE_MAX;
        }
        else if(pmath_equals(obj, _pmath_object_pos_infinity)) {
          last_nonoption = 4;
          info.max = SIZE_MAX;
        }
        else if(!pmath_is_set_of_options(obj)) {
          pmath_unref(obj);
          pmath_unref(levels);
          pmath_message(PMATH_NULL, "innf", 2, PMATH_FROM_INT32(4), pmath_ref(expr));
          return expr;
        }
        
        pmath_unref(obj);
      }
    }
    else if(!pmath_is_set_of_options(levels)) {
      pmath_message(PMATH_NULL, "level", 1, levels);
      return expr;
    }
    
    pmath_unref(levels);
  }
  
  info.pattern = pmath_expr_get_item(expr, 2);
  if(!_pmath_pattern_validate(info.pattern)) {
    pmath_unref(info.pattern);
    return expr;
  }
  
  options = pmath_options_extract(expr, last_nonoption);
  if(pmath_is_null(options)) {
    pmath_unref(info.pattern);
    return expr;
  }
  
  obj = pmath_evaluate(pmath_option_value(PMATH_NULL, PMATH_SYMBOL_HEADS, options));
  if(pmath_same(obj, PMATH_SYMBOL_TRUE)) {
    info.with_heads = TRUE;
  }
  else if(pmath_same(obj, PMATH_SYMBOL_FALSE)) {
    info.with_heads = FALSE;
  }
  else if(!pmath_same(obj, PMATH_SYMBOL_FALSE)) {
    pmath_unref(info.pattern);
    pmath_unref(options);
    pmath_message(
      PMATH_NULL, "opttf", 2,
      pmath_ref(PMATH_SYMBOL_HEADS),
      obj);
    return expr;
  }
  pmath_unref(obj);
  pmath_unref(options);
  
  obj = pmath_expr_get_item(expr, 1);
  pmath_unref(expr);
  
  return position(&info, obj);
}
