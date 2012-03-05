#include <pmath-builtins/formating-private.h>

#include <pmath-util/memory.h>

#include <pmath-builtins/all-symbols-private.h>

#include <limits.h>


#define APPROX_SKELETON_WIDTH  8

struct write_short_span_t {
  pmath_t                    item;
  struct write_short_span_t *owner;
  struct write_short_span_t *down;
  struct write_short_span_t *prev;
  struct write_short_span_t *next;
  int                        start;
  int                        end;
  pmath_write_options_t      options;
  pmath_bool_t               is_skeleton;
};

struct write_short_t {
  pmath_string_t text;
  
  struct write_short_span_t *all_spans;
  struct write_short_span_t *current_span;
  
  pmath_bool_t          have_error;
  pmath_write_options_t options;
  
  void (*write)(void *user, const uint16_t *data, int len);
  void  *user;
  void (*pre_write)( void *user, pmath_t obj, pmath_write_options_t options);
  void (*post_write)(void *user, pmath_t obj, pmath_write_options_t options);
};


static void write_short(void *user, const uint16_t *data, int len) {
  struct write_short_t *ws = user;
  
  if(ws->have_error)
    return;
  
  ws->text = pmath_string_insert_ucs2(ws->text, INT_MAX, data, len);
  if(pmath_is_null(ws->text)) {
    ws->have_error = TRUE;
    return;
  }
}

static void pre_write_short(void *user, pmath_t item, pmath_write_options_t options) {
  struct write_short_span_t *span;
  struct write_short_t *ws = user;
  
  if(ws->have_error)
    return;
    
  span = pmath_mem_alloc(sizeof(struct write_short_span_t));
  if(!span) {
    ws->have_error = TRUE;
    return;
  }
  
  if(ws->current_span) {
    assert(ws->current_span->next == NULL);
    
    if(ws->current_span->end < 0) {
      ws->current_span->down = span;
      span->owner            = ws->current_span;
      span->prev             = NULL;
    }
    else {
      ws->current_span->next = span;
      span->owner            = ws->current_span->owner;
      span->prev             = ws->current_span;
    }
  }
  else {
    assert(ws->all_spans == NULL);
    ws->all_spans = span;
    span->owner   = NULL;
    span->prev    = NULL;
  }
  
  span->item        = pmath_ref(item);
  span->down        = NULL;
  span->next        = NULL;
  span->start       = pmath_string_length(ws->text);
  span->end         = -1;
  span->options     = options;
  span->is_skeleton = FALSE;
  ws->current_span  = span;
}

static void post_write_short(void *user, pmath_t item, pmath_write_options_t options) {
  struct write_short_t *ws = user;
  
  if(ws->have_error)
    return;
    
  assert(ws->current_span != NULL);
  
  if(!pmath_same(item, ws->current_span->item)) {
    assert(ws->current_span->end >= 0);
    
    ws->current_span = ws->current_span->owner;
    
    assert(ws->current_span != NULL);
    assert(pmath_same(item, ws->current_span->item));
    
    // options should equal ws->current_span->options
  }
  
  ws->current_span->end = pmath_string_length(ws->text);
}

static void visit_spans(
  struct write_short_t      *ws,
  struct pmath_write_ex_t   *info,
  int                       *pos,
  struct write_short_span_t *span
) {
  struct write_short_span_t *sub;
  const uint16_t *buf = pmath_string_buffer(&ws->text);
  if(!span)
    return;
    
  if(span->start > *pos) {
    ws->write(ws->user, buf + *pos, span->start - *pos);
    *pos = span->start;
  }
  
  if(span->is_skeleton) {
    pmath_write_ex(info, span->item);
    *pos = span->end;
    return;
  }
  
  if(ws->pre_write)
    ws->pre_write(ws->user, span->item, span->options);
    
  sub = span->down;
  while(sub) {
    visit_spans(ws, info, pos, sub);
    sub = sub->next;
  }
  
  if(span->end > *pos) {
    ws->write(ws->user, buf + *pos, span->end - *pos);
    *pos = span->end;
  }
  
  if(ws->post_write){
    pmath_write_options_t old_options;
    if(span->owner)
      old_options = span->owner->options;
    else
      old_options = ws->options;
    
    ws->post_write(ws->user, span->item, old_options);
  }
}

static void free_spans(struct write_short_span_t *span) {

  if(!span)
    return;
    
  pmath_unref(span->item);
  
  while(span->down) {
    struct write_short_span_t *sub = span->down;
    span->down = sub->next;
    free_spans(sub);
  }
  
  pmath_mem_free(span);
}

