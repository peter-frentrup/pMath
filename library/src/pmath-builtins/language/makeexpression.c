#include <pmath-core/expressions-private.h>
#include <pmath-core/numbers-private.h>
#include <pmath-core/strings-private.h>

#include <pmath-language/charnames.h>
#include <pmath-language/patterns-private.h>
#include <pmath-language/tokens.h>

#include <pmath-util/concurrency/threads.h>
#include <pmath-util/emit-and-gather.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/memory.h>
#include <pmath-util/messages.h>
#include <pmath-util/strtod.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/control-private.h>
#include <pmath-builtins/lists-private.h>

#include <string.h>


static uint16_t unichar_at(
  pmath_expr_t expr,
  size_t i
){
  pmath_string_t obj = pmath_expr_get_item(expr, i);
  uint16_t result = 0;
  if(pmath_is_string(obj) && pmath_string_length(obj) == 1){
    result = *pmath_string_buffer(&obj);
  }
  pmath_unref(obj);
  return result;
}

static pmath_bool_t string_equals(pmath_string_t str, const char *cstr){
  const uint16_t *buf;
  size_t len = strlen(cstr);
  if((size_t)pmath_string_length(str) != len)
    return FALSE;

  buf = pmath_string_buffer(&str);
  while(len-- > 0)
    if(*buf++ != *cstr++)
      return FALSE;

  return TRUE;
}

static pmath_bool_t is_string_at(
  pmath_expr_t expr,
  size_t i,
  const char *str
){
  pmath_string_t obj = pmath_expr_get_item(expr, i);
  const uint16_t *buf;
  size_t len;

  if(!pmath_is_string(obj)){
    pmath_unref(obj);
    return FALSE;
  }

  len = strlen(str);
  if(len != (size_t)pmath_string_length(obj)){
    pmath_unref(obj);
    return FALSE;
  }

  buf = pmath_string_buffer(&obj);
  while(len-- > 0)
    if(*buf++ != (unsigned char)*str++){
      pmath_unref(obj);
      return FALSE;
    }

  pmath_unref(obj);
  return TRUE;
}

static int hex(uint16_t ch){
  if(ch >= '0' && ch <= '9')
    return ch - '0';
  if(ch >= 'a' && ch <= 'f')
    return ch - 'a' + 10;
  if(ch >= 'A' && ch <= 'F')
    return ch - 'A' + 10;
  return -1;
}

#define HOLDCOMPLETE(result) pmath_expr_new_extended(\
  pmath_ref(PMATH_SYMBOL_HOLDCOMPLETE), 1, result)

static pmath_bool_t parse(pmath_t *box){
// *box = PMATH_NULL if result is FALSE
  pmath_t obj;

  *box = pmath_evaluate(
    pmath_expr_new_extended(
      pmath_ref(PMATH_SYMBOL_MAKEEXPRESSION), 1, *box));

  if(!pmath_is_expr(*box)){
    pmath_unref(*box);
    *box = PMATH_NULL;
    return FALSE;
  }

  obj = pmath_expr_get_item(*box, 0);
  pmath_unref(obj);

  if(!pmath_same(obj, PMATH_SYMBOL_HOLDCOMPLETE)){
    pmath_message(PMATH_NULL, "inv", 1, *box);
    *box = PMATH_NULL;
    return FALSE;
  }

  if(pmath_expr_length(*box) != 1){
    *box = pmath_expr_set_item(*box, 0, pmath_ref(PMATH_SYMBOL_SEQUENCE));
    return TRUE;
  }

  obj = *box;
  *box = pmath_expr_get_item(*box, 1);
  pmath_unref(obj);
  return TRUE;
}

/*static pmath_bool_t parse_quiet(pmath_t *box){
  pmath_t obj;
  
  *box = pmath_expr_new_extended(
    pmath_ref(PMATH_SYMBOL_TRY), 3,
    pmath_expr_new_extended(
      pmath_ref(PMATH_SYMBOL_MAKEEXPRESSION), 1, 
      *box),
    pmath_ref(PMATH_SYMBOL_FAILED),
    pmath_expr_new_extended(
      pmath_ref(PMATH_SYMBOL_MESSAGENAME), 2,
      pmath_ref(PMATH_SYMBOL_MESSAGE),
      PMATH_C_STRING("inv")));
    
  *box = pmath_evaluate(*box);
      
  if(!pmath_is_expr(*box)){
    pmath_unref(*box);
    *box = PMATH_NULL;
    return FALSE;
  }

  obj = pmath_expr_get_item(*box, 0);
  pmath_unref(obj);

  if(!pmath_same(obj, PMATH_SYMBOL_HOLDCOMPLETE)){
    pmath_unref(*box);
    *box = PMATH_NULL;
    return FALSE;
  }

  if(pmath_expr_length(*box) != 1){
    *box = pmath_expr_set_item(*box, 0, pmath_ref(PMATH_SYMBOL_SEQUENCE));
    return TRUE;
  }

  obj = *box;
  *box = pmath_expr_get_item(*box, 1);
  pmath_unref(obj);
  return TRUE;
} */

static void handle_row_error_at(pmath_expr_t expr, size_t i){
  if(i == 1){
    pmath_message(PMATH_NULL, "bgn", 1, 
      pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_RAWBOXES), 1, 
        pmath_ref(expr)));
  }
  else{
    pmath_message(PMATH_NULL, "nxt", 2, 
      pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_RAWBOXES), 1, 
        pmath_expr_get_item_range(expr, 1, i-1)), 
      pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_RAWBOXES), 1, 
        pmath_expr_get_item_range(expr, i, SIZE_MAX)));
  }
}

#define is_parse_error(x)  pmath_is_magic(x)

static pmath_t parse_at(pmath_expr_t expr, size_t i){
  pmath_t result = pmath_expr_get_item(expr, i);
  
  if(pmath_is_string(result)){
    if(!parse/*_quiet*/(&result)){
      handle_row_error_at(expr, i);
      
      return PMATH_UNDEFINED;
    }
    
    return result;
  }
  
  if(parse(&result))
    return result;
  
  return PMATH_UNDEFINED;
}

static pmath_string_t box_as_string(pmath_t box){
  while(!pmath_is_null(box)){
    if(pmath_is_string(box))
      return box;

    if(pmath_is_expr(box)
    && pmath_expr_length(box) == 1){
      pmath_t obj = pmath_expr_get_item(box, 1);
      pmath_unref(box);
      box = obj;
    }
    else{
      pmath_unref(box);
      return PMATH_NULL;
    }
  }

  return PMATH_NULL;
}

static pmath_symbol_t inset_operator(uint16_t ch){ // do not free result!
  switch(ch){
    case '|': return PMATH_SYMBOL_ALTERNATIVES;
//    case ':': return PMATH_SYMBOL_PATTERN;
//    case '?': return PMATH_SYMBOL_TESTPATTERN;
    case '^': return PMATH_SYMBOL_POWER;

    case 0x00B7: return PMATH_SYMBOL_DOT;

//    case PMATH_CHAR_RULE:        return PMATH_SYMBOL_RULE;
//    case PMATH_CHAR_RULEDELAYED: return PMATH_SYMBOL_RULEDELAYED;

//    case PMATH_CHAR_ASSIGN:        return PMATH_SYMBOL_ASSIGN;
//    case PMATH_CHAR_ASSIGNDELAYED: return PMATH_SYMBOL_ASSIGNDELAYED;

    case 0x00B1: return PMATH_SYMBOL_PLUSMINUS;

    case 0x2213: return PMATH_SYMBOL_MINUSPLUS;

//    case 0x2227: return PMATH_SYMBOL_AND;
//    case 0x2228: return PMATH_SYMBOL_OR;
    case 0x2229: return PMATH_SYMBOL_INTERSECTION;
    case 0x222A: return PMATH_SYMBOL_UNION;

    case 0x2236: return PMATH_SYMBOL_COLON;

    case 0x2295: return PMATH_SYMBOL_CIRCLEPLUS;
    case 0x2297: return PMATH_SYMBOL_CIRCLETIMES;

    case 0x22C5: return PMATH_SYMBOL_DOT;

    case 0x2A2F: return PMATH_SYMBOL_CROSS;
  }
  return PMATH_NULL;
}

