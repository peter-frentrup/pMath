#ifndef __PMATH_CORE__OBJECTS_H__
#define __PMATH_CORE__OBJECTS_H__

#include <pmath-config.h>
#include <pmath-types.h>

/**\defgroup objects Objects - the Base of pMath
   \brief The basic class for all pMath objects.

   pMath works on objects. They can be expressions (trees of pMath objects),
   symbols, values, strings or `magic objects` (integer value between 0 and 255).

   \see helpers
  @{
 */

/**\class pmath_t
   \brief The basic type of all pMath objects.

   Note that this is not neccessaryly a pointer to some accessible piece of
   memory.
   Use pmath_instance_of() to determine whether an object is of a specific type
   or set of types.

   You must free unused objects with pmath_unref().
 */
typedef struct _pmath_t *pmath_t;

/**\brief The type or class of a pMath object.

   This is a bitset of the \c PMATH_TYPE_XXX constants:

   - \c PMATH_TYPE_MAGIC: The object is a `magic number`, which 
     is any integer value between 0 and 255 cast to ( \ref pmath_t ). 
     These values have special meanings:
     - \c NULL This is simply `nothing`. It is often used to indicate that there 
       is not enough memory.
     - \c PMATH_UNDEFINED Symbol values are initialized with PMATH_UNDEFINED. 
       This is done to enable saving the value NULL in a symbol.
       
     Any function that returns or operates on a pmath_t may return NULL. 
     Exceptions are allways explicitly stated in the documentation.

   - \c PMATH_TYPE_INTEGER: The object is an integer value. You can cast it 
     to \ref pmath_integer_t, \ref pmath_rational_t and \ref pmath_number_t.

   - \c PMATH_TYPE_QUOTIENT: The object is a reduced quotient of two 
     integer values, where the denominator is never 0 or 1. Because of this, you 
     almost never test for this type, but for PMATH_TYPE_RATIONAL, since the 
     result of an operation on two quotients may be an integer. You can cast 
     quotient objects to pmath_rational_t and thus to pmath_number_t too.

   - \c PMATH_TYPE_RATIONAL: The object is either an integer or a quotient. 
     You can cast it to pmath_rational_t and thus to pmath_number_t too.

   - \c PMATH_TYPE_MP_FLOAT: The object is a floating point number with 
     arbitrary precision. You can cast it to \ref pmath_float_t and 
     pmath_number_t.

   - \c PMATH_TYPE_MACHINE_FLOAT: The object is a floating point number 
     with machine precision. You can cast it to pmath_float_t and 
     pmath_number_t.

   - \c PMATH_TYPE_FLOAT: The object is either PMATH_TYPE_MP_FLOAT or 
     PMATH_TYPE_MACHINE_FLOAT.

   - \c PMATH_TYPE_NUMBER: The object is a numerical value (integer, 
     quotient, floating point number). You can cast it to pmath_number_t.

     Note that complex numbers are stored as expressions and thus are not
     numbers in this sense. Additionally, algebraic and special constants as 
     Sqrt(2) or Pi are not numbers, but symbols or expressiones (e.g. Sqrt(2)).

   - \c PMATH_TYPE_STRING: The object is a string. You can cast it to
     \ref pmath_string_t.

   - \c PMATH_TYPE_SYMBOL: The object is a symbol. You can cast it to
     \ref pmath_symbol_t.

   - \c PMATH_TYPE_EXPRESSION: The object is an expression. You can cast it to
     \ref pmath_expr_t.

   - \c PMATH_TYPE_CUSTOM: The object is a custom object. You can cast it to
     \ref pmath_custom_t.

   - \c PMATH_TYPE_EVALUATABLE: The object is evaluatable. That means, if a
     symbol has this object as its value, the object will be returned.
     Function definition rules and custom objects are an example of
     non-evalutable objects.
 */
typedef int pmath_type_t;

enum { 
  PMATH_TYPE_SHIFT_MACHINE_FLOAT = 0,
  PMATH_TYPE_SHIFT_MP_FLOAT,
  PMATH_TYPE_SHIFT_INTEGER,
  PMATH_TYPE_SHIFT_QUOTIENT,
  PMATH_TYPE_SHIFT_STRING,
  PMATH_TYPE_SHIFT_SYMBOL,
  PMATH_TYPE_SHIFT_EXPRESSION_GENERAL,
  PMATH_TYPE_SHIFT_EXPRESSION_GENERAL_PART,
  PMATH_TYPE_SHIFT_RESERVED_1,
  PMATH_TYPE_SHIFT_CUSTOM,

