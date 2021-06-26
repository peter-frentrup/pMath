#ifndef __PMATH_UTIL__SYMBOL_VALUES_PRIVATE_H__
#define __PMATH_UTIL__SYMBOL_VALUES_PRIVATE_H__

#ifndef BUILDING_PMATH
#error This header file is not part of the public pMath API
#endif

#include <pmath-core/symbols.h>

#include <pmath-util/concurrency/atomic-private.h>
#include <pmath-util/hash/hashtables-private.h>

struct _pmath_rulecache_t { // do not access members directly, init all with PMATH_NULL
  pmath_atomic_t  _table; // pmath_hashtable_t, const patterns, no condition in rhs
  pmath_locked_t  _more;  // _pmath_multirule_t
};

struct _pmath_symbol_rules_t {
  struct _pmath_rulecache_t  up_rules;      // ~(~~~,f,~~~), ~(~~~,f(~~~),~~~)
  struct _pmath_rulecache_t  down_rules;    // f(~~~)
  struct _pmath_rulecache_t  sub_rules;     // f(~~~)(~~~)...
  struct _pmath_rulecache_t  approx_rules;  // N(~~~)
  struct _pmath_rulecache_t  default_rules; // Default(~~~), Options(~~~)
  struct _pmath_rulecache_t  format_rules;  // MakeBoxes(~~~)
  
  pmath_atomic_t _messages; // this is a pmath_hashtable_t of class pmath_ht_obj_class
  
  // Check with _pmath_security_check_builtin() before calling: 
  pmath_atomic_t early_call;  // pmath_builtin_func_t
  pmath_atomic_t up_call;     // pmath_builtin_func_t
  pmath_atomic_t down_call;   // pmath_builtin_func_t
  pmath_atomic_t sub_call;    // pmath_builtin_func_t
  pmath_atomic_t approx_call; // pmath_approx_func_t
};

struct _pmath_symbol_rules_entry_t {
  pmath_symbol_t                key;
  struct _pmath_symbol_rules_t  rules;
};

PMATH_PRIVATE
extern const pmath_ht_class_t  _pmath_symbol_rules_ht_class;
// hashtables of struct _pmath_symbol_rules_entry_t*
// used for _pmath_thread_t::local_rules

/*============================================================================*/

PMATH_PRIVATE
PMATH_ATTRIBUTE_NONNULL(1)
void _pmath_rulecache_copy(
  struct _pmath_rulecache_t *dst,
  struct _pmath_rulecache_t *src);

PMATH_PRIVATE
PMATH_ATTRIBUTE_NONNULL(1)
void _pmath_symbol_rules_copy(
  struct _pmath_symbol_rules_t *dst,
  struct _pmath_symbol_rules_t *src);

/*============================================================================*/

PMATH_PRIVATE
PMATH_ATTRIBUTE_NONNULL(1)
void _pmath_rulecache_done(struct _pmath_rulecache_t *rc);

PMATH_PRIVATE
PMATH_ATTRIBUTE_NONNULL(1)
void _pmath_symbol_rules_done(struct _pmath_symbol_rules_t *rules);

/*============================================================================*/
/* the callback function will be called while non-reentrant locks are hold!!! */

enum pmath_visit_result_t {
  PMATH_VISIT_ABORT = 0,
  PMATH_VISIT_NORMAL,
  PMATH_VISIT_SKIP
};

PMATH_PRIVATE
enum pmath_visit_result_t _pmath_symbol_value_visit(
  pmath_t                     value, // will be freed
  enum pmath_visit_result_t (*callback)(pmath_t, void*),
  void                       *closure);

PMATH_PRIVATE
PMATH_ATTRIBUTE_NONNULL(1)
enum pmath_visit_result_t _pmath_rulecache_visit(
  struct _pmath_rulecache_t  *rc,
  enum pmath_visit_result_t (*callback)(pmath_t, void*),
  void                       *closure);

PMATH_PRIVATE
PMATH_ATTRIBUTE_NONNULL(1)
enum pmath_visit_result_t _pmath_symbol_rules_visit(
  struct _pmath_symbol_rules_t  *rules,
  enum pmath_visit_result_t    (*callback)(pmath_t, void*),
  void                          *closure);

