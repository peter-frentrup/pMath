#include <pmath-util/line-writer.h>

#include <pmath-core/objects-private.h>
#include <pmath-core/numbers-private.h>
#include <pmath-core/strings-private.h>

#include <pmath-util/debug.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/memory.h>
#include <pmath-util/overflow-calc-private.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/formating-private.h>

#include <string.h>


struct write_pos_t {
  pmath_t                item;
  struct write_pos_t    *next;
  int                    pos;
  pmath_write_options_t  flags;
  pmath_bool_t           is_start;
};

#define NEWLINE_NONE       0x00
#define NEWLINE_OK         0x01
#define NEWLINE_INSTRING   0x02

struct linewriter_t {
  int                    line_length;   // > 2
  int                    buffer_length; // > line_length!!!, <= 2*line_length
  uint16_t              *buffer;        // size == buffer_length
  int                   *depths;        // size == buffer_length
  pmath_write_options_t *char_flags;    // size == buffer_length
  uint8_t               *newlines;      // size >= line_length + 1
  int                    pos;
  struct write_pos_t    *all_write_pos;
  struct write_pos_t   **next_write_pos;
  int                    string_depth;
  int                    expr_depth;
  int                    prev_depth;
  
  int                    indentation_width;
  pmath_write_options_t  next_flags;
  
  /// Increased inside RawBoxes(...) and inside strings, because strings can contain embedded boxes.
  int  rawboxes_depth;
  
  void          *user;
  void         (*write)(        void *, const uint16_t *, int);
  void         (*pre_write)(    void *, pmath_t, pmath_write_options_t);
  void         (*post_write)(   void *, pmath_t, pmath_write_options_t);
  pmath_bool_t (*custom_writer)(void *, pmath_t, struct pmath_write_ex_t *);
};

static char HEX_DIGITS[] = "0123456789ABCDEF";

static void fill_newlines(struct linewriter_t *lw) {
  struct write_pos_t *wp;
  int string_depth, oldpos;
  pmath_bool_t prefix_needs_hyphenation = FALSE;
  
  memset(lw->newlines, NEWLINE_NONE, lw->line_length + 1);
  
  oldpos = -1;
  wp     = lw->all_write_pos;
  
  while(wp && wp->pos <= lw->line_length) {
    if(wp->is_start) {
      if(wp->pos == oldpos) // two expressions without operator/space in between
        lw->newlines[wp->pos] = NEWLINE_INSTRING;
      else if(prefix_needs_hyphenation)
        lw->newlines[wp->pos] = NEWLINE_INSTRING;
      else
        lw->newlines[wp->pos] = NEWLINE_OK;
      
      prefix_needs_hyphenation = FALSE;
      switch(lw->buffer[wp->pos]) { // better use pmath_token_analyse() ?
        case '#':
        case '~':
          prefix_needs_hyphenation = TRUE;
          break;
      }
    }
    else {
      oldpos = wp->pos;
      prefix_needs_hyphenation = FALSE;
    }
      
    wp = wp->next;
  }
  
  oldpos       = -1;
  string_depth = lw->string_depth;
  wp           = lw->all_write_pos;
  
  while(wp && wp->pos <= lw->line_length) {
    if(string_depth > 0) {
      while(oldpos < wp->pos)
        lw->newlines[++oldpos] |= NEWLINE_INSTRING;
    }
    
    oldpos = wp->pos;
    if(pmath_is_string(wp->item)) {
      if(wp->is_start) {
        ++string_depth;
      }
      else
        --string_depth;
    }
    
    wp = wp->next;
  }
  
  if(string_depth > 0) {
    while(oldpos < lw->line_length)
      lw->newlines[++oldpos] |= NEWLINE_INSTRING;
  }
}

