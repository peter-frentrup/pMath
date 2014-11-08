#include <pmath-core/strings-private.h>

#include <pmath-language/charnames.h>
#include <pmath-language/patterns-private.h>
#include <pmath-language/scanner.h>
#include <pmath-language/tokens.h>

#include <pmath-util/concurrency/threads.h>
#include <pmath-util/debug.h>
#include <pmath-util/emit-and-gather.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/memory.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>

#include <limits.h>
#include <string.h>


//{ spans ...

struct _pmath_span_t {
  pmath_span_t *next;
  int end;
};

struct _pmath_span_array_t {
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

static pmath_span_array_t *create_span_array(int length) {
  pmath_span_array_t *result;
  
  if(length < 1)
    return NULL;
    
  result = (pmath_span_array_t *)pmath_mem_alloc(
             sizeof(pmath_span_array_t) + (size_t)(length - 1) * sizeof(void *));
             
  if(!result)
    return NULL;
    
  result->length = length;
  memset(result->items, 0, (size_t)length * sizeof(void *));
  
  return result;
}

static pmath_span_array_t *enlarge_span_array(
  pmath_span_array_t *spans,
  int extra_len
) {
  pmath_span_array_t *result;
  
  if(extra_len < 1)
    return spans;
    
  if(!spans)
    return create_span_array(extra_len);
    
  result = pmath_mem_realloc_no_failfree(
             spans,
             sizeof(pmath_span_array_t) + (size_t)(spans->length + extra_len - 1) * sizeof(void *));
             
  if(!result)
    return spans;
    
  memset(result->items + result->length, 0, extra_len * sizeof(void *));
  
  result->length += extra_len;
  return result;
}

PMATH_API void pmath_span_array_free(pmath_span_array_t *spans) {
  int i;
  
  if(!spans)
    return;
    
  assert(spans->length > 0);
  for(i = 0; i < spans->length; ++i) {
    pmath_span_t *s = SPAN_PTR(spans->items[i]);
    while(s) {
      pmath_span_t *n = s->next;
      pmath_mem_free(s);
      s = n;
    }
  }
  
  pmath_mem_free(spans);
}

PMATH_API int pmath_span_array_length(pmath_span_array_t *spans) {
  if(!spans)
    return 0;
  assert(spans->length > 0);
  return spans->length;
}

PMATH_API pmath_bool_t pmath_span_array_is_token_end(
  pmath_span_array_t *spans,
  int pos
) {
  if(!spans || pos < 0 || pos >= spans->length) {
    pmath_debug_print("[pmath_span_array_is_token_end: out of bounds]\n");
    return TRUE;
  }
  
  if(pos + 1 == spans->length) {
    if(1 != SPAN_TOK(spans->items[pos])) {
      pmath_debug_print("[missing token_end flag at end of span array]\n");
    }
    
    return TRUE;
  }
  
  return 1 == SPAN_TOK(spans->items[pos]);
}

PMATH_API pmath_bool_t pmath_span_array_is_operand_start(
  pmath_span_array_t *spans,
  int pos
) {
  if(!spans || pos < 0 || pos >= spans->length)
    return FALSE;
    
  return 2 == SPAN_OP(spans->items[pos]);
}

PMATH_API pmath_span_t *pmath_span_at(pmath_span_array_t *spans, int pos) {
  if(!spans || pos < 0 || pos >= spans->length)
    return NULL;
    
  return SPAN_PTR(spans->items[pos]);
}

PMATH_API pmath_span_t *pmath_span_next(pmath_span_t *span) {
  if(!span)
    return NULL;
  return span->next;
}

PMATH_API int pmath_span_end(pmath_span_t *span) {
  if(!span)
    return 0;
  return span->end;
}

//} ... spans

struct scanner_t {
  const uint16_t  *str;
  int              len;
  int              pos;
  uintptr_t       *span_items;
  
  int comment_level;
  
  //pmath_bool_t in_comment;
  pmath_bool_t in_string;
  pmath_bool_t have_error;
};

struct parser_t {
  pmath_string_t code;
  int fencelevel;
  int stack_size;
  
  
  pmath_span_array_t *spans;
  pmath_string_t    (*read_line)(void *);
  pmath_bool_t      (*subsuperscriptbox_at_index)(int, void *);
  pmath_string_t    (*underoverscriptbox_at_index)(int, void *);
  void              (*error)(pmath_string_t, int, void *, pmath_bool_t); //does not free 1st arg
  void               *data;
  
  struct scanner_t tokens;
  
  pmath_bool_t stack_error;
  pmath_bool_t tokenizing;
  pmath_bool_t last_was_newline;
  int          last_space_start;
};

//{ parsing ...

static void span(struct scanner_t *tokens, int start) {
  int i, end;
  
  if(!tokens->span_items)
    return;
    
  if(start < 0)
    start = 0;
    
  end = tokens->pos - 1;
  while( end > start &&
         pmath_token_analyse(&tokens->str[end], 1, NULL) == PMATH_TOK_SPACE)
  {
    --end;
  }
  
  if(end < start)
    return;
    
  if( SPAN_PTR(tokens->span_items[start]) &&
      SPAN_PTR(tokens->span_items[start])->end >= end)
  {
    return;
  }
  
  for(i = start; i < end; ++i)
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
      (uintptr_t)s                        |
      SPAN_TOK(tokens->span_items[start]) |
      SPAN_OP( tokens->span_items[start]);
  }
}

static void handle_error(struct parser_t *parser) {
  if(/*parser->tokens.have_error || */ parser->tokens.comment_level > 0)
    return;
    
  parser->tokens.have_error = TRUE;
  if(parser->error)
    parser->error(parser->code, parser->tokens.pos, parser->data, TRUE);
}

static void handle_newline_multiplication(struct parser_t *parser) {
  int i;
  
  if(!parser->error || parser->tokens.comment_level > 0)
    return;
    
  i = parser->tokens.pos - 1;
  while(i >= 0 && parser->tokens.str[i] != '\n')
    --i;
    
  parser->error(parser->code, i, parser->data, FALSE);
}

static pmath_bool_t enter(struct parser_t *parser) {
  if(++parser->stack_size > 256) { // 4096
    --parser->stack_size;
    
    parser->stack_error = TRUE;
    
    // TODO generate message
    pmath_debug_print("parser: stack overflow");
    return FALSE;
  }
  return TRUE;
}

static void leave(struct parser_t *parser) {
  --parser->stack_size;
}

static void scan_next(struct scanner_t *tokens, struct parser_t *parser);

static void parse_sequence(struct parser_t *parser);
static void parse_prim(    struct parser_t *parser, pmath_bool_t prim_optional);
static void parse_rest(    struct parser_t *parser, int lhs_start, int min_prec);
static void parse_textline(struct parser_t *parser);

static int next_token_pos(struct parser_t *parser) {
  int next;
  struct _pmath_span_t *span;
  
  if(parser->tokens.pos == parser->tokens.len)
    return parser->tokens.pos;
    
  span = SPAN_PTR(parser->spans->items[parser->tokens.pos]);
  
  if(span)
    return span->end + 1;
    
  next = parser->tokens.pos;
  while( next < parser->tokens.len &&
         !pmath_span_array_is_token_end(parser->spans, next))
  {
    ++next;
  }
  
  if(next < parser->tokens.len)
    ++next;
    
  return next;
}

static void skip_to(struct parser_t *parser, int span_start, int next, pmath_bool_t optional);

static pmath_bool_t read_more(struct parser_t *parser) {
  pmath_string_t newline;
  int extralen;
  
  if(!parser)
    return FALSE;
    
  if(!parser->read_line) {
    handle_error(parser);
    return FALSE;
  }
  
  newline = parser->read_line(parser->data);
  
  if(pmath_is_null(newline)) {
    handle_error(parser);
    return FALSE;
  }
  
  extralen = 1 + pmath_string_length(newline);
  parser->spans = enlarge_span_array(parser->spans, extralen);
  if( !parser->spans ||
      parser->spans->length != parser->tokens.len + extralen)
  {
    parser->tokens.span_items = NULL;
    pmath_unref(newline);
    handle_error(parser);
    return FALSE;
  }
  
  parser->code = pmath_string_insert_latin1(parser->code, INT_MAX, "\n", 1);
  parser->code = pmath_string_concat(parser->code, newline);
  
  if(parser->spans->length != pmath_string_length(parser->code)) {
    pmath_unref(newline);
    handle_error(parser);
    return FALSE;
  }
  
  parser->tokens.str = pmath_string_buffer(&parser->code);
  parser->tokens.len = pmath_string_length(parser->code);
  parser->tokens.span_items = parser->spans->items;
  return TRUE;
}

