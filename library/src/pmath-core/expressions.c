#include <pmath-core/expressions-private.h>

#include <pmath-core/custom.h>
#include <pmath-core/numbers-private.h>
#include <pmath-core/packed-arrays-private.h>
#include <pmath-core/strings-private.h>

#include <pmath-language/patterns-private.h>
#include <pmath-language/tokens.h>

#include <pmath-util/concurrency/threads-private.h>
#include <pmath-util/debug.h>
#include <pmath-util/dispatch-tables-private.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/hash/incremental-hash-private.h>
#include <pmath-util/helpers.h>
#include <pmath-util/memory.h>
#include <pmath-util/messages.h>
#include <pmath-util/user-format-private.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/arithmetic-private.h>
#include <pmath-builtins/formating-private.h>
#include <pmath-builtins/lists-private.h>

#include <limits.h>
#include <stdio.h>
#include <string.h>


#ifdef _MSC_VER
#  define snprintf sprintf_s
#endif


struct _pmath_expr_part_t {
  struct _pmath_expr_t   inherited; // item: length = 1
  struct _pmath_expr_t  *buffer;
  size_t                 start;
};

#if PMATH_BITSIZE == 32
PMATH_STATIC_ASSERT(sizeof(struct _pmath_t)           == 8);
PMATH_STATIC_ASSERT(sizeof(struct _pmath_timed_t)     == 16);
PMATH_STATIC_ASSERT(sizeof(struct _pmath_gc_t)        == 24);
PMATH_STATIC_ASSERT(sizeof(struct _pmath_expr_t)      == 40);
PMATH_STATIC_ASSERT(sizeof(struct _pmath_expr_part_t) == 48);
#else
PMATH_STATIC_ASSERT(sizeof(struct _pmath_t)           == 16);
PMATH_STATIC_ASSERT(sizeof(struct _pmath_timed_t)     == 24);
PMATH_STATIC_ASSERT(sizeof(struct _pmath_gc_t)        == 32);
PMATH_STATIC_ASSERT(sizeof(struct _pmath_expr_t)      == 56);
PMATH_STATIC_ASSERT(sizeof(struct _pmath_expr_part_t) == 72);
#endif

PMATH_STATIC_ASSERT(sizeof(PMATH_GC_FLAGS32(((struct _pmath_gc_t*)NULL))) == 4);


#define PMATH_TYPE_EXPRESSION_WITH_GC_FLAGS32   (PMATH_TYPE_EXPRESSION_GENERAL | PMATH_TYPE_EXPRESSION_GENERAL_PART | PMATH_TYPE_CUSTOM_EXPRESSION)


extern pmath_symbol_t pmath_System_Alternatives;
extern pmath_symbol_t pmath_System_And;
extern pmath_symbol_t pmath_System_Assign;
extern pmath_symbol_t pmath_System_AssignDelayed;
extern pmath_symbol_t pmath_System_AssignWith;
extern pmath_symbol_t pmath_System_Automatic;
extern pmath_symbol_t pmath_System_BaseForm;
extern pmath_symbol_t pmath_System_Button;
extern pmath_symbol_t pmath_System_Colon;
extern pmath_symbol_t pmath_System_ColonForm;
extern pmath_symbol_t pmath_System_Complex;
extern pmath_symbol_t pmath_System_ComplexInfinity;
extern pmath_symbol_t pmath_System_Condition;
extern pmath_symbol_t pmath_System_Decrement;
extern pmath_symbol_t pmath_System_Derivative;
extern pmath_symbol_t pmath_System_DirectedInfinity;
extern pmath_symbol_t pmath_System_DivideBy;
extern pmath_symbol_t pmath_System_Equal;
extern pmath_symbol_t pmath_System_EvaluationSequence;
extern pmath_symbol_t pmath_System_FullForm;
extern pmath_symbol_t pmath_System_Function;
extern pmath_symbol_t pmath_System_Graphics;
extern pmath_symbol_t pmath_System_Greater;
extern pmath_symbol_t pmath_System_GreaterEqual;
extern pmath_symbol_t pmath_System_HoldForm;
extern pmath_symbol_t pmath_System_HorizontalForm;
extern pmath_symbol_t pmath_System_Identical;
extern pmath_symbol_t pmath_System_Increment;
extern pmath_symbol_t pmath_System_Inequation;
extern pmath_symbol_t pmath_System_Infinity;
extern pmath_symbol_t pmath_System_InputForm;
extern pmath_symbol_t pmath_System_Less;
extern pmath_symbol_t pmath_System_LessEqual;
extern pmath_symbol_t pmath_System_LinearSolveFunction;
extern pmath_symbol_t pmath_System_List;
extern pmath_symbol_t pmath_System_LongForm;
extern pmath_symbol_t pmath_System_MessageName;
extern pmath_symbol_t pmath_System_Not;
extern pmath_symbol_t pmath_System_Optional;
extern pmath_symbol_t pmath_System_Or;
extern pmath_symbol_t pmath_System_OutputForm;
extern pmath_symbol_t pmath_System_Part;
extern pmath_symbol_t pmath_System_Pattern;
extern pmath_symbol_t pmath_System_Plus;
extern pmath_symbol_t pmath_System_PostDecrement;
extern pmath_symbol_t pmath_System_PostIncrement;
extern pmath_symbol_t pmath_System_Power;
extern pmath_symbol_t pmath_System_PureArgument;
extern pmath_symbol_t pmath_System_Shallow;
extern pmath_symbol_t pmath_System_Short;
extern pmath_symbol_t pmath_System_SingleMatch;
extern pmath_symbol_t pmath_System_Skeleton;
extern pmath_symbol_t pmath_System_Range;
extern pmath_symbol_t pmath_System_RawBoxes;
extern pmath_symbol_t pmath_System_Repeated;
extern pmath_symbol_t pmath_System_Row;
extern pmath_symbol_t pmath_System_Rule;
extern pmath_symbol_t pmath_System_RuleDelayed;
extern pmath_symbol_t pmath_System_StandardForm;
extern pmath_symbol_t pmath_System_StringExpression;
extern pmath_symbol_t pmath_System_StringForm;
extern pmath_symbol_t pmath_System_TagAssign;
extern pmath_symbol_t pmath_System_TagAssignDelayed;
extern pmath_symbol_t pmath_System_TestPattern;
extern pmath_symbol_t pmath_System_Thread;
extern pmath_symbol_t pmath_System_Times;
extern pmath_symbol_t pmath_System_TimesBy;
extern pmath_symbol_t pmath_System_Tooltip;
extern pmath_symbol_t pmath_System_True;
extern pmath_symbol_t pmath_System_Undefined;
extern pmath_symbol_t pmath_System_Unequal;
extern pmath_symbol_t pmath_System_Unidentical;

extern pmath_symbol_t pmath_System_DollarPageWidth;
extern pmath_symbol_t pmath_System_BoxForm_DollarUseTextFormatting;

extern pmath_symbol_t pmath_Developer_PackedArrayForm;


// initialization in pmath_init():
PMATH_PRIVATE pmath_expr_t _pmath_object_memory_exception; // read-only
PMATH_PRIVATE pmath_expr_t _pmath_object_emptylist;        // read-only
PMATH_PRIVATE pmath_expr_t _pmath_object_stop_message;     // read-only
//PMATH_PRIVATE pmath_expr_t _pmath_object_newsym_message; // read-only

#define CACHES_MAX  8
#define CACHE_SIZE  32
#define CACHE_MASK  (CACHE_SIZE-1)

static pmath_atomic_t expr_caches[CACHES_MAX][CACHE_SIZE];
static pmath_atomic_t expr_cache_pos[CACHES_MAX];


typedef void *_pmath_metadata_kind_t; // either a METADATA_KIND_* or a pmath_custom_t destructor
#define METADATA_KIND_debug_metadata  ((void*)1)
#define METADATA_KIND_DISPATCH_TABLE  ((void*)2)


static pmath_bool_t metadata_find(pmath_t *metadata, _pmath_metadata_kind_t kind);
static pmath_bool_t try_merge_metadata(pmath_t *metadata, _pmath_metadata_kind_t kind, pmath_t new_of_kind);
static pmath_t get_metadata(pmath_expr_t expr, _pmath_metadata_kind_t kind);
static pmath_t merge_metadata(pmath_expr_t expr, _pmath_metadata_kind_t kind, pmath_t new_of_kind);
static void clear_metadata(struct _pmath_expr_t *_expr);
static void attach_metadata(struct _pmath_expr_t *_expr, _pmath_metadata_kind_t kind, pmath_t new_of_kind);


static pmath_t      custom_expr_expand_to_normal(struct _pmath_custom_expr_t *_expr);
static size_t       custom_expr_length(          struct _pmath_custom_expr_t *_expr);
static pmath_t      custom_expr_get_item(        struct _pmath_custom_expr_t *_expr, size_t index);
static pmath_bool_t custom_expr_item_equals(     struct _pmath_custom_expr_t *_expr, size_t index, pmath_t expected_item);
static pmath_expr_t custom_expr_get_item_range(struct _pmath_custom_expr_t *_expr, size_t start, size_t length);
static pmath_expr_t custom_expr_set_item(      struct _pmath_custom_expr_t *_expr, size_t index, pmath_t new_item);
static pmath_expr_t custom_expr_shrink_associative(struct _pmath_custom_expr_t *_expr, pmath_t magic_rem);
static size_t custom_expr_find_sorted(             struct _pmath_custom_expr_t *_expr, pmath_t item);

#ifdef PMATH_DEBUG_LOG
static pmath_atomic_t expr_cache_hits   = PMATH_ATOMIC_STATIC_INIT;
static pmath_atomic_t expr_cache_misses = PMATH_ATOMIC_STATIC_INIT;
#endif


static uintptr_t expr_cache_inc(size_t length, intptr_t delta) {
  assert(length < CACHES_MAX);

  return (uintptr_t)pmath_atomic_fetch_add(&expr_cache_pos[length], delta);
}

static struct _pmath_expr_t *expr_cache_swap(
  size_t                         length,
  uintptr_t                      i,
  struct _pmath_expr_t *value
) {
  i = i & CACHE_MASK;

  assert(length < CACHES_MAX);
  assert(!value || value->length == length);

  return (struct _pmath_expr_t *)
         pmath_atomic_fetch_set(&expr_caches[length][i], (intptr_t)value);
}

static void expr_cache_clear(void) {
  size_t len;
  uintptr_t i;

  for(len = 0; len < CACHES_MAX; ++len) {
    for(i = 0; i < CACHE_SIZE; ++i) {
      struct _pmath_expr_t *expr = expr_cache_swap(len, i, NULL);

      if(expr) {
        assert(expr->inherited.inherited.inherited.refcount._data == 0);

        pmath_mem_free(expr);
      }
    }
  }
}

static void reset_expr_flags(struct _pmath_expr_t *expr) {
  pmath_atomic_write_uint8_release( &expr->inherited.inherited.inherited.flags8,  0);
  pmath_atomic_write_uint16_release(&expr->inherited.inherited.inherited.flags16, 0);
  pmath_atomic_write_uint32_release(&PMATH_GC_FLAGS32(&expr->inherited), 0);
}

static void touch_expr(struct _pmath_expr_t *expr) {
  expr->inherited.inherited.last_change = -_pmath_timer_get_next();
}

PMATH_PRIVATE
struct _pmath_expr_t *_pmath_expr_new_noinit(size_t length) {
  struct _pmath_expr_t *expr;

  assert(length < UINTPTR_MAX / sizeof(pmath_t) - sizeof(struct _pmath_expr_t));

  if(length < CACHES_MAX) {
    uintptr_t i = expr_cache_inc(length, -1);

    expr = expr_cache_swap(length, i - 1, NULL);
    if(expr) {
#ifdef PMATH_DEBUG_LOG
      (void)pmath_atomic_fetch_add(&expr_cache_hits, 1);
#endif

#if defined(PMATH_DEBUG_LOG) || defined(NDEBUG)
      if(expr->inherited.inherited.inherited.refcount._data != 0) {
        pmath_debug_print("expr refcount = %d\n", (int)expr->inherited.inherited.inherited.refcount._data);
        assert(expr->inherited.inherited.inherited.refcount._data == 0);
      }
#endif

      pmath_atomic_write_release(&expr->inherited.inherited.inherited.refcount, 1);

      touch_expr(expr);
      expr->inherited.gc_refcount = 0;
      expr->length                = length;
      pmath_atomic_write_release(&expr->metadata, 0);
      reset_expr_flags(expr);

      return expr;
    }

#ifdef PMATH_DEBUG_LOG
    (void)pmath_atomic_fetch_add(&expr_cache_misses, 1);
#endif
  }

  expr = (struct _pmath_expr_t *)PMATH_AS_PTR(_pmath_create_stub(
           PMATH_TYPE_SHIFT_EXPRESSION_GENERAL,
           sizeof(struct _pmath_expr_t) + length * sizeof(pmath_t)));

  if(PMATH_UNLIKELY(!expr))
    return NULL;

  touch_expr(expr);
  expr->inherited.gc_refcount = 0;
  expr->length                = length;
  pmath_atomic_write_uint32_release(&PMATH_GC_FLAGS32(&expr->inherited), 0);
  pmath_atomic_write_release(&expr->metadata, 0);

  return expr;
}

PMATH_PRIVATE
struct _pmath_custom_expr_t *_pmath_expr_new_custom(size_t len_internal, const struct _pmath_custom_expr_api_t *api, size_t data_size) {
  struct _pmath_custom_expr_t *expr;
  
  assert(data_size >= sizeof(struct _pmath_custom_expr_data_t));
  assert(api             != NULL);
  assert(api->get_length != NULL);
  assert(api->get_item   != NULL);
  
  expr = (void*)PMATH_AS_PTR(_pmath_create_stub(
           PMATH_TYPE_SHIFT_CUSTOM_EXPRESSION,
           sizeof(struct _pmath_custom_expr_t) + len_internal * sizeof(pmath_t) + data_size));
  
  if(PMATH_UNLIKELY(!expr))
    return NULL;
  
  touch_expr(&expr->internals);
  expr->internals.inherited.gc_refcount = 0;
  expr->internals.length                = len_internal;
  memset(expr->internals.items, 0, (1 + len_internal) * sizeof(pmath_t) + data_size);
  pmath_atomic_write_uint32_release(&PMATH_GC_FLAGS32(&expr->internals.inherited), 0);
  pmath_atomic_write_release(&expr->internals.metadata, 0);
  PMATH_CUSTOM_EXPR_DATA(expr)->api     = api;
  
  return expr;
}

PMATH_PRIVATE
struct _pmath_custom_expr_t *_pmath_as_custom_expr_by_api(pmath_t obj, const struct _pmath_custom_expr_api_t *api) {
  if(!pmath_is_pointer_of(obj, PMATH_TYPE_CUSTOM_EXPRESSION))
    return NULL;
  
  struct _pmath_custom_expr_t *_expr = (void*)PMATH_AS_PTR(obj);
  if(PMATH_CUSTOM_EXPR_DATA(_expr)->api == api)
    return _expr;
  
  return NULL;
}

PMATH_PRIVATE
void _pmath_custom_expr_changed(struct _pmath_custom_expr_t *e) {
  assert(pmath_atomic_read_aquire(&e->internals.inherited.inherited.inherited.refcount) == 1);
  
  reset_expr_flags(&e->internals);
  clear_metadata(  &e->internals);
  touch_expr(      &e->internals);
}


// called when all items are freed
static void end_destroy_general_expression(struct _pmath_expr_t *expr) {
  if(expr->length < CACHES_MAX) {
    uintptr_t ui = expr_cache_inc(expr->length, +1);

    expr = expr_cache_swap(expr->length, ui, expr);
  }

  pmath_mem_free(expr);
}

static void destroy_general_expression(pmath_t expr) {
  struct _pmath_expr_t *_expr = (void *)PMATH_AS_PTR(expr);
  
  PMATH_OBJECT_MARK_DELETION_TRAP(&_expr->inherited.inherited.inherited);
  
  size_t i;
  for(i = 0; i <= _expr->length; i++)
    pmath_unref(_expr->items[i]);
  
  clear_metadata(_expr);
  end_destroy_general_expression(_expr);
}

static void destroy_part_expression(pmath_t expr) {
  struct _pmath_expr_part_t *_expr = (void *)PMATH_AS_PTR(expr);
  
  PMATH_OBJECT_MARK_DELETION_TRAP(&_expr->inherited.inherited.inherited.inherited);
  
  clear_metadata(&_expr->inherited);

  pmath_unref(_expr->inherited.items[0]);
  pmath_unref(PMATH_FROM_PTR(_expr->buffer));

  pmath_mem_free(_expr);
}

static void destroy_custom_expression(pmath_t expr) {
  struct _pmath_custom_expr_t *_expr = (void *)PMATH_AS_PTR(expr);
  
  struct _pmath_custom_expr_data_t *_data = PMATH_CUSTOM_EXPR_DATA(_expr);
  
  PMATH_OBJECT_MARK_DELETION_TRAP(&_expr->internals.inherited.inherited.inherited);
  
  size_t i;
  for(i = 0; i <= _expr->internals.length; i++)
    pmath_unref(_expr->internals.items[i]);
  
  clear_metadata(&_expr->internals);

  if(_data->api->destroy_data)
    _data->api->destroy_data(_data);
  
  pmath_mem_free(_expr);
}

static pmath_bool_t expr_general_visit_refs(struct _pmath_expr_t *_expr, pmath_bool_t (*callback)(pmath_t *item, void *ctx), void *ctx) {
  for(size_t i = 0; i <= _expr->length; ++i)
    if(callback(&_expr->items[i], ctx))
      return TRUE;
  
  // TODO: if(callback(&_expr->metadata, ctx)) return TRUE;
  
  return FALSE;
}

static pmath_bool_t expr_part_visit_refs(struct _pmath_expr_part_t *_expr, pmath_bool_t (*callback)(pmath_t *item, void *ctx), void *ctx) {
  if(callback(&_expr->inherited.items[0], ctx))
    return TRUE;
  
  // TODO: if(callback(&_expr->inherited.metadata, ctx)) return TRUE;
  
  // TODO: if(callback(&_expr->buffer, ctx)) return TRUE;
  
  return FALSE;
}

static pmath_bool_t obj_visit_refs(struct _pmath_t *obj, pmath_bool_t (*callback)(pmath_t *item, void *ctx), void *ctx) {
  if(obj == NULL)
    return FALSE;
  
  switch(obj->type_shift) {
    case PMATH_TYPE_SHIFT_EXPRESSION_GENERAL:
    case PMATH_TYPE_SHIFT_CUSTOM_EXPRESSION:       return expr_general_visit_refs((void*)obj, callback, ctx);
    case PMATH_TYPE_SHIFT_EXPRESSION_GENERAL_PART: return expr_part_visit_refs(   (void*)obj, callback, ctx);
  }
  
  return FALSE;
}

static pmath_bool_t expr_prevent_destruction(struct _pmath_t *obj) {
  assert(obj != NULL);
  
  if(PMATH_UNLIKELY(obj->type_shift == PMATH_TYPE_SHIFT_CUSTOM_EXPRESSION)) {
    struct _pmath_custom_expr_t      *_expr = (void *)obj;
    struct _pmath_custom_expr_data_t *_data = PMATH_CUSTOM_EXPR_DATA(_expr);
    
    if(_data->api->try_prevent_destruction)
      return _data->api->try_prevent_destruction(_expr);
  }
  
  return FALSE;
}

struct _destructor_leftmost_extraction_t {
  struct _pmath_t *leftmost_to_be_destroyed; // dangling object
  pmath_t         *first_free; // points to a free slot (PMATH_NULL).
  pmath_t         *more;       // points to some still in use slot after leftmost_to_be_destroyed
};

static pmath_bool_t extract_leftmost_callback(pmath_t *next_place, void *_ctx) {
  struct _destructor_leftmost_extraction_t *ctx = _ctx;
  
  if(ctx->first_free == NULL)
    ctx->first_free = next_place;
  
  if(ctx->leftmost_to_be_destroyed == NULL) {
    pmath_t next = *next_place;
    if(pmath_is_null(next))
      return FALSE; // continue;
    
    *next_place = PMATH_NULL;
    if(pmath_is_pointer(next)) {
      struct _pmath_t *next_ptr = PMATH_AS_PTR(next);
      if(!_pmath_prepare_destroy(next_ptr))
        return FALSE; // continue;
      
      if(expr_prevent_destruction(next_ptr))
        return FALSE; // continue;
      
      ctx->leftmost_to_be_destroyed = next_ptr;
    }
    
    return FALSE; // continue;
  }
  
  ctx->more = next_place;
  return TRUE;
}

static void destruction_extract_leftmost(struct _pmath_t *noref_obj, struct _destructor_leftmost_extraction_t *result) {
  result->leftmost_to_be_destroyed = NULL;
  result->first_free               = NULL;
  result->more                     = NULL;
  
  if(noref_obj) {
    assert(pmath_atomic_read_aquire(&noref_obj->refcount) == 0);
    
    obj_visit_refs(noref_obj, extract_leftmost_callback, result);
  }
}

