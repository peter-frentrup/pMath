#include <pmath-core/objects-private.h>
#include <pmath-core/strings-private.h>

#include <pmath-language/charnames.h>
#include <pmath-language/scanner.h>
#include <pmath-language/tokens.h>

#include <pmath-util/concurrency/threads.h>
#include <pmath-util/helpers.h>
#include <pmath-util/incremental-hash-private.h>
#include <pmath-util/memory.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/io-private.h>

#include <errno.h>
#include <iconv.h>
#include <string.h>


static iconv_t to_utf8   = (iconv_t)-1;
static iconv_t from_utf8 = (iconv_t)-1;

static iconv_t to_native   = (iconv_t)-1;
static iconv_t from_native = (iconv_t)-1;



PMATH_PRIVATE 
struct _pmath_string_t *_pmath_new_string_buffer(int size){
  size_t bytes;
  int len = size;
  struct _pmath_string_t *result;
  
  if(size < 0 || (int)LENGTH_TO_CAPACITY(size) < size)
    return NULL;
    
  size = (int)LENGTH_TO_CAPACITY(size);
  bytes = (size_t)size * sizeof(uint16_t);
  if(bytes / sizeof(uint16_t) != (size_t)size)
    return NULL;
  
  result = (void*)PMATH_AS_PTR(_pmath_create_stub(
    PMATH_TYPE_SHIFT_BIGSTRING, 
    STRING_HEADER_SIZE + bytes));
  if(!result)
    return result;
  
  result->length            = len;
  result->buffer            = NULL;
  result->capacity_or_start = size;

  return result;
}

