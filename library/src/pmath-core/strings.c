#include <pmath-core/objects-private.h>
#include <pmath-core/strings-private.h>

#include <pmath-language/charnames.h>
#include <pmath-language/scanner.h>
#include <pmath-language/tokens.h>

#include <pmath-util/concurrency/threads-private.h>
#include <pmath-util/helpers.h>
#include <pmath-util/incremental-hash-private.h>
#include <pmath-util/memory.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/control-private.h>
#include <pmath-builtins/formating-private.h>
#include <pmath-builtins/io-private.h>

#include <errno.h>
#include <iconv.h>
#include <limits.h>
#include <string.h>


static iconv_t to_utf8   = (iconv_t) - 1;
static iconv_t from_utf8 = (iconv_t) - 1;

static iconv_t to_native   = (iconv_t) - 1;
static iconv_t from_native = (iconv_t) - 1;



PMATH_PRIVATE
struct _pmath_string_t *_pmath_new_string_buffer(int size) {
  size_t bytes;
  int len = size;
  struct _pmath_string_t *result;

  if(size < 0 || (int)LENGTH_TO_CAPACITY(size) < size)
    return NULL;

  size = (int)LENGTH_TO_CAPACITY(size);
  bytes = (size_t)size * sizeof(uint16_t);
  if(bytes / sizeof(uint16_t) != (size_t)size)
    return NULL;

  result = (void *)PMATH_AS_PTR(_pmath_create_stub(
      PMATH_TYPE_SHIFT_BIGSTRING,
      STRING_HEADER_SIZE + bytes));
  if(!result)
    return result;
  
  result->debug_info        = NULL;
  result->buffer            = NULL;
  result->length            = len;
  result->capacity_or_start = size;

  return result;
}

PMATH_PRIVATE
pmath_t _pmath_from_buffer(struct _pmath_string_t *b) {
  if(b && b->length <= 2 && b->debug_info == NULL) {
    pmath_t result;
    const uint16_t *buf = (b->buffer ? AFTER_STRING(b->buffer) + b->capacity_or_start : AFTER_STRING(b));

    switch(b->length) {
      case 0:
        result.s.tag = PMATH_TAG_STR0;
        result.s.u.as_int32 = 0;
        pmath_unref(PMATH_FROM_PTR(b));
        return result;

      case 1:
        result.s.tag = PMATH_TAG_STR1;
        result.s.u.as_chars[0] = buf[0];
        result.s.u.as_chars[1] = 0;
        pmath_unref(PMATH_FROM_PTR(b));
        return result;

      case 2:
        result.s.tag = PMATH_TAG_STR2;
        result.s.u.as_chars[0] = buf[0];
        result.s.u.as_chars[1] = buf[1];
        pmath_unref(PMATH_FROM_PTR(b));
        return result;
    }
  }

  return PMATH_FROM_PTR(b);
}

PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
pmath_t _pmath_string_get_debug_info(pmath_t str) {
  struct _pmath_string_t *_str;
  
  if(pmath_is_null(str))
    return PMATH_NULL;
  
  assert(pmath_is_string(str));
  if(!pmath_is_pointer(str))
    return PMATH_NULL;
  
  _str = (struct _pmath_string_t *)PMATH_AS_PTR(str);
  return pmath_ref(PMATH_FROM_PTR(_str->debug_info));
}

PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
pmath_t _pmath_string_set_debug_info(pmath_t str, pmath_t info) {
  struct _pmath_string_t *str_ptr = NULL;
  struct _pmath_string_t *result = NULL;
  struct _pmath_t *info_ptr;
  
  if(!pmath_is_pointer(info))
    return str;
  
  info_ptr = PMATH_AS_PTR(info);
  
  if(pmath_is_null(str)) {
    pmath_unref(info);
    return str;
  }
  
  if(pmath_is_ministr(str)) {
    if(info_ptr == NULL)
      return str;
    
    if(pmath_is_str0(str)) {
      result = _pmath_new_string_buffer(0);
    }
    else if(pmath_is_str1(str)) {
      result = _pmath_new_string_buffer(1);
      if(result) {
        AFTER_STRING(result)[0] = str.s.u.as_chars[0];
      }
    }
    else if(pmath_is_str2(str)) {
      result = _pmath_new_string_buffer(2);
      if(result) {
        AFTER_STRING(result)[0] = str.s.u.as_chars[0];
        AFTER_STRING(result)[1] = str.s.u.as_chars[2];
      }
    }
    if(!result) {
      pmath_unref(info);
      return str;
    }
    
    result->debug_info = info_ptr;
    
    // no need to free str: it is a ministr
    return PMATH_FROM_PTR(result);
  }
  
  assert(pmath_is_bigstr(str));
  
  str_ptr = (void*)PMATH_AS_PTR(str);
  if(str_ptr->debug_info == info_ptr) {
    if(info_ptr)
      _pmath_unref_ptr(info_ptr);
    
    return str;
  }
  
  if(pmath_refcount(str) == 1) {
    if(str_ptr->debug_info)
      _pmath_unref_ptr(str_ptr->debug_info);
    
    str_ptr->debug_info = info_ptr;
    return str;
  }
  else {
    result = (void *)PMATH_AS_PTR(_pmath_create_stub(PMATH_TYPE_SHIFT_BIGSTRING, sizeof(struct _pmath_string_t)));
    if(!result) {
      pmath_unref(info);
      return str;
    }
    
    result->debug_info        = info_ptr;
    result->buffer            = str_ptr;
    result->length            = str_ptr->length;
    result->capacity_or_start = 0;

    return PMATH_FROM_PTR(result);
  }
}

