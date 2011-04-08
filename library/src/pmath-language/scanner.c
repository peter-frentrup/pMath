#include <pmath-core/strings-private.h>

#include <pmath-language/patterns-private.h>
#include <pmath-language/scanner.h>
#include <pmath-language/tokens.h>

#include <pmath-util/concurrency/threads.h>
#include <pmath-util/emit-and-gather.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/memory.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>

#include <limits.h>
#include <string.h>

//{ spans ...

struct _pmath_span_t{
  pmath_span_t *next;
  int end;
};

struct _pmath_span_array_t{
  int length;
  uintptr_t items[1];
};

/* Pointers are aligned by at least 4 bytes (32bit system).
   So we can use the least significant 2 bits to encode other things:
   The token-end-flag and operand-start-flag.
 */
#define SPAN_PTR(p)  ((pmath_span_t*)(((uintptr_t)p) & ~((uintptr_t)3)))
#define SPAN_TOK(p)  (((uintptr_t)p) & 1)
#define SPAN_OP(p)   (((uintptr_t)p) & 2)

static pmath_span_array_t *create_span_array(int length){
  pmath_span_array_t *result;
  
  if(length < 1)
    return NULL;
  
  result = (pmath_span_array_t*)pmath_mem_alloc(
    sizeof(pmath_span_array_t) + (size_t)(length-1) * sizeof(void*));
  
  if(!result)
    return NULL;
  
  result->length = length;
  memset(result->items, 0, (size_t)length * sizeof(void*));
  
  return result;
}

static pmath_span_array_t *enlarge_span_array(
  pmath_span_array_t *spans, 
  int extra_len
){
  pmath_span_array_t *result;
  
  if(extra_len < 1)
    return spans;
    
  if(!spans)
    return create_span_array(extra_len);
  
  result = pmath_mem_realloc_no_failfree(
    spans, 
    sizeof(pmath_span_array_t) + (size_t)(spans->length + extra_len - 1) * sizeof(void*));
  
  if(!result)
    return spans;
  
  memset(result->items + result->length, 0, extra_len * sizeof(void*));
  
  result->length+= extra_len;
  return result;
}

PMATH_API void pmath_span_array_free(pmath_span_array_t *spans){
  int i;
  
  if(!spans)
    return;
  
  assert(spans->length > 0);
  for(i = 0;i < spans->length;++i){
    pmath_span_t *s = SPAN_PTR(spans->items[i]);
    while(s){
      pmath_span_t *n = s->next;
      pmath_mem_free(s);
      s = n;
    }
  }
  
  pmath_mem_free(spans);
}

PMATH_API int pmath_span_array_length(pmath_span_array_t *spans){
  if(!spans)
    return 0;
  assert(spans->length > 0);
  return spans->length;
}

PMATH_API pmath_bool_t pmath_span_array_is_token_end(
  pmath_span_array_t *spans, 
  int pos
){
  if(!spans || pos < 0 || pos >= spans->length)
    return FALSE;
  
  return 1 == SPAN_TOK(spans->items[pos]);
}

PMATH_API pmath_bool_t pmath_span_array_is_operand_start(
  pmath_span_array_t *spans, 
  int pos
){
  if(!spans || pos < 0 || pos >= spans->length)
    return FALSE;
  
  return 2 == SPAN_OP(spans->items[pos]);
}

PMATH_API pmath_span_t *pmath_span_at(pmath_span_array_t *spans, int pos){
  if(!spans || pos < 0 || pos >= spans->length)
    return NULL;
  
  return SPAN_PTR(spans->items[pos]);
}

PMATH_API pmath_span_t *pmath_span_next(pmath_span_t *span){
  if(!span)
    return NULL;
  return span->next;
}

PMATH_API int pmath_span_end(pmath_span_t *span){
  if(!span)
    return 0;
  return span->end;
}

//} ... spans

typedef struct scanner_t{
  const uint16_t  *str;
  int              len;
  int              pos;
  uintptr_t       *span_items;
  
  pmath_bool_t in_comment;
  pmath_bool_t in_string;
  pmath_bool_t have_error;
}scanner_t;

typedef struct parser_t{
  pmath_string_t code;
  int fencelevel;
  int stack_size;
  
  
  pmath_span_array_t *spans;
  pmath_string_t    (*read_line)(void*);
  pmath_bool_t      (*subsuperscriptbox_at_index)(int,void*);
  pmath_string_t    (*underoverscriptbox_at_index)(int,void*);
  void              (*error)(pmath_string_t,int,void*,pmath_bool_t);   //does not free 1st arg
  void               *data;
  
  scanner_t tokens;
  
  pmath_bool_t stack_error;
  pmath_bool_t tokenizing;
  pmath_bool_t last_was_newline;
  int          last_space_start;
}parser_t;

//{ parsing ...

static void span(scanner_t *tokens, int start){
  int i, end;
  
  if(!tokens->span_items)
    return;
  
  if(start < 0)
    start = 0;
  
  end = tokens->pos-1;
  while(end > start 
  && pmath_token_analyse(&tokens->str[end], 1, NULL) == PMATH_TOK_SPACE)
    --end;
  
  if(end < start)
    return;
  
  if(SPAN_PTR(tokens->span_items[start]) 
  && SPAN_PTR(tokens->span_items[start])->end >= end)
    return;
    
  for(i = start;i < end;++i)
    if(SPAN_TOK(tokens->span_items[i]))
      goto HAVE_MULTIPLE_TOKENS;
  
  return;
  
 HAVE_MULTIPLE_TOKENS: ;
  {
    pmath_span_t *s = pmath_mem_alloc(sizeof(pmath_span_t));
    if(!s)
      return;
    
    s->end = end;
    s->next = SPAN_PTR(tokens->span_items[start]);
    tokens->span_items[start] = 
      (uintptr_t)s | SPAN_TOK(tokens->span_items[start])
                   | SPAN_OP( tokens->span_items[start]);
  }
}

static void handle_error(parser_t *parser){
  if(/*parser->tokens.have_error || */ parser->tokens.in_comment)
    return;
  
  parser->tokens.have_error = 1;
  if(parser->error)
    parser->error(parser->code, parser->tokens.pos, parser->data, TRUE);
}

static pmath_bool_t enter(parser_t *parser){
  if(parser->stack_error)
    return FALSE;
  if(++parser->stack_size > 256){ // 4096
    --parser->stack_size;// TODO print message
    //parser->have_error = 1;
    return FALSE;
  }
  return TRUE;
}

static void leave(parser_t *parser){
  --parser->stack_size;
}

static void scan_next(scanner_t *tokens, parser_t *parser);

static void parse_sequence(parser_t *parser);
static void parse_prim(    parser_t *parser, pmath_bool_t prim_optional);
static void parse_rest(    parser_t *parser, int lhs_start, int min_prec);
static void parse_textline(parser_t *parser);

static int next_token_pos(parser_t *parser){
  int next;
  struct _pmath_span_t *span;
  
  if(parser->tokens.pos == parser->tokens.len)
    return parser->tokens.pos;
  
  span = SPAN_PTR(parser->spans->items[parser->tokens.pos]);
  
  if(span)
    return span->end + 1;
  
  next = parser->tokens.pos;
  while(next < parser->tokens.len
  && !pmath_span_array_is_token_end(parser->spans, next)){
    ++next;
  }
  
  if(next < parser->tokens.len)
    ++next;
  
  return next;
}

static void skip_to(parser_t *parser, int span_start, int next, pmath_bool_t optional);

static pmath_bool_t read_more(parser_t *parser){
  pmath_string_t newline;
  int extralen;
  
  if(!parser)
    return FALSE;
  
  if(!parser->read_line){
    handle_error(parser);
    return FALSE;
  }
  
  newline = parser->read_line(parser->data);
        
  if(pmath_is_null(newline)){
    handle_error(parser);
    return FALSE;
  }
  
  extralen = 1 + pmath_string_length(newline);
  parser->spans = enlarge_span_array(parser->spans, extralen);
  if(!parser->spans
  || parser->spans->length != parser->tokens.len + extralen){
    parser->tokens.span_items = NULL;
    pmath_unref(newline);
    handle_error(parser);
    return FALSE;
  }
  
  parser->code = pmath_string_insert_latin1(parser->code, INT_MAX, "\n", 1);
  parser->code = pmath_string_concat(parser->code, newline);
  
  if(parser->spans->length != pmath_string_length(parser->code)){
    pmath_unref(newline);
    handle_error(parser);
    return FALSE;
  }
  
  parser->tokens.str = pmath_string_buffer(&parser->code);
  parser->tokens.len = pmath_string_length(parser->code);
  parser->tokens.span_items = parser->spans->items;
  return TRUE;
}

