#include <pmath-language/regex-private.h>

#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/memory.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/lists-private.h>
#include <pmath-language/patterns-private.h>

#include <pmath-util/concurrency/atomic-private.h>

#include <stdio.h>
#include <string.h>

#define PCRE_STATIC
#include <pcre.h>


#ifdef _MSC_VER
  #define snprintf sprintf_s
#endif

#define SUBPATTERN_PREFIX "sub"

struct _callout_t{
  struct _callout_t *next;
  
  pmath_t expr;
  int pattern_position;
};

struct _regex_key_t{
  pmath_t  object;
  int      pcre_options;
};

struct _regex_t{
  PMATH_DECLARE_ATOMIC(refcount);
  
  struct _regex_key_t key;
  
  pcre                  *code;
  struct _callout_t     *callouts;
  pmath_hashtable_t  named_subpatterns;
};

static void free_callouts(struct _callout_t *c){
  while(c){
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
){
  assert(c != NULL);
  
  c->ovector = NULL;
  c->ovecsize = 0;
  c->capture_max = -1;
  
  if(re 
  && !pcre_fullinfo(re->code, NULL, PCRE_INFO_CAPTURECOUNT, &(c->capture_max))){
    c->ovecsize = 3 * (c->capture_max + 1);
    c->ovector = pmath_mem_alloc(sizeof(int) * c->ovecsize);
    
    if(c->ovector){
      int i;
      for(i = 0;i < c->ovecsize;++i)
        c->ovector[i] = -1;
      
      return TRUE;
    }
    
    c->ovecsize    = 0;
    c->capture_max = -1;
  }
  
  return FALSE;
}

PMATH_PRIVATE void _pmath_regex_free_capture(struct _capture_t *c){
  assert(c != NULL);
  
  pmath_mem_free(c->ovector);
}

static pmath_string_t get_capture_by_name_id(
  const struct _regex_t  *re,
  struct _capture_t      *c, 
  const char             *subject, 
  int                     name_id // entry::value of regex::named_subpatterns
){
  char *first;
  char *last;
  int entrylen;
  char s[20];
    
  assert(c != NULL);

  snprintf(s, sizeof(s), SUBPATTERN_PREFIX"%d", name_id);
  
  entrylen = pcre_get_stringtable_entries(re->code, s, &first, &last);
  if(entrylen < 3)
    return pmath_string_new(0);
    
  while(first <= last){
    int capture_num = (int)(((uint16_t)first[0] << 8) | (uint16_t)first[1]);
    
    capture_num*= 2;
    if(c->ovector[capture_num] >= 0 && c->ovector[capture_num + 1] >= 0){
      return pmath_string_from_utf8(
        subject + c->ovector[capture_num],
        c->ovector[capture_num + 1] - c->ovector[capture_num]);
    }
    
    first+= entrylen;
  }

  return pmath_string_new(0);
}

static pmath_string_t get_capture_by_rhs( // NULL if no capture was found
  const struct _regex_t  *re,
  struct _capture_t      *c, 
  const char             *subject, 
  pmath_string_t          rhs // wont be freed; a string of the form "$..."
){
  const uint16_t *buf;
  char *name;
  int i, len, namecount, nameentrysize;
  int capture_num;
  
  assert(c != NULL);
  
  buf = pmath_string_buffer(rhs);
  len = pmath_string_length(rhs);
  
  if(!re || len < 2 || buf[0] != '$')
    return NULL;
  
  capture_num = 0;
  for(i = 1;i < len;++i){
    if(buf[i] >= '0' && buf[i] <= '9'){
      capture_num = 10 * capture_num + buf[i] - '0';
    }
    else{
      capture_num = -1;
      break;
    }
  }
  
  if(capture_num < 0
  && pcre_fullinfo(re->code, NULL, PCRE_INFO_NAMECOUNT,     &namecount)
  && pcre_fullinfo(re->code, NULL, PCRE_INFO_NAMEENTRYSIZE, &nameentrysize)
  && pcre_fullinfo(re->code, NULL, PCRE_INFO_NAMETABLE,     &name)
  && len + 2 <= nameentrysize){
    while(namecount > 0){
      i = 0;
      while(i < len && name[2 + i] && name[2 + i] == buf[i])
        ++i;
      
      if(i == len && name[2 + i] == '\0'){
        capture_num = (int)(((uint16_t)name[0] << 8) | (uint16_t)name[1]);
        
        if(c->ovector[2 * capture_num] >= 0
        && c->ovector[2 * capture_num + 1] >= 0)
          break;
      }
      
      name+= namecount;
    }
  }
  
  if(capture_num < 0 
  || capture_num > c->capture_max)
    return NULL;
  
  capture_num*= 2;
  
  if(c->ovector[capture_num]     < 0
  || c->ovector[capture_num + 1] < 0)
    return pmath_string_new(0);
  
  return pmath_string_from_utf8(
    subject + c->ovector[capture_num],
    c->ovector[capture_num + 1] - c->ovector[capture_num]);
}

/*----------------------------------------------------------------------------*/

PMATH_PRIVATE struct _regex_t *_pmath_regex_ref(struct _regex_t *re){
  if(re)
    (void)pmath_atomic_fetch_add(&(re->refcount), 1);
  return re;
}

PMATH_PRIVATE void _pmath_regex_unref(struct _regex_t *re){
  if(re){
    pmath_atomic_barrier();
    if(1 == pmath_atomic_fetch_add(&(re->refcount), -1)){ // was 1 -> is 0
      pmath_unref(re->key.object);
      free_callouts(re->callouts);
      pmath_ht_destroy(re->named_subpatterns);
      
      if(re->code)
        pcre_free(re->code);
      pmath_mem_free(re);
    }
    pmath_atomic_barrier();
  }
}

//{ hashtable ...

static unsigned int regex_hash(struct _regex_t *re){
  return pmath_hash(re->key.object) ^ re->key.pcre_options;
}

static pmath_bool_t regex_equal(
  struct _regex_t *re1, 
  struct _regex_t *re2
){
  return pmath_equals(re1->key.object, re2->key.object);
}

static unsigned int regex_key_hash(struct _regex_key_t *key){
  return pmath_hash(key->object) ^ key->pcre_options;
}

static pmath_bool_t regex_equals_key(
  struct _regex_t       *re, 
  struct _regex_key_t   *key
){
  return re->key.pcre_options == key->pcre_options
    && pmath_equals(re->key.object, key->object);
}

static const pmath_ht_class_t regex_cache_ht_class = {
  (pmath_callback_t)                  _pmath_regex_unref,
  (pmath_ht_entry_hash_func_t)        regex_hash,
  (pmath_ht_entry_equal_func_t)       regex_equal,
  (pmath_ht_key_hash_func_t)          regex_key_hash,
  (pmath_ht_entry_equals_key_func_t)  regex_equals_key
};

// all four locked with regex_cache_[un]lock:
static void * volatile _global_regex_cache = NULL;
#define REGEX_CACHE_ARRAY_SIZE  128
static struct _regex_t *regex_cache_array[REGEX_CACHE_ARRAY_SIZE];
static int regex_cache_array_next = 0;

//} ... hashtable

static pmath_hashtable_t regex_cache_lock(void){
  return (pmath_hashtable_t)_pmath_atomic_lock_ptr(&_global_regex_cache);
}

static void regex_cache_unlock(pmath_hashtable_t table){
  _pmath_atomic_unlock_ptr(&_global_regex_cache, table);
}

static struct _regex_t *create_regex(void){
  struct _regex_t *result = pmath_mem_alloc(sizeof(struct _regex_t));
  
  if(result){
    memset(result, 0, sizeof(struct _regex_t));
    result->refcount = 1;
  }
  
  return result;
}

static struct _regex_t *get_regex(pmath_t key, int pcre_options){ // key will be freed
  pmath_hashtable_t     table;
  struct _regex_key_t   k;
  struct _regex_t      *result;
  
  k.object       = key;
  k.pcre_options = pcre_options;
  
  table = regex_cache_lock();
  {
    result = _pmath_regex_ref((struct _regex_t*)pmath_ht_search(table, &k));
  }
  regex_cache_unlock(table);
  
  pmath_unref(key);
  return result;
}

static void store_regex(struct _regex_t *re){
  pmath_hashtable_t table;
  struct _regex_t *arr_re;
  struct _regex_t *rem_re = NULL;
  
  table = regex_cache_lock();
  {
    arr_re = regex_cache_array[regex_cache_array_next];
    regex_cache_array[regex_cache_array_next] = _pmath_regex_ref(re);
    ++regex_cache_array_next;
    
    if(arr_re)
      rem_re = (struct _regex_t*)pmath_ht_remove(table, &arr_re->key);
    
    re = (struct _regex_t*)pmath_ht_insert(table, re);
  }
  regex_cache_unlock(table);
  
  _pmath_regex_unref(re);
  _pmath_regex_unref(arr_re);
  _pmath_regex_unref(rem_re);
}

/*----------------------------------------------------------------------------*/

  struct concat_t{
    char *buf;
    int len;
    int capacity;
  };
  
  static void concat_utf8(struct concat_t *cc, const char *str){
    int len = strlen(str);
    
    if(len + cc->len >= cc->capacity){
      size_t new_capacity = 256;
      char *newbuf;
      
      while((int)new_capacity <= len + cc->len && (int)new_capacity > 0)
        new_capacity*= 2;
      
      if((int)new_capacity <= 0)
        return;
        
      newbuf = pmath_mem_realloc_no_failfree(cc->buf, new_capacity);
      
      if(!newbuf)
        return;
        
      cc->buf = newbuf;
      cc->capacity = (int)new_capacity;
    }
    
    memcpy(cc->buf + cc->len, str, len);
    cc->len+= len;
    cc->buf[cc->len] = '\0';
  }

/*----------------------------------------------------------------------------*/

static pmath_bool_t is_pcre_metachar(uint16_t ch){
  return ch == '\\'
      || ch == '^'
      || ch == '$'
      || ch == '.'
      || ch == '['
      || ch == '|'
      || ch == '('
      || ch == ')'
      || ch == '?'
      || ch == '*'
      || ch == '+'
      || ch == '{';
}

static pmath_bool_t is_pcre_class_metachar(uint16_t ch){
  return ch == '\\'
      || ch == '^'
      || ch == '-'
      || ch == '['
      || ch == ']';
}

struct compile_regex_info_t{
  struct concat_t pattern;
  
  pmath_t all;
  
  struct _callout_t *callouts;
  
  int current_subpattern;
  pmath_hashtable_t subpatterns;
  
  pmath_bool_t shortest;
};
  
  // 0 = not found
  static int get_subpattern(
    struct compile_regex_info_t  *info, 
    pmath_t                name  // will be freed
  ){
    struct _pmath_object_int_entry_t *entry;
    
    entry = pmath_ht_search(info->subpatterns, name);
    
    pmath_unref(name);
    if(entry)
      return entry->value;
    
    return 0;
  }
  
  static int new_subpattern(
    struct compile_regex_info_t  *info, 
    pmath_t                name  // will be freed
  ){
    struct _pmath_object_int_entry_t *entry = pmath_mem_alloc(
      sizeof(struct _pmath_object_int_entry_t));
    
    if(entry){
      int result;
      info->current_subpattern++;
      entry->key = name;
      entry->value = result = info->current_subpattern;
      
      entry = pmath_ht_insert(info->subpatterns, entry);
      
      if(entry)
        pmath_ht_obj_int_class.entry_destructor(entry);
      
      return result;
    }
    
    pmath_unref(name);
    return info->current_subpattern++;
  }
  
  static void add_callout(
    struct compile_regex_info_t  *info, 
    pmath_t                expr  // will be freed
  ){
    struct _callout_t *c = pmath_mem_alloc(sizeof(struct _callout_t));
    
    if(c){
      concat_utf8(&(info->pattern), "(?C)");
      
      c->next = info->callouts;
      
      c->expr = expr;
      c->pattern_position = info->pattern.len;
      
      info->callouts = c;
    }
    else
      pmath_unref(expr);
  }
  
  static pmath_bool_t is_charclass_item(pmath_t obj){
    if(pmath_instance_of(obj, PMATH_TYPE_STRING)){
      return pmath_string_length(obj) == 1;
    }
    
    if(pmath_is_expr_of_len(obj, PMATH_SYMBOL_RANGE, 2)){
      pmath_t start = pmath_expr_get_item(obj, 1);
      pmath_t end   = pmath_expr_get_item(obj, 2);
      
      if(pmath_instance_of(start, PMATH_TYPE_STRING)
      && pmath_instance_of(end,   PMATH_TYPE_STRING)
      && pmath_string_length(start) == 1
      && pmath_string_length(end)   == 1){
        pmath_unref(start);
        pmath_unref(end);
        return TRUE;
      }
      
      pmath_unref(start);
      pmath_unref(end);
      return FALSE;
    }
    
    return obj == PMATH_SYMBOL_DIGITCHARACTER
        || obj == PMATH_SYMBOL_LETTERCHARACTER
        || obj == PMATH_SYMBOL_WHITESPACECHARACTER
        || obj == PMATH_SYMBOL_WORDCHARACTER;
  }
  
  static void put_charclass_item(
    struct compile_regex_info_t  *info, 
    pmath_t                       obj   // will be freed
  ){
    pmath_cstr_writer_info_t u8info;
    
    u8info.write_cstr = (void(*)(void*,const char*))concat_utf8;
    u8info.user = &(info->pattern);
    
    if(pmath_instance_of(obj, PMATH_TYPE_STRING)
    && pmath_string_length(obj) == 1){
      const uint16_t *buf = pmath_string_buffer(obj);
      
      if(is_pcre_class_metachar(*buf))
        concat_utf8(&(info->pattern), "\\");
        
      pmath_utf8_writer(&u8info, buf, 1);
    }
    else if(pmath_is_expr_of_len(obj, PMATH_SYMBOL_RANGE, 2)){
      pmath_t start = pmath_expr_get_item(obj, 1);
      pmath_t end   = pmath_expr_get_item(obj, 2);
      
      if(pmath_instance_of(start, PMATH_TYPE_STRING)
      && pmath_instance_of(end,   PMATH_TYPE_STRING)
      && pmath_string_length(start) == 1
      && pmath_string_length(end)   == 1){
        const uint16_t *s = pmath_string_buffer(start);
        const uint16_t *e = pmath_string_buffer(end);
        
        if(is_pcre_class_metachar(*s))
          concat_utf8(&(info->pattern), "\\");
        pmath_utf8_writer(&u8info, s, 1);
        
        concat_utf8(&(info->pattern), "-");
        
        if(is_pcre_class_metachar(*e))
          concat_utf8(&(info->pattern), "\\");
        pmath_utf8_writer(&u8info, e, 1);
      }
      pmath_unref(start);
      pmath_unref(end);
    }
    else if(obj == PMATH_SYMBOL_DIGITCHARACTER)
      concat_utf8(&(info->pattern), "\\d");
    else if(obj == PMATH_SYMBOL_LETTERCHARACTER)
      concat_utf8(&(info->pattern), "a-zA-Z");
    else if(obj == PMATH_SYMBOL_WHITESPACECHARACTER)
      concat_utf8(&(info->pattern), "\\s");
    else if(obj == PMATH_SYMBOL_WORDCHARACTER)
      concat_utf8(&(info->pattern), "\\w");
    
    pmath_unref(obj);
  }
  
static pmath_bool_t compile_regex_part(
  struct compile_regex_info_t *info,
  pmath_t part
){
  if(pmath_instance_of(part, PMATH_TYPE_STRING)){
    const uint16_t *buf;
    int i, j, len;
    
    pmath_cstr_writer_info_t u8info;
    
    u8info.write_cstr = (void(*)(void*,const char*))concat_utf8;
    u8info.user = &(info->pattern);
    
    buf = pmath_string_buffer(part);
    len = pmath_string_length(part);
    
    i = 0;
    while(i < len){
      j = i;
      while(j < len && buf[j] && !is_pcre_metachar(buf[j]))
        ++j;
      
      pmath_utf8_writer(&u8info, buf + i, j - i);
      if(j < len){
        if(buf[j]){
          concat_utf8(&(info->pattern), "\\");
          pmath_utf8_writer(&u8info, buf + j, 1);
        }
        else{
          concat_utf8(&(info->pattern), "\\x00");
        }
        i = j + 1;
      }
      else
        i = j;
    }
    
    pmath_unref(part);
    return TRUE;
  }
  
  if(pmath_instance_of(part, PMATH_TYPE_EXPRESSION)){
    size_t len;
    pmath_t head;
    
    len = pmath_expr_length(part);
    head = pmath_expr_get_item(part, 0);
    pmath_unref(head);
    
    if(head == PMATH_SYMBOL_STRINGEXPRESSION){
      pmath_bool_t result = TRUE;
      size_t i;
      
      for(i = 1;i <= len && result;++i){
        result = compile_regex_part(info, pmath_expr_get_item(part, i));
      }
      
      pmath_unref(part);
      return result;
    }
    
    if(head == PMATH_SYMBOL_PATTERN && len == 2){
      char s[20];
      pmath_bool_t result = TRUE;
      
      int sub = get_subpattern(info, pmath_expr_get_item(part, 1));
      if(sub == 0){
        sub = new_subpattern(info, pmath_expr_get_item(part, 1));
        
        snprintf(s, sizeof(s), "(?<"SUBPATTERN_PREFIX"%d>", sub);
        concat_utf8(&(info->pattern), s);
        
        result = compile_regex_part(info, pmath_expr_get_item(part, 2));
        
        concat_utf8(&(info->pattern), ")");
      }
      else{
        snprintf(s, sizeof(s), "\\g{"SUBPATTERN_PREFIX"%d}", sub);
        concat_utf8(&(info->pattern), s);
      }
      
      pmath_unref(part);
      return result;
    }
    
    if((head == PMATH_SYMBOL_LIST || head == PMATH_SYMBOL_ALTERNATIVES) && len > 0){
      pmath_bool_t result = TRUE;
      size_t i;
      
      for(i = 1;i <= len && result;++i){
        pmath_t item = pmath_expr_get_item(part, i);
        
        result = is_charclass_item(item);
        
        pmath_unref(item);
      }
      
      if(result){
        concat_utf8(&(info->pattern), "[");
        
        for(i = 1;i <= len;++i){
          put_charclass_item(
            info, 
            pmath_expr_get_item(part, i));
        }
        
        concat_utf8(&(info->pattern), "]");
        pmath_unref(part);
        return TRUE;
      }
      
      result = TRUE;
      
      concat_utf8(&(info->pattern), "(?:");
      for(i = 1;i <= len && result;++i){
        if(i > 1)
          concat_utf8(&(info->pattern), "|");
        
        result = compile_regex_part(info, pmath_expr_get_item(part, i));
      }
      concat_utf8(&(info->pattern), ")");
      
      pmath_unref(part);
      return result;
    }
    
    if(head == PMATH_SYMBOL_EXCEPT && len == 1){
      pmath_t p = pmath_expr_get_item(part, 1);
      
      if(pmath_is_expr_of(p, PMATH_SYMBOL_LIST) 
      || pmath_is_expr_of(p, PMATH_SYMBOL_ALTERNATIVES)){
        pmath_bool_t result = TRUE;
        size_t i, plen;
        
        plen = pmath_expr_length(p);
          
        for(i = 1;i <= plen && result;++i){
          pmath_t item = pmath_expr_get_item(p, i);
          
          result = is_charclass_item(item);
          
          pmath_unref(item);
        }
        
        if(result && plen > 0){
          concat_utf8(&(info->pattern), "[^");
          
          for(i = 1;i <= len;++i){
            put_charclass_item(
              info,
              pmath_expr_get_item(p, i));
          }
          
          concat_utf8(&(info->pattern), "]");
          pmath_unref(part);
          pmath_unref(p);
          return TRUE;
        }
      }
      else if(is_charclass_item(p)){
        concat_utf8(&(info->pattern), "[^");
        put_charclass_item(info, p);
        concat_utf8(&(info->pattern), "]");
        pmath_unref(part);
        return TRUE;
      }
      
      pmath_unref(p);
    }
    
    if(head == PMATH_SYMBOL_SINGLEMATCH && len == 0){
      concat_utf8(&(info->pattern), "(?:(?s).)");
      pmath_unref(part);
      return TRUE;
    }
    
    if(head == PMATH_SYMBOL_REPEATED && len == 2){
      pmath_t range = pmath_expr_get_item(part, 2);
      
      size_t min = 1;
      size_t max = SIZE_MAX;
      
      if(extract_range(range, &min, &max, TRUE)
      && (int)min >= 0 && ((int)max >= 0 || max == SIZE_MAX)){
        pmath_bool_t result;
        pmath_bool_t old_shortest = info->shortest;
        
        info->shortest = FALSE;
        
        concat_utf8(&(info->pattern), "(?:");
        result = compile_regex_part(info, pmath_expr_get_item(part, 1));
        concat_utf8(&(info->pattern), ")");
        
        if(max == SIZE_MAX){
          char s[100];
          
          snprintf(s, sizeof(s), "{%d,}",(int)min);
          
          concat_utf8(&(info->pattern), s);
        }
        else{
          char s[100];
          
          snprintf(s, sizeof(s), "{%d,%d}",(int)min,(int)max);
          
          concat_utf8(&(info->pattern), s);
        }
        
        if(old_shortest){
          concat_utf8(&(info->pattern), "?");
          info->shortest = TRUE;
        }
        
        pmath_unref(range);
        pmath_unref(part);
        return result;
      }
      
      pmath_unref(range);
    }
  
    if(head == PMATH_SYMBOL_LONGEST && len == 1){
      pmath_bool_t result;
      pmath_bool_t old_shortest = info->shortest;
      
      info->shortest = FALSE;
      result = compile_regex_part(
        info, pmath_expr_get_item(part, 1));
      info->shortest = old_shortest;
      
      pmath_unref(part);
      return result;
    }
    
    if(head == PMATH_SYMBOL_SHORTEST && len == 1){
      pmath_bool_t result;
      pmath_bool_t old_shortest = info->shortest;
      
      info->shortest = TRUE;
      result = compile_regex_part(
        info, pmath_expr_get_item(part, 1));
      info->shortest = old_shortest;
      
      pmath_unref(part);
      return result;
    }
    
    if(head == PMATH_SYMBOL_TESTPATTERN && len == 2){
      pmath_bool_t result;
      pmath_t name;
      char s[20];
      int sub;
      
      name = pmath_symbol_create_temporary(
        PMATH_C_STRING("System`Private`regex"), TRUE);
        
      sub = new_subpattern(info, pmath_ref(name));
      
      snprintf(s, sizeof(s), "(?<"SUBPATTERN_PREFIX"%d>", sub);
      concat_utf8(&(info->pattern), s);
      
      result = compile_regex_part(info, pmath_expr_get_item(part, 1));
      
      concat_utf8(&(info->pattern), ")");
      
      add_callout(
        info, 
        pmath_expr_new_extended(
          pmath_expr_get_item(part, 2), 1, 
          name));
      
      pmath_unref(part);
      return result;
    }
    
    if(head == PMATH_SYMBOL_CONDITION && len == 2){
      pmath_bool_t result;
      
      result = compile_regex_part(info, pmath_expr_get_item(part, 1));
      
      add_callout(info, pmath_expr_get_item(part, 2));
      
      pmath_unref(part);
      return result;
    }
    
    if(head == PMATH_SYMBOL_REGULAREXPRESSION && len == 1){
      pmath_t str = pmath_expr_get_item(part, 1);
      
      if(pmath_instance_of(str, PMATH_TYPE_STRING)){
        pmath_cstr_writer_info_t u8info;
        
        u8info.write_cstr = (void(*)(void*,const char*))concat_utf8;
        u8info.user = &(info->pattern);
        
        concat_utf8(&(info->pattern), "(?:");
        pmath_utf8_writer(
          &u8info, 
          pmath_string_buffer(str), 
          pmath_string_length(str));
        concat_utf8(&(info->pattern), ")");
        
        pmath_unref(part);
        pmath_unref(str);
        return TRUE;
      }
      
      pmath_unref(str);
    }
  }

  if(is_charclass_item(part)){
    concat_utf8(&(info->pattern), "[");
    
    put_charclass_item(
      info, 
      part);
    
    concat_utf8(&(info->pattern), "]");
    return TRUE;
  }
  
  if(pmath_instance_of(part, PMATH_TYPE_SYMBOL)){
    if(part == PMATH_SYMBOL_STARTOFSTRING){
      pmath_unref(part);
      concat_utf8(&(info->pattern), "\\A");
      return TRUE;
    }
    
    if(part == PMATH_SYMBOL_ENDOFSTRING){
      pmath_unref(part);
      concat_utf8(&(info->pattern), "\\z");
      return TRUE;
    }
    
    if(part == PMATH_SYMBOL_STARTOFLINE){
      pmath_unref(part);
      concat_utf8(&(info->pattern), "^");
      return TRUE;
    }
    
    if(part == PMATH_SYMBOL_ENDOFLINE){
      pmath_unref(part);
      concat_utf8(&(info->pattern), "$");
      return TRUE;
    }
    
    if(part == PMATH_SYMBOL_NUMBERSTRING){
      pmath_unref(part);
      concat_utf8(&(info->pattern), "\\d+\\.\\d");
      return TRUE;
    }
    
    if(part == PMATH_SYMBOL_WHITESPACE){
      pmath_unref(part);
      concat_utf8(&(info->pattern), "\\s+");
      return TRUE;
    }
    
    if(part == PMATH_SYMBOL_WORDBOUNDARY){
      pmath_unref(part);
      concat_utf8(&(info->pattern), "\\b");
      return TRUE;
    }
    
//    if(part == PMATH_SYMBOL_LETTERCHARACTER){
//      pmath_unref(part);
//      concat_utf8(&(info->pattern), "[^\\W\\d]");
//      return TRUE;
//    }
  }
  
  pmath_message(PMATH_SYMBOL_STRINGEXPRESSION, "invld", 2,
    part,
    pmath_ref(info->all));
  return FALSE;
}

static struct _regex_t *compile_regex(pmath_t obj, int pcre_options){
  struct compile_regex_info_t info;
  struct _regex_t *regex = create_regex();
  pmath_bool_t ok;
  
  if(!regex){
    pmath_unref(obj);
    return NULL;
  }
  
  memset(&info, 0, sizeof(info));
  
  info.all = pmath_ref(obj);
  
  info.subpatterns = pmath_ht_create(&pmath_ht_obj_int_class, 0);
  if(!info.subpatterns){
    _pmath_regex_unref(regex);
    return NULL;
  }
  
  concat_utf8(&(info.pattern), "");
  ok = compile_regex_part(&info, obj);
  regex->callouts = info.callouts;
  
  if(pmath_ht_count(info.subpatterns) == 0)
    pmath_ht_destroy(info.subpatterns);
  else
    regex->named_subpatterns = info.subpatterns;
  
  regex->key.object       = info.all;
  regex->key.pcre_options = pcre_options & ~PCRE_UTF8 & ~PCRE_NO_UTF8_CHECK;
  
  if(!ok || !info.pattern.buf){
    _pmath_regex_unref(regex);
    pmath_mem_free(info.pattern.buf);
    return NULL;
  }
  
  {
    int errorcode, erroffset;
    const char *err;
    
    regex->code = pcre_compile2(
      info.pattern.buf, 
      pcre_options | PCRE_UTF8 | PCRE_NO_UTF8_CHECK,
      &errorcode,
      &err,
      &erroffset,
      NULL);
    
    if(!regex->code){
      char msg[10];
      snprintf(msg, sizeof(msg), "msg%d", errorcode);
      
      pmath_message(PMATH_SYMBOL_REGULAREXPRESSION, msg, 1, 
        pmath_ref(regex->key.object));
      
      _pmath_regex_unref(regex);
      pmath_mem_free(info.pattern.buf);
      return NULL;
    }
  }
  
  pmath_mem_free(info.pattern.buf);
  return regex;
}

PMATH_PRIVATE struct _regex_t *_pmath_regex_compile(
  pmath_t obj, 
  int            pcre_options
){
  struct _regex_t *re = get_regex(pmath_ref(obj), pcre_options);
  
  if(!re){
    re = compile_regex(obj, pcre_options);
    if(re)
      store_regex(_pmath_regex_ref(re));
  }
  else
    pmath_unref(obj);
  
  return re;
}

/*----------------------------------------------------------------------------*/
  
  // replace symols that capture subpatterns by those subpatterns
  static pmath_t replace_named_subpatterns(
    const struct _regex_t  *re,
    struct _capture_t      *c,
    const char             *subject,
    pmath_t          obj         // will be freed
  ){
    struct _pmath_object_int_entry_t *entry;
    
    entry = pmath_ht_search(re->named_subpatterns, obj);
      
    if(entry){
      pmath_unref(obj);
      
      return get_capture_by_name_id(re, c, subject, entry->value);
    }
    
    if(pmath_instance_of(obj, PMATH_TYPE_EXPRESSION)){
      pmath_t item;
      size_t i, len;
      
      item = pmath_expr_get_item(obj, 0);
      if((item == PMATH_SYMBOL_FUNCTION
       || item == PMATH_SYMBOL_LOCAL
       || item == PMATH_SYMBOL_WITH)
      && pmath_expr_length(obj) > 1
      && _pmath_contains_any(obj, re->named_subpatterns)){
        pmath_unref(item);
        
        obj = _pmath_preprocess_local(obj);
      }
      else{
        item = replace_named_subpatterns(re, c, subject, item);
        
        obj = pmath_expr_set_item(obj, 0, item);
      }
      
      len = pmath_expr_length(obj);
      for(i = 1;i <= len;++i){
        item = pmath_expr_get_item(obj, i);
        
        if(obj->refcount == 1)
          obj = pmath_expr_set_item(obj, i, NULL);
          
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
    const char         *subject,
    pmath_t      obj         // will be freed
  ){
    if(pmath_instance_of(obj, PMATH_TYPE_STRING)){
      pmath_t res = get_capture_by_rhs(re, c, subject, obj);
      
      if(res){
        pmath_unref(obj);
        return res;
      }
    }
    else if(pmath_instance_of(obj, PMATH_TYPE_EXPRESSION)){
      size_t i;
      
      for(i = pmath_expr_length(obj);i > 0;--i){
        pmath_t item = pmath_expr_get_item(obj, i);
        
        if(obj->refcount == 1)
          obj = pmath_expr_set_item(obj, i, NULL);
          
        item = replace_string_subpatterns(re, c, subject, item);
          
        obj = pmath_expr_set_item(obj, i, item);
      }
    }
    
    return obj;
  }
  
struct _callout_data_t{
  struct _capture_t  *capture;
  struct _regex_t    *regex;
};

static int callout(pcre_callout_block *block){
  if(block->callout_number == 0 
  && block->version >= 1
  && block->callout_data){
    struct _callout_t      *co;
    struct _callout_data_t *data = (struct _callout_data_t*)block->callout_data;
    
    co = data->regex->callouts;
    while(co && co->pattern_position != block->pattern_position)
      co = co->next;
    
    if(co){
      pmath_t test = pmath_ref(co->expr);
      
      test = replace_named_subpatterns(
        data->regex,
        data->capture, 
        block->subject,
        test);
      
      test = pmath_evaluate(test);
      pmath_unref(test);
      
      if(test == PMATH_SYMBOL_TRUE)
        return 0;
      
      return 1; // try other match
    }
  }
  
  return PCRE_ERROR_CALLOUT;
}

/*----------------------------------------------------------------------------*/

PMATH_PRIVATE pmath_bool_t _pmath_regex_match(
  struct _regex_t    *regex,
  const char         *subject,
  int                 subject_length,
  int                 subject_offset,
  int                 pcre_options,
  struct _capture_t  *capture,        // must be initialized with _pmath_regex_init_capture()
  pmath_t            *rhs             // optional
){
  struct _callout_data_t  data;
  pcre_extra              extra;
  int                     result;
  
  if(subject_offset > subject_length)
    return FALSE;
  
  data.capture = capture;
  data.regex   = regex;
  
  if(!capture->ovector || !regex)
    return FALSE;
  
  memset(&extra, 0, sizeof(extra));
  extra.flags        = PCRE_EXTRA_CALLOUT_DATA;
  extra.callout_data = &data;
  
  result = pcre_exec(
    regex->code,
    &extra,
    subject,
    subject_length,
    subject_offset,
    pcre_options,
    capture->ovector,
    capture->ovecsize);
  
  if(result >= 0){
    if(rhs && *rhs){
      if(pmath_is_expr_of(regex->key.object, PMATH_SYMBOL_REGULAREXPRESSION))
        *rhs = replace_string_subpatterns(regex, capture, subject, *rhs);
      else
        *rhs = replace_named_subpatterns(regex, capture, subject, *rhs);
    }
    
    return TRUE;
  }
  
  return FALSE;
}

/*----------------------------------------------------------------------------*/

PMATH_PRIVATE void _pmath_regex_memory_panic(void){
  pmath_hashtable_t table = regex_cache_lock();
  {
    pmath_ht_clear(table);
  }
  regex_cache_unlock(table);
}

PMATH_PRIVATE pmath_bool_t _pmath_regex_init(void){
  _global_regex_cache = pmath_ht_create(&regex_cache_ht_class, 0);
  memset(regex_cache_array,0,sizeof(regex_cache_array));
  regex_cache_array_next = 0;
  
  pcre_callout = callout;
  return _global_regex_cache != NULL;
}

PMATH_PRIVATE void _pmath_regex_done(void){
  int i;
  for(i = 0;i < REGEX_CACHE_ARRAY_SIZE;++i)
    _pmath_regex_unref(regex_cache_array[i]);
  pmath_ht_destroy((pmath_hashtable_t)_global_regex_cache);
}