static void destroy_final_cleanup(struct _pmath_t *obj) {
  assert(obj != NULL);
  
  // All items that can be visited via obj_visit_refs are now already freed and set to PMATH_NULL.
  // TODO: No need to visit them again to call pmath_unref()
  switch(obj->type_shift) {
    case PMATH_TYPE_SHIFT_EXPRESSION_GENERAL:      destroy_general_expression(PMATH_FROM_PTR(obj)); break;
    case PMATH_TYPE_SHIFT_EXPRESSION_GENERAL_PART: destroy_part_expression(   PMATH_FROM_PTR(obj)); break;
    case PMATH_TYPE_SHIFT_CUSTOM_EXPRESSION:       destroy_custom_expression( PMATH_FROM_PTR(obj)); break;
    default:                                       _pmath_destroy_object(     PMATH_FROM_PTR(obj)); break;
  }
}

// Destroy an arbitrarily nested tree structure without ever exhausting C stack space.
// The idea is to rotate binary trees as long as there is a nontrivial left subtree.
// See e.g. https://matklad.github.io/2022/11/18/if-a-tree-falls-in-a-forest-does-it-overflow-the-stack.html
static void destroy_expr_tree(pmath_t expr) {
  struct _pmath_t *obj = PMATH_AS_PTR(expr);
  
  if(expr_prevent_destruction(obj))
    return;
  
  for(;;) {
    struct _destructor_leftmost_extraction_t A;
    
    destruction_extract_leftmost(obj, &A);
    
    // single leaf node A
    if(A.leftmost_to_be_destroyed == NULL) {
      destroy_final_cleanup(obj);
      return;
    }
  
    assert(A.first_free != NULL);
    assert(pmath_is_null(*A.first_free));
    
    // single linked list: A -> B
    if(A.more == NULL) {
      destroy_final_cleanup(obj);
      obj = A.leftmost_to_be_destroyed;
      continue;
    }
    
    // tree:    A
    //         / \
    //        B   y
    struct _destructor_leftmost_extraction_t B;
    destruction_extract_leftmost(A.leftmost_to_be_destroyed, &B);
    
    // B is a leaf
    if(B.leftmost_to_be_destroyed == NULL) {
      destroy_final_cleanup(A.leftmost_to_be_destroyed);
      continue;
    }
    
    assert(B.first_free != NULL);
    assert(pmath_is_null(*B.first_free));
      
    // B is non-trivial, has at least one child X. Will need to store A into B's subtree and visit A and X again later.
    // Revive A (=obj)
    assert(pmath_atomic_read_aquire(&obj->refcount) == 0);
    //(void)pmath_atomic_fetch_add( &obj->refcount, 1);
    pmath_atomic_write_release(     &obj->refcount, 1);
  
    // Revive X (=B.leftmost_to_be_destroyed)
    assert(pmath_atomic_read_aquire(&B.leftmost_to_be_destroyed->refcount) == 0);
    //(void)pmath_atomic_fetch_add( &B.leftmost_to_be_destroyed->refcount, 1);
    pmath_atomic_write_release(     &B.leftmost_to_be_destroyed->refcount, 1);
    
    if(B.more == NULL) {
      // B is singly linked list   obj=A            B
      //                              / \           |
      //                             B   y   =>     A
      //                             |             / \
      //                             X            X   y
      
      *A.first_free = PMATH_FROM_PTR(B.leftmost_to_be_destroyed);
      *B.first_free = PMATH_FROM_PTR(obj);
      
      obj = A.leftmost_to_be_destroyed;
    }
    else {
      // B is a tree      obj=A              B
      //                     / \            / \
      //                    B   y    =>    X   A
      //                   / \                / \
      //                  X   z              z   y
      
      *A.first_free = *B.more;
      *B.more       = PMATH_FROM_PTR(obj);
      *B.first_free = PMATH_FROM_PTR(B.leftmost_to_be_destroyed);
      
      obj = A.leftmost_to_be_destroyed;
    }
  }
}

/*============================================================================*/

PMATH_API
pmath_expr_t pmath_expr_new(
  pmath_t head,
  size_t  length
) {
  struct _pmath_expr_t *expr;

  if(PMATH_UNLIKELY(length >= UINTPTR_MAX / sizeof(pmath_t) - sizeof(struct _pmath_expr_t))) {
    // overflow
    pmath_unref(head);
    pmath_abort_please();
    return PMATH_NULL;
  }

  expr = _pmath_expr_new_noinit(length);

  if(PMATH_UNLIKELY(!expr)) {
    pmath_unref(head);
    return PMATH_NULL;
  }

  expr->items[0] = head;

  // setting all elements to double 0.0:
  memset(&(expr->items[1]), 0, length * sizeof(pmath_t));

  return PMATH_FROM_PTR(expr);
}

PMATH_API
pmath_expr_t pmath_expr_new_extended(
  pmath_t head,
  size_t  length,
  ...
) {
  struct _pmath_expr_t *expr;
  size_t i;
  va_list items;
  va_start(items, length);

  if(PMATH_UNLIKELY(length >= UINTPTR_MAX / sizeof(pmath_t) - sizeof(struct _pmath_expr_t))) {
    // overflow
    pmath_unref(head);

    for(i = 1; i <= length; i++)
      pmath_unref(va_arg(items, pmath_t));

    va_end(items);
    pmath_abort_please();
    return PMATH_NULL;
  }

  expr = _pmath_expr_new_noinit(length);
  if(PMATH_UNLIKELY(!expr)) {
    pmath_unref(head);

    for(i = 1; i <= length; i++)
      pmath_unref(va_arg(items, pmath_t));

    va_end(items);
    return PMATH_NULL;
  }

  expr->items[0] = head;

  for(i = 1; i <= length; i++)
    expr->items[i] = va_arg(items, pmath_t);

  va_end(items);
  return PMATH_FROM_PTR(expr);
}

PMATH_API
pmath_expr_t pmath_expr_resize(
  pmath_expr_t expr,
  size_t       new_length
) {
  struct _pmath_expr_t *old_expr;
  struct _pmath_expr_t *new_expr;
  size_t old_length;

  if(pmath_is_packed_array(expr))
    return _pmath_packed_array_resize(expr, new_length);
  
  old_expr = (void *)PMATH_AS_PTR(expr);

  if(PMATH_UNLIKELY(!old_expr))
    return pmath_expr_new(PMATH_NULL, new_length);
  
  pmath_bool_t has_multi_ref = (pmath_atomic_read_aquire(&old_expr->inherited.inherited.inherited.refcount) != 1);
  if(old_expr->inherited.inherited.inherited.type_shift == PMATH_TYPE_SHIFT_CUSTOM_EXPRESSION) {
    struct _pmath_custom_expr_t      *cust = (void*)old_expr;
    struct _pmath_custom_expr_data_t *data = PMATH_CUSTOM_EXPR_DATA(cust);
    
    pmath_t result = PMATH_NULL;
    if(has_multi_ref) {
      if(data->api->try_resize_copy && data->api->try_resize_copy(cust, new_length, &result)) {
        pmath_unref(expr);
        return result;
      }
    }
    else {
      if(data->api->try_resize_mutable && data->api->try_resize_mutable(cust, new_length, &result))
        return result;
    }
  }
    
  if(PMATH_UNLIKELY(new_length >= UINTPTR_MAX / sizeof(pmath_t) - sizeof(struct _pmath_expr_t))) {
    pmath_unref(expr);
    pmath_abort_please();
    return PMATH_NULL;
  }

  assert(pmath_is_expr(expr));

  old_length = pmath_expr_length(expr);
  
  if(new_length == old_length)
    return expr;

  if( has_multi_ref ||
      old_expr->inherited.inherited.inherited.type_shift != PMATH_TYPE_SHIFT_EXPRESSION_GENERAL)
  {
    new_expr = _pmath_expr_new_noinit(new_length);

    if(new_expr) {
      size_t init_len = old_length;

      if(init_len < new_length) {
        // init the rest with 0 bytes === (double) 0.0

        memset(&new_expr->items[1 + init_len], 0, (new_length - init_len) * sizeof(pmath_t));
      }
      else {
        init_len = new_length;
      }

      switch(PMATH_AS_PTR(expr)->type_shift) {
        case PMATH_TYPE_SHIFT_EXPRESSION_GENERAL: {
            for(size_t i = 0; i <= init_len; i++)
              new_expr->items[i] = pmath_ref(old_expr->items[i]);
          } break;

        case PMATH_TYPE_SHIFT_EXPRESSION_GENERAL_PART: {
            struct _pmath_expr_part_t *part = (void *)old_expr;

            new_expr->items[0] = pmath_ref(old_expr->items[0]);

            for(size_t i = 1; i <= init_len; i++)
              new_expr->items[i] = pmath_ref(part->buffer->items[part->start + i - 1]);
          } break;
          
        case PMATH_TYPE_SHIFT_CUSTOM_EXPRESSION: {
            struct _pmath_custom_expr_t      *cust = (void*)old_expr;
            struct _pmath_custom_expr_data_t *data = PMATH_CUSTOM_EXPR_DATA(cust);
            
            for(size_t i = 0; i <= init_len; i++)
              new_expr->items[i] = data->api->get_item(cust, i);
          } break;
          
        default:
          assert("invalid expression type" && 0);
      }
    }

    pmath_unref(expr);
    return PMATH_FROM_PTR(new_expr);
  }

  if(new_length < old_length) {
    for(size_t i = new_length + 1; i <= old_length; i++)
      pmath_unref(old_expr->items[i]);
  }

  new_expr = pmath_mem_realloc_no_failfree(
               old_expr,
               sizeof(struct _pmath_expr_t) + new_length * sizeof(pmath_t)
             );

  if(new_expr == NULL) {
    if(new_length < old_length) {
      // setting moved elements to double 0.0:
      memset(
        &(old_expr->items[new_length + 1]),
        0,
        (old_length - new_length) * sizeof(pmath_t));
    }
    pmath_unref(expr);
    return PMATH_NULL;
  }

  if(new_length > old_length) {
    // setting new elements to double 0.0:
    memset(
      &(new_expr->items[old_length + 1]),
      0,
      (new_length - old_length) * sizeof(pmath_t));
  }

  new_expr->length = new_length;
  reset_expr_flags(new_expr);
  clear_metadata(new_expr);
  touch_expr(new_expr);
  return PMATH_FROM_PTR(new_expr);
}

PMATH_API pmath_expr_t pmath_expr_append(
  pmath_expr_t expr,
  size_t       count,
  ...
) {
  size_t i, old_length, new_length;
  va_list items;
  va_start(items, count);

  old_length = pmath_expr_length(expr);
  new_length = old_length + count;

  if(new_length < old_length) { // overflow
    pmath_unref(expr);

    for(i = 1; i <= count; i++)
      pmath_unref(va_arg(items, pmath_t));
    va_end(items);

    pmath_abort_please();
    return PMATH_NULL;
  }

  expr = pmath_expr_resize(expr, new_length);

  if(pmath_is_null(expr)) {
    for(i = 1; i <= count; i++)
      pmath_unref(va_arg(items, pmath_t));

    va_end(items);
    return PMATH_NULL;
  }

  if(PMATH_AS_PTR(expr)->type_shift == PMATH_TYPE_SHIFT_EXPRESSION_GENERAL) {
    struct _pmath_expr_t *new_expr;

    new_expr = (void *)PMATH_AS_PTR(expr);

    for(i = old_length + 1; i <= old_length + count; i++)
      new_expr->items[i] = va_arg(items, pmath_t);
  }
  else {
    for(i = old_length + 1; i <= old_length + count; i++)
      expr = pmath_expr_set_item(expr, i, va_arg(items, pmath_t));
  }

  va_end(items);
  return expr;
}

PMATH_API size_t pmath_expr_length(pmath_expr_t expr) {
  if(pmath_is_null(expr))
    return 0;

  if(pmath_is_packed_array(expr))
    return *pmath_packed_array_get_sizes(expr);

  assert(pmath_is_expr(expr));
  
  struct _pmath_expr_t *_expr = (void*)PMATH_AS_PTR(expr);
  if(_expr->inherited.inherited.inherited.type_shift == PMATH_TYPE_SHIFT_CUSTOM_EXPRESSION) 
    return custom_expr_length((struct _pmath_custom_expr_t*)_expr);
  
  return _expr->length;
}

PMATH_API pmath_t pmath_expr_get_item(
  pmath_expr_t expr,
  size_t       index
) {
  struct _pmath_expr_part_t *expr_part_ptr;
  if(pmath_is_null(expr))
    return PMATH_NULL;

  if(pmath_is_packed_array(expr))
    return _pmath_packed_array_get_item(expr, index);

  assert(pmath_is_expr(expr));
  
  if(PMATH_AS_PTR(expr)->type_shift == PMATH_TYPE_SHIFT_CUSTOM_EXPRESSION) 
    return custom_expr_get_item((struct _pmath_custom_expr_t*)PMATH_AS_PTR(expr), index);
  
  expr_part_ptr = (struct _pmath_expr_part_t *)PMATH_AS_PTR(expr);

  if(index > expr_part_ptr->inherited.length)
    return PMATH_NULL;

  if(PMATH_AS_PTR(expr)->type_shift == PMATH_TYPE_SHIFT_EXPRESSION_GENERAL || index == 0)
    return pmath_ref(expr_part_ptr->inherited.items[index]);

  assert(PMATH_AS_PTR(expr)->type_shift == PMATH_TYPE_SHIFT_EXPRESSION_GENERAL_PART);

  return pmath_ref(expr_part_ptr->buffer->items[expr_part_ptr->start + index - 1]);
}

PMATH_API pmath_t pmath_expr_extract_item(
  pmath_expr_t expr,
  size_t       index
) {
  struct _pmath_expr_part_t *expr_part_ptr;

  if(pmath_is_null(expr))
    return PMATH_NULL;

  if(pmath_is_packed_array(expr))
    return _pmath_packed_array_get_item(expr, index);

  assert(pmath_is_expr(expr));
  
  if(PMATH_AS_PTR(expr)->type_shift == PMATH_TYPE_SHIFT_CUSTOM_EXPRESSION) 
    return custom_expr_get_item((struct _pmath_custom_expr_t*)PMATH_AS_PTR(expr), index);
  
  expr_part_ptr = (struct _pmath_expr_part_t *)PMATH_AS_PTR(expr);

  if(index > expr_part_ptr->inherited.length)
    return PMATH_NULL;

  if(PMATH_AS_PTR(expr)->type_shift == PMATH_TYPE_SHIFT_EXPRESSION_GENERAL || index == 0) {
    pmath_t item = expr_part_ptr->inherited.items[index];

    if(pmath_refcount(expr) == 1) {
      uint8_t  flags8  = pmath_atomic_read_uint8_aquire( &expr_part_ptr->inherited.inherited.inherited.inherited.flags8);
      if(flags8 & PMATH_OBJECT_FLAGS8_VALID) {
        pmath_debug_print_object("[pmath_expr_extract_item: keep do not rip apart expr with recount==1 because of VALID flag: ", expr, "]\n");
      }
      else {
        // Do not edit in-place if e.g. the VALID flag is set, because the flag would get lost when re-inserting the item, even if that did not change.

        // TODO: also check that metadata is NULL?
        expr_part_ptr->inherited.items[index] = PMATH_UNDEFINED;
        return item;
      }
    }

    return pmath_ref(item);
  }

  assert(PMATH_AS_PTR(expr)->type_shift == PMATH_TYPE_SHIFT_EXPRESSION_GENERAL_PART);
  return pmath_ref(expr_part_ptr->buffer->items[expr_part_ptr->start + index - 1]);
}

PMATH_API pmath_bool_t pmath_expr_item_equals(
  pmath_expr_t expr,
  size_t       index,
  pmath_t      expected_item
) {
  if(pmath_is_null(expr))
    return pmath_is_null(expected_item);
  
  assert(pmath_is_expr(expr));
  
  switch(PMATH_AS_PTR(expr)->type_shift) {
    case PMATH_TYPE_SHIFT_PACKED_ARRAY: break; // TODO
    case PMATH_TYPE_SHIFT_EXPRESSION_GENERAL: {
      struct _pmath_expr_t *expr_ptr = (void*)PMATH_AS_PTR(expr);

      if(index > expr_ptr->length)
        return pmath_is_null(expected_item);
      
      return pmath_equals(expr_ptr->items[index], expected_item);
    }
    case PMATH_TYPE_SHIFT_EXPRESSION_GENERAL_PART: {
      struct _pmath_expr_part_t *expr_part_ptr = (void*)PMATH_AS_PTR(expr);
      
      if(index == 0)
        return pmath_equals(expr_part_ptr->inherited.items[0], expected_item);
      
      if(index > expr_part_ptr->inherited.length)
        return pmath_is_null(expected_item);
      
      return pmath_equals(expr_part_ptr->buffer->items[expr_part_ptr->start + index - 1], expected_item);
    }
    case PMATH_TYPE_SHIFT_CUSTOM_EXPRESSION: return custom_expr_item_equals((void*)PMATH_AS_PTR(expr), index, expected_item);
  }
  
  // fall back
  pmath_t item = pmath_expr_get_item(expr, index);
  pmath_bool_t eq = pmath_equals(item, expected_item);
  pmath_unref(item);
  return eq;
}

PMATH_API pmath_expr_t pmath_expr_get_item_range(
  pmath_expr_t expr,
  size_t       start,
  size_t       length
) {
  struct _pmath_expr_t *old_expr;
  const size_t exprlen = pmath_expr_length(expr);

  old_expr = (void *)PMATH_AS_PTR(expr);

  if(PMATH_UNLIKELY(!old_expr))
    return PMATH_NULL;

  if(pmath_is_packed_array(expr))
    return _pmath_packed_array_get_item_range(expr, start, length);

  assert(pmath_is_expr(expr));

  if(start == 1 && length >= exprlen)
    return pmath_ref(expr);

  if(start > exprlen || length == 0)
    return pmath_expr_new(pmath_expr_get_item(expr, 0), 0);

  if(length > exprlen || start + length > exprlen + 1)
    length = exprlen + 1 - start;

  switch(PMATH_AS_PTR(expr)->type_shift) {
    case PMATH_TYPE_SHIFT_EXPRESSION_GENERAL: {
        struct _pmath_expr_part_t *new_expr_part;

        new_expr_part = (void *)PMATH_AS_PTR(
                          _pmath_create_stub(
                            PMATH_TYPE_SHIFT_EXPRESSION_GENERAL_PART,
                            sizeof(struct _pmath_expr_part_t)));

        if(!new_expr_part)
          return PMATH_NULL;

        touch_expr(&new_expr_part->inherited);
        new_expr_part->inherited.inherited.gc_refcount = 0;
        new_expr_part->inherited.length                = length;
        new_expr_part->inherited.items[0]              = pmath_ref(old_expr->items[0]);
        pmath_atomic_write_uint32_release(&PMATH_GC_FLAGS32(&new_expr_part->inherited.inherited), 0);
        pmath_atomic_write_release(       &new_expr_part->inherited.metadata, 0);

        new_expr_part->start  = start;
        new_expr_part->buffer = old_expr;
        _pmath_ref_ptr((void *)  old_expr);

        return PMATH_FROM_PTR(new_expr_part);
      }

    case PMATH_TYPE_SHIFT_EXPRESSION_GENERAL_PART: {
        struct _pmath_expr_part_t *old_expr_part = (void *)old_expr;
        struct _pmath_expr_part_t *new_expr_part;

        if(start == 0) {
          if(old_expr_part->start == 0 ||
              !pmath_same(
                old_expr_part->buffer->items[old_expr_part->start - 1],
                old_expr->items[0]))
          {
            struct _pmath_expr_t *new_expr = _pmath_expr_new_noinit(length);
            size_t i;

            if(!new_expr)
              return PMATH_NULL;

            new_expr->items[0] = pmath_ref(old_expr->items[0]);
            new_expr->items[1] = pmath_ref(old_expr->items[0]);

            for(i = old_expr->length; i > 1; --i) {
              new_expr->items[i] = pmath_ref(
                                     old_expr_part->buffer->items[old_expr_part->start + i - 2]);
            }

            return PMATH_FROM_PTR(new_expr);
          }
        }

        new_expr_part = (void *)PMATH_AS_PTR(
                          _pmath_create_stub(
                            PMATH_TYPE_SHIFT_EXPRESSION_GENERAL_PART,
                            sizeof(struct _pmath_expr_part_t)));

        if(!new_expr_part)
          return PMATH_NULL;

        touch_expr(&new_expr_part->inherited);
        new_expr_part->inherited.inherited.gc_refcount = 0;
        new_expr_part->inherited.length                = length;
        pmath_atomic_write_uint32_release(&PMATH_GC_FLAGS32(&new_expr_part->inherited.inherited), 0);
        pmath_atomic_write_release(       &new_expr_part->inherited.metadata, 0);
        new_expr_part->inherited.items[0]              = pmath_ref(old_expr->items[0]);

        new_expr_part->start  = start + old_expr_part->start - 1;
        new_expr_part->buffer = old_expr_part->buffer;
        _pmath_ref_ptr((void *)  old_expr_part->buffer);

        return PMATH_FROM_PTR(new_expr_part);
      }
    
    case PMATH_TYPE_SHIFT_CUSTOM_EXPRESSION:
      return custom_expr_get_item_range((struct _pmath_custom_expr_t*)PMATH_AS_PTR(expr), start, length);
  }

  assert("invalid expression type" && 0);
  return expr;
}

