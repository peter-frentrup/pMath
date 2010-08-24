#include <pmath-core/objects-private.h>
#include <pmath-core/strings-private.h>

#include <pmath-builtins/io-private.h>
#include <pmath-language/tokens.h>
#include <pmath-util/memory.h>
#include <pmath-util/incremental-hash-private.h>

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <iconv.h>

#include <pmath-util/debug.h>

#include <pmath-util/concurrency/threads.h>

#include <pmath-language/charnames.h>

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
  
  result = (struct _pmath_string_t*)_pmath_create_stub(
    PMATH_TYPE_SHIFT_STRING,
    STRING_HEADER_SIZE + bytes);
  if(!result)
    return result;
  
  result->length            = len;
  result->buffer            = NULL;
  result->capacity_or_start = size;

  return result;
}

PMATH_PRIVATE 
struct _pmath_string_t *enlarge_string(
  struct _pmath_string_t *string, // will be freed
  int extra_start,
  int extralen // not negative
){
  struct _pmath_string_t *result;
  
  if(extralen <= 0){
    pmath_unref((pmath_string_t)string);
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
      pmath_unref((pmath_string_t)string);
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
    pmath_unref((pmath_string_t)string);
    return NULL;
  }

  memcpy(
    AFTER_STRING(result),
    pmath_string_buffer((pmath_string_t)string),
    extra_start * sizeof(uint16_t));
    
  memcpy(
    AFTER_STRING(result) + extra_start + extralen,
    pmath_string_buffer((pmath_string_t)string) + extra_start,
    (string->length - extra_start) * sizeof(uint16_t));

  pmath_unref((pmath_string_t)string);
  return result;
}

static void destroy_string(struct _pmath_string_t *str){
  if(str->buffer)
    pmath_unref((pmath_string_t)str->buffer);
    
  pmath_mem_free(str);
}

static pmath_bool_t equal_strings(
  pmath_string_t strA,
  pmath_string_t strB
){
  const uint16_t *bufA;
  const uint16_t *bufB;
  int len = pmath_string_length(strA);
  
  if(len != pmath_string_length(strB))
    return FALSE;

  bufA = pmath_string_buffer(strA);
  bufB = pmath_string_buffer(strB);

  for(;len > 0;--len){
    if(*(bufA++) != *(bufB++))
      return FALSE;
  }

  return TRUE;
}

static int compare_strings(
  pmath_string_t strA,
  pmath_string_t strB
){
  const uint16_t *bufA = pmath_string_buffer(strA);
  const uint16_t *bufB = pmath_string_buffer(strB);
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

static unsigned int hash_string(pmath_string_t str){
  return incremental_hash(
    pmath_string_buffer(str),
    (size_t)pmath_string_length(str) * sizeof(uint16_t),
    0);
}

PMATH_PRIVATE
void write_cstr(
  const char          *str,
  pmath_write_func_t   write_ucs2,
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

static void write_string(
  pmath_string_t          str,
  pmath_write_options_t   options,
  pmath_write_func_t      write,
  void                   *user
){
  static char hex_digits[16] = "0123456789ABCDEF";

  if(options & PMATH_WRITE_OPTIONS_FULLSTR){
    const uint16_t *buffer = pmath_string_buffer(str);
    const uint16_t *end = buffer + pmath_string_length(str);
    const uint16_t *s = buffer;
    
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

  write(user, pmath_string_buffer(str), pmath_string_length(str));
}

/*============================================================================*/

PMATH_API 
pmath_string_t pmath_string_new(int capacity){
  struct _pmath_string_t *result = _pmath_new_string_buffer(capacity);
  if(!result)
    return NULL;

  result->length = 0;
  return (pmath_string_t)result;
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
  
  if(inslen < 0)
    inslen = strlen(ins);
  
  if(inslen == 0){
    if(str)
      return str;
    return pmath_string_new(0);
  }

  if(str)
    len = ((struct _pmath_string_t*)str)->length;
  else
    len = 0;
  
  if(inspos < 0)
    inspos = 0;
  else if(inspos > len)
    inspos = len;
  
  result = enlarge_string(
    (struct _pmath_string_t*)str, inspos, inslen);
  if(!result)
    return NULL;

  ucs = AFTER_STRING(result) + inspos;
  while(inslen-- > 0)
    *ucs++ = (uint16_t)(unsigned char)*ins++;

  return (pmath_string_t)result;
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
    return NULL;
  
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
          return NULL;
        
        outbuf = ((char*)AFTER_STRING(result)) + bytes_written;
        outbytesleft = sizeof(uint16_t) * result->length - bytes_written;
      }
      else if(errno == EILSEQ){ // invalid input byte
        ++inbuf;
        --inbytesleft;
      }
      else if(errno == EINVAL){ // incomplete input
        result->length = ((size_t)outbuf - (size_t)AFTER_STRING(result)) / 2;
        return (pmath_string_t)result;
      }
    }
  }
  
  result->length = ((size_t)outbuf - (size_t)AFTER_STRING(result)) / 2;
  return (pmath_string_t)result;
}

