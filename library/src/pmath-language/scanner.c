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

extern pmath_symbol_t pmath_System_HoldComplete;
extern pmath_symbol_t pmath_System_List;
extern pmath_symbol_t pmath_System_MakeExpression;
extern pmath_symbol_t pmath_System_MessageName;
extern pmath_symbol_t pmath_System_Off;
extern pmath_symbol_t pmath_System_Sequence;
extern pmath_symbol_t pmath_System_StringBox;

struct _pmath_span_t {
  int next_offset;
  int end;
};

struct _span_array_item_t {
  unsigned is_token_end : 1;
  unsigned is_operand_start : 1;
  unsigned span_index : 30;
};

PMATH_STATIC_ASSERT(sizeof(struct _span_array_item_t) == sizeof(unsigned int));

struct _pmath_span_array_t {
  size_t size;
  int length;
  int span_count;
  struct _span_array_item_t items[1];
};


struct scanner_t {
  const uint16_t            *str;
  int                        len;
  int                        pos;
  pmath_span_array_t        *spans;
  
  unsigned comment_level: 31;
  unsigned in_line_comment: 1;
  
  //pmath_bool_t in_comment;
  pmath_bool_t in_string;
  pmath_bool_t have_error;
};

struct parser_t {
  pmath_string_t code;
  int fencelevel;
  int stack_size;
  
  
  pmath_string_t    (*read_line)(void *);
  pmath_bool_t      (*subsuperscriptbox_at_index)(int, void *);
  pmath_string_t    (*underoverscriptbox_at_index)(int, void *);
  void              (*error)(pmath_string_t, int, void *, pmath_bool_t); //does not free 1st arg
  void               *data;
  
  struct scanner_t tokens;
  
  pmath_bool_t stack_error;
  pmath_bool_t tokenizing;
  int          last_space_start;
};


struct group_t {
  pmath_span_array_t                 *spans;
  pmath_string_t                      string;
  const uint16_t                     *str;
  //int                                 pos;
  struct pmath_text_position_t        tp;
  struct pmath_text_position_t        tp_before_whitespace;
  struct pmath_boxes_from_spans_ex_t  settings;
};

struct ungroup_t {
  pmath_span_array_t  *spans;
  uint16_t            *str;
  int                  pos;
  void               (*make_box)(int, pmath_t, void *);
  void                *data;
  pmath_bool_t         split_tokens;
  
  int comment_level;
};



#define ADDRESS_OF_FIRST_SPAN(ARR) \
  ((struct _pmath_span_t*) ROUND_UP( (size_t)&(ARR)->items[(ARR)->length] , sizeof(struct _pmath_span_t) ))

#define OFFSET_OF_FIRST_SPAN(LEN) \
  ROUND_UP( (size_t)&((struct _pmath_span_array_t*)0)->items[(LEN)] , sizeof(struct _pmath_span_t) )

static pmath_span_t *span_at(pmath_span_array_t *spans, int pos);
static size_t next_power_of_2(size_t x);
static pmath_span_array_t *create_span_array(int length, int spans_capacity);
static pmath_span_array_t *enlarge_span_array(pmath_span_array_t *span_array, int extra_len, int extra_spans);
static pmath_span_t *alloc_span(pmath_span_array_t **span_array, unsigned *new_index);


static void span(struct scanner_t *tokens, int start);
static void handle_error(     struct parser_t *parser);
static pmath_bool_t enter(    struct parser_t *parser);
static void         leave(    struct parser_t *parser);
static int next_token_pos(    struct parser_t *parser);
static pmath_bool_t read_more(struct parser_t *parser);
static void skip_space(       struct parser_t *parser, int span_start, pmath_bool_t optional);
static void skip_to(          struct parser_t *parser, int span_start, int next, pmath_bool_t optional);
static pmath_token_t scan_next_escaped_char(        struct scanner_t *tokens, struct parser_t *parser, uint32_t *chr);
static void          scan_next_backtick_insertion(  struct scanner_t *tokens, struct parser_t *parser);
static pmath_bool_t  scan_next_string(              struct scanner_t *tokens, struct parser_t *parser);
static pmath_bool_t  scan_number_base_specifier(    struct scanner_t *tokens, struct parser_t *parser);
static void          scan_float_decimal_digits_rest(struct scanner_t *tokens, struct parser_t *parser);
static void          scan_float_base36_digits_rest( struct scanner_t *tokens, struct parser_t *parser);
static void          scan_precision_specifier(      struct scanner_t *tokens, struct parser_t *parser);
static void          scan_number_exponent(          struct scanner_t *tokens, struct parser_t *parser);
static void          scan_number_radius(            struct scanner_t *tokens, struct parser_t *parser, pmath_bool_t is_base36);
static void          scan_next_number(              struct scanner_t *tokens, struct parser_t *parser);
static void          scan_next_as_name(             struct scanner_t *tokens, struct parser_t *parser);
static void          scan_next(                     struct scanner_t *tokens, struct parser_t *parser);
static pmath_token_t token_analyse(   struct parser_t *parser, int next, int *prec);
static int  prefix_precedence(        struct parser_t *parser, int next, int defprec);
static pmath_bool_t same_token(       struct parser_t *parser, int last_start, int last_next, int cur_next);
static pmath_bool_t plusplus_is_infix(struct parser_t *parser, int next);
static void skip_prim_newline(          struct parser_t *parser, pmath_bool_t prim_optional);
static void skip_prim_newline_impl(     struct parser_t *parser, int next);
static int  parse_prim_after_newline(   struct parser_t *parser, pmath_bool_t prim_optional);
static void parse_prim(                 struct parser_t *parser, pmath_bool_t prim_optional);
static void parse_rest(                 struct parser_t *parser, int lhs_start, int min_prec);
static void parse_sequence(             struct parser_t *parser);
static void parse_skip_until_rightfence(struct parser_t *parser);
static void parse_textline(             struct parser_t *parser);

static void increment_text_position_to(struct group_t *group, int end);
static void increment_text_position(   struct group_t *group);
static void check_tp_before_whitespace(struct group_t *group, int end);
static void skip_whitespace(struct group_t *group);
static void write_to_str(pmath_string_t *result, const uint16_t *data, int len);
static void emit_span(pmath_span_t *span, struct group_t *group);

static int ungrouped_string_length(pmath_t box);
static void ungroup(struct ungroup_t *g, pmath_t box);


//{ spans ...

PMATH_API pmath_span_t *pmath_span_at(pmath_span_array_t *spans, int pos) {
  if(!spans || pos < 0 || pos >= spans->length)
    return NULL;
    
  return span_at(spans, pos);
}

PMATH_API void pmath_span_array_free(pmath_span_array_t *spans) {
  if(!spans)
    return;
  
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
    if(!spans->items[pos].is_token_end) {
      pmath_debug_print("[missing token_end flag at end of span array]\n");
    }
    
    return TRUE;
  }
  
  return spans->items[pos].is_token_end != 0;
}

PMATH_API pmath_bool_t pmath_span_array_is_operand_start(
  pmath_span_array_t *spans,
  int pos
) {
  if(!spans || pos < 0 || pos >= spans->length)
    return FALSE;
    
  return spans->items[pos].is_operand_start != 0;
}

PMATH_API pmath_span_t *pmath_span_next(pmath_span_t *span) {
  if(!span)
    return NULL;
  if(span->next_offset == 0)
    return NULL;
  return span + span->next_offset;
}

PMATH_API int pmath_span_end(pmath_span_t *span) {
  if(!span)
    return 0;
  return span->end;
}

static pmath_span_t *span_at(pmath_span_array_t *spans, int pos) {
  size_t span_index = spans->items[pos].span_index;
  if(span_index == 0)
    return NULL;
  
  return ADDRESS_OF_FIRST_SPAN(spans) + (span_index - 1);
}

static size_t next_power_of_2(size_t x) {
	x -= 1;
	x |= (x >> 1);
	x |= (x >> 2);
	x |= (x >> 4);
	x |= (x >> 8);
	x |= (x >> 16);
#if PMATH_64BIT
	x |= (x >> 32);
#endif
	return x + 1;
}

static pmath_span_array_t *create_span_array(int length, int spans_capacity) {
  pmath_span_array_t *result;
  size_t size;
  
  assert(length >= 0);
  assert(spans_capacity >= 0);
  
  if(length == 0 && spans_capacity == 0)
    return NULL;
  
  //size = sizeof(pmath_span_array_t) + (size_t)(length - 1) * sizeof(struct _span_array_item_t);
  size = OFFSET_OF_FIRST_SPAN(length) + (size_t)spans_capacity * sizeof(struct _pmath_span_t);
  size = next_power_of_2(size);
  
  result = (pmath_span_array_t *)pmath_mem_alloc(size);
             
  if(!result)
    return NULL;
  
  memset(result, 0, size);
  result->size = size;
  result->length = length;
  result->span_count = 0;
  
  return result;
}

