#ifndef __PMATH_UTIL__HELPERS_H__
#define __PMATH_UTIL__HELPERS_H__

#include <pmath-core/symbols.h>
#include <pmath-core/expressions.h>
#include <stdarg.h>

/**\defgroup helpers Object Utility Functions
   \brief Utility functuions for pMath Objects and Expressions.

   Here are some utility functions that simplify access to Expressions (or pMath
   Objects in general), but do not realy fit one of these topics.

  @{
 */

/**\brief A stack walker function.

   The return value specifies, whether the walk on the stack go on.
 */
typedef pmath_bool_t (*pmath_stack_walker_t)(pmath_t head, void *closure);

/*============================================================================*/

/**\brief Check if an object is an expression with a specified head.
   \param obj    A pMath object. It wont be freed.
   \param head   A pMath symbol. It wont be freed.
   \return TRUE iff obj is an expression with the given \a head symbol.
 */
PMATH_API 
pmath_bool_t pmath_is_expr_of(
  pmath_t        obj,
  pmath_symbol_t head);
  
/**\brief Check if an object is an expression with a specified head and length.
   \param obj    A pMath object. It wont be freed.
   \param head   A pMath symbol. It wont be freed.
   \param length The requested expression length.
   \return TRUE iff obj is an expression with the given \a length and \a head 
           symbol.
 */
PMATH_API 
pmath_bool_t pmath_is_expr_of_len(
  pmath_t        obj,
  pmath_symbol_t head,
  size_t         length);

/*============================================================================*/

/**\brief Generate a List of objects with a format string.
   \relates pmath_t
   \param format   A string that specifies the tuple's item's type.
   \param args     A va_list - variable argument list.
   
   \see pmath_build_value
 */
PMATH_API
PMATH_ATTRIBUTE_USE_RESULT
PMATH_ATTRIBUTE_NONNULL(1)
pmath_t pmath_build_value_v(const char *format, va_list args);

/**\brief Generate a List of objects with a format string.
   \relates pmath_t
   \param format  A string that specifies the tuple's item's type. See below.
   \param ...     The tuple/list items
   
   The format string characters specify the item's type:
   - <tt>b</tt> [int] Converts a C int to True or False.
   
   - <tt>i</tt> [int]
   - <tt>l</tt> [long int]
   - <tt>k</tt> [long long int]
   - <tt>n</tt> [ssize_t]
   
   - <tt>I</tt> [unsigned int]
   - <tt>L</tt> [unsigned long int]
   - <tt>K</tt> [unsigned long long int]
   - <tt>N</tt> [size_t]
   
   - <tt>f</tt> [double] NaN's and Infinity are converted to the symbols 
                         Undefined and +/-Infinity respectively.
   
   - <tt>o</tt> [pmath_t] A pMath object, the reference is stolen.
   
   - <tt>c</tt> [int] Convert a C int representing a (unicode) character to
                      a string of length 1.
   
   - <tt>s</tt> [char*] Converts a zero-terminated C string to a pMath string 
                        using Latin-1 encoding.
   
   - <tt>s#</tt> [char*,int] Takes a char buffer and a length to build a pMath
                             string of that length using Latin-1 encoding.
   
   - <tt>z</tt> [char*] Takes a zero-terminated C string and converts it to a 
                        symbol using pmath_symbol_find().
   
   - <tt>u</tt> [char*] Converts a zero-terminated C string to a pMath string 
                        using UTF-8 encoding.
   
   - <tt>u#</tt> [char*,int] Takes a char buffer and a length to build a pMath
                             string of that length using UTF-8 encoding.
   
   - <tt>U</tt> [uint16_t*] Converts a zero-terminated double-byte C string to 
                            a pMath string using UTF-16 encoding. This is 
                            generally useful only where 
                            sizeof(uint16_t) == sizeof(wchar_t), e.g. on Windows 
                            but not on Linux.
   
   - <tt>U#</tt> [uint16_t*,int] Takes a character buffer and a length to build 
                                 a pMath string of that length using UTF-16 
                                 encoding. This is generally useful only on 
                                 platforms with 
                                 sizeof(uint16_t) == sizeof(wchar_t), e.g. on
                                 Windows but not on Linux.
   
   - <tt>C<i>tt</i></tt> [matching the 2 <tt><i>t</i></tt>'s] Build a complex value.
   - <tt>Q<i>tt</i></tt> [matching the 2 <tt><i>t</i></tt>'s] Build a rational value.
   
   - <tt>(<i>items</i>)</tt> [matching <tt><i>items</i></tt>] constructs a sublist of items.
   
   \note When the format string denotes only one object, this object will be
         returned alone. So for a pmath_t x, pmath_build_value("o", x) == x.
         \n
         If you want to return a list in any case, use "(...)":
         "i" gives an integer, "ii" and "(ii)" a list of two integers and "(i)"
         a list of one integer.
 */
PMATH_API
PMATH_ATTRIBUTE_USE_RESULT
PMATH_ATTRIBUTE_NONNULL(1)
pmath_t pmath_build_value(const char *format, ...);

/*============================================================================*/

/**\brief Get the currently evaluated function.
   \return The head of the expression that is currently evaluated (in the
           calling thread). You have to destroy it.
 */
PMATH_API 
PMATH_ATTRIBUTE_USE_RESULT
pmath_t pmath_current_head(void);

/**\brief Walk up the current thread's and its parents' stack.
   \param walker A callback function.
   \param closure A pointer that will be provided to walker as the second 
                  argument.
 */
PMATH_API 
void pmath_walk_stack(pmath_stack_walker_t walker, void *closure);

/**\brief Walk up the current thread's and its parents' stack.
   \param walker  A callback function. It should return TRUE when walking may 
                  continue. It may not destroy its arguments.
   \param closure A pointer that will be provided to walker as the last argument.
 */
PMATH_API 
void pmath_walk_stack_2(
  pmath_bool_t (*walker)(pmath_t head, pmath_t debug_info, void *closure), 
  void *closure);


/*============================================================================*/

/**\brief Execute an expression and change $History and $Line appropriately
   \ingroup frontend
   \param input   The input expression. It will be freed.
   \param aborted Optional address of a flag that will be set iff the evaluation
                  was aborted by pmath_abort_please()
   \return The return value is the same as pmath_evaluate(input)
   
   This function also calls pmath_collect_temporary_symbols before evaluation.
 */
PMATH_API 
PMATH_ATTRIBUTE_USE_RESULT
pmath_t pmath_session_execute(pmath_t input, pmath_bool_t *aborted);

/**\brief Saves some global state when an interactive dialog session starts.
   \ingroup frontend
   \return A pMath object to be given to pmath_session_end()
   
   This function should be used by a frontend when implementing the Dialog()
   function.
 */
PMATH_API 
PMATH_ATTRIBUTE_USE_RESULT
pmath_t pmath_session_start(void);

/**\brief Restore some global state when an interactive dialog session ends.
   \ingroup frontend
   \param old_state The object returned by pmath_session_start(). It will be 
                    freed.
   
   This function should be used by a frontend when implementing the Dialog()
   function.
 */
PMATH_API 
void pmath_session_end(pmath_t old_state);

/** @} */

#endif /* __PMATH_UTIL__HELPERS_H__ */