static void consume_write_pos(struct linewriter_t *lw, int end) {
  struct write_pos_t *wp;
  int i = 0;
  
  while(lw->all_write_pos) {
    wp = lw->all_write_pos;
    if(wp->pos >= end)
      break;
      
    lw->all_write_pos = wp->next;
    
    if(pmath_is_string(wp->item)) {
      if(wp->is_start)
        lw->string_depth++;
      else
        lw->string_depth--;
    }
    
    assert(i <= wp->pos);
    if(i < wp->pos) {
      lw->write(lw->user, lw->buffer + i, wp->pos - i);
      i = wp->pos;
    }
    
    if(wp->is_start) {
      if(lw->pre_write)
        lw->pre_write(lw->user, wp->item, wp->flags);
    }
    else {
      if(lw->post_write)
        lw->post_write(lw->user, wp->item, wp->flags);
    }
    
    pmath_unref(wp->item);
    pmath_mem_free(wp);
  }
  
  while(lw->all_write_pos) {
    wp = lw->all_write_pos;
    if(wp->pos != end || wp->is_start)
      break;
      
    lw->all_write_pos = wp->next;
    
    if(pmath_is_string(wp->item)) {
      lw->string_depth--;
    }
    
    assert(i <= wp->pos);
    if(i < wp->pos) {
      lw->write(lw->user, lw->buffer + i, wp->pos - i);
      i = wp->pos;
    }
    
    if(lw->post_write)
      lw->post_write(lw->user, wp->item, wp->flags);
      
    pmath_unref(wp->item);
    pmath_mem_free(wp);
  }
  
  assert(i <= end);
  if(i < end) {
    lw->write(lw->user, lw->buffer + i, end - i);
    i = end;
  }
  
  if(!lw->all_write_pos)
    lw->next_write_pos = &lw->all_write_pos;
    
  wp = lw->all_write_pos;
  while(wp) {
    wp->pos -= end;
    wp       = wp->next;
  }
}

static void injection_adjust_write_pos(struct linewriter_t *lw, int pos, int len) {
  struct write_pos_t *wp;
  
  wp = lw->all_write_pos;
  while(wp) {
    if(wp->pos >= pos)
      wp->pos += len;
    wp       = wp->next;
  }
}

static double calc_penalty(struct linewriter_t *lw, int pos, int max) {
  double error;
  
  assert(0   <  pos);
  assert(pos <= max);
  assert(0   <  max);
  
  error = pos - max;
  error = 4.0 * error * error / ((double) max * max);
  error += 1.0 * lw->depths[pos - 1];
  
  if(pos < lw->pos) {
    int prev_depth = lw->depths[0];
    int next_depth = lw->depths[pos];
    
    if(prev_depth + pos <= next_depth)
      error += 10.0;
  }
  
  return error;
}

static int get_expr_indention_depth(struct linewriter_t *lw) {
  int depth;
  if(lw->pos > 0)
    depth = lw->depths[0];
  else
    depth = lw->prev_depth;
    
  if(depth > lw->line_length / 2)
    depth  = lw->line_length / 2;
    
  return depth;
}

static const uint16_t *get_next_string_token(const uint16_t *tok, const uint16_t *end) {
  assert(tok != NULL);
  assert(tok <= end);
  
  if(tok == end)
    return tok;
    
  if(*tok != '\\')
    return tok + 1;
    
  ++tok;
  if(tok == end)
    return tok;
    
  if(*tok == '[') {
    while(tok != end && *tok != ']')
      ++tok;
      
    if(tok != end)
      ++tok;
      
    return tok;
  }
  
  ++tok;
  return tok;
}

static void get_string_token_bounds(
  struct linewriter_t *lw,
  int                pos,
  int               *start,
  int               *next
) {
  int str_start;
  const uint16_t *tok;
  const uint16_t *buf_end;
  
  assert(pos <= lw->line_length);
  
  if(pos >= lw->line_length) {
    *start = pos;
    *next  = pos;
    return;
  }
  
  str_start = pos;
  while(str_start > 0 && (lw->newlines[str_start - 1] & NEWLINE_INSTRING))
    --str_start;
    
  tok = lw->buffer + str_start;
  buf_end = lw->buffer + lw->line_length;
  
  while(tok <= lw->buffer + pos) {
    *start = (int)(tok - lw->buffer);
    tok = get_next_string_token(tok, buf_end);
  }
  
  *next = (int)(tok - lw->buffer);
}

