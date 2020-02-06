#include <pmath-util/dispatch-table-private.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/control-private.h>

#include <pmath-core/expressions-private.h>

#include <pmath-language/patterns-private.h>

#include <pmath-util/concurrency/atomic.h>
#include <pmath-util/debug.h>
#include <pmath-util/helpers.h>
#include <pmath-util/memory.h>


struct dispatch_lookup_info_t {
  pmath_t key;
  unsigned int turn_or_zero;
};

static struct _pmath_dispatch_table_t *find_dispatch_table(pmath_expr_t keys); // keys wont be freed
static struct _pmath_dispatch_table_t *create_dispatch_table_for_keys(pmath_expr_t keys); // keys will be freed
static struct _pmath_dispatch_table_t *get_dispatch_table_for_keys(pmath_expr_t keys); // keys will be freed

/** Look-up a key.
    
    If rules_in_rhs_out is given, it must point to (a copy of) a list of rules whose left-hand-sides 
    are the keys of \a table. It will be used for pattern matching (respecting any Condition in the 
    corresponding rhs). On output, the list-of-rules will be freed and be replaced by the matched 
    right-hand side. It then needs to be freed.
 */
static size_t dispatch_table_lookup(
  struct _pmath_dispatch_table_t *table, // won't be freed
  pmath_t key,                           // won't be freed
  pmath_t *rules_in_rhs_out);            // will be freed

static void destroy_dispatch_table(pmath_t a);
static unsigned int hash_dispatch_table(pmath_t a);
static int cmp_dispatch_tables(pmath_t a, pmath_t b);
static pmath_bool_t equal_dispatch_tables(pmath_t a, pmath_t b);

//{ dispatch table slice ...

static void noop(void *e) {
}

static unsigned int dispatch_entry_hash(void *e) {
  struct _pmath_dispatch_entry_t *entry = (struct _pmath_dispatch_entry_t *)e;
  return pmath_hash(entry->key);
}

static pmath_bool_t dispatch_entry_keys_equal(void *e1, void *e2) {
  struct _pmath_dispatch_entry_t *entry1 = e1;
  struct _pmath_dispatch_entry_t *entry2 = e2;
  
  return entry1->literal_turn_or_zero == entry2->literal_turn_or_zero && pmath_equals(entry1->key, entry2->key);
}

static unsigned int dispatch_lookup_info_hash(void *k) {
  struct dispatch_lookup_info_t *info = k;
  return pmath_hash(info->key);
}

static pmath_bool_t dispatch_entry_equals_lookup_key(void *e, void *k) {
  struct _pmath_dispatch_entry_t *entry = e;
  struct dispatch_lookup_info_t *info = k;
  
  if(info->turn_or_zero) {
    if(entry->literal_turn_or_zero != info->turn_or_zero)
      return FALSE;
  }
  
  return pmath_equals(entry->key, info->key);
}

static const pmath_ht_class_t dispatch_entries_ht_class = {
  noop,
  dispatch_entry_hash,
  dispatch_entry_keys_equal,
  dispatch_lookup_info_hash,
  dispatch_entry_equals_lookup_key
};

//} ... dispatch table slice

//{ dispatch table cache ...

static pmath_atomic_t dispatch_table_cache_lock = PMATH_ATOMIC_STATIC_INIT;

// Maps pmath_t* keys (note: not struct _pmath_t*) to struct _pmath_dispatch_table_t* entries:
static pmath_hashtable_t dispatch_table_cache; 

static void         dispatch_table_cache_entry_destructor(void *entry);
static unsigned int dispatch_table_cache_entry_hash(void *entry);
static unsigned int dispatch_table_cache_key_hash(void *key);
static pmath_bool_t dispatch_table_cache_entry_keys_equal(void *entry1, void *entry2);
static pmath_bool_t dispatch_table_cache_entry_equals_key(void *entry, void *key);

static const pmath_ht_class_t dispatch_table_cache_class = {
  dispatch_table_cache_entry_destructor,
  dispatch_table_cache_entry_hash,
  dispatch_table_cache_entry_keys_equal,
  dispatch_table_cache_key_hash,
  dispatch_table_cache_entry_equals_key
};

struct _pmath_dispatch_table_t *find_dispatch_table(pmath_expr_t keys) {
  struct _pmath_dispatch_table_t *result = NULL;
  
  pmath_atomic_lock(&dispatch_table_cache_lock);
  {
    result = pmath_ht_search(dispatch_table_cache, &keys);
    if(result)
      _pmath_ref_ptr((struct _pmath_t*)result);
  }
  pmath_atomic_unlock(&dispatch_table_cache_lock);
  
