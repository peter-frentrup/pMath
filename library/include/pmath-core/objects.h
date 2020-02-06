#ifndef __PMATH_CORE__OBJECTS_H__
#define __PMATH_CORE__OBJECTS_H__

#include <pmath-config.h>
#include <pmath-types.h>

#include <assert.h>
#include <stdlib.h>

/**\defgroup objects Objects - the Base of pMath
   \brief The basic class for all pMath objects.

   pMath works on objects. They can be @ref expressions (trees of pMath objects),
   @ref symbols, @ref numbers, @ref strings, `magic objects` (specially tagged
   integer constants) or @ref custom.
   The objects are generally reference-counted via pmath_ref() and pmath_unref()
   (except for the NaN-boxing optimization mentioned below) and immutable.
   However, symbols are mutable (their value's can change) and thus may cause
   cyclic references. Those cycles are broken up for `temporary` symbols by a
   cycle detector that runs concurrently on its own thread.

   For efficiency reasons, 32 bit integers, double precision floating point
   values, short strings up to 2 characters and magic values are stored inline
   in the pmath_t struct. The struct size is only 8 bytes (= sizeof(double))
   thanks to a technique called NaN-boxing.
   (The details of this implementation could change in future version and/or
   on different architectures, so do not rely on it.)

   \see helpers
  @{
 */

/* (big endian notation)

  seeeeeee_eeeemmmm_mmmmmmmm_mmmmmmmm__mmmmmmmm_mmmmmmmm_mmmmmmmm_mmmmmmmm = ieee double

  x1111111_1111xxxx_xxxxxxxx_xxxxxxxx__xxxxxxxx_xxxxxxxx_xxxxxxxx_xxxxxxxx = ieee NaN/Inf

  I assume same endianness for double and integer.
 */

#ifndef PMATH_DOXYGEN
#  define PMATH_TAGMASK_BITCOUNT   12           /* |||||||| |||| */
#  define PMATH_TAGMASK_NONDOUBLE  0x7FF00000U  /* 01111111_11110000_00000000_00000000 0...0 : ieee double NaN or Inf */
#  define PMATH_TAGMASK_POINTER    0xFFF00000U  /* 11111111_11110000_00000000_00000000 0...0 */
#  define PMATH_TAG_INVALID        (PMATH_TAGMASK_NONDOUBLE | 0xFFFFF)
#  define PMATH_TAG_MAGIC          (PMATH_TAGMASK_NONDOUBLE | 0x10000)
#  define PMATH_TAG_INT32          (PMATH_TAGMASK_NONDOUBLE | 0x20000)
#  define PMATH_TAG_STR0           (PMATH_TAGMASK_NONDOUBLE | 0x30000)
#  define PMATH_TAG_STR1           (PMATH_TAGMASK_NONDOUBLE | 0x40000)
#  define PMATH_TAG_STR2           (PMATH_TAGMASK_NONDOUBLE | 0x50000)
#endif // PMATH_DOXYGEN

/**\class pmath_t
   \brief The basic type of all pMath objects.

   Use pmath_is_XXX() to determine whether an object is of a specific type.
   Generally, you must free unused objects with pmath_unref(), but if
   pmath_is_pointer() gives FALSE, then calling pmath_ref() and pmath_unref() is
   not neccessary.

   Machine precision floating point values (aka double) and certain special
   values are stored directly in the pmath_t object. Long strings, expressions,
   other values are stored as a pointer. The technique to pack all this in only
   8 bytes is called NaN-boxing. See
   http://blog.mozilla.com/rob-sayre/2010/08/02/mozillas-new-javascript-value-representation
 */
typedef union pmath_t {
/**\privatesection
 */

  uint64_t as_bits;
  double   as_double;

#if PMATH_BITSIZE == 64
  struct _pmath_t *as_pointer_64;
#endif

  struct {
#if PMATH_BYTE_ORDER < 0 // little endian
    union {
      int32_t  as_int32;
      uint16_t as_chars[2];
#if PMATH_BITSIZE == 32
      struct _pmath_t *as_pointer_32;
#endif
    } u;
    uint32_t  tag;
#endif

#if PMATH_BYTE_ORDER > 0 // big endian:
    uint32_t  tag;
    union {
      int32_t  as_int32;
      uint16_t as_chars[2];
#if PMATH_BITSIZE == 32
      struct _pmath_t *as_pointer_32;
#endif
    } u;
#endif
  } s;
} pmath_t;

