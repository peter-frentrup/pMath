#include <pmath-util/symbol-values-private.h>

#include <pmath-core/objects-private.h>
#include <pmath-core/custom.h>
#include <pmath-core/expressions-private.h>
#include <pmath-core/strings-private.h>

#include <pmath-language/patterns-private.h>

#include <pmath-util/concurrency/atomic-private.h> // depends on pmath-objects-inline.h
#include <pmath-util/dispatch-tables-private.h>
#include <pmath-util/emit-and-gather.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/memory.h>

#include <pmath-builtins/all-symbols-private.h>

#include <string.h>


struct _pmath_multirule_t {
  struct _pmath_t inherited;

  pmath_locked_t pattern;
  pmath_locked_t body;

  pmath_locked_t next; // _pmath_multirule_t
};

/* struct _pmath_multirule_t * is also used as a spinlock, so it may only be
   accessed through _pmath_object_atomic_read*() [using with multirule_[read|start|end]()],

   but NOT directly or with _pmath_object_atomic_write(), because
   _pmath_object_atomic_write() does not lock.
 */

/* In struct _pmath_rulecache_t, the table-member is its own lock:
   while using the hashtable, it is set to PMATH_INVALID_PTR.
   Using the following functions to get the actual table:
 */

#define rulecache_table_lock(rc)           ((pmath_hashtable_t)_pmath_atomic_lock_ptr(&((rc)->_table)))
#define rulecache_table_unlock(rc, table)  _pmath_atomic_unlock_ptr(&((rc)->_table), (table));

extern pmath_symbol_t pmath_System_HoldPattern;
extern pmath_symbol_t pmath_System_RuleDelayed;
extern pmath_symbol_t pmath_System_True;
extern pmath_symbol_t pmath_Internal_Condition;

//{ constructors / destructors...

static pmath_t create_multirule(void) {
  struct _pmath_multirule_t *result;

  result = (void*)PMATH_AS_PTR(_pmath_create_stub(
                                 PMATH_TYPE_SHIFT_MULTIRULE,
                                 sizeof(struct _pmath_multirule_t)));

  result->pattern._data = PMATH_NULL;
  result->body._data    = PMATH_NULL;
  result->next._data    = PMATH_NULL;
  return PMATH_FROM_PTR(result);
}

static pmath_t move_multirule(
  pmath_t src // will be freed
) {
  pmath_t result;
  pmath_t current;
  pmath_t src_next;
  struct _pmath_multirule_t *_src = (void*)PMATH_AS_PTR(src);

  if(!_src)
    return PMATH_NULL;

  result = current = create_multirule();

  while(!pmath_is_null(current) && _src) {
    struct _pmath_multirule_t *_dst = (void*)PMATH_AS_PTR(current);

    _dst->pattern._data = _pmath_object_atomic_read(&_src->pattern);
    _dst->body._data    = _pmath_object_atomic_read(&_src->body);
    _dst->next._data    = PMATH_NULL;

    src_next = _pmath_object_atomic_read(&_src->next);
    pmath_unref(src);
    src = src_next;
    _src = (void*)PMATH_AS_PTR(src);
    if(!_src)
      break;

    current = _dst->next._data = create_multirule();
  }

  return result;
}

PMATH_PRIVATE
void _pmath_rulecache_copy(
  struct _pmath_rulecache_t *dst,
  struct _pmath_rulecache_t *src
) {
  assert(dst != NULL);

  if(!src) {
    dst->_more._data = PMATH_NULL;
    pmath_atomic_write_release(&dst->_table, 0);
    return;
  }

  dst->_more._data = move_multirule(_pmath_object_atomic_read(&src->_more));

  if(pmath_atomic_read_aquire(&src->_table)) {
    pmath_hashtable_t table = rulecache_table_lock(src);

    pmath_atomic_write_release(
      &dst->_table,
      (intptr_t)pmath_ht_copy(table, _pmath_object_entry_copy_func));

    rulecache_table_unlock(src, table);
  }
  else
    pmath_atomic_write_release(&dst->_table, 0);
}