PMATH_PRIVATE
struct _pmath_string_t *enlarge_string(
    struct _pmath_string_t *string, // will be freed
    int                     extra_start,
    int                     extralen // not negative
) {
  struct _pmath_string_t *result;
  const uint16_t *buf;

  if(extralen <= 0) {
    pmath_unref(PMATH_FROM_PTR(string));
    return NULL;
  }

  assert(extra_start >= 0);

  if(!string)
    return _pmath_new_string_buffer(extralen);

  assert(extra_start <= string->length);

  if( string->buffer == NULL &&
      pmath_atomic_read_aquire(&string->inherited.refcount) == 1)
  {
    size_t newcap;

    unsigned new_length = (unsigned)string->length + (unsigned)extralen;
    if(new_length < (unsigned)string->length || new_length > INT_MAX) {
      pmath_abort_please();
      pmath_unref(PMATH_FROM_PTR(string));
      return NULL;
    }

    if((unsigned)string->capacity_or_start >= new_length) {
      memmove(
          AFTER_STRING(string) + extra_start + extralen,
          AFTER_STRING(string) + extra_start,
          (string->length - extra_start) * sizeof(uint16_t));
      string->length = (int)new_length;
      return string;
    }

    newcap = LENGTH_TO_CAPACITY(new_length);
    if( newcap > (INT_MAX - STRING_HEADER_SIZE) / sizeof(uint16_t) ||
        (newcap * sizeof(uint16_t)) / sizeof(uint16_t) != newcap)
    {
      pmath_abort_please();
      pmath_unref(PMATH_FROM_PTR(string));
      return NULL;
    }

    result = (struct _pmath_string_t *)
        pmath_mem_realloc(string, STRING_HEADER_SIZE + sizeof(uint16_t) * newcap);
    if(!result)
      return NULL;

    if(result->length > extra_start) {
      memmove(
          AFTER_STRING(result) + extra_start + extralen,
          AFTER_STRING(result) + extra_start,
          (result->length - extra_start) * sizeof(uint16_t));
    }
    result->capacity_or_start = (int)newcap;
    result->length += extralen;
    return result;
  }

  result = _pmath_new_string_buffer(string->length + extralen);
  if(!result) {
    pmath_unref(PMATH_FROM_PTR(string));
    return NULL;
  }

  if(string->buffer)
    buf = AFTER_STRING(string->buffer) + string->capacity_or_start;
  else
    buf = AFTER_STRING(string);

  memcpy(
      AFTER_STRING(result),
      buf,
      extra_start * sizeof(uint16_t));

  memcpy(
      AFTER_STRING(result) + extra_start + extralen,
      buf + extra_start,
      (string->length - extra_start) * sizeof(uint16_t));

  pmath_unref(PMATH_FROM_PTR(string));
  return result;
}

static
struct _pmath_string_t *enlarge_string_2(
    pmath_string_t string,
    int            extra_start,
    int            extralen
) {
  struct _pmath_string_t *result;
  uint16_t *buffer;

  if(pmath_is_pointer(string)) {
    return enlarge_string(
        (struct _pmath_string_t *)PMATH_AS_PTR(string), extra_start, extralen);
  }

  assert(pmath_is_ministr(string));
  assert(extra_start >= 0);
  assert(extralen >= 0);

  if(pmath_is_str0(string)) {
    assert(extra_start == 0);
    return _pmath_new_string_buffer(extralen);
  }

  if(pmath_is_str1(string)) {
    assert(extra_start <= 1);

    result = _pmath_new_string_buffer(1 + extralen);
    if(!result)
      return NULL;

    buffer = AFTER_STRING(result);

    if(extra_start == 0) {
      buffer[extralen] = string.s.u.as_chars[0];
    }
    else {
      buffer[0] = string.s.u.as_chars[0];
    }

    return result;
  }

  assert(pmath_is_str2(string));
  assert(extra_start <= 2);

  result = _pmath_new_string_buffer(2 + extralen);
  if(!result)
    return NULL;

  buffer = AFTER_STRING(result);
  if(extra_start == 0) {
    buffer[extralen]   = string.s.u.as_chars[0];
    buffer[extralen + 1] = string.s.u.as_chars[1];
  }
  else if(extra_start == 1) {
    buffer[0]          = string.s.u.as_chars[0];
    buffer[extralen + 1] = string.s.u.as_chars[1];
  }
  else {
    buffer[0] = string.s.u.as_chars[0];
    buffer[1] = string.s.u.as_chars[1];
  }

  return result;
}

static void destroy_string(pmath_t p) {
  struct _pmath_string_t *str = (void *)PMATH_AS_PTR(p);
  
  if(str->debug_info)
    _pmath_unref_ptr(str->debug_info);
  if(str->buffer)
    _pmath_unref_ptr((void*)str->buffer);

  pmath_mem_free(str);
}

PMATH_PRIVATE
pmath_bool_t _pmath_strings_equal(
    pmath_t strA,
    pmath_t strB
) {
  const uint16_t *bufA;
  const uint16_t *bufB;
  int len = pmath_string_length(strA);

  if(len != pmath_string_length(strB))
    return FALSE;

  bufA = pmath_string_buffer(&strA);
  bufB = pmath_string_buffer(&strB);

  for(; len > 0; --len) {
    if(*(bufA++) != *(bufB++))
      return FALSE;
  }

  return TRUE;
}

PMATH_PRIVATE
int _pmath_strings_compare(
    pmath_t strA,
    pmath_t strB
) {
  const uint16_t *bufA = pmath_string_buffer(&strA);
  const uint16_t *bufB = pmath_string_buffer(&strB);
  int lenA = pmath_string_length(strA);
  int lenB = pmath_string_length(strB);

  int i;
  for(i = 0; i < lenA && i < lenB; i++) {
    if(bufA[i] < bufB[i])
      return -1;
    if(bufA[i] > bufB[i])
      return 1;
  }
  if(lenA < lenB)
    return -1;
  if(lenA > lenB)
    return 1;
  return 0;
}

static unsigned int hash_string(pmath_t str) {
  int len             = pmath_string_length(str);
  const uint16_t *buf = pmath_string_buffer(&str);

  if(len <= 2) { /* could be a ministr */
    pmath_t tmp;
    switch(len) {
      case 0:
        tmp.s.tag = PMATH_TAG_STR0;
        tmp.s.u.as_int32 = 0;
        break;

      case 1:
        tmp.s.tag = PMATH_TAG_STR1;
        tmp.s.u.as_chars[0] = buf[0];
        tmp.s.u.as_chars[1] = 0;
        break;

      case 2:
        tmp.s.tag = PMATH_TAG_STR2;
        tmp.s.u.as_chars[0] = buf[0];
        tmp.s.u.as_chars[1] = buf[1];
        break;
    }

    return incremental_hash(&tmp, sizeof(pmath_t), 0);
  }

  return incremental_hash(buf, (size_t)len * sizeof(uint16_t), 0);
}

