#ifndef __PMATH_UTIL__DYNAMIC_PRIVATE_H__
#define __PMATH_UTIL__DYNAMIC_PRIVATE_H__

#ifndef BUILDING_PMATH
#  error This header file is not part of the public pMath API
#endif

#include <pmath-core/symbols.h>

/** Implementation of a one-shot observer pattern.
    
    Each symbol in pMath can be observed for changes to its definition.
    Unlike the in classical observer pattern, observation is one-shot: every
    interrested observer must re-register (re-bind) itself for the symbol
    upon receiving a change notification if it wants to receive further changes.
    Update notifications are performed during _pmath_dynamic_update(symbol) by 
    evaluating Internal`DynamicUpdate(id1, id2, ...).
    
    Note that the observer might miss changes in between notification and re-binding.
    But this behaviour is an explicit design goal: Let the dynamic object be some 
    text label dynamically displaying a symbol value. There is no point in updating 
    the display 100s of times per second even when the variable changes that quickly.
    
    Re-binding is done automatically during  Internal`DynamicEvaluate(expr, id).
    This is done by temporarily setting <c>current_thread->current_dynamic_id</c> to id
    (and temporarily incrementing _pmath_dynamic_trackers as a speed optimization)
    while evaluating expr.
    Now every symbol that gets evaluated checks for  current_thread->current_dynamic_id
    and binds to that if it is non-zero (the symbol first checks the global
    _pmath_dynamic_trackers flag to speed up the common case when there is no 
    current_dynamic_id). 
    That is done by <c>_pmath_symbol_track_dynamic(symbol, current_thread->current_dynamic_id)</c>
    everywhere where the symbol's definition (value, rules, attributes) are returned.
    _pmath_symbol_track_dynamic() calls _pmath_dynamic_bind() if the symbol is not yet bound 
    to id yet but may do so (e.g. local symbols created during the current Internal`DynamicEvaluate
    may not bind, their ignore_dynamic_id is set to current_dynamic_id during creation).
    
    The helper function Internal`DynamicEvaluateMultiple(Dynamic(expr, ...), id)
    helps in that process by examining given options of Dynamic and calling
    Internal`DynamicEvaluate(...) appropriately.
    
    Dynamic objects are managed by the front end. It does so by calling
    Internal`DynamicEvaluateMultiple(Dynamic(expr, ...), id) when a dynamic object 
    with given id is interrested in the value of expr and changes to that value.
    The front end implements the notification function Internal`DynamicUpdate(id1, id2, ...) 
    to handle value changes.
    Furthermore, whenever a dynamic object is disposed, the front end executes
    Internal`DynamicRemove(id1, id2, ...) to free internal resources (such as first evaluation 
    time for each id and all their symbol bindings).
 */

/** The number of dynamic objects that are currently re-evaluating the value they are 
    interrested in.
    
    This variable is a speed-optimization. When no dynamic
 */
PMATH_PRIVATE extern pmath_atomic_t _pmath_dynamic_trackers;

/** Bind a symbol to a dynamic object id.

    Binding means, that the dynamic object represented by \arg id is interested in the 
    next modification of the value/definition of \arg symbol.
    Calling this function with id==0 is a no-op.
    
    If \arg id was never bound to anything before, its first-evlauation-time is
    set to the current time.
    
    \param symbol  Wont be freed.
    \param id      An non-zerlo opaque dynamic object identifier.
 */
PMATH_PRIVATE void _pmath_dynamic_bind(pmath_symbol_t symbol, intptr_t id);

/** Remove all symbol bindings of a dynamic object and reset/clear its first-evlauation time.
    
    This should be called whenever a dynamic object is disposed.
 */
PMATH_PRIVATE pmath_bool_t _pmath_dynamic_remove(intptr_t id);

/** Inform all bound dynamic objects of a change to a symbol and unbind them all.
    
    If a dynamic object wants further notifications about changes to this particular 
    \arg symbol, they need to bind to it again.
    
    Notification is performed by calling <tt>Internal`DynamicUpdated(id1, id2, id3, ...)</tt>
    with all previously bound dynamic object ids as arguments.
    The front-end is supposed to implement that function.
    
    \param symbol  Wont be freed.
 */
PMATH_PRIVATE void _pmath_dynamic_update(pmath_symbol_t symbol);

/** Get the first evaluation time of a dynamic object.
    \param id  An opaque dynamic object identifier.
 */
PMATH_PRIVATE double _pmath_dynamic_first_eval(intptr_t id);

PMATH_PRIVATE pmath_bool_t _pmath_dynamic_init(void);
PMATH_PRIVATE void         _pmath_dynamic_done(void);

#endif /* __PMATH_UTIL__DYNAMIC_PRIVATE_H__ */
