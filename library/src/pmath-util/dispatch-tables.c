#include <pmath-util/dispatch-tables-private.h>

#include <pmath-builtins/all-symbols-private.h>

#include <pmath-core/expressions-private.h>

#include <pmath-language/patterns-private.h>

#include <pmath-util/concurrency/atomic.h>
#include <pmath-util/debug.h>
#include <pmath-util/helpers.h>
#include <pmath-util/hash/incremental-hash-private.h>
#include <pmath-util/memory.h>
#include <pmath-util/symbol-values-private.h>

#include <string.h>


struct dispatch_lookup_info_t {
  pmath_t key;
  unsigned int turn_or_zero;
};

struct dispatch_table_cache_search_t {
  pmath_t      keys_or_rules;
  pmath_bool_t implicit_keys;
};

static struct _pmath_dispatch_table_t *find_dispatch_table(pmath_expr_t rules); // rules wont be freed
static struct _pmath_dispatch_table_t *create_dispatch_table_for_keys(pmath_expr_t keys); // keys will be freed
static struct _pmath_dispatch_table_t *get_dispatch_table_for_rules(pmath_expr_t rules); // rules wont be freed

static void disown_dispatch_table(pmath_t a);
static unsigned int hash_dispatch_table(pmath_t a);
static int cmp_dispatch_tables(pmath_t a, pmath_t b);
static pmath_bool_t equal_dispatch_tables(pmath_t a, pmath_t b);

extern pmath_symbol_t pmath_System_List;
extern pmath_symbol_t pmath_System_PatternSequence;
extern pmath_symbol_t pmath_System_Rule;
extern pmath_symbol_t pmath_System_RuleDelayed;

//{ dispatch table slice ...

/// Extra data of the _pmath_dispatch_table_t::literal_entries hash table.
struct _pmath_dispatch_ht_extra_t {
  struct _pmath_dispatch_table_t *owner;
};

static struct _pmath_dispatch_ht_extra_t *dispatch_ht_extra(pmath_hashtable_t ht) {
  return (void*)ht;
}

static void noop(pmath_hashtable_t ht, void *e) {
}

static unsigned int dispatch_entry_hash(pmath_hashtable_t ht, void *e) {
  struct _pmath_dispatch_ht_extra_t *extra = dispatch_ht_extra(ht);
  struct _pmath_dispatch_table_t    *tab   = extra->owner;
  struct _pmath_dispatch_entry_t    *entry = (struct _pmath_dispatch_entry_t *)e;
  
  if(!tab) {
    pmath_debug_print("[dispatch_entry_hash(%p, %p): ht not yet fully initialized]\n", ht, e);
    return 0;
  }
  
  size_t num_entries = pmath_expr_length(tab->all_keys);
  assert((uintptr_t)&tab->entries[0] <= (uintptr_t)entry && (uintptr_t)entry < (uintptr_t)&tab->entries[num_entries] && "only support entries from owned table");
  
  //return pmath_hash(entry->key);
  
  size_t i = entry - &tab->entries[0];
  
  pmath_t key = pmath_expr_get_item(tab->all_keys, i + 1);
  //prepare_const_pattern(&key); // that is unfortunate :-(
  if(entry->is_const_pattern_sequence)
    key = pmath_expr_set_item(key, 0, PMATH_MAGIC_PATTERN_SEQUENCE);
  
  unsigned int hash = pmath_hash(key);
  pmath_unref(key);
  return hash;
}

