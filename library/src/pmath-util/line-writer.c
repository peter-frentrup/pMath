#include <pmath-util/line-writer.h>

#include <pmath-core/numbers-private.h>
#include <pmath-core/strings-private.h>

#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/memory.h>

#include <pmath-builtins/all-symbols-private.h>

#include <string.h>


struct write_pos_t {
  pmath_t             item;
  struct write_pos_t *next;
  int                 pos;
  pmath_bool_t        is_start;
};

#define NEWLINE_NONE       0x00
#define NEWLINE_OK         0x01
#define NEWLINE_INSTRING   0x02

struct linewriter_t {
  int                    line_length;   // > 2
  int                    buffer_length; // > line_length!!!, <= 2*line_length
  uint16_t              *buffer;        // size == buffer_length
  int                   *depths;        // size == buffer_length
  pmath_write_options_t *char_options;  // size == buffer_length
  uint8_t               *newlines;      // size >= line_length + 1
  int                    pos;
  struct write_pos_t    *all_write_pos;
  struct write_pos_t   **next_write_pos;
  int                    string_depth;
  int                    expr_depth;
  int                    prev_depth;
  
  int                    indentation_width;
  pmath_write_options_t  next_options;
  pmath_bool_t           is_inside_rawboxes;
  
  void  *user;
  void (*write)(void *, const uint16_t *, int);
};

static char HEX_DIGITS[] = "0123456789ABCDEF";

