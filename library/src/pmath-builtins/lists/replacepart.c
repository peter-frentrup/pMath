#include <pmath-core/numbers-private.h>
#include <pmath-core/packed-arrays.h>

#include <pmath-language/patterns-private.h>

#include <pmath-util/concurrency/threads.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/incremental-hash-private.h>
#include <pmath-util/memory.h>
#include <pmath-util/messages.h>
#include <pmath-util/option-helpers.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/control-private.h>
#include <pmath-builtins/lists-private.h>

#include <string.h>


/* TODO: allow ReplacePart({{a, b, c}, {d, e}, {f}}, {~~~, -1} -> xx)
                                                           ==
 */

struct multiindex_t {
  size_t length;
  size_t indices[1];
};

#define OPT_HEADS_FALSE      0
#define OPT_HEADS_AUTOMATIC  1
#define OPT_HEADS_TRUE       2

struct replace_info_t {
  pmath_expr_t  part_spec;
  pmath_t       value;
  size_t        depth; // = pmath_expr_length(part_spec)

  char                            *part_is_variable_pattern;  // length is depth
  _pmath_pattern_analyse_output_t *pattern_analysis;          // length is depth

  // must never be NULL:
  struct multiindex_t *current_multiindex;
  size_t current_multiindex_capacity;

  pmath_hashtable_t replacements; // HashSet< struct multiindex_t >

  uint8_t  heads_opt;
};

static pmath_bool_t resize_current_multiindex(struct replace_info_t *info, size_t length) {
  struct multiindex_t *mi;
  size_t cap = info->current_multiindex_capacity;

  if(length <= cap) {
    info->current_multiindex->length = length;
    return TRUE;
  }

  if(cap < 8)
    cap = 8;

  while(length > cap) {
    if(cap >= SIZE_MAX / 4 / sizeof(size_t))
      return FALSE;

    cap *= 2;
  }

  mi = pmath_mem_realloc_no_failfree(
      info->current_multiindex,
      sizeof(struct multiindex_t) + (cap - 1) * sizeof(size_t));

  if(!mi)
    return FALSE;

  mi->length = length;
  info->current_multiindex = mi;
  info->current_multiindex_capacity = cap;
  return TRUE;
}

static unsigned int hash_multiindex(void *p) {
  struct multiindex_t *mi = p;

  return incremental_hash(mi, (1 + mi->length) * sizeof(size_t), 0);
}

static pmath_bool_t equal_multiindices(void *p1, void *p2) {
  struct multiindex_t *mi1 = p1;
  struct multiindex_t *mi2 = p2;

  if(mi1->length != mi2->length)
    return FALSE;

  return  0 == memcmp(mi1->indices, mi2->indices, mi1->length * sizeof(size_t));
}

static pmath_ht_class_t multiindex_hashset_class[1] = {{
  pmath_mem_free,
  hash_multiindex,
  equal_multiindices,
  hash_multiindex,
  equal_multiindices
}};

static pmath_bool_t init_pattern_analyze(
    struct replace_info_t *info
) {
  size_t i;
  _pmath_pattern_analyse_input_t  inp;

  if(info->depth > SIZE_MAX / sizeof(_pmath_pattern_analyse_output_t))
    return FALSE;

  info->part_is_variable_pattern = pmath_mem_alloc(info->depth);
  if(!info->part_is_variable_pattern)
    return FALSE;

  info->pattern_analysis = pmath_mem_alloc(info->depth * sizeof(_pmath_pattern_analyse_output_t));
  if(!info->pattern_analysis) {
    pmath_mem_free(info->part_is_variable_pattern);
    return FALSE;
  }

  memset(&inp, 0, sizeof(inp));
  inp.parent_pat_head = PMATH_SYMBOL_LIST;

  for(i = info->depth; i > 0; --i) {
    inp.pat = pmath_expr_get_item(info->part_spec, i);

    info->part_is_variable_pattern[i - 1] = !_pmath_pattern_is_const(inp.pat);

    _pmath_pattern_analyse(&inp, &info->pattern_analysis[i - 1]);

    pmath_unref(inp.pat);
  }

  return TRUE;
}

static pmath_t replace_part_recursive(
    struct replace_info_t *info,
    size_t                 current_depth,
    pmath_t                expr  // will be freed
);

