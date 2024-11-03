#include <pmath-core/numbers.h>

#include <pmath-core/expressions-private.h> // for  _pmath_object_emptylist
#include <pmath-language/regex-private.h>

#include <pmath-util/association-lists.h>
#include <pmath-util/emit-and-gather.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/memory.h>
#include <pmath-util/messages.h>
#include <pmath-util/option-helpers.h>

#include <pmath-builtins/all-symbols-private.h>

#include <pcre.h>

#define SR_EMIT_LIST          1
#define SR_EMIT_EMPTY         2
#define SR_EMIT_EMPTY_BOUNDS  4


extern pmath_symbol_t pmath_System_All;
extern pmath_symbol_t pmath_System_False;
extern pmath_symbol_t pmath_System_IgnoreCase;
extern pmath_symbol_t pmath_System_List;
extern pmath_symbol_t pmath_System_RegularExpression;
extern pmath_symbol_t pmath_System_Rule;
extern pmath_symbol_t pmath_System_StringExpression;
extern pmath_symbol_t pmath_System_True;

static pmath_t stringreplace(
  pmath_t              obj,           // will be freed
  struct _regex_t    **regex_list,
  struct _capture_t   *capture_list,
  pmath_expr_t         rhs_list,      // wont be freed
  size_t               max_matches,
  int                  options        // SR_XXX
) {
  if(pmath_is_string(obj)) {
    pmath_bool_t more;
    size_t i, count;
    int offset, last;
    
    pmath_gather_begin(PMATH_NULL);
    offset = last = 0;
    
    more = TRUE;
    count = pmath_expr_length(rhs_list);
    while(more && max_matches > 0) {
      pmath_expr_t first_rhs = PMATH_UNDEFINED;
      int next_match_pos = -1;
      size_t next_match_rule = 0;
      
      more = FALSE;
      for(i = count; i > 0; --i) {
        pmath_t rhs = pmath_expr_get_item(rhs_list, i);
        
        if( _pmath_regex_match(
              regex_list[i - 1],
              obj,
              offset,
              PCRE_NO_UTF16_CHECK,
              &capture_list[i - 1],
              &rhs))
        {
          if( next_match_rule == 0 ||
              capture_list[i - 1].ovector[0] <= next_match_pos)
          {
            next_match_rule = i - 1;
            next_match_pos = capture_list[i - 1].ovector[0];
            pmath_unref(first_rhs);
            first_rhs = rhs;
            continue;
          }
        }
        
        pmath_unref(rhs);
      }
      
      if(next_match_pos >= 0) {
        more = TRUE;
        
        if( capture_list[next_match_rule].ovector[0] > last ||
            (options & SR_EMIT_EMPTY_BOUNDS)                ||
            (last > 0 && (options & SR_EMIT_EMPTY)))
        {
          pmath_emit(
            pmath_string_part(
              pmath_ref(obj), 
              last, 
              capture_list[next_match_rule].ovector[0] - last),
            PMATH_NULL);
        }
        
        if(!pmath_same(first_rhs, PMATH_UNDEFINED))
          pmath_emit(first_rhs, PMATH_NULL);
          
        first_rhs = PMATH_NULL;
        --max_matches;
        
        last = capture_list[next_match_rule].ovector[1];
        if( next_match_rule == count - 1 &&
            capture_list[next_match_rule].ovector[0] == last)
        {
          offset = last + 1;
        }
        else
          offset = last;
      }
      
      pmath_unref(first_rhs);
    }
    
    if((options & SR_EMIT_EMPTY_BOUNDS) || last < pmath_string_length(obj)) {
      pmath_emit(
        pmath_string_part(pmath_ref(obj), last, -1),
        PMATH_NULL);
    }
    
    pmath_unref(obj);
    obj = pmath_gather_end();
    if(!(options & SR_EMIT_LIST)) {
      obj = pmath_expr_set_item(obj, 0, pmath_ref(pmath_System_StringExpression));
    }
    
    return obj;
  }
  
  if(pmath_is_expr_of(obj, pmath_System_List)) {
    size_t i;
    for(i = 1; i <= pmath_expr_length(obj); ++i) {
      pmath_t item = pmath_expr_get_item(obj, i);
      obj = pmath_expr_set_item(obj, i, PMATH_NULL);
      
      item = stringreplace(item, regex_list, capture_list, rhs_list, max_matches, options);
      
      if(pmath_same(item, PMATH_UNDEFINED)) {
        pmath_unref(obj);
        return PMATH_UNDEFINED;
      }
      
      obj = pmath_expr_set_item(obj, i, item);
    }
    
    return obj;
  }
  
  pmath_unref(obj);
  return PMATH_UNDEFINED;
}

