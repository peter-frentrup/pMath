#ifndef __PMATH_LANGUAGE__TOKENS_H__
#define __PMATH_LANGUAGE__TOKENS_H__

#include <stdlib.h>
#include <pmath-config.h>
#include <pmath-types.h>

/**\addtogroup parser
   
   @{
 */

/**\brief Token classes known in the pMath language.
 */
typedef enum {
  PMATH_TOK_NONE,
  PMATH_TOK_SPACE,
  PMATH_TOK_DIGIT,
  PMATH_TOK_STRING,
  PMATH_TOK_NAME,
  PMATH_TOK_NAME2,
  PMATH_TOK_BINARY_LEFT,
  PMATH_TOK_BINARY_RIGHT,
  PMATH_TOK_BINARY_LEFT_AUTOARG,
  PMATH_TOK_BINARY_LEFT_OR_PREFIX,
  PMATH_TOK_NARY,
  PMATH_TOK_NARY_AUTOARG,
  PMATH_TOK_NARY_OR_PREFIX,
  PMATH_TOK_POSTFIX_OR_PREFIX,
  PMATH_TOK_PREFIX,
  PMATH_TOK_POSTFIX,
  PMATH_TOK_CALL,
  PMATH_TOK_LEFTCALL,
  PMATH_TOK_LEFT,
  PMATH_TOK_RIGHT,
  PMATH_TOK_PRETEXT,
  
  PMATH_TOK_ASSIGNTAG,
  PMATH_TOK_PLUSPLUS,
  PMATH_TOK_COLON,
  PMATH_TOK_TILDES,
  PMATH_TOK_SLOT,
  PMATH_TOK_QUESTION,
  PMATH_TOK_INTEGRAL, /* acts like PMATH_TOK_PREFIX */
  PMATH_TOK_COMMENTEND
} pmath_token_t;

enum{
  PMATH_PREC_ANY =       0,
  PMATH_PREC_SEQ =      10,
  PMATH_PREC_EVAL =     20,
  PMATH_PREC_ASS =      30,
  PMATH_PREC_MODY =     40,
  PMATH_PREC_LAZY =     50,
  PMATH_PREC_FUNC =     60,
//  PMATH_PREC_COLON =    70,
  PMATH_PREC_REPL =     80,
  PMATH_PREC_RULE =     90,
  PMATH_PREC_MAP =     100,
  PMATH_PREC_STR =     110,
  PMATH_PREC_COND =    120,
  PMATH_PREC_ALT =     130,
  PMATH_PREC_OR =      150,
  PMATH_PREC_XOR =     155,
  PMATH_PREC_AND =     160,
  PMATH_PREC_ARROW =   170,
  PMATH_PREC_REL =     180,
  PMATH_PREC_UNION =   190,
  PMATH_PREC_ISECT =   200,
  PMATH_PREC_RANGE =   210,
  PMATH_PREC_ADD =     220,
  PMATH_PREC_CIRCADD = 230,
  PMATH_PREC_PLUMI =   240,
  PMATH_PREC_CIRCMUL = 250,
  PMATH_PREC_MUL =     260,
  PMATH_PREC_DIV =     270,
  PMATH_PREC_MIDDOT =  280,
  PMATH_PREC_CROSS =   290,
  PMATH_PREC_MUL2 =    300,
  PMATH_PREC_POW =     310,
  PMATH_PREC_FAC =     320,
  PMATH_PREC_APL =     330,
  PMATH_PREC_REPEAT =  340,
  PMATH_PREC_TEST =    350,
  PMATH_PREC_INC =     360,

  PMATH_PREC_CALL =    400,
  PMATH_PREC_DIFF =    410,
  PMATH_PREC_PRIM =   1000
};