PMATH_FORCE_INLINE
pmath_t PMATH_FROM_TAG(uint32_t tag, int32_t value) {
  pmath_t r;

  assert((tag & PMATH_TAGMASK_POINTER) == PMATH_TAGMASK_NONDOUBLE);
  assert(tag != PMATH_TAG_INVALID);

  r.s.tag = tag;
  r.s.u.as_int32 = value;

  return r;
}


/**\brief Create an integer object from an int32_t.

   Integer objects of int32_t are not reference-counted, since they are stored
   directly inside the pmath_t. Hence there is no need to pmath_unref() them.
 */
PMATH_FORCE_INLINE
pmath_t PMATH_FROM_INT32(int32_t i) {
  return PMATH_FROM_TAG(PMATH_TAG_INT32, i);
}

PMATH_FORCE_INLINE
PMATH_ATTRIBUTE_USE_RESULT
pmath_t PMATH_FROM_PTR(void *p) {
  pmath_t r;

#if PMATH_BITSIZE == 64
  r.as_pointer_64 = (struct _pmath_t *)p;
  r.s.tag |= PMATH_TAGMASK_POINTER;
#elif PMATH_BITSIZE == 32
  r.s.tag = PMATH_TAGMASK_POINTER;
  r.s.u.as_pointer_32 = (struct _pmath_t *)p;
#endif

  return r;
}

#define PMATH_THREAD_KEY_PARSESYMBOLS     PMATH_FROM_TAG(PMATH_TAG_MAGIC, 252)
#define PMATH_THREAD_KEY_PARSERARGUMENTS  PMATH_FROM_TAG(PMATH_TAG_MAGIC, 253)
#define PMATH_ABORT_EXCEPTION             PMATH_FROM_TAG(PMATH_TAG_MAGIC, 254)

#ifndef PMATH_DOXYGEN
#  define PMATH_STATIC_UNDEFINED            { (((uint64_t)PMATH_TAG_MAGIC) << 32) | 255 }
#  define PMATH_STATIC_NULL                 { ((uint64_t)PMATH_TAGMASK_POINTER) << 32 }
#endif // PMATH_DOXYGEN

/**\brief Magic value to indicate unset variable values/...
   \hideinitializer
 */
PMATH_UNUSED
static const pmath_t PMATH_UNDEFINED = PMATH_STATIC_UNDEFINED;

/**\brief The NULL pointer. /\\/ in pMath.
   \hideinitializer
 */
PMATH_UNUSED
static const pmath_t PMATH_NULL      = PMATH_STATIC_NULL;


/**\brief The type or class of a non-inlined pMath object.

   This is a bitset of the \ref PMATH_TYPE_XXX constants. Note that inlined pMath
   objects (double, int32_t, short strings) have not type code.

   \see pmath_is_pointer_of
 */
typedef int pmath_type_t;

#ifndef PMATH_DOXYGEN
enum {
  PMATH_TYPE_SHIFT_MP_FLOAT = 0,
  PMATH_TYPE_SHIFT_MP_INT,
  PMATH_TYPE_SHIFT_QUOTIENT,
  PMATH_TYPE_SHIFT_BIGSTRING,
  PMATH_TYPE_SHIFT_SYMBOL,
  PMATH_TYPE_SHIFT_EXPRESSION_GENERAL,
  PMATH_TYPE_SHIFT_EXPRESSION_GENERAL_PART,
  PMATH_TYPE_SHIFT_RESERVED_1,
  PMATH_TYPE_SHIFT_CUSTOM,
  PMATH_TYPE_SHIFT_BLOB,
  PMATH_TYPE_SHIFT_PACKED_ARRAY,
  PMATH_TYPE_SHIFT_PINNED_STRING,
  PMATH_TYPE_SHIFT_RESERVED_2,
  PMATH_TYPE_SHIFT_COUNT
};
#endif