PMATH_API
const pmath_t *pmath_expr_read_item_data(pmath_expr_t expr) {
  struct _pmath_expr_t *ptr;

  if(pmath_is_null(expr))
    return NULL;

  assert(pmath_is_expr(expr));
  ptr = (void *)PMATH_AS_PTR(expr);

  switch(ptr->inherited.inherited.inherited.type_shift) {
    case PMATH_TYPE_SHIFT_EXPRESSION_GENERAL:
      return &ptr->items[1];

    case PMATH_TYPE_SHIFT_EXPRESSION_GENERAL_PART:
      {
        struct _pmath_expr_part_t *part_ptr = (void *)ptr;

        return &part_ptr->buffer->items[part_ptr->start];
      }
      break;
    
    case PMATH_TYPE_SHIFT_CUSTOM_EXPRESSION:
    case PMATH_TYPE_SHIFT_PACKED_ARRAY:
      return NULL;
  }

  assert("invalid expression type" && 0);
  return NULL;
}

PMATH_API pmath_expr_t pmath_expr_set_item(
  pmath_expr_t expr,
  size_t       index,
  pmath_t      item
) {
  struct _pmath_expr_t *old_expr;

  if(pmath_is_packed_array(expr))
    return _pmath_packed_array_set_item(expr, index, item);

  old_expr = (void *)PMATH_AS_PTR(expr);
  if(PMATH_UNLIKELY(!old_expr)) {
    pmath_unref(item);
    return PMATH_NULL;
  }
  
  if(old_expr->inherited.inherited.inherited.type_shift == PMATH_TYPE_SHIFT_CUSTOM_EXPRESSION)
    return custom_expr_set_item((struct _pmath_custom_expr_t*)old_expr, index, item);

  if(PMATH_UNLIKELY(index > old_expr->length)) {
    pmath_unref(item);
    return expr;
  }

  switch(PMATH_AS_PTR(expr)->type_shift) {
    case PMATH_TYPE_SHIFT_EXPRESSION_GENERAL: {
        struct _pmath_expr_t *new_expr;
        size_t i;

        if(pmath_same(old_expr->items[index], item)) {
          pmath_unref(item);
          return expr;
        }

        if(_pmath_refcount_ptr((void *)old_expr) == 1) {
          pmath_unref(old_expr->items[index]);
          old_expr->items[index] = item;
          
          reset_expr_flags(old_expr);
          clear_metadata(old_expr);
          touch_expr(old_expr);
          return expr;
        }

        new_expr = _pmath_expr_new_noinit(old_expr->length);
        if(!new_expr) {
          pmath_unref(item);
          pmath_unref(expr);
          return PMATH_NULL;
        }

        new_expr->items[index] = item;
        if(index > 0) {
          new_expr->items[0] = pmath_ref(old_expr->items[0]);

          for(i = index - 1; i > 0; --i)
            new_expr->items[i] = pmath_ref(old_expr->items[i]);
        }

        for(i = old_expr->length; i > index; --i)
          new_expr->items[i] = pmath_ref(old_expr->items[i]);

        pmath_unref(expr);
        return PMATH_FROM_PTR(new_expr);
      }

    case PMATH_TYPE_SHIFT_EXPRESSION_GENERAL_PART: {
        struct _pmath_expr_part_t *old_expr_part = (void *)old_expr;
        struct _pmath_expr_t      *new_expr;
        size_t i;

        if(index == 0) {
          struct _pmath_expr_part_t *new_expr_part;

          if(pmath_same(old_expr->items[0], item)) {
            pmath_unref(item);
            return expr;
          }

          if(_pmath_refcount_ptr((void *)old_expr) == 1) {
            pmath_unref(old_expr->items[0]);

            old_expr->items[0] = item;
            
            reset_expr_flags(old_expr);
            clear_metadata(old_expr);
            touch_expr(old_expr);
            return expr;
          }

          new_expr_part = (void *)PMATH_AS_PTR(
                            _pmath_create_stub(
                              PMATH_TYPE_SHIFT_EXPRESSION_GENERAL_PART,
                              sizeof(struct _pmath_expr_part_t)));

          if(!new_expr_part) {
            pmath_unref(item);
            pmath_unref(expr);
            return PMATH_NULL;
          }

          touch_expr(&new_expr_part->inherited);
          new_expr_part->inherited.inherited.gc_refcount = 0;
          new_expr_part->inherited.length                = old_expr->length;
          pmath_atomic_write_uint32_release(&PMATH_GC_FLAGS32(&new_expr_part->inherited.inherited), 0);
          pmath_atomic_write_release(       &new_expr_part->inherited.metadata, 0);
          new_expr_part->inherited.items[0]              = item;

          new_expr_part->start  = old_expr_part->start;
          new_expr_part->buffer = old_expr_part->buffer;
          _pmath_ref_ptr((void *)  old_expr_part->buffer);

          pmath_unref(expr);
          return PMATH_FROM_PTR(new_expr_part);
        }

        if(pmath_same(old_expr_part->buffer->items[old_expr_part->start + index - 1], item)) {
          pmath_unref(item);
          return expr;
        }

        new_expr = _pmath_expr_new_noinit(old_expr->length);
        if(!new_expr) {
          pmath_unref(item);
          pmath_unref(expr);
          return PMATH_NULL;
        }

        new_expr->items[0] = pmath_ref(old_expr->items[0]);
        new_expr->items[index] = item;

        for(i = index - 1; i > 0; --i) {
          pmath_t item = old_expr_part->buffer->items[old_expr_part->start + i - 1];

          new_expr->items[i] = pmath_ref(item);
        }

        for(i = old_expr->length; i > index; --i) {
          pmath_t item = old_expr_part->buffer->items[old_expr_part->start + i - 1];

          new_expr->items[i] = pmath_ref(item);
        }

        pmath_unref(expr);
        return PMATH_FROM_PTR(new_expr);
      }

  }

  assert("invalid expression type" && 0);
  return expr;
}

PMATH_PRIVATE pmath_t _pmath_expr_shrink_associative(
  pmath_expr_t  expr,
  pmath_t       magic_rem
) {
  size_t len = pmath_expr_length(expr);
  size_t srci0 = 0;
  size_t dsti  = 1;

  if(pmath_is_null(expr))
    return expr;

  switch(PMATH_AS_PTR(expr)->type_shift) {
    case PMATH_TYPE_SHIFT_PACKED_ARRAY:
      /* magic_rem should be a symbol or magic number and thus cannot be inside a
       * packed array.
       */
      return expr;
    
    case PMATH_TYPE_SHIFT_CUSTOM_EXPRESSION:
      return custom_expr_shrink_associative((struct _pmath_custom_expr_t*)PMATH_AS_PTR(expr), magic_rem);

    case PMATH_TYPE_SHIFT_EXPRESSION_GENERAL:
    case PMATH_TYPE_SHIFT_EXPRESSION_GENERAL_PART: {
        while(srci0 < len) {
          const pmath_t *items;

          items = pmath_expr_read_item_data(expr);
          assert(items != NULL && "general expr always have items array");

          while(srci0 <= len && pmath_same(items[srci0], magic_rem))
            ++srci0;

          if(srci0 >= len)
            break;

          expr = pmath_expr_set_item(expr, dsti++, pmath_ref(items[srci0]));
          ++srci0;
        }

        if(dsti == 2) {
          pmath_t item = pmath_expr_get_item(expr, 1);
          pmath_unref(expr);
          return item;
        }

        return pmath_expr_resize(expr, dsti - 1);
      } break;
  }

  assert("invalid expression type" && 0);

  return expr;
}

static pmath_expr_t remove_all_fast(
  pmath_expr_t expr,
  pmath_t      rem
) {
  size_t len = pmath_expr_length(expr);
  size_t srci0 = 0;
  size_t dsti = 1;

  while(srci0 < len) {
    const pmath_t *items;

    items = pmath_expr_read_item_data(expr);
    assert(items != NULL && "general expr always have items array");

    while(srci0 < len && pmath_equals(items[srci0], rem))
      ++srci0;

    if(srci0 >= len)
      break;

    expr = pmath_expr_set_item(expr, dsti++, pmath_ref(items[srci0]));
    ++srci0;

    if(pmath_is_null(expr))
      return expr;
  }

  return pmath_expr_resize(expr, dsti - 1);
}


static pmath_expr_t remove_all_slow(
  pmath_expr_t expr,
  pmath_t      rem
) {
  size_t len = pmath_expr_length(expr);
  size_t srci = 1;
  size_t dsti = 1;

  while(srci <= len) {
    while(srci <= len && pmath_expr_item_equals(expr, srci, rem)) {
      ++srci;
    }

    if(srci > len) 
      break;

    pmath_t item = pmath_expr_get_item(expr, srci);
    expr = pmath_expr_set_item(expr, dsti++, item);
    ++srci;
  }

  return pmath_expr_resize(expr, dsti - 1);
}

PMATH_API pmath_expr_t pmath_expr_remove_all(
  pmath_expr_t expr,
  pmath_t      rem
) {
  const pmath_t *items;

  if(pmath_is_null(expr))
    return expr;

  items = pmath_expr_read_item_data(expr);
  if(items != NULL)
    return remove_all_fast(expr, rem);

  if(pmath_is_packed_array(expr)) {
    pmath_packed_type_t element_type;

    element_type = pmath_packed_array_get_element_type(expr);

    switch(element_type) {
      case PMATH_PACKED_DOUBLE:
        if(pmath_is_double(rem))
          break;
        if(pmath_same(rem, pmath_System_Undefined))
          break;
        if(pmath_equals(rem, _pmath_object_pos_infinity))
          break;
        if(pmath_equals(rem, _pmath_object_neg_infinity))
          break;
        return expr;

      case PMATH_PACKED_INT32:
        if(pmath_is_int32(rem))
          break;
        return expr;
    }
  }

  return remove_all_slow(expr, rem);
}

/*----------------------------------------------------------------------------*/

static int cmp_objs(const void *key, const void *elem) {
  return pmath_compare(*(pmath_t *)key, *(pmath_t *)elem);
}

/* returns 0 on failure and index of found item otherwise */
PMATH_PRIVATE
size_t _pmath_expr_find_sorted(
  pmath_expr_t sorted_expr, // wont be freed
  pmath_t      item         // wont be freed
) {
  size_t length = pmath_expr_length(sorted_expr);
  if(length == 0)
    return FALSE;

  switch(PMATH_AS_PTR(sorted_expr)->type_shift) {
    case PMATH_TYPE_SHIFT_PACKED_ARRAY:
      return _pmath_packed_array_find_sorted(sorted_expr, item);
      
    case PMATH_TYPE_SHIFT_CUSTOM_EXPRESSION:
      return custom_expr_find_sorted((struct _pmath_custom_expr_t*)PMATH_AS_PTR(sorted_expr), item);

    case PMATH_TYPE_SHIFT_EXPRESSION_GENERAL: {
        struct _pmath_expr_t *ge;
        void *found_elem;

        ge = (void *)PMATH_AS_PTR(sorted_expr);

        found_elem = bsearch(
                       &item,
                       &ge->items[1],
                       length,
                       sizeof(pmath_t),
                       cmp_objs);

        if(found_elem)
          return ((size_t)found_elem - (size_t)ge->items) / sizeof(pmath_t);
        return 0;
      }

    case PMATH_TYPE_SHIFT_EXPRESSION_GENERAL_PART: {
        struct _pmath_expr_part_t *ue;
        void *found_elem;

        ue = (void *)PMATH_AS_PTR(sorted_expr);

        found_elem = bsearch(
                       &item,
                       &ue->buffer->items[ue->start],
                       length,
                       sizeof(pmath_t),
                       cmp_objs);

        if(found_elem)
          return 1 + ((size_t)found_elem - (size_t)&ue->buffer->items[ue->start]) / sizeof(pmath_t);
        return 0;
      }
  }

  assert("invalid expression type" && 0);

  return 0;
}

#if defined(__GLIBC__) && !defined(__GNUC__)

struct cmp_glibc_info_t {
  int (*cmp)(void *, const void *, const void *);
  void *context;
};

static int cmp_glibc(const void *a, const void *b, void *c) {
  struct cmp_glibc_info_t *info = (struct cmp_glibc_info_t *)c;

  return info->cmp(info->context, a, b);
}

#endif

/* The C standard does not allow to cast between data pointer and function pointer
   (They could have different size). So we do a double indirection.
 */
struct context_free_comparer_t {
  int (*function)(const pmath_t *, const pmath_t *);
};

static int compare_no_context(void *_cfc, const pmath_t *a, const pmath_t *b) {
  struct context_free_comparer_t *cfc = _cfc;
  return cfc->function(a, b);
}

PMATH_PRIVATE pmath_expr_t _pmath_expr_sort_ex(
  pmath_expr_t expr, // will be freed
  int(*cmp)(const pmath_t *, const pmath_t *)
) {
  struct context_free_comparer_t cfc;
  cfc.function = cmp;
  return _pmath_expr_sort_ex_context(expr, compare_no_context, &cfc);
}

static pmath_bool_t are_items_sorted(
  const pmath_t  *items,
  size_t          count,
  int           (*cmp)(void *, const pmath_t *, const pmath_t *),
  void           *context
) {
  size_t i;
  for(i = 1; i < count; ++i) {
    int c = (*cmp)(context, &items[i - 1], &items[i]);
    if(c > 0) {
      return FALSE;
    }
  }

  return TRUE;
}

PMATH_PRIVATE pmath_expr_t _pmath_expr_sort_ex_context(
  pmath_expr_t expr, // will be freed
  int(*cmp)(void *, const pmath_t *, const pmath_t *),
  void *context
) {
  size_t i, length;
  struct _pmath_expr_part_t *expr_part_ptr;
  const pmath_t *initial_items;

  if(pmath_is_packed_array(expr)) {
    pmath_debug_print("[unpack array: _pmath_expr_sort_ex_context]\n");

    expr = _pmath_expr_unpack_array(expr, FALSE);
  }
  else if(pmath_is_pointer_of(expr, PMATH_TYPE_CUSTOM_EXPRESSION)) {
    pmath_debug_print("[unpack custrom expr: _pmath_expr_sort_ex_context]\n");
    
    pmath_t old = expr;
    expr = custom_expr_expand_to_normal((struct _pmath_custom_expr_t*)PMATH_AS_PTR(expr));
    pmath_unref(old);
  }

  if(PMATH_UNLIKELY(pmath_is_null(expr)))
    return PMATH_NULL;

  assert(pmath_is_pointer_of(expr, PMATH_TYPE_EXPRESSION_GENERAL | PMATH_TYPE_EXPRESSION_GENERAL_PART));
  expr_part_ptr = (void *)PMATH_AS_PTR(expr);

  length = expr_part_ptr->inherited.length;
  if(length < 2)
    return expr;

  initial_items = pmath_expr_read_item_data(expr);
  assert(initial_items != NULL && "general expr always have items array");

  if(are_items_sorted(initial_items, length, cmp, context))
    return expr;

  if( pmath_refcount(expr) > 1 ||
      PMATH_AS_PTR(expr)->type_shift != PMATH_TYPE_SHIFT_EXPRESSION_GENERAL)
  {
    pmath_t head = pmath_ref(expr_part_ptr->inherited.items[0]);
    struct _pmath_expr_t *new_expr =
      (void *)PMATH_AS_PTR(pmath_expr_new(head, length));

    if(!new_expr) {
      pmath_unref(expr);
      return PMATH_NULL;
    }

    switch(PMATH_AS_PTR(expr)->type_shift) {
      case PMATH_TYPE_SHIFT_EXPRESSION_GENERAL: {
          for(i = 1; i <= length; i++)
            new_expr->items[i] = pmath_ref(expr_part_ptr->inherited.items[i]);
        } break;

      case PMATH_TYPE_SHIFT_EXPRESSION_GENERAL_PART: {
          for(i = 1; i <= length; i++)
            new_expr->items[i] = pmath_ref(
                                   expr_part_ptr->buffer->items[expr_part_ptr->start + i - 1]);
        } break;

      default:
        assert("invalid expression type" && 0);
    }

    pmath_unref(expr);
    expr = PMATH_FROM_PTR(new_expr);
    expr_part_ptr = (void *)new_expr;
  }
  else {
    reset_expr_flags(&expr_part_ptr->inherited);
    clear_metadata(  &expr_part_ptr->inherited);
    touch_expr(      &expr_part_ptr->inherited);
  }

#ifdef __GNUC__

  // MinGW does not know Microsoft's qsort_s even though it is in msvcrt.dll
  // And glibc uses a different parameter order for both qsort_r and its
  // comparision function than BSD (which implemented qsort_r first).
  // Sometimes, GNU sucks.
  {
    int cmp2(const void * a, const void * b) {
      return cmp(context, a, b);
    };

    qsort(
      expr_part_ptr->inherited.items + 1,
      length,
      sizeof(pmath_t),
      cmp2);
  }

#elif defined(PMATH_OS_WIN32)

  qsort_s(
    expr_part_ptr->inherited.items + 1,
    length,
    sizeof(pmath_t),
    (int( *)(void *, const void *, const void *))cmp,
    context);

#elif defined(__GLIBC__)
  // using non-gcc compiler with glibc (e.g. clang)
  {
    struct cmp_glibc_info_t info;
    info.cmp     = cmp;
    info.context = context;

    qsort_r(
      expr_part_ptr->inherited.items + 1,
      length,
      sizeof(pmath_t),
      cmp_glibc,
      &info);
  }

#else

  qsort_r(
    expr_part_ptr->inherited.items + 1,
    length,
    sizeof(pmath_t),
    context,
    (int( *)(void *, const void *, const void *))cmp);

#endif

  return expr;
}

static int stable_sort_cmp_objs(const pmath_t *a, const pmath_t *b) {
  int cmp = pmath_compare(*a, *b);
  if(cmp != 0)
    return cmp;

  if((uintptr_t)a < (uintptr_t)b) return -1;
  if((uintptr_t)a > (uintptr_t)b) return +1;
  return 0;
}

PMATH_API pmath_expr_t pmath_expr_sort(
  pmath_expr_t expr
) {
  if(pmath_is_packed_array(expr))
    return _pmath_packed_array_sort(expr);

  return _pmath_expr_sort_ex(expr, stable_sort_cmp_objs);
}

static pmath_expr_t expr_map_fast(
  struct _pmath_expr_t *old_expr, // will be freed
  size_t                start,
  size_t                end,
  pmath_t             (*func)(pmath_t, size_t, void *),
  void                 *context
) {
  const pmath_t *old_items;
  pmath_t item;

  assert(old_expr != NULL);

  if(start > old_expr->length || end < start)
    return PMATH_FROM_PTR((void *)old_expr);

  if(end > old_expr->length)
    end = old_expr->length;

  switch(old_expr->inherited.inherited.inherited.type_shift) {
    case PMATH_TYPE_SHIFT_EXPRESSION_GENERAL:
      old_items = &old_expr->items[0];
      break;

    case PMATH_TYPE_SHIFT_EXPRESSION_GENERAL_PART:
      {
        const struct _pmath_expr_part_t *old_expr_part = (void *)old_expr;
        old_items = &old_expr_part->buffer->items[old_expr_part->start - 1];
      } break;

    default:
      assert("invalid expression type" && 0);
      return PMATH_NULL;
  }

  assert(old_items != NULL);

  for(; start <= end; ++start) {
    item = (*func)(pmath_ref(old_items[start]), start, context);

    if(pmath_same(item, old_items[start])) {
      pmath_unref(item);
      continue;
    }
    else {
      struct _pmath_expr_t *new_expr;
      size_t i;

      switch(old_expr->inherited.inherited.inherited.type_shift) {
        case PMATH_TYPE_SHIFT_EXPRESSION_GENERAL:
          {
            if(_pmath_refcount_ptr((void *)old_expr) == 1) {
              new_expr = old_expr;

              assert(new_expr->items == old_items);

              pmath_unref(new_expr->items[start]);
              new_expr->items[start] = item;
              for(++start; start <= end; ++start) {
                item = (*func)(new_expr->items[start], start, context);
                new_expr->items[start] = item;
              }

              reset_expr_flags(new_expr);
              clear_metadata(  new_expr);
              touch_expr(new_expr);
              return PMATH_FROM_PTR(new_expr);
            }

            new_expr = _pmath_expr_new_noinit(old_expr->length);
            if(!new_expr) {
              pmath_unref(item);
              _pmath_unref_ptr((void *)old_expr);
              return PMATH_NULL;
            }

            new_expr->items[0] = pmath_ref(old_expr->items[0]);
            for(i = 1; i < start; ++i)
              new_expr->items[i] = pmath_ref(old_items[i]);

            for(i = end + 1; i < new_expr->length; ++i)
              new_expr->items[i] = pmath_ref(old_items[i]);
          }
          break;

        case PMATH_TYPE_SHIFT_EXPRESSION_GENERAL_PART:
          {
            new_expr = _pmath_expr_new_noinit(old_expr->length);
            if(!new_expr) {
              pmath_unref(item);
              _pmath_unref_ptr((void *)old_expr);
              return PMATH_NULL;
            }

            new_expr->items[0] = pmath_ref(old_expr->items[0]);
            for(i = 1; i < start; ++i)
              new_expr->items[i] = pmath_ref(old_items[i]);

            for(i = end + 1; i < new_expr->length; ++i)
              new_expr->items[i] = pmath_ref(old_items[i]);
          }
          break;

        default:
          assert("invalid expression type" && 0);
          return PMATH_NULL;
      }

      new_expr->items[start] = item;
      for(++start; start <= end; ++start) {
        item = (*func)(pmath_ref(old_items[start]), start, context);
        new_expr->items[start] = item;
      }

      _pmath_unref_ptr((void *)old_expr);
      return PMATH_FROM_PTR(new_expr);
    }
  }

  return PMATH_FROM_PTR(old_expr);
}

PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
pmath_expr_t _pmath_expr_map(
  pmath_expr_t  expr, // will be freed
  size_t        start,
  size_t        end,
  pmath_t     (*func)(pmath_t, size_t, void *),
  void         *context
) {
  struct _pmath_expr_t *old_expr;

  if(pmath_is_packed_array(expr))
    return _pmath_packed_array_map(expr, start, end, func, context);
  
  if(pmath_is_pointer_of(expr, PMATH_TYPE_CUSTOM_EXPRESSION)) {
    pmath_debug_print("[unpack custrom expr: _pmath_expr_map]\n");
    
    pmath_t old = expr;
    expr = custom_expr_expand_to_normal((struct _pmath_custom_expr_t*)PMATH_AS_PTR(expr));
    pmath_unref(old);
  }
  
  old_expr = (void *)PMATH_AS_PTR(expr);
  if(!old_expr)
    return expr;

  return expr_map_fast(old_expr, start, end, func, context);
}

PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
pmath_expr_t _pmath_expr_map_slow(
  pmath_expr_t  expr, // will be freed
  size_t        start,
  size_t        end,
  pmath_t     (*func)(pmath_t, size_t, void *),
  void         *context
) {
  size_t i;

  assert(pmath_is_expr(expr));

  i = pmath_expr_length(expr);
  if(start > i || end < start)
    return expr;

  if(end > i)
    end = i;

  for(i = start; i <= end; ++i) {
    pmath_t item = pmath_expr_extract_item(expr, i);

    item = func(item, i, context);

    expr = pmath_expr_set_item(expr, i, item);
  }

  return expr;
}

/*----------------------------------------------------------------------------*/

#define PUSH(e,i) do{stack[stack_pos].expr = e; stack[stack_pos++].index = i;}while(0)
#define POP(e,i)  do{e = stack[--stack_pos].expr; i = stack[stack_pos].index;}while(0)

static pmath_bool_t flatten_calc_newlen(
  size_t       *newlen, // [in/out] final length of flattened expr
  pmath_expr_t  expr,   // wont be freed
  pmath_t       head,   // wont be freed
  size_t        depth
) {
  struct {
    pmath_expr_t expr;
    size_t       index;
  } stack[PMATH_EXPRESSION_FLATTEN_MAX_DEPTH];
  size_t stack_pos = 0;

  pmath_bool_t any_change = FALSE;
  size_t srci = 1;

  if(depth == 0) {
    (*newlen) += pmath_expr_length(expr);
    return FALSE;
  }

  expr = pmath_ref(expr);
  for(;;) {
    size_t exprlen;

  COUNT_NEXT_EXPR: ;

    exprlen = pmath_expr_length(expr);
    if(stack_pos == depth) {
      // TODO: what happens here (and why)?
      (*newlen) += exprlen - srci + 1;
      pmath_unref(expr);
      POP(expr, srci);
      continue;
    }

//    if(pmath_is_packed_array(expr) && pmath_same(head, pmath_System_List)) {
//      size_t        dims  = pmath_packed_array_get_dimensions(expr);
//      const size_t *sizes = pmath_packed_array_get_sizes();
//
//      size_t count = 1;
//      size_t i;
//      for(i = 0;i < dims && stack_pos + i < depth;++i)
//        count *= sizes[i];
//
//      any_change = TRUE;
//      (*newlen) += count - 1;
//
//      pmath_unref(expr);
//      if(stack_pos == 0)
//        return any_change;
//      POP(expr, srci);
//      continue;
//    }

    for(; srci <= exprlen; srci++) {
      pmath_t item = pmath_expr_get_item(expr, srci);

      if(pmath_is_expr(item) && stack_pos < depth) {
        if(pmath_expr_item_equals(item, 0, head)) {
          if(stack_pos < PMATH_EXPRESSION_FLATTEN_MAX_DEPTH) {
            PUSH(expr, srci + 1);
            srci = 1;
            expr = item;
            any_change = TRUE;
            goto COUNT_NEXT_EXPR;
          }

          if(depth > PMATH_EXPRESSION_FLATTEN_MAX_DEPTH) {
            any_change |= flatten_calc_newlen(
                            newlen,
                            item,
                            head,
                            depth - 1 - PMATH_EXPRESSION_FLATTEN_MAX_DEPTH);
            pmath_unref(item);
            continue;
          }
        }
      }
      (*newlen)++;
      pmath_unref(item);
    }

    pmath_unref(expr);
    if(stack_pos == 0)
      return any_change;
    POP(expr, srci);
  }

  //return any_change;
}

static void flatten_rearrange(
  pmath_t      **result,   // [in/out] pointer to an array position
  pmath_expr_t   expr,  // wont be freed
  pmath_t        head,  // wont be freed
  size_t         depth
) {
  struct {
    pmath_expr_t expr;
    size_t             index;
  } stack[PMATH_EXPRESSION_FLATTEN_MAX_DEPTH];
  size_t stack_pos = 0;

  size_t srci = 1;

  if(depth == 0) {
    size_t i;
    size_t exprlen = pmath_expr_length(expr);
    for(i = 1; i <= exprlen; i++, (*result)++)
      **result = pmath_expr_get_item(expr, i);
    return;
  }

  expr = pmath_ref(expr);
  //PUSH(expr, srci);
  for(;;) {
    size_t exprlen;

  FLATTEN_NEXT_EXPR: ;

    exprlen = pmath_expr_length(expr);
    for(; srci <= exprlen; srci++) {
      pmath_t item = pmath_expr_get_item(expr, srci);

      if( pmath_is_expr(item) &&
          stack_pos < depth)
      {
        if(pmath_expr_item_equals(item, 0, head)) {
          if(stack_pos < PMATH_EXPRESSION_FLATTEN_MAX_DEPTH) {
            PUSH(expr, srci + 1);
            srci = 1;
            expr = item;
            goto FLATTEN_NEXT_EXPR;
          }

          if(depth > PMATH_EXPRESSION_FLATTEN_MAX_DEPTH) {
            flatten_rearrange(
              result,
              item,
              head,
              depth - 1 - PMATH_EXPRESSION_FLATTEN_MAX_DEPTH);
            pmath_unref(item);
            continue;
          }
        }
      }
      **result = item;
      ++*result;
    }

    pmath_unref(expr);
    if(stack_pos == 0)
      return;
    POP(expr, srci);
  }
}

#undef PUSH
#undef POP

PMATH_API pmath_expr_t pmath_expr_flatten(
  pmath_expr_t  expr,
  pmath_t       head,
  size_t        depth
) {
  struct _pmath_expr_t *new_expr;
  size_t newlen = 0;
  pmath_t *items;

  if(pmath_is_null(expr)) {
    pmath_unref(head);
    return expr;
  }

  if(pmath_is_packed_array(expr) && pmath_same(head, pmath_System_List)) {
    pmath_unref(head);
    return _pmath_packed_array_flatten(expr, depth);
  }

  if(!flatten_calc_newlen(&newlen, expr, head, depth)) {
    pmath_unref(head);
    return expr;
  }

  new_expr = (void *)PMATH_AS_PTR(pmath_expr_new(
                                    pmath_expr_get_item(expr, 0),
                                    newlen));

  if(!new_expr) {
    pmath_unref(head);
    return expr;
  }

  items = new_expr->items + 1;
  flatten_rearrange(&items, expr, head, depth);

  assert(items == new_expr->items + newlen + 1);

  pmath_unref(head);
  pmath_unref(expr);
  return PMATH_FROM_PTR(new_expr);
}

/*----------------------------------------------------------------------------*/

static pmath_bool_t metadata_find(pmath_t *metadata, _pmath_metadata_kind_t kind) {
  assert(metadata);
  
  if(pmath_is_dispatch_table(*metadata)) 
    return kind == METADATA_KIND_DISPATCH_TABLE;
  else if(pmath_is_expr(*metadata)) {
    pmath_t head = pmath_expr_get_item(*metadata, 0);
    pmath_unref(head);
    
    if(pmath_same(head, PMATH_MAGIC_METADATA_LIST)) {
      size_t i;
      for(i = pmath_expr_length(*metadata); i > 0; --i) {
        pmath_t item = pmath_expr_get_item(*metadata, i);
        if(metadata_find(&item, kind)) {
          pmath_unref(*metadata);
          *metadata = item;
          return TRUE;
        }
        pmath_unref(item);
      }
      return FALSE;
    }
  }
  else if(pmath_is_custom(*metadata))
    return pmath_custom_has_destructor(*metadata, kind);
  
  return kind == METADATA_KIND_debug_metadata;
}

static pmath_bool_t try_merge_metadata(pmath_t *metadata, _pmath_metadata_kind_t kind, pmath_t new_of_kind) {
  assert(metadata);
  
  if(pmath_same(*metadata, PMATH_NULL)) {
    *metadata = pmath_ref(new_of_kind);
    return TRUE;
  }
  
  if(pmath_is_dispatch_table(*metadata)) {
    if(kind == METADATA_KIND_DISPATCH_TABLE) {
      pmath_unref(*metadata);
      *metadata = pmath_ref(new_of_kind);
      return TRUE;
    }
    return FALSE;
  }
  else if(pmath_is_expr(*metadata)) {
    pmath_t head = pmath_expr_get_item(*metadata, 0);
    pmath_unref(head);
    
    if(pmath_same(head, PMATH_MAGIC_METADATA_LIST)) {
      size_t i;
      for(i = pmath_expr_length(*metadata); i > 0; --i) {
        pmath_t item = pmath_expr_get_item(*metadata, i);
        if(try_merge_metadata(&item, kind, new_of_kind)) {
          *metadata = pmath_expr_set_item(*metadata, i, item);
          return TRUE;
        }
        pmath_unref(item);
      }
      return FALSE;
    }
  }
  else if(pmath_is_custom(*metadata)) {
    if(pmath_custom_has_destructor(*metadata, kind)) {
      pmath_unref(*metadata);
      *metadata = pmath_ref(new_of_kind);
      return TRUE;
    }
    return FALSE;
  }
  
  if(kind == METADATA_KIND_debug_metadata) {
    pmath_unref(*metadata);
    *metadata = pmath_ref(new_of_kind);
    return TRUE;
  }
  return FALSE;
}

static pmath_t get_metadata(pmath_expr_t expr, _pmath_metadata_kind_t kind) {
  struct _pmath_expr_t *_expr;
  pmath_t metadata;

  if(pmath_is_null(expr))
    return PMATH_NULL;

  assert(pmath_is_expr(expr));

  if(pmath_is_packed_array(expr))
    return PMATH_NULL;

  _expr = (struct _pmath_expr_t *)PMATH_AS_PTR(expr);

  {
    struct _pmath_t *metadata_ptr = _pmath_atomic_lock_ptr(&_expr->metadata);
    metadata = pmath_ref(PMATH_FROM_PTR(metadata_ptr));
    _pmath_atomic_unlock_ptr(&_expr->metadata, metadata_ptr);
  }
  
  if(metadata_find(&metadata, kind))
    return metadata;

  pmath_unref(metadata);
  return PMATH_NULL;
}

static pmath_t merge_metadata(pmath_expr_t expr, _pmath_metadata_kind_t kind, pmath_t new_of_kind) {
  if(try_merge_metadata(&expr, kind, new_of_kind)) {
    pmath_unref(new_of_kind);
    return expr;
  }
  
  return pmath_expr_new_extended(PMATH_MAGIC_METADATA_LIST, 2, expr, new_of_kind);
}

static void clear_metadata(struct _pmath_expr_t *_expr) {
  struct _pmath_t *metadata_ptr = _pmath_atomic_lock_ptr(&_expr->metadata);
  _pmath_atomic_unlock_ptr(&_expr->metadata, NULL);
  
  if(metadata_ptr)
    _pmath_unref_ptr(metadata_ptr);
}

static void attach_metadata(struct _pmath_expr_t *_expr, _pmath_metadata_kind_t kind, pmath_t new_of_kind) {
  pmath_t metadata;
  
  {
    struct _pmath_t *metadata_ptr = _pmath_atomic_lock_ptr(&_expr->metadata);
    
    if(!metadata_ptr) {
      _pmath_atomic_unlock_ptr(&_expr->metadata, PMATH_AS_PTR(new_of_kind));
      metadata = PMATH_NULL;
      new_of_kind = PMATH_NULL;
    }
    else {
      metadata = pmath_ref(PMATH_FROM_PTR(metadata_ptr));
      _pmath_atomic_unlock_ptr(&_expr->metadata, metadata_ptr);
    }
  }
  
  if(pmath_is_null(new_of_kind)) {
    pmath_unref(metadata);
    return;
  }
  
  metadata = merge_metadata(metadata, kind, new_of_kind);
  if(pmath_is_null(metadata))
    return;
  
  {
    struct _pmath_t *old_metadata_ptr = _pmath_atomic_lock_ptr(&_expr->metadata);
    _pmath_atomic_unlock_ptr(&_expr->metadata, PMATH_AS_PTR(metadata));
    
    if(old_metadata_ptr)
      _pmath_unref_ptr(old_metadata_ptr);
  }
}

PMATH_PRIVATE
pmath_dispatch_table_t _pmath_expr_get_dispatch_table(pmath_expr_t expr) {
  pmath_t result = get_metadata(expr, METADATA_KIND_DISPATCH_TABLE);
  if(!pmath_is_null(result))
    return result;
  
  return PMATH_NULL;
}

PMATH_PRIVATE
void _pmath_expr_attach_dispatch_table(pmath_expr_t expr, pmath_dispatch_table_t dispatch_table) {
  struct _pmath_expr_t *_expr;
  
  assert(pmath_is_dispatch_table(dispatch_table) || pmath_is_null(dispatch_table));
  
  if(!pmath_is_pointer_of(expr, PMATH_TYPE_EXPRESSION_GENERAL | PMATH_TYPE_EXPRESSION_GENERAL_PART)) {
    pmath_unref(dispatch_table);
    return;
  }
  
  _expr = (struct _pmath_expr_t*)PMATH_AS_PTR(expr);
  attach_metadata(_expr, METADATA_KIND_DISPATCH_TABLE, dispatch_table);
}

PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
pmath_custom_t _pmath_expr_get_custom_metadata(pmath_expr_t expr, void *dtor) {
  if((uintptr_t)dtor < 100) // Obviously not a pmath_custom_t destructor (function pointer)
    return PMATH_NULL;
  
  pmath_t result = get_metadata(expr, dtor);
  if(pmath_is_custom(result))
    return result;
  
  pmath_unref(result);
  return PMATH_NULL;
}

PMATH_PRIVATE
void _pmath_expr_attach_custom_metadata(pmath_expr_t expr, void *dtor, pmath_custom_t cert) { // cert will be freed
  struct _pmath_expr_t *_expr;
  
  assert(pmath_is_null(cert) || (pmath_is_custom(cert) && pmath_custom_has_destructor(cert, dtor)));
  
  if(!pmath_is_pointer_of(expr, PMATH_TYPE_EXPRESSION_GENERAL | PMATH_TYPE_EXPRESSION_GENERAL_PART)) {
    pmath_unref(cert);
    return;
  }
  
  _expr = (struct _pmath_expr_t*)PMATH_AS_PTR(expr);
  attach_metadata(_expr, dtor, cert);
}


PMATH_PRIVATE pmath_t _pmath_expr_get_debug_metadata(pmath_expr_t expr) {
  return get_metadata(expr, METADATA_KIND_debug_metadata);
}

PMATH_PRIVATE
pmath_expr_t _pmath_expr_set_debug_metadata(pmath_expr_t expr, pmath_t info) {
RESTART:
  if(!pmath_is_pointer(info))
    return expr;

  if( pmath_is_null(expr) ||
      pmath_is_packed_array(expr))
  {
    pmath_unref(info);
    return expr;
  }

  assert(pmath_is_expr(expr));

  struct _pmath_expr_t *_expr = (struct _pmath_expr_t *)PMATH_AS_PTR(expr);
  if(pmath_refcount(expr) == 1) {
    attach_metadata(_expr, METADATA_KIND_debug_metadata, info);

    return expr;
  }
  else if(pmath_atomic_read_aquire(&_expr->metadata) == (intptr_t)PMATH_AS_PTR(info)) {
    pmath_unref(info);
    return expr;
  }
  
  switch(_expr->inherited.inherited.inherited.type_shift) {
    case PMATH_TYPE_SHIFT_EXPRESSION_GENERAL: {
        struct _pmath_expr_t *new_expr;
        size_t i;

        // TODO: maybe better create an EXPRESSION_GENERAL_PART ?
        new_expr = _pmath_expr_new_noinit(_expr->length);
        if(!new_expr) {
          pmath_unref(info);
          return expr;
        }
        //reset_expr_flags(new_expr); // only debug info changed -> do not reset flags

        struct _pmath_t *old_metadata_ptr = _pmath_atomic_lock_ptr(&_expr->metadata);
        _pmath_incref_impl(old_metadata_ptr);
        _pmath_atomic_unlock_ptr(&_expr->metadata, old_metadata_ptr);

        pmath_atomic_write_release(&new_expr->metadata, (intptr_t)old_metadata_ptr);
        attach_metadata(new_expr, METADATA_KIND_debug_metadata, info);

        for(i = 0; i <= _expr->length; ++i)
          new_expr->items[i] = pmath_ref(_expr->items[i]);

        pmath_unref(expr);
        return PMATH_FROM_PTR(new_expr);
      }

    case PMATH_TYPE_SHIFT_EXPRESSION_GENERAL_PART: {
        struct _pmath_expr_part_t *new_expr_part;
        struct _pmath_expr_part_t *old_expr_part;

        new_expr_part = (void *)PMATH_AS_PTR(
                          _pmath_create_stub(
                            PMATH_TYPE_SHIFT_EXPRESSION_GENERAL_PART,
                            sizeof(struct _pmath_expr_part_t)));

        if(!new_expr_part) {
          pmath_unref(info);
          return expr;
        }

        old_expr_part = (void *)_expr;

        new_expr_part->inherited.inherited.inherited.inherited.flags8  = _expr->inherited.inherited.inherited.flags8;
        new_expr_part->inherited.inherited.inherited.inherited.flags16 = _expr->inherited.inherited.inherited.flags16;
        new_expr_part->inherited.inherited.inherited.last_change       = _expr->inherited.inherited.last_change;

        new_expr_part->inherited.inherited.gc_refcount = 0;
        new_expr_part->inherited.length                = _expr->length;
        //reset_expr_flags(new_expr); // only debug info changed -> do not reset flags

        pmath_atomic_write_release(&new_expr_part->inherited.metadata, (intptr_t)PMATH_AS_PTR(info));

        struct _pmath_t *old_metadata_ptr = _pmath_atomic_lock_ptr(&new_expr_part->inherited.metadata);
        _pmath_incref_impl(old_metadata_ptr);
        _pmath_atomic_unlock_ptr(&new_expr_part->inherited.metadata, old_metadata_ptr);

        pmath_atomic_write_release(&new_expr_part->inherited.metadata, (intptr_t)old_metadata_ptr);
        attach_metadata(&new_expr_part->inherited, METADATA_KIND_debug_metadata, info);

        new_expr_part->inherited.items[0]              = pmath_ref(_expr->items[0]);

        new_expr_part->start  = old_expr_part->start;
        new_expr_part->buffer = old_expr_part->buffer;
        _pmath_ref_ptr((void *) old_expr_part->buffer);

        pmath_unref(expr);

        return PMATH_FROM_PTR(new_expr_part);
      }

    case PMATH_TYPE_SHIFT_CUSTOM_EXPRESSION: {
        struct _pmath_custom_expr_t      *cust = (void*)_expr;
        struct _pmath_custom_expr_data_t *data = PMATH_CUSTOM_EXPR_DATA(cust);
        
        pmath_expr_t copy = PMATH_NULL;
        if(data->api->try_copy_shallow && data->api->try_copy_shallow(cust, &copy)) {
          assert(!pmath_is_pointer_of(copy, PMATH_TYPE_CUSTOM_EXPRESSION));
          pmath_unref(expr);
          expr = copy;
          goto RESTART;
        }
        
        pmath_debug_print("[expensive try_set_debug_metadata(...) for custom expr with api=%p]\n", data->api);
        copy = custom_expr_expand_to_normal(cust);
        pmath_unref(expr);
        expr = copy;
        goto RESTART;
      } break;
  }

  pmath_unref(info);
  return expr;
}

