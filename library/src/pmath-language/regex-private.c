#include <pmath-language/regex-private.h>

#include <pmath-core/numbers.h>

#include <pmath-util/emit-and-gather.h>
#include <pmath-util/debug.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/hash/hashtables-private.h>
#include <pmath-util/helpers.h>
#include <pmath-util/memory.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/lists-private.h>
#include <pmath-language/patterns-private.h>

#include <pmath-util/concurrency/atomic-private.h>
#include <pmath-util/concurrency/threads.h> // for pmath_aborting()

#include <limits.h>
#include <stdio.h>
#include <string.h>

#include <pcre.h>


#ifdef _MSC_VER
#  define snprintf sprintf_s
#endif

#define SUBPATTERN_PREFIX "sub"

struct _callout_t {
  struct _callout_t *next;
  
  pmath_t expr;
  int pattern_position;
};

struct _regex_key_t {
  pmath_t  object;
  int      pcre_options;
};

struct _regex_t {
  pmath_atomic_t       refcount;
  
  struct _regex_key_t  key;
  
  pcre16              *code;    // compiled pattern
  pmath_string_t       pattern; // for debugging only
  struct _callout_t   *callouts;
  pmath_hashtable_t    named_subpatterns; // of pmath_ht_obj_int_class
};

extern pmath_symbol_t pmath_System_Alternatives;
extern pmath_symbol_t pmath_System_Condition;
extern pmath_symbol_t pmath_System_DigitCharacter;
extern pmath_symbol_t pmath_System_EndOfLine;
extern pmath_symbol_t pmath_System_EndOfString;
extern pmath_symbol_t pmath_System_Except;
extern pmath_symbol_t pmath_System_Function;
extern pmath_symbol_t pmath_System_Hold;
extern pmath_symbol_t pmath_System_LetterCharacter;
extern pmath_symbol_t pmath_System_List;
extern pmath_symbol_t pmath_System_Local;
extern pmath_symbol_t pmath_System_Longest;
extern pmath_symbol_t pmath_System_NumberString;
extern pmath_symbol_t pmath_System_Pattern;
extern pmath_symbol_t pmath_System_RegularExpression;
extern pmath_symbol_t pmath_System_Range;
extern pmath_symbol_t pmath_System_Repeated;
extern pmath_symbol_t pmath_System_Shortest;
extern pmath_symbol_t pmath_System_SingleMatch;
extern pmath_symbol_t pmath_System_StartOfLine;
extern pmath_symbol_t pmath_System_StartOfString;
extern pmath_symbol_t pmath_System_StringExpression;
extern pmath_symbol_t pmath_System_TestPattern;
extern pmath_symbol_t pmath_System_True;
extern pmath_symbol_t pmath_System_Whitespace;
extern pmath_symbol_t pmath_System_WhitespaceCharacter;
extern pmath_symbol_t pmath_System_With;
extern pmath_symbol_t pmath_System_WordBoundary;
extern pmath_symbol_t pmath_System_WordCharacter;

static void free_callouts(struct _callout_t *c) {
  while(c) {
    struct _callout_t *next = c->next;
    
    pmath_unref(c->expr);
    pmath_mem_free(c);
    
    c = next;
  }
}

/*----------------------------------------------------------------------------*/

PMATH_PRIVATE pmath_bool_t _pmath_regex_init_capture(
  const struct _regex_t *re,
  struct _capture_t     *c
) {
  assert(c != NULL);
  
  c->ovector = NULL;
  c->ovecsize = 0;
  c->capture_max = -1;
  
  if( re &&
      !pcre16_fullinfo(re->code, NULL, PCRE_INFO_CAPTURECOUNT, &(c->capture_max)))
  {
    c->ovecsize = 3 * (c->capture_max + 1);
    c->ovector = pmath_mem_alloc(sizeof(int) * c->ovecsize);
    
    if(c->ovector) {
      int i;
      for(i = 0; i < c->ovecsize; ++i)
        c->ovector[i] = -1;
        
      return TRUE;
    }
    
    c->ovecsize    = 0;
    c->capture_max = -1;
  }
  
  return FALSE;
}

PMATH_PRIVATE void _pmath_regex_free_capture(struct _capture_t *c) {
  assert(c != NULL);
  
  pmath_mem_free(c->ovector);
}

static pmath_string_t get_capture_by_name_id(
  const struct _regex_t  *re,
  struct _capture_t      *c,
  pmath_string_t          subject, // wont be freed
  int                     name_id // entry::value of regex::named_subpatterns
) {
  uint16_t *first;
  uint16_t *last;
  int entrylen;
  char s[20];
  uint16_t us[sizeof(s)];
  int i;
  
  assert(c != NULL);
  
  snprintf(s, sizeof(s), SUBPATTERN_PREFIX"%d", name_id);
  for(i = 0; i < (int)sizeof(s); ++i)
    us[i] = (unsigned char)s[i];
    
  entrylen = pcre16_get_stringtable_entries(re->code, us, &first, &last);
  if(entrylen <= 0)
    return pmath_string_new(0);
    
  while(first <= last) {
    //int capture_num = (int)(((uint16_t)first[0] << 8) | (uint16_t)first[1]);
    int capture_num = (int)first[0];
    
    capture_num *= 2;
    if(c->ovector[capture_num] >= 0 && c->ovector[capture_num + 1] >= 0) {
      return pmath_string_part(
               pmath_ref(subject),
               c->ovector[capture_num],
               c->ovector[capture_num + 1] - c->ovector[capture_num]);
    }
    
    first += entrylen;
  }
  
  return pmath_string_new(0);
}