PMATH_PRIVATE
void _pmath_symbol_rules_copy(
  struct _pmath_symbol_rules_t *dst,
  struct _pmath_symbol_rules_t *src
) {
  pmath_hashtable_t src_messages;
  assert(dst != NULL);

  if(!src) {
    dst->up_rules._more._data      = PMATH_NULL;
    dst->down_rules._more._data    = PMATH_NULL;
    dst->sub_rules._more._data     = PMATH_NULL;
    dst->approx_rules._more._data  = PMATH_NULL;
    dst->default_rules._more._data = PMATH_NULL;
    dst->format_rules._more._data  = PMATH_NULL;

    pmath_atomic_write_release(&dst->up_rules._table,      0);
    pmath_atomic_write_release(&dst->down_rules._table,    0);
    pmath_atomic_write_release(&dst->sub_rules._table,     0);
    pmath_atomic_write_release(&dst->approx_rules._table,  0);
    pmath_atomic_write_release(&dst->default_rules._table, 0);
    pmath_atomic_write_release(&dst->format_rules._table,  0);

    pmath_atomic_write_release(&dst->_messages, 0);
    
    pmath_atomic_write_release(&dst->early_call,  0);
    pmath_atomic_write_release(&dst->down_call,   0);
    pmath_atomic_write_release(&dst->up_call,     0);
    pmath_atomic_write_release(&dst->sub_call,    0);
    pmath_atomic_write_release(&dst->approx_call, 0);
    return;
  }

  _pmath_rulecache_copy(&dst->up_rules,      &src->up_rules);
  _pmath_rulecache_copy(&dst->down_rules,    &src->down_rules);
  _pmath_rulecache_copy(&dst->sub_rules,     &src->sub_rules);
  _pmath_rulecache_copy(&dst->approx_rules,  &src->approx_rules);
  _pmath_rulecache_copy(&dst->default_rules, &src->default_rules);
  _pmath_rulecache_copy(&dst->format_rules,  &src->format_rules);

  src_messages = (pmath_hashtable_t)_pmath_atomic_lock_ptr(&src->_messages);

  pmath_atomic_write_release(
    &dst->_messages,
    (intptr_t)pmath_ht_copy(src_messages, _pmath_object_entry_copy_func));

  _pmath_atomic_unlock_ptr(&src->_messages, src_messages);
  
  {
    intptr_t early_call  = pmath_atomic_read_aquire(&src->early_call);
    intptr_t up_call     = pmath_atomic_read_aquire(&src->up_call);
    intptr_t down_call   = pmath_atomic_read_aquire(&src->down_call);
    intptr_t sub_call    = pmath_atomic_read_aquire(&src->sub_call);
    intptr_t approx_call = pmath_atomic_read_aquire(&src->approx_call);
    
    pmath_atomic_write_release(&dst->early_call,  early_call);
    pmath_atomic_write_release(&dst->up_call,     up_call);
    pmath_atomic_write_release(&dst->down_call,   down_call);
    pmath_atomic_write_release(&dst->sub_call,    sub_call);
    pmath_atomic_write_release(&dst->approx_call, approx_call);
  }
}

static void destroy_multirule(pmath_t p) {
  struct _pmath_multirule_t *mr = (void*)PMATH_AS_PTR(p);

  pmath_unref(mr->pattern._data);
  pmath_unref(mr->body._data);
  pmath_unref(mr->next._data);
  pmath_mem_free(mr);
}

PMATH_PRIVATE
void _pmath_rulecache_done(struct _pmath_rulecache_t *rc) {
  assert(rc != NULL);
  assert(rc->_table._data != (intptr_t)PMATH_INVALID_PTR);

  pmath_ht_destroy((pmath_hashtable_t)pmath_atomic_read_aquire(&rc->_table));
  pmath_unref(rc->_more._data);
}

PMATH_PRIVATE
void _pmath_symbol_rules_done(struct _pmath_symbol_rules_t *rules) {
  assert(rules != NULL);

  _pmath_rulecache_done(&rules->up_rules);
  _pmath_rulecache_done(&rules->down_rules);
  _pmath_rulecache_done(&rules->sub_rules);
  _pmath_rulecache_done(&rules->approx_rules);
  _pmath_rulecache_done(&rules->default_rules);
  _pmath_rulecache_done(&rules->format_rules);

  pmath_ht_destroy((pmath_hashtable_t)pmath_atomic_read_aquire(&rules->_messages));
}

//} ============================================================================
//{ visiting all objects ...

PMATH_PRIVATE
enum pmath_visit_result_t _pmath_symbol_value_visit(
  pmath_t                     value, // will be freed
  enum pmath_visit_result_t (*callback)(pmath_t, void*),
  void                       *closure
) {
  while(!pmath_same(value, PMATH_NULL)) {
    enum pmath_visit_result_t result = callback(value, closure);
    if(result == PMATH_VISIT_ABORT) {
      pmath_unref(value);
      return PMATH_VISIT_ABORT;
    }
    if(result == PMATH_VISIT_SKIP) 
      break;

    if(pmath_is_expr(value)) {
      size_t i, len;
      const pmath_t *items = pmath_expr_read_item_data(value);
      struct _pmath_expr_t *expr_ptr;

      if(!items) // packed arrays neither have general sub-expressions, nor debug expressions, only a pmath_blob_t.
        break;

      if(_pmath_symbol_value_visit(pmath_expr_get_item(value, 0), callback, closure) == PMATH_VISIT_ABORT) {
        pmath_unref(value);
        return PMATH_VISIT_ABORT;
      }

      len = pmath_expr_length(value);
      for(i = 0; i < len; ++i) {
        if(_pmath_symbol_value_visit(pmath_ref(items[i]), callback, closure) == PMATH_VISIT_ABORT) {
          pmath_unref(value);
          return PMATH_VISIT_ABORT;
        }
      }

      expr_ptr = (void*)PMATH_AS_PTR(value);
      if(pmath_atomic_read_aquire(&expr_ptr->metadata)) {
        struct _pmath_t *metadata_ptr = _pmath_atomic_lock_ptr(&expr_ptr->metadata);
        pmath_t next = pmath_ref(PMATH_FROM_PTR(metadata_ptr));
        _pmath_atomic_unlock_ptr(&expr_ptr->metadata, metadata_ptr);

        pmath_unref(value);
        value = next;
        continue;
      }
    }

    if(pmath_is_multirule(value)) {
      pmath_t next;
      pmath_t rule = value;
      struct _pmath_multirule_t *_rule = (void*)PMATH_AS_PTR(rule);
      
      result = PMATH_VISIT_NORMAL;
      while(_rule && result != PMATH_VISIT_ABORT) {
        result = _pmath_symbol_value_visit(
                   _pmath_object_atomic_read(&_rule->pattern),
                   callback,
                   closure);

        if(result != PMATH_VISIT_ABORT) {
          result = _pmath_symbol_value_visit(
                     _pmath_object_atomic_read(&_rule->body),
                     callback,
                     closure);
        }

        next = _pmath_object_atomic_read(&_rule->next);
        pmath_unref(rule);
        rule = next;
        _rule = (void*)PMATH_AS_PTR(rule);
      }

      pmath_unref(rule);
      return result;
    }

    if(pmath_is_bigstr(value)) {
      struct _pmath_string_t *string_ptr = (void*)PMATH_AS_PTR(value);
      if(string_ptr->debug_metadata) {
        if(_pmath_symbol_value_visit(pmath_ref(PMATH_FROM_PTR(string_ptr->debug_metadata)), callback, closure) == PMATH_VISIT_ABORT) {
          pmath_unref(value);
          return PMATH_VISIT_ABORT;
        }
      }

      if(string_ptr->buffer) {
        pmath_t next = pmath_ref(PMATH_FROM_PTR(string_ptr->buffer));
        pmath_unref(value);
        value = next;
        continue;
      }
      else
        break;
    }
    
    if(pmath_is_custom(value)) {
      pmath_t next = pmath_custom_get_attached_object(value);
      pmath_unref(value);
      value = next;
      continue;
    }

    break;
  }

  pmath_unref(value);
  return PMATH_VISIT_NORMAL;
}