static void skip_space(parser_t *parser, int span_start, pmath_bool_t optional){
  parser->last_was_newline = FALSE;
  
  if(parser->tokens.pos < parser->tokens.len 
  && parser->tokens.str[parser->tokens.pos] == PMATH_CHAR_BOX
  && parser->subsuperscriptbox_at_index
  && parser->subsuperscriptbox_at_index(parser->tokens.pos, parser->data)){
    if(span_start >= 0)
      span(&parser->tokens, span_start);
    ++parser->tokens.pos;
  }
  
  parser->last_space_start = parser->tokens.pos;
  for(;;){
    pmath_token_t tok;
  
    while(parser->tokens.pos < parser->tokens.len
    && parser->tokens.str[parser->tokens.pos] != '\n'){
      tok = pmath_token_analyse(parser->tokens.str + parser->tokens.pos, 1, NULL);
      
      if(tok != PMATH_TOK_SPACE)
        break;
      
      ++parser->tokens.pos;
    }
    
    if(parser->tokens.pos < parser->tokens.len){
      struct _pmath_span_t *span;
      
      span = SPAN_PTR(parser->spans->items[parser->tokens.pos]);
      
      if(span){
        if(parser->tokens.pos + 1 < parser->tokens.len 
        && parser->tokens.str[parser->tokens.pos]   == '/'
        && parser->tokens.str[parser->tokens.pos+1] == '*'){
          parser->tokens.pos = span->end + 1;
          continue;
        }
        else
          break;
      }
    }
    
    if(parser->tokens.pos < parser->tokens.len 
    && parser->tokens.str[parser->tokens.pos] == '\n'){
      if(!optional || parser->fencelevel > 0){
        ++parser->tokens.pos;
        parser->last_was_newline = TRUE;
        continue;
      }
      
      break;
    }
    
    if(parser->tokens.pos < parser->tokens.len 
    && parser->tokens.str[parser->tokens.pos] == '\\'){
      int i = parser->tokens.pos + 1;
      
      while(i < parser->tokens.len
      && parser->tokens.str[i] <= ' ')
        ++i;
      
      if(i == parser->tokens.len){
        parser->tokens.pos = i;
        optional = FALSE;
      }
    }
    
    if(parser->tokens.pos + 1 < parser->tokens.len 
    && !parser->tokens.in_comment
    && parser->tokens.str[parser->tokens.pos] == '/'
    && parser->tokens.str[parser->tokens.pos+1] == '*'){
      pmath_bool_t last_was_newline = FALSE;
      int last_space_start = parser->last_space_start;
      
      int start = parser->tokens.pos;
      
      skip_to(parser, -1, next_token_pos(parser), FALSE);
      parser->tokens.in_comment = TRUE;
      ++parser->fencelevel;
      while(parser->tokens.pos < parser->tokens.len){
        int tmp;
        
        if(parser->tokens.pos + 1 < parser->tokens.len
        && parser->tokens.str[parser->tokens.pos]   == '*'
        && parser->tokens.str[parser->tokens.pos+1] == '/'){
          parser->tokens.pos+= 2;
          break;
        }
        
        tmp = parser->tokens.pos;
        parse_sequence(parser);
        if(tmp == parser->tokens.pos)
          ++parser->tokens.pos;
      }
      span(&parser->tokens, start);
      --parser->fencelevel;
      parser->tokens.in_comment = FALSE;
      
      parser->last_was_newline = last_was_newline;
      parser->last_space_start = last_space_start;
    }
    else if(parser->tokens.pos == parser->tokens.len 
    && (!optional || parser->fencelevel > 0)
    && !pmath_aborting()){
      if(!read_more(parser))
        break;
        
      if(!parser->tokenizing){
        int          old_pos        = parser->tokens.pos;
        pmath_bool_t old_in_comment = parser->tokens.in_comment;
        pmath_bool_t old_in_string  = parser->tokens.in_string;
        
        parser->stack_error = FALSE;
        parser->tokens.have_error = FALSE;
        parser->tokenizing = TRUE;
        
        while(parser->tokens.pos < parser->tokens.len)
          scan_next(&parser->tokens, parser);
          
        parser->tokens.pos        = old_pos;
        parser->tokens.in_comment = old_in_comment;
        parser->tokens.in_string  = old_in_string;
        parser->tokenizing = FALSE;
      }
    }
    else break;
  }
}

static void skip_to(parser_t *parser, int span_start, int next, pmath_bool_t optional){
  parser->tokens.pos = next;
  skip_space(parser, span_start, optional);
}

