#include <pmath-core/numbers.h>
#include <pmath-core/expressions-private.h>

#include <pmath-language/patterns-private.h>

#include <pmath-util/debug.h>
#include <pmath-util/dispatch-tables-private.h>
#include <pmath-util/hash/hashtables-private.h>
#include <pmath-util/helpers.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/option-helpers.h>
#include <pmath-util/memory.h>
#include <pmath-util/messages.h>

#include <pmath-util/concurrency/threads.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/lists-private.h>

#include <string.h>


// initialization in pmath_init():
PMATH_PRIVATE pmath_t _pmath_object_range_from_one; /* readonly */
PMATH_PRIVATE pmath_t _pmath_object_range_from_zero; /* readonly */
PMATH_PRIVATE pmath_t _pmath_object_singlematch; /* readonly */
PMATH_PRIVATE pmath_t _pmath_object_multimatch; /* readonly */
PMATH_PRIVATE pmath_t _pmath_object_zeromultimatch; /* readonly */
PMATH_PRIVATE pmath_t _pmath_object_empty_pattern_sequence; /* readonly */

extern pmath_symbol_t pmath_System_Alternatives;
extern pmath_symbol_t pmath_System_Assign;
extern pmath_symbol_t pmath_System_AssignDelayed;
extern pmath_symbol_t pmath_System_Automatic;
extern pmath_symbol_t pmath_System_Condition;
extern pmath_symbol_t pmath_System_Default;
extern pmath_symbol_t pmath_System_EvaluationSequence;
extern pmath_symbol_t pmath_System_Except;
extern pmath_symbol_t pmath_System_Function;
extern pmath_symbol_t pmath_System_HoldPattern;
extern pmath_symbol_t pmath_System_KeyValuePattern;
extern pmath_symbol_t pmath_System_List;
extern pmath_symbol_t pmath_System_Literal;
extern pmath_symbol_t pmath_System_Local;
extern pmath_symbol_t pmath_System_Longest;
extern pmath_symbol_t pmath_System_Optional;
extern pmath_symbol_t pmath_System_OptionsPattern;
extern pmath_symbol_t pmath_System_OptionValue;
extern pmath_symbol_t pmath_System_Pattern;
extern pmath_symbol_t pmath_System_PatternSequence;
extern pmath_symbol_t pmath_System_Repeated;
extern pmath_symbol_t pmath_System_Rule;
extern pmath_symbol_t pmath_System_RuleDelayed;
extern pmath_symbol_t pmath_System_Sequence;
extern pmath_symbol_t pmath_System_Shortest;
extern pmath_symbol_t pmath_System_SingleMatch;
extern pmath_symbol_t pmath_System_TestPattern;
extern pmath_symbol_t pmath_System_True;
extern pmath_symbol_t pmath_System_With;

extern pmath_symbol_t pmath_Internal_Condition;

//{ compare patterns ...

struct int_entry_t {
  intptr_t key;
};

struct int_int_entry_t {
  struct int_entry_t inherited;
  intptr_t value;
};

static pmath_bool_t ht_int_class__entry_keys_equal(void *e1, void *e2) {
  struct int_entry_t *entry1 = e1;
  struct int_entry_t *entry2 = e2;
  return entry1->key == entry2->key;
}

static unsigned int ht_int_class__entry_hash(void *e) {
  struct int_entry_t *entry = e;
  return _pmath_hash_pointer((void *)entry->key);
}

static pmath_bool_t ht_int_class__entry_equals_key(void *e, void *k) {
  struct int_entry_t *entry = e;
  return entry->key == (intptr_t)k;
}

static const pmath_ht_class_t ht_int_class = {
  pmath_mem_free,
  ht_int_class__entry_hash,
  ht_int_class__entry_keys_equal,
  _pmath_hash_pointer,
  ht_int_class__entry_equals_key
};

