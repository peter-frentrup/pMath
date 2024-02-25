#ifndef __PMATH_LANGUAGE__CHARNAMES_H__
#define __PMATH_LANGUAGE__CHARNAMES_H__

#include <stddef.h>
#include <pmath-config.h>
#include <pmath-types.h>

/**\addtogroup strings

   @{
 */

/**\brief Get a named character
   \param name The ASCII-name of the character. e.g. "Sum"
   \return The character code or 0xFFFFFFFFU on error
 */
PMATH_API
uint32_t pmath_char_from_name(const char *name);

/**\brief Get a character's name
   \param unichar A unicode character
   \return The ASCII-name or NULL if it is unnamed
 */
PMATH_API
const char *pmath_char_to_name(uint32_t unichar);

/**\brief Parse an escaped character to a unicode codepoint.
   \param str    A string of the form `\[name]` or `\[U+HHHH]` or `\n` or ...
   \param maxlen The buffer length of \a str.
   \param result Here goes the parsed character, 0xFFFFFFFFU on error.
   \return The end of the parsed character or the error position.
 */
PMATH_API const uint16_t *pmath_char_parse(const uint16_t *str, int maxlen, uint32_t *result);


struct pmath_named_char_t {
  uint32_t unichar;
  const char *name;
};

/**\brief Get an array of all known character names.
   \param length Set to the array length on return.
   \return An internal array of character and name entires.
 */
PMATH_API const struct pmath_named_char_t *pmath_get_char_names(size_t *length);


/** @} */
/*============================================================================*/

#ifdef BUILDING_PMATH
PMATH_PRIVATE pmath_bool_t _pmath_charnames_init(void);
PMATH_PRIVATE void         _pmath_charnames_done(void);
#endif

#endif /* __PMATH_LANGUAGE__CHARNAMES_H__ */
