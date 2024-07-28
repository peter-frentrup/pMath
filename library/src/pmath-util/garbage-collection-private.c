#include <pmath-util/garbage-collection-private.h>

#include <pmath-core/objects-private.h>
#include <pmath-core/symbols-private.h>

#include <pmath-builtins/all-symbols-private.h>

#include <pmath-util/concurrency/threadmsg.h>
#include <pmath-util/dispatch-tables-private.h>
#include <pmath-util/debug.h>
#include <pmath-util/symbol-values-private.h>


extern pmath_symbol_t pmath_System_List;

#define GC_PASS_BITS   1u
#define GC_PASS_COUNT  (1u << GC_PASS_BITS)
#define GC_PASS_MASK   (GC_PASS_COUNT - 1u)

static uintptr_t gc_pass = 0;
static pmath_bool_t gc_is_running = FALSE;

#ifdef PMATH_DEBUG_LOG

PMATH_PRIVATE
pmath_atomic_t _pmath_debug_current_gc_symbol = PMATH_ATOMIC_STATIC_INIT;

//static enum pmath_visit_result_t gc_visit_check_ref(pmath_t obj, void *dummy);
//static void gc_debug_check_all_refs(void); // (debug) check that all gc_refs are set to the previous pass
#endif


static void gc_symbol_defs_visit(
  pmath_symbol_t              sym, // won't be freed
  enum pmath_visit_result_t (*callback)(pmath_t, void*),
  void                       *closure);
  
static enum pmath_visit_result_t gc_visit_ref(pmath_t obj, void *dummy);
static pmath_bool_t gc_visit_limbo_dispatch_table(_pmath_dispatch_table_new_t *table, void *dummy);
static void gc_init_all_refs(void);

static uintptr_t get_gc_refs(pmath_t obj);
static pmath_bool_t gc_did_see_all_refs(pmath_symbol_t sym);

#define GC_POSSIBLY_ALIVE_VISITED     (gc_pass)
#define GC_POSSIBLY_ALIVE_UNVISITED   (gc_pass | GC_PASS_COUNT)
#define GC_POSSIBLY_DEAD              (gc_pass | (GC_PASS_COUNT + GC_PASS_COUNT))
static size_t gc_mark_possibly_dead_or_alive_unvisited(void); // returns num_possibly_dead

struct _pmath_gc_propagate_life_context_t {
  size_t num_resurrected;
};
static enum pmath_visit_result_t gc_propagate_life_visitor(pmath_t obj, void *_context);
static size_t gc_propagate_life_step(void); // returns num_resurrected (possibly dead => alive unvisited)


static void gc_clear_all_possibly_dead_symbols(size_t num_possibly_dead);
static pmath_bool_t gc_dispatch_table_has_only_alive_symbols(_pmath_dispatch_table_new_t *table, void *dummy); 
static enum pmath_visit_result_t gc_visit_all_alive(pmath_t obj, void *dummy);

PMATH_PRIVATE void _pmath_unsafe_run_gc(void) {
  size_t num_possibly_dead;
#ifdef PMATH_DEBUG_LOG
  double mark_start, propagate_start, clear_start, end;
#endif

  assert(!gc_is_running);
  gc_is_running = TRUE;

  gc_pass = (gc_pass + 1) & GC_PASS_MASK;

#ifdef PMATH_DEBUG_LOG
  mark_start = pmath_tickcount();
#endif

//#ifdef PMATH_DEBUG_LOG
//  gc_debug_check_all_refs();
//#endif
  
  gc_init_all_refs();
  
#ifdef PMATH_DEBUG_LOG
  propagate_start = pmath_tickcount();
#endif
  
  // Temp. symbols seen from alive symbols are also still alive
  num_possibly_dead = gc_mark_possibly_dead_or_alive_unvisited();
  if(num_possibly_dead > 0) {
    size_t num_resurrected = 0;
    do {
      num_resurrected = gc_propagate_life_step();
      num_possibly_dead-= num_resurrected;
    } while(num_resurrected > 0);
  }
  
#ifdef PMATH_DEBUG_LOG
  clear_start = pmath_tickcount();
#endif
  
  gc_clear_all_possibly_dead_symbols(num_possibly_dead);
  _pmath_dispatch_table_filter_limbo(gc_dispatch_table_has_only_alive_symbols, NULL);
  
  gc_is_running = FALSE;

#ifdef PMATH_DEBUG_LOG
  end = pmath_tickcount();

  if(end - mark_start > 1.0) {
    pmath_debug_print("[gc %f + %f + %f = %f secs]\n",
                      propagate_start - mark_start,
                      clear_start - propagate_start,
                      end - clear_start,
                      end - mark_start);
  }
#endif

}



