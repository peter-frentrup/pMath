#ifndef __PATH_UTIL__MEMORY_H__
#define __PATH_UTIL__MEMORY_H__

#include <pmath-config.h>
#include <pmath-types.h>

#include <stdlib.h>


/**\defgroup memory Memory Management
   \brief Memory management for pMath.

   These functions may return NULL. In this case, the current evaluation will
   abort and used cache will be freed to safe memory (the garbage collector is
   invoked and a pMath exception is thrown so pmath_aborting() yields TRUE).

  @{
 */

/**\brief Allocate some amount of memory.
   \param size The number of bytes to be allocated.
   \return A pointer to a block of mamory of at least size bytes or NULL.

   You must free the result with pmath_mem_free() or indirectly via
   pmath_mem_realloc().
 */
PMATH_API
PMATH_ATTRIBUTE_USE_RESULT
void *pmath_mem_alloc(size_t size);

///**\brief Allocate some amount of memory with alignment.
//   \param size The number of bytes to be allocated.
//   \return A pointer to a block of mamory of at least size bytes or NULL.
//
//   You must free the result with pmath_memalign_free() or indirectly via
//   pmath_mem_realloc().
// */
//PMATH_API
//PMATH_ATTRIBUTE_USE_RESULT
//void *pmath_memalign_alloc(size_t alignment, size_t size);

/**\brief Change the size of a memory-chunk.
   \param p NULL or a pointer to a block of old_size bytes allocated with
          pmath_mem_alloc() or pmath_mem_realloc().
   \param new_size The requested new size.
   \return A pointer to a block of at least new_size bytes or NULL.

   If there is not enough memory available or if new_size == 0, NULL is
   returned. Otherwise, the result points to a block of new_size bytes, whose
   first min(old_size,new_size) bytes are copied from the old p. The rest is
   initialized with 0.

   The old pointer p will _allways_be freed. even, if the resizing failed with 
   NULL.

   You must later free the result with pmath_mem_free() or indirectly via
   pmath_mem_realloc().
 */
PMATH_API
PMATH_ATTRIBUTE_USE_RESULT
void *pmath_mem_realloc(
  void   *p,
  size_t  new_size);

/**\brief Change the size of a memory-chunk.
   \param p NULL or a pointer to a block of old_size bytes allocated with
          pmath_mem_alloc() or pmath_mem_realloc().
   \param new_size The requested new size.
   \return A pointer to a block of at least new_size bytes or NULL.

   If there is enough memory, this acts like pmath_mem_realloc(). Otherwise,
   p is \em not freed and NULL is returned.
 */
PMATH_API
PMATH_ATTRIBUTE_USE_RESULT
void *pmath_mem_realloc_no_failfree(
  void   *p,
  size_t  new_size);

/**\brief Free a memory-chunk.
   \param p NULL or a pointer to a block of old_size bytes allocated with
          pmath_mem_alloc() or pmath_mem_realloc().
 */
PMATH_API
void pmath_mem_free(void *p);

///**\brief Free an aligned memory-chunk.
//   \param p NULL or a pointer to a block of old_size bytes allocated with
//          pmath_memalign_alloc().
// */
//PMATH_API
//PMATH_ATTRIBUTE_USE_RESULT
//void *pmath_memalign_free(void *p);

/**\brief Get memory usage information
   \param current Here goes the number of currently allocated bytes.
   \param max here goes the maximum number of allocated bytes so far.
 */
PMATH_API
void pmath_mem_usage(size_t *current, size_t *max);

/** @} */
/*============================================================================*/

#ifdef BUILDING_PMATH
#ifdef PMATH_DEBUG_MEMORY
#include <pmath-util/concurrency/atomic.h>
extern pmath_atomic_t _pmath_debug_global_time;
#endif
PMATH_PRIVATE pmath_bool_t _pmath_memory_manager_init(void);
PMATH_PRIVATE void         _pmath_memory_manager_done(void);
#endif

#endif /* __PATH_UTIL__MEMORY_H__ */