static pmath_t replace_int_part_recursive(
    struct replace_info_t *info,
    size_t                 current_depth,
    pmath_expr_t           expr,  // will be freed
    intptr_t               idx
) {
  size_t len = pmath_expr_length(expr);
  size_t i, mi_len;
  pmath_t sub;

  if(idx < 0) {
    if(idx == INTPTR_MIN)
      return expr;

    idx = -idx;
    if((size_t)idx > len)
      return expr;

    i = len + 1 - (size_t)idx;
  }
  else {
    i = (size_t)idx;
    if(i > len)
      return expr;
  }

  if(i == 0 && info->heads_opt == OPT_HEADS_FALSE)
    return expr;

  mi_len = info->current_multiindex->length;
  if(resize_current_multiindex(info, mi_len + 1)) {
    info->current_multiindex->indices[mi_len] = i;

    if(!pmath_ht_search(info->replacements, info->current_multiindex)) {
      sub = pmath_expr_extract_item(expr, i);
      sub = replace_part_recursive(info, current_depth + 1, sub);
      expr = pmath_expr_set_item(expr, i, sub);
    }

    info->current_multiindex->length = mi_len;
  }
  return expr;
}

static pmath_t replace_multiindex_part_recursive(
    struct replace_info_t *info,
    size_t                 next_depth,
    pmath_expr_t           expr,  // Will be freed. The element expr[indices] must exist!
    size_t                *indices,
    size_t                 count_indices
) {
  pmath_t sub;
  size_t mi_len;

  if(count_indices == 0)
    return replace_part_recursive(info, next_depth, expr);

  assert(pmath_is_expr(expr));

  mi_len = info->current_multiindex->length;
  if(!resize_current_multiindex(info, mi_len + 1))
    return expr;

  info->current_multiindex->indices[mi_len] = *indices;

  if(!pmath_ht_search(info->replacements, info->current_multiindex)) {
    sub = pmath_expr_extract_item(expr, *indices);
    sub = replace_multiindex_part_recursive(info, next_depth, sub, indices + 1, count_indices - 1);
    expr = pmath_expr_set_item(expr, *indices, sub);
  }

  info->current_multiindex->length = mi_len;

  return expr;
}

/* Initialize rest_indices to the first multi-index, for which expr[rest_indices] exists.
   Return such a multi-index exists (i.e. depth large enough)
 */
static pmath_bool_t init_multiindex(
    pmath_expr_t  expr,         // wont be freed
    size_t       *rest_indices, // [out]
    size_t        depth,
    pmath_bool_t  allow_heads
) {
  size_t len;

  if(depth == 0)
    return TRUE;

  if(!pmath_is_expr(expr))
    return FALSE;

  len = pmath_expr_length(expr);
  for(*rest_indices = allow_heads ? 0 : 1; *rest_indices <= len; ++*rest_indices) {
    pmath_t sub;
    pmath_bool_t success;

    sub = pmath_expr_get_item(expr, *rest_indices);
    success = init_multiindex(sub, rest_indices + 1, depth - 1, allow_heads);
    pmath_unref(sub);

    if(success)
      return TRUE;
  }

  return FALSE;
}

/* Advance rest_indices to the next multi-index (in lexicographical order),
   for which expr[rest_indices] exists.
   Return if such multi-index exists.
 */
static pmath_bool_t next_multiindex(
    pmath_expr_t  expr,         // wont be freed
    size_t       *rest_indices, // [inout]
    size_t        depth,
    pmath_bool_t  allow_heads
) {
  pmath_t sub;
  pmath_bool_t success;
  size_t len;

  if(depth == 0)
    return FALSE;

  if(!pmath_is_expr(expr))
    return FALSE;

  sub = pmath_expr_get_item(expr, *rest_indices);
  success = next_multiindex(sub, rest_indices + 1, depth - 1, allow_heads);
  pmath_unref(sub);

  if(success)
    return TRUE;

  len = pmath_expr_length(expr);
  while(++*rest_indices <= len) {

    sub = pmath_expr_get_item(expr, *rest_indices);
    success = init_multiindex(sub, rest_indices + 1, depth - 1, allow_heads);
    pmath_unref(sub);

    if(success)
      return TRUE;
  }

  return FALSE;
}