static void scan_next(scanner_t *tokens, parser_t *parser){
  assert(!parser || &parser->tokens == tokens);
  
  if(tokens->pos >= tokens->len)
    return;
    
  switch(tokens->str[tokens->pos]){
    case '#': {
      ++tokens->pos;
      
      if(tokens->pos == tokens->len)
        break;
      
      if(tokens->str[tokens->pos] == '#'){ // ##
        ++tokens->pos;
        break;
      }
    } break;
    
    case '+': { // ++ += 
      ++tokens->pos;
      
      if(tokens->pos == tokens->len)
        break;
      
      if(tokens->str[tokens->pos] == '+'   // ++
      || tokens->str[tokens->pos] == '='){ // +=
        ++tokens->pos;
        break;
      }
    } break;
      
    case '-': {
      ++tokens->pos;
      
      if(tokens->pos == tokens->len)
        break;
      
      if(tokens->str[tokens->pos] == '-'   // --
      || tokens->str[tokens->pos] == '='   // -=
      || tokens->str[tokens->pos] == '>'){ // ->
        ++tokens->pos;
        break;
      }
    } break;
    
    case '*': {
      ++tokens->pos;
      
      if(tokens->pos == tokens->len)
        break;
      
      if(tokens->str[tokens->pos] == '='){ // *=
        ++tokens->pos;
        break;
      }
      
      if(tokens->str[tokens->pos] == '*'){
        ++tokens->pos;
        
        if(tokens->pos == tokens->len) // **
          break;
        
        if(tokens->str[tokens->pos] == '*'){
          ++tokens->pos;
          
          if(tokens->in_comment 
          && tokens->pos < tokens->len 
          && tokens->str[tokens->pos] == '/'){ // ***/   ===   ** */
            tokens->in_comment = 0;
            if(tokens->span_items)
              tokens->span_items[tokens->pos-2] = 1;
            ++tokens->pos;
            break;
          }
          
          break; // ***
        }
        
        if(tokens->in_comment 
        && tokens->str[tokens->pos] == '/'){ // **/   ===   * */
          tokens->in_comment = 0;
          if(tokens->span_items)
            tokens->span_items[tokens->pos-2] = 1;
          ++tokens->pos;
          break;
        }
        
        break; // **
      }
      
      if(tokens->in_comment
      && tokens->str[tokens->pos] == '/'){ // */
        tokens->in_comment = 0;
        ++tokens->pos;
        break;
      }
      
    } break;
    
    case '/': {
      ++tokens->pos;
      
      if(tokens->pos == tokens->len)
        break;
      
      if(!tokens->in_comment 
      && !tokens->in_string
      && tokens->str[tokens->pos] == '*'){ // /*
        tokens->in_comment = 1;
        ++tokens->pos;
        break;
      }
      
      if(tokens->str[tokens->pos] == '='   // /=
      || tokens->str[tokens->pos] == '?'   // /?
      || tokens->str[tokens->pos] == '@'   // /@
      || tokens->str[tokens->pos] == '.'   // /.
      || tokens->str[tokens->pos] == ':'){ // /:
        ++tokens->pos;
        break;
      }
      
      if(tokens->str[tokens->pos] == '/'){
        ++tokens->pos;
        
        if(tokens->pos == tokens->len) // //
          break;
        
        if(tokens->str[tokens->pos] == '@'   // //@
        || tokens->str[tokens->pos] == '.'){ // //.
          ++tokens->pos;
          break;
        }
        
        break;
      }
      
      if(tokens->pos + 1 < tokens->len 
      && tokens->str[tokens->pos]     == '\\'
      && tokens->str[tokens->pos + 1] == '/')
        tokens->pos+= 2;
      
    } break;
    
    case '\\': {
      ++tokens->pos;
      
      if(tokens->pos == tokens->len){
        read_more(parser);
        break;
      }
      
      if(tokens->str[tokens->pos] == 'x'){
        int start = tokens->pos;
        ++tokens->pos;
        
        if(tokens->pos >= tokens->len 
        || !pmath_char_is_hexdigit(tokens->str[tokens->pos])){
          if(parser)
            handle_error(parser);
          tokens->pos = start;
          break;
        }
        
        ++tokens->pos;
        if(tokens->pos >= tokens->len 
        || !pmath_char_is_hexdigit(tokens->str[tokens->pos])){
          if(parser)
            handle_error(parser);
          tokens->pos = start;
          break;
        }
        
        ++tokens->pos;
        break;
      }
      
      if(tokens->str[tokens->pos] == 'u'){
        int start = tokens->pos;
        int count;
        
        for(count = 4;count > 0;--count){
          ++tokens->pos;
          if(tokens->pos >= tokens->len 
          || !pmath_char_is_hexdigit(tokens->str[tokens->pos])){
            if(parser)
              handle_error(parser);
            tokens->pos = start;
            break;
          }
        }
        
        if(count > 0)
          break;
        
        ++tokens->pos;
        break;
      }
      
      if(tokens->str[tokens->pos] == 'U'){
        int start = tokens->pos;
        int count;
        
        for(count = 8;count > 0;--count){
          ++tokens->pos;
          if(tokens->pos >= tokens->len 
          || !pmath_char_is_hexdigit(tokens->str[tokens->pos])){
            if(parser)
              handle_error(parser);
            tokens->pos = start;
            break;
          }
        }
        
        if(count > 0)
          break;
        
        ++tokens->pos;
        break;
      }
      
      if(tokens->str[tokens->pos] == '"' 
      || tokens->str[tokens->pos] == '\\'){
        ++tokens->pos;
        break;
      }
    } break;
    
    case '<':
    case '>':
    case '!': {
      ++tokens->pos;
      
      if(tokens->pos == tokens->len)
        break;
      
      if(tokens->str[tokens->pos] == '='                            // <= >= !=
      || tokens->str[tokens->pos] == tokens->str[tokens->pos - 1]){ // << >> !!
        ++tokens->pos;
        break;
      }
    } break;
    
    case '=': {
      ++tokens->pos;
      
      if(tokens->pos + 1 < tokens->len 
      && (tokens->str[tokens->pos] == '!'
       || tokens->str[tokens->pos] == '=')
      && tokens->str[tokens->pos+1] == '='){
        tokens->pos+= 2;
      }
    } break;
    
    case '~': {
      ++tokens->pos;
      
      if(tokens->pos < tokens->len
      && tokens->str[tokens->pos] == '~'){
        ++tokens->pos;
        
        if(tokens->pos < tokens->len
        && tokens->str[tokens->pos] == '~')
          ++tokens->pos;
      }
    } break;
    
    case ':': {
      ++tokens->pos;
      
      if(tokens->pos == tokens->len)
        break;
      
      if(tokens->str[tokens->pos] == ':'){
        ++tokens->pos;
          
        if(tokens->pos == tokens->len) // ::
          break;
        
        if(tokens->str[tokens->pos] == '='){ // ::=
          ++tokens->pos;
          break;
        }
        
        break; // ::
      }
      
      if(tokens->str[tokens->pos] == '='   // :=
      || tokens->str[tokens->pos] == '>'){ // :>
        ++tokens->pos;
        break;
      }
    } break;
    
    case '?':
    case '|':
    case '&':{
      ++tokens->pos;
      
      if(tokens->pos < tokens->len
      && tokens->str[tokens->pos] == tokens->str[tokens->pos-1]) // ?? || &&
        ++tokens->pos;
    } break;
    
    //case '\'':
    case '@':
    case '%': 
    case '.': {
      ++tokens->pos;
      while(tokens->pos < tokens->len
      && tokens->str[tokens->pos] == tokens->str[tokens->pos-1])
        ++tokens->pos;
    } break;
    
    case '`': {
      ++tokens->pos;
      while(tokens->pos < tokens->len
      && pmath_char_is_digit(tokens->str[tokens->pos]))
        ++tokens->pos;
        
      if(tokens->pos < tokens->len
      && tokens->str[tokens->pos] == '`')
        ++tokens->pos;
    } break;
    
    case '"': {
      if(!tokens->in_string){
        int k = 0;
        int start = tokens->pos;
        tokens->in_string = TRUE;
        
        ++tokens->pos;
        for(;;){
          while(tokens->pos < tokens->len
          && (k > 0 || tokens->str[tokens->pos] != '"')){
            if(tokens->str[tokens->pos] == PMATH_CHAR_LEFT_BOX){
              ++k;
              tokens->pos++;
            }
            else if(tokens->str[tokens->pos] == PMATH_CHAR_RIGHT_BOX){
              --k;
              tokens->pos++;
            }
            else
              scan_next(tokens, parser);
          }
          
          if(tokens->pos < tokens->len)
            break;
          
          if(!read_more(parser)){
            if(tokens->span_items)
              tokens->span_items[start] |= 3;
              
            span(tokens, start);
            goto END_SCAN;
          }
        }
        
        if(tokens->pos < tokens->len
        && tokens->str[tokens->pos] == '"')
          scan_next(tokens, parser);
        else if(parser)
          handle_error(parser);
        tokens->in_string = FALSE;
        
        if(tokens->span_items)
          tokens->span_items[start] |= 3;
          
        span(tokens, start);
      }
      else
        ++tokens->pos;
    } break;
    
    default: {
      pmath_token_t tok;
      
      tok = pmath_token_analyse(tokens->str + tokens->pos, 1, NULL);
      if(tok == PMATH_TOK_NAME){
        ++tokens->pos;
        while(tokens->pos < tokens->len){
          if(tokens->str[tokens->pos] == '`'){
            if(tokens->pos + 1 == tokens->len)
              break;
              
            ++tokens->pos;
            tok = pmath_token_analyse(tokens->str + tokens->pos, 1, NULL);
            if(tok != PMATH_TOK_NAME)
              break;
            
            ++tokens->pos;
            continue;
          }
          
          tok = pmath_token_analyse(tokens->str + tokens->pos, 1, NULL);
          if(tok != PMATH_TOK_NAME 
          && tok != PMATH_TOK_DIGIT)
            break;
          
          ++tokens->pos;
        }
        
        break;
      }
      
      if(tok == PMATH_TOK_DIGIT){
        do{
          ++tokens->pos;
        }while(tokens->pos < tokens->len
        && pmath_char_is_digit(tokens->str[tokens->pos]));
        
        if(tokens->pos + 2 < tokens->len
        && tokens->str[tokens->pos]   == '^'
        && tokens->str[tokens->pos+1] == '^'
        && pmath_char_is_36digit(tokens->str[tokens->pos+2])){
          tokens->pos+= 3;
          while(tokens->pos < tokens->len
          && pmath_char_is_36digit(tokens->str[tokens->pos]))
            ++tokens->pos;
          
          if(tokens->pos + 1 < tokens->len
          && tokens->str[tokens->pos] == '.'
          && pmath_char_is_36digit(tokens->str[tokens->pos+1])){
            tokens->pos+= 3;
            while(tokens->pos < tokens->len
            && pmath_char_is_36digit(tokens->str[tokens->pos]))
              ++tokens->pos;
          }
        }
        else if(tokens->pos + 1 < tokens->len
        && tokens->str[tokens->pos] == '.'
        && pmath_char_is_digit(tokens->str[tokens->pos+1])){
          tokens->pos+= 2;
          while(tokens->pos < tokens->len
          && pmath_char_is_digit(tokens->str[tokens->pos]))
            ++tokens->pos;
        }
        
        if(tokens->pos < tokens->len
        && tokens->str[tokens->pos] == '`'){
          ++tokens->pos;
          
          if(tokens->pos < tokens->len
          && tokens->str[tokens->pos] == '`'){
            ++tokens->pos;
          }
          
          if(tokens->pos + 1 < tokens->len
          && (tokens->str[tokens->pos] == '+'
           || tokens->str[tokens->pos] == '-')
          && pmath_char_is_digit(tokens->str[tokens->pos + 1])){
            ++tokens->pos;
          }
            
          if(pmath_char_is_digit(tokens->str[tokens->pos])){
            ++tokens->pos;
            while(tokens->pos < tokens->len
            && pmath_char_is_digit(tokens->str[tokens->pos]))
              ++tokens->pos;
            
            if(tokens->pos + 1 < tokens->len
            && tokens->str[tokens->pos] == '.'
            && pmath_char_is_digit(tokens->str[tokens->pos+1])){
              tokens->pos+= 2;
              while(tokens->pos < tokens->len
              && pmath_char_is_digit(tokens->str[tokens->pos]))
                ++tokens->pos;
            }
          }
        }
        
        if(tokens->pos + 2 < tokens->len
        && tokens->str[tokens->pos]   == '*'
        && tokens->str[tokens->pos+1] == '^'
        && (pmath_char_is_digit(tokens->str[tokens->pos+2])
         || (tokens->pos + 3 < tokens->len
          && (tokens->str[tokens->pos+2] == '-'
           || tokens->str[tokens->pos+2] == '+')
          && pmath_char_is_digit(tokens->str[tokens->pos+3])))){
          tokens->pos+= 3;
          while(tokens->pos < tokens->len
          && pmath_char_is_digit(tokens->str[tokens->pos]))
            ++tokens->pos;
        }
      }
      else
        ++tokens->pos;
    }
  }
  
 END_SCAN:
  if(tokens->span_items)
    tokens->span_items[tokens->pos-1] = 1;
}