static pmath_string_t get_capture_by_rhs( // PMATH_NULL if no capture was found
  const struct _regex_t  *re,
  struct _capture_t      *c,
  pmath_string_t          subject, // wont be freed
  pmath_string_t          rhs // wont be freed; a string of the form "$..."
) {
  const uint16_t *buf;
  uint16_t *name;
  int i, len, namecount, nameentrysize;
  int capture_num;
  
  assert(c != NULL);
  
  buf = pmath_string_buffer(&rhs);
  len = pmath_string_length(rhs);
  
  if(!re || len < 2 || buf[0] != '$')
    return PMATH_NULL;
    
  capture_num = 0;
  for(i = 1; i < len; ++i) {
    if(buf[i] >= '0' && buf[i] <= '9') {
      capture_num = 10 * capture_num + buf[i] - '0';
    }
    else {
      capture_num = -1;
      break;
    }
  }
  
  if( capture_num < 0                                                          &&
      pcre16_fullinfo(re->code, NULL, PCRE_INFO_NAMECOUNT,     &namecount)     &&
      pcre16_fullinfo(re->code, NULL, PCRE_INFO_NAMEENTRYSIZE, &nameentrysize) &&
      pcre16_fullinfo(re->code, NULL, PCRE_INFO_NAMETABLE,     &name)          &&
      len + 2 <= nameentrysize)
  {
    while(namecount > 0) {
      i = 0;
      while(i < len && name[1 + i] && name[1 + i] == buf[i])
        ++i;
        
      if(i == len && name[1 + i] == '\0') {
        capture_num = (int)name[0];
        
        if( c->ovector[2 * capture_num] >= 0 &&
            c->ovector[2 * capture_num + 1] >= 0)
        {
          break;
        }
      }
      
      name += nameentrysize / sizeof(name[0]);
      --namecount;
    }
  }
  
  if( capture_num < 0 ||
      capture_num > c->capture_max)
  {
    return PMATH_NULL;
  }
  
  capture_num *= 2;
  
  if( c->ovector[capture_num]     < 0 ||
      c->ovector[capture_num + 1] < 0)
  {
    return pmath_string_new(0);
  }
  
  return pmath_string_part(
           subject,
           c->ovector[capture_num],
           c->ovector[capture_num + 1] - c->ovector[capture_num]);
}

/*----------------------------------------------------------------------------*/

PMATH_PRIVATE struct _regex_t *_pmath_regex_ref(struct _regex_t *re) {
  if(re)
    (void)pmath_atomic_fetch_add(&re->refcount, 1);
  return re;
}

PMATH_PRIVATE void _pmath_regex_unref(struct _regex_t *re) {
  if(re) {
    pmath_atomic_barrier();
    if(1 == pmath_atomic_fetch_add(&re->refcount, -1)) { // was 1 -> is 0
      pmath_unref(re->key.object);
      pmath_unref(re->pattern);
      free_callouts(re->callouts);
      pmath_ht_destroy(re->named_subpatterns);
      
      if(re->code)
        pcre16_free(re->code);
      pmath_mem_free(re);
    }
    pmath_atomic_barrier();
  }
}

//{ hashtable ...

static unsigned int regex_hash(struct _regex_t *re) {
  return pmath_hash(re->key.object) ^ re->key.pcre_options;
}

static pmath_bool_t regex_equal(
  struct _regex_t *re1,
  struct _regex_t *re2
) {
  return pmath_equals(re1->key.object, re2->key.object);
}

static unsigned int regex_key_hash(struct _regex_key_t *key) {
  return pmath_hash(key->object) ^ key->pcre_options;
}

static pmath_bool_t regex_equals_key(
  struct _regex_t       *re,
  struct _regex_key_t   *key
) {
  return re->key.pcre_options == key->pcre_options &&
         pmath_equals(re->key.object, key->object);
}

static const pmath_ht_class_t regex_cache_ht_class = {
  (pmath_callback_t)                  _pmath_regex_unref,
  (pmath_ht_entry_hash_func_t)        regex_hash,
  (pmath_ht_entry_equal_func_t)       regex_equal,
  (pmath_ht_key_hash_func_t)          regex_key_hash,
  (pmath_ht_entry_equals_key_func_t)  regex_equals_key
};

// all four locked with regex_cache_[un]lock:
static pmath_atomic_t _global_regex_cache = PMATH_ATOMIC_STATIC_INIT;
#define REGEX_CACHE_ARRAY_SIZE  128
static struct _regex_t *regex_cache_array[REGEX_CACHE_ARRAY_SIZE];
static int regex_cache_array_next = 0;