/*----------------------------------------------------------------------------*/

static pmath_t custom_expr_expand_to_normal(struct _pmath_custom_expr_t *_expr) {
  struct _pmath_custom_expr_data_t *data = PMATH_CUSTOM_EXPR_DATA(_expr);
  
  size_t len = data->api->get_length(_expr);
  
  struct _pmath_expr_t *normal = _pmath_expr_new_noinit(len);
  if(PMATH_UNLIKELY(!normal))
    return PMATH_NULL;
  
  for(size_t i = 0; i <= len; ++i)
    normal->items[i] = data->api->get_item(_expr, i);
  
  return PMATH_FROM_PTR(&normal->inherited.inherited.inherited);
}

static size_t custom_expr_length(struct _pmath_custom_expr_t *_expr) {
  struct _pmath_custom_expr_data_t *data = PMATH_CUSTOM_EXPR_DATA(_expr);
  return data->api->get_length(_expr);
}

static pmath_t custom_expr_get_item(struct _pmath_custom_expr_t *_expr, size_t index) {
  struct _pmath_custom_expr_data_t *data = PMATH_CUSTOM_EXPR_DATA(_expr);
  size_t len = data->api->get_length(_expr);
  
  if(PMATH_UNLIKELY(index > len))
    return PMATH_NULL;
  
  return data->api->get_item(_expr, index);
}

static pmath_bool_t custom_expr_item_equals(struct _pmath_custom_expr_t *_expr, size_t index, pmath_t expected_item) {
  struct _pmath_custom_expr_data_t *data = PMATH_CUSTOM_EXPR_DATA(_expr);

  pmath_bool_t result;
  if(data->api->try_item_equals && data->api->try_item_equals(_expr, index, expected_item, &result))
    return result;
  
  pmath_t item = data->api->get_item(_expr, index);
  pmath_bool_t eq = pmath_equals(item, expected_item);
  pmath_unref(item);
  return eq;
}

static pmath_expr_t custom_expr_get_item_range(struct _pmath_custom_expr_t *_expr, size_t start, size_t length) {
  struct _pmath_custom_expr_data_t *data = PMATH_CUSTOM_EXPR_DATA(_expr);

  pmath_expr_t result = PMATH_NULL;
  if(data->api->try_get_item_range && data->api->try_get_item_range(_expr, start, length, &result))
    return result;
  
  struct _pmath_expr_t *new_expr = _pmath_expr_new_noinit(length);
  if(PMATH_UNLIKELY(!new_expr))
    return PMATH_NULL;
  
  new_expr->items[0] = data->api->get_item(_expr, 0);
  for(size_t i = 1; i <= length; ++i)
    new_expr->items[i] = data->api->get_item(_expr, start + i - 1);
  
  return PMATH_FROM_PTR(&new_expr->inherited.inherited.inherited);
}

static pmath_expr_t custom_expr_set_item(struct _pmath_custom_expr_t *_expr, size_t index, pmath_t new_item) {
  struct _pmath_custom_expr_data_t *data = PMATH_CUSTOM_EXPR_DATA(_expr);
  size_t len = data->api->get_length(_expr);
  
  if(PMATH_UNLIKELY(index > len)) {
    pmath_unref(new_item);
    return PMATH_FROM_PTR(&_expr->internals.inherited.inherited.inherited);
  }
  
  if(pmath_atomic_read_aquire(&_expr->internals.inherited.inherited.inherited.refcount) == 1) {
    if(data->api->try_set_item_mutable && data->api->try_set_item_mutable(_expr, index, new_item)) {
      return PMATH_FROM_PTR(&_expr->internals.inherited.inherited.inherited);
    }
  }
  
  pmath_expr_t result = PMATH_NULL;
  if(data->api->try_set_item_copy && data->api->try_set_item_copy(_expr, index, new_item, &result)) {
    assert(pmath_is_expr(result) || pmath_is_null(result));
    pmath_unref(PMATH_FROM_PTR(&_expr->internals.inherited.inherited.inherited));
    return result;
  }
  
#ifdef PMATH_DEBUG_LOG
  if(data->api->try_set_item_copy) {
    pmath_debug_print("[try_set_item_copy failed at [%d]:= ", index);
    pmath_debug_print_object("", new_item, " ");
    pmath_debug_print_object("in ", PMATH_FROM_PTR(_expr), "]\n");
  }
  else if(data->api->try_set_item_mutable && pmath_atomic_read_aquire(&_expr->internals.inherited.inherited.inherited.refcount) == 1) {
    pmath_debug_print("[try_set_item_mutable failed at [%d]:= ", index);
    pmath_debug_print_object("", new_item, " ");
    pmath_debug_print_object("in ", PMATH_FROM_PTR(_expr), "]\n");
  }
  else {
    pmath_debug_print("[no setter for custom expr at [%d]:= ", index);
    pmath_debug_print_object("", new_item, " ");
    pmath_debug_print_object("in ", PMATH_FROM_PTR(_expr), "]\n");
  }
#endif // PMATH_DEBUG_LOG
  
  assert(pmath_is_null(result));
  result = custom_expr_expand_to_normal(_expr);
  pmath_unref(PMATH_FROM_PTR(&_expr->internals.inherited.inherited.inherited));
  return pmath_expr_set_item(result, index, new_item);
}

static pmath_expr_t custom_expr_shrink_associative(struct _pmath_custom_expr_t *_expr, pmath_t magic_rem) {
  struct _pmath_custom_expr_data_t *data = PMATH_CUSTOM_EXPR_DATA(_expr);
  size_t len = data->api->get_length(_expr);
  
  if(data->api->try_shrink_associative) {
    pmath_t result = PMATH_NULL;
    if(data->api->try_shrink_associative(_expr, magic_rem, &result))
      return result;
    
    assert(pmath_is_null(result));
  }
  
  for(size_t i = len; i > 0; --i) {
    pmath_t item = data->api->get_item(_expr, i);
    if(pmath_same(item, magic_rem)) {
      pmath_unref(item);
      pmath_t normal = custom_expr_expand_to_normal(_expr);
      pmath_unref(PMATH_FROM_PTR(&_expr->internals.inherited.inherited.inherited));
      return _pmath_expr_shrink_associative(normal, magic_rem);
    }
  }
  
  return PMATH_FROM_PTR(&_expr->internals.inherited.inherited.inherited);
}

// returns 0 on failure and index of found item otherwise
static size_t custom_expr_find_sorted(struct _pmath_custom_expr_t *_expr, pmath_t item) {
  struct _pmath_custom_expr_data_t *data = PMATH_CUSTOM_EXPR_DATA(_expr);
  
  pmath_bool_t maybe_contained = TRUE;
  if(data->api->try_maybe_contains_item && data->api->try_maybe_contains_item(_expr, item, &maybe_contained)) {
    if(!maybe_contained)
      return 0;
  }
  
  // It is late in the evening and I am too lazy to implement a binary search.
  pmath_debug_print("[custom_expr_find_sorted: using linear search]\n");
  
  size_t len = data->api->get_length(_expr);
  for(size_t i = 1; i <= len; ++i) {
    pmath_t sub = data->api->get_item(_expr, i);
    
    int cmp = pmath_compare(sub, item);
    pmath_unref(sub);
    if(cmp == 0)
      return i;
  }
  
  return 0;
}

/*----------------------------------------------------------------------------*/

struct thread_one_arg_t {
  size_t       arg_index;
  pmath_t      expr;
  pmath_bool_t can_eval;
};

static pmath_t thread_one_arg_callback(pmath_t obj, size_t i, void *data) {
  struct thread_one_arg_t *context = data;

  obj = pmath_expr_set_item(pmath_ref(context->expr), context->arg_index, obj);
  if(context->can_eval)
    obj = pmath_evaluate(obj);

  return obj;
}

PMATH_PRIVATE pmath_expr_t _pmath_expr_thread(
  pmath_expr_t  expr, // will be freed
  pmath_t       head, // wont be freed
  size_t        start,
  size_t        end,
  pmath_bool_t *error_message
) {
  pmath_bool_t have_sth_to_thread = FALSE;
  pmath_bool_t show_message       = error_message && *error_message;
  pmath_bool_t can_eval;
  size_t i, len, exprlen;
  size_t first_thread_over_arg, last_thread_over_arg;
  struct _pmath_expr_t *result;


  exprlen = pmath_expr_length(expr);

  if(end > exprlen)
    end = exprlen;

  len = 0;

  if(show_message)
    *error_message = FALSE;

  first_thread_over_arg = 0;
  last_thread_over_arg  = 0;

  for(i = start; i <= end; ++i) {
    pmath_t arg = pmath_expr_get_item(expr, i);

    if(pmath_is_expr(arg)) {
      if(pmath_expr_item_equals(arg, 0, head)) {
        if(have_sth_to_thread) {
          if(len != pmath_expr_length(arg)) {
            pmath_unref(arg);
            if(show_message) {
              *error_message = TRUE;
              pmath_message(pmath_System_Thread, "len", 1, pmath_ref(expr));
            }
            return expr;
          }

          last_thread_over_arg = i;
        }
        else {
          len = pmath_expr_length(arg);
          have_sth_to_thread = TRUE;
          first_thread_over_arg = i;
          last_thread_over_arg  = i;
        }
      }
    }

    pmath_unref(arg);
  }

  if(!have_sth_to_thread)
    return expr;

  can_eval = FALSE;
  if(pmath_is_symbol(head)) {
    pmath_symbol_attributes_t att = pmath_symbol_get_attributes(head);

    can_eval = !(att & (PMATH_SYMBOL_ATTRIBUTE_HOLDALL | PMATH_SYMBOL_ATTRIBUTE_HOLDALLCOMPLETE));
  }

  if(first_thread_over_arg == last_thread_over_arg) {
    struct thread_one_arg_t context;
    pmath_t list;

    context.arg_index = first_thread_over_arg;
    context.expr      = expr;
    context.can_eval  = can_eval;

    list = pmath_expr_extract_item(expr, context.arg_index);
    list = _pmath_expr_map(list, 1, SIZE_MAX, thread_one_arg_callback, &context);

    if(can_eval) {
      if(pmath_same(head, pmath_System_List))
        _pmath_expr_update(list);
    }

    pmath_unref(expr);
    return list;
  }

  result = _pmath_expr_new_noinit(len);
  if(!result)
    return expr;

  result->items[0] = pmath_ref(head);

  for(i = 1; i <= len; ++i) {
    pmath_expr_t f = pmath_ref(expr);

    size_t j;
    for(j = start; j <= end; ++j) {
      pmath_t arg = pmath_expr_get_item(expr, j);

      if(pmath_is_expr(arg)) {
        if(pmath_expr_item_equals(arg, 0, head)) {
          f = pmath_expr_set_item(
                f, j,
                pmath_expr_get_item(arg, i));
          pmath_unref(arg);
        }
        else
          f = pmath_expr_set_item(f, j, arg);
      }
      else
        f = pmath_expr_set_item(f, j, arg);
    }

    if(can_eval)
      f = pmath_evaluate(f);

    result->items[i] = f;
  }

  pmath_unref(expr);
  expr = PMATH_FROM_PTR((void *)result);

  if(can_eval) {
    if(pmath_same(head, pmath_System_List))
      _pmath_expr_update(expr);
  }

  return expr;
}

/*----------------------------------------------------------------------------*/

static pmath_bool_t has_changed(
  pmath_t        obj,            // will be freed
  _pmath_timer_t current_time
) {
  if(pmath_is_symbol(obj)) {
    pmath_bool_t result = (((struct _pmath_timed_t *)PMATH_AS_PTR(obj))->last_change > current_time);
    pmath_unref(obj);
    return result;
  }

  if(pmath_is_expr(obj)) {
    size_t i, len;

    if(((struct _pmath_timed_t *)PMATH_AS_PTR(obj))->last_change > current_time) {
      pmath_unref(obj);
      return TRUE;
    }

    len = pmath_expr_length(obj);
    for(i = 0; i <= len; ++i) {
      pmath_t item = pmath_expr_get_item(obj, i);

      if(has_changed(item, current_time)) {
        pmath_unref(obj);
        return TRUE;
      }
    }
  }

  pmath_unref(obj);
  return FALSE;
}

PMATH_PRIVATE pmath_bool_t _pmath_expr_is_updated(pmath_expr_t expr) {
  size_t i, len;
  _pmath_timer_t current_time;

  if(PMATH_UNLIKELY(pmath_is_null(expr)))
    return TRUE;

  assert(pmath_is_expr(expr));

  current_time = ((struct _pmath_timed_t *)PMATH_AS_PTR(expr))->last_change;
  if(current_time < 0)
    return FALSE;

  if(current_time >= _pmath_timer_get())
    return TRUE;

  len = pmath_expr_length(expr);
  for(i = 0; i <= len; i++) {
    pmath_t item = pmath_expr_get_item(expr, i);

    if(has_changed(item, current_time))
      return FALSE;
  }

  return TRUE;
}

PMATH_PRIVATE
void _pmath_expr_update(pmath_expr_t expr) {
  struct _pmath_timed_t *tt;

  if(PMATH_UNLIKELY(pmath_is_null(expr)))
    return;

  tt = (struct _pmath_timed_t*)PMATH_AS_PTR(expr);

  switch(tt->inherited.type_shift) {
    case PMATH_TYPE_SHIFT_EXPRESSION_GENERAL:
    case PMATH_TYPE_SHIFT_EXPRESSION_GENERAL_PART:
    case PMATH_TYPE_SHIFT_CUSTOM_EXPRESSION:
      tt->last_change = _pmath_timer_get();
      break;
  }
}

PMATH_PRIVATE
_pmath_timer_t _pmath_expr_last_change(pmath_expr_t expr) {
  struct _pmath_timed_t *tt;

  if(PMATH_UNLIKELY(pmath_is_null(expr)))
    return 0;

  tt = (struct _pmath_timed_t*)PMATH_AS_PTR(expr);

  switch(tt->inherited.type_shift) {
    case PMATH_TYPE_SHIFT_EXPRESSION_GENERAL:
    case PMATH_TYPE_SHIFT_EXPRESSION_GENERAL_PART:
    case PMATH_TYPE_SHIFT_CUSTOM_EXPRESSION:
      return tt->last_change;
  }

  return 0;
}

/*============================================================================*/
/* pMath object functions ... */

static int compare_expr_general(pmath_expr_t a, pmath_expr_t b) {
  size_t i;
  size_t a_len;
  size_t b_len;
  const pmath_t *a_items;
  const pmath_t *b_items;
  pmath_t a_head = pmath_expr_get_item(a, 0);
  pmath_t b_head = pmath_expr_get_item(b, 0);
  int cmp;

  if(pmath_same(a_head, pmath_System_Complex)) {
    if(!pmath_same(b_head, pmath_System_Complex)) {
      pmath_unref(a_head);
      pmath_unref(b_head);
      return -1;
    }
  }
  else if(pmath_same(b_head, pmath_System_Complex)) {
    pmath_unref(a_head);
    pmath_unref(b_head);
    return +1;
  }
  
  a_len = pmath_expr_length(a);
  b_len = pmath_expr_length(b);
  if(a_len < b_len) {
    pmath_unref(a_head);
    pmath_unref(b_head);
    return -1;
  }
  if(a_len > b_len) {
    pmath_unref(a_head);
    pmath_unref(b_head);
    return +1;
  }
  
  cmp = pmath_compare(a_head, b_head);
  pmath_unref(a_head);
  pmath_unref(b_head);
  if(cmp != 0) 
    return cmp;
  
  a_items = pmath_expr_read_item_data(a);
  b_items = pmath_expr_read_item_data(b);

  if(a_items != NULL && b_items != NULL) {
    // compare arguments
    for(i = 0; i < a_len; ++i) {
      cmp = pmath_compare(a_items[i], b_items[i]);

      if(cmp != 0)
        return cmp;
    }
  }
  else {
    for(i = 0; i <= a_len; ++i) {
      pmath_t itema = pmath_expr_get_item(a, i);
      pmath_t itemb = pmath_expr_get_item(b, i);

      int cmp = pmath_compare(itema, itemb);

      pmath_unref(itema);
      pmath_unref(itemb);
      if(cmp != 0)
        return cmp;
    }
  }

  return 0;
}

// Complex(...) < symbols < other expr
PMATH_PRIVATE
int _pmath_compare_exprsym(pmath_t a, pmath_t b) {
  switch(PMATH_AS_PTR(a)->type_shift) {

    case PMATH_TYPE_SHIFT_PACKED_ARRAY: {
        switch(PMATH_AS_PTR(b)->type_shift) {
          case PMATH_TYPE_SHIFT_PACKED_ARRAY: {
              int cmp = _pmath_packed_array_compare(a, b);

              if(cmp == PMATH_ARRAYS_INCOMPATIBLE_CMP)
                return compare_expr_general(a, b);

              return cmp;
            } break;

          case PMATH_TYPE_SHIFT_EXPRESSION_GENERAL:
          case PMATH_TYPE_SHIFT_EXPRESSION_GENERAL_PART:
          case PMATH_TYPE_SHIFT_CUSTOM_EXPRESSION:
            return compare_expr_general(a, b);

          case PMATH_TYPE_SHIFT_SYMBOL:
            return +1;
        }
        assert("neither an expression nor a symbol" && 0);
      } break;

    case PMATH_TYPE_SHIFT_EXPRESSION_GENERAL:
    case PMATH_TYPE_SHIFT_EXPRESSION_GENERAL_PART: {
        switch(PMATH_AS_PTR(b)->type_shift) {
          case PMATH_TYPE_SHIFT_PACKED_ARRAY:
          case PMATH_TYPE_SHIFT_EXPRESSION_GENERAL:
          case PMATH_TYPE_SHIFT_EXPRESSION_GENERAL_PART:
          case PMATH_TYPE_SHIFT_CUSTOM_EXPRESSION:
            return compare_expr_general(a, b);

          case PMATH_TYPE_SHIFT_SYMBOL: {
              struct _pmath_expr_t *ua = (void *)PMATH_AS_PTR(a);

              if(pmath_same(ua->items[0], pmath_System_Complex))
                return -1;

              return +1;
            } break;
        }
        assert("neither an expression nor a symbol" && 0);
      } break;
    
    case PMATH_TYPE_SHIFT_CUSTOM_EXPRESSION: {
        switch(PMATH_AS_PTR(b)->type_shift) {
          case PMATH_TYPE_SHIFT_PACKED_ARRAY:
          case PMATH_TYPE_SHIFT_EXPRESSION_GENERAL:
          case PMATH_TYPE_SHIFT_EXPRESSION_GENERAL_PART:
          case PMATH_TYPE_SHIFT_CUSTOM_EXPRESSION:
            return compare_expr_general(a, b);
          
          case PMATH_TYPE_SHIFT_SYMBOL: {
              pmath_t head_a = custom_expr_get_item((struct _pmath_custom_expr_t*)PMATH_AS_PTR(a), 0);
              pmath_unref(head_a);
              
              if(pmath_same(head_a, pmath_System_Complex))
                return -1;

              return +1;
          } break;
        }
        assert("neither an expression nor a symbol" && 0);
      } break;

    case PMATH_TYPE_SHIFT_SYMBOL: {
        switch(PMATH_AS_PTR(b)->type_shift) {
          case PMATH_TYPE_SHIFT_PACKED_ARRAY:
            return -1;

          case PMATH_TYPE_SHIFT_EXPRESSION_GENERAL:
          case PMATH_TYPE_SHIFT_EXPRESSION_GENERAL_PART:
          case PMATH_TYPE_SHIFT_CUSTOM_EXPRESSION:
            return -_pmath_compare_exprsym(b, a);

          case PMATH_TYPE_SHIFT_SYMBOL: {
              pmath_string_t namea = pmath_symbol_name(a);
              pmath_string_t nameb = pmath_symbol_name(b);
              int cmp = pmath_compare(namea, nameb);
              pmath_unref(namea);
              pmath_unref(nameb);
              if(cmp == 0) {
                if(PMATH_AS_PTR(a) < PMATH_AS_PTR(b))
                  cmp = -1;
                else if(PMATH_AS_PTR(a) > PMATH_AS_PTR(b))
                  cmp = 1;
              }
              return cmp;
            } break;
        }
        assert("neither an expression nor a symbol" && 0);
      } break;
  }

  assert("neither an expression nor a symbol" && 0);
  return 0;
}

