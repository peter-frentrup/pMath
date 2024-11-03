#include <pmath-util/association-lists-private.h>

#include <pmath-util/debug.h>
#include <pmath-util/dispatch-tables-private.h>
#include <pmath-util/helpers.h>

#include <pmath-core/symbols.h>


struct _pmath_association_list_extra_data_t {
  struct _pmath_custom_expr_data_t base;

  size_t used_length;
  uint8_t rule_delayed_bitset[1]; // at least (used_length + 7) / 8  many entries.
};

#define ASSOC_EXPR_EXTRA(EXPR_PTR)      ((struct _pmath_association_list_extra_data_t*)PMATH_CUSTOM_EXPR_DATA(EXPR_PTR))

#define IS_RULE_DELAYED_INDEX0(   extra, index0)  (0 != ((extra)->rule_delayed_bitset[(index0) >> 3]  &  (1U << ((index0) & 7))) )
#define SET_RULE_DELAYED_INDEX0(  extra, index0)  do{    (extra)->rule_delayed_bitset[(index0) >> 3] |=  (1U << ((index0) & 7)); } while(0)
#define CLEAR_RULE_DELAYED_INDEX0(extra, index0)  do{    (extra)->rule_delayed_bitset[(index0) >> 3] &= ~(1U << ((index0) & 7)); } while(0)


#define ASSOC_EXTRA_SIZE_FOR_CAPACITY(CAPACITY)  ( sizeof(struct _pmath_association_list_extra_data_t)   \
                                                 + ((CAPACITY) >> 3))

static _pmath_association_list_t *create_association_list(pmath_t keys, pmath_t values); // keys and values will be freed


extern pmath_symbol_t pmath_System_List;
extern pmath_symbol_t pmath_System_Rule;
extern pmath_symbol_t pmath_System_RuleDelayed;


#ifdef PMATH_DEBUG_MEMORY
static struct {
  pmath_atomic_t alloc_counter;
} association_lists_stats;
#endif // PMATH_DEBUG_MEMORY


//{ custom expr API for association lists ...

static void         association_list_destroy_data(        struct _pmath_custom_expr_data_t *_data);
static size_t       association_list_get_length(          struct _pmath_custom_expr_t *e);
static pmath_t      association_list_get_item(            struct _pmath_custom_expr_t *e, size_t i);
static pmath_bool_t association_list_try_item_equals(     struct _pmath_custom_expr_t *e, size_t i, pmath_t expected_item, pmath_bool_t *result); // does not free e or expected_item
static pmath_bool_t association_list_try_set_item_copy(   struct _pmath_custom_expr_t *e, size_t i, pmath_t new_item, pmath_expr_t *result);      // does not free e, but frees new_item (only if returning TRUE)
static pmath_bool_t association_list_try_set_item_mutable(struct _pmath_custom_expr_t *e, size_t i, pmath_t new_item);
static pmath_bool_t association_list_try_compare_equal(   struct _pmath_custom_expr_t *e, pmath_t other, pmath_bool_t *result);                   // does not free e or other


static const struct _pmath_custom_expr_api_t association_list_expr_api = {
  .destroy_data         = association_list_destroy_data,
  .get_length           = association_list_get_length,
  .get_item             = association_list_get_item,
  .try_item_equals      = association_list_try_item_equals,
  .try_set_item_copy    = association_list_try_set_item_copy,
  .try_set_item_mutable = association_list_try_set_item_mutable,
  .try_compare_equal    = association_list_try_compare_equal,
};

//} ... custom expr API for association lists

PMATH_API pmath_bool_t pmath_is_association_list(pmath_t obj) {
  return NULL != _pmath_as_custom_expr_by_api(obj, &association_list_expr_api);
}