static pmath_bool_t dispatch_entry_keys_equal(pmath_hashtable_t ht, void *e1, void *e2) {
  struct _pmath_dispatch_ht_extra_t *extra  = dispatch_ht_extra(ht);
  struct _pmath_dispatch_table_t    *tab    = extra->owner;
  struct _pmath_dispatch_entry_t    *entry1 = e1;
  struct _pmath_dispatch_entry_t    *entry2 = e2;
  
  if(!tab) {
    pmath_debug_print("[dispatch_entry_keys_equal(%p, %p, %p): ht not yet fully initialized]\n", ht, e1, e2);
    return FALSE;
  }
  
  size_t num_entries = pmath_expr_length(tab->all_keys);
  assert((uintptr_t)&tab->entries[0] <= (uintptr_t)entry1 && (uintptr_t)entry1 < (uintptr_t)&tab->entries[num_entries] && "only support entries from owned table");
  assert((uintptr_t)&tab->entries[0] <= (uintptr_t)entry2 && (uintptr_t)entry2 < (uintptr_t)&tab->entries[num_entries] && "only support entries from owned table");
  
  if(entry1->literal_turn_or_zero != entry2->literal_turn_or_zero)
    return FALSE;
  
  //return pmath_equals(entry1->key, entry2->key);
  
  size_t i1 = entry1 - &tab->entries[0];
  size_t i2 = entry2 - &tab->entries[0];
  
  pmath_t key1 = pmath_expr_get_item(tab->all_keys, i1 + 1);
  pmath_t key2 = pmath_expr_get_item(tab->all_keys, i2 + 1);
  
  pmath_bool_t eq = pmath_equals(key1, key2);
  
  pmath_unref(key1);
  pmath_unref(key2);
  return eq;
}

static unsigned int dispatch_lookup_info_hash(pmath_hashtable_t ht, void *k) {
  struct dispatch_lookup_info_t *info = k;
  return pmath_hash(info->key);
}

static pmath_bool_t dispatch_entry_equals_lookup_key(pmath_hashtable_t ht, void *e, void *k) {
  struct _pmath_dispatch_ht_extra_t *extra  = dispatch_ht_extra(ht);
  struct _pmath_dispatch_table_t    *tab    = extra->owner;
  struct _pmath_dispatch_entry_t    *entry  = e;
  struct dispatch_lookup_info_t     *info   = k;
  
  if(!tab) {
    pmath_debug_print("[dispatch_entry_equals_lookup_key(%p, %p, %p): ht not yet fully initialized]\n", ht, e, k);
    return FALSE;
  }
  
  size_t num_entries = pmath_expr_length(tab->all_keys);
  assert((uintptr_t)&tab->entries[0] <= (uintptr_t)entry && (uintptr_t)entry < (uintptr_t)&tab->entries[num_entries] && "only support entries from owned table");
  
  if(info->turn_or_zero) {
    if(entry->literal_turn_or_zero != info->turn_or_zero)
      return FALSE;
  }
  
  //return pmath_equals(entry->key, info->key);
  
  size_t i = entry - &tab->entries[0];
  
  pmath_t entry_key = pmath_expr_get_item(tab->all_keys, i + 1);
  //prepare_const_pattern(&entry_key); // that is unfortunate :-(
  if(entry->is_const_pattern_sequence)
    entry_key = pmath_expr_set_item(entry_key, 0, PMATH_MAGIC_PATTERN_SEQUENCE);
  
  pmath_bool_t eq = pmath_equals(entry_key, info->key);
  pmath_unref(entry_key);
  
  return eq;
}

static const pmath_ht_class_ex_t dispatch_entries_ht_class = {
  .num_extra_bytes  = sizeof(struct _pmath_dispatch_ht_extra_t),
  .entry_destructor = noop,
  .entry_hash       = dispatch_entry_hash,
  .entry_keys_equal = dispatch_entry_keys_equal,
  .key_hash         = dispatch_lookup_info_hash,
  .entry_equals_key = dispatch_entry_equals_lookup_key,
};

//} ... dispatch table slice

//{ dispatch table cache ...

static pmath_atomic_t dispatch_table_cache_lock = PMATH_ATOMIC_STATIC_INIT;

// Maps dispatch_table_cache_search_t* keys to struct _pmath_dispatch_table_t* entries:
static pmath_hashtable_t dispatch_table_cache;

#define DISPATCH_TABLE_LIMBO_SIZE       8   // Must be a power of two
#define DISPATCH_TABLE_LIMBO_SIZE_MASK  (DISPATCH_TABLE_LIMBO_SIZE - 1)