static void gc_symbol_defs_visit(
  pmath_symbol_t              sym, // won't be freed
  enum pmath_visit_result_t (*callback)(pmath_t, void*),
  void                       *closure
) {
  struct _pmath_symbol_rules_t *rules;
  
  _pmath_symbol_value_visit(_pmath_symbol_get_global_value(sym), callback, closure);
  
  rules = _pmath_symbol_get_rules(sym, RULES_READ);
  if(rules)
    _pmath_symbol_rules_visit(rules, callback, closure);
}



static enum pmath_visit_result_t gc_visit_ref(pmath_t obj, void *dummy) {
  if(_pmath_is_gcobj(obj)) {
    struct _pmath_gc_t *gc_obj = (void *)PMATH_AS_PTR(obj);
    uintptr_t gc_refs;

    if((gc_obj->gc_refcount & GC_PASS_MASK) == gc_pass) {
      gc_obj->gc_refcount += GC_PASS_COUNT;
    }
    else
      gc_obj->gc_refcount = gc_pass | GC_PASS_COUNT;
    
    if(pmath_is_symbol(obj))
      return PMATH_VISIT_NORMAL;
    
    gc_refs = gc_obj->gc_refcount >> GC_PASS_BITS;
    
    // one reference is held by caller
    ++gc_refs;

    if(gc_refs == pmath_refcount(obj))
      return PMATH_VISIT_NORMAL;
    else
      return PMATH_VISIT_SKIP;
  }
  return PMATH_VISIT_NORMAL;
}

static pmath_bool_t gc_visit_limbo_dispatch_table(_pmath_dispatch_table_new_t *table, void *dummy) {
  _pmath_symbol_value_visit(
    pmath_ref(PMATH_FROM_PTR((void*)table)),//pmath_ref(PMATH_FROM_PTR(&table->internals.inherited.inherited.inherited)), 
    gc_visit_ref, NULL);
  return TRUE;
}

static void gc_init_all_refs(void) {
  pmath_symbol_t sym;
  
  sym = pmath_ref(pmath_System_List);
  do {
    if(pmath_symbol_get_attributes(sym) & PMATH_SYMBOL_ATTRIBUTE_TEMPORARY) {
      gc_symbol_defs_visit(sym, gc_visit_ref, NULL);
    }
    // TODO(?): else reset gc_refcount in case a symbol concurrently becomes Temporary?

    sym = pmath_symbol_iter_next(sym);
  } while(!pmath_is_null(sym) && !pmath_same(sym, pmath_System_List));
  pmath_unref(sym);

  _pmath_dispatch_table_filter_limbo(gc_visit_limbo_dispatch_table, NULL);
}



static uintptr_t get_gc_refs(pmath_t obj) {
  struct _pmath_gc_t *gc_obj;

  assert(_pmath_is_gcobj(obj));

  gc_obj = (void *)PMATH_AS_PTR(obj);

  if((gc_obj->gc_refcount & GC_PASS_MASK) == gc_pass) {
    return gc_obj->gc_refcount >> GC_PASS_BITS;
  }

  return 0;
}

static pmath_bool_t gc_did_see_all_refs(pmath_symbol_t sym) { // sym won't be freed
  uintptr_t gc_refs = get_gc_refs(sym);
  uintptr_t actual_refs = (uintptr_t)pmath_refcount(sym);

  // one reference is held by sym
  ++gc_refs;

  return gc_refs == actual_refs;
}

