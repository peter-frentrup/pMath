#ifndef __PMATH_BUILTINS__CONTROL__FLOW_PRIVATE_H__
#define __PMATH_BUILTINS__CONTROL__FLOW_PRIVATE_H__

#ifndef BUILDING_PMATH
  #error This header file is not part of the public pMath API
#endif

/* This header exports all definitions of the sources in
   src/pmath-builtins/control/flow/
 */

PMATH_PRIVATE pmath_bool_t _pmath_run(pmath_t *in_out); // return = stop? (break/return/...)

PMATH_PRIVATE 
PMATH_ATTRIBUTE_NONNULL(2,3)
void _pmath_iterate(
  pmath_t         iter, // will be freed
  void          (*init)(size_t,pmath_symbol_t,void*), // symbol must not be freed
  pmath_bool_t  (*next)(void*),
  void           *data);
/* iter = n
          i->n
          i->a..b
          i->a..b..d
          i->{x1,x2,...}
   
   messages:
     General::iter
     General::iterb
   
   _count_ is determined by iter.
   init(_count_, data) is called iff there is no error (no message).
   next(data) is called at most _count_ times. The return value specifies 
   whether to go on.
 */
 
#endif // __PMATH_BUILTINS__CONTROL__FLOW_PRIVATE_H__