  return result;
}

static struct _pmath_dispatch_table_t *get_dispatch_table_for_keys(pmath_expr_t keys) {
  struct _pmath_dispatch_table_t *tab;
  struct _pmath_dispatch_table_t *old;
  
  tab = find_dispatch_table(keys);
  if(tab) {
    //pmath_debug_print("[use cached dispatch table (%d refs) ", (int)_pmath_refcount_ptr(&tab->inherited));
    //pmath_debug_print_object("for ", keys, "]\n");
    pmath_unref(keys);
    return tab;
  }
  
  tab = create_dispatch_table_for_keys(keys);
  if(!tab)
    return NULL;
  
  pmath_atomic_lock(&dispatch_table_cache_lock);
  {
    old = pmath_ht_insert(dispatch_table_cache, tab);
    if(old)
      _pmath_ref_ptr((struct _pmath_t*)old);
  }
  pmath_atomic_unlock(&dispatch_table_cache_lock);
  
  if(old)
    _pmath_unref_ptr((struct _pmath_t*)old);
  
  //pmath_debug_print("[cache new dispatch table (%d refs) ", (int)_pmath_refcount_ptr(&tab->inherited));
  //pmath_debug_print_object(" for ", tab->all_keys, "]\n");
  return tab;
}

//} ... dispatch table cache

struct _pmath_dispatch_table_t *create_dispatch_table_for_keys(pmath_expr_t keys) {
  struct _pmath_dispatch_table_t *tab;
  size_t num_keys = pmath_expr_length(keys);
  size_t size;
  size_t i;
  pmath_hashtable_t literal_entries;
  pmath_hashtable_t key_to_turn;
  struct dispatch_lookup_info_t lookup_no_turn;
  struct _pmath_dispatch_entry_t *current_slice_start;
  
  if(num_keys > INT32_MAX/2) {
    pmath_debug_print("[maximum number of dispatch table keys exceeded]\n");
    pmath_unref(keys);
    return NULL;
  }
  
  //pmath_debug_print_object("[creating dispatch table for ", keys, "]\n");
  literal_entries = pmath_ht_create(&dispatch_entries_ht_class, (unsigned)num_keys);
  if(!literal_entries) {
    pmath_unref(keys);
    return NULL;
  }
  
  key_to_turn = pmath_ht_create(&dispatch_entries_ht_class, (unsigned)num_keys);
  if(!key_to_turn) {
    pmath_ht_destroy(literal_entries);
    pmath_unref(keys);
    return NULL;
  }
  
  size = sizeof(struct _pmath_dispatch_table_t);
  if(num_keys)
    size+= (num_keys - 1) * sizeof(struct _pmath_dispatch_entry_t);
  
  tab = (struct _pmath_dispatch_table_t*)PMATH_AS_PTR(_pmath_create_stub(PMATH_TYPE_SHIFT_DISPATCH_TABLE, size));
  if(!tab) {
    pmath_ht_destroy(key_to_turn);
    pmath_ht_destroy(literal_entries);
    pmath_unref(keys);
    return NULL;
  }
  
  tab->all_keys = keys;
  tab->literal_entries = literal_entries;
  current_slice_start = &tab->entries[0];
  
  lookup_no_turn.turn_or_zero = 0;
  
  for(i = 1; i <= num_keys; ++i) {
    struct _pmath_dispatch_entry_t *entry = &tab->entries[i-1];
    entry->key = pmath_expr_get_item(tab->all_keys, i);
    
    if(_pmath_pattern_is_const(entry->key)) {
      struct _pmath_dispatch_entry_t *latest_turn;
      
      entry->next_slice_or_slice_start = current_slice_start;
      
      lookup_no_turn.key = entry->key;
      latest_turn = pmath_ht_search(key_to_turn, &lookup_no_turn);
      entry->literal_turn_or_zero = latest_turn ? latest_turn->literal_turn_or_zero : 0;
      latest_turn = pmath_ht_insert(key_to_turn, entry);
      entry->literal_turn_or_zero++;
      
      entry = pmath_ht_insert(literal_entries, entry);
      if(PMATH_UNLIKELY(entry)) {
        pmath_debug_print("[failed to insert entry for %d", (int)i);
        pmath_debug_print_object(" of ", keys, "]\n");
      }
    }
    else {
      current_slice_start->next_slice_or_slice_start = entry;
      entry->next_slice_or_slice_start = entry + 1;
      current_slice_start = entry + 1;
      
      entry->literal_turn_or_zero = 0;
    }
  }
  
