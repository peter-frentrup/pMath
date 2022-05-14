#include <pmath-language/tokens.h>

struct char_info_t {
  int16_t prec;
  int8_t  tok;
};

#define ERR  {PMATH_PREC_ANY,      PMATH_TOK_NONE}
#define ___  {PMATH_PREC_ANY,      PMATH_TOK_SPACE}
#define FAC  {PMATH_PREC_FAC,      PMATH_TOK_POSTFIX_OR_PREFIX}
#define STR  {PMATH_PREC_PRIM,     PMATH_TOK_STRING}
#define ARG  {PMATH_PREC_PRIM,     PMATH_TOK_SLOT}
#define ID   {PMATH_PREC_PRIM,     PMATH_TOK_NAME}
#define ID2  {PMATH_PREC_PRIM,     PMATH_TOK_NAME2}
#define PW   {PMATH_PREC_PRIM,     PMATH_TOK_PREFIX}
#define FUN  {PMATH_PREC_FUNC,     PMATH_TOK_POSTFIX}
#define DIF  {PMATH_PREC_DIFF,     PMATH_TOK_POSTFIX}
#define LCL  {PMATH_PREC_CALL,     PMATH_TOK_LEFTCALL}
#define LEF  {PMATH_PREC_ANY,      PMATH_TOK_LEFT}
#define RI1  {-1,                  PMATH_TOK_RIGHT}
#define RI2  {-2,                  PMATH_TOK_RIGHT}
#define MUL  {PMATH_PREC_MUL,      PMATH_TOK_NARY}
#define ADD  {PMATH_PREC_ADD,      PMATH_TOK_NARY_OR_PREFIX}
#define SEQ  {PMATH_PREC_SEQ,      PMATH_TOK_NARY_AUTOARG}
#define DOT  {PMATH_PREC_CALL,     PMATH_TOK_CALL}
#define DIV  {PMATH_PREC_DIV,      PMATH_TOK_NARY}
#define NUM  {PMATH_PREC_PRIM,     PMATH_TOK_DIGIT}
#define COL  {PMATH_PREC_MODY,     PMATH_TOK_COLON}
#define EVA  {PMATH_PREC_EVAL,     PMATH_TOK_NARY_AUTOARG}
#define EV_  {PMATH_PREC_EVAL,     PMATH_TOK_NEWLINE}
#define REL  {PMATH_PREC_REL,      PMATH_TOK_NARY}
#define QUE  {PMATH_PREC_TEST,     PMATH_TOK_QUESTION}
#define APL  {PMATH_PREC_APL,      PMATH_TOK_BINARY_RIGHT}
#define ALT  {PMATH_PREC_ALT,      PMATH_TOK_NARY}
#define POW  {PMATH_PREC_POW,      PMATH_TOK_BINARY_RIGHT}
#define TIL  {PMATH_PREC_CALL,     PMATH_TOK_TILDES}
#define NOT  {PMATH_PREC_REL,      PMATH_TOK_PREFIX}
#define AD2  {PMATH_PREC_PLUMI,    PMATH_TOK_BINARY_LEFT}
#define MID  {PMATH_PREC_MIDDOT,   PMATH_TOK_NARY}
#define ARR  {PMATH_PREC_ARROW,    PMATH_TOK_NARY}
#define RAR  {PMATH_PREC_ARROW,    PMATH_TOK_BINARY_RIGHT}
#define ALL  {PMATH_PREC_REL,      PMATH_TOK_PREFIX}
#define PAR  {PMATH_PREC_POW,      PMATH_TOK_PREFIX}
#define BIG  {PMATH_PREC_CIRCADD,  PMATH_TOK_PREFIX}
#define MU2  {PMATH_PREC_MUL2,     PMATH_TOK_NARY}
#define AND  {PMATH_PREC_AND,      PMATH_TOK_NARY}
#define OR   {PMATH_PREC_OR,       PMATH_TOK_NARY}
#define XOR  {PMATH_PREC_XOR,      PMATH_TOK_NARY}
#define ISC  {PMATH_PREC_ISECT,    PMATH_TOK_NARY}
#define UNI  {PMATH_PREC_UNION,    PMATH_TOK_NARY}
#define INT  {PMATH_PREC_MUL,      PMATH_TOK_INTEGRAL}
#define AD3  {PMATH_PREC_CIRCADD,  PMATH_TOK_NARY}
#define MU3  {PMATH_PREC_CIRCMUL,  PMATH_TOK_NARY}
#define RUL  {PMATH_PREC_RULE,     PMATH_TOK_BINARY_RIGHT}
#define ASS  {PMATH_PREC_ASS,      PMATH_TOK_BINARY_RIGHT}
#define CRX  {PMATH_PREC_CROSS,    PMATH_TOK_NARY}
#define DID  {PMATH_PREC_DIV,      PMATH_TOK_PREFIX}
#define AD_  {PMATH_PREC_INVISADD, PMATH_TOK_NARY_OR_PREFIX}

