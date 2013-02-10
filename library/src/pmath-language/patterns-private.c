#include <pmath-core/numbers.h>
#include <pmath-core/expressions-private.h>

#include <pmath-language/patterns-private.h>

#include <pmath-util/debug.h>
#include <pmath-util/hashtables-private.h>
#include <pmath-util/helpers.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/option-helpers.h>
#include <pmath-util/memory.h>

#include <pmath-util/concurrency/threads.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/lists-private.h>

#include <inttypes.h>
#include <string.h>


#define MAGIC_PATTERN_FOUND     PMATH_FROM_TAG(PMATH_TAG_MAGIC, 1)
#define MAGIC_PATTERN_SEQUENCE  PMATH_FROM_TAG(PMATH_TAG_MAGIC, 2)

// initialization in pmath_init():
PMATH_PRIVATE pmath_t _pmath_object_range_from_one; /* readonly */
PMATH_PRIVATE pmath_t _pmath_object_range_from_zero; /* readonly */
PMATH_PRIVATE pmath_t _pmath_object_singlematch; /* readonly */
PMATH_PRIVATE pmath_t _pmath_object_multimatch; /* readonly */
PMATH_PRIVATE pmath_t _pmath_object_zeromultimatch; /* readonly */

//{ compare patterns ...

typedef struct pattern_compare_status_t {
  pmath_hashtable_t pat1_counts; // entries: object_count_entry_t
  pmath_hashtable_t pat2_counts; // entries: object_count_entry_t
} pattern_compare_status_t;

static int pattern_compare(
  pmath_t                    pat1,
  pmath_t                    pat2,
  pattern_compare_status_t  *status);

PMATH_PRIVATE int _pmath_pattern_compare(
  pmath_t pat1,
  pmath_t pat2
) {
  pattern_compare_status_t status;
  int result;
  
  status.pat1_counts = pmath_ht_create(&pmath_ht_obj_int_class, 0);
  status.pat2_counts = pmath_ht_create(&pmath_ht_obj_int_class, 0);
  
  if(status.pat1_counts && status.pat2_counts) {
    result = pattern_compare(pat1, pat2, &status);
  }
  else
    result = 0;
    
  pmath_ht_destroy(status.pat1_counts);
  pmath_ht_destroy(status.pat2_counts);
  return result;
}

PMATH_PRIVATE
pmath_bool_t _pmath_rhs_condition(
  pmath_t  *rhs,
  pmath_bool_t  adjust
) {
  pmath_t obj;
  
  if(!pmath_is_expr(*rhs))
    return FALSE;
    
  obj = pmath_expr_get_item(*rhs, 0);
  pmath_unref(obj);
  
  if( (pmath_same(obj, PMATH_SYMBOL_CONDITION) ||
       pmath_same(obj, PMATH_SYMBOL_INTERNAL_CONDITION)) &&
      pmath_expr_length(*rhs) == 2)
  {
    if(adjust) {
      *rhs = pmath_expr_set_item(
               *rhs, 0,
               pmath_ref(PMATH_SYMBOL_INTERNAL_CONDITION));
    }
    
    return TRUE;
  }
  
  if( pmath_same(obj, PMATH_SYMBOL_EVALUATIONSEQUENCE) ||
      pmath_same(obj, PMATH_SYMBOL_LOCAL)              ||
      pmath_same(obj, PMATH_SYMBOL_WITH))
  {
    obj = pmath_expr_get_item(
            *rhs,
            pmath_expr_length(*rhs));
            
    if(_pmath_rhs_condition(&obj, adjust)) {
      *rhs = pmath_expr_set_item(
               *rhs,
               pmath_expr_length(*rhs),
               obj);
               
      return TRUE;
    }
    
    pmath_unref(obj);
  }
  
  return FALSE;
}

static size_t inc_object_count(pmath_hashtable_t table, pmath_t key) {
// returns old object count
  struct _pmath_object_int_entry_t *entry = pmath_ht_search(table, &key);
  
  if(!entry) {
    entry = pmath_mem_alloc(sizeof(struct _pmath_object_int_entry_t));
    
    if(entry) {
      entry->key   = pmath_ref(key);
      entry->value = 1;
      entry = pmath_ht_insert(table, entry);
      
      if(entry)
        pmath_ht_obj_int_class.entry_destructor(entry);
    }
    return 0;
  }
  return entry->value++;
}