static enum pmath_visit_result_t object_table_visit(
  pmath_hashtable_t           table,
  enum pmath_visit_result_t (*callback)(pmath_t, void*),
  void                       *closure
) {
  struct _pmath_object_entry_t *entry;
  unsigned int i, cap;

  cap = pmath_ht_capacity(table);
  for(i = 0; i < cap; ++i) {
    entry = pmath_ht_entry(table, i);

    if(entry) {
      if(_pmath_symbol_value_visit(pmath_ref(entry->key),   callback, closure) == PMATH_VISIT_ABORT) return PMATH_VISIT_ABORT;
      if(_pmath_symbol_value_visit(pmath_ref(entry->value), callback, closure) == PMATH_VISIT_ABORT) return PMATH_VISIT_ABORT;
    }
  }

  return PMATH_VISIT_NORMAL;
}

PMATH_PRIVATE
PMATH_ATTRIBUTE_NONNULL(1)
enum pmath_visit_result_t _pmath_rulecache_visit(
  struct _pmath_rulecache_t  *rc,
  enum pmath_visit_result_t (*callback)(pmath_t, void*),
  void                       *closure
) {
  pmath_hashtable_t table;
  enum pmath_visit_result_t result;

  if(!rc)
    return PMATH_VISIT_ABORT;

  table = rulecache_table_lock(rc);
  result = object_table_visit(table, callback, closure);
  rulecache_table_unlock(rc, table);

  if(result == PMATH_VISIT_ABORT) return result;
  
  return _pmath_symbol_value_visit(
           _pmath_object_atomic_read(&rc->_more),
           callback,
           closure);
}

PMATH_PRIVATE
PMATH_ATTRIBUTE_NONNULL(1)
enum pmath_visit_result_t _pmath_symbol_rules_visit(
  struct _pmath_symbol_rules_t  *rules,
  enum pmath_visit_result_t    (*callback)(pmath_t, void*),
  void                          *closure
) {
  pmath_hashtable_t table;
  enum pmath_visit_result_t result;

  if(_pmath_rulecache_visit(&rules->up_rules,      callback, closure) == PMATH_VISIT_ABORT) return PMATH_VISIT_ABORT;
  if(_pmath_rulecache_visit(&rules->down_rules,    callback, closure) == PMATH_VISIT_ABORT) return PMATH_VISIT_ABORT;
  if(_pmath_rulecache_visit(&rules->sub_rules,     callback, closure) == PMATH_VISIT_ABORT) return PMATH_VISIT_ABORT;
  if(_pmath_rulecache_visit(&rules->approx_rules,  callback, closure) == PMATH_VISIT_ABORT) return PMATH_VISIT_ABORT;
  if(_pmath_rulecache_visit(&rules->default_rules, callback, closure) == PMATH_VISIT_ABORT) return PMATH_VISIT_ABORT;
  if(_pmath_rulecache_visit(&rules->format_rules,  callback, closure) == PMATH_VISIT_ABORT) return PMATH_VISIT_ABORT;

  table = (pmath_hashtable_t)_pmath_atomic_lock_ptr(&rules->_messages);
  result = object_table_visit(table, callback, closure);
  _pmath_atomic_unlock_ptr(&rules->_messages, table);

  return result;
}

//} ============================================================================
//{ removing a symbol ...

