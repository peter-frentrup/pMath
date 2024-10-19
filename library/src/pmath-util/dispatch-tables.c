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

enum dispatch_cache_search_mode_t {
  SEARCH_BY_DISPATCH_PTR   = 0,
  SEARCH_BY_KEY_LIST       = 1,
  SEARCH_BY_LIST_OF_RULES  = 2,
};

struct dispatch_table_cache_search_t {
  enum dispatch_cache_search_mode_t mode;
  union {
    struct _pmath_dispatch_table_extra_data_t *ptr;
    pmath_expr_t keys;
    pmath_expr_t rules;
  } u;
};

struct _pmath_dispatch_entry_t {
  unsigned int literal_turn_or_zero;
  pmath_bool_t is_const_pattern_sequence;
  // ... padding :-/ ...
  struct _pmath_dispatch_entry_t *next_slice_or_slice_start;
};

struct _pmath_dispatch_table_extra_data_t {
  struct _pmath_custom_expr_data_t base;     // let capacity := expr.internals.length + 1, the number of expr.internals.items[]
 
  size_t used_length;                        // used_length <= capacity
  unsigned hash_for_cache;
  pmath_hashtable_t literal_entries;
  struct _pmath_dispatch_entry_t entries[1]; // capacity many elements
};

static _pmath_dispatch_table_expr_t *find_dispatch_table(pmath_expr_t rules); // rules wont be freed
pmath_bool_t finish_create_dispatch_table(_pmath_dispatch_table_expr_t *tab, size_t num_keys);
static _pmath_dispatch_table_expr_t *get_dispatch_table_for_rules(pmath_expr_t rules); // rules wont be freed

extern pmath_symbol_t pmath_System_List;
extern pmath_symbol_t pmath_System_PatternSequence;
extern pmath_symbol_t pmath_System_Rule;
extern pmath_symbol_t pmath_System_RuleDelayed;

//{ custom expr API for dispatch table ...

static void         dispatch_expr_destroy_data(           struct _pmath_custom_expr_data_t *_data);
static pmath_bool_t dispatch_expr_try_prevent_destruction(struct _pmath_custom_expr_t *tab);
static size_t       dispatch_expr_get_length(             struct _pmath_custom_expr_t *e);
static pmath_t      dispatch_expr_get_item(               struct _pmath_custom_expr_t *e, size_t i);
static pmath_bool_t dispatch_expr_try_item_equals(        struct _pmath_custom_expr_t *e, size_t i, pmath_t expected_item, pmath_bool_t *result);

#define DISPATCH_EXPR_EXTRA(EXPR_PTR)      ((struct _pmath_dispatch_table_extra_data_t*)PMATH_CUSTOM_EXPR_DATA(EXPR_PTR))

#define EXTRA_SIZE_FOR_CAPACITY(CAPACITY)  ( sizeof(struct _pmath_dispatch_table_extra_data_t)   \
                                           - sizeof(struct _pmath_dispatch_entry_t)              \
                                           + sizeof(struct _pmath_dispatch_entry_t) * (CAPACITY))

#define MAX_CAPACITY_RAW    ( (SIZE_MAX - sizeof(struct _pmath_dispatch_table_extra_data_t) - sizeof(struct _pmath_custom_expr_t)) \
                            / (sizeof(struct _pmath_dispatch_entry_t) + sizeof(pmath_t)))
#define MAX_CAPACITY        ((MAX_CAPACITY_RAW < (INT32_MAX / 2)) ? MAX_CAPACITY_RAW : (INT32_MAX / 2))

static size_t dispatch_expr_get_length(struct _pmath_custom_expr_t *e) {
  struct _pmath_dispatch_table_extra_data_t *tab_extra = DISPATCH_EXPR_EXTRA(e);
  
  return tab_extra->used_length;
}

static pmath_t dispatch_expr_get_item(struct _pmath_custom_expr_t *e, size_t i) {
  struct _pmath_dispatch_table_extra_data_t *tab_extra = DISPATCH_EXPR_EXTRA(e);
  
  if(i == 0)
    return pmath_ref(pmath_System_List);
  
  if(i > tab_extra->used_length)
    return PMATH_NULL;
  
  pmath_t key = pmath_ref(e->internals.items[i - 1]);
  if(pmath_is_expr_of(key, PMATH_MAGIC_PATTERN_SEQUENCE)) {
    key = pmath_expr_set_item(key, 0, pmath_ref(pmath_System_PatternSequence));
  }
  return key;
}

