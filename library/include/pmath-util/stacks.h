#ifndef __PMATH_UTIL__STACKS_H__
#define __PMATH_UTIL__STACKS_H__

#include <pmath-config.h>

/**\defgroup stacks Threadsafe Stacks
   \brief Fast threadsafe stacks in pMath.
   
   pmath_stack_t is a stack abstraction that provides threadsafe push and pop 
   operations. You can push and pop any structure whose first element is a 
   pointer (which you must not touch). 
   
   If your CPU supports it, very fast lockfree operations are used for push and 
   pop.
   
   References
   - Fober, Orlarey, Letz: "Lock-Free Techniques for Concurrent Access to 
     Shared Objects"
     (http://jim.afim-asso.org/jim2002/articles/L17_Fober.pdf)
   
   @{
 */

/**\class pmath_stack_t
   \brief The type of pMath's threadsafe stacks.

   You can create it with pmath_stack_new() and must destroy it with
   pmath_stack_free().
 */
typedef struct _pmath_stack_t  *pmath_stack_t;

/**\brief Create an empty stack.
   \return A new stack or PMATH_NULL.
   
   Note that you cannot push anything onto a PMATH_NULL stack, so check the result.
   Free the result with pmath_stack_free().
 */
PMATH_API 
PMATH_ATTRIBUTE_USE_RESULT
pmath_stack_t pmath_stack_new(void);

/**\brief Destroy a stack.
   \param stack A stack previously created with pmath_stack_new(). 
          May be PMATH_NULL.
   
   You must manually pop and free all items on the stack before calling this
   function, because those items would not be freed automatically.
 */
PMATH_API 
void pmath_stack_free(pmath_stack_t stack);

/**\brief Push an item onto a stack.
   \param stack The stack to where the item should be pushed. Must not be PMATH_NULL.
   \param item The item to be pushed. It must point to a structure whose first
          element is a pointer. Must not be PMATH_NULL.
 */
PMATH_API 
PMATH_ATTRIBUTE_NONNULL(1,2)
void pmath_stack_push(pmath_stack_t stack, void *item);

/**\brief Pop an item from a stack.
   \param stack The stack to pop an item from. Must not be PMATH_NULL.
   \return The top of stack or PMATH_NULL if it is empty.
 */
PMATH_API 
PMATH_ATTRIBUTE_USE_RESULT
PMATH_ATTRIBUTE_NONNULL(1)
void *pmath_stack_pop(pmath_stack_t stack);

/** @} */

#endif /* __PMATH_UTIL__STACKS_H__ */
