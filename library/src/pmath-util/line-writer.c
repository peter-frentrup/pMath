#include <pmath-util/line-writer.h>

#include <pmath-core/numbers-private.h>
#include <pmath-core/strings-private.h>

#include <pmath-util/evaluation.h>
#include <pmath-util/memory.h>

#include <pmath-builtins/all-symbols-private.h>


#include <string.h>

struct write_pos_t{
  pmath_t             item;
  struct write_pos_t *next;
  int                 pos;
  pmath_bool_t        is_start;
};

#define NEWLINE_NONE       0x00
#define NEWLINE_OK         0x01
#define NEWLINE_INSTRING   0x02

struct linewriter_t{
  int                  line_length;   // > 2
  int                  buffer_length; // > line_length!!!, <= 2*line_length
  uint16_t            *buffer;
  uint8_t             *newlines; // >= line_length + 1
  int                  pos;
  struct write_pos_t  *all_write_pos;
  struct write_pos_t **next_write_pos;
  int                  string_depth;
  
  int indention_width;
  
  void  *user;
  void (*write)(void*,const uint16_t*,int);
};

static void fill_newlines(struct linewriter_t *lw){
  struct write_pos_t *wp;
  int string_depth, oldpos;
  
  memset(lw->newlines, NEWLINE_NONE, lw->line_length + 1);
  
  wp = lw->all_write_pos;
  while(wp && wp->pos <= lw->line_length){
    if(wp->is_start)
      lw->newlines[wp->pos] = NEWLINE_OK;
    
    wp = wp->next;
  }
  
  oldpos = 0;
  string_depth = lw->string_depth;
  wp = lw->all_write_pos;
  while(wp && wp->pos <= lw->line_length){
    if(string_depth > 0){
      while(oldpos < wp->pos)
        lw->newlines[oldpos++] |= NEWLINE_INSTRING;
    }
    
    oldpos = wp->pos;
    if(pmath_is_string(wp->item)){
      if(wp->is_start){
        ++string_depth;
      }
      else
        --string_depth;
    }
    
    wp = wp->next;
  }
}