PMATH_PRIVATE pmath_bool_t _pmath_expr_equal(
  pmath_expr_t exprA,
  pmath_expr_t exprB
) {
  size_t i, lenA;

  if(pmath_is_packed_array(exprA) && pmath_is_packed_array(exprB))
    return _pmath_packed_array_equal(exprA, exprB);
  
  if(pmath_is_pointer_of(exprA, PMATH_TYPE_CUSTOM_EXPRESSION)) {
    struct _pmath_custom_expr_t      *custA = (void*)PMATH_AS_PTR(exprA);
    struct _pmath_custom_expr_data_t *dataA = PMATH_CUSTOM_EXPR_DATA(custA);
    
    if(pmath_is_pointer_of(exprB, PMATH_TYPE_EXPRESSION_WITH_GC_FLAGS32)) {
      struct _pmath_gc_t *gcB = (void*)PMATH_AS_PTR(exprB);
      
      uint32_t cached_hash_A = pmath_atomic_read_uint32_aquire(&PMATH_GC_FLAGS32(&custA->internals.inherited));
      uint32_t cached_hash_B = pmath_atomic_read_uint32_aquire(&PMATH_GC_FLAGS32(gcB));
      
      if(cached_hash_A && cached_hash_B && cached_hash_A != cached_hash_B)
        return FALSE;
    }
    
    pmath_bool_t eq = FALSE;
    if(dataA->api->try_compare_equal && dataA->api->try_compare_equal(custA, exprB, &eq))
      return eq;
  }
  
  if(pmath_is_pointer_of(exprB, PMATH_TYPE_CUSTOM_EXPRESSION)) {
    struct _pmath_custom_expr_t      *custB = (void*)PMATH_AS_PTR(exprB);
    struct _pmath_custom_expr_data_t *dataB = PMATH_CUSTOM_EXPR_DATA(custB);
    
    if(pmath_is_pointer_of(exprA, PMATH_TYPE_EXPRESSION_WITH_GC_FLAGS32)) {
      struct _pmath_gc_t *gcA = (void*)PMATH_AS_PTR(exprA);
      
      uint32_t cached_hash_A = pmath_atomic_read_uint32_aquire(&PMATH_GC_FLAGS32(gcA));
      uint32_t cached_hash_B = pmath_atomic_read_uint32_aquire(&PMATH_GC_FLAGS32(&custB->internals.inherited));
      
      if(cached_hash_A && cached_hash_B && cached_hash_A != cached_hash_B)
        return FALSE;
    }
    
    pmath_bool_t eq = FALSE;
    if(dataB->api->try_compare_equal && dataB->api->try_compare_equal(custB, exprA, &eq))
      return eq;
  }
  
  if(PMATH_LIKELY(
      pmath_is_pointer_of(exprA, PMATH_TYPE_EXPRESSION_WITH_GC_FLAGS32) && 
      pmath_is_pointer_of(exprB, PMATH_TYPE_EXPRESSION_WITH_GC_FLAGS32))
  ) {
    struct _pmath_gc_t *gcA = (void*)PMATH_AS_PTR(exprA);
    struct _pmath_gc_t *gcB = (void*)PMATH_AS_PTR(exprB);

    
    uint32_t cached_hash_A = pmath_atomic_read_uint32_aquire(&PMATH_GC_FLAGS32(gcA));
    uint32_t cached_hash_B = pmath_atomic_read_uint32_aquire(&PMATH_GC_FLAGS32(gcB));
    
    if(cached_hash_A && cached_hash_B && cached_hash_A != cached_hash_B)
      return FALSE;
  }
  
  lenA = pmath_expr_length(exprA);

  if(lenA != pmath_expr_length(exprB))
    return FALSE;

  for(i = 0; i <= lenA; i++) {
    pmath_t itemB = pmath_expr_get_item(exprB, i);
    pmath_bool_t eq = pmath_expr_item_equals(exprA, i, itemB);
    pmath_unref(itemB);
    if(!eq)
      return FALSE;
  }
  return TRUE;
}

static unsigned int hash_expression_fallback(pmath_expr_t expr) {
  unsigned int next = 0;
  size_t i;
  size_t len = pmath_expr_length(expr);

  for(i = 0; i <= len; i++) {
    pmath_t item = pmath_expr_get_item(expr, i);
    unsigned int h = pmath_hash(item);
    pmath_unref(item);
    next = _pmath_incremental_hash(&h, sizeof(h), next);
  }
  return next;
}

static unsigned int hash_expression_cached(pmath_expr_t expr) {
  assert(pmath_is_pointer_of(expr, PMATH_TYPE_EXPRESSION_WITH_GC_FLAGS32));
  
  struct _pmath_gc_t *gc_ptr = (void*)PMATH_AS_PTR(expr);

  uint32_t cached_hash = pmath_atomic_read_uint32_aquire(&PMATH_GC_FLAGS32(gc_ptr));
#ifdef NDEBUG
  if(cached_hash != 0)
    return cached_hash;
#endif

  uint32_t calculated_hash = hash_expression_fallback(expr);
  
  assert(calculated_hash == cached_hash || cached_hash == 0);
  pmath_atomic_write_uint32_release(&PMATH_GC_FLAGS32(gc_ptr), calculated_hash);

  return calculated_hash;
}

//{ writing expressions

#define PREC_NOT      (PMATH_PREC_AND + 1)
#define PREC_FACTOR   (PMATH_PREC_MUL + 1)

#define WRITE_CSTR(str) _pmath_write_cstr((str), info->write, info->user)

static void write_spaced_operator2(
  struct pmath_write_ex_t *info,
  pmath_bool_t             allow_left_space,
  pmath_bool_t             allow_right_space,
  uint16_t                 unicode,
  const char              *ascii
) {
  if(unicode && (info->options & PMATH_WRITE_OPTIONS_PREFERUNICODE)) {
    if(info->options & PMATH_WRITE_OPTIONS_NOSPACES) {
      info->write(info->user, &unicode, 1);
    }
    else {
      uint16_t uni3[3] = {' ',  unicode, ' '};
      info->write(
        info->user,
        allow_left_space ? uni3 : uni3 + 1,
        (allow_left_space ? 1 : 0) + 1 + (allow_right_space ? 1 : 0));
    }
  }
  else {
    if(info->options & PMATH_WRITE_OPTIONS_NOSPACES) {
      _pmath_write_cstr(ascii, info->write, info->user);
    }
    else {
      uint16_t space = ' ';
      if(allow_left_space)
        info->write(info->user, &space, 1);

      _pmath_write_cstr(ascii, info->write, info->user);

      if(allow_right_space)
        info->write(info->user, &space, 1);
    }
  }
}

static void write_spaced_operator(struct pmath_write_ex_t *info, uint16_t unicode, const char *ascii) {
  write_spaced_operator2(info, TRUE, TRUE, unicode, ascii);
}

static void write_expr_ex(
  struct pmath_write_ex_t *info,
  int                      priority,
  pmath_expr_t             expr);

static void write_ex(
  struct pmath_write_ex_t *info,
  int                      priority,
  pmath_t                  obj
) {
  if(pmath_is_expr(obj)) {
    if(info->pre_write)
      info->pre_write(info->user, obj, info->options);
    
    if(!info->custom_writer || !info->custom_writer(info->user, obj, info)) {
      if(!_pmath_write_user_format(info, obj))
        write_expr_ex(info, priority, obj);
    }
    
    if(info->post_write)
      info->post_write(info->user, obj, info->options);
    return;
  }
  
  if( pmath_is_number(obj) &&
       ((priority > PMATH_PREC_MUL  && pmath_number_sign(obj) < 0) ||
        (priority > PREC_FACTOR && pmath_is_quotient(obj))))
  {
    WRITE_CSTR("(");

    _pmath_write_impl(info, obj);

    WRITE_CSTR(")");
  }
  else
    _pmath_write_impl(info, obj);
}

struct _writer_hook_data_t {
  struct pmath_write_ex_t *next;
  int                      prefix_status;
  pmath_write_options_t    options;
  pmath_bool_t             special_end; // for product_writer
};

static void hook_pre_write(void *user, pmath_t obj, pmath_write_options_t options) {
  struct _writer_hook_data_t *hook = user;

  assert(hook->next->pre_write);
  hook->next->pre_write(hook->next->user, obj, options);
}

static void hook_post_write(void *user, pmath_t obj, pmath_write_options_t options) {
  struct _writer_hook_data_t *hook = user;

  assert(hook->next->post_write);
  hook->next->post_write(hook->next->user, obj, options);
}

static pmath_bool_t hook_custom_writer(void *user, pmath_t obj, struct pmath_write_ex_t *info) {
  struct _writer_hook_data_t *hook = user;

  assert(hook->next->custom_writer);
  return hook->next->custom_writer(hook->next->user, obj, info);
}

static void init_hook_info(struct pmath_write_ex_t *info, struct _writer_hook_data_t *user) {
  memset(info, 0, sizeof(struct pmath_write_ex_t));
  info->size = sizeof(struct pmath_write_ex_t);

  /* We are guaranteed that user->size == sizeof(struct pmath_write_ex_t), because pmath_write_ex() ensures this. */
  info->options       = user->next->options;
  info->user          = user;
  info->pre_write     = user->next->pre_write     ? hook_pre_write    : NULL;
  info->post_write    = user->next->post_write    ? hook_post_write   : NULL;
  info->custom_writer = user->next->custom_writer ? hook_custom_writer: NULL;
}

/* Hook in the given writer function and insert a space before the first
   character if it is '?'.

   needed to distinguish
     a/?b    (= Condition(a, b)) and
     a/ ?b   (= Times(a, Power(Optional(b), -1)))
 */
static void division_writer(
  void           *user,
  const uint16_t *data,
  int len
) {
  struct _writer_hook_data_t *hook = user;
  if(len == 0)
    return;

  if(hook->prefix_status == 0) {
    hook->prefix_status = 1;

    if(data[0] == '?')
      _pmath_write_cstr(" ", hook->next->write, hook->next->user);
  }

  hook->next->write(hook->next->user, data, len);
}

/* Hook in the given writer function and place stars as multilication signs when
   needed (before (,[,{ ) or a space otherwise.
   => Times(a,b) becomes "a b",
      Times(a,Plus(b,c)) becomes "a*(b+c)".
 */
static void product_writer(
  void           *user,
  const uint16_t *data,
  int             len
) {
  struct _writer_hook_data_t *hook = user;
  int i = 0;

  if(len == 0)
    return;

  while(i < len && data[i] <= ' ')
    i++;

  if(hook->prefix_status == 1) {
    if(i < len && data[i] == '/')
      _pmath_write_cstr("1", hook->next->write, hook->next->user);
    hook->prefix_status = 2;
  }
  else if(hook->prefix_status == 0) {
    hook->prefix_status = 2;//i < len;
    if( i < len &&
        (data[i] == '(' ||
         data[i] == '[' ||
         data[i] == '{' ||
         data[i] == '.'))
    {
      if(hook->options & PMATH_WRITE_OPTIONS_PREFERUNICODE) {
        uint16_t uni = 0x00D7;
        hook->next->write(hook->next->user, &uni, 1);
      }
      else
        _pmath_write_cstr("*", hook->next->write, hook->next->user);
    }
    else if(len >= 2 && data[0] == '1' && data[1] == '/') {
      --len;
      ++data;
    }
    else if(i < len && data[i] != '/')
      _pmath_write_cstr(" ", hook->next->write, hook->next->user);
  }
  //hook->prefix_status = hook->prefix_status && i >= len;
  i = len;
  while(i > 0 && data[i - 1] <= ' ')
    i--;
  hook->special_end = i > 0 && data[i - 1] == '~';

  hook->next->write(hook->next->user, data, len);
}

/* Hook in the given writer function and place plus or minus signs
   appropriately.
   => Plus(a,Times(-2,b)) becomes "a - 2 b" instead of "a + -2 b".
 */
static void sum_writer(
  void           *user,
  const uint16_t *data,
  int             len
) {
  struct _writer_hook_data_t *hook = user;

  if(len == 0)
    return;

  if(!hook->prefix_status) {
    int i = 0;
    while(i < len && data[i] <= ' ')
      i++;
    hook->prefix_status = i < len;
    if(i < len && data[i] == '-') {
      if(hook->options & PMATH_WRITE_OPTIONS_NOSPACES)
        _pmath_write_cstr("-", hook->next->write, hook->next->user);
      else
        _pmath_write_cstr(" - ", hook->next->write, hook->next->user);

      data += i + 1;
      len -= i + 1;
      hook->prefix_status = TRUE;
    }
    else if(i < len) {
      if(hook->options & PMATH_WRITE_OPTIONS_NOSPACES)
        _pmath_write_cstr("+", hook->next->write, hook->next->user);
      else
        _pmath_write_cstr(" + ", hook->next->write, hook->next->user);

      hook->prefix_status = TRUE;
    }
  }

  hook->next->write(hook->next->user, data, len);
}