static struct _pmath_dispatch_table_t *dispatch_table_limbo[DISPATCH_TABLE_LIMBO_SIZE];
static size_t dispatch_table_limbo_next = 0;

static pmath_t      dispatch_table_cache_key_at(struct dispatch_table_cache_search_t *search, size_t i);

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

static size_t unsafe_find_in_limbo(struct _pmath_dispatch_table_t *disp) {
  // The limbo is small enough for a linear search. Otherwise we would have to introduce a 
  // _pmath_dispatch_table_t::limbo_index member.
  size_t i;
  for(i = 0; i < DISPATCH_TABLE_LIMBO_SIZE; ++i)
    if(dispatch_table_limbo[i] == disp)
      return i+1;
  
  return 0;
}

struct _pmath_dispatch_table_t *find_dispatch_table(pmath_expr_t rules) {
  struct _pmath_dispatch_table_t *result = NULL;
  struct dispatch_table_cache_search_t search;
  search.implicit_keys = TRUE;
  search.keys_or_rules = rules;
  
  pmath_atomic_lock(&dispatch_table_cache_lock);
  {
    result = pmath_ht_search(dispatch_table_cache, &search);
    if(result) {
      _pmath_ref_ptr((struct _pmath_t*)result);
      if(_pmath_refcount_ptr((struct _pmath_t*)result) == 1) {
        size_t i = unsafe_find_in_limbo(result);
        if(i--) {
          size_t last = (dispatch_table_limbo_next-1) & DISPATCH_TABLE_LIMBO_SIZE_MASK;
          dispatch_table_limbo[i] = dispatch_table_limbo[last];
          dispatch_table_limbo[last] = NULL;
          dispatch_table_limbo_next = last;
//          pmath_debug_print("[found zombie dispatch table %p in limbo]\n", result);
        }
        else {
//          pmath_debug_print("[zombie dispatch table %p ", result);
//          pmath_debug_print_object("not from limbo: ", rules, "]\n");
        }
      }
    }
  }
  pmath_atomic_unlock(&dispatch_table_cache_lock);
  
  return result;
}

static struct _pmath_dispatch_table_t *get_dispatch_table_for_rules(pmath_expr_t rules) {
  struct _pmath_dispatch_table_t *tab;
  struct _pmath_dispatch_table_t *old;
  pmath_t keys;
  size_t len;
  
  tab = find_dispatch_table(rules);
  if(tab) {
//    pmath_debug_print("[use cached dispatch table %p (%d refs) ", tab, (int)_pmath_refcount_ptr(&tab->inherited));
//    pmath_debug_print_object("for ", rules, "]\n");
    return tab;
  }
  
