#ifndef __PMATH_CORE__STRINGS_PRIVATE_H__
#define __PMATH_CORE__STRINGS_PRIVATE_H__

#ifndef BUILDING_PMATH
  #error This header file is not part of the public pMath API
#endif

#include <pmath-core/objects-inline.h>

/* Strings are not zero-terminated.
   Strings can be a buffer themselves or point to a buffer. This reduces memory
   usage, because when reading a part of a string into a new string, the actual
   data is not copied. Instead, the new string's buffer field is set to the
   original string (or it's buffer if it is not PMATH_NULL).

   When a string with its reference counter > 1 is modified, it is
   copied first and the copy will be modified. So strings are copy-on-write as
   all pMath objects.
   
   The data of a string s with s->buffer==PMATH_NULL begins at AFTER_STRING(s)
 */

struct _pmath_string_t{
  struct _pmath_t          inherited;
  int                      length; // >= 0 !!!
  struct _pmath_string_t  *buffer;
  int                      capacity_or_start; // start index iff buffer != PMATH_NULL
};

PMATH_FORCE_INLINE size_t _pmath_next_2power_ui(size_t v){
  --v;
  v|= v >> 1;
  v|= v >> 2;
  v|= v >> 4;
  v|= v >> 8;
  v|= v >> 16;
#if PMATH_BITSIZE > 32
  v|= v >> 32;
#endif
  return v+1;
}

#define ROUND_UP(x, r)   ((((x) + (r) - 1) / (r)) * (r))
#define STRING_HEADER_SIZE (ROUND_UP(sizeof(struct _pmath_string_t), sizeof(size_t)))
#define AFTER_STRING(s) ((uint16_t*)(STRING_HEADER_SIZE + (char*)(s)))
//#define LENGTH_TO_CAPACITY(length) ((length) <= 0xFFF ? ROUND_UP(2*(length),sizeof(size_t)) : ROUND_UP((length),sizeof(size_t)))
#define LENGTH_TO_CAPACITY(length)  (((size_t)(length) & ~((size_t)0xFFF)) + _pmath_next_2power_ui(((length) & 0xFFF) + STRING_HEADER_SIZE / sizeof(uint16_t)) - STRING_HEADER_SIZE / sizeof(uint16_t))

#define UCS2_CHAR(chr) ((uint16_t)(chr))

PMATH_PRIVATE 
PMATH_ATTRIBUTE_USE_RESULT
struct _pmath_string_t *_pmath_new_string_buffer(int size);

PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
pmath_t _pmath_from_buffer(struct _pmath_string_t *b);

PMATH_PRIVATE
pmath_bool_t _pmath_strings_equal(
  pmath_t strA,
  pmath_t strB);

PMATH_PRIVATE 
int _pmath_strings_compare(
  pmath_t strA,
  pmath_t strB);

PMATH_PRIVATE 
void _pmath_string_write(struct pmath_write_ex_t *info, pmath_t str);

PMATH_PRIVATE void write_cstr(
  const char  *str, 
  void       (*write_ucs2)(void*,const uint16_t*,int),
  void        *user);

static PMATH_INLINE void write_unichar(
  uint16_t   ch,
  void     (*write_ucs2)(void*,const uint16_t*,int),
  void      *user
){
  write_ucs2(user, &ch, 1);
}

PMATH_PRIVATE pmath_bool_t _pmath_strings_init(void);
PMATH_PRIVATE void         _pmath_strings_done(void);

#endif /* __PMATH_CORE__STRINGS_PRIVATE_H__ */