PMATH_PRIVATE
pmath_t _pmath_symbol_value_remove_all(
  pmath_t  value,           // will be freed
  pmath_t  to_be_removed,   // wont be freed
  pmath_t  replacement      // wont be freed
) {
  if(pmath_same(value, to_be_removed)) {
    pmath_unref(value);
    return pmath_ref(replacement);
  }

  if(pmath_is_expr(value)) {
    size_t i;

    for(i = 0; i <= pmath_expr_length(value); ++i) {
      value = pmath_expr_set_item(
                value, i,
                _pmath_symbol_value_remove_all(
                  pmath_expr_get_item(value, i),
                  to_be_removed,
                  replacement));
    }

    return value;
  }

  if(pmath_is_multirule(value)) {
    pmath_t obj;
    pmath_t next;
    pmath_t rule = pmath_ref(value);
    struct _pmath_multirule_t *_rule = (void*)PMATH_AS_PTR(rule);

    while(_rule) {
      obj = _pmath_object_atomic_read(&_rule->pattern);
      obj = _pmath_symbol_value_remove_all(obj, to_be_removed, replacement);
      _pmath_object_atomic_write(&_rule->pattern, obj);

      obj = _pmath_object_atomic_read(&_rule->body);
      obj = _pmath_symbol_value_remove_all(obj, to_be_removed, replacement);
      _pmath_object_atomic_write(&_rule->body, obj);

      next = _pmath_object_atomic_read(&_rule->next);
      pmath_unref(rule);
      rule = next;
      _rule = (void*)PMATH_AS_PTR(rule);
    }
  }

  return value;
}

static void object_table_remove_all(
  pmath_hashtable_t table,
  pmath_t           to_be_removed,   // wont be freed
  pmath_t           replacement      // wont be freed
) {
  struct _pmath_object_entry_t *entry;
  unsigned int i, cap;

  cap = pmath_ht_capacity(table);
  for(i = 0; i < cap; ++i) {
    entry = pmath_ht_entry(table, i);

    if(entry) {
      entry->key   = _pmath_symbol_value_remove_all(entry->key,   to_be_removed, replacement);
      entry->value = _pmath_symbol_value_remove_all(entry->value, to_be_removed, replacement);
    }
  }
}

PMATH_PRIVATE
void _pmath_rulecache_remove_all(
  struct _pmath_rulecache_t *rc,
  pmath_t                    to_be_removed, // wont be freed
  pmath_t                    replacement    // wont be freed
) {
  pmath_hashtable_t table;
  pmath_t obj;

  table = rulecache_table_lock(rc);
  object_table_remove_all(table, to_be_removed, replacement);
  rulecache_table_unlock(rc, table);

  obj = _pmath_object_atomic_read_start(&rc->_more);
  obj = _pmath_symbol_value_remove_all(obj, to_be_removed, replacement);
  _pmath_object_atomic_read_end(&rc->_more, obj);
}

PMATH_PRIVATE
void _pmath_symbol_rules_remove_all(
  struct _pmath_symbol_rules_t *rules,
  pmath_t                       to_be_removed, // wont be freed
  pmath_t                       replacement    // wont be freed
) {
  pmath_hashtable_t table;

  _pmath_rulecache_remove_all(&rules->up_rules,      to_be_removed, replacement);
  _pmath_rulecache_remove_all(&rules->down_rules,    to_be_removed, replacement);
  _pmath_rulecache_remove_all(&rules->sub_rules,     to_be_removed, replacement);
  _pmath_rulecache_remove_all(&rules->approx_rules,  to_be_removed, replacement);
  _pmath_rulecache_remove_all(&rules->default_rules, to_be_removed, replacement);
  _pmath_rulecache_remove_all(&rules->format_rules,  to_be_removed, replacement);

  table = (pmath_hashtable_t)_pmath_atomic_lock_ptr(&rules->_messages);
  object_table_remove_all(table, to_be_removed, replacement);
  _pmath_atomic_unlock_ptr(&rules->_messages, table);
}

//} ============================================================================
//{ _pmath_symbol_rules_ht_class

static void symbol_rules_entry_destroy(void *e) {
  struct _pmath_symbol_rules_entry_t *entry = e;

  pmath_unref(entry->key);
  _pmath_symbol_rules_done(&entry->rules);

  pmath_mem_free(entry);
}

static unsigned int symbol_rules_entry_hash(void *e) {
  struct _pmath_symbol_rules_entry_t *entry = e;

  return _pmath_hash_pointer(PMATH_AS_PTR(entry->key));
}

static pmath_bool_t symbol_rules_entries_equal(void *e1, void *e2) {
  struct _pmath_symbol_rules_entry_t *entry1 = e1;
  struct _pmath_symbol_rules_entry_t *entry2 = e2;

  return pmath_same(entry1->key, entry2->key);
}

static unsigned int symbol_rules_key_hash(void *k) {
  return _pmath_hash_pointer(PMATH_AS_PTR(*(pmath_t*)k));
}

static pmath_bool_t symbol_rules_entry_equals_key(void *e, void *k) {
  struct _pmath_symbol_rules_entry_t *entry = e;

  return pmath_same(entry->key, *(pmath_t*)k);
}

