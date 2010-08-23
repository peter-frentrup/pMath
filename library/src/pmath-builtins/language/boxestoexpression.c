#include <pmath-core/symbols.h>
#include <pmath-language/tokens.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/memory.h>

#include <assert.h>
#include <string.h>

#include <pmath-util/debug.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-util/concurrency/threads.h>

#include <pmath-core/objects-private.h>
#include <pmath-core/numbers-private.h>
#include <pmath-core/expressions-private.h>
#include <pmath-core/strings-private.h>

#include <pmath-builtins/lists-private.h>
#include <pmath-builtins/all-symbols.h>
#include <pmath-builtins/all-symbols-private.h>

#include <pmath-language/charnames.h>
#include <pmath-language/patterns-private.h>
#include <pmath-language/scanner.h>

static uint16_t unichar_at(
  pmath_expr_t expr, 
  size_t i
){
  pmath_string_t obj = (pmath_string_t)pmath_expr_get_item(expr, i);
  uint16_t result = 0;
  if(pmath_instance_of(obj, PMATH_TYPE_STRING)
  && pmath_string_length(obj) == 1){
    result = *pmath_string_buffer(obj);
  }
  pmath_unref(obj);
  return result;
}

static pmath_bool_t string_equals(pmath_string_t str, const char *cstr){
  const uint16_t *buf;
  size_t len = strlen(cstr);
  if((size_t)pmath_string_length(str) != len)
    return FALSE;
  
  buf = pmath_string_buffer(str);
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
  pmath_string_t obj = (pmath_string_t)pmath_expr_get_item(expr, i);
  const uint16_t *buf;
  size_t len;
  
  if(!pmath_instance_of(obj, PMATH_TYPE_STRING)){
    pmath_unref(obj);
    return FALSE;
  }
  
  len = strlen(str);
  if(len != (size_t)pmath_string_length(obj)){
    pmath_unref(obj);
    return FALSE;
  }
  
  buf = pmath_string_buffer(obj);
  while(len-- > 0)
    if(*buf++ != (unsigned char)*str++){
      pmath_unref(obj);
      return FALSE;
    }
    
  pmath_unref(obj);
  return TRUE;
}

static __inline int hex(uint16_t ch){
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
// *box = NULL if result is FALSE
  pmath_t obj;
  
  *box = pmath_evaluate(
    pmath_expr_new_extended(
      pmath_ref(PMATH_SYMBOL_BOXESTOEXPRESSION), 1, *box));
  
  if(!pmath_instance_of(*box, PMATH_TYPE_EXPRESSION)){
    pmath_unref(*box);
    *box = NULL;
    return FALSE;
  }
  
  obj = pmath_expr_get_item(*box, 0);
  pmath_unref(obj);
  
  if(obj != PMATH_SYMBOL_HOLDCOMPLETE){
    pmath_message(NULL, "inv", 1, *box);
    *box = NULL;
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

static pmath_string_t box_as_string(pmath_t box){
  while(box){
    if(pmath_instance_of(box, PMATH_TYPE_STRING))
      return (pmath_string_t)box;
    
    if(pmath_instance_of(box, PMATH_TYPE_EXPRESSION)
    && pmath_expr_length(box) == 1){
      pmath_t obj = pmath_expr_get_item(box, 1);
      pmath_unref(box);
      box = obj;
    }
    else{
      pmath_unref(box);
      return NULL;
    }
  }
  
  return NULL;
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
  return NULL;
}

static pmath_symbol_t relation_at(pmath_expr_t expr, size_t i){ // do not free result
  switch(unichar_at(expr, i)){
    case '<': return PMATH_SYMBOL_LESS;
    case '>': return PMATH_SYMBOL_GREATER;
    case '=': return PMATH_SYMBOL_EQUAL;
    
    case 0x2260: return PMATH_SYMBOL_UNEQUAL;
    
    case 0x2264: return PMATH_SYMBOL_LESSEQUAL;
    case 0x2265: return PMATH_SYMBOL_GREATEREQUAL;
  }
  
  if(is_string_at(expr, i, "<="))  return PMATH_SYMBOL_LESSEQUAL;
  if(is_string_at(expr, i, ">="))  return PMATH_SYMBOL_GREATEREQUAL;
  if(is_string_at(expr, i, "!="))  return PMATH_SYMBOL_UNEQUAL;
  if(is_string_at(expr, i, "===")) return PMATH_SYMBOL_IDENTICAL;
  if(is_string_at(expr, i, "=!=")) return PMATH_SYMBOL_UNIDENTICAL;
  
  return NULL;
}

static pmath_t parse_gridbox( // NULL on error
  pmath_expr_t expr,           // wont be freed
  pmath_bool_t    remove_styling
){
  pmath_expr_t options, matrix, row;
  size_t height, width, i, j;
  
  options = pmath_options_extract(expr, 1);
  
  if(!options)
    return NULL;
  
  matrix = (pmath_expr_t)pmath_expr_get_item(expr, 1);
    
  if(!_pmath_is_matrix(matrix, &height, &width)){
    pmath_unref(options);
    pmath_unref(matrix);
    return NULL;
  }
  
  for(i = 1;i <= height;++i){
    row = pmath_expr_get_item(matrix, i);
    matrix = pmath_expr_set_item(matrix, i, NULL);
    
    for(j = 1;j <= width;++j){
      pmath_t obj = pmath_expr_get_item(row, j);
      row = pmath_expr_set_item(row, j, NULL);
      
      if(!parse(&obj)){
        pmath_unref(options);
        pmath_unref(matrix);
        pmath_unref(row);
        return NULL;
      }
      
      row = pmath_expr_set_item(row, j, obj);
    }
    
    matrix = pmath_expr_set_item(matrix, i, row);
  }
  
  if(remove_styling){
    pmath_unref(options);
    return HOLDCOMPLETE(matrix);
  }
  
  i = pmath_expr_length(options);
  
  row = pmath_expr_new(
    pmath_ref(PMATH_SYMBOL_GRID), 
    1 + i);
  
  row = pmath_expr_set_item(row, 1, matrix);
  
  for(;i > 0;--i){
    row = pmath_expr_set_item(
      row, i + 1, 
      pmath_expr_get_item(
        row, i));
  }
    
  return HOLDCOMPLETE(row);
}

PMATH_PRIVATE pmath_t _pmath_parse_number(
  pmath_string_t  string, // will be freed
  pmath_bool_t alternative
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
  
  const uint16_t *str = pmath_string_buffer(string);
  int len = pmath_string_length(string);
  
  if(len == 0){
    pmath_unref(string);
    return NULL;
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
    return NULL;
  }
  
  while(i < len && '0' <= str[i] && str[i] <= '9')
    ++i;
  
  if(i + 2 < len && str[i] == '^' && str[i+1] == '^'){
    cstr = (char*)pmath_mem_alloc(i + 1);
    if(!cstr){
      pmath_unref(string);
      return NULL;
    }
    
    for(j = 0;j < i;++j)
      cstr[j] = (char)str[j];
    cstr[i] = '\0';
    base = (int)strtol(cstr, NULL, 10);
    pmath_mem_free(cstr);
    
    if(base < 2 || base > 36){
      if(!alternative){
        pmath_message(
          NULL, "base", 2,
          pmath_string_part(string, 0, i),
          pmath_ref(string));
      }
      else
        pmath_unref(string);
        
      return NULL;
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
      NULL, "digit", 3,
      pmath_integer_new_si(i - start + 1),
      pmath_string_part(string, start, end - start),
      pmath_integer_new_si(base));
      
    return NULL;
  }
  
  end = i;
  
  cstr = (char*)pmath_mem_alloc(len - start + 1);
  if(!cstr){
    pmath_unref(string);
    return NULL;
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
      
      prec_acc = strtod(cstr, NULL);
    }
    
    if(prec_control == PMATH_PREC_CTRL_GIVEN_PREC
    && !isfinite(prec_acc))
      prec_control = PMATH_PREC_CTRL_MACHINE_PREC;
  }
  
  for(j = start;j < end;++j)
    cstr[j - start] = (char)str[j];
  cstr[end - start] = '\0';
  
  exponent = NULL;
  if((i + 2 < len && str[i] == '*' && str[i+1] == '^')
  || (alternative && i + 1 < len && (str[i] == 'e' || str[i] == 'E'))){
    int exp;
    int delta = end - start - i - 1;
      
    if(str[i] == '*'){
      exp = i+= 2;
      
      if(is_mp_float){
        cstr[i + delta - 1] = base <= 10 ? 'e' : '@';
      }
    }
    else
      exp = i+= 1;
      
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
  
  if(exponent){
    result = pmath_evaluate(
      pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_TIMES), 2,
        result,
        pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_POWER), 2,
          pmath_integer_new_si(base),
          exponent)));
  }
    
  pmath_mem_free(cstr);
  
  if(i < len){
    if(!alternative)
      pmath_message(NULL, "inv", 1, string);
    pmath_unref(result);
    return NULL;
  }
  
  pmath_unref(string);
  
  if(pmath_instance_of(result, PMATH_TYPE_NUMBER)){
    if(neg)
      return pmath_number_neg(result);
    
    return result;
  }
  
  pmath_unref(result);
  return NULL;
}

