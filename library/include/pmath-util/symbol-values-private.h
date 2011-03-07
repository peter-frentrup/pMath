#ifndef __PMATH_UTIL__SYMBOL_VALUES_PRIVATE_H__
#define __PMATH_UTIL__SYMBOL_VALUES_PRIVATE_H__

#ifndef BUILDING_PMATH
  #error This header file is not part of the public pMath API
#endif

#include <pmath-core/symbols.h>

#include <pmath-util/concurrency/atomic-private.h>
#include <pmath-util/hashtables-private.h>

struct _pmath_rulecache_t{ // do not access members directly, init all with PMATH_NULL
  void * volatile _table; // const patterns, no condition in rhs
  pmath_locked_t  _more;  // _pmath_multirule_t
};

struct _pmath_symbol_rules_t{ // init all with PMATH_NULL
  struct _pmath_rulecache_t  up_rules;      // ~(~~~,f,~~~), ~(~~~,f(~~~),~~~)
  struct _pmath_rulecache_t  down_rules;    // f(~~~)
  struct _pmath_rulecache_t  sub_rules;     // f(~~~)(~~~)...
  struct _pmath_rulecache_t  approx_rules;  // Approximate(~~~)
  struct _pmath_rulecache_t  default_rules; // Default(~~~), Options(~~~)
  struct _pmath_rulecache_t  format_rules;  // MakeBoxes(~~~)
  
  void * volatile _messages; // only access with _pmath_atomic_[un]lock_ptr()
                             // this is a hashtable of class pmath_ht_obj_class
};

struct _pmath_symbol_rules_entry_t{
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

PMATH_PRIVATE
pmath_bool_t _pmath_symbol_value_visit(
  pmath_t        value, // will be freed
  pmath_bool_t (*callback)(pmath_t,void*),
  void          *closure);

PMATH_PRIVATE
PMATH_ATTRIBUTE_NONNULL(1)
pmath_bool_t _pmath_rulecache_visit(
  struct _pmath_rulecache_t *rc, 
  pmath_bool_t (*callback)(pmath_t,void*),
  void *closure);

PMATH_PRIVATE
PMATH_ATTRIBUTE_NONNULL(1)
pmath_bool_t _pmath_symbol_rules_visit(
  struct _pmath_symbol_rules_t *rules, 
  pmath_bool_t (*callback)(pmath_t,void*),
  void *closure);

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
  pmath_t             to_be_removed, // wont be freed
  pmath_t             replacement);  // wont be freed

PMATH_PRIVATE
PMATH_ATTRIBUTE_NONNULL(1)
void _pmath_symbol_rules_remove_all(
  struct _pmath_symbol_rules_t *rules,
  pmath_t                to_be_removed, // wont be freed
  pmath_t                replacement);  // wont be freed

/*============================================================================*/

PMATH_PRIVATE
PMATH_ATTRIBUTE_NONNULL(1)
pmath_bool_t _pmath_rulecache_find(
  struct _pmath_rulecache_t *rc,
  pmath_t            *inout);

PMATH_PRIVATE
PMATH_ATTRIBUTE_NONNULL(1)
void _pmath_rulecache_change(
  struct _pmath_rulecache_t *rc,
  pmath_t             pattern, // will be freed
  pmath_t             body);   // will be freed, PMATH_UNDEFINED => remove rules

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

PMATH_PRIVATE
PMATH_ATTRIBUTE_NONNULL(1)
void _pmath_symbol_define_value_pos(
  pmath_locked_t *value_position,  //accessed via _pmath_object_atomic_[read|write]()
  pmath_t         pattern,         // will be freed; PMATH_UNDEFINED: ignore old value
  pmath_t         body);           // will be freed; PMATH_UNDEFINED: remove patterns
 
/*============================================================================*/

PMATH_PRIVATE pmath_bool_t _pmath_symbol_values_init(void);
PMATH_PRIVATE void         _pmath_symbol_values_done(void);

#endif // __PMATH_UTIL__SYMBOL_VALUES_PRIVATE_H__