static void skip_space(struct parser_t *parser, int span_start, pmath_bool_t optional) {
  parser->last_was_newline = FALSE;
  
  if( parser->tokens.pos < parser->tokens.len &&
      parser->tokens.str[parser->tokens.pos] == PMATH_CHAR_BOX &&
      parser->subsuperscriptbox_at_index &&
      parser->subsuperscriptbox_at_index(parser->tokens.pos, parser->data))
  {
    if(span_start >= 0)
      span(&parser->tokens, span_start);
    ++parser->tokens.pos;
  }
  
  parser->last_space_start = parser->tokens.pos;
  for(;;) {
    pmath_token_t tok;
    
    while( parser->tokens.pos < parser->tokens.len &&
           parser->tokens.str[parser->tokens.pos] != '\n')
    {
      tok = pmath_token_analyse(parser->tokens.str + parser->tokens.pos, 1, NULL);
      
      if(tok != PMATH_TOK_SPACE)
        break;
        
      ++parser->tokens.pos;
    }
    
    if(parser->tokens.pos < parser->tokens.len) {
      struct _pmath_span_t *span;
      
      span = SPAN_PTR(parser->spans->items[parser->tokens.pos]);
      
      if(span) {
        if( parser->tokens.pos + 1 < parser->tokens.len       &&
            parser->tokens.str[parser->tokens.pos]     == '/' &&
            parser->tokens.str[parser->tokens.pos + 1] == '*')
        {
          parser->tokens.pos = span->end + 1;
          continue;
        }
        else
          break;
      }
    }
    
    if( parser->tokens.pos < parser->tokens.len &&
        parser->tokens.str[parser->tokens.pos] == '\n')
    {
      if(!optional || parser->fencelevel > 0) {
        ++parser->tokens.pos;
        parser->last_was_newline = TRUE;
        continue;
      }
      
      break;
    }
    
    if( parser->tokens.pos < parser->tokens.len &&
        parser->tokens.str[parser->tokens.pos] == '\\')
    {
      if( parser->tokens.pos + 1 == parser->tokens.len ||
          parser->tokens.str[parser->tokens.pos] <= ' ')
      {
        int i = parser->tokens.pos + 1;
        
        while( i < parser->tokens.len &&
               parser->tokens.str[i] <= ' ')
        {
          ++i;
        }
        
        if(i == parser->tokens.len) {
          parser->tokens.pos = i;
          optional = FALSE;
        }
      }
    }
    
    if( parser->tokens.pos + 1 < parser->tokens.len       &&
        //!parser->tokens.in_comment                        &&
        parser->tokens.str[parser->tokens.pos]     == '/' &&
        parser->tokens.str[parser->tokens.pos + 1] == '*')
    {
      pmath_bool_t last_was_newline = FALSE;
      int last_space_start = parser->last_space_start;
      
      int start = parser->tokens.pos;
      
      skip_to(parser, -1, next_token_pos(parser), FALSE);
      parser->tokens.comment_level++;
      ++parser->fencelevel;
      while(parser->tokens.pos < parser->tokens.len) {
        int tmp;
        
        if( parser->tokens.pos + 1 < parser->tokens.len       &&
            parser->tokens.str[parser->tokens.pos]     == '*' &&
            parser->tokens.str[parser->tokens.pos + 1] == '/')
        {
          parser->tokens.pos += 2;
          break;
        }
        
        tmp = parser->tokens.pos;
        parse_sequence(parser);
        if(tmp == parser->tokens.pos)
          ++parser->tokens.pos;
      }
      
      span(&parser->tokens, start);
      --parser->fencelevel;
      parser->tokens.comment_level--;
      
      parser->last_was_newline = last_was_newline;
      parser->last_space_start = last_space_start;
    }
    else if( parser->tokens.pos == parser->tokens.len &&
             (!optional ||
              parser->fencelevel > 0) &&
             !pmath_aborting())
    {
      if(!read_more(parser))
        break;
        
      if(!parser->tokenizing) {
        int          old_pos           = parser->tokens.pos;
        int          old_comment_level = parser->tokens.comment_level;
        pmath_bool_t old_in_string     = parser->tokens.in_string;
        
        parser->stack_error       = FALSE;
        parser->tokens.have_error = FALSE;
        parser->tokenizing        = TRUE;
        
        while(parser->tokens.pos < parser->tokens.len)
          scan_next(&parser->tokens, parser);
          
        parser->tokens.pos           = old_pos;
        parser->tokens.comment_level = old_comment_level;
        parser->tokens.in_string     = old_in_string;
        parser->tokenizing           = FALSE;
      }
    }
    else break;
  }
}

static void skip_to(struct parser_t *parser, int span_start, int next, pmath_bool_t optional) {
  parser->tokens.pos = next;
  skip_space(parser, span_start, optional);
}

// like pmath_token_analyze(,,NULL), but can handle \[NNN], \[U+HHHH], \n, \\, ...
static pmath_token_t scan_next_escaped_char(struct scanner_t *tokens, struct parser_t *parser, uint32_t *chr) {
  uint32_t u;
  const uint16_t *endstr;
  int endpos;
  uint16_t u16[2];
  
START:

  endstr = pmath_char_parse(tokens->str + tokens->pos, tokens->len - tokens->pos, &u);
  endpos = endstr - tokens->str;
  *chr = u;
  
  if(u <= 0xFFFF) {
    tokens->pos = endpos;
    
    u16[0] = (uint16_t)u;
    return pmath_token_analyse(u16, 1, NULL);
  }
  
  if(u <= 0x10FFFF) {
    tokens->pos = endpos;
    
    u -= 0x10000;
    u16[0] = 0xD800 | (uint16_t)((u >> 10) & 0x03FF);
    u16[1] = 0xDC00 | (uint16_t)(u & 0x03FF);
    return pmath_token_analyse(u16, 2, NULL);
  }
  
  if( tokens->pos + 1 == endpos)
  {
    tokens->pos = endpos;
    //++tokens->pos;
    
    return PMATH_TOK_NONE;
  }
  
  if(endpos + 1 == tokens->len && tokens->str[endpos] == '\\') {
    /*   "\[Raw\
           Plus]"
      should be parsed as "\[RawPlus]"
    */
    
    if(parser && parser->tokenizing && read_more(parser))
      goto START;
  }
  
  tokens->pos = endpos;
  
  if(parser && tokens->comment_level == 0)
    handle_error(parser);
    
  return PMATH_TOK_NONE;
}