static void write_expr_ex(
  struct pmath_write_ex_t  *info,
  int                       priority,
  pmath_expr_t              expr
) {
  size_t exprlen = pmath_expr_length(expr);
  pmath_t head = pmath_expr_get_item(expr, 0);

  if(info->options & PMATH_WRITE_OPTIONS_FULLEXPR)
    goto FULLFORM;

  if(info->options & PMATH_WRITE_OPTIONS_INPUTEXPR)
    goto INPUTFORM;

  if(pmath_same(head, pmath_System_BaseForm)) {
    pmath_thread_t thread = pmath_thread_get_current();

    if(exprlen != 2 || !thread)
      goto FULLFORM;

    uint8_t old_base_flags = thread->base_flags;
    
    pmath_t item = pmath_expr_get_item(expr, 2);
    
    if(pmath_same(item, pmath_System_Automatic)) {
      pmath_unref(item);
      thread->base_flags = (PMATH_BASE_FLAGS_BASE_MASK & old_base_flags) | PMATH_BASE_FLAGS_AUTOMATIC;
    }
    else if(pmath_is_int32(item) && 2 <= PMATH_AS_INT32(item) && PMATH_AS_INT32(item) <= 36) {
      thread->base_flags = (uint8_t)PMATH_AS_INT32(item);
    }
    else {
      pmath_unref(item);
      goto FULLFORM;
    }

    item = pmath_expr_get_item(expr, 1);
    write_ex(info, priority, item);
    pmath_unref(item);

    thread->base_flags = old_base_flags;
  }
  else if(pmath_same(head, pmath_System_DirectedInfinity)) {
    if(exprlen == 0) {
      _pmath_write_impl(info, pmath_System_ComplexInfinity);
    }
    else if(exprlen == 1) {
      pmath_t direction = pmath_expr_get_item(expr, 1);
      if(pmath_equals(direction, PMATH_FROM_INT32(1))) {
        _pmath_write_impl(info, pmath_System_Infinity);
      }
      else if(pmath_equals(direction, PMATH_FROM_INT32(-1))) {
        if(priority > PREC_FACTOR)
          WRITE_CSTR("(-");
        else
          WRITE_CSTR("-");

        _pmath_write_impl(info, pmath_System_Infinity);

        if(priority > PREC_FACTOR)
          WRITE_CSTR(")");
      }
      else {
        pmath_unref(direction);
        goto FULLFORM;
      }
      pmath_unref(direction);
    }
    else
      goto FULLFORM;
  }
  else if(pmath_same(head, pmath_System_FullForm)) {
    if(exprlen != 1)
      goto FULLFORM;

    pmath_t item = pmath_expr_get_item(expr, 1);
    int old_options = info->options;
    info->options |= PMATH_WRITE_OPTIONS_FULLSTR | PMATH_WRITE_OPTIONS_FULLEXPR | PMATH_WRITE_OPTIONS_ROUNDTRIP_NUMBERS;
    _pmath_write_impl(info, item);
    info->options = old_options;
    pmath_unref(item);
  }
  else if(pmath_same(head, pmath_System_InputForm)) {
    if(exprlen != 1)
      goto FULLFORM;

    pmath_t item = pmath_expr_get_item(expr, 1);
    int old_options = info->options;
    info->options |= PMATH_WRITE_OPTIONS_FULLSTR | PMATH_WRITE_OPTIONS_INPUTEXPR;
    _pmath_write_impl(info, item);
    info->options = old_options;
    pmath_unref(item);
  }
  else if(pmath_same(head, pmath_System_HoldForm)   ||
          pmath_same(head, pmath_System_HorizontalForm)   ||
          pmath_same(head, pmath_System_OutputForm) ||
          pmath_same(head, pmath_System_StandardForm))
  {
    if(exprlen != 1)
      goto FULLFORM;

    pmath_t item = pmath_expr_get_item(expr, 1);
    write_ex(info, priority, item);
    pmath_unref(item);
  }
  else if(pmath_same(head, pmath_System_LinearSolveFunction)) {
    if(exprlen <= 1)
      goto FULLFORM;

    pmath_t item = pmath_expr_get_item(expr, 1);

    write_ex(info, PMATH_PREC_CALL, head);
    WRITE_CSTR("(");
    _pmath_write_impl(info, item);

    char s[100];
    snprintf(s, sizeof(s), ", <<%u>>)", (int)(exprlen - 1));
    WRITE_CSTR(s);

    pmath_unref(item);
  }
  else if(pmath_same(head, pmath_System_LongForm)) {
    if(exprlen != 1)
      goto FULLFORM;

    pmath_t item    = pmath_expr_get_item(expr, 1);
    int old_options = info->options;
    info->options  |= PMATH_WRITE_OPTIONS_FULLNAME;
    _pmath_write_impl(info, item);
    info->options = old_options;
    pmath_unref(item);
  }
  else if(pmath_same(head, pmath_Developer_PackedArrayForm)) {
    if(exprlen != 1)
      goto FULLFORM;

    pmath_t item    = pmath_expr_get_item(expr, 1);
    int old_options = info->options;
    info->options  |= PMATH_WRITE_OPTIONS_PACKEDARRAYFORM;
    _pmath_write_impl(info, item);
    info->options = old_options;
    pmath_unref(item);
  }
  else if(pmath_same(head, pmath_System_StringForm)) {
    if(!_pmath_stringform_write(info, expr)) {
      goto FULLFORM;
    }
  }
  else if(pmath_same(head, pmath_System_Colon)) {
    if(exprlen < 2)
      goto FULLFORM;

    if(priority > PMATH_PREC_REL)
      WRITE_CSTR("(");

    for(size_t i = 1; i <= exprlen; i++) {
      if(i > 1)
        write_spaced_operator(info, 0x2236, ":");

      pmath_t item = pmath_expr_get_item(expr, i);
      write_ex(info, PMATH_PREC_REL + 1, item);
      pmath_unref(item);
    }

    if(priority > PMATH_PREC_REL)
      WRITE_CSTR(")");
  }
  else if(pmath_same(head, pmath_System_ColonForm)) {
    pmath_t item;

    if(exprlen != 2)
      goto FULLFORM;

    if(priority > PMATH_PREC_REL)
      WRITE_CSTR("(");
    
    item = pmath_expr_get_item(expr, 1);
    write_ex(info, PMATH_PREC_REL + 1, item);
    pmath_unref(item);
    
    WRITE_CSTR(": ");

    item = pmath_expr_get_item(expr, 2);
    write_ex(info, PMATH_PREC_REL, item);
    pmath_unref(item);
    
    if(priority > PMATH_PREC_REL)
      WRITE_CSTR(")");
  }
  else if(pmath_same(head, pmath_System_Graphics)) {
    WRITE_CSTR("<< Graphics >>");
  }
  else if(pmath_same(head, pmath_System_RawBoxes)) {
    pmath_t item;

    if(exprlen != 1)
      goto FULLFORM;

    item = pmath_symbol_get_value(pmath_System_BoxForm_DollarUseTextFormatting);
    pmath_unref(item);
    if(pmath_same(item, pmath_System_True))
      goto FULLFORM;

    item = pmath_expr_get_item(expr, 1);
    _pmath_write_boxes(info, item);
    pmath_unref(item);
  }
  else if(pmath_same(head, pmath_System_Row)) {
    pmath_expr_t list;
    pmath_t obj;
    size_t i, listlen;

    if(exprlen < 1 || exprlen > 2)
      goto FULLFORM;

    list = pmath_expr_get_item(expr, 1);
    if(!pmath_is_expr_of(list, pmath_System_List)) {
      pmath_unref(list);
      goto FULLFORM;
    }

    listlen = pmath_expr_length(list);
    if(exprlen == 2) {
      pmath_t seperator = pmath_expr_get_item(expr, 2);

      for(i = 1; i < listlen; ++i) {
        obj = pmath_expr_get_item(list, i);
        _pmath_write_impl(info, obj);
        pmath_unref(obj);
        _pmath_write_impl(info, seperator);
      }

      pmath_unref(seperator);

      if(listlen > 0) {
        obj = pmath_expr_get_item(list, listlen);
        _pmath_write_impl(info, obj);
        pmath_unref(obj);
      }
    }
    else {
      for(i = 1; i <= listlen; ++i) {
        obj = pmath_expr_get_item(list, i);
        _pmath_write_impl(info, obj);
        pmath_unref(obj);
      }
    }
    pmath_unref(list);
  }
  else if(pmath_same(head, pmath_System_Shallow)) {
    pmath_t item;
    size_t maxdepth = 4;
    size_t maxlength = 10;

    if(exprlen < 1 || exprlen > 2)
      goto FULLFORM;

    if(exprlen == 2) {
      item = pmath_expr_get_item(expr, 2);

      if(pmath_is_int32(item) && PMATH_AS_INT32(item) >= 0) {
        maxdepth = (unsigned)PMATH_AS_INT32(item);
      }
      else if(pmath_equals(item, _pmath_object_pos_infinity)) {
        maxdepth = SIZE_MAX;
      }
      else if(pmath_is_expr_of_len(item, pmath_System_List, 2)) {
        pmath_t obj = pmath_expr_get_item(item, 1);

        if(pmath_is_int32(obj) && PMATH_AS_INT32(obj) >= 0) {
          maxdepth = (unsigned)PMATH_AS_INT32(obj);
        }
        else if(pmath_equals(obj, _pmath_object_pos_infinity)) {
          maxdepth = SIZE_MAX;
        }
        else {
          pmath_unref(obj);
          pmath_unref(item);
          goto FULLFORM;
        }

        pmath_unref(obj);
        obj = pmath_expr_get_item(item, 2);

        if(pmath_is_int32(obj) && PMATH_AS_INT32(obj) >= 0) {
          maxlength = (unsigned)PMATH_AS_INT32(obj);
        }
        else if(pmath_equals(obj, _pmath_object_pos_infinity)) {
          maxlength = SIZE_MAX;
        }
        else {
          pmath_unref(obj);
          pmath_unref(item);
          goto FULLFORM;
        }

        pmath_unref(obj);
      }
      else {
        pmath_unref(item);
        goto FULLFORM;
      }

      pmath_unref(item);
    }

    item = pmath_expr_get_item(expr, 1);
    item = _pmath_prepare_shallow(item, maxdepth, maxlength);

    _pmath_write_impl(info, item);

    pmath_unref(item);
  }
  else if(pmath_same(head, pmath_System_Short)) {
    pmath_t item;
    double pagewidth = 72;
    double lines = 1;

    if(exprlen < 1 || exprlen > 2)
      goto FULLFORM;

    if(pmath_expr_length(expr) == 2) {
      item = pmath_expr_get_item(expr, 2);

      if(pmath_equals(item, _pmath_object_pos_infinity)) {
        lines = HUGE_VAL;
      }
      else if(pmath_is_number(item)) {
        lines = pmath_number_get_d(item);

        if(lines < 0) {
          pmath_unref(item);
          goto FULLFORM;
        }
      }
      else {
        pmath_unref(item);
        goto FULLFORM;
      }

      pmath_unref(item);
    }

    item = pmath_evaluate(pmath_ref(pmath_System_DollarPageWidth));
    if(pmath_equals(item, _pmath_object_pos_infinity)) {
      pagewidth = HUGE_VAL;
    }
    else if(pmath_is_number(item) && pmath_number_sign(item) > 0) {
      pagewidth = pmath_number_get_d(item);
    }
    else {
      pmath_unref(item);
      goto FULLFORM;
    }

    pmath_unref(item);
    item = pmath_expr_get_item(expr, 1);

    if(lines * pagewidth < INT_MAX / 4)
      _pmath_write_short(info, item, (int)(lines * pagewidth));
    else
      _pmath_write_impl(info, item);

    pmath_unref(item);
  }
  else if(pmath_same(head, pmath_System_Skeleton)) {
    pmath_t item;

    if(exprlen != 1)
      goto FULLFORM;

    item = pmath_expr_get_item(expr, 1);

    WRITE_CSTR("<<");
    _pmath_write_impl(info, item);
    WRITE_CSTR(">>");

    pmath_unref(item);
  }
  else if(pmath_is_expr_of_len(head, pmath_System_Derivative, 1)) {
    const uint16_t primes[] = {'\'', '\'', '\''};
    pmath_t item;
    int order = 0;
    
    if(exprlen != 1)
      goto FULLFORM;
    
    item = pmath_expr_get_item(head, 1);
    if(pmath_is_int32(item)) {
      order = PMATH_AS_INT32(item);
    }
    else {
      pmath_unref(item);
      goto FULLFORM;
    }
    
    if(order < 1 || order > (int)(sizeof(primes)/sizeof(primes[0])))
      goto FULLFORM;
    
    
    if(priority > PMATH_PREC_DIFF)
      WRITE_CSTR("(");

    item = pmath_expr_get_item(expr, 1);
    write_ex(info, PMATH_PREC_DIFF + 1, item);
    pmath_unref(item);
    
    info->write(info->user, primes, order);
    
    if(priority > PMATH_PREC_DIFF)
      WRITE_CSTR(")");
  }
  else if(pmath_same(head, pmath_System_Button)) {
    if(exprlen < 1)
      goto FULLFORM;
    
    write_ex(info, PMATH_PREC_CALL, head);
    WRITE_CSTR("(");
    
    pmath_t item = pmath_expr_get_item(expr, 1);
   _pmath_write_impl(info, item);
    pmath_unref(item);
    
    WRITE_CSTR(")");
  }
  else if(pmath_same(head, pmath_System_Tooltip)) {
    if(exprlen < 1)
      goto FULLFORM;
    
    pmath_t item = pmath_expr_get_item(expr, 1);
   _pmath_write_impl(info, item);
    pmath_unref(item);
  }
  else INPUTFORM:
    if(exprlen == 2 && /*=========================================*/
        (pmath_same(head, pmath_System_Assign)        ||
         pmath_same(head, pmath_System_AssignDelayed) ||
         pmath_same(head, pmath_System_AssignWith)    ||
         pmath_same(head, pmath_System_Decrement)     ||
         pmath_same(head, pmath_System_DivideBy)      ||
         pmath_same(head, pmath_System_Increment)     ||
         pmath_same(head, pmath_System_TimesBy)))
    {
      pmath_t lhs, rhs;

      if(priority > PMATH_PREC_ASS)
        WRITE_CSTR("(");

      lhs = pmath_expr_get_item(expr, 1);
      rhs = pmath_expr_get_item(expr, 2);

      write_ex(info, PMATH_PREC_ASS + 1, lhs);

      if(     pmath_same(head, pmath_System_Assign))         write_spaced_operator2(info, FALSE, TRUE, PMATH_CHAR_ASSIGN,        ":=");
      else if(pmath_same(head, pmath_System_AssignDelayed))  write_spaced_operator2(info, FALSE, TRUE, PMATH_CHAR_ASSIGNDELAYED, "::=");
      else if(pmath_same(head, pmath_System_AssignWith))     write_spaced_operator2(info, FALSE, TRUE, 0, "//=");
      else if(pmath_same(head, pmath_System_Decrement))      write_spaced_operator2(info, FALSE, TRUE, 0, "-=");
      else if(pmath_same(head, pmath_System_DivideBy))       write_spaced_operator2(info, FALSE, TRUE, 0, "/=");
      else if(pmath_same(head, pmath_System_Increment))      write_spaced_operator2(info, FALSE, TRUE, 0, "+=");
      else                                                   write_spaced_operator2(info, FALSE, TRUE, 0, "*=");

      write_ex(info, PMATH_PREC_ASS, rhs);

      pmath_unref(lhs);
      pmath_unref(rhs);

      if(priority > PMATH_PREC_ASS)
        WRITE_CSTR(")");
    }
    else if(pmath_same(head, pmath_System_Decrement)     ||
            pmath_same(head, pmath_System_Increment)     ||
            pmath_same(head, pmath_System_PostDecrement) ||
            pmath_same(head, pmath_System_PostIncrement))
    {
      pmath_t arg;

      if(exprlen != 1)
        goto FULLFORM;

      if(priority > PMATH_PREC_INC)
        WRITE_CSTR("(");

      if(     pmath_same(head, pmath_System_Decrement))  WRITE_CSTR("--");
      else if(pmath_same(head, pmath_System_Increment))  WRITE_CSTR("++");

      arg = pmath_expr_get_item(expr, 1);
      write_ex(info, PMATH_PREC_INC + 1, arg);
      pmath_unref(arg);

      if(     pmath_same(head, pmath_System_PostDecrement))  WRITE_CSTR("--");
      else if(pmath_same(head, pmath_System_PostIncrement))  WRITE_CSTR("++");

      if(priority > PMATH_PREC_INC)
        WRITE_CSTR(")");
    }
    else if(pmath_same(head, pmath_System_TagAssign) ||
            pmath_same(head, pmath_System_TagAssignDelayed))
    {
      pmath_t obj;

      if(exprlen != 3)
        goto FULLFORM;

      if(priority > PMATH_PREC_ASS)
        WRITE_CSTR("(");

      obj = pmath_expr_get_item(expr, 1);
      write_ex(info, PMATH_PREC_PRIM, obj);
      pmath_unref(obj);

      write_spaced_operator2(info, FALSE, TRUE, 0, "/:");

      obj = pmath_expr_get_item(expr, 2);
      write_ex(info, PMATH_PREC_ASS + 1, obj);
      pmath_unref(obj);

      if(pmath_same(head, pmath_System_TagAssign))  write_spaced_operator2(info, FALSE, TRUE, PMATH_CHAR_ASSIGN,        ":=");
      else                                          write_spaced_operator2(info, FALSE, TRUE, PMATH_CHAR_ASSIGNDELAYED, "::=");

      obj = pmath_expr_get_item(expr, 3);
      write_ex(info, PMATH_PREC_ASS, obj);
      pmath_unref(obj);

      if(priority > PMATH_PREC_ASS)
        WRITE_CSTR(")");
    }
    else if(pmath_same(head, pmath_System_Rule) ||
            pmath_same(head, pmath_System_RuleDelayed))
    {
      pmath_t lhs, rhs;

      if(exprlen != 2)
        goto FULLFORM;

      if(priority > PMATH_PREC_RULE)
        WRITE_CSTR("(");

      lhs = pmath_expr_get_item(expr, 1);
      rhs = pmath_expr_get_item(expr, 2);

      write_ex(info, PMATH_PREC_RULE + 1, lhs);

      if(pmath_same(head, pmath_System_Rule)) write_spaced_operator(info, PMATH_CHAR_RULE,        "->");
      else                                    write_spaced_operator(info, PMATH_CHAR_RULEDELAYED, ":>");

      write_ex(info, PMATH_PREC_RULE, rhs);

      pmath_unref(lhs);
      pmath_unref(rhs);

      if(priority > PMATH_PREC_RULE)
        WRITE_CSTR(")");
    }
    else if(pmath_same(head, pmath_System_List)) {
      size_t i;

      WRITE_CSTR("{");
      for(i = 1; i <= exprlen; i++) {
        pmath_t item;

        if(i > 1)
          write_spaced_operator2(info, FALSE, TRUE, ',', ",");

        item = pmath_expr_get_item(expr, i);
        _pmath_write_impl(info, item);
        pmath_unref(item);
      }

      WRITE_CSTR("}");
    }
    else if(pmath_same(head, pmath_System_StringExpression)) {
      pmath_t item;
      size_t i;

      if(exprlen < 2)
        goto FULLFORM;

      if(priority > PMATH_PREC_STR)
        WRITE_CSTR("(");

      for(i = 1; i <= exprlen; i++) {
        if(i > 1)
          write_spaced_operator(info, 0, "++");

        item = pmath_expr_get_item(expr, i);
        write_ex(info, PMATH_PREC_STR + 1, item);
        pmath_unref(item);
      }

      if(priority > PMATH_PREC_STR)
        WRITE_CSTR(")");
    }
    else if(pmath_same(head, pmath_System_EvaluationSequence)) {
      pmath_t item;
      size_t i;

      if(exprlen < 2)
        goto FULLFORM;

      if(priority > PMATH_PREC_ANY)
        WRITE_CSTR("(");

      for(i = 1; i < exprlen; i++) {
        if(i > 1)
          write_spaced_operator2(info, FALSE, TRUE, ';', ";");

        item = pmath_expr_get_item(expr, i);
        write_ex(info, PMATH_PREC_ANY + 1, item);
        pmath_unref(item);
      }

      item = pmath_expr_get_item(expr, exprlen);
      if(!pmath_is_null(item)) {
        write_spaced_operator2(info, FALSE, TRUE, ';', ";");
        write_ex(info, PMATH_PREC_ANY + 1, item);
        pmath_unref(item);
      }
      else
        WRITE_CSTR(";");

      if(priority > PMATH_PREC_ANY)
        WRITE_CSTR(")");
    }
    else if(pmath_same(head, pmath_System_Alternatives)) {
      pmath_t item;
      size_t i;

      if(exprlen < 2)
        goto FULLFORM;

      if(priority > PMATH_PREC_ALT)
        WRITE_CSTR("(");

      for(i = 1; i <= exprlen; i++) {
        if(i > 1)
          write_spaced_operator(info, '|', "|");

        item = pmath_expr_get_item(expr, i);
        write_ex(info, PMATH_PREC_ALT + 1, item);
        pmath_unref(item);
      }

      if(priority > PMATH_PREC_ALT)
        WRITE_CSTR(")");
    }
    else if(pmath_same(head, pmath_System_Or)) {
      pmath_t item;
      size_t i;

      if(exprlen < 2)
        goto FULLFORM;

      if(priority > PMATH_PREC_OR)
        WRITE_CSTR("(");

      for(i = 1; i <= exprlen; i++) {
        if(i > 1)
          write_spaced_operator(info, 0, "||");

        item = pmath_expr_get_item(expr, i);
        write_ex(info, PMATH_PREC_OR + 1, item);
        pmath_unref(item);
      }

      if(priority > PMATH_PREC_OR)
        WRITE_CSTR(")");
    }
    else if(pmath_same(head, pmath_System_And)) {
      pmath_t item;
      size_t i;

      if(exprlen < 2)
        goto FULLFORM;

      if(priority > PMATH_PREC_AND)
        WRITE_CSTR("(");

      for(i = 1; i <= exprlen; i++) {
        if(i > 1)
          write_spaced_operator(info, 0, "&&");

        item = pmath_expr_get_item(expr, i);
        write_ex(info, PMATH_PREC_AND + 1, item);
        pmath_unref(item);
      }

      if(priority > PMATH_PREC_AND)
        WRITE_CSTR(")");
    }
    else if(pmath_same(head, pmath_System_Not)) {
      pmath_t item;

      if(exprlen != 1)
        goto FULLFORM;

      if(priority > PREC_NOT)  WRITE_CSTR("(!");
      else                     WRITE_CSTR("!");

      item = pmath_expr_get_item(expr, 1);
      write_ex(info, PREC_NOT + 1, item);
      pmath_unref(item);

      if(priority > PREC_NOT)
        WRITE_CSTR(")");
    }
    else if(pmath_same(head, pmath_System_Identical)) {
      pmath_t item;
      size_t i;

      if(exprlen < 2)
        goto FULLFORM;

      if(priority > PMATH_PREC_REL)
        WRITE_CSTR("(");

      for(i = 1; i <= exprlen; i++) {
        if(i > 1)
          write_spaced_operator(info, 0, "==="); // note that U+2261 is \[Congruent]

        item = pmath_expr_get_item(expr, i);
        write_ex(info, PMATH_PREC_REL + 1, item);
        pmath_unref(item);
      }

      if(priority > PMATH_PREC_REL)
        WRITE_CSTR(")");
    }
    else if(pmath_same(head, pmath_System_Unidentical)) {
      pmath_t item;
      size_t i;

      if(exprlen < 2)
        goto FULLFORM;

      if(priority > PMATH_PREC_REL)
        WRITE_CSTR("(");

      for(i = 1; i <= exprlen; i++) {
        if(i > 1)
          write_spaced_operator(info, 0, "=!="); // note that U+2262 is \[NotCongruent]

        item = pmath_expr_get_item(expr, i);
        write_ex(info, PMATH_PREC_REL + 1, item);
        pmath_unref(item);
      }

      if(priority > PMATH_PREC_REL)
        WRITE_CSTR(")");
    }
    else if(pmath_same(head, pmath_System_Less)         ||
            pmath_same(head, pmath_System_LessEqual)    ||
            pmath_same(head, pmath_System_Greater)      ||
            pmath_same(head, pmath_System_GreaterEqual) ||
            pmath_same(head, pmath_System_Equal)        ||
            pmath_same(head, pmath_System_Unequal))
    {
      pmath_t item;
      uint16_t uni_op;
      char   *ascii_op;
      size_t  i;

      if(exprlen < 2)
        goto FULLFORM;

      if(     pmath_same(head, pmath_System_Less))         { uni_op = '<';    ascii_op = "<";  }
      else if(pmath_same(head, pmath_System_LessEqual))    { uni_op = 0x2264; ascii_op = "<="; }
      else if(pmath_same(head, pmath_System_Greater))      { uni_op = '>';    ascii_op = ">";  }
      else if(pmath_same(head, pmath_System_GreaterEqual)) { uni_op = 0x2265; ascii_op = ">="; }
      else if(pmath_same(head, pmath_System_Equal))        { uni_op = '=';    ascii_op = "=";  }
      else if(pmath_same(head, pmath_System_Unequal))      { uni_op = 0x2260; ascii_op = "!="; }
      else                                                 { uni_op = 0;      ascii_op = "<<?>>"; }

      if(priority > PMATH_PREC_REL)
        WRITE_CSTR("(");

      for(i = 1; i <= exprlen; i++) {
        if(i > 1)
          write_spaced_operator(info, uni_op, ascii_op);

        item = pmath_expr_get_item(expr, i);
        write_ex(info, PMATH_PREC_REL + 1, item);
        pmath_unref(item);
      }

      if(priority > PMATH_PREC_REL)
        WRITE_CSTR(")");
    }
    else if(pmath_same(head, pmath_System_Inequation)) {
      pmath_t item;
      size_t i;

      if(exprlen < 3 || (exprlen & 1) == 0)
        goto FULLFORM;

      for(i = 2; i < exprlen; i+= 2) {
        item = pmath_expr_get_item(expr, i);
        pmath_unref(item);
        if( !pmath_same(item, pmath_System_Less) &&
            !pmath_same(item, pmath_System_LessEqual) &&
            !pmath_same(item, pmath_System_Greater) &&
            !pmath_same(item, pmath_System_GreaterEqual) &&
            !pmath_same(item, pmath_System_Equal) &&
            !pmath_same(item, pmath_System_Unequal) &&
            !pmath_same(item, pmath_System_Identical) &&
            !pmath_same(item, pmath_System_Unidentical))
        {
          goto FULLFORM;
        }
      }

      if(priority > PMATH_PREC_REL)
        WRITE_CSTR("(");

      item = pmath_expr_get_item(expr, 1);
      write_ex(info, PMATH_PREC_REL + 1, item);
      pmath_unref(item);

      exprlen /= 2;
      for(i = 1; i <= exprlen; i++) {
        item = pmath_expr_get_item(expr, 2 * i);
        if(     pmath_same(item, pmath_System_Less))          write_spaced_operator(info, '<',    "<");
        else if(pmath_same(item, pmath_System_LessEqual))     write_spaced_operator(info, 0x2264, "<=");
        else if(pmath_same(item, pmath_System_Greater))       write_spaced_operator(info, '>',    ">");
        else if(pmath_same(item, pmath_System_GreaterEqual))  write_spaced_operator(info, 0x2265, ">=");
        else if(pmath_same(item, pmath_System_Equal))         write_spaced_operator(info, '=',    "=");
        else if(pmath_same(item, pmath_System_Unequal))       write_spaced_operator(info, 0x2260, "!=");
        else if(pmath_same(item, pmath_System_Identical))     write_spaced_operator(info, 0,      "===");
        else if(pmath_same(item, pmath_System_Unidentical))   write_spaced_operator(info, 0,      "=!=");
        else                                                  write_spaced_operator(info, 0,      "<<?>>");
        pmath_unref(item);

        item = pmath_expr_get_item(expr, 2 * i + 1);
        write_ex(info, PMATH_PREC_REL + 1, item);
        pmath_unref(item);
      }

      if(priority > PMATH_PREC_REL)
        WRITE_CSTR(")");
    }
    else if(pmath_same(head, pmath_System_Plus)) {
      struct _writer_hook_data_t sum_writer_data;
      struct pmath_write_ex_t    hook_info;
      size_t i;

      if(exprlen < 2)
        goto FULLFORM;

      if(priority > PMATH_PREC_ADD)
        WRITE_CSTR("(");

      sum_writer_data.next          = info;
      sum_writer_data.options       = info->options;
      sum_writer_data.prefix_status = TRUE;
      init_hook_info(&hook_info, &sum_writer_data);
      hook_info.write = sum_writer;

      for(i = 1; i <= exprlen; i++) {
        pmath_t item = pmath_expr_get_item(expr, i);
        write_ex(&hook_info, PMATH_PREC_ADD + 1, item);

        sum_writer_data.prefix_status = FALSE;
        pmath_unref(item);
      }

      if(priority > PMATH_PREC_ADD)
        WRITE_CSTR(")");
    }
    else if(pmath_same(head, pmath_System_Times)) {
      struct _writer_hook_data_t  product_writer_data;
      struct pmath_write_ex_t     hook_info;
      pmath_t item;
      size_t i;
      pmath_bool_t skip_star = FALSE;

      if(exprlen < 2)
        goto FULLFORM;

      if(priority > PMATH_PREC_MUL)
        WRITE_CSTR("(");

      product_writer_data.next          = info;
      product_writer_data.options       = info->options;
      product_writer_data.prefix_status = 1;
      product_writer_data.special_end   = FALSE;
      init_hook_info(&hook_info, &product_writer_data);
      hook_info.write = product_writer;

      item = pmath_expr_get_item(expr, 1);
      if(pmath_same(item, PMATH_FROM_INT32(-1))) {
        _pmath_write_cstr("-", info->write, info->user);
        skip_star = TRUE;
      }
      else {
        write_ex(&hook_info, PMATH_PREC_MUL, item);
        product_writer_data.prefix_status = 0;
      }
      pmath_unref(item);

      for(i = 2; i <= exprlen; i++) {
        item = pmath_expr_get_item(expr, i);

        if(product_writer_data.special_end ||
            (info->options & PMATH_WRITE_OPTIONS_INPUTEXPR))
        {
          if(skip_star) {
            skip_star = FALSE;
          }
          else {
            write_spaced_operator2(info, FALSE, FALSE, 0x00D7, "*");
            product_writer_data.prefix_status = 1;
          }
        }

        write_ex(&hook_info, PREC_FACTOR, item);
        product_writer_data.prefix_status = 0;
        pmath_unref(item);
      }

      if(priority > PMATH_PREC_MUL)
        WRITE_CSTR(")");
    }
    else if(pmath_same(head, pmath_System_Power)) {
      struct _writer_hook_data_t  division_writer_data;
      struct pmath_write_ex_t     hook_info;
      pmath_t base, exponent;

      if(exprlen != 2)
        goto FULLFORM;

      exponent = pmath_expr_get_item(expr, 2);
      base = pmath_expr_get_item(expr, 1);

      division_writer_data.next          = info;
      division_writer_data.options       = info->options;
      division_writer_data.prefix_status = 0;
      init_hook_info(&hook_info, &division_writer_data);
      hook_info.write = division_writer;

      if(pmath_equals(exponent, _pmath_one_half)) {
        WRITE_CSTR("Sqrt(");
        _pmath_write_impl(info, base);
        WRITE_CSTR(")");
      }
      else if(pmath_equals(exponent, PMATH_FROM_INT32(-1))) {
        if(priority > PREC_FACTOR)
          WRITE_CSTR("(");
        
        WRITE_CSTR("1/");

        write_ex(&hook_info, PMATH_PREC_POW + 1, base);
        
        if(priority > PREC_FACTOR)
          WRITE_CSTR(")");
      }
      else if(pmath_is_integer(exponent) && pmath_number_sign(exponent) < 0) {
        if(priority > PREC_FACTOR)
          WRITE_CSTR("(");
        
        WRITE_CSTR("1/");

        exponent = pmath_number_neg(exponent);

        write_ex(&hook_info, PMATH_PREC_POW + 1, base);

        WRITE_CSTR("^");
        write_ex(info, PMATH_PREC_POW, exponent);
        
        if(priority > PREC_FACTOR)
          WRITE_CSTR(")");
      }
      else {
        pmath_t minus_one_half = pmath_number_neg(pmath_ref(_pmath_one_half));

        if(pmath_equals(exponent, minus_one_half)) {
          if(priority > PREC_FACTOR)
            WRITE_CSTR("(");
            
          WRITE_CSTR("1/Sqrt(");
          _pmath_write_impl(info, base);
          WRITE_CSTR(")");
          
          if(priority > PREC_FACTOR)
            WRITE_CSTR(")");
        }
        else {
          if(priority > PMATH_PREC_POW)
            WRITE_CSTR("(");

          write_ex(info, PMATH_PREC_POW + 1, base);
          WRITE_CSTR("^");
          write_ex(info, PMATH_PREC_POW, exponent);

          if(priority > PMATH_PREC_POW)
            WRITE_CSTR(")");
        }

        pmath_unref(minus_one_half);
      }

      pmath_unref(base);
      pmath_unref(exponent);
    }
    else if(pmath_same(head, pmath_System_Range)) {
      pmath_t item;
      if(exprlen < 2 || exprlen > 3)
        goto FULLFORM;

      if(priority > PMATH_PREC_RANGE)
        WRITE_CSTR("(");

      item = pmath_expr_get_item(expr, 1);
      if(exprlen > 2 || !pmath_same(item, pmath_System_Automatic))
        write_ex(info, PMATH_PREC_RANGE + 1, item);
      pmath_unref(item);

      write_spaced_operator(info, 0, "..");

      item = pmath_expr_get_item(expr, 2);
      if(exprlen > 2 || !pmath_same(item, pmath_System_Automatic))
        write_ex(info, PMATH_PREC_RANGE + 1, item);
      pmath_unref(item);

      if(exprlen == 3) {
        write_spaced_operator(info, 0, "..");

        item = pmath_expr_get_item(expr, 3);
        write_ex(info, PMATH_PREC_RANGE + 1, item);
        pmath_unref(item);
      }

      if(priority > PMATH_PREC_RANGE)
        WRITE_CSTR(")");
    }
    else if(pmath_same(head, pmath_System_Part)) {
      pmath_t item;
      size_t i;

      if(exprlen < 1)
        goto FULLFORM;

      item = pmath_expr_get_item(expr, 1);
      write_ex(info, PMATH_PREC_CALL, item);
      pmath_unref(item);

      WRITE_CSTR("[");
      for(i = 2; i <= exprlen; i++) {
        if(i > 2)
          write_spaced_operator2(info, FALSE, TRUE, ',', ",");

        item = pmath_expr_get_item(expr, i);
        _pmath_write_impl(info, item);
        pmath_unref(item);
      }
      WRITE_CSTR("]");
    }
    else if(pmath_same(head, pmath_System_MessageName)) {
      pmath_t symbol;
      pmath_t tag;

      if(exprlen != 2)
        goto FULLFORM;

      tag = pmath_expr_get_item(expr, 2);

      if(!pmath_is_string(tag)) {
        pmath_unref(tag);
        goto FULLFORM;
      }

      if(priority > PMATH_PREC_CALL)
        WRITE_CSTR("(");
      symbol = pmath_expr_get_item(expr, 1);

      write_ex(info, PMATH_PREC_CALL, symbol);
      WRITE_CSTR("::");
      _pmath_write_impl(info, tag);

      pmath_unref(symbol);
      pmath_unref(tag);

      if(priority > PMATH_PREC_CALL)
        WRITE_CSTR(")");
    }
    else if(pmath_same(head, pmath_System_Complex)) {
      pmath_number_t re, im;
      pmath_bool_t lparen = FALSE;

      if(!_pmath_is_nonreal_complex_number(expr))
        goto FULLFORM;

      re = pmath_expr_get_item(expr, 1);
      im = pmath_expr_get_item(expr, 2);

      if(!pmath_same(re, PMATH_FROM_INT32(0))) {
        if(priority > PMATH_PREC_ADD) {
          WRITE_CSTR("(");
          lparen = TRUE;
        }

        _pmath_write_impl(info, re);

        if(pmath_number_sign(im) >= 0)
          write_spaced_operator(info, '+', "+");
        else if(!(info->options & PMATH_WRITE_OPTIONS_NOSPACES))
          WRITE_CSTR(" ");
      }

      if(pmath_equals(im, PMATH_FROM_INT32(-1))) {
        if(!lparen && priority > PMATH_PREC_MUL) {
          WRITE_CSTR("(");
          lparen = TRUE;
        }
        WRITE_CSTR("-");
      }
      else if(!pmath_equals(im, PMATH_FROM_INT32(1))) {
        if(!lparen && priority > PMATH_PREC_MUL) {
          WRITE_CSTR("(");
          lparen = TRUE;
        }
        _pmath_write_impl(info, im);
        WRITE_CSTR(" ");
      }
      
      if(info->options & PMATH_WRITE_OPTIONS_PREFERUNICODE) {
        const uint16_t imaginary_i_char = 0x2148;
        if(info->can_write_unicode && info->can_write_unicode(info->user, &imaginary_i_char, 1)) {
          info->write(info->user, &imaginary_i_char, 1);
        }
        else {
          WRITE_CSTR("ImaginaryI");
        }
      }
      else {
        WRITE_CSTR("ImaginaryI");
      }
      
      if(lparen)
        WRITE_CSTR(")");

      pmath_unref(re);
      pmath_unref(im);
    }
    else if(pmath_same(head, pmath_System_Pattern)) {
      pmath_t sym, pat;

      if(exprlen != 2)
        goto FULLFORM;

      sym = pmath_expr_get_item(expr, 1);
      if(!pmath_is_symbol(sym)) {
        pmath_unref(sym);
        goto FULLFORM;
      }

      pat = pmath_expr_get_item(expr, 2);
      if(pmath_equals(pat, _pmath_object_singlematch)) {
        WRITE_CSTR("~");
        _pmath_write_impl(info, sym);
      }
      else if(pmath_equals(pat, _pmath_object_multimatch)) {
        WRITE_CSTR("~~");
        _pmath_write_impl(info, sym);
      }
      else if(pmath_equals(pat, _pmath_object_zeromultimatch)) {
        WRITE_CSTR("~~~");
        _pmath_write_impl(info, sym);
      }
      else {
        pmath_bool_t default_pattern = TRUE;

        if(pmath_is_expr_of_len(pat, pmath_System_SingleMatch, 1)) {
          pmath_t type = pmath_expr_get_item(pat, 1);

          WRITE_CSTR("~");
          _pmath_write_impl(info, sym);
          WRITE_CSTR(":");

          write_ex(info, PMATH_PREC_PRIM, type);

          pmath_unref(type);
          default_pattern = FALSE;
        }
        else if(pmath_is_expr_of_len(pat, pmath_System_Repeated, 2)) {
          pmath_t rep   = pmath_expr_get_item(pat, 1);
          pmath_t range = pmath_expr_get_item(pat, 2);

          if(pmath_is_expr_of_len(rep, pmath_System_SingleMatch, 1)) {
            pmath_t type = pmath_expr_get_item(rep, 1);

            if(pmath_equals(range, _pmath_object_range_from_one)) {
              WRITE_CSTR("~~");
              _pmath_write_impl(info, sym);
              WRITE_CSTR(":");

              write_ex(info, PMATH_PREC_PRIM, type);
              default_pattern = FALSE;
            }
            else if(pmath_equals(range, _pmath_object_range_from_zero)) {
              WRITE_CSTR("~~~");
              _pmath_write_impl(info, sym);
              WRITE_CSTR(":");

              write_ex(info, PMATH_PREC_PRIM, type);
              default_pattern = FALSE;
            }

            pmath_unref(type);
          }

          pmath_unref(rep);
          pmath_unref(range);
        }

        if(default_pattern) {
          if(priority > PMATH_PREC_ALT)
            WRITE_CSTR("(");

          _pmath_write_impl(info, sym);
          write_spaced_operator(info, ':', ":");
          write_ex(info, PMATH_PREC_ALT, pat);

          if(priority > PMATH_PREC_ALT)
            WRITE_CSTR(")");
        }
      }

      pmath_unref(sym);
      pmath_unref(pat);
    }
    else if(pmath_same(head, pmath_System_TestPattern)) {
      pmath_t item;

      if(exprlen != 2)
        goto FULLFORM;

      if(priority > PMATH_PREC_TEST)
        WRITE_CSTR("(");

      item = pmath_expr_get_item(expr, 1);
      write_ex(info, PMATH_PREC_TEST + 1, item);
      pmath_unref(item);

      write_spaced_operator(info, '?', "?");

      item = pmath_expr_get_item(expr, 2);
      write_ex(info, PMATH_PREC_CALL, item);
      pmath_unref(item);

      if(priority > PMATH_PREC_TEST)
        WRITE_CSTR(")");
    }
    else if(pmath_same(head, pmath_System_Condition)) {
      pmath_t item;

      if(exprlen != 2)
        goto FULLFORM;

      if(priority > PMATH_PREC_COND)
        WRITE_CSTR("(");

      item = pmath_expr_get_item(expr, 1);
      write_ex(info, PMATH_PREC_COND + 1, item);
      pmath_unref(item);

      write_spaced_operator(info, 0, "/?");

      item = pmath_expr_get_item(expr, 2);
      write_ex(info, PMATH_PREC_ALT + 1, item);
      pmath_unref(item);

      if(priority > PMATH_PREC_COND)
        WRITE_CSTR(")");
    }
    else if(pmath_same(head, pmath_System_SingleMatch)) {
      if(exprlen == 0) {
        WRITE_CSTR("~");
      }
      else if(exprlen == 1) {
        pmath_t type = pmath_expr_get_item(expr, 1);

        WRITE_CSTR("~:");
        write_ex(info, PMATH_PREC_PRIM, type);

        pmath_unref(type);
      }
      else
        goto FULLFORM;
    }
    else if(pmath_same(head, pmath_System_Repeated)) {
      pmath_t pattern, range;

      if(exprlen != 2)
        goto FULLFORM;

      pattern = pmath_expr_get_item(expr, 1);
      range = pmath_expr_get_item(expr, 2);

      if(pmath_equals(range, _pmath_object_range_from_one)) {
        if(pmath_equals(pattern, _pmath_object_singlematch)) {
          WRITE_CSTR("~~");
        }
        else if(pmath_is_expr_of_len(pattern, pmath_System_SingleMatch, 1)) {
          pmath_t type = pmath_expr_get_item(pattern, 1);

          WRITE_CSTR("~~:");
          write_ex(info, PMATH_PREC_PRIM, type);

          pmath_unref(type);
        }
        else {
          if(priority > PMATH_PREC_REPEAT)
            WRITE_CSTR("(");

          write_ex(info, PMATH_PREC_REPEAT + 1, pattern);
          WRITE_CSTR("**");

          if(priority > PMATH_PREC_REPEAT)
            WRITE_CSTR(")");
        }
      }
      else if(pmath_equals(range, _pmath_object_range_from_zero)) {
        if(pmath_equals(pattern, _pmath_object_singlematch)) {
          WRITE_CSTR("~~~");
        }
        else if(pmath_is_expr_of_len(pattern, pmath_System_SingleMatch, 1)) {
          pmath_t type = pmath_expr_get_item(pattern, 1);

          WRITE_CSTR("~~~:");
          write_ex(info, PMATH_PREC_PRIM, type);

          pmath_unref(type);
        }
        else {
          if(priority > PMATH_PREC_REPEAT)
            WRITE_CSTR("(");

          write_ex(info, PMATH_PREC_REPEAT + 1, pattern);
          WRITE_CSTR("***");

          if(priority > PMATH_PREC_REPEAT)
            WRITE_CSTR(")");
        }
      }
      else {
        pmath_unref(pattern);
        pmath_unref(range);
        goto FULLFORM;
      }

      pmath_unref(pattern);
      pmath_unref(range);
    }
    else if(pmath_same(head, pmath_System_PureArgument)) {
      pmath_t item;

      if(exprlen != 1)
        goto FULLFORM;

      item = pmath_expr_get_item(expr, 1);

      if(pmath_is_integer(item) && pmath_number_sign(item) > 0) {
        if(priority > PMATH_PREC_CALL)
          WRITE_CSTR("(#");
        else
          WRITE_CSTR("#");

        write_ex(info, PMATH_PREC_CALL + 1, item);

        if(priority > PMATH_PREC_CALL)
          WRITE_CSTR(")");
      }
      else if(pmath_is_expr_of_len(item, pmath_System_Range, 2)) {
        pmath_t a = pmath_expr_get_item(item, 1);

        if( pmath_expr_item_equals(item, 2, pmath_System_Automatic) &&
            pmath_is_integer(a)                   &&
            pmath_number_sign(a) > 0)
        {
          if(priority > PMATH_PREC_CALL)
            WRITE_CSTR("(##");
          else
            WRITE_CSTR("##");

          write_ex(info, PMATH_PREC_CALL + 1, a);
          pmath_unref(a);

          if(priority > PMATH_PREC_CALL)
            WRITE_CSTR(")");
        }
        else {
          pmath_unref(a);
          pmath_unref(item);
          goto FULLFORM;
        }
      }
      else {
        pmath_unref(item);
        goto FULLFORM;
      }

      pmath_unref(item);
    }
    else if(pmath_same(head, pmath_System_Optional)) {
      pmath_t item;
      pmath_t sub_item;

      if(exprlen < 1 || exprlen > 2)
        goto FULLFORM;

      item = pmath_expr_get_item(expr, 1);
      if(!pmath_is_expr_of_len(item, pmath_System_Pattern, 2)) {
        pmath_unref(item);
        goto FULLFORM;
      }

      sub_item = pmath_expr_get_item(item, 2);
      if(!pmath_equals(sub_item, _pmath_object_singlematch)) {
        pmath_unref(sub_item);
        pmath_unref(item);
        goto FULLFORM;
      }

      pmath_unref(sub_item);
      sub_item = pmath_expr_get_item(item, 1);
      if(!pmath_is_symbol(sub_item)) {
        pmath_unref(sub_item);
        pmath_unref(item);
        goto FULLFORM;
      }

      if(exprlen == 2 && priority > PMATH_PREC_MUL)  WRITE_CSTR("(?");
      else                                       WRITE_CSTR("?");

      _pmath_write_impl(info, sub_item);
      pmath_unref(sub_item);
      pmath_unref(item);

      if(exprlen == 2) {
        WRITE_CSTR(":");

        item = pmath_expr_get_item(expr, 2);
        write_ex(info, PMATH_PREC_ADD + 1, item);
        pmath_unref(item);

        if(priority > PMATH_PREC_MUL)
          WRITE_CSTR(")");
      }
    }
    else { /*====================================================================*/
      pmath_t item;
      size_t i;

    FULLFORM:

      write_ex(info, PMATH_PREC_CALL, head);
      WRITE_CSTR("(");
      for(i = 1; i <= exprlen; i++) {
        if(i > 1)
          write_spaced_operator2(info, FALSE, TRUE, ',', ",");

        item = pmath_expr_get_item(expr, i);
        if(!pmath_is_null(item) || exprlen < 2)
          _pmath_write_impl(info, item);
        pmath_unref(item);
      }
      WRITE_CSTR(")");
    }

  pmath_unref(head);
}

