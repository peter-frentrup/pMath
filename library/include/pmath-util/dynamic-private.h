#ifndef __PMATH_UTIL__DYNAMIC_PRIVATE_H__
#define __PMATH_UTIL__DYNAMIC_PRIVATE_H__

#ifndef BUILDING_PMATH
#  error This header file is not part of the public pMath API
#endif

#include <pmath-core/symbols.h>

/** Implementation of a one-shot observer pattern.
    
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