PMATH_PRIVATE pmath_bool_t pmath_try_make_association_list(pmath_t *rules) {
  assert(rules != NULL);
  
  if(pmath_is_association_list(*rules))
    return TRUE;
  
  pmath_dispatch_table_t disp = _pmath_rules_need_dispatch_table(*rules);
  if(pmath_is_null(disp))
    return FALSE;
  
  // Now we know that *rules is indeed a list of rules
  
  size_t len = pmath_expr_length(disp);
  pmath_expr_t values = pmath_expr_new(pmath_ref(pmath_System_List), len);
  if(pmath_is_null(values)) {
    pmath_unref(disp);
    return FALSE;
  }
  
  _pmath_association_list_t *assoc_ptr = create_association_list(disp, values);
  if(PMATH_UNLIKELY(!assoc_ptr))
    return FALSE;
  
  assert(pmath_same(values, assoc_ptr->internals.items[1]));
  
  struct _pmath_association_list_extra_data_t *assoc_extra = ASSOC_EXPR_EXTRA(assoc_ptr);
  
  for(size_t i = len; i > 0; --i) {
    pmath_t rule = pmath_expr_get_item(*rules, i);
    pmath_t rhs  = pmath_expr_get_item(rule, 2);
    
    values = pmath_expr_set_item(values, i, rhs);
    
    if(pmath_expr_item_equals(rule, 0, pmath_System_RuleDelayed)) {
      SET_RULE_DELAYED_INDEX0(assoc_extra, i-1);
    }
    pmath_unref(rule);
  }
  
  assoc_ptr->internals.items[1] = values;
  
  if(PMATH_UNLIKELY(pmath_is_null(values))) {
    pmath_unref(PMATH_FROM_PTR(assoc_ptr));
    return FALSE;
  }
  
  pmath_unref(*rules);
  *rules = PMATH_FROM_PTR(assoc_ptr);
  return TRUE;
}

PMATH_PRIVATE pmath_association_list_t _pmath_create_association_list(pmath_t keys, pmath_t values) {
  _pmath_association_list_t *_assoc = create_association_list(keys, values);
  
  if(PMATH_UNLIKELY(!_assoc)) 
    return PMATH_NULL;
  
  pmath_association_list_t assoc = PMATH_FROM_PTR(_assoc);
  return assoc;
}

PMATH_PRIVATE pmath_bool_t _pmath_association_list_try_replace_keys_in_place(pmath_association_list_t assoc, pmath_t keys) {
  if(PMATH_UNLIKELY(pmath_is_null(assoc))) {
    pmath_unref(keys);
    return FALSE;
  }
  
  assert(pmath_is_association_list(assoc));
  
  if(PMATH_UNLIKELY(pmath_refcount(assoc) != 1)) {
    pmath_unref(keys);
    return FALSE;
  }
  
  _pmath_association_list_t                   *assoc_ptr   = (void*)PMATH_AS_PTR(assoc);
  struct _pmath_association_list_extra_data_t *assoc_extra = ASSOC_EXPR_EXTRA(assoc_ptr);
  
  if(PMATH_UNLIKELY(!pmath_is_expr_of_len(keys, pmath_System_List, assoc_extra->used_length))) {
    pmath_unref(keys);
    return FALSE;
  }
  
  pmath_t old_keys = assoc_ptr->internals.items[0];
  assoc_ptr->internals.items[0] = keys;
  pmath_unref(old_keys);
  return TRUE;
}

PMATH_API pmath_expr_t pmath_association_list_get_keys(pmath_association_list_t assoc) {
  if(PMATH_UNLIKELY(pmath_is_null(assoc)))
    return PMATH_NULL;
  
  assert(pmath_is_association_list(assoc));
  
  _pmath_association_list_t *assoc_ptr = (void*)PMATH_AS_PTR(assoc);
  return pmath_ref(assoc_ptr->internals.items[0]);
}

PMATH_API pmath_expr_t pmath_association_list_get_values(pmath_association_list_t assoc) {
  if(PMATH_UNLIKELY(pmath_is_null(assoc)))
    return PMATH_NULL;
  
  assert(pmath_is_association_list(assoc));
  
  _pmath_association_list_t *assoc_ptr = (void*)PMATH_AS_PTR(assoc);
  return pmath_ref(assoc_ptr->internals.items[1]);
}


PMATH_PRIVATE pmath_bool_t _pmath_association_lists_init(void) {
#ifdef PMATH_DEBUG_MEMORY
  pmath_atomic_write_release(&association_lists_stats.alloc_counter, 0);
#endif

  return TRUE;
}

PMATH_PRIVATE void _pmath_association_lists_done(void) {
#ifdef PMATH_DEBUG_MEMORY
  {
    size_t alloc_counter = (size_t)pmath_atomic_read_aquire(&association_lists_stats.alloc_counter);
    
    pmath_debug_print("association list allocations: %4"PRIdPTR"\n", alloc_counter);
  }
#endif
}


