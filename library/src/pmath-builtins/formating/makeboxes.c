#include <pmath-core/numbers-private.h>
#include <pmath-core/symbols-private.h>

#include <pmath-language/patterns-private.h>
#include <pmath-language/tokens.h>

#include <pmath-util/concurrency/threads-private.h>
#include <pmath-util/debug.h>
#include <pmath-util/emit-and-gather.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>
#include <pmath-util/symbol-values-private.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/control/definitions-private.h>
#include <pmath-builtins/control-private.h>
#include <pmath-builtins/formating-private.h>
#include <pmath-builtins/lists-private.h>

#include <stdio.h>
#include <string.h>

#ifdef _MSC_VER
  #define snprintf sprintf_s
#endif

//{ operator precedence of boxes ...

static pmath_token_t box_token_analyse(pmath_t box, int *prec){ // box will be freed
  if(pmath_is_string(box)){
    pmath_token_t tok = pmath_token_analyse(
      pmath_string_buffer(box),
      pmath_string_length(box),
      prec);
    
    pmath_unref(box);
    return tok;
  }
  
  if(pmath_is_expr_of_len(box, PMATH_SYMBOL_LIST, 1)
  || pmath_is_expr_of(box, PMATH_SYMBOL_OVERSCRIPTBOX)
  || pmath_is_expr_of(box, PMATH_SYMBOL_UNDERSCRIPTBOX)
  || pmath_is_expr_of(box, PMATH_SYMBOL_UNDEROVERSCRIPTBOX)
  || pmath_is_expr_of(box, PMATH_SYMBOL_STYLEBOX)
  || pmath_is_expr_of(box, PMATH_SYMBOL_TAGBOX)
  || pmath_is_expr_of(box, PMATH_SYMBOL_INTERPRETATIONBOX)){
    pmath_token_t tok = box_token_analyse(pmath_expr_get_item(box, 1), prec);
    pmath_unref(box);
    return tok;
  }
  
  if(pmath_is_expr_of(box, PMATH_SYMBOL_SUBSCRIPTBOX)
  || pmath_is_expr_of(box, PMATH_SYMBOL_SUBSUPERSCRIPTBOX)
  || pmath_is_expr_of(box, PMATH_SYMBOL_SUPERSCRIPTBOX)){
    pmath_unref(box);
    *prec = PMATH_PREC_PRIM;//PMATH_PREC_POW;
    return PMATH_TOK_BINARY_RIGHT;
  }
  
  pmath_unref(box);
  *prec = PMATH_PREC_ANY;
  return PMATH_TOK_NAME2;
}

static int box_token_prefix_prec(pmath_t box, int defprec){ // box will be freed
  if(pmath_is_string(box)){
    int prec = pmath_token_prefix_precedence(
      pmath_string_buffer(box),
      pmath_string_length(box),
      defprec);
    
    pmath_unref(box);
    return prec;
  }
  
  if(pmath_is_expr_of_len(box, PMATH_SYMBOL_LIST, 1)
  || pmath_is_expr_of_len(box, PMATH_SYMBOL_OVERSCRIPTBOX, 2)
  || pmath_is_expr_of_len(box, PMATH_SYMBOL_UNDERSCRIPTBOX, 2)
  || pmath_is_expr_of_len(box, PMATH_SYMBOL_UNDEROVERSCRIPTBOX, 3)
  || pmath_is_expr_of(box, PMATH_SYMBOL_STYLEBOX)
  || pmath_is_expr_of(box, PMATH_SYMBOL_TAGBOX)
  || pmath_is_expr_of(box, PMATH_SYMBOL_INTERPRETATIONBOX)){
    int prec = box_token_prefix_prec(pmath_expr_get_item(box, 1), defprec);
    pmath_unref(box);
    return prec;
  }
  
  return defprec+1;
}


// pos = -1 => Prefix
// pos =  0 => Infix/Other
// pos = +1 => Postfix
static int expr_precedence(pmath_t box, int *pos){ // box wont be freed
  pmath_token_t tok;
  int prec;
  
  *pos = 0; // infix or other by default
  
  if(pmath_is_expr_of(box, PMATH_SYMBOL_LIST)){
    size_t len = pmath_expr_length(box);
    
    if(len <= 1){
      pmath_t item = pmath_expr_get_item(box, 1);
      prec = expr_precedence(item, pos);
      pmath_unref(item);
      return prec;
    }
    
    tok = box_token_analyse(pmath_expr_get_item(box, 1), &prec);
    switch(tok){
      case PMATH_TOK_NONE:
      case PMATH_TOK_SPACE:
      case PMATH_TOK_DIGIT:
      case PMATH_TOK_STRING:
      case PMATH_TOK_NAME:
      case PMATH_TOK_NAME2:
      case PMATH_TOK_SLOT:
        break;
      
      case PMATH_TOK_BINARY_LEFT_OR_PREFIX:
      case PMATH_TOK_NARY_OR_PREFIX:
      case PMATH_TOK_POSTFIX_OR_PREFIX:
      case PMATH_TOK_PLUSPLUS:
        prec = box_token_prefix_prec(pmath_expr_get_item(box, 1), prec);
        if(len == 2){
          *pos = -1;
          goto FINISH;
        }
        return prec;
        
      case PMATH_TOK_BINARY_LEFT:
      case PMATH_TOK_BINARY_RIGHT:
      case PMATH_TOK_BINARY_LEFT_AUTOARG:
      case PMATH_TOK_NARY:
      case PMATH_TOK_NARY_AUTOARG:
      case PMATH_TOK_PREFIX:
      case PMATH_TOK_POSTFIX:
      case PMATH_TOK_CALL:
      case PMATH_TOK_ASSIGNTAG:
      case PMATH_TOK_COLON:
      case PMATH_TOK_TILDES:
      case PMATH_TOK_INTEGRAL:
        if(len == 2){
          *pos = -1;
          goto FINISH;
        }
        return prec;
      
      case PMATH_TOK_LEFTCALL:
      case PMATH_TOK_LEFT:
      case PMATH_TOK_RIGHT:
      case PMATH_TOK_COMMENTEND:
        return PMATH_PREC_PRIM;
      
      case PMATH_TOK_PRETEXT:
      case PMATH_TOK_QUESTION:
        *pos = -1;
        if(len == 4)
          prec = PMATH_PREC_CIRCMUL;
        
        goto FINISH;
    }
    
    tok = box_token_analyse(pmath_expr_get_item(box, 2), &prec);
    switch(tok){
      case PMATH_TOK_NONE:
      case PMATH_TOK_SPACE:
      case PMATH_TOK_DIGIT:
      case PMATH_TOK_STRING:
      case PMATH_TOK_NAME:
      case PMATH_TOK_NAME2:
      case PMATH_TOK_LEFT:
      case PMATH_TOK_PREFIX:
      case PMATH_TOK_PRETEXT:
      case PMATH_TOK_TILDES:
      case PMATH_TOK_SLOT:
      case PMATH_TOK_INTEGRAL:
        prec = PMATH_PREC_MUL;
        break;
      
      case PMATH_TOK_CALL:
      case PMATH_TOK_LEFTCALL:
        prec = PMATH_PREC_CALL;
        break;
      
      case PMATH_TOK_RIGHT:
      case PMATH_TOK_COMMENTEND:
        prec = PMATH_PREC_PRIM;
        break;
        
      case PMATH_TOK_BINARY_LEFT:
      case PMATH_TOK_BINARY_LEFT_AUTOARG:
      case PMATH_TOK_BINARY_LEFT_OR_PREFIX:
      case PMATH_TOK_QUESTION:
      case PMATH_TOK_BINARY_RIGHT:
      case PMATH_TOK_ASSIGNTAG:
      case PMATH_TOK_NARY:
      case PMATH_TOK_NARY_AUTOARG:
      case PMATH_TOK_NARY_OR_PREFIX:
      case PMATH_TOK_COLON:
        //prec = prec;  // TODO: what did i mean here?
        break;
      
      case PMATH_TOK_POSTFIX_OR_PREFIX:
      case PMATH_TOK_POSTFIX:
        if(len == 2) 
          *pos = +1;
        prec = prec;
        break;
        
      case PMATH_TOK_PLUSPLUS:
        if(len == 2){
          *pos = +1;
          prec = PMATH_PREC_INC;
        }
        else{
          prec = PMATH_PREC_STR;
        }
        break;
    }
    
   FINISH:
    if(*pos >= 0){ // infix or postfix
      int prec2, pos2;
      pmath_t item;
      
      item = pmath_expr_get_item(box, 1);
      prec2 = expr_precedence(item, &pos2);
      pmath_unref(item);
      
      if(pos2 > 0 && prec2 < prec){ // starts with postfix operator, eg. box = a+b&*c = ((a+b)&)*c
        prec = prec2;
        *pos = 0;
      }
    }
    
    if(*pos <= 0){ // prefix or infix
      int prec2, pos2;
      pmath_t item;
      
      item = pmath_expr_get_item(box, pmath_expr_length(box));
      prec2 = expr_precedence(item, &pos2);
      pmath_unref(item);
      
      if(pos2 < 0 && prec2 < prec){ // ends with prefix operator, eg. box = a*!b+c = a*(!(b+c))
        prec = prec2;
        *pos = 0;
      }
    }
    
    return prec;
  }
  
  if(pmath_is_expr_of(box, PMATH_SYMBOL_STYLEBOX)
  || pmath_is_expr_of(box, PMATH_SYMBOL_TAGBOX)
  || pmath_is_expr_of(box, PMATH_SYMBOL_INTERPRETATIONBOX)){
    pmath_t item = pmath_expr_get_item(box, 1);
    tok = expr_precedence(item, pos);
    pmath_unref(item);
    return tok;
  }
  
  return PMATH_PREC_PRIM;
}

// pos = -1: box at start => allow any Postfix operator
// pos = +1: box at end   => allow any Prefix operator
static pmath_t ensure_min_precedence(pmath_t box, int minprec, int pos){
  int expr_pos;
  int p = expr_precedence(box, &expr_pos);
  
  if(p < minprec){
    if(pos < 0 && expr_pos > 0)
      return box;
    
    if(pos > 0 && expr_pos < 0)
      return box;
    
    return pmath_build_value("(sos)", "(", box, ")");
  }
  
  return box;
}

//} ... operator precedence of boxes

//{ boxforms for simple functions ...

static pmath_t relation(pmath_symbol_t head, int boxform){ // head wont be freed
#define  RET_CH(C)  do{ int         retc = (C); return pmath_build_value("c", retc); }while(0)
#define  RET_ST(S)  do{ const char *rets = (S); return pmath_build_value("s", rets); }while(0)

  if(pmath_same(head, PMATH_SYMBOL_EQUAL))        RET_CH('=');
  if(pmath_same(head, PMATH_SYMBOL_LESS))         RET_CH('<');
  if(pmath_same(head, PMATH_SYMBOL_GREATER))      RET_CH('>');
  if(pmath_same(head, PMATH_SYMBOL_IDENTICAL))    RET_ST("===");
  if(pmath_same(head, PMATH_SYMBOL_UNIDENTICAL))  RET_ST("=!=");

  if(boxform < BOXFORM_OUTPUT){
    if(pmath_same(head, PMATH_SYMBOL_UNEQUAL))       RET_CH(0x2260);
    if(pmath_same(head, PMATH_SYMBOL_LESSEQUAL))     RET_CH(0x2264);
    if(pmath_same(head, PMATH_SYMBOL_GREATEREQUAL))  RET_CH(0x2265);
    if(pmath_same(head, PMATH_SYMBOL_COLON))         RET_CH(0x2236);
    
    if(pmath_same(head, PMATH_SYMBOL_ELEMENT))           RET_CH(0x2208);
    if(pmath_same(head, PMATH_SYMBOL_NOTELEMENT))        RET_CH(0x2209);
    if(pmath_same(head, PMATH_SYMBOL_REVERSEELEMENT))    RET_CH(0x220B);
    if(pmath_same(head, PMATH_SYMBOL_NOTREVERSEELEMENT)) RET_CH(0x220C);
    
    if(pmath_same(head, PMATH_SYMBOL_SUBSET))           RET_CH(0x2282);
    if(pmath_same(head, PMATH_SYMBOL_SUPERSET))         RET_CH(0x2283);
    if(pmath_same(head, PMATH_SYMBOL_NOTSUBSET))        RET_CH(0x2284);
    if(pmath_same(head, PMATH_SYMBOL_NOTSUPERSET))      RET_CH(0x2285);
    if(pmath_same(head, PMATH_SYMBOL_SUBSETEQUAL))      RET_CH(0x2286);
    if(pmath_same(head, PMATH_SYMBOL_SUPERSETEQUAL))    RET_CH(0x2287);
    if(pmath_same(head, PMATH_SYMBOL_NOTSUBSETEQUAL))   RET_CH(0x2288);
    if(pmath_same(head, PMATH_SYMBOL_NOTSUPERSETEQUAL)) RET_CH(0x2289);
  }
  else{
    if(pmath_same(head, PMATH_SYMBOL_UNEQUAL))       RET_ST("!=");
    if(pmath_same(head, PMATH_SYMBOL_LESSEQUAL))     RET_ST("<=");
    if(pmath_same(head, PMATH_SYMBOL_GREATEREQUAL))  RET_ST(">=");
    
    if(boxform < BOXFORM_INPUT){
      if(pmath_same(head, PMATH_SYMBOL_COLON))   RET_CH(':');
    }
  }
  
  return PMATH_NULL;

#undef RET_CH
#undef RET_ST
}

