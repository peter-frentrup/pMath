#ifndef __PMATH_UTIL__MESSAGES_H__
#define __PMATH_UTIL__MESSAGES_H__

#include <pmath-core/symbols.h>
#include <stdlib.h>

/**\defgroup messages Messages
   \brief Error handling and informing the user.
   
   When you encounter an error such as wrong argument usage in pMath functions,
   you just print out a message and return the handled expression unevaluated.
   You do \em not call pmath_abort_please(). 
   
   Example: The built in Power function invokes the Power::indet in case of 0^0.
   
   If you notice that a memory allocation failed, do nothing at all (even do not
   try to print a message). pMath automatically calls pmath_abort_please() on 
   out-of-memory and thus no message would be shown.
   
   Messages are similar to Mathematicas approach. On the language level, you 
   enter `Message(symbol::tag, ...)` to print a message with optional inserted
   values `...`. You can use Backquotes to refer to given values.
   
   You can use all messages of the symbol General with every other symbol.
   
  @{
 */

/**\brief Print a message with pMath object arguments.
   \param symbol The symbol, that defines the message. It wont be freed. PMATH_NULL
          will be treated as pmath_current_head(). This is useful, because 
          pmath_current_head() returns a reference, but pmath_message() would
          not free it.
   \param tag The message's tag as a zero-terminated C string.
   \param argcount The number of following arguments.
   \param ... Exactly \a argcount pMath objects. They all will be freed.
   
   \note If symbol==PMATH_NULL and pmath_current_head() is an expression f(), the 
   function acts as if pmath_current_head() was f.
   
   \note All the symbols and expressions in ... will be embedded in 
   HoldForm(...), because Message() would evaluate them. If you want one of the 
   values to be evaluated, do it manually.
 */
PMATH_API 
void pmath_message(
  pmath_symbol_t symbol,
  const char *tag,
  size_t argcount,
  ...);

/**\brief Generate a General::arg* message (invalid argument count).
   \param given The given number of arguments.
   \param min The minimum expected number of arguments.
   \param max The maximum expected number of arguments.
 */
PMATH_API 
void pmath_message_argxxx(size_t given, size_t min, size_t max);

/**\brief Find a message's text.
   \param name An expression of the form symbol::name 
          (that is MessageName(symbol, "name")). It will be freed. 
   \return The message's content or PMATH_NULL if nothing was found or the 
           \ref pmath_type_t "magic value PMATH_UNDEFINED", if \a name does 
           not have the expected form.
   
   This function can be used in front-ends that overwrite the built-in Message 
   function.
 */
PMATH_API 
PMATH_ATTRIBUTE_USE_RESULT
pmath_string_t pmath_message_find_text(pmath_t  name);

/**\brief Print a syntax warning or error message.
   \param code The code. A pMath string. It wont be freed.
   \param position The position of the syntax error in the code.
   \param filename The file from where the input came or PMATH_NULL to omit it. It 
          will be freed.
   \param lines_before_code The number of lines in the input file before \a code
          was read. This is ignored if \a filename is PMATH_NULL.
   
   This function poduces a Syntax::bgn, Syntax::bgnf, Syntax::nxt, Syntax::nxtf,
   Syntax::more, Syntax::moref, Syntax::newl or Syntax::newlf message, depending 
   on where the syntax error is in the \a code and whether \a filename is not 
   PMATH_NULL. It can be used to report errors/warnings from 
   pmath_spans_from_string().
 */
PMATH_API 
void pmath_message_syntax_error(
  pmath_string_t  code,
  int             position,
  pmath_string_t  filename,
  int             lines_before_code);

/** @} */

#endif /* __PMATH_UTIL__MESSAGES_H__ */