PMATH_API 
PMATH_ATTRIBUTE_USE_RESULT
char *pmath_string_to_utf8(
  pmath_string_t  str,
  int            *result_len
){
  const uint16_t *buf = pmath_string_buffer(str);
  int             len = pmath_string_length(str);
  size_t size = 4 * ((size_t)len) + 1; // worst case: every character is 4 bytes in utf8
  char *res = pmath_mem_alloc(size);
  char *inbuf  = (char*)buf;
  char *outbuf = res;
  size_t inbytesleft = sizeof(uint16_t) * (size_t)len;
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
  char *inbuf  = (char*)data;
  size_t inbytesleft = sizeof(uint16_t) * (size_t)len;
  char buf[100];
  char *outbuf = buf;
  size_t outbytesleft = sizeof(buf) - 1;
  
  while(inbytesleft > 0){
    size_t ret = iconv(to_native, &inbuf, &inbytesleft, &outbuf, &outbytesleft);
    
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
    if(str)
      return str;
    return pmath_string_new(0);
  }

  if(str)
    len = ((struct _pmath_string_t*)str)->length;
  else
    len = 0;
    
  if(inspos < 0)
    inspos = 0;
  else if(inspos > len)
    inspos = len;
  
  result = enlarge_string(
    (struct _pmath_string_t*)str, inspos, inslen);
  if(!result)
    return NULL;

  ucs = AFTER_STRING(result) + inspos;
  while(inslen-- > 0)
    *ucs++ = cp[(unsigned char)*ins++];

  return (pmath_string_t)result;
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
    if(str)
      return str;
    return pmath_string_new(0);
  }

  if(str)
    len = ((struct _pmath_string_t*)str)->length;
  else
    len = 0;
    
  if(inspos < 0)
    inspos = 0;
  else if(inspos > len)
    inspos = len;
  
  result = enlarge_string(
    (struct _pmath_string_t*)str, inspos, inslen);
  if(!result)
    return NULL;

  ucs = AFTER_STRING(result) + inspos;
  while(inslen-- > 0)
    *ucs++ = *ins++;

  return (pmath_string_t)result;
}

PMATH_API 
pmath_string_t pmath_string_insert(
  pmath_string_t str,
  int            inspos,
  pmath_string_t ins
){
  pmath_string_t result;
  
  struct _pmath_string_t *_str = (struct _pmath_string_t*)str;
  struct _pmath_string_t *_ins = (struct _pmath_string_t*)ins;
  
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
    
    result = pmath_ref((pmath_t)_str->buffer);
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
    pmath_string_buffer(ins), 
    pmath_string_length(ins));
  
  pmath_unref(ins);
  return result;
}

PMATH_API 
pmath_string_t pmath_string_concat(
  pmath_string_t prefix,
  pmath_string_t postfix
){
  if(!prefix)
    return postfix;
  if(!postfix)
    return prefix;
  
  return pmath_string_insert(prefix, pmath_string_length(prefix), postfix);
}