//} ... hashtable

static pmath_hashtable_t regex_cache_lock(void) {
  return (pmath_hashtable_t)_pmath_atomic_lock_ptr(&_global_regex_cache);
}

static void regex_cache_unlock(pmath_hashtable_t table) {
  _pmath_atomic_unlock_ptr(&_global_regex_cache, table);
}

static struct _regex_t *create_regex(void) {
  struct _regex_t *result = pmath_mem_alloc(sizeof(struct _regex_t));
  
  if(result) {
    memset(result, 0, sizeof(struct _regex_t));
    pmath_atomic_write_release(&result->refcount, 1);
    result->pattern = PMATH_NULL;
  }
  
  return result;
}

static struct _regex_t *get_regex(pmath_t key, int pcre_options) { // key will be freed
  pmath_hashtable_t     table;
  struct _regex_key_t   k;
  struct _regex_t      *result;
  
  k.object       = key;
  k.pcre_options = pcre_options;
  
  table = regex_cache_lock();
  {
    result = _pmath_regex_ref((struct _regex_t *)pmath_ht_search(table, &k));
  }
  regex_cache_unlock(table);
  
  pmath_unref(key);
  return result;
}

static void store_regex(struct _regex_t *re) {
  pmath_hashtable_t table;
  struct _regex_t *arr_re;
  struct _regex_t *rem_re = NULL;
  
  table = regex_cache_lock();
  {
    arr_re = regex_cache_array[regex_cache_array_next];
    regex_cache_array[regex_cache_array_next] = _pmath_regex_ref(re);
    ++regex_cache_array_next;
    if(regex_cache_array_next >= REGEX_CACHE_ARRAY_SIZE)
      regex_cache_array_next = 0;
    
    if(arr_re)
      rem_re = (struct _regex_t *)pmath_ht_remove(table, &arr_re->key);
      
    re = (struct _regex_t *)pmath_ht_insert(table, re);
  }
  regex_cache_unlock(table);
  
  _pmath_regex_unref(re);
  _pmath_regex_unref(arr_re);
  _pmath_regex_unref(rem_re);
}

/*----------------------------------------------------------------------------*/

static pmath_bool_t is_pcre_metachar(uint16_t ch) {
  return ch == '\\' ||
         ch == '^'  ||
         ch == '$'  ||
         ch == '.'  ||
         ch == '['  ||
         ch == '|'  ||
         ch == '('  ||
         ch == ')'  ||
         ch == '?'  ||
         ch == '*'  ||
         ch == '+'  ||
         ch == '{';
}

static pmath_bool_t is_pcre_class_metachar(uint16_t ch) {
  return ch == '\\' ||
         ch == '^'  ||
         ch == '-'  ||
         ch == '['  ||
         ch == ']';
}

struct compile_regex_info_t {
  pmath_string_t pattern;
  
  pmath_t all;
  
  struct _callout_t *callouts;
  
  int current_subpattern;
  pmath_hashtable_t subpatterns; // of pmath_ht_obj_int_class
  
  pmath_bool_t shortest;
};

// 0 = not found
static int get_subpattern(
  struct compile_regex_info_t  *info,
  pmath_t                       name  // will be freed
) {
  struct _pmath_object_int_entry_t *entry;
  
  entry = pmath_ht_search(info->subpatterns, &name);
  
  pmath_unref(name);
  if(entry)
    return entry->value;
    
  return 0;
}

static int new_subpattern(
  struct compile_regex_info_t  *info,
  pmath_t                       name  // will be freed
) {
  struct _pmath_object_int_entry_t *entry = pmath_mem_alloc(
        sizeof(struct _pmath_object_int_entry_t));
        
  if(entry) {
    int result;
    info->current_subpattern++;
    entry->key = name;
    entry->value = result = info->current_subpattern;
    
    entry = pmath_ht_insert(info->subpatterns, entry);
    
    if(entry)
      pmath_ht_obj_int_class.entry_destructor(info->subpatterns, entry);
      
    return result;
  }
  
  pmath_unref(name);
  return info->current_subpattern++;
}

static void add_callout(
  struct compile_regex_info_t  *info,
  pmath_t                       expr  // will be freed
) {
  struct _callout_t *c = pmath_mem_alloc(sizeof(struct _callout_t));
  
  if(c) {
    info->pattern = pmath_string_insert_latin1(info->pattern, INT_MAX, "(?C)", -1);
    
    c->next = info->callouts;
    
    c->expr = expr;
    c->pattern_position = pmath_string_length(info->pattern);
    
    info->callouts = c;
  }
  else
    pmath_unref(expr);
}