static int pattern_compare(
  pmath_t                    pat1,
  pmath_t                    pat2,
  pattern_compare_status_t  *status
) {
  /* result: -1 == "pat1 < pat2", 0 == "pat1 == pat2", 1 == "pat1 > pat2"
  
     count(x) > count(y) => Pattern(x, A)           < Pattern(y, A)           for any A
     count(x) > count(y) => Optional(x, value)      < Optional(y, value)      for any value
     A < B               => Pattern(x, A)           < Pattern(y, B)           for any x, y
     A < B               => Repeated(A, r1)         < Repeated(B, r2)         for any r1, r2
     A < B               => TestPattern(A, fn1)     < TestPattern(B, fn2)     for any fn1, fn2
     A < B               => Alternatives(A,...)     < Alternatives(B,...)     when both Alternatives(...) have the same length
     A < B               => Condition(A, c1)        < Condition(B, c2)        when both Condition(...) have the same length
     A < B               => PatternSequence(A, ...) < PatternSequence(B, ...) when both PatternSequence(...) have the same length
     A < B               => Except(A)               > Except(B)
     A < B               => Except(no, A)           < Except(no, B)
     constants  <  SingleMatch(type)  <  SingleMatch()  <  Repeated(...)
  
     Except(no) == Except(no, SingleMatch())
     Longest(pattern) == pattern
     Shortest(pattern) == pattern
     HoldPattern(pattern) == pattern
     Alternatives(pattern) == pattern
     Alternatives(...) < Alternatives(...) when first Alternatives()-expr is
     shorter that second.
   */
  
  pmath_t head1 = PMATH_NULL;
  pmath_t head2 = PMATH_NULL;
  size_t len1 = 0;
  size_t len2 = 0;
  
  if(pmath_is_expr(pat1)) {
    len1 = pmath_expr_length(pat1);
    head1 = pmath_expr_get_item(pat1, 0);
    pmath_unref(head1);
  }
  if(pmath_is_expr(pat2)) {
    len2 = pmath_expr_length(pat2);
    head2 = pmath_expr_get_item(pat2, 0);
    pmath_unref(head2);
  }
  
  //{ PatternSequence(...) ...
  if(pmath_same(head1, PMATH_SYMBOL_PATTERNSEQUENCE)) {
    pmath_t p1;
    int cmp;
    
    if(pmath_same(head2, PMATH_SYMBOL_PATTERNSEQUENCE)) {
      size_t i;
      
      if(len1 < len2)
        return 1;
      if(len1 > len2)
        return -1;
        
      for(i = 1; i <= len1; ++i) {
        pmath_t p2 = pmath_expr_get_item(pat2, i);
        p1 = pmath_expr_get_item(pat1, i);
        
        cmp = pattern_compare(p1, p2, status);
        pmath_unref(p1);
        pmath_unref(p2);
        if(cmp != 0)
          return cmp;
      }
      return 0;
    }
    if(len2 < 1)
      return 1;
    if(len2 > 1)
      return -1;
      
    p1 = pmath_expr_get_item(pat1, 1);
    cmp = pattern_compare(p1, pat2, status);
    pmath_unref(p1);
    return cmp;
  }
  
  if(pmath_same(head2, PMATH_SYMBOL_PATTERNSEQUENCE)) {
    pmath_t p2;
    int cmp;
    
    if(len2 < 1)
      return 1;
    if(len2 > 1)
      return -1;
      
    p2 = pmath_expr_get_item(pat2, 1);
    cmp = pattern_compare(pat1, p2, status);
    pmath_unref(p2);
    return cmp;
  }
  //} ... PatternSequence(...)
  
  //{ Repeated(pat, range) ...
  if(len1 == 2 && pmath_same(head1, PMATH_SYMBOL_REPEATED)) {
    pmath_t p1 = pmath_expr_get_item(pat1, 1);
    int cmp;
    
    if(len2 == 2 && pmath_same(head2, PMATH_SYMBOL_REPEATED)) {
      size_t min1, min2, max1, max2;
      pmath_bool_t valid1, valid2;
      
      pmath_t p2 = pmath_expr_get_item(pat2, 1);
      cmp = pattern_compare(p1, p2, status);
      
      pmath_unref(p1);
      pmath_unref(p2);
      if(cmp != 0)
        return cmp;
        
      min1 = 1;
      min2 = 1;
      max1 = SIZE_MAX;
      max2 = SIZE_MAX;
      
      p1 = pmath_expr_get_item(pat1, 2);
      p2 = pmath_expr_get_item(pat2, 2);
      valid1 = extract_range(p1, &min1, &max1, TRUE);
      valid2 = extract_range(p2, &min2, &max2, TRUE);
      
      pmath_unref(p1);
      pmath_unref(p2);
      if(valid1 && valid2) {
        if(min1 > min2) return -1;
        if(min1 < min2) return 1;
        if(max1 - min1 < max2 - min2) return -1;
        if(max1 - min1 > max2 - min2) return  1;
        if(max1 > max2) return -1;
        if(max1 < max2) return  1;
        return 0;
      }
      return pmath_compare(pat1, pat2);
    }
    
    cmp = pattern_compare(p1, pat2, status);
    pmath_unref(p1);
    if(cmp == 0)
      return 1;
    return cmp;
  }
  
  if(len2 == 2 && pmath_same(head2, PMATH_SYMBOL_REPEATED)) {
    pmath_t p2 = pmath_expr_get_item(pat2, 1);
    int cmp = pattern_compare(pat1, p2, status);
    pmath_unref(p2);
    if(cmp == 0)
      return -1;
    return cmp;
  }
  //} ... Repeated(pat, range)
  
  //{ SingleMatch(), SingleMatch(type) ...
  if(pmath_same(head1, PMATH_SYMBOL_SINGLEMATCH)) {
    if(pmath_same(head2, PMATH_SYMBOL_SINGLEMATCH)) {
      if(len1 == 0)
        return len2 == 0 ? 0 : 1;
        
      if(len2 == 0)
        return len1 == 0 ? 0 : -1;
        
      if(len1 == 1 && len2 == 1) {
        pmath_t t1 = pmath_expr_get_item(pat1, 1);
        pmath_t t2 = pmath_expr_get_item(pat2, 1);
        
        int cmp = pmath_compare(t1, t2);
        
        pmath_unref(t1);
        pmath_unref(t2);
        return cmp;
      }
    }
    else
      return 1;
  }
  else if(pmath_same(head2, PMATH_SYMBOL_SINGLEMATCH))
    return -1;
  //} ... SingleMatch(), SingleMatch(type)
  
  //{ TestPattern(pat, fn), Condition(pat, cond) ...
  if( len1 == 2 &&
      (pmath_same(head1, PMATH_SYMBOL_TESTPATTERN) ||
       pmath_same(head1, PMATH_SYMBOL_CONDITION)))
  {
    pmath_t p1 = pmath_expr_get_item(pat1, 1);
    int cmp;
    
    if( len2 == 2 &&
        (pmath_same(head2, PMATH_SYMBOL_TESTPATTERN) ||
         pmath_same(head2, PMATH_SYMBOL_CONDITION)))
    {
      pmath_t p2 = pmath_expr_get_item(pat2, 1);
      cmp = pattern_compare(p1, p2, status);
      pmath_unref(p1);
      pmath_unref(p2);
      if(cmp != 0)
        return cmp;
        
      if(pmath_same(head1, head2)) {
        p1 = pmath_expr_get_item(pat1, 2);
        p2 = pmath_expr_get_item(pat2, 2);
        cmp = pmath_compare(p1, p2);
        pmath_unref(p1);
        pmath_unref(p2);
        return cmp;
      }
      
      if(pmath_same(head1, PMATH_SYMBOL_CONDITION))
        return -1;
      return 1;
    }
    
    cmp = pattern_compare(p1, pat2, status);
    pmath_unref(p1);
    if(cmp == 0)
      return -1;
    return cmp;
  }
  
  if(len2 == 2 && pmath_same(head2, PMATH_SYMBOL_TESTPATTERN)) {
    pmath_t p2 = pmath_expr_get_item(pat2, 1);
    
    int cmp = pattern_compare(pat1, p2, status);
    
    pmath_unref(p2);
    if(cmp == 0)
      return 1;
    return cmp;
  }
  //} ... TestPattern(pat, fn), Condition(pat, cond)
  
  //{ Longest(pat), Shortest(pat), HoldPattern(pat) ...
  if( (pmath_same(head1, PMATH_SYMBOL_LONGEST) ||
       pmath_same(head1, PMATH_SYMBOL_SHORTEST) ||
       pmath_same(head1, PMATH_SYMBOL_HOLDPATTERN)) &&
      len1 == 1)
  {
    pmath_t p1 = pmath_expr_get_item(pat1, 1);
    int cmp = pattern_compare(p1, pat2, status);
    pmath_unref(p1);
    return cmp;
  }
  
  if( (pmath_same(head2, PMATH_SYMBOL_LONGEST) ||
       pmath_same(head2, PMATH_SYMBOL_SHORTEST) ||
       pmath_same(head2, PMATH_SYMBOL_HOLDPATTERN)) &&
      len2 == 1)
  {
    pmath_t p2 = pmath_expr_get_item(pat2, 1);
    int cmp = pattern_compare(pat1, p2, status);
    pmath_unref(p2);
    return cmp;
  }
  //} ... Longest(pat), Shortest(pat), HoldPattern(pat)
  
  //{ Alternatives(...) ...
  if(pmath_same(head1, PMATH_SYMBOL_ALTERNATIVES)) {
    pmath_t p1;
    int cmp;
    
    if(pmath_same(head2, PMATH_SYMBOL_ALTERNATIVES)) {
      size_t i;
      
      if(len1 < len2)
        return -1;
      if(len1 > len2)
        return 1;
        
      for(i = 1; i <= len1; ++i) {
        pmath_t p2 = pmath_expr_get_item(pat2, i);
        p1 = pmath_expr_get_item(pat1, i);
        
        cmp = pattern_compare(p1, p2, status);
        pmath_unref(p1);
        pmath_unref(p2);
        if(cmp != 0)
          return cmp;
      }
      return 0;
    }
    if(len2 < 1)
      return -1;
    if(len2 > 1)
      return 1;
      
    p1 = pmath_expr_get_item(pat1, 1);
    cmp = pattern_compare(p1, pat2, status);
    pmath_unref(p1);
    return cmp;
  }
  
  if(pmath_same(head2, PMATH_SYMBOL_ALTERNATIVES)) {
    pmath_t p2;
    int cmp;
    
    if(len2 < 1)
      return -1;
    if(len2 > 1)
      return 1;
      
    p2 = pmath_expr_get_item(pat2, 1);
    cmp = pattern_compare(pat1, p2, status);
    pmath_unref(p2);
    return cmp;
  }
  //} ... Alternatives(...)
  
  //{ Optional(name), Optional(name, value) ...
  if(pmath_same(head1, PMATH_SYMBOL_OPTIONAL) && (len1 == 1 || len1 == 2)) {
    if(pmath_same(head2, PMATH_SYMBOL_OPTIONAL) && (len2 == 1 || len2 == 2)) {
      pmath_t p1 = pmath_expr_get_item(pat1, 1);
      pmath_t p2 = pmath_expr_get_item(pat2, 1);
      size_t count1 = inc_object_count(status->pat1_counts, p1);
      size_t count2 = inc_object_count(status->pat2_counts, p2);
      pmath_unref(p1);
      pmath_unref(p2);
      
      if(count1 > count2) return -1;
      if(count1 < count2) return  1;
      
      if(len1 < len2) return -1;
      if(len1 > len2) return 1;
      
      if(len1 == 2) {
        int cmp;
        p1 = pmath_expr_get_item(pat1, 2);
        p2 = pmath_expr_get_item(pat2, 2);
        cmp = pmath_compare(p1, p2);
        pmath_unref(p1);
        pmath_unref(p2);
        return cmp;
      }
      
      return 0;
    }
    return 1;
  }
  
  if(pmath_same(head2, PMATH_SYMBOL_OPTIONAL) && (len2 == 1 || len2 == 2)) {
    return -1;
  }
  //} ... Optional(name), Optional(name, value)
  
  //{ Pattern(name, pat) ...
  if(pmath_same(head1, PMATH_SYMBOL_PATTERN) && len1 == 2) {
    if(pmath_same(head2, PMATH_SYMBOL_PATTERN) && len2 == 2) {
      pmath_t p1 = pmath_expr_get_item(pat1, 1);
      pmath_t p2 = pmath_expr_get_item(pat2, 1);
      size_t count1 = inc_object_count(status->pat1_counts, p1);
      size_t count2 = inc_object_count(status->pat2_counts, p2);
      int cmp;
      pmath_unref(p1);
      pmath_unref(p2);
      
      if(count1 > count2) return -1;
      if(count1 < count2) return  1;
      
      p1 = pmath_expr_get_item(pat1, 2);
      p2 = pmath_expr_get_item(pat2, 2);
      cmp = pattern_compare(p1, p2, status);
      pmath_unref(p1);
      pmath_unref(p2);
      return cmp;
    }
    else {
      pmath_t p1 = pmath_expr_get_item(pat1, 2);
      int cmp = pattern_compare(p1, pat2, status);
      pmath_unref(p1);
      
      p1 = pmath_expr_get_item(pat1, 1);
      inc_object_count(status->pat1_counts, p1);
      pmath_unref(p1);
      
      if(cmp == 0)
        return -1;
      return cmp;
    }
  }
  
  if(pmath_same(head2, PMATH_SYMBOL_PATTERN) && len2 == 2) {
    pmath_t p2 = pmath_expr_get_item(pat2, 2);
    int cmp = pattern_compare(pat1, p2, status);
    pmath_unref(p2);
    
    p2 = pmath_expr_get_item(pat2, 1);
    inc_object_count(status->pat2_counts, p2);
    pmath_unref(p2);
    
    if(cmp == 0)
      return 1;
    return cmp;
  }
  //} ... Pattern(name, pat)
  
  //{ Except(no), Except(no, pat)
  if(pmath_same(head1, PMATH_SYMBOL_EXCEPT) && (len1 == 1 || len1 == 2)) {
    pmath_t p1;
    
    if(len1 == 2)
      p1 = pmath_expr_get_item(pat1, 2);
    else
      p1 = pmath_ref(_pmath_object_singlematch);
      
    if(pmath_same(head2, PMATH_SYMBOL_EXCEPT) && (len2 == 1 || len2 == 2)) {
      pmath_t p2;
      int cmp;
      
      if(len2 == 2)
        p2 = pmath_expr_get_item(pat2, 2);
      else
        p2 = pmath_ref(_pmath_object_singlematch);
        
      cmp = pattern_compare(p1, p2, status);
      pmath_unref(p1);
      pmath_unref(p2);
      
      if(cmp == 0) {
        p1 = pmath_expr_get_item(pat1, 1);
        p2 = pmath_expr_get_item(pat1, 1);
        
        cmp = pattern_compare(p1, p2, status);
        pmath_unref(p1);
        pmath_unref(p2);
      }
      
      return cmp;
    }
    else {
      int cmp = pattern_compare(p1, pat2, status);
      pmath_unref(p1);
      
      if(cmp == 0)
        return -1;
        
      return cmp;
    }
  }
  
  if(pmath_same(head2, PMATH_SYMBOL_EXCEPT) && (len2 == 1 || len2 == 2)) {
    pmath_t p2;
    int cmp;
    
    if(len1 == 2)
      p2 = pmath_expr_get_item(pat2, 2);
    else
      p2 = pmath_ref(_pmath_object_singlematch);
      
    cmp = pattern_compare(pat1, p2, status);
    pmath_unref(p2);
    
    if(cmp == 0)
      return 1;
      
    return cmp;
  }
  //}
  
  //{ Literal(x) ...
  if(pmath_same(head1, PMATH_SYMBOL_LITERAL) && len1 == 1) {
    pmath_t p1, p2;
    int cmp;
    
    p1 = pmath_expr_get_item(pat1, 1);
    
    if(pmath_same(head2, PMATH_SYMBOL_LITERAL) && len2 == 1)
      p2 = pmath_expr_get_item(pat2, 1);
    else
      p2 = pmath_ref(pat2);
      
    cmp = pmath_compare(p1, p2);
    pmath_unref(p1);
    pmath_unref(p2);
    return cmp;
  }
  
  if(pmath_same(head2, PMATH_SYMBOL_PATTERN) && len2 == 1) {
    pmath_t p2;
    int cmp;
    
    p2 = pmath_expr_get_item(pat2, 2);
    cmp = pmath_compare(pat1, p2);
    pmath_unref(p2);
    
    return cmp;
  }
  //} ... Literal(x)
  
  //{ non-expressions ...
  if(!pmath_is_expr(pat1)) {
    if(!pmath_is_expr(pat2))
      return pmath_compare(pat1, pat2);
    return -1;
  }
  
  if(!pmath_is_expr(pat2))
    return 1;
  //} ... non-expressions
  
  if(len1 < len2) return 1;
  if(len1 > len2) return -1;
  
  {
    size_t i;
    for(i = 0; i <= len1; i++) {
      pmath_t p1i = pmath_expr_get_item(pat1, i);
      pmath_t p2i = pmath_expr_get_item(pat2, i);
      int c = pattern_compare(p1i, p2i, status);
      pmath_unref(p1i);
      pmath_unref(p2i);
      if(c != 0)
        return c;
    }
  }
  return 0;
}

//} ... compare patterns