static int find_best_linebreak(
  struct linewriter_t *lw,
  pmath_bool_t        *is_inside_string,
  pmath_bool_t        *is_inside_token
) {
  int depth = get_expr_indention_depth(lw);
  int last  = lw->line_length - 1 - depth;
  int nl;
  
  fill_newlines(lw);
  nl = last;
  while(nl > 0 && !(lw->newlines[nl] & NEWLINE_OK))
    --nl;
    
  if(nl == 0) {
    nl = last - 1;
    while(nl > 0 && lw->buffer[nl] > ' ')
      --nl;
      
    if(nl == 0)
      nl = last - 1;
    else
      ++nl;
      
    if((lw->newlines[nl] & NEWLINE_INSTRING) != 0) {
      int s, e;
      
      get_string_token_bounds(lw, nl, &s, &e);
      if(s > 0)
        nl = s;
      else
        nl = e;
        
      *is_inside_string = TRUE;
      *is_inside_token  = FALSE;
    }
    else {
      *is_inside_string = FALSE;
      *is_inside_token  = TRUE;
    }
  }
  else {
    double error = calc_penalty(lw, nl, last);
    int new_nl = nl - 1;
    
    while(new_nl > 0) {
      if(lw->newlines[new_nl] & NEWLINE_OK) {
        double new_error = calc_penalty(lw, new_nl, last);
        
        if(new_error < error) {
          nl    = new_nl;
          error = new_error;
        }
      }
      
      --new_nl;
    }
    
    *is_inside_string = (lw->newlines[nl] & NEWLINE_INSTRING) != 0;
    *is_inside_token  = FALSE;
  }
  
  return nl;
}