static const struct char_info_t u0000_u00ff[256] = {
  /*         0    1    2    3    4    5    6    7    8    9    A    B    C    D    E    F  */
  /* 000 */ ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ___, EV_, ___, ___, ___, ERR, ERR,
  /* 001 */ ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR,
  /* 002 */ ___, FAC, STR, ARG, ID,  ERR, FUN, DIF, LCL, RI1, MUL, ADD, SEQ, ADD, DOT, DIV,
  /* 003 */ NUM, NUM, NUM, NUM, NUM, NUM, NUM, NUM, NUM, NUM, COL, EVA, REL, REL, REL, QUE,
  /* 004 */ APL, ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,
  /* 005 */ ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  LCL, STR, RI2, POW, /*ERR*/ID,
  /* 006 */ ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,
  /* 007 */ ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  LEF, ALT, RI2, TIL, ERR,
  /* 008 */ ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR,
  /* 009 */ ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR,
  /* 00A */ ___, ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID2, NOT, ID,  ID,  ID,
  /* 00B */ ID2, AD2, DIF, DIF, ID,  ID,  ID,  MID, ID,  DIF, ID,  ID2, ID2, ID2, ID2, ID,
  /* 00C */ ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,
  /* 00D */ ID,  ID,  ID,  ID,  ID,  ID,  ID,  MUL, ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,
  /* 00E */ ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,
  /* 00F */ ID,  ID,  ID,  ID,  ID,  ID,  ID,  DIV, ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID
};

static const struct char_info_t u2000_u206f[112] = {
  /*         0    1    2    3    4    5    6    7    8    9    A    B    C    D    E    F  */
  /* 200 */ ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___,
  /* 201 */ ID2, ID2, ID2, ID2, ID2, ID2, ID2, ID2, ID2, ID2, ID2, ID2, ID2, ID2, ID2, ID2,
  /* 202 */ ID2, ID2, ID2, ID2, ID2, ID2, ID2, ID2, ID2, ID2, ID2, ID2, ID2, ID2, ID2, ID2,
  /* 203 */ ID2, ID2, ID,  ID,  ID,  ID,  ID,  ID,  ID2, LEF, RI1, ID2, ID2, ID2, ID2, ID2,
  /* 204 */ ID2, ID2, ID2, ID2, DIV, LEF, RI1, ID2, ID2, ID2, ID2, ID2, ID2, ID2, ID2, ID2,
  /* 205 */ ID2, ID2, ID2, ID2, ID2, ID2, ID2, ID,  ID2, ID2, ID2, ID2, ID2, ID2, ID2, ID2,
  /* 206 */ ID,  APL, MUL, SEQ, AD_, ERR, ERR, ERR, ERR, ERR, ID,  ID,  ID,  ID,  ID,  ID
};

static const struct char_info_t u2100_u230f[528] = {
  /*         0    1    2    3    4    5    6    7    8    9    A    B    C    D    E    F  */
  /* 210 */ ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,
  /* 211 */ ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,
  /* 212 */ ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,
  /* 213 */ ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,
  /* 214 */ ID,  ID,  ID,  ID,  ID,  DID, DID, ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,
  /* 215 */ ERR, ERR, ERR, ID2, ID2, ID2, ID2, ID2, ID2, ID2, ID2, ID2, ID2, ID2, ID2, ID2,
  /* 216 */ ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,
  /* 217 */ ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,
  /* 218 */ ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,
  /* 219 */ ARR, ARR, RUL, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR,
  /* 21A */ ARR, ARR, ARR, ARR, ARR, ARR, RAR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR,
  /* 21B */ ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR,
  /* 21C */ ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR,
  /* 21D */ ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR,
  /* 21E */ ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR,
  /* 21F */ ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR,
  /* 220 */ ALL, ID2, PAR, ALL, ALL, ID2, PAR, REL, REL, REL, REL, REL, REL, REL, REL, BIG,
  /* 221 */ BIG, BIG, ADD, AD2, AD2, DIV, DIV, MU2, MU2, MU2, PAR, PAR, PAR, REL, ID,  ID,
  /* 222 */ ID,  ID,  ID,  REL, REL, REL, REL, AND, OR,  ISC, UNI, INT, INT, INT, INT, INT,
  /* 223 */ INT, INT, INT, INT, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL,
  /* 224 */ REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL,
  /* 225 */ REL, REL, REL, REL, ASS, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL,
  /* 226 */ REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL,
  /* 227 */ REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL,
  /* 228 */ REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, BIG, BIG, BIG, REL,
  /* 229 */ REL, REL, REL, REL, REL, AD3, AD3, MU3, MU3, MU3, MU3, MU3, REL, AD3, AD3, AD3,
  /* 22A */ MU3, MU3, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL,
  /* 22B */ REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, XOR, AND, OR,  ID,  ID,
  /* 22C */ AND, OR,  ISC, UNI, MU2, MU2, MU2, MU2, REL, REL, REL, REL, REL, REL, OR,  AND,
  /* 22D */ REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL,
  /* 22E */ REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL,
  /* 22F */ REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL,
  /* 230 */ ID,  REL, ID,  REL, REL, REL, REL, REL, LEF, RI1, LEF, RI1, ID,  ID,  ID,  ID
};

