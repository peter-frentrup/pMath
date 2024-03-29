#ifndef __PMATH_LANGUAGE__TOKENS_H__
#define __PMATH_LANGUAGE__TOKENS_H__

#include <pmath-config.h>
#include <pmath-types.h>

#include <stdlib.h>


/**\addtogroup parsing_code

   @{
 */

/**\brief Token classes known in the pMath language.
 */
typedef enum {
  PMATH_TOK_NONE = 0,
  PMATH_TOK_SPACE = 1,
  PMATH_TOK_DIGIT = 2,
  PMATH_TOK_STRING = 3,
  PMATH_TOK_NAME = 4,
  PMATH_TOK_NAME2 = 5,
  PMATH_TOK_BINARY_LEFT = 6,
  PMATH_TOK_BINARY_RIGHT = 7,
  PMATH_TOK_NARY = 10,
  PMATH_TOK_NARY_AUTOARG = 11,
  PMATH_TOK_NARY_OR_PREFIX = 12,
  PMATH_TOK_POSTFIX_OR_PREFIX = 13,
  PMATH_TOK_PREFIX = 14,
  PMATH_TOK_POSTFIX = 15,
  PMATH_TOK_CALL = 16,
  PMATH_TOK_LEFTCALL = 17,
  PMATH_TOK_LEFT = 18,
  PMATH_TOK_RIGHT = 19,
  PMATH_TOK_PRETEXT = 20,

  PMATH_TOK_ASSIGNTAG = 21,
  PMATH_TOK_PLUSPLUS = 22,
  PMATH_TOK_COLON = 23,
  PMATH_TOK_TILDES = 24,
  PMATH_TOK_SLOT = 25,
  PMATH_TOK_QUESTION = 26,
  PMATH_TOK_INTEGRAL = 27, /* acts like PMATH_TOK_PREFIX */
  PMATH_TOK_COMMENTEND = 28,
  PMATH_TOK_NEWLINE = 29,
  PMATH_TOK_CALLPIPE = 30,
} pmath_token_t;

enum {
  PMATH_PREC_ANY =              0,
  PMATH_PREC_SEQ =             10,
  PMATH_PREC_EVAL =            20,
  PMATH_PREC_ASS =             30,
  PMATH_PREC_MODY =            40,
  PMATH_PREC_LAZY =            50,
  PMATH_PREC_FUNC =            60,
//  PMATH_PREC_COLON =           70,
  PMATH_PREC_REPL =            80,
  PMATH_PREC_RULE =            90,
  PMATH_PREC_MAP =            100,
  PMATH_PREC_STR =            110,
  PMATH_PREC_COND =           120,
  PMATH_PREC_ALT =            130,
  PMATH_PREC_OR =             150,
  PMATH_PREC_XOR =            155,
  PMATH_PREC_AND =            160,
  PMATH_PREC_ARROW =          170,
  PMATH_PREC_REL =            180,
  PMATH_PREC_UNION =          190,
  PMATH_PREC_ISECT =          200,
  PMATH_PREC_RANGE =          210,
  PMATH_PREC_ADD =            220,
  PMATH_PREC_CIRCADD =        230,
  PMATH_PREC_PLUMI =          240,
  PMATH_PREC_CIRCMUL =        250,
  PMATH_PREC_CALLPIPE_RIGHT = 255,
  PMATH_PREC_MUL =            260,
  PMATH_PREC_DIV =            270,
  PMATH_PREC_MIDDOT =         280,
  PMATH_PREC_CROSS =          290,
  PMATH_PREC_MUL2 =           300,
  PMATH_PREC_POW =            310,
  PMATH_PREC_FAC =            320,
  PMATH_PREC_APL =            330,
  PMATH_PREC_REPEAT =         340,
  PMATH_PREC_TEST =           350,
  PMATH_PREC_INC =            360,

  PMATH_PREC_CALL =           400,
  PMATH_PREC_INVISADD =       410,
  PMATH_PREC_DIFF =           450,
  PMATH_PREC_PRIM =          1000
};

