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

PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
struct _pmath_expr_t *_pmath_expr_new_noinit(size_t length);

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