PMATH_PRIVATE pmath_t builtin_boxestoexpression(pmath_expr_t expr){
/* BoxesToExpression(boxes)
   returns $Failed on error and HoldComplete(result) on success.
   
   options:
     ParserArguments     {arg1, arg2, ...}
     ParseSymbols        True/False
 */
  pmath_t box;
  size_t exprlen = pmath_expr_length(expr);
  
  if(exprlen > 1){
    pmath_expr_t options = pmath_options_extract(expr, 1);
    
    if(options){
      pmath_t args = pmath_option_value(NULL, PMATH_SYMBOL_PARSERARGUMENTS, options);
      pmath_t syms = pmath_option_value(NULL, PMATH_SYMBOL_PARSESYMBOLS,    options);
      
      if(args != PMATH_SYMBOL_AUTOMATIC){
        args = pmath_thread_local_save(PMATH_THREAD_KEY_PARSERARGUMENTS, args);
      }
      
      if(syms != PMATH_SYMBOL_AUTOMATIC){
        syms = pmath_thread_local_save(PMATH_THREAD_KEY_PARSESYMBOLS, syms);
      }
      
      box = pmath_expr_get_item(expr, 1);
      pmath_unref(expr);
      expr = pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_BOXESTOEXPRESSION), 1,
        box);
      
      expr = pmath_evaluate(expr);
      
      pmath_unref(options);
      pmath_unref(pmath_thread_local_save(PMATH_THREAD_KEY_PARSERARGUMENTS, args));
      pmath_unref(pmath_thread_local_save(PMATH_THREAD_KEY_PARSESYMBOLS,    syms));
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
  
  if(pmath_instance_of(box, PMATH_TYPE_STRING)){
    pmath_token_t tok;
    const uint16_t *str;
    int len = pmath_string_length(box);
    
    if(len == 0){
      pmath_unref(box);
      return HOLDCOMPLETE(NULL);
    }
    
    str = pmath_string_buffer((pmath_string_t)box);
    
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
      if(!pmath_instance_of(box, PMATH_TYPE_EXPRESSION))
        return HOLDCOMPLETE(box);
      
      result = pmath_expr_get_item((pmath_expr_t)box, argi);
      pmath_unref(box);
      return HOLDCOMPLETE(result);
    }
    
    tok = pmath_token_analyse(str, 1, NULL);
    if(tok == PMATH_TOK_DIGIT){
      pmath_number_t result = _pmath_parse_number(box, FALSE);
      
      if(!result)
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
//        if(str[i] == '`' && i + 1 < len && pmath_char_is_name(str[i+1]))
//          i+= 2;
//        else if(pmath_char_is_name(str[i]) || pmath_char_is_digit(str[i]))
//          ++i;
//        else
//          break;
      }
      
      if(i < len){
        pmath_message(NULL, "inv", 1, box);
        return pmath_ref(PMATH_SYMBOL_FAILED);
      }
      
      expr = pmath_thread_local_load(PMATH_THREAD_KEY_PARSESYMBOLS);
      pmath_unref(expr);
      
      expr = pmath_symbol_find((pmath_string_t)pmath_ref(box), 
        expr == PMATH_SYMBOL_TRUE || expr == PMATH_UNDEFINED);
      
      if(expr){
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
      while(i < len - 1 /*&& str[i] != '"'*/){
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
              AFTER_STRING(result)[j++] = '\\';
              AFTER_STRING(result)[j++] = str[i++];
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
        return HOLDCOMPLETE((pmath_string_t)result);
      }
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
          pmath_integer_new_si(-(long)len)));
    }
    
    if(len == 1 && str[0] == '#'){
      pmath_unref(box);
      return HOLDCOMPLETE(
        pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_PUREARGUMENT), 1,
          pmath_integer_new_si(1)));
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
    
    if(len == 2 && str[0] == '~' && str[1] == '~'){
      pmath_unref(box);
      return HOLDCOMPLETE(pmath_ref(_pmath_object_multimatch));
    }
    
    if(len == 2 && str[0] == '.' && str[1] == '.'){
      pmath_unref(box);
      return HOLDCOMPLETE(
        pmath_expr_new(
          pmath_ref(PMATH_SYMBOL_RANGE), 2));
    }
    
    if(len == 3 && str[0] == '~' && str[1] == '~' && str[2] == '~'){
      pmath_unref(box);
      return HOLDCOMPLETE(pmath_ref(_pmath_object_zeromultimatch));
    }
    
    if(len == 3 && str[0] == '/' && str[1] == '\\' && str[2] == '/'){
      pmath_unref(box);
      return HOLDCOMPLETE(NULL);
    }
    
    pmath_message(NULL, "inv", 1, box);
    return pmath_ref(PMATH_SYMBOL_FAILED);
  }
  else if(pmath_instance_of(box, PMATH_TYPE_EXPRESSION)){
    uint16_t firstchar, secondchar;
    size_t i;
    
    expr = (pmath_expr_t)box;
    exprlen = pmath_expr_length(expr);
    box = pmath_expr_get_item(expr, 0);
    pmath_unref(box);
    
    if(box && box != PMATH_SYMBOL_LIST){
      if(box == PMATH_SYMBOL_FRACTIONBOX && exprlen == 2){
        pmath_t num = pmath_expr_get_item(expr, 1);
        box = pmath_expr_get_item(expr, 2);
        
        if(parse(&num) && parse(&box)){
          pmath_unref(expr);
          
          if(pmath_instance_of(num, PMATH_TYPE_INTEGER)
          && pmath_instance_of(box, PMATH_TYPE_INTEGER))
            return HOLDCOMPLETE(pmath_rational_new(num, box));
          
          if(pmath_equals(num, PMATH_NUMBER_ONE)){
            pmath_unref(num);
            
            return HOLDCOMPLETE(pmath_expr_new_extended(
              pmath_ref(PMATH_SYMBOL_POWER), 2,
              box,
              pmath_integer_new_si(-1)));
          }
          
          return HOLDCOMPLETE(
            pmath_expr_new_extended(
              pmath_ref(PMATH_SYMBOL_TIMES), 2,
              num,
              pmath_expr_new_extended(
                pmath_ref(PMATH_SYMBOL_POWER), 2,
                box,
                pmath_integer_new_si(-1))));
        }
        
        pmath_unref(num);
        pmath_unref(box);
      }
      else if(box == PMATH_SYMBOL_FRAMEBOX && exprlen == 1){
        box = pmath_expr_get_item(expr, 1);
        
        if(parse(&box)){
          pmath_unref(expr);
          return HOLDCOMPLETE(
            pmath_expr_new_extended(
              pmath_ref(PMATH_SYMBOL_FRAMED), 1,
              box));
        }
      }
      else if(box == PMATH_SYMBOL_GRIDBOX && exprlen >= 1){
        box = parse_gridbox(expr, TRUE);
        if(box){
          pmath_unref(expr);
          return box;
        }
      }
      else if(box == PMATH_SYMBOL_HOLDCOMPLETE){
        return expr;
      }
      else if(box == PMATH_SYMBOL_INTERPRETATIONBOX && exprlen >= 2){
        pmath_expr_t options = pmath_options_extract(expr, 2);
        pmath_unref(options);
        if(options){
          box = pmath_expr_get_item(expr, 2);
          pmath_unref(expr);
          return HOLDCOMPLETE(box);
        }
      }
      else if(box == PMATH_SYMBOL_RADICALBOX && exprlen == 2){
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
                pmath_integer_new_si(-1))));
        }
        
        pmath_unref(base);
        pmath_unref(box);
      }
      else if(box == PMATH_SYMBOL_SQRTBOX && exprlen == 1){
        box = pmath_expr_get_item(expr, 1);
        
        if(parse(&box)){
          pmath_unref(expr);
          return HOLDCOMPLETE(
            pmath_expr_new_extended(
              pmath_ref(PMATH_SYMBOL_POWER), 2,
              box,
              pmath_ref(_pmath_one_half)));
        }
      }
      else if(box == PMATH_SYMBOL_TAGBOX && exprlen == 2){
        pmath_t view = pmath_expr_get_item(expr, 1);
        pmath_t tag  = pmath_expr_get_item(expr, 2);
        
        if(pmath_instance_of(tag, PMATH_TYPE_STRING)){
          if(pmath_string_equals_latin1(tag, "Grid")){
            if(pmath_is_expr_of(view, PMATH_SYMBOL_GRIDBOX)){
              box = parse_gridbox(view, FALSE);
              
              pmath_unref(view);
              pmath_unref(tag);
              
              if(box){
                pmath_unref(expr);
                return box;
              }
            }
            else if(parse(&view)){
              pmath_unref(expr);
              pmath_unref(tag);
              return HOLDCOMPLETE(view);
            }
            
            goto FAILED;
          }
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
      }
      else if(box == PMATH_SYMBOL_ROTATIONBOX && exprlen >= 1){
        pmath_t options = pmath_options_extract(expr, 1);
        
        if(options){
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
      }
      
      goto FAILED;
    }
    
    box = NULL;
    
    // box is invalid, expr is valid
    
    if(exprlen == 0){
      pmath_unref(expr);
      return HOLDCOMPLETE(NULL);
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
        pmath_ref(PMATH_SYMBOL_BOXESTOEXPRESSION), 1, box);
    }
    
    // comma sepearted list ...
    if(firstchar == ',' || secondchar == ','){
      pmath_t prev = NULL;
      pmath_bool_t last_was_comma = unichar_at(expr, 1) == ',';
      if(!last_was_comma){
        prev = pmath_expr_get_item(expr, 1);
        if(!parse(&prev)){
          pmath_unref(expr);
          return pmath_ref(PMATH_SYMBOL_FAILED);
        }
        i = 2;
      }
      else
        i = 1;
        
      pmath_gather_begin(NULL);
      
      while(i <= exprlen){
        if(unichar_at(expr, i) == ','){
          last_was_comma = TRUE;
          pmath_emit(prev, NULL);
          prev = NULL;
        }
        else if(!last_was_comma){
          last_was_comma = FALSE;
          pmath_message(NULL, "inv", 1, expr);
          pmath_unref(pmath_gather_end());
          pmath_unref(prev);
          return pmath_ref(PMATH_SYMBOL_FAILED);
        }
        else{
          last_was_comma = FALSE;
          prev = pmath_expr_get_item(expr, i);
          if(!parse(&prev)){
            pmath_unref(expr);
            pmath_unref(pmath_gather_end());
            return pmath_ref(PMATH_SYMBOL_FAILED);
          }
        }
        ++i;
      }
      pmath_emit(prev, NULL);
      
      pmath_unref(expr);
      return pmath_expr_set_item(
        pmath_gather_end(), 0,
        pmath_ref(PMATH_SYMBOL_HOLDCOMPLETE));
    }
    
    // evaluation sequence ...
    if(firstchar == ';' || secondchar == ';' || firstchar == '\n' || secondchar == '\n'){
      pmath_t prev = NULL;
      pmath_bool_t last_was_semicolon = TRUE;
      
      pmath_gather_begin(NULL);
      
      i = 1;
      while(i <= exprlen){
        uint16_t ch = unichar_at(expr, i);
        if(ch == ';' || ch == '\n'){
          last_was_semicolon = TRUE;
          pmath_emit(prev, NULL);
          prev = NULL;
        }
        else if(!last_was_semicolon){
          last_was_semicolon = FALSE;
          pmath_message(NULL, "inv", 1, expr);
          pmath_unref(pmath_gather_end());
          pmath_unref(prev);
          return pmath_ref(PMATH_SYMBOL_FAILED);
        }
        else{
          last_was_semicolon = FALSE;
          prev = pmath_expr_get_item(expr, i);
          if(!parse(&prev)){
            pmath_unref(expr);
            pmath_unref(pmath_gather_end());
            return pmath_ref(PMATH_SYMBOL_FAILED);
          }
        }
        ++i;
      }
      pmath_emit(prev, NULL);
      
      pmath_unref(expr);
      return HOLDCOMPLETE(
        pmath_expr_set_item(
          pmath_gather_end(), 0,
          pmath_ref(PMATH_SYMBOL_EVALUATIONSEQUENCE)));
    }
    
    if(exprlen == 1)
      return pmath_expr_set_item(expr, 0, pmath_ref(PMATH_SYMBOL_BOXESTOEXPRESSION));
    
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
          pmath_ref(PMATH_SYMBOL_BOXESTOEXPRESSION), 1,
          pmath_expr_get_item(expr, 2)));
      
      pmath_unref(expr);
      if(pmath_instance_of(args, PMATH_TYPE_EXPRESSION)){
        return HOLDCOMPLETE(
          pmath_expr_set_item(
            (pmath_expr_t)args, 0, 
            pmath_ref(PMATH_SYMBOL_LIST)));
      }
      
      pmath_unref(args);
      return pmath_ref(PMATH_SYMBOL_FAILED);
    }
    
    // ?x  and  ?x:v
    if(firstchar == '?'){
      if(exprlen == 2){
        box = pmath_expr_get_item(expr, 2);
        if(parse(&box)){
          pmath_unref(expr);
          return HOLDCOMPLETE(
            pmath_expr_new_extended(
              pmath_ref(PMATH_SYMBOL_OPTIONAL), 1,
              box));
        }
      }
      else if(exprlen == 4 && unichar_at(expr, 3) == ':'){
        box = pmath_expr_get_item(expr, 2);
        if(parse(&box)){
          pmath_t value = pmath_expr_get_item(expr, 4);
          if(parse(&value)){
            pmath_unref(expr);
            return HOLDCOMPLETE(
              pmath_expr_new_extended(
                pmath_ref(PMATH_SYMBOL_OPTIONAL), 2,
                box,
                value));
          }
        }
      }
      
      pmath_unref(expr);
      return pmath_ref(PMATH_SYMBOL_FAILED);
    }
    
    if(exprlen == 2){ // x& x! x++ x-- x.. p** p*** +x -x !x #x ++x --x ..x ??x <<x ~x ~~x ~~~x
      // x &
      if(secondchar == '&'){
        box = pmath_expr_get_item(expr, 1);
        pmath_unref(expr);
        if(parse(&box)){
          return HOLDCOMPLETE(
            pmath_expr_new_extended(
              pmath_ref(PMATH_SYMBOL_FUNCTION), 1,
              box));
        }
        return pmath_ref(PMATH_SYMBOL_FAILED);
      }
      
      // x!
      if(secondchar == '!'){
        box = pmath_expr_get_item(expr, 1);
        pmath_unref(expr);
        if(parse(&box)){
          return HOLDCOMPLETE(
            pmath_expr_new_extended(
              pmath_ref(PMATH_SYMBOL_FACTORIAL), 1,
              box));
        }
        return pmath_ref(PMATH_SYMBOL_FAILED);
      }
      
      // x!!
      if(is_string_at(expr, 2, "!!")){
        box = pmath_expr_get_item(expr, 1);
        pmath_unref(expr);
        if(parse(&box)){
          return HOLDCOMPLETE(
            pmath_expr_new_extended(
              pmath_ref(PMATH_SYMBOL_FACTORIAL2), 1,
              box));
        }
        return pmath_ref(PMATH_SYMBOL_FAILED);
      }
      
      // x++
      if(is_string_at(expr, 2, "++")){
        box = pmath_expr_get_item(expr, 1);
        pmath_unref(expr);
        if(parse(&box)){
          return HOLDCOMPLETE(
            pmath_expr_new_extended(
              pmath_ref(PMATH_SYMBOL_POSTINCREMENT), 1,
              box));
        }
        return pmath_ref(PMATH_SYMBOL_FAILED);
      }
      
      // x--
      if(is_string_at(expr, 2, "--")){
        box = pmath_expr_get_item(expr, 1);
        pmath_unref(expr);
        if(parse(&box)){
          return HOLDCOMPLETE(
            pmath_expr_new_extended(
              pmath_ref(PMATH_SYMBOL_POSTDECREMENT), 1,
              box));
        }
        return pmath_ref(PMATH_SYMBOL_FAILED);
      }
      
      // x..
      if(is_string_at(expr, 2, "..")){
        box = pmath_expr_get_item(expr, 1);
        pmath_unref(expr);
        if(parse(&box)){
          return HOLDCOMPLETE(
            pmath_expr_new_extended(
              pmath_ref(PMATH_SYMBOL_RANGE), 2,
              box,
              NULL));
        }
        return pmath_ref(PMATH_SYMBOL_FAILED);
      }
      
      // p**
      if(is_string_at(expr, 2, "**")){
        box = pmath_expr_get_item(expr, 1);
        pmath_unref(expr);
        if(parse(&box)){
          return HOLDCOMPLETE(
            pmath_expr_new_extended(
              pmath_ref(PMATH_SYMBOL_REPEATED), 2,
              box,
              pmath_ref(_pmath_object_range_from_one)));
        }
        return pmath_ref(PMATH_SYMBOL_FAILED);
      }
      
      // p***
      if(is_string_at(expr, 2, "***")){
        box = pmath_expr_get_item(expr, 1);
        pmath_unref(expr);
        if(parse(&box)){
          return HOLDCOMPLETE(
            pmath_expr_new_extended(
              pmath_ref(PMATH_SYMBOL_REPEATED), 2,
              box,
              pmath_ref(_pmath_object_range_from_zero)));
        }
        return pmath_ref(PMATH_SYMBOL_FAILED);
      }
      
      // +x
      if(firstchar == '+'){
        box = pmath_expr_get_item(expr, 2);
        pmath_unref(expr);
        return pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_BOXESTOEXPRESSION), 1, box);
      }
      
      // -x
      if(firstchar == '-'){
        box = pmath_expr_get_item(expr, 2);
        pmath_unref(expr);
        if(parse(&box)){
          if(pmath_instance_of(box, PMATH_TYPE_NUMBER))
            return HOLDCOMPLETE(pmath_number_neg((pmath_number_t)box));
            
          return HOLDCOMPLETE(
            pmath_expr_new_extended(
              pmath_ref(PMATH_SYMBOL_TIMES), 2, 
              pmath_integer_new_si(-1),
              box));
        }
        return pmath_ref(PMATH_SYMBOL_FAILED);
      }
      
      if(firstchar == PMATH_CHAR_PIECEWISE){
        box = pmath_expr_get_item(expr, 2);
        pmath_unref(expr);
        if(parse(&box)){
          return HOLDCOMPLETE(
            pmath_expr_new_extended(
              pmath_ref(PMATH_SYMBOL_PIECEWISE), 1,
              box));
        }
        return pmath_ref(PMATH_SYMBOL_FAILED);
      }
      
      // !x
      if(firstchar == '!'){
        box = pmath_expr_get_item(expr, 2);
        pmath_unref(expr);
        if(parse(&box)){
          return HOLDCOMPLETE(
            pmath_expr_new_extended(
              pmath_ref(PMATH_SYMBOL_NOT), 1,
              box));
        }
        return pmath_ref(PMATH_SYMBOL_FAILED);
      }
      
      // #x
      if(firstchar == '#'){
        box = pmath_expr_get_item(expr, 2);
        pmath_unref(expr);
        if(parse(&box)){
          return HOLDCOMPLETE(
            pmath_expr_new_extended(
              pmath_ref(PMATH_SYMBOL_PUREARGUMENT), 1,
              box));
        }
        return pmath_ref(PMATH_SYMBOL_FAILED);
      }
      
      // ++x
      if(is_string_at(expr, 1, "++")){
        box = pmath_expr_get_item(expr, 2);
        pmath_unref(expr);
        if(parse(&box)){
          return HOLDCOMPLETE(
            pmath_expr_new_extended(
              pmath_ref(PMATH_SYMBOL_INCREMENT), 1,
              box));
        }
        return pmath_ref(PMATH_SYMBOL_FAILED);
      }
      
      // --x
      if(is_string_at(expr, 1, "--")){
        box = pmath_expr_get_item(expr, 2);
        pmath_unref(expr);
        if(parse(&box)){
          return HOLDCOMPLETE(
            pmath_expr_new_extended(
              pmath_ref(PMATH_SYMBOL_DECREMENT), 1,
              box));
        }
        return pmath_ref(PMATH_SYMBOL_FAILED);
      }
      
      // ..x
      if(is_string_at(expr, 1, "..")){
        box = pmath_expr_get_item(expr, 2);
        pmath_unref(expr);
        if(parse(&box)){
          return HOLDCOMPLETE(
            pmath_expr_new_extended(
              pmath_ref(PMATH_SYMBOL_RANGE), 2,
              NULL,
              box));
        }
        return pmath_ref(PMATH_SYMBOL_FAILED);
      }
      
      // ??x
      if(is_string_at(expr, 1, "??")){
        box = pmath_expr_get_item(expr, 2);
        
        pmath_unref(expr);
        if(!pmath_instance_of(box, PMATH_TYPE_STRING)
        || pmath_string_length(box) == 0
        || pmath_string_buffer(box)[0] == '"'){
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
        if(!pmath_instance_of(box, PMATH_TYPE_STRING)
        || pmath_string_length(box) == 0
        || pmath_string_buffer(box)[0] == '"'){
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
        box = pmath_expr_get_item(expr, 2);
        pmath_unref(expr);
        if(parse(&box)){
          return HOLDCOMPLETE(
            pmath_expr_new_extended(
              pmath_ref(PMATH_SYMBOL_PATTERN), 2,
              box,
              pmath_ref(_pmath_object_singlematch)));
        }
        return pmath_ref(PMATH_SYMBOL_FAILED);
      }
      
      // ~~x
      if(is_string_at(expr, 1, "~~")){
        box = pmath_expr_get_item(expr, 2);
        pmath_unref(expr);
        if(parse(&box)){
          return HOLDCOMPLETE(
            pmath_expr_new_extended(
              pmath_ref(PMATH_SYMBOL_PATTERN), 2,
              box,
              pmath_ref(_pmath_object_multimatch)));
        }
        return pmath_ref(PMATH_SYMBOL_FAILED);
      }
      
      // ~~~x
      if(is_string_at(expr, 1, "~~~")){
        box = pmath_expr_get_item(expr, 2);
        pmath_unref(expr);
        if(parse(&box)){
          return HOLDCOMPLETE(
            pmath_expr_new_extended(
              pmath_ref(PMATH_SYMBOL_PATTERN), 2,
              box,
              pmath_ref(_pmath_object_zeromultimatch)));
        }
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
                pmath_ref(PMATH_SYMBOL_BOXESTOEXPRESSION), 1,
                idx));
            
            if(pmath_instance_of(idx, PMATH_TYPE_EXPRESSION)){
              pmath_t head = pmath_expr_get_item(idx, 0);
              pmath_unref(head);
              
              if(head == PMATH_SYMBOL_HOLDCOMPLETE){
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
            pmath_ref(PMATH_SYMBOL_BOXESTOEXPRESSION), 1,
            expr);
          
//          pmath_t base = pmath_expr_get_item(expr, 1);
//          pmath_t exp  = pmath_expr_get_item(box,  2);
//          
//          pmath_unref(expr);
//          
//          if(parse(&base) && parse(&exp)){
//            pmath_t idx = pmath_expr_get_item(box, 1);
//            pmath_unref(box);
//            
//            idx = pmath_evaluate(
//              pmath_expr_new_extended(
//                pmath_ref(PMATH_SYMBOL_BOXESTOEXPRESSION), 1,
//                idx));
//            
//            if(pmath_instance_of(idx, PMATH_TYPE_EXPRESSION)){
//              pmath_t head = pmath_expr_get_item(idx, 0);
//              pmath_unref(head);
//              
//              if(head == PMATH_SYMBOL_HOLDCOMPLETE){
//                exprlen = pmath_expr_length(idx) + 1;
//                
//                expr = pmath_expr_new(
//                  pmath_ref(PMATH_SYMBOL_SUBSCRIPT), 
//                  exprlen);
//                
//                expr = pmath_expr_set_item(expr, 1, base);
//                
//                for(;exprlen > 1;--exprlen){
//                  expr = pmath_expr_set_item(
//                    expr, exprlen,
//                    pmath_expr_get_item(idx, exprlen - 1));
//                }
//                
//                pmath_unref(idx);
//                return HOLDCOMPLETE(
//                  pmath_expr_new_extended(
//                    pmath_ref(PMATH_SYMBOL_POWER), 2,
//                    expr,
//                    exp));
//              }
//            }
//            
//            pmath_unref(idx);
//          }
//          else
//            pmath_unref(box);
//          
//          pmath_unref(base);
//          pmath_unref(exp);
//          return pmath_ref(PMATH_SYMBOL_FAILED);
        }
      
        pmath_unref(box);
        box = NULL;
      }
    }
    
    if(exprlen == 3){ // a.f  x/y  f@x  f@@list  s::tag  f()  ~:t  ~~:t  ~~~:t  x:p  a//f  p/?c  l->r  l:=r  l+=r  l-=r  l:>r  l::=r  l..r
      // a.f
      if(secondchar == '.'){
        pmath_t arg = pmath_expr_get_item(expr, 1);
        pmath_t f = pmath_expr_get_item(expr, 3);
        pmath_unref(expr);
          
        if(parse(&arg) && parse(&f))
          return HOLDCOMPLETE(pmath_expr_new_extended(f, 1, arg));
        
        pmath_unref(arg);
        pmath_unref(f);
        return pmath_ref(PMATH_SYMBOL_FAILED);
      }
      
      // x/y
//      if(secondchar == '/' || secondchar == 0x00F7){
//        pmath_t x = pmath_expr_get_item(expr, 1);
//        pmath_t y = pmath_expr_get_item(expr, 3);
//        pmath_unref(expr);
//          
//        if(parse(&x) && parse(&y)){
//          if(pmath_instance_of(x, PMATH_TYPE_INTEGER)
//          && pmath_instance_of(y, PMATH_TYPE_INTEGER)
//          && pmath_number_sign(y) > 0)
//            return HOLDCOMPLETE(pmath_rational_new(
//              (pmath_integer_t)x, 
//              (pmath_integer_t)y));
//          
//          if(pmath_equals(x, PMATH_NUMBER_ONE)){
//            pmath_unref(x);
//            
//            return HOLDCOMPLETE(
//              pmath_expr_new_extended(
//                pmath_ref(PMATH_SYMBOL_POWER), 2,
//                  y,
//                  pmath_integer_new_si(-1)));
//          }
//          
//          return HOLDCOMPLETE(
//            pmath_expr_new_extended(
//              pmath_ref(PMATH_SYMBOL_TIMES), 2, 
//              x,
//              pmath_expr_new_extended(
//                pmath_ref(PMATH_SYMBOL_POWER), 2,
//                  y,
//                  pmath_integer_new_si(-1))));
//        }
//        
//        pmath_unref(x);
//        pmath_unref(y);
//        return pmath_ref(PMATH_SYMBOL_FAILED);
//      }
      
      // f@x
      if(secondchar == '@' || secondchar == PMATH_CHAR_INVISIBLECALL){
        pmath_t f = pmath_expr_get_item(expr, 1);
        pmath_t x = pmath_expr_get_item(expr, 3);
        pmath_unref(expr);
          
        if(parse(&f) && parse(&x)){
          return HOLDCOMPLETE(
            pmath_expr_new_extended(f, 1, x));
        }
        
        pmath_unref(f);
        pmath_unref(x);
        return pmath_ref(PMATH_SYMBOL_FAILED);
      }
      
      // f()
      if(secondchar == '(' && unichar_at(expr, 3) == ')'){
        box = pmath_expr_get_item(expr, 1);
        pmath_unref(expr);
        
        if(parse(&box))
          return HOLDCOMPLETE(pmath_expr_new(box, 0));
        
        pmath_unref(box);
        return pmath_ref(PMATH_SYMBOL_FAILED);
      }
      
      // ~:t  ~~:t  ~~~:t  x:p
      if(secondchar == ':'){
        pmath_t x;
        
        box = pmath_expr_get_item(expr, 3);
        if(!parse(&box)){
          pmath_unref(box);
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
        
        x = pmath_expr_get_item(expr, 1);
        pmath_unref(expr);
        
        if(parse(&x)){
          return HOLDCOMPLETE(pmath_expr_new_extended(
            pmath_ref(PMATH_SYMBOL_PATTERN), 2,
            x,
            box));
        }
        
        pmath_unref(x);
        pmath_unref(box);
        return pmath_ref(PMATH_SYMBOL_FAILED);
      }
      
      // args :-> body
      if(secondchar == 0x21A6){
        pmath_t args = pmath_expr_get_item(expr, 1);
        pmath_t body = pmath_expr_get_item(expr, 3);
        pmath_unref(expr);
        
        if(parse(&args) && parse(&body)){
          if(pmath_is_expr_of(args, PMATH_SYMBOL_SEQUENCE))
            args = pmath_expr_set_item(
              args, 0, 
              pmath_ref(PMATH_SYMBOL_LIST));
          else
            args = pmath_expr_new_extended(
              pmath_ref(PMATH_SYMBOL_LIST), 1, 
              args);
          
          return HOLDCOMPLETE(
            pmath_expr_new_extended(
              pmath_ref(PMATH_SYMBOL_FUNCTION), 2,
              args,
              body));
        }
        
        pmath_unref(args);
        pmath_unref(body);
        return pmath_ref(PMATH_SYMBOL_FAILED);
      }
      
      // f@@list
      if(is_string_at(expr, 2, "@@")){
        pmath_t f =    pmath_expr_get_item(expr, 1);
        pmath_t list = pmath_expr_get_item(expr, 3);
        pmath_unref(expr);
          
        if(parse(&f) && parse(&list)){
          return HOLDCOMPLETE(
            pmath_expr_new_extended(
              pmath_ref(PMATH_SYMBOL_APPLY), 2, 
              list, 
              f));
        }
        
        pmath_unref(f);
        pmath_unref(list);
        return pmath_ref(PMATH_SYMBOL_FAILED);
      }
      
      // s::tag
      if(is_string_at(expr, 2, "::")){
        pmath_string_t tag = box_as_string(pmath_expr_get_item(expr, 3));
        if(!tag)
          goto FAILED;
        
        box = pmath_expr_get_item(expr, 1);
        pmath_unref(expr);
        
        if(parse(&box)){
          return HOLDCOMPLETE(
            pmath_expr_new_extended(
              pmath_ref(PMATH_SYMBOL_MESSAGENAME), 2,
              box,
              tag));
        }
        
        pmath_unref(box);
        pmath_unref(tag);
        return pmath_ref(PMATH_SYMBOL_FAILED);
      }
      
      // arg // f
      if(is_string_at(expr, 2, "//")){
        pmath_t arg = pmath_expr_get_item(expr, 1);
        pmath_t f   = pmath_expr_get_item(expr, 3);
        pmath_unref(expr);
        
        if(parse(&arg) && parse(&f)){
          return HOLDCOMPLETE(
            pmath_expr_new_extended(f, 1, arg));
        }
        
        pmath_unref(arg);
        pmath_unref(f);
        return pmath_ref(PMATH_SYMBOL_FAILED);
      }
      
      box = NULL;
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
      else if(is_string_at(expr, 2, ".."))  box = PMATH_SYMBOL_RANGE;
    
      // lhs->rhs  lhs:>rhs  lhs:=rhs  lhs::=rhs  lhs+=rhs  lhs-=lhs  lhs//rhs  lhs..rhs
      if(box){
        pmath_string_t lhs = pmath_expr_get_item(expr, 1);
        pmath_string_t rhs = pmath_expr_get_item(expr, 3);
        pmath_unref(expr);
        
        if(parse(&lhs)){
          if(box == PMATH_SYMBOL_ASSIGN
          && pmath_instance_of(rhs, PMATH_TYPE_STRING)
          && pmath_string_length(rhs) == 1
          && '.' == *pmath_string_buffer(rhs)){
            pmath_unref(rhs);
            
            return HOLDCOMPLETE(
              pmath_expr_new_extended(
                pmath_ref(PMATH_SYMBOL_UNASSIGN), 1,
                lhs));
          }
          else if(parse(&rhs)){
            return HOLDCOMPLETE(
              pmath_expr_new_extended(
                pmath_ref(box), 2,
                lhs,
                rhs));
          }
        }
        
        pmath_unref(lhs);
        pmath_unref(rhs);
        return pmath_ref(PMATH_SYMBOL_FAILED);
      }
    }
    
    if(exprlen == 4 && unichar_at(expr, 3) == ':'){ // ~x:t  ~~x:t  ~~~x:t
      pmath_t name = pmath_expr_get_item(expr, 2);
      pmath_t type = pmath_expr_get_item(expr, 4);
      
      if(!parse(&name) || !parse(&type)){
        pmath_unref(expr);
        pmath_unref(type);
        pmath_unref(name);
        return pmath_ref(PMATH_SYMBOL_FAILED);
      }
        
      if(firstchar == '~'){
        pmath_unref(expr);
        
        return HOLDCOMPLETE(
          pmath_expr_new_extended(
            pmath_ref(PMATH_SYMBOL_PATTERN), 2,
            name,
            pmath_expr_new_extended(
              pmath_ref(PMATH_SYMBOL_SINGLEMATCH), 1,
              type)));
      }
      
      if(is_string_at(expr, 1, "~~")){
        pmath_unref(expr);
        
        return HOLDCOMPLETE(
          pmath_expr_new_extended(
            pmath_ref(PMATH_SYMBOL_PATTERN), 2,
            name,
            pmath_expr_new_extended(
              pmath_ref(PMATH_SYMBOL_REPEATED), 2,
              pmath_expr_new_extended(
                pmath_ref(PMATH_SYMBOL_SINGLEMATCH), 1,
                type),
              pmath_ref(_pmath_object_range_from_one))));
      }
      
      if(is_string_at(expr, 1, "~~~")){
        pmath_unref(expr);
        
        return HOLDCOMPLETE(
          pmath_expr_new_extended(
            pmath_ref(PMATH_SYMBOL_PATTERN), 2,
            name,
            pmath_expr_new_extended(
              pmath_ref(PMATH_SYMBOL_REPEATED), 2,
              pmath_expr_new_extended(
                pmath_ref(PMATH_SYMBOL_SINGLEMATCH), 1,
                type),
              pmath_ref(_pmath_object_range_from_zero))));
      }
      
      pmath_unref(name);
      pmath_unref(type);
    }
    
    // t /: l := r  t /: l ::= r
    if(exprlen == 5 && is_string_at(expr, 2, "/:")){
      pmath_t tag, lhs, rhs;
      
      if(     unichar_at(expr, 4) == PMATH_CHAR_ASSIGN)        box = PMATH_SYMBOL_TAGASSIGN;
      else if(unichar_at(expr, 4) == PMATH_CHAR_ASSIGNDELAYED) box = PMATH_SYMBOL_TAGASSIGNDELAYED;
      else if(is_string_at(expr, 4, ":="))                     box = PMATH_SYMBOL_TAGASSIGN;
      else if(is_string_at(expr, 4, "::="))                    box = PMATH_SYMBOL_TAGASSIGNDELAYED;
      
      if(!box)
        goto FAILED;
      
      tag = pmath_expr_get_item(expr, 1);
      lhs = pmath_expr_get_item(expr, 3);
      rhs = pmath_expr_get_item(expr, 5);
      pmath_unref(expr);
      
      if(parse(&tag) && parse(&lhs)){
        if(box == PMATH_SYMBOL_TAGASSIGN
        && pmath_instance_of(rhs, PMATH_TYPE_STRING)
        && pmath_string_length(rhs) == 1
        && '.' == *pmath_string_buffer(rhs)){
          pmath_unref(rhs);
          
          return HOLDCOMPLETE(
            pmath_expr_new_extended(
              pmath_ref(PMATH_SYMBOL_TAGUNASSIGN), 2,
              tag,
              lhs));
        }
        if(parse(&rhs)){
          return HOLDCOMPLETE(
            pmath_expr_new_extended(
              pmath_ref(box), 3,
              tag,
              lhs,
              rhs));
        }
      }
      
      pmath_unref(tag);
      pmath_unref(lhs);
      pmath_unref(rhs);
      return pmath_ref(PMATH_SYMBOL_FAILED);
    }
    
    // infix operators (except * /) ...
    if(exprlen & 1){ 
      int tokprec;
      
      if(secondchar == '+' || secondchar == '-'){
        pmath_expr_t result;
        pmath_t arg = pmath_expr_get_item(expr, 1);
        
        if(!parse(&arg)){
          pmath_unref(arg);
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
          pmath_t arg = pmath_expr_get_item(expr, 2 * i + 1);
          uint16_t ch;
          
          if(!parse(&arg)){
            pmath_unref(arg);
            pmath_unref(result);
            pmath_unref(expr);
            return pmath_ref(PMATH_SYMBOL_FAILED);
          }
          
          ch = unichar_at(expr, 2 * i);
          if(ch == '-'){
            if(pmath_instance_of(arg, PMATH_TYPE_NUMBER))
              arg = pmath_number_neg(arg);
            else
              arg = pmath_expr_new_extended(
                pmath_ref(PMATH_SYMBOL_TIMES), 2,
                pmath_integer_new_si(-1),
                arg);
          }
          else if(ch != '+'){
            pmath_unref(arg);
            pmath_unref(result);
            goto FAILED;
          }
          
          result = pmath_expr_set_item(result, i + 1, arg);
        }
        
        pmath_unref(expr);
        return HOLDCOMPLETE(result);
      }
      
      // single character infix operators (except + - * / && || and relations) ...
      box = inset_operator(secondchar);
      if(box){
        pmath_expr_t result;
        
        for(i = 4;i < exprlen;i+= 2){
          if(inset_operator(unichar_at(expr, i)) != box)
            goto FAILED;
        }
        
        result = pmath_expr_new(pmath_ref(box), (1 + exprlen) / 2);
        for(i = 0;i <= exprlen / 2;++i){
          pmath_t arg = pmath_expr_get_item(expr, 2 * i + 1);
          if(!parse(&arg)){
            pmath_unref(arg);
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
          
          if(tokprec != PMATH_PREC_DIV)
            goto FAILED;
        }
        
        result = pmath_expr_new(pmath_ref(PMATH_SYMBOL_TIMES), (1 + exprlen) / 2);
        for(i = 0;i <= exprlen / 2;++i){
          pmath_t arg = pmath_expr_get_item(expr, 2 * i + 1);
          if(!parse(&arg)){
            pmath_unref(arg);
            pmath_unref(result);
            pmath_unref(expr);
            return pmath_ref(PMATH_SYMBOL_FAILED);
          }
          
          if(i > 0){
            arg = pmath_expr_new_extended(
              pmath_ref(PMATH_SYMBOL_POWER), 2,
              arg,
              pmath_integer_new_si(-1));
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
        pmath_string_t snd = (pmath_string_t)pmath_expr_get_item(expr, 2);
        if(pmath_instance_of(snd, PMATH_TYPE_STRING)){
          if(     string_equals(snd, "&&"))  box = PMATH_SYMBOL_AND;
          else if(string_equals(snd, "||"))  box = PMATH_SYMBOL_OR;
          else if(string_equals(snd, "++"))  box = PMATH_SYMBOL_STRINGEXPRESSION;
        }
        pmath_unref(snd);
      }
      
      if(box == PMATH_SYMBOL_AND){
        for(i = 4;i < exprlen;i+= 2){
          pmath_string_t op = (pmath_string_t)pmath_expr_get_item(expr, i);
          if((pmath_string_length(op) != 1
           || *pmath_string_buffer(op) != 0x2227)
          && !string_equals(op, "&&")){
            pmath_unref(op);
            goto FAILED;
          }
          pmath_unref(op);
        }
      }
      else if(box == PMATH_SYMBOL_OR){
        for(i = 4;i < exprlen;i+= 2){
          pmath_string_t op = (pmath_string_t)pmath_expr_get_item(expr, i);
          if((pmath_string_length(op) != 1
           || *pmath_string_buffer(op) != 0x2228)
          && !string_equals(op, "||")){
            pmath_unref(op);
            goto FAILED;
          }
          pmath_unref(op);
        }
      }
      
      if(box){
        pmath_expr_t result = pmath_expr_new(pmath_ref(box), (1 + exprlen) / 2);
        for(i = 0;i <= exprlen / 2;++i){
          pmath_t arg = pmath_expr_get_item(expr, 2 * i + 1);
          if(!parse(&arg)){
            pmath_unref(arg);
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
      if(box){
        pmath_expr_t result;
        
        for(i = 4;i < exprlen;++i){
          if(box != relation_at(expr, i)){
            box = pmath_expr_get_item(expr, 1);
            if(!parse(&box)){
              pmath_unref(box);
              pmath_unref(expr);
              return pmath_ref(PMATH_SYMBOL_FAILED);
            }
            
            expr = pmath_expr_set_item(expr, 1, box);
            for(i = 3;i <= exprlen;i+= 2){
              box = relation_at(expr, i - 1);
              if(!box){
                box = expr;
                expr = pmath_expr_get_item_range(box, i, exprlen);
                pmath_unref(box);
                goto FAILED;
              }
              expr = pmath_expr_set_item(expr, i - 1, pmath_ref(box));
              
              box = pmath_expr_get_item(expr, i);
              if(!parse(&box)){
                pmath_unref(box);
                pmath_unref(expr);
                return pmath_ref(PMATH_SYMBOL_FAILED);
              }
              expr = pmath_expr_set_item(expr, i, box);
            }
            
            return HOLDCOMPLETE(
              pmath_expr_set_item(expr, 0, 
                pmath_ref(PMATH_SYMBOL_INEQUATION)));
          }
        }
        
        result = pmath_expr_new(
          pmath_ref(box), (1 + exprlen) / 2);
        for(i = 0;i <= exprlen / 2;++i){
          pmath_t arg = pmath_expr_get_item(expr, 2 * i + 1);
          if(!parse(&arg)){
            pmath_unref(arg);
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
            pmath_ref(PMATH_SYMBOL_BOXESTOEXPRESSION), 1,
            pmath_expr_get_item(expr, 3)));
        
        if(pmath_instance_of(args, PMATH_TYPE_EXPRESSION)){
          pmath_t f = pmath_expr_get_item(expr, 1);
          
          pmath_unref(expr);
          if(parse(&f))
            return HOLDCOMPLETE(
              pmath_expr_set_item(
                (pmath_expr_t)args, 0, f));
          
          return pmath_ref(PMATH_SYMBOL_FAILED);
        }
        pmath_unref(args);
        pmath_unref(expr);
        return pmath_ref(PMATH_SYMBOL_FAILED);
      }
      
      // l[x]
      if(secondchar =='[' && unichar_at(expr, 4) == ']'){
        pmath_t args;
        pmath_t list = pmath_expr_get_item(expr, 1);
        if(!parse(&list)){
          pmath_unref(expr);
          return pmath_ref(PMATH_SYMBOL_FAILED);
        }
        
        args = pmath_evaluate(
          pmath_expr_new_extended(
            pmath_ref(PMATH_SYMBOL_BOXESTOEXPRESSION), 1,
            pmath_expr_get_item(expr, 3)));
        pmath_unref(expr);
        
        if(pmath_instance_of(args, PMATH_TYPE_EXPRESSION)){
          size_t argslen = pmath_expr_length((pmath_expr_t)args);
          
          pmath_expr_t result = pmath_expr_set_item(
            pmath_expr_new(
              pmath_ref(PMATH_SYMBOL_PART), 
              argslen + 1),
            1, list);
          
          for(i = argslen + 1;i > 1;--i)
            result = pmath_expr_set_item(
              result, i,
              pmath_expr_get_item(
                (pmath_expr_t)args, 
                i - 1));
          
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
        pmath_t args;
        pmath_t list = pmath_expr_get_item(expr, 1);
        if(!parse(&list)){
          pmath_unref(expr);
          return pmath_ref(PMATH_SYMBOL_FAILED);
        }
        
        args = pmath_evaluate(
          pmath_expr_new_extended(
            pmath_ref(PMATH_SYMBOL_BOXESTOEXPRESSION), 1,
            pmath_expr_get_item(expr, 3)));
        pmath_unref(expr);
        
        if(pmath_instance_of(args, PMATH_TYPE_EXPRESSION)){
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
//           = pmath_expr_set_item(
//            pmath_expr_new(
//              pmath_ref(PMATH_SYMBOL_PART), 
//              argslen + 1),
//            1, list);
//          
//          for(i = argslen + 1;i > 1;--i)
//            result = pmath_expr_set_item(
//              result, i,
//              pmath_expr_get_item(args, i - 1));
//          
//          pmath_unref(args);
//          return HOLDCOMPLETE(result);
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
      pmath_t arg = pmath_expr_get_item(expr, 1);
      pmath_t f = pmath_expr_get_item(expr, 3);
      pmath_unref(expr);
        
      if(parse(&arg) && parse(&f))
        return HOLDCOMPLETE(pmath_expr_new_extended(f, 1, arg));
      
      pmath_unref(arg);
      pmath_unref(f);
      return pmath_ref(PMATH_SYMBOL_FAILED);
    }
    
    // a.f(x)
    if(exprlen == 6
    && secondchar == '.' 
    && unichar_at(expr, 4) == '(' 
    && unichar_at(expr, 6) == ')'){
      pmath_t args;
      pmath_t arg1 = pmath_expr_get_item(expr, 1);
      pmath_t f = pmath_expr_get_item(expr, 3);
      
      if(!parse(&arg1) || !parse(&f)){
        pmath_unref(arg1);
        pmath_unref(f);
        
        return pmath_ref(PMATH_SYMBOL_FAILED);
      }
      
      args = pmath_evaluate(
        pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_BOXESTOEXPRESSION), 1,
          pmath_expr_get_item(expr, 5)));
      
      if(pmath_instance_of(args, PMATH_TYPE_EXPRESSION)){
        size_t argslen = pmath_expr_length((pmath_expr_t)args);
        
        pmath_unref(expr);
        expr = pmath_expr_resize((pmath_expr_t)args, argslen + 1);
        
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
    
    // implicit evaluation sequence (newlines -> head = /\/ = NULL) ...
    box = pmath_expr_get_item(expr, 0);
    pmath_unref(box);
    
    if(box == NULL){
      for(i = 1;i <= exprlen;++i){
        box = pmath_expr_get_item(expr, i);
        expr = pmath_expr_set_item(expr, i, NULL);
        
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
    pmath_gather_begin(NULL);
    
    i = 1;
    while(i <= exprlen){
      box = pmath_expr_get_item(expr, i);
      if(!parse(&box)){
        pmath_unref(box);
        pmath_unref(expr);
        pmath_unref(pmath_gather_end());
        return pmath_ref(PMATH_SYMBOL_FAILED);
      }
      pmath_emit(box, NULL);
      
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
    pmath_message(NULL, "inv", 1, expr);
    return pmath_ref(PMATH_SYMBOL_FAILED);
  }
  else
    return HOLDCOMPLETE(box);
}
