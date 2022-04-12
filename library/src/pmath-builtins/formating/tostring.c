#include <pmath-builtins/formating-private.h>

#include <pmath-core/objects-private.h>
#include <pmath-core/numbers-private.h>

#include <pmath-language/charnames.h>

#include <pmath-util/evaluation.h>
#include <pmath-util/line-writer.h>
#include <pmath-util/memory.h>
#include <pmath-util/messages.h>
#include <pmath-util/option-helpers.h>

#include <pmath-builtins/all-symbols-private.h>

#include <limits.h>

#define APPROX_SKELETON_WIDTH  8


extern pmath_symbol_t pmath_Developer_PackedArrayForm;
extern pmath_symbol_t pmath_System_Automatic;
extern pmath_symbol_t pmath_System_CharacterEncoding;
extern pmath_symbol_t pmath_System_False;
extern pmath_symbol_t pmath_System_FullForm;
extern pmath_symbol_t pmath_System_HoldForm;
extern pmath_symbol_t pmath_System_InputForm;
extern pmath_symbol_t pmath_System_OutputForm;
extern pmath_symbol_t pmath_System_PageWidth;
extern pmath_symbol_t pmath_System_ShowStringCharacters;
extern pmath_symbol_t pmath_System_Skeleton;
extern pmath_symbol_t pmath_System_StandardForm;
extern pmath_symbol_t pmath_System_True;
extern pmath_symbol_t pmath_System_Whitespace;


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
  void                      *user;
  pmath_bool_t             (*custom_writer)(void *user, pmath_t obj, struct pmath_write_ex_t *info);
  pmath_bool_t             (*can_write_unicode)(void *user, const uint16_t *str, int len);
  pmath_bool_t               have_error;
};

static void write_short(void *_ws, const uint16_t *data, int len);
static void pre_write_short(void *_ws, pmath_t item, pmath_write_options_t options);
static void post_write_short(void *_ws, pmath_t item, pmath_write_options_t options);
static pmath_bool_t short_custom_writer(void *_ws, pmath_t obj, struct pmath_write_ex_t *info);
static pmath_bool_t short_can_write_unicode(void *_ws, const uint16_t *str, int len);
static void visit_spans(
  struct write_short_t      *ws,
  struct pmath_write_ex_t   *info,
  int                       *pos,
  struct write_short_span_t *span);
// TODO: use arena allocator for spans
static void free_spans(struct write_short_span_t *span);
static void shorten_span(struct write_short_t *ws, struct write_short_span_t *span, int length);