PMATH_API 
pmath_string_t pmath_string_part(
  pmath_string_t string,
  int start,
  int length
){
  struct _pmath_string_t *_str;
  
  if(!string)
    return NULL;
  
  _str = (struct _pmath_string_t*)string;
  
  if(start < 0 || start >= _str->length){
    pmath_unref(string);
    return pmath_string_new(0);
  }

  if(length < 0
  || start + length > _str->length
  || length > _str->length)
    length = _str->length - start;

  if(length <= 0){
    pmath_unref(string);
    return pmath_string_new(0);
  }

  if(length == _str->length)
    return string;

  if(string->refcount == 1){
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
    string = pmath_ref((pmath_t)_str->buffer);
    _str = (struct _pmath_string_t*)string;
    pmath_unref(tmp);
    
    assert(pmath_instance_of(string, PMATH_TYPE_STRING));

    if(start == 0 && string->refcount == 1){
      _str->length = length;
      return string;
    }
  }
  
  {
    struct _pmath_string_t *result = (struct _pmath_string_t*)_pmath_create_stub(
      PMATH_TYPE_SHIFT_STRING,
      sizeof(struct _pmath_string_t));
    
    if(!result)
      return (pmath_string_t)result;
    
    result->length            = length;
    result->buffer            = _str;
    result->capacity_or_start = start;

    return (pmath_string_t)result;
  }
}

PMATH_API 
const uint16_t *pmath_string_buffer(pmath_string_t string){
  struct _pmath_string_t *_str;
  
  if(!string)
    return NULL;
  
  assert(pmath_instance_of(string, PMATH_TYPE_STRING));
  _str = (struct _pmath_string_t*)string;

  if(_str->buffer == NULL)
    return AFTER_STRING(string);

  return AFTER_STRING(_str->buffer) + _str->capacity_or_start;
}

PMATH_API 
int pmath_string_length(pmath_string_t string){
  struct _pmath_string_t *_str;
  
  if(!string)
    return 0;
  
  assert(pmath_instance_of(string, PMATH_TYPE_STRING));
  
  _str = (struct _pmath_string_t*)string;
  
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
  
  buf = pmath_string_buffer(string);
  len = pmath_string_length(string);
  for(i = 0;i < len;++i)
    if(latin1[i] == '\0' || buf[i] != latin1[i])
      return FALSE;
  
  return latin1[len] == '\0';
}

/*============================================================================*/

PMATH_API
pmath_string_t pmath_string_from_native(
  const char    *str,
  int            len
){
  struct _pmath_string_t *result;
  size_t inbytesleft;
  size_t outbytesleft;
  char *inbuf;
  char *outbuf;
  
  if(!str)
    return NULL;
  
  if(len < 0)
    len = strlen(str);
  
  result = _pmath_new_string_buffer(len);
  
  inbytesleft = (size_t)len;
  outbytesleft = sizeof(uint16_t) * (size_t)len;
  inbuf  = (char*)str;
  outbuf = (char*)AFTER_STRING(result);
  
  if(!result)
    return NULL;

  while(inbytesleft > 0){
    size_t ret = iconv(from_native, &inbuf, &inbytesleft, &outbuf, &outbytesleft);
    
    if(ret == (size_t)(-1)){
      pmath_unref((pmath_string_t)result);
      return NULL;
    }
  }
  
  result->length = ((size_t)outbuf - (size_t)AFTER_STRING(result))/2;
  return (pmath_string_t)result;
}

PMATH_API 
char *pmath_string_to_native(pmath_string_t str, int *result_len){
  int len = pmath_string_length(str);
  const uint16_t *buf = pmath_string_buffer(str);

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
    PMATH_TYPE_SHIFT_STRING,
    (pmath_compare_func_t)        compare_strings,
    (pmath_hash_func_t)           hash_string,
    (pmath_proc_t)                destroy_string,
    (pmath_equal_func_t)          equal_strings,
    (_pmath_object_write_func_t)  write_string/*,
    (pmath_proc_t)                write_string_boxes*/);
  
  to_utf8 = iconv_open(
    "UTF-8",
    PMATH_BYTE_ORDER < 0 ? "UTF-16LE" : "UTF-16BE");
  
  if(to_utf8 == (iconv_t)-1){
    return FALSE;
  }
  
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
