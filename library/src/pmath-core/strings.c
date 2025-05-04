#define ICONV_CONST

#include <pmath-core/objects-private.h>
#include <pmath-core/strings-private.h>

#include <pmath-language/charnames.h>
#include <pmath-language/scanner.h>
#include <pmath-language/tokens.h>

#include <pmath-util/concurrency/threads-private.h>
#include <pmath-util/debug.h>
#include <pmath-util/hash/incremental-hash-private.h>
#include <pmath-util/helpers.h>
#include <pmath-util/memory.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/formating-private.h>
#include <pmath-builtins/io-private.h>

#include <errno.h>
#include <iconv.h>
#include <limits.h>
#include <string.h>

#ifndef iconv_errno
#  define iconv_errno  errno
#endif


extern pmath_symbol_t pmath_System_False;
extern pmath_symbol_t pmath_System_InterpretationBox;
extern pmath_symbol_t pmath_System_List;
extern pmath_symbol_t pmath_System_OverscriptBox;
extern pmath_symbol_t pmath_System_ShowStringCharacters;
extern pmath_symbol_t pmath_System_StringBox;
extern pmath_symbol_t pmath_System_SubscriptBox;
extern pmath_symbol_t pmath_System_SubsuperscriptBox;
extern pmath_symbol_t pmath_System_SuperscriptBox;
extern pmath_symbol_t pmath_System_StyleBox;
extern pmath_symbol_t pmath_System_TagBox;
extern pmath_symbol_t pmath_System_TooltipBox;
extern pmath_symbol_t pmath_System_UnderoverscriptBox;
extern pmath_symbol_t pmath_System_UnderscriptBox;

static iconv_t create_to_utf8(void);
static iconv_t create_from_utf8(void);
static iconv_t create_to_native(void);
static iconv_t create_from_native(void);


static pmath_string_t string_from_iconv(iconv_t cd, const char *str, int len);
static char *string_to_iconv(iconv_t cd, pmath_string_t str, int *result_len);

static uint16_t unicode_subscript(uint16_t ch);
static uint16_t unicode_superscript(uint16_t ch);

static pmath_bool_t write_boxes_try_unicode_subsuperscript(struct pmath_write_ex_t *info, pmath_t box, uint16_t (*char_sel)(uint16_t));

#define _pmath_ref_string_ptr(P)    _pmath_ref_ptr(&(P)->inherited)
#define _pmath_unref_string_ptr(P)  _pmath_unref_ptr(&(P)->inherited)


#if PMATH_BITSIZE >= 64
#  define USE_CACHE_FOR_STRING_HASH  1
#else
#  define USE_CACHE_FOR_STRING_HASH  0
#endif

#if USE_CACHE_FOR_STRING_HASH
#  define RESET_CACHED_HASH(P)  pmath_atomic_write_uint32_release(&(P)->inherited.padding_flags32, 0)
#else
#  define RESET_CACHED_HASH(P)  ((void)0)
#endif // USE_CACHE_FOR_STRING_HASH


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

  result->debug_metadata    = NULL;
  result->buffer            = NULL;
  result->length            = len;
  result->capacity_or_start = size;

  return result;
}