static _pmath_association_list_t *create_association_list(pmath_t keys, pmath_t values) {
  _pmath_association_list_t *result;
  
  if(PMATH_UNLIKELY(!pmath_is_expr_of(keys, pmath_System_List))) {
    pmath_unref(keys);
    pmath_unref(values);
    return NULL;
  }
  
  size_t len = pmath_expr_length(keys);
  if(PMATH_UNLIKELY(!pmath_is_expr_of_len(values, pmath_System_List, len))) {
    pmath_unref(keys);
    pmath_unref(values);
    return NULL;
  }
  
  size_t extra_size = ASSOC_EXTRA_SIZE_FOR_CAPACITY(len);
  result = _pmath_expr_new_custom(1, &association_list_expr_api, extra_size);
  if(PMATH_UNLIKELY(!result)) {
    pmath_unref(keys);
    pmath_unref(values);
    return NULL;
  }
  
#ifdef PMATH_DEBUG_MEMORY
  (void)pmath_atomic_fetch_add(&association_lists_stats.alloc_counter, 1);
#endif

  result->internals.items[0] = keys;
  result->internals.items[1] = values;
  
  struct _pmath_association_list_extra_data_t *extra = ASSOC_EXPR_EXTRA(result);
  
  extra->used_length = len;
  memset(&extra->rule_delayed_bitset[0], 0, (len + 7) >> 3);
  
  return result;
}

//{ custom expr API for association lists ...

static void association_list_destroy_data(struct _pmath_custom_expr_data_t *_data) {
  //struct _pmath_association_list_extra_data_t *assoc_extra = (void*)_data;
}

static size_t association_list_get_length(struct _pmath_custom_expr_t *e) {
  const struct _pmath_association_list_extra_data_t *assoc_extra = ASSOC_EXPR_EXTRA(e);
  
  return assoc_extra->used_length;
}

static pmath_t association_list_get_item(struct _pmath_custom_expr_t *e, size_t i) {
  const struct _pmath_association_list_extra_data_t *assoc_extra = ASSOC_EXPR_EXTRA(e);
  
  if(i == 0)
    return pmath_ref(pmath_System_List);
  
  if(PMATH_UNLIKELY(i > assoc_extra->used_length))
    return PMATH_NULL;
  
  assert(e->internals.length == 1);
  
  pmath_t keys   = e->internals.items[0];
  pmath_t values = e->internals.items[1];
  
  pmath_t head = IS_RULE_DELAYED_INDEX0(assoc_extra, i-1) ? pmath_ref(pmath_System_RuleDelayed) : pmath_ref(pmath_System_Rule);
  
  return pmath_expr_new_extended(
    head, 2,
    pmath_expr_get_item(keys, i),
    pmath_expr_get_item(values, i));
}

static pmath_bool_t association_list_try_item_equals(
  struct _pmath_custom_expr_t *e,  // will not be freed
  size_t                       i, 
  pmath_t                      expected_item, // will not be freed
  pmath_bool_t                *result
) {
  const struct _pmath_association_list_extra_data_t *assoc_extra = ASSOC_EXPR_EXTRA(e);
  
  if(i == 0) {
    *result = pmath_same(expected_item, pmath_System_List);
    return TRUE;
  }
  
  if(PMATH_UNLIKELY(i > assoc_extra->used_length)) {
    *result = pmath_is_null(expected_item);
    return TRUE;
  }
  
  if(!pmath_is_expr(expected_item)) {
    *result = FALSE;
    return TRUE;
  }
  
  pmath_t head = pmath_expr_get_item(expected_item, 0);
  pmath_unref(head);
  
  if(pmath_same(head, pmath_System_Rule)) {
    if(IS_RULE_DELAYED_INDEX0(assoc_extra, i-1)) {
      *result = FALSE;
      return TRUE;
    }
  }
  else if(pmath_same(head, pmath_System_RuleDelayed)) {
    if(IS_RULE_DELAYED_INDEX0(assoc_extra, i-1)) {
      *result = FALSE;
      return TRUE;
    }
  }
  else {
    *result = FALSE;
    return TRUE;
  }
  
  pmath_t keys   = e->internals.items[0];
  pmath_t values = e->internals.items[1];
  
  pmath_t lhs = pmath_expr_get_item(expected_item, 1);
  pmath_t rhs = pmath_expr_get_item(expected_item, 2);
  
  *result = pmath_expr_item_equals(keys, i, lhs) && pmath_expr_item_equals(values, i, rhs);
  
  pmath_unref(lhs);
  pmath_unref(rhs);
  return TRUE;
}

