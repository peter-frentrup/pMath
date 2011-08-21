#ifndef __PMATH_CORE__CUSTOM_H__
#define __PMATH_CORE__CUSTOM_H__

#include <pmath-core/objects-inline.h>

/**\defgroup custom Custom Objects
   \brief Encapsulate arbitrary data in pMath objects.

   Custom Objects consist of a pointer and a destructor. The destructor is
   called (with the pointer as its argument) when the custom object's reference
   pointer yields zero.

   Custom Objects are not evaluateable. This means evaluation of such an object
   returns PMATH_NULL. But you can store custom objects in symbols (directly with
   pmath_symbol_set_value()).

   A symbol that holds a custom object remains unevaluated. It can also contain
   function definitions. But those must be set \em after setting the value with
   pmath_symbol_set_value(my_symbol, my_custom_object):

   Example: You want to store a custom object and a function definition in a
   symbol (my_symbol/: answer(my_symbol):= 42).
   \code
pmath_custom_t my_custom_object = pmath_custom_new(my_data, my_destructor);
pmath_symbol_set_value(my_symbol, my_custom_object);

pmath_unref(pmath_evaluate(
  pmath_parse_string("`1`/: answer(`1`):= 42", 1, pmath_ref(my_symbol))));
   \endcode

  @{
 */

/**\class pmath_custom_t
   \extends pmath_t
   \brief The Custom Object class.

   Since it is derived from \ref pmath_t, you can provide it to any
   function that accepts a pmath_t.
 */
typedef pmath_t pmath_custom_t;

/**\brief Create a custom object.
   \memberof pmath_custom_t
   \param data An arbitrary pointer.
   \param destructor A function that will be called on object destruction to
          enable freeing of \a data.
   \return A custom object or PMATH_NULL on failure (in that case, \a destructor(data)
           is called immediately).
 */
PMATH_API 
PMATH_ATTRIBUTE_USE_RESULT
pmath_custom_t pmath_custom_new(
  void              *data,
  pmath_callback_t   destructor);

/**\brief Get a custom object's data member.
   \memberof pmath_custom_t
   \param custom A custom object.
   \return The objects data member or PMATH_NULL if \a custom is PMATH_NULL.

   Note that you cannot assume anything about the content of this pointer unless
   you know its destructor (check \ref pmath_custom_has_destructor ).

   All access to *data must be threadsafe/synchronized. By convention, you are
   the only one who moves custom objects with your destructor around (other
   modules should not handle custom objects whose destructor they do not know).
   And normally, each of your custom objects is stored in one symbol. So
   synchronization can be done with pmath_symbol_synchronized(). If one of these
   conditions is not met and a custom object could be accessd from multiple
   threads (See \ref threads), you must also store a synchronization object
   (e.g. symbol or threadlock) in the \a data member und use this.
 */
PMATH_API 
void *pmath_custom_get_data(pmath_custom_t custom);

/**\brief Check for a custom object's data type.
   \memberof pmath_custom_t
   \param custom A custom object.
   \param dtor A callback function.
   \return TRUE if the object's destructor is \a dtor.
 */
PMATH_API 
pmath_bool_t pmath_custom_has_destructor(
  pmath_custom_t    custom, 
  pmath_callback_t  dtor);

/** @} */

#endif /* __PMATH_CORE__CUSTOM_H__ */