static pmath_packed_array_t try_to_multiindex_array(size_t *indices, size_t count_indices) {
  pmath_packed_array_t array;
  int32_t *data;
  size_t i;

  array = pmath_packed_array_new(PMATH_NULL, PMATH_PACKED_INT32, 1, &count_indices, NULL, 0);
  data = pmath_packed_array_begin_write(&array, NULL, 0);

  if(data) {
    for(i = 0; i < count_indices; ++i) {
      if(indices[i] > INT32_MAX) {
        pmath_unref(array);
        return PMATH_NULL;
      }

      data[i] = (int32_t)indices[i];
    }
  }

  return array;
}

static pmath_expr_t to_multiindex_expr_large(size_t *indices, size_t count_indices) {
  pmath_expr_t expr;
  size_t i;

  expr = pmath_expr_new(pmath_ref(PMATH_SYMBOL_LIST), count_indices);
  for(i = count_indices; i > 0; --i) {
    expr = pmath_expr_set_item(expr, i, pmath_integer_new_uiptr(indices[i - 1]));
  }

  return expr;
}

static pmath_expr_t to_multiindex_expr(size_t *indices, size_t count_indices) {
  pmath_expr_t expr;

  expr = try_to_multiindex_array(indices, count_indices);
  if(!pmath_is_null(expr))
    return expr;

  return to_multiindex_expr_large(indices, count_indices);
}

static size_t add_clamp(size_t a, size_t b) {
  size_t sum = a + b;

  if(sum < a) // overflow
    return SIZE_MAX;

  return sum;
}

static pmath_t replace_pattern_part_recursive(
    struct replace_info_t *info,
    size_t                 current_depth,
    pmath_expr_t           expr           // will be freed
) {
  pmath_t old_value = pmath_ref(info->value);
  size_t *indices;
  size_t short_multiindex[4];
  size_t indices_count;
  size_t min_indices_count;
  size_t max_indices_count;
  pmath_t index_pattern;
  size_t current_depth_end;

  min_indices_count = info->pattern_analysis[current_depth - 1].size.min;
  max_indices_count = info->pattern_analysis[current_depth - 1].size.max;

  /* TODO: match patterns iteratively: ReplacePart(IdentityMatrix(1000), {~i,~i}->x)
     should not match (~i,~i) at once, because that would mean 1000*1000 tries.
     Instead,
     1) try to match the first ~i only (1000 tries),
     2) remember that a pattern named `i` is already matched,
     3) try matching the second ~i next, knowing that ~i is some fixed index
        (at best 1 try only).

     This would need to open the black-box _pmath_pattern_match() and allow
     partial matches with continuation information.

     Benifits are
     1) Faster code (e.g. replace {~i,1,1,1,1,~i}->x)
     2) Slightly simpler code here (no need for `current_depth_end`)
     3) Bugfix Heads->Automatic handling when a const 0 index appears within patterns:
          ReplacePart({f(1)(2,3)}, {~i,0,~i} -> x, Heads->Automatic)
        should give
          {f(x)(2,3)}
        But currently (~i,0,~i) is handled as one pattern, and `0` indices are skipped
        for patterns when Heads->Automatic, so no replacement occurs.
   */
  current_depth_end = info->depth;
  while(current_depth_end > current_depth && !info->part_is_variable_pattern[current_depth_end - 1]) {
    --current_depth_end;
  }

  if(max_indices_count > SIZE_MAX / sizeof(size_t))
    max_indices_count = SIZE_MAX / sizeof(size_t);

  index_pattern = pmath_expr_get_item_range(info->part_spec, current_depth, current_depth_end);
  while(++current_depth <= current_depth_end) {
    min_indices_count = add_clamp(min_indices_count, info->pattern_analysis[current_depth - 1].size.min);
    max_indices_count = add_clamp(max_indices_count, info->pattern_analysis[current_depth - 1].size.max);
  }

  // TODO?: support outp->options.longest
  indices = short_multiindex;
  for(indices_count = min_indices_count; indices_count <= max_indices_count; ++indices_count) {
    pmath_t indices_expr;
    pmath_bool_t more;

    if(indices_count > sizeof(short_multiindex) / sizeof(size_t)) {
      if(indices != short_multiindex)
        pmath_mem_free(indices);

      indices = pmath_mem_alloc(indices_count * sizeof(size_t));
      if(!indices)
        break;
    }

    more = init_multiindex(
      expr,
      indices,
      indices_count,
      info->heads_opt == OPT_HEADS_TRUE);
    if(!more || pmath_aborting())
      break;

    while(more) {
      indices_expr = to_multiindex_expr(indices, indices_count);

      if(_pmath_pattern_match(indices_expr, pmath_ref(index_pattern), &info->value)) {

        expr = replace_multiindex_part_recursive(info, current_depth, expr, indices, indices_count);

        pmath_unref(info->value);
        info->value = pmath_ref(old_value);
      }

      pmath_unref(indices_expr);
      more = next_multiindex(
        expr,
        indices,
        indices_count,
        info->heads_opt == OPT_HEADS_TRUE);
    }
  }

  if(indices != short_multiindex)
    pmath_mem_free(indices);

  pmath_unref(old_value);
  pmath_unref(index_pattern);
  return expr;
}