static pmath_bool_t is_charclass_item(pmath_t obj) {
  if(pmath_is_string(obj)) {
    return pmath_string_length(obj) == 1;
  }
  
  if(pmath_is_expr_of_len(obj, pmath_System_Range, 2)) {
    pmath_t start = pmath_expr_get_item(obj, 1);
    pmath_t end   = pmath_expr_get_item(obj, 2);
    
    if( pmath_is_string(start)          &&
        pmath_is_string(end)            &&
        pmath_string_length(start) == 1 &&
        pmath_string_length(end)   == 1)
    {
      pmath_unref(start);
      pmath_unref(end);
      return TRUE;
    }
    
    pmath_unref(start);
    pmath_unref(end);
    return FALSE;
  }
  
  return pmath_same(obj, pmath_System_DigitCharacter)      ||
         pmath_same(obj, pmath_System_LetterCharacter)     ||
         pmath_same(obj, pmath_System_WhitespaceCharacter) ||
         pmath_same(obj, pmath_System_WordCharacter);
}

static void append_latin1(pmath_string_t *str, const char *s) {
  *str = pmath_string_insert_latin1(*str, INT_MAX, s, -1);
}

static void append_str_part(pmath_string_t *str, pmath_string_t s, int start, int len) { // s wont be freed
  assert(start >= 0);
  assert(len >= 0);
  assert(start <= pmath_string_length(s));
  assert(len <= pmath_string_length(s) - start);
  
  if(pmath_is_null(*str)) {
    *str = pmath_string_part(pmath_ref(s), start, len);
  }
  else {
    const uint16_t *buf = pmath_string_buffer(&s);
    
    *str = pmath_string_insert_ucs2(*str, INT_MAX, buf + start, len);
  }
}

static void put_charclass_item(
  struct compile_regex_info_t  *info,
  pmath_t                       obj   // will be freed
) {
  if(pmath_is_string(obj) && pmath_string_length(obj) == 1) {
    const uint16_t *buf = pmath_string_buffer(&obj);
    
    if(is_pcre_class_metachar(*buf))
      append_latin1(&info->pattern, "\\");
    info->pattern = pmath_string_concat(info->pattern, obj);
    return;
  }
  
  if(pmath_is_expr_of_len(obj, pmath_System_Range, 2)) {
    pmath_t start = pmath_expr_get_item(obj, 1);
    pmath_t end   = pmath_expr_get_item(obj, 2);
    
    if( pmath_is_string(start)          &&
        pmath_is_string(end)            &&
        pmath_string_length(start) == 1 &&
        pmath_string_length(end)   == 1)
    {
      const uint16_t *s = pmath_string_buffer(&start);
      const uint16_t *e = pmath_string_buffer(&end);
      
      if(is_pcre_class_metachar(*s))
        append_latin1(&info->pattern, "\\");
      info->pattern = pmath_string_concat(info->pattern, start);
      
      append_latin1(&info->pattern, "-");
      
      if(is_pcre_class_metachar(*e))
        append_latin1(&info->pattern, "\\");
      info->pattern = pmath_string_concat(info->pattern, end);
    }
    else {
      pmath_unref(start);
      pmath_unref(end);
    }
    
    pmath_unref(obj);
    return;
  }
  
  if(pmath_same(obj, pmath_System_DigitCharacter)) {
    append_latin1(&info->pattern, "\\d");
    pmath_unref(obj);
    return;
  }
  
  if(pmath_same(obj, pmath_System_LetterCharacter)) {
    append_latin1(&info->pattern, "a-zA-Z");
    pmath_unref(obj);
    return;
  }
  
  if(pmath_same(obj, pmath_System_WhitespaceCharacter)) {
    append_latin1(&info->pattern, "\\s");
    pmath_unref(obj);
    return;
  }
  
  if(pmath_same(obj, pmath_System_WordCharacter)) {
    append_latin1(&info->pattern, "\\w");
    pmath_unref(obj);
    return;
  }
  
  pmath_debug_print_object("[not a regex charclass item: ", obj, "]\n");
  
  pmath_unref(obj);
}