#define  RET_CH(C,P)  do{ int         retc = (C); *prec = (P); return pmath_build_value("c", retc); }while(0)
#define  RET_ST(S,P)  do{ const char *rets = (S); *prec = (P); return pmath_build_value("s", rets); }while(0)
static pmath_t simple_nary(pmath_symbol_t head, int *prec, int boxform){ // head wont be freed
//  if(pmath_same(head, PMATH_SYMBOL_SEQUENCE))            RET_CH(',',  PMATH_PREC_SEQ);
//  if(pmath_same(head, PMATH_SYMBOL_EVALUATIONSEQUENCE))  RET_CH(';',  PMATH_PREC_EVAL);
  if(pmath_same(head, PMATH_SYMBOL_STRINGEXPRESSION))    RET_ST("++", PMATH_PREC_STR);
  if(pmath_same(head, PMATH_SYMBOL_ALTERNATIVES))        RET_CH('|',  PMATH_PREC_ALT);
  
  if(boxform < BOXFORM_OUTPUT){
    if(pmath_same(head, PMATH_SYMBOL_AND))    RET_CH( 0x2227, PMATH_PREC_AND);
    if(pmath_same(head, PMATH_SYMBOL_OR))     RET_CH( 0x2228, PMATH_PREC_AND);
    
    if(pmath_same(head, PMATH_SYMBOL_CIRCLEPLUS))   RET_CH( 0x2A2F, PMATH_PREC_CIRCADD);
    if(pmath_same(head, PMATH_SYMBOL_CIRCLETIMES))  RET_CH( 0x2A2F, PMATH_PREC_CIRCMUL);
    if(pmath_same(head, PMATH_SYMBOL_PLUSMINUS))    RET_CH( 0x00B1, PMATH_PREC_PLUMI);
    if(pmath_same(head, PMATH_SYMBOL_MINUSPLUS))    RET_CH( 0x2213, PMATH_PREC_PLUMI);
    
    if(pmath_same(head, PMATH_SYMBOL_DOT))    RET_CH( 0x22C5, PMATH_PREC_MIDDOT); //0x00B7
    if(pmath_same(head, PMATH_SYMBOL_CROSS))  RET_CH( 0x2A2F, PMATH_PREC_CROSS);
  }
  else{
    if(pmath_same(head, PMATH_SYMBOL_AND))  RET_ST("&&", PMATH_PREC_AND);
    if(pmath_same(head, PMATH_SYMBOL_OR))   RET_ST("||", PMATH_PREC_AND);
  }
  
  *prec = PMATH_PREC_REL;
  return relation(head, boxform);
}

static pmath_t simple_prefix(pmath_symbol_t head, int *prec, int boxform){ // head wont be freed
  
  if(pmath_same(head, PMATH_SYMBOL_INCREMENT))     RET_ST("++", PMATH_PREC_INC);
  if(pmath_same(head, PMATH_SYMBOL_DECREMENT))     RET_ST("--", PMATH_PREC_INC);
  
  if(boxform < BOXFORM_OUTPUT){
    if(pmath_same(head, PMATH_SYMBOL_NOT))  RET_CH( 0x00AC, PMATH_PREC_REL);
    
    if(pmath_same(head, PMATH_SYMBOL_PLUSMINUS))    RET_CH( 0x00B1, PMATH_PREC_PLUMI);
    if(pmath_same(head, PMATH_SYMBOL_MINUSPLUS))    RET_CH( 0x2213, PMATH_PREC_PLUMI);
  }
  else{
    if(pmath_same(head, PMATH_SYMBOL_NOT))  RET_CH('!', PMATH_PREC_REL);
  }
  
  return PMATH_NULL;
}

static pmath_t simple_postfix(pmath_symbol_t head, int *prec, int boxform){ // head wont be freed
  
  if(pmath_same(head, PMATH_SYMBOL_FUNCTION))       RET_CH('&',  PMATH_PREC_FUNC);
  if(pmath_same(head, PMATH_SYMBOL_FACTORIAL))      RET_CH('!',  PMATH_PREC_FAC);
  if(pmath_same(head, PMATH_SYMBOL_FACTORIAL2))     RET_ST("!!", PMATH_PREC_FAC);
  if(pmath_same(head, PMATH_SYMBOL_POSTINCREMENT))  RET_ST("++", PMATH_PREC_INC);
  if(pmath_same(head, PMATH_SYMBOL_POSTDECREMENT))  RET_ST("--", PMATH_PREC_INC);
  
  return PMATH_NULL;
}
#undef RET_CH
#undef RET_ST

static pmath_t simple_binary(pmath_symbol_t head, int *leftprec, int *rightprec, int boxform){ // head wont be freed
#define  RET_CH(C, L, R)  do{ int         retc = (C); *leftprec = (L); *rightprec = (R); return pmath_build_value("c", retc); }while(0)
#define  RET_ST(S, L, R)  do{ const char *rets = (S); *leftprec = (L); *rightprec = (R); return pmath_build_value("s", rets); }while(0)
#define  RET_CH_L(C, L)  RET_CH(C, L, (L)+1)
#define  RET_CH_R(C, R)  RET_CH(C, (R)+1, R)
#define  RET_ST_L(S, L)  RET_ST(S, L, (L)+1)
#define  RET_ST_R(S, R)  RET_ST(S, (R)+1, R)

  if(pmath_same(head, PMATH_SYMBOL_INCREMENT))   RET_ST_R("+=", PMATH_PREC_MODY);
  if(pmath_same(head, PMATH_SYMBOL_DECREMENT))   RET_ST_R("-=", PMATH_PREC_MODY);
  if(pmath_same(head, PMATH_SYMBOL_TIMESBY))     RET_ST_R("*=", PMATH_PREC_MODY);
  if(pmath_same(head, PMATH_SYMBOL_DIVIDEBY))    RET_ST_R("/=", PMATH_PREC_MODY);
  if(pmath_same(head, PMATH_SYMBOL_CONDITION))   RET_ST_L("/?", PMATH_PREC_COND);
  if(pmath_same(head, PMATH_SYMBOL_TESTPATTERN)) RET_CH_L('?',  PMATH_PREC_TEST);
  if(pmath_same(head, PMATH_SYMBOL_MESSAGENAME)) RET_ST_L("::", PMATH_PREC_CALL);
  
  if(boxform < BOXFORM_OUTPUT){
    if(pmath_same(head, PMATH_SYMBOL_ASSIGN))         RET_CH_R( PMATH_CHAR_ASSIGN,        PMATH_PREC_ASS);
    if(pmath_same(head, PMATH_SYMBOL_ASSIGNDELAYED))  RET_CH_R( PMATH_CHAR_ASSIGNDELAYED, PMATH_PREC_ASS);
    
    if(pmath_same(head, PMATH_SYMBOL_RULE))           RET_CH_R( PMATH_CHAR_RULE,        PMATH_PREC_RULE);
    if(pmath_same(head, PMATH_SYMBOL_RULEDELAYED))    RET_CH_R( PMATH_CHAR_RULEDELAYED, PMATH_PREC_RULE);
  }
  else{
    if(pmath_same(head, PMATH_SYMBOL_ASSIGN))         RET_ST_R(":=",  PMATH_PREC_ASS);
    if(pmath_same(head, PMATH_SYMBOL_ASSIGNDELAYED))  RET_ST_R("::=", PMATH_PREC_ASS);
    
    if(pmath_same(head, PMATH_SYMBOL_RULE))           RET_ST_R("->", PMATH_PREC_RULE);
    if(pmath_same(head, PMATH_SYMBOL_RULEDELAYED))    RET_ST_R(":>", PMATH_PREC_RULE);
  }
  
  {
    pmath_t op = simple_nary(head, leftprec, boxform);
    *rightprec = *leftprec+= 1;
    return op;
  }

#undef RET_ST_L
#undef RET_ST_R
#undef RET_CH_L
#undef RET_CH_R
#undef RET_CH
#undef RET_ST
}

static int _pmath_symbol_to_precedence(pmath_t head){ // head wont be freed
  pmath_t op;
  int prec, prec2;
  
  op = simple_binary(head, &prec, &prec2, BOXFORM_STANDARD);
  pmath_unref(op);
  if(!pmath_is_null(op)){
    if(prec == prec2) // n-ary
      return prec-1;
    
    return (prec < prec2) ? prec : prec2;
  }
  
  op = simple_prefix(head, &prec, BOXFORM_STANDARD);
  pmath_unref(op);
  if(!pmath_is_null(op))
    return prec;
  
  op = simple_postfix(head, &prec, BOXFORM_STANDARD);
  pmath_unref(op);
  if(!pmath_is_null(op))
    return prec;
  
  if(pmath_same(head, PMATH_SYMBOL_SEQUENCE)) return PMATH_PREC_SEQ;
  if(pmath_same(head, PMATH_SYMBOL_RANGE))    return PMATH_PREC_RANGE;
  if(pmath_same(head, PMATH_SYMBOL_PLUS))     return PMATH_PREC_ADD;
  if(pmath_same(head, PMATH_SYMBOL_TIMES))    return PMATH_PREC_MUL;
  if(pmath_same(head, PMATH_SYMBOL_POWER))    return PMATH_PREC_POW;
  
  return PMATH_PREC_PRIM;
}

//} ... boxforms for simple functions

//{ boxforms for more complex functions ...

static pmath_t object_to_boxes(pmath_thread_t thread, pmath_t obj);

//{ boxforms valid for InputForm ...

  static pmath_t nary_to_boxes(
    pmath_thread_t thread, 
    pmath_expr_t   expr,        // will be freed
    pmath_t        op_box,      // will be freed
    int            firstprec, 
    int            restprec, 
    pmath_bool_t   skip_null
  ){
    pmath_t item;
    size_t len = pmath_expr_length(expr);
    size_t i;
    
    pmath_gather_begin(PMATH_NULL);
    
    item = pmath_expr_get_item(expr, 1);
    if(!pmath_is_null(item) || !skip_null || len == 1){
      pmath_emit(
        ensure_min_precedence(
          object_to_boxes(thread, item), 
          firstprec,
          -1), 
        PMATH_NULL);
    }
    
    if(len > 1){
      for(i = 2;i < len;++i){
        pmath_emit(pmath_ref(op_box), PMATH_NULL);
        
        item = pmath_expr_get_item(expr, i);
        if(!pmath_is_null(item) || !skip_null){
          pmath_emit(
            ensure_min_precedence(
              object_to_boxes(thread, item), 
              restprec,
              0),
            PMATH_NULL);
        }
      }
      
      pmath_emit(pmath_ref(op_box), PMATH_NULL);
      
      item = pmath_expr_get_item(expr, len);
      if(!pmath_is_null(item) || !skip_null){
        pmath_emit(
          ensure_min_precedence(
            object_to_boxes(thread, item), 
            restprec,
            +1),
          PMATH_NULL);
      }
    }
    
    pmath_unref(expr);
    pmath_unref(op_box);
    return pmath_gather_end();
  }

  // *box = {"-", x}     -->  *box = x      and return value is "-"
  // otherwise *box unchanged and return value is PMATH_NULL
  static pmath_t extract_minus(pmath_t *box){
    if(pmath_is_expr_of_len(*box, PMATH_SYMBOL_LIST, 2)){
      pmath_t minus = pmath_expr_get_item(*box, 1);
      
      if(pmath_is_string(minus)
      && pmath_string_equals_latin1(minus, "-")){
        pmath_t x = pmath_expr_get_item(*box, 2);
        pmath_unref(*box);
        *box = x;
        return minus;
      }
      
      pmath_unref(minus);
    }
    
    return PMATH_NULL;
  }

  // x^-n  ==>  x^n
  static pmath_bool_t negate_exponent(pmath_t *obj){
    if(pmath_is_expr_of_len(*obj, PMATH_SYMBOL_POWER, 2)){
      pmath_t exp = pmath_expr_get_item(*obj, 2);

      if(pmath_equals(exp, PMATH_FROM_INT32(-1))){
        pmath_unref(exp);
        exp = *obj;
        *obj = pmath_expr_get_item(exp, 1);
        pmath_unref(exp);
        return TRUE;
      }

      if(pmath_is_number(exp) && pmath_number_sign(exp) < 0){
        exp = pmath_number_neg(exp);
        *obj = pmath_expr_set_item(*obj, 2, exp);
        return TRUE;
      }

      pmath_unref(exp);
    }
    return FALSE;
  }
  
  static pmath_bool_t is_char(pmath_t obj, uint16_t ch){
    return pmath_is_string(obj)
        && pmath_string_length(obj) == 1
        && pmath_string_buffer(obj)[0] == ch;
  }
  
  static pmath_bool_t is_char_at(pmath_expr_t expr, size_t i, uint16_t ch){
    pmath_t obj = pmath_expr_get_item(expr, i);
    
    if(is_char(obj, ch)){
      pmath_unref(obj);
      return TRUE;
    }
    
    pmath_unref(obj);
    return FALSE;
  }
  
  static pmath_bool_t is_minus(pmath_t box){
    return pmath_is_expr_of_len(box, PMATH_SYMBOL_LIST, 2) && is_char_at(box, 1, '-');
  }
  
  static pmath_bool_t is_inversion(pmath_t box){
    return pmath_is_expr_of_len(box, PMATH_SYMBOL_LIST, 3) 
      && is_char_at(box, 2, '/')
      && is_char_at(box, 1, '1');
  }
  
  static pmath_t remove_paren(pmath_t box){
    if(pmath_is_expr_of_len(box, PMATH_SYMBOL_LIST, 3)
    && is_char_at(box, 1, '(')
    && is_char_at(box, 3, ')')){
      pmath_t tmp = box;
      box = pmath_expr_get_item(tmp, 1);
      pmath_unref(tmp);
    }
    
    return box;
  }
  
  static uint16_t first_char(pmath_t box){
    if(pmath_is_string(box)){
      if(pmath_string_length(box) > 0)
        return *pmath_string_buffer(box);

      return 0;
    }

    if(pmath_is_expr(box) && pmath_expr_length(box) > 0){
      pmath_t item = pmath_expr_get_item(box, 0);
      pmath_unref(item);

      if(pmath_same(item, PMATH_SYMBOL_LIST)){
        uint16_t result;

        item = pmath_expr_get_item(box, 1);
        result = first_char(item);
        pmath_unref(item);

        return result;
      }
    }

    return 0;
  }

  static uint16_t last_char(pmath_t box){
    if(pmath_is_string(box)){
      int len = pmath_string_length(box);
      if(len > 0)
        return pmath_string_buffer(box)[len - 1];

      return 0;
    }

    if(pmath_is_expr(box) && pmath_expr_length(box) > 0){
      pmath_t item = pmath_expr_get_item(box, 0);
      pmath_unref(item);

      if(pmath_same(item, PMATH_SYMBOL_LIST)){
        uint16_t result;

        item = pmath_expr_get_item(box, pmath_expr_length(box));
        result = last_char(item);
        pmath_unref(item);

        return result;
      }
    }

    return 0;
  }

  #define NUM_TAG  PMATH_NULL
  #define DEN_TAG  PMATH_UNDEFINED
  
  static void emit_num_den(pmath_expr_t product){ // product will be freed
    size_t i;
    
    for(i = 1;i <= pmath_expr_length(product);++i){
      pmath_t factor = pmath_expr_get_item(product, i);
      
      if(pmath_is_expr_of(factor, PMATH_SYMBOL_TIMES)){
        emit_num_den(factor);
        continue;
      }
      
      if(negate_exponent(&factor)){
        if(pmath_is_expr_of(factor, PMATH_SYMBOL_TIMES)){
          size_t j;
          for(j = 1;j <= pmath_expr_length(factor);++j){
            pmath_emit(pmath_expr_get_item(factor, j), DEN_TAG);
          }
          
          pmath_unref(factor);
          continue;
        }
        
        pmath_emit(factor, DEN_TAG);
        continue;
      }
      
      if(pmath_is_quotient(factor)){
        pmath_t num = pmath_rational_numerator(factor);
        pmath_t den = pmath_rational_denominator(factor);
        
        if(pmath_equals(num, PMATH_FROM_INT32(-1)))
          pmath_unref(num);
        else
          pmath_emit(num, NUM_TAG);
        
        pmath_emit(den, DEN_TAG);
        
        pmath_unref(factor);
        continue;
      }
      
      pmath_emit(factor, NUM_TAG);
    }
    
    pmath_unref(product);
  }
  