typedef struct pattern_compare_status_t {
  pmath_hashtable_t pat1_counts; // entries: struct int_int_entry_t
  pmath_hashtable_t pat2_counts; // entries: struct int_int_entry_t
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

  status.pat1_counts = pmath_ht_create(&ht_int_class, 0);
  status.pat2_counts = pmath_ht_create(&ht_int_class, 0);

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
pmath_bool_t _pmath_rhs_has_condition(
  pmath_t  *rhs,
  pmath_bool_t  adjust
) {
  pmath_t obj;

  if(!pmath_is_expr(*rhs))
    return FALSE;

  obj = pmath_expr_get_item(*rhs, 0);
  pmath_unref(obj);

  if( (pmath_same(obj, pmath_System_Condition) ||
       pmath_same(obj, pmath_Internal_Condition)) &&
      pmath_expr_length(*rhs) == 2)
  {
    if(adjust) {
      *rhs = pmath_expr_set_item(*rhs, 0, pmath_ref(pmath_Internal_Condition));
    }

    return TRUE;
  }

  if( pmath_same(obj, pmath_System_EvaluationSequence) ||
      pmath_same(obj, pmath_System_Local)              ||
      pmath_same(obj, pmath_System_With))
  {
    obj = pmath_expr_get_item(
            *rhs,
            pmath_expr_length(*rhs));

    if(_pmath_rhs_has_condition(&obj, adjust)) {
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
  struct int_int_entry_t *entry;
  
  if(!pmath_is_symbol(key))
    return 0;

  entry = pmath_ht_search(table, PMATH_AS_PTR(key));
  if(!entry) {
    entry = pmath_mem_alloc(sizeof(struct int_int_entry_t));

    if(entry) {
      entry->inherited.key = (intptr_t)PMATH_AS_PTR(key);
      entry->value = 1;
      entry = pmath_ht_insert(table, entry);

      if(entry)
        pmath_mem_free(entry);
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

     count(x) > count(y) => Pattern(x, A)            < Pattern(y, A)            for any A
     A < B               => Pattern(x, A)            < Pattern(y, B)            for any x, y
     A < B               => Optional(A, value1)      < Optional(B, value2)      for any value1, value2
     A < B               => Repeated(A, r1)          < Repeated(B, r2)          for any r1, r2
     A < B               => TestPattern(A, fn1)      < TestPattern(B, fn2)      for any fn1, fn2
     A < B               => Alternatives(A,...)      < Alternatives(B,...)      when both Alternatives(...) have the same length
     A < B               => KeyValuePattern({A,...}) < KeyValuePattern({B,...}) when both {...} have the same length
     A < B               => Condition(A, c1)         < Condition(B, c2)         when both Condition(...) have the same length
     A < B               => PatternSequence(A, ...)  < PatternSequence(B, ...)  when both PatternSequence(...) have the same length
     A < B               => Except(A)                > Except(B)
     A < B               => Except(no, A)            < Except(no, B)
     constants  <  SingleMatch(type)  <  SingleMatch()  <  Repeated(...)

     Except(no) == Except(no, SingleMatch())
     Longest(pattern) == pattern
     Shortest(pattern) == pattern
     HoldPattern(pattern) == pattern
     Alternatives(pattern) == pattern
     Alternatives(...) < Alternatives(...) when first Alternatives()-expr is shorter that second.
     KeyValuePattern({...}) > KeyValuePattern({...}) when first {...} is longer that second.
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
  if(pmath_same(head1, pmath_System_PatternSequence)) {
    pmath_t p1;
    int cmp;

    if(pmath_same(head2, pmath_System_PatternSequence)) {
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
    if(len1 < 1)
      return -1;
    if(len1 > 1)
      return 1;

    p1 = pmath_expr_get_item(pat1, 1);
    cmp = pattern_compare(p1, pat2, status);
    pmath_unref(p1);
    return cmp;
  }

  if(pmath_same(head2, pmath_System_PatternSequence)) {
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
  if(len1 == 2 && pmath_same(head1, pmath_System_Repeated)) {
    pmath_t p1 = pmath_expr_get_item(pat1, 1);
    int cmp;

    if(len2 == 2 && pmath_same(head2, pmath_System_Repeated)) {
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

  if(len2 == 2 && pmath_same(head2, pmath_System_Repeated)) {
    pmath_t p2 = pmath_expr_get_item(pat2, 1);
    int cmp = pattern_compare(pat1, p2, status);
    pmath_unref(p2);
    if(cmp == 0)
      return -1;
    return cmp;
  }
  //} ... Repeated(pat, range)

  //{ SingleMatch(), SingleMatch(type) ...
  if(pmath_same(head1, pmath_System_SingleMatch)) {
    if(pmath_same(head2, pmath_System_SingleMatch)) {
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
  else if(pmath_same(head2, pmath_System_SingleMatch))
    return -1;
  //} ... SingleMatch(), SingleMatch(type)

  //{ TestPattern(pat, fn), Condition(pat, cond) ...
  if( len1 == 2 &&
      (pmath_same(head1, pmath_System_TestPattern) ||
       pmath_same(head1, pmath_System_Condition)))
  {
    pmath_t p1 = pmath_expr_get_item(pat1, 1);
    int cmp;

    if( len2 == 2 &&
        (pmath_same(head2, pmath_System_TestPattern) ||
         pmath_same(head2, pmath_System_Condition)))
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

      if(pmath_same(head1, pmath_System_Condition))
        return -1;
      return 1;
    }

    cmp = pattern_compare(p1, pat2, status);
    pmath_unref(p1);
    if(cmp == 0)
      return -1;
    return cmp;
  }

  if(len2 == 2 && pmath_same(head2, pmath_System_TestPattern)) {
    pmath_t p2 = pmath_expr_get_item(pat2, 1);

    int cmp = pattern_compare(pat1, p2, status);

    pmath_unref(p2);
    if(cmp == 0)
      return 1;
    return cmp;
  }
  //} ... TestPattern(pat, fn), Condition(pat, cond)

  //{ Longest(pat), Shortest(pat), HoldPattern(pat) ...
  if( (pmath_same(head1, pmath_System_Longest) ||
       pmath_same(head1, pmath_System_Shortest) ||
       pmath_same(head1, pmath_System_HoldPattern)) &&
      len1 == 1)
  {
    pmath_t p1 = pmath_expr_get_item(pat1, 1);
    int cmp = pattern_compare(p1, pat2, status);
    pmath_unref(p1);
    return cmp;
  }

  if( (pmath_same(head2, pmath_System_Longest) ||
       pmath_same(head2, pmath_System_Shortest) ||
       pmath_same(head2, pmath_System_HoldPattern)) &&
      len2 == 1)
  {
    pmath_t p2 = pmath_expr_get_item(pat2, 1);
    int cmp = pattern_compare(pat1, p2, status);
    pmath_unref(p2);
    return cmp;
  }
  //} ... Longest(pat), Shortest(pat), HoldPattern(pat)

  //{ Alternatives(...) ...
  if(pmath_same(head1, pmath_System_Alternatives)) {
    pmath_t p1;
    int cmp;

    if(pmath_same(head2, pmath_System_Alternatives)) {
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
    if(len1 < 1)
      return -1;
    if(len1 > 1)
      return 1;

    p1 = pmath_expr_get_item(pat1, 1);
    cmp = pattern_compare(p1, pat2, status);
    pmath_unref(p1);
    return cmp;
  }

  if(pmath_same(head2, pmath_System_Alternatives)) {
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

  //{ Optional(pat), Optional(pat, value) ...
  if(pmath_same(head1, pmath_System_Optional) && (len1 == 1 || len1 == 2)) {
    pmath_t p1 = pmath_expr_get_item(pat1, 1);
    int cmp;

    if(pmath_same(head2, pmath_System_Optional) && (len2 == 1 || len2 == 2)) {
      pmath_t p2 = pmath_expr_get_item(pat2, 1);

      cmp = pattern_compare(p1, p2, status);

      pmath_unref(p1);
      pmath_unref(p2);
      return cmp;
    }

    cmp = pattern_compare(p1, pat2, status);

    pmath_unref(p1);
    if(cmp == 0)
      return 1;
    return cmp;
  }

  if(pmath_same(head2, pmath_System_Optional) && (len2 == 1 || len2 == 2)) {
    pmath_t p2 = pmath_expr_get_item(pat2, 1);

    int cmp = pattern_compare(pat1, p2, status);

    pmath_unref(p2);
    if(cmp == 0)
      return -1;
    return cmp;
  }
  //} ... Optional(pat), Optional(pat, value)

  //{ Pattern(name, pat) ...
  if(pmath_same(head1, pmath_System_Pattern) && len1 == 2) {
    if(pmath_same(head2, pmath_System_Pattern) && len2 == 2) {
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

  if(pmath_same(head2, pmath_System_Pattern) && len2 == 2) {
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
  if(pmath_same(head1, pmath_System_Except) && (len1 == 1 || len1 == 2)) {
    pmath_t p1;

    if(len1 == 2)
      p1 = pmath_expr_get_item(pat1, 2);
    else
      p1 = pmath_ref(_pmath_object_singlematch);

    if(pmath_same(head2, pmath_System_Except) && (len2 == 1 || len2 == 2)) {
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

  if(pmath_same(head2, pmath_System_Except) && (len2 == 1 || len2 == 2)) {
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

  //{ KeyValuePattern(), KeyValuePattern(rule), KeyValuePattern({rules}) ...
  if(pmath_same(head1, pmath_System_KeyValuePattern) && len1 <= 1) {
    pmath_t rules_1 = pmath_expr_get_item(pat1, 1);
    if(pmath_is_rule(rules_1)) {
      pmath_unref(rules_1);
      rules_1 = pmath_ref(pat1);
    }
    if(len1 == 0 || pmath_same(rules_1, pat1) || pmath_is_expr_of(rules_1, pmath_System_List)) {
      if(pmath_same(head2, pmath_System_KeyValuePattern) && len2 <= 1) {
        pmath_t rules_2 = pmath_expr_get_item(pat2, 1);
        if(pmath_is_rule(rules_2)) {
          pmath_unref(rules_2);
          rules_2 = pmath_ref(pat2);
        }
        if(len2 == 0 || pmath_same(rules_2, pat2) || pmath_is_expr_of(rules_2, pmath_System_List)) {
          size_t num_rules_1 = pmath_expr_length(rules_1);
          size_t num_rules_2 = pmath_expr_length(rules_2);
          
          if(num_rules_1 == num_rules_2) {
            int cmp = 0;
            size_t i;
            
            for(i = 1; i <= num_rules_1; ++i) {
              pmath_t p1 = pmath_expr_get_item(rules_1, i);
              pmath_t p2 = pmath_expr_get_item(rules_2, i);
              
              cmp = pattern_compare(p1, p2, status);
              pmath_unref(p1);
              pmath_unref(p2);
              if(cmp != 0) 
                break;
            }
            
            pmath_unref(rules_1);
            pmath_unref(rules_2);
            return cmp;
          }
          
          pmath_unref(rules_1);
          pmath_unref(rules_2);
          if(num_rules_1 > num_rules_2)
            return -1;
          else
            return 1;
        }
        pmath_unref(rules_2);
      }
    }
    pmath_unref(rules_1);
    return 1;
  }
  
  if(pmath_same(head2, pmath_System_KeyValuePattern) && len2 <= 1) {
    return -1;
  }
  //} ... KeyValuePattern(), KeyValuePattern(rule), KeyValuePattern({rules})
  
  //{ Literal(x) ...
  if(pmath_same(head1, pmath_System_Literal) && len1 == 1) {
    pmath_t p1, p2;
    int cmp;

    p1 = pmath_expr_get_item(pat1, 1);

    if(pmath_same(head2, pmath_System_Literal) && len2 == 1)
      p2 = pmath_expr_get_item(pat2, 1);
    else
      p2 = pmath_ref(pat2);

    cmp = pmath_compare(p1, p2);
    pmath_unref(p1);
    pmath_unref(p2);
    return cmp;
  }

  if(pmath_same(head2, pmath_System_Literal) && len2 == 1) {
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

  if( (pmath_same(head, pmath_System_Condition)        &&  len == 2)              ||
      (pmath_same(head, pmath_System_Alternatives))                               ||
      (pmath_same(head, pmath_System_HoldPattern)      &&  len == 1)              ||
      (pmath_same(head, pmath_System_Longest)          &&  len == 1)              ||
      (pmath_same(head, pmath_System_Optional)         && (len == 1 || len == 2)) ||
      (pmath_same(head, pmath_System_Except)           && (len == 1 || len == 2)) ||
      (pmath_same(head, pmath_System_Pattern)          &&  len == 2)              ||
      (pmath_same(head, pmath_System_PatternSequence))                            ||
      (pmath_same(head, pmath_System_Repeated)         &&  len == 2)              ||
      (pmath_same(head, pmath_System_Shortest)         &&  len == 1)              ||
      (pmath_same(head, pmath_System_SingleMatch)      &&  len <= 1)              ||
      (pmath_same(head, pmath_System_OptionsPattern)   &&  len <= 1)              ||
      (pmath_same(head, pmath_System_TestPattern)      &&  len == 2)              ||
      (pmath_same(head, pmath_System_KeyValuePattern)  &&  len <= 1))
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

  pmath_hashtable_t pattern_variables; // is ht_pattern_variable_class

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

struct pattern_variable_entry_t {
  struct int_entry_t  inherited;
  pmath_t       value;
  pmath_t       first_matched_pattern; // PMATH_UNDEFINED when not matched
  pmath_bool_t  is_currently_matching;
};

static void pattern_variable_entry_destructor(void *e);
static const pmath_ht_class_t ht_pattern_variable_class = {
  pattern_variable_entry_destructor,
  ht_int_class__entry_hash,
  ht_int_class__entry_keys_equal,
  _pmath_hash_pointer,
  ht_int_class__entry_equals_key
};

typedef enum match_kind_t {
  PMATH_MATCH_KIND_NONE,
  PMATH_MATCH_KIND_LOCAL,
  PMATH_MATCH_KIND_GLOBAL
} match_kind_t;

typedef struct match_func_data_t {
  pmath_expr_t     pat;       // wont be freed
  pmath_expr_t     func;      // wont be freed

  pattern_info_t  *info;

  // initialized by init_analyse_match_func()
  // zero based index: ifo for pat[1] is pat_infos[0]
  _pmath_pattern_analyse_output_t *pat_infos;

  pmath_bool_t associative;
  pmath_bool_t one_identity;
  pmath_bool_t symmetric;
} match_func_data_t;

static void init_pattern_variables(
  pmath_t pattern,               // wont be freed
  pmath_hashtable_t table);

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

static pmath_bool_t init_analyse_match_func(match_func_data_t  *data);
static void         done_analyse_match_func(match_func_data_t  *data);

static match_kind_t match_func_left( // for non-symmetric functions
  match_func_data_t  *data,
  size_t              pat_start,
  size_t              func_start);

static match_kind_t match_func(
  pattern_info_t  *info,
  pmath_expr_t     pat,   // wont be freed
  pmath_expr_t     arg);  // wont be freed

static pmath_bool_t replace_with_literal_exact_once(
  pmath_t *pattern,
  pmath_t  _old,
  pmath_t  _new_literal);

// retains debug-info
static pmath_t replace_option_value(
  pmath_t      body,          // will be freed
  pmath_t      default_head,
  pmath_expr_t optionvaluerules);  // form: <T> = <Function>(<OptionValueRules>,<T>)

// retains debug-info
static pmath_t replace_multiple_symbols(
  pmath_t           object,        // will be freed
  pmath_hashtable_t replacements); // entries are struct pattern_variable_entry_t*

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

  info.pattern_variables = NULL;
//  info.pattern_variables = pmath_ht_create(&ht_pattern_variable_class, 0);
//  if(!info.pattern_variables) {
//    pmath_unref(pattern);
//    return FALSE;
//  }
//
//  init_pattern_variables(info.pattern, info.pattern_variables);

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
          //pmath_ht_destroy(info.pattern_variables);
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
        if(info.pattern_variables) {
          *rhs = replace_multiple_symbols(*rhs, info.pattern_variables);
        }

        if(!pmath_is_null(info.options)) {
          pmath_t default_head = PMATH_NULL;
          if(pmath_is_expr(obj))
            default_head = pmath_expr_get_item(obj, 0);

          *rhs = replace_option_value(*rhs, default_head, info.options);
          pmath_unref(default_head);
        }

        if(_pmath_rhs_has_condition(rhs, TRUE)) {
          *rhs = pmath_evaluate(*rhs);

          if(pmath_is_expr_of_len(*rhs, pmath_Internal_Condition, 2)) {
            pmath_t res = pmath_expr_get_item(*rhs, 2);
            pmath_unref(res);

            if(!pmath_same(res, pmath_System_True)) {
              pmath_unref(*rhs);
              *rhs = PMATH_NULL;
              kind = PMATH_MATCH_KIND_NONE;
              goto CLEANUP;
            }

            res = pmath_expr_get_item(*rhs, 1);
            pmath_unref(*rhs);
            *rhs = res;
          }
        }

        if(pmath_is_expr_of(*rhs, PMATH_MAGIC_PATTERN_SEQUENCE)) {
          *rhs = pmath_expr_set_item(*rhs, 0, pmath_ref(pmath_System_Sequence));
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


CLEANUP:

  pmath_unref(info.options);
  pmath_unref(info.pattern);
  if(info.pattern_variables) {
    pmath_ht_destroy(info.pattern_variables);
  }
  if(info.symmetric)
    pmath_mem_free(info.arg_usage);
  return kind != PMATH_MATCH_KIND_NONE;
}

static void pattern_variable_entry_destructor(void *e) {
  struct pattern_variable_entry_t *entry = e;
  pmath_unref(entry->value);
  pmath_unref(entry->first_matched_pattern);

  ht_int_class.entry_destructor(e);
}

static void init_pattern_variables(
  pmath_t pattern,               // wont be freed
  pmath_hashtable_t table
) {
  if(pmath_is_expr_of_len(pattern, pmath_System_Pattern, 2)) {
    pmath_t name = pmath_expr_get_item(pattern, 1);
    if(pmath_is_symbol(name)) {
      struct pattern_variable_entry_t *entry;
      entry = pmath_mem_alloc(sizeof(struct pattern_variable_entry_t));
      if(!entry)
        return;

      entry->inherited.key = (intptr_t)PMATH_AS_PTR(name);
      entry->value = pmath_ref(_pmath_object_empty_pattern_sequence);
      entry->first_matched_pattern = PMATH_UNDEFINED;
      entry->is_currently_matching = FALSE;

      entry = pmath_ht_insert(table, entry);

      if(entry)
        pattern_variable_entry_destructor(entry);
    }
    pmath_unref(name);
  }

  if(pmath_is_expr(pattern)) {
    size_t i;

    for(i = 0; i <= pmath_expr_length(pattern); ++i) {
      pmath_t item = pmath_expr_get_item(pattern, i);

      init_pattern_variables(item, table);

      pmath_unref(item);
    }
  }
}

static match_kind_t match_atom_to_typed_singlematch(pattern_info_t *info, pmath_t pat, pmath_t arg, size_t index_of_arg, size_t count_of_arg);
static match_kind_t match_atom_to_condition(        pattern_info_t *info, pmath_t pat, pmath_t arg, size_t index_of_arg, size_t count_of_arg);
static match_kind_t match_atom_to_testpattern(      pattern_info_t *info, pmath_t pat, pmath_t arg, size_t index_of_arg, size_t count_of_arg);
static match_kind_t match_atom_to_repeatedpattern(  pattern_info_t *info, pmath_t pat, pmath_t arg, size_t index_of_arg, size_t count_of_arg);
static match_kind_t match_atom_to_named_pattern(    pattern_info_t *info, pmath_t pat, pmath_t arg, size_t index_of_arg, size_t count_of_arg);
static match_kind_t match_atom_to_optional(         pattern_info_t *info, pmath_t pat, pmath_t arg, size_t index_of_arg, size_t count_of_arg);
static match_kind_t match_atom_to_alternatives(     pattern_info_t *info, pmath_t pat, pmath_t arg, size_t index_of_arg, size_t count_of_arg);
static match_kind_t match_atom_to_optionspattern(   pattern_info_t *info, pmath_t pat, pmath_t arg, size_t index_of_arg, size_t count_of_arg);
static match_kind_t match_atom_to_literal(          pattern_info_t *info, pmath_t pat, pmath_t arg, size_t index_of_arg, size_t count_of_arg);
static match_kind_t match_atom_to_patternsequence(  pattern_info_t *info, pmath_t pat, pmath_t arg, size_t index_of_arg, size_t count_of_arg);
static match_kind_t match_atom_to_except(           pattern_info_t *info, pmath_t pat, pmath_t arg, size_t index_of_arg, size_t count_of_arg);
static match_kind_t match_atom_to_keyvaluepattern(  pattern_info_t *info, pmath_t pat, pmath_t arg, size_t index_of_arg, size_t count_of_arg);
static match_kind_t match_atom_to_other_function(   pattern_info_t *info, pmath_t pat, pmath_t arg, size_t index_of_arg, size_t count_of_arg);

static match_kind_t match_atom(
  pattern_info_t  *info,
  pmath_t          pat,  // wont be freed
  pmath_t          arg,  // wont be freed
  size_t           index_of_arg,
  size_t           count_of_arg
) {
  if(pmath_equals(pat, arg) && _pmath_pattern_is_const(pat))
    return PMATH_MATCH_KIND_LOCAL;

  if(pmath_is_expr(pat)) {
    const size_t len  = pmath_expr_length(pat);
    pmath_t      head = pmath_expr_get_item(pat, 0);
    pmath_unref(head);

    if(pmath_same(head, pmath_System_SingleMatch) && len == 0) // SingleMatch()
      return PMATH_MATCH_KIND_LOCAL;

    if(pmath_same(head, pmath_System_SingleMatch) && len == 1) // SingleMatch(type)
      return match_atom_to_typed_singlematch(info, pat, arg, index_of_arg, count_of_arg);

    if(len == 2 && pmath_same(head, pmath_System_Condition)) // pattern /? condition
      return match_atom_to_condition(info, pat, arg, index_of_arg, count_of_arg);
    
    if(len == 2 && pmath_same(head, pmath_System_TestPattern)) // pattern ? testfunc
      return match_atom_to_testpattern(info, pat, arg, index_of_arg, count_of_arg);

    if(len == 2 && pmath_same(head, pmath_System_Repeated)) // Repeated(pattern, range)
      return match_atom_to_repeatedpattern(info, pat, arg, index_of_arg, count_of_arg);

    if(pmath_same(head, pmath_System_Pattern) && len == 2) // name: pattern
      return match_atom_to_named_pattern(info, pat, arg, index_of_arg, count_of_arg);

    if(pmath_same(head, pmath_System_Optional) && (len == 1 || len == 2)) // Optional(pattern), Optional(pattern, value)
      return match_atom_to_optional(info, pat, arg, index_of_arg, count_of_arg);

    if( len == 1 &&
        (pmath_same(head, pmath_System_HoldPattern) || // HoldPattern(p)
         pmath_same(head, pmath_System_Longest)     || // Longest(p)
         pmath_same(head, pmath_System_Shortest)))     // Shortest(p)
    {
      pmath_t p = pmath_expr_get_item(pat, 1);
      match_kind_t kind = match_atom(info, p, arg, index_of_arg, count_of_arg);
      pmath_unref(p);
      return kind;
    }

    if(pmath_same(head, pmath_System_Alternatives)) // p1 | p2 | ...
      return match_atom_to_alternatives(info, pat, arg, index_of_arg, count_of_arg);
    
    if(len <= 1 && pmath_same(head, pmath_System_OptionsPattern)) 
      return match_atom_to_optionspattern(info, pat, arg, index_of_arg, count_of_arg);

    if(pmath_same(head, pmath_System_Literal)) 
      return match_atom_to_literal(info, pat, arg, index_of_arg, count_of_arg);

    if(pmath_same(head, pmath_System_PatternSequence)) 
      return match_atom_to_patternsequence(info, pat, arg, index_of_arg, count_of_arg);

    if(pmath_same(head, pmath_System_Except) && (len == 1 || len == 2)) // Except(no) Except(no, but)
      return match_atom_to_except(info, pat, arg, index_of_arg, count_of_arg);
    
    if(pmath_same(head, pmath_System_KeyValuePattern) && (len == 0 || len == 1)) // Except(no) Except(no, but)
      return match_atom_to_keyvaluepattern(info, pat, arg, index_of_arg, count_of_arg);
    
    return match_atom_to_other_function(info, pat, arg, index_of_arg, count_of_arg);
  }

  return PMATH_MATCH_KIND_NONE;
}

static match_kind_t match_atom_to_typed_singlematch(pattern_info_t *info, pmath_t pat, pmath_t arg, size_t index_of_arg, size_t count_of_arg) {
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

static match_kind_t match_atom_to_condition(pattern_info_t *info, pmath_t pat, pmath_t arg, size_t index_of_arg, size_t count_of_arg) {
  pmath_t pattern = pmath_expr_get_item(pat, 1);
  pmath_t test = PMATH_NULL;

  match_kind_t kind = match_atom(info, pattern, arg, index_of_arg, count_of_arg);
  pmath_unref(pattern);
  if(kind != PMATH_MATCH_KIND_LOCAL)
    return kind;

  test = pmath_expr_get_item(pat, 2);
  if(info->pattern_variables) {
    test = replace_multiple_symbols(test, info->pattern_variables);
  }

  test = pmath_evaluate(test);
  pmath_unref(test);

  if(pmath_aborting())
    return PMATH_MATCH_KIND_GLOBAL;

  if(pmath_same(test, pmath_System_True))
    return PMATH_MATCH_KIND_LOCAL;

  return PMATH_MATCH_KIND_NONE;
}

static match_kind_t match_atom_to_testpattern(pattern_info_t *info, pmath_t pat, pmath_t arg, size_t index_of_arg, size_t count_of_arg) {
  pmath_t pattern = pmath_expr_get_item(pat, 1);
  pmath_t test = PMATH_NULL;

  match_kind_t kind = match_atom(info, pattern, arg, index_of_arg, count_of_arg);
  pmath_unref(pattern);
  if(kind != PMATH_MATCH_KIND_LOCAL)
    return kind;

  if(pmath_is_expr_of(arg, PMATH_MAGIC_PATTERN_SEQUENCE)) {
    test = pmath_expr_set_item(
             pmath_ref(arg), 0,
             pmath_expr_get_item(pat, 2));
  }
  else {
    test = pmath_expr_new_extended(
             pmath_expr_get_item(pat, 2), 1,
             pmath_ref(arg));
  }

  test = pmath_evaluate(test);
  pmath_unref(test);

  if(pmath_aborting())
    return PMATH_MATCH_KIND_GLOBAL;

  if(pmath_same(test, pmath_System_True))
    return PMATH_MATCH_KIND_LOCAL;

  return PMATH_MATCH_KIND_NONE;
}

static match_kind_t match_atom_to_repeatedpattern(pattern_info_t *info, pmath_t pat, pmath_t arg, size_t index_of_arg, size_t count_of_arg) {
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
    if(pmath_is_expr_of_len(arg, PMATH_MAGIC_PATTERN_SEQUENCE, 0))
      return PMATH_MATCH_KIND_LOCAL;
    return PMATH_MATCH_KIND_NONE;
  }

  pattern = pmath_expr_get_item(pat, 1);

  if(!pmath_is_expr_of(arg, PMATH_MAGIC_PATTERN_SEQUENCE)) {
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

static match_kind_t match_atom_to_named_pattern(pattern_info_t *info, pmath_t pat, pmath_t arg, size_t index_of_arg, size_t count_of_arg) {
  struct pattern_variable_entry_t *entry;
  match_kind_t kind;

  pmath_t pat_item = pmath_expr_get_item(pat, 1);
  if(!pmath_is_symbol(pat_item)) {
    pmath_debug_print_object("[Internal Pattern Error: ignore pattern with invalid name ", pat_item, "]\n");
    pmath_unref(pat_item);
    return PMATH_MATCH_KIND_NONE;
  }

  if(!info->pattern_variables) {
    info->pattern_variables = pmath_ht_create(&ht_pattern_variable_class, 0);
    if(!info->pattern_variables) {
      pmath_debug_print("[Internal Pattern Error: failed to initialize pattern_variables]\n");
      pmath_unref(pat_item);
      return PMATH_MATCH_KIND_NONE;
    }

    init_pattern_variables(info->pattern, info->pattern_variables);
  }

  entry = pmath_ht_search(info->pattern_variables, PMATH_AS_PTR(pat_item));
  if(!entry) {
    pmath_debug_print_object("[Internal Pattern Error: unknown pattern name ", pat_item, "]\n");
    pmath_unref(pat_item);
    return PMATH_MATCH_KIND_NONE;
  }

//  // Do not allow recursive patterns like  (x: x: ~), i.e.
//  // Pattern(x, Pattern(x, ~)) to prevent possible infinite recursion.
//  if(entry->is_currently_matching) {
//    pmath_debug_print_object("[recursive pattern ", pat, "]\n");
//    return PMATH_MATCH_KIND_NONE;
//  }

  pmath_unref(pat_item);
  pat_item = pmath_expr_get_item(pat, 2);

  if( pmath_same(entry->first_matched_pattern, PMATH_UNDEFINED) &&
      !pmath_same(pat_item, PMATH_UNDEFINED))
  {
    // Didn't (fully) match a pattern with this name before.

    pmath_bool_t recursive_matching = entry->is_currently_matching;

    entry->is_currently_matching = TRUE;
    kind = match_atom(info, pat_item, arg, index_of_arg, count_of_arg);
    entry->is_currently_matching = recursive_matching;

    if(kind != PMATH_MATCH_KIND_NONE) {
      pmath_t default_value;

      if(kind == PMATH_MATCH_KIND_GLOBAL) {
        pmath_unref(pat_item);
        return kind;
      }

      default_value = entry->value;
      entry->value = pmath_ref(arg);

      if(recursive_matching) {
        pmath_unref(pat_item);
        pmath_unref(default_value);
        return kind;
      }

      assert(pmath_same(PMATH_UNDEFINED, entry->first_matched_pattern));
      entry->first_matched_pattern = pat_item;

      // Success: now do global matching because there could be other
      // occurences of the name.
      // TODO: maybe we could add a flag to pattern_variable_entry_t which
      // says whether there are other occurences. But be careful, because
      // these could be hidden inside Condition(...) or an Optional(name,...)
      kind = match_atom(info, info->pattern, info->func, 0, 0);
      if(kind != PMATH_MATCH_KIND_NONE) {
        pmath_unref(default_value);
        return PMATH_MATCH_KIND_GLOBAL;
      }

      pmath_unref(entry->value);
      entry->value = default_value;
      entry->first_matched_pattern = PMATH_UNDEFINED;
    }

    pmath_unref(pat_item);
    return PMATH_MATCH_KIND_NONE;
  }
  else { // Already matched a pattern with this name before.
    if(!pmath_equals(arg, entry->value)) {
      pmath_unref(pat_item);
      return PMATH_MATCH_KIND_NONE;
    }

    if(pmath_equals(pat_item, entry->first_matched_pattern)) {
      // No need to match the same pattern twice.
      //
      // Actually, if pat_item == PMATH_UNDEFINED, then there occurs no
      // matching at all, because that special value is used as the
      // already-visited-flag above.
      // A fix would be to use a seperate flag but that would cost another
      // byte per pattern and PMATH_UNDEFINED cannot be entered by the user,
      // so there is no real need to fix this corner case.

      pmath_unref(pat_item);
      return PMATH_MATCH_KIND_LOCAL;
    }

    kind = match_atom(info, pat_item, arg, index_of_arg, count_of_arg);
    pmath_unref(pat_item);

    return kind;
  }
}

static match_kind_t match_atom_to_optional(pattern_info_t *info, pmath_t pat, pmath_t arg, size_t index_of_arg, size_t count_of_arg) {
  pmath_t pattern;
  pmath_t value;
  match_kind_t kind;

  pattern = pmath_expr_get_item(pat, 1);

  if(pmath_is_expr_of_len(arg, PMATH_MAGIC_PATTERN_SEQUENCE, 0)) {
    if(pmath_expr_length(pat) == 1) {
      value = pmath_evaluate(
                pmath_expr_new_extended(
                  pmath_ref(pmath_System_Default), 3,
                  pmath_ref(info->current_head),
                  pmath_integer_new_uiptr(index_of_arg),
                  pmath_integer_new_uiptr(count_of_arg)));
    }
    else
      value = pmath_expr_get_item(pat, 2);
  }
  else {
    value = pmath_ref(arg);
  }

  kind = match_atom(info, pattern, value, index_of_arg, count_of_arg);

  pmath_unref(pattern);
  pmath_unref(value);
  return kind;
}

static match_kind_t match_atom_to_alternatives(pattern_info_t *info, pmath_t pat, pmath_t arg, size_t index_of_arg, size_t count_of_arg) {
  const size_t len  = pmath_expr_length(pat);
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

static match_kind_t match_atom_to_optionspattern(pattern_info_t *info, pmath_t pat, pmath_t arg, size_t index_of_arg, size_t count_of_arg) {
  pmath_t arghead;
  size_t i, arglen;

  if(!pmath_is_expr(arg))
    return PMATH_MATCH_KIND_NONE;

  arglen = pmath_expr_length(arg);
  arghead = pmath_expr_get_item(arg, 0);
  pmath_unref(arghead);
  if( arglen == 2 &&
      (pmath_same(arghead, pmath_System_Rule) ||
       pmath_same(arghead, pmath_System_RuleDelayed)))
  {
    goto OPTIONSPATTERN_FIT;
  }

  if( !pmath_same(arghead, PMATH_MAGIC_PATTERN_SEQUENCE) &&
      !pmath_same(arghead, pmath_System_List))
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

    if(pmath_expr_length(pat) == 0)
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
    replace_with_literal_exact_once(&info->pattern, pat, arg);

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

static match_kind_t match_atom_to_literal(pattern_info_t *info, pmath_t pat, pmath_t arg, size_t index_of_arg, size_t count_of_arg) {
  const size_t len = pmath_expr_length(pat);
  
  if(pmath_is_expr_of(arg, PMATH_MAGIC_PATTERN_SEQUENCE)) {
    size_t i;

    if(pmath_expr_length(arg) != len)
      return PMATH_MATCH_KIND_NONE;

    for(i = len; i > 0; --i) {
      pmath_t p = pmath_expr_get_item(pat, i);
      pmath_t a = pmath_expr_get_item(arg, i);

      if(!pmath_equals(p, a)) {
        pmath_unref(p);
        pmath_unref(a);
        return PMATH_MATCH_KIND_NONE;
      }

      pmath_unref(p);
      pmath_unref(a);
    }

    return PMATH_MATCH_KIND_LOCAL;
  }

  if(len == 1) {
    pmath_t p = pmath_expr_get_item(pat, 1);
    if(pmath_equals(p, arg)) {
      pmath_unref(p);
      return PMATH_MATCH_KIND_LOCAL;
    }

    pmath_unref(p);
  }

  return PMATH_MATCH_KIND_NONE;
}

static match_kind_t match_atom_to_patternsequence(pattern_info_t *info, pmath_t pat, pmath_t arg, size_t index_of_arg, size_t count_of_arg) {
  match_func_data_t data;
  match_kind_t kind;

  memset(&data, 0, sizeof(data));
  data.info = info;
  data.pat  = pat;
  data.func = pmath_ref(arg);
  data.associative = FALSE;
  data.one_identity = FALSE;
  data.symmetric = FALSE;

  if(!pmath_is_expr_of(arg, PMATH_MAGIC_PATTERN_SEQUENCE))
    data.func = pmath_expr_new_extended(PMATH_MAGIC_PATTERN_SEQUENCE, 1, data.func);

#ifdef DEBUG_LOG_MATCH
  debug_indent(); pmath_debug_print("sequence ...\n");
  ++indented;
#endif

  if(init_analyse_match_func(&data)) {
    kind = match_func_left(&data, 1, 1);

    done_analyse_match_func(&data);
  }
  else
    kind = PMATH_MATCH_KIND_NONE;

#ifdef DEBUG_LOG_MATCH
  --indented;
  debug_indent(); pmath_debug_print("... sequence %d\n", kind);
#endif

  pmath_unref(data.func);
  return kind;
}

static match_kind_t match_atom_to_except(pattern_info_t *info, pmath_t pat, pmath_t arg, size_t index_of_arg, size_t count_of_arg) {
  pmath_t p = pmath_expr_get_item(pat, 1);
  match_kind_t kind = match_atom(info, p, arg, index_of_arg, count_of_arg);
  pmath_unref(p);

  if(kind == PMATH_MATCH_KIND_GLOBAL)
    return kind;

  if(kind == PMATH_MATCH_KIND_LOCAL)
    return PMATH_MATCH_KIND_NONE;

  kind = PMATH_MATCH_KIND_LOCAL;
  if(pmath_expr_length(pat) == 2) {
    p = pmath_expr_get_item(pat, 2);
    kind = match_atom(info, p, arg, index_of_arg, count_of_arg);
    pmath_unref(p);
  }

  return kind;
}

static match_kind_t match_atom_to_keyvaluepattern(pattern_info_t *info, pmath_t pat, pmath_t arg, size_t index_of_arg, size_t count_of_arg) {
  pmath_t rule_pats;
  pmath_dispatch_table_t tab;
  size_t num_rule_pats;
  size_t arglen;
  size_t i;
  size_t first_unused_arg;
  uint8_t *arg_usage;
  
  tab = _pmath_rules_need_dispatch_table(arg);
  if(pmath_is_null(tab))
    return PMATH_MATCH_KIND_NONE;
  
  if(pmath_expr_length(pat) == 0)
    return PMATH_MATCH_KIND_LOCAL;
  
  rule_pats = pmath_expr_get_item(pat, 1);
  if(pmath_is_rule(rule_pats)) {
    pmath_unref(rule_pats);
    rule_pats = pmath_ref(pat);
  }
  else if(!pmath_is_expr_of(rule_pats, pmath_System_List)) {
    pmath_unref(rule_pats);
    pmath_unref(tab);
    return PMATH_MATCH_KIND_NONE;
  }
  
  num_rule_pats = pmath_expr_length(rule_pats);
  if(num_rule_pats == 0) {
    pmath_unref(rule_pats);
    pmath_unref(tab);
    return PMATH_MATCH_KIND_LOCAL;
  }
  
  arglen = pmath_expr_length(arg);
  if(arglen == 0) {
    pmath_t empty = pmath_expr_new(PMATH_MAGIC_PATTERN_SEQUENCE, 0);
    for(i = 1; i <= num_rule_pats; ++i) {
      pmath_t sub_pat = pmath_expr_get_item(rule_pats, i);
      match_kind_t kind = match_atom(info, sub_pat, empty, 1, 0);
      pmath_unref(sub_pat);
      if(kind != PMATH_MATCH_KIND_LOCAL) {
        pmath_unref(empty);
        pmath_unref(rule_pats);
        pmath_unref(tab);
        return kind;
      }
    }
    pmath_unref(empty);
    pmath_unref(rule_pats);
    pmath_unref(tab);
    return PMATH_MATCH_KIND_LOCAL;
  }
  
  if(arglen < num_rule_pats) {
    _pmath_pattern_analyse_input_t  analyise_in;
    _pmath_pattern_analyse_output_t analyise_out;
    
    rule_pats = pmath_expr_set_item(rule_pats, 0, pmath_ref(pmath_System_PatternSequence));
    memset(&analyise_in, 0, sizeof(analyise_in));
    analyise_in.parent_pat_head = pmath_System_PatternSequence;
    analyise_in.pat = rule_pats;
    analyise_in.pattern_variables = info->pattern_variables;
    _pmath_pattern_analyse(&analyise_in, &analyise_out);
    
    if(analyise_out.size.min > arglen) {
      pmath_unref(rule_pats);
      pmath_unref(tab);
      return PMATH_MATCH_KIND_NONE;
    }
  }

  arg_usage = pmath_mem_alloc(arglen);
  if(!arg_usage) {
    pmath_unref(rule_pats);
    pmath_unref(tab);
    return PMATH_MATCH_KIND_GLOBAL; // pmath_aborting() will be true
  }
  memset(arg_usage, NOT_IN_USE, arglen);
  
  first_unused_arg = 1;
  // TODO: every rule can only match once
  for(i = 1; i <= num_rule_pats; ++i) {
    pmath_t sub_pat = pmath_expr_get_item(rule_pats, i);
    match_kind_t kind;
    size_t arg_i;
    
    if(pmath_is_rule(sub_pat)) {
      pmath_t lhs = pmath_expr_get_item(sub_pat, 1);
      if(_pmath_pattern_is_const(lhs)) {
        arg_i = _pmath_dispatch_table_lookup((void*)PMATH_AS_PTR(tab), lhs, NULL, TRUE);
        if(arg_i == 0) {
          pmath_unref(lhs);
          pmath_unref(sub_pat);
          pmath_mem_free(arg_usage);
          pmath_unref(rule_pats);
          pmath_unref(tab);
          return PMATH_MATCH_KIND_NONE;
        }
        assert(arg_i <= arglen);
        if(arg_usage[arg_i - 1] == NOT_IN_USE) {
          pmath_t rule = pmath_expr_get_item(arg, arg_i);
          arg_usage[arg_i - 1] = TESTING_USE;
          kind = match_atom(info, sub_pat, rule, arg_i, arglen);
          pmath_unref(rule);
          if(kind == PMATH_MATCH_KIND_GLOBAL) {
            pmath_unref(lhs);
            pmath_unref(sub_pat);
            pmath_mem_free(arg_usage);
            pmath_unref(rule_pats);
            pmath_unref(tab);
            return kind;
          }
          if(kind == PMATH_MATCH_KIND_LOCAL) {
            arg_usage[arg_i - 1] = IN_USE;
            pmath_unref(lhs);
            pmath_unref(sub_pat);
            continue;
          }
          arg_usage[arg_i - 1] = NOT_IN_USE;
        }
      }
      pmath_unref(lhs);
    }
    
    while(first_unused_arg <= arglen && arg_usage[first_unused_arg - 1] == IN_USE)
      ++first_unused_arg;
    
    kind = PMATH_MATCH_KIND_NONE;
    for(arg_i = first_unused_arg; arg_i <= arglen; ++arg_i) {
      if(arg_usage[arg_i - 1] == NOT_IN_USE) {
        pmath_t rule = pmath_expr_get_item(arg, arg_i);
        arg_usage[arg_i - 1] = TESTING_USE;
        kind = match_atom(info, sub_pat, rule, arg_i, arglen);
        pmath_unref(rule);
        if(kind == PMATH_MATCH_KIND_GLOBAL) {
          pmath_unref(sub_pat);
          pmath_mem_free(arg_usage);
          pmath_unref(rule_pats);
          pmath_unref(tab);
          return kind;
        }
        if(kind == PMATH_MATCH_KIND_LOCAL) {
          arg_usage[arg_i - 1] = IN_USE;
          break;
        }
        arg_usage[arg_i - 1] = NOT_IN_USE;
      }
    }
    
    pmath_unref(sub_pat);
    if(kind == PMATH_MATCH_KIND_NONE) {
      pmath_mem_free(arg_usage);
      pmath_unref(rule_pats);
      pmath_unref(tab);
      return PMATH_MATCH_KIND_NONE;
    }
  }
  
  pmath_mem_free(arg_usage);
  pmath_unref(rule_pats);
  pmath_unref(tab);
  return PMATH_MATCH_KIND_LOCAL;
}

static match_kind_t match_atom_to_other_function(pattern_info_t *info, pmath_t pat, pmath_t arg, size_t index_of_arg, size_t count_of_arg) {
  pmath_symbol_attributes_t attr = 0;
  pmath_t head = pmath_expr_get_item(pat, 0);
  if(pmath_is_symbol(head))
    attr = pmath_symbol_get_attributes(head);
  
  pmath_unref(head);
  if(attr & PMATH_SYMBOL_ATTRIBUTE_ONEIDENTITY) {
    match_kind_t kind;

    if(pmath_is_expr(arg)) {
      kind = match_func(info, pat, arg);
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

static pmath_bool_t validate_pattern_and_free(pmath_t pattern) {
  size_t i;
  size_t exprlen;
  pmath_t item;

  if(!pmath_is_expr(pattern) || pmath_is_packed_array(pattern)) {
    pmath_unref(pattern);
    return TRUE;
  }

  exprlen = pmath_expr_length(pattern);
  if(exprlen == 2 && pmath_is_expr_of(pattern, pmath_System_Pattern)) {
    item = pmath_expr_get_item(pattern, 1);
    if(!pmath_is_symbol(item)) {
      pmath_unref(item);
      pmath_message(pmath_System_Pattern, "patvar", 1, pattern);
      return FALSE;
    }
     pmath_unref(item);
  }
  else {
    for(i = 0; i < exprlen; ++i) {
      item = pmath_expr_get_item(pattern, i);
      if(!validate_pattern_and_free(item)) {
        pmath_unref(pattern);
        return FALSE;
      }
    }
  }

  item = pmath_expr_get_item(pattern, exprlen);
  pmath_unref(pattern);
  return validate_pattern_and_free(item);
}

PMATH_PRIVATE pmath_bool_t _pmath_pattern_validate(pmath_t pattern) {
  return validate_pattern_and_free(pmath_ref(pattern));
}

PMATH_PRIVATE void _pmath_pattern_analyse(
  _pmath_pattern_analyse_input_t  *input,
  _pmath_pattern_analyse_output_t *output
) {
  output->size.min = 1;
  output->size.max = 1;
  output->options.no_sequence = FALSE;
  output->options.longest = TRUE;
  output->options.prefer_nonempty = FALSE;

  if(pmath_is_expr(input->pat)) {
    size_t len = pmath_expr_length(input->pat);
    pmath_t head = pmath_expr_get_item(input->pat, 0);
    pmath_unref(head);

    if(len == 2 && pmath_same(head, pmath_System_Repeated)) { // Repeated(pat, range)
      size_t min2, max2, m;
      pmath_t obj = input->pat;
      pmath_bool_t old_associative = input->associative;

      input->pat = pmath_expr_get_item(obj, 1);
      input->associative = FALSE;
      _pmath_pattern_analyse(input, output);
      pmath_unref(input->pat);
      input->pat = obj;
      input->associative = !!old_associative;

      min2 = 1;
      max2 = SIZE_MAX;
      obj = pmath_expr_get_item(input->pat, 2);
      extract_range(obj, &min2, &max2, TRUE);
      pmath_unref(obj);


      m = output->size.min * min2;
      if(min2 && m / min2 != output->size.min)
        output->size.min = SIZE_MAX;
      else
        output->size.min = m;


      m = output->size.max * max2;
      if(max2 && m / max2 != output->size.max)
        output->size.max = SIZE_MAX;
      else
        output->size.max = m;

      output->options.no_sequence = FALSE;
    }
    else if(len == 2 && pmath_same(head, pmath_System_Pattern)) { // name: pat
      struct pattern_variable_entry_t *entry = NULL;

      if(input->pattern_variables) {
        pmath_t pat_name = pmath_expr_get_item(input->pat, 1);
        if(pmath_is_symbol(pat_name)) 
          entry = pmath_ht_search(input->pattern_variables, PMATH_AS_PTR(pat_name));

        if(!entry) {
          pmath_debug_print_object("[Internal Pattern Error: unknown pattern name ", pat_name, "]");
        }

        pmath_unref(pat_name);
      }

      if(entry && !pmath_same(entry->first_matched_pattern, PMATH_UNDEFINED)) {
        // Already visited -> one definite size

        if(pmath_is_expr(entry->value)) {
          head = pmath_expr_get_item(entry->value, 0);
          pmath_unref(head);

          if(input->associative && pmath_same(head, input->parent_pat_head)) {
            output->options.no_sequence = TRUE;
            output->size.min = output->size.max = pmath_expr_length(entry->value);
          }
          else if(pmath_same(head, PMATH_MAGIC_PATTERN_SEQUENCE)) {
            output->size.min =
              output->size.max = pmath_expr_length(entry->value);
          }
        }
      }
      else {
        pmath_t tmppat = input->pat;
        input->pat = pmath_expr_get_item(tmppat, 2);

        _pmath_pattern_analyse(input, output);

        pmath_unref(input->pat);
        input->pat = tmppat;
      }
    }
    else if( (len == 2 && pmath_same(head, pmath_System_TestPattern)) || // pat ? testfn
             (len == 1 && pmath_same(head, pmath_System_Condition))   || // pat /? testfn
             (len == 1 && pmath_same(head, pmath_System_HoldPattern)))   // HoldPattern(pat)
    {
      pmath_t tmppat = input->pat;
      input->pat = pmath_expr_get_item(tmppat, 1);
      _pmath_pattern_analyse(input, output);
      pmath_unref(input->pat);
      input->pat = tmppat;
    }
    else if(len > 0 && pmath_same(head, pmath_System_Alternatives)) { // Alternatives(...)
      size_t res_min = SIZE_MAX;
      size_t res_max = 0;
      pmath_t tmppat = input->pat;
      size_t i;
      for(i = pmath_expr_length(input->pat); i > 0; --i) {
        input->pat = pmath_expr_get_item(tmppat, i);

        _pmath_pattern_analyse(input, output);

        if(output->size.min < res_min)
          res_min = output->size.min;
        if(output->size.max > res_max)
          res_max = output->size.max;

        pmath_unref(input->pat);
      }
      input->pat = tmppat;
      output->size.min = res_min;
      output->size.max = res_max;
    }
    else if(pmath_same(head, pmath_System_PatternSequence)) { // PatternSequence(...)
      _pmath_pattern_analyse_output_t  out2;
      pmath_t tmppat = input->pat;
      size_t i;

      output->size.min = 0;
      output->size.max = 0;

      for(i = pmath_expr_length(input->pat); i > 0; --i) {
        input->pat = pmath_expr_get_item(tmppat, i);

        _pmath_pattern_analyse(input, &out2);

        if(output->size.min <= SIZE_MAX - out2.size.min)
          output->size.min += out2.size.min;
        else
          output->size.min = SIZE_MAX;

        if(output->size.max <= SIZE_MAX - out2.size.max)
          output->size.max += out2.size.max;
        else
          output->size.max = SIZE_MAX;

        pmath_unref(input->pat);
      }

      input->pat = tmppat;
    }
    else if(pmath_same(head, pmath_System_Literal)) { // Literal(...)
      output->size.min = output->size.max = pmath_expr_length(input->pat);
    }
    else if(len <= 1 && pmath_same(head, pmath_System_OptionsPattern)) { // OptionsPattern() or OptionsPattern(fn)
      output->size.min = 0;
      output->size.max = SIZE_MAX;
    }
    else if( input->associative &&
             len <= 1           &&
             pmath_same(head, pmath_System_SingleMatch))  // ~ or ~:type
    {
      output->size.max = SIZE_MAX;
      output->options.no_sequence = TRUE;
      output->options.longest = FALSE;
    }
    else if(pmath_same(head, pmath_System_Optional) && (len == 1 || len == 2)) { // Optional(pattern), Optional(pattern, value)
      pmath_t tmppat = input->pat;
      input->pat = pmath_expr_get_item(tmppat, 1);

      _pmath_pattern_analyse(input, output);

      pmath_unref(input->pat);
      input->pat = tmppat;

      output->size.min = 0;

      output->options.prefer_nonempty = TRUE;
    }
    else if(len == 1 && pmath_same(head, pmath_System_Longest)) { // Longest(pat)
      pmath_t tmppat = input->pat;
      input->pat = pmath_expr_get_item(tmppat, 1);

      _pmath_pattern_analyse(input, output);

      pmath_unref(input->pat);
      input->pat = tmppat;

      output->options.longest = TRUE;
    }
    else if(len == 1 && pmath_same(head, pmath_System_Shortest)) { // Shortest(pat)
      pmath_t tmppat = input->pat;
      input->pat = pmath_expr_get_item(tmppat, 1);

      _pmath_pattern_analyse(input, output);

      pmath_unref(input->pat);
      input->pat = tmppat;

      output->options.longest = FALSE;
    }
    else if(len == 2 && pmath_same(head, pmath_System_Except)) { // Except(no, pat)
      pmath_t tmppat = input->pat;
      input->pat = pmath_expr_get_item(tmppat, 2);

      _pmath_pattern_analyse(input, output);

      pmath_unref(input->pat);
      input->pat = tmppat;
    }
  }
}

#define MATCH_MULTI_ARGS_LOOP(n, last_flag, ANALYSE_OUTPUT, BODY) \
  do {                                                                                               \
    if((ANALYSE_OUTPUT).size.min <= (ANALYSE_OUTPUT).size.max) {                                     \
      last_flag = FALSE;                                                                             \
      if((ANALYSE_OUTPUT).options.longest) {                                                         \
        for(n = (ANALYSE_OUTPUT).size.max; n > (ANALYSE_OUTPUT).size.min; --n) {                     \
          BODY                                                                                       \
        }                                                                                            \
        last_flag = TRUE;                                                                            \
        BODY                                                                                         \
      }                                                                                              \
      else if((ANALYSE_OUTPUT).options.prefer_nonempty && (ANALYSE_OUTPUT).size.min == 0) {          \
        for(n = 1; n < (ANALYSE_OUTPUT).size.max; ++n) {                                             \
          BODY                                                                                       \
        }                                                                                            \
        last_flag = TRUE;                                                                            \
        n = 0;                                                                                       \
        BODY                                                                                         \
      }                                                                                              \
      else {                                                                                         \
        for(n = (ANALYSE_OUTPUT).size.min; n < (ANALYSE_OUTPUT).size.max; ++n) {                     \
          BODY                                                                                       \
        }                                                                                            \
        last_flag = TRUE;                                                                            \
        BODY                                                                                         \
      }                                                                                              \
    }                                                                                                \
  } while(0)

typedef struct {
  pattern_info_t *info;
  pmath_expr_t    pat;                  // wont be freed
  pmath_expr_t    func;                 // wont be freed

  _pmath_pattern_analyse_output_t  analysed;
} match_repeated_data_t;

static match_kind_t match_repeated_left( // for non-symmetric functions
  match_repeated_data_t  *data,
  size_t                  func_start
) {
  size_t flen = pmath_expr_length(data->func);
  size_t n;
  pmath_bool_t is_last_iter;

#ifdef DEBUG_LOG_MATCH
  debug_indent(); pmath_debug_print("match_repeated_left ...\n");
#endif

  while(func_start <= flen) {
  NEXT_ARG:
    if(data->analysed.size.max > flen - func_start + 1)
      data->analysed.size.max = flen - func_start + 1;

    MATCH_MULTI_ARGS_LOOP(n, is_last_iter, data->analysed,
    {
      pmath_t arg = PMATH_NULL;
      match_kind_t kind;

      if(n == 1) {
        arg = pmath_expr_get_item(data->func, func_start);
      }
      else {
        arg = pmath_expr_set_item(
          pmath_expr_get_item_range(data->func, func_start, n),
          0, PMATH_MAGIC_PATTERN_SEQUENCE);
      }

      kind = match_atom(data->info, data->pat, arg, func_start, flen);
      pmath_unref(arg);

      if(kind == PMATH_MATCH_KIND_GLOBAL)
        return kind;

      if(kind == PMATH_MATCH_KIND_LOCAL) {
        if(is_last_iter) {
          if(func_start + n <= flen) {
            func_start += n;
            goto NEXT_ARG;
          }
        }

        kind = match_repeated_left(data, func_start + n);
        if(kind != PMATH_MATCH_KIND_NONE)
          return kind;
      }
    });

    return PMATH_MATCH_KIND_NONE;
  }

  return PMATH_MATCH_KIND_LOCAL;
}

static match_kind_t match_repeated(
  pattern_info_t *info,
  pmath_t         pat,   // wont be freed
  pmath_expr_t    arg    // wont be freed
) {
  _pmath_pattern_analyse_input_t   analyse_in;
  match_repeated_data_t data;
  match_kind_t kind;

  data.info = info;
  data.pat  = pat;
  data.func = arg;

  memset(&analyse_in, 0, sizeof(_pmath_pattern_analyse_input_t));
  analyse_in.pat               = pat;
  analyse_in.parent_pat_head   = PMATH_NULL;
  analyse_in.pattern_variables = info->pattern_variables;

  _pmath_pattern_analyse(&analyse_in, &data.analysed);

  if(data.analysed.size.max == 0) {
    if(pmath_expr_length(arg) == 0)
      return PMATH_MATCH_KIND_LOCAL;

    return PMATH_MATCH_KIND_NONE;
  }

  if(data.analysed.size.min == 0)
    data.analysed.size.min = 1;

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

/*============================================================================*/

static pmath_bool_t init_analyse_match_func(match_func_data_t *data) {
  size_t i;
  _pmath_pattern_size_t total_sizes;

  assert(data->pat_infos == NULL);

  total_sizes.min = 0;
  total_sizes.max = 0;

  i = pmath_expr_length(data->pat);
  if(i > 0) {
    _pmath_pattern_analyse_input_t   input;

    data->pat_infos = pmath_mem_alloc(sizeof(data->pat_infos[0]) * i);
    if(!data->pat_infos)
      return FALSE;

    memset(&input, 0, sizeof(input));
    input.parent_pat_head   = data->info->current_head;
    input.pattern_variables = data->info->pattern_variables;
    input.associative       = !!data->associative;

    for(; i > 0; i--) {
      _pmath_pattern_analyse_output_t *output;

      input.pat = pmath_expr_get_item(data->pat, i);

      output = &data->pat_infos[i - 1];
      _pmath_pattern_analyse(&input, output);

      if(total_sizes.min <= SIZE_MAX - output->size.min)
        total_sizes.min += output->size.min;
      else
        total_sizes.min = SIZE_MAX;

      if(total_sizes.max <= SIZE_MAX - output->size.max)
        total_sizes.max += output->size.max;
      else
        total_sizes.max = SIZE_MAX;

      pmath_unref(input.pat);
    }
  }

  if(!data->associative) {
    size_t flen = pmath_expr_length(data->func);

    if(total_sizes.max < flen || total_sizes.min > flen) {
      done_analyse_match_func(data);
      return FALSE;
    }
  }

  return TRUE;
}

static void done_analyse_match_func(match_func_data_t *data) {
  pmath_mem_free(data->pat_infos);
  data->pat_infos = NULL;
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

  assert(pat_start > 0);
  assert(func_start > 0);

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
    size_t n;
    pmath_bool_t is_last_iter;

    pmath_t current_patarg = pmath_expr_get_item(data->pat, pat_start);
    _pmath_pattern_analyse_output_t patarg_out = data->pat_infos[pat_start - 1];

  NEXT_FUNCARG:
#ifdef DEBUG_LOG_MATCH
    debug_indent(); pmath_debug_print("next func arg (%"PRIuPTR")\n",
                                      func_start);
#endif

    if(func_start > flen)
      patarg_out.size.max = 0;
    else if(patarg_out.size.max > flen - func_start + 1)
      patarg_out.size.max = flen - func_start + 1;


    MATCH_MULTI_ARGS_LOOP(n, is_last_iter, patarg_out,
    {
      pmath_t arg = PMATH_NULL;
      match_kind_t kind;

      if(patarg_out.options.no_sequence) {
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
            0, PMATH_MAGIC_PATTERN_SEQUENCE);
      }

      kind = match_atom(data->info, current_patarg, arg, func_start, flen);
      pmath_unref(arg);
      if(kind == PMATH_MATCH_KIND_GLOBAL) {
        pmath_unref(current_patarg);
        return kind;
      }

      if(kind == PMATH_MATCH_KIND_LOCAL) {
        if(is_last_iter) {
          pmath_unref(current_patarg);
          ++pat_start;
          func_start += n;
          goto NEXT_PATARG;
        }

        kind = match_func_left(data, pat_start + 1, func_start + n);
        if(kind != PMATH_MATCH_KIND_NONE) {
          pmath_unref(current_patarg);
          return kind;
        }
      }
    });

    if( data->info->associative                  &&
        pmath_same(data->func, data->info->func) &&
        func_start + patarg_out.size.min - 1 <= flen)
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
        pmath_unref(current_patarg);
        goto FIRST_PATARG;
      }
    }

    pmath_unref(current_patarg);
    return PMATH_MATCH_KIND_NONE;
  }
}

/*============================================================================*/

static PMATH_INLINE pmath_bool_t index_start(
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

  match_kind_t kind;
  char *args_in_use;
  size_t *indices;
  size_t i, n, j;

  PMATH_ATTRIBUTE_UNUSED pmath_bool_t is_last_iter;

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
    pmath_t current_patarg = pmath_expr_get_item(data->pat, i);
    _pmath_pattern_analyse_output_t patarg_out = data->pat_infos[i - 1];

#ifdef DEBUG_LOG_MATCH
    debug_indent(); pmath_debug_print("next pat arg (%"PRIuPTR")\n", i);
#endif

    if(patarg_out.size.max > flen)
      patarg_out.size.max = flen;

    MATCH_MULTI_ARGS_LOOP(n, is_last_iter, patarg_out,
    {
      if(index_start(indices, args_in_use, n, flen)) {
        do {
          pmath_t arg = PMATH_NULL;

          if(n == 1 && (!patarg_out.options.no_sequence || data->one_identity)) {
            arg = pmath_expr_get_item(data->func, *indices);
          }
          else {
            if(patarg_out.options.no_sequence) {
              arg = pmath_expr_new(pmath_ref(data->info->current_head), n);
            }
            else {
              arg = pmath_expr_new(PMATH_MAGIC_PATTERN_SEQUENCE, n);
            }

            for(j = 0; j < n; ++j) {
              arg = pmath_expr_set_item(
                      arg, j + 1,
                      pmath_expr_get_item(data->func, indices[j]));
            }
          }

//#ifdef DEBUG_LOG_MATCH
//          debug_indent(); pmath_debug_print_object("arg: ", arg, "\n");
//          debug_indent(); show_indices(  "indices: ", indices, n, "\n");
//          debug_indent(); show_arg_usage("arg usage: ", args_in_use, flen, "\n\n");
//#endif

          kind = match_atom(data->info, current_patarg, arg, 1, flen);
          pmath_unref(arg);

          if(kind == PMATH_MATCH_KIND_GLOBAL) {
            pmath_mem_free(args_in_use);
            pmath_mem_free(indices);
            pmath_unref(current_patarg);
            return PMATH_MATCH_KIND_GLOBAL;
          }

          if(kind == PMATH_MATCH_KIND_LOCAL) {
            for(j = 0; j < n; ++j)
              args_in_use[indices[j] - 1] = IN_USE;

            goto NEXT_PATARG;
          }

        } while(index_next(indices, args_in_use, n, flen));
      }
    });


    if( data->associative  &&
        data->one_identity &&
        patarg_out.size.min > 1 &&
        patarg_out.size.max >= patarg_out.size.min)
    {
#ifdef DEBUG_LOG_MATCH
      debug_indent(); pmath_debug_print("... retry with n = 1 ...\n");
#endif

      n = 1;

      if(index_start(indices, args_in_use, n, flen)) {
        do {
          pmath_t arg;

          arg = pmath_expr_get_item(data->func, *indices);

#ifdef DEBUG_LOG_MATCH
          debug_indent(); pmath_debug_print_object("arg: ", arg, "\n");
          debug_indent(); show_indices(  "indices: ", indices, 1, "\n");
          debug_indent(); show_arg_usage("arg usage: ", args_in_use, flen, "\n\n");
#endif

          kind = match_atom(data->info, current_patarg, arg, 1, flen);
          pmath_unref(arg);

          if(kind == PMATH_MATCH_KIND_GLOBAL) {
            pmath_mem_free(args_in_use);
            pmath_mem_free(indices);
            pmath_unref(current_patarg);
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

    pmath_unref(current_patarg);
    pmath_mem_free(args_in_use);
    pmath_mem_free(indices);
    return PMATH_MATCH_KIND_NONE;

  NEXT_PATARG:
    pmath_unref(current_patarg);
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

    memset(&data, 0, sizeof(data));
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

    if(init_analyse_match_func(&data)) {
      if(data.symmetric)
        kind = match_func_symmetric(&data);
      else
        kind = match_func_left(&data, 1, 1);

      done_analyse_match_func(&data);
    }
    else
      kind = PMATH_MATCH_KIND_NONE;

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

//} ... match patterns
//{ replace expression parts ...

static pmath_bool_t replace_with_literal_exact_once(
  pmath_t *pattern,
  pmath_t  _old,
  pmath_t  _new_literal
) {
  size_t i, len;

  if(pmath_same(*pattern, _old)) {
    pmath_unref(*pattern);

    if(pmath_is_expr_of(_new_literal, PMATH_MAGIC_PATTERN_SEQUENCE)) {
      *pattern = pmath_expr_set_item(
                   pmath_ref(_new_literal),
                   0,
                   pmath_ref(pmath_System_Literal));
    }
    else {
      *pattern = pmath_expr_new_extended(
                   pmath_ref(pmath_System_Literal), 1, pmath_ref(_new_literal));
    }

    return TRUE;
  }
  if(!pmath_is_expr(*pattern))
    return FALSE;

  len = pmath_expr_length(*pattern);
  for(i = len + 1; i > 0; --i) {
    pmath_t pat = pmath_expr_get_item(*pattern, i - 1);
    if(replace_with_literal_exact_once(&pat, _old, _new_literal)) {
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

  if(pmath_same(head, pmath_System_OptionValue) && (len == 1 || len == 2 || len == 4)) {
    pmath_t current_fn;
    pmath_expr_t iter_optionvaluerules;

    if(len == 4) {
      // OptionValue(Automatic, Automatic, name, h)
      pmath_t rules = pmath_expr_get_item(body, 2);
      pmath_unref(rules);

      if(pmath_same(rules, pmath_System_Automatic)) {
        current_fn = pmath_expr_get_item(body, 1);

        if(pmath_same(current_fn, pmath_System_Automatic)) {
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
          if(pmath_same(arghead, PMATH_MAGIC_PATTERN_SEQUENCE))
            arg = pmath_expr_set_item(
                    arg, 0,
                    pmath_ref(pmath_System_List));

          if(len == 4) {
            body = pmath_expr_set_item(body, 1, current_fn);
            body = pmath_expr_set_item(body, 2, arg);
          }
          else {
            tmp = body;
            body = pmath_expr_new_extended(
                     pmath_ref(pmath_System_OptionValue), 3,
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
  pmath_hashtable_t replacements  // entries are _pmath_object_entry_t*
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

static pmath_bool_t contains_replacement_symbols(
  pmath_t           object,       // wont be freed
  pmath_hashtable_t replacements  // entries are struct int_entry_t
) {
  size_t i, len;
  if(pmath_is_symbol(object)) 
    return NULL != pmath_ht_search(replacements, PMATH_AS_PTR(object));

  if(!pmath_is_expr(object))
    return FALSE;
  
  if(pmath_is_packed_array(object)) 
    return NULL != pmath_ht_search(replacements, PMATH_AS_PTR(pmath_System_List));

  len = pmath_expr_length(object);
  for(i = 0; i <= len; ++i) {
    pmath_t item = pmath_expr_get_item(object, i);
    pmath_bool_t result = contains_replacement_symbols(item, replacements);
    pmath_unref(item);

    if(result)
      return TRUE;
  }

  return FALSE;
}

// retains debug-info
static void preprocess_local_assignment(
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
  else if(pmath_is_expr_of(*def, pmath_System_List)) {
    pmath_t debug_info = _pmath_expr_get_debug_info(*def);
    size_t i;
    
    for(i = pmath_expr_length(*def); i > 0; --i) {
      pmath_t item = pmath_expr_extract_item(*def, i);
      
      preprocess_local_assignment(local_expr, &item);
      
      *def = pmath_expr_set_item(*def, i, item);
    }
    
    *def = _pmath_expr_set_debug_info(*def, debug_info);
  }
}

// retains debug-info
static void preprocess_local_one(
  pmath_expr_t *local_expr,
  pmath_t      *def
) {
  if(pmath_is_expr(*def) && pmath_expr_length(*def) == 2) {
    pmath_t obj = pmath_expr_get_item(*def, 0);
    pmath_unref(obj);

    if( pmath_same(obj, pmath_System_Assign) ||
        pmath_same(obj, pmath_System_AssignDelayed))
    {
      pmath_t debug_info = _pmath_expr_get_debug_info(*def);
      obj = pmath_expr_extract_item(*def, 1);
      
      preprocess_local_assignment(local_expr, &obj);
      
      *def = pmath_expr_set_item(*def, 1, obj);
      *def = _pmath_expr_set_debug_info(*def, debug_info);
      return;
    }
  }
  
  preprocess_local_assignment(local_expr, def);
}

// retains debug-info
PMATH_PRIVATE pmath_expr_t _pmath_preprocess_local(
  pmath_expr_t local_expr // will be freed.
) {
  pmath_t debug_info = _pmath_expr_get_debug_info(local_expr);
  pmath_expr_t defs = pmath_expr_get_item(local_expr, 1);
  local_expr = pmath_expr_set_item(local_expr, 1, PMATH_NULL);

  if(pmath_is_expr_of(defs, pmath_System_List)) {
    pmath_t defs_debug_info = _pmath_expr_get_debug_info(defs);
    size_t i;

    for(i = pmath_expr_length(defs); i > 0; --i) {
      pmath_t def = pmath_expr_get_item(defs, i);

      defs = pmath_expr_set_item(defs, i, PMATH_NULL);
      preprocess_local_one(&local_expr, &def);
      defs = pmath_expr_set_item(defs, i, def);
    }

    defs = _pmath_expr_set_debug_info(defs, defs_debug_info);
  }
  else if(!pmath_is_null(defs))
    preprocess_local_one(&local_expr, &defs);

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

  if( (pmath_same(item, pmath_System_Function) ||
       pmath_same(item, pmath_System_Local)    ||
       pmath_same(item, pmath_System_With)) &&
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
      do_flatten = pmath_is_expr_of(item, PMATH_MAGIC_PATTERN_SEQUENCE);
    }

    object = pmath_expr_set_item(object, i, item);
  }

  if(do_flatten)
    object = pmath_expr_flatten(object, PMATH_MAGIC_PATTERN_SEQUENCE, 1);

  object = _pmath_expr_set_debug_info(object, debug_info);

  return object;
}

// retains debug-info
static pmath_t replace_multiple_symbols(
  pmath_t           object,        // will be freed
  pmath_hashtable_t replacements   // entries are pattern_variable_entry_t*
) {
  pmath_bool_t do_flatten;
  pmath_t item, debug_info;
  size_t i, len;

  if(pmath_is_symbol(object)) {
    struct pattern_variable_entry_t *entry;
    entry = pmath_ht_search(replacements, PMATH_AS_PTR(object));
    if(entry) {
      pmath_unref(object);
      return pmath_ref(entry->value);
    }
  }

  if(!pmath_is_expr(object))
    return object;
  
  if(pmath_is_packed_array(object) && !pmath_ht_search(replacements, PMATH_AS_PTR(pmath_System_List)))
    return object;

  debug_info = _pmath_expr_get_debug_info(object);
  item       = pmath_expr_get_item(object, 0);

  if( (pmath_same(item, pmath_System_Function) ||
       pmath_same(item, pmath_System_Local)    ||
       pmath_same(item, pmath_System_With)) &&
      pmath_expr_length(object) > 1         &&
      contains_replacement_symbols(object, replacements))
  {
    pmath_unref(item);

    object = _pmath_preprocess_local(object);
  }
  else {
    item = replace_multiple_symbols(item, replacements);

    object = pmath_expr_set_item(object, 0, item);
  }

  do_flatten = FALSE;

  len = pmath_expr_length(object);
  for(i = 1; i <= len; ++i) {
    item = pmath_expr_extract_item(object, i);

    item = replace_multiple_symbols(item, replacements);

    if(/*i != 0 && */!do_flatten && pmath_is_expr(item)) {
      pmath_t head = pmath_expr_get_item(item, 0);
      pmath_unref(head);
      do_flatten = pmath_same(head, PMATH_MAGIC_PATTERN_SEQUENCE);
    }

    object = pmath_expr_set_item(object, i, item);
  }

  if(do_flatten)
    object = pmath_expr_flatten(object, PMATH_MAGIC_PATTERN_SEQUENCE, 1);

  object = _pmath_expr_set_debug_info(object, debug_info);
  return object;
}

//} ... replace expression parts