#define PMATH_CHAR_PLUSMINUS                 ((uint16_t) 0x00B1 ) ///< The "+/-" character
#define PMATH_CHAR_TIMES                     ((uint16_t) 0x00D7 ) ///< The multiplication character
#define PMATH_CHAR_INVISIBLECALL             ((uint16_t) 0x2061 ) ///< The Function application character
#define PMATH_CHAR_INVISIBLETIMES            ((uint16_t) 0x2062 ) ///< The invisible "*" character
#define PMATH_CHAR_INVISIBLECOMMA            ((uint16_t) 0x2063 ) ///< The invisible "," character
#define PMATH_CHAR_INVISIBLEPLUS             ((uint16_t) 0x2064 ) ///< The invisible "+" character
#define PMATH_CHAR_NOMINALDIGITS             ((uint16_t) 0x206F ) ///< Character that indicates that the following sequence of digits *and* letters and dot is to be treated as one token (as if prefixed by "36^^"). But undefined parsing (unspecified base).
#define PMATH_CHAR_VECTOR                    ((uint16_t) 0x21C0 ) ///< The arrow above names to indicate a vector
#define PMATH_CHAR_RULE                      ((uint16_t) 0x2192 ) ///< The "->" operator
#define PMATH_CHAR_RULEDELAYED               ((uint16_t) 0x29F4 ) ///< The ":>" operator
#define PMATH_CHAR_ASSIGN                    ((uint16_t) 0x2254 ) ///< The ":=" operator
#define PMATH_CHAR_ASSIGNDELAYED             ((uint16_t) 0x2A74 ) ///< The "::=" operator
#define PMATH_CHAR_INTEGRAL_D                ((uint16_t) 0x2146 ) ///< The integral "d"
#define PMATH_CHAR_PIECEWISE                 ((uint16_t) 0xF361 ) ///< The left curly bracket for cases
#define PMATH_CHAR_LEFTINVISIBLEBRACKET      ((uint16_t) 0xF362 ) ///< TeX's "\left."
#define PMATH_CHAR_RIGHTINVISIBLEBRACKET     ((uint16_t) 0xF363 ) ///< TeX's "\right."
#define PMATH_CHAR_LEFTBRACKETINGBAR         ((uint16_t) 0xF603 ) ///< TeX's "\left|"
#define PMATH_CHAR_RIGHTBRACKETINGBAR        ((uint16_t) 0xF604 ) ///< TeX's "\right|"
#define PMATH_CHAR_LEFTDOUBLEBRACKETINGBAR   ((uint16_t) 0xF605 ) ///< TeX's "\left\|"
#define PMATH_CHAR_RIGHTDOUBLEBRACKETINGBAR  ((uint16_t) 0xF606 ) ///< TeX's "\right\|"
#define PMATH_CHAR_ALIASDELIMITER            ((uint16_t) 0xF764 ) ///< The character inserted by Richmath with ESCAPE or CAPSLOCK
#define PMATH_CHAR_ALIASINDICATOR            ((uint16_t) 0xF768 ) ///< A character that looks like PMATH_CHAR_ALIASDELIMITER but has no effect
#define PMATH_CHAR_LEFT_BOX                  ((uint16_t) 0xFFF9 ) ///< Start of box code inside a string.
#define PMATH_CHAR_RIGHT_BOX                 ((uint16_t) 0xFFFB ) ///< End of box code inside a string.
#define PMATH_CHAR_BOX                       ((uint16_t) 0xFDD0 ) ///< Represents a box.
#define PMATH_CHAR_PLACEHOLDER               ((uint16_t) 0xFFFD ) ///< The Placeholder character. In richmath, type CAPSLOCK pl CAPSLOCK to insert it.
#define PMATH_CHAR_SELECTIONPLACEHOLDER      ((uint16_t) 0xF527 ) ///< The selection placeholder character. In richmath, type CAPSLOCK spl CAPSLOCK to insert it.

/**\brief Analyse a token.
   \param str A UTF16-string.
   \param len The length (in uint16_t-s) of the token \a str.
   \param prec Optional address, where to store the default operator precedence
               for the token.
   \return The associated token class.
 */
PMATH_API pmath_token_t pmath_token_analyse(
  const uint16_t *str,
  int             len,
  int            *prec);

/**\brief Give the prefix oprator precedence for a token
   \param str A UTF16-string.
   \param len The length (in uint16_t elements) of the token \a str.
   \param defprec The default operator precedence as given by
                  pmath_token_analyse()
   \return The prefix operator precedence.
 */
PMATH_API int pmath_token_prefix_precedence(
  const uint16_t *str,
  int             len,
  int             defprec);

/**\brief Test whether a token may be the first token in a subexpression.
   \param tok A token class.
   \return Whether tok may start a new subexpression
 */
static PMATH_INLINE pmath_bool_t pmath_token_maybe_first(pmath_token_t tok) {
  return tok == PMATH_TOK_NONE                  ||
         tok == PMATH_TOK_SPACE                 ||
         tok == PMATH_TOK_NAME                  ||
         tok == PMATH_TOK_NAME2                 ||
         tok == PMATH_TOK_DIGIT                 ||
         tok == PMATH_TOK_STRING                ||
         tok == PMATH_TOK_PREFIX                ||
         tok == PMATH_TOK_NARY_AUTOARG          ||
         tok == PMATH_TOK_NARY_OR_PREFIX        ||
         tok == PMATH_TOK_POSTFIX_OR_PREFIX     ||
         tok == PMATH_TOK_LEFT                  ||
         tok == PMATH_TOK_LEFTCALL              ||
         tok == PMATH_TOK_PRETEXT               ||
         tok == PMATH_TOK_PLUSPLUS              ||
         tok == PMATH_TOK_TILDES                ||
         tok == PMATH_TOK_SLOT                  ||
         tok == PMATH_TOK_QUESTION              ||
         tok == PMATH_TOK_INTEGRAL              ||
         tok == PMATH_TOK_NEWLINE;
}

/**\brief Test whether a token need not be the first token in a subexpression.
   \param tok A token class.
   \return Whether tok may reside inside a subexpression.
 */