static pmath_t call_to_boxes(
  pmath_thread_t thread,
  pmath_expr_t   expr    // will be freed
){
  pmath_t item;
  
  pmath_gather_begin(PMATH_NULL);
  
  item = pmath_expr_get_item(expr, 0);
  pmath_emit(
    ensure_min_precedence(
      object_to_boxes(thread, item), 
      PMATH_PREC_CALL,
      -1), 
    PMATH_NULL);
  
  pmath_emit(PMATH_C_STRING("("), PMATH_NULL);
  
  if(pmath_expr_length(expr) > 0){
    pmath_emit(
      nary_to_boxes(
        thread, 
        expr, 
        PMATH_C_STRING(","), 
        PMATH_PREC_SEQ+1, 
        PMATH_PREC_SEQ+1,
        TRUE),
      PMATH_NULL);
  }
  else
    pmath_unref(expr);
  
  pmath_emit(PMATH_C_STRING(")"), PMATH_NULL);
  
  return pmath_gather_end();
}

static pmath_t complex_to_boxes(
  pmath_thread_t thread,
  pmath_expr_t   expr    // will be freed
){
  if(pmath_expr_length(expr) == 2){
    pmath_t re = pmath_expr_get_item(expr, 1);
    pmath_t im = pmath_expr_get_item(expr, 2);
    
    if(pmath_equals(re, PMATH_FROM_INT32(0))){
      if(pmath_equals(im, PMATH_FROM_INT32(1))){
        pmath_unref(expr);
        pmath_unref(re);
        pmath_unref(im);
        
        if(thread->boxform < BOXFORM_OUTPUT)
          return pmath_build_value("c", 0x2148);
          
        return PMATH_C_STRING("I");
      }
      
      if(pmath_is_number(im)){
        pmath_unref(re);
        expr = pmath_expr_set_item(expr, 2, PMATH_FROM_INT32(1));
        
        return object_to_boxes(thread,
          pmath_expr_new_extended(
            pmath_ref(PMATH_SYMBOL_TIMES), 2,
            im,
            expr));
      }
    }
    else if(pmath_is_number(re) && pmath_is_number(im)){
      pmath_unref(im);
      expr = pmath_expr_set_item(expr, 1, PMATH_FROM_INT32(0));
      
      return object_to_boxes(thread,
        pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_PLUS), 2,
          re,
          expr));
    }
  }

  return call_to_boxes(thread, expr);
}

static pmath_t directedinfinity_to_boxes(
  pmath_thread_t thread,
  pmath_expr_t   expr    // will be freed
){
  if(pmath_expr_length(expr) == 1){
    pmath_t dir = pmath_expr_get_item(expr, 1);
    
    if(pmath_equals(dir, PMATH_FROM_INT32(1))){
      pmath_unref(expr);
      pmath_unref(dir);
      
      return object_to_boxes(thread, pmath_ref(PMATH_SYMBOL_INFINITY));
    }
    
    if(pmath_equals(dir, PMATH_FROM_INT32(-1))){
      pmath_unref(expr);
      pmath_unref(dir);
      
      return object_to_boxes(thread, 
        pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_TIMES), 2,
          PMATH_FROM_INT32(-1),
          pmath_ref(PMATH_SYMBOL_INFINITY)));
    }
    
    if(pmath_is_expr_of_len(dir, PMATH_SYMBOL_COMPLEX, 2)){
      pmath_t x = pmath_expr_get_item(dir, 1);
      
      if(pmath_equals(x, PMATH_FROM_INT32(0))){
        pmath_unref(x);
        x = pmath_expr_get_item(dir, 2);
        
        if(pmath_equals(x, PMATH_FROM_INT32(1))){
          pmath_unref(expr);
          pmath_unref(dir);
          pmath_unref(x);
          
          return object_to_boxes(thread, 
            pmath_expr_new_extended(
              pmath_ref(PMATH_SYMBOL_TIMES), 2,
              pmath_expr_new_extended(
                pmath_ref(PMATH_SYMBOL_COMPLEX), 2,
                PMATH_FROM_INT32(0),
                PMATH_FROM_INT32(1)),
              pmath_ref(PMATH_SYMBOL_INFINITY)));
        }
        
        if(pmath_equals(x, PMATH_FROM_INT32(-1))){
          pmath_unref(expr);
          pmath_unref(dir);
          pmath_unref(x);
          
          return object_to_boxes(thread, 
            pmath_expr_new_extended(
              pmath_ref(PMATH_SYMBOL_TIMES), 2,
              pmath_expr_new_extended(
                pmath_ref(PMATH_SYMBOL_COMPLEX), 2,
                PMATH_FROM_INT32(0),
                PMATH_FROM_INT32(-1)),
              pmath_ref(PMATH_SYMBOL_INFINITY)));
        }
      }
      
      pmath_unref(x);
    }
    
    pmath_unref(dir);
  }
  else if(pmath_expr_length(expr) == 0){
    pmath_unref(expr);
    return object_to_boxes(thread, pmath_ref(PMATH_SYMBOL_COMPLEXINFINITY));
  }
  
  return call_to_boxes(thread, expr);
}

static pmath_t evaluationsequence_to_boxes(
  pmath_thread_t thread,
  pmath_expr_t   expr    // will be freed
){
  if(pmath_expr_length(expr) > 1){
    return nary_to_boxes(
      thread,
      expr,
      PMATH_C_STRING(";"),
      PMATH_PREC_EVAL + 1,
      PMATH_PREC_EVAL + 1,
      TRUE);
  }

  return call_to_boxes(thread, expr);
}

static pmath_t inequation_to_boxes(
  pmath_thread_t thread,
  pmath_expr_t   expr    // will be freed
){
  size_t len = pmath_expr_length(expr);

  if((len & 1) == 1 && len >= 3){
    pmath_t item;
    size_t i;
    
    pmath_gather_begin(PMATH_NULL);
    
    item = pmath_expr_get_item(expr, 1);
    pmath_emit(
      ensure_min_precedence(
        object_to_boxes(thread, item), 
        PMATH_PREC_REL+1,
        -1), 
      PMATH_NULL);
    
    for(i = 1;i <= len/2;++i){
      pmath_t op;
      
      item = pmath_expr_get_item(expr, 2*i);
      op = relation(item, thread->boxform);
      pmath_unref(item);
      
      if(pmath_is_null(op)){
        pmath_unref(pmath_gather_end());
        return call_to_boxes(thread, expr);
      }
      
      pmath_emit(op, PMATH_NULL);
      
      item = pmath_expr_get_item(expr, 2*i+1);
      pmath_emit(
        ensure_min_precedence(
          object_to_boxes(thread, item), 
          PMATH_PREC_REL+1,
          (2*i+1 == len) ? +1 : 0), 
        PMATH_NULL);
    }
    
    pmath_unref(expr);
    return pmath_gather_end();
  }

  return call_to_boxes(thread, expr);
}

static pmath_t list_to_boxes(
  pmath_thread_t thread,
  pmath_expr_t   expr    // will be freed
){
  if(pmath_expr_length(expr) == 0){
    pmath_unref(expr);
    return pmath_build_value("ss", "{", "}");
  }
  
  return pmath_build_value("sos", 
    "{", 
    nary_to_boxes(
      thread, 
      expr,
      PMATH_C_STRING(","),
      PMATH_PREC_SEQ+1,
      PMATH_PREC_SEQ+1,
      TRUE), 
    "}");
}

static pmath_t optional_to_boxes(
  pmath_thread_t thread,
  pmath_expr_t   expr    // will be freed
){
  if(pmath_expr_length(expr) == 1){
    pmath_t name = pmath_expr_get_item(expr, 1);
    
    if(pmath_is_symbol(name)){
      pmath_unref(expr);
      
      name = object_to_boxes(thread, name);
      name = ensure_min_precedence(name, PMATH_PREC_PRIM, 0);
      
      return pmath_build_value("so", "?", name);
    }
  }
  else if(pmath_expr_length(expr) == 2){
    pmath_t name = pmath_expr_get_item(expr, 1);
    pmath_t value =  pmath_expr_get_item(expr, 2);

    if(pmath_is_symbol(name)){
      pmath_unref(expr);

      name = object_to_boxes(thread, name);
      name = ensure_min_precedence(name, PMATH_PREC_PRIM, 0);

      value = object_to_boxes(thread, value);
      value = ensure_min_precedence(value, PMATH_PREC_CIRCMUL+1, +1);

      return pmath_build_value("soso", "?", name, ":", value);
    }

    pmath_unref(name);
    pmath_unref(value);
  }

  return call_to_boxes(thread, expr);
}

static pmath_t pattern_to_boxes(
  pmath_thread_t thread,
  pmath_expr_t   expr    // will be freed
){
  if(pmath_expr_length(expr) == 2){
    pmath_t name = pmath_expr_get_item(expr, 1);

    if(pmath_is_symbol(name)){
      pmath_t pat =  pmath_expr_get_item(expr, 2);

      name = object_to_boxes(thread, name);
      name = ensure_min_precedence(name, PMATH_PREC_PRIM, 0);

      pmath_unref(expr);

      if(pmath_equals(pat, _pmath_object_singlematch)){
        pmath_unref(pat);

        return pmath_build_value("so", "~", name);
      }
      
      if(pmath_is_expr_of_len(pat, PMATH_SYMBOL_SINGLEMATCH, 1)){
        pmath_t type = pmath_expr_get_item(pat, 1);
        
        pmath_unref(pat);
        type = object_to_boxes(thread, type);
        type = ensure_min_precedence(type, PMATH_PREC_PRIM, +1);
      
        return pmath_build_value("soso", "~", name, ":", type);
      }
      
      if(pmath_equals(pat, _pmath_object_multimatch)){
        pmath_unref(pat);
        
        return pmath_build_value("so", "~~", name);
      }

      if(pmath_equals(pat, _pmath_object_zeromultimatch)){
        pmath_unref(pat);
        
        return pmath_build_value("so", "~~~", name);
      }
      
      if(pmath_is_expr_of_len(pat, PMATH_SYMBOL_REPEATED, 2)){
        pmath_t rep = pmath_expr_get_item(pat, 1);
        
        if(pmath_is_expr_of_len(rep, PMATH_SYMBOL_SINGLEMATCH, 1)){
          pmath_t range = pmath_expr_get_item(pat, 2);
          
          if(pmath_equals(range, _pmath_object_range_from_one)){
            pmath_t type = pmath_expr_get_item(rep, 1);
            
            pmath_unref(rep);
            pmath_unref(range);
            pmath_unref(pat);
            type = object_to_boxes(thread, type);
            type = ensure_min_precedence(type, PMATH_PREC_PRIM, +1);
          
            return pmath_build_value("soso", "~~", name, ":", type);
          }
          
          if(pmath_equals(range, _pmath_object_range_from_zero)){
            pmath_t type = pmath_expr_get_item(rep, 1);
            
            pmath_unref(rep);
            pmath_unref(range);
            pmath_unref(pat);
            type = object_to_boxes(thread, type);
            type = ensure_min_precedence(type, PMATH_PREC_PRIM, +1);
            
            return pmath_build_value("soso", "~~~", name, ":", type);
          }
          
          pmath_unref(range);
        }
        
        pmath_unref(rep);
      }
      
      pat = object_to_boxes(thread, pat);
      pat = ensure_min_precedence(pat, PMATH_PREC_MUL+1, +1);

      return pmath_build_value("oso", name, ":", pat);
    }

    pmath_unref(name);
  }

  return call_to_boxes(thread, expr);
}

