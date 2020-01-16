#include <pmath-util/garbage-collection-private.h>

#include <pmath-core/objects-private.h>
#include <pmath-core/symbols-private.h>

#include <pmath-builtins/all-symbols-private.h>

#include <pmath-util/concurrency/threadmsg.h>
#include <pmath-util/debug.h>
#include <pmath-util/symbol-values-private.h>


#define GC_PASS_BITS   1
#define GC_PASS_COUNT  (1 << GC_PASS_BITS)
#define GC_PASS_MASK   (GC_PASS_COUNT-1)

static uintptr_t gc_pass = 0;
static pmath_bool_t gc_is_running = FALSE;

#ifdef PMATH_DEBUG_LOG

PMATH_PRIVATE
pmath_atomic_t _pmath_debug_current_gc_symbol = PMATH_ATOMIC_STATIC_INIT;

#endif

//#ifdef PMATH_DEBUG_LOG
//static pmath_bool_t gc_visit_check_ref(pmath_t obj, void *dummy) {
//  if(pmath_is_symbol(obj) || pmath_is_expr(obj)) {
//    struct _pmath_gc_t *gc_obj = (void *)PMATH_AS_PTR(obj);
//
//    if( 0 != gc_obj->gc_refcount &&
//        (gc_obj->gc_refcount & GC_PASS_MASK) != ((gc_pass - 1) & GC_PASS_MASK))
//    {
//      pmath_debug_print("[not from prev. pass: %"PRIxPTR", ", gc_obj->gc_refcount);
//      pmath_debug_print_object("", obj , "]\n");
//
//      return FALSE;
//    }
//  }
//  return TRUE;
//}
//#endif

static pmath_bool_t gc_visit_ref(pmath_t obj, void *dummy) {
  if(pmath_is_symbol(obj) || pmath_is_expr(obj)) {
    struct _pmath_gc_t *gc_obj = (void *)PMATH_AS_PTR(obj);

    if((gc_obj->gc_refcount & GC_PASS_MASK) == gc_pass) {
      gc_obj->gc_refcount += GC_PASS_COUNT;
    }
    else
      gc_obj->gc_refcount = gc_pass | GC_PASS_COUNT;
  }
  return TRUE;
}

static uintptr_t get_gc_refs(pmath_t obj) {
  struct _pmath_gc_t *gc_obj;

  assert(pmath_is_expr(obj) || pmath_is_symbol(obj));

  gc_obj = (void *)PMATH_AS_PTR(obj);

  if((gc_obj->gc_refcount & GC_PASS_MASK) == gc_pass) {
    return gc_obj->gc_refcount >> GC_PASS_BITS;
  }

  return 0;
}

static pmath_bool_t gc_all_expr_visited(pmath_t obj, void *dummy) {
  if(pmath_is_expr(obj)) {
    uintptr_t gc_refs = get_gc_refs(obj);

    // one reference is held by caller
    ++gc_refs;

    return gc_refs == (uintptr_t)pmath_refcount(obj);
  }

  return TRUE;
}