static pmath_bool_t association_list_try_set_item_copy(
  struct _pmath_custom_expr_t *e,        // will not be freed or modified
  size_t                       i, 
  pmath_t                      new_item, // will be freed iff returning TRUE
  pmath_expr_t                *result    // will be set iff returning TRUE
) {
  const struct _pmath_association_list_extra_data_t *assoc_extra = ASSOC_EXPR_EXTRA(e);
  
  if(PMATH_UNLIKELY(i == 0)) {
    if(pmath_same(new_item, pmath_System_List)) {
      pmath_unref(new_item);
      *result = pmath_ref(PMATH_FROM_PTR(&e->internals.inherited.inherited.inherited));
      return TRUE; // NO change
    }
    else
      return FALSE;
  }
  
  if(PMATH_UNLIKELY(i > assoc_extra->used_length)) {
    pmath_unref(new_item);
    *result = pmath_ref(PMATH_FROM_PTR(&e->internals.inherited.inherited.inherited));
    return TRUE; // NO change
  }

  if(!pmath_is_expr(new_item))
    return FALSE;
  
  pmath_t head = pmath_expr_get_item(new_item, 0);
  pmath_unref(head);
  
  pmath_bool_t head_changed = FALSE;
  if(pmath_same(head, pmath_System_RuleDelayed)) {
    if(!IS_RULE_DELAYED_INDEX0(assoc_extra, i-1))
      head_changed = TRUE;
  }
  else if(pmath_same(head, pmath_System_Rule)) {
    if(IS_RULE_DELAYED_INDEX0(assoc_extra, i-1))
      head_changed = TRUE;
  }
  else
    return FALSE;
  
  pmath_t keys   = e->internals.items[0];
  pmath_t values = e->internals.items[1];
  
  pmath_t lhs = pmath_expr_get_item(new_item, 1);
  pmath_t rhs = pmath_expr_get_item(new_item, 2);
  
  pmath_bool_t keys_changed   = !pmath_expr_item_equals(keys,   i, lhs);
  pmath_bool_t values_changed = !pmath_expr_item_equals(values, i, rhs);
  
  if(!head_changed && !keys_changed && !values_changed) {
    pmath_unref(new_item);
    pmath_unref(lhs);
    pmath_unref(rhs);
    *result = pmath_ref(PMATH_FROM_PTR(&e->internals.inherited.inherited.inherited));
    return TRUE; // NO change
  }
  
  keys = pmath_ref(keys);
  if(keys_changed) {
    keys = pmath_expr_set_item(keys, i, lhs);
  }
  else
    pmath_unref(lhs);
  
  values = pmath_ref(values);
  if(values_changed) {
    values = pmath_expr_set_item(values, i, rhs);
  }
  else
    pmath_unref(rhs);
  
  _pmath_association_list_t *new_assoc = create_association_list(keys, values);
  if(PMATH_UNLIKELY(!new_assoc)) {
    pmath_unref(new_item);
    return FALSE;
  }
  
  struct _pmath_association_list_extra_data_t *new_assoc_extra = ASSOC_EXPR_EXTRA(new_assoc);
  
  size_t num_bytes_bitset = (new_assoc_extra->used_length + 7) >> 3;
  memcpy(&new_assoc_extra->rule_delayed_bitset, &assoc_extra->rule_delayed_bitset, num_bytes_bitset);
  
  if(head_changed) {
    if(pmath_same(head, pmath_System_RuleDelayed))
      SET_RULE_DELAYED_INDEX0(new_assoc_extra, i-1);
    else
      CLEAR_RULE_DELAYED_INDEX0(new_assoc_extra, i-1);
  }
  
  pmath_unref(new_item);
  *result = PMATH_FROM_PTR(new_assoc);
  return TRUE;
}