static pmath_t plus_to_boxes(
  pmath_thread_t thread,
  pmath_expr_t   expr    // will be freed
){
  size_t len = pmath_expr_length(expr);

  if(len >= 2){
    pmath_t item, minus;
    size_t i;
    
    pmath_gather_begin(PMATH_NULL);
    
    item = pmath_expr_get_item(expr, 1);
    pmath_emit(
      ensure_min_precedence(
        object_to_boxes(thread, item), 
        PMATH_PREC_ADD+1,
        -1), 
      PMATH_NULL);
    
    for(i = 2;i <= len;++i){
      item = pmath_expr_get_item(expr, i);
      item = object_to_boxes(thread, item);
      
      minus = extract_minus(&item);
      if(pmath_is_null(minus))
        pmath_emit(PMATH_C_STRING("+"), PMATH_NULL);
      else
        pmath_emit(minus, PMATH_NULL);
      
      pmath_emit(
        ensure_min_precedence(
          item, 
          PMATH_PREC_ADD+1,
          (i == len) ? +1 : 0), 
        PMATH_NULL);
    }
    
    pmath_unref(expr);
    return pmath_gather_end();
  }

  return call_to_boxes(thread, expr);
}
  
  static pmath_t enclose_subsuper_base(pmath_t box){
    if(pmath_is_expr_of(box, PMATH_SYMBOL_FRACTIONBOX))
      return pmath_build_value("(sos)", "(", box, ")");
    
    return ensure_min_precedence(box, PMATH_PREC_POW+1, -1);
  }

static pmath_t power_to_boxes(
  pmath_thread_t thread,
  pmath_expr_t   expr  // will be freed
){
  if(pmath_expr_length(expr) == 2){
    pmath_t base, exp;
    pmath_bool_t fraction = FALSE;

    base = pmath_expr_get_item(expr, 1);
    exp = pmath_expr_get_item(expr, 2);

    pmath_unref(expr);
    
    if(pmath_is_number(exp) && pmath_number_sign(exp) < 0){
      exp = pmath_number_neg(exp);
      fraction = TRUE;
    }

    if(pmath_equals(exp, _pmath_one_half)){
      pmath_unref(exp);
      
      if(thread->boxform < BOXFORM_OUTPUT){
        expr = pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_SQRTBOX), 1,
          object_to_boxes(thread, base));
      }
      else{
        expr = pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_LIST), 4,
          PMATH_C_STRING("Sqrt"),
          PMATH_C_STRING("("),
          object_to_boxes(thread, base),
          PMATH_C_STRING(")"));
      }
      
      if(fraction){
        if(thread->boxform != BOXFORM_STANDARDEXPONENT
        && thread->boxform != BOXFORM_OUTPUTEXPONENT
        && thread->boxform <= BOXFORM_OUTPUT){
          return pmath_expr_new_extended(
            pmath_ref(PMATH_SYMBOL_FRACTIONBOX), 2,
            PMATH_C_STRING("1"),
            expr);
        }
        else{
          if(first_char(expr) == '?')
            expr = pmath_build_value("(so)", " ", expr);
          return pmath_build_value("(sso)", "1", "/", expr);
        }
      }
      
      return expr;
    }

    if(!pmath_equals(exp, PMATH_FROM_INT32(1))){
      int old_boxform = thread->boxform;
      
      if(thread->boxform == BOXFORM_STANDARD)
        thread->boxform = BOXFORM_STANDARDEXPONENT;
      else if(thread->boxform == BOXFORM_OUTPUT)
        thread->boxform = BOXFORM_OUTPUTEXPONENT;
        
      exp = object_to_boxes(thread, exp);
      thread->boxform = old_boxform;
      
      if(thread->boxform < BOXFORM_INPUT){
        if(pmath_is_expr_of(base, PMATH_SYMBOL_SUBSCRIPT)
        && pmath_expr_length(base) >= 2){
          pmath_expr_t indices;

          expr = base;
          base = pmath_expr_get_item(expr, 1);
          indices = pmath_expr_get_item_range(expr, 2, SIZE_MAX);
          pmath_unref(expr);

          exp = pmath_expr_new_extended(
            pmath_ref(PMATH_SYMBOL_SUBSUPERSCRIPTBOX), 2,
            nary_to_boxes(
              thread,
              indices,
              PMATH_C_STRING(","),
              PMATH_PREC_SEQ+1,
              PMATH_PREC_SEQ+1,
              TRUE),
            exp);
        }
        else if(thread->boxform <= BOXFORM_OUTPUT)
          exp = pmath_expr_new_extended(
            pmath_ref(PMATH_SYMBOL_SUPERSCRIPTBOX), 1,
            exp);
      }
      else
        exp = ensure_min_precedence(exp, PMATH_PREC_POW, +1);
    }
    else{ // exp = 1
      pmath_unref(exp);
      exp = PMATH_NULL;
    }
    
    base = object_to_boxes(thread, base);
    base = enclose_subsuper_base(base);

    if(!pmath_is_null(exp)){
      if(thread->boxform <= BOXFORM_OUTPUT)
        expr = pmath_build_value("(oo)", base, exp);
      else
        expr = pmath_build_value("(oso)", base, "^", exp);
    }
    else
      expr = base;

    if(fraction){
      if(thread->boxform != BOXFORM_STANDARDEXPONENT
      && thread->boxform != BOXFORM_OUTPUTEXPONENT
      && thread->boxform <= BOXFORM_OUTPUT){
        return pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_FRACTIONBOX), 2,
          PMATH_C_STRING("1"),
          expr);
      }
      else{
        if(first_char(expr) == '?')
          expr = pmath_build_value("(so)", " ", expr);
        return pmath_build_value("(sso)", "1", "/", expr);
      }
    }
    
    return expr;
  }

  return call_to_boxes(thread, expr);
}

static pmath_t pureargument_to_boxes(
  pmath_thread_t thread,
  pmath_expr_t   expr    // will be freed
){
  if(pmath_expr_length(expr) == 1){
    pmath_t item = pmath_expr_get_item(expr, 1);
    
    if(pmath_is_integer(item) && pmath_number_sign(item) > 0){
      pmath_unref(expr);
      return pmath_build_value("(so)", "#", object_to_boxes(thread, item));
    }
    
    if(pmath_is_expr_of_len(item, PMATH_SYMBOL_RANGE, 2)){
      pmath_t a = pmath_expr_get_item(item, 1);
      pmath_t b = pmath_expr_get_item(item, 2);
      pmath_unref(b);
      
      if(pmath_same(b, PMATH_SYMBOL_AUTOMATIC)
      && pmath_is_integer(a)
      && pmath_number_sign(a) > 0){
        pmath_unref(item);
        pmath_unref(expr);
        
        return pmath_build_value("(so)", "##", object_to_boxes(thread, a));
      }
      
      pmath_unref(a);
    }
    
    pmath_unref(item);
  }

  return call_to_boxes(thread, expr);
}

static pmath_t range_to_boxes(
  pmath_thread_t thread,
  pmath_expr_t   expr    // will be freed
){
  if(pmath_expr_length(expr) > 1){
    return nary_to_boxes(
      thread,
      expr,
      PMATH_C_STRING(".."),
      PMATH_PREC_RANGE,
      PMATH_PREC_RANGE + 1,
      TRUE);
  }

  return call_to_boxes(thread, expr);
}

static pmath_t repeated_to_boxes(
  pmath_thread_t thread,
  pmath_expr_t   expr    // will be freed
){
  assert(thread != NULL);

  if(pmath_expr_length(expr) == 2){
    pmath_t item = pmath_expr_get_item(expr, 2);

    if(pmath_equals(item, _pmath_object_range_from_one)){
      pmath_unref(item);

      item = pmath_expr_get_item(expr, 1);
      pmath_unref(expr);

      if(pmath_equals(item, _pmath_object_singlematch)){
        pmath_unref(item);
        return PMATH_C_STRING("~~");
      }
      
      if(pmath_is_expr_of_len(item, PMATH_SYMBOL_SINGLEMATCH, 1)){
        pmath_t type = pmath_expr_get_item(item, 1);
        
        pmath_unref(item);
        type = object_to_boxes(thread, type);
        type = ensure_min_precedence(type, PMATH_PREC_PRIM, +1);
      
        return pmath_build_value("(sso)", "~~", ":", type);
      }

      item = object_to_boxes(thread, item);
      item = ensure_min_precedence(item, PMATH_PREC_REPEAT+1, -1);

      return pmath_build_value("(os)", item, "**");
    }

    if(pmath_equals(item, _pmath_object_range_from_zero)){
      pmath_unref(item);

      item = pmath_expr_get_item(expr, 1);
      pmath_unref(expr);

      if(pmath_equals(item, _pmath_object_singlematch)){
        pmath_unref(item);
        return PMATH_C_STRING("~~~");
      }
      
      if(pmath_is_expr_of_len(item, PMATH_SYMBOL_SINGLEMATCH, 1)){
        pmath_t type = pmath_expr_get_item(item, 1);
        
        pmath_unref(item);
        type = object_to_boxes(thread, type);
        type = ensure_min_precedence(type, PMATH_PREC_PRIM, +1);
      
        return pmath_build_value("(sso)", "~~~", ":", type);
      }

      item = object_to_boxes(thread, item);
      item = ensure_min_precedence(item, PMATH_PREC_REPEAT+1, -1);

      return pmath_build_value("(os)", item, "***");
    }

    pmath_unref(item);
  }

  return call_to_boxes(thread, expr);
}

static pmath_t singlematch_to_boxes(
  pmath_thread_t thread,
  pmath_expr_t   expr    // will be freed
){
  if(pmath_expr_length(expr) == 0){
    pmath_unref(expr);
    return PMATH_C_STRING("~");
  }
  
  if(pmath_expr_length(expr) == 1){
    pmath_t type = pmath_expr_get_item(expr, 1);
    
    pmath_unref(expr);
    type = object_to_boxes(thread, type);
    type = ensure_min_precedence(type, PMATH_PREC_PRIM, +1);
  
    return pmath_build_value("sso", "~", ":", type);
  }

  return call_to_boxes(thread, expr);
}

static pmath_t subscript_to_boxes(
  pmath_thread_t thread,
  pmath_expr_t   expr    // will be freed
){
  if(pmath_expr_length(expr) >= 2){
    pmath_t base;
    pmath_expr_t indices;

    base =  pmath_expr_get_item(expr, 1);
    indices = pmath_expr_get_item_range(expr, 2, SIZE_MAX);
    pmath_unref(expr);
    
    base = object_to_boxes(thread, base);
    base = enclose_subsuper_base(base);

    indices = pmath_expr_new_extended(
      pmath_ref(PMATH_SYMBOL_SUBSCRIPTBOX), 1,
      nary_to_boxes(
        thread,
        indices,
        PMATH_C_STRING(","),
        PMATH_PREC_SEQ+1,
        PMATH_PREC_SEQ+1,
        TRUE));

    return pmath_build_value("(oo)", base, indices);
  }

  return call_to_boxes(thread, expr);
}

static pmath_t subsuperscript_to_boxes(
  pmath_thread_t thread,
  pmath_expr_t   expr    // will be freed
){
  if(pmath_expr_length(expr) == 3){
    pmath_t base;
    pmath_t sub;
    pmath_t super;

    base  = pmath_expr_get_item(expr, 1);
    sub   = pmath_expr_get_item(expr, 2);
    super = pmath_expr_get_item(expr, 3);
    pmath_unref(expr);
    
    base  = object_to_boxes(thread, base);
    sub   = object_to_boxes(thread, sub);
    super = object_to_boxes(thread, super);
    base = enclose_subsuper_base(base);
    
    return pmath_build_value("(oo)", 
      base,
      pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_SUBSUPERSCRIPTBOX), 2,
        sub,
        super));
  }

  return call_to_boxes(thread, expr);
}

static pmath_t superscript_to_boxes(
  pmath_thread_t thread,
  pmath_expr_t   expr  // will be freed
){
  if(pmath_expr_length(expr) == 2){
    pmath_t base;
    pmath_t super;

    base  = pmath_expr_get_item(expr, 1);
    super = pmath_expr_get_item(expr, 2);
    pmath_unref(expr);

    base  = object_to_boxes(thread, base);
    super = object_to_boxes(thread, super);
    base = enclose_subsuper_base(base);
    
    return pmath_build_value("(oo)",
      base,
      pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_SUPERSCRIPTBOX), 1,
        super));
  }

  return call_to_boxes(thread, expr);
}