PMATH_PRIVATE
void _pmath_write_short(struct pmath_write_ex_t *info, pmath_t obj, int length) {
  struct write_short_t ws;
  struct pmath_write_ex_t new_info;
  
  ws.text          = PMATH_NULL;
  ws.all_spans     = NULL;
  ws.current_span  = NULL;
  ws.user          = info->user;
  ws.custom_writer = NULL;
  ws.have_error    = FALSE;
  
  memset(&new_info, 0, sizeof(new_info));
  new_info.size              = sizeof(new_info);
  new_info.options           = info->options;
  new_info.user              = &ws;
  new_info.write             = write_short;
  new_info.pre_write         = pre_write_short;
  new_info.post_write        = post_write_short;
  
  if(PMATH_HAS_MEMBER(info, custom_writer) && info->custom_writer) {
    ws.custom_writer       = info->custom_writer;
    new_info.custom_writer = short_custom_writer;
  }
  
  if(PMATH_HAS_MEMBER(info, can_write_unicode) && info->can_write_unicode) {
    ws.can_write_unicode       = info->can_write_unicode;
    new_info.can_write_unicode = short_can_write_unicode;
  }
  
  _pmath_write_impl(&new_info, obj);
  
  if(ws.have_error || ws.current_span != ws.all_spans) {
    _pmath_write_impl(info, obj);
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

static void write_short(void *_ws, const uint16_t *data, int len) {
  struct write_short_t *ws = _ws;
  
  if(ws->have_error)
    return;
    
  ws->text = pmath_string_insert_ucs2(ws->text, INT_MAX, data, len);
  if(pmath_is_null(ws->text)) {
    ws->have_error = TRUE;
    return;
  }
}

static void pre_write_short(void *_ws, pmath_t item, pmath_write_options_t options) {
  struct write_short_span_t *span;
  struct write_short_t *ws = _ws;
  
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

static void post_write_short(void *_ws, pmath_t item, pmath_write_options_t options) {
  struct write_short_t *ws = _ws;
  struct write_short_span_t *span;
  const uint16_t *buf;
  
  if(ws->have_error)
    return;
    
  span = ws->current_span;
  assert(span != NULL);
  
  if(!pmath_same(item, span->item) || span->end >= 0) {
    assert(span->end >= 0);
    
    span = ws->current_span = span->owner;
    
    assert(span != NULL);
    assert(pmath_same(item, span->item));
    
    // options should equal ws->current_span->options
  }
  
  span->end = pmath_string_length(ws->text);
  
  buf = pmath_string_buffer(&ws->text);
  while(span->start < span->end && buf[span->start] == ' ')
    span->start++;
  
  if(span->start < span->end && (buf[span->start] == '+' || buf[span->start] == '-')) {
    span->start++;
    while(span->start < span->end && buf[span->start] == ' ')
      span->start++;
  }
}

static pmath_bool_t short_custom_writer(void *_ws, pmath_t obj, struct pmath_write_ex_t *info) {
  struct write_short_t *ws = _ws;
  
  if(ws->have_error)
    return TRUE;
  
  return ws->custom_writer(ws->user, obj, info);
}

static pmath_bool_t short_can_write_unicode(void *_ws, const uint16_t *str, int len) {
  struct write_short_t *ws = _ws;
  
  if(ws->have_error)
    return FALSE;
  
  return ws->can_write_unicode(ws->user, str, len);
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
    info->write(info->user, buf + *pos, span->start - *pos);
    *pos = span->start;
  }
  
  if(span->is_skeleton) {
    _pmath_write_impl(info, span->item);
    *pos = span->end;
    return;
  }
  
  if(info->pre_write)
    info->pre_write(info->user, span->item, span->options);
    
  sub = span->down;
  while(sub) {
    visit_spans(ws, info, pos, sub);
    sub = sub->next;
  }
  
  if(span->end > *pos) {
    info->write(info->user, buf + *pos, span->end - *pos);
    *pos = span->end;
  }
  
  if(info->post_write) {
    pmath_write_options_t old_options;
    if(span->owner)
      old_options = span->owner->options;
    else
      old_options = info->options;
      
    info->post_write(info->user, span->item, old_options);
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
            
          shorten_span(ws, left, length - lew - riw);
          if(!left->is_skeleton) {
            left     = left->next;
            left_pos = left->start;
          }
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
            
          shorten_span(ws, right, length - lew - riw);
          if(!right->is_skeleton) {
            right     = right->prev;
            right_pos = right->end;
          }
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
                     pmath_ref(pmath_System_Skeleton), 1,
                     PMATH_FROM_INT32(1));
                     
      left->is_skeleton = TRUE;
    }
    else if(left == span->down && !right->next) { // whole span
      pmath_unref(span->item);
      span->item = pmath_expr_new_extended(
                     pmath_ref(pmath_System_Skeleton), 1,
                     PMATH_FROM_INT32(num_skip));
      span->is_skeleton = TRUE;
    }
    else if(num_skip > 0) {
      skip = pmath_mem_alloc(sizeof(struct write_short_span_t));
      
      if(skip) {
        skip->item = pmath_expr_new_extended(
                       pmath_ref(pmath_System_Skeleton), 1,
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
  else if( length > 6                  &&
           pmath_is_string(span->item) &&
           pmath_string_length(span->item) > 6)
  {
    struct write_short_span_t *skip;
    skip = pmath_mem_alloc(sizeof(struct write_short_span_t));
    
    if(skip) {
      skip->item = pmath_expr_new_extended(
                     pmath_ref(pmath_System_Skeleton), 1,
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
                   pmath_ref(pmath_System_Skeleton), 1,
                   PMATH_FROM_INT32(1));
    span->is_skeleton = TRUE;
  }
}

//=================================================================================================

PMATH_PRIVATE
void _pmath_write_to_string(void *pointer_to_pmath_string, const uint16_t *data, int len) {
  pmath_string_t *result = pointer_to_pmath_string;
  *result = pmath_string_insert_ucs2(*result, pmath_string_length(*result), data, len);
}

static void write_ascii_to_string(void *pointer_to_pmath_string, const uint16_t *data, int len) {
  pmath_string_t *result = pointer_to_pmath_string;
  while(len) {
    int i = 0;
    
    while(i < len && data[i] < 0x7F && (data[i] >= ' ' || data[i] == '\n' || data[i] == '\t' || data[i] == '\r'))
      ++i;
    
    if(i) {
      _pmath_write_to_string(result, data, i);
      data+= i;
      len-= i;
    }
    
    if(len) {
      uint32_t unichar = data[0];
      const char *name;
      
      if(len >= 2 &&
        (data[0] & 0xFC00) == 0xD800 &&
        (data[1] & 0xFC00) == 0xDC00)
      {
        unichar = 0x10000 + (((data[0] & 0x03FF) << 10) | (data[1] & 0x03FF));
        data += 2;
        len -= 2;
      }
      else {
        ++data;
        --len;
      }
      
      name = pmath_char_to_name(unichar);
      if(name) {
        //_pmath_write_cstr("\\[", _pmath_write_to_string, result)
        *result = pmath_string_insert_latin1(*result, INT_MAX, "\\[", 2);
        *result = pmath_string_insert_latin1(*result, INT_MAX, name, -1);
        *result = pmath_string_insert_latin1(*result, INT_MAX, "]", 1);
      }
      else {
        static const char hex_digits[] = "0123456789ABCDEF";
        char special[8];
        
        *result = pmath_string_insert_latin1(*result, INT_MAX, "\\[U+", 4);
        
        special[0] = hex_digits[(unichar & 0xF0000000U) >> 28];
        special[1] = hex_digits[(unichar & 0x0F000000U) >> 24];
        special[2] = hex_digits[(unichar & 0x00F00000U) >> 20];
        special[3] = hex_digits[(unichar & 0x000F0000U) >> 16];
        special[4] = hex_digits[(unichar & 0x0000F000U) >> 12];
        special[5] = hex_digits[(unichar & 0x00000F00U) >> 8];
        special[6] = hex_digits[(unichar & 0x000000F0U) >> 4];
        special[7] = hex_digits[ unichar & 0x0000000FU];

        for(i = 0; i <= 3; ++i)
          if(special[i] != '0')
            break;
        
        *result = pmath_string_insert_latin1(*result, INT_MAX, special+i, 8-i);
        *result = pmath_string_insert_latin1(*result, INT_MAX, "]", 1);
      }
      
    }
  }
  
}

static pmath_bool_t apply_format_type_and_free(       pmath_write_options_t              *flags, pmath_t format_type);
static pmath_bool_t apply_characterencoding_option(   struct pmath_line_writer_options_t *lwo,   pmath_t options);
static pmath_bool_t apply_whitespace_option(          pmath_write_options_t              *flags, pmath_t options);
static pmath_bool_t apply_showstringcharacters_option(pmath_write_options_t              *flags, pmath_t options);
static pmath_bool_t extract_pagewidth_option(int *page_width_or_negative, pmath_t options);

PMATH_PRIVATE pmath_t builtin_tostring(pmath_expr_t expr) {
  /*  ToString(object)
      ToString(object, form)    InputForm | OutputForm | StandardForm
  
      options:
        CharacterEncoding -> Automatic | "ASCII" | "Unicode"
        PageWidth -> Infinity
        Whitespace -> Automatic | True | False
        ShowStringCharacters -> False | True | Automatic
   */
  struct pmath_line_writer_options_t lwo;
  pmath_string_t result;
  pmath_t        options;
  pmath_t        obj;
  size_t         exprlen;
  
  exprlen = pmath_expr_length(expr);
  if(exprlen == 0) {
    pmath_unref(expr);
    return pmath_string_new(0);
  }
    
  memset(&lwo, 0, sizeof(lwo));
  lwo.size = sizeof(lwo);
  lwo.user = &result;
  lwo.write = _pmath_write_to_string;
  lwo.page_width = INT_MAX;
  
  options = PMATH_NULL;
  obj = pmath_expr_get_item(expr, 2);
  if(pmath_is_null(obj) || pmath_is_set_of_options(obj)) {
    pmath_unref(obj);
    obj = PMATH_NULL;
    options = pmath_options_extract(expr, 1);
  }
  else {
    if(!apply_format_type_and_free(&lwo.flags, obj))
      return expr;
    
    obj = PMATH_NULL;
    options = pmath_options_extract(expr, 2);
  }
  
  if(pmath_is_null(options))
    return expr;
  
  if( !apply_characterencoding_option(   &lwo, options) ||
      !apply_whitespace_option(          &lwo.flags, options) ||
      !apply_showstringcharacters_option(&lwo.flags, options) ||
      !extract_pagewidth_option(         &lwo.page_width, options))
  {
    pmath_unref(options);
    return expr;
  }
  
  if(lwo.page_width < 0)
    lwo.page_width = INT_MAX;
  
  pmath_unref(options); options = PMATH_NULL;
  result = PMATH_NULL;
  
  obj = pmath_expr_get_item(expr, 1);
  
  pmath_write_with_pagewidth_ex(&lwo, obj);
  
  pmath_unref(obj);
  pmath_unref(expr);
  
  return result;
}

static pmath_bool_t apply_format_type_and_free(pmath_write_options_t *flags, pmath_t format_type) { // format_type will be freed
  if(pmath_same(format_type, pmath_System_InputForm)) 
    *flags = PMATH_WRITE_OPTIONS_FULLSTR | PMATH_WRITE_OPTIONS_INPUTEXPR;
  else if(pmath_same(format_type, pmath_System_FullForm))
    *flags = PMATH_WRITE_OPTIONS_FULLSTR | PMATH_WRITE_OPTIONS_FULLEXPR;
  else if(pmath_same(format_type, pmath_System_OutputForm))
    *flags = 0;
  else if(pmath_same(format_type, pmath_System_StandardForm))
    *flags = 0;
  else if(pmath_same(format_type, pmath_System_HoldForm))
    *flags = 0;
  else if(pmath_same(format_type, pmath_Developer_PackedArrayForm))
    *flags = PMATH_WRITE_OPTIONS_PACKEDARRAYFORM;
  else {
    pmath_message(PMATH_NULL, "fmtval", 1, format_type);
    return FALSE;
  }
  
  pmath_unref(format_type);
  return FALSE;
}

static pmath_bool_t can_write_unicode_true(void *user, const uint16_t *str, int len) {
  return TRUE;
}

static pmath_bool_t apply_characterencoding_option(struct pmath_line_writer_options_t *lwo, pmath_t options) {
  pmath_t enc = pmath_evaluate(pmath_option_value(PMATH_NULL, pmath_System_CharacterEncoding, options));
 
  if(pmath_is_string(enc)) {
    if(pmath_string_equals_latin1(enc, "Unicode")) {
      lwo->flags |= PMATH_WRITE_OPTIONS_PREFERUNICODE;
      lwo->can_write_unicode = can_write_unicode_true;
      pmath_unref(enc);
      return TRUE;
    }
    
    if(pmath_string_equals_latin1(enc, "ASCII")) {
      lwo->write = write_ascii_to_string;
      lwo->flags &= ~PMATH_WRITE_OPTIONS_PREFERUNICODE;
      pmath_unref(enc);
      return TRUE;
    }
  }
  else if(pmath_same(enc, pmath_System_Automatic)) {
    pmath_unref(enc);
    return TRUE;
  }
  
  pmath_message(PMATH_NULL, "charcode", 1, enc); 
  return FALSE;
}

static pmath_bool_t apply_whitespace_option(pmath_write_options_t *flags, pmath_t options) {
  pmath_t whitespace = pmath_evaluate(pmath_option_value(PMATH_NULL, pmath_System_Whitespace, options));
 
  if(pmath_same(whitespace, pmath_System_True)) {
    *flags &= ~PMATH_WRITE_OPTIONS_NOSPACES;
    pmath_unref(whitespace);
    return TRUE;
  }
  
  if(pmath_same(whitespace, pmath_System_False)) {
    *flags |= PMATH_WRITE_OPTIONS_NOSPACES;
    pmath_unref(whitespace);
    return TRUE;
  }
  
  if(pmath_same(whitespace, pmath_System_Automatic)) {
    if(*flags & PMATH_WRITE_OPTIONS_PREFERUNICODE)
      *flags|= PMATH_WRITE_OPTIONS_NOSPACES;
    
    pmath_unref(whitespace);
    return TRUE;
  }
  
  pmath_message(PMATH_NULL, "opttfa", 2, pmath_ref(pmath_System_Whitespace), whitespace); 
  return FALSE;
}

static pmath_bool_t apply_showstringcharacters_option(pmath_write_options_t *flags, pmath_t options) {
  pmath_t fullstr = pmath_evaluate(pmath_option_value(PMATH_NULL, pmath_System_ShowStringCharacters, options));
 
  if(pmath_same(fullstr, pmath_System_True)) {
    *flags |= PMATH_WRITE_OPTIONS_FULLSTR;
    pmath_unref(fullstr);
    return TRUE;
  }
  
  if(pmath_same(fullstr, pmath_System_False)) {
    *flags &= ~PMATH_WRITE_OPTIONS_FULLSTR;
    pmath_unref(fullstr);
    return TRUE;
  }
  
  if(pmath_same(fullstr, pmath_System_Automatic)) {
    pmath_unref(fullstr);
    return TRUE;
  }
  
  pmath_message(PMATH_NULL, "opttfa", 2, pmath_ref(pmath_System_ShowStringCharacters), fullstr); 
  return FALSE;
}

static pmath_bool_t extract_pagewidth_option(int *page_width_or_negative, pmath_t options) {
  pmath_t page_width = pmath_evaluate(pmath_option_value(PMATH_NULL, pmath_System_PageWidth, options));
  
  *page_width_or_negative = -1;
  
  if(pmath_is_int32(page_width) && PMATH_AS_INT32(page_width) > 0) {
    *page_width_or_negative = (int)PMATH_AS_INT32(page_width);
    return TRUE;
  }
  
  if(pmath_is_integer(page_width) && pmath_number_sign(page_width) > 0) {
    *page_width_or_negative = -1;
    pmath_unref(page_width);
    return TRUE;
  }
  
  if(pmath_equals(page_width, _pmath_object_pos_infinity)) {
    *page_width_or_negative = -1;
    pmath_unref(page_width);
    return TRUE;
  }
  
  pmath_message(PMATH_NULL, "ioppf", 2, pmath_ref(pmath_System_PageWidth), page_width);
  return FALSE;
}