static const struct char_info_t u27c0_u27ff[64] = {
  /*         0    1    2    3    4    5    6    7    8    9    A    B    C    D    E    F  */
  /* 27C */ ID2, ID2, REL, REL, REL, LEF, RI1, OR,  REL, REL, REL, ERR, DIV, ERR, ERR, ERR,
  /* 27D */ REL, AND, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL,
  /* 27E */ REL, REL, REL, REL, REL, REL, LCL, RI1, LEF, RI1, LEF, RI1, LEF, RI1, LEF, RI1,
  /* 27F */ ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR
};

static const struct char_info_t u2900_u2aff[512] = {
  /*         0    1    2    3    4    5    6    7    8    9    A    B    C    D    E    F  */
  /* 290 */ ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR,
  /* 291 */ ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR,
  /* 292 */ ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR,
  /* 293 */ ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR,
  /* 294 */ ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR,
  /* 295 */ ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR,
  /* 296 */ ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR,
  /* 297 */ ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR, ARR,
  /* 298 */ ARR, REL, REL, LEF, RI1, LEF, RI1, LEF, RI1, LEF, RI1, LEF, RI1, LEF, RI1, LEF,
  /* 299 */ RI1, LEF, RI1, LEF, RI1, LEF, RI1, LEF, RI1, REL, ID2, ID2, ID2, ID2, ID2, ID2,
  /* 29A */ ID2, ID2, ID2, ID2, ID2, ID2, ID2, ID2, ID2, ID2, ID2, ID2, ID2, ID2, ID2, ID2,
  /* 29B */ ID2, ID2, ID2, ID2, ID2, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL,
  /* 29C */ REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL,
  /* 29D */ REL, REL, REL, REL, REL, REL, REL, REL, LEF, RI1, LEF, RI1, REL, REL, REL, REL,
  /* 29E */ REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL,
  /* 29F */ REL, REL, REL, REL, RUL, REL, REL, REL, REL, REL, REL, REL, LEF, RI1, REL, REL,
  /* 2A0 */ MU3, AD3, MU3, UNI, UNI, ISC, UNI, AND, OR,  BIG, BIG, INT, INT, INT, INT, INT,
  /* 2A1 */ INT, INT, INT, INT, INT, INT, INT, INT, INT, INT, INT, INT, INT, REL, REL, ID,
  /* 2A2 */ REL, REL, REL, REL, REL, REL, REL, REL, LEF, RI1, LEF, RI1, REL, REL, REL, CRX,
  /* 2A3 */ MU2, MU2, MU2, MU2, MU2, MU2, MU2, MU2, MU2, MU2, MU2, MU2, MU2, MU2, ID,  BIG,
  /* 2A4 */ ISC, UNI, UNI, ISC, ISC, UNI, UNI, UNI, UNI, UNI, UNI, ISC, UNI, ISC, ISC, UNI,
  /* 2A5 */ UNI, AND, OR,  AND, OR,  AND, OR,  OR,  AND, AND, AND, OR,  AND, OR,  AND, AND,
  /* 2A6 */ AND, OR,  OR,  OR,  REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL,
  /* 2A7 */ REL, REL, REL, REL, ASS, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL,
  /* 2A8 */ REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL,
  /* 2A9 */ REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL,
  /* 2AA */ REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL,
  /* 2AB */ REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL,
  /* 2AC */ REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL,
  /* 2AD */ REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL,
  /* 2AE */ REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL,
  /* 2AF */ REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL, REL
};