static pmath_symbol_t relation_at(pmath_expr_t expr, size_t i){ // do not free result
  uint16_t ch = unichar_at(expr, i);
  
  switch(ch){
    case 0:
      if(is_string_at(expr, i, "<="))  return PMATH_SYMBOL_LESSEQUAL;
      if(is_string_at(expr, i, ">="))  return PMATH_SYMBOL_GREATEREQUAL;
      if(is_string_at(expr, i, "!="))  return PMATH_SYMBOL_UNEQUAL;
      if(is_string_at(expr, i, "===")) return PMATH_SYMBOL_IDENTICAL;
      if(is_string_at(expr, i, "=!=")) return PMATH_SYMBOL_UNIDENTICAL;
      return PMATH_NULL;
    
    case '<': return PMATH_SYMBOL_LESS;
    case '>': return PMATH_SYMBOL_GREATER;
    case '=': return PMATH_SYMBOL_EQUAL;
  }
  
  switch(ch){
    case 0x2208: return PMATH_SYMBOL_ELEMENT;
    case 0x2209: return PMATH_SYMBOL_NOTELEMENT;
    case 0x220B: return PMATH_SYMBOL_REVERSEELEMENT;
    case 0x220C: return PMATH_SYMBOL_NOTREVERSEELEMENT;
  }
  
  switch(ch){
    case 0x2243: return PMATH_SYMBOL_TILDEEQUAL;
    case 0x2244: return PMATH_SYMBOL_NOTTILDEEQUAL;
    case 0x2245: return PMATH_SYMBOL_TILDEFULLEQUAL;
    
    case 0x2247: return PMATH_SYMBOL_NOTTILDEFULLEQUAL;
    case 0x2248: return PMATH_SYMBOL_TILDETILDE;
    case 0x2249: return PMATH_SYMBOL_NOTTILDETILDE;
    
    case 0x224D: return PMATH_SYMBOL_CUPCAP;
    case 0x224E: return PMATH_SYMBOL_HUMPDOWNHUMP;
    case 0x224F: return PMATH_SYMBOL_HUMPEQUAL;
    case 0x2250: return PMATH_SYMBOL_DOTEQUAL;
  }
  
  switch(ch){
    case 0x2260: return PMATH_SYMBOL_UNEQUAL;
    case 0x2261: return PMATH_SYMBOL_CONGRUENT;
    case 0x2262: return PMATH_SYMBOL_NOTCONGRUENT;
    
    case 0x2264: return PMATH_SYMBOL_LESSEQUAL;
    case 0x2265: return PMATH_SYMBOL_GREATEREQUAL;
    case 0x2266: return PMATH_SYMBOL_LESSFULLEQUAL;
    case 0x2267: return PMATH_SYMBOL_GREATERFULLEQUAL;
    
    case 0x226A: return PMATH_SYMBOL_LESSLESS;
    case 0x226B: return PMATH_SYMBOL_GREATERGREATER;
    
    case 0x226D: return PMATH_SYMBOL_NOTCUPCAP;
    case 0x226E: return PMATH_SYMBOL_NOTLESS;
    case 0x226F: return PMATH_SYMBOL_NOTGREATER;
    case 0x2270: return PMATH_SYMBOL_NOTLESSEQUAL;
    case 0x2271: return PMATH_SYMBOL_NOTGREATEREQUAL;
    case 0x2272: return PMATH_SYMBOL_LESSTILDE;
    case 0x2273: return PMATH_SYMBOL_GREATERTILDE;
    case 0x2274: return PMATH_SYMBOL_NOTLESSTILDE;
    case 0x2275: return PMATH_SYMBOL_NOTGREATERTILDE;
    case 0x2276: return PMATH_SYMBOL_LESSGREATER;
    case 0x2277: return PMATH_SYMBOL_GREATERLESS;
    case 0x2278: return PMATH_SYMBOL_NOTLESSGREATER;
    case 0x2279: return PMATH_SYMBOL_NOTGREATERLESS;
    case 0x227A: return PMATH_SYMBOL_PRECEDES;
    case 0x227B: return PMATH_SYMBOL_SUCCEEDS;
    case 0x227C: return PMATH_SYMBOL_PRECEDESEQUAL;
    case 0x227D: return PMATH_SYMBOL_SUCCEEDSEQUAL;
    case 0x227E: return PMATH_SYMBOL_PRECEDESTILDE;
    case 0x227F: return PMATH_SYMBOL_SUCCEEDSTILDE;
    case 0x2280: return PMATH_SYMBOL_NOTPRECEDES;
    case 0x2281: return PMATH_SYMBOL_NOTSUCCEEDS;
    case 0x2282: return PMATH_SYMBOL_SUBSET;
    case 0x2283: return PMATH_SYMBOL_SUPERSET;
    case 0x2284: return PMATH_SYMBOL_NOTSUBSET;
    case 0x2285: return PMATH_SYMBOL_NOTSUPERSET;
    case 0x2286: return PMATH_SYMBOL_SUBSETEQUAL;
    case 0x2287: return PMATH_SYMBOL_SUPERSETEQUAL;
    case 0x2288: return PMATH_SYMBOL_NOTSUBSETEQUAL;
    case 0x2289: return PMATH_SYMBOL_NOTSUPERSETEQUAL;
  }
  
  switch(ch){
    case 0x22B3: return PMATH_SYMBOL_LEFTTRIANGLE;
    case 0x22B4: return PMATH_SYMBOL_RIGHTTRIANGLE;
    case 0x22B5: return PMATH_SYMBOL_LEFTTRIANGLEEQUAL;
    case 0x22B6: return PMATH_SYMBOL_RIGHTTRIANGLEEQUAL;
  }
  
  switch(ch){
    case 0x22DA: return PMATH_SYMBOL_LESSEQUALGREATER;
    case 0x22DB: return PMATH_SYMBOL_GREATEREQUALLESS;
    
    case 0x22E0: return PMATH_SYMBOL_NOTPRECEDESEQUAL;
    case 0x22E1: return PMATH_SYMBOL_NOTSUCCEEDSEQUAL;
    
    case 0x22EA: return PMATH_SYMBOL_NOTLEFTTRIANGLE;
    case 0x22EB: return PMATH_SYMBOL_NOTRIGHTTRIANGLE;
    case 0x22EC: return PMATH_SYMBOL_NOTLEFTTRIANGLEEQUAL;
    case 0x22ED: return PMATH_SYMBOL_NOTRIGHTTRIANGLEEQUAL;
  }
  
  return PMATH_NULL;
}

  static void emit_grid_options(
    pmath_expr_t options // wont be freed
  ){
    size_t i;

    for(i = 1;i <= pmath_expr_length(options);++i){
      pmath_t item = pmath_expr_get_item(options, i);

      if(_pmath_is_rule(item)){
        pmath_t lhs = pmath_expr_get_item(item, 1);
        pmath_unref(lhs);

        if(pmath_same(lhs, PMATH_SYMBOL_GRIDBOXCOLUMNSPACING)){
          item = pmath_expr_set_item(item, 1, pmath_ref(PMATH_SYMBOL_COLUMNSPACING));
          pmath_emit(item, PMATH_NULL);
          continue;
        }

        if(pmath_same(lhs, PMATH_SYMBOL_GRIDBOXROWSPACING)){
          item = pmath_expr_set_item(item, 1, pmath_ref(PMATH_SYMBOL_ROWSPACING));
          pmath_emit(item, PMATH_NULL);
          continue;
        }

        pmath_emit(item, PMATH_NULL);
        continue;
      }

      if(pmath_is_expr_of(item, PMATH_SYMBOL_LIST)){
        emit_grid_options(item);
        pmath_unref(item);
        continue;
      }

      pmath_unref(item);
    }
  }

static pmath_t parse_gridbox( // PMATH_NULL on error
  pmath_expr_t expr,           // wont be freed
  pmath_bool_t remove_styling
){
  pmath_expr_t options, matrix, row;
  size_t height, width, i, j;

  options = pmath_options_extract(expr, 1);

  if(pmath_is_null(options))
    return PMATH_NULL;

  matrix = pmath_expr_get_item(expr, 1);

  if(!_pmath_is_matrix(matrix, &height, &width, FALSE)){
    pmath_unref(options);
    pmath_unref(matrix);
    return PMATH_NULL;
  }

  for(i = 1;i <= height;++i){
    row = pmath_expr_get_item(matrix, i);
    matrix = pmath_expr_set_item(matrix, i, PMATH_NULL);

    for(j = 1;j <= width;++j){
      pmath_t obj = pmath_expr_get_item(row, j);
      row = pmath_expr_set_item(row, j, PMATH_NULL);

      if(!parse(&obj)){
        pmath_unref(options);
        pmath_unref(matrix);
        pmath_unref(row);
        return PMATH_NULL;
      }

      row = pmath_expr_set_item(row, j, obj);
    }

    matrix = pmath_expr_set_item(matrix, i, row);
  }

  if(remove_styling){
    pmath_unref(options);
    return HOLDCOMPLETE(matrix);
  }

  pmath_gather_begin(PMATH_NULL);
  pmath_emit(matrix, PMATH_NULL);
  emit_grid_options(options);
  pmath_unref(options);

  row = pmath_gather_end();
  row = pmath_expr_set_item(row, 0, pmath_ref(PMATH_SYMBOL_GRID));
  return HOLDCOMPLETE(row);
}

PMATH_PRIVATE pmath_t _pmath_parse_number(
  pmath_string_t  string, // will be freed
  pmath_bool_t    alternative
){
  pmath_number_t result;
  pmath_integer_t exponent;
  char *cstr;
  pmath_bool_t is_mp_float = FALSE;
  int start = 0;
  int end;
  int base = 10;
  int i, j;
  pmath_precision_control_t prec_control = PMATH_PREC_CTRL_AUTO;
  double prec_acc = HUGE_VAL;
  pmath_bool_t neg = FALSE;

  const uint16_t *str = pmath_string_buffer(&string);
  int             len = pmath_string_length(string);

  if(len == 0){
    pmath_unref(string);
    return PMATH_NULL;
  }

  i = 0;

  if(str[0] == '+'){
    start = i = 1;
  }
  else if(str[0] == '-'){
    start = i = 1;
    neg = TRUE;
  }

  if(len == 0){
    pmath_unref(string);
    return PMATH_NULL;
  }

  while(i < len && '0' <= str[i] && str[i] <= '9')
    ++i;

  if(i + 2 < len && str[i] == '^' && str[i+1] == '^'){
    cstr = (char*)pmath_mem_alloc(i + 1);
    if(!cstr){
      pmath_unref(string);
      return PMATH_NULL;
    }

    for(j = 0;j < i;++j)
      cstr[j] = (char)str[j];
    cstr[i] = '\0';
    base = (int)strtol(cstr, NULL, 10);
    pmath_mem_free(cstr);

    if(base < 2 || base > 36){
      if(!alternative){
        pmath_message(
          PMATH_NULL, "base", 2,
          pmath_string_part(string, 0, i),
          pmath_ref(string));
      }
      else
        pmath_unref(string);

      return PMATH_NULL;
    }

    start = i+= 2;
  }

  while(i < len && pmath_char_is_basedigit(base, str[i]))
    ++i;

  if(i < len && str[i] == '.'){
    ++i;
    is_mp_float = TRUE;
    while(i < len && pmath_char_is_basedigit(base, str[i]))
      ++i;
  }

  if(!alternative && i < len && pmath_char_is_36digit(str[i])){
    end = i + 1;
    while(end < len && pmath_char_is_36digit(str[end]))
      ++end;

    pmath_message(
      PMATH_NULL, "digit", 3,
      PMATH_FROM_INT32(i - start + 1),
      pmath_string_part(string, start, end - start),
      PMATH_FROM_INT32(base));

    return PMATH_NULL;
  }

  end = i;

  cstr = (char*)pmath_mem_alloc(len - start + 1);
  if(!cstr){
    pmath_unref(string);
    return PMATH_NULL;
  }

  if(i < len && str[i] == '`'){
    is_mp_float = TRUE;

    ++i;
    if(i < len && str[i] == '`'){
      ++i;

      prec_control = PMATH_PREC_CTRL_GIVEN_ACC;
    }
    else{
      prec_control = PMATH_PREC_CTRL_GIVEN_PREC;
    }

    if(i < len
    && (str[i] == '+' || str[i] == '-' || pmath_char_is_digit(str[i]))){
      cstr[0] = (char)str[i];

      j = i;
      ++i;

      while(i < len && pmath_char_is_digit(str[i])){
        cstr[i - j] = (char)str[i];
        ++i;
      }
      if(i < len && str[i] == '.'){
        cstr[i - j] = (char)str[i];
        ++i;
      }
      while(i < len && pmath_char_is_digit(str[i])){
        cstr[i - j] = (char)str[i];
        ++i;
      }
      cstr[i - j] = '\0';

      prec_acc = pmath_strtod(cstr, NULL);
    }

    if(prec_control == PMATH_PREC_CTRL_GIVEN_PREC
    && !isfinite(prec_acc))
      prec_control = PMATH_PREC_CTRL_MACHINE_PREC;
  }

  for(j = start;j < end;++j)
    cstr[j - start] = (char)str[j];
  cstr[end - start] = '\0';

  exponent = PMATH_NULL;
  if((i + 2 < len && str[i] == '*' && str[i+1] == '^')
  || (alternative && i + 1 < len && (str[i] == 'e' || str[i] == 'E'))){
    int exp;
    int delta = end - start - i;

    if(str[i] == '*'){
      exp = i+= 2;
      delta-= 1;
      if(is_mp_float){
        cstr[i + delta - 1] = base <= 10 ? 'e' : '@';
      }
    }
    else{
      exp = i+= 1;

      is_mp_float = TRUE;
      cstr[i + delta - 1] = base <= 10 ? 'e' : '@';
    }

    if(i + 1 < len && (str[i] == '+' || str[i] == '-'))
      ++i;

    while(i < len && pmath_char_is_digit(str[i]))
      ++i;

    for(j = exp;j < i;++j)
      cstr[j + delta] = (char)str[j];
    cstr[i + delta] = '\0';

    if(!is_mp_float)
      exponent = pmath_integer_new_str(cstr + exp + delta, 10);
  }

  if(is_mp_float)
    result = pmath_float_new_str(cstr, base, prec_control, prec_acc);
  else
    result = pmath_integer_new_str(cstr, base);

  if(!pmath_is_null(exponent)){
    result = pmath_evaluate(
      pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_TIMES), 2,
        result,
        pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_POWER), 2,
          PMATH_FROM_INT32(base),
          exponent)));
  }

  pmath_mem_free(cstr);

  if(i < len){
    if(!alternative)
      pmath_message(PMATH_NULL, "inv", 1, string);
    pmath_unref(result);
    return PMATH_NULL;
  }

  pmath_unref(string);

  if(pmath_is_number(result)){
    if(neg)
      return pmath_number_neg(result);

    return result;
  }

  pmath_unref(result);
  return PMATH_NULL;
}