PMATH_PRIVATE pmath_bool_t _pmath_pattern_is_const(
  pmath_t pattern
) {
  pmath_t head;
  size_t i, len;
  
  if(!pmath_is_expr(pattern))
    return TRUE;
    
  len = pmath_expr_length(pattern);
  head = pmath_expr_get_item(pattern, 0);
  pmath_unref(head);
  
  if( (pmath_same(head, PMATH_SYMBOL_CONDITION)        &&  len == 2)              ||
      (pmath_same(head, PMATH_SYMBOL_ALTERNATIVES))                               ||
      (pmath_same(head, PMATH_SYMBOL_HOLDPATTERN)      &&  len == 1)              ||
      (pmath_same(head, PMATH_SYMBOL_LONGEST)          &&  len == 1)              ||
      (pmath_same(head, PMATH_SYMBOL_OPTIONAL)         && (len == 1 || len == 2)) ||
      (pmath_same(head, PMATH_SYMBOL_EXCEPT)           && (len == 1 || len == 2)) ||
      (pmath_same(head, PMATH_SYMBOL_PATTERNSEQUENCE))                            ||
      (pmath_same(head, PMATH_SYMBOL_REPEATED)         &&  len == 2)              ||
      (pmath_same(head, PMATH_SYMBOL_SHORTEST)         &&  len == 1)              ||
      (pmath_same(head, PMATH_SYMBOL_SINGLEMATCH)      &&  len <= 1)              ||
      (pmath_same(head, PMATH_SYMBOL_OPTIONSPATTERN)   &&  len <= 1)              ||
      (pmath_same(head, PMATH_SYMBOL_TESTPATTERN)      &&  len == 2))
  {
    return FALSE;
  }
  
  for(i = 0; i <= len; i++) {
    pmath_t pattern_i = pmath_expr_get_item(pattern, i);
    if(!_pmath_pattern_is_const(pattern_i)) {
      pmath_unref(pattern_i);
      return FALSE;
    }
    pmath_unref(pattern_i);
  }
  return TRUE;
}

//{ match patterns ...
typedef struct pattern_info_t {
  pmath_t       current_head;
  pmath_t       pattern;
  pmath_t       func;
  pmath_expr_t  variables;   // form: <T> = <VariableName>(<Value>,<T>)
  pmath_expr_t  options;     // form: <T> = <Function>(<OptionValueRules>,<T>)
  
  // pattern/func is Associative but not Symmetric
  size_t        assoc_start;
  size_t        assoc_end;
  
  // used iff pattern/func is Symmetric; length of array is length of func
  char         *arg_usage;
  // arg_usage items' values:
#define NOT_IN_USE   0
#define IN_USE       1
#define TESTING_USE  2
  
  pmath_bool_t  associative;
  pmath_bool_t  symmetric;
} pattern_info_t;

typedef enum match_kind_t {
  PMATH_MATCH_KIND_NONE,
  PMATH_MATCH_KIND_LOCAL,
  PMATH_MATCH_KIND_GLOBAL
} match_kind_t;

typedef struct match_func_data_t {
  pattern_info_t  *info;
  pmath_expr_t     pat;       // wont be freed
  pmath_expr_t     func;      // wont be freed
  
  pmath_bool_t associative;
  pmath_bool_t one_identity;
  pmath_bool_t symmetric;
} match_func_data_t;

static match_kind_t match_atom(
  pattern_info_t  *info,
  pmath_t          pat,          // wont be freed
  pmath_t          arg,          // wont be freed
  size_t           index_of_arg,
  size_t           count_of_arg);

static match_kind_t match_repeated(
  pattern_info_t  *info,
  pmath_t          pat,   // wont be freed
  pmath_expr_t     arg);  // wont be freed

static match_kind_t match_func_left( // for non-symmetric functions
  match_func_data_t  *data,
  size_t              pat_start,
  size_t              func_start);

static match_kind_t match_func(
  pattern_info_t  *info,
  pmath_expr_t     pat,   // wont be freed
  pmath_expr_t     arg);  // wont be freed

static pmath_t replace_pattern_var(
  pmath_t pattern,          // will be freed
  pmath_t invoking_pattern,
  pmath_t name,
  pmath_t value);

static pmath_bool_t replace_exact_once(
  pmath_t *pattern,
  pmath_t  _old,
  pmath_t  _new);

// retains debug-info
static pmath_t replace_option_value(
  pmath_t      body,          // will be freed
  pmath_t      default_head,
  pmath_expr_t optionvaluerules);  // form: <T> = <Function>(<OptionValueRules>,<T>)

// retains debug-info
static pmath_t replace_multiple(
  pmath_t           object,        // will be freed
  pmath_hashtable_t replacements); // entries are struct _pmath_object_entry_t*

static pmath_bool_t varlist_to_hashtable(
  pmath_hashtable_t hashtable,   // entries are struct _pmath_object_entry_t*
  pmath_expr_t      varlist);    // will be freed

//#define DEBUG_LOG_MATCH

#ifdef DEBUG_LOG_MATCH
static int indented = 0;
static void debug_indent(void) {
  int i = indented % 30;
  while(i-- > 0)
    pmath_debug_print("  ");
}

static void show_indices(
  const char *pre,
  size_t  *indices,
  size_t   indices_len,
  const char *post
) {
  size_t i;
  
  if(indices_len == 0) {
    pmath_debug_print("%s[]%s", pre, post);
    return;
  }
  pmath_debug_print("%s[%"PRIuPTR, pre, *indices++);
  
  for(i = 1; i < indices_len; ++i) {
    pmath_debug_print(" %"PRIuPTR, *indices++);
  }
  pmath_debug_print("]%s", post);
}

static void show_arg_usage(
  const char *pre,
  char    *args_in_use,
  size_t   max_index,
  const char *post
) {
  size_t i;
  
  pmath_debug_print("%s[", pre);
  for(i = 0; i < max_index; ++i) {
    switch(args_in_use[i]) {
      case NOT_IN_USE:  pmath_debug_print("_"); break;
      case IN_USE:      pmath_debug_print("x"); break;
      case TESTING_USE: pmath_debug_print("?"); break;
      default:          pmath_debug_print("E"); // error
    }
  }
  pmath_debug_print("]%s", post);
}
#endif

PMATH_PRIVATE pmath_bool_t _pmath_pattern_match(
  pmath_t  obj,      // wont be freed
  pmath_t  pattern,  // will be freed
  pmath_t *rhs       // in/out (right hand side of assign, Rule, ...)
) {
  pattern_info_t  info;
  match_kind_t    kind;
  size_t funclen;
  
  info.current_head = PMATH_NULL;
  info.pattern      = pattern;
  info.func         = obj;
  info.variables    = PMATH_NULL;
  info.options      = PMATH_NULL;
  
  info.assoc_start  = 1;
  info.assoc_end    = SIZE_MAX;
  info.arg_usage    = NULL;
  info.associative  = FALSE;
  info.symmetric    = FALSE;
  
  funclen = 1;
  
  if(pmath_is_expr(info.func)) {
    pmath_t head = pmath_expr_get_item(info.func, 0);
    funclen = pmath_expr_length(info.func);
    
    if(pmath_is_symbol(head)) {
      pmath_symbol_attributes_t attrib = pmath_symbol_get_attributes(head);
      
      info.associative = (attrib & PMATH_SYMBOL_ATTRIBUTE_ASSOCIATIVE) != 0;
      if((attrib & PMATH_SYMBOL_ATTRIBUTE_SYMMETRIC) != 0) {
        info.symmetric = TRUE;
        info.arg_usage = (char *)pmath_mem_alloc(funclen);
        if(!info.arg_usage) {
          pmath_unref(head);
          pmath_unref(pattern);
          return FALSE;
        }
      }
    }
    
    pmath_unref(head);
  }
  
  kind = match_atom(&info, info.pattern, info.func, 0, 0);
  
  if(kind != PMATH_MATCH_KIND_NONE && !pmath_aborting()) {
    if(rhs) {
      if(!pmath_is_null(*rhs)) {
        pmath_hashtable_t vartable = pmath_ht_create(&pmath_ht_obj_class, 0);
        varlist_to_hashtable(vartable, info.variables); // frees info.variables
        info.variables = PMATH_NULL;
        
        *rhs = replace_multiple(*rhs, vartable);
        pmath_ht_destroy(vartable);
        
        if(!pmath_is_null(info.options)) {
          pmath_t default_head = PMATH_NULL;
          if(pmath_is_expr(obj))
            default_head = pmath_expr_get_item(obj, 0);
            
          *rhs = replace_option_value(*rhs, default_head, info.options);
          pmath_unref(default_head);
        }
        
        if(_pmath_rhs_condition(rhs, TRUE)) {
          *rhs = pmath_evaluate(*rhs);
          
          if(pmath_is_expr_of_len(*rhs, PMATH_SYMBOL_INTERNAL_CONDITION, 2)) {
            pmath_t res = pmath_expr_get_item(*rhs, 2);
            pmath_unref(res);
            
            if(!pmath_same(res, PMATH_SYMBOL_TRUE)) {
              pmath_unref(*rhs);
              *rhs = PMATH_NULL;
              kind = PMATH_MATCH_KIND_NONE;
              goto NO_MATCH;
            }
            
            res = pmath_expr_get_item(*rhs, 1);
            pmath_unref(*rhs);
            *rhs = res;
          }
        }
        
        if(pmath_is_expr_of(*rhs, MAGIC_PATTERN_SEQUENCE)) {
          *rhs = pmath_expr_set_item(*rhs, 0, pmath_ref(PMATH_SYMBOL_SEQUENCE));
        }
      }
      
      if(info.symmetric) {
        size_t i;
        pmath_bool_t have_unused_args = FALSE;
        
#ifdef DEBUG_LOG_MATCH
        show_arg_usage("info.arg_usage: ", info.arg_usage, funclen, "\n");
#endif
        
        for(i = 0; i < funclen; ++i) {
          if(!info.arg_usage[i]) {
            have_unused_args = TRUE;
            break;
          }
        }
        
        if(have_unused_args) {
          obj = *rhs;
          *rhs = pmath_ref(info.func); // old obj == info.func
          assert(pmath_is_null(*rhs) || pmath_is_expr(*rhs));
          
          for(i = 0; i < funclen; ++i) {
            if(info.arg_usage[i]) {
              *rhs = pmath_expr_set_item(*rhs, i + 1, obj);
              obj = PMATH_UNDEFINED;
            }
          }
          
          *rhs = pmath_expr_remove_all(*rhs, PMATH_UNDEFINED);
        }
      }
      else if(info.associative && info.assoc_start <= info.assoc_end) {
        if(info.assoc_start > 1 || info.assoc_end < funclen) {
          *rhs = pmath_expr_set_item(
                   pmath_ref(obj),
                   info.assoc_start,
                   *rhs);
                   
          if(info.assoc_start < info.assoc_end) {
            size_t i;
            for(i = info.assoc_start + 1; i <= info.assoc_end; ++i) {
              *rhs = pmath_expr_set_item(*rhs, i, PMATH_UNDEFINED);
            }
            *rhs = pmath_expr_remove_all(*rhs, PMATH_UNDEFINED);
          }
        }
      }
    }
  }
  
NO_MATCH:

  pmath_unref(info.options);
  pmath_unref(info.variables);
  pmath_unref(info.pattern);
  if(info.symmetric)
    pmath_mem_free(info.arg_usage);
  return kind != PMATH_MATCH_KIND_NONE;
}