static void fill_newlines(struct linewriter_t *lw) {
  struct write_pos_t *wp;
  int string_depth, oldpos;
  
  memset(lw->newlines, NEWLINE_NONE, lw->line_length + 1);
  
  oldpos = -1;
  wp     = lw->all_write_pos;
  
  while(wp && wp->pos <= lw->line_length) {
    if(wp->is_start) {
      if(wp->pos == oldpos) // two expressions without operator/space in between
        lw->newlines[wp->pos] = NEWLINE_INSTRING;
      else
        lw->newlines[wp->pos] = NEWLINE_OK;
    }
    else
      oldpos = wp->pos;
      
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
  
  while(lw->all_write_pos && lw->all_write_pos->pos < end) {
  
    wp                = lw->all_write_pos;
    lw->all_write_pos = wp->next;
    
    if(pmath_is_string(wp->item)) {
      if(wp->is_start)
        lw->string_depth++;
      else
        lw->string_depth--;
    }
    
    pmath_unref(wp->item);
    pmath_mem_free(wp);
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

static void get_string_token_bounds(
  struct linewriter_t *lw,
  int                  pos,
  int                 *start,
  int                 *next
) {
  int i;
  assert(pos <= lw->line_length);
  
  if(pos >= lw->line_length) {
    *start = pos;
    *next  = pos;
    return;
  }
  
  *start = pos;
  *next  = pos + 1;
  
  for(i = pos; i > 0; --i) {
    if((lw->newlines[i] & NEWLINE_INSTRING) == 0)
      break;
      
    if(lw->buffer[i - 1] <= ' ')
      break;
      
    if(lw->buffer[i - 1] == '\\') {
      int s = i;
      while(s > 0 && lw->buffer[s - 1] == '\\')
        --s;
        
      if((i - s) % 2 == 0)
        return;
        
      switch(lw->buffer[i]) {
        case 'x':
          if(pos - i >= 3)
            return;
            
          *start = i - 1;
          *next  = i + 3;
          if(*next > lw->buffer_length)
            *next = lw->buffer_length;
          return;
          
        case 'u':
          if(pos - i >= 5)
            return;
            
          *start = i - 1;
          *next  = i + 5;
          if(*next > lw->buffer_length)
            *next = lw->buffer_length;
          return;
          
        case 'U':
          if(pos - i >= 9)
            return;
            
          *start = i - 1;
          *next  = i + 9;
          if(*next > lw->buffer_length)
            *next = lw->buffer_length;
          return;
      }
      
      if(pos > i)
        return;
        
      *start = i - 1;
      *next  = i + 1;
      return;
    }
  }
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
        
        lw->write(lw->user, lw->buffer, i);
        memmove(lw->buffer,       lw->buffer       + i, sizeof(lw->buffer[      0]) * (lw->buffer_length - i));
        memmove(lw->depths,       lw->depths       + i, sizeof(lw->depths[      0]) * (lw->buffer_length - i));
        memmove(lw->char_options, lw->char_options + i, sizeof(lw->char_options[0]) * (lw->buffer_length - i));
        lw->pos -= i;
        
        depth = get_expr_indention_depth(lw);
        i     = lw->indentation_width + depth;
        while(i-- > 0)
          _pmath_write_cstr(" ", lw->write, lw->user);
        return;
      }
    }
    
    consume_write_pos(lw, lw->pos);
    lw->write(lw->user, lw->buffer, lw->pos);
    lw->pos = 0;
    return;
  }
  
  for(i = 0; i < lw->line_length - depth; ++i) {
    if(lw->buffer[i] == '\n') {
      ++i;
      consume_write_pos(lw, i);
      lw->write(lw->user, lw->buffer, i);
      memmove(lw->buffer,       lw->buffer       + i, sizeof(lw->buffer[      0]) * (lw->buffer_length - i));
      memmove(lw->depths,       lw->depths       + i, sizeof(lw->depths[      0]) * (lw->buffer_length - i));
      memmove(lw->char_options, lw->char_options + i, sizeof(lw->char_options[0]) * (lw->buffer_length - i));
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
    ignore_linebreak = (lw->char_options[nl - 1] & PMATH_WRITE_OPTIONS_INPUTEXPR) != 0;
  else if(is_inside_string)
    hyphenate = (lw->char_options[nl - 1] & PMATH_WRITE_OPTIONS_FULLSTR) != 0;
    
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
  lw->write(lw->user, lw->buffer, nl);
  memmove(lw->buffer,       lw->buffer       + nl, sizeof(lw->buffer[      0]) * (lw->buffer_length - nl));
  memmove(lw->depths,       lw->depths       + nl, sizeof(lw->depths[      0]) * (lw->buffer_length - nl));
  memmove(lw->char_options, lw->char_options + nl, sizeof(lw->char_options[0]) * (lw->buffer_length - nl));
  lw->pos -= nl;
  
  if(ignore_linebreak)
    return;
    
  depth = get_expr_indention_depth(lw);
  if(depth >= nl)
    return;
    
  if(hyphenate) {
    _pmath_write_cstr("\\\n", lw->write, lw->user);
    
    if(is_inside_string) {
      if( lw->pos > 0                         &&
          lw->pos < lw->buffer_length - 4 + 1 &&
          lw->buffer[0] <= ' ')
      {
        char hex_hi = HEX_DIGITS[lw->buffer[0] >> 4];
        char hex_lo = HEX_DIGITS[lw->buffer[0] & 0xF];
        
        injection_adjust_write_pos(lw, 1, 3);
        memmove(lw->buffer       + 3, lw->buffer,       sizeof(lw->buffer[      0]) * (lw->buffer_length - 3));
        memmove(lw->depths       + 3, lw->depths,       sizeof(lw->depths[      0]) * (lw->buffer_length - 3));
        memmove(lw->char_options + 3, lw->char_options, sizeof(lw->char_options[0]) * (lw->buffer_length - 3));
        lw->pos += 3;
        
        lw->depths[0]       = lw->depths[1]       = lw->depths[2]       = lw->depths[3];
        lw->char_options[0] = lw->char_options[1] = lw->char_options[2] = lw->char_options[3];
        lw->buffer[0] = '\\';
        lw->buffer[1] = 'x';
        lw->buffer[2] = hex_hi;
        lw->buffer[3] = hex_lo;
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
        lw->char_options[lw->pos + i] = lw->next_options;
        
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
        lw->char_options[lw->pos + i] = lw->next_options;
        
      len  -= copylen;
      data += copylen;
      lw->pos = lw->buffer_length;
    }
    
    flush_line(lw);
  }
}

static void pre_write(void *user, pmath_t item, pmath_write_options_t options) {
  struct linewriter_t *lw = user;
  struct write_pos_t  *wp = pmath_mem_alloc(sizeof(struct write_pos_t));
  
  if(!pmath_is_string(item)) {
    if( 0 == (options & PMATH_WRITE_OPTIONS_INPUTEXPR) &&
        pmath_is_expr_of_len(item, PMATH_SYMBOL_RAWBOXES, 1))
    {
      lw->is_inside_rawboxes = TRUE;
    }
    
    if(lw->is_inside_rawboxes) {
      if(pmath_is_expr_of(item, PMATH_SYMBOL_LIST))
        lw->expr_depth++;
    }
    else
      lw->expr_depth++;
  }
  
  lw->next_options = options;
  
  if(wp) {
    wp->item            = pmath_ref(item);
    wp->next            = NULL;
    wp->pos             = lw->pos;
    wp->is_start        = TRUE;
    *lw->next_write_pos = wp;
    lw->next_write_pos  = &wp->next;
  }
}

static void post_write(void *user, pmath_t item, pmath_write_options_t options) {
  struct linewriter_t *lw = user;
  struct write_pos_t  *wp = pmath_mem_alloc(sizeof(struct write_pos_t));
  
  if(!pmath_is_string(item)) {
    if(lw->is_inside_rawboxes) {
      if(pmath_is_expr_of(item, PMATH_SYMBOL_LIST))
        lw->expr_depth--;
    }
    else
      lw->expr_depth--;
    
    if( 0 == (options & PMATH_WRITE_OPTIONS_INPUTEXPR) &&
        pmath_is_expr_of_len(item, PMATH_SYMBOL_RAWBOXES, 1))
    {
      lw->is_inside_rawboxes = FALSE;
    }
  }
  
  if(lw->prev_depth > lw->expr_depth)
    lw->prev_depth = lw->expr_depth;
    
  lw->next_options = options;
  
  if(wp) {
    wp->item            = pmath_ref(item);
    wp->next            = NULL;
    wp->pos             = lw->pos;
    wp->is_start        = FALSE;
    *lw->next_write_pos = wp;
    lw->next_write_pos  = &wp->next;
  }
}

PMATH_API
void pmath_write_with_pagewidth(
  pmath_t                 obj,
  pmath_write_options_t   options,
  void                  (*write)(void *user, const uint16_t *data, int len),
  void                   *user,
  int                     page_width,
  int                     indentation_width
) {
  struct linewriter_t lw;
  struct pmath_write_ex_t info;
  
  if(page_width < 0) {
    pmath_t tmp = pmath_evaluate(pmath_ref(PMATH_SYMBOL_PAGEWIDTHDEFAULT));
    
    if(pmath_is_int32(tmp))
      page_width = PMATH_AS_INT32(tmp);
    else if(pmath_equals(tmp, _pmath_object_infinity))
      page_width = 0xFFFFFF;
    else
      page_width = 72;
      
    pmath_unref(tmp);
  }
  
  if(page_width > 0xFFFF) {
    pmath_write(obj, options, write, user);
    return;
  }
  
  //page_width-= indentation_width;
  
  if(page_width < 6)
    page_width = 6;
    
  lw.line_length   = page_width;
  lw.buffer_length = 2 * page_width;
  lw.buffer        = pmath_mem_alloc(sizeof(lw.buffer[0])       *  lw.buffer_length);
  lw.depths        = pmath_mem_alloc(sizeof(lw.depths[0])       *  lw.buffer_length);
  lw.char_options  = pmath_mem_alloc(sizeof(lw.char_options[0]) *  lw.buffer_length);
  lw.newlines      = pmath_mem_alloc(sizeof(lw.newlines[0])     * (lw.line_length + 1));
  
  if(!lw.buffer || !lw.depths || !lw.newlines) {
    pmath_mem_free(lw.buffer);
    pmath_mem_free(lw.depths);
    pmath_mem_free(lw.char_options);
    pmath_mem_free(lw.newlines);
    pmath_write(obj, options, write, user);
    return;
  }
  
  lw.pos                  = 0;
  lw.all_write_pos        = NULL;
  lw.next_write_pos       = &lw.all_write_pos;
  lw.string_depth         = 0;
  lw.expr_depth           = 0;
  lw.prev_depth           = 0;
  lw.indentation_width    = indentation_width;
  lw.next_options         = 0;
  lw.is_inside_rawboxes   = FALSE;
  lw.write                = write;
  lw.user                 = user;
  
  memset(&info, 0, sizeof(info));
  info.size       = sizeof(info);
  info.options    = options;
  info.user       = &lw;
  info.write      = line_write;
  info.pre_write  = pre_write;
  info.post_write = post_write;
  
  pmath_write_ex(&info, obj);
  
  while(lw.pos > 0)
    flush_line(&lw);
    
  while(lw.all_write_pos) {
    struct write_pos_t *wp = lw.all_write_pos;
    lw.all_write_pos = wp->next;
    
    pmath_unref(wp->item);
    pmath_mem_free(wp);
  }
  
  pmath_mem_free(lw.buffer);
  pmath_mem_free(lw.depths);
  pmath_mem_free(lw.char_options);
  pmath_mem_free(lw.newlines);
}