static pmath_t replace_all(
  pmath_expr_t       expr,          // will be freed
  pmath_t            obj,           // will be freed
  pmath_expr_t       rules,         // will be freed
  int                regex_options,
  size_t             max_matches,
  int                sr_options     // SR_XXX, for stringreplace()
) {
  struct _regex_t    **regex_list;
  struct _capture_t   *capture_list;
  size_t               count, i;
  
  if(pmath_equals(rules, _pmath_object_emptylist)) {
    pmath_unref(rules);
    pmath_unref(expr);
    return obj;
  }
  
  if(pmath_is_rule(rules)) {
    rules = pmath_expr_new_extended(pmath_ref(pmath_System_List), 1, rules);
  }
  
  if(!pmath_is_expr_of(rules, pmath_System_List)) {
    rules = pmath_expr_new_extended(
              pmath_ref(pmath_System_List), 1,
              rules);
  }
  
  count = pmath_expr_length(rules);
  if(count == 0) {
    pmath_unref(rules);
    pmath_unref(rules);
    pmath_unref(rules);
    pmath_unref(expr);
    return obj;
  }
  
  pmath_bool_t has_separate_lhs_rhs = FALSE;
  pmath_t patlist  = PMATH_UNDEFINED;
  pmath_t rhs_list;
  
  if(pmath_is_association_list(rules)) {
    patlist  = pmath_association_list_get_keys(rules);
    rhs_list = pmath_association_list_get_values(rules);
    has_separate_lhs_rhs = TRUE;
  }
  else {
    rhs_list = pmath_expr_new(pmath_ref(pmath_System_List), count);
  }
  
  regex_list   = pmath_mem_alloc(count * sizeof(struct _regex_t *));
  capture_list = pmath_mem_alloc(count * sizeof(struct _capture_t));
  
  if(regex_list && capture_list) {
    for(i = 0; i < count; ++i) {
      pmath_t lhs;
      
      if(has_separate_lhs_rhs) {
        lhs = pmath_expr_get_item(patlist, i + 1);
      }
      else {
        pmath_t rule_i = pmath_expr_get_item(rules, i + 1);
        
        if(!pmath_is_rule(rule_i)) {
          pmath_message(PMATH_NULL, "srep", 1, rule_i);
          pmath_unref(rules);
          pmath_unref(obj);
          
          for(; i > 0; --i) {
            _pmath_regex_unref(regex_list[i - 1]);
            _pmath_regex_free_capture(&capture_list[i - 1]);
          }
          
          pmath_mem_free(regex_list);
          pmath_mem_free(capture_list);
          return expr;
        }
        
        pmath_t rhs = pmath_expr_get_item(rule_i, 2);
        lhs         = pmath_expr_get_item(rule_i, 1);
        pmath_unref(rule_i);
        rhs_list = pmath_expr_set_item(rhs_list, i + 1, rhs);
      }
      
      regex_list[i] = _pmath_regex_compile(lhs, regex_options);
      _pmath_regex_init_capture(regex_list[i], &capture_list[i]);
      
      if(!regex_list[i] || !capture_list[i].ovector) {
        pmath_unref(patlist);
        pmath_unref(rhs_list);
        pmath_unref(rules);
        pmath_unref(obj);
        
        ++i;
        for(; i > 0; --i) {
          _pmath_regex_unref(regex_list[i - 1]);
          _pmath_regex_free_capture(&capture_list[i - 1]);
        }
        
        pmath_mem_free(regex_list);
        pmath_mem_free(capture_list);
        return expr;
      }
    }
    
    obj = stringreplace(obj, regex_list, capture_list, rhs_list, max_matches, sr_options);
    
    if(pmath_same(obj, PMATH_UNDEFINED)) {
      pmath_message(PMATH_NULL, "strse", 2, PMATH_FROM_INT32(1), pmath_ref(expr));
      obj = expr;
      expr = PMATH_NULL;
    }
  }
  
  for(i = 0; i < count; ++i) {
    _pmath_regex_unref(regex_list[i]);
    _pmath_regex_free_capture(&capture_list[i]);
  }
  
  pmath_mem_free(regex_list);
  pmath_mem_free(capture_list);
  pmath_unref(patlist);
  pmath_unref(rhs_list);
  pmath_unref(rules);
  pmath_unref(expr);
  return obj;
}

