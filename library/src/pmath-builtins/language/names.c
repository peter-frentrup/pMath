#include <pmath-language/scanner.h>

#include <pmath-language/regex-private.h>

#include <pmath-util/debug.h>
#include <pmath-util/emit-and-gather.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/option-helpers.h>
#include <pmath-util/memory.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>

#include <pcre.h>
#include <inttypes.h>


static pmath_bool_t has_namespace_tick(pmath_t obj) {
  if(pmath_is_string(obj)) {
    int len = pmath_string_length(obj);
    const uint16_t *buf = pmath_string_buffer(&obj);
    int i;
    
    for(i = 0; i < len; ++i)
      if(buf[i] == '`')
        return TRUE;
        
    return FALSE;
  }
  
  if(pmath_is_expr(obj)) {
    size_t i;
    
    for(i = pmath_expr_length(obj); i > 0; --i) {
      pmath_t item = pmath_expr_get_item(obj, i);
      
      if(has_namespace_tick(item)) {
        pmath_unref(item);
        return TRUE;
      }
      
      pmath_unref(item);
    }
    
    return FALSE;
  }
  
  return FALSE;
}


static pmath_t collect_names(struct _regex_t *regex) {
  struct _capture_t capture;
  
  if(regex)
    _pmath_regex_init_capture(regex, &capture);
    
  pmath_gather_begin(PMATH_NULL);
  
  {
    pmath_symbol_t sym = pmath_ref(PMATH_SYMBOL_LIST);
    do {
      pmath_symbol_attributes_t attr = pmath_symbol_get_attributes(sym);
      
      if(attr & PMATH_SYMBOL_ATTRIBUTE_REMOVED) {
        pmath_debug_print("[removed (refcount = %" PRIdPTR "): ", pmath_refcount(sym) - 1);
        pmath_debug_print_object("", sym, "]\n");
      }
      else {
        pmath_string_t name = pmath_symbol_name(sym);
        pmath_bool_t match = TRUE;
        
        if(regex) {
          match = _pmath_regex_match(
                    regex,
                    name,
                    0,
                    PCRE_NO_UTF16_CHECK,
                    &capture,
                    NULL);
        }
        
        if(match)
          pmath_emit(name, PMATH_NULL);
        else
          pmath_unref(name);
      }
      
      sym = pmath_symbol_iter_next(sym);
    } while(!pmath_is_null(sym) && !pmath_same(sym, PMATH_SYMBOL_LIST));
    pmath_unref(sym);
  }
  
  if(regex)
    _pmath_regex_free_capture(&capture);
    
  return pmath_gather_end();
}


PMATH_PRIVATE pmath_t builtin_names(pmath_expr_t expr) {
  /* Names("wildcardstring")
     Names(form)
     Names()
   */
  struct _regex_t *regex = NULL;
  int pcre_options = 0;
  
  if(pmath_expr_length(expr) >= 1) {
    pmath_expr_t options;
    pmath_string_t pattern;
    pmath_t obj;
    
    options = pmath_options_extract(expr, 1);
    if(pmath_is_null(options))
      return expr;
    
    obj = pmath_option_value(PMATH_NULL, PMATH_SYMBOL_IGNORECASE, options);
    if(pmath_same(obj, PMATH_SYMBOL_TRUE)) {
      pcre_options |= PCRE_CASELESS;
    }
    else if(!pmath_same(obj, PMATH_SYMBOL_FALSE)) {
      pmath_message(
        PMATH_NULL, "opttf", 2,
        pmath_ref(PMATH_SYMBOL_IGNORECASE),
        obj);
      pmath_unref(options);
      return expr;
    }
    pmath_unref(obj);
    pmath_unref(options);
    
    pattern = pmath_expr_get_item(expr, 1);
    
    if(pmath_is_string(pattern)) {
      pattern = pmath_parse_string_args(
                  "With("
                  " {System`Private`p:= StringReplace(`1`, {\"*\" -> Except(\"`\")***, \"@\" -> RegularExpression(\"\\\\p{Ll}+\")})},"
                  " If(StringPosition(`1`,\"`\") === {},"
                  "  StartOfString ++ Prepend($NamespacePath, $Namespace) ++ System`Private`p ++ EndOfString"
                  " ,"
                  "  StartOfString ++ System`Private`p ++ EndOfString))",
                  "(o)", pattern);
      pattern = pmath_evaluate(pattern);
    }
    else {
      if(!has_namespace_tick(pattern)) {
        pattern = pmath_parse_string_args(
                    "StartOfString ++ Prepend($NamespacePath, $Namespace) ++ Replace(`1`, Literal(SingleMatch()) -> Except(\"`\")) ++ EndOfString",
                    "(o)", pattern);
        pattern = pmath_evaluate(pattern);
      }
      else {
        pattern = pmath_expr_new_extended(
                    pmath_ref(PMATH_SYMBOL_STRINGEXPRESSION), 3,
                    pmath_ref(PMATH_SYMBOL_STARTOFSTRING),
                    pattern,
                    pmath_ref(PMATH_SYMBOL_ENDOFSTRING));
      }
    }
    
    regex = _pmath_regex_compile(pattern, pcre_options);
  }
  
  pmath_unref(expr);
  
  expr = collect_names(regex);
  
  _pmath_regex_unref(regex);
  return expr;
}