static pmath_bool_t compile_regex_part(
  struct compile_regex_info_t *info,
  pmath_t part                         // will be freed
) {
  if(pmath_is_string(part)) {
    const uint16_t *buf;
    int i, j, len;
    
//    pmath_cstr_writer_info_t u8info;
//
//    u8info._pmath_write_cstr = (void( *)(void *, const char *))concat_utf8;
//    u8info.user = &(info->pattern);

    buf = pmath_string_buffer(&part);
    len = pmath_string_length(part);
    
    i = 0;
    while(i < len) {
      j = i;
      while(j < len && buf[j] && !is_pcre_metachar(buf[j]))
        ++j;
        
      append_str_part(&info->pattern, part, i, j - i);
      if(j < len) {
        if(buf[j]) {
          append_latin1(&info->pattern, "\\");
          append_str_part(&info->pattern, part, j, 1);
        }
        else {
          append_latin1(&info->pattern, "\\x00");
        }
        i = j + 1;
      }
      else
        i = j;
    }
    
    pmath_unref(part);
    return TRUE;
  }
  
  if(pmath_is_expr(part)) {
    size_t len;
    pmath_t head;
    
    len = pmath_expr_length(part);
    head = pmath_expr_get_item(part, 0);
    pmath_unref(head);
    
    if(pmath_same(head, pmath_System_StringExpression)) {
      pmath_bool_t result = TRUE;
      size_t i;
      
      for(i = 1; i <= len && result; ++i) {
        result = compile_regex_part(info, pmath_expr_get_item(part, i));
      }
      
      pmath_unref(part);
      return result;
    }
    
    if(len == 2 && pmath_same(head, pmath_System_Pattern)) {
      char s[20];
      pmath_bool_t result = TRUE;
      
      int sub = get_subpattern(info, pmath_expr_get_item(part, 1));
      if(sub == 0) {
        sub = new_subpattern(info, pmath_expr_get_item(part, 1));
        
        snprintf(s, sizeof(s), "(?<"SUBPATTERN_PREFIX"%d>", sub);
        append_latin1(&info->pattern, s);
        
        result = compile_regex_part(info, pmath_expr_get_item(part, 2));
        
        append_latin1(&info->pattern, ")");
      }
      else {
        snprintf(s, sizeof(s), "\\g{"SUBPATTERN_PREFIX"%d}", sub);
        append_latin1(&info->pattern, s);
      }
      
      pmath_unref(part);
      return result;
    }
    
    if( len > 0 &&
        (pmath_same(head, pmath_System_List) ||
         pmath_same(head, pmath_System_Alternatives)))
    {
      pmath_bool_t result = TRUE;
      size_t i;
      
      for(i = 1; i <= len && result; ++i) {
        pmath_t item = pmath_expr_get_item(part, i);
        
        result = is_charclass_item(item);
        
        pmath_unref(item);
      }
      
      if(result) {
        append_latin1(&info->pattern, "[");
        
        for(i = 1; i <= len; ++i) {
          put_charclass_item(
            info,
            pmath_expr_get_item(part, i));
        }
        
        append_latin1(&info->pattern, "]");
        pmath_unref(part);
        return TRUE;
      }
      
      result = TRUE;
      
      append_latin1(&info->pattern, "(?:");
      for(i = 1; i <= len && result; ++i) {
        if(i > 1)
          append_latin1(&info->pattern, "|");
          
        result = compile_regex_part(info, pmath_expr_get_item(part, i));
      }
      append_latin1(&info->pattern, ")");
      
      pmath_unref(part);
      return result;
    }
    
    if((len == 1 || len == 2) && pmath_same(head, pmath_System_Except)) { // Except(p)  or  Except(p, c)
      pmath_bool_t result = TRUE;
      
      if(len > 1)
        append_latin1(&info->pattern, "(?=");
      
      {
        pmath_t p = pmath_expr_get_item(part, 1);
        if( pmath_is_expr_of(p, pmath_System_List) ||
            pmath_is_expr_of(p, pmath_System_Alternatives))
        {
          size_t i, plen;
          
          plen = pmath_expr_length(p);
          
          for(i = 1; i <= plen && result; ++i) {
            pmath_t item = pmath_expr_get_item(p, i);
            
            result = is_charclass_item(item);
            
            pmath_unref(item);
          }
          
          if(result && plen > 0) {
            append_latin1(&info->pattern, "[^");
            
            for(i = 1; i <= plen; ++i) {
              put_charclass_item(
                info,
                pmath_expr_get_item(p, i));
            }
            
            append_latin1(&info->pattern, "]");
          }
        }
        else if(is_charclass_item(p)) {
          append_latin1(&info->pattern, "[^");
          put_charclass_item(info, p); 
          p = PMATH_NULL;
          append_latin1(&info->pattern, "]");
        }
        else
          result = FALSE;
        
        pmath_unref(p);
      }
      
      if(result && len > 1) {
        append_latin1(&info->pattern, ")");
        
        result = compile_regex_part(info, pmath_expr_get_item(part, 2));
      }
      
      pmath_unref(part);
      return result;
    }
    
    if(len == 0 && pmath_same(head, pmath_System_SingleMatch)) {
      append_latin1(&info->pattern, "(?:(?s).)");
      pmath_unref(part);
      return TRUE;
    }
    
    if(len == 2 && pmath_same(head, pmath_System_Repeated)) {
      pmath_t range = pmath_expr_get_item(part, 2);
      
      size_t min = 1;
      size_t max = SIZE_MAX;
      
      if( extract_range(range, &min, &max, TRUE) &&
          (int)min >= 0 && ((int)max >= 0 || max == SIZE_MAX))
      {
        pmath_bool_t result;
        pmath_bool_t old_shortest = info->shortest;
        
        info->shortest = FALSE;
        
        append_latin1(&info->pattern, "(?:");
        result = compile_regex_part(info, pmath_expr_get_item(part, 1));
        append_latin1(&info->pattern, ")");
        
        if(max == SIZE_MAX) {
          char s[100];
          
          snprintf(s, sizeof(s), "{%d,}", (int)min);
          
          append_latin1(&info->pattern, s);
        }
        else {
          char s[100];
          
          snprintf(s, sizeof(s), "{%d,%d}", (int)min, (int)max);
          
          append_latin1(&info->pattern, s);
        }
        
        if(old_shortest) {
          append_latin1(&info->pattern, "?");
          info->shortest = TRUE;
        }
        // else: append "!" for explicit longest?
        
        pmath_unref(range);
        pmath_unref(part);
        return result;
      }
      
      pmath_unref(range);
    }
    
    if(len == 1 && pmath_same(head, pmath_System_Longest)) {
      pmath_bool_t result;
      pmath_bool_t old_shortest = info->shortest;
      
      info->shortest = FALSE;
      result = compile_regex_part(
                 info, pmath_expr_get_item(part, 1));
      info->shortest = old_shortest;
      
      pmath_unref(part);
      return result;
    }
    
    if(len == 1 && pmath_same(head, pmath_System_Shortest)) {
      pmath_bool_t result;
      pmath_bool_t old_shortest = info->shortest;
      
      info->shortest = TRUE;
      result = compile_regex_part(
                 info, pmath_expr_get_item(part, 1));
      info->shortest = old_shortest;
      
      pmath_unref(part);
      return result;
    }
    
    if(len == 2 && pmath_same(head, pmath_System_TestPattern)) {
      pmath_bool_t result;
      pmath_t name;
      char s[20];
      int sub;
      
      name = pmath_symbol_create_temporary(
               PMATH_C_STRING("System`Private`regex"), TRUE);
               
      sub = new_subpattern(info, pmath_ref(name));
      
      snprintf(s, sizeof(s), "(?<"SUBPATTERN_PREFIX"%d>", sub);
      append_latin1(&info->pattern, s);
      
      result = compile_regex_part(info, pmath_expr_get_item(part, 1));
      
      append_latin1(&info->pattern, ")");
      
      add_callout(
        info,
        pmath_expr_new_extended(
          pmath_expr_get_item(part, 2), 1,
          name));
          
      pmath_unref(part);
      return result;
    }
    
    if(len == 2 && pmath_same(head, pmath_System_Condition)) {
      pmath_bool_t result;
      
      result = compile_regex_part(info, pmath_expr_get_item(part, 1));
      
      add_callout(info, pmath_expr_get_item(part, 2));
      
      pmath_unref(part);
      return result;
    }
    
    if(len == 1 && pmath_same(head, pmath_System_RegularExpression)) {
      pmath_t str = pmath_expr_get_item(part, 1);
      
      if(pmath_is_string(str)) {
//        pmath_cstr_writer_info_t u8info;
//
//        u8info._pmath_write_cstr = (void( *)(void *, const char *))concat_utf8;
//        u8info.user = &(info->pattern);

        append_latin1(&info->pattern, "(?:");
        info->pattern = pmath_string_concat(info->pattern, str);
        append_latin1(&info->pattern, ")");
        
        pmath_unref(part);
        return TRUE;
      }
      
      pmath_unref(str);
    }
  }
  
  if(is_charclass_item(part)) {
    append_latin1(&info->pattern, "[");
    
    put_charclass_item(
      info,
      part);
      
    append_latin1(&info->pattern, "]");
    return TRUE;
  }
  
  if(pmath_is_symbol(part)) {
    if(pmath_same(part, pmath_System_StartOfString)) {
      pmath_unref(part);
      append_latin1(&info->pattern, "\\A");
      return TRUE;
    }
    
    if(pmath_same(part, pmath_System_EndOfString)) {
      pmath_unref(part);
      append_latin1(&info->pattern, "\\z");
      return TRUE;
    }
    
    if(pmath_same(part, pmath_System_StartOfLine)) {
      pmath_unref(part);
      append_latin1(&info->pattern, "^");
      return TRUE;
    }
    
    if(pmath_same(part, pmath_System_EndOfLine)) {
      pmath_unref(part);
      append_latin1(&info->pattern, "$");
      return TRUE;
    }
    
    if(pmath_same(part, pmath_System_NumberString)) {
      pmath_unref(part);
      append_latin1(&info->pattern, "\\d+\\.\\d");
      return TRUE;
    }
    
    if(pmath_same(part, pmath_System_Whitespace)) {
      pmath_unref(part);
      append_latin1(&info->pattern, "\\s+");
      return TRUE;
    }
    
    if(pmath_same(part, pmath_System_WordBoundary)) {
      pmath_unref(part);
      append_latin1(&info->pattern, "\\b");
      return TRUE;
    }
    
//    if(pmath_same(part, pmath_System_LetterCharacter)){
//      pmath_unref(part);
//      append_latin1(&info->pattern, "[^\\W\\d]");
//      return TRUE;
//    }
  }
  
  pmath_message(pmath_System_StringExpression, "invld", 2,
                part,
                pmath_ref(info->all));
  return FALSE;
}

