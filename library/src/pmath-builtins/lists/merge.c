#include <pmath-builtins/lists-private.h>

#include <pmath-builtins/build-expr-private.h>

#include <pmath-util/evaluation.h>
#include <pmath-util/hash/hashtables-private.h>
#include <pmath-util/helpers.h>
#include <pmath-util/memory.h>
#include <pmath-util/messages.h>


extern pmath_symbol_t pmath_System_First;
extern pmath_symbol_t pmath_System_Identity;
extern pmath_symbol_t pmath_System_Last;
extern pmath_symbol_t pmath_System_Length;
extern pmath_symbol_t pmath_System_List;
extern pmath_symbol_t pmath_System_Plus;
extern pmath_symbol_t pmath_System_Rule;
extern pmath_symbol_t pmath_System_Total;


struct merge_context_t {
  pmath_t           merge_fun;
  pmath_expr_t      result;    ///< list of rules
  pmath_hashtable_t cache;     ///< [pmath_ht_obj_int_class] maps pmath_t lhs to index within `rules`
  
  pmath_t      (*make_new)(    struct merge_context_t *ctx, pmath_t new_rhs); // frees new_rhs
  pmath_t      (*combine)(     struct merge_context_t *ctx, pmath_t old_rhs, pmath_t new_rhs); // frees old_rhs and new_rhs
  pmath_expr_t (*post_process)(struct merge_context_t *ctx, pmath_expr_t rules);
};

static pmath_bool_t do_merge(struct merge_context_t *ctx, pmath_t rules); // rules will be freed
static pmath_bool_t has_any_rule_delayed(pmath_t rules);

static pmath_t new_identity(struct merge_context_t *ctx, pmath_t new_rhs);
static pmath_t new_list_of( struct merge_context_t *ctx, pmath_t new_rhs);
static pmath_t new_one(     struct merge_context_t *ctx, pmath_t new_rhs);
static pmath_t combine_append(       struct merge_context_t *ctx, pmath_t old_rhs, pmath_t new_rhs);
static pmath_t combine_last(         struct merge_context_t *ctx, pmath_t old_rhs, pmath_t new_rhs);
static pmath_t combine_first(        struct merge_context_t *ctx, pmath_t old_rhs, pmath_t new_rhs);
static pmath_t combine_increment_int(struct merge_context_t *ctx, pmath_t old_rhs, pmath_t new_rhs);
static pmath_t combine_eval_plus(    struct merge_context_t *ctx, pmath_t old_rhs, pmath_t new_rhs);
static pmath_expr_t post_process_identity( struct merge_context_t *ctx, pmath_expr_t rules);
static pmath_expr_t post_process_apply_rhs(struct merge_context_t *ctx, pmath_expr_t rules);

static void init_functions(struct merge_context_t *ctx, pmath_bool_t may_eval);

PMATH_PRIVATE pmath_t builtin_merge(pmath_expr_t expr) {
  /*  Merge({dict1, dict2, ...}, f)
      Merge(rules, f)
  */
  struct merge_context_t ctx;
  pmath_t rules;
  
  if(pmath_expr_length(expr) != 2) {
    pmath_message_argxxx(pmath_expr_length(expr), 2, 2);
    return expr;
  }
  
  ctx.merge_fun = pmath_expr_get_item(expr, 2);
  ctx.result    = pmath_expr_new(pmath_ref(pmath_System_List), 0);
  ctx.cache     = pmath_ht_create_ex(&pmath_ht_obj_int_class, 0);
  
  rules = pmath_expr_get_item(expr, 1);
  init_functions(&ctx, !has_any_rule_delayed(rules));
  
  if(do_merge(&ctx, rules)) { // frees `rules`
    ctx.result = ctx.post_process(&ctx, ctx.result);
  }
  else {
    pmath_t tmp;
    pmath_message(PMATH_NULL, "list1", 1, pmath_expr_get_item(expr, 1));
    
    tmp         = ctx.result;
    ctx.result  = expr;
    expr        = tmp;
  }
  
  pmath_unref(expr);
  pmath_unref(ctx.merge_fun);
  pmath_ht_destroy(ctx.cache);
  return ctx.result;
}

static pmath_bool_t do_merge(struct merge_context_t *ctx, pmath_t rules) {
  if(pmath_is_rule(rules)) {
    pmath_t lhs = pmath_expr_get_item(rules, 1);
    
    struct _pmath_object_int_entry_t *entry = pmath_ht_search(ctx->cache, &lhs);
    if(entry) {
      pmath_t old_rule = pmath_expr_extract_item(ctx->result, (size_t)entry->value);
      pmath_t old_rhs  = pmath_expr_extract_item(old_rule, 2);
      pmath_t new_rhs  = pmath_expr_get_item(rules, 2);
      
      pmath_unref(old_rule);
      pmath_unref(lhs);
      
      new_rhs     = ctx->combine(ctx, old_rhs, new_rhs);
      rules       = pmath_expr_set_item(rules, 2, new_rhs);
      ctx->result = pmath_expr_set_item(ctx->result, (size_t)entry->value, rules);
    }
    else {
      pmath_t new_rhs  = pmath_expr_get_item(rules, 2);
      new_rhs          = ctx->make_new(ctx, new_rhs);
      rules            = pmath_expr_set_item(rules, 2, new_rhs);
      ctx->result      = pmath_expr_append(ctx->result, 1, rules);
      
      entry = pmath_mem_alloc(sizeof(*entry));
      if(!entry) {
        pmath_unref(lhs);
        return FALSE;
      }
      
      entry->key   = lhs;
      entry->value = pmath_expr_length(ctx->result);
      
      entry = pmath_ht_insert(ctx->cache, entry);
      if(entry) {
        pmath_ht_obj_int_class.entry_destructor(ctx->cache, entry);
        return FALSE;
      }
    }
    
    return TRUE;
  }
  
  if(pmath_is_expr_of(rules, pmath_System_List)) {
    size_t len = pmath_expr_length(rules);
    size_t i;
    for(i = 1; i <= len; ++i) {
      if(!do_merge(ctx, pmath_expr_get_item(rules, i))) {
        pmath_unref(rules);
        return FALSE;
      }
    }
    
    pmath_unref(rules);
    return TRUE;
  }
  
  pmath_unref(rules);
  return FALSE;
}