static pmath_t tagassign_to_boxes(
  pmath_thread_t thread,
  pmath_expr_t   expr    // will be freed
){
  pmath_t head, op;
  size_t len;

  assert(thread != NULL);

  len = pmath_expr_length(expr);
  head = pmath_expr_get_item(expr, 0);
  pmath_unref(head);
  
  if(len == 3){
    op = PMATH_NULL;
    if(thread->boxform < BOXFORM_OUTPUT){
      if(pmath_same(head, PMATH_SYMBOL_TAGASSIGN))
        op = pmath_build_value("c", PMATH_CHAR_ASSIGN);
      else if(pmath_same(head, PMATH_SYMBOL_TAGASSIGNDELAYED))
        op = pmath_build_value("c", PMATH_CHAR_ASSIGNDELAYED);
    }
    else{
      if(pmath_same(head, PMATH_SYMBOL_TAGASSIGN))
        op = PMATH_C_STRING(":=");
      else if(pmath_same(head, PMATH_SYMBOL_TAGASSIGNDELAYED))
        op = PMATH_C_STRING("::=");
    }
    
    if(!pmath_is_null(op)){
      pmath_t item;
      
      pmath_gather_begin(PMATH_NULL);
      
      item = pmath_expr_get_item(expr, 1);
      pmath_emit(
        ensure_min_precedence(
          object_to_boxes(thread, item), 
          PMATH_PREC_ASS+1,
          -1), 
        PMATH_NULL);
      
      pmath_emit(PMATH_C_STRING("/:"), PMATH_NULL);
      
      item = pmath_expr_get_item(expr, 2);
      pmath_emit(
        ensure_min_precedence(
          object_to_boxes(thread, item), 
          PMATH_PREC_ASS+1,
          0), 
        PMATH_NULL);
      
      pmath_emit(op, PMATH_NULL);
      
      item = pmath_expr_get_item(expr, 3);
      pmath_emit(
        ensure_min_precedence(
          object_to_boxes(thread, item), 
          PMATH_PREC_ASS,
          +1), 
        PMATH_NULL);
      
      pmath_unref(expr);
      return pmath_gather_end();
    }
  }

  return call_to_boxes(thread, expr);
}

static pmath_t times_to_boxes(
  pmath_thread_t thread,
  pmath_expr_t   expr  // will be freed
){
  size_t len = pmath_expr_length(expr);
  
  if(len >= 2){
    pmath_t minus = PMATH_NULL;
    pmath_t num, den;
    
    if(thread->boxform != BOXFORM_STANDARDEXPONENT
    && thread->boxform != BOXFORM_OUTPUTEXPONENT
    && thread->boxform <= BOXFORM_OUTPUT){
      pmath_gather_begin(NUM_TAG);
      pmath_gather_begin(DEN_TAG);
      
      emit_num_den(expr); expr = PMATH_NULL;
      
      den = pmath_gather_end();
      num = pmath_gather_end();
    }
    else{
      num = expr;
      expr = PMATH_NULL;
      den = PMATH_NULL;
    }
    
    if(pmath_expr_length(den) == 0){
      pmath_token_t last_tok;
      pmath_t factor;
      pmath_t prevfactor;
      pmath_bool_t div = FALSE;
      uint16_t ch;
      size_t i;
      size_t numlen = pmath_expr_length(num);
      
      pmath_unref(den);
      pmath_gather_begin(PMATH_NULL);
      
      prevfactor = pmath_expr_get_item(num, 1);
      prevfactor = object_to_boxes(thread, prevfactor);
      
      if(is_minus(prevfactor)){
        den = prevfactor;
        minus      = pmath_expr_get_item(den, 1);
        prevfactor = pmath_expr_get_item(den, 2);
        pmath_unref(den);
        
        prevfactor = ensure_min_precedence(prevfactor, PMATH_PREC_MUL+1, -1);
        ch = last_char(prevfactor);
        last_tok = pmath_token_analyse(&ch, 1, NULL);
      }
      else{
        prevfactor = ensure_min_precedence(prevfactor, PMATH_PREC_MUL+1, -1);
        ch = last_char(prevfactor);
        last_tok = pmath_token_analyse(&ch, 1, NULL);
      }
      
      for(i = 2;i <= numlen;++i){
        factor = pmath_expr_get_item(num, i);
        factor = object_to_boxes(thread, factor);
        factor = ensure_min_precedence(
          factor, 
          PMATH_PREC_MUL+1,
          (i == numlen) ? +1 : 0);
        
        if(is_inversion(factor)){
          if(!div){
            div = TRUE;
            pmath_gather_begin(PMATH_NULL);
          }
          pmath_emit(prevfactor, PMATH_NULL);
          
          den = factor;
          factor = pmath_expr_get_item(den, 3);
          pmath_unref(den);
          
          pmath_emit(PMATH_C_STRING("/"), PMATH_NULL);
        }
        else{
          if(i == 2 && is_char(prevfactor, '1'))
            pmath_unref(prevfactor);
          else
            pmath_emit(prevfactor, PMATH_NULL);
            
          if(div)
            pmath_emit(pmath_gather_end(), PMATH_NULL);
          
          if(last_tok != PMATH_TOK_DIGIT){
            pmath_emit(PMATH_C_STRING(" "), PMATH_NULL);
          }
          else{
            ch = first_char(factor);
            last_tok = pmath_token_analyse(&ch, 1, NULL);
            
            if(last_tok == PMATH_TOK_LEFTCALL)
              pmath_emit(PMATH_C_STRING(" "), PMATH_NULL);
          }
        }

        ch = last_char(factor);
        last_tok = pmath_token_analyse(&ch, 1, NULL);
        
        prevfactor = factor;
      }
      
      pmath_emit(prevfactor, PMATH_NULL);
      if(div)
        pmath_emit(pmath_gather_end(), PMATH_NULL);
        
      pmath_unref(num);
      expr = pmath_gather_end();
      if(pmath_expr_length(expr) == 1){
        num = expr;
        expr = pmath_expr_get_item(num, 1);
        pmath_unref(num);
      }
    }
    else{
      pmath_bool_t use_fraction_box = thread->boxform != BOXFORM_STANDARDEXPONENT
                                   && thread->boxform != BOXFORM_OUTPUTEXPONENT
                                   && thread->boxform <= BOXFORM_OUTPUT;
      
      if(pmath_expr_length(den) == 1){
        pmath_t tmp = den;
        den = pmath_expr_get_item(tmp, 1);
        pmath_unref(tmp);
      }
      else
        den = pmath_expr_set_item(den, 0, pmath_ref(PMATH_SYMBOL_TIMES));
      
      if(pmath_expr_length(num) == 0){
        pmath_unref(num);
        num = PMATH_FROM_INT32(1);
      }
      else if(pmath_expr_length(num) == 1){
        pmath_t tmp = num;
        num = pmath_expr_get_item(tmp, 1);
        pmath_unref(tmp);
      }
      else
        num = pmath_expr_set_item(num, 0, pmath_ref(PMATH_SYMBOL_TIMES));
      
      num = object_to_boxes(thread, num);
      den = object_to_boxes(thread, den);
      
      if(is_minus(num)){
        if(!is_minus(den)){
          pmath_t tmp = num;
          minus = pmath_expr_get_item(tmp, 1);
          num = remove_paren(pmath_expr_get_item(tmp, 2));
          pmath_unref(tmp);
        }
      }
      else if(is_minus(den)){
        pmath_t tmp = den;
        minus = pmath_expr_get_item(tmp, 1);
        den = remove_paren(pmath_expr_get_item(tmp, 2));
        pmath_unref(tmp);
      }
      
      if(use_fraction_box){
        expr = pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_FRACTIONBOX), 2,
          num, den);
      }
      else{
        num = ensure_min_precedence(num, PMATH_PREC_DIV, -1);
        den = ensure_min_precedence(den, PMATH_PREC_DIV, +1);
        expr = pmath_build_value("(oco)", num, '/', den);
      }
    }
    
    if(!pmath_is_null(minus))
      return pmath_build_value("(oo)", minus, expr);
    
    return expr;
  }

  return call_to_boxes(thread, expr);
}

//} ... boxforms valid for InputForm

//{ boxforms valid for OutputForm ...

static pmath_t column_to_boxes(
  pmath_thread_t thread,
  pmath_expr_t   expr    // will be freed
){
  if(pmath_expr_length(expr) == 1){
    pmath_t list = pmath_expr_get_item(expr, 1);
    
    if(pmath_is_expr_of(list, PMATH_SYMBOL_LIST)){
      size_t i;
      
      pmath_unref(expr);
      
      for(i = pmath_expr_length(list);i > 0;--i){
        list = pmath_expr_set_item(list, i,
          pmath_expr_new_extended(
            pmath_ref(PMATH_SYMBOL_LIST), 1,
            object_to_boxes(
              thread,
              pmath_expr_get_item(list, i))));
      }
      
      return pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_TAGBOX), 2,
        pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_GRIDBOX), 1,
          list),
        PMATH_C_STRING("Column"));
    }
    
    pmath_unref(list);
  }

  return call_to_boxes(thread, expr);
}
  
  static void emit_gridbox_options(
    pmath_expr_t expr, // wont be freed
    size_t       start
  ){
    size_t i;
    
    for(i = start;i <= pmath_expr_length(expr);++i){
      pmath_t item = pmath_expr_get_item(expr, i);
      
      if(_pmath_is_rule(item)){
        pmath_t lhs = pmath_expr_get_item(item, 1);
        pmath_unref(lhs);
        
        if(pmath_same(lhs, PMATH_SYMBOL_COLUMNSPACING)){
          item = pmath_expr_set_item(item, 1, pmath_ref(PMATH_SYMBOL_GRIDBOXCOLUMNSPACING));
          pmath_emit(item, PMATH_NULL);
          continue;
        }
        
        if(pmath_same(lhs, PMATH_SYMBOL_ROWSPACING)){
          item = pmath_expr_set_item(item, 1, pmath_ref(PMATH_SYMBOL_GRIDBOXROWSPACING));
          pmath_emit(item, PMATH_NULL);
          continue;
        }
        
        pmath_emit(item, PMATH_NULL);
        continue;
      }
      
      if(pmath_is_expr_of(item, PMATH_SYMBOL_LIST)){
        emit_gridbox_options(item, 1);
        pmath_unref(item);
        continue;
      }
      
      pmath_unref(item);
    }
  }

static pmath_t grid_to_boxes(
  pmath_thread_t thread,
  pmath_expr_t   expr    // will be freed
){
  if(pmath_expr_length(expr) >= 1){
    pmath_expr_t obj = pmath_options_extract(expr, 1);
    
    if(!pmath_is_null(obj)){
      size_t rows, cols;
      
      pmath_unref(obj);
      obj = pmath_expr_get_item(expr, 1);
      
      if(_pmath_is_matrix(obj, &rows, &cols)){
        size_t i, j;
        
        expr = pmath_expr_set_item(expr, 1, PMATH_NULL);
        
        for(i = 1;i <= rows;++i){
          pmath_t row = pmath_expr_get_item(obj, i);
          obj = pmath_expr_set_item(obj, i, PMATH_NULL);
          
          for(j = 1;j <= cols;++j){
            pmath_t item = pmath_expr_get_item(row, j);
            
            item = object_to_boxes(thread, item);
            
            row = pmath_expr_set_item(row, j, item);
          }
          
          obj = pmath_expr_set_item(obj, i, row);
        }
        
        pmath_gather_begin(PMATH_NULL);
        
        pmath_emit(obj, PMATH_NULL);
        emit_gridbox_options(expr, 2);
        
        pmath_unref(expr);
        expr = pmath_gather_end();
        expr = pmath_expr_set_item(expr, 0, pmath_ref(PMATH_SYMBOL_GRIDBOX));
        
        return pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_TAGBOX), 2,
          expr,
          PMATH_C_STRING("Grid"));
      }
      
      pmath_unref(obj);
    }
  }

  return call_to_boxes(thread, expr);
}

  static pmath_t fullform(
    pmath_thread_t thread,
    pmath_t        obj     // will be freed
  ){
    if(pmath_is_expr(obj)){
      pmath_expr_t result;
      size_t len;
      len = pmath_expr_length(obj);
      
      if(len > 0){
        pmath_t comma;
        size_t i;

        comma = PMATH_C_STRING(",");

        result = pmath_expr_new(pmath_ref(PMATH_SYMBOL_LIST), 2 * len - 1);

        result = pmath_expr_set_item(
          result, 1,
          fullform(
            thread,
            pmath_expr_get_item(obj, 1)));

        for(i = 2;i <= len;++i){
          result = pmath_expr_set_item(
            result, 2 * i - 2,
            pmath_ref(comma));

          result = pmath_expr_set_item(
            result, 2 * i - 1,
            fullform(
              thread,
              pmath_expr_get_item(obj, i)));
        }

        pmath_unref(comma);

        result = pmath_build_value("osos", 
          fullform(
            thread,
            pmath_expr_get_item(obj, 0)),
          "(",
          result,
          ")");
      }
      else{
        result = pmath_build_value("oss",
          fullform(
            thread,
            pmath_expr_get_item(obj, 0)),
          "(",
          ")");
      }

      pmath_unref(obj);
      return result;
    }

    return object_to_boxes(thread, obj);
  }

