#ifndef __PMATH_CORE__EXPRESSIONS_PRIVATE_H__
#define __PMATH_CORE__EXPRESSIONS_PRIVATE_H__

#ifndef BUILDING_PMATH
#  error This header file is not part of the public pMath API
#endif

#include <pmath-core/objects-private.h>
#include <pmath-core/expressions.h>

#define PMATH_EXPRESSION_FLATTEN_MAX_DEPTH   (8)


typedef pmath_t pmath_dispatch_table_t;
typedef pmath_t pmath_custom_t;

struct _pmath_expr_t {
  struct _pmath_gc_t   inherited;
  size_t               length;
  pmath_atomic_t       metadata; // shared, mutable, struct _pmath_t*
  pmath_t              items[1];
};

struct _pmath_custom_expr_t {
  struct _pmath_expr_t internals;
  // After the  internals.length+1  many elements in  internals.items[*], 
  // there starts a `struct _pmath_custom_expr_data_t`
};

#define PMATH_CUSTOM_EXPR_DATA(EXPR_PTR)  ((struct _pmath_custom_expr_data_t*)&(EXPR_PTR)->internals.items[(EXPR_PTR)->internals.length + 1])

/**\brief Additional non-pmath_t data for a custom expression.
 */
struct _pmath_custom_expr_data_t {
  const struct _pmath_custom_expr_api_t *api;
};

struct _pmath_custom_expr_api_t {
  void         (*destroy_data)(           struct _pmath_custom_expr_data_t *data); // disposes contents of data, but not data itself
  size_t       (*get_length)(             struct _pmath_custom_expr_t *e);                                                        // does not free e
  pmath_t      (*get_item)(               struct _pmath_custom_expr_t *e, size_t i);       
  pmath_bool_t (*try_prevent_destruction)(struct _pmath_custom_expr_t *e);                                                        // does not free e
  pmath_bool_t (*try_get_item_range)(     struct _pmath_custom_expr_t *e, size_t start, size_t length, pmath_expr_t *result);     // does not free e
  pmath_bool_t (*try_set_item_copy)(      struct _pmath_custom_expr_t *e, size_t i, pmath_t new_item, pmath_expr_t *result);      // does not free e
  pmath_bool_t (*try_set_item_mutable)(   struct _pmath_custom_expr_t *e, size_t i, pmath_t new_item);                            // modifes e in-place (only if retuning TRUE).
  pmath_bool_t (*try_copy_shallow)(       struct _pmath_custom_expr_t *e, pmath_expr_t *result);                                  // does not free e
  pmath_bool_t (*try_shrink_associative)( struct _pmath_custom_expr_t *e, pmath_t magic_rem, pmath_expr_t *result);               // frees e iff returning TRUE
  pmath_bool_t (*try_resize_copy)(        struct _pmath_custom_expr_t *e, size_t new_len, pmath_expr_t *result);                  // does not free e
  pmath_bool_t (*try_resize_mutable)(     struct _pmath_custom_expr_t *e, size_t new_len, pmath_expr_t *result);                  // frees e iff returing TRUE
  pmath_bool_t (*try_compare_equal)(      struct _pmath_custom_expr_t *e, pmath_t other, pmath_bool_t *result);                   // does not free e or other
  pmath_bool_t (*try_maybe_contains_item)(struct _pmath_custom_expr_t *e, pmath_t item, pmath_bool_t *result);                    // does not free e or other
  pmath_bool_t (*try_item_equals)(        struct _pmath_custom_expr_t *e, size_t i, pmath_t expected_item, pmath_bool_t *result); // does not free e or expected_item
  //pmath_bool_t (*try_compare)(         struct _pmath_custom_expr_t *e, pmath_t other, int          *result); // does not free e or other
  //pmath_bool_t (*try_get_hash)(        struct _pmath_custom_expr_t *e, int *result);                         // does not free e
};

PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
struct _pmath_expr_t *_pmath_expr_new_noinit(size_t length);

PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
struct _pmath_custom_expr_t *_pmath_expr_new_custom(size_t len_internal, const struct _pmath_custom_expr_api_t *api, size_t data_size);

PMATH_PRIVATE
struct _pmath_custom_expr_t *_pmath_as_custom_expr_by_api(pmath_t obj, const struct _pmath_custom_expr_api_t *api);

PMATH_PRIVATE
size_t _pmath_expr_find_sorted(
  pmath_expr_t sorted_expr, // wont be freed
  pmath_t      item);       // wont be freed

PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
pmath_expr_t _pmath_expr_sort_ex(
  pmath_expr_t   expr, // will be freed
  int          (*cmp)(const pmath_t*, const pmath_t*));

PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
pmath_expr_t _pmath_expr_sort_ex_context(
  pmath_expr_t   expr, // will be freed
  int          (*cmp)(void*, const pmath_t*, const pmath_t*),
  void          *context);

PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
pmath_expr_t _pmath_expr_map(
  pmath_expr_t  expr, // will be freed
  size_t        start,
  size_t        end,
  pmath_t     (*func)(pmath_t, size_t, void*),
  void         *context);

PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
pmath_expr_t _pmath_expr_map_slow(
  pmath_expr_t  expr, // will be freed
  size_t        start,
  size_t        end,
  pmath_t     (*func)(pmath_t, size_t, void*),
  void         *context);

/* expr=f(args): thread f over any expression with given head in the range
                 start .. end
 */
PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
pmath_expr_t _pmath_expr_thread(
  pmath_expr_t  expr, // will be freed
  pmath_t       head, // wont be freed
  size_t        start,
  size_t        end,
  pmath_bool_t *error_message);



PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
pmath_t _pmath_expr_shrink_associative(
  pmath_expr_t  expr,
  pmath_t       magic_rem);


PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
pmath_t _pmath_expr_get_debug_metadata(pmath_expr_t expr);

PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
pmath_expr_t _pmath_expr_set_debug_metadata(pmath_expr_t expr, pmath_t info);

PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
pmath_dispatch_table_t _pmath_expr_get_dispatch_table(pmath_expr_t expr);

PMATH_PRIVATE
void _pmath_expr_attach_dispatch_table(pmath_expr_t expr, pmath_dispatch_table_t dispatch_table); // dispatch_table will be freeds

PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
pmath_custom_t _pmath_expr_get_custom_metadata(pmath_expr_t expr, void *dtor);

PMATH_PRIVATE
void _pmath_expr_attach_custom_metadata(pmath_expr_t expr, void *dtor, pmath_custom_t cert); // cert will be freed


PMATH_PRIVATE
pmath_bool_t _pmath_expr_is_updated(pmath_expr_t expr);

PMATH_PRIVATE
_pmath_timer_t _pmath_expr_last_change(pmath_expr_t expr);

PMATH_PRIVATE
void _pmath_expr_update(pmath_expr_t expr);

PMATH_PRIVATE
int _pmath_compare_exprsym(pmath_t a, pmath_t b);

PMATH_PRIVATE
pmath_bool_t _pmath_expr_equal(
  pmath_expr_t exprA,
  pmath_expr_t exprB);

PMATH_PRIVATE
void _pmath_expr_write(struct pmath_write_ex_t *info, pmath_t expr);

extern PMATH_PRIVATE pmath_expr_t _pmath_object_memory_exception; // read-only
extern PMATH_PRIVATE pmath_expr_t _pmath_object_emptylist;        // read-only
extern PMATH_PRIVATE pmath_expr_t _pmath_object_stop_message;     // read-only
//extern PMATH_PRIVATE pmath_expr_t _pmath_object_newsym_message; // read-only

PMATH_PRIVATE pmath_bool_t _pmath_expressions_init(void);
PMATH_PRIVATE void         _pmath_expressions_done(void);

#endif /* __PMATH_CORE__EXPRESSIONS_PRIVATE_H__ */