  len = pmath_expr_length(rules);
  keys = pmath_expr_new(pmath_ref(pmath_System_List), len);
  for(; len > 0; --len) {
    pmath_t rule = pmath_expr_get_item(rules, len);
    pmath_t lhs = pmath_expr_get_item(rule, 1);
    pmath_unref(rule);
    keys = pmath_expr_set_item(keys, len, lhs);
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

static pmath_bool_t is_const_pattern_sequence(pmath_t key) {
  if(!pmath_is_expr_of(key, pmath_System_PatternSequence))
    return FALSE;
  
  size_t i = pmath_expr_length(key);
  for(; i > 0; --i) {
    pmath_t sub = pmath_expr_get_item(key, i);
    if(!_pmath_pattern_is_const(sub)) {
      pmath_unref(sub);
      return FALSE;
    }
    pmath_unref(sub);
  }
  
  return FALSE;
}

static pmath_bool_t prepare_const_pattern(pmath_t *key) {
  if(is_const_pattern_sequence(*key)) {
    *key = pmath_expr_set_item(*key, 0, PMATH_MAGIC_PATTERN_SEQUENCE);
    return TRUE;
  }
  
  return _pmath_pattern_is_const(*key);
}

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
  literal_entries = pmath_ht_create_ex(&dispatch_entries_ht_class, (unsigned)num_keys);
  if(!literal_entries) {
    pmath_unref(keys);
    return NULL;
  }
  
  key_to_turn = pmath_ht_create_ex(&dispatch_entries_ht_class, (unsigned)num_keys);
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
  
  dispatch_ht_extra(literal_entries)->owner = tab;
  dispatch_ht_extra(key_to_turn)->owner     = tab;
  
//  pmath_debug_print("[new dispatch table %p ", tab);
//  pmath_debug_print_object("for ", keys, "]\n");
  
  tab->all_keys = keys;
  tab->literal_entries = literal_entries;
  current_slice_start = &tab->entries[0];
  
  lookup_no_turn.turn_or_zero = 0;
  
  for(i = 1; i <= num_keys; ++i) {
    struct _pmath_dispatch_entry_t *entry = &tab->entries[i-1];
    pmath_t key = pmath_expr_get_item(tab->all_keys, i);
    entry->is_const_pattern_sequence = is_const_pattern_sequence(key);
    
    if(entry->is_const_pattern_sequence || _pmath_pattern_is_const(key)) {
      struct _pmath_dispatch_entry_t *latest_turn;
      
      entry->next_slice_or_slice_start = current_slice_start;
      
      if(entry->is_const_pattern_sequence) {
        lookup_no_turn.key = pmath_ref(key);
        lookup_no_turn.key = pmath_expr_set_item(lookup_no_turn.key, 0, PMATH_MAGIC_PATTERN_SEQUENCE);
      }
      else
        lookup_no_turn.key = key;
      
      latest_turn = pmath_ht_search(key_to_turn, &lookup_no_turn);
      
      if(entry->is_const_pattern_sequence)
        pmath_unref(lookup_no_turn.key);
      
      entry->literal_turn_or_zero = latest_turn ? latest_turn->literal_turn_or_zero : 0;
      latest_turn = pmath_ht_insert(key_to_turn, entry);
      entry->literal_turn_or_zero++;
      
      entry = pmath_ht_insert(literal_entries, entry);
      if(PMATH_UNLIKELY(entry)) {
        pmath_debug_print("[failed to insert entry %p for %d", entry, (int)i);
        pmath_debug_print_object(" of ", keys, "]\n");
      }
    }
    else {
      current_slice_start->next_slice_or_slice_start = entry;
      entry->next_slice_or_slice_start = entry + 1;
      current_slice_start = entry + 1;
      
      entry->literal_turn_or_zero = 0;
    }
    
    pmath_unref(key);
  }
  
  if(current_slice_start < tab->entries + num_keys)
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

PMATH_PRIVATE size_t _pmath_dispatch_table_lookup(
  struct _pmath_dispatch_table_t *table, // won't be freed
  pmath_t key,                           // won't be freed
  pmath_t *rules_in_rhs_out,             // will be freed
  pmath_bool_t literal
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
        struct _pmath_dispatch_entry_t *next = get_next_slice(entry);
        assert(next > entry);
        
        last_index = 1 + (next - table->entries);
        continue;
      }
      
      assert(entry->next_slice_or_slice_start == entry + 1);
      
      pmath_t entry_key = pmath_expr_get_item(table->all_keys, last_index);
      
      if(rules_in_rhs_out) {
        pmath_t rule = pmath_expr_get_item(*rules_in_rhs_out, last_index);
        pmath_t rhs = pmath_expr_get_item(rule, 2);
        pmath_bool_t is_match;
        pmath_unref(rule);
        
        is_match = literal ? pmath_equals(key, entry_key) : _pmath_pattern_match(key, pmath_ref(entry_key), &rhs);
        if(is_match) {
          pmath_unref(entry_key);
          pmath_unref(*rules_in_rhs_out);
          *rules_in_rhs_out = rhs;
          return last_index;
        }
        pmath_unref(rhs);
      }
      else {
        pmath_bool_t is_match = literal ? pmath_equals(key, entry_key) : _pmath_pattern_match(key, pmath_ref(entry_key), NULL);
        if(is_match) {
          pmath_unref(entry_key);
          return last_index;
        }
      }
      
      pmath_unref(entry_key);
      ++last_index;
    }
    