static pmath_t fullform_to_boxes(
  pmath_thread_t thread,
  pmath_expr_t   expr    // will be freed
){
  if(pmath_expr_length(expr) == 1){
    pmath_t result;
    uint8_t old_boxform = thread->boxform;
    thread->boxform = BOXFORM_INPUT;
    
    result = fullform(
      thread,
      pmath_expr_get_item(expr, 1));
    
    thread->boxform = old_boxform;
    
    pmath_unref(expr);
    return pmath_expr_new_extended(
      pmath_ref(PMATH_SYMBOL_STYLEBOX), 3,
      result,
      pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_RULE), 2,
        pmath_ref(PMATH_SYMBOL_AUTODELETE),
        pmath_ref(PMATH_SYMBOL_TRUE)),
      pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_RULE), 2,
        pmath_ref(PMATH_SYMBOL_SHOWSTRINGCHARACTERS),
        pmath_ref(PMATH_SYMBOL_TRUE)));
  }

  return call_to_boxes(thread, expr);
}

static pmath_t holdform_to_boxes(
  pmath_thread_t thread,
  pmath_expr_t   expr    // will be freed
){
  if(pmath_expr_length(expr) == 1){
    pmath_t obj = pmath_expr_get_item(expr, 1);
    
    pmath_unref(expr);
    
    return object_to_boxes(thread, obj);
  }
  
  return call_to_boxes(thread, expr);
}

static pmath_t inputform_to_boxes(
  pmath_thread_t thread,
  pmath_expr_t   expr    // will be freed
){
  if(pmath_expr_length(expr) == 1){
    pmath_t result;
    uint8_t old_boxform = thread->boxform;
    thread->boxform = BOXFORM_INPUT;

    result = object_to_boxes(thread, pmath_expr_get_item(expr, 1));

    thread->boxform = old_boxform;
    
    pmath_unref(expr);
    return pmath_expr_new_extended(
      pmath_ref(PMATH_SYMBOL_STYLEBOX), 4,
      result,
      pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_RULE), 2,
        pmath_ref(PMATH_SYMBOL_AUTODELETE),
        pmath_ref(PMATH_SYMBOL_TRUE)),
      pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_RULE), 2,
        pmath_ref(PMATH_SYMBOL_AUTONUMBERFORMATING),
        pmath_ref(PMATH_SYMBOL_FALSE)),
      pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_RULE), 2,
        pmath_ref(PMATH_SYMBOL_SHOWSTRINGCHARACTERS),
        pmath_ref(PMATH_SYMBOL_TRUE)));
  }

  return call_to_boxes(thread, expr);
}

static pmath_t interpretation_to_boxes(
  pmath_thread_t thread,
  pmath_expr_t   expr    // will be freed
){
  if(pmath_expr_length(expr) == 2){
    pmath_t form  = pmath_expr_get_item(expr, 1);
    pmath_t value = pmath_expr_get_item(expr, 2);

    pmath_unref(expr);
    form = object_to_boxes(thread, form);

    return pmath_expr_new_extended(
      pmath_ref(PMATH_SYMBOL_INTERPRETATIONBOX), 2,
      form,
      value);
  }

  return call_to_boxes(thread, expr);
}

static pmath_t longform_to_boxes(
  pmath_thread_t thread,
  pmath_expr_t   expr    // will be freed
){
  if(pmath_expr_length(expr) == 1){
    pmath_t obj = pmath_expr_get_item(expr, 1);
    pmath_bool_t old_longform = thread->longform;
    thread->longform = TRUE;
    
    pmath_unref(expr);
    expr = object_to_boxes(thread, obj);
    
    thread->longform = old_longform;
    return expr;
  }
  
  return call_to_boxes(thread, expr);
}

  static pmath_t matrixform(
    pmath_thread_t thread,
    pmath_t        obj     // will be freed
  ){
    size_t rows, cols;
    
    if(_pmath_is_matrix(obj, &rows, &cols) && rows > 0 && cols > 0){
      size_t i, j;
      
      for(i = 1;i <= rows;++i){
        pmath_t row = pmath_expr_extract_item(obj, i);
        
        for(j = 1;j <= cols;++j){
          pmath_t item = pmath_expr_extract_item(row, j);
          
          item = matrixform(thread, item);
          
          row = pmath_expr_set_item(row, j, item);
        }
        
        obj = pmath_expr_set_item(obj, i, row);
      }
      
      obj = pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_GRIDBOX), 1,
        obj);
      
      return pmath_build_value("sos", "(", obj, ")");
    }
    
    if(pmath_is_expr_of(obj, PMATH_SYMBOL_LIST)){
      size_t i;
      
      rows = pmath_expr_length(obj);
      
      for(i = 1;i <= rows;++i){
        pmath_t item = pmath_expr_extract_item(obj, i);
        
        item = object_to_boxes(thread, item);
        item = pmath_build_value("(o)", item);
        
        obj = pmath_expr_set_item(obj, i, item);
      }
      
      obj = pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_GRIDBOX), 1,
        obj);
      
      obj = pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_TAGBOX), 2,
        obj,
        pmath_ref(PMATH_SYMBOL_COLUMN));
      
      return pmath_build_value("sos", "(", obj, ")");
    }
    
    return object_to_boxes(thread, obj);
  }

static pmath_t matrixform_to_boxes(
  pmath_thread_t thread,
  pmath_expr_t   expr    // will be freed
){
  if(pmath_expr_length(expr) == 1){
    pmath_t obj;
    
    obj = pmath_expr_get_item(expr, 1);
    pmath_unref(expr);
    
    return matrixform(thread, obj);
  }

  return call_to_boxes(thread, expr);
}

static pmath_t outputform_to_boxes(
  pmath_thread_t thread,
  pmath_expr_t   expr    // will be freed
){
  if(pmath_expr_length(expr) == 1){
    pmath_t result;
    uint8_t old_boxform = thread->boxform;
    thread->boxform = BOXFORM_OUTPUT;

    result = object_to_boxes(thread, pmath_expr_get_item(expr, 1));

    thread->boxform = old_boxform;

    return pmath_expr_new_extended(
      pmath_ref(PMATH_SYMBOL_INTERPRETATIONBOX), 2,
      result,
      expr);
  }

  return call_to_boxes(thread, expr);
}

static pmath_t row_to_boxes(
  pmath_thread_t thread,
  pmath_expr_t   expr    // will be freed
){
  size_t len = pmath_expr_length(expr);

  assert(thread != NULL);
  
  if(len == 1){
    pmath_t list = pmath_expr_get_item(expr, 1);
    
    if(pmath_is_expr_of(list, PMATH_SYMBOL_LIST)){
      size_t i;
      
      pmath_unref(expr);
      pmath_gather_begin(PMATH_NULL);
      
      for(i = 1;i <= pmath_expr_length(list);++i){
        pmath_t item = pmath_expr_get_item(list, i);
        item = object_to_boxes(thread, item);
        pmath_emit(item, PMATH_NULL);
      }
      
      pmath_unref(list);
      return pmath_gather_end();
    }
    
    pmath_unref(list);
  }
  else if(len == 2){
    pmath_t list = pmath_expr_get_item(expr, 1);
    
    if(pmath_is_expr_of(list, PMATH_SYMBOL_LIST)){
      pmath_t delim = pmath_expr_get_item(expr, 2);
      
      if(!pmath_is_string(delim))
        delim = object_to_boxes(thread, delim);
      
      pmath_unref(expr);
      
      return nary_to_boxes(
        thread,
        list,
        delim,
        PMATH_PREC_ANY,
        PMATH_PREC_ANY,
        TRUE);
    }
    pmath_unref(list);
  }
  
  return call_to_boxes(thread, expr);
}

static pmath_t shallow_to_boxes(
  pmath_thread_t thread,
  pmath_expr_t   expr    // will be freed
){
  pmath_t item;
  size_t maxdepth = 4;
  size_t maxlength = 10;
    
  if(pmath_expr_length(expr) == 2){
    item = pmath_expr_get_item(expr, 2);
    
    if(pmath_is_int32(item) && PMATH_AS_INT32(item) >= 0){
      maxdepth = (size_t)PMATH_AS_INT32(item);
    }
    else if(pmath_equals(item, _pmath_object_infinity)){
      maxdepth = SIZE_MAX;
    }
    else if(pmath_is_expr_of_len(item, PMATH_SYMBOL_LIST, 2)){ 
    // {maxdepth, maxlength}
      pmath_t obj = pmath_expr_get_item(item, 1);
      
      if(pmath_is_int32(obj) && PMATH_AS_INT32(obj) >= 0){
        maxdepth = (size_t)PMATH_AS_INT32(obj);
      }
      else if(pmath_equals(obj, _pmath_object_infinity)){
        maxdepth = SIZE_MAX;
      }
      else{
        pmath_unref(obj);
        pmath_unref(item);
        return call_to_boxes(thread, expr);
      }
      
      pmath_unref(obj);
      obj = pmath_expr_get_item(item, 2);
      
      if(pmath_is_int32(obj) && PMATH_AS_INT32(obj) >= 0){
        maxlength = (size_t)PMATH_AS_INT32(obj);
      }
      else if(pmath_equals(obj, _pmath_object_infinity)){
        maxlength = SIZE_MAX;
      }
      else{
        pmath_unref(obj);
        pmath_unref(item);
        return call_to_boxes(thread, expr);
      }
      
      pmath_unref(obj);
    }
    else{
      pmath_unref(item);
      return call_to_boxes(thread, expr);
    }
    
    pmath_unref(item);
  }
  else if(pmath_expr_length(expr) != 1)
    return call_to_boxes(thread, expr);
    
  item = pmath_expr_get_item(expr, 1);
  pmath_unref(expr);
  
  item = _pmath_prepare_shallow(item, maxdepth, maxlength);
  
  return object_to_boxes(thread, item);
}

static pmath_t short_to_boxes(
  pmath_thread_t thread,
  pmath_expr_t   expr    // will be freed
){
  pmath_t item;
  double pagewidth = 72;
  double lines = 1;
  
  if(pmath_expr_length(expr) == 2){
    item = pmath_expr_get_item(expr, 2);
    
    if(pmath_equals(item, _pmath_object_infinity)){
      pmath_unref(item);
      item = pmath_expr_get_item(expr, 1);
      pmath_unref(expr);
      return object_to_boxes(thread, item);
    }
    
    if(pmath_is_number(item)){
      lines = pmath_number_get_d(item);
      
      if(lines < 0){
        pmath_unref(item);
        return call_to_boxes(thread, expr);
      }
    }
    else{
      pmath_unref(item);
      return call_to_boxes(thread, expr);
    }
    
    pmath_unref(item);
  }
  else if(pmath_expr_length(expr) != 1)
    return call_to_boxes(thread, expr);
  
  item = pmath_evaluate(pmath_ref(PMATH_SYMBOL_PAGEWIDTHDEFAULT));
  if(pmath_equals(item, _pmath_object_infinity)){
    pmath_unref(item);
    item = pmath_expr_get_item(expr, 1);
    pmath_unref(expr);
    return object_to_boxes(thread, item);
  }
  
  if(pmath_is_number(item)){
    pagewidth = pmath_number_get_d(item);
  }
  
  pmath_unref(item);
  
  pagewidth*= lines;
  if(pagewidth < 1 || pagewidth > 1000000)
    return call_to_boxes(thread, expr);
  
  item = pmath_expr_get_item(expr, 1);
  pmath_unref(expr);
  
  item = object_to_boxes(thread, item);
  return _pmath_shorten_boxes(item, (long)pagewidth);
}

static pmath_t skeleton_to_boxes(
  pmath_thread_t thread,
  pmath_expr_t   expr    // will be freed
){
  if(pmath_expr_length(expr) == 1){
    pmath_expr_t obj = pmath_expr_get_item(expr, 1);
    pmath_unref(expr);
    
    if(thread->boxform < BOXFORM_OUTPUT){
      return pmath_build_value("(coc)", 0x00AB, object_to_boxes(thread, obj), 0x00BB);
    }
    
    return pmath_build_value("(sos)", "<<", object_to_boxes(thread, obj), ">>");
  }

  return call_to_boxes(thread, expr);
}
  
  struct emit_stylebox_options_info_t{
    pmath_bool_t have_explicit_strip_on_input;
  };
  
  static pmath_bool_t emit_stylebox_options(
    pmath_expr_t                         expr,  // wont be freed
    size_t                               start, 
    struct emit_stylebox_options_info_t *info
  ){
    size_t i;
    
    for(i = start;i <= pmath_expr_length(expr);++i){
      pmath_t item = pmath_expr_get_item(expr, i);
      
      if(pmath_same(item, PMATH_SYMBOL_BOLD)){
        pmath_emit(
          pmath_expr_new_extended(
            pmath_ref(PMATH_SYMBOL_RULE), 2, 
            pmath_ref(PMATH_SYMBOL_FONTWEIGHT),
            item),
          PMATH_NULL);
        continue;
      }
      
      if(pmath_same(item, PMATH_SYMBOL_ITALIC)){
        pmath_emit(
          pmath_expr_new_extended(
            pmath_ref(PMATH_SYMBOL_RULE), 2, 
            pmath_ref(PMATH_SYMBOL_FONTSLANT),
            item),
          PMATH_NULL);
        continue;
      }
      
      if(pmath_same(item, PMATH_SYMBOL_PLAIN)){
        pmath_emit(
          pmath_expr_new_extended(
            pmath_ref(PMATH_SYMBOL_RULE), 2, 
            pmath_ref(PMATH_SYMBOL_FONTSLANT),
            pmath_ref(item)),
          PMATH_NULL);
        pmath_emit(
          pmath_expr_new_extended(
            pmath_ref(PMATH_SYMBOL_RULE), 2, 
            pmath_ref(PMATH_SYMBOL_FONTWEIGHT),
            item),
          PMATH_NULL);
        continue;
      }
      
      if(pmath_is_number(item)){
        pmath_emit(
          pmath_expr_new_extended(
            pmath_ref(PMATH_SYMBOL_RULE), 2, 
            pmath_ref(PMATH_SYMBOL_FONTSIZE),
            item),
          PMATH_NULL);
        continue;
      }
      
      if(pmath_is_expr_of(item, PMATH_SYMBOL_LIST)){
        if(!emit_stylebox_options(expr, 1, info)){
          pmath_unref(item);
          return FALSE;
        }
        
        pmath_unref(item);
        continue;
      }
      
      if(pmath_is_expr_of_len(item, PMATH_SYMBOL_GRAYLEVEL, 1)
      || pmath_is_expr_of_len(item, PMATH_SYMBOL_RGBCOLOR, 3)){
        pmath_emit(
          pmath_expr_new_extended(
            pmath_ref(PMATH_SYMBOL_RULE), 2, 
            pmath_ref(PMATH_SYMBOL_FONTCOLOR),
            item),
          PMATH_NULL);
        continue;
      }
      
      if(_pmath_is_rule(item)){
        pmath_t lhs = pmath_expr_get_item(item, 1);
        pmath_unref(lhs);
        
        if(!info->have_explicit_strip_on_input){
          info->have_explicit_strip_on_input = pmath_same(lhs, PMATH_SYMBOL_STRIPONINPUT);
        }
      
        pmath_emit(item, PMATH_NULL);
        continue;
      }
      
      if(pmath_is_string(item) && i == start && i > 1){
        pmath_emit(item, PMATH_NULL);
        continue;
      }
      
      pmath_unref(item);
      return FALSE;
    }
    
    return TRUE;
  }