PMATH_PRIVATE void _pmath_unsafe_run_gc(void) {
  pmath_symbol_t sym;
#ifdef PMATH_DEBUG_LOG
  double mark_start, clear_start, end;
#endif

  assert(!gc_is_running);
  gc_is_running = TRUE;

  gc_pass = (gc_pass + 1) & GC_PASS_MASK;

#ifdef PMATH_DEBUG_LOG
  mark_start = pmath_tickcount();
#endif

//#ifdef PMATH_DEBUG_LOG
//  { // (debug) check that all gc_refs are set to the previous pass
//    sym = pmath_ref(PMATH_SYMBOL_LIST);
//    do {
//      if(pmath_symbol_get_attributes(sym) & PMATH_SYMBOL_ATTRIBUTE_TEMPORARY) {
//        struct _pmath_symbol_rules_t *rules;
//
//        _pmath_symbol_value_visit(
//          _pmath_symbol_get_global_value(sym),
//          gc_visit_check_ref,
//          NULL);
//
//        rules = _pmath_symbol_get_rules(sym, RULES_READ);
//
//        if(rules)
//          _pmath_symbol_rules_visit(rules, gc_visit_check_ref, NULL);
//      }
//
//      sym = pmath_symbol_iter_next(sym);
//    } while(!pmath_is_null(sym) && !pmath_same(sym, PMATH_SYMBOL_LIST));
//    pmath_unref(sym);
//  }
//#endif

  // mark/reference all temp. symbol values
  sym = pmath_ref(PMATH_SYMBOL_LIST);
  do {
    if(pmath_symbol_get_attributes(sym) & PMATH_SYMBOL_ATTRIBUTE_TEMPORARY) {
      struct _pmath_symbol_rules_t *rules;

      _pmath_symbol_value_visit(
        _pmath_symbol_get_global_value(sym),
        gc_visit_ref,
        NULL);

      rules = _pmath_symbol_get_rules(sym, RULES_READ);

      if(rules)
        _pmath_symbol_rules_visit(rules, gc_visit_ref, NULL);
    }

    sym = pmath_symbol_iter_next(sym);
  } while(!pmath_is_null(sym) && !pmath_same(sym, PMATH_SYMBOL_LIST));
  pmath_unref(sym);

#ifdef PMATH_DEBUG_LOG
  clear_start = pmath_tickcount();
#endif

  // clear all temp. symbols that are only referenced by temp. symbols.
  sym = pmath_ref(PMATH_SYMBOL_LIST);
  do {
    if(pmath_symbol_get_attributes(sym) & PMATH_SYMBOL_ATTRIBUTE_TEMPORARY) {
      uintptr_t gc_refs = get_gc_refs(sym);
      uintptr_t actual_refs = (uintptr_t)pmath_refcount(sym);

      // one reference is held by sym
      ++gc_refs;

      if(gc_refs < actual_refs && actual_refs <= gc_refs + 3) {
        if(_pmath_have_code(sym, PMATH_CODE_USAGE_DOWNCALL))
          ++gc_refs;

        if(_pmath_have_code(sym, PMATH_CODE_USAGE_UPCALL))
          ++gc_refs;

        if(_pmath_have_code(sym, PMATH_CODE_USAGE_SUBCALL))
          ++gc_refs;
      }

      if(gc_refs == actual_refs) {
        pmath_bool_t all_visited;

        all_visited = _pmath_symbol_value_visit(
                        _pmath_symbol_get_global_value(sym),
                        gc_all_expr_visited,
                        NULL);

        if(all_visited) {
          struct _pmath_symbol_rules_t *rules;
          rules = _pmath_symbol_get_rules(sym, RULES_READ);

          all_visited = _pmath_symbol_rules_visit(rules, gc_all_expr_visited, NULL);

          /* all_visited: the whole symbol value (expr tree) is only referenced
             by temp. symbols and so was visited by the gc in the previous loop.
           */
          if(all_visited) {
#          ifdef PMATH_DEBUG_LOG
            pmath_atomic_write_release(&_pmath_debug_current_gc_symbol, (intptr_t)PMATH_AS_PTR(sym));
#          endif
            pmath_symbol_set_attributes(sym, PMATH_SYMBOL_ATTRIBUTE_TEMPORARY);
            _pmath_symbol_set_global_value(sym, PMATH_UNDEFINED);

            if(rules) {
              rules = _pmath_symbol_get_rules(sym, RULES_WRITEOPTIONS);
              if(rules)
                _pmath_symbol_rules_clear(rules);
            }

            pmath_register_code(sym, NULL, PMATH_CODE_USAGE_DOWNCALL);
            pmath_register_code(sym, NULL, PMATH_CODE_USAGE_UPCALL);
            pmath_register_code(sym, NULL, PMATH_CODE_USAGE_SUBCALL);

#          ifdef PMATH_DEBUG_LOG
            pmath_atomic_write_release(&_pmath_debug_current_gc_symbol, 0);
#          endif
          }
        }
      }
    }

    sym = pmath_symbol_iter_next(sym);
  } while(!pmath_is_null(sym) && !pmath_same(sym, PMATH_SYMBOL_LIST));
  pmath_unref(sym);

  gc_is_running = FALSE;

#ifdef PMATH_DEBUG_LOG
  end = pmath_tickcount();

  if(end - mark_start > 1.0) {
    pmath_debug_print("[gc %f + %f = %f secs]\n",
                      clear_start - mark_start,
                      end - clear_start,
                      end - mark_start);
  }
#endif

}