static PMATH_INLINE pmath_bool_t pmath_token_maybe_rest(pmath_token_t tok) {
  return tok != PMATH_TOK_NONE    &&
         tok != PMATH_TOK_SPACE   &&
         tok != PMATH_TOK_NAME    &&
         tok != PMATH_TOK_NAME2   &&
         tok != PMATH_TOK_DIGIT   &&
         tok != PMATH_TOK_STRING  &&
         tok != PMATH_TOK_PREFIX  &&
         tok != PMATH_TOK_PRETEXT &&
         tok != PMATH_TOK_LEFT    &&
         tok != PMATH_TOK_RIGHT   &&
         tok != PMATH_TOK_TILDES  &&
         tok != PMATH_TOK_SLOT    &&
         tok != PMATH_TOK_INTEGRAL;
}

/**\brief Test if a unicode character is a left bracket
 */
static PMATH_INLINE pmath_bool_t pmath_char_is_left(uint16_t ch) {
  pmath_token_t tok;

  if(ch == PMATH_CHAR_PIECEWISE)
    return TRUE;

  tok = pmath_token_analyse(&ch, 1, NULL);
  return tok == PMATH_TOK_LEFT || tok == PMATH_TOK_LEFTCALL;
}

/**\brief Get the corresponding right bracket to a given left bracket or 0.
 */
static PMATH_INLINE uint16_t pmath_right_fence(uint16_t left) {
  int prec;
  pmath_token_t tok;

  ++left;
  tok = pmath_token_analyse(&left, 1, &prec);

  if(tok == PMATH_TOK_RIGHT && prec == -1)
    return left;

  ++left;
  tok = pmath_token_analyse(&left, 1, &prec);

  if(tok == PMATH_TOK_RIGHT && prec == -2)
    return left;

  ++left;
  tok = pmath_token_analyse(&left, 1, &prec);

  if(tok == PMATH_TOK_RIGHT && prec == -3)
    return left;

  ++left;
  tok = pmath_token_analyse(&left, 1, &prec);

  if(tok == PMATH_TOK_RIGHT && prec == -4)
    return left;

  return 0;
}

/**\brief Test if a unicode character is a right bracket
 */
static PMATH_INLINE pmath_bool_t pmath_char_is_right(uint16_t ch) {
  return PMATH_TOK_RIGHT == pmath_token_analyse(&ch, 1, NULL);
}

/**\brief Test if a unicode character can be the start of an identifier/name.
 */
static PMATH_INLINE pmath_bool_t pmath_char_is_name(uint16_t ch) {
  return PMATH_TOK_NAME == pmath_token_analyse(&ch, 1, NULL);
}

/**\brief Test if a unicode character is an integral.
 */
static PMATH_INLINE pmath_bool_t pmath_char_is_integral(uint16_t ch) {
  return PMATH_TOK_INTEGRAL == pmath_token_analyse(&ch, 1, NULL);
}

/**\brief Test if a token may be a big operator
 */
static PMATH_INLINE pmath_bool_t pmath_token_maybe_bigop(pmath_token_t tok) {
  return tok == PMATH_TOK_BINARY_LEFT           ||
         tok == PMATH_TOK_BINARY_RIGHT          ||
         tok == PMATH_TOK_NARY                  ||
         tok == PMATH_TOK_NARY_OR_PREFIX        ||
         tok == PMATH_TOK_POSTFIX_OR_PREFIX     ||
         tok == PMATH_TOK_PREFIX;
}

/**\brief Test if a unicode character may be a big operation, e.g. Union, Sum.
 */
static PMATH_INLINE pmath_bool_t pmath_char_maybe_bigop(uint16_t ch) {
  return pmath_token_maybe_bigop(pmath_token_analyse(&ch, 1, NULL));
}

/**\brief Test if a unicode character is a digit '0' - '9'.
 */
static PMATH_INLINE pmath_bool_t pmath_char_is_digit(uint16_t ch) {
  return ch >= '0' && ch <= '9';
}

/**\brief Test if a unicode character is a base-36 digit '0' - '9', 'a' - 'z',
          'A' - 'Z'.
 */
static PMATH_INLINE pmath_bool_t pmath_char_is_36digit(uint16_t ch) {
  return (ch >= '0' && ch <= '9') ||
         (ch >= 'a' && ch <= 'z') ||
         (ch >= 'A' && ch <= 'Z');
}

/**\brief Test if in a given base, a unicode character is a digit.
 */
static PMATH_INLINE pmath_bool_t pmath_char_is_basedigit(int base, uint16_t ch) {
  return (base <= 10 && ch >= '0' && ch < '0' + base) ||
         (base > 10 &&
          ((ch >= '0' && ch <= '9') ||
           (ch >= 'a' && ch <= 'a' + base - 10) ||
           (ch >= 'A' && ch <= 'A' + base - 10)));
}

/**\brief Test if a unicode character is a hexadecimal digit.
 */
static PMATH_INLINE pmath_bool_t pmath_char_is_hexdigit(uint16_t ch) {
  return (ch >= '0' && ch <= '9') ||
         (ch >= 'a' && ch <= 'f') ||
         (ch >= 'A' && ch <= 'F');
}

/** @} */

#endif /* __PMATH_LANGUAGE__TOKENS_H__ */