PMATH_PRIVATE
pmath_t _pmath_from_buffer(struct _pmath_string_t *b) {
  if(b && b->length <= 2 && b->debug_metadata == NULL) {
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

PMATH_API
pmath_bool_t pmath_string_begin_write(pmath_string_t *str, uint16_t **buffer, int *length) {
  struct _pmath_string_t *_str;

  assert(str != NULL);
  assert(buffer != NULL);

  *buffer = NULL;
  if(length)
    *length = pmath_string_length(*str);

  if(pmath_is_null(*str))
    return FALSE;

  if(pmath_is_ministr(*str)) {
    *buffer = &str->s.u.as_chars[0];
    return TRUE;
  }

  assert(pmath_is_bigstr(*str));

  _str = (struct _pmath_string_t *)PMATH_AS_PTR(*str);
  if(pmath_refcount(*str) != 1 || _str->buffer != NULL) {
    pmath_string_t new_str = pmath_string_insert_ucs2(PMATH_NULL, 0, pmath_string_buffer(str), _str->length);
    if(pmath_is_null(new_str))
      return FALSE;

    pmath_unref(*str);
    *str = new_str;
    if(pmath_is_ministr(*str)) {
      *buffer = &str->s.u.as_chars[0];
      return TRUE;
    }
    _str = (struct _pmath_string_t *)PMATH_AS_PTR(*str);
  }

  assert(_str->buffer == NULL);

  if(_str->debug_metadata) {
    _pmath_unref_ptr(_str->debug_metadata);
    _str->debug_metadata = NULL;
  }

  *buffer = AFTER_STRING(_str);
  _str->inherited.type_shift = PMATH_TYPE_SHIFT_PINNED_STRING;
  return TRUE;
}

PMATH_API
void pmath_string_end_write(pmath_string_t *str, uint16_t **buffer) {
  struct _pmath_string_t *_str;

  assert(str != NULL);
  assert(buffer != NULL);
  assert(*buffer != NULL);

  if(pmath_is_null(*str))
    return;

  if(pmath_is_ministr(*str)) {
    assert(*buffer == &str->s.u.as_chars[0]);
    *buffer = NULL;
    return;
  }

  assert(pmath_is_pointer(*str));
  assert(pmath_refcount(*str) == 1);

  _str = (struct _pmath_string_t *)PMATH_AS_PTR(*str);
  assert(_str->inherited.type_shift == PMATH_TYPE_SHIFT_PINNED_STRING);
  assert(_str->buffer == NULL);
  assert(*buffer == AFTER_STRING(_str));
  *buffer = NULL;
  RESET_CACHED_HASH(_str);
  _str->inherited.type_shift = PMATH_TYPE_SHIFT_BIGSTRING;
}


PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
pmath_t _pmath_string_get_debug_metadata(pmath_t str) {
  struct _pmath_string_t *_str;

  if(pmath_is_null(str))
    return PMATH_NULL;

  assert(pmath_is_string(str));
  if(!pmath_is_pointer(str))
    return PMATH_NULL;

  _str = (struct _pmath_string_t *)PMATH_AS_PTR(str);
  return pmath_ref(PMATH_FROM_PTR(_str->debug_metadata));
}

PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
pmath_t _pmath_string_set_debug_metadata(pmath_t str, pmath_t info) {
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
        AFTER_STRING(result)[1] = str.s.u.as_chars[1];
      }
    }
    if(!result) {
      pmath_unref(info);
      return str;
    }

    result->debug_metadata = info_ptr;

    // no need to free str: it is a ministr
    return PMATH_FROM_PTR(result);
  }

  assert(pmath_is_bigstr(str));

  str_ptr = (void*)PMATH_AS_PTR(str);
  if(str_ptr->debug_metadata == info_ptr) {
    if(info_ptr)
      _pmath_unref_ptr(info_ptr);

    return str;
  }

  if(pmath_refcount(str) == 1) {
    if(str_ptr->debug_metadata)
      _pmath_unref_ptr(str_ptr->debug_metadata);

    str_ptr->debug_metadata = info_ptr;
    return str;
  }
  else {
    if( str_ptr->buffer &&
        str_ptr->buffer->debug_metadata == info_ptr &&
        str_ptr->capacity_or_start == 0 &&
        str_ptr->length == str_ptr->buffer->length)
    {
      result = str_ptr->buffer;
      _pmath_ref_string_ptr(result);
      _pmath_unref_string_ptr(str_ptr);
      pmath_unref(info);
      return PMATH_FROM_PTR(result);
    }

    result = (void *)PMATH_AS_PTR(_pmath_create_stub(PMATH_TYPE_SHIFT_BIGSTRING, sizeof(struct _pmath_string_t)));
    if(!result) {
      pmath_unref(info);
      return str;
    }

    result->debug_metadata = info_ptr;
    result->length     = str_ptr->length;
    if(str_ptr->buffer) {
      _pmath_ref_string_ptr(str_ptr->buffer);
      result->buffer            = str_ptr->buffer;
      result->capacity_or_start = str_ptr->capacity_or_start;
      _pmath_unref_string_ptr(str_ptr);
    }
    else{
      result->buffer            = str_ptr;
      result->capacity_or_start = 0;
    }

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

    RESET_CACHED_HASH(string);
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
  
  PMATH_OBJECT_MARK_DELETION_TRAP(&str->inherited);
  
  if(str->debug_metadata)
    _pmath_unref_ptr(str->debug_metadata);
  if(str->buffer)
    _pmath_unref_ptr((void*)str->buffer);

  pmath_mem_free(str);
}

