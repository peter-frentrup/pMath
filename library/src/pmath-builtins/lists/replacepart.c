#include <pmath-core/numbers-private.h>

#include <pmath-language/patterns-private.h>

#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/memory.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/control-private.h>
#include <pmath-builtins/lists-private.h>


/* TODO: allow ReplacePart({{a, b, c}, {d, e}, {f}}, {~~~, -1} -> xx)
                                                           ==
 */
static pmath_t replace_const_part(
  pmath_expr_t  list,           // will be freed; may be no expression
  pmath_expr_t  position,       // wont be freed
  size_t        position_start,
  pmath_bool_t  allow_head,
  pmath_t       newpart,        // wont be freed
  pmath_bool_t *error
){
  pmath_t pos;
  size_t index, listlen;

  if(position_start > pmath_expr_length(position)){
    pmath_unref(list);
    return pmath_ref(newpart);
  }

  if(!pmath_is_expr(list)){
    if(error){
      if(!*error){
        pmath_message(PMATH_NULL, "partd", 1, pmath_ref(position));
      }
      *error = TRUE;
    }
    return list;
  }

  pos = pmath_expr_get_item(position, position_start);
  if(!_pmath_is_integer(pos)){
    if(error){
      if(!*error){
        pmath_message(PMATH_NULL, "pspec", 1, pmath_ref(pos));
      }
      *error = TRUE;
    }
    pmath_unref(pos);
    return list;
  }

  listlen = pmath_expr_length(list);
  index = SIZE_MAX;

  if(!extract_number(pos, listlen, &index)){
    if(error){
      if(!*error){
        pmath_message(PMATH_NULL, "partw", 2, pmath_ref(list), pmath_ref(pos));
      }
      *error = TRUE;
    }
    pmath_unref(pos);
    return list;
  }

  if(index > listlen){
    if(error){
      if(!*error){
        pmath_message(PMATH_NULL, "partw", 2, pmath_ref(list), pmath_ref(pos));
      }
      *error = TRUE;
    }
    pmath_unref(pos);
    return list;
  }

  pmath_unref(pos);
  
  if(index == 0 && !allow_head)
    return list;

  return pmath_expr_set_item(list, index,
    replace_const_part(
      pmath_expr_extract_item(list, index),
      position,
      position_start + 1,
      allow_head,
      newpart,
      error));
}

static pmath_bool_t replace_all_const_parts( // return = are there other rules?
  pmath_expr_t  *list,
  pmath_expr_t  *rules,
  pmath_bool_t   allow_head
){
  pmath_bool_t result = FALSE;

  size_t i;
  for(i = 1;i <= pmath_expr_length(*rules);++i){
    pmath_expr_t rule = pmath_expr_get_item(*rules, i);
    pmath_t pattern = pmath_expr_get_item(rule, 1);
    if(pmath_is_expr(pattern) && _pmath_pattern_is_const(pattern)){
      pmath_t rhs = pmath_expr_get_item(rule, 2);

      *rules = pmath_expr_set_item(*rules, i, PMATH_NULL);
      *list = replace_const_part(*list, pattern, 1, allow_head, rhs, NULL);

      pmath_unref(rhs);
    }
    else
      result = TRUE;
    pmath_unref(pattern);
    pmath_unref(rule);
  }

  return result;
}

static pmath_bool_t prepare_pattern_len_index( // stop?
  pmath_t *pos,
  size_t   len
){
  if(pmath_is_int32(*pos) && PMATH_AS_INT32(*pos) < 0){
    *pos = PMATH_FROM_INT32(len + PMATH_AS_INT32(*pos) + 1);
    return TRUE;
  }
  
  if(pmath_is_mpint(*pos) && mpz_sgn(PMATH_AS_MPZ(*pos)) < 0){
    pmath_mpint_t newpos = _pmath_create_mp_int(0);
    
    if(!pmath_is_null(newpos)){
      mpz_add_ui(
        PMATH_AS_MPZ(newpos),
        PMATH_AS_MPZ(*pos),
        (unsigned long)len + 1);

      pmath_unref(*pos);
      *pos = newpos;
      return FALSE;
    }
    
    return TRUE;
  }
  else{
    _pmath_pattern_analyse_input_t  input;
    _pmath_pattern_analyse_output_t output;
    input.parent_pat_head = PMATH_NULL;
    input.pat = *pos;
    input.associative = 0;
    _pmath_pattern_analyse(&input, &output);
    return output.min != 1 || output.max != 1;
  }
}