static match_kind_t match_atom(
  pattern_info_t  *info,
  pmath_t          pat,  // wont be freed
  pmath_t          arg,  // wont be freed
  size_t           index_of_arg,
  size_t           count_of_arg
) {
  if(pmath_equals(pat, arg))
    return PMATH_MATCH_KIND_LOCAL;
    
  if(pmath_is_expr(pat)) {
    const size_t              len  = pmath_expr_length(pat);
    pmath_t                   head = pmath_expr_get_item(pat, 0);
    pmath_symbol_attributes_t attr = 0;
    
    if(pmath_is_symbol(head))
      attr = pmath_symbol_get_attributes(head);
    pmath_unref(head);
    
    if(len == 1 && pmath_same(head, MAGIC_PATTERN_FOUND)) { // FOUND(val)
      pmath_t val = pmath_expr_get_item(pat, 1);
      
      if(pmath_equals(val, arg)) {
        pmath_unref(val);
        return PMATH_MATCH_KIND_LOCAL;
      }
      
      pmath_unref(val);
      return PMATH_MATCH_KIND_NONE;
    }
    
    if(pmath_same(head, PMATH_SYMBOL_SINGLEMATCH) && len == 0) // SingleMatch()
      return PMATH_MATCH_KIND_LOCAL;
      
    if(pmath_same(head, PMATH_SYMBOL_SINGLEMATCH) && len == 1) { // SingleMatch(type)
      pmath_t type    = pmath_expr_get_item(pat, 1);
      pmath_t arghead = _pmath_object_head(arg);
      
      if(pmath_equals(type, arghead)) {
        pmath_unref(type);
        pmath_unref(arghead);
        return PMATH_MATCH_KIND_LOCAL;
      }
      pmath_unref(type);
      pmath_unref(arghead);
      return PMATH_MATCH_KIND_NONE;
    }
    
    if( len == 2 &&
        (pmath_same(head, PMATH_SYMBOL_TESTPATTERN) || // pattern ? testfunc
         pmath_same(head, PMATH_SYMBOL_CONDITION)))  // pattern /? condition
    {
      pmath_t pattern = pmath_expr_get_item(pat, 1);
      pmath_t test = PMATH_NULL;
      
      match_kind_t kind = match_atom(info, pattern, arg, index_of_arg, count_of_arg);
      pmath_unref(pattern);
      if(kind != PMATH_MATCH_KIND_LOCAL)
        return kind;
        
      if(pmath_same(head, PMATH_SYMBOL_TESTPATTERN)) {
        if(pmath_is_expr(arg)) {
          pmath_t arghead = pmath_expr_get_item(arg, 0);
          pmath_unref(arghead);
          
          if(pmath_same(arghead, MAGIC_PATTERN_SEQUENCE)) {
            test = pmath_expr_set_item(
                     pmath_ref(arg), 0,
                     pmath_expr_get_item(pat, 2));
                     
            goto AFTER_TEST_INIT;
          }
        }
        test = pmath_expr_new_extended(
                 pmath_expr_get_item(pat, 2), 1,
                 pmath_ref(arg));
      }
      else
        test = pmath_expr_get_item(pat, 2);
        
    AFTER_TEST_INIT:
      test = pmath_evaluate(test);
      pmath_unref(test);
      
      if(pmath_aborting())
        return PMATH_MATCH_KIND_GLOBAL;
        
      if(pmath_same(test, PMATH_SYMBOL_TRUE))
        return PMATH_MATCH_KIND_LOCAL;
        
      return PMATH_MATCH_KIND_NONE;
    }
    
    if(len == 2 && pmath_same(head, PMATH_SYMBOL_REPEATED)) { // Repeated(pattern, range)
      pmath_t range, pattern;
      match_kind_t kind;
      size_t min, max, arglen;
      
      range = pmath_expr_get_item(pat, 2);
      
      min = 1;
      max = SIZE_MAX;
      
      if(!extract_range(range, &min, &max, TRUE)) {
        pmath_unref(range);
        return PMATH_MATCH_KIND_NONE;
      }
      
      pmath_unref(range);
      
      if(min > max)
        return PMATH_MATCH_KIND_NONE;
        
      if(max == 0) {
        if(pmath_is_expr_of_len(arg, MAGIC_PATTERN_SEQUENCE, 0))
          return PMATH_MATCH_KIND_LOCAL;
        return PMATH_MATCH_KIND_NONE;
      }
      
      pattern = pmath_expr_get_item(pat, 1);
      
      if(!pmath_is_expr_of(arg, MAGIC_PATTERN_SEQUENCE)) {
        if(min > 1) {
          pmath_unref(pattern);
          return PMATH_MATCH_KIND_NONE;
        }
        
        kind = match_atom(info, pattern, arg, index_of_arg, count_of_arg);
        pmath_unref(pattern);
        return kind;
      }
      
      arglen = pmath_expr_length(arg);
      
      if(arglen < min || arglen > max) {
        pmath_unref(pattern);
        return PMATH_MATCH_KIND_NONE;
      }
      
      kind = match_repeated(info, pattern, arg);
      pmath_unref(pattern);
      return kind;
    }
    
    if( (pmath_same(head, MAGIC_PATTERN_FOUND) ||  // FOUND(value, pattern)
         pmath_same(head, PMATH_SYMBOL_PATTERN)) && // name: pattern
        len == 2)
    {
      pmath_t pattern = pmath_expr_get_item(pat, 2);
      
      match_kind_t kind = match_atom(info, pattern, arg, index_of_arg, count_of_arg);
      if(kind != PMATH_MATCH_KIND_LOCAL) {
        pmath_unref(pattern);
        return kind;
      }
      pmath_unref(pattern);
      
      if(pmath_same(head, MAGIC_PATTERN_FOUND)) { // FOUND(value, pattern)
        pmath_t value = pmath_expr_get_item(pat, 1);
        if(pmath_equals(arg, value)) {
          pmath_unref(value);
          return PMATH_MATCH_KIND_LOCAL;
        }
        pmath_unref(value);
      }
      else {
        pmath_t old_pattern   = pmath_ref(info->pattern);
        pmath_t old_variables = pmath_ref(info->variables);
        pmath_t name = pmath_expr_get_item(pat, 1);
        
        info->variables = pmath_expr_new_extended(
                            name, 2, pmath_ref(arg), info->variables);
        info->pattern = replace_pattern_var(info->pattern, pat, name, arg);
        
        kind = match_atom(info, info->pattern, info->func, 0, 0);
        if(kind != PMATH_MATCH_KIND_NONE) {
          pmath_unref(old_pattern);
          pmath_unref(old_variables);
          return PMATH_MATCH_KIND_GLOBAL;
        }
        
        pmath_unref(info->pattern);
        pmath_unref(info->variables);
        info->pattern   = old_pattern;
        info->variables = old_variables;
      }
      return PMATH_MATCH_KIND_NONE;
    }
    
    if(pmath_same(head, PMATH_SYMBOL_OPTIONAL) && (len == 1 || len == 2)) { // Optional(name), Optional(name,value)
      pmath_t value = pmath_ref(arg);
      pmath_t old_pattern;
      pmath_t old_variables;
      pmath_t name;
      match_kind_t   kind;
      
      if(pmath_is_expr_of_len(arg, MAGIC_PATTERN_SEQUENCE, 0)) {
        pmath_unref(value);
        if(len == 1) {
          value = pmath_evaluate(
                    pmath_expr_new_extended(
                      pmath_ref(PMATH_SYMBOL_DEFAULT), 3,
                      pmath_ref(info->current_head),
                      pmath_integer_new_uiptr(index_of_arg),
                      pmath_integer_new_uiptr(count_of_arg)));
        }
        else
          value = pmath_expr_get_item(pat, 2);
      }
      
      old_pattern   = pmath_ref(info->pattern);
      old_variables = pmath_ref(info->variables);
      name = pmath_expr_get_item(pat, 1);
      
      info->variables = pmath_expr_new_extended(
                          name, 2, value, info->variables);
      info->pattern = replace_pattern_var(info->pattern, pat, name, arg);
      
      kind = match_atom(info, info->pattern, info->func, 0, 0);
      if(kind != PMATH_MATCH_KIND_NONE) {
        pmath_unref(old_pattern);
        pmath_unref(old_variables);
        return PMATH_MATCH_KIND_GLOBAL;
      }
      
      pmath_unref(info->pattern);
      pmath_unref(info->variables);
      info->pattern   = old_pattern;
      info->variables = old_variables;
      return PMATH_MATCH_KIND_NONE;
    }
    
    if( len == 1 &&
        (pmath_same(head, PMATH_SYMBOL_HOLDPATTERN) || // HoldPattern(p)
         pmath_same(head, PMATH_SYMBOL_LONGEST)     || // Longest(p)
         pmath_same(head, PMATH_SYMBOL_SHORTEST)))     // Shortest(p)
    {
      pmath_t p = pmath_expr_get_item(pat, 1);
      match_kind_t kind = match_atom(info, p, arg, index_of_arg, count_of_arg);
      pmath_unref(p);
      return kind;
    }
    
    if(pmath_same(head, PMATH_SYMBOL_ALTERNATIVES)) { // p1 | p2 | ...
      size_t i;
      for(i = 1; i <= len; ++i) {
        pmath_t p = pmath_expr_get_item(pat, i);
        match_kind_t result = match_atom(info, p, arg, index_of_arg, count_of_arg);
        pmath_unref(p);
        if(result != PMATH_MATCH_KIND_NONE)
          return result;
      }
      return PMATH_MATCH_KIND_NONE;
    }
    
    if(len <= 1 && pmath_same(head, PMATH_SYMBOL_OPTIONSPATTERN)) { // OptionsPattern(), OptionsPattern(f)
      pmath_t arghead;
      size_t i, arglen;
      
      if(!pmath_is_expr(arg))
        return PMATH_MATCH_KIND_NONE;
        
      arglen = pmath_expr_length(arg);
      arghead = pmath_expr_get_item(arg, 0);
      pmath_unref(arghead);
      if( arglen == 2 &&
          (pmath_same(arghead, PMATH_SYMBOL_RULE) ||
           pmath_same(arghead, PMATH_SYMBOL_RULEDELAYED)))
      {
        goto OPTIONSPATTERN_FIT;
      }
      
      if( !pmath_same(arghead, MAGIC_PATTERN_SEQUENCE) &&
          !pmath_same(arghead, PMATH_SYMBOL_LIST))
      {
        return PMATH_MATCH_KIND_NONE;
      }
      
      for(i = 1; i <= arglen; ++i) {
        pmath_t argi = pmath_expr_get_item(arg, i);
        
        if(!pmath_is_set_of_options(argi)) {
          pmath_unref(argi);
          return PMATH_MATCH_KIND_NONE;
        }
        
        pmath_unref(argi);
      }
      
    OPTIONSPATTERN_FIT:
      {
        pmath_t old_pattern = pmath_ref(info->pattern);
        pmath_t old_options = pmath_ref(info->options);
        pmath_t fn;
        match_kind_t kind;
        
        if(len == 0)
          fn = pmath_ref(info->current_head);
        else
          fn = pmath_expr_get_item(pat, 1);
        info->options = pmath_expr_new_extended(
                          fn, 2,
                          pmath_ref(arg),
                          info->options);
                          
        /* replaces the first pat and since all pattern matching is
           left-to-right, it replaces exactly our given pat and not another,
           equal one.
         */
        replace_exact_once(&info->pattern, pat, arg);
        
        kind = match_atom(info, info->pattern, info->func, 0, 0);
        if(kind != PMATH_MATCH_KIND_NONE) {
          pmath_unref(old_pattern);
          pmath_unref(old_options);
          return PMATH_MATCH_KIND_GLOBAL;
        }
        
        pmath_unref(info->pattern);
        pmath_unref(info->options);
        info->pattern = old_pattern;
        info->options = old_options;
      }
      return PMATH_MATCH_KIND_NONE;
    }
    
    if(len == 1 && pmath_same(head, PMATH_SYMBOL_LITERAL)) {
      pmath_t p = pmath_expr_get_item(pat, 1);
      
      if(pmath_equals(p, arg)) {
        pmath_unref(p);
        return PMATH_MATCH_KIND_LOCAL;
      }
      
      pmath_unref(p);
      return PMATH_MATCH_KIND_NONE;
    }
    
    if(pmath_same(head, PMATH_SYMBOL_PATTERNSEQUENCE)) {
      match_func_data_t data;
      match_kind_t kind;
      
      data.info = info;
      data.pat  = pat;
      data.func = pmath_ref(arg);
      data.associative = FALSE;
      data.one_identity = FALSE;
      data.symmetric = FALSE;
      
      if(!pmath_is_expr_of(arg, MAGIC_PATTERN_SEQUENCE))
        data.func = pmath_expr_new_extended(MAGIC_PATTERN_SEQUENCE, 1, data.func);
        
#ifdef DEBUG_LOG_MATCH
      debug_indent(); pmath_debug_print("sequence ...\n");
      ++indented;
#endif
      
      kind = match_func_left(&data, 1, 1);
      
#ifdef DEBUG_LOG_MATCH
      --indented;
      debug_indent(); pmath_debug_print("... sequence %d\n", kind);
#endif
      
      pmath_unref(data.func);
      return kind;
    }
    
    if(pmath_same(head, PMATH_SYMBOL_EXCEPT) && (len == 1 || len == 2)) { // Except(no) Except(no, but)
      pmath_t p = pmath_expr_get_item(pat, 1);
      match_kind_t kind = match_atom(info, p, arg, index_of_arg, count_of_arg);
      pmath_unref(p);
      
      if(kind == PMATH_MATCH_KIND_GLOBAL)
        return kind;
        
      if(kind == PMATH_MATCH_KIND_LOCAL)
        return PMATH_MATCH_KIND_NONE;
        
      kind = PMATH_MATCH_KIND_LOCAL;
      if(len == 2) {
        p = pmath_expr_get_item(pat, 2);
        kind = match_atom(info, p, arg, index_of_arg, count_of_arg);
        pmath_unref(p);
      }
      
      return kind;
    }
    
    if(attr & PMATH_SYMBOL_ATTRIBUTE_ONEIDENTITY) {
      match_kind_t kind;
      
      if(pmath_is_expr(arg)) {
        match_kind_t kind = match_func(info, pat, arg);
        if(kind != PMATH_MATCH_KIND_NONE)
          return kind;
      }
      
      arg = pmath_expr_new_extended(
              pmath_expr_get_item(pat, 0), 1,
              pmath_ref(arg));
              
      kind = match_func(info, pat, arg);
      pmath_unref(arg);
      return kind;
    }
    
    if(!pmath_is_expr(arg))
      return PMATH_MATCH_KIND_NONE;
      
    return match_func(info, pat, arg);
  }
  
  return PMATH_MATCH_KIND_NONE;
}