static void destroy_pinned_string(pmath_t p) {
  pmath_debug_print("[WARNING: destroy pinned string...]\n");
  destroy_string(p);
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

static int compare_pinned_strings(
    pmath_t strA,
    pmath_t strB
) {
  pmath_debug_print("[ERROR: compare pinned strings...]\n");
  if(strA.as_bits < strB.as_bits)
    return -1;
  if(strA.as_bits > strB.as_bits)
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

    return _pmath_incremental_hash(&tmp, sizeof(pmath_t), 0);
  }
  
#if USE_CACHE_FOR_STRING_HASH
  assert(pmath_is_bigstr(str) && "Strings with lengths > 2 are bigstr");
  struct _pmath_string_t *str_ptr = (void*)PMATH_AS_PTR(str);
  
  uint32_t cached_hash = pmath_atomic_read_uint32_aquire(&str_ptr->inherited.padding_flags32);
#ifdef NDEBUG
  if(cached_hash != 0)
    return cached_hash;
#endif
#endif // USE_CACHE_FOR_STRING_HASH

  uint32_t calculated_hash = _pmath_incremental_hash(buf, (size_t)len * sizeof(uint16_t), 0);

#if USE_CACHE_FOR_STRING_HASH
  assert(calculated_hash == cached_hash || cached_hash == 0);
  pmath_atomic_write_uint32_release(&str_ptr->inherited.padding_flags32, calculated_hash);
#endif // USE_CACHE_FOR_STRING_HASH

  return calculated_hash;
  //return _pmath_incremental_hash(buf, (size_t)len * sizeof(uint16_t), 0);
}

static unsigned int hash_pinned_string(pmath_t str) {
  pmath_debug_print("[ERROR: hash pinned string...]\n");
  return 0;
}

PMATH_PRIVATE
void _pmath_write_cstr(
    const char          *str,
    void (*write_ucs2)(void *, const uint16_t *, int),
    void                *user
) {
  _pmath_write_latin1(str, -1, write_ucs2, user);
}