PMATH_PRIVATE
void _pmath_write_cstr(
    const char          *str,
    void (*write_ucs2)(void *, const uint16_t *, int),
    void                *user
) {
  int len = strlen(str);
#define BUFLEN 256
  uint16_t buf[BUFLEN];
  while(len > BUFLEN) {
    int i;
    for(i = 0; i < BUFLEN; ++i)
      buf[i] = (uint16_t)(unsigned char)str[i];
    write_ucs2(user, buf, BUFLEN);
    str += BUFLEN;
    len -= BUFLEN;
  }
  if(len > 0) {
    int i;
    for(i = 0; i < len; ++i)
      buf[i] = (uint16_t)(unsigned char)str[i];
    write_ucs2(user, buf, len);
  }
#undef BUFLEN
}

static pmath_bool_t is_single_token(pmath_t box) {
  if(pmath_is_string(box))
    return TRUE;

  if( pmath_is_expr_of(box, PMATH_NULL) ||
      pmath_is_expr_of(box, PMATH_SYMBOL_LIST))
  {
    pmath_t part;
    pmath_bool_t result;

    if(pmath_expr_length(box) != 1)
      return FALSE;

    part = pmath_expr_get_item(box, 1);
    result = is_single_token(part);
    pmath_unref(part);
    return result;
  }

  if( pmath_is_expr_of(box, PMATH_SYMBOL_STYLEBOX) ||
      pmath_is_expr_of(box, PMATH_SYMBOL_TAGBOX) ||
      pmath_is_expr_of(box, PMATH_SYMBOL_INTERPRETATIONBOX))
  {
    pmath_t part;
    pmath_bool_t result;

    part = pmath_expr_get_item(box, 1);
    result = is_single_token(part);
    pmath_unref(part);
    return result;
  }

  return FALSE;
}

static void write_single_token_box(struct pmath_write_ex_t *info, pmath_t box) {
  if(is_single_token(box)) {
    _pmath_write_boxes(info, box);
  }
  else {
    _pmath_write_cstr("(", info->write, info->user);
    _pmath_write_boxes(info, box);
    _pmath_write_cstr(")", info->write, info->user);
  }
}


static void write_and_skip_string_chars(void *user, const uint16_t *data, int len) {
  struct pmath_write_ex_t *old_info = user;
  int i = 0;

  while(i < len) {
    if( data[i] == '\\' &&
        i + 1 < len &&
        (data[i + 1] == '"' || data[i + 1] == '\\'))
    {
      old_info->write(old_info->user, data, i);

      old_info->write(old_info->user, &data[i + 1], 1);

      i    += 2;
      data += i;
      len  -= i;
      i     = 0;
      continue;
    }

    if(data[i] == '"') {
      old_info->write(old_info->user, data, i);
      ++i;
      data += i;
      len  -= i;
      i     = 0;
      continue;
    }

    ++i;
  }

  if(len > 0)
    old_info->write(old_info->user, data, len);
}

static void call_old_pre_write(void *user, pmath_t obj, pmath_write_options_t options) {
  struct pmath_write_ex_t *old_info = user;

  old_info->pre_write(old_info->user, obj, options);
}

static void get_token_spacing(pmath_string_t token, const char **pre, const char **post) {
  PMATH_ATTRIBUTE_UNUSED pmath_token_t tok;
  int prec;

  *pre  = "";
  *post = "";

  tok = pmath_token_analyse(pmath_string_buffer(&token), pmath_string_length(token), &prec);

  if(prec <= PMATH_PREC_ANY) {
  }
  else if(prec <= PMATH_PREC_MODY) {
    *post = " ";
  }
  else if(prec == PMATH_PREC_FUNC) {
    *pre  = " ";
  }
  else if(prec <= PMATH_PREC_PLUMI) {
    *pre  = " ";
    *post = " ";
  }
}

static void call_old_post_write(void *user, pmath_t obj, pmath_write_options_t options) {
  struct pmath_write_ex_t *old_info = user;

  old_info->post_write(old_info->user, obj, options);
}