PMATH_PRIVATE void _pmath_pattern_analyse(
  _pmath_pattern_analyse_input_t  *input,
  _pmath_pattern_analyse_output_t *output
) {
  output->min = 1;
  output->max = 1;
  output->no_sequence = FALSE;
  output->longest = TRUE;
  
  if(pmath_is_expr(input->pat)) {
    size_t len = pmath_expr_length(input->pat);
    pmath_t head = pmath_expr_get_item(input->pat, 0);
    pmath_unref(head);
    
    if(len == 2 && pmath_same(head, PMATH_SYMBOL_REPEATED)) { // Repeated(pat, range)
      size_t min2, max2, m;
      pmath_t obj = input->pat;
      pmath_bool_t old_associative = input->associative;
      
      input->pat = pmath_expr_get_item(obj, 1);
      input->associative = FALSE;
      _pmath_pattern_analyse(input, output);
      pmath_unref(input->pat);
      input->pat = obj;
      input->associative = old_associative;
      
      min2 = 1;
      max2 = SIZE_MAX;
      obj = pmath_expr_get_item(input->pat, 2);
      extract_range(obj, &min2, &max2, TRUE);
      pmath_unref(obj);
      
//      if(min2 == SIZE_MAX)
//        output->min = SIZE_MAX;
//      else if(output->min < SIZE_MAX){
//        output->min*= min2;
//      }

      m = output->min * min2;
      if(min2 && m / min2 != output->min)
        output->min = SIZE_MAX;
      else
        output->min = m;
        
//      if(max2 == SIZE_MAX)
//        output->max = SIZE_MAX;
//      else if(output->max < SIZE_MAX)
//        output->max*= max2;

      m = output->max * max2;
      if(max2 && m / max2 != output->max)
        output->max = SIZE_MAX;
      else
        output->max = m;
        
      output->no_sequence = FALSE;
    }
    else if(len == 2 && pmath_same(head, PMATH_SYMBOL_PATTERN)) { // name: pat
      pmath_t tmppat = input->pat;
      input->pat = pmath_expr_get_item(tmppat, 2);
      _pmath_pattern_analyse(input, output);
      pmath_unref(input->pat);
      input->pat = tmppat;
    }
    else if( (len == 2 && pmath_same(head, PMATH_SYMBOL_TESTPATTERN)) || // pat ? testfn
             (len == 1 && pmath_same(head, PMATH_SYMBOL_HOLDPATTERN)))   // pat /? testexpr
    {
      pmath_t tmppat = input->pat;
      input->pat = pmath_expr_get_item(tmppat, 1);
      _pmath_pattern_analyse(input, output);
      pmath_unref(input->pat);
      input->pat = tmppat;
    }
    else if(len > 0 && pmath_same(head, PMATH_SYMBOL_ALTERNATIVES)) { // Alternatives(...)
      size_t res_min = SIZE_MAX;
      size_t res_max = 0;
      pmath_t tmppat = input->pat;
      size_t i;
      for(i = pmath_expr_length(input->pat); i > 0; --i) {
        input->pat = pmath_expr_get_item(tmppat, i);
        
        _pmath_pattern_analyse(input, output);
        
        if(output->min < res_min)
          res_min = output->min;
        if(output->max > res_max)
          res_max = output->max;
          
        pmath_unref(input->pat);
      }
      input->pat = tmppat;
      output->min = res_min;
      output->max = res_max;
    }
    else if(pmath_same(head, MAGIC_PATTERN_FOUND) && (len == 1 || len == 2)) { // FOUND(value) or FOUND(value, pattern)
      pmath_t value = pmath_expr_get_item(input->pat, 1);
      if(pmath_is_expr(value)) {
        head = pmath_expr_get_item(value, 0);
        pmath_unref(head);
        
        if(input->associative && pmath_same(head, input->parent_pat_head)) {
          output->no_sequence = TRUE;
          output->min = output->max = pmath_expr_length(value);
        }
        else if(pmath_same(head, MAGIC_PATTERN_SEQUENCE)) {
          output->min =
            output->max = pmath_expr_length(value);
        }
      }
      pmath_unref(value);
    }
    else if(pmath_same(head, PMATH_SYMBOL_PATTERNSEQUENCE)) {
      _pmath_pattern_analyse_output_t  out2;
      pmath_t tmppat = input->pat;
      size_t i;
      
      output->min = 0;
      output->max = 0;
      
      for(i = pmath_expr_length(input->pat); i > 0; --i) {
        input->pat = pmath_expr_get_item(tmppat, i);
        
        _pmath_pattern_analyse(input, &out2);
        
        if(output->min <= SIZE_MAX - out2.min)
          output->min += out2.min;
        else
          output->min = SIZE_MAX;
          
        if(output->max <= SIZE_MAX - out2.max)
          output->max += out2.max;
        else
          output->max = SIZE_MAX;
          
        pmath_unref(input->pat);
      }
      
      input->pat = tmppat;
    }
    else if(len <= 1 && pmath_same(head, PMATH_SYMBOL_OPTIONSPATTERN)) { // OptionsPattern() or OptionsPattern(fn)
      output->min = 0;
      output->max = SIZE_MAX;
    }
    else if( input->associative &&
             len <= 1           &&
             pmath_same(head, PMATH_SYMBOL_SINGLEMATCH))  // ~ or ~:type
    {
      output->max = SIZE_MAX;
      output->no_sequence = TRUE;
      output->longest = FALSE;
    }
    else if(pmath_same(head, PMATH_SYMBOL_OPTIONAL) && (len == 1 || len == 2)) { // Optional(name), Optional(name, value)
      output->min = 0;
//      output->longest = TRUE;

//      if(input->associative){
//        output->max = SIZE_MAX;
//        output->no_sequence = 1;
//      }
    }
    else if(len == 1 && pmath_same(head, PMATH_SYMBOL_LONGEST)) { // Longest(pat)
      pmath_t tmppat = input->pat;
      input->pat = pmath_expr_get_item(tmppat, 1);
      _pmath_pattern_analyse(input, output);
      pmath_unref(input->pat);
      input->pat = tmppat;
      
      output->longest = TRUE;
    }
    else if(len == 1 && pmath_same(head, PMATH_SYMBOL_SHORTEST)) { // Shortest(pat)
      pmath_t tmppat = input->pat;
      input->pat = pmath_expr_get_item(tmppat, 1);
      _pmath_pattern_analyse(input, output);
      pmath_unref(input->pat);
      input->pat = tmppat;
      
      output->longest = FALSE;
    }
    else if(len == 2 && pmath_same(head, PMATH_SYMBOL_EXCEPT)) { // Except(no, pat)
      pmath_t tmppat = input->pat;
      input->pat = pmath_expr_get_item(tmppat, 2);
      _pmath_pattern_analyse(input, output);
      pmath_unref(input->pat);
      input->pat = tmppat;
    }
  }
}

typedef struct {
  pattern_info_t      *info;
  pmath_expr_t   pat;                  // wont be freed
  pmath_expr_t   func;                 // wont be freed
  
  _pmath_pattern_analyse_output_t  analysed;
} match_repeated_data_t;