PMATH_PRIVATE
void _pmath_expr_write(struct pmath_write_ex_t *info, pmath_t expr) {
  if(_pmath_write_user_format(info, expr))
    return;

  write_expr_ex(info, PMATH_PREC_ANY, expr);
}

//}

/*============================================================================*/

PMATH_PRIVATE pmath_bool_t _pmath_expressions_init(void) {
//  global_change_time = 0;

  memset(expr_caches,            0, sizeof(expr_caches));
  memset((void *)expr_cache_pos, 0, sizeof(expr_cache_pos));

#ifdef PMATH_DEBUG_LOG
  pmath_atomic_write_release(&expr_cache_hits,   0);
  pmath_atomic_write_release(&expr_cache_misses, 0);
#endif

  _pmath_init_special_type(
    PMATH_TYPE_SHIFT_EXPRESSION_GENERAL,
    _pmath_compare_exprsym,
    hash_expression_cached,
    destroy_expr_tree,//destroy_general_expression,//
    _pmath_expr_equal,
    _pmath_expr_write);

  _pmath_init_special_type(
    PMATH_TYPE_SHIFT_EXPRESSION_GENERAL_PART,
    _pmath_compare_exprsym,
    hash_expression_cached,
    destroy_expr_tree,//destroy_part_expression,//
    _pmath_expr_equal,
    _pmath_expr_write);

  _pmath_init_special_type(
    PMATH_TYPE_SHIFT_CUSTOM_EXPRESSION,
    _pmath_compare_exprsym,
    hash_expression_cached,
    destroy_expr_tree,//destroy_custom_expression,//
    _pmath_expr_equal,
    _pmath_expr_write);

  return TRUE;

//  expr_cache_clear();
}

PMATH_PRIVATE void _pmath_expressions_done(void) {
#ifdef PMATH_DEBUG_LOG
  {
    intptr_t hits   = pmath_atomic_read_aquire(&expr_cache_hits);
    intptr_t misses = pmath_atomic_read_aquire(&expr_cache_misses);

    pmath_debug_print("expr cache hit rate:             %f (%d of %d)\n",
    hits / (double)(hits + misses),
    (int) hits,
    (int)(hits + misses));
  }
#endif

  expr_cache_clear();
//  pmath_debug_print("%"PRIdPTR" changes to expressions\n", global_change_time);
}