#define PMATH_CHAR_INVISIBLECALL  0x2061 ///< The Function application character
#define PMATH_CHAR_VECTOR         0x21C0 ///< The arrow above names to indicate a vector
#define PMATH_CHAR_RULE           0x2192 ///< The "->" operator
#define PMATH_CHAR_RULEDELAYED    0x29F4 ///< The ":>" operator
#define PMATH_CHAR_ASSIGN         0x2254 ///< The ":=" operator
#define PMATH_CHAR_ASSIGNDELAYED  0x2A74 ///< The "::=" operator
#define PMATH_CHAR_INTEGRAL_D     0x2146 ///< The integral "d"
#define PMATH_CHAR_PIECEWISE      0xF361 ///< The left curly bracket for cases
#define PMATH_CHAR_ALIASDELIMITER 0xF764 ///< The character inserted by Richmath with ESCAPE or CAPSLOCK
#define PMATH_CHAR_ALIASINDICATOR 0xF768 ///< A character that looks like PMATH_CHAR_ALIASDELIMITER but has no effect
#define PMATH_CHAR_LEFT_BOX       0xFFF9 ///< Start of box code inside a string.
#define PMATH_CHAR_RIGHT_BOX      0xFFFB ///< End of box code inside a string.
#define PMATH_CHAR_BOX            0xFDD0 ///< Represents a box.
#define PMATH_CHAR_PLACEHOLDER    0xFFFD ///< The Placeholder character. In richmath, type CAPSLOCK pl CAPSLOCK to insert it.

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
   \param len The length (in uint16_t-s) of the token \a str.
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
static PMATH_INLINE pmath_bool_t pmath_token_maybe_first(pmath_token_t tok){
  return tok == PMATH_TOK_NONE
      || tok == PMATH_TOK_SPACE
      || tok == PMATH_TOK_NAME
      || tok == PMATH_TOK_NAME2
      || tok == PMATH_TOK_DIGIT
      || tok == PMATH_TOK_STRING
      || tok == PMATH_TOK_PREFIX
      || tok == PMATH_TOK_BINARY_LEFT_AUTOARG
      || tok == PMATH_TOK_BINARY_LEFT_OR_PREFIX
      || tok == PMATH_TOK_NARY_AUTOARG
      || tok == PMATH_TOK_NARY_OR_PREFIX
      || tok == PMATH_TOK_POSTFIX_OR_PREFIX
      || tok == PMATH_TOK_LEFT
      || tok == PMATH_TOK_LEFTCALL
      || tok == PMATH_TOK_PRETEXT
      || tok == PMATH_TOK_PLUSPLUS
      || tok == PMATH_TOK_TILDES
      || tok == PMATH_TOK_SLOT
      || tok == PMATH_TOK_QUESTION
      || tok == PMATH_TOK_INTEGRAL;
}

/**\brief Test whether a token need not be the first token in a subexpression.
   \param tok A token class.
   \return Whether tok may reside inside a subexpression.
 */
static PMATH_INLINE pmath_bool_t pmath_token_maybe_rest(pmath_token_t tok){
  return tok != PMATH_TOK_NONE
      && tok != PMATH_TOK_SPACE
      && tok != PMATH_TOK_NAME
      && tok != PMATH_TOK_NAME2
      && tok != PMATH_TOK_DIGIT
      && tok != PMATH_TOK_PREFIX
      && tok != PMATH_TOK_PRETEXT
      && tok != PMATH_TOK_LEFT
      && tok != PMATH_TOK_RIGHT
      && tok != PMATH_TOK_TILDES
      && tok != PMATH_TOK_SLOT
      && tok != PMATH_TOK_INTEGRAL;
}

/**\brief Test if a unicode character is a left bracket
 */
static PMATH_INLINE pmath_bool_t pmath_char_is_left(uint16_t ch){
  pmath_token_t tok;
  
  if(ch == PMATH_CHAR_PIECEWISE)
    return TRUE;
  
  tok = pmath_token_analyse(&ch, 1, NULL);
  return tok == PMATH_TOK_LEFT || tok == PMATH_TOK_LEFTCALL;
  /*return ch == '(' 
      || ch == '[' 
      || ch == '{' 
      || ch == 0x2045 
      || ch == 0x2308 
      || ch == 0x230A 
      || ch == 0x2329 
      || ch == 0x27E6 
      || ch == 0x27E8 
      || ch == 0x27EA
      || ch == PMATH_CHAR_PIECEWISE;*/
}

/**\brief Get the corresponding right bracket to a given left bracket or 0.
 */
static PMATH_INLINE uint16_t pmath_right_fence(uint16_t left){
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
  
  return 0;
  /*switch(left){
    case '(': return ')';
    case '[': return ']';
    case '{': return '}';
	  case 0x2045: return 0x2046;
    case 0x2308: return 0x2309;
    case 0x230A: return 0x230B;
    case 0x2329: return 0x232A;
    case 0x27E6: return 0x27E7;
    case 0x27E8: return 0x27E9;
    case 0x27EA: return 0x27EB;
  }
  return 0;*/
}