PMATH_PRIVATE
void _pmath_write_latin1(
  const char  *str,
  int          len,
  void (*write_ucs2)(void*, const uint16_t*, int),
  void        *user
) {
  if(len < 0)
    len = strlen(str);
  
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


static pmath_bool_t find_single_token(pmath_t box, pmath_string_t *result) {
  if(pmath_is_string(box)) {
    if(result) *result = pmath_ref(box);
    return TRUE;
  }
  
  if( pmath_is_expr_of(box, PMATH_NULL) ||
      pmath_is_expr_of(box, pmath_System_List))
  {
    pmath_t part;
    pmath_bool_t found;

    if(pmath_expr_length(box) != 1)
      return FALSE;

    part = pmath_expr_get_item(box, 1);
    found = find_single_token(part, result);
    pmath_unref(part);
    return found;
  }
  
  if( pmath_is_expr_of(box, pmath_System_StyleBox) ||
      pmath_is_expr_of(box, pmath_System_TagBox) ||
      pmath_is_expr_of(box, pmath_System_InterpretationBox))
  {
    pmath_t part;
    pmath_bool_t found;

    part = pmath_expr_get_item(box, 1);
    found = find_single_token(part, result);
    pmath_unref(part);
    return found;
  }

  return FALSE;
}

static pmath_bool_t is_single_token(pmath_t box) {
  return find_single_token(box, NULL);
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

static pmath_bool_t call_old_custom_writer(void *user, pmath_t obj, struct pmath_write_ex_t *info) {
  struct pmath_write_ex_t *old_info = user;

  return old_info->custom_writer(old_info->user, obj, info);
}

static void write_boxes_impl(struct pmath_write_ex_t *info, pmath_t box) {
  if(pmath_is_string(box)) {
    info->write(info->user, pmath_string_buffer(&box), pmath_string_length(box));
    return;
  }

  if( pmath_is_expr_of(box, pmath_System_StringBox) ||
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

  if(pmath_is_expr_of(box, pmath_System_List)) {
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
          if( pmath_is_expr_of(next, pmath_System_SubscriptBox)     ||
              pmath_is_expr_of(next, pmath_System_SuperscriptBox)   ||
              pmath_is_expr_of(next, pmath_System_SubsuperscriptBox))
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

  if(pmath_is_expr_of(box, pmath_System_StyleBox)) {
    pmath_bool_t hide_string_characters = FALSE;
    pmath_t part;

    size_t i;
    for(i = pmath_expr_length(box); i > 1; --i) {
      pmath_t option = pmath_expr_get_item(box, i);

      if(pmath_is_rule(option)) {
        pmath_t lhs = pmath_expr_get_item(option, 1);
        pmath_t rhs = pmath_expr_get_item(option, 2);
        pmath_unref(lhs);
        pmath_unref(rhs);

        if(pmath_same(lhs, pmath_System_ShowStringCharacters))
          hide_string_characters = pmath_same(rhs, pmath_System_False);
      }

      pmath_unref(option);
    }

    if(hide_string_characters) {
      struct pmath_write_ex_t info2;
      memset(&info2, 0, sizeof(info2));
      info2.size          = sizeof(info2);
      info2.options       = info->options;
      info2.user          = info;
      info2.write         = write_and_skip_string_chars;
      info2.pre_write     = info->pre_write     ? call_old_pre_write     : NULL;
      info2.post_write    = info->post_write    ? call_old_post_write    : NULL;
      info2.custom_writer = info->custom_writer ? call_old_custom_writer : NULL;

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

  if( pmath_is_expr_of(box, pmath_System_TagBox)       ||
      pmath_is_expr_of(box, pmath_System_TooltipBox)   ||
      pmath_is_expr_of(box, pmath_System_InterpretationBox))
  {
    pmath_t part = pmath_expr_get_item(box, 1);
    _pmath_write_boxes(info, part);
    pmath_unref(part);

    return;
  }

  if(pmath_is_expr_of(box, pmath_System_SubscriptBox)) {
    pmath_t part = pmath_expr_get_item(box, 1);

    if(!write_boxes_try_unicode_subsuperscript(info, part, unicode_subscript)) {
      _pmath_write_cstr("_", info->write, info->user);
      write_single_token_box(info, part);
    }
    pmath_unref(part);

    return;
  }

  if(pmath_is_expr_of(box, pmath_System_SuperscriptBox)) {
    pmath_t part = pmath_expr_get_item(box, 1);
    
    if(!write_boxes_try_unicode_subsuperscript(info, part, unicode_superscript)) {
      _pmath_write_cstr("^", info->write, info->user);
      write_single_token_box(info, part);
    }
    pmath_unref(part);

    return;
  }

  if(pmath_is_expr_of(box, pmath_System_SubsuperscriptBox)) {
    pmath_t part = pmath_expr_get_item(box, 1);
    
    if(!write_boxes_try_unicode_subsuperscript(info, part, unicode_subscript)) {
      _pmath_write_cstr("_", info->write, info->user);
      write_single_token_box(info, part);
    }
    pmath_unref(part);

    part = pmath_expr_get_item(box, 2);
    if(!write_boxes_try_unicode_subsuperscript(info, part, unicode_superscript)) {
      _pmath_write_cstr("^", info->write, info->user);
      write_single_token_box(info, part);
    }
    pmath_unref(part);

    return;
  }

  if(pmath_is_expr_of(box, pmath_System_UnderscriptBox)) {
    pmath_t part = pmath_expr_get_item(box, 1);
    _pmath_write_boxes(info, part);
    pmath_unref(part);

    part = pmath_expr_get_item(box, 2);
    _pmath_write_cstr("_", info->write, info->user);
    write_single_token_box(info, part);
    pmath_unref(part);

    return;
  }

  if(pmath_is_expr_of(box, pmath_System_OverscriptBox)) {
    pmath_t part = pmath_expr_get_item(box, 1);
    _pmath_write_boxes(info, part);
    pmath_unref(part);

    part = pmath_expr_get_item(box, 2);
    _pmath_write_cstr("^", info->write, info->user);
    write_single_token_box(info, part);
    pmath_unref(part);

    return;
  }

  if(pmath_is_expr_of(box, pmath_System_UnderoverscriptBox)) {
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
    _pmath_write_impl(info, part);
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

  _pmath_write_impl(info, box);
}

PMATH_PRIVATE
void _pmath_write_boxes(struct pmath_write_ex_t *info, pmath_t box) {
  if(info->pre_write)
    info->pre_write(info->user, box, info->options);
  
  if(info->custom_writer) {
    if(info->custom_writer(info->user, box, info)) {
      if(info->post_write)
        info->post_write(info->user, box, info->options);
      
      return;
    }
  }
  
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
  static const char hex_digits[16] = "0123456789ABCDEF";

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
      _pmath_write_to_string,
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

PMATH_API
pmath_string_t pmath_string_new_raw(int length) {
  pmath_string_t str = PMATH_NULL;

  assert(length >= 0);

  switch(length) {
    case 0:
      str.s.tag = PMATH_TAG_STR0;
      str.s.u.as_int32 = 0;
      return str;
    case 1:
      str.s.tag = PMATH_TAG_STR1;
      str.s.u.as_int32 = 0;
      return str;
    case 2:
      str.s.tag = PMATH_TAG_STR2;
      str.s.u.as_int32 = 0;
      return str;
  }

  return PMATH_FROM_PTR(_pmath_new_string_buffer(length));
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
  pmath_string_t result;
  iconv_t from_utf8 = create_from_utf8();
  if(from_utf8 == (iconv_t)(-1))
    return PMATH_NULL;
  
  result = string_from_iconv(from_utf8, str, len);
  iconv_close(from_utf8);
  return result;
}

PMATH_API
PMATH_ATTRIBUTE_USE_RESULT
char *pmath_string_to_utf8(pmath_string_t str, int *result_len) {
  char *result;
  iconv_t to_utf8 = create_to_utf8();
  if(to_utf8 == (iconv_t)(-1)) {
    if(result_len)
      *result_len = 0;
    return NULL;
  }
  result = string_to_iconv(to_utf8, str, result_len);
  iconv_close(to_utf8);
  return result;
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
  iconv_t to_utf8 = create_to_utf8();
  if(to_utf8 == (iconv_t)(-1))
    return;

  while(inbytesleft > 0) {
    size_t ret;
    
    iconv_errno = 0;
    ret = iconv(to_utf8, &inbuf, &inbytesleft, &outbuf, &outbytesleft);

    if(ret == (size_t) - 1) {
      int err = iconv_errno;
      if(err == E2BIG) { // output buffer too small
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
  
  iconv_close(to_utf8);
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
  iconv_t to_native = create_to_native();
  if(to_native == (iconv_t)(-1))
    return;

  while(inbytesleft > 0) {
    size_t ret;

    iconv_errno = 0;
    ret = iconv(to_native, &inbuf, &inbytesleft, &outbuf, &outbytesleft);

    if(ret == (size_t) - 1) {
      int err = iconv_errno;
      if(err != E2BIG && err != EILSEQ && err != EINVAL) {
        pmath_debug_print("[pmath_native_writer: unknown errno %d from failed iconv()]\n", err);
      
        if(inbytesleft < 4)
          err = EINVAL;
        else if(outbytesleft < 4)
          err = E2BIG;
        else 
          err = EILSEQ;
      }
      
//      if(err == E2BIG){ // output buffer too small
      *outbuf = '\0';

      write_with_nulls(info, buf, outbuf);

      outbuf = buf;
      outbytesleft = sizeof(buf) - 1;
//      }

      if(err == EILSEQ && ((size_t)inbuf & 1) == 0 && inbytesleft >= 2) { // 2-byte aligned
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
      else if(err != E2BIG) { // invalid input byte  or  incomplete input
        ++inbuf;
        --inbytesleft;
      }
    }
  }
  
  iconv_close(to_native);
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
      RESET_CACHED_HASH(_str);
      return str;
    }

    if(pmath_atomic_read_aquire(&_ins->inherited.refcount) == 1) {
      _ins->length +=            _str->length;
      _ins->capacity_or_start = _str->capacity_or_start;
      pmath_unref(str);
      RESET_CACHED_HASH(_ins);
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
      RESET_CACHED_HASH(_str);
      return string;
    }

    if(start == 0) {
      _str->length = length;
      RESET_CACHED_HASH(_str);
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
      RESET_CACHED_HASH(_str);
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

    result->debug_metadata    = NULL;
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
  pmath_string_t result;
  iconv_t from_native = create_from_native();
  if(from_native == (iconv_t)(-1))
    return PMATH_NULL;
  
  result = string_from_iconv(from_native, str, len);
  iconv_close(from_native);
  return result;
}

PMATH_API
char *pmath_string_to_native(pmath_string_t str, int *result_len) {
  char *result;
  iconv_t to_native = create_to_native();
  if(to_native == (iconv_t)(-1)) {
    if(result_len)
      *result_len = 0;
    return NULL;
  }
  result = string_to_iconv(to_native, str, result_len);
  iconv_close(to_native);
  return result;
}

static pmath_string_t string_from_iconv(iconv_t cd, const char *str, int len) {
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
    size_t ret = iconv(cd, &inbuf, &inbytesleft, &outbuf, &outbytesleft);

    if(ret == (size_t)(-1)) {
      pmath_unref(PMATH_FROM_PTR(result));
      return PMATH_NULL;
    }
  }

  result->length = ((size_t)outbuf - (size_t)AFTER_STRING(result)) / 2;
  return _pmath_from_buffer(result);
}

static char *string_to_iconv(iconv_t cd, pmath_string_t str, int *result_len) {
  const uint16_t *buf = pmath_string_buffer(&str);
  int             len = pmath_string_length(str);
  size_t size         = 4 * ((size_t)len) + 1; // worst case for UTF-8: every character is 4 bytes in utf8
  char *res           = pmath_mem_alloc(size);
  char *inbuf         = (char *)buf;
  char *outbuf        = res;
  size_t inbytesleft  = sizeof(uint16_t) * (size_t)len;
  size_t outbytesleft = size - 1;
  iconv_t to_utf8;
  
  if(!res) {
    if(result_len)
      *result_len = 0;
    return NULL;
  }

  to_utf8 = create_to_utf8();
  if(to_utf8 == (iconv_t)(-1)) {
    pmath_mem_free(res);
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
  
  iconv_close(to_utf8);
  return res;
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
  _pmath_init_special_type(
      PMATH_TYPE_SHIFT_PINNED_STRING,
      compare_pinned_strings,
      hash_pinned_string,
      destroy_pinned_string,
      NULL,
      NULL);

  _init_pmath_native_encoding();

  return TRUE;
}

PMATH_PRIVATE
void _pmath_strings_done(void) {
}

static iconv_t create_to_utf8(void) {
  return iconv_open("UTF-8", PMATH_BYTE_ORDER < 0 ? "UTF-16LE" : "UTF-16BE");
}

static iconv_t create_from_utf8(void) {
  return iconv_open(PMATH_BYTE_ORDER < 0 ? "UTF-16LE" : "UTF-16BE", "UTF-8");
}

static iconv_t create_to_native(void) {
  return iconv_open(_pmath_native_encoding, PMATH_BYTE_ORDER < 0 ? "UTF-16LE" : "UTF-16BE");
}

static iconv_t create_from_native(void) {
  return iconv_open(PMATH_BYTE_ORDER < 0 ? "UTF-16LE" : "UTF-16BE", _pmath_native_encoding);
}

static uint16_t unicode_subscript(uint16_t ch) {
  switch(ch) {
    case '0': return 0x2080; // U+2080 SUBSCRIPT ZERO
    case '1': return 0x2081; // U+2081 SUBSCRIPT ONE
    case '2': return 0x2082; // U+2082 SUBSCRIPT TWO
    case '3': return 0x2083; // U+2083 SUBSCRIPT THREE
    case '4': return 0x2084; // U+2084 SUBSCRIPT FOUR
    case '5': return 0x2085; // U+2085 SUBSCRIPT FIVE
    case '6': return 0x2086; // U+2086 SUBSCRIPT SIX
    case '7': return 0x2087; // U+2087 SUBSCRIPT SEVEN
    case '8': return 0x2088; // U+2088 SUBSCRIPT EIGHT
    case '9': return 0x2089; // U+2089 SUBSCRIPT NINE
    case '+': return 0x208A; // U+208A SUBSCRIPT PLUS SIGN
    case '-': return 0x208B; // U+208B SUBSCRIPT MINUS
    case '=': return 0x208C; // U+208C SUBSCRIPT EQUALS SIGN
    case '(': return 0x208D; // U+208D SUBSCRIPT LEFT PARENTHESIS
    case ')': return 0x208E; // U+208E SUBSCRIPT RIGHT PARENTHESIS
    
    case 'a': return 0x2090; // U+2090 LATIN SUBSCRIPT SMALL LETTER A
    case 'e': return 0x2091; // U+2091 LATIN SUBSCRIPT SMALL LETTER E
    case 'h': return 0x2095; // U+2095 LATIN SUBSCRIPT SMALL LETTER H
    case 'i': return 0x1D62; // U+1D62 LATIN SUBSCRIPT SMALL LETTER I
    case 'j': return 0x2C7C; // U+2C7C LATIN SUBSCRIPT SMALL LETTER J
    case 'k': return 0x2096; // U+2096 LATIN SUBSCRIPT SMALL LETTER K
    case 'l': return 0x2097; // U+2097 LATIN SUBSCRIPT SMALL LETTER L
    case 'm': return 0x2098; // U+2098 LATIN SUBSCRIPT SMALL LETTER M
    case 'n': return 0x2099; // U+2099 LATIN SUBSCRIPT SMALL LETTER N
    case 'o': return 0x2092; // U+2092 LATIN SUBSCRIPT SMALL LETTER O
    case 'p': return 0x209A; // U+209A LATIN SUBSCRIPT SMALL LETTER P
    case 's': return 0x209B; // U+209B LATIN SUBSCRIPT SMALL LETTER S
    case 't': return 0x209C; // U+209C LATIN SUBSCRIPT SMALL LETTER T
    case 'r': return 0x1D63; // U+1D63 LATIN SUBSCRIPT SMALL LETTER R
    case 'u': return 0x1D64; // U+1D64 LATIN SUBSCRIPT SMALL LETTER U
    case 'v': return 0x1D65; // U+1D65 LATIN SUBSCRIPT SMALL LETTER V
    case 'x': return 0x2093; // U+2093 LATIN SUBSCRIPT SMALL LETTER X

    // TODO: probably will be in future Unicode (> 17) (see Additional draft repertoire for provisionally assigned code points for Unicode (post 17.0) and possibly ISO/IEC 10646 7th edition, 2026-11-26)
    // case 'w': return 0x209D; // U+209D LATIN SUBSCRIPT SMALL LETTER W
    // case 'y': return 0x209E; // U+209E LATIN SUBSCRIPT SMALL LETTER Y
    // case 'z': return 0x209F; // U+209D LATIN SUBSCRIPT SMALL LETTER Z

    case 0x03B2: return 0x1D66; // U+1D66 GREEK SUBSCRIPT SMALL LETTER BETA
    case 0x03B3: return 0x1D67; // U+1D67 GREEK SUBSCRIPT SMALL LETTER GAMMA
    case 0x03C1: return 0x1D68; // U+1D68 GREEK SUBSCRIPT SMALL LETTER RHO
    case 0x03C6: return 0x1D69; // U+1D69 GREEK SUBSCRIPT SMALL LETTER PHI
    case 0x03C7: return 0x1D6A; // U+1D6A GREEK SUBSCRIPT SMALL LETTER CHI
  }
  return 0;
}

static uint16_t unicode_superscript(uint16_t ch) {
  switch(ch) {
    case '0': return 0x2070; // U+2070 SUPERSCRIPT ZERO
    case '1': return 0x00B9; // U+00B9 SUPERSCRIPT ONE
    case '2': return 0x00B2; // U+00B2 SUPERSCRIPT TWO
    case '3': return 0x00B3; // U+00B3 SUPERSCRIPT THREE
    case '4': return 0x2074; // U+2074 SUPERSCRIPT FOUR
    case '5': return 0x2075; // U+2075 SUPERSCRIPT FIVE
    case '6': return 0x2076; // U+2076 SUPERSCRIPT SIX
    case '7': return 0x2077; // U+2077 SUPERSCRIPT SEVEN
    case '8': return 0x2078; // U+2078 SUPERSCRIPT EIGHT
    case '9': return 0x2079; // U+2079 SUPERSCRIPT NINE
    case '+': return 0x207A; // U+207A SUPERSCRIPT PLUS SIGN
    case '-': return 0x207B; // U+207B SUPERSCRIPT MINUS
    case '=': return 0x207C; // U+207C SUPERSCRIPT EQUALS SIGN
    case '(': return 0x207D; // U+207D SUPERSCRIPT LEFT PARENTHESIS
    case ')': return 0x207E; // U+207E SUPERSCRIPT RIGHT PARENTHESIS
    
    case 'A': return 0x1D2C; // U+1D2C MODIFIER LETTER CAPITAL A
    case 'B': return 0x1D2E; // U+1D2E MODIFIER LETTER CAPITAL B
    case 'D': return 0x1D30; // U+1D30 MODIFIER LETTER CAPITAL D
    case 'E': return 0x1D31; // U+1D31 MODIFIER LETTER CAPITAL E
    case 'G': return 0x1D33; // U+1D33 MODIFIER LETTER CAPITAL G
    case 'H': return 0x1D34; // U+1D34 MODIFIER LETTER CAPITAL H
    case 'I': return 0x1D35; // U+1D35 MODIFIER LETTER CAPITAL I
    case 'J': return 0x1D36; // U+1D36 MODIFIER LETTER CAPITAL J
    case 'K': return 0x1D37; // U+1D37 MODIFIER LETTER CAPITAL K
    case 'L': return 0x1D38; // U+1D38 MODIFIER LETTER CAPITAL L
    case 'M': return 0x1D39; // U+1D39 MODIFIER LETTER CAPITAL M
    case 'N': return 0x1D3A; // U+1D3A MODIFIER LETTER CAPITAL N
    case 'O': return 0x1D3C; // U+1D3C MODIFIER LETTER CAPITAL O
    case 'P': return 0x1D3E; // U+1D3E MODIFIER LETTER CAPITAL P
    case 'R': return 0x1D3F; // U+1D3F MODIFIER LETTER CAPITAL R
    case 'T': return 0x1D40; // U+1D40 MODIFIER LETTER CAPITAL T
    case 'U': return 0x1D41; // U+1D41 MODIFIER LETTER CAPITAL U
    case 'V': return 0x2C7D; // U+2C7D MODIFIER LETTER CAPITAL V
    case 'W': return 0x1D42; // U+1D42 MODIFIER LETTER CAPITAL W
    
    case 'a': return 0x1D43; // U+1D43 MODIFIER LETTER SMALL A
    case 'b': return 0x1D47; // U+1D47 MODIFIER LETTER SMALL B
    case 'c': return 0x1D9C; // U+1D9C MODIFIER LETTER SMALL C
    case 'd': return 0x1D48; // U+1D48 MODIFIER LETTER SMALL D
    case 'e': return 0x1D49; // U+1D49 MODIFIER LETTER SMALL E
    case 'f': return 0x1DA0; // U+1DA0 MODIFIER LETTER SMALL F
    case 'g': return 0x1D4D; // U+1D4D MODIFIER LETTER SMALL G
    case 'h': return 0x02B0; // U+02B0 MODIFIER LETTER SMALL H
    case 'i': return 0x2071; // U+2071 SUPERSCRIPT LATIN SMALL LETTER I
    case 'j': return 0x02B2; // U+02B2 MODIFIER LETTER SMALL J
    case 'k': return 0x1D4F; // U+1D4F MODIFIER LETTER SMALL K
    case 'l': return 0x02E1; // U+02E1 MODIFIER LETTER SMALL L
    case 'm': return 0x1D50; // U+1D50 MODIFIER LETTER SMALL M
    case 'n': return 0x207F; // U+207F SUPERSCRIPT LATIN SMALL LETTER N
    case 'o': return 0x1D52; // U+1D52 MODIFIER LETTER SMALL O
    case 'p': return 0x1D56; // U+1D56 MODIFIER LETTER SMALL P
    //case 'q': return 0x107A5; // 0x107A5 MODIFIER LETTER SMALL Q  (not in BMP)
    case 'r': return 0x02B3; // U+02B3 MODIFIER LETTER SMALL R
    case 's': return 0x02E2; // U+02E2 MODIFIER LETTER SMALL S
    case 't': return 0x1D57; // U+1D57 MODIFIER LETTER SMALL T
    case 'u': return 0x1D58; // U+1D58 MODIFIER LETTER SMALL U
    case 'v': return 0x1D5B; // U+1D5B MODIFIER LETTER SMALL V
    case 'w': return 0x02B7; // U+02B7 MODIFIER LETTER SMALL W
    case 'x': return 0x02E3; // U+02E3 MODIFIER LETTER SMALL X
    case 'y': return 0x02B8; // U+02B8 MODIFIER LETTER SMALL Y
    case 'z': return 0x1DBB; // U+1DBB MODIFIER LETTER SMALL Z
    
    case 0x03B1: return 0x1D45; // U+1D45 MODIFIER LETTER SMALL ALPHA
    case 0x03B2: return 0x1D5D; // U+1D5D MODIFIER LETTER SMALL BETA
    case 0x03B3: return 0x1D5E; // U+1D5E MODIFIER LETTER SMALL GREEK GAMMA
    case 0x03B4: return 0x1D5F; // U+1D5F MODIFIER LETTER SMALL DELTA
    case 0x03B5: return 0x1D4B; // U+1D4B MODIFIER LETTER SMALL OPEN E     (epsilon)
    case 0x03B8: return 0x1DBF; // U+1DBF MODIFIER LETTER SMALL THETA
    case 0x03C6: return 0x1D60; // U+1D60 MODIFIER LETTER SMALL GREEK PHI
    case 0x03C7: return 0x1D61; // U+1D61 MODIFIER LETTER SMALL CHI
  }
  return 0;
}

static pmath_bool_t write_boxes_try_unicode_subsuperscript(struct pmath_write_ex_t *info, pmath_t box, uint16_t (*char_sel)(uint16_t)) {
  pmath_string_t tok = PMATH_NULL;
  
  if(!info->can_write_unicode)
    return FALSE;
  
//  if(!(info->options & PMATH_WRITE_OPTIONS_PREFERUNICODE))
//    return FALSE;
  
  if(find_single_token(box, &tok)) {
#define TMP_BUF_LEN  8
    uint16_t tmp_buf[TMP_BUF_LEN];
    const uint16_t *buf = pmath_string_buffer(&tok);
    int len = pmath_string_length(tok);
    int i;
    
    for(i = 0; i < len; ++i) {
      if(!char_sel(buf[i])) {
        pmath_unref(tok);
        return FALSE;
      }
    }
    
    for(i = 0; i < len;) {
      int block_len = len - i;
      int j;
      
      if(block_len > TMP_BUF_LEN)
        block_len = TMP_BUF_LEN;
      
      for(j = 0; j < block_len; ++j) {
        tmp_buf[j] = char_sel(buf[i + j]);
      }
      
      if(!info->can_write_unicode(info->user, tmp_buf, block_len)) {
        pmath_unref(tok);
        return FALSE;
      }
      
      i+= block_len;
    }
    
    for(i = 0; i < len;) {
      int block_len = len - i;
      int j;
      
      if(block_len > TMP_BUF_LEN)
        block_len = TMP_BUF_LEN;
      
      for(j = 0; j < block_len; ++j) {
        tmp_buf[j] = char_sel(buf[i + j]);
      }
      
      info->write(info->user, tmp_buf, block_len);
      
      i+= block_len;
    }
    
//    for(i = 0; i < len; ++i) {
//      uint16_t ch = char_sel(buf[i]);
//      info->write(info->user, &ch, 1);
//    }
    
    pmath_unref(tok);
    return TRUE;
  }
  
  return FALSE;
}