  current_slice_start->next_slice_or_slice_start = &tab->entries[num_keys];
  
  pmath_ht_destroy(key_to_turn);
  return tab;
}

static struct _pmath_dispatch_entry_t *get_slice_start(struct _pmath_dispatch_entry_t *entry) {
  if(entry->next_slice_or_slice_start < entry)
    return entry->next_slice_or_slice_start;
  else
    return entry;
}

static struct _pmath_dispatch_entry_t *get_next_slice(struct _pmath_dispatch_entry_t *entry) {
  return get_slice_start(entry)->next_slice_or_slice_start;
}

static size_t dispatch_table_lookup(
  struct _pmath_dispatch_table_t *table, // won't be freed
  pmath_t key,                           // won't be freed
  pmath_t *rules_in_rhs_out              // will be freed
) {
  struct dispatch_lookup_info_t info;
  size_t last_index = 0;
  size_t num_keys = pmath_expr_length(table->all_keys);
  
  info.key = key;
  info.turn_or_zero = 1;
  
  while(last_index < num_keys) {
    size_t found_index;
    size_t slice_start;
    struct _pmath_dispatch_entry_t *entry = pmath_ht_search(table->literal_entries, &info);
    if(entry) {
      found_index = 1 + (entry - table->entries);
      slice_start = 1 + (get_slice_start(entry) - table->entries);
    }
    else {
      found_index = slice_start = 1 + num_keys;
    }
    
    ++last_index;
    while(last_index < slice_start) {
      entry = &table->entries[last_index - 1];
      if(entry->literal_turn_or_zero > 0) { // literal pattern, cannot match
        struct _pmath_dispatch_entry_t *next = entry->next_slice_or_slice_start;
        if(next <= entry)
          next = next->next_slice_or_slice_start;
        assert(next > entry);
        
        last_index = 1 + (next - table->entries);
        continue;
      }
      
      assert(entry->next_slice_or_slice_start == entry + 1);
      
      if(rules_in_rhs_out) {
        pmath_t rule = pmath_expr_get_item(*rules_in_rhs_out, last_index);
        pmath_t rhs = pmath_expr_get_item(rule, 2);
        pmath_unref(rule);
        
        if(_pmath_pattern_match(key, pmath_ref(entry->key), &rhs)) {
          pmath_unref(*rules_in_rhs_out);
          *rules_in_rhs_out = rhs;
          return last_index;
        }
        pmath_unref(rhs);
      }
      else {
        if(_pmath_pattern_match(key, pmath_ref(entry->key), NULL))
          return last_index;
      }
      
      ++last_index;
    }
    
    if(found_index > num_keys)
      break;
    
    last_index = found_index;
    if(rules_in_rhs_out) {
      pmath_t rule = pmath_expr_get_item(*rules_in_rhs_out, last_index);
      pmath_t rhs = pmath_expr_get_item(rule, 2);
      pmath_unref(rule);
      
      if(_pmath_pattern_match(PMATH_NULL, PMATH_NULL, &rhs)) {
        pmath_unref(*rules_in_rhs_out);
        *rules_in_rhs_out = rhs;
        return found_index;
      }
      pmath_unref(rhs);
    }
    else
      return found_index;
    
    info.turn_or_zero++;
  }
  
  return 0;
}

PMATH_PRIVATE pmath_dispatch_table_t _pmath_rules_need_dispatch_table(pmath_t expr) {
  pmath_dispatch_table_t tab;
  pmath_t keys;
  size_t len;
  
  if(!pmath_is_expr(expr))
    return PMATH_NULL;
  
  tab = _pmath_expr_get_dispatch_table(expr);
  if(!pmath_is_null(tab))
    return tab;
  
  if(!pmath_is_expr_of(expr, PMATH_SYMBOL_LIST))
    return PMATH_NULL;
  
  len = pmath_expr_length(expr);
  keys = pmath_expr_new(pmath_ref(PMATH_SYMBOL_LIST), len);
  for(; len > 0; --len) {
    pmath_t rule = pmath_expr_get_item(expr, len);
    pmath_t lhs;
    
    if(!_pmath_is_rule(rule)) {
      pmath_unref(rule);
      pmath_unref(keys);
      return PMATH_NULL;
    }
    
    lhs = pmath_expr_get_item(rule, 1);
    pmath_unref(rule);
    keys = pmath_expr_set_item(keys, len, lhs);
  }
  
  tab = PMATH_FROM_PTR(get_dispatch_table_for_keys(keys));
  _pmath_expr_attach_dispatch_table(expr, pmath_ref(tab));
  return tab;
}