static void write_boxes_impl(struct pmath_write_ex_t *info, pmath_t box) {
  if(pmath_is_string(box)) {
    info->write(info->user, pmath_string_buffer(&box), pmath_string_length(box));
    return;
  }

  if( pmath_is_expr_of(box, PMATH_SYMBOL_COMPLEXSTRINGBOX) ||
      pmath_is_expr_of(box, PMATH_NULL))
  {
    size_t i;

    for(i = 1; i <= pmath_expr_length(box); ++i) {
      pmath_t part = pmath_expr_get_item(box, i);

      _pmath_write_boxes(info, part);

      pmath_unref(part);
    }

    return;
  }

  if(pmath_is_expr_of(box, PMATH_SYMBOL_LIST)) {
    size_t i;
    size_t boxlen = pmath_expr_length(box);

    for(i = 1; i <= boxlen; ++i) {
      pmath_t part = pmath_expr_get_item(box, i);

      if(pmath_is_string(part)) {
        const char *pre;
        const char *post;

        get_token_spacing(part, &pre, &post);

        if(*pre && i > 1)
          _pmath_write_cstr(pre, info->write, info->user);

        info->write(info->user, pmath_string_buffer(&part), pmath_string_length(part));

        if(*post && i < boxlen) {
          pmath_t next = pmath_expr_get_item(box, i + 1);
          if( pmath_is_expr_of(next, PMATH_SYMBOL_SUBSCRIPTBOX)     ||
              pmath_is_expr_of(next, PMATH_SYMBOL_SUPERSCRIPTBOX)   ||
              pmath_is_expr_of(next, PMATH_SYMBOL_SUBSUPERSCRIPTBOX))
          {
            _pmath_write_boxes(info, next);
            ++i;
          }

          pmath_unref(next);

          if(i < boxlen)
            _pmath_write_cstr(post, info->write, info->user);
        }
      }
      else
        _pmath_write_boxes(info, part);

      pmath_unref(part);
    }

    return;
  }

  if(pmath_is_expr_of(box, PMATH_SYMBOL_STYLEBOX)) {
    pmath_bool_t hide_string_characters = FALSE;
    pmath_t part;

    size_t i;
    for(i = pmath_expr_length(box); i > 1; --i) {
      pmath_t option = pmath_expr_get_item(box, i);

      if(_pmath_is_rule(option)) {
        pmath_t lhs = pmath_expr_get_item(option, 1);
        pmath_t rhs = pmath_expr_get_item(option, 2);
        pmath_unref(lhs);
        pmath_unref(rhs);

        if(pmath_same(lhs, PMATH_SYMBOL_SHOWSTRINGCHARACTERS))
          hide_string_characters = pmath_same(rhs, PMATH_SYMBOL_FALSE);
      }

      pmath_unref(option);
    }

    if(hide_string_characters) {
      struct pmath_write_ex_t info2;
      memset(&info2, 0, sizeof(info2));
      info2.size       = sizeof(info2);
      info2.options    = info->options;
      info2.user       = info;
      info2.write      = write_and_skip_string_chars;
      info2.pre_write  = call_old_pre_write;
      info2.post_write = call_old_post_write;

      part = pmath_expr_get_item(box, 1);
      _pmath_write_boxes(&info2, part);
      pmath_unref(part);

      return;
    }

    part = pmath_expr_get_item(box, 1);
    _pmath_write_boxes(info, part);
    pmath_unref(part);

    return;
  }

  if( pmath_is_expr_of(box, PMATH_SYMBOL_TAGBOX)       ||
      pmath_is_expr_of(box, PMATH_SYMBOL_TOOLTIPBOX)   ||
      pmath_is_expr_of(box, PMATH_SYMBOL_INTERPRETATIONBOX))
  {
    pmath_t part = pmath_expr_get_item(box, 1);
    _pmath_write_boxes(info, part);
    pmath_unref(part);

    return;
  }

  if(pmath_is_expr_of(box, PMATH_SYMBOL_SUBSCRIPTBOX)) {
    pmath_t part = pmath_expr_get_item(box, 1);
    _pmath_write_cstr("_", info->write, info->user);
    write_single_token_box(info, part);
    pmath_unref(part);

    return;
  }

  if(pmath_is_expr_of(box, PMATH_SYMBOL_SUPERSCRIPTBOX)) {
    pmath_t part = pmath_expr_get_item(box, 1);
    _pmath_write_cstr("^", info->write, info->user);
    write_single_token_box(info, part);
    pmath_unref(part);

    return;
  }

  if(pmath_is_expr_of(box, PMATH_SYMBOL_SUBSUPERSCRIPTBOX)) {
    pmath_t part = pmath_expr_get_item(box, 1);
    _pmath_write_cstr("_", info->write, info->user);
    write_single_token_box(info, part);
    pmath_unref(part);

    part = pmath_expr_get_item(box, 2);
    _pmath_write_cstr("^", info->write, info->user);
    write_single_token_box(info, part);
    pmath_unref(part);

    return;
  }

  if(pmath_is_expr_of(box, PMATH_SYMBOL_UNDERSCRIPTBOX)) {
    pmath_t part = pmath_expr_get_item(box, 1);
    _pmath_write_boxes(info, part);
    pmath_unref(part);

    part = pmath_expr_get_item(box, 2);
    _pmath_write_cstr("_", info->write, info->user);
    write_single_token_box(info, part);
    pmath_unref(part);

    return;
  }

  if(pmath_is_expr_of(box, PMATH_SYMBOL_OVERSCRIPTBOX)) {
    pmath_t part = pmath_expr_get_item(box, 1);
    _pmath_write_boxes(info, part);
    pmath_unref(part);

    part = pmath_expr_get_item(box, 2);
    _pmath_write_cstr("^", info->write, info->user);
    write_single_token_box(info, part);
    pmath_unref(part);

    return;
  }

  if(pmath_is_expr_of(box, PMATH_SYMBOL_UNDEROVERSCRIPTBOX)) {
    pmath_t part = pmath_expr_get_item(box, 1);
    _pmath_write_boxes(info, part);
    pmath_unref(part);

    part = pmath_expr_get_item(box, 2);
    _pmath_write_cstr("_", info->write, info->user);
    write_single_token_box(info, part);
    pmath_unref(part);

    part = pmath_expr_get_item(box, 3);
    _pmath_write_cstr("^", info->write, info->user);
    write_single_token_box(info, part);
    pmath_unref(part);

    return;
  }

  if(pmath_is_expr(box)) {
    pmath_t part;
    size_t i;

    part = pmath_expr_get_item(box, 0);
    pmath_write_ex(info, part);
    pmath_unref(part);

    _pmath_write_cstr("(", info->write, info->user);
    for(i = 1; i <= pmath_expr_length(box); ++i) {
      if(i > 1)
        _pmath_write_cstr(", ", info->write, info->user);

      part = pmath_expr_get_item(box, i);
      _pmath_write_boxes(info, part);
      pmath_unref(part);

    }
    _pmath_write_cstr(")", info->write, info->user);

    return;
  }

  pmath_write_ex(info, box);
}

PMATH_PRIVATE
void _pmath_write_boxes(struct pmath_write_ex_t *info, pmath_t box) {
  if(info->pre_write)
    info->pre_write(info->user, box, info->options);

  write_boxes_impl(info, box);

  if(info->post_write)
    info->post_write(info->user, box, info->options);
}

PMATH_PRIVATE
void _pmath_string_write_escaped(
    pmath_t          str,     // wont be freed
    pmath_bool_t     only_ascii,
    void           (*write)(void *user, const uint16_t *data, int len),
    void            *user
) {
  static char hex_digits[16] = "0123456789ABCDEF";

  const uint16_t *buffer = pmath_string_buffer(&str);
  const uint16_t *end    = buffer + pmath_string_length(str);
  const uint16_t *s      = buffer;

  if(only_ascii) {
    while(s != end) {
      const uint16_t *start = s;
      int len = 0;
      while( s != end   &&
          *s >= ' '  &&
          *s != '\"' &&
          *s != '\\' &&
          *s <= 0x7F)
      {
        ++s;
        ++len;
      }

      if(start != s)
        write(user, start, len);

      if(s != end) {
        uint16_t special[10];
        special[0] = '\\';
        switch(*s) {
          case '\"': special[1] = '\"'; write(user, special, 2); break;
          case '\\': special[1] = '\\'; write(user, special, 2); break;
          case '\n': special[1] = 'n';  write(user, special, 2); break;
          case '\r': special[1] = 'r';  write(user, special, 2); break;
          case PMATH_CHAR_LEFT_BOX:
            special[1] = '(';
            write(user, special, 2);
            break;
          case PMATH_CHAR_RIGHT_BOX:
            special[1] = ')';
            write(user, special, 2);
            break;

          default: {
              uint32_t unichar;
              const char *charname;

              if( s + 1 != end              &&
                  (s[0] & 0xFC00) == 0xD800 &&
                  (s[1] & 0xFC00) == 0xDC00)
              {
                unichar = 0x10000 + ((((uint32_t)s[0] & 0x03FF) << 10) | (s[1] & 0x03FF));

                ++s;
              }
              else
                unichar = *s;

              charname = pmath_char_to_name(unichar);
              _pmath_write_cstr("\\[", write, user);

              if(charname) {
                _pmath_write_cstr(charname, write, user);
              }
              else {
                int i;
                _pmath_write_cstr("U+", write, user);

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

                write(user, special + i, 8 - i);
              }

              _pmath_write_cstr("]",   write, user);
            }
        }
        ++s;
      }
    }
  }
  else {
    while(s != end) {
      const uint16_t *start = s;
      int len = 0;
      while(s != end && *s != '\"' && *s != '\\')
      {
        ++s;
        ++len;
      }

      if(start != s)
        write(user, start, len);

      if(s != end) {
        uint16_t special[10];
        special[0] = '\\';
        switch(*s) {
          case '\"': special[1] = '\"'; write(user, special, 2); break;
          case '\\': special[1] = '\\'; write(user, special, 2); break;
        }
        ++s;
      }
    }
  }
}

PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
pmath_t _pmath_escape_string(
    pmath_string_t prefix, // will be freed
    pmath_string_t string, // will be freed
    pmath_string_t suffix, // will be freed
    pmath_bool_t   only_ascii
) {
  pmath_string_t result = prefix;

  if(pmath_is_null(result))
    result = PMATH_FROM_TAG(PMATH_TAG_STR0, 0);

  _pmath_string_write_escaped(
      string,
      only_ascii,
      (void( *)(void *, const uint16_t *, int))_pmath_write_to_string,
      &result);

  pmath_unref(string);
  return pmath_string_concat(result, suffix);
}

PMATH_PRIVATE
void _pmath_string_write(struct pmath_write_ex_t *info, pmath_t str) {
  if(info->options & PMATH_WRITE_OPTIONS_FULLSTR)  {
    _pmath_write_cstr("\"", info->write, info->user);

    _pmath_string_write_escaped(
        str,
        (info->options & PMATH_WRITE_OPTIONS_INPUTEXPR) != 0,
        info->write,
        info->user);

    _pmath_write_cstr("\"", info->write, info->user);
  }
  else {
    pmath_t expanded = pmath_string_expand_boxes(pmath_ref(str));

    _pmath_write_boxes(info, expanded);

    pmath_unref(expanded);
  }
}

/*============================================================================*/

PMATH_API
pmath_string_t pmath_string_new(int capacity) {
  struct _pmath_string_t *result;

  assert(capacity >= 0);

  if(capacity <= 2) {
    pmath_string_t str;
    str.s.tag = PMATH_TAG_STR0;
    str.s.u.as_int32 = 0;
    return str;
  }

  result = _pmath_new_string_buffer(capacity);
  if(!result)
    return PMATH_NULL;

  result->length = 0;
  return PMATH_FROM_PTR(result);
}

PMATH_API pmath_string_t pmath_string_insert_latin1(
    pmath_string_t str,
    int            inspos,
    const char    *ins,
    int            inslen
) {
  struct _pmath_string_t *result;
  uint16_t *ucs;
  int len;

  assert(pmath_is_null(str) || pmath_is_string(str));

  if(inslen < 0)
    inslen = strlen(ins);

  if(inslen == 0) {
    if(pmath_is_null(str))
      return pmath_string_new(0);
    return str;
  }

  if(pmath_is_null(str) || pmath_is_str0(str))
    len = 0;
  else if(pmath_is_str1(str))
    len = 1;
  else if(pmath_is_str2(str))
    len = 2;
  else
    len = ((struct _pmath_string_t *)PMATH_AS_PTR(str))->length;

  if(inspos < 0)
    inspos = 0;
  else if(inspos > len)
    inspos = len;

  result = enlarge_string_2(str, inspos, inslen);
  if(!result)
    return PMATH_NULL;

  ucs = AFTER_STRING(result) + inspos;
  while(inslen-- > 0)
    *ucs++ = (uint16_t)(unsigned char) * ins++;

  return _pmath_from_buffer(result);
}

PMATH_API
pmath_string_t pmath_string_from_utf8(
    const char    *str,
    int            len
) {
  struct _pmath_string_t *result;
  size_t inbytesleft;
  size_t outbytesleft;
  char *inbuf;
  char *outbuf;

  if(len < 0)
    len = strlen(str);

  result = _pmath_new_string_buffer((3 * len) / 2);
  if(!result)
    return PMATH_NULL;

  inbytesleft = (size_t)len;
  outbytesleft = sizeof(uint16_t) * result->length;
  inbuf  = (char *)str;
  outbuf = (char *)AFTER_STRING(result);

  while(inbytesleft > 0) {
    size_t ret = iconv(from_utf8, &inbuf, &inbytesleft, &outbuf, &outbytesleft);

    if(ret == (size_t) - 1) {
      if(errno == E2BIG) { // output buffer too small
        size_t bytes_written = (size_t)outbuf - (size_t)AFTER_STRING(result);
        result->length = (int)(bytes_written / sizeof(uint16_t));

        result = enlarge_string(result, result->length, (3 * inbytesleft) / 2 + 1);

        if(!result)
          return PMATH_NULL;

        outbuf = ((char *)AFTER_STRING(result)) + bytes_written;
        outbytesleft = sizeof(uint16_t) * result->length - bytes_written;
      }
      else if(errno == EILSEQ) { // invalid input byte
        ++inbuf;
        --inbytesleft;
      }
      else if(errno == EINVAL) { // incomplete input
        result->length = ((size_t)outbuf - (size_t)AFTER_STRING(result)) / 2;
        return _pmath_from_buffer(result);
      }
    }
  }

  result->length = ((size_t)outbuf - (size_t)AFTER_STRING(result)) / 2;
  return _pmath_from_buffer(result);
}