static void flush_line(struct linewriter_t *lw) {
  int i, nl;
  pmath_bool_t is_inside_string;
  pmath_bool_t is_inside_token;
  pmath_bool_t hyphenate;
  pmath_bool_t ignore_linebreak;
  int depth = get_expr_indention_depth(lw);
  
  if(lw->pos <= lw->line_length - depth) {
    for(i = 0; i < lw->pos; ++i) {
      if(lw->buffer[i] == '\n') {
        ++i;
        consume_write_pos(lw, i);
        
        lw->prev_depth = lw->depths[i];
        memmove(lw->buffer,     lw->buffer     + i, sizeof(lw->buffer[    0]) * (lw->buffer_length - i));
        memmove(lw->depths,     lw->depths     + i, sizeof(lw->depths[    0]) * (lw->buffer_length - i));
        memmove(lw->char_flags, lw->char_flags + i, sizeof(lw->char_flags[0]) * (lw->buffer_length - i));
        lw->pos -= i;
        
        depth = get_expr_indention_depth(lw);
        i     = lw->indentation_width + depth;
        while(i-- > 0)
          _pmath_write_cstr(" ", lw->write, lw->user);
        return;
      }
    }
    
    consume_write_pos(lw, lw->pos);
    lw->pos = 0;
    return;
  }
  
  for(i = 0; i < lw->line_length - depth; ++i) {
    if(lw->buffer[i] == '\n') {
      ++i;
      consume_write_pos(lw, i);
      memmove(lw->buffer,     lw->buffer     + i, sizeof(lw->buffer[    0]) * (lw->buffer_length - i));
      memmove(lw->depths,     lw->depths     + i, sizeof(lw->depths[    0]) * (lw->buffer_length - i));
      memmove(lw->char_flags, lw->char_flags + i, sizeof(lw->char_flags[0]) * (lw->buffer_length - i));
      lw->pos -= i;
      
      depth = get_expr_indention_depth(lw);
      i     = lw->indentation_width + depth;
      while(i-- > 0)
        _pmath_write_cstr(" ", lw->write, lw->user);
      return;
    }
  }
  
  nl = find_best_linebreak(lw, &is_inside_string, &is_inside_token);
  
  ignore_linebreak = FALSE;
  hyphenate        = FALSE;
  
  if(is_inside_token)
    ignore_linebreak = (lw->char_flags[nl - 1] & PMATH_WRITE_OPTIONS_INPUTEXPR) != 0;
  else if(is_inside_string)
    hyphenate = (lw->char_flags[nl - 1] & PMATH_WRITE_OPTIONS_FULLSTR) != 0;
    
  if(hyphenate && !ignore_linebreak) {
  
    if(nl > lw->line_length - 2)
      nl = lw->line_length - 2;
      
    if(lw->buffer[nl - 1] == '\\') {
      i = nl - 1;
      while(i > 0 && lw->buffer[i - 1] == '\\')
        --i;
        
      if((nl - i) % 2 == 1 && nl > 1)
        --nl;
    }
  }
  
  consume_write_pos(lw, nl);
  memmove(lw->buffer,     lw->buffer     + nl, sizeof(lw->buffer[    0]) * (lw->buffer_length - nl));
  memmove(lw->depths,     lw->depths     + nl, sizeof(lw->depths[    0]) * (lw->buffer_length - nl));
  memmove(lw->char_flags, lw->char_flags + nl, sizeof(lw->char_flags[0]) * (lw->buffer_length - nl));
  lw->pos -= nl;
  
  if(ignore_linebreak)
    return;
    
  i = depth;
  depth = get_expr_indention_depth(lw);
  if(depth >= i + nl)
    return;
    
  if(hyphenate) {
    _pmath_write_cstr("\\\n", lw->write, lw->user);
    
    if(is_inside_string) {
      // replace initial space with \[U+00xx]
      if( lw->pos > 0                     &&
          lw->pos < lw->buffer_length - 8 &&
          lw->buffer[0] <= ' ')
      {
        char hex_hi = HEX_DIGITS[lw->buffer[0] >> 4];
        char hex_lo = HEX_DIGITS[lw->buffer[0] & 0xF];
        
        injection_adjust_write_pos(lw, 1, 8);
        memmove(lw->buffer     + 8, lw->buffer,     sizeof(lw->buffer[    0]) * (lw->buffer_length - 8));
        memmove(lw->depths     + 8, lw->depths,     sizeof(lw->depths[    0]) * (lw->buffer_length - 8));
        memmove(lw->char_flags + 8, lw->char_flags, sizeof(lw->char_flags[0]) * (lw->buffer_length - 8));
        lw->pos += 8;
        
        for(i = 0; i <= 8; ++i)
          lw->depths[i] = lw->depths[0];
        for(i = 0; i <= 8; ++i)
          lw->char_flags[i] = lw->char_flags[0];
          
        lw->buffer[0] = '\\';
        lw->buffer[1] = '[';
        lw->buffer[2] = 'U';
        lw->buffer[3] = '+';
        lw->buffer[4] = '0';
        lw->buffer[5] = '0';
        lw->buffer[6] = hex_hi;
        lw->buffer[7] = hex_lo;
        lw->buffer[8] = ']';
      }
    }
  }
  else
    _pmath_write_cstr("\n", lw->write, lw->user);
    
  i     = lw->indentation_width + depth;
  while(i-- > 0)
    _pmath_write_cstr(" ", lw->write, lw->user);
}

static void line_write(void *user, const uint16_t *data, int len) {
  struct linewriter_t *lw = user;
  int i;
  
  assert(lw->line_length > 2);
  
  while(len > 0) {
    if(lw->pos + len <= lw->buffer_length) {
      memcpy(lw->buffer + lw->pos, data, len * sizeof(lw->buffer[0]));
      
      lw->depths[lw->pos] = lw->prev_depth;
      lw->prev_depth      = lw->expr_depth;
      for(i = 1; i < len; ++i)
        lw->depths[lw->pos + i] = lw->expr_depth;
        
      for(i = 0; i < len; ++i)
        lw->char_flags[lw->pos + i] = lw->next_flags;
        
      lw->pos += len;
      return;
    }
    
    if(lw->pos < lw->buffer_length) {
      int copylen = lw->buffer_length - lw->pos;
      memcpy(lw->buffer + lw->pos, data, copylen * sizeof(lw->buffer[0]));
      
      lw->depths[lw->pos] = lw->prev_depth;
      lw->prev_depth      = lw->expr_depth;
      for(i = 1; i < copylen; ++i)
        lw->depths[lw->pos + i] = lw->expr_depth;
        
      for(i = 0; i < copylen; ++i)
        lw->char_flags[lw->pos + i] = lw->next_flags;
        
      len  -= copylen;
      data += copylen;
      lw->pos = lw->buffer_length;
    }
    
    flush_line(lw);
  }
}