static pmath_t style_to_boxes(
  pmath_thread_t thread,
  pmath_expr_t   expr    // will be freed
){
  size_t len = pmath_expr_length(expr);
  
  if(len >= 1){
    struct emit_stylebox_options_info_t info;
    memset(&info, 0, sizeof(info));
    
    pmath_gather_begin(PMATH_NULL);
    pmath_emit(object_to_boxes(thread, pmath_expr_get_item(expr, 1)), PMATH_NULL);
    
    if(!emit_stylebox_options(expr, 2, &info)){
      pmath_unref(pmath_gather_end());
      
      return call_to_boxes(thread, expr);
    }
    
    pmath_unref(expr);
    
    if(!info.have_explicit_strip_on_input){
      pmath_emit(
        pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_RULE), 2,
          pmath_ref(PMATH_SYMBOL_STRIPONINPUT),
          pmath_ref(PMATH_SYMBOL_FALSE)),
        PMATH_NULL);
    }
    
    expr = pmath_gather_end();
    return pmath_expr_set_item(expr, 0, pmath_ref(PMATH_SYMBOL_STYLEBOX));
  }
  
  return call_to_boxes(thread, expr);
}

static pmath_t underscript_or_overscript_to_boxes(
  pmath_thread_t thread,
  pmath_expr_t   expr    // will be freed
){
  if(pmath_expr_length(expr) == 2){
    pmath_t head = pmath_expr_get_item(expr, 0);
    pmath_t base = pmath_expr_get_item(expr, 1);
    pmath_t uo   = pmath_expr_get_item(expr, 2);

    pmath_unref(head);
    pmath_unref(expr);

    base = object_to_boxes(thread, base);
    uo   = object_to_boxes(thread, uo);

    if(pmath_same(head, PMATH_SYMBOL_UNDERSCRIPT))
      head = pmath_ref(PMATH_SYMBOL_UNDERSCRIPTBOX);
    else
      head = pmath_ref(PMATH_SYMBOL_OVERSCRIPTBOX);

    return pmath_expr_new_extended(
      head, 2,
      base,
      uo);
  }

  return call_to_boxes(thread, expr);
}

static pmath_t underoverscript_to_boxes(
  pmath_thread_t thread,
  pmath_expr_t   expr    // will be freed
){
  if(pmath_expr_length(expr) == 3){
    pmath_t base  = pmath_expr_get_item(expr, 1);
    pmath_t under = pmath_expr_get_item(expr, 2);
    pmath_t over  = pmath_expr_get_item(expr, 3);

    pmath_unref(expr);

    base  = object_to_boxes(thread, base);
    under = object_to_boxes(thread, under);
    over  = object_to_boxes(thread, over);

    return pmath_expr_new_extended(
      pmath_ref(PMATH_SYMBOL_UNDEROVERSCRIPTBOX), 3,
      base,
      under,
      over);
  }

  return call_to_boxes(thread, expr);
}

//} ... boxforms valid for OutputForm

//{ boxforms valid for StandardForm ...

static pmath_t framed_to_boxes(
  pmath_thread_t thread,
  pmath_expr_t   expr    // will be freed
){
  if(pmath_expr_length(expr) == 1){
    pmath_t item = pmath_expr_get_item(expr, 1);

    pmath_unref(expr);

    item = object_to_boxes(thread, item);

    return pmath_expr_new_extended(
      pmath_ref(PMATH_SYMBOL_FRAMEBOX), 1,
      item);
  }

  return call_to_boxes(thread, expr);
}

static pmath_t piecewise_to_boxes(
  pmath_thread_t thread,
  pmath_expr_t   expr    // will be freed
){
  size_t exprlen = pmath_expr_length(expr);
  
  if(exprlen >= 1 && exprlen <= 2){
    size_t rows, cols;
    pmath_expr_t mat = pmath_expr_get_item(expr, 1);
    
    if(_pmath_is_matrix(mat, &rows, &cols)
    && cols == 2){
      size_t i, j;
      
      if(exprlen == 2){
        pmath_t def = pmath_expr_get_item(expr, 2);
        pmath_unref(expr);
        
        def = object_to_boxes(thread, def);
        
        def = pmath_build_value("os", def, "True");
        
        mat = pmath_expr_append(mat, 1, def);
      }
      else
        pmath_unref(expr);
      
      for(i = 1;i <= rows;++i){
        pmath_t row = pmath_expr_get_item(mat, i);
        mat = pmath_expr_set_item(mat, i, PMATH_NULL);
        
        for(j = 1;j <= cols;++j){
          pmath_t item = pmath_expr_get_item(row, j);
          
          item = object_to_boxes(thread, item);
          
          row = pmath_expr_set_item(row, j, item);
        }
        
        mat = pmath_expr_set_item(mat, i, row);
      }
      
      return pmath_build_value("co", 
        PMATH_CHAR_PIECEWISE, 
        pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_GRIDBOX), 1,
          mat));
    }
    
    pmath_unref(mat);
  }

  return call_to_boxes(thread, expr);
}

static pmath_t rotate_to_boxes(
  pmath_thread_t thread,
  pmath_expr_t   expr    // will be freed
){
  if(pmath_expr_length(expr) == 2){
    pmath_t angle, obj;
      
    obj = pmath_expr_get_item(expr, 1);
    obj = object_to_boxes(thread, obj);
    
    angle = pmath_expr_get_item(expr, 2);
    pmath_unref(expr);
    
    return pmath_expr_new_extended(
      pmath_ref(PMATH_SYMBOL_ROTATIONBOX), 2,
      obj,
      pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_RULE), 2,
        pmath_ref(PMATH_SYMBOL_BOXROTATION),
        angle));
  }
  
  return call_to_boxes(thread, expr);
}

static pmath_t rawboxes_to_boxes(
  pmath_thread_t thread,
  pmath_expr_t   expr    // will be freed
){
  if(pmath_expr_length(expr) == 1){
    pmath_t obj;
      
    obj = pmath_expr_get_item(expr, 1);
    pmath_unref(expr);
    
    return obj;
  }
  
  return call_to_boxes(thread, expr);
}

static pmath_t standardform_to_boxes(
  pmath_thread_t thread,
  pmath_expr_t   expr    // will be freed
){
  if(pmath_expr_length(expr) == 1){
    pmath_t result;
    uint8_t old_boxform = thread->boxform;
    thread->boxform = BOXFORM_STANDARD;

    result = object_to_boxes(thread, pmath_expr_get_item(expr, 1));

    thread->boxform = old_boxform;

    return pmath_expr_new_extended(
      pmath_ref(PMATH_SYMBOL_INTERPRETATIONBOX), 2,
      result,
      expr);
  }

  return call_to_boxes(thread, expr);
}

static pmath_t placeholder_to_boxes(
  pmath_thread_t thread,
  pmath_expr_t   expr    // will be freed
){
  if(pmath_expr_length(expr) == 0){
    pmath_unref(expr);
    
    return pmath_build_value("c", PMATH_CHAR_PLACEHOLDER);
  }
  
  if(pmath_expr_length(expr) == 1){
    pmath_t description = pmath_expr_get_item(expr, 1);
    pmath_unref(expr);
    
    description = object_to_boxes(thread, description);
    
    return pmath_expr_new_extended(
      pmath_ref(PMATH_SYMBOL_TAGBOX), 2,
      pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_FRAMEBOX), 1,
        description),
      PMATH_C_STRING("Placeholder"));
  }
  
  return call_to_boxes(thread, expr);
}

//} ... boxforms valid for StandardForm

static pmath_t expr_to_boxes(pmath_thread_t thread, pmath_expr_t expr){
  pmath_t head = pmath_expr_get_item(expr, 0);
  size_t  len  = pmath_expr_length(expr);
  
  if(pmath_is_symbol(head)){
    if(len == 1){
      pmath_t op;
      int prec;
      
      op = simple_prefix(head, &prec, thread->boxform);
      if(!pmath_is_null(op)){
        pmath_unref(head);
        head = pmath_expr_get_item(expr, 1);
        pmath_unref(expr);
        head = ensure_min_precedence(object_to_boxes(thread, head), prec, +1);
        return pmath_build_value("(oo)", op, head);
      }
      
      op = simple_postfix(head, &prec, thread->boxform);
      if(!pmath_is_null(op)){
        pmath_unref(head);
        head = pmath_expr_get_item(expr, 1);
        pmath_unref(expr);
        head = ensure_min_precedence(object_to_boxes(thread, head), prec, -1);
        return pmath_build_value("(oo)", head, op);
      }
    }
    else if(len == 2){
      pmath_t op;
      int prec1, prec2;
      
      op = simple_binary(head, &prec1, &prec2, thread->boxform);
      if(!pmath_is_null(op)){
        pmath_unref(head);
        return nary_to_boxes(
          thread,
          expr,
          op,
          prec1,
          prec2,
          FALSE);
      }
    }
    else if(len >= 3){
      pmath_t op;
      int prec;
      
      op = simple_nary(head, &prec, thread->boxform);
      if(!pmath_is_null(op)){
        pmath_unref(head);
        return nary_to_boxes(
          thread,
          expr,
          op,
          prec+1,
          prec+1,
          FALSE);
      }
    }
  
    pmath_unref(head);
    
    if(pmath_same(head, PMATH_SYMBOL_COMPLEX))
      return complex_to_boxes(thread, expr);
    
    if(pmath_same(head, PMATH_SYMBOL_DIRECTEDINFINITY))
      return directedinfinity_to_boxes(thread, expr);
    
    if(pmath_same(head, PMATH_SYMBOL_EVALUATIONSEQUENCE))
      return evaluationsequence_to_boxes(thread, expr);
    
    if(pmath_same(head, PMATH_SYMBOL_INEQUATION))
      return inequation_to_boxes(thread, expr);
    
    if(pmath_same(head, PMATH_SYMBOL_LIST))
      return list_to_boxes(thread, expr);
    
    if(pmath_same(head, PMATH_SYMBOL_OPTIONAL))
      return optional_to_boxes(thread, expr);
    
    if(pmath_same(head, PMATH_SYMBOL_PATTERN))
      return pattern_to_boxes(thread, expr);
    
    if(pmath_same(head, PMATH_SYMBOL_PLUS))
      return plus_to_boxes(thread, expr);
    
    if(pmath_same(head, PMATH_SYMBOL_POWER))
      return power_to_boxes(thread, expr);
    
    if(pmath_same(head, PMATH_SYMBOL_PUREARGUMENT))
      return pureargument_to_boxes(thread, expr);
    
    if(pmath_same(head, PMATH_SYMBOL_RANGE))
      return range_to_boxes(thread, expr);
    
    if(pmath_same(head, PMATH_SYMBOL_REPEATED))
      return repeated_to_boxes(thread, expr);
    
    if(pmath_same(head, PMATH_SYMBOL_SINGLEMATCH))
      return singlematch_to_boxes(thread, expr);
    
    if(pmath_same(head, PMATH_SYMBOL_TAGASSIGN)
    || pmath_same(head, PMATH_SYMBOL_TAGASSIGNDELAYED))
      return tagassign_to_boxes(thread, expr);
    
    if(pmath_same(head, PMATH_SYMBOL_TIMES))
      return times_to_boxes(thread, expr);
    
    /*------------------------------------------------------------------------*/
    if(thread->boxform < BOXFORM_INPUT){
      if(pmath_same(head, PMATH_SYMBOL_COLUMN))
        return column_to_boxes(thread, expr);
      
      if(pmath_same(head, PMATH_SYMBOL_FULLFORM))
        return fullform_to_boxes(thread, expr);
      
      if(pmath_same(head, PMATH_SYMBOL_GRID))
        return grid_to_boxes(thread, expr);
      
      if(pmath_same(head, PMATH_SYMBOL_HOLDFORM))
        return holdform_to_boxes(thread, expr);
      
      if(pmath_same(head, PMATH_SYMBOL_INPUTFORM))
        return inputform_to_boxes(thread, expr);
      
      if(pmath_same(head, PMATH_SYMBOL_INTERPRETATION))
        return interpretation_to_boxes(thread, expr);
      
      if(pmath_same(head, PMATH_SYMBOL_LONGFORM))
        return longform_to_boxes(thread, expr);
      
      if(pmath_same(head, PMATH_SYMBOL_MATRIXFORM))
        return matrixform_to_boxes(thread, expr);
      
      if(pmath_same(head, PMATH_SYMBOL_OUTPUTFORM))
        return outputform_to_boxes(thread, expr);
        
      if(pmath_same(head, PMATH_SYMBOL_ROW))
        return row_to_boxes(thread, expr);
      
      if(pmath_same(head, PMATH_SYMBOL_SHALLOW))
        return shallow_to_boxes(thread, expr);
      
      if(pmath_same(head, PMATH_SYMBOL_SHORT))
        return short_to_boxes(thread, expr);
      
      if(pmath_same(head, PMATH_SYMBOL_SKELETON))
        return skeleton_to_boxes(thread, expr);
      
      if(pmath_same(head, PMATH_SYMBOL_STRINGFORM)){
        pmath_t res = _pmath_stringform_to_boxes(expr);
        
        if(!pmath_is_null(res)){
          pmath_unref(expr);
          return res;
        }
        
        return call_to_boxes(thread, expr);
      }
        
      if(pmath_same(head, PMATH_SYMBOL_SUBSCRIPT))
        return subscript_to_boxes(thread, expr);
      
      if(pmath_same(head, PMATH_SYMBOL_SUBSUPERSCRIPT))
        return subsuperscript_to_boxes(thread, expr);
      
      if(pmath_same(head, PMATH_SYMBOL_SUPERSCRIPT))
        return superscript_to_boxes(thread, expr);
      
      if(pmath_same(head, PMATH_SYMBOL_STYLE))
        return style_to_boxes(thread, expr);
      
      if(pmath_same(head, PMATH_SYMBOL_UNDERSCRIPT)
      || pmath_same(head, PMATH_SYMBOL_OVERSCRIPT))
        return underscript_or_overscript_to_boxes(thread, expr);
      
      if(pmath_same(head, PMATH_SYMBOL_UNDEROVERSCRIPT))
        return underoverscript_to_boxes(thread, expr);
      
      /*----------------------------------------------------------------------*/
      if(thread->boxform < BOXFORM_OUTPUT){
        if(pmath_same(head, PMATH_SYMBOL_FRAMED))
          return framed_to_boxes(thread, expr);
          
        if(pmath_same(head, PMATH_SYMBOL_PIECEWISE))
          return piecewise_to_boxes(thread, expr);
        
        if(pmath_same(head, PMATH_SYMBOL_ROTATE))
          return rotate_to_boxes(thread, expr);
        
        if(pmath_same(head, PMATH_SYMBOL_RAWBOXES))
          return rawboxes_to_boxes(thread, expr);
        
        if(pmath_same(head, PMATH_SYMBOL_STANDARDFORM))
          return standardform_to_boxes(thread, expr);
        
        if(pmath_same(head, PMATH_SYMBOL_PLACEHOLDER))
          return placeholder_to_boxes(thread, expr);
      }
    }
  }
  else
    pmath_unref(head);
    
  return call_to_boxes(thread, expr);
}

  static pmath_bool_t user_make_boxes(pmath_t *obj){
    if(pmath_is_symbol(*obj) || pmath_is_expr(*obj)){
      pmath_symbol_t head = _pmath_topmost_symbol(*obj);
      
      if(!pmath_is_null(head)){
        struct _pmath_symbol_rules_t *rules;
        
        rules = _pmath_symbol_get_rules(head, RULES_READ);
        
        if(rules){
          pmath_t result = pmath_expr_new_extended(
            pmath_ref(PMATH_SYMBOL_MAKEBOXES), 1,
            pmath_ref(*obj));
            
          if(_pmath_rulecache_find(&rules->format_rules, &result)){
            pmath_unref(head);
            pmath_unref(*obj);
            *obj = pmath_evaluate(result);
            return TRUE;
          }
          
          pmath_unref(result);
        }
        
        pmath_unref(head);
      }
    }
    
    return FALSE;
  }