static const struct char_info_t uf360_uf36f[16] = {
  /*         0    1    2    3    4    5    6    7    8    9    A    B    C    D    E    F  */
  /* F36 */ ID,  PW,  LEF, RI1, ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID
};

static const struct char_info_t uf600_uf60f[16] = {
  /*         0    1    2    3    4    5    6    7    8    9    A    B    C    D    E    F  */
  /* F60 */ ID,  ID,  ID,  LEF, RI1, LEF, RI1, ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID
};

static const struct char_info_t *find_char_info(uint16_t ch) {
  static const struct char_info_t id  = ID;
  static const struct char_info_t id2 = ID2;
  
  if(ch <= 0x00FF)
    return &u0000_u00ff[ch];
    
  if(ch >= 0x2000 && ch <= 0x206F)
    return &u2000_u206f[ch - 0x2000];
    
  if(ch >= 0x2100 && ch <= 0x230F)
    return &u2100_u230f[ch - 0x2100];
    
  if(ch >= 0x27C0 && ch <= 0x27FF)
    return &u27c0_u27ff[ch - 0x27C0];
    
  if(ch >= 0x2900 && ch <= 0x2AFF)
    return &u2900_u2aff[ch - 0x2900];
    
  if(ch >= 0xF360 && ch <= 0xF36F)
    return &uf360_uf36f[ch - 0xF360];
    
  if(ch >= 0xF600 && ch <= 0xF60F)
    return &uf600_uf60f[ch - 0xF600];
    
  switch(ch) {
    case PMATH_CHAR_BOX:
    case PMATH_CHAR_LEFT_BOX:
    case PMATH_CHAR_RIGHT_BOX: 
    case PMATH_CHAR_PLACEHOLDER:
    case PMATH_CHAR_SELECTIONPLACEHOLDER: return &id2;
  }
  
  return &id;
}