static void consume_write_pos(struct linewriter_t *lw, int end){
  struct write_pos_t *wp;
  
  while(lw->all_write_pos && lw->all_write_pos->pos < end){
    wp = lw->all_write_pos;
    lw->all_write_pos = wp->next;
    
    if(pmath_is_string(wp->item)){
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
  while(wp){
    wp->pos-= end;
    wp = wp->next;
  }
}

static void flush_line(struct linewriter_t *lw){
  int i, nl;
  pmath_bool_t in_string;
  
  if(lw->pos <= lw->line_length){
    consume_write_pos(lw, lw->pos);
    lw->write(lw->user, lw->buffer, lw->pos);
    lw->pos = 0;
    return;
  }
  
  for(i = 0;i < lw->line_length;++i){
    if(lw->buffer[i] == '\n'){
      ++i;
      consume_write_pos(lw, i);
      lw->write(lw->user, lw->buffer, i);
      memmove(lw->buffer, lw->buffer + i, sizeof(uint16_t) * (lw->buffer_length - i));
      lw->pos-= i;
      return;
    }
  }
  
  fill_newlines(lw);
  nl = lw->line_length - 1;
  while(nl > 0 && !(lw->newlines[nl] & NEWLINE_OK))
    --nl;
  
  if(nl == 0){
    in_string = TRUE;
    
    nl = lw->line_length - 2;
    while(nl > 0 && lw->buffer[nl] > ' ')
      --nl;
    
    if(nl == 0)
      nl = lw->line_length - 2;
    else
      ++nl;
  }
  else{
    in_string = (lw->newlines[nl - 1] & NEWLINE_INSTRING) != 0;
  }
  
  if(in_string){
    if(nl > lw->line_length - 2)
      nl = lw->line_length - 2;
    
    if(lw->buffer[nl-1] == '\\'){
      i = nl-1;
      while(i > 0 && lw->buffer[i-1] == '\\')
        --i;
      
      if((nl - i) % 2 == 1 && nl > 1) 
        --nl;
    }
  }
  
  consume_write_pos(lw, nl);
  lw->write(lw->user, lw->buffer, nl);
  memmove(lw->buffer, lw->buffer + nl, sizeof(uint16_t) * (lw->buffer_length - nl));
  lw->pos-= nl;
  
  if(in_string)
    write_cstr("\\\n", lw->write, lw->user);
  else
    write_cstr("\n", lw->write, lw->user);
  
  i = lw->indention_width;
  while(i-- > 0)
    write_cstr(" ", lw->write, lw->user);
}

static void line_write(void *user, const uint16_t *data, int len){
  struct linewriter_t *lw = user;
  
  assert(lw->line_length > 2);
  
  while(len > 0){
    if(lw->pos + len <= lw->buffer_length){
      memcpy(lw->buffer + lw->pos, data, len * sizeof(uint16_t));
      lw->pos+= len;
      return;
    }
    
    if(lw->pos < lw->buffer_length){
      int copylen = lw->buffer_length - lw->pos;
      memcpy(lw->buffer + lw->pos, data, copylen * sizeof(uint16_t));
      len-=  copylen;
      data+= copylen;
      lw->pos = lw->buffer_length;
    }
    
    flush_line(lw);
  }
}

static void pre_write(void *user, pmath_t item){
  struct linewriter_t *lw = user;
  struct write_pos_t *wp = pmath_mem_alloc(sizeof(struct write_pos_t));
  
  if(wp){
    wp->item     = pmath_ref(item);
    wp->next     = NULL;
    wp->pos      = lw->pos;
    wp->is_start = TRUE;
    *lw->next_write_pos = wp;
    lw->next_write_pos = &wp->next;
  }
}

static void post_write(void *user, pmath_t item){
  struct linewriter_t *lw = user;
  struct write_pos_t *wp = pmath_mem_alloc(sizeof(struct write_pos_t));
  
  if(wp){
    wp->item     = pmath_ref(item);
    wp->next     = NULL;
    wp->pos      = lw->pos;
    wp->is_start = FALSE;
    *lw->next_write_pos = wp;
    lw->next_write_pos = &wp->next;
  }
}

PMATH_API
void pmath_write_with_pagewidth(
  pmath_t                 obj,
  pmath_write_options_t   options,
  void                  (*write)(void *user, const uint16_t *data, int len),
  void                   *user,
  int                     page_width,
  int                     indention_width
){
  struct linewriter_t lw;
  struct pmath_write_ex_t info;
  
  if(page_width < 0){
    pmath_t tmp = pmath_evaluate(pmath_ref(PMATH_SYMBOL_PAGEWIDTHDEFAULT));
    
    if(pmath_is_int32(tmp))
      page_width = PMATH_AS_INT32(tmp);
    else if(pmath_equals(tmp, _pmath_object_infinity))
      page_width = 0xFFFFFF;
    else
      page_width = 72;
    
    pmath_unref(tmp);
  }
  
  if(page_width > 0xFFFF){
    pmath_write(obj, options, write, user);
    return;
  }
  
  //page_width-= indention_width;
  
  if(page_width < 6)
     page_width = 6;
    
  lw.line_length   = page_width;
  lw.buffer_length = 2 * page_width;
  lw.buffer   = pmath_mem_alloc(sizeof(uint16_t) * lw.buffer_length);
  lw.newlines = pmath_mem_alloc(lw.line_length + 1);
  if(!lw.buffer || !lw.newlines){
    pmath_mem_free(lw.buffer);
    pmath_mem_free(lw.newlines);
    pmath_write(obj, options, write, user);
    return;
  }
  
  lw.pos             = 0;
  lw.all_write_pos   = NULL;
  lw.next_write_pos  = &lw.all_write_pos;
  lw.string_depth    = 0;
  lw.indention_width = indention_width;
  lw.write           = write;
  lw.user            = user;
  
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
  
  while(lw.all_write_pos){
    struct write_pos_t *wp = lw.all_write_pos;
    lw.all_write_pos = wp->next;
    
    pmath_unref(wp->item);
    pmath_mem_free(wp);
  }
  
  pmath_mem_free(lw.buffer);
  pmath_mem_free(lw.newlines);
}