PMATH_PRIVATE
const pmath_ht_class_t  _pmath_symbol_rules_ht_class = {
  symbol_rules_entry_destroy,
  symbol_rules_entry_hash,
  symbol_rules_entries_equal,
  symbol_rules_key_hash,
  symbol_rules_entry_equals_key
};

//}
//{ pattern matching ...

static pmath_bool_t _pmath_multirule_find(
  pmath_t  rule,   // will be freed
  pmath_t *inout
) {
  struct _pmath_multirule_t *_rule;
  pmath_t next;
  pmath_t rule_body;

  assert(inout != NULL);

  _rule = (void*)PMATH_AS_PTR(rule);
  while(_rule) {
    rule_body = _pmath_object_atomic_read(&_rule->body);

    if(_pmath_pattern_match(
          *inout,
          _pmath_object_atomic_read(&_rule->pattern),
          &rule_body))
    {
      if(_pmath_rhs_has_condition(&rule_body, TRUE)) {
        rule_body = pmath_evaluate(rule_body);

        if(pmath_is_expr_of_len(rule_body, pmath_Internal_Condition, 2)) {
          if(pmath_expr_item_equals(rule_body, 2, pmath_System_True)) {
            pmath_unref(*inout);
            *inout = pmath_expr_get_item(rule_body, 1);
            pmath_unref(rule_body);
            pmath_unref(rule);
            return TRUE;
          }
        }
      }
      else {
        pmath_unref(*inout);
        *inout = rule_body;
        pmath_unref(rule);
        return TRUE;
      }
    }

    pmath_unref(rule_body);

    next = _pmath_object_atomic_read(&_rule->next);
    pmath_unref(rule);
    rule = next;
    _rule = (void*)PMATH_AS_PTR(rule);
  }

  return FALSE;
}

PMATH_PRIVATE
pmath_bool_t _pmath_rulecache_is_empty(struct _pmath_rulecache_t *rc) {
  assert(rc != NULL);
  
  if(pmath_atomic_read_aquire(&rc->_table)) {
    pmath_hashtable_t table = rulecache_table_lock(rc);
    
    int count = pmath_ht_count(table);
    
    rulecache_table_unlock(rc, table);
    if(count != 0)
      return FALSE;
  }
  
  pmath_t more = _pmath_object_atomic_read(&rc->_more);
  pmath_unref(more);
  
  return pmath_is_null(more);
}

PMATH_PRIVATE
pmath_bool_t _pmath_rulecache_find(
  struct _pmath_rulecache_t *rc,
  pmath_t                   *inout
) {
  struct _pmath_object_entry_t *entry;
  pmath_hashtable_t table;

  assert(rc    != NULL);
  assert(inout != NULL);

  table = rulecache_table_lock(rc);

  entry = pmath_ht_search(table, inout);

  if(entry) {
    pmath_unref(*inout);
    *inout = pmath_ref(entry->value);
  }

  rulecache_table_unlock(rc, table);

  if(entry)
    return TRUE;

  return _pmath_multirule_find(_pmath_object_atomic_read(&rc->_more), inout);
}

//} ============================================================================
//{ changing rules ...

/* result: whether the change succeeded
           (otherwise caller must call the function again)
 */