PMATH_PRIVATE pmath_bool_t _pmath_rules_lookup(pmath_t rules, pmath_t key, pmath_t *result) {
  pmath_dispatch_table_t tab = _pmath_rules_need_dispatch_table(rules);
  struct _pmath_dispatch_table_t *tab_ptr = (void*)PMATH_AS_PTR(tab);
  size_t i;
  pmath_t rules_in_rhs_out;
  
  assert(result != NULL);
  
  if(!tab_ptr) {
    pmath_unref(key);
    return FALSE;
  }
  
  rules_in_rhs_out = pmath_ref(rules);
  i = dispatch_table_lookup(tab_ptr, key, &rules_in_rhs_out);  
  pmath_unref(tab);
  
  pmath_unref(key);
  if(i) {
    pmath_unref(*result);
    *result = rules_in_rhs_out;
    return TRUE;
  }
  
  pmath_unref(rules_in_rhs_out);
  return FALSE;
}

static pmath_t replace_rule_rhs(pmath_t rules, pmath_dispatch_table_t tab, size_t i, pmath_t new_value) {
  struct _pmath_dispatch_table_t *tab_ptr = (void*)PMATH_AS_PTR(tab);
  
  pmath_t rule = pmath_expr_new_extended(
                   pmath_is_evaluated(new_value) ? pmath_ref(PMATH_SYMBOL_RULE) : pmath_ref(PMATH_SYMBOL_RULEDELAYED),
                   2,
                   pmath_ref(tab_ptr->entries[i - 1].key),
                   new_value);
  
  if(pmath_is_null(rule)) {
    pmath_unref(tab);
    pmath_unref(rules);
    return PMATH_NULL;
  }
  
  rules = pmath_expr_set_item(rules, i, rule);
  _pmath_expr_attach_dispatch_table(rules, tab);
  return rules;
}

static pmath_t append_rule(pmath_t rules, pmath_t key, pmath_t new_value) {
  pmath_t rule;
  
  if(pmath_same(new_value, PMATH_UNDEFINED)) {
    pmath_unref(key);
    return rules;
  }
  
  rule = pmath_expr_new_extended(
           pmath_is_evaluated(new_value) ? pmath_ref(PMATH_SYMBOL_RULE) : pmath_ref(PMATH_SYMBOL_RULEDELAYED),
           2,
           key,
           new_value);
  return pmath_expr_append(rules, 1, rule);
}

PMATH_PRIVATE pmath_t _pmath_rules_assign(pmath_t rules, pmath_t key, pmath_t new_value) {
  pmath_dispatch_table_t tab = _pmath_rules_need_dispatch_table(rules);
  struct _pmath_dispatch_table_t *tab_ptr = (void*)PMATH_AS_PTR(tab);
  size_t i, len;
  struct _pmath_dispatch_entry_t *entry;
  
  if(!tab_ptr) {
    pmath_unref(key);
    pmath_unref(new_value);
    return rules;
  }
  
  if(_pmath_pattern_is_const(key)) {
    i = dispatch_table_lookup(tab_ptr, key, NULL);
    if(i == 0) {
      pmath_unref(tab);
      return append_rule(rules, key, new_value);
    }
    else {
      if(pmath_same(new_value, PMATH_UNDEFINED)) {
        pmath_unref(key);
        pmath_unref(tab);
        rules = pmath_expr_set_item(rules, i, PMATH_UNDEFINED);
        return pmath_expr_remove_all(rules, PMATH_UNDEFINED);
      }
      
      pmath_unref(key);
      return replace_rule_rhs(rules, tab, i, new_value);
    }
  }

  len = pmath_expr_length(rules);
  entry = tab_ptr->entries;
  while(entry != tab_ptr->entries + len) {
    if(entry->literal_turn_or_zero > 0) { // literal pattern, cannot match
      entry = get_next_slice(entry);
    }
    else if(pmath_equals(entry->key, key)) {
      i = 1 + (entry - tab_ptr->entries);
      
      if(pmath_same(new_value, PMATH_UNDEFINED)) {
        pmath_unref(tab);
        pmath_unref(key);
        rules = pmath_expr_set_item(rules, i, PMATH_UNDEFINED);
        return pmath_expr_remove_all(rules, PMATH_UNDEFINED);
      }
      
      pmath_unref(key);
      return replace_rule_rhs(rules, tab, i, new_value);
    }
    // TODO: compare patterns instead of just appending to the end?
  }

  pmath_unref(tab);
  return append_rule(rules, key, new_value);
}

//{ module init/done ...