PMATH_API
PMATH_ATTRIBUTE_USE_RESULT
char *pmath_string_to_utf8(
    pmath_string_t  str,
    int            *result_len
) {
  const uint16_t *buf = pmath_string_buffer(&str);
  int             len = pmath_string_length(str);
  size_t size         = 4 * ((size_t)len) + 1; // worst case: every character is 4 bytes in utf8
  char *res           = pmath_mem_alloc(size);
  char *inbuf         = (char *)buf;
  char *outbuf        = res;
  size_t inbytesleft  = sizeof(uint16_t) * (size_t)len;
  size_t outbytesleft = size - 1;

  if(!res) {
    if(result_len)
      *result_len = 0;
    return NULL;
  }

  while(inbytesleft > 0) {
    size_t ret = iconv(to_utf8, &inbuf, &inbytesleft, &outbuf, &outbytesleft);

    if(ret == (size_t)(-1)) {
      pmath_mem_free(res);
      if(result_len)
        *result_len = 0;
      return NULL;
    }
  }

  *outbuf = '\0';
  if(result_len)
    *result_len = (int)((size_t)outbuf - (size_t)res);
  return res;
}

static void write_with_nulls(pmath_cstr_writer_info_t *info, const char *str, const char *end) {
  int len = (int)(end - str);

  while(len > 0) {
    int sublen = strlen(str) + 1;

    info->_pmath_write_cstr(info->user, str);

    str += sublen;
    len -= sublen;

    if(len > 0) {
      info->_pmath_write_cstr(info->user, "\\[U+0000]");
    }
  }
}

PMATH_API
void pmath_utf8_writer(void *user, const uint16_t *data, int len) {
  char *inbuf  = (char *)data;
  size_t inbytesleft = sizeof(uint16_t) * (size_t)len;
  char buf[100];
  char *outbuf = buf;
  size_t outbytesleft = sizeof(buf) - 1;

  while(inbytesleft > 0) {
    size_t ret = iconv(to_utf8, &inbuf, &inbytesleft, &outbuf, &outbytesleft);

    if(ret == (size_t) - 1) {
      if(errno == E2BIG) { // output buffer too small
        *outbuf = '\0';

        write_with_nulls(user, buf, outbuf);

        outbuf = buf;
        outbytesleft = sizeof(buf) - 1;
      }
      else { // invalid input byte  or  incomplete input
        ++inbuf;
        --inbytesleft;
      }
    }
  }

  *outbuf = '\0';
  write_with_nulls(user, buf, outbuf);
}

PMATH_API
void pmath_native_writer(void *user, const uint16_t *data, int len) {
  pmath_cstr_writer_info_t *info = user;
  char *inbuf  = (char *)data;
  size_t inbytesleft = sizeof(uint16_t) * (size_t)len;
  char buf[100];
  char *outbuf = buf;
  size_t outbytesleft = sizeof(buf) - 1;

  while(inbytesleft > 0) {
    size_t ret = iconv(to_native, &inbuf, &inbytesleft, &outbuf, &outbytesleft);

    if(ret == (size_t) - 1) {
//      if(errno == E2BIG){ // output buffer too small
      *outbuf = '\0';

      write_with_nulls(info, buf, outbuf);

      outbuf = buf;
      outbytesleft = sizeof(buf) - 1;
//      }

      if(errno == EILSEQ && ((size_t)inbuf & 1) == 0 && inbytesleft >= 2) { // 2-byte aligned
        const char *name;
        uint16_t *u16 = (void *)inbuf;
        uint32_t ch;

        if( (u16[0] & 0xFC00) == 0xD800 &&
            inbytesleft >= 4 &&
            (u16[1] & 0xFC00) == 0xDC00)
        {
          ch = 0x10000 + ((uint32_t)(u16[0] & 0x3FF) << 10) + (u16[1] & 0x3FF);

          inbuf += 4;
          inbytesleft -= 4;
        }
        else {
          ch = u16[0];

          inbuf += 2;
          inbytesleft -= 2;
        }

        switch(ch) {
          case PMATH_CHAR_ASSIGN:        info->_pmath_write_cstr(info->user, ":="); break;
          case PMATH_CHAR_ASSIGNDELAYED: info->_pmath_write_cstr(info->user, "::="); break;
          case PMATH_CHAR_RULE:          info->_pmath_write_cstr(info->user, "->"); break;
          case PMATH_CHAR_RULEDELAYED:   info->_pmath_write_cstr(info->user, ":>"); break;

          default:
            name = pmath_char_to_name(ch);
            if(name) {
              info->_pmath_write_cstr(info->user, "\\[");
              info->_pmath_write_cstr(info->user, name);
              info->_pmath_write_cstr(info->user, "]");
            }
        }
      }
      else if(errno != E2BIG) { // invalid input byte  or  incomplete input
        ++inbuf;
        --inbytesleft;
      }
    }
  }

  *outbuf = '\0';
  write_with_nulls(info, buf, outbuf);
}

PMATH_API
pmath_string_t pmath_string_insert_codepage(
    pmath_string_t str,
    int            inspos,
    const char    *ins,
    int            inslen,
    const uint16_t *cp
) {
  struct _pmath_string_t *result;
  uint16_t *ucs;
  int len;

  if(inslen < 0)
    inslen = strlen(ins);

  if(inslen == 0) {
    if(pmath_is_null(str))
      return pmath_string_new(0);
    return str;
  }

  if(pmath_is_null(str) || pmath_is_str0(str))
    len = 0;
  else if(pmath_is_str1(str))
    len = 1;
  else if(pmath_is_str2(str))
    len = 2;
  else
    len = ((struct _pmath_string_t *)PMATH_AS_PTR(str))->length;

  if(inspos < 0)
    inspos = 0;
  else if(inspos > len)
    inspos = len;

  result = enlarge_string_2(str, inspos, inslen);
  if(!result)
    return PMATH_NULL;

  ucs = AFTER_STRING(result) + inspos;
  while(inslen-- > 0)
    *ucs++ = cp[(unsigned char) * ins++];

  return _pmath_from_buffer(result);
}

PMATH_API
pmath_string_t pmath_string_insert_ucs2(
    pmath_string_t  str,
    int             inspos,
    const uint16_t *ins,
    int             inslen
) {
  struct _pmath_string_t *result;
  uint16_t *ucs;
  int len;

  if(inslen < 0) {
    register const uint16_t *tmp = ins;
    inslen = 0;
    while(*tmp++)
      ++inslen;
  }

  if(inslen == 0) {
    if(pmath_is_null(str))
      return pmath_string_new(0);
    return str;
  }

  if(pmath_is_null(str) || pmath_is_str0(str))
    len = 0;
  else if(pmath_is_str1(str))
    len = 1;
  else if(pmath_is_str2(str))
    len = 2;
  else
    len = ((struct _pmath_string_t *)PMATH_AS_PTR(str))->length;

  if(inspos < 0)
    inspos = 0;
  else if(inspos > len)
    inspos = len;

  result = enlarge_string_2(str, inspos, inslen);
  if(!result)
    return PMATH_NULL;

  ucs = AFTER_STRING(result) + inspos;
  while(inslen-- > 0)
    *ucs++ = *ins++;

  return _pmath_from_buffer(result);
}