static pmath_bool_t is_row(pmath_t expr) {
  size_t exprlen;
  pmath_t item;
  
  if(!pmath_is_expr_of(expr, PMATH_SYMBOL_ROW))
    return FALSE;
    
  exprlen = pmath_expr_length(expr);
  if(exprlen < 1 || exprlen > 2)
    return FALSE;
    
  item = pmath_expr_get_item(expr, 1);
  if(pmath_is_expr_of(item, PMATH_SYMBOL_LIST)) {
    pmath_unref(item);
    return TRUE;
  }
  
  pmath_unref(item);
  return FALSE;
}

static void linewriter_pre_write(void *user, pmath_t item, pmath_write_options_t flags) {
  struct linewriter_t *lw = user;
  struct write_pos_t  *wp = pmath_mem_alloc(sizeof(struct write_pos_t));
  
  if(!pmath_is_string(item)) {
    if( 0 == (flags & PMATH_WRITE_OPTIONS_INPUTEXPR) &&
        pmath_is_expr_of_len(item, PMATH_SYMBOL_RAWBOXES, 1))
    {
      lw->rawboxes_depth++;
    }
    
    if(lw->rawboxes_depth > 0) {
      if(pmath_is_expr_of(item, PMATH_SYMBOL_LIST))
        lw->expr_depth++;
    }
    else if(!is_row(item))
      lw->expr_depth++;
  }
  else
    lw->rawboxes_depth++;
    
  lw->next_flags = flags;
  
  if(wp) {
    wp->item            = pmath_ref(item);
    wp->next            = NULL;
    wp->pos             = lw->pos;
    wp->flags           = flags;
    wp->is_start        = TRUE;
    *lw->next_write_pos = wp;
    lw->next_write_pos  = &wp->next;
  }
}

static void linewriter_post_write(void *user, pmath_t item, pmath_write_options_t flags) {
  struct linewriter_t *lw = user;
  struct write_pos_t  *wp = pmath_mem_alloc(sizeof(struct write_pos_t));
  
  if(!pmath_is_string(item)) {
    if(lw->rawboxes_depth > 0) {
      if(pmath_is_expr_of(item, PMATH_SYMBOL_LIST))
        lw->expr_depth--;
    }
    else if(!is_row(item))
      lw->expr_depth--;
      
    if( 0 == (flags & PMATH_WRITE_OPTIONS_INPUTEXPR) &&
        pmath_is_expr_of_len(item, PMATH_SYMBOL_RAWBOXES, 1))
    {
      lw->rawboxes_depth--;
    }
  }
  else
    lw->rawboxes_depth--;
    
  if(lw->prev_depth > lw->expr_depth)
    lw->prev_depth = lw->expr_depth;
    
  lw->next_flags = flags;
  
  if(wp) {
    wp->item            = pmath_ref(item);
    wp->next            = NULL;
    wp->pos             = lw->pos;
    wp->flags           = flags;
    wp->is_start        = FALSE;
    *lw->next_write_pos = wp;
    lw->next_write_pos  = &wp->next;
  }
}

static pmath_bool_t linewriter_custom_formatter(void *user, pmath_t obj, struct pmath_write_ex_t *info) {
  struct linewriter_t *lw = user;
  
  return lw->custom_writer(lw->user, obj, info);
}


static void fallback_write_ex(
  struct pmath_line_writer_options_t *options,
  pmath_t                             obj
) {
  struct pmath_write_ex_t info;
  memset(&info, 0, sizeof(info));
  info.size       = sizeof(info);
  info.options    = options->flags;
  info.user       = options->user;
  info.write      = options->write;
  
  if(PMATH_HAS_MEMBER(options, pre_write))
    info.pre_write = options->pre_write;
    
  if(PMATH_HAS_MEMBER(options, post_write))
    info.post_write = options->post_write;
    
  if(PMATH_HAS_MEMBER(options, custom_formatter))
    info.custom_writer = options->custom_formatter;
    
  _pmath_write_impl(&info, obj);
}