static void prepare_pattern_len(
  pmath_expr_t *pattern,
  size_t       *lengths,
  size_t        depth
){
  size_t start, end, len;

  assert(pmath_is_null(*pattern) || pmath_is_expr(*pattern));
  assert(depth >= 1);

  end = len = pmath_expr_length(*pattern);

  if(end > depth)
    end = depth;

  for(start = 1;start <= end;++start){
    pmath_t pos = pmath_expr_get_item(*pattern, start);
    if(prepare_pattern_len_index(&pos, lengths[start-1])){
      pmath_unref(pos);
      break;
    }
    *pattern = pmath_expr_set_item(*pattern, start, pos);
  }

  if(start + depth < len)
    start = len - depth;

  for(end = len;end > start;--end){
    pmath_t pos = pmath_expr_get_item(*pattern, end);
    if(prepare_pattern_len_index(&pos, lengths[depth + end - len - 1])){
      pmath_unref(pos);
      break;
    }
    *pattern = pmath_expr_set_item(*pattern, end, pos);
  }
}

static pmath_t replace_rule_part(
  pmath_expr_t   list,     // will be freed
  pmath_expr_t  *position, // at least calc_depth(list) long
  size_t        *lengths,  // at least calc_depth(list) long
  size_t         level,    // >= 1
  pmath_bool_t   with_heads,
  pmath_expr_t   rules     // wont be freed
){
  pmath_expr_t local_rules;
  size_t i, listlen;

  assert(level >= 1);
  if(!pmath_is_expr(list))
    return list;

  listlen = pmath_expr_length(list);
  lengths[level-1] = listlen;

  local_rules = pmath_ref(rules);

  for(i = 1;i <= pmath_expr_length(local_rules);++i){
    pmath_expr_t rule = pmath_expr_get_item(local_rules, i);
    
    if(!pmath_is_null(rule)){
      pmath_t pattern = pmath_expr_get_item(rule, 1);
      if(pmath_is_expr(pattern))
        prepare_pattern_len(&pattern, lengths, level);

      rule = pmath_expr_set_item(rule, 1, pattern);
      local_rules = pmath_expr_set_item(local_rules, i, rule);
    }
  }

  for(i = with_heads ? 0 : 1;i <= listlen;++i){
    pmath_expr_t current_pos;
    size_t j;

    *position = pmath_expr_set_item(*position, level,
      pmath_integer_new_size(i));

    current_pos = pmath_expr_get_item_range(*position, 1, level);

    for(j = 1;j <= pmath_expr_length(local_rules);++j){
      pmath_expr_t rule = pmath_expr_get_item(local_rules, j);
      
      if(!pmath_is_null(rule)){
        pmath_t pattern = pmath_expr_get_item(rule, 1);
        pmath_t rhs     = pmath_expr_get_item(rule, 2);
        pmath_unref(rule);

        if(_pmath_pattern_match(current_pos, pattern, &rhs)){
          list = pmath_expr_set_item(list, i, rhs);
          goto HAVE_FIT;
        }
        pmath_unref(rhs);
      }
    }
    pmath_unref(current_pos);
    current_pos = PMATH_NULL;
    list = pmath_expr_set_item(list, i,
      replace_rule_part(
        pmath_expr_get_item(list, i),
        position,
        lengths,
        level + 1,
        with_heads,
        rules));

   HAVE_FIT:
    pmath_unref(current_pos);
  }

  pmath_unref(local_rules);

  return list;
}