static pmath_t object_to_boxes(pmath_thread_t thread, pmath_t obj){
  if(pmath_is_double(obj)
  || pmath_is_int32(obj)){
    pmath_string_t s = PMATH_NULL;
    pmath_write(
      obj, 
      PMATH_WRITE_OPTIONS_INPUTEXPR, 
      (pmath_write_func_t)_pmath_write_to_string, 
      &s);
    pmath_unref(obj);

    if(pmath_string_length(s) > 0
    && *pmath_string_buffer(s) == '-'){
      pmath_string_t minus = pmath_string_part(pmath_ref(s), 0, 1);
      return pmath_build_value("(oo)", minus, pmath_string_part(s, 1, -1));
    }

    return s;
  }
  
  if(pmath_is_ministr(obj)){
    pmath_string_t quote = PMATH_C_STRING("\"");

    obj = _pmath_string_escape(
      pmath_ref(quote),
      obj,
      pmath_ref(quote),
      thread->boxform >= BOXFORM_INPUT);

    pmath_unref(quote);
    return obj;
  }
  
  if(!pmath_is_pointer(obj)){
    char s[80];
    
    if(thread->boxform <= BOXFORM_OUTPUTEXPONENT){
      snprintf(s, sizeof(s), "<<\? %d,%x \?>>", 
        (int)PMATH_AS_TAG(obj), 
        (int)PMATH_AS_INT32(obj));
      return PMATH_C_STRING(s);
    }
    
    snprintf(s, sizeof(s), "/* %d,%x */", 
      (int)PMATH_AS_TAG(obj), 
      (int)PMATH_AS_INT32(obj));
    return pmath_build_value("sss", "/\\/", " ", s);
  }
  
  if(PMATH_AS_PTR(obj) == NULL)
    return PMATH_C_STRING("/\\/");
  
  if(thread->boxform < BOXFORM_OUTPUT
  && user_make_boxes(&obj))
    return obj;
  
  if(!pmath_is_pointer(obj) || PMATH_AS_PTR(obj) == NULL){
    pmath_debug_print("makeboxes: unexpected\n");
    return PMATH_C_STRING("/\\/");
  }
  
  switch(PMATH_AS_PTR(obj)->type_shift){
    case PMATH_TYPE_SHIFT_SYMBOL: {
      pmath_string_t s = PMATH_NULL;
      
      if(thread->boxform < BOXFORM_OUTPUT){
        if(pmath_same(obj, PMATH_SYMBOL_PI)){
          pmath_unref(obj);
          return pmath_build_value("c", 0x03C0);
        }
        
        if(pmath_same(obj, PMATH_SYMBOL_E)){
          pmath_unref(obj);
          return pmath_build_value("c", 0x2147);
        }
        
        if(pmath_same(obj, PMATH_SYMBOL_INFINITY)){
          pmath_unref(obj);
          return pmath_build_value("c", 0x221E);
        }
      }
      
      pmath_write(
        obj, 
        thread->longform ? PMATH_WRITE_OPTIONS_FULLNAME : 0, 
        (pmath_write_func_t)_pmath_write_to_string, 
        &s);
      pmath_unref(obj);
      return s;
    }
    
    case PMATH_TYPE_SHIFT_EXPRESSION_GENERAL:
    case PMATH_TYPE_SHIFT_EXPRESSION_GENERAL_PART: {
      return expr_to_boxes(thread, obj);
    }
    
    case PMATH_TYPE_SHIFT_MP_FLOAT:
    case PMATH_TYPE_SHIFT_MP_INT: {
      pmath_string_t s = PMATH_NULL;
      pmath_write(
        obj, 
        PMATH_WRITE_OPTIONS_INPUTEXPR, 
        (pmath_write_func_t)_pmath_write_to_string, 
        &s);
      pmath_unref(obj);

      if(pmath_string_length(s) > 0
      && *pmath_string_buffer(s) == '-'){
        pmath_string_t minus = pmath_string_part(pmath_ref(s), 0, 1);
        return pmath_build_value("(oo)", minus, pmath_string_part(s, 1, -1));
      }

      return s;
    }

    case PMATH_TYPE_SHIFT_QUOTIENT: {
      pmath_string_t s, n, d;
      pmath_t result;
      s = n = d = PMATH_NULL;

      pmath_write(
        PMATH_QUOT_NUM(obj),
        PMATH_WRITE_OPTIONS_INPUTEXPR,
        (pmath_write_func_t)_pmath_write_to_string,
        &n);

      pmath_write(
        PMATH_QUOT_DEN(obj),
        PMATH_WRITE_OPTIONS_INPUTEXPR,
        (pmath_write_func_t)_pmath_write_to_string,
        &d);

      if(pmath_string_length(n) > 0 && *pmath_string_buffer(n) == '-'){
        s = pmath_string_part(pmath_ref(n), 0, 1);
        n = pmath_string_part(n, 1, -1);
      }
      
      pmath_unref(obj);
      
      if(thread->boxform != BOXFORM_STANDARDEXPONENT
      && thread->boxform != BOXFORM_OUTPUTEXPONENT
      && thread->boxform <= BOXFORM_OUTPUT){
        result = pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_FRACTIONBOX), 2,
          n,
          d);
      }
      else{
        result = pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_LIST), 3,
          n,
          PMATH_C_STRING("/"),
          d);
      }
      
      if(!pmath_is_null(s))
        return pmath_build_value("(oo)", s, result);
      
      return result;
    }

    case PMATH_TYPE_SHIFT_BIGSTRING: {
      pmath_string_t quote = PMATH_C_STRING("\"");

      obj = _pmath_string_escape(
        pmath_ref(quote),
        obj,
        pmath_ref(quote),
        thread->boxform >= BOXFORM_INPUT);

      pmath_unref(quote);
      return obj;
    }
  }
  
  pmath_unref(obj);
  if(thread->boxform < BOXFORM_OUTPUT){
    return PMATH_C_STRING("\"<<\? unprintable \?>>\"");
  }
  else{
    return PMATH_C_STRING("\"\xAB\? unprintable \?\xBB\"");
  }
}

//} ... boxforms for more complex functions


PMATH_PRIVATE pmath_t builtin_makeboxes(pmath_expr_t expr){
/* MakeBoxes(object)
 */
  pmath_thread_t thread;
  pmath_t obj;

  thread = pmath_thread_get_current();
  if(!thread){
    pmath_unref(expr);
    return PMATH_NULL;
  }

  if(pmath_expr_length(expr) != 1){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }

  obj = pmath_expr_get_item(expr, 1);
  pmath_unref(expr);
  
  return object_to_boxes(thread, obj);
}

PMATH_PRIVATE pmath_t builtin_assign_makeboxes(pmath_expr_t expr){
  struct _pmath_symbol_rules_t *rules;
  pmath_t tag;
  pmath_t lhs;
  pmath_t rhs;
  pmath_t sym;
  pmath_t arg;
  
  if(!_pmath_is_assignment(expr, &tag, &lhs, &rhs))
    return expr;
  
  if(!pmath_is_expr_of_len(lhs, PMATH_SYMBOL_MAKEBOXES, 1)){
    pmath_unref(tag);
    pmath_unref(lhs);
    pmath_unref(rhs);
    return expr;
  }
  
  arg = pmath_expr_get_item(lhs, 1);
  sym = _pmath_topmost_symbol(arg);
  pmath_unref(arg);
  
  if(!pmath_same(tag, PMATH_UNDEFINED)
  && !pmath_same(tag, sym)){
    pmath_message(PMATH_NULL, "tag", 3, tag, lhs, sym);
    
    pmath_unref(expr);
    if(pmath_same(rhs, PMATH_UNDEFINED))
      return pmath_ref(PMATH_SYMBOL_FAILED);
    return rhs;
  }
  
  pmath_unref(tag);
  pmath_unref(expr);
  
  rules = _pmath_symbol_get_rules(sym, RULES_WRITE);
  pmath_unref(sym);
  
  if(!rules){
    pmath_unref(lhs);
    pmath_unref(rhs);
    return pmath_ref(PMATH_SYMBOL_FAILED);
  }
  
  _pmath_rulecache_change(&rules->format_rules, lhs, rhs);
  
  return PMATH_NULL;
}

PMATH_PRIVATE pmath_t builtin_parenthesizeboxes(pmath_expr_t expr){
/* ParenthesizeBoxes(obj, prec, pos)
 */
  pmath_t box, precobj, posobj;
  int prec, pos;
  
  if(pmath_expr_length(expr) != 3){
    pmath_message_argxxx(pmath_expr_length(expr), 3, 3);
    return expr;
  }
  
  pos = 0;
  box     = pmath_expr_get_item(expr, 1);
  precobj = pmath_expr_get_item(expr, 2);
  posobj  = pmath_expr_get_item(expr, 3);
  
  if(pmath_is_string(posobj)){
    if(     pmath_string_equals_latin1(posobj, "Prefix"))  pos = +1;
    else if(pmath_string_equals_latin1(posobj, "Postfix")) pos = -1;
    else if(pmath_string_equals_latin1(posobj, "Infix"))   pos = 0;
  }
  
  prec = _pmath_symbol_to_precedence(precobj);
  pmath_unref(precobj);
  pmath_unref(posobj);
  pmath_unref(expr);
  
  box = ensure_min_precedence(box, prec+1, pos);

  return box;
}