PMATH_API pmath_span_array_t *pmath_spans_from_string(
  pmath_string_t   *code,
  pmath_string_t  (*line_reader)(void*),
  pmath_bool_t    (*subsuperscriptbox_at_index)(int,void*),
  pmath_string_t  (*underoverscriptbox_at_index)(int,void*),
  void            (*error)(  pmath_string_t,int,void*,pmath_bool_t), //does not free 1st arg
  void             *data
){
  parser_t parser;
  
  parser.code = *code;
  parser.tokens.str = pmath_string_buffer(&parser.code);
  parser.tokens.len = pmath_string_length(parser.code);
  parser.tokens.pos = 0;
  parser.fencelevel = 0;
  parser.stack_size = 0;
  parser.spans = create_span_array(parser.tokens.len);
  
  if(!parser.spans)
    return NULL;
    
  parser.read_line                   = line_reader;
  parser.subsuperscriptbox_at_index  = subsuperscriptbox_at_index;
  parser.underoverscriptbox_at_index = underoverscriptbox_at_index;
  parser.error                       = error;
  parser.data                        = data;
  parser.tokens.span_items           = parser.spans->items;
  parser.tokens.in_comment           = FALSE;
  parser.tokens.in_string            = FALSE;
  parser.tokens.have_error           = FALSE;
  parser.stack_error                 = FALSE;
  parser.last_was_newline            = FALSE;
  parser.last_space_start            = 0;
  
  if(!parser.spans)
    return NULL;
  
  
  parser.tokenizing = TRUE;
  while(parser.tokens.pos < parser.tokens.len)
    scan_next(&parser.tokens, &parser);
  parser.tokens.pos = 0;
  parser.tokens.in_comment = FALSE;
  parser.tokens.in_string  = FALSE;
  parser.tokenizing = FALSE;
  
  if(parser.tokens.have_error){
    skip_space(&parser, -1, FALSE);
  }
  else{
    parser.tokens.have_error = TRUE;
    skip_space(&parser, -1, FALSE);
    parser.tokens.have_error = FALSE;
  }
  
  while(parser.tokens.pos < parser.tokens.len){
    int tmp = parser.tokens.pos;
    parse_sequence(&parser);
    
    if(parser.tokens.pos < parser.tokens.len){
      if(parser.tokens.str[parser.tokens.pos] == '\n')
        skip_to(&parser, -1, next_token_pos(&parser), FALSE);
      else
        handle_error(&parser);
    }
      
    if(tmp == parser.tokens.pos)
      skip_to(&parser, -1, next_token_pos(&parser), FALSE);
  }
  
  *code = parser.code;
  return parser.spans;
}

static pmath_token_t token_analyse(
  parser_t *parser, 
  int       next,
  int      *prec
){
  pmath_token_t tok = pmath_token_analyse(
    parser->tokens.str + parser->tokens.pos, 
    next - parser->tokens.pos, 
    prec);
  
  if(tok == PMATH_TOK_NAME2
  && parser->tokens.str[parser->tokens.pos] == PMATH_CHAR_BOX
  && parser->underoverscriptbox_at_index){
    pmath_string_t str = parser->underoverscriptbox_at_index(parser->tokens.pos, parser->data);
    
    if(!pmath_is_null(str)){
      tok = pmath_token_analyse(
        pmath_string_buffer(&str),
        pmath_string_length(str),
        prec);
      
      pmath_unref(str);
      
      if(tok == PMATH_TOK_NONE)
        return PMATH_TOK_NAME2;
    }
  }
  
  return tok;
}

static int prefix_precedence(
  parser_t *parser, 
  int       next,
  int       defprec
){
  return pmath_token_prefix_precedence(
    parser->tokens.str + parser->tokens.pos, 
    next - parser->tokens.pos, 
    defprec);
}

static pmath_bool_t same_token(
  parser_t *parser, 
  int       last_start, 
  int       last_next, 
  int       cur_next
){
  const uint16_t *buf = parser->tokens.str;
  int cur_start = parser->tokens.pos;
  int cur_len   = cur_next  - cur_start;
  int last_len  = last_next - last_start;
  
  if(last_len == cur_len){
    int i;
    
    for(i = 0;i < cur_len;++i)
      if(buf[cur_start + i] != buf[last_start + i])
        return FALSE;
    
    return TRUE;
  }
  
  return FALSE;
}

static pmath_bool_t plusplus_is_infix(parser_t *parser, int next){
  pmath_bool_t oldnl  = parser->last_was_newline;
  int          oldss  = parser->last_space_start;
  int          oldpos = parser->tokens.pos;
  pmath_bool_t result;
  
  skip_to(parser, -1, next, TRUE);
  next = next_token_pos(parser);
  
  result = next > parser->tokens.pos
    && pmath_token_maybe_first(token_analyse(parser, next, NULL));
  
  parser->last_was_newline = oldnl;
  parser->last_space_start = oldss;
  parser->tokens.pos       = oldpos;
  return result;
}

static void parse_sequence(parser_t *parser){
  pmath_token_t tok;
  int last;
  int start;
  int next;
  
  if(!enter(parser))
    return;
  
  start = last = parser->tokens.pos;
  next  = next_token_pos(parser);
  
  while(next != parser->tokens.pos){
    tok = token_analyse(parser, next, NULL);
    
    if(tok == PMATH_TOK_RIGHT
    || tok == PMATH_TOK_COMMENTEND
    || tok == PMATH_TOK_SPACE)
      break;
    
    parse_prim(parser, FALSE);
    parse_rest(parser, start, PMATH_PREC_ANY);
    
    if(parser->tokens.pos == last){
      skip_to(parser, -1, next, TRUE);
    }
    
    last = parser->tokens.pos;
    next = next_token_pos(parser);
  }
  
  span(&parser->tokens, start);
  leave(parser);
}