PMATH_PRIVATE pmath_bool_t _pmath_dispatch_tables_init(void) {
  
  dispatch_table_cache = pmath_ht_create(&dispatch_table_cache_class, 10);
  if(!dispatch_table_cache) goto FAIL_CACHE;
  
  _pmath_init_special_type(
    PMATH_TYPE_SHIFT_DISPATCH_TABLE,
    cmp_dispatch_tables,
    hash_dispatch_table,
    destroy_dispatch_table,
    equal_dispatch_tables,
    NULL);

  return TRUE;

FAIL_CACHE:
  return FALSE;
}

PMATH_PRIVATE void _pmath_dispatch_tables_done(void) {
  pmath_ht_destroy(dispatch_table_cache);
  dispatch_table_cache = NULL;
}

//} ... module init/done

static void dispatch_table_cache_entry_destructor(void *entry) {
  struct _pmath_dispatch_table_t *tab = entry;
  
  if(PMATH_LIKELY(_pmath_refcount_ptr(&tab->inherited) == 0)) {
    size_t i;
    
    //pmath_debug_print_object("[free dispatch table for ", tab->all_keys, "]\n");
    
    pmath_ht_destroy(tab->literal_entries);
    
    for(i = pmath_expr_length(tab->all_keys); i > 0; --i)
      pmath_unref(tab->entries[i-1].key);
      
    pmath_unref(tab->all_keys);
    pmath_mem_free(tab);
  }
  else {
    pmath_debug_print("[ignored attempt to free dispatch table cache entry %p with refcount > 0]", tab);
  }
}

static void destroy_dispatch_table(pmath_t a) {
  struct _pmath_dispatch_table_t *tab = (void*)PMATH_AS_PTR(a);
  struct _pmath_dispatch_table_t *cached = NULL;
  
  //pmath_debug_print_object("[orphaned dispatch table for ", tab->all_keys, "]\n");
  
  pmath_atomic_lock(&dispatch_table_cache_lock);
  {
    if(PMATH_UNLIKELY(_pmath_refcount_ptr(&tab->inherited)) != 0) {
      pmath_debug_print("[dispatch table %p revived just before burial]", tab);
      tab = NULL; // do not free below
    }
    else {
      cached = pmath_ht_remove(dispatch_table_cache, &tab->all_keys);
      if(PMATH_LIKELY(tab == cached)) {
        cached = NULL; // prevent double-free
      }
      else {
        pmath_debug_print("[dispatch table cache contains %p instead of %p]", cached, tab);
        if(cached) { // undo the removal
          cached = pmath_ht_insert(dispatch_table_cache, cached);
          // Now cached is NULL on success or remains unchanged on failure. 
          // Technically, failure should not be possible, because the insertion should not 
          // require a memory allocation.
        }
      }
    } 
  }
  pmath_atomic_unlock(&dispatch_table_cache_lock);
  
  if(tab)
    dispatch_table_cache_entry_destructor(tab);
  if(cached)
    dispatch_table_cache_entry_destructor(cached);
}

static unsigned int dispatch_table_cache_entry_hash(void *entry) {
  struct _pmath_dispatch_table_t *tab = entry;
  return pmath_hash(tab->all_keys);
}

static unsigned int dispatch_table_cache_key_hash(void *key) {
  return pmath_hash(*(pmath_t*)key);
}

static unsigned int hash_dispatch_table(pmath_t a) {
  return dispatch_table_cache_entry_hash(PMATH_AS_PTR(a));
}

static int cmp_dispatch_tables(pmath_t a, pmath_t b) {
  struct _pmath_dispatch_table_t *tab_a = (void*)PMATH_AS_PTR(a);
  struct _pmath_dispatch_table_t *tab_b = (void*)PMATH_AS_PTR(b);
  
  return pmath_compare(tab_a->all_keys, tab_b->all_keys);
}

static pmath_bool_t dispatch_table_cache_entry_keys_equal(void *entry1, void *entry2) {
  struct _pmath_dispatch_table_t *tab1 = entry1;
  struct _pmath_dispatch_table_t *tab2 = entry2;
  return pmath_equals(tab1->all_keys, tab2->all_keys);
}

static pmath_bool_t dispatch_table_cache_entry_equals_key(void *entry, void *key) {
  struct _pmath_dispatch_table_t *tab = entry;
  return pmath_equals(tab->all_keys, *(pmath_t*)key);
}

static pmath_bool_t equal_dispatch_tables(pmath_t a, pmath_t b) {
  return dispatch_table_cache_entry_keys_equal(PMATH_AS_PTR(a), PMATH_AS_PTR(b));
}