static size_t calc_depth(pmath_t obj){ // without heads
  size_t i, len, result;

  if(!pmath_is_expr(obj))
    return 0;

  result = 0;
  len = pmath_expr_length(obj);

  for(i = 1;i <= len;++i){
    pmath_t obji = pmath_expr_get_item(obj, i);
    size_t resi = calc_depth(obji);
    pmath_unref(obji);

    if(resi > result)
      result = resi;
  }

  return 1 + result;
}

static pmath_expr_t prepare_rule(pmath_expr_t rule){
  pmath_t fst, fst_head;

  assert(pmath_is_null(rule) || pmath_is_expr(rule));

  fst = pmath_expr_get_item(rule, 1);
  if(!pmath_is_expr(fst)){
    return pmath_expr_set_item(rule, 1,
      pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_LIST), 1,
        fst));
  }
  
  fst_head = pmath_expr_get_item(fst, 0);
  pmath_unref(fst_head);
  if(!pmath_same(fst_head, PMATH_SYMBOL_LIST)){
    return pmath_expr_set_item(rule, 1,
      pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_LIST), 1,
        fst));
  }
  
  pmath_unref(fst);
  return rule;
}

static pmath_expr_t prepare_rule_list(pmath_expr_t rules){
  size_t i;

  assert(pmath_is_null(rules) || pmath_is_expr(rules));

  for(i = 1;i <= pmath_expr_length(rules);++i)
    rules = pmath_expr_set_item(
      rules, i,
      prepare_rule(
        pmath_expr_get_item(rules, i)));

  return rules;
}

PMATH_PRIVATE pmath_t builtin_replacepart(pmath_expr_t expr){
  /* ReplacePart(list, i -> new)
     ReplacePart(list, {i1 -> new1, i2 -> new2, ...})
     ReplacePart(list, {i, j, ...} -> new)
     ReplacePart(list, {{i1, j1, ...} -> new1, {i2, j2, ...} -> new2, ...})
     ReplacePart(list, pattern -> new)
     ReplacePart(list, {pattern1 -> new1, pattern2 -> new2, ...})
     
     Options:
       Head -> False

     messages (not listing argument count and option related messages):
       General::reps:=  "`1` is not a list of replacement rules."
   */
  pmath_bool_t with_heads = FALSE;
  pmath_bool_t const_heads = TRUE;
  pmath_expr_t rules, list;

  {
    pmath_t heads_value;
    pmath_expr_t options = pmath_options_extract(expr, 2);

    if(pmath_is_null(options))
      return expr;

    heads_value = pmath_evaluate(pmath_option_value(PMATH_NULL, PMATH_SYMBOL_HEADS, options));
    pmath_unref(options);

    if(pmath_same(heads_value, PMATH_SYMBOL_TRUE)){
      with_heads = TRUE;
    }
    else if(pmath_same(heads_value, PMATH_SYMBOL_FALSE)){
      const_heads = FALSE;
    }
    else if(!pmath_same(heads_value, PMATH_SYMBOL_AUTOMATIC)){
      pmath_message(
        PMATH_NULL, "opttfa", 2,
        pmath_ref(PMATH_SYMBOL_HEADS),
        heads_value);
      return expr;
    }

    pmath_unref(heads_value);
  }

  rules = pmath_expr_get_item(expr, 2);
  if(!_pmath_is_list_of_rules(rules)){
    rules = pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_LIST), 1, rules);

    if(!_pmath_is_list_of_rules(rules)){
      pmath_message(PMATH_NULL, "reps", 1, rules);
      return expr;
    }
  }

  list = pmath_expr_get_item(expr, 1);
  pmath_unref(expr);
  rules = prepare_rule_list(rules);

  if(replace_all_const_parts(&list, &rules, const_heads)){
    pmath_expr_t  position;
    size_t        depth;
    size_t       *lengths;
    
    depth = calc_depth(list);
    position = pmath_expr_new(pmath_ref(PMATH_SYMBOL_LIST), depth);
    lengths = pmath_mem_alloc(sizeof(size_t) * depth);

    if(lengths && !pmath_is_null(position))
      list = replace_rule_part(list, &position, lengths, 1, with_heads, rules);

    pmath_mem_free(lengths);
    pmath_unref(position);
  }

  pmath_unref(rules);
  return list;
}