static void parse_prim(parser_t *parser, pmath_bool_t prim_optional){
  pmath_token_t tok;
  int prec;
  int start;
  int next;
  
  start = parser->tokens.pos;
  next  = next_token_pos(parser);
  
  if(next == parser->tokens.pos){
    if(!prim_optional)
      handle_error(parser);
    return;
  }
  
  if(!enter(parser))
    return;
  
  tok = token_analyse(parser, next, &prec);
  
  switch(tok){
    case PMATH_TOK_DIGIT: 
    case PMATH_TOK_STRING:
    case PMATH_TOK_NAME: 
    case PMATH_TOK_NAME2: {
      parser->spans->items[start] |= 2; //operand start
      skip_to(parser, start, next, TRUE);
      
      next = next_token_pos(parser);
      tok  = token_analyse(parser, next, &prec);
      
      if(tok == PMATH_TOK_COLON){ // x:pattern
        skip_to(parser, -1, next, FALSE);
        next = parser->tokens.pos;
        parse_prim(parser, FALSE);
        parse_rest(parser, next, PMATH_PREC_ALT);
      }
    } break;
    
    case PMATH_TOK_CALL: { // no error:  "a:=."
      parser->spans->items[start] |= 2; //operand start
      skip_to(parser, start, next, TRUE);
    } break;
    
    case PMATH_TOK_TILDES: {
      parser->spans->items[start] |= 2; //operand start
      skip_to(parser, start, next, TRUE);
      
      next = next_token_pos(parser);
      tok  = token_analyse(parser, next, &prec);
      
      if(tok == PMATH_TOK_NAME){
        skip_to(parser, start, next, TRUE);
        
        next = next_token_pos(parser);
        tok = token_analyse(parser, next, &prec);
      }
      
      if(tok == PMATH_TOK_COLON){ // ~x:type  ~:type
        skip_to(parser, -1, next, FALSE);
        parse_prim(parser, FALSE);
      }
    } break;
    
    case PMATH_TOK_SLOT: {
      parser->spans->items[start] |= 2; //operand start
      skip_to(parser, start, next, TRUE);
      
      next = next_token_pos(parser);
      tok  = token_analyse(parser, next, &prec);
      
      if(tok == PMATH_TOK_DIGIT){
        skip_to(parser, start, next, TRUE);
        
        next = next_token_pos(parser);
        tok = token_analyse(parser, next, &prec);
      }
    } break;
    
    case PMATH_TOK_QUESTION: {
      parser->spans->items[start] |= 2; //operand start
      skip_to(parser, start, next, FALSE);
      
      next = next_token_pos(parser);
      tok  = token_analyse(parser, next, &prec);
      if(tok == PMATH_TOK_NAME){
        skip_to(parser, start, next, TRUE);
        
        next = next_token_pos(parser);
        tok  = token_analyse(parser, next, &prec);
      }
      else if(tok != PMATH_TOK_COLON){
        handle_error(parser);
      }
      
      if(tok == PMATH_TOK_COLON){ // ?x:type  ?:type
        skip_to(parser, -1, next, FALSE);
        next = parser->tokens.pos;
        parse_prim(parser, FALSE);
        parse_rest(parser, next, PMATH_PREC_CIRCMUL);
      }
    } break;
    
    case PMATH_TOK_LEFT:
    case PMATH_TOK_LEFTCALL: {
      int next2;
      pmath_token_t tok2;
      
      parser->spans->items[start] |= 2; //operand start
      skip_to(parser, -1, next, FALSE);
      
      next = parser->tokens.pos;
      
      next2 = next_token_pos(parser);
      tok2  = token_analyse(parser, next2, &prec);
      
      ++parser->fencelevel;
      
      if(tok2 != PMATH_TOK_RIGHT){
        
        parse_prim(parser, FALSE);
        parse_rest(parser, next, PMATH_PREC_ANY);
        
        next2 = next_token_pos(parser);
        tok2  = token_analyse(parser, next2, &prec);
      }
      
      --parser->fencelevel;
      
      if(tok2 == PMATH_TOK_RIGHT)
        skip_to(parser, start, next2, TRUE);
      else
        handle_error(parser);
    } break;
    
    case PMATH_TOK_BINARY_LEFT_AUTOARG:
    case PMATH_TOK_NARY_AUTOARG:
      break;
    
    case PMATH_TOK_PLUSPLUS: 
      prec = PMATH_PREC_INC; // +1
      goto DO_PREFIX;
    
    case PMATH_TOK_BINARY_LEFT_OR_PREFIX:
    case PMATH_TOK_NARY_OR_PREFIX:
    case PMATH_TOK_POSTFIX_OR_PREFIX:
      prec = prefix_precedence(parser, next, prec);
      goto DO_PREFIX;
      
    case PMATH_TOK_PREFIX: 
    case PMATH_TOK_INTEGRAL: { DO_PREFIX:
      parser->spans->items[start] |= 2; //operand start
      skip_to(parser, -1, next, FALSE);
      next = parser->tokens.pos;
      parse_prim(parser, FALSE);
      parse_rest(parser, next, prec);
    } break;
    
    case PMATH_TOK_PRETEXT: {
      parser->spans->items[start] |= 2; //operand start
      skip_to(parser, -1, next, FALSE);
      parse_textline(parser);
    } break;
    
    default:
      if(!prim_optional)
        handle_error(parser);
      
      break;
  }

  span(&parser->tokens, start);
  leave(parser);
}