  PMATH_TYPE_SHIFT_COUNT,
  
  PMATH_TYPE_SHIFT_MAGIC = 31
};

enum { 
  PMATH_TYPE_MAGIC                   = 1 << PMATH_TYPE_SHIFT_MAGIC,
  PMATH_TYPE_INTEGER                 = 1 << PMATH_TYPE_SHIFT_INTEGER,
  PMATH_TYPE_QUOTIENT                = 1 << PMATH_TYPE_SHIFT_QUOTIENT,
  PMATH_TYPE_RATIONAL                = PMATH_TYPE_INTEGER | PMATH_TYPE_QUOTIENT,
  PMATH_TYPE_MP_FLOAT                = 1 << PMATH_TYPE_SHIFT_MP_FLOAT,
  PMATH_TYPE_MACHINE_FLOAT           = 1 << PMATH_TYPE_SHIFT_MACHINE_FLOAT,
  PMATH_TYPE_FLOAT                   = PMATH_TYPE_MP_FLOAT | PMATH_TYPE_MACHINE_FLOAT,
  PMATH_TYPE_NUMBER                  = PMATH_TYPE_FLOAT | PMATH_TYPE_RATIONAL,
  PMATH_TYPE_STRING                  = 1 << PMATH_TYPE_SHIFT_STRING,
  PMATH_TYPE_SYMBOL                  = 1 << PMATH_TYPE_SHIFT_SYMBOL,
  PMATH_TYPE_EXPRESSION_GENERAL      = 1 << PMATH_TYPE_SHIFT_EXPRESSION_GENERAL,
  PMATH_TYPE_EXPRESSION_GENERAL_PART = 1 << PMATH_TYPE_SHIFT_EXPRESSION_GENERAL_PART,
  PMATH_TYPE_EXPRESSION              = PMATH_TYPE_EXPRESSION_GENERAL | PMATH_TYPE_EXPRESSION_GENERAL_PART,
  PMATH_TYPE_CUSTOM                  = 1 << PMATH_TYPE_SHIFT_CUSTOM,
  PMATH_TYPE_EVALUATABLE             = PMATH_TYPE_NUMBER | PMATH_TYPE_STRING | PMATH_TYPE_SYMBOL | PMATH_TYPE_EXPRESSION
};

static const pmath_t PMATH_THREAD_KEY_PARSESYMBOLS     = (pmath_t)252; /* see builtin_boxestoexpression() */
static const pmath_t PMATH_THREAD_KEY_PARSERARGUMENTS  = (pmath_t)253; /* see builtin_boxestoexpression() */
static const pmath_t PMATH_ABORT_EXCEPTION   = (pmath_t)254; /* see builtin_abort(), ... */
static const pmath_t PMATH_THREAD_KEY_SORTFN = (pmath_t)254; /* see builtin_sort() */
static const pmath_t PMATH_UNDEFINED         = (pmath_t)255;

/**\brief Options for pmath_write().

   These options can be one or more of the following:
   - PMATH_WRITE_OPTIONS_FULLEXPR All expressions are written in the form
     f(a, b, ...) without any syntactic sugar.

     Supersedes PMATH_WRITE_OPTIONS_INPUTEXPR.

   - PMATH_WRITE_OPTIONS_FULLSTR Strings are written with quotes and escape
     sequences.

   - PMATH_WRITE_OPTIONS_FULLNAME Names are written with their full namespace
     path.

   - PMATH_WRITE_OPTIONS_INPUTEXPR Expressions are written in a
     form that is valid pMath input.

     Note that this does not automatically imply PMATH_WRITE_OPTIONS_FULLSTR.
 */

typedef int pmath_write_options_t;
enum{
  PMATH_WRITE_OPTIONS_FULLEXPR  = 1 << 0,
  PMATH_WRITE_OPTIONS_FULLSTR   = 1 << 1,
  PMATH_WRITE_OPTIONS_FULLNAME  = 1 << 2,
  PMATH_WRITE_OPTIONS_INPUTEXPR = 1 << 3
};