PMATH_PRIVATE pmath_t builtin_stringreplace(pmath_expr_t expr) {
  /* StringReplace(strings, rules, n)
     StringReplace(s, r)  =  StringCases(s, r, Infinity)
   */
  pmath_expr_t options;
  pmath_t obj;
  size_t max_matches;
  size_t last_nonoption;
  int regex_options;
  
  max_matches = SIZE_MAX;
  last_nonoption = 2;
  if(pmath_expr_length(expr) >= 3) {
    obj = pmath_expr_get_item(expr, 3);
    
    if(pmath_is_int32(obj) && PMATH_AS_INT32(obj) >= 0) {
      max_matches = (size_t)PMATH_AS_INT32(obj);
      last_nonoption = 3;
    }
    else if(!pmath_is_set_of_options(obj)) {
      pmath_unref(obj);
      pmath_message(PMATH_NULL, "intnm", 2, PMATH_FROM_INT32(3), pmath_ref(expr));
      return expr;
    }
    
    pmath_unref(obj);
  }
  else if(pmath_expr_length(expr) < 2) {
    pmath_message_argxxx(pmath_expr_length(expr), 2, 3);
    return expr;
  }
  
  
  options = pmath_options_extract(expr, last_nonoption);
  if(pmath_is_null(options))
    return expr;
    
  regex_options = 0;
  obj = pmath_evaluate(pmath_option_value(PMATH_NULL, pmath_System_IgnoreCase, options));
  if(pmath_same(obj, pmath_System_True)) {
    regex_options |= PCRE_CASELESS;
  }
  else if(!pmath_same(obj, pmath_System_False)) {
    pmath_message(
      PMATH_NULL, "opttf", 2,
      pmath_ref(pmath_System_IgnoreCase),
      obj);
    pmath_unref(options);
    return expr;
  }
  pmath_unref(obj);
  pmath_unref(options);
  
  
  
  return replace_all(
           expr,
           pmath_expr_get_item(expr, 1),
           pmath_expr_get_item(expr, 2),
           regex_options,
           max_matches,
           0);
}

PMATH_PRIVATE pmath_t builtin_stringsplit(pmath_expr_t expr) {
  /* StringSplit(string, split)
     StringSplit(string) = StringSplit(string, " ")
   */
  pmath_expr_t options;
  pmath_t obj, rules;
  size_t max_matches;
  size_t last_nonoption;
  int regex_options;
  int sr_options = SR_EMIT_LIST | SR_EMIT_EMPTY;
  
  max_matches = SIZE_MAX;
  last_nonoption = 2;
  if(pmath_expr_length(expr) >= 3) {
    obj = pmath_expr_get_item(expr, 3);
    
    if(pmath_is_int32(obj) && PMATH_AS_INT32(obj) > 0) {
      max_matches = PMATH_AS_INT32(obj) - 1;
      last_nonoption = 3;
    }
    else if(pmath_same(obj, pmath_System_All)) {
      sr_options |= SR_EMIT_EMPTY_BOUNDS;
      last_nonoption = 3;
    }
    else if(!pmath_is_set_of_options(obj)) {
      pmath_unref(obj);
      pmath_message(PMATH_NULL, "intpm", 2, pmath_ref(expr), PMATH_FROM_INT32(3));
      return expr;
    }
    
    pmath_unref(obj);
  }
  else if(pmath_expr_length(expr) == 1) {
    last_nonoption = 1;
  }
  else if(pmath_expr_length(expr) < 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 2, 3);
    return expr;
  }
  
  
  options = pmath_options_extract(expr, last_nonoption);
  if(pmath_is_null(options))
    return expr;
    
  regex_options = 0;
  obj = pmath_evaluate(pmath_option_value(PMATH_NULL, pmath_System_IgnoreCase, options));
  if(pmath_same(obj, pmath_System_True)) {
    regex_options |= PCRE_CASELESS;
  }
  else if(!pmath_same(obj, pmath_System_False)) {
    pmath_message(
      PMATH_NULL, "opttf", 2,
      pmath_ref(pmath_System_IgnoreCase),
      obj);
    pmath_unref(options);
    return expr;
  }
  pmath_unref(obj);
  pmath_unref(options);
  
  
  if(pmath_expr_length(expr) == 1) {
    rules = pmath_expr_new_extended(
              pmath_ref(pmath_System_Rule), 2,
              pmath_expr_new_extended(
                pmath_ref(pmath_System_RegularExpression), 1,
                PMATH_C_STRING("\\s+")),
              PMATH_UNDEFINED);
  }
  else {
    rules = pmath_expr_get_item(expr, 2);
    if(pmath_is_expr_of(rules, pmath_System_List)) {
      size_t i;
      for(i = pmath_expr_length(rules); i > 0; --i) {
        pmath_t rule = pmath_expr_get_item(rules, i);
        
        if(!pmath_is_rule(rule)) {
          rules = pmath_expr_set_item(rules, i,
                                      pmath_expr_new_extended(
                                        pmath_ref(pmath_System_Rule), 2,
                                        rule,
                                        PMATH_UNDEFINED));
        }
        else
          pmath_unref(rule);
      }
    }
    else if(!pmath_is_rule(rules)) {
      rules = pmath_expr_new_extended(
                pmath_ref(pmath_System_Rule), 2,
                rules,
                PMATH_UNDEFINED);
    }
  }
  
  return replace_all(
           expr,
           pmath_expr_get_item(expr, 1),
           rules,
           regex_options,
           max_matches,
           sr_options);
}