static pmath_bool_t has_any_rule_delayed(pmath_t rules) { // does not free `rules`
  size_t i;
  pmath_t head = pmath_expr_get_item(rules, 0);
  pmath_unref(head);
  
  if(pmath_same(head, pmath_System_Rule))        return pmath_expr_length(rules) != 2;
  if(!pmath_same(head, pmath_System_List))       return TRUE;
  
  for(i = pmath_expr_length(rules); i > 0; --i) {
    pmath_t item = pmath_expr_get_item(rules, i);
    if(has_any_rule_delayed(item)) {
      pmath_unref(item);
      return TRUE;
    }
    pmath_unref(item);
  }
  
  return FALSE;
}

static void init_functions(struct merge_context_t *ctx, pmath_bool_t may_eval) {
  if(may_eval) {
    if(pmath_same(ctx->merge_fun, pmath_System_First)) {
      ctx->make_new     = new_identity;
      ctx->combine      = combine_first;
      ctx->post_process = post_process_identity;
      return;
    }
    if(pmath_same(ctx->merge_fun, pmath_System_Identity)) {
      ctx->make_new     = new_list_of;
      ctx->combine      = combine_append;
      ctx->post_process = post_process_identity;
      return;
    }
    if(pmath_same(ctx->merge_fun, pmath_System_Last)) {
      ctx->make_new     = new_identity;
      ctx->combine      = combine_last;
      ctx->post_process = post_process_identity;
      return;
    }
    if(pmath_same(ctx->merge_fun, pmath_System_Length)) {
      ctx->make_new     = new_one;
      ctx->combine      = combine_increment_int;
      ctx->post_process = post_process_identity;
      return;
    }
    if(pmath_same(ctx->merge_fun, pmath_System_Total)) {
      // But note that Total({a, b, c, ...}) and ((a + b) + c) + ... could give different results for floating point numbers
      ctx->make_new     = new_identity;
      ctx->combine      = combine_eval_plus;
      ctx->post_process = post_process_identity;
      return;
    }
  }
  
  ctx->make_new     = new_list_of;
  ctx->combine      = combine_append;
  ctx->post_process = post_process_apply_rhs;
}

static pmath_t new_identity(struct merge_context_t *ctx, pmath_t new_rhs) {
  return new_rhs;
}

static pmath_t new_list_of(struct merge_context_t *ctx, pmath_t new_rhs) {
  return pmath_expr_new_extended(pmath_ref(pmath_System_List), 1, new_rhs);
}

static pmath_t new_one(struct merge_context_t *ctx, pmath_t new_rhs) {
  pmath_unref(new_rhs);
  return INT(1);
}

static pmath_t combine_append(struct merge_context_t *ctx, pmath_t old_rhs, pmath_t new_rhs) {
  return pmath_expr_append(old_rhs, 1, new_rhs);
}

static pmath_t combine_first(struct merge_context_t *ctx, pmath_t old_rhs, pmath_t new_rhs) {
  pmath_unref(new_rhs);
  return old_rhs;
}

static pmath_t combine_last(struct merge_context_t *ctx, pmath_t old_rhs, pmath_t new_rhs) {
  pmath_unref(old_rhs);
  return new_rhs;
}

static pmath_t combine_increment_int(struct merge_context_t *ctx, pmath_t old_rhs, pmath_t new_rhs) {
  pmath_unref(new_rhs);
  if(pmath_is_int32(old_rhs)) {
    int cnt = PMATH_AS_INT32(old_rhs);
    if(cnt < INT32_MAX)
      return INT(cnt + 1);
  }
  
  return pmath_evaluate(PLUS(old_rhs, INT(1)));
}

static pmath_t combine_eval_plus(struct merge_context_t *ctx, pmath_t old_rhs, pmath_t new_rhs) {
  return pmath_evaluate(PLUS(old_rhs, new_rhs));
}

static pmath_expr_t post_process_identity(struct merge_context_t *ctx, pmath_expr_t rules) {
  return rules;
}

static pmath_expr_t post_process_apply_rhs(struct merge_context_t *ctx, pmath_expr_t rules) {
  size_t i;
  for(i = pmath_expr_length(rules); i > 0; --i) {
    pmath_t rule = pmath_expr_extract_item(rules, i);
    pmath_t rhs  = pmath_expr_extract_item(rule, 2);
    rhs          = pmath_expr_new_extended(pmath_ref(ctx->merge_fun), 1, rhs);
    rule         = pmath_expr_set_item(rule, 2, rhs);
    rules        = pmath_expr_set_item(rules, i, rule);
  }
  return rules;
}