/**\brief A simple procedure operating on an object.

   It depends on the context whether the argument is destroyed by the procedure
   or not.
 */
typedef void (*pmath_proc_t)(pmath_t);

/**\brief A parameterized procedure operating on an object.

   It depends on the context whether the (second) argument is destroyed by the
   procedure or not.
 */
typedef void (*pmath_param_proc_t)(void*,pmath_t);

/**\brief A simple function operating on an object and returning one.

   It depends on the context whether the argument is destroyed by the function
   or not.
 */
typedef pmath_t (*pmath_func_t)(pmath_t);

/**\brief A hash function for an object.

   If two objects equal, their hash values equal.
 */
typedef unsigned int (*pmath_hash_func_t)(pmath_t);

/**\brief A comparision function for two objects.

   The return value is nonzero, if both objects equal and zero otherwise.
   Note that a pmath_compare_func_t cannot be cast to pmath_equal_func_t,
   because their return values have opposite meanings.
 */
typedef pmath_bool_t (*pmath_equal_func_t)(pmath_t, pmath_t);

/**\brief A comparision function to determine the order of two objects.

   The return value is <0, =0 or >0, if the first argument is less, equal to or
   greater than the second respectively. Both arguments won't be destroyed by
   the function.
 */
typedef int (*pmath_compare_func_t)(pmath_t, pmath_t);

/*============================================================================*/

/**\brief Fast test, whether an object is a `magic value`
   \hideinitializer
   \memberof pmath_t
   \param obj A pMath object
   \return A boolean value.

   This is the faster equivalent to pmath_instance_of(obj, PMATH_TYPE_MAGIC).
   
   \see pmath_instance_of()
 */
#define PMATH_IS_MAGIC(obj) (((uintptr_t)(obj)) <= 255)

/**\brief Calculates an object's hash value
   \memberof pmath_t
   \param obj The object.
   \return A hash value.

   pmath_equals(A, B) implies pmath_hash(A) == pmath_hash(B).
 */
PMATH_API 
PMATH_ATTRIBUTE_PURE
unsigned int pmath_hash(pmath_t obj);

/**\brief Compares two objects for identity.
   \memberof pmath_t
   \param objA The first object.
   \param objB The second one.
   \return TRUE iff both objects are identical.

   `identity` means, that X != Y is possible, even if X and Y evaluate to the 
   same value. 
   
   If objA and objB are symbols, the result is identical to testing
   objA == objB. 
   
   \note pmath_equals(A, B) might return FALSE although pmath_compare(A, B) == 0
   e.g. for an integer A and q floating point value B.
 */
PMATH_API
PMATH_ATTRIBUTE_PURE
pmath_bool_t pmath_equals(
  pmath_t objA,
  pmath_t objB);

/**\brief Compares two objects syntactically.
   \memberof pmath_t
   \param objA The first object.
   \param objB The second one.
   \return < 0 if \a objA is less than \a objB, == 0 if both are equal and > 0
           if \a objA is greater than \a objB.

   `syntactically` means that for two symbols X and Y, pmath_compare(X, Y) < 0 
   even if X:=2 and Y:=1, because X appears before Y in the alphabet.
   
   \note pmath_equals(A, B) might return FALSE although pmath_compare(A, B) == 0
   e.g. for an integer A and q floating point value B.
*/
PMATH_API 
PMATH_ATTRIBUTE_PURE
int pmath_compare(
  pmath_t objA,
  pmath_t objB);

/**\brief Write an object to a stream.
   \memberof pmath_t
   \param obj The object to be written.
   \param options Some options defining the format.
   \param write The stream's output function.
   \param user The user-argument of write (e.g. the stream itself).
   
   \see pmath_utf8_writer
 */
PMATH_API 
PMATH_ATTRIBUTE_NONNULL(3)
void pmath_write(
  pmath_t                 obj,
  pmath_write_options_t   options,
  pmath_write_func_t      write,
  void                   *user);

/**\brief Test whether an object is already evaluated.
   \memberof pmath_t
   \param obj Any pMath object. It will *not* be freed.
   \return TRUE if a call to pmath_evaluate would not change the object.
 */
PMATH_API
PMATH_ATTRIBUTE_PURE
pmath_bool_t pmath_is_evaluated(pmath_t obj);

/** @} */

#endif /* __PMATH_CORE__OBJECTS_H__ */
