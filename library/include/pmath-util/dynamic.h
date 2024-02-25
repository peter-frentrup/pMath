#ifndef PMATH__UTIL__DYNAMIC_H__INCLUDED
#define PMATH__UTIL__DYNAMIC_H__INCLUDED


#include <pmath-types.h>

/** \defgroup dynamics Dynamic Evaluation
    \brief Implementation of a one-shot observer pattern.

    Each symbol in pMath can be observed for changes to its definition.
    Unlike the in classical observer pattern, observation is one-shot: every interrested
    observer must re-register (re-bind) itself for the symbol upon receiving a change 
    notification if it wants to receive further changes.
    Update notifications are performed with the internal function 
    _pmath_dynamic_update(symbol) by calling Internal`DynamicUpdate(id1, id2, ...), 
    which is implemented by the front-end.
    
    Note that the observer might miss changes in between notification and re-binding.
    But this behaviour is an explicit design goal: Let the dynamic object be some 
    text label dynamically displaying a symbol value. There is no point in updating 
    the display 100s of times per second even when the variable changes that quickly.
    
    Re-binding is done automatically during  Internal`DynamicEvaluate(expr, id).
    This is done by temporarily setting pmath_dynamic_get_current_tracker_id() to id
    (and temporarily incrementing the internal variable _pmath_dynamic_trackers as a 
    speed optimization) while evaluating expr.
    Now every symbol that gets evaluated checks for pmath_dynamic_get_current_tracker_id()
    and binds to that if it is non-zero (the symbol first checks the internal
    _pmath_dynamic_trackers flag to speed up the common case when there is no tracker). 
    The binding is done by the internal function 
    <c>_pmath_symbol_track_dynamic(symbol, pmath_dynamic_get_current_tracker_id())</c>
    everywhere where the symbol's definitions (value, rules, attributes) are returned.
    _pmath_symbol_track_dynamic() calls the internal function _pmath_dynamic_bind() 
    if the symbol is not yet bound to id but may bind to it (e.g. local symbols created during 
    the current Internal`DynamicEvaluate may not bind, their <c>ignore_dynamic_id</c> is set
    to <c>current_dynamic_id</c> during creation).
    
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
  @{
 */

/** Get the currently active dynamic tracker id on this thread.
 * 
 * This is the same as Internal`GetCurrentDynamicID()
 */
PMATH_API intptr_t pmath_dynamic_get_current_tracker_id(void);

/** @}*/

#endif // PMATH__UTIL__DYNAMIC_H__INCLUDED