static struct _regex_t *compile_regex(pmath_t obj, int pcre_options) {
  struct compile_regex_info_t info;
  struct _regex_t *regex = create_regex();
  pmath_bool_t ok;
  
  if(!regex) {
    pmath_unref(obj);
    return NULL;
  }
  
  memset(&info, 0, sizeof(info));
  
  info.all = pmath_ref(obj);
  
  info.subpatterns = pmath_ht_create_ex(&pmath_ht_obj_int_class, 0);
  if(!info.subpatterns) {
    _pmath_regex_unref(regex);
    return NULL;
  }
  
  info.pattern = PMATH_NULL;
  ok = compile_regex_part(&info, obj);
  regex->callouts = info.callouts;
  
  if(pmath_ht_count(info.subpatterns) == 0)
    pmath_ht_destroy(info.subpatterns);
  else
    regex->named_subpatterns = info.subpatterns;
    
  regex->key.object       = info.all;
  regex->key.pcre_options = pcre_options &~PCRE_UTF16 &~PCRE_NO_UTF16_CHECK;
  
  if(pmath_is_null(info.pattern) && pmath_aborting()) {
    _pmath_regex_unref(regex);
    return NULL;
  }
  
  info.pattern = pmath_string_insert_latin1(info.pattern, INT_MAX, "", 1); // 0-termination
  