/*============================================================================*/

PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
pmath_t _pmath_symbol_value_remove_all(
  pmath_t  value,           // will be freed
  pmath_t  to_be_removed,   // wont be freed
  pmath_t  replacement);    // wont be freed

PMATH_PRIVATE
PMATH_ATTRIBUTE_NONNULL(1)
void _pmath_rulecache_remove_all(
  struct _pmath_rulecache_t *rc,
  pmath_t                    to_be_removed, // wont be freed
  pmath_t                    replacement);  // wont be freed

PMATH_PRIVATE
PMATH_ATTRIBUTE_NONNULL(1)
void _pmath_symbol_rules_remove_all(
  struct _pmath_symbol_rules_t *rules,
  pmath_t                       to_be_removed, // wont be freed
  pmath_t                       replacement);  // wont be freed

/*============================================================================*/

PMATH_PRIVATE
PMATH_ATTRIBUTE_NONNULL(1)
pmath_bool_t _pmath_rulecache_find(
  struct _pmath_rulecache_t *rc,
  pmath_t                   *inout);

/** Add, remove or modify a rule.
    \param rc The set of rules to modify. Must not be NULL
    \param pattern The new pattern (rule left-hand side). Will be freed.
    \param body The new right-hand side. Will be freed. PMATH_UNDEFINED means "remove the rule".
    \return Whether any modification took place.
    
    If validation of \a pattern with _pmath_pattern_validate() fails, \a rc will not be changed.
 */
PMATH_PRIVATE
PMATH_ATTRIBUTE_NONNULL(1)
pmath_bool_t _pmath_rulecache_change(
  struct _pmath_rulecache_t *rc,
  pmath_t                    pattern, // will be freed
  pmath_t                    body);   // will be freed, PMATH_UNDEFINED => remove rules

/*============================================================================*/

PMATH_PRIVATE
PMATH_ATTRIBUTE_NONNULL(1)
void _pmath_rulecache_clear(struct _pmath_rulecache_t *rc);

PMATH_PRIVATE
PMATH_ATTRIBUTE_NONNULL(1)
void _pmath_symbol_rules_clear(struct _pmath_symbol_rules_t *rules);

/*============================================================================*/

PMATH_PRIVATE
void _pmath_symbol_value_emit(
  pmath_symbol_t sym,    // wont be freed
  pmath_t        value); // will be freed

PMATH_PRIVATE
void _pmath_rule_table_emit(
  pmath_hashtable_t table); // class: pmath_ht_obj_class

PMATH_PRIVATE
PMATH_ATTRIBUTE_NONNULL(1)
void _pmath_rulecache_emit(
  struct _pmath_rulecache_t *rc);

/*============================================================================*/

PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
pmath_t _pmath_symbol_value_prepare(
  pmath_symbol_t sym,    // wont be freed
  pmath_t        value); // will be freed

PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
pmath_t _pmath_symbol_find_value(pmath_symbol_t sym); // sym wont be freed

/** Assign a function or value definition to \a value_position.
    \param value_position Where to store the value. This must always be accessed via 
                          _pmath_object_atomic_read(), _pmath_object_atomic_write() etc.
    \param pattern The left-hand side of the assignment; will be freed.
    \param body    The right-hand side of the assignment; will be freed.
    \return TRUE if the value changed and FALSE if the assignment was a no-op.
 */
PMATH_PRIVATE
PMATH_ATTRIBUTE_NONNULL(1)
pmath_bool_t _pmath_symbol_define_value_pos(
  pmath_locked_t *value_position,  //accessed via _pmath_object_atomic_[read|write]()
  pmath_t         pattern,         // will be freed; PMATH_UNDEFINED: ignore old value
  pmath_t         body);           // will be freed; PMATH_UNDEFINED: remove patterns

/*============================================================================*/

PMATH_PRIVATE pmath_bool_t _pmath_symbol_values_init(void);
PMATH_PRIVATE void         _pmath_symbol_values_done(void);

#endif // __PMATH_UTIL__SYMBOL_VALUES_PRIVATE_H__