/**\anchor PMATH_TYPE_XXX
   \see pmath_type_t
 */
enum {
  /**\hideinitializer
     A pmath_integer_t that does not fit into \c int32_t.
   */
  PMATH_TYPE_MP_INT                  = 1 << PMATH_TYPE_SHIFT_MP_INT,

  /**\hideinitializer
     A pmath_quotient_t.
   */
  PMATH_TYPE_QUOTIENT                = 1 << PMATH_TYPE_SHIFT_QUOTIENT,

  /**\hideinitializer
     A multi-precision pmath_float_t.
   */
  PMATH_TYPE_MP_FLOAT                = 1 << PMATH_TYPE_SHIFT_MP_FLOAT,

  /**\hideinitializer
     A pmath_string_t that is not inlined.
   */
  PMATH_TYPE_BIGSTRING               = 1 << PMATH_TYPE_SHIFT_BIGSTRING,

  /**\hideinitializer
     A pmath_symbol_t.
   */
  PMATH_TYPE_SYMBOL                  = 1 << PMATH_TYPE_SHIFT_SYMBOL,

  /**\hideinitializer
     A general pmath_expr_t that is not one of the special representations.
   */
  PMATH_TYPE_EXPRESSION_GENERAL      = 1 << PMATH_TYPE_SHIFT_EXPRESSION_GENERAL,

  /**\hideinitializer
     A part of another pmath_expr_t.
   */
  PMATH_TYPE_EXPRESSION_GENERAL_PART = 1 << PMATH_TYPE_SHIFT_EXPRESSION_GENERAL_PART,

  /**\hideinitializer
     A pmath_packed_array_t.
   */
  PMATH_TYPE_PACKED_ARRAY            = 1 << PMATH_TYPE_SHIFT_PACKED_ARRAY,

  /** Any pmath_expr_t.
   */
  PMATH_TYPE_EXPRESSION              = PMATH_TYPE_EXPRESSION_GENERAL | PMATH_TYPE_EXPRESSION_GENERAL_PART | PMATH_TYPE_PACKED_ARRAY,

  /**\hideinitializer
     A pmath_custom_t.
   */
  PMATH_TYPE_CUSTOM                  = 1 << PMATH_TYPE_SHIFT_CUSTOM,

  /**\hideinitializer
     A pmath_blob_t.
   */
  PMATH_TYPE_BLOB                    = 1 << PMATH_TYPE_SHIFT_BLOB
};


/**\brief Options for pmath_write().

   These options can be one or more of the \ref PMATH_WRITE_OPTIONS_XXX flags.
 */
typedef int pmath_write_options_t;

/**\anchor PMATH_WRITE_OPTIONS_XXX
   \see pmath_write_options_t
 */
enum {
  /**\hideinitializer
     All expressions are written in the form `f(a, b, ...)` without any syntactic
     sugar. Supersedes PMATH_WRITE_OPTIONS_INPUTEXPR.
   */
  PMATH_WRITE_OPTIONS_FULLEXPR        = 1 << 0,

  /**\hideinitializer
     Strings are written with quotes and escape sequences.
   */
  PMATH_WRITE_OPTIONS_FULLSTR         = 1 << 1,

  /**\hideinitializer
     Names are written with their full namespace path.
   */
  PMATH_WRITE_OPTIONS_FULLNAME        = 1 << 2,

  /**\hideinitializer
     Expressions are written in a form that is valid pMath input, except that
     this does not automatically imply PMATH_WRITE_OPTIONS_FULLSTR.
   */
  PMATH_WRITE_OPTIONS_INPUTEXPR       = 1 << 3,

  /**\hideinitializer
     Packed Arrays are written in the form
     `"PackedArray"(type, \<\<dimensions\>\>)` instead of nested lists.
   */
  PMATH_WRITE_OPTIONS_PACKEDARRAYFORM = 1 << 4,

