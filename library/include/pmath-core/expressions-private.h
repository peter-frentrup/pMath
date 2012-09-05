#ifndef __PMATH_CORE__EXPRESSIONS_PRIVATE_H__
#define __PMATH_CORE__EXPRESSIONS_PRIVATE_H__

#ifndef BUILDING_PMATH
#error This header file is not part of the public pMath API
#endif

#include <pmath-core/objects-private.h>
#include <pmath-core/expressions.h>

#define PMATH_EXPRESSION_FLATTEN_MAX_DEPTH   (8)

struct _pmath_expr_t {
  struct _pmath_gc_t   inherited;
  size_t               length;
  pmath_t              items[1];
};

PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
struct _pmath_expr_t *_pmath_expr_new_noinit(size_t length);

// expr will be freed
PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
struct _pmath_expr_t *_pmath_expr_make_writeable(pmath_expr_t expr);

PMATH_PRIVATE
size_t _pmath_expr_find_sorted(
  pmath_expr_t sorted_expr, // wont be freed
  pmath_t      item);       // wont be freed

PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
pmath_expr_t _pmath_expr_sort_ex(
  pmath_expr_t   expr, // will be freed
  int          (*cmp)(pmath_t*, pmath_t*));

PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
pmath_expr_t _pmath_expr_sort_ex_context(
  pmath_expr_t   expr, // will be freed
  int          (*cmp)(void*, pmath_t*, pmath_t*),
  void          *context);
  
PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
pmath_expr_t _pmath_expr_map(
  pmath_expr_t  expr, // will be freed
  size_t        start,
  size_t        end,
  pmath_t     (*func)(pmath_t, void*),
  void         *context);

// expr=f(args): thread f over any expression with head h in the first coun args.
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
pmath_bool_t _pmath_expr_is_updated(pmath_expr_t expr);

PMATH_PRIVATE
_pmath_timer_t _pmath_expr_last_change(pmath_expr_t expr);

PMATH_PRIVATE
void _pmath_expr_update(pmath_expr_t expr);

PMATH_PRIVATE
int _pmath_compare_exprsym(pmath_t a, pmath_t b);

extern PMATH_PRIVATE pmath_expr_t _pmath_object_memory_exception; // read-only
extern PMATH_PRIVATE pmath_expr_t _pmath_object_emptylist;        // read-only
extern PMATH_PRIVATE pmath_expr_t _pmath_object_stop_message;     // read-only
//extern PMATH_PRIVATE pmath_expr_t _pmath_object_newsym_message; // read-only

PMATH_PRIVATE pmath_bool_t _pmath_expressions_init(void);
PMATH_PRIVATE void         _pmath_expressions_done(void);

#endif /* __PMATH_CORE__EXPRESSIONS_PRIVATE_H__ */