static void scan_next(struct scanner_t *tokens, struct parser_t *parser) {
  assert(!parser || &parser->tokens == tokens);
  
  if(tokens->pos >= tokens->len)
    return;
    
  switch(tokens->str[tokens->pos]) {
    case '#': {
        tokens->span_items[tokens->pos] |= 2;
        ++tokens->pos;
        
        if(tokens->pos == tokens->len)
          break;
          
        if(tokens->str[tokens->pos] == '#') { // ##
          ++tokens->pos;
          break;
        }
      } break;
      
    case '+': { // ++ +=
        ++tokens->pos;
        
        if(tokens->pos == tokens->len)
          break;
          
        if( tokens->str[tokens->pos] == '+' || // ++
            tokens->str[tokens->pos] == '=')   // +=
        {
          ++tokens->pos;
          break;
        }
      } break;
      
    case '-': {
        ++tokens->pos;
        
        if(tokens->pos == tokens->len)
          break;
          
        if( tokens->str[tokens->pos] == '-' || // --
            tokens->str[tokens->pos] == '=' || // -=
            tokens->str[tokens->pos] == '>')   // ->
        {
          ++tokens->pos;
          break;
        }
      } break;
      
    case '*': {
        ++tokens->pos;
        
        if(tokens->pos == tokens->len)
          break;
          
        if(tokens->str[tokens->pos] == '=') { // *=
          ++tokens->pos;
          break;
        }
        
        if(tokens->str[tokens->pos] == '*') {
          ++tokens->pos;
          
          if(tokens->pos == tokens->len) // **
            break;
            
          if(tokens->str[tokens->pos] == '*') {
            ++tokens->pos;
            
            if( tokens->comment_level > 0  &&
                tokens->pos < tokens->len &&
                tokens->str[tokens->pos] == '/') // ***/   ===   ** */
            {
              tokens->comment_level--;
              if(tokens->span_items)
                tokens->span_items[tokens->pos - 2] = 1;
              ++tokens->pos;
              break;
            }
            
            break; // ***
          }
          
          if( tokens->comment_level > 0 &&
              tokens->str[tokens->pos] == '/') // **/   ===   * */
          {
            tokens->comment_level--;
            if(tokens->span_items)
              tokens->span_items[tokens->pos - 2] = 1;
            ++tokens->pos;
            break;
          }
          
          break; // **
        }
        
        if( tokens->comment_level > 0 &&
            tokens->str[tokens->pos] == '/') // */
        {
          tokens->comment_level--;
          ++tokens->pos;
          break;
        }
        
      } break;
      
    case '/': {
        ++tokens->pos;
        
        if(tokens->pos == tokens->len)
          break;
          
        if( //!tokens->in_comment &&
          !tokens->in_string &&
          tokens->str[tokens->pos] == '*') // /*
        {
          tokens->comment_level++;
          ++tokens->pos;
          break;
        }
        
        if( tokens->str[tokens->pos] == '=' || // /=
            tokens->str[tokens->pos] == '?' || // /?
            tokens->str[tokens->pos] == '@' || // /@
            tokens->str[tokens->pos] == '.' || // /.
            tokens->str[tokens->pos] == ':')   // /:
        {
          ++tokens->pos;
          break;
        }
        
        if(tokens->str[tokens->pos] == '/') {
          ++tokens->pos;
          
          if(tokens->pos == tokens->len) // //
            break;
            
          if( tokens->str[tokens->pos] == '@' || // //@
              tokens->str[tokens->pos] == '.')   // //.
          {
            ++tokens->pos;
            break;
          }
          
          break;
        }
        
        if( tokens->pos + 1 < tokens->len        &&
            tokens->str[tokens->pos]     == '\\' &&
            tokens->str[tokens->pos + 1] == '/')
        {
          tokens->pos += 2;
        }
        
      } break;
      
    case '<':
    case '>':
    case '!': {
        ++tokens->pos;
        
        if(tokens->pos == tokens->len)
          break;
          
        if( tokens->str[tokens->pos] == '=' ||                         // <= >= !=
            tokens->str[tokens->pos] == tokens->str[tokens->pos - 1])  // << >> !!
        {
          ++tokens->pos;
          break;
        }
      } break;
      
    case '=': {
        ++tokens->pos;
        
        if( tokens->pos + 1 < tokens->len     &&
            (tokens->str[tokens->pos] == '!' ||
             tokens->str[tokens->pos] == '=') &&
            tokens->str[tokens->pos + 1] == '=')
        {
          tokens->pos += 2;
        }
      } break;
      
    case '~': {
        ++tokens->pos;
        
        if( tokens->pos < tokens->len &&
            tokens->str[tokens->pos] == '~')
        {
          ++tokens->pos;
          
          if( tokens->pos < tokens->len &&
              tokens->str[tokens->pos] == '~')
          {
            ++tokens->pos;
          }
        }
      } break;
      
    case ':': {
        ++tokens->pos;
        
        if(tokens->pos == tokens->len)
          break;
          
        if(tokens->str[tokens->pos] == ':') {
          ++tokens->pos;
          
          if(tokens->pos == tokens->len) // ::
            break;
            
          if(tokens->str[tokens->pos] == '=') { // ::=
            ++tokens->pos;
            break;
          }
          
          break; // ::
        }
        
        if( tokens->str[tokens->pos] == '=' || // :=
            tokens->str[tokens->pos] == '>')   // :>
        {
          ++tokens->pos;
          break;
        }
      } break;
      
    case '?':
    case '|':
    case '&': {
        ++tokens->pos;
        
        if( tokens->pos < tokens->len &&
            tokens->str[tokens->pos] == tokens->str[tokens->pos - 1]) // ?? || &&
        {
          ++tokens->pos;
        }
      } break;
      
    case '%':
      tokens->span_items[tokens->pos] |= 2;
    /* fall through */
    case '@':
    case '.': {
        ++tokens->pos;
        while( tokens->pos < tokens->len &&
               tokens->str[tokens->pos] == tokens->str[tokens->pos - 1])
        {
          ++tokens->pos;
        }
      } break;
      
    case '`': {
        ++tokens->pos;
        while( tokens->pos < tokens->len &&
               pmath_char_is_digit(tokens->str[tokens->pos]))
        {
          ++tokens->pos;
        }
        
        if( tokens->pos < tokens->len &&
            tokens->str[tokens->pos] == '`')
        {
          ++tokens->pos;
        }
      } break;
      
    case '"': {
        if(!tokens->in_string) {
          int k = 0;
          int start = tokens->pos;
          tokens->in_string = TRUE;
          
          ++tokens->pos;
          for(;;) {
            while( tokens->pos < tokens->len &&
                   (k > 0 || tokens->str[tokens->pos] != '"'))
            {
              if(tokens->str[tokens->pos] == PMATH_CHAR_LEFT_BOX) {
                ++k;
                tokens->pos++;
              }
              else if(tokens->str[tokens->pos] == PMATH_CHAR_RIGHT_BOX) {
                --k;
                tokens->pos++;
              }
              else
                scan_next(tokens, parser);
            }
            
            if(tokens->pos < tokens->len)
              break;
              
            if(!read_more(parser)) {
              if(tokens->span_items)
                tokens->span_items[start] |= 3;
                
              span(tokens, start);
              goto END_SCAN;
            }
          }
          
          if( tokens->pos < tokens->len &&
              tokens->str[tokens->pos] == '"')
          {
            scan_next(tokens, parser);
          }
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
      
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9': {
        tokens->span_items[tokens->pos] |= 2;
        ++tokens->pos;
        
        while(tokens->pos < tokens->len &&
              pmath_char_is_digit(tokens->str[tokens->pos]))
        {
          ++tokens->pos;
        }
        
        if( tokens->pos + 2 < tokens->len       &&
            tokens->str[tokens->pos]     == '^' &&
            tokens->str[tokens->pos + 1] == '^' &&
            pmath_char_is_36digit(tokens->str[tokens->pos + 2]))
        {
          tokens->pos += 3;
          while(tokens->pos < tokens->len &&
                pmath_char_is_36digit(tokens->str[tokens->pos]))
          {
            ++tokens->pos;
          }
          
          if( tokens->pos + 1 < tokens->len   &&
              tokens->str[tokens->pos] == '.' &&
              pmath_char_is_36digit(tokens->str[tokens->pos + 1]))
          {
            tokens->pos += 2;
            while( tokens->pos < tokens->len &&
                   pmath_char_is_36digit(tokens->str[tokens->pos]))
            {
              ++tokens->pos;
            }
          }
        }
        else if(tokens->pos + 1 < tokens->len   &&
                tokens->str[tokens->pos] == '.' &&
                pmath_char_is_digit(tokens->str[tokens->pos + 1]))
        {
          tokens->pos += 2;
          while( tokens->pos < tokens->len &&
                 pmath_char_is_digit(tokens->str[tokens->pos]))
          {
            ++tokens->pos;
          }
        }
        
        if( tokens->pos < tokens->len &&
            tokens->str[tokens->pos] == '`')
        {
          ++tokens->pos;
          
          if( tokens->pos < tokens->len &&
              tokens->str[tokens->pos] == '`')
          {
            ++tokens->pos;
          }
          
          if( tokens->pos + 1 < tokens->len     &&
              (tokens->str[tokens->pos] == '+' ||
               tokens->str[tokens->pos] == '-') &&
              pmath_char_is_digit(tokens->str[tokens->pos + 1]))
          {
            ++tokens->pos;
          }
          
          if(tokens->pos < tokens->len && pmath_char_is_digit(tokens->str[tokens->pos])) {
            ++tokens->pos;
            while(tokens->pos < tokens->len &&
                  pmath_char_is_digit(tokens->str[tokens->pos]))
            {
              ++tokens->pos;
            }
            
            if( tokens->pos + 1 < tokens->len &&
                tokens->str[tokens->pos] == '.' &&
                pmath_char_is_digit(tokens->str[tokens->pos + 1]))
            {
              tokens->pos += 2;
              while(tokens->pos < tokens->len &&
                    pmath_char_is_digit(tokens->str[tokens->pos]))
              {
                ++tokens->pos;
              }
            }
          }
        }
        
        if( tokens->pos + 2 < tokens->len &&
            tokens->str[tokens->pos]   == '*' &&
            tokens->str[tokens->pos + 1] == '^' &&
            (pmath_char_is_digit(tokens->str[tokens->pos + 2]) ||
             (tokens->pos + 3 < tokens->len &&
              (tokens->str[tokens->pos + 2] == '-' ||
               tokens->str[tokens->pos + 2] == '+') &&
              pmath_char_is_digit(tokens->str[tokens->pos + 3]))))
        {
          tokens->pos += 3;
          while(tokens->pos < tokens->len &&
                pmath_char_is_digit(tokens->str[tokens->pos]))
          {
            ++tokens->pos;
          }
        }
      }
      break;
      
    default: {
        pmath_token_t tok;
        uint32_t u;
        
        int prev = tokens->pos;
        tok = scan_next_escaped_char(tokens, parser, &u);
        
        if(tok == PMATH_TOK_NAME) {
          tokens->span_items[prev] |= 2;
          
          while(tokens->pos < tokens->len) {
            prev = tokens->pos;
            tok = scan_next_escaped_char(tokens, parser, &u);
            
            if( tok != PMATH_TOK_NAME &&
                tok != PMATH_TOK_DIGIT)
            {
              tokens->pos = prev;
              break;
            }
          }
        }
      }
  }
  
END_SCAN:
  if(tokens->span_items)
    tokens->span_items[tokens->pos - 1] |= 1;
}

PMATH_API pmath_span_array_t *pmath_spans_from_string(
  pmath_string_t   *code,
  pmath_string_t  (*line_reader)(void *),
  pmath_bool_t    (*subsuperscriptbox_at_index)(int, void *),
  pmath_string_t  (*underoverscriptbox_at_index)(int, void *),
  void            (*error)(  pmath_string_t, int, void *, pmath_bool_t), //does not free 1st arg
  void             *data
) {
  struct parser_t parser;
  
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
  parser.tokens.comment_level        = 0;
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
  parser.tokens.comment_level = 0;
  parser.tokens.in_string  = FALSE;
  parser.tokenizing = FALSE;
  
  if(parser.tokens.have_error) {
    skip_space(&parser, -1, FALSE);
  }
  else {
    parser.tokens.have_error = TRUE;
    skip_space(&parser, -1, FALSE);
    parser.tokens.have_error = FALSE;
  }
  
  while(parser.tokens.pos < parser.tokens.len) {
    int tmp = parser.tokens.pos;
    parse_sequence(&parser);
    
    if(parser.tokens.pos < parser.tokens.len) {
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
  struct parser_t *parser,
  int              next,
  int             *prec
) {
  pmath_token_t tok = pmath_token_analyse(
                        parser->tokens.str + parser->tokens.pos,
                        next - parser->tokens.pos,
                        prec);
                        
  if( tok == PMATH_TOK_NAME2 &&
      parser->tokens.str[parser->tokens.pos] == PMATH_CHAR_BOX &&
      parser->underoverscriptbox_at_index)
  {
    pmath_string_t str = parser->underoverscriptbox_at_index(parser->tokens.pos, parser->data);
    
    if(!pmath_is_null(str)) {
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
  struct parser_t *parser,
  int              next,
  int              defprec
) {
  return pmath_token_prefix_precedence(
           parser->tokens.str + parser->tokens.pos,
           next - parser->tokens.pos,
           defprec);
}

static pmath_bool_t same_token(
  struct parser_t *parser,
  int              last_start,
  int              last_next,
  int              cur_next
) {
  const uint16_t *buf = parser->tokens.str;
  int cur_start = parser->tokens.pos;
  int cur_len   = cur_next  - cur_start;
  int last_len  = last_next - last_start;
  
  if(last_len == cur_len) {
    int i;
    
    for(i = 0; i < cur_len; ++i)
      if(buf[cur_start + i] != buf[last_start + i])
        return FALSE;
        
    return TRUE;
  }
  
  return FALSE;
}

static pmath_bool_t plusplus_is_infix(struct parser_t *parser, int next) {
  pmath_bool_t oldnl  = parser->last_was_newline;
  int          oldss  = parser->last_space_start;
  int          oldpos = parser->tokens.pos;
  pmath_bool_t result;
  
  skip_to(parser, -1, next, TRUE);
  next = next_token_pos(parser);
  
  result = next > parser->tokens.pos &&
           pmath_token_maybe_first(token_analyse(parser, next, NULL));
           
  parser->last_was_newline = oldnl;
  parser->last_space_start = oldss;
  parser->tokens.pos       = oldpos;
  return result;
}

static void parse_skip_until_rightfence(struct parser_t *parser) {
  pmath_token_t tok;
  int start;
  int next;
  int fences;
  
  start = parser->tokens.pos;
  next  = next_token_pos(parser);
  fences = 0;
  
  while(next != parser->tokens.pos) {
    tok = token_analyse(parser, next, NULL);
    
    if(tok == PMATH_TOK_RIGHT) {
      --fences;
      if(fences < 0)
        break;
    }
    
    if( tok == PMATH_TOK_LEFT ||
        tok == PMATH_TOK_LEFTCALL)
    {
      ++fences;
    }
    
    skip_to(parser, -1, next, TRUE);
    
    next = next_token_pos(parser);
  }
  
  span(&parser->tokens, start);
}

static void parse_sequence(struct parser_t *parser) {
  pmath_token_t tok;
  int last;
  int start;
  int next;
  
  if(!enter(parser)) {
    parse_skip_until_rightfence(parser);
    return;
  }
  
  start = last = parser->tokens.pos;
  next  = next_token_pos(parser);
  
  while(next != parser->tokens.pos) {
    tok = token_analyse(parser, next, NULL);
    
    if( tok == PMATH_TOK_RIGHT      ||
        tok == PMATH_TOK_COMMENTEND ||
        tok == PMATH_TOK_SPACE)
    {
      break;
    }
    
    parse_prim(parser, FALSE);
    parse_rest(parser, start, PMATH_PREC_ANY);
    
    if(parser->tokens.pos == last) {
      skip_to(parser, -1, next, TRUE);
    }
    
    last = parser->tokens.pos;
    next = next_token_pos(parser);
  }
  
  span(&parser->tokens, start);
  leave(parser);
}

static void parse_prim(struct parser_t *parser, pmath_bool_t prim_optional) {
  pmath_token_t tok;
  int prec;
  int start;
  int next;
  
  start = parser->tokens.pos;
  next  = next_token_pos(parser);
  
  if(next == parser->tokens.pos) {
    if(!prim_optional)
      handle_error(parser);
    return;
  }
  
  if(!enter(parser)) {
    parse_skip_until_rightfence(parser);
    return;
  }
  
  tok = token_analyse(parser, next, &prec);
  
  switch(tok) {
    case PMATH_TOK_DIGIT:
    case PMATH_TOK_STRING:
    case PMATH_TOK_NAME:
    case PMATH_TOK_NAME2: {
        parser->spans->items[start] |= 2; //operand start
        skip_to(parser, start, next, TRUE);
        
        next = next_token_pos(parser);
        tok  = token_analyse(parser, next, &prec);
        
        if(tok == PMATH_TOK_COLON) { // x:pattern
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
        
        if(tok == PMATH_TOK_NAME) {
          skip_to(parser, start, next, TRUE);
          
          next = next_token_pos(parser);
          tok = token_analyse(parser, next, &prec);
        }
        
        if(tok == PMATH_TOK_COLON) { // ~x:type  ~:type
          skip_to(parser, -1, next, FALSE);
          parse_prim(parser, FALSE);
        }
      } break;
      
    case PMATH_TOK_SLOT: {
        parser->spans->items[start] |= 2; //operand start
        skip_to(parser, start, next, TRUE);
        
        next = next_token_pos(parser);
        tok  = token_analyse(parser, next, &prec);
        
        if(tok == PMATH_TOK_DIGIT) {
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
        if(tok == PMATH_TOK_NAME) {
          skip_to(parser, start, next, TRUE);
          
          next = next_token_pos(parser);
          tok  = token_analyse(parser, next, &prec);
        }
        else
          handle_error(parser);
          
        if(tok == PMATH_TOK_COLON) { // ?x:type  ?:type
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
        
        if(tok2 != PMATH_TOK_RIGHT) {
        
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
    case PMATH_TOK_INTEGRAL: {
      DO_PREFIX:
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

static void parse_rest(struct parser_t *parser, int lhs, int min_prec) {
  int last_tok_start = lhs;
  int last_tok_end   = lhs;
  int last_prec      = -1;
  int cur_prec;
  pmath_token_t tok;
  int rhs;
  int next;
  
  if(!enter(parser)) {
    parse_skip_until_rightfence(parser);
    return;
  }
  
  next = next_token_pos(parser);
  while(next != parser->tokens.pos) {
    tok = token_analyse(parser, next, &cur_prec);
    
    switch(tok) {
      case PMATH_TOK_POSTFIX:
      case PMATH_TOK_POSTFIX_OR_PREFIX: {
          if(cur_prec >= min_prec) {
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
            
          if(PMATH_PREC_INC >= min_prec) { // x++
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
          if(prec != PMATH_PREC_ASS) {
            handle_error(parser);
          }
          else {
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
          
        } continue;
        
      case PMATH_TOK_LEFTCALL:
        if(parser->last_space_start == parser->tokens.pos) { // no preceding space
          pmath_bool_t doublesq = FALSE;
          
          if(PMATH_PREC_CALL < min_prec)
            break;
            
          if(last_prec == cur_prec) {
            if( cur_prec != PMATH_PREC_OR                               &&
                cur_prec != PMATH_PREC_AND                              &&
                cur_prec != PMATH_PREC_REL                              &&
                cur_prec != PMATH_PREC_ADD                              &&
                cur_prec != PMATH_PREC_MUL                              &&
                !same_token(parser, last_tok_start, last_tok_end, next) &&
                (last_tok_start + 1 != last_tok_end ||
                 parser->tokens.str[last_tok_start] != '.'))
            {
              span(&parser->tokens, lhs);
            }
          }
          else if(last_prec >= 0)
            span(&parser->tokens, lhs);
            
          last_tok_start = parser->tokens.pos;
          last_tok_end   = next;
          
          ++parser->fencelevel;
          
          if( next == parser->tokens.pos + 1                &&
              next < parser->tokens.len                     &&
              parser->tokens.str[parser->tokens.pos] == '[' &&
              parser->tokens.str[next]               == '[') // [[
          {
            parser->spans->items[parser->tokens.pos] &= ~1; // no token end
            doublesq = TRUE;
            ++next;
          }
          
          skip_to(parser, -1, next, FALSE);
          next = next_token_pos(parser);
          if(token_analyse(parser, next, &cur_prec) != PMATH_TOK_RIGHT)
            parse_sequence(parser);
            
          next = next_token_pos(parser);
          
          --parser->fencelevel;
          
          if(token_analyse(parser, next, &cur_prec) == PMATH_TOK_RIGHT) {
            if(doublesq) {
              if( next == parser->tokens.pos + 1                &&
                  next < parser->tokens.len                     &&
                  parser->tokens.str[parser->tokens.pos] == ']' &&
                  parser->tokens.str[next]               == ']')
              {
                parser->spans->items[parser->tokens.pos] &= ~1; // no token end
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
            
          if(parser->last_was_newline)
            handle_newline_multiplication(parser);
            
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
          if(!pmath_token_maybe_first(tok2)) {
          
            if( tok == PMATH_TOK_BINARY_LEFT_AUTOARG ||
                tok == PMATH_TOK_NARY_AUTOARG)
            {
              if(cur_prec < last_prec) {
                int pos = parser->tokens.pos;
                parser->tokens.pos = oldpos;
                span(&parser->tokens, lhs);
                parser->tokens.pos = pos;
              }
              
              span(&parser->tokens, lhs);
            }
            else {
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
          if(tok == PMATH_TOK_BINARY_LEFT_AUTOARG) {
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
          if(cur_prec >= min_prec) {
            int oldpos;
          NARY:
          
            if(last_prec == cur_prec) {
              if( cur_prec != PMATH_PREC_OR  &&
                  cur_prec != PMATH_PREC_AND &&
                  cur_prec != PMATH_PREC_REL &&
                  cur_prec != PMATH_PREC_ADD &&
                  cur_prec != PMATH_PREC_MUL &&
                  !same_token(parser, last_tok_start, last_tok_end, next))
              {
                span(&parser->tokens, lhs);
              }
            }
            else //if(last_prec >= 0)
              span(&parser->tokens, lhs);
              
          BINARY:
          
            last_tok_start = parser->tokens.pos;
            last_tok_end   = next;
            
            skip_to(parser, -1, next,
                    (tok == PMATH_TOK_NARY_AUTOARG ||
                     tok == PMATH_TOK_BINARY_LEFT_AUTOARG));
                     
            rhs = parser->tokens.pos;
            parse_prim(
              parser,
              (tok == PMATH_TOK_NARY_AUTOARG ||
               tok == PMATH_TOK_BINARY_LEFT_AUTOARG));
               
            next = next_token_pos(parser);
            oldpos = parser->tokens.pos;
            while(oldpos != next) {
              int next_prec;
              tok = token_analyse(parser, next, &next_prec);
              oldpos = next;
              
              if(tok == PMATH_TOK_BINARY_RIGHT) {
                if(next_prec >= cur_prec) {
                  parse_rest(parser, rhs, next_prec); // =: rhs
                  
                  next = next_token_pos(parser);
                  continue;
                }
                
                break;
              }
              
              if(tok == PMATH_TOK_PLUSPLUS) {
                if(!plusplus_is_infix(parser, next))
                  next_prec = PMATH_PREC_INC;
              }
              
              if(pmath_token_maybe_first(tok)) {
              
                if( (tok != PMATH_TOK_LEFTCALL ||
                     parser->last_space_start == parser->tokens.pos) &&
                    pmath_token_maybe_rest(tok))
                {
                  if(next_prec > cur_prec) {
                    parse_rest(parser, rhs, next_prec); // =: rhs
                    
                    next = next_token_pos(parser);
                    continue;
                  }
                }
                else if(PMATH_PREC_MUL > cur_prec) {
                  parse_rest(parser, rhs, PMATH_PREC_MUL); // =: rhs
                  
                  next = next_token_pos(parser);
                  continue;
                }
                
                break;
              }
              
              if(pmath_token_maybe_rest(tok)) {
                if(next_prec > cur_prec) {
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

static void parse_textline(struct parser_t *parser) {
  pmath_token_t tok;
  int i;
  int start = parser->tokens.pos;
  
  if(parser->tokens.str[parser->tokens.pos] == '"') {
    parse_prim(parser, FALSE);
    return;
  }
  
  i = next_token_pos(parser);
  tok  = token_analyse(parser, i, NULL);
  if(tok == PMATH_TOK_LEFT) {
    parse_prim(parser, FALSE);
    return;
  }
  
  while( parser->tokens.pos < parser->tokens.len &&
         parser->tokens.str[parser->tokens.pos] != '\n')
  {
    switch(parser->tokens.str[parser->tokens.pos]) {
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
    if( tok == PMATH_TOK_NAME  ||
        tok == PMATH_TOK_NAME2 ||
        tok == PMATH_TOK_DIGIT)
    {
      ++parser->tokens.pos;
      continue;
    }
    
    break;
  }
  
  for(i = start; i < parser->tokens.pos; ++i)
    parser->spans->items[i] &= ~1; // no token end
    
  parser->spans->items[parser->tokens.pos - 1] |= 1; // token end
  parser->spans->items[start] |= 2; //operand start
  
  span(&parser->tokens, start);
  skip_space(parser, start, TRUE);
}

//} ... parsing

//{ group spans ...

struct group_t {
  pmath_span_array_t                 *spans;
  pmath_string_t                      string;
  const uint16_t                     *str;
  //int                                 pos;
  struct pmath_text_position_t        tp;
  struct pmath_text_position_t        tp_before_whitespace;
  struct pmath_boxes_from_spans_ex_t  settings;
};

static void increment_text_position_to(struct group_t *group, int end) {
  if(group->tp.index > end){
    assert(group->tp.index <= end && "cannot decrement text pointer");
  }
  
  while(group->tp.index < end) {
    if(group->str[group->tp.index++] == '\n') {
      group->tp.line++;
      group->tp.line_start_index = group->tp.index;
    }
  }
  
  group->tp_before_whitespace = group->tp;
}

static void increment_text_position(struct group_t *group) {
  increment_text_position_to(group, group->tp.index + 1);
}

// Example code:
//    
//    1 * F(2) /*comment*/   + 3
//                           ^-tp
//             ^-tp_before_whitespace
//                        ^-end
//     \_________________/ 
//
// Here, we want the debug range info for the selected span \___/ to end after 
// the comment, but before the plus sign. So we increment tp_before_whitespace
// to end.
static void check_tp_before_whitespace(struct group_t *group, int end) {
  if(group->tp_before_whitespace.index != end) {
    const struct pmath_text_position_t old_tp = group->tp;
    
    group->tp = group->tp_before_whitespace;
    increment_text_position_to(group, end);
    
    group->tp = old_tp;
    
//    pmath_debug_print("[tp_b_w = %d (%d:%d) != %d = end]\n", 
//      group->tp_before_whitespace.index,
//      group->tp_before_whitespace.line,
//      group->tp_before_whitespace.index - group->tp_before_whitespace.line_start_index,
//      end);
  }
}

static void skip_whitespace(struct group_t *group) {
  const struct pmath_text_position_t tp_before_whitespace = group->tp;
  
  for(;;) {
    while( group->tp.index < group->spans->length &&
           pmath_token_analyse(&group->str[group->tp.index], 1, NULL) == PMATH_TOK_SPACE)
    {
      increment_text_position(group);
    }
    
    if( group->tp.index < group->spans->length &&
        group->str[group->tp.index] == '\\')
    {
      if( group->tp.index + 1 == group->spans->length ||
          group->str[group->tp.index + 1] <= ' ')
      {
        increment_text_position(group);
        continue;
      }
    }
    
    if( group->tp.index + 1 < group->spans->length &&
        group->str[group->tp.index]     == '/'     &&
        group->str[group->tp.index + 1] == '*'     &&
        !SPAN_TOK(group->spans->items[group->tp.index]))
    {
      pmath_span_t *s = SPAN_PTR(group->spans->items[group->tp.index]);
      if(s) {
        while(s->next)
          s = s->next;
          
        increment_text_position_to(group, s->end + 1);
      }
      else { // increment by 2
        increment_text_position(group);
        increment_text_position(group);
      }
      
      continue;
    }
    
    group->tp_before_whitespace = tp_before_whitespace;
    return;
  }
}

static void write_to_str(pmath_string_t *result, const uint16_t *data, int len) {
  *result = pmath_string_insert_ucs2(
              *result,
              pmath_string_length(*result),
              data,
              len);
}

static void emit_span(pmath_span_t *span, struct group_t *group) {
  const struct pmath_text_position_t span_start = group->tp;
  
  if(!span) {
    if(group->str[group->tp.index] == PMATH_CHAR_BOX) {
      if(group->settings.box_at_index)
        pmath_emit(
          group->settings.box_at_index(group->tp.index, group->settings.data),
          PMATH_NULL);
      else
        pmath_emit(PMATH_NULL, PMATH_NULL);
        
      increment_text_position(group);
    }
    else {
      pmath_t result;
      
      while( group->tp.index < group->spans->length &&
             !SPAN_TOK(group->spans->items[group->tp.index]))
      {
        increment_text_position(group);
      }
      
      increment_text_position(group);
      
      result = pmath_string_part(
                 pmath_ref(group->string),
                 span_start.index,
                 group->tp.index - span_start.index);
                 
      if(group->settings.add_debug_info) {
        result = group->settings.add_debug_info(
                   result,
                   &span_start,
                   &group->tp,
                   group->settings.data);
      }
      
      pmath_emit(result, PMATH_NULL);
    }
    
    if(group->settings.flags & PMATH_BFS_PARSEABLE)
      skip_whitespace(group);
      
    return;
  }
  
  if( /*(group->flags & PMATH_BFS_PARSEABLE) && */
    !span->next &&
    group->str[group->tp.index] == '"')
  {
    pmath_string_t result = PMATH_NULL;
    struct pmath_text_position_t start = group->tp;
    
    if(group->settings.flags & PMATH_BFS_USECOMPLEXSTRINGBOX) {
      pmath_t all;
      pmath_gather_begin(PMATH_NULL);
      
      while(group->tp.index <= span->end) {
        if(group->str[group->tp.index] == PMATH_CHAR_BOX) {
          pmath_t box;
          
          if(start.index < group->tp.index) {
            pmath_t pre = pmath_string_part(
                            pmath_ref(group->string),
                            start.index,
                            group->tp.index - start.index);
                            
            if(group->settings.add_debug_info) {
              pre = group->settings.add_debug_info(
                      pre,
                      &start,
                      &group->tp,
                      group->settings.data);
            }
            
            pmath_emit(pre, PMATH_NULL);
          }
          
          if(group->settings.box_at_index)
            box = group->settings.box_at_index(group->tp.index, group->settings.data);
          else
            box = PMATH_NULL;
            
          pmath_emit(box, PMATH_NULL);
          
          increment_text_position(group);
          start = group->tp;
        }
        else
          increment_text_position(group);
      }
      
      if(start.index < group->tp.index) {
        pmath_t rest = pmath_string_part(
                         pmath_ref(group->string),
                         start.index,
                         group->tp.index - start.index);
                         
        if(group->settings.add_debug_info) {
          rest = group->settings.add_debug_info(
                   rest,
                   &start,
                   &group->tp,
                   group->settings.data);
        }
        
        pmath_emit(rest, PMATH_NULL);
      }
      
      all = pmath_gather_end();
      all = pmath_expr_set_item(all, 0, pmath_ref(PMATH_SYMBOL_COMPLEXSTRINGBOX));
      
      check_tp_before_whitespace(group, span->end + 1);
      
      if(group->settings.add_debug_info) {
        all = group->settings.add_debug_info(
                all,
                &span_start,
                &group->tp_before_whitespace, //span->end + 1,
                group->settings.data);
      }
      
      pmath_emit(
        all,
        PMATH_NULL);
    }
    else {
      while(group->tp.index <= span->end) {
        if(group->str[group->tp.index] == PMATH_CHAR_BOX) {
          static const uint16_t left_box_char  = PMATH_CHAR_LEFT_BOX;
          static const uint16_t right_box_char = PMATH_CHAR_RIGHT_BOX;
          pmath_t box;
          
          if(start.index < group->tp.index) {
            if(!pmath_is_null(result)) {
              result = pmath_string_insert_ucs2(
                         result,
                         pmath_string_length(result),
                         group->str + start.index,
                         group->tp.index - start.index);
            }
            else {
              result = pmath_string_part(
                         pmath_ref(group->string),
                         start.index,
                         group->tp.index - start.index);
            }
          }
          
          result = pmath_string_insert_ucs2(
                     result,
                     pmath_string_length(result),
                     &left_box_char,
                     1);
                     
          if(group->settings.box_at_index)
            box = group->settings.box_at_index(group->tp.index, group->settings.data);
          else
            box = PMATH_NULL;
            
          pmath_write(
            box,
            PMATH_WRITE_OPTIONS_FULLSTR | PMATH_WRITE_OPTIONS_INPUTEXPR,
            (void( *)(void *, const uint16_t *, int))write_to_str,
            &result);
            
          pmath_unref(box);
          
          result = pmath_string_insert_ucs2(
                     result,
                     pmath_string_length(result),
                     &right_box_char,
                     1);
          
          increment_text_position(group);
          start = group->tp;
        }
        else
          increment_text_position(group);
      }
      
      if(pmath_is_null(result)) {
        result = pmath_string_part(
                   pmath_ref(group->string),
                   start.index,
                   group->tp.index - start.index);
      }
      else if(start.index < group->tp.index) {
        result = pmath_string_insert_ucs2(
                   result,
                   pmath_string_length(result),
                   group->str + start.index,
                   group->tp.index - start.index);
      }
      
      check_tp_before_whitespace(group, span->end + 1);
      
      if(group->settings.add_debug_info) {
        result = group->settings.add_debug_info(
                   result,
                   &span_start,
                   &group->tp, //span->end + 1,
                   group->settings.data);
      }
      
      pmath_emit(
        result,
        PMATH_NULL);
    }
    
    // Is this save? Might have skipped some white space ...
    increment_text_position_to(group, span->end + 1);
    if(group->settings.flags & PMATH_BFS_PARSEABLE)
      skip_whitespace(group);
      
    return;
  }
  else {
    pmath_t expr;
    
    pmath_gather_begin(PMATH_NULL);
    emit_span(span->next, group);
    while(group->tp.index <= span->end)
      emit_span(SPAN_PTR(group->spans->items[group->tp.index]), group);
      
    expr = pmath_gather_end();
    check_tp_before_whitespace(group, span->end + 1);
      
    if(group->settings.add_debug_info) {
      expr = group->settings.add_debug_info(
               expr,
               &span_start,
               &group->tp_before_whitespace,//span->end + 1,
               group->settings.data);
    }
    
    pmath_emit(expr, PMATH_NULL);
  }
}

PMATH_API pmath_t pmath_boxes_from_spans(
  pmath_span_array_t   *spans,
  pmath_string_t        string,
  pmath_bool_t          parseable,
  pmath_t             (*box_at_index)(int, void *),
  void                 *data
) {
  struct pmath_boxes_from_spans_ex_t settings;
  
  memset(&settings, 0, sizeof(settings));
  settings.size         = sizeof(settings);
  settings.data         = data;
  settings.box_at_index = box_at_index;
  
  if(parseable)
    settings.flags = PMATH_BFS_PARSEABLE;
    
  return pmath_boxes_from_spans_ex(spans, string, &settings);
}

PMATH_API
PMATH_ATTRIBUTE_USE_RESULT
pmath_t pmath_boxes_from_spans_ex(
  pmath_span_array_t                 *spans,
  pmath_string_t                      string, // wont be freed
  struct pmath_boxes_from_spans_ex_t *settings
) {
  struct group_t group;
  pmath_expr_t result;
  
  if(!spans || spans->length != pmath_string_length(string))
    return PMATH_C_STRING("");//pmath_ref(_pmath_object_emptylist);
    
  group.spans                 = spans;
  group.string                = string;
  group.str                   = pmath_string_buffer(&string);
  
  //group.pos                   = 0;
  memset(&group.tp, 0, sizeof(group.tp));
  
  memset(&group.settings, 0, sizeof(group.settings));
  if(settings) {
    group.settings.size = settings->size;
    if(group.settings.size > sizeof(group.settings))
      group.settings.size  = sizeof(group.settings);
      
    memcpy(&group.settings, settings, group.settings.size);
  }
  
  pmath_gather_begin(PMATH_NULL);
  
  if(group.settings.flags & PMATH_BFS_PARSEABLE)
    skip_whitespace(&group);
    
  while(group.tp.index < spans->length)
    emit_span(SPAN_PTR(spans->items[group.tp.index]), &group);
    
  result = pmath_expr_set_item(pmath_gather_end(), 0, PMATH_NULL);
  if(pmath_expr_length(result) == 1) {
    pmath_t tmp = pmath_expr_get_item(result, 1);
    pmath_unref(result);
    return tmp;
  }
  return result;
}

//} ... group spans

//{ ungroup span_array ...

typedef struct {
  pmath_span_array_t  *spans;
  uint16_t            *str;
  int                  pos;
  void               (*make_box)(int, pmath_t, void *);
  void                *data;
  pmath_bool_t         split_tokens;
  
  int comment_level;
} _pmath_ungroup_t;

static int ungrouped_string_length(pmath_t box) { // box wont be freed
  if(pmath_is_string(box)) {
    const uint16_t *str = pmath_string_buffer(&box);
    int i, k, len, result;
    
    len = pmath_string_length(box);
    result = 0;
    i = 0;
    while(i < len) {
      if(str[i] == PMATH_CHAR_LEFT_BOX) {
        ++i;
        k = 1;
        while(i < len) {
          if(str[i] == PMATH_CHAR_LEFT_BOX) {
            ++k;
          }
          else if(str[i] == PMATH_CHAR_RIGHT_BOX) {
            if(--k == 0)
              break;
          }
          ++i;
        }
        if(i < len)
          ++i;
          
        ++result;
      }
      else {
        ++i;
        ++result;
      }
    }
    return result;
  }
  
  if(pmath_is_expr(box)) {
    int result = 0;
    size_t i;
    pmath_t head = pmath_expr_get_item(box, 0);
    pmath_unref(head);
    
    if(pmath_is_null(head) || pmath_same(head, PMATH_SYMBOL_LIST)) {
      for(i = pmath_expr_length(box); i > 0; --i) {
        pmath_t boxi = pmath_expr_get_item(box, i);
        
        result += ungrouped_string_length(boxi);
        
        pmath_unref(boxi);
      }
      return result;
    }
    
    if(pmath_same(head, PMATH_SYMBOL_COMPLEXSTRINGBOX)) {
      for(i = pmath_expr_length(box); i > 0; --i) {
        pmath_t boxi = pmath_expr_get_item(box, i);
        
        
        if(pmath_is_string(boxi))
          result += ungrouped_string_length(boxi);
        else
          result += 1;
          
        pmath_unref(boxi);
      }
      return result;
    }
  }
  
  return 1;
}

static void ungroup(
  _pmath_ungroup_t *g,
  pmath_t           box // will be freed
) {
  if(pmath_is_string(box)) {
    const uint16_t *str;
    int len, i, j, start;
    struct scanner_t tokens;
    
    len = pmath_string_length(box);
    if(len == 0) {
      pmath_unref(box);
      return;
    }
    
    str = pmath_string_buffer(&box);
    start = g->pos;
    
    i = 0;
    while(i < len) {
      if(str[i] == PMATH_CHAR_LEFT_BOX) {
        pmath_string_t box_in_str;
        int k = 1;
        int l;
        
        j = ++i;
        while(j < len) {
          if(str[j] == PMATH_CHAR_LEFT_BOX) {
            ++k;
          }
          else if(str[j] == PMATH_CHAR_RIGHT_BOX) {
            if(--k == 0)
              break;
          }
          ++j;
        }
        
        l = j - i;
        
        box_in_str = pmath_string_part(pmath_ref(box), i, l);
        
        if(g->make_box) {
          g->make_box(
            g->pos,
            pmath_parse_string(box_in_str),
            g->data);
        }
        
        i = i + l < len ? i + l + 1 : i + l;
        if(g->pos > 0)
          g->spans->items[g->pos - 1] |= 1; // token end
        g->spans->items[g->pos] |= 1; // token end
        g->str[g->pos++] = PMATH_CHAR_BOX;
      }
      else if(str[i] == PMATH_CHAR_BOX) {
        g->str[g->pos++] = 0xFFFF;
        i++;
      }
      else
        g->str[g->pos++] = str[i++];
    }
    
    if(g->split_tokens) {
      memset(&tokens, 0, sizeof(tokens));
      tokens.str           = g->str;
      tokens.span_items    = g->spans->items;
      tokens.pos           = start;
      tokens.len           = g->pos;
      tokens.comment_level = g->comment_level;
      while(tokens.pos < tokens.len)
        scan_next(&tokens, NULL);
        
      g->comment_level = tokens.comment_level;
    }
    else {
      g->spans->items[start]      |= 2; // operand start
      g->spans->items[g->pos - 1] |= 1; // token end
    }
    
    pmath_unref(box);
    return;
  }
  
  if(pmath_is_expr(box)) {
    pmath_t head = pmath_expr_get_item(box, 0);
    pmath_unref(head);
    if( pmath_is_null(head) ||
        pmath_same(head, PMATH_SYMBOL_LIST))
    {
      size_t i, len;
      int start = g->pos;
      int old_comment_level = g->comment_level;
      
      len = pmath_expr_length(box);
      
      if(len == 2) {
        pmath_t first  = pmath_expr_get_item(box, 1);
        pmath_t second = pmath_expr_get_item(box, 2);
        
        if( pmath_is_string(first) &&
            pmath_is_string(second) &&
            (pmath_string_equals_latin1(first, "<<") ||
             pmath_string_equals_latin1(first, "??")))
        {
          pmath_bool_t old_split_tokens = g->split_tokens;
          g->split_tokens = FALSE;
          
          ungroup(g, first);
          ungroup(g, second);
          
          g->split_tokens = old_split_tokens;
          goto AFTER_UNGROUP;
        }
        
        pmath_unref(first);
        pmath_unref(second);
      }
      
      for(i = 1; i <= len; ++i)
        ungroup(g, pmath_expr_get_item(box, i));
        
    AFTER_UNGROUP:
      g->spans->items[start] |= 2; // operand start
      
      g->comment_level = old_comment_level;
      
      if( len >= 1 &&
          start < g->pos &&
          pmath_same(head, PMATH_SYMBOL_LIST))
      {
        pmath_t first = pmath_expr_get_item(box, 1);
        
        pmath_span_t *old = SPAN_PTR(g->spans->items[start]);
        
        if(pmath_is_string(first)) {
          const uint16_t *fbuf = pmath_string_buffer(&first);
          int             flen = pmath_string_length(first);
          
          if(flen > 0 && fbuf[0] == '"' && (flen == 1 || fbuf[flen - 1] != '"')) {
            if(old) {
              old->end = g->pos - 1;
            }
            else {
              old = pmath_mem_alloc(sizeof(pmath_span_t));
              if(old) {
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
        
        if(!old || old->end != g->pos - 1) {
        
          pmath_bool_t have_mutliple_tokens = FALSE;
          int i;
          for(i = start; i < g->pos - 1; ++i) {
            if(1 == SPAN_TOK(g->spans->items[i])) {
              have_mutliple_tokens = TRUE;
              break;
            }
          }
          
          if(have_mutliple_tokens) {
            pmath_span_t *s = pmath_mem_alloc(sizeof(pmath_span_t));
            if(s) {
              s->next = old;
              s->end = g->pos - 1;
              g->spans->items[start] = (uintptr_t)s
                                       | SPAN_TOK(g->spans->items[start])
                                       | SPAN_OP( g->spans->items[start]);
            }
          }
        }
      }
      
      pmath_unref(box);
      return;
    }
    
    if(pmath_same(head, PMATH_SYMBOL_COMPLEXSTRINGBOX)) {
      size_t i, len;
      int start = g->pos;
      pmath_span_t *s;
      len = pmath_expr_length(box);
      
      for(i = 1; i <= len; ++i) {
        pmath_t boxi = pmath_expr_get_item(box, i);
        
        if(pmath_is_string(boxi)) {
          ungroup(g, boxi);
          continue;
        }
        
        if(g->make_box)
          g->make_box(g->pos, boxi, g->data);
          
        if(g->pos > 0)
          g->spans->items[g->pos - 1] |= 1; // token end
        g->spans->items[g->pos] |= 1; // token end
        g->str[g->pos++] = PMATH_CHAR_BOX;
      }
      
      if(g->pos > start) {
        s = pmath_mem_alloc(sizeof(pmath_span_t));
        if(s) {
          pmath_span_t *old = SPAN_PTR(g->spans->items[start]);
          
          while(old) {
            pmath_span_t *s = old->next;
            pmath_mem_free(old);
            old = s;
          }
          
          s->next = NULL;
          s->end = g->pos - 1;
          g->spans->items[start] = (uintptr_t)s | 2; // operand start
        }
        
        g->spans->items[g->pos - 1] |= 1; // token end
      }
      pmath_unref(box);
      return;
    }
  }
  
  if(g->make_box)
    g->make_box(g->pos, box, g->data);
  else
    pmath_unref(box);
    
  g->spans->items[g->pos] = 3; // operand start (2), token end (1)
  g->str[g->pos++] = PMATH_CHAR_BOX;
}

PMATH_API pmath_span_array_t *pmath_spans_from_boxes(
  pmath_t          boxes,  // will be freed
  pmath_string_t  *result_string,
  void           (*make_box)(int, pmath_t, void *),
  void            *data
) {
  _pmath_ungroup_t g;
  
  assert(result_string);
  
  g.spans = create_span_array(ungrouped_string_length(boxes));
  if(!g.spans) {
    pmath_unref(boxes);
    *result_string = pmath_string_new(0);
    return NULL;
  }
  
  *result_string = PMATH_FROM_PTR(_pmath_new_string_buffer(g.spans->length));
  if(pmath_is_null(*result_string)) {
    pmath_span_array_free(g.spans);
    return NULL;
  }
  
  g.str           = (uint16_t *)pmath_string_buffer(result_string);
  g.pos           = 0;
  g.make_box      = make_box;
  g.data          = data;
  g.split_tokens  = TRUE;
  g.comment_level = 0;
  
  ungroup(&g, boxes);
  
  if(g.spans->length > 0)
    g.spans->items[0] |= 2; // operand start
    
  assert(g.pos == g.spans->length);
  return g.spans;
}

//} ... ungroup spans

static void quiet_syntax_error(pmath_string_t code, int pos, void *flag, pmath_bool_t critical) {
  pmath_bool_t *have_critical = flag;
  
  if(critical)
    *have_critical = TRUE;
}

static pmath_t quiet_parse(pmath_t str) {
  pmath_bool_t error_flag = FALSE;
  pmath_t result;
  pmath_span_array_t *spans;
  struct pmath_boxes_from_spans_ex_t settings;
  pmath_t message_name;
  pmath_t on_off;
  
  spans = pmath_spans_from_string(
            &str,
            NULL,
            NULL,
            NULL,
            quiet_syntax_error,
            &error_flag);
            
  if(error_flag) {
    pmath_span_array_free(spans);
    pmath_unref(str);
    return PMATH_UNDEFINED;
  }
  
  memset(&settings, 0, sizeof(settings));
  settings.size  = sizeof(settings);
  settings.flags = PMATH_BFS_PARSEABLE;
  
  result = pmath_boxes_from_spans_ex(spans, str, &settings);
  
  pmath_span_array_free(spans);
  pmath_unref(str);
  
  message_name = pmath_expr_new_extended(
                   pmath_ref(PMATH_SYMBOL_MESSAGENAME), 2,
                   pmath_ref(PMATH_SYMBOL_MAKEEXPRESSION),
                   PMATH_C_STRING("inv"));
                   
  // Off(MakeExpression::inv)
  on_off = pmath_thread_local_save(
             message_name,
             pmath_ref(PMATH_SYMBOL_OFF));
             
  result = pmath_evaluate(
             pmath_expr_new_extended(
               pmath_ref(PMATH_SYMBOL_MAKEEXPRESSION), 1,
               result));
               
  pmath_unref(
    pmath_thread_local_save(
      message_name,
      on_off));
      
  pmath_unref(message_name);
  
  if(pmath_is_expr_of_len(result, PMATH_SYMBOL_HOLDCOMPLETE, 1)) {
    pmath_t value = pmath_expr_get_item(result, 1);
    pmath_unref(result);
    return value;
  }
  
  pmath_unref(result);
  return PMATH_UNDEFINED;
}

PMATH_API
pmath_t pmath_string_expand_boxes(
  pmath_string_t  s
) {
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
    
    while(i < len) {
      if(buf[i] == PMATH_CHAR_LEFT_BOX) {
        int k;
        int pre_start = start;
        int pre_end   = i;
        
        start = ++i;
        k = 1;
        while(i < len) {
          if(buf[i] == PMATH_CHAR_LEFT_BOX) {
            ++k;
          }
          else if(buf[i] == PMATH_CHAR_RIGHT_BOX) {
            if(--k == 0) {
              pmath_t box = pmath_string_part(pmath_ref(s), start, i - start);
              
              box = quiet_parse(box);
              
              if(!pmath_same(box, PMATH_UNDEFINED)) {
              
                if(pre_end > pre_start) {
                  pmath_emit(
                    pmath_string_part(pmath_ref(s), pre_start, pre_end - pre_start),
                    PMATH_NULL);
                }
                
                pmath_emit(box, PMATH_NULL); box = PMATH_NULL;
                
                start = ++i;
              }
              
              break;
            }
          }
          ++i;
        }
      }
      else
        ++i;
    }
    
    if(start < len) {
      pmath_emit(
        pmath_string_part(pmath_ref(s), start, len - start),
        PMATH_NULL);
    }
    
    pmath_unref(s);
    
    {
      pmath_expr_t result = pmath_gather_end();
      
      if(pmath_expr_length(result) == 1) {
        s = pmath_expr_get_item(result, 1);
        pmath_unref(result);
        return s;
      }
      
      result = pmath_expr_set_item(result, 0, pmath_ref(PMATH_SYMBOL_COMPLEXSTRINGBOX));
      return result;
    }
  }
}

static void syntax_error(pmath_string_t code, int pos, void *flag, pmath_bool_t critical) {
  pmath_bool_t *have_critical = flag;
  
  if(!*have_critical)
    pmath_message_syntax_error(code, pos, PMATH_NULL, 0);
    
  if(critical)
    *have_critical = TRUE;
}

PMATH_API pmath_t pmath_parse_string(
  pmath_string_t code
) {
  pmath_bool_t error_flag = FALSE;
  pmath_t result;
  
  // TODO: in debug build, add the C stack trace as debug info to the expression
  
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
    
  if(pmath_expr_length(result) == 1) {
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
) {
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