static match_kind_t match_repeated_left( // for non-symmetric functions
  match_repeated_data_t  *data,
  size_t                  func_start
) {
  size_t flen = pmath_expr_length(data->func);
  size_t n;
  
#ifdef DEBUG_LOG_MATCH
  debug_indent(); pmath_debug_print("match_repeated_left ...\n");
#endif
  
  while(func_start <= flen) {
  NEXT_ARG:
    if(data->analysed.max > flen - func_start + 1)
      data->analysed.max = flen - func_start + 1;
      
    for(n = data->analysed.longest ? data->analysed.max : data->analysed.min
            ; data->analysed.min <= n && n <= data->analysed.max
        ; data->analysed.longest ? --n : ++n
       ) {
      pmath_t arg = PMATH_NULL;
      match_kind_t kind;
      
      if(n == 1)
        arg = pmath_expr_get_item(data->func, func_start);
      else
        arg = pmath_expr_set_item(
                pmath_expr_get_item_range(data->func, func_start, n),
                0, MAGIC_PATTERN_SEQUENCE);
                
      kind = match_atom(data->info, data->pat, arg, func_start, flen);
      pmath_unref(arg);
      
      if(kind == PMATH_MATCH_KIND_GLOBAL)
        return PMATH_MATCH_KIND_GLOBAL;
        
      if(kind == PMATH_MATCH_KIND_LOCAL) {
        if( ( data->analysed.longest && n == data->analysed.min) ||
            (!data->analysed.longest && n == data->analysed.max))
        {
          if(func_start + n <= flen) {
            func_start += n;
            goto NEXT_ARG;
          }
        }
        
        kind = match_repeated_left(data, func_start + n);
        if(kind != PMATH_MATCH_KIND_NONE)
          return kind;
      }
    }
    
    return PMATH_MATCH_KIND_NONE;
  }
  
  return PMATH_MATCH_KIND_LOCAL;
}

static match_kind_t match_repeated(
  pattern_info_t     *info,
  pmath_t      pat,   // wont be freed
  pmath_expr_t  arg    // wont be freed
) {
  _pmath_pattern_analyse_input_t   analyse_in;
  match_repeated_data_t data;
  match_kind_t kind;
  
  data.info = info;
  data.pat  = pat;
  data.func = arg;
  
  memset(&analyse_in, 0, sizeof(_pmath_pattern_analyse_input_t));
  analyse_in.pat             = pat;
  analyse_in.parent_pat_head = PMATH_NULL;
  
  _pmath_pattern_analyse(&analyse_in, &data.analysed);
  
  if(data.analysed.max == 0) {
    if(pmath_expr_length(arg) == 0)
      return PMATH_MATCH_KIND_LOCAL;
      
    return PMATH_MATCH_KIND_NONE;
  }
  
  if(data.analysed.min == 0)
    data.analysed.min = 1;
    
#ifdef DEBUG_LOG_MATCH
  debug_indent(); pmath_debug_print("match_repeated ...\n");
  debug_indent(); pmath_debug_print_object("pat: ", pat, "\n");
  debug_indent(); pmath_debug_print_object("arg: ", arg, "\n");
  ++indented;
#endif
  
  kind = match_repeated_left(&data, 1);
  
#ifdef DEBUG_LOG_MATCH
  --indented;
  debug_indent(); pmath_debug_print("... match_repeated %d\n", kind);
#endif
  return kind;
}

static match_kind_t match_func_left( // for non-symmetric functions
  match_func_data_t  *data,
  size_t              pat_start,
  size_t              func_start
) {
  const size_t plen = pmath_expr_length(data->pat);
  const size_t flen = pmath_expr_length(data->func);
  size_t first_pat_func_start = pat_start == 1 ? 1 : 0;
  // this call is the first one for the pattern--^   ^
  //                                                 |- used as a flag!
  // the caller is match_func_left itself => pat_start > 1
  
  _pmath_pattern_analyse_input_t patarg;
  patarg.parent_pat_head = data->info->current_head;
  patarg.associative = data->associative;
  
#ifdef DEBUG_LOG_MATCH
  debug_indent(); pmath_debug_print("match_func_left ...\n");
#endif
  
NEXT_PATARG:
#ifdef DEBUG_LOG_MATCH
  debug_indent(); pmath_debug_print("next pat arg (%"PRIuPTR")\n", pat_start);
#endif
  
  if(pat_start > plen) {
    if(data->info->associative && pmath_same(data->func, data->info->func)) {
      data->info->assoc_end = func_start - 1;
#ifdef DEBUG_LOG_MATCH
      debug_indent(); pmath_debug_print("assoc [%"PRIuPTR" .. %"PRIuPTR"]\n",
                                        data->info->assoc_start,
                                        data->info->assoc_end);
#endif
      if(data->info->assoc_start <= data->info->assoc_end)
        return PMATH_MATCH_KIND_LOCAL;
      return PMATH_MATCH_KIND_NONE;
    }
    if(func_start > flen)
      return PMATH_MATCH_KIND_LOCAL;
    return PMATH_MATCH_KIND_NONE;
  }
  
FIRST_PATARG: ;
  {
    _pmath_pattern_analyse_output_t patarg_out;
    size_t n;
    
    patarg.pat = pmath_expr_get_item(data->pat, pat_start);
    _pmath_pattern_analyse(&patarg, &patarg_out);
    
  NEXT_FUNCARG:
#ifdef DEBUG_LOG_MATCH
    debug_indent(); pmath_debug_print("next func arg (%"PRIuPTR")\n",
                                      func_start);
#endif
                                      
    if(func_start > flen)
      patarg_out.max = 0;
    else if(patarg_out.max > flen - func_start + 1)
      patarg_out.max = flen - func_start + 1;
      
    for(n = patarg_out.longest ? patarg_out.max : patarg_out.min
            ; patarg_out.min <= n && n <= patarg_out.max
        ; patarg_out.longest ? --n : ++n
       ) {
      pmath_t arg = PMATH_NULL;
      match_kind_t kind;
      
      if(patarg_out.no_sequence) {
        if(n == 1 && data->one_identity)
          arg = pmath_expr_get_item(data->func, func_start);
        else
          arg = pmath_expr_get_item_range(data->func, func_start, n);
      }
      else {
        if(n == 1)
          arg = pmath_expr_get_item(data->func, func_start);
        else
          arg = pmath_expr_set_item(
                  pmath_expr_get_item_range(data->func, func_start, n),
                  0, MAGIC_PATTERN_SEQUENCE);
      }
      
      kind = match_atom(data->info, patarg.pat, arg, func_start, flen);
      pmath_unref(arg);
      if(kind == PMATH_MATCH_KIND_GLOBAL) {
        pmath_unref(patarg.pat);
        return PMATH_MATCH_KIND_GLOBAL;
      }
      
      if(kind == PMATH_MATCH_KIND_LOCAL) {
        if( ( patarg_out.longest && n == patarg_out.min) ||
            (!patarg_out.longest && n == patarg_out.max))
        {
          pmath_unref(patarg.pat);
          ++pat_start;
          func_start += n;
          goto NEXT_PATARG;
        }
        
        // *pat = pmath_expr_set_item(*pat, pat_start, pmath_ref(pati));
        kind = match_func_left(data, pat_start + 1, func_start + n);
        if(kind != PMATH_MATCH_KIND_NONE) {
          pmath_unref(patarg.pat);
          return kind;
        }
      }
    }
    
    if( data->info->associative                  &&
        pmath_same(data->func, data->info->func) &&
        func_start + patarg_out.min - 1 <= flen)
    {
      if(pat_start == 1) {
        data->info->assoc_start = first_pat_func_start = ++func_start;
        goto NEXT_FUNCARG;
      }
      else if(first_pat_func_start > 0) {
        data->info->assoc_start = func_start = ++first_pat_func_start;
        pat_start = 1;
#ifdef DEBUG_LOG_MATCH
        debug_indent(); pmath_debug_print("first pat arg\n");
#endif
        pmath_unref(patarg.pat);
        goto FIRST_PATARG;
      }
    }
    
    pmath_unref(patarg.pat);
    return PMATH_MATCH_KIND_NONE;
  }
}

/*============================================================================*/

static __inline pmath_bool_t index_start(
  size_t  *indices,
  char    *args_in_use,
  size_t   indices_len,
  size_t   max_index
) {
  size_t i, j;
  
  assert(indices_len <= max_index);
  
  for(i = 0, j = 0; i < indices_len; ++i) {
    while(j < max_index && args_in_use[j])
      ++j;
    if(j == max_index)
      return FALSE;
      
    *indices++ = j + 1;
    args_in_use[j] = TESTING_USE;
  }
  return TRUE;
}

static pmath_bool_t index_next(
  size_t  *indices,
  char    *args_in_use,
  size_t   indices_len,
  size_t   max_index
) {
  size_t i, j;
  
  assert(indices_len <= max_index);
  
  --indices;     // indexing this array `indices`     from 1 to indices_len.
  --args_in_use; // indexing this array `args_in_use` from 1 to max_index.
  for(i = indices_len; i > 0; --i) {
    assert(indices[i] >= i);
    
    j = indices[i] + 1; // j >= i + 1 >= 2
    while(j <= max_index && args_in_use[j])
      ++j;
      
    if(indices[i] >= j)
      return FALSE;
      
    if(j <= max_index) {
      assert(args_in_use[indices[i]] == TESTING_USE);
      
      args_in_use[indices[i]] = NOT_IN_USE;
      indices[i] = j;
      args_in_use[j] = TESTING_USE;
      return TRUE;
    }
    max_index = j - 2;
  }
  for(i = 1; i <= indices_len; ++i)
    args_in_use[indices[i]] = NOT_IN_USE;
  return FALSE;
}

