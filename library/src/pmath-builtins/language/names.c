#include <pmath-language/scanner.h>

#include <pmath-language/regex-private.h>

#include <pmath-util/debug.h>
#include <pmath-util/emit-and-gather.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/memory.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>

#define PCRE_STATIC
#include <pcre.h>


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
  
  if( pmath_is_expr_of(obj, PMATH_SYMBOL_STRINGEXPRESSION) ||
      pmath_is_expr_of(obj, PMATH_SYMBOL_LIST))
  {
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


static pmath_bool_t stringmatch(
  pmath_string_t str,
  struct _regex_t *regex // NULL means all
) {
  if(!regex)
    return TRUE;
    
  if(pmath_is_string(str)) {
    pmath_bool_t result = FALSE;
    struct _capture_t capture;
    int length;
    char *subject = pmath_string_to_utf8(str, &length);
    
    if(!subject)
      return FALSE;
      
    _pmath_regex_init_capture(regex, &capture);
    
    if(capture.ovector) {
      result = _pmath_regex_match(
                 regex,
                 subject,
                 length,
                 0,
                 PCRE_NO_UTF8_CHECK,
                 &capture,
                 NULL);
    }
    
    _pmath_regex_free_capture(&capture);
    pmath_mem_free(subject);
    return result;
  }
  
  return FALSE;
}


static pmath_t collect_names(struct _regex_t *regex) {
  pmath_gather_begin(PMATH_NULL);
  
  {
    pmath_symbol_t sym = pmath_ref(PMATH_SYMBOL_LIST);
    do {
      pmath_symbol_attributes_t attr = pmath_symbol_get_attributes(sym);
      
      if(attr & PMATH_SYMBOL_ATTRIBUTE_REMOVED) {
        pmath_debug_print_object("removed: ", sym, "\n");
      }
      else {
        pmath_string_t name = pmath_symbol_name(sym);
        if(stringmatch(name, regex))
          pmath_emit(name, PMATH_NULL);
        else
          pmath_unref(name);
      }
      
      sym = pmath_symbol_iter_next(sym);
    } while(!pmath_is_null(sym) && !pmath_same(sym, PMATH_SYMBOL_LIST));
    pmath_unref(sym);
  }
  
  return pmath_gather_end();
}


PMATH_PRIVATE pmath_t builtin_names(pmath_expr_t expr) {
  /* Names("wildcardstring")
     Names(form)
     Names()
   */
  struct _regex_t *regex = NULL;
  int regex_options = 0;
  
  if(pmath_expr_length(expr) == 1) {
    pmath_string_t pattern = pmath_expr_get_item(expr, 1);
    
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
      
//      expr = pmath_parse_string_args(
//               "Local("
//               "{System`Private`p:= StringReplace(`1`, {\"*\" -> Except(\"`\")***, \"@\" -> RegularExpression(\"\\\\p{Ll}+\")})},"
//               "If(StringPosition(`1`,\"`\") === {},"
//               "System`Private`p:= Prepend($NamespacePath, $Namespace) ++ System`Private`p);"
//               "Names().Select(StringMatch(#, StartOfString ++ System`Private`p ++ EndOfString)&)"
//               ")",
//               "(o)", pattern);
    }
    else {
      if(!has_namespace_tick(pattern)) {
        pattern = pmath_parse_string_args(
                    "StartOfString ++ Prepend($NamespacePath, $Namespace) ++ StringReplace(`1`, SingleMatch() -> Except(\"`\")) ++ EndOfString",
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
    
    regex = _pmath_regex_compile(pattern, regex_options);
  }
  else if(pmath_expr_length(expr) != 0) {
    pmath_message_argxxx(pmath_expr_length(expr), 0, 0);
    return expr;
  }
  
  pmath_unref(expr);
  
  expr = collect_names(regex);
  
  _pmath_regex_unref(regex);
  return expr;
}