PMATH_PRIVATE
pmath_t _pmath_from_buffer(struct _pmath_string_t *b){
  if(b && b->length <= 2){
    pmath_t result;
    const uint16_t *buf = (b->buffer ? AFTER_STRING(b->buffer) + b->capacity_or_start : AFTER_STRING(b));
    
    switch(b->length){
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
struct _pmath_string_t *enlarge_string(
  struct _pmath_string_t *string, // will be freed
  int                     extra_start,
  int                     extralen // not negative
){
  struct _pmath_string_t *result;
  const uint16_t *buf;
  
  if(extralen <= 0){
    pmath_unref(PMATH_FROM_PTR(string));
    return NULL;
  }
  
  assert(extra_start >= 0);

  if(!string)
    return _pmath_new_string_buffer(extralen);
  
  assert(extra_start <= string->length);

  if(string->buffer == NULL && string->inherited.refcount == 1){
    int newcap;
    size_t bytes;
    if(string->capacity_or_start >= string->length + extralen){
      memmove(
        AFTER_STRING(string) + extra_start + extralen, 
        AFTER_STRING(string) + extra_start,
        (string->length - extra_start) * sizeof(uint16_t));
      string->length+= extralen;
      return string;
    }

    newcap = LENGTH_TO_CAPACITY(string->length + extralen);
    bytes = (size_t)newcap * sizeof(uint16_t);
    if(newcap < 0 || bytes/sizeof(uint16_t) != (size_t)newcap){
      pmath_abort_please();
      pmath_unref(PMATH_FROM_PTR(string));
      return NULL;
    }

    result = (struct _pmath_string_t*)
      pmath_mem_realloc(string, STRING_HEADER_SIZE + sizeof(uint16_t) * newcap);
    if(!result)
      return NULL;
    
    if(result->length > extra_start){
      memmove(
        AFTER_STRING(result) + extra_start + extralen, 
        AFTER_STRING(result) + extra_start,
        (result->length - extra_start) * sizeof(uint16_t));
    }
    result->capacity_or_start = newcap;
    result->length+=            extralen;
    return result;
  }

  result = _pmath_new_string_buffer(string->length + extralen);
  if(!result){
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
){
  struct _pmath_string_t *result;
  uint16_t *buffer;
  
  if(pmath_is_pointer(string)){
    return enlarge_string(
      (struct _pmath_string_t*)PMATH_AS_PTR(string), extra_start, extralen);
  }
  
  assert(pmath_is_ministr(string));
  assert(extra_start >= 0);
  assert(extralen >= 0);
  
  if(pmath_is_str0(string)){
    assert(extra_start == 0);
    return _pmath_new_string_buffer(extralen);
  }
  
  if(pmath_is_str1(string)){
    assert(extra_start <= 1);
    
    result = _pmath_new_string_buffer(1 + extralen);
    if(!result)
      return NULL;
    
    buffer = AFTER_STRING(result);
    
    if(extra_start == 0){
      buffer[extralen] = string.s.u.as_chars[0];
    }
    else{
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
  if(extra_start == 0){
    buffer[extralen]   = string.s.u.as_chars[0];
    buffer[extralen+1] = string.s.u.as_chars[1];
  }
  else if(extra_start == 1){
    buffer[0]          = string.s.u.as_chars[0];
    buffer[extralen+1] = string.s.u.as_chars[1];
  }
  else{
    buffer[0] = string.s.u.as_chars[0];
    buffer[1] = string.s.u.as_chars[1];
  }
  
  return result;
}

static void destroy_string(pmath_t p){
  struct _pmath_string_t *str = (void*)PMATH_AS_PTR(p);
  
  if(str->buffer)
    pmath_unref(PMATH_FROM_PTR(str->buffer));
    
  pmath_mem_free(str);
}

PMATH_PRIVATE
pmath_bool_t _pmath_strings_equal(
  pmath_t strA,
  pmath_t strB
){
  const uint16_t *bufA;
  const uint16_t *bufB;
  int len = pmath_string_length(strA);
  
  if(len != pmath_string_length(strB))
    return FALSE;

  bufA = pmath_string_buffer(&strA);
  bufB = pmath_string_buffer(&strB);

  for(;len > 0;--len){
    if(*(bufA++) != *(bufB++))
      return FALSE;
  }

  return TRUE;
}

PMATH_PRIVATE 
int _pmath_strings_compare(
  pmath_t strA,
  pmath_t strB
){
  const uint16_t *bufA = pmath_string_buffer(&strA);
  const uint16_t *bufB = pmath_string_buffer(&strB);
  int lenA = pmath_string_length(strA);
  int lenB = pmath_string_length(strB);

  int i;
  for(i = 0;i < lenA && i < lenB;i++){
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

static unsigned int hash_string(pmath_t str){
  int len             = pmath_string_length(str);
  const uint16_t *buf = pmath_string_buffer(&str);
  
  if(len <= 2){ /* could be a ministr */
    pmath_t tmp;
    switch(len){
      case 0:
        tmp.s.tag = PMATH_TAG_STR0;
        tmp.s.u.as_int32 = 0;
        
      case 1:
        tmp.s.tag = PMATH_TAG_STR1;
        tmp.s.u.as_chars[0] = buf[0];
        tmp.s.u.as_chars[1] = 0;
        
      case 2:
        tmp.s.tag = PMATH_TAG_STR2;
        tmp.s.u.as_chars[0] = buf[0];
        tmp.s.u.as_chars[1] = buf[1];
    }
    
    return incremental_hash(&tmp, sizeof(pmath_t), 0); 
  }
  
  return incremental_hash(buf, (size_t)len * sizeof(uint16_t), 0);
}

PMATH_PRIVATE
void write_cstr(
  const char          *str,
  void               (*write_ucs2)(void*,const uint16_t*,int),
  void                *user
){
  int len = strlen(str);
  #define BUFLEN 256
  uint16_t buf[BUFLEN];
  while(len > BUFLEN){
    int i;
    for(i = 0;i < BUFLEN;++i)
      buf[i] = (uint16_t)(unsigned char)str[i];
    write_ucs2(user, buf, BUFLEN);
    str+= BUFLEN;
    len-= BUFLEN;
  }
  if(len > 0){
    int i;
    for(i = 0;i < len;++i)
      buf[i] = (uint16_t)(unsigned char)str[i];
    write_ucs2(user, buf, len);
  }
  #undef BUFLEN
}

static pmath_bool_t is_single_token(pmath_t box){
  if(pmath_is_string(box))
    return TRUE;
  
  if(pmath_is_expr_of(box, PMATH_NULL)
  || pmath_is_expr_of(box, PMATH_SYMBOL_LIST)){
    pmath_t part;
    pmath_bool_t result;
    
    if(pmath_expr_length(box) != 1)
      return FALSE;
    
    part = pmath_expr_get_item(box, 1);
    result = is_single_token(part);
    pmath_unref(part);
    return result;
  }
  
  if(pmath_is_expr_of(box, PMATH_SYMBOL_STYLEBOX)
  || pmath_is_expr_of(box, PMATH_SYMBOL_TAGBOX)
  || pmath_is_expr_of(box, PMATH_SYMBOL_INTERPRETATIONBOX)){
    pmath_t part;
    pmath_bool_t result;
    
    part = pmath_expr_get_item(box, 1);
    result = is_single_token(part);
    pmath_unref(part);
    return result;
  }
  
  return FALSE;
}

static void write_boxes(
  pmath_t                 box,
  pmath_write_options_t   options,
  void                  (*write)(void*,const uint16_t*,int),
  void                   *user);

static void write_single_token_box(
  pmath_t                 box,
  pmath_write_options_t   options,
  void                  (*write)(void*,const uint16_t*,int),
  void                   *user
){
  if(is_single_token(box)){
    write_boxes(box, options, write, user);
  }
  else{
    write_cstr("(", write, user);
    write_boxes(box, options, write, user);
    write_cstr(")", write, user);
  }
}

static void write_boxes(
  pmath_t                 box,
  pmath_write_options_t   options,
  void                  (*write)(void*,const uint16_t*,int),
  void                   *user
){
  if(pmath_is_string(box)){
    write(user, pmath_string_buffer(&box), pmath_string_length(box));
  }
  else if(pmath_is_expr_of(box, PMATH_SYMBOL_LIST)
  ||      pmath_is_expr_of(box, PMATH_NULL)){
    size_t i;
    
    for(i = 1;i <= pmath_expr_length(box);++i){
      pmath_t part = pmath_expr_get_item(box, i);
      write_boxes(part, options, write, user);
      pmath_unref(part);
    }
  }
  else if(pmath_is_expr_of(box, PMATH_SYMBOL_STYLEBOX)
  ||      pmath_is_expr_of(box, PMATH_SYMBOL_TAGBOX)
  ||      pmath_is_expr_of(box, PMATH_SYMBOL_INTERPRETATIONBOX)){
    pmath_t part = pmath_expr_get_item(box, 1);
    write_boxes(part, options, write, user);
    pmath_unref(part);
  }
  else if(pmath_is_expr_of(box, PMATH_SYMBOL_SUBSCRIPTBOX)){
    pmath_t part = pmath_expr_get_item(box, 1);
    write_cstr("_", write, user);
    write_single_token_box(part, options, write, user);
    pmath_unref(part);
  }
  else if(pmath_is_expr_of(box, PMATH_SYMBOL_SUPERSCRIPTBOX)){
    pmath_t part = pmath_expr_get_item(box, 1);
    write_cstr("^", write, user);
    write_single_token_box(part, options, write, user);
    pmath_unref(part);
  }
  else if(pmath_is_expr_of(box, PMATH_SYMBOL_SUBSUPERSCRIPTBOX)){
    pmath_t part = pmath_expr_get_item(box, 1);
    write_cstr("_", write, user);
    write_single_token_box(part, options, write, user);
    pmath_unref(part);
    
    part = pmath_expr_get_item(box, 2);
    write_cstr("^", write, user);
    write_single_token_box(part, options, write, user);
    pmath_unref(part);
  }
  else if(pmath_is_expr_of(box, PMATH_SYMBOL_UNDERSCRIPTBOX)){
    pmath_t part = pmath_expr_get_item(box, 1);
    write_boxes(part, options, write, user);
    pmath_unref(part);
    
    part = pmath_expr_get_item(box, 2);
    write_cstr("_", write, user);
    write_single_token_box(part, options, write, user);
    pmath_unref(part);
  }
  else if(pmath_is_expr_of(box, PMATH_SYMBOL_OVERSCRIPTBOX)){
    pmath_t part = pmath_expr_get_item(box, 1);
    write_boxes(part, options, write, user);
    pmath_unref(part);
    
    part = pmath_expr_get_item(box, 2);
    write_cstr("^", write, user);
    write_single_token_box(part, options, write, user);
    pmath_unref(part);
  }
  else if(pmath_is_expr_of(box, PMATH_SYMBOL_UNDEROVERSCRIPTBOX)){
    pmath_t part = pmath_expr_get_item(box, 1);
    write_boxes(part, options, write, user);
    pmath_unref(part);
    
    part = pmath_expr_get_item(box, 2);
    write_cstr("_", write, user);
    write_single_token_box(part, options, write, user);
    pmath_unref(part);
    
    part = pmath_expr_get_item(box, 3);
    write_cstr("^", write, user);
    write_single_token_box(part, options, write, user);
    pmath_unref(part);
  }
  else if(pmath_is_expr(box)){
    pmath_t part;
    size_t i;
    
    part = pmath_expr_get_item(box, 0);
    pmath_write(part, options, write, user);
    pmath_unref(part);
    
    write_cstr("(", write, user);
    for(i = 1;i <= pmath_expr_length(box);++i){
      if(i > 1)
        write_cstr(", ", write, user);
      
      part = pmath_expr_get_item(box, i);
      write_boxes(part, options, write, user);
      pmath_unref(part);
      
    }
    write_cstr(")", write, user);
  }
  else
    pmath_write(box, options, write, user);
}

PMATH_PRIVATE 
void _pmath_string_write(
  pmath_t                 str,
  pmath_write_options_t   options,
  void                  (*write)(void*,const uint16_t*,int),
  void                   *user
){
  static char hex_digits[16] = "0123456789ABCDEF";

  if(options & PMATH_WRITE_OPTIONS_FULLSTR){
    const uint16_t *buffer = pmath_string_buffer(&str);
    const uint16_t *end    = buffer + pmath_string_length(str);
    const uint16_t *s      = buffer;
    
    write_cstr("\"", write, user);
    
    while(s != end){
      const uint16_t *start = s;
      int len = 0;
      while(s != end 
      && *s >= ' ' 
      && *s != '\"' 
      && *s != '\\' 
      && *s <= 0x7F){
        ++s;
        ++len;
      }

      if(start != s)
        write(user, start, len);

      if(s != end){
        uint16_t special[10];
        special[0] = '\\';
        switch(*s){
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
            if(s + 1 != end 
            && (s[0] & 0xFC00) == 0xD800
            && (s[1] & 0xFC00) == 0xDC00){
              uint32_t u = 0x10000 | (((uint32_t)s[0] & 0x03FF) << 10) | (s[1] & 0x03FF);
              const char *name = pmath_char_to_name(u);
              
              ++s;
              if(name){
                write_cstr("\\[", write, user);
                write_cstr(name, write, user);
                write_cstr("]", write, user);
              }
              else{
                special[1] = 'U';
                special[2] = hex_digits[(u & 0xF0000000U) >> 28];
                special[3] = hex_digits[(u & 0x0F000000U) >> 24];
                special[4] = hex_digits[(u & 0x00F00000U) >> 20];
                special[5] = hex_digits[(u & 0x000F0000U) >> 16];
                special[6] = hex_digits[(u & 0x0000F000U) >> 12];
                special[7] = hex_digits[(u & 0x00000F00U) >> 8];
                special[8] = hex_digits[(u & 0x000000F0U) >> 4];
                special[9] = hex_digits[ u & 0x0000000FU];
                write(user, special, 10);
              }
            }
            else{
              const char *name = pmath_char_to_name(*s);
              
              if(name){
                write_cstr("\\[", write, user);
                write_cstr(name, write, user);
                write_cstr("]", write, user);
              }
              else if(*s <= 0xFF){
                special[1] = 'x';
                special[2] = hex_digits[((*s) & 0xF0) >> 4];
                special[3] = hex_digits[ (*s) & 0x0F];
                write(user, special, 4);
              }
              else{
                special[1] = 'u';
                special[2] = hex_digits[((*s) & 0xF000) >> 12];
                special[3] = hex_digits[((*s) & 0x0F00) >>  8];
                special[4] = hex_digits[((*s) & 0x00F0) >>  4];
                special[5] = hex_digits[ (*s) & 0x000F];
                write(user, special, 6);
              }
            }
          }
        }
        ++s;
      }
    }
    write_cstr("\"", write, user);

    return;
  }
  else{
    pmath_t expanded = pmath_string_expand_boxes(pmath_ref(str));
    
    write_boxes(expanded, options, write, user);
    
    pmath_unref(expanded);
  }
  
  //write(user, pmath_string_buffer(&str), pmath_string_length(str));
}

/*============================================================================*/

PMATH_API 
pmath_string_t pmath_string_new(int capacity){
  struct _pmath_string_t *result;
  
  assert(capacity >= 0);
  
  if(capacity <= 2){
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
){
  struct _pmath_string_t *result;
  uint16_t *ucs;
  int len;
  
  assert(pmath_is_null(str) || pmath_is_string(str));
  
  if(inslen < 0)
    inslen = strlen(ins);
  
  if(inslen == 0){
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
    len = ((struct _pmath_string_t*)PMATH_AS_PTR(str))->length;
  
  if(inspos < 0)
    inspos = 0;
  else if(inspos > len)
    inspos = len;
  
  result = enlarge_string_2(str, inspos, inslen);
  if(!result)
    return PMATH_NULL;
  
  ucs = AFTER_STRING(result) + inspos;
  while(inslen-- > 0)
    *ucs++ = (uint16_t)(unsigned char)*ins++;

  return _pmath_from_buffer(result);
}

PMATH_API 
pmath_string_t pmath_string_from_utf8(
  const char    *str,
  int            len
){
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
  inbuf  = (char*)str;
  outbuf = (char*)AFTER_STRING(result);
  
  while(inbytesleft > 0){
    size_t ret = iconv(from_utf8, &inbuf, &inbytesleft, &outbuf, &outbytesleft);
    
    if(ret == (size_t)-1){
      if(errno == E2BIG){ // output buffer too small
        size_t bytes_written = (size_t)outbuf - (size_t)AFTER_STRING(result);
        result->length = (int)(bytes_written/sizeof(uint16_t));
        
        result = enlarge_string(result, result->length, (3 * inbytesleft) / 2 + 1);
        
        if(!result)
          return PMATH_NULL;
        
        outbuf = ((char*)AFTER_STRING(result)) + bytes_written;
        outbytesleft = sizeof(uint16_t) * result->length - bytes_written;
      }
      else if(errno == EILSEQ){ // invalid input byte
        ++inbuf;
        --inbytesleft;
      }
      else if(errno == EINVAL){ // incomplete input
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
){
  const uint16_t *buf = pmath_string_buffer(&str);
  int             len = pmath_string_length(str);
  size_t size         = 4 * ((size_t)len) + 1; // worst case: every character is 4 bytes in utf8
  char *res           = pmath_mem_alloc(size);
  char *inbuf         = (char*)buf;
  char *outbuf        = res;
  size_t inbytesleft  = sizeof(uint16_t) * (size_t)len;
  size_t outbytesleft = size - 1;
  
  if(!res){
    if(result_len)
      *result_len = 0;
    return NULL;
  }
  
  while(inbytesleft > 0){
    size_t ret = iconv(to_utf8, &inbuf, &inbytesleft, &outbuf, &outbytesleft);
    
    if(ret == (size_t)(-1)){
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

PMATH_API 
void pmath_utf8_writer(void *user, const uint16_t *data, int len){
  char *inbuf  = (char*)data;
  size_t inbytesleft = sizeof(uint16_t) * (size_t)len;
  char buf[100];
  char *outbuf = buf;
  size_t outbytesleft = sizeof(buf) - 1;
  
  while(inbytesleft > 0){
    size_t ret = iconv(to_utf8, &inbuf, &inbytesleft, &outbuf, &outbytesleft);
    
    if(ret == (size_t)-1){
      if(errno == E2BIG){ // output buffer too small
        *outbuf = '\0';
        
        ((pmath_cstr_writer_info_t*)user)->write_cstr(
          ((pmath_cstr_writer_info_t*)user)->user,
          buf);
        
        outbuf = buf;
        outbytesleft = sizeof(buf) - 1;
      }
      else{ // invalid input byte  or  incomplete input
        ++inbuf;
        --inbytesleft;
      }
    }
  }
  
  *outbuf = '\0';
  if(*buf){
    ((pmath_cstr_writer_info_t*)user)->write_cstr(
      ((pmath_cstr_writer_info_t*)user)->user,
      buf);
  }
}

PMATH_API 
void pmath_native_writer(void *user, const uint16_t *data, int len){
  pmath_cstr_writer_info_t *info = user;
  char *inbuf  = (char*)data;
  size_t inbytesleft = sizeof(uint16_t) * (size_t)len;
  char buf[100];
  char *outbuf = buf;
  size_t outbytesleft = sizeof(buf) - 1;
  
  while(inbytesleft > 0){
    size_t ret = iconv(to_native, &inbuf, &inbytesleft, &outbuf, &outbytesleft);
    
    if(ret == (size_t)-1){
//      if(errno == E2BIG){ // output buffer too small
        *outbuf = '\0';
        
        info->write_cstr(info->user, buf);
        
        outbuf = buf;
        outbytesleft = sizeof(buf) - 1;
//      }

      if(errno == EILSEQ && ((size_t)inbuf & 1) == 0 && inbytesleft >= 2){ // 2-byte aligned
        const char *name;
        uint16_t *u16 = (void*)inbuf;
        uint32_t ch;
        
        if((u16[0] & 0xFC00) == 0xD800 && inbytesleft >= 4
        && (u16[1] & 0xFC00) == 0xDC00){
          ch = 0x10000 + ((uint32_t)(u16[0] & 0x3FF) << 10) + (u16[1] & 0x3FF);
          
          inbuf+= 4;
          inbytesleft-= 4;
        }
        else{ 
          ch = u16[0];
          
          inbuf+= 2;
          inbytesleft-= 2;
        }
        
        switch(ch){
          case PMATH_CHAR_ASSIGN:        info->write_cstr(info->user, ":="); break;
          case PMATH_CHAR_ASSIGNDELAYED: info->write_cstr(info->user, "::="); break;
          case PMATH_CHAR_RULE:          info->write_cstr(info->user, "->"); break;
          case PMATH_CHAR_RULEDELAYED:   info->write_cstr(info->user, ":>"); break;
          
          default:
            name = pmath_char_to_name(ch);
            if(name){
              info->write_cstr(info->user, "\\[");
              info->write_cstr(info->user, name);
              info->write_cstr(info->user, "]");
            }
        }
      }
      else if(errno != E2BIG){ // invalid input byte  or  incomplete input
        ++inbuf;
        --inbytesleft;
      }
    }
  }
  
  *outbuf = '\0';
  if(*buf)
    info->write_cstr(info->user, buf);
}

PMATH_API 
pmath_string_t pmath_string_insert_codepage(
  pmath_string_t str,
  int            inspos,
  const char    *ins,
  int            inslen,
  const uint16_t *cp
){
  struct _pmath_string_t *result;
  uint16_t *ucs;
  int len;
 
  if(inslen < 0)
    inslen = strlen(ins);

  if(inslen == 0){
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
    len = ((struct _pmath_string_t*)PMATH_AS_PTR(str))->length;
  
  if(inspos < 0)
    inspos = 0;
  else if(inspos > len)
    inspos = len;
  
  result = enlarge_string_2(str, inspos, inslen);
  if(!result)
    return PMATH_NULL;
  
  ucs = AFTER_STRING(result) + inspos;
  while(inslen-- > 0)
    *ucs++ = cp[(unsigned char)*ins++];

  return _pmath_from_buffer(result);
}

PMATH_API 
pmath_string_t pmath_string_insert_ucs2(
  pmath_string_t  str,
  int             inspos,
  const uint16_t *ins,
  int             inslen
){
  struct _pmath_string_t *result;
  uint16_t *ucs;
  int len;
 
  if(inslen < 0){
    register const uint16_t *tmp = ins;
    inslen = 0;
    while(*tmp++)
      ++inslen;
  }
  
  if(inslen == 0){
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
    len = ((struct _pmath_string_t*)PMATH_AS_PTR(str))->length;
  
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
){
  pmath_string_t result;
  struct _pmath_string_t *_str;
  struct _pmath_string_t *_ins;
  
  if(pmath_is_ministr(ins)){
    int len = 0;
    if(pmath_is_str1(ins))
      len = 1;
    if(pmath_is_str2(ins))
      len = 2;
    
    return pmath_string_insert_ucs2(str, inspos, ins.s.u.as_chars, len);
  }
  
  _ins = (struct _pmath_string_t*)PMATH_AS_PTR(ins);
  
  if(pmath_is_str0(str)){
    _str = NULL;
  }
  else if(pmath_is_str1(str)){
    _str = _pmath_new_string_buffer(1);
    
    if(_str == NULL){
      pmath_unref(ins);
      return PMATH_NULL;
    }
    
    AFTER_STRING(_str)[0] = str.s.u.as_chars[0];
    str = PMATH_FROM_PTR(_str);
  }
  else if(pmath_is_str2(str)){
    _str = _pmath_new_string_buffer(2);
    
    if(_str == NULL){
      pmath_unref(ins);
      return PMATH_NULL;
    }
    
    AFTER_STRING(_str)[0] = str.s.u.as_chars[0];
    AFTER_STRING(_str)[1] = str.s.u.as_chars[1];
    str = PMATH_FROM_PTR(_str);
  }
  else{
    _str = (struct _pmath_string_t*)PMATH_AS_PTR(str);
  }
  
  if(_str && _ins 
  && _str->buffer
  && _str->buffer == _ins->buffer
  && _str->length == inspos
  && _str->capacity_or_start + inspos == _ins->capacity_or_start){
    if(_str->inherited.refcount == 1){
      _str->length+= _ins->length;
      pmath_unref(ins);
      return str;
    }
    
    if(_ins->inherited.refcount == 1){
      _ins->length+=            _str->length;
      _ins->capacity_or_start = _str->capacity_or_start;
      pmath_unref(str);
      return ins;
    }
    
    result = pmath_ref(PMATH_FROM_PTR(_str->buffer));
    result = pmath_string_part(
      result, 
      _str->capacity_or_start + inspos,
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
){
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
){
  struct _pmath_string_t *_str;
  
  if(pmath_is_null(string))
    return PMATH_NULL;
  
  if(pmath_is_str0(string))
    return string;
  
  if(length == 0){
    pmath_unref(string);
    
    string.s.tag = PMATH_TAG_STR0;
    string.s.u.as_chars[0] = 0;
    string.s.u.as_chars[1] = 0;
    return string;
  }
  
  if(pmath_is_str1(string)){
    if(start == 0)
      return string;
    
    string.s.tag = PMATH_TAG_STR0;
    string.s.u.as_chars[0] = 0;
    return string;
  }
  
  if(pmath_is_str2(string)){
    if(start == 0){
      if(length == 1){
        string.s.tag = PMATH_TAG_STR1;
        string.s.u.as_chars[1] = 0;
      }
      
      return string;
    }
    
    if(start == 1){
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
  
  _str = (struct _pmath_string_t*)PMATH_AS_PTR(string);
  
  if(start < 0 || start >= _str->length){
    pmath_unref(string);
    
    string.s.tag = PMATH_TAG_STR0;
    string.s.u.as_chars[0] = 0;
    string.s.u.as_chars[1] = 0;
    return string;
  }

  if(length < 0
  || start + length > _str->length
  || length > _str->length)
    length = _str->length - start;

  if(length <= 0){
    pmath_unref(string);
    
    string.s.tag = PMATH_TAG_STR0;
    string.s.u.as_chars[0] = 0;
    string.s.u.as_chars[1] = 0;
    return string;
  }

  if(length == _str->length)
    return string;
  
  if(length <= 2){
    pmath_t result;
    const uint16_t *buffer = start + (_str->buffer ? AFTER_STRING(_str->buffer) + _str->capacity_or_start : AFTER_STRING(_str));
    
    if(length == 1){
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

  if(_str->inherited.refcount == 1){
    if(_str->buffer){
      _str->capacity_or_start+= start;
      _str->length            = length;
      return string;
    }
    
    if(start == 0){
      _str->length = length;
      return string;
    }
  }

  if(_str->buffer){
    pmath_t tmp = string;
    
    start+= _str->capacity_or_start;
    string = pmath_ref(PMATH_FROM_PTR(_str->buffer));
    _str = (struct _pmath_string_t*)PMATH_AS_PTR(string);
    pmath_unref(tmp);
    
    assert(pmath_is_string(string));

    if(start == 0 && PMATH_AS_PTR(string)->refcount == 1){
      _str->length = length;
      return string;
    }
  }
  
  {
    struct _pmath_string_t *result = (void*)PMATH_AS_PTR(_pmath_create_stub(
      PMATH_TYPE_SHIFT_BIGSTRING,
      sizeof(struct _pmath_string_t)));
    
    if(!result){
      pmath_unref(string);
      return PMATH_NULL;
    }
    
    result->length            = length;
    result->buffer            = _str;
    result->capacity_or_start = start;

    return PMATH_FROM_PTR(result); /* already know length > 2 */
  }
}

PMATH_API 
const uint16_t *pmath_string_buffer(pmath_string_t *string){
  struct _pmath_string_t *_str;
  
  if(pmath_is_null(*string))
    return NULL;
  
  if(pmath_is_ministr(*string))
    return &string->s.u.as_chars[0];
  
  assert(pmath_is_string(*string));
  _str = (void*)PMATH_AS_PTR(*string);

  if(_str->buffer == NULL)
    return AFTER_STRING(_str);

  return AFTER_STRING(_str->buffer) + _str->capacity_or_start;
}

PMATH_API 
int pmath_string_length(pmath_string_t string){
  struct _pmath_string_t *_str;
  
  if(pmath_is_null(string) || pmath_is_str0(string))
    return 0;
  
  if(pmath_is_str1(string))
    return 1;
  
  if(pmath_is_str2(string))
    return 2;
  
  assert(pmath_is_bigstr(string));
  
  _str = (void*)PMATH_AS_PTR(string);
  
  assert(_str->length >= 0);
  return _str->length;
}

PMATH_API
PMATH_ATTRIBUTE_PURE
pmath_bool_t pmath_string_equals_latin1(
  pmath_string_t  string, 
  const char     *latin1
){
  const uint16_t *buf;
  int i, len;
  
  buf = pmath_string_buffer(&string);
  len = pmath_string_length(string);
  for(i = 0;i < len;++i)
    if(latin1[i] == '\0' || buf[i] != latin1[i])
      return FALSE;
  
  return latin1[len] == '\0';
}

/*============================================================================*/

PMATH_API
pmath_string_t pmath_string_from_native(
  const char  *str,
  int          len
){
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
  inbuf  = (char*)str;
  outbuf = (char*)AFTER_STRING(result);
  
  if(!result)
    return PMATH_NULL;

  while(inbytesleft > 0){
    size_t ret = iconv(from_native, &inbuf, &inbytesleft, &outbuf, &outbytesleft);
    
    if(ret == (size_t)(-1)){
      pmath_unref(PMATH_FROM_PTR(result));
      return PMATH_NULL;
    }
  }
  
  result->length = ((size_t)outbuf - (size_t)AFTER_STRING(result))/2;
  return _pmath_from_buffer(result);
}

PMATH_API 
char *pmath_string_to_native(pmath_string_t str, int *result_len){
  int len = pmath_string_length(str);
  const uint16_t *buf = pmath_string_buffer(&str);

  size_t s_size = 4 * ((size_t)len) + 1;
  char *s = (char*)pmath_mem_alloc(s_size);
  
  size_t inbytesleft = sizeof(uint16_t) * (size_t)len;
  size_t outbytesleft = s_size - 1;
  char *inbuf  = (char*)buf;
  char *outbuf = s;
  
  if(!s){
    if(result_len)
      *result_len = 0;
    return NULL;
  }
  
  while(inbytesleft > 0){
    size_t ret = iconv(to_native, &inbuf, &inbytesleft, &outbuf, &outbytesleft);
    
    if(ret == (size_t)(-1)){
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
pmath_bool_t _pmath_strings_init(void){
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
  
  if(to_utf8 == (iconv_t)-1)
    return FALSE;
  
  from_utf8 = iconv_open(
    PMATH_BYTE_ORDER < 0 ? "UTF-16LE" : "UTF-16BE",
    "UTF-8");
  
  if(from_utf8 == (iconv_t)-1){
    iconv_close(to_utf8);
    return FALSE;
  }
  
  _init_pmath_native_encoding();
  
  to_native = iconv_open(
    _pmath_native_encoding,
    PMATH_BYTE_ORDER < 0 ? "UTF-16LE" : "UTF-16BE");
  
  if(to_native == (iconv_t)-1){
    iconv_close(to_utf8);
    iconv_close(from_utf8);
    return FALSE;
  }
  
  from_native = iconv_open(
    PMATH_BYTE_ORDER < 0 ? "UTF-16LE" : "UTF-16BE",
    _pmath_native_encoding);
  
  if(from_native == (iconv_t)-1){
    iconv_close(to_utf8);
    iconv_close(from_utf8);
    iconv_close(to_native);
    return FALSE;
  }
  
  return TRUE;
}

PMATH_PRIVATE 
void _pmath_strings_done(void){
  iconv_close(to_utf8);
  iconv_close(from_utf8);
  iconv_close(to_native);
  iconv_close(from_native);
}