static void shorten_span(struct write_short_t *ws, struct write_short_span_t *span, int length) {
  if(span->end - span->start < length)
    return;
    
  if(span->down && span->end - span->start > APPROX_SKELETON_WIDTH) {
    struct write_short_span_t *skip;
    int left_pos;
    int right_pos;
    int num_skip;
    
    struct write_short_span_t *left  = span->down;
    struct write_short_span_t *right = left;
    
    while(right->next) {
      assert(right == right->next->prev);
      
      right = right->next;
    }
    
    if(left == right) {
      length -= left->start - span->start;
      length -= span->end - right->end;
      shorten_span(ws, left, length);
      return;
    }
    
    left_pos  = left->start;
    right_pos = right->end;
    
    while(left != right->next) {
      int lew = left_pos - span->start;
      int riw = span->end - right_pos;
      
      if(lew < riw || left == span->down) {
        int w = left->next->start - left_pos;
        
        if(lew + w + riw + APPROX_SKELETON_WIDTH > length) {
          if(!left->down || w < APPROX_SKELETON_WIDTH)
            break;
            
          left     = left->next;
          left_pos = left->start;
          shorten_span(ws, left->prev, length - lew - riw);
          break;
        }
        else {
          left     = left->next;
          left_pos = left->start;
        }
      }
      else {
        int w = right_pos - right->prev->end;
        
        if(lew + w + riw + APPROX_SKELETON_WIDTH > length) {
          if(!right->down || w <= APPROX_SKELETON_WIDTH)
            break;
            
          right     = right->prev;
          right_pos = right->end;
          shorten_span(ws, right->next, length - lew - riw);
          break;
        }
        else {
          right     = right->prev;
          right_pos = right->end;
        }
      }
    }
    
    num_skip = 0;
    skip = left;
    while(skip != right->next) {
      ++num_skip;
      skip = skip->next;
    }
    
    if(num_skip == 1) {
      pmath_unref(left->item);
      left->item = pmath_expr_new_extended(
                     pmath_ref(PMATH_SYMBOL_SKELETON), 1,
                     PMATH_FROM_INT32(1));
                     
      left->is_skeleton = TRUE;
    }
    else if(num_skip > 0) {
      skip = pmath_mem_alloc(sizeof(struct write_short_span_t));
      
      if(skip) {
        skip->item = pmath_expr_new_extended(
                       pmath_ref(PMATH_SYMBOL_SKELETON), 1,
                       PMATH_FROM_INT32(num_skip));
        skip->owner       = span;
        skip->down        = left;
        skip->prev        = left->prev;
        skip->next        = right->next;
        skip->start       = left->start;
        skip->end         = right->end;
        skip->is_skeleton = TRUE;
        
        left->prev  = NULL;
        right->next = NULL;
        
        if(skip->prev)
          skip->prev->next = skip;
        else {
          assert(skip->owner->down == left);
          skip->owner->down = skip;
        }
        
        if(skip->next)
          skip->next->prev = skip;
      }
    }
  }
  else if(length > 6
          && pmath_is_string(span->item)
          && pmath_string_length(span->item) > 6) {
    struct write_short_span_t *skip;
    skip = pmath_mem_alloc(sizeof(struct write_short_span_t));
    
    if(skip) {
      skip->item = pmath_expr_new_extended(
                     pmath_ref(PMATH_SYMBOL_SKELETON), 1,
                     PMATH_FROM_INT32(span->end - span->start - 2 * (length / 2) + 6));
      skip->owner       = span;
      skip->down        = NULL;
      skip->prev        = NULL;
      skip->next        = NULL;
      skip->start       = span->start + length / 2 - 3;
      skip->end         = span->end   - length / 2 + 3;
      skip->is_skeleton = TRUE;
      
      span->down = skip;
    }
  }
  else {
    pmath_unref(span->item);
    span->item = pmath_expr_new_extended(
                   pmath_ref(PMATH_SYMBOL_SKELETON), 1,
                   PMATH_FROM_INT32(1));
    span->is_skeleton = TRUE;
  }
}

PMATH_PRIVATE
void _pmath_write_short(struct pmath_write_ex_t *info, pmath_t obj, int length) {
  struct write_short_t ws;
  
  ws.text         = PMATH_NULL;
  ws.all_spans    = NULL;
  ws.current_span = NULL;
  ws.have_error   = FALSE;
  ws.options      = info->options;
  ws.write        = info->write;
  ws.user         = info->user;
  ws.pre_write    = info->pre_write;
  ws.post_write   = info->post_write;
  
  info->write      = write_short;
  info->user       = &ws;
  info->pre_write  = pre_write_short;
  info->post_write = post_write_short;
  
  pmath_write_ex(info, obj);
  
  info->write      = ws.write;
  info->user       = ws.user;
  info->pre_write  = ws.pre_write;
  info->post_write = ws.post_write;
  
  if(ws.have_error || ws.current_span != ws.all_spans) {
    pmath_write_ex(info, obj);
  }
  else {
    int pos;
    
    shorten_span(&ws, ws.all_spans, length);
    
    pos = 0;
    visit_spans(&ws, info, &pos, ws.all_spans);
    assert(pos == pmath_string_length(ws.text));
  }
  
  pmath_unref(ws.text);
  free_spans(ws.all_spans);
}


PMATH_PRIVATE
void _pmath_write_to_string(
  pmath_string_t *result,
  const uint16_t *data,
  int             len
) {
  *result = pmath_string_insert_ucs2(
              *result, pmath_string_length(*result), data, len);
}

PMATH_PRIVATE pmath_t builtin_tostring(pmath_expr_t expr) {
  /* ToString(object)
   */
  pmath_string_t result;
  size_t i, len;
  
  len = pmath_expr_length(expr);
  if(len == 0) {
    pmath_unref(expr);
    return pmath_string_new(0);
  }
  
  result = PMATH_NULL;
  
  for(i = 1; i <= len; ++i) {
    pmath_t obj = pmath_expr_get_item(expr, i);
    pmath_write(obj, 0, (void(*)(void*, const uint16_t*, int))_pmath_write_to_string, &result);
    pmath_unref(obj);
  }
  pmath_unref(expr);
  
  return result;
}