/**\brief Test if a unicode character is a right bracket
 */
static PMATH_INLINE pmath_bool_t pmath_char_is_right(uint16_t ch){
  return PMATH_TOK_RIGHT == pmath_token_analyse(&ch, 1, NULL);
       /*ch == ')'
      || ch == ']'
      || ch == '}'
	    || ch == 0x2046
      || ch == 0x2309
      || ch == 0x230B
      || ch == 0x232A
      || ch == 0x27E7
      || ch == 0x27E9
      || ch == 0x27EB;*/
}

/**\brief Test if a unicode character can be the start of an identifier/name.
 */
static PMATH_INLINE pmath_bool_t pmath_char_is_name(uint16_t ch){
  return PMATH_TOK_NAME == pmath_token_analyse(&ch, 1, NULL);
}

/**\brief Test if a unicode character is an integral.
 */
static PMATH_INLINE pmath_bool_t pmath_char_is_integral(uint16_t ch){
  return PMATH_TOK_INTEGRAL == pmath_token_analyse(&ch, 1, NULL);
}

/**\brief Test if a token may be a big operator
 */
static PMATH_INLINE pmath_bool_t pmath_token_maybe_bigop(pmath_token_t tok){
  return tok == PMATH_TOK_BINARY_LEFT
      || tok == PMATH_TOK_BINARY_RIGHT
      || tok == PMATH_TOK_BINARY_LEFT_OR_PREFIX
      || tok == PMATH_TOK_NARY
      || tok == PMATH_TOK_NARY_OR_PREFIX
      || tok == PMATH_TOK_POSTFIX_OR_PREFIX
      || tok == PMATH_TOK_PREFIX;
}

/**\brief Test if a unicode character may be a big operation, e.g. Union, Sum.
 */
static PMATH_INLINE pmath_bool_t pmath_char_maybe_bigop(uint16_t ch){
  return pmath_token_maybe_bigop(pmath_token_analyse(&ch, 1, NULL));
  /*return (ch >= 0x2200 && ch <= 0x2204)
      || (ch >= 0x220F && ch <= 0x2211)
      
      || (ch >= 0x22C0 && ch <= 0x22C3)
      || (ch >= 0x2A00 && ch <= 0x2A0B)
      || (ch >= 0x2A1D && ch <= 0x2A65)
      
      || (ch >= 0x2227 && ch <= 0x222A)
      || (ch >= 0x228C && ch <= 0x228E)
      || (ch >= 0x2295 && ch <= 0x22A1)
      || (ch >= 0x22BB && ch <= 0x22BD)
      ||  ch == 0x00D7;*/
}

/**\brief Test if a unicode character is a digit '0' - '9'.
 */
static PMATH_INLINE pmath_bool_t pmath_char_is_digit(uint16_t ch){
  return ch >= '0' && ch <= '9';
}

/**\brief Test if a unicode character is a base-36 digit '0' - '9', 'a' - 'z', 
          'A' - 'Z'.
 */
static PMATH_INLINE pmath_bool_t pmath_char_is_36digit(uint16_t ch){
  return (ch >= '0' && ch <= '9') 
      || (ch >= 'a' && ch <= 'z') 
      || (ch >= 'A' && ch <= 'Z');
}

/**\brief Test if in a given base, a unicode character is a digit.
 */
static PMATH_INLINE pmath_bool_t pmath_char_is_basedigit(int base, uint16_t ch){
  return (base <= 10 && ch >= '0' && ch < '0' + base) 
      || (base > 10 
       && ((ch >= '0' && ch <= '9')
        || (ch >= 'a' && ch <= 'a' + base - 10)
        || (ch >= 'A' && ch <= 'A' + base - 10)));
}

/**\brief Test if a unicode character is a hexadecimal digit.
 */
static PMATH_INLINE pmath_bool_t pmath_char_is_hexdigit(uint16_t ch){
  return (ch >= '0' && ch <= '9') 
      || (ch >= 'a' && ch <= 'f') 
      || (ch >= 'A' && ch <= 'F');
}

/** @} */

#endif /* __PMATH_LANGUAGE__TOKENS_H__ */
