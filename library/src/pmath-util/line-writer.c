#include <pmath-util/line-writer.h>

#include <pmath-core/numbers-private.h>
#include <pmath-core/strings-private.h>

#include <pmath-util/evaluation.h>
#include <pmath-util/memory.h>

#include <pmath-builtins/all-symbols-private.h>


#include <string.h>


struct linewriter_t{
  int            line_length;   // > 2
  int            buffer_length; // > line_length!!!, <= 2*line_length
  uint16_t      *buffer;
  int            pos;
  
  int indention_width;
  
  void  *user;
  void (*write)(void*,const uint16_t*,int);
};

static void flush_line(struct linewriter_t *lw){
  int i;
  
  if(lw->pos <= lw->line_length){
    lw->write(lw->user, lw->buffer, lw->pos);
    lw->pos = 0;
    return;
  }
  
  for(i = 0;i < lw->line_length;++i){
    if(lw->buffer[i] == '\n'){
      ++i;
      lw->write(lw->user, lw->buffer, i);
      memmove(lw->buffer, lw->buffer + i, sizeof(uint16_t) * (lw->buffer_length - i));
      lw->pos-= i;
      return;
    }
  }
  
  i = lw->line_length-1;
  while(i > 0 && lw->buffer[i] > ' ')
    --i;
  
  if(i > 0){
    ++i;
    lw->write(lw->user, lw->buffer, i);
    memmove(lw->buffer, lw->buffer + i, sizeof(uint16_t) * (lw->buffer_length - i));
    lw->pos-= i;
    
    write_cstr("\n", lw->write, lw->user);
  }
  else{
    int wlen = lw->line_length - 2;
    
    lw->write(lw->user, lw->buffer, wlen);
    memmove(lw->buffer, lw->buffer + wlen, sizeof(uint16_t) * (lw->buffer_length - wlen));
    lw->pos-= wlen;
    
    write_cstr("\\\n", lw->write, lw->user);
  }
  
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
  
  page_width-= indention_width;
  
  if(page_width < 6)
     page_width = 6;
    
  lw.line_length   = page_width;
  lw.buffer_length = 2 * page_width;
  lw.buffer = pmath_mem_alloc(sizeof(uint16_t) * lw.buffer_length);
  if(!lw.buffer){
    pmath_write(obj, options, write, user);
    return;
  }
  
  lw.pos             = 0;
  lw.indention_width = indention_width;
  lw.write           = write;
  lw.user            = user;
  
  pmath_write(obj, options, line_write, &lw);
  
  while(lw.pos > 0)
    flush_line(&lw);
  
  pmath_mem_free(lw.buffer);
}