static size_t gc_mark_possibly_dead_or_alive_unvisited(void) { // returns num_possibly_dead
  size_t num_possibly_dead = 0;
  pmath_symbol_t sym;
  
  sym = pmath_ref(pmath_System_List);
  do {
    if(pmath_symbol_get_attributes(sym) & PMATH_SYMBOL_ATTRIBUTE_TEMPORARY) {
      struct _pmath_gc_t *gc_obj;
      gc_obj = (void *)PMATH_AS_PTR(sym);
      if(gc_did_see_all_refs(sym)) { // possible dead cycle
        gc_obj->gc_refcount = GC_POSSIBLY_DEAD;
        ++num_possibly_dead;
      }
      else { // seen from outside => definitely alive
        gc_obj->gc_refcount = GC_POSSIBLY_ALIVE_UNVISITED;
      }
    }
    sym = pmath_symbol_iter_next(sym);
  } while(!pmath_is_null(sym) && !pmath_same(sym, pmath_System_List));
  pmath_unref(sym);
  
  return num_possibly_dead;
}

static enum pmath_visit_result_t gc_propagate_life_visitor(pmath_t obj, void *_context) {
  struct _pmath_gc_propagate_life_context_t *context = _context;
  
  if(_pmath_is_gcobj(obj)) {
    struct _pmath_gc_t *gc_obj = (void *)PMATH_AS_PTR(obj);
    if(gc_obj->gc_refcount == GC_POSSIBLY_ALIVE_VISITED)
      return PMATH_VISIT_SKIP;
    
    if(pmath_is_symbol(obj)) {
      if(gc_obj->gc_refcount == GC_POSSIBLY_DEAD && (pmath_symbol_get_attributes(obj) & PMATH_SYMBOL_ATTRIBUTE_TEMPORARY)) {
        gc_obj->gc_refcount = GC_POSSIBLY_ALIVE_UNVISITED;
        context->num_resurrected++;
      }
    }
    else { // general expr has arbitrary gc_refcount, but > 0 if we alread saw it gc_init_all_refs()
      gc_obj->gc_refcount = GC_POSSIBLY_ALIVE_VISITED;
    }
  }
  
  return PMATH_VISIT_NORMAL;
}

static size_t gc_propagate_life_step(void) { // returns num_resurrected (possibly dead => alive unvisited)
  pmath_symbol_t sym; 
  struct _pmath_gc_propagate_life_context_t context;
  context.num_resurrected = 0;
  
  sym = pmath_ref(pmath_System_List);
  do {
    if(pmath_symbol_get_attributes(sym) & PMATH_SYMBOL_ATTRIBUTE_TEMPORARY) {
      struct _pmath_gc_t *gc_obj = (void *)PMATH_AS_PTR(sym);
      if(gc_obj->gc_refcount == GC_POSSIBLY_ALIVE_VISITED) {
        // do nothing
      }
      else if(gc_obj->gc_refcount == GC_POSSIBLY_DEAD) {
        // do nothing
      }
      else /*if(gc_obj->gc_refcount == GC_POSSIBLY_ALIVE_UNVISITED)*/ {
        gc_obj->gc_refcount = GC_POSSIBLY_ALIVE_VISITED;
        gc_symbol_defs_visit(sym, gc_propagate_life_visitor, &context);
      }
    }
    sym = pmath_symbol_iter_next(sym);
  } while(!pmath_is_null(sym) && !pmath_same(sym, pmath_System_List));
  pmath_unref(sym);
  
  return context.num_resurrected;
}