static pmath_t replace_part_recursive(
    struct replace_info_t *info,
    size_t                 current_pattern_depth,
    pmath_t                expr           // will be freed
) {
  pmath_t index;

  if(current_pattern_depth > info->depth) {
    struct multiindex_t *success_pos = pmath_mem_alloc(
        (info->current_multiindex->length + 1) * sizeof(size_t));

    if(success_pos) {
      memcpy(
          success_pos,
          info->current_multiindex,
          (info->current_multiindex->length + 1) * sizeof(size_t));

      success_pos = pmath_ht_insert(info->replacements, success_pos);
      pmath_mem_free(success_pos);
    }

    pmath_unref(expr);
    return pmath_ref(info->value);
  }

  if(!pmath_is_expr(expr))
    return expr;

  index = pmath_expr_get_item(info->part_spec, current_pattern_depth);

  if(pmath_is_int32(index))
    return replace_int_part_recursive(info, current_pattern_depth, expr, PMATH_AS_INT32(index));

  if(pmath_is_mpint(index) && pmath_integer_fits_siptr(index)) {
    intptr_t idx = pmath_integer_get_siptr(index);
    pmath_unref(index);
    return replace_int_part_recursive(info, current_pattern_depth, expr, idx);
  }

  if(info->part_is_variable_pattern[current_pattern_depth - 1]) {
    pmath_unref(index);
    return replace_pattern_part_recursive(info, current_pattern_depth, expr);
  }

  pmath_unref(index);
  return expr;
}

static pmath_t replace_part(
    pmath_t           expr,      // will be freed
    pmath_expr_t      part_spec, // will be freed
    pmath_t           value,     // will be freed
    uint8_t           heads_opt,
    pmath_hashtable_t replacements
) {
  struct replace_info_t info;

  memset(&info, 0, sizeof(info));

  info.part_spec  = part_spec;
  info.value      = value;
  info.depth      = pmath_expr_length(part_spec);
  info.heads_opt  = heads_opt;

  info.replacements = replacements;

  if(resize_current_multiindex(&info, 8)) {
    info.current_multiindex->length = 0;

    if(init_pattern_analyze(&info)) {
      expr = replace_part_recursive(&info, 1, expr);

      pmath_mem_free(info.part_is_variable_pattern);
      pmath_mem_free(info.pattern_analysis);
    }

    pmath_mem_free(info.current_multiindex);
  }

  pmath_unref(info.part_spec);
  pmath_unref(info.value);
  return expr;
}

/* Normalize to {{i1,j1,...}->rhs1, {i2,j2,...}->rhs2, ...}
   Return PMATH_UNDEFINED on error.
 */