PMATH_API pmath_token_t pmath_token_analyse(
  const uint16_t *str,
  int             len,
  int            *prec
) {
  const struct char_info_t *info;
  int dummy_prec;
  
  if(prec)
    *prec = PMATH_PREC_ANY;
  else
    prec = &dummy_prec;
    
  if(len <= 0)
    return PMATH_TOK_NONE;
    
  if(len == 1) {
    info = find_char_info(*str);
    *prec = info->prec;
    return (pmath_token_t)info->tok;
  }
  
  switch(*str) {
    case '+': {
        if(str[1] == '+') { // ++
          *prec = PMATH_PREC_STR;
          return PMATH_TOK_PLUSPLUS;//PMATH_TOK_NARY_OR_PREFIX;
        }
        
        if(str[1] == '=') { // +=
          *prec = PMATH_PREC_MODY;
          return PMATH_TOK_BINARY_RIGHT;
        }
      } return PMATH_TOK_NONE;
      
    case '-': {
        if(str[1] == '-') { // --
          *prec = PMATH_PREC_INC; // +1
          return PMATH_TOK_POSTFIX_OR_PREFIX;
        }
        
        if(str[1] == '=') { // -=
          *prec = PMATH_PREC_MODY;
          return PMATH_TOK_BINARY_RIGHT;
        }
        
        if(str[1] == '>') { // ->
          *prec = PMATH_PREC_RULE;
          return PMATH_TOK_BINARY_RIGHT;
        }
      } return PMATH_TOK_NONE;
      
    case ':': {
        if(str[1] == '=') { // :=
          *prec = PMATH_PREC_ASS;
          return PMATH_TOK_BINARY_RIGHT;
        }
        
        if(str[1] == '>') { // :>
          *prec = PMATH_PREC_RULE;
          return PMATH_TOK_BINARY_RIGHT;
        }
        
        if(str[1] == ':') {
          if(len == 2) { // ::
            *prec = PMATH_PREC_CALL;
            return PMATH_TOK_BINARY_LEFT;
          }
          
          *prec = PMATH_PREC_ASS;
          return PMATH_TOK_BINARY_RIGHT; // ::=
        }
      } return PMATH_TOK_NONE;
      
    case '<': {
        if(str[1] == '<') { // <<
          *prec = PMATH_PREC_PRIM;
          return PMATH_TOK_PRETEXT;
        }
        
        if(str[1] == '=') { // <=
          *prec = PMATH_PREC_REL;
          return PMATH_TOK_NARY;
        }
      } return PMATH_TOK_NONE;
      
    case '>': {
        if(str[1] == '=') { // >=
          *prec = PMATH_PREC_REL;
          return PMATH_TOK_NARY;
        }
      } return PMATH_TOK_NONE;
      
    case '!': {
        if(str[1] == '=') { // !=
          *prec = PMATH_PREC_REL;
          return PMATH_TOK_NARY;
        }
        
        if(str[1] == '!') { // !!
          *prec = PMATH_PREC_FAC;
          return PMATH_TOK_POSTFIX;
        }
      } return PMATH_TOK_NONE;
      
    case '=': {
        if(str[1] == '>') { // =>
          *prec = PMATH_PREC_ARROW;
          return PMATH_TOK_NARY;
        }
        
        if( str[1] == '!' || // =!=
            str[1] == '=')   // ===
        {
          *prec = PMATH_PREC_REL;
          return PMATH_TOK_NARY;
        }
      } return PMATH_TOK_NONE;
      
    case '*': {
        if(str[1] == '=') { // *=
          *prec = PMATH_PREC_MODY;
          return PMATH_TOK_BINARY_RIGHT;
        }
        
        if(str[1] == '*') { // **
          *prec = PMATH_PREC_REPEAT;
          return PMATH_TOK_POSTFIX;
        }
      } return PMATH_TOK_COMMENTEND; // */
      
    case '/': {
        if(str[1] == ':') { // /:
          *prec = PMATH_PREC_ASS;
          return PMATH_TOK_ASSIGNTAG;
        }
        
        if(str[1] == '=') { // /=
          *prec = PMATH_PREC_MODY;
          return PMATH_TOK_BINARY_RIGHT;
        }
        
        if(str[1] == '/') { // //
          if(len == 2) {
            *prec = PMATH_PREC_LAZY;
            return PMATH_TOK_BINARY_LEFT;
          }
          
          if(str[2] == '.') { // //.
            *prec = PMATH_PREC_REPL;
            return PMATH_TOK_BINARY_RIGHT;
          }
          
          if(str[2] == '@') { // //@
            *prec = PMATH_PREC_MAP;
            return PMATH_TOK_BINARY_RIGHT;
          }
          
          if(str[2] == '=') { // //=
            *prec = PMATH_PREC_MODY;
            return PMATH_TOK_BINARY_RIGHT;
          }
          
          return PMATH_TOK_NONE;
        }
        
        if(str[1] == '.') { // /.
          *prec = PMATH_PREC_REPL;
          return PMATH_TOK_BINARY_RIGHT;
        }
        
        if(str[1] == '@') { // /@
          *prec = PMATH_PREC_MAP;
          return PMATH_TOK_BINARY_RIGHT;
        }
        
        if(str[1] == '\\') { // /\/
          *prec = PMATH_PREC_PRIM;
          return PMATH_TOK_NAME2;
        }
        
        if(str[1] == '?') { // /?
          *prec = PMATH_PREC_COND;
          return PMATH_TOK_BINARY_LEFT;
        }
      } return PMATH_TOK_NONE;
      
    case '.': // ..
      *prec = PMATH_PREC_RANGE;
      return PMATH_TOK_NARY_AUTOARG;
      
    case '|': //  ||  |->  |>
      if(len == 3 && str[1] == '-' && str[2] == '>') { //  |->
        *prec = PMATH_PREC_ARROW;
        return PMATH_TOK_BINARY_RIGHT;
      }
      if(len == 2 && str[1] == '>') { //  |>
        *prec = PMATH_PREC_ARROW;
        //return PMATH_TOK_CALL;//PMATH_TOK_NARY;
        return PMATH_TOK_CALLPIPE;
      }
      *prec = PMATH_PREC_OR;
      return PMATH_TOK_NARY;
      
    case '&': // &&
      *prec = PMATH_PREC_AND;
      return PMATH_TOK_NARY;
      
    case '@': // @@
      *prec = PMATH_PREC_APL;
      return PMATH_TOK_BINARY_RIGHT;
      
    case '?': // ??
      return PMATH_TOK_PRETEXT;
  }
  
  info = find_char_info(*str);
  *prec = info->prec;
  return info->tok;
}

PMATH_API int pmath_token_prefix_precedence(
  const uint16_t *str,
  int             len,
  int             defprec
) {
  if(len == 1) {
    switch(str[0]) {
      case '!': return PMATH_PREC_REL;
    }
  }
  else if(len == 2) {
    switch(str[0]) {
      case '+':
        if(str[1] == '+')
          return PMATH_PREC_INC;
        break;
        
      case '-':
        if(str[1] == '-')
          return PMATH_PREC_INC;
        break;
        
      case '.': // ..
        return PMATH_PREC_ADD;
    }
  }
  
  switch(defprec) {
    case PMATH_PREC_ADD:
    case PMATH_PREC_PLUMI: return PMATH_PREC_DIV + 1;
  }
  
  return defprec + 1;
}