PMATH_API
pmath_string_t pmath_string_insert(
    pmath_string_t str,
    int            inspos,
    pmath_string_t ins
) {
  pmath_string_t result;
  struct _pmath_string_t *_str;
  struct _pmath_string_t *_ins;

  if(pmath_is_ministr(ins)) {
    int len = 0;
    if(pmath_is_str1(ins))
      len = 1;
    if(pmath_is_str2(ins))
      len = 2;

    return pmath_string_insert_ucs2(str, inspos, ins.s.u.as_chars, len);
  }

  _ins = (struct _pmath_string_t *)PMATH_AS_PTR(ins);

  if(pmath_is_str0(str)) {
    _str = NULL;
  }
  else if(pmath_is_str1(str)) {
    _str = _pmath_new_string_buffer(1);

    if(_str == NULL) {
      pmath_unref(ins);
      return PMATH_NULL;
    }

    AFTER_STRING(_str)[0] = str.s.u.as_chars[0];
    str = PMATH_FROM_PTR(_str);
  }
  else if(pmath_is_str2(str)) {
    _str = _pmath_new_string_buffer(2);

    if(_str == NULL) {
      pmath_unref(ins);
      return PMATH_NULL;
    }

    AFTER_STRING(_str)[0] = str.s.u.as_chars[0];
    AFTER_STRING(_str)[1] = str.s.u.as_chars[1];
    str = PMATH_FROM_PTR(_str);
  }
  else {
    _str = (struct _pmath_string_t *)PMATH_AS_PTR(str);
  }

  if( _str && _ins                 &&
      _str->buffer                 &&
      _str->buffer == _ins->buffer &&
      _str->length == inspos       &&
      _str->capacity_or_start + inspos == _ins->capacity_or_start)
  {
    if(pmath_atomic_read_aquire(&_str->inherited.refcount) == 1) {
      _str->length += _ins->length;
      pmath_unref(ins);
      return str;
    }

    if(pmath_atomic_read_aquire(&_ins->inherited.refcount) == 1) {
      _ins->length +=            _str->length;
      _ins->capacity_or_start = _str->capacity_or_start;
      pmath_unref(str);
      return ins;
    }

    result = pmath_ref(PMATH_FROM_PTR(_str->buffer));
    result = pmath_string_part(
        result,
        _str->capacity_or_start,
        _str->length + _ins->length);
    pmath_unref(str);
    pmath_unref(ins);
    return result;
  }

  result = pmath_string_insert_ucs2(
      str,
      inspos,
      pmath_string_buffer(&ins),
      pmath_string_length(ins));

  pmath_unref(ins);
  return result;
}

PMATH_API
pmath_string_t pmath_string_concat(
    pmath_string_t prefix,
    pmath_string_t postfix
) {
  if(pmath_is_null(prefix))
    return postfix;

  if(pmath_is_null(postfix))
    return prefix;

  return pmath_string_insert(prefix, pmath_string_length(prefix), postfix);
}

PMATH_API
pmath_string_t pmath_string_part(
    pmath_string_t string,
    int            start,
    int            length
) {
  struct _pmath_string_t *_str;

  if(pmath_is_null(string))
    return PMATH_NULL;

  if(pmath_is_str0(string))
    return string;

  if(length == 0) {
    pmath_unref(string);

    string.s.tag = PMATH_TAG_STR0;
    string.s.u.as_chars[0] = 0;
    string.s.u.as_chars[1] = 0;
    return string;
  }

  if(pmath_is_str1(string)) {
    if(start == 0)
      return string;

    string.s.tag = PMATH_TAG_STR0;
    string.s.u.as_chars[0] = 0;
    return string;
  }

  if(pmath_is_str2(string)) {
    if(start == 0) {
      if(length == 1) {
        string.s.tag = PMATH_TAG_STR1;
        string.s.u.as_chars[1] = 0;
      }

      return string;
    }

    if(start == 1) {
      string.s.tag = PMATH_TAG_STR1;
      string.s.u.as_chars[0] = string.s.u.as_chars[1];
      string.s.u.as_chars[1] = 0;
      return string;
    }

    string.s.tag = PMATH_TAG_STR0;
    string.s.u.as_chars[0] = 0;
    string.s.u.as_chars[1] = 0;
    return string;
  }

  _str = (struct _pmath_string_t *)PMATH_AS_PTR(string);

  if(start < 0 || start >= _str->length) {
    pmath_unref(string);

    string.s.tag = PMATH_TAG_STR0;
    string.s.u.as_chars[0] = 0;
    string.s.u.as_chars[1] = 0;
    return string;
  }

  if( length < 0                    ||
      start + length > _str->length ||
      length         > _str->length)
  {
    length = _str->length - start;
  }

  if(length <= 0) {
    pmath_unref(string);

    string.s.tag = PMATH_TAG_STR0;
    string.s.u.as_chars[0] = 0;
    string.s.u.as_chars[1] = 0;
    return string;
  }

  if(length == _str->length)
    return string;

  if(length <= 2) {
    pmath_t result;
    const uint16_t *buffer = start + (_str->buffer ? AFTER_STRING(_str->buffer) + _str->capacity_or_start : AFTER_STRING(_str));

    if(length == 1) {
      result.s.tag = PMATH_TAG_STR1;
      result.s.u.as_chars[0] = buffer[0];
      result.s.u.as_chars[1] = 0;
      pmath_unref(string);
      return result;
    }

    result.s.tag = PMATH_TAG_STR2;
    result.s.u.as_chars[0] = buffer[0];
    result.s.u.as_chars[1] = buffer[1];
    pmath_unref(string);
    return result;
  }

  if(pmath_atomic_read_aquire(&_str->inherited.refcount) == 1) {
    if(_str->buffer) {
      _str->capacity_or_start += start;
      _str->length            = length;
      return string;
    }

    if(start == 0) {
      _str->length = length;
      return string;
    }
  }

  if(_str->buffer) {
    pmath_t tmp = string;

    start += _str->capacity_or_start;
    string = pmath_ref(PMATH_FROM_PTR(_str->buffer));
    _str = (struct _pmath_string_t *)PMATH_AS_PTR(string);
    pmath_unref(tmp);

    assert(pmath_is_string(string));

    if(start == 0 && pmath_refcount(string) == 1) {
      _str->length = length;
      return string;
    }
  }

  {
    struct _pmath_string_t *result = (void *)PMATH_AS_PTR(_pmath_create_stub(
        PMATH_TYPE_SHIFT_BIGSTRING,
        sizeof(struct _pmath_string_t)));

    if(!result) {
      pmath_unref(string);
      return PMATH_NULL;
    }
    
    result->debug_info        = NULL;
    result->buffer            = _str;
    result->length            = length;
    result->capacity_or_start = start;

    return PMATH_FROM_PTR(result); /* already know length > 2 */
  }
}