static match_kind_t match_func_symmetric(
  match_func_data_t  *data
) {
  const size_t plen = pmath_expr_length(data->pat);
  const size_t flen = pmath_expr_length(data->func);
  
  char *args_in_use;
  size_t *indices;
  size_t i, n, j;
  
#ifdef DEBUG_LOG_MATCH
  debug_indent(); pmath_debug_print("match_func_symmetric ...\n");
#endif
  
  args_in_use = (char *)pmath_mem_alloc(flen);
  if(!args_in_use)
    return PMATH_MATCH_KIND_NONE;
    
  memset(args_in_use, NOT_IN_USE, flen);
  
  indices = (size_t *)pmath_mem_alloc(flen * sizeof(size_t));
  if(!indices)
    return PMATH_MATCH_KIND_NONE;
    
  for(i = 1; i <= plen; ++i) {
    _pmath_pattern_analyse_input_t  patarg;
    _pmath_pattern_analyse_output_t patarg_out;
    patarg.parent_pat_head = data->info->current_head;
    patarg.associative = data->associative;
    patarg.pat = pmath_expr_get_item(data->pat, i);
    
#ifdef DEBUG_LOG_MATCH
    debug_indent(); pmath_debug_print("next pat arg (%"PRIuPTR")\n", i);
#endif
    
    _pmath_pattern_analyse(&patarg, &patarg_out);
    
    if(patarg_out.max > flen)
      patarg_out.max = flen;
      
    for(n = patarg_out.longest ? patarg_out.max : patarg_out.min
            ; patarg_out.min <= n && n <= patarg_out.max
        ; patarg_out.longest ? --n : ++n
       ) {
      if(index_start(indices, args_in_use, n, flen)) do {
          pmath_t arg = PMATH_NULL;
          match_kind_t kind;
          
          if(n == 1 && (!patarg_out.no_sequence || data->one_identity)) {
            arg = pmath_expr_get_item(data->func, *indices);
          }
          else {
            if(patarg_out.no_sequence)
              arg = pmath_expr_new(pmath_ref(data->info->current_head), n);
            else
              arg = pmath_expr_new(MAGIC_PATTERN_SEQUENCE, n);
              
            for(j = 0; j < n; ++j)
              arg = pmath_expr_set_item(
                      arg, j + 1,
                      pmath_expr_get_item(data->func, indices[j]));
                      
          }
          
#ifdef DEBUG_LOG_MATCH
          debug_indent(); pmath_debug_print_object("arg: ", arg, "\n");
          debug_indent(); show_indices(  "indices: ", indices, n, "\n");
          debug_indent(); show_arg_usage("arg usage: ", args_in_use, flen, "\n\n");
#endif
          
          kind = match_atom(data->info, patarg.pat, arg, 1, flen);
          pmath_unref(arg);
          
          if(kind == PMATH_MATCH_KIND_GLOBAL) {
            pmath_mem_free(args_in_use);
            pmath_mem_free(indices);
            pmath_unref(patarg.pat);
            return PMATH_MATCH_KIND_GLOBAL;
          }
          
          if(kind == PMATH_MATCH_KIND_LOCAL) {
            for(j = 0; j < n; ++j)
              args_in_use[indices[j] - 1] = IN_USE;
              
            goto NEXT_PATARG;
          }
          
        } while(index_next(indices, args_in_use, n, flen));
    }
    
    if( data->associative  &&
        data->one_identity &&
        patarg_out.min > 1 &&
        patarg_out.max >= patarg_out.min)
    {
#ifdef DEBUG_LOG_MATCH
      debug_indent(); pmath_debug_print("... retry with n = 1 ...\n");
#endif
      
      n = 1;
      
      if(index_start(indices, args_in_use, n, flen)) {
        do {
          pmath_t arg = PMATH_NULL;
          match_kind_t kind;
          
          arg = pmath_expr_get_item(data->func, *indices);
          
#ifdef DEBUG_LOG_MATCH
          debug_indent(); pmath_debug_print_object("arg: ", arg, "\n");
          debug_indent(); show_indices(  "indices: ", indices, 1, "\n");
          debug_indent(); show_arg_usage("arg usage: ", args_in_use, flen, "\n\n");
#endif
          
          kind = match_atom(data->info, patarg.pat, arg, 1, flen);
          pmath_unref(arg);
          
          if(kind == PMATH_MATCH_KIND_GLOBAL) {
            pmath_mem_free(args_in_use);
            pmath_mem_free(indices);
            pmath_unref(patarg.pat);
            return PMATH_MATCH_KIND_GLOBAL;
          }
          
          if(kind == PMATH_MATCH_KIND_LOCAL) {
            for(j = 0; j < n; ++j)
              args_in_use[indices[j] - 1] = IN_USE;
              
            goto NEXT_PATARG;
          }
          
        } while(index_next(indices, args_in_use, n, flen));
      }
    }
    
    pmath_unref(patarg.pat);
    pmath_mem_free(args_in_use);
    pmath_mem_free(indices);
    return PMATH_MATCH_KIND_NONE;
    
  NEXT_PATARG:
    pmath_unref(patarg.pat);
  }
  
  if(pmath_same(data->info->func, data->func)) {
    if(!data->info->symmetric) {
      pmath_debug_print(
        "[%s, %d] data->info->symmetric is expected to be TRUE. "
        "Maybe another thread changed the Symmetric attribute of the function?", __FILE__, __LINE__);
    }
    memcpy(data->info->arg_usage, args_in_use, flen);
  }
  else {
    for(i = 0; i < flen; ++i) {
      if(!args_in_use[i]) {
        pmath_mem_free(args_in_use);
        pmath_mem_free(indices);
        return PMATH_MATCH_KIND_NONE;
      }
    }
  }
  pmath_mem_free(args_in_use);
  pmath_mem_free(indices);
  return PMATH_MATCH_KIND_LOCAL;
}

static match_kind_t match_func(
  pattern_info_t  *info,
  pmath_expr_t     pat,
  pmath_expr_t     func
) {
  pmath_t phead = pmath_expr_get_item(pat, 0);
  pmath_t old_head = info->current_head;
  match_kind_t kind;
  
  info->current_head = pmath_expr_get_item(func, 0);
  
  kind = match_atom(info, phead, info->current_head, 0, pmath_expr_length(func));
  if(kind == PMATH_MATCH_KIND_LOCAL) {
    match_func_data_t data;
    
#ifdef DEBUG_LOG_MATCH
    debug_indent(); pmath_debug_print("match_func ...\n");
    debug_indent(); pmath_debug_print_object("pat:  ", pat, "\n");
    debug_indent(); pmath_debug_print_object("func: ", func, "\n");
    ++indented;
#endif
    
    data.info = info;
    data.pat  = pat;
    data.func = func;
    data.associative = FALSE;
    data.one_identity = FALSE;
    data.symmetric = FALSE;
    if(pmath_is_symbol(phead)) {
      pmath_symbol_attributes_t attrib = pmath_symbol_get_attributes(phead);
      
      data.associative  = (attrib & PMATH_SYMBOL_ATTRIBUTE_ASSOCIATIVE) != 0;
      data.one_identity = (attrib & PMATH_SYMBOL_ATTRIBUTE_ONEIDENTITY) != 0;
      data.symmetric    = (attrib & PMATH_SYMBOL_ATTRIBUTE_SYMMETRIC)   != 0;
    }
    
    if(!info->symmetric && info->associative && pmath_same(func, info->func))
      info->assoc_start = 1;
      
    if(data.symmetric)
      kind = match_func_symmetric(&data);
    else
      kind = match_func_left(&data, 1, 1);
      
#ifdef DEBUG_LOG_MATCH
    --indented;
    debug_indent(); pmath_debug_print("... match_func %d\n", kind);
#endif
  }
  
  pmath_unref(phead);
  pmath_unref(info->current_head);
  info->current_head = old_head;
  return kind;
}

static pmath_bool_t varlist_to_hashtable(
  pmath_hashtable_t hashtable, // entries are pmath_ht_obj_entry_t*
  pmath_expr_t      varlist    // will be freed; form: <T> = <VariableName>(<Value>,<T>)
) {
  pmath_expr_t next;
  
  while(!pmath_is_null(varlist)) {
    struct _pmath_object_entry_t *entry =
      pmath_mem_alloc(sizeof(struct _pmath_object_entry_t));
      
    if(!entry) {
      pmath_unref(varlist);
      return FALSE;
    }
    
    entry->key   = pmath_expr_get_item(varlist, 0);
    entry->value = pmath_expr_get_item(varlist, 1);
    
    entry = pmath_ht_insert(hashtable, entry);
    if(entry)
      pmath_ht_obj_class.entry_destructor(entry);
      
    next = pmath_expr_get_item(varlist, 2);
    pmath_unref(varlist);
    varlist = next;
  }
  return TRUE;
}

//} ... match patterns
//{ replace expression parts ...

static pmath_t replace_pattern_var(
  pmath_t pattern,          // will be freed
  pmath_t invoking_pattern,
  pmath_t name,
  pmath_t value
) {
  pmath_t head;
  size_t i, len;
  
  if(pmath_same(invoking_pattern, pattern)) {
    pmath_unref(pattern);
    return pmath_expr_new_extended(
             MAGIC_PATTERN_FOUND, 1, pmath_ref(value));
  }
  
  if(!pmath_is_expr(pattern))
    return pattern;
    
  len = pmath_expr_length(pattern);
  head = pmath_expr_get_item(pattern, 0);
  pmath_unref(head);
  
  if(len == 2 && pmath_same(head, PMATH_SYMBOL_PATTERN)) { // Pattern(n, p)
    pmath_t pat_name = pmath_expr_get_item(pattern, 1);
    pmath_t new_pat = replace_pattern_var(
                        pmath_expr_get_item(pattern, 2),
                        invoking_pattern,
                        name,
                        value);
                        
    // Pattern(name, value) -> FOUND(value)
    // Pattern(name, p) -> FOUND(value, p)
    if(pmath_equals(pat_name, name)) {
      if(pmath_equals(new_pat, value)) {
        pmath_unref(pattern);
        pmath_unref(pat_name);
        return pmath_expr_new_extended(MAGIC_PATTERN_FOUND, 1, new_pat);
      }
      
      pattern = pmath_expr_set_item(pattern, 0, MAGIC_PATTERN_FOUND);
      pattern = pmath_expr_set_item(pattern, 1, pmath_ref(value));
    }
    
    pmath_unref(pat_name);
    return pmath_expr_set_item(pattern, 2, new_pat);
  }
  
  if(len == 2 && pmath_same(head, MAGIC_PATTERN_FOUND)) { // FOUND(v, p)
    pmath_t pat_val = pmath_expr_get_item(pattern, 1);
    pmath_t new_pat = replace_pattern_var(
                        pmath_expr_get_item(pattern, 2),
                        invoking_pattern,
                        name,
                        value);
                        
    // FOUND(x, x) -> FOUND(x)
    if(pmath_equals(pat_val, new_pat)) {
      pmath_unref(pat_val);
      pmath_unref(pattern);
      return pmath_expr_new_extended(MAGIC_PATTERN_FOUND, 1, new_pat);
    }
    
    pmath_unref(pat_val);
    return pmath_expr_set_item(pattern, 2, new_pat);
  }
  
  if(len == 1 && pmath_same(head, MAGIC_PATTERN_FOUND))
    return pattern;
    
  if(len == 2 && pmath_same(head, PMATH_SYMBOL_CONDITION)) { //  pat // cond
    pattern = pmath_expr_set_item(
                pattern, 1,
                replace_pattern_var(
                  pmath_expr_extract_item(pattern, 1),
                  invoking_pattern,
                  name,
                  value));
                  
    pattern = pmath_expr_set_item(
                pattern, 2,
                _pmath_replace_local(
                  pmath_expr_extract_item(pattern, 2),
                  name,
                  value));
                  
    return pattern;
  }
  
  for(i = 0; i <= len; i++) {
    pattern = pmath_expr_set_item(pattern, i,
                                  replace_pattern_var(
                                    pmath_expr_get_item(pattern, i),
                                    invoking_pattern,
                                    name,
                                    value));
  }
  
  return pattern;
}

static pmath_bool_t replace_exact_once(
  pmath_t *pattern,
  pmath_t  _old,
  pmath_t  _new
) {
  size_t i, len;
  
  if(pmath_same(*pattern, _old)) {
    pmath_unref(*pattern);
    *pattern = pmath_expr_new_extended(
                 MAGIC_PATTERN_FOUND, 1, pmath_ref(_new));
    return TRUE;
  }
  if(!pmath_is_expr(*pattern))
    return FALSE;
    
  len = pmath_expr_length(*pattern);
  for(i = len + 1; i > 0; --i) {
    pmath_t pat = pmath_expr_get_item(*pattern, i - 1);
    if(replace_exact_once(&pat, _old, _new)) {
      *pattern = pmath_expr_set_item(*pattern, i - 1, pat);
      return TRUE;
    }
    pmath_unref(pat);
  }
  return FALSE;
}