static void parse_rest(parser_t *parser, int lhs, int min_prec){
  int last_tok_start = lhs;
  int last_tok_end   = lhs;
  int last_prec      = -1;
  int cur_prec;
  pmath_token_t tok;
  int rhs;
  int next;
  
  if(!enter(parser))
    return;
  
  next = next_token_pos(parser);
  while(next != parser->tokens.pos){
    tok = token_analyse(parser, next, &cur_prec);
    
    switch(tok){
      case PMATH_TOK_POSTFIX:
      case PMATH_TOK_POSTFIX_OR_PREFIX: {
        if(cur_prec >= min_prec){
          span(&parser->tokens, lhs);
          
          last_tok_start = parser->tokens.pos;
          last_tok_end   = next;
          last_prec      = cur_prec;
          skip_to(parser, lhs, next, TRUE);
          next = next_token_pos(parser);
          continue;
        }
      } break;
      
      case PMATH_TOK_PLUSPLUS: {
        if(plusplus_is_infix(parser, next)) // x++y
          goto NARY;
        
        if(PMATH_PREC_INC >= min_prec){ // x++
          span(&parser->tokens, lhs);
          
          last_tok_start = parser->tokens.pos;
          last_tok_end   = next;
          last_prec      = PMATH_PREC_INC;
          skip_to(parser, lhs, next, TRUE);
          next = next_token_pos(parser);
          continue;
        }
      } break;
      
      case PMATH_TOK_ASSIGNTAG: {
        int rhs;
        int prec;
        
        span(&parser->tokens, lhs);
        
        skip_to(parser, -1, next, FALSE);
        rhs = parser->tokens.pos;
        
        parse_prim(parser, FALSE);
        parse_rest(parser, rhs, PMATH_PREC_ASS + 1);
        
        next = next_token_pos(parser);
        token_analyse(parser, next, &prec);
        if(prec != PMATH_PREC_ASS){
          handle_error(parser);
        }
        else{
          skip_to(parser, -1, next, FALSE);
          rhs = parser->tokens.pos;
          
          parse_prim(parser, FALSE);
          parse_rest(parser, rhs, PMATH_PREC_ASS);
        }
        
        span(&parser->tokens, lhs);
      } break;
      
      case PMATH_TOK_CALL: {
        if(cur_prec < min_prec)
          break;
        
        span(&parser->tokens, lhs);
        
        last_tok_start = parser->tokens.pos;
        last_tok_end   = next;
        
        skip_to(parser, lhs, next, FALSE);
        parse_prim(parser, FALSE);
        
        last_prec = cur_prec;
        
        next = next_token_pos(parser);
        tok  = token_analyse(parser, next, &cur_prec);
//        if(tok != PMATH_TOK_LEFTCALL 
//        || parser->last_space_start < parser->tokens.pos) // preceding space
//          span(&parser->tokens, lhs);
      } continue;
      
      case PMATH_TOK_LEFTCALL: 
        if(parser->last_space_start == parser->tokens.pos){ // no preceding space
          pmath_bool_t doublesq = FALSE;
          
          if(PMATH_PREC_CALL < min_prec)
            break;
          
          if(last_prec == cur_prec){
            if(cur_prec != PMATH_PREC_OR 
            && cur_prec != PMATH_PREC_AND 
            && cur_prec != PMATH_PREC_REL 
            && cur_prec != PMATH_PREC_ADD
            && cur_prec != PMATH_PREC_MUL
            && !same_token(parser, last_tok_start, last_tok_end, next)
            && (last_tok_start + 1 != last_tok_end
             || parser->tokens.str[last_tok_start] != '.'))
              span(&parser->tokens, lhs);
          }
          else if(last_prec >= 0)
            span(&parser->tokens, lhs);
          
          last_tok_start = parser->tokens.pos;
          last_tok_end   = next;
          
          ++parser->fencelevel;
          
          if(next == parser->tokens.pos + 1
          && next < parser->tokens.len
          && parser->tokens.str[parser->tokens.pos] == '['
          && parser->tokens.str[next]               == '['){ // [[
            parser->spans->items[parser->tokens.pos]&= ~1; // no token end
            doublesq = TRUE;
            ++next;
          }
          
          skip_to(parser, -1, next, FALSE);
          next = next_token_pos(parser);
          if(token_analyse(parser, next, &cur_prec) != PMATH_TOK_RIGHT)
            parse_sequence(parser);
          
          next = next_token_pos(parser);
          
          --parser->fencelevel;
          
          if(token_analyse(parser, next, &cur_prec) == PMATH_TOK_RIGHT){
            if(doublesq){
              if(next == parser->tokens.pos + 1
              && next < parser->tokens.len
              && parser->tokens.str[parser->tokens.pos] == ']'
              && parser->tokens.str[next]               == ']'){
                parser->spans->items[parser->tokens.pos]&= ~1; // no token end
                ++next;
              }
              else
                handle_error(parser);
            }
            
            skip_to(parser, lhs, next, TRUE);
          }
          else
            handle_error(parser);
          
          last_prec = cur_prec;
          span(&parser->tokens, lhs);
          next = next_token_pos(parser);
          continue;
        } 
        /* no break */
        
      case PMATH_TOK_TILDES: 
      case PMATH_TOK_DIGIT: 
      case PMATH_TOK_STRING:
      case PMATH_TOK_NAME: 
      case PMATH_TOK_NAME2: 
      case PMATH_TOK_PREFIX: 
      case PMATH_TOK_PRETEXT: 
      case PMATH_TOK_LEFT: 
      case PMATH_TOK_SLOT: 
      case PMATH_TOK_INTEGRAL: {
        if(PMATH_PREC_MUL < min_prec)
          break;
          
        cur_prec = PMATH_PREC_MUL;
        tok      = PMATH_TOK_NARY;
        next     = parser->tokens.pos;
      } goto NARY;
        
      case PMATH_TOK_BINARY_LEFT_AUTOARG: 
      case PMATH_TOK_BINARY_LEFT_OR_PREFIX: 
      case PMATH_TOK_NARY_AUTOARG: {
        pmath_token_t tok2;
        int           next2;
        pmath_bool_t  oldnl  = parser->last_was_newline;
        int           oldss  = parser->last_space_start;
        int           oldpos = parser->tokens.pos;
        
        if(cur_prec < min_prec)
          break;
        
        skip_to(parser, -1, next, TRUE);
        
        next2 = next_token_pos(parser);
        tok2  = token_analyse(parser, next2, NULL);
        if(!pmath_token_maybe_first(tok2)){
          if(tok == PMATH_TOK_BINARY_LEFT_AUTOARG
          || tok == PMATH_TOK_NARY_AUTOARG){
            span(&parser->tokens, lhs);
          }
          else{
            next               = parser->tokens.pos;
            parser->tokens.pos = oldpos;
            span(&parser->tokens, lhs);
            skip_to(parser, lhs, next, TRUE);
          }
          
          next           = next2;
          last_tok_start = parser->tokens.pos;
          last_tok_end   = next;
          last_prec      = cur_prec;
          continue;
        }
        
        parser->last_was_newline = oldnl;
        parser->last_space_start = oldss;
        parser->tokens.pos       = oldpos;
        if(tok == PMATH_TOK_BINARY_LEFT_AUTOARG){
          span(&parser->tokens, lhs);
          goto BINARY;
        }
      } goto NARY;
      
      case PMATH_TOK_QUESTION: 
      case PMATH_TOK_BINARY_LEFT: {
        if(cur_prec < min_prec)
          break;
        
        span(&parser->tokens, lhs);
      } goto BINARY;
        
      case PMATH_TOK_BINARY_RIGHT: 
      case PMATH_TOK_NARY: 
      case PMATH_TOK_NARY_OR_PREFIX: {
        if(cur_prec >= min_prec){
          int oldpos;
         NARY:
          
          if(last_prec == cur_prec){
            if(cur_prec != PMATH_PREC_OR 
            && cur_prec != PMATH_PREC_AND 
            && cur_prec != PMATH_PREC_REL 
            && cur_prec != PMATH_PREC_ADD
            && cur_prec != PMATH_PREC_MUL
            && !same_token(parser, last_tok_start, last_tok_end, next))
              span(&parser->tokens, lhs);
          }
          else //if(last_prec >= 0)
            span(&parser->tokens, lhs);
          
         BINARY:
         
          last_tok_start = parser->tokens.pos;
          last_tok_end   = next;
          
          skip_to(parser, -1, next, 
               tok == PMATH_TOK_NARY_AUTOARG 
            || tok == PMATH_TOK_BINARY_LEFT_AUTOARG);
          
          rhs = parser->tokens.pos;
          parse_prim(
            parser,
               tok == PMATH_TOK_NARY_AUTOARG 
            || tok == PMATH_TOK_BINARY_LEFT_AUTOARG);
          
          next = next_token_pos(parser);
          oldpos = parser->tokens.pos;
          while(oldpos != next){
            int next_prec;
            tok = token_analyse(parser, next, &next_prec);
            oldpos = next;
            
            if(tok == PMATH_TOK_BINARY_RIGHT){
              if(next_prec >= cur_prec){
                parse_rest(parser, rhs, next_prec); // =: rhs
                
                next = next_token_pos(parser);
                continue;
              }
              
              break;
            }
            
            if(tok == PMATH_TOK_PLUSPLUS){
              if(!plusplus_is_infix(parser, next))
                next_prec = PMATH_PREC_INC;
            }
            
            if(pmath_token_maybe_first(tok)){
              
              if((tok != PMATH_TOK_LEFTCALL 
               || parser->last_space_start == parser->tokens.pos)
              && pmath_token_maybe_rest(tok)){
                if(next_prec > cur_prec){
                  parse_rest(parser, rhs, next_prec); // =: rhs
                  
                  next = next_token_pos(parser);
                  continue;
                }
              }
              else if(PMATH_PREC_MUL > cur_prec){
                parse_rest(parser, rhs, PMATH_PREC_MUL); // =: rhs
                
                next = next_token_pos(parser);
                continue;
              }
              
              break;
            }
            
            if(pmath_token_maybe_rest(tok)){
              if(next_prec > cur_prec){
                parse_rest(parser, rhs, next_prec); // =: rhs
                
                next = next_token_pos(parser);
                continue;
              }
              
              break;
            }
            
            break;
          }
          
          next = next_token_pos(parser);
          last_prec = cur_prec;
          continue;
        }
      } break;
      
      case PMATH_TOK_NONE:
      case PMATH_TOK_SPACE:
      case PMATH_TOK_RIGHT:
      case PMATH_TOK_COLON:
      case PMATH_TOK_COMMENTEND:
        break;
    }
    
    break;
  }
  
  span(&parser->tokens, lhs);
  leave(parser);
}

static void parse_textline(parser_t *parser){
  pmath_token_t tok;
  int i;
  int start = parser->tokens.pos;
  
  if(parser->tokens.str[parser->tokens.pos] == '"'){
    parse_prim(parser, FALSE);
    return;
  }
  
  i = next_token_pos(parser);
  tok  = token_analyse(parser, i, NULL);
  if(tok == PMATH_TOK_LEFT){
    parse_prim(parser, FALSE);
    return;
  }
  
  while(parser->tokens.pos < parser->tokens.len
  && parser->tokens.str[parser->tokens.pos] != '\n'){
    switch(parser->tokens.str[parser->tokens.pos]){
      case '.':
      case '/':
      case '\\':
      case '~':
      case '*':
      case '?':
      case '!':
      case ':':
      case '-':
      case '+':
        ++parser->tokens.pos;
        continue;
    }
    
    tok = token_analyse(parser, parser->tokens.pos + 1, NULL);
    if(tok == PMATH_TOK_NAME 
    || tok == PMATH_TOK_NAME2 
    || tok == PMATH_TOK_DIGIT){
      ++parser->tokens.pos;
      continue;
    }
    
    break;
  }
  
  for(i = start;i < parser->tokens.pos;++i)
    parser->spans->items[i]&= ~1; // no token end
  
  parser->spans->items[parser->tokens.pos - 1]|= 1; // token end
  parser->spans->items[start] |= 2; //operand start
  
  span(&parser->tokens, start);
  skip_space(parser, start, TRUE);
}