static pmath_bool_t dispatch_expr_try_item_equals(struct _pmath_custom_expr_t *e, size_t i, pmath_t expected_item, pmath_bool_t *result) { // does not free e or expected_item
  struct _pmath_dispatch_table_extra_data_t *tab_extra = DISPATCH_EXPR_EXTRA(e);
  
  if(i == 0) {
    *result = pmath_same(expected_item, pmath_System_List);
    return TRUE;
  }
  
  if(i > tab_extra->used_length) {
    *result = pmath_is_null(expected_item);
    return TRUE;
  }
  
  pmath_t key = e->internals.items[i - 1];
  if(pmath_is_expr_of(key, PMATH_MAGIC_PATTERN_SEQUENCE)) {
    //return FALSE;
    key = pmath_ref(key);
    key = pmath_expr_set_item(key, 0, pmath_ref(pmath_System_PatternSequence));
    *result = pmath_equals(key, expected_item);
    pmath_unref(key);
    return TRUE;
  }
  
  *result = pmath_equals(key, expected_item);
  return TRUE;
}

static const struct _pmath_custom_expr_api_t dispatch_expr_api = {
  .destroy_data            = dispatch_expr_destroy_data,
  .try_prevent_destruction = dispatch_expr_try_prevent_destruction,
  .get_length              = dispatch_expr_get_length,
  .get_item                = dispatch_expr_get_item,
  .try_item_equals         = dispatch_expr_try_item_equals,
};

//} ... custom expr API for dispatch table

//{ dispatch table slice ...

/// Extra data of the _pmath_dispatch_table_extra_data_t::literal_entries hash table.
struct _pmath_dispatch_ht_extra_t {
  _pmath_dispatch_table_expr_t *owner;
};

static struct _pmath_dispatch_ht_extra_t *dispatch_ht_extra(pmath_hashtable_t ht) {
  return (void*)ht;
}

static void noop(pmath_hashtable_t ht, void *e) {
}

static unsigned int dispatch_entry_hash(pmath_hashtable_t ht, void *e) {
  struct _pmath_dispatch_ht_extra_t *extra = dispatch_ht_extra(ht);
  _pmath_dispatch_table_expr_t       *tab   = extra->owner;
  struct _pmath_dispatch_entry_t    *entry = (struct _pmath_dispatch_entry_t *)e;
  
  if(!tab) {
    pmath_debug_print("[dispatch_entry_hash(%p, %p): ht not yet fully initialized]\n", ht, e);
    return 0;
  }
  
  struct _pmath_dispatch_table_extra_data_t *tab_extra = DISPATCH_EXPR_EXTRA(tab);
  
  assert((uintptr_t)&tab_extra->entries[0] <= (uintptr_t)entry && (uintptr_t)entry < (uintptr_t)&tab_extra->entries[tab_extra->used_length] && "only support entries from owned table");
  
  size_t i = entry - &tab_extra->entries[0];
  
  pmath_t key = tab->internals.items[i];
  //prepare_const_pattern(&key); // that is unfortunate :-(
  if(entry->is_const_pattern_sequence) {
    key = pmath_ref(key);
    key = pmath_expr_set_item(key, 0, PMATH_MAGIC_PATTERN_SEQUENCE);
    
    unsigned int hash = pmath_hash(key);
    
    pmath_unref(key);
    return hash;
  }
  
  return pmath_hash(key);
}

static pmath_bool_t dispatch_entry_keys_equal(pmath_hashtable_t ht, void *e1, void *e2) {
  struct _pmath_dispatch_ht_extra_t *extra  = dispatch_ht_extra(ht);
  _pmath_dispatch_table_expr_t       *tab    = extra->owner;
  struct _pmath_dispatch_entry_t    *entry1 = e1;
  struct _pmath_dispatch_entry_t    *entry2 = e2;
  
  if(!tab) {
    pmath_debug_print("[dispatch_entry_keys_equal(%p, %p, %p): ht not yet fully initialized]\n", ht, e1, e2);
    return FALSE;
  }
  
  struct _pmath_dispatch_table_extra_data_t *tab_extra = DISPATCH_EXPR_EXTRA(tab);
  
  assert((uintptr_t)&tab_extra->entries[0] <= (uintptr_t)entry1 && (uintptr_t)entry1 < (uintptr_t)&tab_extra->entries[tab_extra->used_length] && "only support entries from owned table");
  assert((uintptr_t)&tab_extra->entries[0] <= (uintptr_t)entry2 && (uintptr_t)entry2 < (uintptr_t)&tab_extra->entries[tab_extra->used_length] && "only support entries from owned table");
  
  if(entry1->literal_turn_or_zero      != entry2->literal_turn_or_zero)      return FALSE;
  if(entry1->is_const_pattern_sequence != entry2->is_const_pattern_sequence) return FALSE;
  
  size_t i1 = entry1 - &tab_extra->entries[0];
  size_t i2 = entry2 - &tab_extra->entries[0];
  
  pmath_t key1 = tab->internals.items[i1];
  pmath_t key2 = tab->internals.items[i2];
  
  return pmath_equals(key1, key2);
}