// retains debug-info
static pmath_t replace_option_value(
  pmath_t      body,            // will be freed
  pmath_t      default_head,
  pmath_expr_t optionvaluerules // form: <T> = <Function>(<OptionValueRules>,<T>)
) {
  pmath_t head, debug_info;
  size_t i, len;
  
  if(!pmath_is_expr(body) || pmath_is_null(optionvaluerules))
    return body;
    
  len        = pmath_expr_length(body);
  head       = pmath_expr_get_item(body, 0);
  debug_info = _pmath_expr_get_debug_info(body);
  
  if(pmath_same(head, PMATH_SYMBOL_OPTIONVALUE) && (len == 1 || len == 2 || len == 4)) {
    pmath_t current_fn;
    pmath_expr_t iter_optionvaluerules;
    
    if(len == 4) {
      // OptionValue(Automatic, Automatic, name, h)
      pmath_t rules = pmath_expr_get_item(body, 2);
      pmath_unref(rules);
      
      if(pmath_same(rules, PMATH_SYMBOL_AUTOMATIC)) {
        current_fn = pmath_expr_get_item(body, 1);
        
        if(pmath_same(current_fn, PMATH_SYMBOL_AUTOMATIC)) {
          pmath_unref(current_fn);
          
          current_fn = pmath_ref(default_head);
        }
      }
      else
        current_fn = PMATH_UNDEFINED;
    }
    else if(len == 1)
      current_fn = pmath_ref(default_head);
    else
      current_fn = pmath_expr_get_item(body, 1);
      
    iter_optionvaluerules = pmath_ref(optionvaluerules);
    do {
      pmath_expr_t tmp;
      pmath_t fn = pmath_expr_get_item(iter_optionvaluerules, 0);
      
      if(pmath_equals(fn, current_fn)) {
        pmath_t arg = pmath_expr_get_item(iter_optionvaluerules, 1);
        
        if(pmath_is_expr(arg)) {
          pmath_t arghead = pmath_expr_get_item(arg, 0);
          pmath_unref(arghead);
          if(pmath_same(arghead, MAGIC_PATTERN_SEQUENCE))
            arg = pmath_expr_set_item(
                    arg, 0,
                    pmath_ref(PMATH_SYMBOL_LIST));
                    
          if(len == 4) {
            body = pmath_expr_set_item(body, 1, current_fn);
            body = pmath_expr_set_item(body, 2, arg);
          }
          else {
            tmp = body;
            body = pmath_expr_new_extended(
                     pmath_ref(PMATH_SYMBOL_OPTIONVALUE), 3,
                     current_fn,
                     arg,
                     pmath_expr_get_item(tmp, len));
            pmath_unref(tmp);
          }
          
          body = _pmath_expr_set_debug_info(body, debug_info);
          
          pmath_unref(fn);
          pmath_unref(iter_optionvaluerules);
          pmath_unref(head);
          return body;
        }
        pmath_unref(arg);
      }
      pmath_unref(fn);
      
      tmp = iter_optionvaluerules;
      iter_optionvaluerules = pmath_expr_get_item(tmp, 2);
      pmath_unref(tmp);
    } while(!pmath_is_null(iter_optionvaluerules));
    
    pmath_unref(current_fn);
  }
  
  body = pmath_expr_set_item(
           body, 0,
           replace_option_value(head, default_head, optionvaluerules));
           
  for(i = 1; i <= len; ++i) {
    body = pmath_expr_set_item(
             body, i,
             replace_option_value(
               pmath_expr_extract_item(body, i),
               default_head,
               optionvaluerules));
  }

  body = _pmath_expr_set_debug_info(body, debug_info);
  
  return body;
}

static pmath_bool_t contains(
  pmath_t object,  // wont be freed
  pmath_t rep      // wont be freed
) {
  size_t i, len;
  
  if(pmath_equals(object, rep))
    return TRUE;
    
  if(!pmath_is_expr(object))
    return FALSE;
    
  len = pmath_expr_length(object);
  for(i = 0; i <= len; ++i) {
    pmath_t item = pmath_expr_get_item(object, i);
    pmath_bool_t result = contains(item, rep);
    pmath_unref(item);
    
    if(result)
      return TRUE;
  }
  
  return FALSE;
}

PMATH_PRIVATE pmath_bool_t _pmath_contains_any(
  pmath_t           object,       // wont be freed
  pmath_hashtable_t replacements  // entries are pmath_ht_obj_entry_t*
) {
  struct _pmath_object_entry_t *entry = pmath_ht_search(replacements, &object);
  size_t i, len;
  
  if(entry)
    return TRUE;
    
  if(!pmath_is_expr(object))
    return FALSE;
    
  len = pmath_expr_length(object);
  for(i = 0; i <= len; ++i) {
    pmath_t item = pmath_expr_get_item(object, i);
    pmath_bool_t result = _pmath_contains_any(item, replacements);
    pmath_unref(item);
    
    if(result)
      return TRUE;
  }
  
  return FALSE;
}

// retains debug-info
static void preprocess_local_one(
  pmath_expr_t *local_expr,
  pmath_t      *def
) {
  if(pmath_is_symbol(*def)) {
    pmath_symbol_t newsym = pmath_symbol_create_temporary(
                              pmath_symbol_name(*def),
                              FALSE);
                              
    *local_expr = _pmath_replace_local(*local_expr, *def, newsym);
    
    pmath_unref(*def);
    *def = newsym;
  }
  else if(pmath_is_expr(*def) &&
          pmath_expr_length(*def) == 2)
  {
    pmath_t obj = pmath_expr_get_item(*def, 0);
    pmath_unref(obj);
    
    if( pmath_same(obj, PMATH_SYMBOL_ASSIGN) ||
        pmath_same(obj, PMATH_SYMBOL_ASSIGNDELAYED))
    {
      obj = pmath_expr_get_item(*def, 1);
      
      if(pmath_is_symbol(obj)) {
        pmath_t debug_info = _pmath_expr_get_debug_info(*def);
        pmath_symbol_t newsym = pmath_symbol_create_temporary(
                                  pmath_symbol_name(obj),
                                  FALSE);
                                  
        *local_expr = _pmath_replace_local(*local_expr, obj, newsym);
        
        *def = pmath_expr_set_item(*def, 1, newsym);
        *def = _pmath_expr_set_debug_info(*def, debug_info);
      }
      
      pmath_unref(obj);
    }
  }
}

// retains debug-info
PMATH_PRIVATE pmath_expr_t _pmath_preprocess_local(
  pmath_expr_t local_expr // will be freed.
) {
  pmath_expr_t defs = pmath_expr_get_item(local_expr, 1);
  pmath_t debug_info;
  size_t i;
  
  local_expr = pmath_expr_set_item(local_expr, 1, PMATH_NULL);
  
  if(pmath_is_expr_of(defs, PMATH_SYMBOL_LIST)) {
    debug_info = _pmath_expr_get_debug_info(defs);
    
    for(i = pmath_expr_length(defs); i > 0; --i) {
      pmath_t def = pmath_expr_get_item(defs, i);
    
      defs = pmath_expr_set_item(defs, i, PMATH_NULL);
      preprocess_local_one(&local_expr, &def);
      defs = pmath_expr_set_item(defs, i, def);
    }
    
    defs = _pmath_expr_set_debug_info(defs, debug_info);
  }
  else
    preprocess_local_one(&local_expr, &defs);
    
  debug_info = _pmath_expr_get_debug_info(local_expr);
  local_expr = pmath_expr_set_item(local_expr, 1, defs);
  local_expr = _pmath_expr_set_debug_info(local_expr, debug_info);
  
  return local_expr;
}

// retains debug-info
PMATH_PRIVATE pmath_t _pmath_replace_local(
  pmath_t  object, // will be freed
  pmath_t  name,
  pmath_t  value
) {
  pmath_bool_t do_flatten;
  pmath_t item, debug_info;
  size_t i, len;
  
  if(pmath_equals(object, name)) {
    pmath_unref(object);
    return pmath_ref(value);
  }
  
  if(!pmath_is_expr(object))
    return object;
    
  debug_info = _pmath_expr_get_debug_info(object);
  
  item = pmath_expr_get_item(object, 0);
  
  if( (pmath_same(item, PMATH_SYMBOL_FUNCTION) ||
       pmath_same(item, PMATH_SYMBOL_LOCAL)    ||
       pmath_same(item, PMATH_SYMBOL_WITH)) &&
      pmath_expr_length(object) > 1         &&
      contains(object, name))
  {
    pmath_unref(item);
    object = _pmath_preprocess_local(object);
  }
  else {
    item = _pmath_replace_local(item, name, value);
    
    object = pmath_expr_set_item(object, 0, item);
  }
  
  do_flatten = FALSE;
  
  len = pmath_expr_length(object);
  for(i = 1; i <= len; ++i) {
    item = pmath_expr_extract_item(object, i);
    
    item = _pmath_replace_local(item, name, value);
    
    if(i != 0 && !do_flatten) {
      do_flatten = pmath_is_expr_of(item, MAGIC_PATTERN_SEQUENCE);
    }
    
    object = pmath_expr_set_item(object, i, item);
  }
  
  if(do_flatten)
    object = pmath_expr_flatten(object, MAGIC_PATTERN_SEQUENCE, 1);
    
  object = _pmath_expr_set_debug_info(object, debug_info);
  
  return object;
}

// retains debug-info
static pmath_t replace_multiple(
  pmath_t           object,        // will be freed
  pmath_hashtable_t replacements   // entries are pmath_ht_obj_entry_t*
) {
  struct _pmath_object_entry_t *entry = pmath_ht_search(replacements, &object);
  pmath_bool_t do_flatten;
  pmath_t item, debug_info;
  size_t i, len;
  
  if(entry) {
    pmath_unref(object);
    return pmath_ref(entry->value);
  }
  
  if(!pmath_is_expr(object))
    return object;
    
  debug_info = _pmath_expr_get_debug_info(object);
  item       = pmath_expr_get_item(object, 0);
  
  if( (pmath_same(item, PMATH_SYMBOL_FUNCTION) ||
       pmath_same(item, PMATH_SYMBOL_LOCAL)    ||
       pmath_same(item, PMATH_SYMBOL_WITH)) &&
      pmath_expr_length(object) > 1         &&
      _pmath_contains_any(object, replacements))
  {
    pmath_unref(item);
    
    object = _pmath_preprocess_local(object);
  }
  else {
    item = replace_multiple(item, replacements);
    
    object = pmath_expr_set_item(object, 0, item);
  }
  
  do_flatten = FALSE;
  
  len = pmath_expr_length(object);
  for(i = 1; i <= len; ++i) {
    item = pmath_expr_extract_item(object, i);
    
    item = replace_multiple(item, replacements);
    
    if(/*i != 0 && */!do_flatten && pmath_is_expr(item)) {
      pmath_t head = pmath_expr_get_item(item, 0);
      pmath_unref(head);
      do_flatten = pmath_same(head, MAGIC_PATTERN_SEQUENCE);
    }
    
    object = pmath_expr_set_item(object, i, item);
  }
  
  if(do_flatten)
    object = pmath_expr_flatten(object, MAGIC_PATTERN_SEQUENCE, 1);
    
  object = _pmath_expr_set_debug_info(object, debug_info);
  return object;
}

//} ... replace expression parts