PMATH_PRIVATE pmath_t builtin_makeexpression(pmath_expr_t expr){
/* MakeExpression(boxes)
   returns $Failed on error and HoldComplete(result) on success.

   options:
     ParserArguments     {arg1, arg2, ...}
     ParseSymbols        True/False
 */
  pmath_t box;
  size_t exprlen = pmath_expr_length(expr);

  if(exprlen > 1){
    pmath_expr_t options = pmath_options_extract(expr, 1);

    if(!pmath_is_null(options)){
      pmath_t args = pmath_option_value(PMATH_NULL, PMATH_SYMBOL_PARSERARGUMENTS, options);
      pmath_t syms = pmath_option_value(PMATH_NULL, PMATH_SYMBOL_PARSESYMBOLS,    options);

      if(!pmath_same(args, PMATH_SYMBOL_AUTOMATIC)){
        args = pmath_thread_local_save(PMATH_THREAD_KEY_PARSERARGUMENTS, args);
      }

      if(!pmath_same(syms, PMATH_SYMBOL_AUTOMATIC)){
        syms = pmath_thread_local_save(PMATH_THREAD_KEY_PARSESYMBOLS, syms);
      }

      box = pmath_expr_get_item(expr, 1);
      pmath_unref(expr);
      expr = pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_MAKEEXPRESSION), 1,
        box);

      expr = pmath_evaluate(expr);

      pmath_unref(options);
      if(!pmath_same(args, PMATH_SYMBOL_AUTOMATIC)){
        pmath_unref(pmath_thread_local_save(PMATH_THREAD_KEY_PARSERARGUMENTS, args));
      }
      else
        pmath_unref(args);

      if(!pmath_same(syms, PMATH_SYMBOL_AUTOMATIC)){
        pmath_unref(pmath_thread_local_save(PMATH_THREAD_KEY_PARSESYMBOLS, syms));
      }
      else
        pmath_unref(syms);
    }

    return expr;
  }

  if(exprlen != 1){
    pmath_message_argxxx(exprlen, 1, 1);
    pmath_unref(expr);
    return pmath_ref(PMATH_SYMBOL_FAILED);
  }

  box = pmath_expr_get_item(expr, 1);
  pmath_unref(expr);

  if(pmath_is_string(box)){
    pmath_token_t tok;
    const uint16_t *str;
    int len = pmath_string_length(box);

    if(len == 0){
      pmath_unref(box);
      return HOLDCOMPLETE(PMATH_NULL);
    }

    str = pmath_string_buffer(&box);

    if(len > 1 && str[0] == '`' && str[len-1] == '`'){
      pmath_t result;

      size_t argi = 1;
      if(len > 2){
        int i;
        argi = 0;
        for(i = 1;i < len - 1;++i)
          argi = 10 * argi + str[i] - '0';
      }

      pmath_unref(box);
      box = pmath_thread_local_load(PMATH_THREAD_KEY_PARSERARGUMENTS);
      if(!pmath_is_expr(box))
        return HOLDCOMPLETE(box);

      result = pmath_expr_get_item(box, argi);
      pmath_unref(box);
      return HOLDCOMPLETE(result);
    }

    tok = pmath_token_analyse(str, 1, NULL);
    if(tok == PMATH_TOK_DIGIT){
      pmath_number_t result = _pmath_parse_number(box, TRUE);

      if(pmath_is_null(result))
        return pmath_ref(PMATH_SYMBOL_FAILED);

      return HOLDCOMPLETE(result);
    }

    if(tok == PMATH_TOK_NAME){
      int i = 1;
      while(i < len){
        if(str[i] == '`'){
          if(i + 1 == len)
            break;

          ++i;
          tok = pmath_token_analyse(str + i, 1, NULL);
          if(tok != PMATH_TOK_NAME)
            break;

          ++i;
          continue;
        }

        tok = pmath_token_analyse(str + i, 1, NULL);
        if(tok != PMATH_TOK_NAME
        && tok != PMATH_TOK_DIGIT)
          break;

        ++i;
      }

      if(i < len){
        pmath_message(PMATH_NULL, "inv", 1, box);
        return pmath_ref(PMATH_SYMBOL_FAILED);
      }

      expr = pmath_thread_local_load(PMATH_THREAD_KEY_PARSESYMBOLS);
      pmath_unref(expr);

      expr = pmath_symbol_find(
        pmath_ref(box),
        pmath_same(expr, PMATH_SYMBOL_TRUE) || pmath_same(expr, PMATH_UNDEFINED));

      if(!pmath_is_null(expr)){
        pmath_unref(box);
        return HOLDCOMPLETE(expr);
      }

      return HOLDCOMPLETE(pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_SYMBOL), 1,
        box));
    }

    if(str[0] == '%'){
      int i;
      for(i = 1;i < len;++i)
        if(str[i] != '%'){
          pmath_unref(box);
          return pmath_ref(PMATH_SYMBOL_FAILED);
        }

      pmath_unref(box);
      return HOLDCOMPLETE(
        pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_HISTORY), 1,
          PMATH_FROM_INT32(-len)));
    }

    if(tok == PMATH_TOK_NAME2){
      expr = pmath_thread_local_load(PMATH_THREAD_KEY_PARSESYMBOLS);
      pmath_unref(expr);

      expr = pmath_symbol_find(
        pmath_ref(box),
        pmath_same(expr, PMATH_SYMBOL_TRUE) || pmath_same(expr, PMATH_UNDEFINED));

      if(!pmath_is_null(expr)){
        pmath_unref(box);
        return HOLDCOMPLETE(expr);
      }

      return HOLDCOMPLETE(pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_SYMBOL), 1,
        box));
    }

    if(str[0] == '"'){
      struct _pmath_string_t *result = _pmath_new_string_buffer(len - 1);
      int j = 0;
      int i = 1;
      int k = 0;
      while(i < len - 1){
        if(k == 0 && str[i] == '\\'){
          ++i;
          if(i == len){
            AFTER_STRING(result)[j++] = '\\';
          }
          else switch(str[i]){
            case '\\':
            case '"': AFTER_STRING(result)[j++] = str[i++]; break;

            case 'n':
              ++i;
              AFTER_STRING(result)[j++] = '\n';
              break;

            case 'r':
              ++i;
              AFTER_STRING(result)[j++] = '\r';
              break;

            case 't':
              ++i;
              AFTER_STRING(result)[j++] = '\t';
              break;

            case '(':
              ++i;
              AFTER_STRING(result)[j++] = PMATH_CHAR_LEFT_BOX;
              break;

            case ')':
              ++i;
              AFTER_STRING(result)[j++] = PMATH_CHAR_RIGHT_BOX;
              break;

            case 'x':
              if(i + 2 < len){
                int h1 = hex(str[++i]);
                int h2 = hex(str[++i]);
                if(h1 >= 0 && h2 >= 0){
                  ++i;
                  AFTER_STRING(result)[j++] = (uint16_t)((h1 << 4) | h2);
                }
                else
                  i-= 2;
              }
              break;

            case 'u':
              if(i + 4 < len){
                int h1 = hex(str[++i]);
                int h2 = hex(str[++i]);
                int h3 = hex(str[++i]);
                int h4 = hex(str[++i]);
                if(h1 >= 0 && h2 >= 0 && h3 >= 0 && h4 >= 0){
                  ++i;
                  AFTER_STRING(result)[j++] = (uint16_t)((h1 << 12) | (h2 << 8) | (h3 << 4) | h4);
                }
                else
                  i-= 4;
              }
              break;

            case 'U':
              if(i + 8 < len){
                int h1 = hex(str[++i]);
                int h2 = hex(str[++i]);
                int h3 = hex(str[++i]);
                int h4 = hex(str[++i]);
                int h5 = hex(str[++i]);
                int h6 = hex(str[++i]);
                int h7 = hex(str[++i]);
                int h8 = hex(str[++i]);
                if(h1 >= 0 && h2 >= 0 && h3 >= 0 && h4 >= 0
                && h5 >= 0 && h6 >= 0 && h7 >= 0 && h8 >= 0){
                  uint32_t u = ((uint32_t)h1) << 28;
                  u|= h2 << 24;
                  u|= h3 << 20;
                  u|= h4 << 16;
                  u|= h5 << 12;
                  u|= h6 <<  8;
                  u|= h7 <<  4;
                  u|= h8;

                  if(u <= 0x10FFFF){
                    ++i;
                    if(u <= 0xFFFF){
                      AFTER_STRING(result)[j++] = (uint16_t)u;
                    }
                    else{
                      u-= 0x10000;
                      AFTER_STRING(result)[j++] = 0xD800 | (uint16_t)((u >> 10) & 0x03FF);
                      AFTER_STRING(result)[j++] = 0xDC00 | (uint16_t)(u & 0x03FF);
                    }
                  }
                  else
                    i-= 8;
                }
                else
                  i-= 8;
              }
              break;

            case '[': {
              int e = i;
              while(e < len && str[e] <= 0x7F && str[e] != ']'){
                ++e;
              }

              if(e < len && str[e] == ']' && e - i - 1 < 64){
                char s[64];
                int ii;
                unsigned int unichar;

                for(ii=0; ii < e-i-1; ++ii){
                  s[ii] = (char)str[i+1+ii];
                }

                s[ii] = '\0';
                unichar = pmath_char_from_name(s);
                if(unichar != 0xFFFFFFFFU){

                  if(unichar <= 0xFFFF){
                    AFTER_STRING(result)[j++] = (uint16_t)unichar;
                  }
                  else{
                    unichar-= 0x10000;
                    AFTER_STRING(result)[j++] = 0xD800 | (uint16_t)((unichar >> 10) & 0x03FF);
                    AFTER_STRING(result)[j++] = 0xDC00 | (uint16_t)(unichar & 0x03FF);
                  }

                  i = e+1;
                  break;
                }
              }
            } /* fall through */

            default:
              if(str[i] <= ' '){
                ++i;
                while(i < len && str[i] <= ' ')
                  ++i;
              }
              else{
                AFTER_STRING(result)[j++] = '\\';
                AFTER_STRING(result)[j++] = str[i++];
              }
          }
        }
        else{
          if(str[i] == PMATH_CHAR_LEFT_BOX)
            ++k;
          else if(str[i] == PMATH_CHAR_RIGHT_BOX)
            --k;

          AFTER_STRING(result)[j++] = str[i++];
        }
      }

      if(i + 1 == len && str[i] == '"'){
        pmath_unref(box);
        result->length = j;
        return HOLDCOMPLETE(_pmath_from_buffer(result));
      }
    }

    if(len == 1 && str[0] == '#'){
      pmath_unref(box);
      return HOLDCOMPLETE(
        pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_PUREARGUMENT), 1,
          PMATH_FROM_INT32(1)));
    }

    if(len == 1 && str[0] == ','){
      pmath_unref(box);
      return pmath_expr_new(
          pmath_ref(PMATH_SYMBOL_HOLDCOMPLETE), 2);
    }

    if(len == 1 && str[0] == '~'){
      pmath_unref(box);
      return HOLDCOMPLETE(pmath_ref(_pmath_object_singlematch));
    }

    if(len == 2 && str[0] == '#' && str[1] == '#'){
      pmath_unref(box);
      return HOLDCOMPLETE(
        pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_PUREARGUMENT), 1,
          pmath_ref(_pmath_object_range_from_one)));
    }

    if(len == 2 && str[0] == '~' && str[1] == '~'){
      pmath_unref(box);
      return HOLDCOMPLETE(pmath_ref(_pmath_object_multimatch));
    }

    if(len == 2 && str[0] == '.' && str[1] == '.'){
      pmath_unref(box);
      return HOLDCOMPLETE(
        pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_RANGE), 2,
          pmath_ref(PMATH_SYMBOL_AUTOMATIC),
          pmath_ref(PMATH_SYMBOL_AUTOMATIC)));
    }

    if(len == 3 && str[0] == '~' && str[1] == '~' && str[2] == '~'){
      pmath_unref(box);
      return HOLDCOMPLETE(pmath_ref(_pmath_object_zeromultimatch));
    }

    if(len == 3 && str[0] == '/' && str[1] == '\\' && str[2] == '/'){
      pmath_unref(box);
      return HOLDCOMPLETE(PMATH_NULL);
    }

    pmath_message(PMATH_NULL, "inv", 1, box);
    return pmath_ref(PMATH_SYMBOL_FAILED);
  }

  if(pmath_is_expr(box)){
    uint16_t firstchar, secondchar;
    size_t i;

    expr = box;
    exprlen = pmath_expr_length(expr);
    box = pmath_expr_get_item(expr, 0);
    pmath_unref(box);

    if(!pmath_is_null(box) && !pmath_same(box, PMATH_SYMBOL_LIST)){
      if(exprlen == 2 && pmath_same(box, PMATH_SYMBOL_FRACTIONBOX)){
        pmath_t num = pmath_expr_get_item(expr, 1);
        box = pmath_expr_get_item(expr, 2);

        if(parse(&num) && parse(&box)){
          pmath_unref(expr);

          if(pmath_is_integer(num) && pmath_is_integer(box))
            return HOLDCOMPLETE(pmath_rational_new(num, box));

          if(pmath_equals(num, PMATH_FROM_INT32(1))){
            pmath_unref(num);

            return HOLDCOMPLETE(pmath_expr_new_extended(
              pmath_ref(PMATH_SYMBOL_POWER), 2,
              box,
              PMATH_FROM_INT32(-1)));
          }

          return HOLDCOMPLETE(
            pmath_expr_new_extended(
              pmath_ref(PMATH_SYMBOL_TIMES), 2,
              num,
              pmath_expr_new_extended(
                pmath_ref(PMATH_SYMBOL_POWER), 2,
                box,
                PMATH_FROM_INT32(-1))));
        }

        pmath_unref(num);
        pmath_unref(box);
        goto FAILED;
      }

      if(exprlen == 1 && pmath_same(box, PMATH_SYMBOL_FRAMEBOX)){
        box = pmath_expr_get_item(expr, 1);

        if(parse(&box)){
          pmath_unref(expr);
          return HOLDCOMPLETE(
            pmath_expr_new_extended(
              pmath_ref(PMATH_SYMBOL_FRAMED), 1,
              box));
        }

        goto FAILED;
      }

      if(exprlen >= 1 && pmath_same(box, PMATH_SYMBOL_GRIDBOX)){
        box = parse_gridbox(expr, TRUE);

        if(!pmath_is_null(box)){
          pmath_unref(expr);
          return box;
        }

        goto FAILED;
      }

      if(pmath_same(box, PMATH_SYMBOL_HOLDCOMPLETE)){
        return expr;
      }

      if(exprlen >= 2 && pmath_same(box, PMATH_SYMBOL_INTERPRETATIONBOX)){
        pmath_expr_t options = pmath_options_extract(expr, 2);
        pmath_unref(options);

        if(!pmath_is_null(options)){
          box = pmath_expr_get_item(expr, 2);
          pmath_unref(expr);
          return HOLDCOMPLETE(box);
        }

        goto FAILED;
      }

      if(exprlen == 2 && pmath_same(box, PMATH_SYMBOL_OVERSCRIPTBOX)){
        pmath_t base = pmath_expr_get_item(expr, 1);
        box = pmath_expr_get_item(expr, 2);

        if(parse(&base) && parse(&box)){
          expr = pmath_expr_set_item(expr, 1, base);
          expr = pmath_expr_set_item(expr, 2, box);
          expr = pmath_expr_set_item(expr, 0, pmath_ref(PMATH_SYMBOL_OVERSCRIPT));

          return HOLDCOMPLETE(expr);
        }

        pmath_unref(base);
        pmath_unref(box);
        goto FAILED;
      }

      if(exprlen == 2 && pmath_same(box, PMATH_SYMBOL_RADICALBOX)){
        pmath_t base = pmath_expr_get_item(expr, 1);
        box = pmath_expr_get_item(expr, 2);

        if(parse(&base) && parse(&box)){
          pmath_unref(expr);

          return HOLDCOMPLETE(
            pmath_expr_new_extended(
              pmath_ref(PMATH_SYMBOL_POWER), 2,
              base,
              pmath_expr_new_extended(
                pmath_ref(PMATH_SYMBOL_POWER), 2,
                box,
                PMATH_FROM_INT32(-1))));
        }

        pmath_unref(base);
        pmath_unref(box);
        goto FAILED;
      }

      if(exprlen >= 1 && pmath_same(box, PMATH_SYMBOL_ROTATIONBOX)){
        pmath_t options = pmath_options_extract(expr, 1);

        if(!pmath_is_null(options)){
          pmath_t angle = pmath_option_value(
            PMATH_SYMBOL_ROTATIONBOX,
            PMATH_SYMBOL_BOXROTATION,
            options);

          pmath_unref(options);
          box = pmath_expr_get_item(expr, 1);

          if(parse(&box)){
            pmath_unref(expr);
            return HOLDCOMPLETE(
              pmath_expr_new_extended(
                pmath_ref(PMATH_SYMBOL_ROTATE), 2,
                box,
                angle));
          }
        }

        goto FAILED;
      }

      if(exprlen == 1 && pmath_same(box, PMATH_SYMBOL_SQRTBOX)){
        box = pmath_expr_get_item(expr, 1);

        if(parse(&box)){
          pmath_unref(expr);
          return HOLDCOMPLETE(
            pmath_expr_new_extended(
              pmath_ref(PMATH_SYMBOL_POWER), 2,
              box,
              pmath_ref(_pmath_one_half)));
        }

        goto FAILED;
      }

      if(exprlen >= 1 && pmath_same(box, PMATH_SYMBOL_STYLEBOX)){
        box = pmath_expr_get_item(expr, 1);

        if(parse(&box)){
          expr = pmath_expr_set_item(expr, 1, box);
          expr = pmath_expr_set_item(expr, 0, pmath_ref(PMATH_SYMBOL_STYLE));

          return HOLDCOMPLETE(expr);
        }

        goto FAILED;
      }

      if(exprlen == 2 && pmath_same(box, PMATH_SYMBOL_TAGBOX)){
        pmath_t view = pmath_expr_get_item(expr, 1);
        pmath_t tag  = pmath_expr_get_item(expr, 2);

        if(pmath_is_string(tag)){

          if(pmath_string_equals_latin1(tag, "Column")
          && pmath_is_expr_of(view, PMATH_SYMBOL_GRIDBOX)){
            pmath_t held;
            pmath_t grid;
            pmath_t matrix;
            pmath_t row;

            held = parse_gridbox(view, FALSE);
            grid = pmath_expr_get_item(held, 1);
            matrix = pmath_expr_get_item(grid, 1);
            row = pmath_expr_get_item(matrix, 1);

            if(pmath_expr_length(row) == 1){
              size_t i;

              pmath_unref(held);
              pmath_unref(grid);
              pmath_unref(row);
              pmath_unref(view);
              pmath_unref(tag);
              pmath_unref(expr);

              for(i = pmath_expr_length(matrix);i > 0;--i){
                row = pmath_expr_get_item(matrix, i);

                matrix = pmath_expr_set_item(matrix, i,
                  pmath_expr_get_item(row, 1));

                pmath_unref(row);
              }

              return HOLDCOMPLETE(
                pmath_expr_new_extended(
                  pmath_ref(PMATH_SYMBOL_COLUMN), 1,
                  matrix));
            }

            pmath_unref(grid);
            pmath_unref(matrix);
            pmath_unref(row);

            if(!pmath_is_null(held)){
              pmath_unref(expr);
              return held;
            }

            goto FAILED;
          }

          if(pmath_string_equals_latin1(tag, "Placeholder")){
            if(pmath_is_expr_of(view, PMATH_SYMBOL_FRAMEBOX)){
              box = pmath_expr_get_item(view, 1);
              pmath_unref(view);
              view = box;
            }

            if(parse(&view)){
              pmath_unref(expr);
              pmath_unref(tag);
              return HOLDCOMPLETE(
                pmath_expr_new_extended(
                  pmath_ref(PMATH_SYMBOL_PLACEHOLDER), 1,
                  view));
            }

            pmath_unref(view);
            pmath_unref(tag);
            goto FAILED;
          }

          if(pmath_string_equals_latin1(tag, "Grid")
          && pmath_is_expr_of(view, PMATH_SYMBOL_GRIDBOX)){
            box = parse_gridbox(view, FALSE);

            pmath_unref(view);
            pmath_unref(tag);

            if(!pmath_is_null(box)){
              pmath_unref(expr);
              return box;
            }

            goto FAILED;
          }
        }

        if(pmath_same(tag, PMATH_SYMBOL_COLUMN)
        && pmath_is_expr_of(view, PMATH_SYMBOL_GRIDBOX)){
          pmath_t held;
          pmath_t grid;
          pmath_t matrix;
          pmath_t row;

          held   = parse_gridbox(view, FALSE);
          grid   = pmath_expr_get_item(held, 1);
          matrix = pmath_expr_get_item(grid, 1);
          row    = pmath_expr_get_item(matrix, 1);

          if(pmath_expr_length(row) == 1){
            size_t i;

            pmath_unref(expr);
            pmath_unref(tag);
            pmath_unref(view);
            pmath_unref(held);
            pmath_unref(grid);
            pmath_unref(row);

            for(i = pmath_expr_length(matrix);i > 0;--i){
              row = pmath_expr_get_item(matrix, i);

              matrix = pmath_expr_set_item(matrix, i,
                pmath_expr_get_item(row, 1));

              pmath_unref(row);
            }

            return HOLDCOMPLETE(matrix);
          }

          pmath_unref(grid);
          pmath_unref(matrix);
          pmath_unref(row);
          pmath_unref(view);
          pmath_unref(tag);

          if(!pmath_is_null(held)){
            pmath_unref(expr);
            return held;
          }

          goto FAILED;
        }

        if(parse(&view)){
          pmath_unref(expr);
          return HOLDCOMPLETE(
            pmath_expr_new_extended(
              tag, 1,
              view));
        }

        pmath_unref(view);
        pmath_unref(tag);
        goto FAILED;
      }

      if(exprlen == 2 && pmath_same(box, PMATH_SYMBOL_UNDERSCRIPTBOX)){
        pmath_t base = pmath_expr_get_item(expr, 1);
        box = pmath_expr_get_item(expr, 2);

        if(parse(&base) && parse(&box)){
          expr = pmath_expr_set_item(expr, 1, base);
          expr = pmath_expr_set_item(expr, 2, box);
          expr = pmath_expr_set_item(expr, 0, pmath_ref(PMATH_SYMBOL_UNDERSCRIPT));

          return HOLDCOMPLETE(expr);
        }

        pmath_unref(base);
        pmath_unref(box);
        goto FAILED;
      }

      if(exprlen == 3 && pmath_same(box, PMATH_SYMBOL_UNDEROVERSCRIPTBOX)){
        pmath_t base = pmath_expr_get_item(expr, 1);
        pmath_t under = pmath_expr_get_item(expr, 2);
        box = pmath_expr_get_item(expr, 3);

        if(parse(&base) && parse(&under) && parse(&box)){
          expr = pmath_expr_set_item(expr, 1, base);
          expr = pmath_expr_set_item(expr, 2, under);
          expr = pmath_expr_set_item(expr, 3, box);
          expr = pmath_expr_set_item(expr, 0, pmath_ref(PMATH_SYMBOL_UNDEROVERSCRIPT));

          return HOLDCOMPLETE(expr);
        }

        pmath_unref(base);
        pmath_unref(box);
        goto FAILED;
      }

      goto FAILED;
    }

    box = PMATH_NULL;

    // box is invalid, expr is valid

    if(exprlen == 0){
      pmath_unref(expr);
      return HOLDCOMPLETE(PMATH_NULL);
      //return pmath_expr_set_item(expr, 0, pmath_ref(PMATH_SYMBOL_HOLDCOMPLETE));
    }

    firstchar = unichar_at(expr, 1);
    secondchar = unichar_at(expr, 2);

    // ()  and  (x) ...
    if(firstchar == '(' && unichar_at(expr, exprlen) == ')'){
      if(exprlen == 2){
        pmath_unref(expr);
        return pmath_expr_new(
          pmath_ref(PMATH_SYMBOL_HOLDCOMPLETE), 0);
      }

      if(exprlen > 3)
        goto FAILED;

      box = pmath_expr_get_item(expr, 2);
      pmath_unref(expr);
      return pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_MAKEEXPRESSION), 1, box);
    }

    // comma sepearted list ...
    if(firstchar == ',' || secondchar == ','){
      pmath_t prev = PMATH_NULL;
      pmath_bool_t last_was_comma = unichar_at(expr, 1) == ',';
      if(!last_was_comma){
        prev = parse_at(expr, 1);
        
        if(is_parse_error(prev)){
          pmath_unref(expr);
          return pmath_ref(PMATH_SYMBOL_FAILED);
        }
        i = 2;
      }
      else
        i = 1;

      pmath_gather_begin(PMATH_NULL);

      while(i <= exprlen){
        if(unichar_at(expr, i) == ','){
          last_was_comma = TRUE;
          pmath_emit(prev, PMATH_NULL);
          prev = PMATH_NULL;
        }
        else if(!last_was_comma){
          last_was_comma = FALSE;
          pmath_message(PMATH_NULL, "inv", 1, expr);
          pmath_unref(pmath_gather_end());
          pmath_unref(prev);
          return pmath_ref(PMATH_SYMBOL_FAILED);
        }
        else{
          last_was_comma = FALSE;
          prev = parse_at(expr, i);
          if(is_parse_error(prev)){
            pmath_unref(expr);
            pmath_unref(pmath_gather_end());
            return pmath_ref(PMATH_SYMBOL_FAILED);
          }
        }
        ++i;
      }
      pmath_emit(prev, PMATH_NULL);

      pmath_unref(expr);
      return pmath_expr_set_item(
        pmath_gather_end(), 0,
        pmath_ref(PMATH_SYMBOL_HOLDCOMPLETE));
    }

    // evaluation sequence ...
    if(firstchar == ';' || secondchar == ';' || firstchar == '\n' || secondchar == '\n'){
      pmath_t prev = PMATH_NULL;
      pmath_bool_t last_was_semicolon = TRUE;

      pmath_gather_begin(PMATH_NULL);

      i = 1;
      while(i <= exprlen){
        uint16_t ch = unichar_at(expr, i);
        if(ch == ';' || ch == '\n'){
          last_was_semicolon = TRUE;
          pmath_emit(prev, PMATH_NULL);
          prev = PMATH_NULL;
        }
        else if(!last_was_semicolon){
          last_was_semicolon = FALSE;
          pmath_message(PMATH_NULL, "inv", 1, expr);
          pmath_unref(pmath_gather_end());
          pmath_unref(prev);
          return pmath_ref(PMATH_SYMBOL_FAILED);
        }
        else{
          last_was_semicolon = FALSE;
          prev = parse_at(expr, i);
          if(is_parse_error(prev)){
            pmath_unref(expr);
            pmath_unref(pmath_gather_end());
            return pmath_ref(PMATH_SYMBOL_FAILED);
          }
        }
        ++i;
      }
      pmath_emit(prev, PMATH_NULL);

      pmath_unref(expr);
      return HOLDCOMPLETE(
        pmath_expr_set_item(
          pmath_gather_end(), 0,
          pmath_ref(PMATH_SYMBOL_EVALUATIONSEQUENCE)));
    }

    if(exprlen == 1)
      return pmath_expr_set_item(expr, 0, pmath_ref(PMATH_SYMBOL_MAKEEXPRESSION));

    // {}  and  {x}
    if(firstchar == '{' && unichar_at(expr, exprlen) == '}'){
      pmath_t args;
      if(exprlen == 2){
        pmath_unref(expr);
        return HOLDCOMPLETE(pmath_ref(_pmath_object_emptylist));
      }

      if(exprlen != 3)
        goto FAILED;

      args = pmath_evaluate(
        pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_MAKEEXPRESSION), 1,
          pmath_expr_get_item(expr, 2)));

      pmath_unref(expr);
      if(pmath_is_expr(args)){
        return HOLDCOMPLETE(
          pmath_expr_set_item(args, 0, pmath_ref(PMATH_SYMBOL_LIST)));
      }

      pmath_unref(args);
      return pmath_ref(PMATH_SYMBOL_FAILED);
    }

    // ?x  and  ?x:v
    if(firstchar == '?'){
      if(exprlen == 2){
        box = parse_at(expr, 2);
        
        if(!is_parse_error(box)){
          pmath_unref(expr);
          return HOLDCOMPLETE(
            pmath_expr_new_extended(
              pmath_ref(PMATH_SYMBOL_OPTIONAL), 1,
              box));
        }
      }
      else if(exprlen == 4 && unichar_at(expr, 3) == ':'){
        box = parse_at(expr, 2);
        
        if(!is_parse_error(box)){
          pmath_t value = parse_at(expr, 4);
          
          if(!is_parse_error(value)){
            pmath_unref(expr);
            return HOLDCOMPLETE(
              pmath_expr_new_extended(
                pmath_ref(PMATH_SYMBOL_OPTIONAL), 2,
                box,
                value));
          }
          
          pmath_unref(box);
        }
      }

      pmath_unref(expr);
      return pmath_ref(PMATH_SYMBOL_FAILED);
    }

    if(exprlen == 2){ // x& x! x++ x-- x.. p** p*** +x -x !x #x ++x --x ..x ??x <<x ~x ~~x ~~~x
      // x &
      if(secondchar == '&'){
        box = parse_at(expr, 1);
        
        if(!is_parse_error(box)){
          pmath_unref(expr);
          return HOLDCOMPLETE(
            pmath_expr_new_extended(
              pmath_ref(PMATH_SYMBOL_FUNCTION), 1,
              box));
        }
        
        pmath_unref(expr);
        return pmath_ref(PMATH_SYMBOL_FAILED);
      }

      // x!
      if(secondchar == '!'){
        box = parse_at(expr, 1);
        
        if(!is_parse_error(box)){
          pmath_unref(expr);
          return HOLDCOMPLETE(
            pmath_expr_new_extended(
              pmath_ref(PMATH_SYMBOL_FACTORIAL), 1,
              box));
        }
        
        pmath_unref(expr);
        return pmath_ref(PMATH_SYMBOL_FAILED);
      }

      // x!!
      if(is_string_at(expr, 2, "!!")){
        box = parse_at(expr, 1);
        
        if(!is_parse_error(box)){
          pmath_unref(expr);
          return HOLDCOMPLETE(
            pmath_expr_new_extended(
              pmath_ref(PMATH_SYMBOL_FACTORIAL2), 1,
              box));
        }
        
        pmath_unref(expr);
        return pmath_ref(PMATH_SYMBOL_FAILED);
      }

      // x++
      if(is_string_at(expr, 2, "++")){
        box = parse_at(expr, 1);
        
        if(!is_parse_error(box)){
          pmath_unref(expr);
          return HOLDCOMPLETE(
            pmath_expr_new_extended(
              pmath_ref(PMATH_SYMBOL_POSTINCREMENT), 1,
              box));
        }
        
        pmath_unref(expr);
        return pmath_ref(PMATH_SYMBOL_FAILED);
      }

      // x--
      if(is_string_at(expr, 2, "--")){
        box = parse_at(expr, 1);
        
        if(!is_parse_error(box)){
          pmath_unref(expr);
          return HOLDCOMPLETE(
            pmath_expr_new_extended(
              pmath_ref(PMATH_SYMBOL_POSTDECREMENT), 1,
              box));
        }
        
        pmath_unref(expr);
        return pmath_ref(PMATH_SYMBOL_FAILED);
      }

      // p**
      if(is_string_at(expr, 2, "**")){
        box = parse_at(expr, 1);
        
        if(!is_parse_error(box)){
          pmath_unref(expr);
          return HOLDCOMPLETE(
            pmath_expr_new_extended(
              pmath_ref(PMATH_SYMBOL_REPEATED), 2,
              box,
              pmath_ref(_pmath_object_range_from_one)));
        }
        
        pmath_unref(expr);
        return pmath_ref(PMATH_SYMBOL_FAILED);
      }

      // p***
      if(is_string_at(expr, 2, "***")){
        box = parse_at(expr, 1);
        
        if(!is_parse_error(box)){
          pmath_unref(expr);
          return HOLDCOMPLETE(
            pmath_expr_new_extended(
              pmath_ref(PMATH_SYMBOL_REPEATED), 2,
              box,
              pmath_ref(_pmath_object_range_from_zero)));
        }
        
        pmath_unref(expr);
        return pmath_ref(PMATH_SYMBOL_FAILED);
      }

      // +x
      if(firstchar == '+'){
        box = pmath_expr_get_item(expr, 2);
        pmath_unref(expr);
        return pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_MAKEEXPRESSION), 1, box);
      }

      // -x
      if(firstchar == '-'){
        box = parse_at(expr, 2);
        
        if(!is_parse_error(box)){
          pmath_unref(expr);
          if(pmath_is_number(box))
            return HOLDCOMPLETE(pmath_number_neg(box));

          return HOLDCOMPLETE(
            pmath_expr_new_extended(
              pmath_ref(PMATH_SYMBOL_TIMES), 2,
              PMATH_FROM_INT32(-1),
              box));
        }
        
        pmath_unref(expr);
        return pmath_ref(PMATH_SYMBOL_FAILED);
      }

      if(firstchar == PMATH_CHAR_PIECEWISE){
        box = parse_at(expr, 2);
        
        if(!is_parse_error(box)){
          pmath_unref(expr);
          return HOLDCOMPLETE(
            pmath_expr_new_extended(
              pmath_ref(PMATH_SYMBOL_PIECEWISE), 1,
              box));
        }
        
        pmath_unref(expr);
        return pmath_ref(PMATH_SYMBOL_FAILED);
      }

      // !x
      if(firstchar == '!' || firstchar == 0x00AC){
        box = parse_at(expr, 2);
        
        if(!is_parse_error(box)){
          pmath_unref(expr);
          return HOLDCOMPLETE(
            pmath_expr_new_extended(
              pmath_ref(PMATH_SYMBOL_NOT), 1,
              box));
        }
        
        pmath_unref(expr);
        return pmath_ref(PMATH_SYMBOL_FAILED);
      }

      // #x
      if(firstchar == '#'){
        box = parse_at(expr, 2);
        
        if(is_parse_error(box)){
          pmath_unref(expr);
          return pmath_ref(PMATH_SYMBOL_FAILED);
        }

        if(pmath_is_integer(box)){
          pmath_unref(expr);
          return HOLDCOMPLETE(
            pmath_expr_new_extended(
              pmath_ref(PMATH_SYMBOL_PUREARGUMENT), 1,
              box));
        }

        pmath_unref(box); box = PMATH_NULL;
      }

      // ##x
      if(is_string_at(expr, 1, "##")){
        box = parse_at(expr, 2);
        
        if(is_parse_error(box)){
          pmath_unref(expr);
          return pmath_ref(PMATH_SYMBOL_FAILED);
        }

        if(pmath_is_integer(box)){
          pmath_unref(expr);
          return HOLDCOMPLETE(
            pmath_expr_new_extended(
              pmath_ref(PMATH_SYMBOL_PUREARGUMENT), 1,
              pmath_expr_new_extended(
                pmath_ref(PMATH_SYMBOL_RANGE), 2,
                box,
                pmath_ref(PMATH_SYMBOL_AUTOMATIC))));
        }

        pmath_unref(box); box = PMATH_NULL;
      }

      // ++x
      if(is_string_at(expr, 1, "++")){
        box = parse_at(expr, 2);
        
        if(!is_parse_error(box)){
          pmath_unref(expr);
          return HOLDCOMPLETE(
            pmath_expr_new_extended(
              pmath_ref(PMATH_SYMBOL_INCREMENT), 1,
              box));
        }
        
        pmath_unref(expr);
        return pmath_ref(PMATH_SYMBOL_FAILED);
      }

      // --x
      if(is_string_at(expr, 1, "--")){
        box = parse_at(expr, 2);
        
        if(!is_parse_error(box)){
          pmath_unref(expr);
          return HOLDCOMPLETE(
            pmath_expr_new_extended(
              pmath_ref(PMATH_SYMBOL_DECREMENT), 1,
              box));
        }
        
        pmath_unref(expr);
        return pmath_ref(PMATH_SYMBOL_FAILED);
      }

      // ??x
      if(is_string_at(expr, 1, "??")){
        box = pmath_expr_get_item(expr, 2);

        pmath_unref(expr);
        if(!pmath_is_string(box)
        || pmath_string_length(box) == 0
        || pmath_string_buffer(&box)[0] == '"'){
          if(!parse(&box))
            return pmath_ref(PMATH_SYMBOL_FAILED);
        }

        return HOLDCOMPLETE(
          pmath_expr_new_extended(
            pmath_ref(PMATH_SYMBOL_SHOWDEFINITION), 1,
            box));
      }

      // <<x
      if(is_string_at(expr, 1, "<<")){
        box = pmath_expr_get_item(expr, 2);

        pmath_unref(expr);
        if(!pmath_is_string(box)
        || pmath_string_length(box) == 0
        || pmath_string_buffer(&box)[0] == '"'){
          if(!parse(&box))
            return pmath_ref(PMATH_SYMBOL_FAILED);
        }

        return HOLDCOMPLETE(
          pmath_expr_new_extended(
            pmath_ref(PMATH_SYMBOL_GET), 1,
            box));
      }

      // ~x
      if(firstchar == '~'){
        box = parse_at(expr, 2);
        
        if(!is_parse_error(box)){
          pmath_unref(expr);
          return HOLDCOMPLETE(
            pmath_expr_new_extended(
              pmath_ref(PMATH_SYMBOL_PATTERN), 2,
              box,
              pmath_ref(_pmath_object_singlematch)));
        }
        
        pmath_unref(expr);
        return pmath_ref(PMATH_SYMBOL_FAILED);
      }

      // ~~x
      if(is_string_at(expr, 1, "~~")){
        box = parse_at(expr, 2);
        
        if(!is_parse_error(box)){
          pmath_unref(expr);
          return HOLDCOMPLETE(
            pmath_expr_new_extended(
              pmath_ref(PMATH_SYMBOL_PATTERN), 2,
              box,
              pmath_ref(_pmath_object_multimatch)));
        }
        
        pmath_unref(expr);
        return pmath_ref(PMATH_SYMBOL_FAILED);
      }

      // ~~~x
      if(is_string_at(expr, 1, "~~~")){
        box = parse_at(expr, 2);
        
        if(!is_parse_error(box)){
          pmath_unref(expr);
          return HOLDCOMPLETE(
            pmath_expr_new_extended(
              pmath_ref(PMATH_SYMBOL_PATTERN), 2,
              box,
              pmath_ref(_pmath_object_zeromultimatch)));
        }
        
        pmath_unref(expr);
        return pmath_ref(PMATH_SYMBOL_FAILED);
      }

      // x^y   Subscript(x, y, ...)   Subscript(x,y,...)^z
      if(secondchar == 0){
        box = pmath_expr_get_item(expr, 2);

        if(pmath_is_expr_of_len(box, PMATH_SYMBOL_SUPERSCRIPTBOX, 1)){
          pmath_t base = pmath_expr_get_item(expr, 1);
          pmath_t exp  = pmath_expr_get_item(box,  1);

          pmath_unref(box);
          pmath_unref(expr);

          if(parse(&base) && parse(&exp)){
            return HOLDCOMPLETE(
              pmath_expr_new_extended(
                pmath_ref(PMATH_SYMBOL_POWER), 2,
                base,
                exp));
          }

          pmath_unref(base);
          pmath_unref(exp);
          return pmath_ref(PMATH_SYMBOL_FAILED);
        }

        if(pmath_is_expr_of_len(box, PMATH_SYMBOL_SUBSCRIPTBOX, 1)){
          pmath_t base = pmath_expr_get_item(expr, 1);
          pmath_unref(expr);

          if(parse(&base)){
            pmath_t idx = pmath_expr_get_item(box, 1);
            pmath_unref(box);

            idx = pmath_evaluate(
              pmath_expr_new_extended(
                pmath_ref(PMATH_SYMBOL_MAKEEXPRESSION), 1,
                idx));

            if(pmath_is_expr(idx)){
              pmath_t head = pmath_expr_get_item(idx, 0);
              pmath_unref(head);

              if(pmath_same(head, PMATH_SYMBOL_HOLDCOMPLETE)){
                exprlen = pmath_expr_length(idx) + 1;

                expr = pmath_expr_new(
                  pmath_ref(PMATH_SYMBOL_SUBSCRIPT),
                  exprlen);

                expr = pmath_expr_set_item(expr, 1, base);

                for(;exprlen > 1;--exprlen){
                  expr = pmath_expr_set_item(
                    expr, exprlen,
                    pmath_expr_get_item(idx, exprlen - 1));
                }

                pmath_unref(idx);
                return HOLDCOMPLETE(expr);
              }
            }

            pmath_unref(idx);
          }
          else
            pmath_unref(box);

          pmath_unref(base);
          return pmath_ref(PMATH_SYMBOL_FAILED);
        }

        if(pmath_is_expr_of_len(box, PMATH_SYMBOL_SUBSUPERSCRIPTBOX, 2)){
          pmath_t idx  = pmath_expr_get_item(box,  1);
          pmath_t exp  = pmath_expr_get_item(box,  2);

          pmath_unref(box);
          box = pmath_expr_new_extended(
            pmath_ref(PMATH_SYMBOL_SUBSCRIPTBOX), 1,
            idx);

          expr = pmath_expr_set_item(expr, 2, box);

          expr = pmath_expr_new_extended(
            pmath_ref(PMATH_SYMBOL_LIST), 2,
            expr,
            pmath_expr_new_extended(
              pmath_ref(PMATH_SYMBOL_SUPERSCRIPTBOX), 1,
              exp));

          return pmath_expr_new_extended(
            pmath_ref(PMATH_SYMBOL_MAKEEXPRESSION), 1,
            expr);
        }

        pmath_unref(box);
        box = PMATH_NULL;
      }
    }

    if(exprlen == 3){ // a.f  x/y  f@x  f@@list  s::tag  f()  ~:t  ~~:t  ~~~:t  x:p  a//f  p/?c  l->r  l:=r  l+=r  l-=r  l:>r  l::=r  l..r
      // a.f
      if(secondchar == '.'){
        pmath_t arg = parse_at(expr, 1);
        
        if(!is_parse_error(arg)){
          pmath_t f = parse_at(expr, 3);
          
          if(!is_parse_error(f)){
            pmath_unref(expr);
            return HOLDCOMPLETE(pmath_expr_new_extended(f, 1, arg));
          }
          
          pmath_unref(arg);
        }
        
        pmath_unref(expr);
        return pmath_ref(PMATH_SYMBOL_FAILED);
      }

      // f@x
      if(secondchar == '@' || secondchar == PMATH_CHAR_INVISIBLECALL){
        pmath_t f = parse_at(expr, 1);
        
        if(!is_parse_error(f)){
          pmath_t x = parse_at(expr, 3);
          
          if(!is_parse_error(x)){
            pmath_unref(expr);
            return HOLDCOMPLETE(
              pmath_expr_new_extended(f, 1, x));
          }
          
          pmath_unref(f);
        }
        
        pmath_unref(expr);
        return pmath_ref(PMATH_SYMBOL_FAILED);
      }

      // f()
      if(secondchar == '(' && unichar_at(expr, 3) == ')'){
        box = parse_at(expr, 1);
        
        if(!is_parse_error(box)){
          pmath_unref(expr);
          return HOLDCOMPLETE(pmath_expr_new(box, 0));
        }
        
        return pmath_ref(PMATH_SYMBOL_FAILED);
      }

      // ~:t  ~~:t  ~~~:t  x:p
      if(secondchar == ':'){
        pmath_t x;
        box = parse_at(expr, 3);
        
        if(is_parse_error(box)){
          pmath_unref(expr);
          return pmath_ref(PMATH_SYMBOL_FAILED);
        }

        if(firstchar == '~'){
          pmath_unref(expr);
          return HOLDCOMPLETE(
            pmath_expr_new_extended(
              pmath_ref(PMATH_SYMBOL_SINGLEMATCH), 1,
              box));
        }

        if(is_string_at(expr, 2, "~~")){
          pmath_unref(expr);
          return HOLDCOMPLETE(
            pmath_expr_new_extended(
              pmath_ref(PMATH_SYMBOL_REPEATED), 2,
              pmath_expr_new_extended(
                pmath_ref(PMATH_SYMBOL_SINGLEMATCH), 1,
                box),
              pmath_ref(_pmath_object_range_from_one)));
        }

        if(is_string_at(expr, 2, "~~~")){
          pmath_unref(expr);
          return HOLDCOMPLETE(
            pmath_expr_new_extended(
              pmath_ref(PMATH_SYMBOL_REPEATED), 2,
              pmath_expr_new_extended(
                pmath_ref(PMATH_SYMBOL_SINGLEMATCH), 1,
                box),
              pmath_ref(_pmath_object_range_from_zero)));
        }
        
        x = parse_at(expr, 1);
        
        if(!is_parse_error(x)){
          pmath_unref(expr);
          return HOLDCOMPLETE(pmath_expr_new_extended(
            pmath_ref(PMATH_SYMBOL_PATTERN), 2,
            x,
            box));
        }
        
        pmath_unref(expr);
        pmath_unref(box);
        return pmath_ref(PMATH_SYMBOL_FAILED);
      }

      // args :-> body
      if(secondchar == 0x21A6){
        pmath_t args = parse_at(expr, 1);
        
        if(!is_parse_error(args)){
          pmath_t body = parse_at(expr, 3);
          
          if(!is_parse_error(body)){
            pmath_unref(expr);
            if(pmath_is_expr_of(args, PMATH_SYMBOL_SEQUENCE)){
              args = pmath_expr_set_item(
                args, 0,
                pmath_ref(PMATH_SYMBOL_LIST));
            }
            else{
              args = pmath_expr_new_extended(
                pmath_ref(PMATH_SYMBOL_LIST), 1,
                args);
            }
            
            return HOLDCOMPLETE(
              pmath_expr_new_extended(
                pmath_ref(PMATH_SYMBOL_FUNCTION), 2,
                args,
                body));
          }
          
          pmath_unref(args);
        }
        
        pmath_unref(expr);
        return pmath_ref(PMATH_SYMBOL_FAILED);
      }

      // f@@list
      if(is_string_at(expr, 2, "@@")){
        pmath_t f = parse_at(expr, 1);
        
        if(!is_parse_error(f)){
          pmath_t list = parse_at(expr, 3);
          
          if(!is_parse_error(list)){
            pmath_unref(expr);
            return HOLDCOMPLETE(
              pmath_expr_new_extended(
                pmath_ref(PMATH_SYMBOL_APPLY), 2,
                list,
                f));
          }
          
          pmath_unref(f);
        }
        
        pmath_unref(expr);
        return pmath_ref(PMATH_SYMBOL_FAILED);
      }

      // s::tag
      if(is_string_at(expr, 2, "::")){
        pmath_string_t tag = box_as_string(pmath_expr_get_item(expr, 3));
        if(pmath_is_null(tag))
          goto FAILED;
        
        box = parse_at(expr, 1);
        if(!is_parse_error(box)){
          pmath_unref(expr);
          return HOLDCOMPLETE(
            pmath_expr_new_extended(
              pmath_ref(PMATH_SYMBOL_MESSAGENAME), 2,
              box,
              tag));
        }
        
        pmath_unref(tag);
        pmath_unref(expr);
        return pmath_ref(PMATH_SYMBOL_FAILED);
      }

      // arg // f
      if(is_string_at(expr, 2, "//")){
        pmath_t arg = parse_at(expr, 1);
        
        if(!is_parse_error(arg)){
          pmath_t f = parse_at(expr, 3);
          
          if(!is_parse_error(f)){
            pmath_unref(expr);
            return HOLDCOMPLETE(
              pmath_expr_new_extended(f, 1, arg));
          }
          
          pmath_unref(arg);
        }
        
        pmath_unref(expr);
        return pmath_ref(PMATH_SYMBOL_FAILED);
      }

      box = PMATH_NULL;
      if(     secondchar == PMATH_CHAR_ASSIGN)        box = PMATH_SYMBOL_ASSIGN;
      if(     secondchar == PMATH_CHAR_ASSIGNDELAYED) box = PMATH_SYMBOL_ASSIGNDELAYED;
      if(     secondchar == PMATH_CHAR_RULE)          box = PMATH_SYMBOL_RULE;
      if(     secondchar == PMATH_CHAR_RULEDELAYED)   box = PMATH_SYMBOL_RULEDELAYED;
      if(     secondchar == '?')            box = PMATH_SYMBOL_TESTPATTERN;
      else if(is_string_at(expr, 2, ":="))  box = PMATH_SYMBOL_ASSIGN;
      else if(is_string_at(expr, 2, "::=")) box = PMATH_SYMBOL_ASSIGNDELAYED;
      else if(is_string_at(expr, 2, "->"))  box = PMATH_SYMBOL_RULE;
      else if(is_string_at(expr, 2, ":>"))  box = PMATH_SYMBOL_RULEDELAYED;
      else if(is_string_at(expr, 2, "+="))  box = PMATH_SYMBOL_INCREMENT;
      else if(is_string_at(expr, 2, "-="))  box = PMATH_SYMBOL_DECREMENT;
      else if(is_string_at(expr, 2, "*="))  box = PMATH_SYMBOL_TIMESBY;
      else if(is_string_at(expr, 2, "/="))  box = PMATH_SYMBOL_DIVIDEBY;
      else if(is_string_at(expr, 2, "/?"))  box = PMATH_SYMBOL_CONDITION;

      // lhs->rhs  lhs:>rhs  lhs:=rhs  lhs::=rhs  lhs+=rhs  lhs-=lhs  lhs//rhs  lhs..rhs
      if(!pmath_is_null(box)){
        pmath_t lhs = parse_at(expr, 1);

        if(!is_parse_error(lhs)){
          pmath_t rhs;
          
          if(pmath_same(box, PMATH_SYMBOL_ASSIGN)
          && unichar_at(expr, 3) == '.'){
            pmath_unref(expr);

            return HOLDCOMPLETE(
              pmath_expr_new_extended(
                pmath_ref(PMATH_SYMBOL_UNASSIGN), 1,
                lhs));
          }
          
          rhs = parse_at(expr, 3);
          if(!is_parse_error(rhs)){
            pmath_unref(expr);
            return HOLDCOMPLETE(
              pmath_expr_new_extended(
                pmath_ref(box), 2,
                lhs,
                rhs));
          }
          
          pmath_unref(lhs);
        }

        pmath_unref(expr);
        return pmath_ref(PMATH_SYMBOL_FAILED);
      }
    }

    if(exprlen == 4 && unichar_at(expr, 3) == ':'){ // ~x:t  ~~x:t  ~~~x:t
      pmath_t x, t;
      
      x = parse_at(expr, 2);
      if(is_parse_error(x)){
        pmath_unref(expr);
        return pmath_ref(PMATH_SYMBOL_FAILED);
      }
      
      t = parse_at(expr, 4);
      if(is_parse_error(t)){
        pmath_unref(x);
        pmath_unref(expr);
        return pmath_ref(PMATH_SYMBOL_FAILED);
      }

      if(firstchar == '~'){
        pmath_unref(expr);

        return HOLDCOMPLETE(
          pmath_expr_new_extended(
            pmath_ref(PMATH_SYMBOL_PATTERN), 2,
            x,
            pmath_expr_new_extended(
              pmath_ref(PMATH_SYMBOL_SINGLEMATCH), 1,
              t)));
      }

      if(is_string_at(expr, 1, "~~")){
        pmath_unref(expr);

        return HOLDCOMPLETE(
          pmath_expr_new_extended(
            pmath_ref(PMATH_SYMBOL_PATTERN), 2,
            x,
            pmath_expr_new_extended(
              pmath_ref(PMATH_SYMBOL_REPEATED), 2,
              pmath_expr_new_extended(
                pmath_ref(PMATH_SYMBOL_SINGLEMATCH), 1,
                t),
              pmath_ref(_pmath_object_range_from_one))));
      }

      if(is_string_at(expr, 1, "~~~")){
        pmath_unref(expr);

        return HOLDCOMPLETE(
          pmath_expr_new_extended(
            pmath_ref(PMATH_SYMBOL_PATTERN), 2,
            x,
            pmath_expr_new_extended(
              pmath_ref(PMATH_SYMBOL_REPEATED), 2,
              pmath_expr_new_extended(
                pmath_ref(PMATH_SYMBOL_SINGLEMATCH), 1,
                t),
              pmath_ref(_pmath_object_range_from_zero))));
      }

      pmath_unref(x);
      pmath_unref(t);
    }

    if(exprlen == 5){ // t/:l:=r   t/:l::=r
      if(is_string_at(expr, 2, "/:")){
        pmath_t tag;
        
        if(     unichar_at(expr, 4) == PMATH_CHAR_ASSIGN)        box = PMATH_SYMBOL_TAGASSIGN;
        else if(unichar_at(expr, 4) == PMATH_CHAR_ASSIGNDELAYED) box = PMATH_SYMBOL_TAGASSIGNDELAYED;
        else if(is_string_at(expr, 4, ":="))                     box = PMATH_SYMBOL_TAGASSIGN;
        else if(is_string_at(expr, 4, "::="))                    box = PMATH_SYMBOL_TAGASSIGNDELAYED;
        else{
          handle_row_error_at(expr, 4);
          pmath_unref(expr);
          return pmath_ref(PMATH_SYMBOL_FAILED);
        }
        
        tag = parse_at(expr, 1);
        if(!is_parse_error(tag)){
          pmath_t lhs = parse_at(expr, 3);
          
          if(!is_parse_error(lhs)){
            pmath_t rhs;
          
            if(pmath_same(box, PMATH_SYMBOL_TAGASSIGN)
            && unichar_at(expr, 5) == '.'){
              pmath_unref(expr);
              
              return HOLDCOMPLETE(
                pmath_expr_new_extended(
                  pmath_ref(PMATH_SYMBOL_TAGUNASSIGN), 2,
                  tag,
                  lhs));
            }
            
            rhs = parse_at(expr, 5);
            if(!is_parse_error(rhs)){
              pmath_unref(expr);
              return HOLDCOMPLETE(
                pmath_expr_new_extended(
                  pmath_ref(box), 3,
                  tag,
                  lhs,
                  rhs));
            }
            
            pmath_unref(lhs);
          }
          
          pmath_unref(tag);
        }
        
        pmath_unref(expr);
        return pmath_ref(PMATH_SYMBOL_FAILED);
      }
    }

    // infix operators (except * /) ...
    if(exprlen & 1){
      int tokprec;

      if(secondchar == '+' || secondchar == '-'){
        pmath_expr_t result;
        pmath_t arg = parse_at(expr, 1);

        if(is_parse_error(arg)){
          pmath_unref(expr);
          return pmath_ref(PMATH_SYMBOL_FAILED);
        }

        result = pmath_expr_set_item(
          pmath_expr_new(
            pmath_ref(PMATH_SYMBOL_PLUS),
            (1 + exprlen) / 2),
          1,
          arg);

        for(i = 1;i <= exprlen / 2;++i){
          uint16_t ch;
          
          arg = parse_at(expr, 2 * i + 1);
          if(is_parse_error(arg)){
            pmath_unref(result);
            pmath_unref(expr);
            return pmath_ref(PMATH_SYMBOL_FAILED);
          }

          ch = unichar_at(expr, 2 * i);
          if(ch == '-'){
            if(pmath_is_number(arg)){
              arg = pmath_number_neg(arg);
            }
            else{
              arg = pmath_expr_new_extended(
                pmath_ref(PMATH_SYMBOL_TIMES), 2,
                PMATH_FROM_INT32(-1),
                arg);
            }
          }
          else if(ch != '+'){
            pmath_unref(arg);
            pmath_unref(result);
            handle_row_error_at(expr, 2 * i);
            pmath_unref(expr);
            return pmath_ref(PMATH_SYMBOL_FAILED);
          }

          result = pmath_expr_set_item(result, i + 1, arg);
        }

        pmath_unref(expr);
        return HOLDCOMPLETE(result);
      }

      // single character infix operators (except + - * / && || and relations) ...
      box = inset_operator(secondchar);
      if(!pmath_is_null(box)){
        pmath_expr_t result;

        for(i = 4;i < exprlen;i+= 2){
          if(!pmath_same(inset_operator(unichar_at(expr, i)), box)){
            handle_row_error_at(expr, i);
            pmath_unref(expr);
            return pmath_ref(PMATH_SYMBOL_FAILED);
          }
        }

        result = pmath_expr_new(pmath_ref(box), (1 + exprlen) / 2);
        for(i = 0;i <= exprlen / 2;++i){
          pmath_t arg = parse_at(expr, 2 * i + 1);
          
          if(is_parse_error(arg)){
            pmath_unref(result);
            pmath_unref(expr);
            return pmath_ref(PMATH_SYMBOL_FAILED);
          }
          
          result = pmath_expr_set_item(result, i + 1, arg);
        }

        pmath_unref(expr);
        return HOLDCOMPLETE(result);
      }

      pmath_token_analyse(&secondchar, 1, &tokprec);
      if(tokprec == PMATH_PREC_DIV){ // x/y/.../z
        pmath_expr_t result;

        for(i = 4;i < exprlen;i+= 2){
          uint16_t ch = unichar_at(expr, i);
          pmath_token_analyse(&ch, 1, &tokprec);

          if(tokprec != PMATH_PREC_DIV){
            handle_row_error_at(expr, i);
            pmath_unref(expr);
            return pmath_ref(PMATH_SYMBOL_FAILED);
          }
        }

        result = pmath_expr_new(pmath_ref(PMATH_SYMBOL_TIMES), (1 + exprlen) / 2);
        for(i = 0;i <= exprlen / 2;++i){
          pmath_t arg = parse_at(expr, 2 * i + 1);
          
          if(is_parse_error(arg)){
            pmath_unref(result);
            pmath_unref(expr);
            return pmath_ref(PMATH_SYMBOL_FAILED);
          }

          if(i > 0){
            arg = pmath_expr_new_extended(
              pmath_ref(PMATH_SYMBOL_POWER), 2,
              arg,
              PMATH_FROM_INT32(-1));
          }

          result = pmath_expr_set_item(result, i + 1, arg);
        }

        pmath_unref(expr);
        return HOLDCOMPLETE(result);
      }

      // a&&b  a||b  a++b...
      if(     secondchar == 0x2227)  box = PMATH_SYMBOL_AND;
      else if(secondchar == 0x2228)  box = PMATH_SYMBOL_OR;
      else{
        pmath_string_t snd = pmath_expr_get_item(expr, 2);
        if(pmath_is_string(snd)){
          if(     string_equals(snd, "&&"))  box = PMATH_SYMBOL_AND;
          else if(string_equals(snd, "||"))  box = PMATH_SYMBOL_OR;
          else if(string_equals(snd, "++"))  box = PMATH_SYMBOL_STRINGEXPRESSION;
        }
        pmath_unref(snd);
      }

      if(pmath_same(box, PMATH_SYMBOL_AND)){
        for(i = 4;i < exprlen;i+= 2){
          pmath_string_t op = pmath_expr_get_item(expr, i);
          
          if((pmath_string_length(op) != 1
           || *pmath_string_buffer(&op) != 0x2227)
          && !string_equals(op, "&&")){
            pmath_unref(op);
            handle_row_error_at(expr, i);
            pmath_unref(expr);
            return pmath_ref(PMATH_SYMBOL_FAILED);
          }
          
          pmath_unref(op);
        }
      }
      else if(pmath_same(box, PMATH_SYMBOL_OR)){
        for(i = 4;i < exprlen;i+= 2){
          pmath_string_t op = pmath_expr_get_item(expr, i);
          
          if((pmath_string_length(op) != 1
           || *pmath_string_buffer(&op) != 0x2228)
          && !string_equals(op, "||")){
            pmath_unref(op);
            handle_row_error_at(expr, i);
            pmath_unref(expr);
            return pmath_ref(PMATH_SYMBOL_FAILED);
          }
          
          pmath_unref(op);
        }
      }

      if(!pmath_is_null(box)){
        pmath_expr_t result = pmath_expr_new(pmath_ref(box), (1 + exprlen) / 2);
        for(i = 0;i <= exprlen / 2;++i){
          pmath_t arg = parse_at(expr, 2 * i + 1);
          
          if(is_parse_error(arg)){
            pmath_unref(result);
            pmath_unref(expr);
            return pmath_ref(PMATH_SYMBOL_FAILED);
          }
          
          result = pmath_expr_set_item(result, i + 1, arg);
        }

        pmath_unref(expr);
        return HOLDCOMPLETE(result);
      }

      // relations ...
      box = relation_at(expr, 2);
      if(!pmath_is_null(box)){
        pmath_expr_t result = pmath_ref(expr);

        for(i = 4;i < exprlen;++i){
          if(!pmath_same(box, relation_at(expr, i))){
            box = parse_at(expr, 1);
            
            if(is_parse_error(box)){
              pmath_unref(expr);
              return pmath_ref(PMATH_SYMBOL_FAILED);
            }

            result = pmath_expr_set_item(result, 1, box);
            for(i = 3;i <= exprlen;i+= 2){
              box = relation_at(expr, i - 1);
              if(pmath_is_null(box)){
                handle_row_error_at(expr, i-1);
                pmath_unref(expr);
                pmath_unref(result);
                return pmath_ref(PMATH_SYMBOL_FAILED);
              }
              
              result = pmath_expr_set_item(result, i - 1, pmath_ref(box));
            
              box = parse_at(expr, i);
              if(is_parse_error(box)){
                pmath_unref(expr);
                pmath_unref(result);
                return pmath_ref(PMATH_SYMBOL_FAILED);
              }
              
              result = pmath_expr_set_item(result, i, box);
            }
            
            pmath_unref(expr);
            return HOLDCOMPLETE(
              pmath_expr_set_item(result, 0,
                pmath_ref(PMATH_SYMBOL_INEQUATION)));
          }
        }
        
        pmath_unref(result);
        result = pmath_expr_new(pmath_ref(box), (1 + exprlen) / 2);
        for(i = 0;i <= exprlen / 2;++i){
          pmath_t arg = parse_at(expr, 2 * i + 1);
          
          if(is_parse_error(arg)){
            pmath_unref(result);
            pmath_unref(expr);
            return pmath_ref(PMATH_SYMBOL_FAILED);
          }
          
          result = pmath_expr_set_item(result, i + 1, arg);
        }

        pmath_unref(expr);
        return HOLDCOMPLETE(result);
      }
    }

    if(exprlen == 4){
      // f(x)
      if(secondchar =='(' && unichar_at(expr, 4) == ')'){
        pmath_t args = pmath_evaluate(
          pmath_expr_new_extended(
            pmath_ref(PMATH_SYMBOL_MAKEEXPRESSION), 1,
            pmath_expr_get_item(expr, 3)));

        if(pmath_is_expr(args)){
          pmath_t f = parse_at(expr, 1);
          
          if(!is_parse_error(f)){
            pmath_unref(expr);
            return HOLDCOMPLETE(
              pmath_expr_set_item(args, 0, f));
          }
          
          pmath_unref(expr);
          return pmath_ref(PMATH_SYMBOL_FAILED);
        }
        
        pmath_unref(args);
        pmath_unref(expr);
        return pmath_ref(PMATH_SYMBOL_FAILED);
      }

      // l[x]
      if(secondchar =='[' && unichar_at(expr, 4) == ']'){
        pmath_t args, list;
        
        list = parse_at(expr, 1);
        if(is_parse_error(list)){
          pmath_unref(expr);
          return pmath_ref(PMATH_SYMBOL_FAILED);
        }

        args = pmath_evaluate(
          pmath_expr_new_extended(
            pmath_ref(PMATH_SYMBOL_MAKEEXPRESSION), 1,
            pmath_expr_get_item(expr, 3)));
        pmath_unref(expr);

        if(pmath_is_expr(args)){
          size_t argslen = pmath_expr_length(args);

          pmath_expr_t result = pmath_expr_set_item(
            pmath_expr_new(
              pmath_ref(PMATH_SYMBOL_PART),
              argslen + 1),
            1, list);

          for(i = argslen + 1;i > 1;--i)
            result = pmath_expr_set_item(
              result, i,
              pmath_expr_get_item(args, i - 1));

          pmath_unref(args);
          return HOLDCOMPLETE(result);
        }

        pmath_unref(list);
        pmath_unref(args);
        return pmath_ref(PMATH_SYMBOL_FAILED);
      }

      // l[[x]]
      if((secondchar == 0x27E6        && unichar_at(expr, 4) == 0x27E7)
      || (is_string_at(expr, 2, "[[") && is_string_at(expr, 4, "]]"))){
        pmath_t args, list;
        
        list = parse_at(expr, 1);
        if(is_parse_error(list)){
          pmath_unref(expr);
          return pmath_ref(PMATH_SYMBOL_FAILED);
        }

        args = pmath_evaluate(
          pmath_expr_new_extended(
            pmath_ref(PMATH_SYMBOL_MAKEEXPRESSION), 1,
            pmath_expr_get_item(expr, 3)));
        pmath_unref(expr);

        if(pmath_is_expr(args)){
          size_t i, argslen;
          argslen = pmath_expr_length(args);

          for(i = 1;i <= argslen;++i){
            list = pmath_expr_new_extended(
              pmath_ref(PMATH_SYMBOL_OPTIONVALUE), 2,
              list,
              pmath_expr_get_item(args, i));
          }

          pmath_unref(args);
          return HOLDCOMPLETE(list);
        }

        pmath_unref(list);
        pmath_unref(args);
        return pmath_ref(PMATH_SYMBOL_FAILED);
      }
    }

    // a.f()
    if(exprlen == 5
    && secondchar == '.'
    && unichar_at(expr, 4) == '('
    && unichar_at(expr, 5) == ')'){
      pmath_t arg = parse_at(expr, 1);
      
      if(!is_parse_error(arg)){
        pmath_t f = parse_at(expr, 3);
        
        if(!is_parse_error(f)){
          pmath_unref(expr);
          return HOLDCOMPLETE(pmath_expr_new_extended(f, 1, arg));
        }
        
        pmath_unref(arg);
      }
      
      pmath_unref(expr);
      return pmath_ref(PMATH_SYMBOL_FAILED);
    }

    // a.f(x)
    if(exprlen == 6
    && secondchar == '.'
    && unichar_at(expr, 4) == '('
    && unichar_at(expr, 6) == ')'){
      pmath_t args, arg1, f;
      
      arg1 = parse_at(expr, 1);
      if(is_parse_error(arg1)){
        pmath_unref(expr);
        return pmath_ref(PMATH_SYMBOL_FAILED);
      }
      
      f = parse_at(expr, 3);
      if(is_parse_error(f)){
        pmath_unref(arg1);
        pmath_unref(expr);
        return pmath_ref(PMATH_SYMBOL_FAILED);
      }

      args = pmath_evaluate(
        pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_MAKEEXPRESSION), 1,
          pmath_expr_get_item(expr, 5)));

      if(pmath_is_expr(args)){
        size_t argslen = pmath_expr_length(args);

        pmath_unref(expr);
        expr = pmath_expr_resize(args, argslen + 1);

        for(i = argslen;i > 0;--i)
          expr = pmath_expr_set_item(
            expr, i + 1,
            pmath_expr_get_item(expr, i));

        return HOLDCOMPLETE(
            pmath_expr_set_item(
              pmath_expr_set_item(
                expr, 0,
                f), 1,
              arg1));
      }

      pmath_unref(arg1);
      pmath_unref(f);
      pmath_unref(args);
      goto FAILED;
    }

    // a..b..c
    if(is_string_at(expr, 2, "..")
    || is_string_at(expr, 1, "..")){
      size_t have_arg = FALSE;
      size_t i;

      pmath_gather_begin(PMATH_NULL);

      for(i = 1;i <= pmath_expr_length(expr);++i){
        if(is_string_at(expr, i, "..")){
          if(!have_arg)
            pmath_emit(pmath_ref(PMATH_SYMBOL_AUTOMATIC), PMATH_NULL);

          have_arg = FALSE;
        }
        else{
          if(have_arg){
            pmath_unref(pmath_gather_end());
            handle_row_error_at(expr, i);
            pmath_unref(expr);
            return pmath_ref(PMATH_SYMBOL_FAILED);
          }

          have_arg = TRUE;
          
          box = parse_at(expr, i);
          if(is_parse_error(box)){
            pmath_unref(expr);
            pmath_unref(pmath_gather_end());
            return pmath_ref(PMATH_SYMBOL_FAILED);
          }

          pmath_emit(box, PMATH_NULL);
        }
      }

      if(!have_arg)
        pmath_emit(pmath_ref(PMATH_SYMBOL_AUTOMATIC), PMATH_NULL);

      pmath_unref(expr);
      expr = pmath_gather_end();
      expr = pmath_expr_set_item(expr, 0, pmath_ref(PMATH_SYMBOL_RANGE));
      return HOLDCOMPLETE(expr);
    }

    // implicit evaluation sequence (newlines -> head = /\/ = PMATH_NULL) ...
    box = pmath_expr_get_item(expr, 0);
    pmath_unref(box);

    if(pmath_is_null(box)){
      for(i = 1;i <= exprlen;++i){
        box = pmath_expr_extract_item(expr, i);

        if(!parse(&box)){
          pmath_unref(box);
          pmath_unref(expr);
          return pmath_ref(PMATH_SYMBOL_FAILED);
        }

        expr = pmath_expr_set_item(expr, i, box);
      }

      return HOLDCOMPLETE(
        pmath_expr_set_item(
          expr, 0,
          pmath_ref(PMATH_SYMBOL_EVALUATIONSEQUENCE)));
    }

    // multiplication ...
    pmath_gather_begin(PMATH_NULL);

    i = 1;
    while(i <= exprlen){
      box = parse_at(expr, i);
      if(is_parse_error(box)){
        pmath_unref(expr);
        pmath_unref(pmath_gather_end());
        return pmath_ref(PMATH_SYMBOL_FAILED);
      }
      pmath_emit(box, PMATH_NULL);

      firstchar = i + 1 >= exprlen ? 0 : unichar_at(expr, i + 1);
      if(firstchar == '*' || firstchar == 0x00D7 || firstchar == ' ')
        i+= 2;
      else
        ++i;
    }

    pmath_unref(expr);
    return HOLDCOMPLETE(
      pmath_expr_set_item(
        pmath_gather_end(), 0,
        pmath_ref(PMATH_SYMBOL_TIMES)));

   FAILED:
    pmath_message(PMATH_NULL, "inv", 1, expr);
    return pmath_ref(PMATH_SYMBOL_FAILED);
  }

  return HOLDCOMPLETE(box);
}