static pmath_bool_t _pmath_multirule_change_ex(
  pmath_locked_t *rule_base,
  pmath_t         rule,
//  struct _pmath_multirule_t * volatile *rule_base, // accessed through _pmath_object_atomic_[read|write]
//  struct _pmath_multirule_t            *rule,      // will be freed
  pmath_t pattern,                                 // wont be freed
  pmath_t body,                                    // wont be freed, PMATH_UNDEFINED => remove rules
  pmath_bool_t *did_any_change
) {
  struct _pmath_multirule_t *_rule = (void*)PMATH_AS_PTR(rule);
  struct _pmath_multirule_t *_prev;
  pmath_t rule_member;
  int cmp;

  *did_any_change = FALSE;

  assert(rule_base != NULL);

  _prev = NULL;
  while(_rule) {
    assert(pmath_is_multirule(rule));

    rule_member = _pmath_object_atomic_read(&_rule->pattern);
    cmp = _pmath_pattern_compare(pattern, rule_member);
    pmath_unref(rule_member);

    if(cmp == 0) { // rhs coditions? (pattern :> value//condition)
      rule_member = _pmath_object_atomic_read(&_rule->body);

      if(_pmath_rhs_has_condition(&rule_member, FALSE)) {
        cmp = 1;

        if(pmath_same(body, PMATH_UNDEFINED)) { // remove this rule and go on
          pmath_t tmp = _pmath_object_atomic_read_start(rule_base); // start atomic ...

          if(!pmath_same(tmp, rule)) { // another thread annoyed us
            _pmath_object_atomic_read_end(rule_base, tmp);
            pmath_unref(PMATH_FROM_PTR(_prev));
            pmath_unref(rule);
            pmath_unref(rule_member);
            return FALSE;
          }

          pmath_unref(tmp);
          tmp = _pmath_object_atomic_read(&_rule->next);
          assert(pmath_is_multirule(tmp));

          pmath_unref(rule);
          rule = pmath_ref(tmp);
          _rule = (void*)PMATH_AS_PTR(rule);

          _pmath_object_atomic_read_end(rule_base, tmp); // end atomic

          pmath_unref(rule_member);
          continue;
        }
      }
      else if(_pmath_rhs_has_condition(&body, FALSE))
        cmp = -1;

      pmath_unref(rule_member);
    }

    if(cmp == 0) { // replace current rule
      pmath_t tmp = _pmath_object_atomic_read_start(rule_base); // start atomic ...

      if(!pmath_same(tmp, rule)) { // another thread annoyed us
        _pmath_object_atomic_read_end(rule_base, tmp);
        pmath_unref(PMATH_FROM_PTR(_prev));
        pmath_unref(rule);
        return FALSE;
      }

      if(pmath_same(body, PMATH_UNDEFINED)) { // remove rule
        pmath_unref(tmp);
        tmp = _pmath_object_atomic_read(&_rule->next);
        *did_any_change = TRUE;
      }
      else { // change rule in place
        pmath_t old_pat;
        pmath_t old_body;

        old_pat = _pmath_object_atomic_read_start(&_rule->pattern);
        _pmath_object_atomic_read_end(&_rule->pattern, pmath_ref(pattern));

        old_body = _pmath_object_atomic_read_start(&_rule->body);
        _pmath_object_atomic_read_end(&_rule->body, pmath_ref(body));

        // Intentionally being sloppy here and using pmath_same instead of pmath_equals. 
        // This could cause false positives, but no false negatives since !pmath_equals(A,B) implies !pmath_same(A,B).
        *did_any_change = !pmath_same(old_body, body) || !pmath_same(old_pat, pattern);
        
        pmath_unref(old_pat);
        pmath_unref(old_body);
      }

      _pmath_object_atomic_read_end(rule_base, tmp); // end atomic
      pmath_unref(PMATH_FROM_PTR(_prev));
      pmath_unref(rule);
      return TRUE;
    }

    if(cmp < 0) { // insert before curren rule
      if(!pmath_same(body, PMATH_UNDEFINED)) {
        struct _pmath_multirule_t *_new_rule = (void*)PMATH_AS_PTR(create_multirule());
        pmath_t tmp;

        if(!_new_rule) {
          // no memory, but repurt success so caller does not stuck in an infinite loop
          pmath_unref(PMATH_FROM_PTR(_prev));
          pmath_unref(rule);
          *did_any_change = FALSE;
          return TRUE;
        }

        _new_rule->pattern._data = pmath_ref(pattern);
        _new_rule->body._data    = pmath_ref(body);

        tmp = _pmath_object_atomic_read_start(rule_base); // start atomic ...

        if(!pmath_same(tmp, rule)) { // another thread annoyed us
          _pmath_object_atomic_read_end(rule_base, tmp);
          pmath_unref(rule);
          pmath_unref(PMATH_FROM_PTR(_prev));
          pmath_unref(PMATH_FROM_PTR(_new_rule));
          return FALSE;
        }

        pmath_atomic_barrier();

        pmath_unref(tmp);
        tmp = PMATH_FROM_PTR(_new_rule);
        _pmath_object_atomic_write(&_new_rule->next, rule);

        _pmath_object_atomic_read_end(rule_base, tmp); // end atomic
        pmath_unref(PMATH_FROM_PTR(_prev));
        *did_any_change = TRUE;
        return TRUE;
      }

      pmath_unref(PMATH_FROM_PTR(_prev));
      pmath_unref(rule);
      *did_any_change = FALSE;
      return TRUE;
    }

    pmath_unref(PMATH_FROM_PTR(_prev));
    _prev = _rule;
    rule_base = &_prev->next;
    rule = _pmath_object_atomic_read(rule_base);
    _rule = (void*)PMATH_AS_PTR(rule);
  }

  if(!pmath_same(body, PMATH_UNDEFINED)) {
    struct _pmath_multirule_t *_new_rule = (void*)PMATH_AS_PTR(create_multirule());

    if(_new_rule) {
      pmath_t tmp;

      _new_rule->pattern._data = pmath_ref(pattern);
      _new_rule->body._data    = pmath_ref(body);

      tmp = _pmath_object_atomic_read_start(rule_base); // start atomic ...

      if(!pmath_is_null(tmp)) { // tmp != rule, another thread annoyed us
        _pmath_object_atomic_read_end(rule_base, tmp);
        pmath_unref(PMATH_FROM_PTR(_prev));
        pmath_unref(PMATH_FROM_PTR(_new_rule));
        return FALSE;
      }

      _pmath_object_atomic_read_end(rule_base, PMATH_FROM_PTR(_new_rule)); // end atomic
      *did_any_change = TRUE;
    }
  }
  else
    *did_any_change = FALSE;

  pmath_unref(PMATH_FROM_PTR(_prev));
  return TRUE;
}

static pmath_bool_t _pmath_multirule_change(
  pmath_locked_t *rule_base, // accessed through _pmath_object_atomic_[read|write]
  pmath_t         pattern,   // wont be freed
  pmath_t         body,      // wont be freed, PMATH_UNDEFINED => remove rules
  pmath_bool_t   *did_any_change
) {
  assert(rule_base != NULL);

  return _pmath_multirule_change_ex(
           rule_base,
           _pmath_object_atomic_read(rule_base),
           pattern,
           body,
           did_any_change);
}