//} ... parsing

//{ group spans ...

typedef struct{
  pmath_span_array_t   *spans;
  pmath_string_t        string;
  const uint16_t       *str;
  int                   pos;
  pmath_bool_t          parseable;
  pmath_t             (*box_at_index)(int,void*);
  void                 *data;
}_pmath_group_t;

static void skip_whitespace(_pmath_group_t *group){
  for(;;){
    while(group->pos < group->spans->length 
    && pmath_token_analyse(&group->str[group->pos], 1, NULL) == PMATH_TOK_SPACE)
      ++group->pos;
    
    if(group->pos < group->spans->length
    && group->str[group->pos] == '\\'){
      ++group->pos;
    }
    else if(group->pos + 1 < group->spans->length
    && group->str[group->pos]     == '/'
    && group->str[group->pos + 1] == '*'
    && !SPAN_TOK(group->spans->items[group->pos])){
      pmath_span_t *s = SPAN_PTR(group->spans->items[group->pos]);
      if(s){
        while(s->next)
          s = s->next;
        
        group->pos = s->end + 1;
      }
      else
        group->pos+= 2;
    }
    else return;
  }
}

void write_to_str(pmath_string_t *result, const uint16_t *data, int len){
  *result = pmath_string_insert_ucs2(
    *result,
    pmath_string_length(*result),
    data,
    len);
}

static void emit_span(pmath_span_t *span, _pmath_group_t *group){
  if(!span){
    if(group->str[group->pos] == PMATH_CHAR_BOX){
      if(group->box_at_index)
        pmath_emit(
          group->box_at_index(group->pos, group->data), 
          PMATH_NULL);
      else
        pmath_emit(PMATH_NULL, PMATH_NULL);
      ++group->pos;
    }
    else{
      int start = group->pos;
      
      while(group->pos < group->spans->length 
      && !SPAN_TOK(group->spans->items[group->pos]))
        ++group->pos;
      ++group->pos;
      
      pmath_emit(
        pmath_string_part(
          pmath_ref(group->string), 
          start, 
          group->pos - start), 
        PMATH_NULL);
    }
    
    if(group->parseable)
      skip_whitespace(group);
    return;
  }
  
  if(/*group->parseable 
  && */!span->next 
  && group->str[group->pos] == '"'){
    pmath_string_t result = PMATH_NULL;
    int start = group->pos;
    while(group->pos <= span->end){
      if(group->str[group->pos] == PMATH_CHAR_BOX){
        static const uint16_t left_box_char  = PMATH_CHAR_LEFT_BOX;
        static const uint16_t right_box_char = PMATH_CHAR_RIGHT_BOX;
        pmath_t box;
        
        if(start < group->pos){
          if(!pmath_is_null(result)){
            result = pmath_string_insert_ucs2(
              result,
              pmath_string_length(result),
              group->str + start,
              group->pos - start);
          }
          else{
            result = pmath_string_part(
              pmath_ref(group->string),
              start,
              group->pos - start);
          }
        }
        
        result = pmath_string_insert_ucs2(
          result,
          pmath_string_length(result),
          &left_box_char,
          1);
        
        if(group->box_at_index)
          box = group->box_at_index(group->pos, group->data);
        else
          box = PMATH_NULL;
          
        pmath_write(
          box, 
          PMATH_WRITE_OPTIONS_FULLSTR, 
          (void(*)(void*,const uint16_t*,int))write_to_str, 
          &result);
      
        pmath_unref(box);
        
        result = pmath_string_insert_ucs2(
          result,
          pmath_string_length(result),
          &right_box_char,
          1);
          
        start = ++group->pos;
      }
      else
        ++group->pos;
    }
    
    if(pmath_is_null(result)){
      result = pmath_string_part(
        pmath_ref(group->string),
        start,
        group->pos - start);
    }
    else if(start < group->pos){
      result = pmath_string_insert_ucs2(
        result,
        pmath_string_length(result),
        group->str + start,
        group->pos - start);
    }
    
    pmath_emit(
      result, 
      PMATH_NULL);
    group->pos = span->end + 1;
    
    if(group->parseable)
      skip_whitespace(group);
    return;
  }
  
  pmath_gather_begin(PMATH_NULL);
  emit_span(span->next, group);
  while(group->pos <= span->end)
    emit_span(SPAN_PTR(group->spans->items[group->pos]), group);
  
  pmath_emit(pmath_gather_end(), PMATH_NULL);
}

PMATH_API pmath_t pmath_boxes_from_spans(
  pmath_span_array_t   *spans,
  pmath_string_t        string,
  pmath_bool_t          parseable,
  pmath_t             (*box_at_index)(int,void*),
  void                 *data
){
  _pmath_group_t group;
  pmath_expr_t result;
  
  if(!spans || spans->length != pmath_string_length(string))
    return PMATH_C_STRING("");//pmath_ref(_pmath_object_emptylist);
  
  group.spans        = spans;
  group.string       = string;
  group.str          = pmath_string_buffer(&string);
  group.pos          = 0;
  group.parseable    = parseable;
  group.box_at_index = box_at_index;
  group.data         = data;
  
  pmath_gather_begin(PMATH_NULL);
  
  if(parseable)
    skip_whitespace(&group);
    
  while(group.pos < spans->length)
    emit_span(SPAN_PTR(spans->items[group.pos]), &group);
  
  result = pmath_expr_set_item(pmath_gather_end(), 0, PMATH_NULL);
  if(pmath_expr_length(result) == 1){
    pmath_t tmp = pmath_expr_get_item(result, 1);
    pmath_unref(result);
    return tmp;
  }
  return result;
}

//} ... group spans

//{ ungroup span_array ...

typedef struct{
  pmath_span_array_t  *spans;
  uint16_t            *str;
  int                  pos;
  void               (*make_box)(int,pmath_t,void*);
  void                *data;
}_pmath_ungroup_t;

static int ungrouped_string_length(pmath_t box){ // box wont be freed
  if(pmath_is_string(box)){
    const uint16_t *str = pmath_string_buffer(&box);
    int i, k, len, result;
    
    len = pmath_string_length(box);
    result = 0;
    i = 0;
    while(i < len){
      if(str[i] == PMATH_CHAR_LEFT_BOX){
        ++i;
        k = 1;
        while(i < len){
          if(str[i] == PMATH_CHAR_LEFT_BOX){
            ++k;
          }
          else if(str[i] == PMATH_CHAR_RIGHT_BOX){
            if(--k == 0)
              break;
          }
          ++i;
        }
        if(i < len)
          ++i;
          
        ++result;
      }
      else{
        ++i;
        ++result;
      }
    }
    return result;
  }
  
  if(pmath_is_expr(box)){
    int result = 0;
    size_t i;
    pmath_t head = pmath_expr_get_item(box, 0);
    pmath_unref(head);
    if(!pmath_is_null(head) && !pmath_same(head, PMATH_SYMBOL_LIST))
      return 1;
    
    for(i = pmath_expr_length(box);i > 0;--i){
      pmath_t boxi = pmath_expr_get_item(box, i);
      result+= ungrouped_string_length(boxi);
      pmath_unref(boxi);
    }
    return result;
  }
  
  return 1;
}