static pmath_bool_t association_list_try_set_item_mutable(
  struct _pmath_custom_expr_t *e, // will be modified iff returning TRUE
  size_t                       i, 
  pmath_t                      new_item // will be freed iff returning TRUE
) {
  struct _pmath_association_list_extra_data_t *assoc_extra = ASSOC_EXPR_EXTRA(e);

  if(PMATH_UNLIKELY(i == 0)) {
    if(pmath_same(new_item, pmath_System_List)) {
      pmath_unref(new_item);
      return TRUE; // NO change
    }
    else
      return FALSE;
  }
  
  if(PMATH_UNLIKELY(i > assoc_extra->used_length)) {
    pmath_unref(new_item);
    return TRUE; // NO change
  }
  
  if(!pmath_is_expr(new_item))
    return FALSE;
  
  if(pmath_expr_length(new_item) != 2)
    return FALSE;
  
  pmath_bool_t any_change = FALSE;
  
  pmath_t head = pmath_expr_get_item(new_item, 0);
  pmath_unref(head);
  if(!pmath_same(head, pmath_System_Rule) && !pmath_same(head, pmath_System_RuleDelayed))
    return FALSE;
  
  pmath_t lhs = pmath_expr_get_item(new_item, 1);
  pmath_t rhs = pmath_expr_get_item(new_item, 2);
  
  pmath_t keys   = e->internals.items[0];
  pmath_t values = e->internals.items[1];
  
  if(pmath_expr_item_equals(keys, i, lhs)) {
    pmath_unref(lhs);
  }
  else {
    any_change = TRUE;
    keys = pmath_expr_set_item(keys, i, lhs);
    e->internals.items[0] = keys;
  }
  
  if(pmath_expr_item_equals(values, i, rhs)) {
    pmath_unref(rhs);
  }
  else {
    any_change = TRUE;
    values = pmath_expr_set_item(values, i, rhs);
    e->internals.items[1] = values;
  }
  
  if(pmath_same(head, pmath_System_RuleDelayed)) {
    if(!IS_RULE_DELAYED_INDEX0(assoc_extra, i-1)) {
      any_change = TRUE;
      SET_RULE_DELAYED_INDEX0(assoc_extra, i-1);
    }
  }
  else {
    if(IS_RULE_DELAYED_INDEX0(assoc_extra, i-1)) {
      any_change = TRUE;
      CLEAR_RULE_DELAYED_INDEX0(assoc_extra, i-1);
    }
  }
  
  if(any_change) {
    _pmath_custom_expr_changed(e);
  }
  
  pmath_unref(new_item);
  return TRUE;
}

static pmath_bool_t association_list_try_compare_equal(
  struct _pmath_custom_expr_t *e,     // wont be freed
  pmath_t                      other, // wont be freed
  pmath_bool_t                *result
) {
  struct _pmath_association_list_extra_data_t *assoc_extra = ASSOC_EXPR_EXTRA(e);

  if(pmath_is_association_list(other)) {
    _pmath_association_list_t                   *other_ptr   = (void*)PMATH_AS_PTR(other);
    struct _pmath_association_list_extra_data_t *other_extra = ASSOC_EXPR_EXTRA(other_ptr);
    
    size_t len = assoc_extra->used_length;
    if(len != other_extra->used_length) {
      *result = FALSE;
      return TRUE;
    }
    
    if(len >= 8) {
      if(0 != memcmp(assoc_extra->rule_delayed_bitset, other_extra->rule_delayed_bitset, len >> 3)) {
        *result = FALSE;
        return TRUE;
      }
    }
    
    if(len & 7) {
      uint8_t assoc_last_bits = assoc_extra->rule_delayed_bitset[len >> 3];
      uint8_t other_last_bits = other_extra->rule_delayed_bitset[len >> 3];
      uint8_t mask            = (1U << (len & 7)) - 1;
      if((assoc_last_bits ^ other_last_bits) & mask) {
        *result = FALSE;
        return TRUE;
      }
    }
    
    *result = (  pmath_equals(e->internals.items[0], other_ptr->internals.items[0]) 
              && pmath_equals(e->internals.items[1], other_ptr->internals.items[1]));
    return TRUE;
  }
  else
    return FALSE;
}

//} ... custom expr API for association lists