PMATH_API
void pmath_write_with_pagewidth_ex(
  struct pmath_line_writer_options_t *options,
  pmath_t                             obj
) {
  struct linewriter_t lw;
  struct pmath_write_ex_t info;
  
  assert(options != NULL);
  
  assert(options != NULL);
  assert(PMATH_HAS_MEMBER(options, user));
  assert(&options->write != NULL);
  
  if(options->page_width < 0) {
    pmath_t tmp = pmath_evaluate(pmath_ref(PMATH_SYMBOL_PAGEWIDTHDEFAULT));
    
    if(pmath_is_int32(tmp))
      options->page_width = PMATH_AS_INT32(tmp);
    else if(pmath_equals(tmp, _pmath_object_pos_infinity))
      options->page_width = 0xFFFFFF;
    else
      options->page_width = 72;
      
    pmath_unref(tmp);
  }
  
  if(options->page_width > 0xFFFF) {
    fallback_write_ex(options, obj);
    return;
  }
  
  //options->page_width-= options->indentation_width;
  
  if(options->page_width < 6)
    options->page_width = 6;
    
  memset(&lw, 0, sizeof(lw));
  lw.line_length   = options->page_width;
  lw.buffer_length = 2 * options->page_width;
  lw.buffer        = pmath_mem_alloc(sizeof(lw.buffer[0])     *  lw.buffer_length);
  lw.depths        = pmath_mem_alloc(sizeof(lw.depths[0])     *  lw.buffer_length);
  lw.char_flags    = pmath_mem_alloc(sizeof(lw.char_flags[0]) *  lw.buffer_length);
  lw.newlines      = pmath_mem_alloc(sizeof(lw.newlines[0])   * (lw.line_length + 1));
  
  if(!lw.buffer || !lw.depths || !lw.newlines) {
    pmath_mem_free(lw.buffer);
    pmath_mem_free(lw.depths);
    pmath_mem_free(lw.char_flags);
    pmath_mem_free(lw.newlines);
    fallback_write_ex(options, obj);
    return;
  }
  
  lw.next_write_pos       = &lw.all_write_pos;
  lw.indentation_width    = options->indentation_width;
  lw.write                = options->write;
  lw.user                 = options->user;
  if(PMATH_HAS_MEMBER(options, pre_write))
    lw.pre_write          = options->pre_write;
  if(PMATH_HAS_MEMBER(options, post_write))
    lw.post_write         = options->post_write;
  if(PMATH_HAS_MEMBER(options, custom_formatter))
    lw.custom_writer      = options->custom_formatter;
    
  memset(&info, 0, sizeof(info));
  info.size          = sizeof(info);
  info.options       = options->flags;
  info.user          = &lw;
  info.write         = line_write;
  info.pre_write     = linewriter_pre_write;
  info.post_write    = linewriter_post_write;
  info.custom_writer = lw.custom_writer ? linewriter_custom_formatter : NULL;
  
  _pmath_write_impl(&info, obj);
  
  do {
    flush_line(&lw);
  } while(lw.pos > 0);
    
  if(lw.all_write_pos) {
    pmath_debug_print("[unexpected write_pos remaining]\n");
  }
  
  while(lw.all_write_pos) {
    struct write_pos_t *wp = lw.all_write_pos;
    lw.all_write_pos = wp->next;
    
    pmath_unref(wp->item);
    pmath_mem_free(wp);
  }
  
  pmath_mem_free(lw.buffer);
  pmath_mem_free(lw.depths);
  pmath_mem_free(lw.char_flags);
  pmath_mem_free(lw.newlines);
}

PMATH_API
void pmath_write_with_pagewidth(
  pmath_t                 obj,
  pmath_write_options_t   flags,
  void                  (*write)(void *user, const uint16_t *data, int len),
  void                   *user,
  int                     page_width,
  int                     indentation_width
) {
  struct pmath_line_writer_options_t info;
  
  memset(&info, 0, sizeof(info));
  info.size              = sizeof(info);
  info.flags             = flags;
  info.page_width        = page_width;
  info.indentation_width = indentation_width;
  info.user              = user;
  info.write             = write;
  
  pmath_write_with_pagewidth_ex(&info, obj);
}