static void ungroup(
  _pmath_ungroup_t *g,
  pmath_t           box // will be freed
){
  if(pmath_is_string(box)){
    const uint16_t *str;
    int len, i, j, start;
    scanner_t tokens;
    
    len = pmath_string_length(box);
    if(len == 0){
      pmath_unref(box);
      return;
    }
    
    str = pmath_string_buffer(&box);
    start = g->pos;
    
    i = 0;
    while(i < len){
      if(str[i] == PMATH_CHAR_LEFT_BOX){
        pmath_string_t box_in_str;
        int k = 1;
        int l;
        const uint16_t *sub;
        
        j = ++i;
        while(j < len){
          if(str[j] == PMATH_CHAR_LEFT_BOX){
            ++k;
          }
          else if(str[j] == PMATH_CHAR_RIGHT_BOX){
            if(--k == 0)
              break;
          }
          ++j;
        }
        
        l = j - i;
        
        box_in_str = pmath_string_part(pmath_ref(box), i, l);
        
        sub = pmath_string_buffer(&box_in_str);
        
        if(g->make_box){
          g->make_box(
            g->pos, 
            pmath_parse_string(box_in_str), 
            g->data);
        }
        
        i = i + l < len ? i + l + 1 : i + l;
        if(g->pos > 0)
          g->spans->items[g->pos - 1]|= 1; // token end
        g->spans->items[g->pos]|= 1; // token end
        g->str[g->pos++] = PMATH_CHAR_BOX;
      }
      else if(str[i] == PMATH_CHAR_BOX){
        g->str[g->pos++] = 0xFFFF;
        i++;
      }
      else
        g->str[g->pos++] = str[i++];
    }
    
    memset(&tokens, 0, sizeof(tokens));
    tokens.str        = g->str;
    tokens.span_items = g->spans->items;
    tokens.pos        = start;
    tokens.len        = g->pos;
    while(tokens.pos < tokens.len)
      scan_next(&tokens, NULL);
        
    pmath_unref(box);
    return;
  }
  
  if(pmath_is_expr(box)){
    pmath_t head = pmath_expr_get_item(box, 0);
    pmath_unref(head);
    if(pmath_same(head, PMATH_SYMBOL_LIST)
    || pmath_is_null(head)){
      size_t i, len;
      int start = g->pos;
      
      len = pmath_expr_length(box);
      for(i = 1;i <= len;++i)
        ungroup(g, pmath_expr_get_item(box, i));
        
	    g->spans->items[start]|= 2; // operand start
      
      if(len >= 1 
      && start < g->pos
      && pmath_same(head, PMATH_SYMBOL_LIST)){
        pmath_t first = pmath_expr_get_item(box, 1);
        
        pmath_span_t *old = SPAN_PTR(g->spans->items[start]);
        
        if(pmath_is_string(first)){
          const uint16_t *fbuf = pmath_string_buffer(&first);
          int flen = pmath_string_length(first);
          
          if(flen > 0 && fbuf[0] == '"' && (flen == 1 || fbuf[flen - 1] != '"')){
            if(old){
              old->end = g->pos - 1;
            }
            else{
              old = pmath_mem_alloc(sizeof(pmath_span_t));
              if(old){
                old->next = NULL;
                old->end = g->pos - 1;
                g->spans->items[start] = (uintptr_t)old
                  | SPAN_TOK(g->spans->items[start])
                  | SPAN_OP( g->spans->items[start]);
              }
            }
          }
        }
        
        pmath_unref(first);
        
        if(!old || old->end != g->pos - 1){
          pmath_span_t *s = pmath_mem_alloc(sizeof(pmath_span_t));
          if(s){
            s->next = old;
            s->end = g->pos - 1;
            g->spans->items[start] = (uintptr_t)s
              | SPAN_TOK(g->spans->items[start])
              | SPAN_OP( g->spans->items[start]);
          }
        }
      }
      
      pmath_unref(box);
      return;
    }
  }
  
  if(g->make_box)
    g->make_box(g->pos, box, g->data);
  else
    pmath_unref(box);
  
  g->spans->items[g->pos] = 1;
  g->str[g->pos++] = PMATH_CHAR_BOX;
}

PMATH_API pmath_span_array_t *pmath_spans_from_boxes(
  pmath_t          boxes,  // will be freed
  pmath_string_t  *result_string,
  void           (*make_box)(int,pmath_t,void*),
  void            *data
){
  _pmath_ungroup_t g;
  
  assert(result_string);
  
  g.spans = create_span_array(
    ungrouped_string_length(boxes));
  if(!g.spans){
    pmath_unref(boxes);
    *result_string = pmath_string_new(0);
    return NULL;
  }
  
  *result_string = PMATH_FROM_PTR(_pmath_new_string_buffer(g.spans->length));
  if(pmath_is_null(*result_string)){
    pmath_span_array_free(g.spans);
    return NULL;
  }
  
  g.str      = (uint16_t*)pmath_string_buffer(result_string);
  g.pos      = 0;
  g.make_box = make_box;
  g.data     = data;
  
  ungroup(&g, boxes);
  
  if(g.spans->length > 0)
    g.spans->items[0]|= 2; // operand start
  
  assert(g.pos == g.spans->length);
  return g.spans;
}

//} ... ungroup spans

PMATH_API
pmath_t pmath_string_expand_boxes(
  pmath_string_t  s
){
  const uint16_t *buf = pmath_string_buffer(&s);
  int len = pmath_string_length(s);
  
  while(len-- > 0)
    if(*buf++ == PMATH_CHAR_LEFT_BOX)
      goto HAVE_STH_TO_EXPAND;
  
  return s;
  
 HAVE_STH_TO_EXPAND:
  {
    int start, i;
    buf = pmath_string_buffer(&s);
    len = pmath_string_length(s);
    
    start = i = 0;
    
    pmath_gather_begin(PMATH_NULL);
    
    while(i < len){
      if(buf[i] == PMATH_CHAR_LEFT_BOX){
        int k;
        
        if(i > start){
          pmath_emit(
            pmath_string_part(pmath_ref(s), start, i - start),
            PMATH_NULL);
        }
        
        start = ++i;
        k = 1;
        while(i < len){
          if(buf[i] == PMATH_CHAR_LEFT_BOX){
            ++k;
          }
          else if(buf[i] == PMATH_CHAR_RIGHT_BOX){
            if(--k == 0)
              break;
          }
          ++i;
        }
        
        pmath_emit(
          pmath_parse_string(
            pmath_string_part(
              pmath_ref(s), 
              start, 
              i - start)),
          PMATH_NULL);
        
        if(i < len)
          ++i;
          
        start = i;
      }
      else
        ++i;
    }
    
    if(start < len){
      pmath_emit(
        pmath_string_part(pmath_ref(s), start, len - start),
        PMATH_NULL);
    }
    
    pmath_unref(s);
    
    {
      pmath_expr_t result = pmath_gather_end();
      
      if(pmath_expr_length(result) == 1){
        s = pmath_expr_get_item(result, 1);
        pmath_unref(result);
        return s;
      }
      
      return result;
    }
  }
}

static void syntax_error(pmath_string_t code, int pos, void *flag, pmath_bool_t critical){
  if(critical)
    *(pmath_bool_t*)flag = TRUE;
  pmath_message_syntax_error(code, pos, PMATH_NULL, 0);
}

PMATH_API pmath_t pmath_parse_string(
  pmath_string_t code
){
  pmath_bool_t error_flag = FALSE;
  pmath_t result;
  
  pmath_span_array_t *spans = pmath_spans_from_string(
    &code, 
    NULL, 
    NULL, 
    NULL, 
    syntax_error,
    &error_flag);
  
  result = pmath_boxes_from_spans(
    spans, 
    code, 
    TRUE, 
    NULL, 
    NULL);
    
  pmath_span_array_free(spans);
  pmath_unref(code);
  
  result = pmath_evaluate(
    pmath_expr_new_extended(
      pmath_ref(PMATH_SYMBOL_MAKEEXPRESSION), 1,
      result));
  
  if(!pmath_is_expr_of(result, PMATH_SYMBOL_HOLDCOMPLETE))
    return result;
  
  if(pmath_expr_length(result) == 1){
    pmath_t item = pmath_expr_get_item(result, 1);
    pmath_unref(result);
    return item;
  }
  
  return pmath_expr_set_item(
    result, 0,
    pmath_ref(PMATH_SYMBOL_SEQUENCE));
}

PMATH_API 
pmath_t pmath_parse_string_args(
  const char *code,
  const char *format,
  ...
){
  pmath_t old_parser_arguments;
  pmath_t new_parser_arguments;
  pmath_t result;
  
  va_list args;
  
  va_start(args, format);
  
  new_parser_arguments = pmath_build_value_v(format, args);
  
  va_end(args);
  
  old_parser_arguments = pmath_thread_local_save(
    PMATH_THREAD_KEY_PARSERARGUMENTS, 
    new_parser_arguments);
  
  result = pmath_parse_string(PMATH_C_STRING(code));
  
  pmath_unref(pmath_thread_local_save(
    PMATH_THREAD_KEY_PARSERARGUMENTS,
    old_parser_arguments));
  
  return result;
}