PMATH_PRIVATE
pmath_bool_t _pmath_rulecache_change(
  struct _pmath_rulecache_t *rc,
  pmath_t                    pattern, // will be freed
  pmath_t                    body     // will be freed, PMATH_UNDEFINED => remove rules
) {
  pmath_bool_t body_has_condition;
  pmath_bool_t did_any_change = FALSE;
  struct _pmath_object_entry_t *entry;
  pmath_hashtable_t table;

  assert(rc != NULL);

  if(!_pmath_pattern_validate(pattern)) {
    pmath_unref(pattern);
    pmath_unref(body);
    return FALSE;
  }

  body_has_condition = _pmath_rhs_has_condition(&body, FALSE);

  table = rulecache_table_lock(rc);

  if(body_has_condition || pmath_same(body, PMATH_UNDEFINED)) {
    if(body_has_condition) {
      while(!_pmath_multirule_change(&rc->_more, pattern, body, &did_any_change)) {
      }
    }

    entry = pmath_ht_remove(table, &pattern);

    if(entry) {
      if(body_has_condition) {
        while(!_pmath_multirule_change(&rc->_more, entry->key, entry->value, &did_any_change)) {
        }
      }

      rulecache_table_unlock(rc, table);

      pmath_ht_obj_class.entry_destructor(NULL, entry);
      did_any_change = TRUE;
    }
    else
      rulecache_table_unlock(rc, table);

    pmath_unref(pattern);
    pmath_unref(body);
    return did_any_change;
  }
  else {
    entry = pmath_ht_search(table, &pattern);

    if(entry) {
      // Intentionally being sloppy here and using pmath_same instead of pmath_equals. 
      // This could cause false positives, but no false negatives since !pmath_equals(A,B) implies !pmath_same(A,B).
      did_any_change = !pmath_same(entry->value, body);

      pmath_unref(entry->key);
      pmath_unref(entry->value);
      entry->key   = pattern;
      entry->value = body;

      rulecache_table_unlock(rc, table);
      return did_any_change;
    }

    if(_pmath_pattern_is_const(pattern)) {
      if(!table)
        table = pmath_ht_create_ex(&pmath_ht_obj_class, 1);

      entry = pmath_mem_alloc(sizeof(struct _pmath_object_entry_t));

      if(entry) {
        entry->key = pattern;
        entry->value = body;

        entry = pmath_ht_insert(table, entry);
        assert(entry == NULL);

        rulecache_table_unlock(rc, table);
        return TRUE;
      }
    }
  }

  rulecache_table_unlock(rc, table);

  while(!_pmath_multirule_change(&rc->_more, pattern, body, &did_any_change)) {
  }

  pmath_unref(pattern);
  pmath_unref(body);
  return did_any_change;
}

PMATH_PRIVATE
void _pmath_rulecache_clear(struct _pmath_rulecache_t *rc) {
  pmath_hashtable_t table;
  pmath_t           more;

  assert(rc != NULL);

  more = _pmath_object_atomic_read_start(&rc->_more);
  _pmath_object_atomic_read_end(&rc->_more, PMATH_NULL);

  table = rulecache_table_lock(rc);
  rulecache_table_unlock(rc, NULL);

  pmath_unref(more);
  if(table){
    pmath_ht_destroy(table);
  }
}

PMATH_PRIVATE
void _pmath_symbol_rules_clear(struct _pmath_symbol_rules_t *rules) {
  pmath_hashtable_t  messages;

  assert(rules != NULL);

  _pmath_rulecache_clear(&rules->up_rules);
  _pmath_rulecache_clear(&rules->down_rules);
  _pmath_rulecache_clear(&rules->sub_rules);
  _pmath_rulecache_clear(&rules->approx_rules);
  _pmath_rulecache_clear(&rules->default_rules);
  _pmath_rulecache_clear(&rules->format_rules);

  messages = (pmath_hashtable_t)_pmath_atomic_lock_ptr(&rules->_messages);
  _pmath_atomic_unlock_ptr(&rules->_messages, NULL);

  pmath_ht_destroy(messages);
  
//  pmath_atomic_write_release(&rules->early_call,  0);
//  pmath_atomic_write_release(&rules->up_call,     0);
//  pmath_atomic_write_release(&rules->down_call,   0);
//  pmath_atomic_write_release(&rules->sub_call,    0);
//  pmath_atomic_write_release(&rules->approx_call, 0);
}

PMATH_PRIVATE
void _pmath_symbol_rules_clear_code(struct _pmath_symbol_rules_t *rules) {
  assert(rules != NULL);

  pmath_atomic_write_release(&rules->early_call,  0);
  pmath_atomic_write_release(&rules->up_call,     0);
  pmath_atomic_write_release(&rules->down_call,   0);
  pmath_atomic_write_release(&rules->sub_call,    0);
  pmath_atomic_write_release(&rules->approx_call, 0);
}

//} ============================================================================
//{ symbol values ...