PMATH_API
const uint16_t *pmath_string_buffer(const pmath_string_t *string) {
  const struct _pmath_string_t *_str;

  if(pmath_is_null(*string))
    return NULL;

  if(pmath_is_ministr(*string))
    return &string->s.u.as_chars[0];

  assert(pmath_is_string(*string));
  _str = (void *)PMATH_AS_PTR(*string);

  if(_str->buffer == NULL)
    return AFTER_STRING(_str);

  return AFTER_STRING(_str->buffer) + _str->capacity_or_start;
}

PMATH_API
int pmath_string_length(pmath_string_t string) {
  struct _pmath_string_t *_str;

  if(pmath_is_null(string) || pmath_is_str0(string))
    return 0;

  if(pmath_is_str1(string))
    return 1;

  if(pmath_is_str2(string))
    return 2;

  assert(pmath_is_bigstr(string));

  _str = (void *)PMATH_AS_PTR(string);

  assert(_str->length >= 0);
  return _str->length;
}

PMATH_API
PMATH_ATTRIBUTE_PURE
pmath_bool_t pmath_string_equals_latin1(
    pmath_string_t  string,
    const char     *latin1
) {
  const uint16_t *buf;
  int i, len;

  buf = pmath_string_buffer(&string);
  len = pmath_string_length(string);
  for(i = 0; i < len; ++i)
    if(latin1[i] == '\0' || buf[i] != latin1[i])
      return FALSE;

  return latin1[len] == '\0';
}

/*============================================================================*/

PMATH_API
pmath_string_t pmath_string_from_native(
    const char  *str,
    int          len
) {
  struct _pmath_string_t *result;
  size_t inbytesleft;
  size_t outbytesleft;
  char *inbuf;
  char *outbuf;

  if(!str)
    return PMATH_NULL;

  if(len < 0)
    len = strlen(str);

  result = _pmath_new_string_buffer(len);

  inbytesleft = (size_t)len;
  outbytesleft = sizeof(uint16_t) * (size_t)len;
  inbuf  = (char *)str;
  outbuf = (char *)AFTER_STRING(result);

  if(!result)
    return PMATH_NULL;

  while(inbytesleft > 0) {
    size_t ret = iconv(from_native, &inbuf, &inbytesleft, &outbuf, &outbytesleft);

    if(ret == (size_t)(-1)) {
      pmath_unref(PMATH_FROM_PTR(result));
      return PMATH_NULL;
    }
  }

  result->length = ((size_t)outbuf - (size_t)AFTER_STRING(result)) / 2;
  return _pmath_from_buffer(result);
}

PMATH_API
char *pmath_string_to_native(pmath_string_t str, int *result_len) {
  int len = pmath_string_length(str);
  const uint16_t *buf = pmath_string_buffer(&str);

  size_t s_size = 4 * ((size_t)len) + 1;
  char *s = (char *)pmath_mem_alloc(s_size);

  size_t inbytesleft = sizeof(uint16_t) * (size_t)len;
  size_t outbytesleft = s_size - 1;
  char *inbuf  = (char *)buf;
  char *outbuf = s;

  if(!s) {
    if(result_len)
      *result_len = 0;
    return NULL;
  }

  while(inbytesleft > 0) {
    size_t ret = iconv(to_native, &inbuf, &inbytesleft, &outbuf, &outbytesleft);

    if(ret == (size_t)(-1)) {
      pmath_mem_free(s);
      if(result_len)
        *result_len = 0;
      return NULL;
    }
  }

  *outbuf = '\0';
  if(result_len)
    *result_len = (int)((size_t)outbuf - (size_t)s);
  return s;
}

/*============================================================================*/

PMATH_PRIVATE
pmath_bool_t _pmath_strings_init(void) {
  _pmath_init_special_type(
      PMATH_TYPE_SHIFT_BIGSTRING,
      _pmath_strings_compare,
      hash_string,
      destroy_string,
      _pmath_strings_equal,
      _pmath_string_write);

  to_utf8 = iconv_open(
      "UTF-8",
      PMATH_BYTE_ORDER < 0 ? "UTF-16LE" : "UTF-16BE");

  if(to_utf8 == (iconv_t) - 1)
    return FALSE;

  from_utf8 = iconv_open(
      PMATH_BYTE_ORDER < 0 ? "UTF-16LE" : "UTF-16BE",
      "UTF-8");

  if(from_utf8 == (iconv_t) - 1) {
    iconv_close(to_utf8);
    return FALSE;
  }

  _init_pmath_native_encoding();

  to_native = iconv_open(
      _pmath_native_encoding,
      PMATH_BYTE_ORDER < 0 ? "UTF-16LE" : "UTF-16BE");

  if(to_native == (iconv_t) - 1) {
    iconv_close(to_utf8);
    iconv_close(from_utf8);
    return FALSE;
  }

  from_native = iconv_open(
      PMATH_BYTE_ORDER < 0 ? "UTF-16LE" : "UTF-16BE",
      _pmath_native_encoding);

  if(from_native == (iconv_t) - 1) {
    iconv_close(to_utf8);
    iconv_close(from_utf8);
    iconv_close(to_native);
    return FALSE;
  }

  return TRUE;
}

PMATH_PRIVATE
void _pmath_strings_done(void) {
  iconv_close(to_utf8);
  iconv_close(from_utf8);
  iconv_close(to_native);
  iconv_close(from_native);
}