static unsigned int dispatch_lookup_info_hash(pmath_hashtable_t ht, void *k) {
  struct dispatch_lookup_info_t *info = k;
  return pmath_hash(info->key);
}

static pmath_bool_t dispatch_entry_equals_lookup_key(pmath_hashtable_t ht, void *e, void *k) {
  struct _pmath_dispatch_ht_extra_t *extra  = dispatch_ht_extra(ht);
  _pmath_dispatch_table_expr_t       *tab    = extra->owner;
  struct _pmath_dispatch_entry_t    *entry  = e;
  struct dispatch_lookup_info_t     *info   = k;
  
  if(!tab) {
    pmath_debug_print("[dispatch_entry_equals_lookup_key(%p, %p, %p): ht not yet fully initialized]\n", ht, e, k);
    return FALSE;
  }
  
  struct _pmath_dispatch_table_extra_data_t *tab_extra = DISPATCH_EXPR_EXTRA(tab);
  
  assert((uintptr_t)&tab_extra->entries[0] <= (uintptr_t)entry && (uintptr_t)entry < (uintptr_t)&tab_extra->entries[tab_extra->used_length] && "only support entries from owned table");
  
  if(info->turn_or_zero) {
    if(entry->literal_turn_or_zero != info->turn_or_zero)
      return FALSE;
  }
  
  size_t i = entry - &tab_extra->entries[0];
  
  pmath_t entry_key = tab->internals.items[i];
  //prepare_const_pattern(&entry_key); // that is unfortunate :-(
  if(entry->is_const_pattern_sequence) {
    entry_key = pmath_ref(entry_key);
    entry_key = pmath_expr_set_item(entry_key, 0, PMATH_MAGIC_PATTERN_SEQUENCE);
    
    pmath_bool_t eq = pmath_equals(entry_key, info->key);
    
    pmath_unref(entry_key);
    return eq;
  }
  
  return pmath_equals(entry_key, info->key);
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

// Maps dispatch_table_cache_search_t* keys to _pmath_dispatch_table_expr_t* entries:
static pmath_hashtable_t dispatch_table_cache;

#define DISPATCH_TABLE_LIMBO_SIZE       8   // Must be a power of two
#define DISPATCH_TABLE_LIMBO_SIZE_MASK  (DISPATCH_TABLE_LIMBO_SIZE - 1)

static _pmath_dispatch_table_expr_t *dispatch_table_limbo[DISPATCH_TABLE_LIMBO_SIZE];
static size_t dispatch_table_limbo_next = 0;

static pmath_t      dispatch_table_cache_key_at(struct dispatch_table_cache_search_t *search, size_t i);

static void         dispatch_table_cache_entry_destructor(void *entry);
static unsigned int dispatch_table_cache_entry_hash(void *entry);
static void         dispatch_table_init_cache_hash(_pmath_dispatch_table_expr_t *tab);
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

static size_t unsafe_find_in_limbo(_pmath_dispatch_table_expr_t *disp) {
  // The limbo is small enough for a linear search. Otherwise we would have to introduce a 
  // _pmath_dispatch_table_t::limbo_index member.
  size_t i;
  for(i = 0; i < DISPATCH_TABLE_LIMBO_SIZE; ++i)
    if(dispatch_table_limbo[i] == disp)
      return i+1;
  
  return 0;
}

_pmath_dispatch_table_expr_t *find_dispatch_table(pmath_expr_t rules) {
  _pmath_dispatch_table_expr_t *result = NULL;
  struct dispatch_table_cache_search_t search;
  search.mode    = SEARCH_BY_LIST_OF_RULES;
  search.u.rules = rules;
  
  pmath_atomic_lock(&dispatch_table_cache_lock);
  {
    result = pmath_ht_search(dispatch_table_cache, &search);
    if(result) {
      //_pmath_ref_ptr((struct _pmath_t*)result);
      _pmath_ref_ptr(&result->internals.inherited.inherited.inherited);
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

static _pmath_dispatch_table_expr_t *get_dispatch_table_for_rules(pmath_expr_t rules) {
  _pmath_dispatch_table_expr_t *tab;
  _pmath_dispatch_table_expr_t *old;
  //pmath_t keys;
  size_t len;
  
  tab = find_dispatch_table(rules);
  if(tab) {
//    pmath_debug_print("[use cached dispatch table %p (%d refs) ", tab, (int)_pmath_refcount_ptr(&tab->inherited));
//    pmath_debug_print_object("for ", rules, "]\n");
    return tab;
  }
  
  len = pmath_expr_length(rules);
  size_t capacity = len >= 1 ? len : 1;
  if(capacity > MAX_CAPACITY)
    return NULL;
  
  size_t extra_size = EXTRA_SIZE_FOR_CAPACITY(capacity);
  tab = _pmath_expr_new_custom(capacity-1, &dispatch_expr_api, extra_size); // -1 because +1 will be added for 'head'
  if(!tab)
    return NULL;
  
  assert(len <= tab->internals.length + 1);
  
  for(size_t i = 0; i < len; ++i) {
    pmath_t rule = pmath_expr_get_item(rules, i+1);
    pmath_t lhs  = pmath_expr_get_item(rule, 1);
    pmath_unref(rule);
    tab->internals.items[i] = lhs;
  }
  
  if(!finish_create_dispatch_table(tab, len)) {
    _pmath_unref_ptr(&tab->internals.inherited.inherited.inherited);
    return NULL;
  }
  
  pmath_atomic_lock(&dispatch_table_cache_lock);
  {
    old = pmath_ht_insert(dispatch_table_cache, tab);
    if(old)
      _pmath_ref_ptr(&old->internals.inherited.inherited.inherited);
  }
  pmath_atomic_unlock(&dispatch_table_cache_lock);
  
  if(old)
    _pmath_unref_ptr(&old->internals.inherited.inherited.inherited);
  
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

// Assuming tab->internals.items  is  already initialized.
pmath_bool_t finish_create_dispatch_table(_pmath_dispatch_table_expr_t *tab, size_t num_keys) {
  struct _pmath_dispatch_table_extra_data_t *tab_extra = DISPATCH_EXPR_EXTRA(tab);

  pmath_hashtable_t literal_entries;
  pmath_hashtable_t key_to_turn;
  struct dispatch_lookup_info_t lookup_no_turn;
  struct _pmath_dispatch_entry_t *current_slice_start;
  
  assert(num_keys <= tab->internals.length + 1); // tab->internals.items[] has length+1 many elements
  assert(tab_extra->literal_entries == NULL);
  assert(_pmath_refcount_ptr(&tab->internals.inherited.inherited.inherited) == 1);
  
  if(num_keys > INT32_MAX/2)
    return FALSE;
  
  literal_entries = pmath_ht_create_ex(&dispatch_entries_ht_class, (unsigned)num_keys);
  if(!literal_entries)
    return FALSE;
  
  key_to_turn = pmath_ht_create_ex(&dispatch_entries_ht_class, (unsigned)num_keys);
  if(!key_to_turn) {
    pmath_ht_destroy(literal_entries);
    return FALSE;
  }
  
  dispatch_ht_extra(literal_entries)->owner = tab;
  dispatch_ht_extra(key_to_turn)->owner     = tab;
  
  tab_extra->used_length     = num_keys;
  tab_extra->literal_entries = literal_entries;
  current_slice_start        = &tab_extra->entries[0];
  
  dispatch_table_init_cache_hash(tab);
  
  lookup_no_turn.turn_or_zero = 0;
  
  for(size_t ii = 0; ii < num_keys; ++ii) {
    struct _pmath_dispatch_entry_t *entry = &tab_extra->entries[ii];
    pmath_t                         key   = tab->internals.items[ii];
    
    entry->is_const_pattern_sequence = is_const_pattern_sequence(key);
    // TODO: permanently change PatternSequence head to PMATH_MAGIC_PATTERN_SEQUENCE   if 'is_const_pattern_sequence'
    
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
        pmath_debug_print("[failed to insert entry %p for item %d]\n", entry, (int)ii + 1);
        dispatch_expr_destroy_data(&tab_extra->base); // Idempotent. Clears 'literal_entries' so that tab will not go to limbo.
        pmath_ht_destroy(key_to_turn);
        return FALSE;
      }
    }
    else {
      current_slice_start->next_slice_or_slice_start = entry;
      entry->next_slice_or_slice_start = entry + 1;
      current_slice_start = entry + 1;
      
      entry->literal_turn_or_zero = 0;
    }
  }
  
  if(current_slice_start < tab_extra->entries + num_keys)
    current_slice_start->next_slice_or_slice_start = &tab_extra->entries[num_keys];
  
  pmath_ht_destroy(key_to_turn);
  return TRUE;
}

// Idempotent! Disposes contents of data, but not data itself. Clears 'literal_entries' to NULL
static void dispatch_expr_destroy_data(struct _pmath_custom_expr_data_t *_data) {
  struct _pmath_dispatch_table_extra_data_t *tab_extra = (void*)_data;
  
//  if(tab_extra->literal_entries) {
//    pmath_atomic_lock(&dispatch_table_cache_lock);
//    {
//    // TODO: what if the dispatch table was revived???
//    // Remove from cache ...
//    _pmath_dispatch_table_expr_t *tab = dispatch_ht_extra(tab_extra->literal_entries)->owner;
//    struct dispatch_table_cache_search_t search;
//    search.mode  = SEARCH_BY_DISPATCH_PTR;
//    search.u.ptr = tab;
//    
//    ....other_zombie_cached = pmath_ht_remove(dispatch_table_cache, &search);
//    }
//    pmath_atomic_unlock(&dispatch_table_cache_lock);
//  }
  
  pmath_ht_destroy(tab_extra->literal_entries);
  tab_extra->literal_entries = NULL;
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
  _pmath_dispatch_table_expr_t *table, // won't be freed
  pmath_t key,                        // won't be freed
  pmath_t *rules_in_rhs_out,          // will be freed
  pmath_bool_t literal
) {
  struct _pmath_dispatch_table_extra_data_t *tab_extra = DISPATCH_EXPR_EXTRA(table);
  struct dispatch_lookup_info_t info;
  size_t last_index = 0;
  size_t num_keys = tab_extra->used_length;
  
  info.key = key;
  info.turn_or_zero = 1;
  
  while(last_index < num_keys) {
    size_t found_index;
    size_t slice_start;
    struct _pmath_dispatch_entry_t *entry = pmath_ht_search(tab_extra->literal_entries, &info);
    if(entry) {
      found_index = 1 + (entry - tab_extra->entries);
      slice_start = 1 + (get_slice_start(entry) - tab_extra->entries);
    }
    else {
      found_index = slice_start = 1 + num_keys;
    }
    
    ++last_index;
    while(last_index < slice_start) {
      entry = &tab_extra->entries[last_index - 1];
      if(entry->literal_turn_or_zero > 0) { // literal pattern, cannot match
        struct _pmath_dispatch_entry_t *next = get_next_slice(entry);
        assert(next > entry);
        
        last_index = 1 + (next - tab_extra->entries);
        continue;
      }
      
      assert(entry->next_slice_or_slice_start == entry + 1);
      
      pmath_t entry_key = table->internals.items[last_index - 1];
      
      if(rules_in_rhs_out) {
        pmath_t rule = pmath_expr_get_item(*rules_in_rhs_out, last_index);
        pmath_t rhs = pmath_expr_get_item(rule, 2);
        pmath_bool_t is_match;
        pmath_unref(rule);
        
        is_match = literal ? pmath_equals(key, entry_key) : _pmath_pattern_match(key, pmath_ref(entry_key), &rhs);
        if(is_match) {
          pmath_unref(*rules_in_rhs_out);
          *rules_in_rhs_out = rhs;
          return last_index;
        }
        pmath_unref(rhs);
      }
      else {
        pmath_bool_t is_match = literal ? pmath_equals(key, entry_key) : _pmath_pattern_match(key, pmath_ref(entry_key), NULL);
        if(is_match) {
          return last_index;
        }
      }
      
      ++last_index;
    }
    
    if(found_index > num_keys)
      break;
    
    last_index = found_index;
    if(rules_in_rhs_out) {
      pmath_t rule = pmath_expr_get_item(*rules_in_rhs_out, last_index);
      pmath_t rhs  = pmath_expr_get_item(rule, 2);
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

PMATH_API pmath_bool_t pmath_is_dispatch_table(pmath_t obj) {
  return NULL != _pmath_as_custom_expr_by_api(obj, &dispatch_expr_api);
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
  pmath_dispatch_table_t       tab     = _pmath_rules_need_dispatch_table(rules);
  _pmath_dispatch_table_expr_t *tab_ptr = (void*)PMATH_AS_PTR(tab);
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
  pmath_dispatch_table_t       tab     = _pmath_rules_need_dispatch_table(rules);
  _pmath_dispatch_table_expr_t *tab_ptr = (void*)PMATH_AS_PTR(tab);
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
  _pmath_dispatch_table_expr_t *tab_ptr = (void*)PMATH_AS_PTR(tab);
  size_t i, len;
  struct _pmath_dispatch_entry_t *entry;
  
  if(!tab_ptr) {
    pmath_unref(key);
    return rules;
  }
  
  struct _pmath_dispatch_table_extra_data_t *tab_extra = DISPATCH_EXPR_EXTRA(tab_ptr);
  
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
  entry = tab_extra->entries;
  while(entry != tab_extra->entries + len) {
    if(entry->literal_turn_or_zero > 0) { // literal pattern, cannot match
      entry = get_next_slice(entry);
    }
    else {
      size_t i = entry - &tab_extra->entries[0];
      pmath_t entry_key = tab_ptr->internals.items[i];
      if(pmath_equals(entry_key, key)) {
        pmath_unref(key);
        return replace_rule_rhs(rules, tab, i+1, callback, callback_context);
      }
      // TODO: compare patterns instead of just appending to the end?
    }
  }

  pmath_unref(tab);
  return append_rule(rules, key, callback, callback_context);
}

//{ module init/done ...

PMATH_PRIVATE void _pmath_dispatch_table_filter_limbo(
  pmath_bool_t (*keep_callback)(_pmath_dispatch_table_expr_t*, void*),
  void          *closure
) {
  _pmath_dispatch_table_expr_t *old_limbo[DISPATCH_TABLE_LIMBO_SIZE];
  size_t i;
  size_t num_del;
  
  PMATH_STATIC_ASSERT(sizeof(dispatch_table_limbo) == sizeof(old_limbo));
  
  pmath_atomic_lock(&dispatch_table_cache_lock);
  {
    for(i = 0; i < DISPATCH_TABLE_LIMBO_SIZE; ++i) {
      _pmath_dispatch_table_expr_t *table = dispatch_table_limbo[i];
      old_limbo[i] = table;
      if(table) {
        _pmath_ref_ptr(&table->internals.inherited.inherited.inherited); // Note that table has refcount == 1 now.
        dispatch_table_limbo[i] = NULL;
      }
    }
    dispatch_table_limbo_next = 0;
  }
  pmath_atomic_unlock(&dispatch_table_cache_lock);
  
  num_del = 0;
  for(i = 0; i < DISPATCH_TABLE_LIMBO_SIZE; ++i) {
    _pmath_dispatch_table_expr_t *table = old_limbo[i];
    if(table) {
      // Note that table has refcount >= 1 here (could be concurrenlty revived).
      if(keep_callback(table, closure)) {
        old_limbo[i] = NULL;
        _pmath_unref_ptr(&table->internals.inherited.inherited.inherited); // Will sent table back to limbo (eventually if it was just revived)
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
      _pmath_dispatch_table_expr_t *table = old_limbo[i]; 
      if(table) { // table.refcount >= 1 (>1 if already revived)
        if(_pmath_prepare_destroy(&table->internals.inherited.inherited.inherited)) { // Afterwards: table.refcount >= 0 (>0 if already revived)
          // table.refcount == 0.
          
          struct dispatch_table_cache_search_t search;
          search.mode  = SEARCH_BY_DISPATCH_PTR;
          search.u.ptr = DISPATCH_EXPR_EXTRA(table);
          old_limbo[i] = pmath_ht_remove(dispatch_table_cache, &search);
        }
        else
          old_limbo[i] = NULL; // table was revived concurrently
        --remaining;
      }
    }
  }
  pmath_atomic_unlock(&dispatch_table_cache_lock);
  
  for(i = 0; i < DISPATCH_TABLE_LIMBO_SIZE && num_del; ++i) {
    _pmath_dispatch_table_expr_t *table = old_limbo[i]; // refcount == 0 here (cannot have been revived, becaus it was removed from cache)
    if(table) { 
      --num_del;
      dispatch_table_cache_entry_destructor(table);
    }
  }
}

PMATH_PRIVATE void _pmath_dispatch_tables_memory_panic(void) {
  _pmath_dispatch_table_expr_t *old_limbo[DISPATCH_TABLE_LIMBO_SIZE];
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
        search.mode  = SEARCH_BY_DISPATCH_PTR;
        search.u.ptr = DISPATCH_EXPR_EXTRA(old_limbo[i]);
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
  _pmath_dispatch_table_expr_t *tab = entry;
  
  if(PMATH_LIKELY(_pmath_refcount_ptr(&tab->internals.inherited.inherited.inherited) == 0)) {
    dispatch_expr_destroy_data(&DISPATCH_EXPR_EXTRA(tab)->base); // Idempotent. Clear literal_entries to NULL
    _pmath_ref_ptr(  &tab->internals.inherited.inherited.inherited);
    _pmath_unref_ptr(&tab->internals.inherited.inherited.inherited); // Final burrial: will not go to limbo, since literal_entries==NULL
  }
  else {
    pmath_debug_print("[ignored attempt to free dispatch table cache entry %p with refcount > 0]\n", tab);
  }
}

static pmath_bool_t dispatch_expr_try_prevent_destruction(struct _pmath_custom_expr_t *tab) {
  // Note that *e has ref_count = 0 now
  
  struct _pmath_dispatch_table_extra_data_t *tab_extra = DISPATCH_EXPR_EXTRA(tab);
  
  pmath_bool_t keep_alive = FALSE;
  
  _pmath_dispatch_table_expr_t *other_zombie        = NULL;
  _pmath_dispatch_table_expr_t *other_zombie_cached = NULL;
  
  pmath_atomic_lock(&dispatch_table_cache_lock);
  {
    if(PMATH_UNLIKELY(tab_extra->literal_entries == NULL)) { // literal_entries = NULL means final burial, no limbo.
      if(PMATH_UNLIKELY(_pmath_refcount_ptr(&tab->internals.inherited.inherited.inherited)) != 0) {
        pmath_debug_print("[ERROR: dispatch table %p revived just before final burial. It already lost its hashtable!]\n", tab);
        keep_alive = TRUE;
      }
      else
        keep_alive = FALSE;
    }
    else if(PMATH_UNLIKELY(_pmath_refcount_ptr(&tab->internals.inherited.inherited.inherited)) != 0) {
      pmath_debug_print("[dispatch table %p revived just before burial]\n", tab);
      keep_alive = TRUE;
    }
    else {
      // Put into limbo cache.
      size_t i = (dispatch_table_limbo_next++) & DISPATCH_TABLE_LIMBO_SIZE_MASK;
      other_zombie = dispatch_table_limbo[i];
      dispatch_table_limbo[i] = tab;
      keep_alive = TRUE;
      
      // TODO: Remove 'other_zombie' from dispatch table cache when not kept in limbo.
      if(other_zombie) {
        struct dispatch_table_cache_search_t search;
        search.mode  = SEARCH_BY_DISPATCH_PTR;
        search.u.ptr = DISPATCH_EXPR_EXTRA(other_zombie);
//        pmath_debug_print("[dispatch table limbo contained old_zomie %p]\n", other_zombie);
        other_zombie_cached = pmath_ht_remove(dispatch_table_cache, &search);
        if(PMATH_LIKELY(other_zombie == other_zombie_cached)) {
          other_zombie_cached = NULL; // prevent double-free
        }
        else {
          pmath_debug_print("[dispatch table cache contains %p instead of %p]\n", other_zombie_cached, other_zombie);
          if(other_zombie_cached) { // undo the removal
            other_zombie_cached = pmath_ht_insert(dispatch_table_cache, other_zombie_cached);
            // Now other_zombie_cached is NULL on success or remains unchanged on failure. 
            // Technically, failure should not be possible, because the insertion should not 
            // require a memory allocation.
          }
        }
      }
    }
  }
  pmath_atomic_unlock(&dispatch_table_cache_lock);
  
  if(other_zombie)
    dispatch_table_cache_entry_destructor(other_zombie);
  
  if(other_zombie_cached)
    dispatch_table_cache_entry_destructor(other_zombie_cached);
  
  return keep_alive;
}

static unsigned int dispatch_table_cache_entry_hash(void *entry) {
  _pmath_dispatch_table_expr_t               *tab       = entry;
  struct _pmath_dispatch_table_extra_data_t *tab_extra = DISPATCH_EXPR_EXTRA(tab);
  return tab_extra->hash_for_cache;
}

static unsigned int dispatch_table_cache_key_hash(void *key) {
  struct dispatch_table_cache_search_t *search = key;
  
  unsigned int hash = 17;
  size_t len = 0;
  
  switch(search->mode) {
    case SEARCH_BY_DISPATCH_PTR: return search->u.ptr->hash_for_cache;
    case SEARCH_BY_KEY_LIST:      len = pmath_expr_length(search->u.keys); break;
    case SEARCH_BY_LIST_OF_RULES: len = pmath_expr_length(search->u.rules); break;
    default:
      assert(!"invalid search mode");
      return 0;
  }
  
  if(search->mode == SEARCH_BY_DISPATCH_PTR) {
    return search->u.ptr->hash_for_cache;
  }
  
  for(size_t i = len; i > 0; --i) {
    pmath_t item   = dispatch_table_cache_key_at(search, i);
    unsigned int h = pmath_hash(item);
    hash = _pmath_incremental_hash(&h, sizeof(h), hash);
    pmath_unref(item);
  }

  return hash;
}

static pmath_t dispatch_table_cache_key_at(struct dispatch_table_cache_search_t *search, size_t i) {
  switch(search->mode) {
    case SEARCH_BY_KEY_LIST: 
      return pmath_expr_get_item(search->u.keys, i);
    case SEARCH_BY_LIST_OF_RULES: {
        pmath_t rule = pmath_expr_get_item(search->u.rules, i);
        if(pmath_is_expr(rule)) {
          pmath_t key = pmath_expr_get_item(rule, 1);
          pmath_unref(rule);
          return key;
        }
        pmath_unref(rule);
      }
      return PMATH_NULL;
    default:
      assert(!"invalid search mode");
      return PMATH_NULL;
  }
}

static void dispatch_table_init_cache_hash(_pmath_dispatch_table_expr_t *tab) {
  struct _pmath_dispatch_table_extra_data_t *tab_extra = DISPATCH_EXPR_EXTRA(tab);

  unsigned int hash = 17;
  size_t i;

  for(i = tab_extra->used_length; i > 0; --i) {
    pmath_t item = tab->internals.items[i - 1];
    unsigned int h = pmath_hash(item);
    hash = _pmath_incremental_hash(&h, sizeof(h), hash);
  }
  
  tab_extra->hash_for_cache = hash;
}

static pmath_bool_t dispatch_table_cache_entry_keys_equal(void *entry1, void *entry2) {
  _pmath_dispatch_table_expr_t *tab1 = entry1;
  _pmath_dispatch_table_expr_t *tab2 = entry2;
  return pmath_equals(
           PMATH_FROM_PTR(&tab1->internals.inherited.inherited.inherited),
           PMATH_FROM_PTR(&tab2->internals.inherited.inherited.inherited));
}

static pmath_bool_t dispatch_table_cache_entry_equals_key(void *entry, void *key) {
  _pmath_dispatch_table_expr_t               *tab       = entry;
  struct _pmath_dispatch_table_extra_data_t *tab_extra = DISPATCH_EXPR_EXTRA(tab);
  struct dispatch_table_cache_search_t      *search    = key;
  
  switch(search->mode) {
    case SEARCH_BY_DISPATCH_PTR: return search->u.ptr == tab_extra;
    case SEARCH_BY_KEY_LIST: 
      return pmath_equals(search->u.keys, PMATH_FROM_PTR(&tab->internals.inherited.inherited.inherited));
    case SEARCH_BY_LIST_OF_RULES: {
        size_t i = pmath_expr_length(search->u.rules);
        
        if(i != tab_extra->used_length)
          return FALSE;
        
        for(; i > 0; --i) {
          pmath_t search_item = dispatch_table_cache_key_at(search, i);
          pmath_t tab_item    = tab->internals.items[i - 1];
          
          //prepare_const_pattern(&search_item);
          
          if(!pmath_equals(search_item, tab_item)) {
            pmath_unref(search_item);
            return FALSE;
          }
          
          pmath_unref(search_item);
        }
      }
      return TRUE;
    default:
      assert(!"invalid search mode");
      return FALSE;
  }
}