static void _pmath_multirules_emit(pmath_t rule) { // will be freed
  struct _pmath_multirule_t *_rule;
  pmath_t next;

  _rule = (void*)PMATH_AS_PTR(rule);
  while(_rule) {
    assert(pmath_is_multirule(rule));

    pmath_emit(
      pmath_expr_new_extended(
        pmath_ref(pmath_System_RuleDelayed), 2,
        pmath_expr_new_extended(
          pmath_ref(pmath_System_HoldPattern), 1,
          _pmath_object_atomic_read(&_rule->pattern)),
        _pmath_object_atomic_read(&_rule->body)),
      PMATH_NULL);

    next = _pmath_object_atomic_read(&_rule->next);
    pmath_unref(rule);
    rule = next;
    _rule = (void*)PMATH_AS_PTR(rule);
  }
}

PMATH_PRIVATE
void _pmath_symbol_value_emit(
  pmath_symbol_t sym,    // wont be freed
  pmath_t        value   // will be freed
) {
  if(pmath_is_multirule(value)) {
    _pmath_multirules_emit(value);
  }
  else if(!pmath_same(value, PMATH_UNDEFINED)){
    pmath_emit(
      pmath_expr_new_extended(
        pmath_ref(pmath_System_RuleDelayed), 2,
        pmath_expr_new_extended(
          pmath_ref(pmath_System_HoldPattern), 1,
          pmath_ref(sym)),
        value),
      PMATH_NULL);
  }
}

PMATH_PRIVATE
void _pmath_rule_table_emit(
  pmath_hashtable_t table
) {
  struct _pmath_object_entry_t *entry;
  unsigned int i, cap;

  cap = pmath_ht_capacity(table);

  for(i = 0; i < cap; ++i) {
    entry = pmath_ht_entry(table, i);

    if(entry) {
      pmath_emit(
        pmath_expr_new_extended(
          pmath_ref(pmath_System_RuleDelayed), 2,
          pmath_expr_new_extended(
            pmath_ref(pmath_System_HoldPattern), 1,
            pmath_ref(entry->key)),
          pmath_ref(entry->value)),
        PMATH_NULL);
    }
  }
}

PMATH_PRIVATE
void _pmath_rulecache_emit(
  struct _pmath_rulecache_t *rc
) {
  pmath_hashtable_t  table;

  assert(rc != NULL);

  table = rulecache_table_lock(rc);

  _pmath_rule_table_emit(table);

  rulecache_table_unlock(rc, table);

  _pmath_multirules_emit(_pmath_object_atomic_read(&rc->_more));
}

PMATH_PRIVATE
pmath_t _pmath_symbol_value_prepare(
  pmath_symbol_t sym,    // wont be freed
  pmath_t        value   // will be freed
) {
  if(pmath_is_multirule(value)) {
    pmath_t rule = value;
    value = pmath_ref(sym);

    if(!_pmath_multirule_find(rule, &value)) {
      pmath_unref(value);
      return PMATH_UNDEFINED;
    }
  }

  return value;
}

PMATH_PRIVATE
pmath_t _pmath_symbol_find_value(pmath_symbol_t sym) {
  return _pmath_symbol_value_prepare(sym, pmath_symbol_get_value(sym));
}

PMATH_PRIVATE
pmath_bool_t _pmath_symbol_define_value_pos(
  pmath_locked_t *value_position,
  pmath_t         pattern,
  pmath_t         body
) {
  pmath_bool_t did_any_change = FALSE;
  pmath_t value = _pmath_object_atomic_read(value_position);

  if(pmath_is_multirule(value)) {
    while(!_pmath_multirule_change_ex(value_position, value, pattern, body, &did_any_change)) {
    }

    value = _pmath_object_atomic_read(value_position);
    if(pmath_is_null(value))
      _pmath_object_atomic_write(value_position, PMATH_UNDEFINED);

    pmath_unref(value);
    pmath_unref(pattern);
    pmath_unref(body);
    return did_any_change;
  }

  if(_pmath_rhs_has_condition(&body, FALSE)) {
    _pmath_object_atomic_write(value_position, PMATH_NULL);

    // (soft) race condition: other threads see no definition now

    while(!_pmath_multirule_change_ex(value_position, PMATH_NULL, pattern, body, &did_any_change)) {
    }

    pmath_unref(body);
    if(pmath_same(value, PMATH_UNDEFINED))
      pmath_unref(pattern);
    else
      did_any_change = _pmath_symbol_define_value_pos(value_position, pattern, value) || did_any_change;

    return did_any_change;
  }

  pmath_unref(pattern);

  _pmath_object_atomic_write(value_position, pmath_ref(body));

  // Intentionally being sloppy here and using pmath_same instead of pmath_equals. 
  // This could cause false positives, but no false negatives since !pmath_equals(A,B) implies !pmath_same(A,B).
  did_any_change = !pmath_same(body, value);

  pmath_unref(value);
  pmath_unref(body);
  return did_any_change;
}

//} ============================================================================
//{ module init/done ...

static int dummy_cmp(pmath_t a, pmath_t b) {
  return 0;
}

static unsigned int dummy_hash(pmath_t a) {
  return 0;
}

PMATH_PRIVATE pmath_bool_t _pmath_symbol_values_init(void) {
  _pmath_init_special_type(
    PMATH_TYPE_SHIFT_MULTIRULE,
    dummy_cmp,
    dummy_hash,
    destroy_multirule,
    NULL,
    NULL);

  return TRUE;
}

PMATH_PRIVATE void _pmath_symbol_values_done(void) {
}

//}