  if(!ok || pmath_is_null(info.pattern)) {
    _pmath_regex_unref(regex);
    pmath_unref(info.pattern);
    return NULL;
  }
  
  {
    int errorcode, erroffset;
    const char *err;
    
    regex->pattern = info.pattern; info.pattern = PMATH_NULL;
    regex->code = pcre16_compile2(
                    pmath_string_buffer(&regex->pattern),
                    pcre_options | PCRE_UTF16 | PCRE_NO_UTF16_CHECK,
                    &errorcode,
                    &err,
                    &erroffset,
                    NULL);
                    
    if(!regex->code) {
      char msg[20];
      snprintf(msg, sizeof(msg), "msg%d", errorcode);
      
      pmath_message(pmath_System_RegularExpression, msg, 1,
                    pmath_ref(regex->key.object));
                    
      _pmath_regex_unref(regex);
      return NULL;
    }
  }
  
  pmath_unref(info.pattern);
  return regex;
}

PMATH_PRIVATE struct _regex_t *_pmath_regex_compile(
  pmath_t obj,
  int     pcre_options
) {
  struct _regex_t *re = get_regex(pmath_ref(obj), pcre_options);
  
  if(!re) {
    re = compile_regex(obj, pcre_options);
    if(re)
      store_regex(_pmath_regex_ref(re));
  }
  else
    pmath_unref(obj);
    
  return re;
}

PMATH_PRIVATE pmath_t _pmath_regex_decode(struct _regex_t *regex) {
  if(!regex)
    return PMATH_NULL;
  
  pmath_string_t regex_pattern = pmath_ref(regex->pattern);
  
  pmath_gather_begin(PMATH_NULL);
  {
    int cap = pmath_ht_capacity(regex->named_subpatterns);

    for(int i = 0; i < cap; ++i) {
      struct _pmath_object_int_entry_t *entry;
      entry = pmath_ht_entry(regex->named_subpatterns, i);
      if(entry) {
        // {Hold(name), index}     TODO: Maybe instead  `HoldPattern(name) -> index` ?
        pmath_emit(
          pmath_expr_new_extended(pmath_ref(pmath_System_List), 2, 
            pmath_expr_new_extended(pmath_ref(pmath_System_Hold), 1, pmath_ref(entry->key)), 
            pmath_integer_new_siptr(entry->value)), 
          PMATH_NULL);
      }
    }
  }
  pmath_t named_subpatterns = pmath_gather_end();
  
  pmath_gather_begin(PMATH_NULL);
  {
    for(struct _callout_t *callout = regex->callouts; callout; callout = callout->next) {
      // {Hold(expr), pos}
      pmath_emit(
        pmath_expr_new_extended(pmath_ref(pmath_System_List), 2,
          pmath_expr_new_extended(pmath_ref(pmath_System_Hold), 1, pmath_ref(callout->expr)), 
          pmath_integer_new_sint(callout->pattern_position)),
        PMATH_NULL);
    }
  }
  pmath_t callouts = pmath_gather_end();
  
  return pmath_expr_new_extended(pmath_ref(pmath_System_List), 3, regex_pattern, named_subpatterns, callouts);
}

/*----------------------------------------------------------------------------*/

// replace symols that capture subpatterns by those subpatterns
static pmath_t replace_named_subpatterns(
  const struct _regex_t  *re,
  struct _capture_t      *c,
  pmath_string_t          subject,    // wont be freed
  pmath_t                 obj         // will be freed
) {
  struct _pmath_object_int_entry_t *entry;
  
  entry = pmath_ht_search(re->named_subpatterns, &obj);
  
  if(entry) {
    pmath_unref(obj);
    
    return get_capture_by_name_id(re, c, subject, entry->value);
  }
  
  if(pmath_is_expr(obj)) {
    pmath_t item;
    size_t i, len;
    
    item = pmath_expr_get_item(obj, 0);
    if( (pmath_same(item, pmath_System_Function) ||
         pmath_same(item, pmath_System_Local)    ||
         pmath_same(item, pmath_System_With)) &&
        pmath_expr_length(obj) > 1            &&
        _pmath_contains_any(obj, re->named_subpatterns))
    {
      pmath_unref(item);
      
      obj = _pmath_preprocess_local(obj);
    }
    else {
      item = replace_named_subpatterns(re, c, subject, item);
      
      obj = pmath_expr_set_item(obj, 0, item);
    }
    
    len = pmath_expr_length(obj);
    for(i = 1; i <= len; ++i) {
      item = pmath_expr_extract_item(obj, i);
      
      item = replace_named_subpatterns(re, c, subject, item);
      
      obj = pmath_expr_set_item(obj, i, item);
    }
  }
  
  return obj;
}