static pmath_span_array_t *enlarge_span_array(
  pmath_span_array_t *span_array,
  int extra_len,
  int extra_spans
) {
  pmath_span_array_t *result;
  size_t size;
  int old_length;
  const struct _pmath_span_t *old_spans;
  struct _pmath_span_t *new_spans;
  
  assert(extra_len >= 0);
  assert(extra_spans >= 0);
  
  if(extra_len == 0 && extra_spans == 0)
    return span_array;
  
  if(!span_array) 
    return create_span_array(extra_len, extra_spans);
    
  size = OFFSET_OF_FIRST_SPAN(span_array->length + extra_len)
           + (size_t)(span_array->span_count + extra_spans) * sizeof(struct _pmath_span_t);
  size = next_power_of_2(size);
  
  result = pmath_mem_realloc_no_failfree(span_array, size);
  if(!result)
    return span_array;
  
  old_spans = ADDRESS_OF_FIRST_SPAN(result);
  old_length = result->length;
  result->length += extra_len;
  result->size = size;
  new_spans = ADDRESS_OF_FIRST_SPAN(result);
  
  memmove(new_spans, old_spans, (size_t)result->span_count * sizeof(struct _pmath_span_t));

  memset(result->items + old_length, 0, extra_len * sizeof(struct _span_array_item_t));
  memset(new_spans + result->span_count, 0xCD, ((char*)result + size) - (char*)(new_spans + result->span_count));

  return result;
}

static pmath_span_t *alloc_span(pmath_span_array_t **span_array, unsigned *new_index) {
  struct _pmath_span_t *spans;
  pmath_span_t *span;
  
  assert(span_array);
  assert(*span_array);
  assert(new_index);
  
  *new_index = 0;
  
  spans = ADDRESS_OF_FIRST_SPAN(*span_array);
  if((*span_array)->size < (size_t)((char*)&spans[(*span_array)->span_count + 1] - (char*)(*span_array))) {
    *span_array = enlarge_span_array(*span_array, 0, 1);
    spans = ADDRESS_OF_FIRST_SPAN(*span_array);

    if((*span_array)->size < (size_t)((char*)&spans[(*span_array)->span_count + 1] - (char*)(*span_array)))
      return NULL;
  }
  
  (*span_array)->span_count++;
  *new_index = (*span_array)->span_count;
  
  span = &spans[*new_index - 1];
  span->end = 0;
  span->next_offset = 0;
  return span;
}

//} ... spans

//{ parsing ...

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
  parser.tokens.spans = create_span_array(parser.tokens.len, 0);
  parser.fencelevel = 0;
  parser.stack_size = 0;
  
  if(!parser.tokens.spans)
    return NULL;
    
  parser.read_line                   = line_reader;
  parser.subsuperscriptbox_at_index  = subsuperscriptbox_at_index;
  parser.underoverscriptbox_at_index = underoverscriptbox_at_index;
  parser.error                       = error;
  parser.data                        = data;
  parser.tokens.comment_level        = 0;
  parser.tokens.in_string            = FALSE;
  parser.tokens.have_error           = FALSE;
  parser.stack_error                 = FALSE;
  parser.last_space_start            = 0;
    
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
        skip_to(&parser, -1, next_token_pos(&parser), TRUE);
      else
        handle_error(&parser);
    }
    
    if(tmp == parser.tokens.pos)
      skip_to(&parser, -1, next_token_pos(&parser), FALSE);
  }
  
  *code = parser.code;
  return parser.tokens.spans;
}

static void span(struct scanner_t *tokens, int start) {
  int i, end;
  
  if(!tokens->spans)
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
    
  if( span_at(tokens->spans, start) && span_at(tokens->spans, start)->end >= end)
    return;
  
  for(i = start; i < end; ++i)
    if(tokens->spans->items[i].is_token_end)
      goto HAVE_MULTIPLE_TOKENS;
      
  return;
  
HAVE_MULTIPLE_TOKENS: ;
  {
    unsigned span_index;
    pmath_span_t *next;
    pmath_span_t *s = alloc_span(&tokens->spans, &span_index);
    if(!s)
      return;
    
    next = span_at(tokens->spans, start);
    //s->next = next;
    s->next_offset = next ? (int)(next - s) : 0;
    s->end = end;
    tokens->spans->items[start].span_index = span_index;
  }
}

static void handle_error(struct parser_t *parser) {
  if(/*parser->tokens.have_error || */ parser->tokens.comment_level > 0 || parser->tokens.in_line_comment)
    return;
    
  parser->tokens.have_error = TRUE;
  if(parser->error)
    parser->error(parser->code, parser->tokens.pos, parser->data, TRUE);
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

static int next_token_pos(struct parser_t *parser) {
  int next;
  struct _pmath_span_t *span;
  
  if(parser->tokens.pos == parser->tokens.len)
    return parser->tokens.pos;
    
  span = span_at(parser->tokens.spans, parser->tokens.pos);
  
  if(span) {
    assert(span->end < parser->tokens.len);
    return span->end + 1;
  }
  
  next = parser->tokens.pos;
  while( next < parser->tokens.len &&
         !pmath_span_array_is_token_end(parser->tokens.spans, next))
  {
    ++next;
  }
  
  if(next < parser->tokens.len)
    ++next;
    
  return next;
}

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
  parser->tokens.spans = enlarge_span_array(parser->tokens.spans, extralen, 0);
  if( !parser->tokens.spans ||
      parser->tokens.spans->length != parser->tokens.len + extralen)
  {
    pmath_unref(newline);
    handle_error(parser);
    return FALSE;
  }
  
  parser->code = pmath_string_insert_latin1(parser->code, INT_MAX, "\n", 1);
  parser->code = pmath_string_concat(parser->code, newline);
  
  if(parser->tokens.spans->length != pmath_string_length(parser->code)) {
    pmath_unref(newline);
    handle_error(parser);
    return FALSE;
  }
  
  parser->tokens.str = pmath_string_buffer(&parser->code);
  parser->tokens.len = pmath_string_length(parser->code);
  return TRUE;
}