static void gc_clear_all_possibly_dead_symbols(size_t num_possibly_dead) {
  pmath_symbol_t sym; 
  
  if(num_possibly_dead == 0)
    return;
  
  sym = pmath_ref(pmath_System_List);
  do {
    if(pmath_symbol_get_attributes(sym) & PMATH_SYMBOL_ATTRIBUTE_TEMPORARY) {
      struct _pmath_gc_t *gc_obj;
      gc_obj = (void *)PMATH_AS_PTR(sym);
      if(gc_obj->gc_refcount == GC_POSSIBLY_DEAD) {
        struct _pmath_symbol_rules_t *rules;
        
#      ifdef PMATH_DEBUG_LOG
        pmath_atomic_write_release(&_pmath_debug_current_gc_symbol, (intptr_t)PMATH_AS_PTR(sym));
#      endif
        
        pmath_symbol_set_attributes(sym, PMATH_SYMBOL_ATTRIBUTE_TEMPORARY);
        _pmath_symbol_set_global_value(sym, PMATH_UNDEFINED);
        
        rules = _pmath_symbol_get_rules(sym, RULES_READ);
        if(rules) {
          rules = _pmath_symbol_get_rules(sym, RULES_WRITEOPTIONS);
          if(rules)
            _pmath_symbol_rules_clear(rules);
        }

        pmath_register_code(sym, NULL, PMATH_CODE_USAGE_DOWNCALL);
        pmath_register_code(sym, NULL, PMATH_CODE_USAGE_UPCALL);
        pmath_register_code(sym, NULL, PMATH_CODE_USAGE_SUBCALL);

#      ifdef PMATH_DEBUG_LOG
        pmath_atomic_write_release(&_pmath_debug_current_gc_symbol, 0);
#      endif
        
        if(--num_possibly_dead == 0)
          break;
      }
    }
    
    sym = pmath_symbol_iter_next(sym);
  } while(!pmath_is_null(sym) && !pmath_same(sym, pmath_System_List));
  pmath_unref(sym);
}

static pmath_bool_t gc_dispatch_table_has_only_alive_symbols(_pmath_dispatch_table_new_t *table, void *dummy) {
  return PMATH_VISIT_ABORT != _pmath_symbol_value_visit(
    pmath_ref(PMATH_FROM_PTR((void*)table)), 
    gc_visit_all_alive, NULL);
}

static enum pmath_visit_result_t gc_visit_all_alive(pmath_t obj, void *dummy) {
  if(pmath_is_symbol(obj)) {
    if(pmath_symbol_get_attributes(obj) & PMATH_SYMBOL_ATTRIBUTE_TEMPORARY) {
      struct _pmath_gc_t *gc_obj = (void *)PMATH_AS_PTR(obj);
      if(gc_obj->gc_refcount == GC_POSSIBLY_DEAD)
        return PMATH_VISIT_ABORT;
    }
  }
  
  return PMATH_VISIT_NORMAL;
}

//#ifdef PMATH_DEBUG_LOG
//static enum pmath_visit_result_t gc_visit_check_ref(pmath_t obj, void *dummy) {
//  if(pmath_is_symbol(obj) || pmath_is_expr(obj)) {
//    struct _pmath_gc_t *gc_obj = (void *)PMATH_AS_PTR(obj);
//
//    if( 0 != gc_obj->gc_refcount &&
//        (gc_obj->gc_refcount & GC_PASS_MASK) != ((gc_pass - 1) & GC_PASS_MASK))
//    {
//      pmath_debug_print("[not from prev. pass: %"PRIxPTR", ", gc_obj->gc_refcount);
//      pmath_debug_print_object("", obj , "]\n");
//
//      return PMATH_VISIT_ABORT;
//    }
//  }
//  return PMATH_VISIT_NORMAL;
//}
//static void gc_debug_check_all_refs(void) { // (debug) check that all gc_refs are set to the previous pass
//  pmath_symbol_t sym = pmath_ref(pmath_System_List);
//  do {
//    if(pmath_symbol_get_attributes(sym) & PMATH_SYMBOL_ATTRIBUTE_TEMPORARY) {
//      struct _pmath_symbol_rules_t *rules;
//
//      _pmath_symbol_value_visit(
//        _pmath_symbol_get_global_value(sym),
//        gc_visit_check_ref,
//        NULL);
//
//      rules = _pmath_symbol_get_rules(sym, RULES_READ);
//
//      if(rules)
//        _pmath_symbol_rules_visit(rules, gc_visit_check_ref, NULL);
//    }
//
//    sym = pmath_symbol_iter_next(sym);
//  } while(!pmath_is_null(sym) && !pmath_same(sym, pmath_System_List));
//  pmath_unref(sym);
//}
//#endif