static pmath_expr_t prepare_repl_rules(pmath_t rules) {
  // ... -> rhs
  if(_pmath_is_rule(rules)) {
    pmath_t lhs = pmath_expr_get_item(rules, 1);

    // {...} -> rhs
    if(pmath_is_expr_of(lhs, PMATH_SYMBOL_LIST)) {
      pmath_t item = pmath_expr_get_item(lhs, 1);

      // {{i1,j1,...}, {i2,j2,...}, ...} -> rhs   ~~>   {{i1,j1,...}->rhs, {i2,j2,...}->rhs, ...}
      if(pmath_is_expr_of(item, PMATH_SYMBOL_LIST)) {
        size_t i;
        pmath_unref(item);

        rules = pmath_expr_set_item(rules, 1, PMATH_NULL);

        for(i = pmath_expr_length(lhs); i > 0; --i) {
          item = pmath_expr_extract_item(lhs, i);

          item = pmath_expr_set_item(pmath_ref(rules), 1, item);

          lhs = pmath_expr_set_item(lhs, i, item);
        }

        pmath_unref(rules);
        return lhs;
      }

      pmath_unref(item);
      pmath_unref(lhs);
      // {i,j,...} -> rhs  ~~>  {{i,j,...} -> rhs}
      return pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_LIST), 1, rules);
    }

    // i -> rhs  ~~>  {{i} -> rhs}
    rules = pmath_expr_set_item(rules, 1, PMATH_NULL);
    lhs = pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_LIST), 1, lhs);
    rules = pmath_expr_set_item(rules, 1, lhs);
    return pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_LIST), 1, rules);
  }

  // {... -> rhs1, ... -> rhs2, ...}
  if(_pmath_is_list_of_rules(rules)) {
    size_t i;

    for(i = pmath_expr_length(rules); i > 0; --i) {
      pmath_t rule = pmath_expr_extract_item(rules, i);
      pmath_t lhs = pmath_expr_extract_item(rule, 1);

      // {..., i->rhs, ...}  ~~>  {..., {i}->rhs, ...}
      if(!pmath_is_expr_of(lhs, PMATH_SYMBOL_LIST))
        lhs = pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_LIST), 1, lhs);

      rule = pmath_expr_set_item(rule, 1, lhs);
      rules = pmath_expr_set_item(rules, i, rule);
    }

    return rules;
  }

  pmath_unref(rules);
  return PMATH_UNDEFINED;
}

PMATH_PRIVATE pmath_t builtin_replacepart(pmath_expr_t expr) {
  /* ReplacePart(list, i -> new)
     ReplacePart(list, {i1 -> new1, i2 -> new2, ...})
     ReplacePart(list, {i, j, ...} -> new)
     ReplacePart(list, {{i1, j1, ...} -> new1, {i2, j2, ...} -> new2, ...})
     ReplacePart(list, pattern -> new)
     ReplacePart(list, {pattern1 -> new1, pattern2 -> new2, ...})

     Options:
       Head -> False

     messages:
       General::reps:=  "`1` is not a list of replacement rules."
   */
  uint8_t heads_opt = OPT_HEADS_AUTOMATIC;
  pmath_expr_t rules, list;
  size_t i, len;
  pmath_hashtable_t replacements;

  {
    pmath_t heads_value;
    pmath_expr_t options = pmath_options_extract(expr, 2);

    if(pmath_is_null(options))
      return expr;

    heads_value = pmath_evaluate(pmath_option_value(PMATH_NULL, PMATH_SYMBOL_HEADS, options));
    pmath_unref(options);

    if(pmath_same(heads_value, PMATH_SYMBOL_TRUE)) {
      heads_opt = OPT_HEADS_TRUE;
    }
    else if(pmath_same(heads_value, PMATH_SYMBOL_FALSE)) {
      heads_opt = OPT_HEADS_FALSE;
    }
    else if(!pmath_same(heads_value, PMATH_SYMBOL_AUTOMATIC)) {
      pmath_message(
          PMATH_NULL, "opttfa", 2,
          pmath_ref(PMATH_SYMBOL_HEADS),
          heads_value);
      return expr;
    }

    pmath_unref(heads_value);
  }

  rules = pmath_expr_get_item(expr, 2);
  rules = prepare_repl_rules(rules);
  if(pmath_same(rules, PMATH_UNDEFINED)) {
    rules = pmath_expr_get_item(expr, 2);
    pmath_message(PMATH_NULL, "reps", 1, rules);
    return expr;
  }

  list = pmath_expr_get_item(expr, 1);
  pmath_unref(expr);

  replacements = pmath_ht_create(multiindex_hashset_class, 0);

  len = pmath_expr_length(rules);
  for(i = 1; i <= len; ++i) {
    pmath_t rule = pmath_expr_extract_item(rules, i);
    pmath_t lhs = pmath_expr_extract_item(rule, 1);
    pmath_t rhs = pmath_expr_extract_item(rule, 2);

    list = replace_part(list, lhs, rhs, heads_opt, replacements);

    pmath_unref(rule);
  }

  pmath_ht_destroy(replacements);

  pmath_unref(rules);
  return list;
}