static void skip_space(struct parser_t *parser, int span_start, pmath_bool_t optional) {
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
      
      span = span_at(parser->tokens.spans, parser->tokens.pos);
      
      if(span) {
        if( parser->tokens.pos < parser->tokens.len &&
            parser->tokens.str[parser->tokens.pos] == '%')
        {
          parser->tokens.pos = span->end + 1;
          continue;
        }
        else if( parser->tokens.pos + 1 < parser->tokens.len       &&
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
        parser->tokens.str[parser->tokens.pos] == '\\')
    {
      if( parser->tokens.pos + 1 == parser->tokens.len ||
          parser->tokens.str[parser->tokens.pos + 1] <= ' ')
      {
        int i = parser->tokens.pos + 1;
        
        while( i < parser->tokens.len && parser->tokens.str[i] <= ' ')
          ++i;
        
        if(i == parser->tokens.len) 
          optional = FALSE;
        
        if(parser->last_space_start == parser->tokens.pos)
          parser->last_space_start = i;
        
        parser->tokens.pos = i;
      }
    }
    
    if( parser->tokens.pos < parser->tokens.len &&
        parser->tokens.str[parser->tokens.pos] == '%')
    {
      int last_space_start = parser->last_space_start;
      
      int start = parser->tokens.pos;
      int end = start + 1;
      int oldlen = parser->tokens.len;
      pmath_string_t (*old_read_line)(void *) = parser->read_line;
      parser->read_line = NULL;
      
      parser->tokens.comment_level++;
      skip_to(parser, -1, next_token_pos(parser), FALSE);
      while(end < parser->tokens.len && parser->tokens.str[end] != '\n')
        ++end;
        
      parser->tokens.len = end;
      while(parser->tokens.pos < end) {
        int tmp = parser->tokens.pos;
        parse_sequence(parser);
        if(tmp == parser->tokens.pos)
          ++parser->tokens.pos;
      }
      
      span(&parser->tokens, start);
      
      parser->tokens.len = oldlen;
      parser->tokens.comment_level--;
      parser->last_space_start = last_space_start;
      parser->read_line = old_read_line;
    }
    else if( parser->tokens.pos + 1 < parser->tokens.len &&
             //!parser->tokens.in_comment                        &&
             parser->tokens.str[parser->tokens.pos]     == '/' &&
             parser->tokens.str[parser->tokens.pos + 1] == '*')
    {
      int oldss = parser->last_space_start;
      
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
      
      parser->last_space_start = oldss;
    }
    else if( parser->tokens.pos == parser->tokens.len &&
             (!optional || parser->fencelevel > 0) &&
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
  endpos = (int)(endstr - tokens->str);
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

static void scan_next_backtick_insertion(struct scanner_t *tokens, struct parser_t *parser) {
  assert(!parser || &parser->tokens == tokens);
  assert(tokens->pos < tokens->len);
  assert(tokens->str[tokens->pos] == '`');
  
  ++tokens->pos;
  while( tokens->pos < tokens->len && pmath_char_is_digit(tokens->str[tokens->pos]))
    ++tokens->pos;
    
  if( tokens->pos < tokens->len && tokens->str[tokens->pos] == '`')
    ++tokens->pos;
}

// returns whether goto END_SCAN should be executed
static pmath_bool_t scan_next_string(struct scanner_t *tokens, struct parser_t *parser) {
  int k;
  int start;
  
  assert(!parser || &parser->tokens == tokens);
  assert(tokens->pos < tokens->len);
  assert(tokens->str[tokens->pos] == '"');
  
  if(tokens->in_string) {
    ++tokens->pos;
    return TRUE;
  }
  
  k = 0;
  start = tokens->pos;
  tokens->in_string = TRUE;
  
  ++tokens->pos;
  for(;;) {
    while( tokens->pos < tokens->len &&
           (k > 0 || tokens->str[tokens->pos] != '"'))
    {
      if(tokens->str[tokens->pos] == '\n' && tokens->in_line_comment)
        break;
        
      if( tokens->pos + 1 < tokens->len &&
          tokens->str[tokens->pos] == '*' &&
          tokens->str[tokens->pos + 1] == '/' &&
          tokens->comment_level > 0)
      {
        break;
      }
      
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
    
    if(tokens->pos < tokens->len || tokens->comment_level > 0 || tokens->in_line_comment)
      break;
      
    if(!read_more(parser)) {
      if(tokens->spans) {
        tokens->spans->items[start].is_token_end     = TRUE;
        tokens->spans->items[start].is_operand_start = TRUE;
      }
        
      span(tokens, start);
      return FALSE; // i.e. goto END_SCAN;
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
  
  if(tokens->spans) {
    tokens->spans->items[start].is_token_end = TRUE;
    tokens->spans->items[start].is_operand_start = TRUE;
  }
  
  span(tokens, start);
  return TRUE;
}

// Skip the "dd^^" base specifier if that is followed by a digit. Return if such a base specifier was found.
static pmath_bool_t scan_number_base_specifier(struct scanner_t *tokens, struct parser_t *parser) {
  int i;
  
  assert(tokens->pos < tokens->len);
  assert(pmath_char_is_digit(tokens->str[tokens->pos]));
  
  i = tokens->pos + 1;
  if(i < tokens->len && pmath_char_is_digit(tokens->str[i]))
    ++i;
    
  if(i + 2 >= tokens->len)
    return FALSE;
    
  if(tokens->str[i] != '^' || tokens->str[i + 1] != '^' || !pmath_char_is_36digit(tokens->str[i + 2]))
    return FALSE;
    
  tokens->pos = i + 2;
  return TRUE;
}

#define SCAN_FLOAT_DIGITS_REST(DIGIT_TEST) \
do {                                                                          \
  assert(tokens->pos < tokens->len);                                          \
  ++tokens->pos;                                                              \
  while(tokens->pos < tokens->len && DIGIT_TEST(tokens->str[tokens->pos]))    \
    ++tokens->pos;                                                            \
                                                                              \
  if( tokens->pos + 1 < tokens->len   &&                                      \
      tokens->str[tokens->pos] == '.' &&                                      \
      DIGIT_TEST(tokens->str[tokens->pos + 1]))                               \
  {                                                                           \
    tokens->pos += 2;                                                         \
    while( tokens->pos < tokens->len && DIGIT_TEST(tokens->str[tokens->pos])) \
      ++tokens->pos;                                                          \
  }                                                                           \
} while(0)

// Skip a "ddd.ddd" decimal floating point number. Skips at least one character.
static void scan_float_decimal_digits_rest(struct scanner_t *tokens, struct parser_t *parser) {
  SCAN_FLOAT_DIGITS_REST(pmath_char_is_digit);
}

// Skip a "xxx.xxx" base-36 floating point number. Skips at least one character.
static void scan_float_base36_digits_rest(struct scanner_t *tokens, struct parser_t *parser) {
  SCAN_FLOAT_DIGITS_REST(pmath_char_is_36digit);
}

#undef SCAN_FLOAT_DIGITS_REST

// Skip a "`ddd.ddd" precision specifier, if any. Digits are optional.
static void scan_precision_specifier(struct scanner_t *tokens, struct parser_t *parser) {
  if(tokens->pos >= tokens->len)
    return;
    
  if(tokens->str[tokens->pos] != '`')
    return;
    
  ++tokens->pos;
  
  // deprecated: ``
  if( tokens->pos < tokens->len && tokens->str[tokens->pos] == '`')
    ++tokens->pos;
    
  // deprecated: ``-ddd.ddd
  if( tokens->pos + 1 < tokens->len     &&
      (tokens->str[tokens->pos] == '+' ||
       tokens->str[tokens->pos] == '-') &&
      pmath_char_is_digit(tokens->str[tokens->pos + 1]))
  {
    ++tokens->pos;
  }
  
  if(tokens->pos >= tokens->len)
    return;
    
  if(!pmath_char_is_digit(tokens->str[tokens->pos]))
    return;
    
  scan_float_decimal_digits_rest(tokens, parser);
}

// Skip a "*^-ddd" exponent speifier, if any. The sign is optional.
static void scan_number_exponent(struct scanner_t *tokens, struct parser_t *parser) {
  if(tokens->pos + 2 >= tokens->len)
    return;
    
  if(tokens->str[tokens->pos] != '*' || tokens->str[tokens->pos + 1] != '^')
    return;
    
  if(pmath_char_is_digit(tokens->str[tokens->pos + 2])) {
    tokens->pos += 3;
    while(tokens->pos < tokens->len && pmath_char_is_digit(tokens->str[tokens->pos]))
      ++tokens->pos;
      
    return;
  }
  
  if(tokens->pos + 3 >= tokens->len)
    return;
    
  if(tokens->str[tokens->pos + 2] != '+' && tokens->str[tokens->pos + 2] != '-')
    return;
    
  if(!pmath_char_is_digit(tokens->str[tokens->pos + 3]))
    return;
    
  tokens->pos += 4;
  while(tokens->pos < tokens->len && pmath_char_is_digit(tokens->str[tokens->pos]))
    ++tokens->pos;
}

// Skip a "[+/-xxx.xxx*^-ddd]" real ball radius specification, if present.
static void scan_number_radius(struct scanner_t *tokens, struct parser_t *parser, pmath_bool_t is_base36) {
  int start = tokens->pos;
  
  if(tokens->pos >= tokens->len || tokens->str[tokens->pos] != '[')
    return;
    
  ++tokens->pos;
  
  if( tokens->pos + 2 < tokens->len &&
      tokens->str[tokens->pos]   == '+' &&
      tokens->str[tokens->pos + 1] == '/' &&
      tokens->str[tokens->pos + 2] == '-')
  {
    tokens->pos += 3;
  }
  else if(tokens->pos < tokens->len && tokens->str[tokens->pos] == PMATH_CHAR_PLUSMINUS) {
    ++tokens->pos;
  }
  else if(tokens->pos < tokens->len && tokens->str[tokens->pos] == '\\') {  // \[PlusMinus] escape sequence
    uint32_t chr;
    scan_next_escaped_char(tokens, parser, &chr);
    if(chr != PMATH_CHAR_PLUSMINUS)
      goto FAIL;
  }
  else
    goto FAIL;
    
  if(tokens->pos >= tokens->len)
    goto FAIL;
    
  if(is_base36) {
    if(!pmath_char_is_36digit(tokens->str[tokens->pos]))
      goto FAIL;
      
    scan_float_base36_digits_rest(tokens, parser);
  }
  else {
    if(!pmath_char_is_digit(tokens->str[tokens->pos]))
      goto FAIL;
      
    scan_float_decimal_digits_rest(tokens, parser);
  }
  
  scan_number_exponent(tokens, parser);
  
  if(tokens->pos >= tokens->len || tokens->str[tokens->pos] != ']')
    goto FAIL;
    
  ++tokens->pos;
  return;
  
FAIL:
  tokens->pos = start;
  return;
}

static void scan_next_number(struct scanner_t *tokens, struct parser_t *parser) {
  pmath_bool_t have_explicit_base;
  
  assert(!parser || &parser->tokens == tokens);
  assert(tokens->pos < tokens->len);
  assert(pmath_char_is_digit(tokens->str[tokens->pos]));
  
  tokens->spans->items[tokens->pos].is_operand_start = TRUE;
  
  have_explicit_base = scan_number_base_specifier(tokens, parser);
  if(have_explicit_base)
    scan_float_base36_digits_rest(tokens, parser);
  else
    scan_float_decimal_digits_rest(tokens, parser);
    
  scan_number_radius(tokens, parser, have_explicit_base);
  
  scan_precision_specifier(tokens, parser);
  scan_number_exponent(tokens, parser);
}

static void scan_next_as_name(struct scanner_t *tokens, struct parser_t *parser) {
  pmath_token_t tok;
  uint32_t u;
  int prev;
  
  assert(!parser || &parser->tokens == tokens);
  assert(tokens->pos < tokens->len);
  
  
  prev = tokens->pos;
  tok = scan_next_escaped_char(tokens, parser, &u);
  
  if(tok != PMATH_TOK_NAME)
    return;
    
  tokens->spans->items[prev].is_operand_start = TRUE;
  
  while(tokens->pos < tokens->len) {
    prev = tokens->pos;
    tok = scan_next_escaped_char(tokens, parser, &u);
    
    if(tok != PMATH_TOK_NAME && tok != PMATH_TOK_DIGIT) {
      tokens->pos = prev;
      break;
    }
  }
}

static void scan_next(struct scanner_t *tokens, struct parser_t *parser) {
  assert(!parser || &parser->tokens == tokens);
  
  if(tokens->pos >= tokens->len)
    return;
    
  switch(tokens->str[tokens->pos]) {
    case '#': { //  #  ##
        tokens->spans->items[tokens->pos].is_operand_start = TRUE;
        ++tokens->pos;
        
        if(tokens->pos == tokens->len)
          break;
          
        if(tokens->str[tokens->pos] == '#') { //  ##
          ++tokens->pos;
          break;
        }
      } break;
      
    case '+': { //  ++  +=
        ++tokens->pos;
        
        if(tokens->pos == tokens->len)
          break;
          
        if( tokens->str[tokens->pos] == '+' || //  ++
            tokens->str[tokens->pos] == '=')   //  +=
        {
          ++tokens->pos;
          break;
        }
      } break;
      
    case '-': { //  --  -=  ->
        ++tokens->pos;
        
        if(tokens->pos == tokens->len)
          break;
          
        if( tokens->str[tokens->pos] == '-' || //  --
            tokens->str[tokens->pos] == '=' || //  -=
            tokens->str[tokens->pos] == '>')   //  ->
        {
          ++tokens->pos;
          break;
        }
      } break;
      
    case '*': { //  *  **  ***  *=  */
        ++tokens->pos;
        
        if(tokens->pos == tokens->len)
          break;
          
        if(tokens->str[tokens->pos] == '=') { //  *=
          ++tokens->pos;
          break;
        }
        
        if(tokens->str[tokens->pos] == '*') {
          ++tokens->pos;
          
          if(tokens->pos == tokens->len) //  **
            break;
            
          if(tokens->str[tokens->pos] == '*') {
            ++tokens->pos;
            
            if( tokens->comment_level > 0  &&
                tokens->pos < tokens->len &&
                tokens->str[tokens->pos] == '/') //  ***/   ===   ** */
            {
              tokens->comment_level--;
              if(tokens->spans) {
                tokens->spans->items[tokens->pos - 2].is_token_end = TRUE;
                //tokens->spans->items[tokens->pos - 2].is_operand_start = FALSE;
                //tokens->spans->items[tokens->pos - 2].span_index = FALSE;
              }
              ++tokens->pos;
              break;
            }
            
            break; // ***
          }
          
          if( tokens->comment_level > 0 &&
              tokens->str[tokens->pos] == '/') //  **/   ===   * */
          {
            tokens->comment_level--;
            if(tokens->spans) {
              tokens->spans->items[tokens->pos - 2].is_token_end = TRUE;
              //tokens->spans->items[tokens->pos - 2].is_operand_start = FALSE;
              //tokens->spans->items[tokens->pos - 2].span_index = FALSE;
            }
            ++tokens->pos;
            break;
          }
          
          break; // **
        }
        
        if( tokens->comment_level > 0 &&
            tokens->str[tokens->pos] == '/') //  */
        {
          tokens->comment_level--;
          ++tokens->pos;
          break;
        }
        
      } break;
      
    case '/': { //  /  /*  /=  /?  /@  /.  /:  //@  //.  /\/
        ++tokens->pos;
        
        if(tokens->pos == tokens->len)
          break;
          
        if( //!tokens->in_comment &&
          !tokens->in_string &&
          tokens->str[tokens->pos] == '*') //  /*
        {
          tokens->comment_level++;
          ++tokens->pos;
          break;
        }
        
        if( tokens->str[tokens->pos] == '=' || //  /=
            tokens->str[tokens->pos] == '?' || //  /?
            tokens->str[tokens->pos] == '@' || //  /@
            tokens->str[tokens->pos] == '.' || //  /.
            tokens->str[tokens->pos] == ':')   //  /:
        {
          ++tokens->pos;
          break;
        }
        
        if(tokens->str[tokens->pos] == '/') {
          ++tokens->pos;
          
          if(tokens->pos == tokens->len) //  //
            break;
            
          if( tokens->str[tokens->pos] == '@' || //  //@
              tokens->str[tokens->pos] == '.' || //  //.
              tokens->str[tokens->pos] == '=')   //  //=
          {
            ++tokens->pos;
            break;
          }
          
          break;
        }
        
        if( tokens->pos + 1 < tokens->len        &&
            tokens->str[tokens->pos]     == '\\' &&
            tokens->str[tokens->pos + 1] == '/')  //  /\/
        {
          tokens->pos += 2;
        }
        
      } break;
      
    case '<':
    case '>':
    case '!': { //  <  <<  <=  >  >>  >=  !  !!  !=
        ++tokens->pos;
        
        if(tokens->pos == tokens->len)
          break;
          
        if( tokens->str[tokens->pos] == '=' ||                         //  <= >= !=
            tokens->str[tokens->pos] == tokens->str[tokens->pos - 1])  //  << >> !!
        {
          ++tokens->pos;
          break;
        }
      } break;
      
    case '=': { //  =  =!=
        ++tokens->pos;
        
        if( tokens->pos + 1 < tokens->len     &&
            (tokens->str[tokens->pos] == '!' ||
             tokens->str[tokens->pos] == '=') &&
            tokens->str[tokens->pos + 1] == '=')
        {
          tokens->pos += 2;
        }
      } break;
      
    case '~': { //  ~  ~~  ~~~
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
      
    case ':': { //  :  ::  ::=  :=  :>
        ++tokens->pos;
        
        if(tokens->pos == tokens->len)
          break;
          
        if(tokens->str[tokens->pos] == ':') {
          ++tokens->pos;
          
          if(tokens->pos == tokens->len) //  ::
            break;
            
          if(tokens->str[tokens->pos] == '=') { //  ::=
            ++tokens->pos;
            break;
          }
          
          break; // ::
        }
        
        if( tokens->str[tokens->pos] == '=' || //  :=
            tokens->str[tokens->pos] == '>')   //  :>
        {
          ++tokens->pos;
          break;
        }
      } break;
      
    case '?':
    case '&': { //  ?  ??  &  &&
        ++tokens->pos;
        
        if( tokens->pos < tokens->len &&
            tokens->str[tokens->pos] == tokens->str[tokens->pos - 1]) //  ??  ||  &&
        {
          ++tokens->pos;
        }
      } break;
    
    case '|': { //  |  ||  |->
        ++tokens->pos;
        
        if(tokens->pos == tokens->len)
          break;
        
        if( tokens->str[tokens->pos] == '|' ||
            tokens->str[tokens->pos] == '>')   //  ||  |>
        {
          ++tokens->pos;
          break;
        }
        
        if( tokens->pos + 1 < tokens->len &&
            tokens->str[tokens->pos]     == '-' &&
            tokens->str[tokens->pos + 1] == '>')  // |->
        { 
          tokens->pos+= 2;
          break;
        }
      } break;
      
    case '%': //  %  %%  %%%  %%%%  etc.
      tokens->spans->items[tokens->pos].is_operand_start = TRUE;
      if(!tokens->in_string)
        tokens->in_line_comment = TRUE;
    /* fall through */
    case '@':
    case '\'':
    case '.': { //  @  @@  @@@  @@@@  '  ''  '''  ''''  .  ..  ...  .... etc.
        ++tokens->pos;
        while( tokens->pos < tokens->len &&
               tokens->str[tokens->pos] == tokens->str[tokens->pos - 1])
        {
          ++tokens->pos;
        }
      } break;
      
    case '`':
      scan_next_backtick_insertion(tokens, parser);
      break;
      
    case '"':
      if(!scan_next_string(tokens, parser))
        goto END_SCAN;
      break;
      
    case PMATH_CHAR_NOMINALDIGITS:
      scan_float_base36_digits_rest(tokens, parser);
      break;
      
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      scan_next_number(tokens, parser);
      break;
      
    case '\n':
      tokens->in_line_comment = FALSE;
    /* fall through */
    default:
      scan_next_as_name(tokens, parser);
      break;
  }
  
END_SCAN:
  if(tokens->spans)
    tokens->spans->items[tokens->pos - 1].is_token_end = TRUE;
}

static pmath_token_t token_analyse(struct parser_t *parser, int next, int *prec) {
  if(parser->tokens.str[parser->tokens.pos] == PMATH_CHAR_BOX && parser->underoverscriptbox_at_index) {
    pmath_string_t str = parser->underoverscriptbox_at_index(parser->tokens.pos, parser->data);
    
    if(!pmath_is_null(str)) {
      pmath_token_t tok = pmath_token_analyse(pmath_string_buffer(&str), pmath_string_length(str), prec);
              
      pmath_unref(str);
      
      if(tok != PMATH_TOK_NONE)
        return tok;
    }
    
    return PMATH_TOK_NAME2;
  }
  
  return pmath_token_analyse(
           parser->tokens.str + parser->tokens.pos,
           next - parser->tokens.pos,
           prec);
}

static int prefix_precedence(struct parser_t *parser, int next, int defprec) {
  if(parser->tokens.str[parser->tokens.pos] == PMATH_CHAR_BOX && parser->underoverscriptbox_at_index) {
    pmath_string_t str = parser->underoverscriptbox_at_index(parser->tokens.pos, parser->data);
    
    if(!pmath_is_null(str)) {
      int prec = pmath_token_prefix_precedence(pmath_string_buffer(&str), pmath_string_length(str), defprec);
              
      pmath_unref(str);
      
      return prec;
    }
    
    return defprec;
  }
  
  return pmath_token_prefix_precedence(
           parser->tokens.str + parser->tokens.pos,
           next - parser->tokens.pos,
           defprec);
}

static pmath_bool_t same_token(struct parser_t *parser, int last_start, int last_next, int cur_next) {
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
  int          oldss  = parser->last_space_start;
  int          oldpos = parser->tokens.pos;
  pmath_bool_t result;
  
  skip_to(parser, -1, next, TRUE);
  next = next_token_pos(parser);
  
  result = next > parser->tokens.pos &&
           pmath_token_maybe_first(token_analyse(parser, next, NULL));
           
  parser->tokens.pos       = oldpos;
  parser->last_space_start = oldss;
  return result;
}

static void skip_prim_newline(struct parser_t *parser, pmath_bool_t prim_optional) {
  pmath_token_t tok;
  int next;
  
  next = next_token_pos(parser);
  
  if(next == parser->tokens.pos) {
    if(!prim_optional || parser->fencelevel > 0)
      handle_error(parser);
    return;
  }
  
  tok = token_analyse(parser, next, NULL);
  
  if(tok == PMATH_TOK_NEWLINE) {
    if(prim_optional && parser->fencelevel == 0)
      return;
      
    skip_prim_newline_impl(parser, next);
  }
}

static void skip_prim_newline_impl(struct parser_t *parser, int next) {
  pmath_token_t tok;
  int start;
  
  start = parser->tokens.pos;
  
  parser->tokens.spans->items[start].is_operand_start = TRUE;
  for(;;) {
    skip_to(parser, -1, next, FALSE);
    next = next_token_pos(parser);
    
    if(next == parser->tokens.pos)
      break;
      
    tok = token_analyse(parser, next, NULL);
    if(tok != PMATH_TOK_NEWLINE)
      break;
  }
}

static int parse_prim_after_newline(struct parser_t *parser, pmath_bool_t prim_optional) {
  int prim_pos;
  skip_prim_newline(parser, prim_optional);
  prim_pos = parser->tokens.pos;
  parse_prim(parser, prim_optional);
  return prim_pos;
}

static void parse_prim(struct parser_t *parser, pmath_bool_t prim_optional) {
  pmath_token_t tok;
  int prec;
  int start;
  int next;
  int after_nl;
  
  start = parser->tokens.pos;
  next  = next_token_pos(parser);
  
  if(next == parser->tokens.pos) {
    if(!prim_optional || parser->fencelevel > 0)
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
        parser->tokens.spans->items[start].is_operand_start = TRUE;
        skip_to(parser, start, next, TRUE);
        
        next = next_token_pos(parser);
        tok  = token_analyse(parser, next, &prec);
        
        if(tok == PMATH_TOK_COLON) { // x:pattern
          skip_to(parser, -1, next, FALSE);
          next = parser->tokens.pos;
          after_nl = parse_prim_after_newline(parser, FALSE);
          parse_rest(parser, after_nl, PMATH_PREC_ALT);
          if(next != after_nl)
            span(&parser->tokens, next);
        }
      } break;
      
    case PMATH_TOK_CALL: { // no error:  "a:=."
        parser->tokens.spans->items[start].is_operand_start = TRUE;
        skip_to(parser, start, next, TRUE);
      } break;
      
    case PMATH_TOK_TILDES: {
        parser->tokens.spans->items[start].is_operand_start = TRUE;
        skip_to(parser, start, next, TRUE);
        
        // TODO: warn about "~\[RawNewline]..." or skip line-break?
        
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
        parser->tokens.spans->items[start].is_operand_start = TRUE;
        skip_to(parser, start, next, TRUE);
        
        // TODO: warn about "#\[RawNewline]..." ?
        
        next = next_token_pos(parser);
        tok  = token_analyse(parser, next, &prec);
        
        if(tok == PMATH_TOK_DIGIT) {
          skip_to(parser, start, next, TRUE);
          
          next = next_token_pos(parser);
          tok = token_analyse(parser, next, &prec);
        }
      } break;
      
    case PMATH_TOK_QUESTION: {
        parser->tokens.spans->items[start].is_operand_start = TRUE;
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
          after_nl = parse_prim_after_newline(parser, FALSE);
          parse_rest(parser, after_nl, PMATH_PREC_CIRCMUL);
          if(next != after_nl)
            span(&parser->tokens, next);
        }
      } break;
      
    case PMATH_TOK_LEFT:
    case PMATH_TOK_LEFTCALL: {
        int lhs;
        
        parser->tokens.spans->items[start].is_operand_start = TRUE;
        skip_to(parser, -1, next, FALSE);
        
        ++parser->fencelevel;
        
        lhs = parser->tokens.pos;
        after_nl = parse_prim_after_newline(parser, TRUE);
        
        next = next_token_pos(parser);
        tok  = token_analyse(parser, next, &prec);
        
        if(tok != PMATH_TOK_RIGHT) {
          if(lhs != after_nl) {
            parse_rest(parser, after_nl, PMATH_PREC_SEQ + 1);
          }
          parse_rest(parser, lhs, PMATH_PREC_ANY);
          
          next = next_token_pos(parser);
          tok  = token_analyse(parser, next, &prec);
        }
        
        if(lhs != after_nl)
          span(&parser->tokens, lhs);
          
        --parser->fencelevel;
        
        if(tok == PMATH_TOK_RIGHT)
          skip_to(parser, start, next, TRUE);
        else
          handle_error(parser);
      } break;
      
    case PMATH_TOK_NARY_AUTOARG:
      break;
      
    case PMATH_TOK_NEWLINE: {
        if(prim_optional && parser->fencelevel == 0)
          return;
          
        skip_prim_newline_impl(parser, next);
        
        parse_prim(parser, prim_optional);
        span(&parser->tokens, start);
        leave(parser);
        return;
      } break;
      
    case PMATH_TOK_PLUSPLUS:
      prec = PMATH_PREC_INC; // +1
      goto DO_PREFIX;
      
    case PMATH_TOK_NARY_OR_PREFIX:
    case PMATH_TOK_POSTFIX_OR_PREFIX:
      prec = prefix_precedence(parser, next, prec);
      goto DO_PREFIX;
      
    case PMATH_TOK_PREFIX:
    case PMATH_TOK_INTEGRAL: {
      DO_PREFIX:
        parser->tokens.spans->items[start].is_operand_start = TRUE;
        skip_to(parser, -1, next, FALSE);
        next = parser->tokens.pos;
        after_nl = parse_prim_after_newline(parser, FALSE);
        parse_rest(parser, after_nl, prec);
        if(next != after_nl)
          span(&parser->tokens, next);
      } break;
      
    case PMATH_TOK_PRETEXT: {
        parser->tokens.spans->items[start].is_operand_start = TRUE;
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
  pmath_token_t last_tok = PMATH_TOK_NONE;
  int rhs;
  int next;
  int after_nl;
  
  if(!enter(parser)) {
    parse_skip_until_rightfence(parser);
    return;
  }
  
  next = next_token_pos(parser);
  while(next != parser->tokens.pos) {
    tok = token_analyse(parser, next, &cur_prec);
    if(tok == PMATH_TOK_NEWLINE && parser->fencelevel > 0) {
      /* Check whether the token after the line break can start an expression.
         If it can start an expression, then this line break is significant and
         essentially serves as an implicit `;`.
         If the next token can only continue but not start an expression, then this
         line break is non-significant.
       */
      int oldpos = parser->tokens.pos;
      int next2 = next;
      pmath_token_t tok2 = tok;
      int prec2 = cur_prec;
      while(next2 != parser->tokens.pos && tok2 == PMATH_TOK_NEWLINE) {
        skip_to(parser, -1, next2, TRUE);
        next2 = next_token_pos(parser);
        tok2 = token_analyse(parser, next2, &prec2);
      }
      
      if(!pmath_token_maybe_first(tok2) && pmath_token_maybe_rest(tok2)) {
        /* The line break is not significant. */
        int pos = parser->tokens.pos;
        parser->tokens.pos = oldpos;
        span(&parser->tokens, lhs);
        parser->tokens.pos = pos;
        
        tok = tok2;
        next = next2;
        cur_prec = prec2;
      }
      else {
        parser->tokens.pos = oldpos;
      }
    }
    
    switch(tok) {
      case PMATH_TOK_POSTFIX:
      case PMATH_TOK_POSTFIX_OR_PREFIX: {
          if(cur_prec >= min_prec) {
            span(&parser->tokens, lhs);
            
            last_tok       = tok;
            last_tok_start = parser->tokens.pos;
            last_tok_end   = next;
            last_prec      = cur_prec;
            skip_to(parser, lhs, next, TRUE);
            next = next_token_pos(parser);
            continue;
          }
        } break;
        
      case PMATH_TOK_PLUSPLUS: {
          if(plusplus_is_infix(parser, next)) { // x++y
            if(cur_prec >= min_prec)
              goto NARY;
          }
          else if(PMATH_PREC_INC >= min_prec) { // x++
            span(&parser->tokens, lhs);
            
            last_tok       = tok;
            last_tok_start = parser->tokens.pos;
            last_tok_end   = next;
            last_prec      = PMATH_PREC_INC;
            skip_to(parser, lhs, next, TRUE);
            next = next_token_pos(parser);
            continue;
          }
        } break;
        
      case PMATH_TOK_ASSIGNTAG: {
          int prec;
          
          span(&parser->tokens, lhs);
          
          skip_to(parser, -1, next, FALSE);
          rhs = parser->tokens.pos;
          after_nl = parse_prim_after_newline(parser, FALSE);
          parse_rest(parser, after_nl, PMATH_PREC_ASS + 1);
          if(rhs != after_nl)
            span(&parser->tokens, rhs);
            
          next = next_token_pos(parser);
          token_analyse(parser, next, &prec);
          if(prec != PMATH_PREC_ASS) {
            handle_error(parser);
          }
          else {
            skip_to(parser, -1, next, FALSE);
            rhs = parser->tokens.pos;
            after_nl = parse_prim_after_newline(parser, FALSE);
            parse_rest(parser, after_nl, PMATH_PREC_ASS);
            if(rhs != after_nl)
              span(&parser->tokens, rhs);
          }
          
          span(&parser->tokens, lhs);
        } break;
      
      case PMATH_TOK_CALL: {
          if(cur_prec < min_prec)
            break;
            
          span(&parser->tokens, lhs);
          
          last_tok       = tok;
          last_tok_start = parser->tokens.pos;
          last_tok_end   = next;
          
          skip_to(parser, lhs, next, FALSE);
          parse_prim(parser, FALSE); // do not skip newline
          
          last_prec = PMATH_PREC_CALL;
//          if(parser->tokens.str[last_tok_start] == '.')
//            last_prec = PMATH_PREC_CALL;
//          else
//            last_prec = PMATH_PREC_MUL;
          
          next = next_token_pos(parser);
          tok  = token_analyse(parser, next, &cur_prec);
          
        } continue;
        
      case PMATH_TOK_LEFTCALL:
        if(parser->last_space_start == parser->tokens.pos) { // no preceding space
          pmath_bool_t doublesq = FALSE;
          int arglhs;
          
          if(PMATH_PREC_CALL < min_prec)
            break;
            
          if(last_prec == cur_prec) {
            if( cur_prec != PMATH_PREC_OR                               &&
                cur_prec != PMATH_PREC_AND                              &&
                cur_prec != PMATH_PREC_REL                              &&
                cur_prec != PMATH_PREC_ADD                              &&
                cur_prec != PMATH_PREC_MUL                              &&
                !same_token(parser, last_tok_start, last_tok_end, next) &&
                last_tok != PMATH_TOK_CALL
            ) {
              span(&parser->tokens, lhs);
            }
          }
          else if(last_prec >= 0)
            span(&parser->tokens, lhs);
            
          last_tok       = tok;
          last_tok_start = parser->tokens.pos;
          last_tok_end   = next;
          
          ++parser->fencelevel;
          
          if( next == parser->tokens.pos + 1                &&
              next < parser->tokens.len                     &&
              parser->tokens.str[parser->tokens.pos] == '[' &&
              parser->tokens.str[next]               == '[') // [[
          {
            parser->tokens.spans->items[parser->tokens.pos].is_token_end = FALSE;
            doublesq = TRUE;
            ++next;
          }
          
          skip_to(parser, -1, next, FALSE);
          
          arglhs = parser->tokens.pos;
          after_nl = parse_prim_after_newline(parser, TRUE);
          
          next = next_token_pos(parser);
          tok  = token_analyse(parser, next, &cur_prec);
          
          if(tok != PMATH_TOK_RIGHT) {
            if(arglhs != after_nl) {
              parse_rest(parser, after_nl, PMATH_PREC_SEQ + 1);
            }
            parse_rest(parser, arglhs, PMATH_PREC_ANY);
            
            next = next_token_pos(parser);
            tok  = token_analyse(parser, next, &cur_prec);
          }
          
          if(arglhs != after_nl)
            span(&parser->tokens, arglhs);
            
          --parser->fencelevel;
          
          if(tok == PMATH_TOK_RIGHT) {
            if(doublesq) {
              if( next == parser->tokens.pos + 1                &&
                  next < parser->tokens.len                     &&
                  parser->tokens.str[parser->tokens.pos] == ']' &&
                  parser->tokens.str[next]               == ']')
              {
                parser->tokens.spans->items[parser->tokens.pos].is_token_end = FALSE;
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
        
      case PMATH_TOK_NEWLINE: {
          if(parser->fencelevel == 0)
            break;
        }
      /* no break */
      case PMATH_TOK_NARY_AUTOARG: {
          pmath_token_t tok2;
          int           next2;
          int           oldss  = parser->last_space_start;
          int           oldpos = parser->tokens.pos;
          
          if(cur_prec < min_prec)
            break;
            
          skip_to(parser, -1, next, TRUE);
          
          next2 = next_token_pos(parser);
          tok2  = token_analyse(parser, next2, NULL);
          
          if(tok == PMATH_TOK_NEWLINE && parser->fencelevel > 0) {
            /* Skip subsequent newlines (multiple empty lines).
               Note that pmath_token_maybe_first(PMATH_TOK_NEWLINE) == TRUE
             */
            while(next2 != parser->tokens.pos && tok2 == PMATH_TOK_NEWLINE) {
              skip_to(parser, -1, next2, TRUE);
              next2 = next_token_pos(parser);
              tok2  = token_analyse(parser, next2, NULL);
            }
          }
          
          if(!pmath_token_maybe_first(tok2)) {
          
            if(tok == PMATH_TOK_NARY_AUTOARG) {
              if(cur_prec < last_prec) {
                int pos = parser->tokens.pos;
                parser->tokens.pos = oldpos;
                span(&parser->tokens, lhs);
                parser->tokens.pos = pos;
              }
              
              span(&parser->tokens, lhs);
            }
            else {
              int pos = parser->tokens.pos;
              parser->tokens.pos = oldpos;
              span(&parser->tokens, lhs);
              parser->tokens.pos = pos;
            }
            
            next           = next2;
            last_tok       = tok;
            last_tok_start = parser->tokens.pos;
            last_tok_end   = next;
            last_prec      = cur_prec;
            continue;
          }
          
          parser->tokens.pos       = oldpos;
          parser->last_space_start = oldss;
        } goto NARY;
      
      case PMATH_TOK_CALLPIPE: {
          if(cur_prec < min_prec)
            break;
            
          span(&parser->tokens, lhs);
          
          cur_prec = PMATH_PREC_CALLPIPE_RIGHT;
        } goto BINARY;
        
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
              if( cur_prec != PMATH_PREC_EVAL &&
                  cur_prec != PMATH_PREC_OR   &&
                  cur_prec != PMATH_PREC_AND  &&
                  cur_prec != PMATH_PREC_REL  &&
                  cur_prec != PMATH_PREC_ADD  &&
                  cur_prec != PMATH_PREC_MUL  &&
                  !same_token(parser, last_tok_start, last_tok_end, next))
              {
                span(&parser->tokens, lhs);
              }
            }
            else //if(last_prec >= 0)
              span(&parser->tokens, lhs);
              
          BINARY:
          
            last_tok       = tok;
            last_tok_start = parser->tokens.pos;
            last_tok_end   = next;
            
            skip_to(parser, -1, next, tok == PMATH_TOK_NARY_AUTOARG);
            
            rhs = parser->tokens.pos;
            after_nl = parse_prim_after_newline(parser, tok == PMATH_TOK_NARY_AUTOARG);
            
            next = next_token_pos(parser);
            oldpos = parser->tokens.pos;
            while(oldpos != next) {
              int next_prec;
              tok = token_analyse(parser, next, &next_prec);
              oldpos = next;
              
              if(tok == PMATH_TOK_NEWLINE && parser->fencelevel > 0) {
                int nlpos = parser->tokens.pos;
                int nlprec = next_prec;
                
                /* Skip subsequent newlines (multiple empty lines).
                   If the next token after that cannot start an expression but can
                   continue an expression, then the newline was not significant and
                   that token should be considered.
                   Otherwise the newline is significant (and acts as an implicit `;`)
                 */
                while(next != parser->tokens.pos && tok == PMATH_TOK_NEWLINE) {
                  skip_to(parser, -1, next, TRUE);
                  next = next_token_pos(parser);
                  tok  = token_analyse(parser, next, &next_prec);
                }
                
                if(!pmath_token_maybe_first(tok) && pmath_token_maybe_rest(tok)) {
                  /* The line break is not significant (`tok` and `next_prec` refer to the
                     next token), but we reset the position to not interfere with span(). */
                  next = oldpos;
                  parser->tokens.pos = nlpos;
                }
                else {
                  tok = PMATH_TOK_NEWLINE;
                  next = oldpos;
                  next_prec = nlprec;
                  parser->tokens.pos = nlpos;
                }
              }
              
              if(tok == PMATH_TOK_BINARY_RIGHT) {
                if(next_prec >= cur_prec) {
                  parse_rest(parser, after_nl, next_prec); // =: rhs
                  
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
                    parse_rest(parser, after_nl, next_prec); // =: rhs
                    
                    next = next_token_pos(parser);
                    continue;
                  }
                }
                else if(PMATH_PREC_MUL > cur_prec) {
                  parse_rest(parser, after_nl, PMATH_PREC_MUL); // =: rhs
                  
                  next = next_token_pos(parser);
                  continue;
                }
                
                break;
              }
              
              if(pmath_token_maybe_rest(tok)) {
                if(next_prec > cur_prec) {
                  parse_rest(parser, after_nl, next_prec); // =: rhs
                  
                  next = next_token_pos(parser);
                  continue;
                }
                
                break;
              }
              
              break;
            }
            
            if(rhs != after_nl)
              span(&parser->tokens, rhs);
              
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
    
    if(tok == PMATH_TOK_NEWLINE && parser->fencelevel == 0)
      break;
      
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

static void parse_textline(struct parser_t *parser) {
  pmath_token_t tok;
  int i;
  int start = parser->tokens.pos;
  
  if(start == parser->tokens.len) {
    handle_error(parser);
    return;
  }
  
  assert(start < parser->tokens.len);
  
  if(parser->tokens.str[start] == '"') {
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
    parser->tokens.spans->items[i].is_token_end = FALSE;
    
  parser->tokens.spans->items[parser->tokens.pos - 1].is_token_end = TRUE;
  parser->tokens.spans->items[start].is_operand_start = TRUE;
  
  span(&parser->tokens, start);
  skip_space(parser, start, TRUE);
}

//} ... parsing

//{ group spans ...

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
    emit_span(span_at(spans, group.tp.index), &group);
    
  result = pmath_expr_set_item(pmath_gather_end(), 0, PMATH_NULL);
  if(pmath_expr_length(result) == 1) {
    pmath_t tmp = pmath_expr_get_item(result, 1);
    pmath_unref(result);
    return tmp;
  }
  return result;
}

static void increment_text_position_to(struct group_t *group, int end) {
  if(group->tp.index > end) {
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
        increment_text_position(group);
        continue;
      }
    }
    
    if( group->tp.index < group->spans->length &&
        group->str[group->tp.index] == '%')
    {
      pmath_span_t *s = span_at(group->spans, group->tp.index);
      if(s) {
        while(s->next_offset)
          s += s->next_offset;
          
        increment_text_position_to(group, s->end + 1);
      }
      else { // increment by 2
        increment_text_position(group);
        increment_text_position(group);
      }
      
      continue;
    }
    
    if( group->tp.index + 1 < group->spans->length &&
        group->str[group->tp.index]     == '/'     &&
        group->str[group->tp.index + 1] == '*'     &&
        !group->spans->items[group->tp.index].is_token_end)
    {
      pmath_span_t *s = span_at(group->spans, group->tp.index);
      if(s) {
        while(s->next_offset)
          s += s->next_offset;
          
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
      pmath_t box;
      if(group->settings.box_at_index)
        box = group->settings.box_at_index(group->tp.index, group->settings.data);
      else
        box = PMATH_NULL;
        
      increment_text_position(group);
      if(group->settings.add_debug_metadata)  {
        box = group->settings.add_debug_metadata(
                box,
                &span_start,
                &group->tp,
                group->settings.data);
      }
      pmath_emit(box, PMATH_NULL);
    }
    else {
      pmath_t result;
      
      while( group->tp.index < group->spans->length &&
             !group->spans->items[group->tp.index].is_token_end)
      {
        increment_text_position(group);
      }
      
      increment_text_position(group);
      
      result = pmath_string_part(
                 pmath_ref(group->string),
                 span_start.index,
                 group->tp.index - span_start.index);
                 
      if(group->settings.add_debug_metadata) {
        result = group->settings.add_debug_metadata(
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
    !span->next_offset &&
    group->str[group->tp.index] == '"')
  {
    pmath_string_t result = PMATH_NULL;
    struct pmath_text_position_t start = group->tp;
    
    if(group->settings.flags & PMATH_BFS_USESTRINGBOX) {
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
                            
            if(group->settings.add_debug_metadata) {
              pre = group->settings.add_debug_metadata(
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
                         
        if(group->settings.add_debug_metadata) {
          rest = group->settings.add_debug_metadata(
                   rest,
                   &start,
                   &group->tp,
                   group->settings.data);
        }
        
        pmath_emit(rest, PMATH_NULL);
      }
      
      all = pmath_gather_end();
      all = pmath_expr_set_item(all, 0, pmath_ref(pmath_System_StringBox));
      
      check_tp_before_whitespace(group, span->end + 1);
      
      if(group->settings.add_debug_metadata) {
        all = group->settings.add_debug_metadata(
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
      
      if(group->settings.add_debug_metadata) {
        result = group->settings.add_debug_metadata(
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
    
    if(span->next_offset)
      emit_span(span + span->next_offset, group);
    else
      emit_span(NULL, group);
    
    while(group->tp.index <= span->end)
      emit_span(span_at(group->spans, group->tp.index), group);
      
    expr = pmath_gather_end();
    if(pmath_expr_length(expr) > 0) {
      check_tp_before_whitespace(group, span->end + 1);
      
      if(group->settings.add_debug_metadata) {
        expr = group->settings.add_debug_metadata(
                 expr,
                 &span_start,
                 &group->tp_before_whitespace,//span->end + 1,
                 group->settings.data);
      }
      
      pmath_emit(expr, PMATH_NULL);
    }
    else
      pmath_unref(expr);
  }
}

//} ... group spans

//{ ungroup span_array ...

PMATH_API pmath_span_array_t *pmath_spans_from_boxes(
  pmath_t          boxes,  // will be freed
  pmath_string_t  *result_string,
  void           (*make_box)(int, pmath_t, void *),
  void            *data
) {
  struct ungroup_t g;
  
  assert(result_string);
  
  g.spans = create_span_array(ungrouped_string_length(boxes), 0);
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
    g.spans->items[0].is_operand_start = TRUE;
    
  assert(g.pos == g.spans->length);
  return g.spans;
}

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
    
    if(pmath_is_null(head) || pmath_same(head, pmath_System_List)) {
      for(i = pmath_expr_length(box); i > 0; --i) {
        pmath_t boxi = pmath_expr_get_item(box, i);
        
        result += ungrouped_string_length(boxi);
        
        pmath_unref(boxi);
      }
      return result;
    }
    
    if(pmath_same(head, pmath_System_StringBox)) {
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

static void ungroup(struct ungroup_t *g, pmath_t box) { // box will be freed
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
          g->spans->items[g->pos - 1].is_token_end = TRUE;
        g->spans->items[g->pos].is_token_end = TRUE;
        g->str[g->pos++] = PMATH_CHAR_BOX;
      }
      else if(str[i] == PMATH_CHAR_BOX) {
        if(g->make_box) {
          g->make_box(
            g->pos,
            pmath_string_insert_ucs2(PMATH_NULL, g->pos, str + i, 1),
            g->data);
        }
        g->spans->items[g->pos].is_token_end = TRUE;
        g->spans->items[g->pos].is_operand_start = TRUE;
        //g->spans->items[g->pos].span_index = 0;
        g->str[g->pos++] = PMATH_CHAR_BOX;
        i++;
      }
      else
        g->str[g->pos++] = str[i++];
    }
    
    if(g->split_tokens) {
      memset(&tokens, 0, sizeof(tokens));
      tokens.str           = g->str;
      tokens.spans         = g->spans;
      tokens.pos           = start;
      tokens.len           = g->pos;
      tokens.comment_level = g->comment_level;
      while(tokens.pos < tokens.len)
        scan_next(&tokens, NULL);
        
      g->comment_level = tokens.comment_level;
      g->spans = tokens.spans;
    }
    else {
      g->spans->items[start].is_operand_start = TRUE;
      g->spans->items[g->pos - 1].is_token_end = TRUE;
    }
    
    pmath_unref(box);
    return;
  }
  
  if(pmath_is_expr(box)) {
    pmath_t head = pmath_expr_get_item(box, 0);
    pmath_unref(head);
    if( pmath_is_null(head) ||
        pmath_same(head, pmath_System_List))
    {
      size_t ei, len;
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
      
      for(ei = 1; ei <= len; ++ei)
        ungroup(g, pmath_expr_get_item(box, ei));
        
    AFTER_UNGROUP:
      g->spans->items[start].is_operand_start = TRUE;
      
      g->comment_level = old_comment_level;
      
      if( len >= 1 &&
          start < g->pos &&
          pmath_same(head, pmath_System_List))
      {
        pmath_t first = pmath_expr_get_item(box, 1);
        
        pmath_span_t *old = span_at(g->spans, start);
        
        if(pmath_is_string(first)) {
          const uint16_t *fbuf = pmath_string_buffer(&first);
          int             flen = pmath_string_length(first);
          
          if(flen > 0 && fbuf[0] == '"' && (flen == 1 || fbuf[flen - 1] != '"')) {
            if(old) {
              old->end = g->pos - 1;
            }
            else {
              unsigned span_index;
              old = alloc_span(&g->spans, &span_index);
              if(old) {
                old->next_offset = 0;
                old->end = g->pos - 1;
                g->spans->items[start].span_index = span_index;
              }
            }
          }
        }
        
        pmath_unref(first);
        
        if(!old || old->end != g->pos - 1) {
          int i;
          
          pmath_bool_t have_mutliple_tokens = FALSE;
          for(i = start; i < g->pos - 1; ++i) {
            if(g->spans->items[i].is_token_end) {
              have_mutliple_tokens = TRUE;
              break;
            }
          }
          
          if(have_mutliple_tokens) {
            unsigned span_index;
            pmath_span_t *s = alloc_span(&g->spans, &span_index);
            if(s) {
              old = span_at(g->spans, start);
              //s->next = old;
              s->next_offset = old ? (int)(old - s) : 0;
              s->end = g->pos - 1;
              g->spans->items[start].span_index = span_index;
            }
          }
        }
      }
      
      pmath_unref(box);
      return;
    }
    
    if(pmath_same(head, pmath_System_StringBox)) {
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
          g->spans->items[g->pos - 1].is_token_end = TRUE;
        g->spans->items[g->pos].is_token_end = TRUE;
        g->str[g->pos++] = PMATH_CHAR_BOX;
      }
      
      if(g->pos > start) {
        unsigned span_index;
        s = alloc_span(&g->spans, &span_index);
        if(s) {
//          pmath_span_t *old = span_at(g->spans, start);
//          
//          while(old) {
//            pmath_span_t *tmp = old->next;
//            pmath_mem_free(old);
//            old = tmp;
//          }
          
          s->next_offset = 0;
          s->end = g->pos - 1;
          g->spans->items[start].is_operand_start = TRUE;
          g->spans->items[start].span_index = span_index;
        }
        
        g->spans->items[g->pos - 1].is_token_end = TRUE;
      }
      pmath_unref(box);
      return;
    }
  }
  
  if(g->make_box)
    g->make_box(g->pos, box, g->data);
  else
    pmath_unref(box);
    
  g->spans->items[g->pos].is_token_end = TRUE;
  g->spans->items[g->pos].is_operand_start = TRUE;
  g->str[g->pos++] = PMATH_CHAR_BOX;
}

//} ... ungroup spans

static pmath_t quiet_parse(pmath_t str);
static void quiet_syntax_error(pmath_string_t code, int pos, void *flag, pmath_bool_t critical);
static void syntax_error(      pmath_string_t code, int pos, void *flag, pmath_bool_t critical);

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
        if(pmath_is_string(s)) {
          pmath_unref(result);
          return s;
        }
        pmath_unref(s);
      }
      
      result = pmath_expr_set_item(result, 0, pmath_ref(pmath_System_StringBox));
      return result;
    }
  }
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
               pmath_ref(pmath_System_MakeExpression), 1,
               result));
               
  if(!pmath_is_expr_of(result, pmath_System_HoldComplete))
    return result;
    
  if(pmath_expr_length(result) == 1) {
    pmath_t item = pmath_expr_get_item(result, 1);
    pmath_unref(result);
    return item;
  }
  
  return pmath_expr_set_item(
           result, 0,
           pmath_ref(pmath_System_Sequence));
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

PMATH_API pmath_bool_t pmath_is_namespace(pmath_t name) {
  const uint16_t *buf;
  int len, i;
  pmath_token_t tok;

  if(!pmath_is_string(name))
    return FALSE;

  len = pmath_string_length(name);
  buf = pmath_string_buffer(&name);

  if(len < 2 || buf[len - 1] != '`' || buf[0] == '`')
    return FALSE;

  for(i = 0; i < len - 1; ++i) {
    if(buf[i] == '`') {
      ++i;
      tok = pmath_token_analyse(buf + i, 1, NULL);
      if(tok != PMATH_TOK_NAME)
        return FALSE;
    }
    else {
      tok = pmath_token_analyse(buf + i, 1, NULL);
      if(tok != PMATH_TOK_DIGIT && tok != PMATH_TOK_NAME)
        return FALSE;
    }
  }

  return TRUE;
}

PMATH_API pmath_bool_t pmath_is_namespace_list(pmath_t list) {
  size_t i;

  if(!pmath_is_expr_of(list, pmath_System_List))
    return FALSE;

  for(i = pmath_expr_length(list); i > 0; --i) {
    pmath_t name = pmath_expr_get_item(list, i);

    if(!pmath_is_namespace(name)) {
      pmath_unref(name);
      return FALSE;
    }

    pmath_unref(name);
  }

  return TRUE;
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
                   pmath_ref(pmath_System_MessageName), 2,
                   pmath_ref(pmath_System_MakeExpression),
                   PMATH_C_STRING("inv"));
                   
  // Off(MakeExpression::inv)
  on_off = pmath_thread_local_save(
             message_name,
             pmath_ref(pmath_System_Off));
             
  result = pmath_evaluate(
             pmath_expr_new_extended(
               pmath_ref(pmath_System_MakeExpression), 1,
               result));
               
  pmath_unref(
    pmath_thread_local_save(
      message_name,
      on_off));
      
  pmath_unref(message_name);
  
  if(pmath_is_expr_of_len(result, pmath_System_HoldComplete, 1)) {
    pmath_t value = pmath_expr_get_item(result, 1);
    pmath_unref(result);
    return value;
  }
  
  pmath_unref(result);
  return PMATH_UNDEFINED;
}

static void quiet_syntax_error(pmath_string_t code, int pos, void *flag, pmath_bool_t critical) {
  pmath_bool_t *have_critical = flag;
  
  if(critical)
    *have_critical = TRUE;
}

static void syntax_error(pmath_string_t code, int pos, void *flag, pmath_bool_t critical) {
  pmath_bool_t *have_critical = flag;
  
  if(!*have_critical)
    pmath_message_syntax_error(code, pos, PMATH_NULL, 0);
    
  if(critical)
    *have_critical = TRUE;
}