    if(found_index > num_keys)
      break;
    
    last_index = found_index;
    if(rules_in_rhs_out) {
      pmath_t rule = pmath_expr_get_item(*rules_in_rhs_out, last_index);
      pmath_t rhs = pmath_expr_get_item(rule, 2);
      pmath_unref(rule);
      
      if(literal || _pmath_pattern_match(PMATH_NULL, PMATH_NULL, &rhs)) {
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

PMATH_API pmath_bool_t pmath_is_list_of_rules(pmath_t obj) {
  pmath_dispatch_table_t disp = _pmath_rules_need_dispatch_table(obj);
  pmath_unref(disp);
  return !pmath_same(disp, PMATH_NULL);
}

PMATH_PRIVATE pmath_dispatch_table_t _pmath_rules_need_dispatch_table(pmath_t expr) {
  pmath_dispatch_table_t tab;
  size_t len;
  
  if(!pmath_is_expr(expr))
    return PMATH_NULL;
  
  tab = _pmath_expr_get_dispatch_table(expr);
  if(!pmath_is_null(tab))
    return tab;
  
  if(!pmath_is_expr_of(expr, pmath_System_List))
    return PMATH_NULL;
  
  for(len = pmath_expr_length(expr); len > 0; --len) {
    pmath_t rule = pmath_expr_get_item(expr, len);
    
    if(!pmath_is_rule(rule)) {
      pmath_unref(rule);
      return PMATH_NULL;
    }
    
    pmath_unref(rule);
  }
  
  tab = PMATH_FROM_PTR(get_dispatch_table_for_rules(expr));
  _pmath_expr_attach_dispatch_table(expr, pmath_ref(tab));
  return tab;
}

PMATH_API pmath_bool_t pmath_rules_lookup(pmath_t rules, pmath_t key, pmath_t *result) {
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
  i = _pmath_dispatch_table_lookup(tab_ptr, key, &rules_in_rhs_out, FALSE);
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

PMATH_PRIVATE pmath_t _pmath_rules_find_rule(pmath_t rules, pmath_t lhs, pmath_bool_t literal) {
  pmath_dispatch_table_t tab = _pmath_rules_need_dispatch_table(rules);
  struct _pmath_dispatch_table_t *tab_ptr = (void*)PMATH_AS_PTR(tab);
  size_t i;
  
  if(!tab_ptr)
    return PMATH_NULL;
  
  i = _pmath_dispatch_table_lookup(tab_ptr, lhs, NULL, literal);  
  pmath_unref(tab);
  
  if(i) 
    return pmath_expr_get_item(rules, i);
  
  return PMATH_NULL;
}

static pmath_t replace_rule_rhs(
    pmath_t                  rules, // will be freed
    pmath_dispatch_table_t   tab,   // will be freed
    size_t                   i, 
    pmath_bool_t           (*callback)(pmath_t*, pmath_bool_t, void*), 
    void                    *callback_context
) {
  pmath_t rule = pmath_expr_extract_item(rules, i);
  pmath_bool_t old_no_delay = pmath_is_expr_of(rule, pmath_System_Rule);
  pmath_t value = pmath_expr_extract_item(rule, 2);
  
  pmath_bool_t new_no_delay = callback(&value, old_no_delay, callback_context);
  
  if(pmath_same(value, PMATH_UNDEFINED)) {
    pmath_unref(rule);
    pmath_unref(tab);
    rules = pmath_expr_set_item(rules, i, PMATH_UNDEFINED);
    return pmath_expr_remove_all(rules, PMATH_UNDEFINED);
  }
  
  if(old_no_delay != new_no_delay) 
    rule = pmath_expr_set_item(rule, 0, pmath_ref(new_no_delay ? pmath_System_Rule : pmath_System_RuleDelayed));
  
  rule = pmath_expr_set_item(rule, 2, value);
  if(pmath_is_null(rule)) {
    pmath_unref(tab);
    pmath_unref(rules);
    return PMATH_NULL;
  }
  
  rules = pmath_expr_set_item(rules, i, rule);
  _pmath_expr_attach_dispatch_table(rules, tab);
  return rules;
}

static pmath_t append_rule(
    pmath_t        rules, // will be freed
    pmath_t        key,   // will be freed
    pmath_bool_t (*callback)(pmath_t*, pmath_bool_t, void*), 
    void          *callback_context
) {
  pmath_t value = PMATH_UNDEFINED;
  pmath_bool_t no_delay = callback(&value, TRUE, callback_context);
  pmath_t rule;
      
  if(pmath_same(value, PMATH_UNDEFINED)) {
    pmath_unref(key);
    return rules;
  }
  
  rule = pmath_expr_new_extended(
           pmath_ref(no_delay ? pmath_System_Rule : pmath_System_RuleDelayed),
           2,
           key,
           value);
  return pmath_expr_append(rules, 1, rule);
}

PMATH_API pmath_t pmath_rules_modify(
  pmath_t        rules, 
  pmath_t        key, 
  pmath_bool_t (*callback)(pmath_t*, pmath_bool_t, void*), 
  void          *callback_context
) {
  pmath_dispatch_table_t tab = _pmath_rules_need_dispatch_table(rules);
  struct _pmath_dispatch_table_t *tab_ptr = (void*)PMATH_AS_PTR(tab);
  size_t i, len;
  struct _pmath_dispatch_entry_t *entry;
  
  if(!tab_ptr) {
    pmath_unref(key);
    return rules;
  }
  
  if(prepare_const_pattern(&key)) {
    i = _pmath_dispatch_table_lookup(tab_ptr, key, NULL, FALSE);
    if(i == 0) {
      pmath_unref(tab);
      if(pmath_is_expr_of(key, PMATH_MAGIC_PATTERN_SEQUENCE))
        key = pmath_expr_set_item(key, 0, pmath_ref(pmath_System_PatternSequence));
      
      return append_rule(rules, key, callback, callback_context);
    }
    else {
      pmath_unref(key);
      return replace_rule_rhs(rules, tab, i, callback, callback_context);
    }
  }

  len = pmath_expr_length(rules);
  entry = tab_ptr->entries;
  while(entry != tab_ptr->entries + len) {
    if(entry->literal_turn_or_zero > 0) { // literal pattern, cannot match
      entry = get_next_slice(entry);
    }
    else {
      size_t i = entry - &tab_ptr->entries[0];
      pmath_t entry_key = pmath_expr_get_item(tab_ptr->all_keys, i + 1);
      if(pmath_equals(entry_key, key)) {
        i = 1 + (entry - tab_ptr->entries);
        pmath_unref(key);
        pmath_unref(entry_key);
        return replace_rule_rhs(rules, tab, i, callback, callback_context);
      }
      pmath_unref(entry_key);
      // TODO: compare patterns instead of just appending to the end?
    }
  }

  pmath_unref(tab);
  return append_rule(rules, key, callback, callback_context);
}

//{ module init/done ...

PMATH_PRIVATE void _pmath_dispatch_table_filter_limbo(
  pmath_bool_t (*keep_callback)(const struct _pmath_dispatch_table_t*, void*),
  void          *closure
) {
  struct _pmath_dispatch_table_t *old_limbo[DISPATCH_TABLE_LIMBO_SIZE];
  size_t i;
  size_t num_del;
  
  PMATH_STATIC_ASSERT(sizeof(dispatch_table_limbo) == sizeof(old_limbo));
  
  pmath_atomic_lock(&dispatch_table_cache_lock);
  {
    memcpy(old_limbo, dispatch_table_limbo, sizeof(dispatch_table_limbo));
    memset(dispatch_table_limbo, 0, sizeof(dispatch_table_limbo));
    dispatch_table_limbo_next = 0;
  }
  pmath_atomic_unlock(&dispatch_table_cache_lock);
  
  num_del = 0;
  for(i = 0; i < DISPATCH_TABLE_LIMBO_SIZE; ++i) {
    struct _pmath_dispatch_table_t *table = old_limbo[i];
    if(table) {
      // note that table normaly has refcount == 0 here.
      if(keep_callback(table, closure)) {
//        pmath_debug_print("[keep dispatch table %p in limbo (%d) via disown ...]\n", table, (int)_pmath_refcount_ptr(&table->inherited));
        _pmath_ref_ptr(&table->inherited);
        _pmath_unref_ptr(&table->inherited); // will eventually return the table to limbo
        old_limbo[i] = NULL;
      }
      else
        ++num_del;
    }
  }
  
  if(num_del == 0)
    return;
  
  pmath_atomic_lock(&dispatch_table_cache_lock);
  {
    size_t remaining = num_del;
    for(i = 0; i < DISPATCH_TABLE_LIMBO_SIZE && remaining; ++i) {
      struct _pmath_dispatch_table_t *table = old_limbo[i];
      if(table) {
        struct dispatch_table_cache_search_t search;
        search.keys_or_rules = table->all_keys;
        search.implicit_keys = FALSE;
        old_limbo[i] = pmath_ht_remove(dispatch_table_cache, &search);
        --remaining;
      }
    }
  }
  pmath_atomic_unlock(&dispatch_table_cache_lock);
  
  for(i = 0; i < DISPATCH_TABLE_LIMBO_SIZE && num_del; ++i) {
    if(old_limbo[i]) {
      --num_del;
      dispatch_table_cache_entry_destructor(old_limbo[i]);
    }
  }
}

PMATH_PRIVATE void _pmath_dispatch_tables_memory_panic(void) {
  struct _pmath_dispatch_table_t *old_limbo[DISPATCH_TABLE_LIMBO_SIZE];
  size_t i;
  
  PMATH_STATIC_ASSERT(sizeof(dispatch_table_limbo) == sizeof(old_limbo));
  
  pmath_atomic_lock(&dispatch_table_cache_lock);
  {
    memcpy(old_limbo, dispatch_table_limbo, sizeof(dispatch_table_limbo));
    memset(dispatch_table_limbo, 0, sizeof(dispatch_table_limbo));
    dispatch_table_limbo_next = 0;
    
    for(i = 0; i < DISPATCH_TABLE_LIMBO_SIZE; ++i) {
      if(old_limbo[i]) {
        struct dispatch_table_cache_search_t search;
        search.keys_or_rules = old_limbo[i]->all_keys;
        search.implicit_keys = FALSE;
        old_limbo[i] = pmath_ht_remove(dispatch_table_cache, &search);
      }
    }
  }
  pmath_atomic_unlock(&dispatch_table_cache_lock);
  
  for(i = 0; i < DISPATCH_TABLE_LIMBO_SIZE; ++i) {
    if(old_limbo[i])
      dispatch_table_cache_entry_destructor(old_limbo[i]);
  }
}

PMATH_PRIVATE pmath_bool_t _pmath_dispatch_tables_init(void) {
  memset(dispatch_table_limbo, 0, sizeof(dispatch_table_limbo));
  dispatch_table_cache = pmath_ht_create(&dispatch_table_cache_class, 10);
  if(!dispatch_table_cache) goto FAIL_CACHE;
  
  _pmath_init_special_type(
    PMATH_TYPE_SHIFT_DISPATCH_TABLE,
    cmp_dispatch_tables,
    hash_dispatch_table,
    disown_dispatch_table,
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
//    pmath_debug_print("[free dispatch table %p ]\n", tab);
    
    pmath_ht_destroy(tab->literal_entries);
    
    pmath_unref(tab->all_keys);
    pmath_mem_free(tab);
  }
  else {
    pmath_debug_print("[ignored attempt to free dispatch table cache entry %p with refcount > 0]\n", tab);
  }
}

static void disown_dispatch_table(pmath_t a) {
  struct _pmath_dispatch_table_t *tab = (void*)PMATH_AS_PTR(a);
  struct _pmath_dispatch_table_t *cached = NULL;
  
//  pmath_debug_print("[disowned dispatch table %p ]\n", tab);
  
  pmath_atomic_lock(&dispatch_table_cache_lock);
  {
    if(PMATH_UNLIKELY(_pmath_refcount_ptr(&tab->inherited)) != 0) {
      pmath_debug_print("[dispatch table %p revived just before burial]\n", tab);
      tab = NULL; // do not free below
    }
    else {
      size_t i = (dispatch_table_limbo_next++) & DISPATCH_TABLE_LIMBO_SIZE_MASK;
      struct _pmath_dispatch_table_t *old_zombie = dispatch_table_limbo[i];
      dispatch_table_limbo[i] = tab;
      tab = old_zombie;
      
      if(tab) {
        struct dispatch_table_cache_search_t search;
        search.keys_or_rules = tab->all_keys;
        search.implicit_keys = FALSE;
//        pmath_debug_print("[dispatch table limbo contained old_zomie %p]\n", tab);
        cached = pmath_ht_remove(dispatch_table_cache, &search);
        if(PMATH_LIKELY(tab == cached)) {
          cached = NULL; // prevent double-free
        }
        else {
          pmath_debug_print("[dispatch table cache contains %p instead of %p]\n", cached, tab);
          if(cached) { // undo the removal
            cached = pmath_ht_insert(dispatch_table_cache, cached);
            // Now cached is NULL on success or remains unchanged on failure. 
            // Technically, failure should not be possible, because the insertion should not 
            // require a memory allocation.
          }
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
  unsigned int hash = 17;
  size_t i;

  for(i = pmath_expr_length(tab->all_keys); i > 0; --i) {
    pmath_t item = pmath_expr_get_item(tab->all_keys, i);
    unsigned int h = pmath_hash(item);
    hash = _pmath_incremental_hash(&h, sizeof(h), hash);
    pmath_unref(item);
  }
  
  return hash;
}

static pmath_t dispatch_table_cache_key_at(struct dispatch_table_cache_search_t *search, size_t i) {
  if(!pmath_is_expr(search->keys_or_rules))
    return PMATH_NULL;
  
  if(search->implicit_keys) {
    pmath_t rule = pmath_expr_get_item(search->keys_or_rules, i);
    if(pmath_is_expr(rule)) {
      pmath_t key = pmath_expr_get_item(rule, 1);
      pmath_unref(rule);
      return key;
    }
    pmath_unref(rule);
    return PMATH_NULL;
  }
  else {
    return pmath_expr_get_item(search->keys_or_rules, i);
  }
}

static unsigned int dispatch_table_cache_key_hash(void *key) {
  struct dispatch_table_cache_search_t *search = key;
  unsigned int hash = 17;
  size_t i;
  
  if(pmath_is_expr(search->keys_or_rules)) {
    for(i = pmath_expr_length(search->keys_or_rules); i > 0; --i) {
      pmath_t item = dispatch_table_cache_key_at(search, i);
      unsigned int h = pmath_hash(item);
      hash = _pmath_incremental_hash(&h, sizeof(h), hash);
      pmath_unref(item);
    }
  }
  
  return hash;
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
  struct _pmath_dispatch_table_t       *tab    = entry;
  struct dispatch_table_cache_search_t *search = key;
  
  if(pmath_is_expr(search->keys_or_rules)) {
    size_t i = pmath_expr_length(search->keys_or_rules);
    
    if(i != pmath_expr_length(tab->all_keys))
      return FALSE;
    
    for(; i > 0; --i) {
      pmath_t search_item = dispatch_table_cache_key_at(search, i);
      pmath_t tab_item    = pmath_expr_get_item(tab->all_keys, i);
      
      if(!pmath_equals(search_item, tab_item)) {
        pmath_unref(tab_item);
        pmath_unref(search_item);
        return FALSE;
      }
      
      pmath_unref(tab_item);
      pmath_unref(search_item);
    }
    
    return TRUE;
  }
  else
    return FALSE;
}

static pmath_bool_t equal_dispatch_tables(pmath_t a, pmath_t b) {
  return dispatch_table_cache_entry_keys_equal(PMATH_AS_PTR(a), PMATH_AS_PTR(b));
}