// replace "$..." by the appropriate subpattern, ignore head of expressions
static pmath_t replace_string_subpatterns(
  struct _regex_t    *re,
  struct _capture_t  *c,
  pmath_string_t      subject,    // wont be freed
  pmath_t             obj         // will be freed
) {
  if(pmath_is_string(obj)) {
    pmath_t res = get_capture_by_rhs(re, c, subject, obj);
    
    if(!pmath_is_null(res)) {
      pmath_unref(obj);
      return res;
    }
  }
  else if(pmath_is_expr(obj)) {
    size_t i;
    
    for(i = pmath_expr_length(obj); i > 0; --i) {
      pmath_t item = pmath_expr_extract_item(obj, i);
      
      item = replace_string_subpatterns(re, c, subject, item);
      
      obj = pmath_expr_set_item(obj, i, item);
    }
  }
  
  return obj;
}

struct _callout_data_t {
  struct _capture_t  *capture;
  struct _regex_t    *regex;
  
  pmath_string_t current_subject;
};

static int callout(pcre16_callout_block *block) {
  if( block->callout_number == 0 &&
      block->version >= 1        &&
      block->callout_data)
  {
    struct _callout_t      *co;
    struct _callout_data_t *data = (struct _callout_data_t *)block->callout_data;
    
    co = data->regex->callouts;
    while(co && co->pattern_position != block->pattern_position)
      co = co->next;
      
    if(co) {
      pmath_t test = pmath_ref(co->expr);
      
      assert(pmath_string_buffer(&data->current_subject) == block->subject);
      assert(pmath_string_length( data->current_subject) == block->subject_length);
      
      test = replace_named_subpatterns(
               data->regex,
               data->capture,
               data->current_subject,
               test);
               
      test = pmath_evaluate(test);
      pmath_unref(test);
      
      if(pmath_same(test, pmath_System_True))
        return 0;
        
      return 1; // try other match
    }
  }
  
  return PCRE_ERROR_CALLOUT;
}

/*----------------------------------------------------------------------------*/

PMATH_PRIVATE pmath_bool_t _pmath_regex_match(
  struct _regex_t    *regex,
  pmath_string_t      subject,
  int                 subject_offset,
  int                 pcre_options,
  struct _capture_t  *capture,        // must be initialized with _pmath_regex_init_capture()
  pmath_t            *rhs             // optional
) {
  struct _callout_data_t  data;
  pcre16_extra            extra;
  int                     result;
  
  if(pmath_is_null(subject))
    return FALSE;
    
  assert(pmath_is_string(subject));
  
  data.capture         = capture;
  data.regex           = regex;
  data.current_subject = subject;
  
  if(!capture->ovector || !regex)
    return FALSE;
    
  memset(&extra, 0, sizeof(extra));
  extra.flags        = PCRE_EXTRA_CALLOUT_DATA;
  extra.callout_data = &data;
  
  result = pcre16_exec(
             regex->code,
             &extra,
             pmath_string_buffer(&subject),
             pmath_string_length(subject),  // subject_length
             subject_offset,
             pcre_options,
             capture->ovector,
             capture->ovecsize);
             
  if(result >= 0) {
    if(rhs && !pmath_is_null(*rhs)) {
      if(pmath_is_expr_of(regex->key.object, pmath_System_RegularExpression))
        *rhs = replace_string_subpatterns(regex, capture, subject, *rhs);
      else
        *rhs = replace_named_subpatterns(regex, capture, subject, *rhs);
    }
    
    return TRUE;
  }
  
  return FALSE;
}

/*----------------------------------------------------------------------------*/

PMATH_PRIVATE void _pmath_regex_memory_panic(void) {
  pmath_hashtable_t table = regex_cache_lock();
  {
    pmath_ht_clear(table);
  }
  regex_cache_unlock(table);
}

PMATH_PRIVATE pmath_bool_t _pmath_regex_init(void) {
  pmath_hashtable_t table = pmath_ht_create(&regex_cache_ht_class, 0);
  pmath_atomic_write_release(&_global_regex_cache, (intptr_t)table);
  memset(regex_cache_array, 0, sizeof(regex_cache_array));
  regex_cache_array_next = 0;
  
  pmath_debug_print("[pcre %s]\n", pcre16_version());
  
  pcre16_callout = callout;
  return table != NULL;
}

PMATH_PRIVATE void _pmath_regex_done(void) {
  int i;
  for(i = 0; i < REGEX_CACHE_ARRAY_SIZE; ++i)
    _pmath_regex_unref(regex_cache_array[i]);
    
  pmath_ht_destroy((pmath_hashtable_t)pmath_atomic_read_aquire(&_global_regex_cache));
}