  /**\hideinitializer
     Do not add unnecessary extra spaces arount operators.
   */
  PMATH_WRITE_OPTIONS_NOSPACES   = 1 << 5,

  /**\hideinitializer
     Use unicode characters like PMATH_CHAR_RULE instead of "->" etc.
   */
  PMATH_WRITE_OPTIONS_PREFERUNICODE = 1 << 6,

  /**\hideinitializer
     Non-System`-symbols are written with their full namespace path.
   */
  PMATH_WRITE_OPTIONS_FULLNAME_NONSYSTEM = 1 << 7,

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
typedef void (*pmath_param_proc_t)(void *, pmath_t);

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

/**\brief Command structure for pmath_write_ex().

   This should be inistialized with
   <tt>memset(&ex, 0, sizeof(ex)); ex.size = sizeof(ex); ... </tt>
 */
struct pmath_write_ex_t {
  /**\brief The structure's size in bytes.
     This must be initialized with sizeof(struct pmath_write_ex_t), for version
     control.
   */
  size_t size;

  /**\brief The output options.
   */
  pmath_write_options_t   options;

  /**\brief Write callback.

     This is mandatory.
   */
  void (*write)(void *user, const uint16_t *data, int len);

  /**\brief First parameter of the callbacks
   */
  void *user;

  /**\brief Optional, called before an object is written.
   */
  void (*pre_write)( void *user, pmath_t obj, pmath_write_options_t options);

  /**\brief Optional, called after an object is written.
   */
  void (*post_write)(void *user, pmath_t obj, pmath_write_options_t options);
};

/*============================================================================*/

/**\brief Calculates an object's hash value
   \memberof pmath_t
   \param obj The object.
   \return A hash value.

   pmath_equals(A, B) implies pmath_hash(A) == pmath_hash(B).
 */
PMATH_API
PMATH_ATTRIBUTE_PURE
unsigned int pmath_hash(pmath_t obj);

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
  void                  (*write)(void *user, const uint16_t *data, int len),
  void                   *user);

/**\brief Advanced function to write an object to a stream.
   \memberof pmath_t
   \param info All the acutal parameters.
   \param obj The object to be written.

   \see pmath_write
 */
PMATH_API
PMATH_ATTRIBUTE_NONNULL(1)
void pmath_write_ex(struct pmath_write_ex_t *info, pmath_t obj);

/**\brief Test whether an object is already evaluated.
   \memberof pmath_t
   \param obj Any pMath object. It will *not* be freed.
   \return TRUE if a call to pmath_evaluate would not change the object.
 */
PMATH_API
PMATH_ATTRIBUTE_PURE
pmath_bool_t pmath_is_evaluated(pmath_t obj);

/**\brief Get the byte count of an object
   \memberof pmath_t
   \param obj The object. It wont be freed
   \return An estimate for the memory usage of this object. Symbols count as 0.
           Any elements that reference to the same object are treated as
           distinct.
 */
PMATH_API
size_t pmath_object_bytecount(pmath_t obj); // implemented in bytecount.c

/**\brief Get debug information for an object.
   \memberof pmath_t
   \param obj A pMath object. It wont be freed.
   \return The debug information for the object or PMATH_NULL. You have to
           destroy it when it is no longer used.
 */
PMATH_API
PMATH_ATTRIBUTE_USE_RESULT
pmath_t pmath_get_debug_info(pmath_t obj);

/**\brief Set debug information for an object.
   \memberof pmath_t
   \param obj        A pMath object. It will be freed.
   \param debug_info A pMath object. It will be freed.
   \return A copy of \a obj with changed debug information.

   \note Not all object types can store debug all kind of information. When
   storing is not possible, this function is a no-op. \a debug_info hould
   satisfy pmath_is_pointer()
 */
PMATH_API
PMATH_ATTRIBUTE_USE_RESULT
pmath_t pmath_try_set_debug_info(pmath_t obj, pmath_t debug_info);


/** @} */

#endif /* __PMATH_CORE__OBJECTS_H__ */
