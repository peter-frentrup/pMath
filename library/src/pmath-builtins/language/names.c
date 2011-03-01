#include <pmath-language/scanner.h>

#include <pmath-util/debug.h>
#include <pmath-util/emit-and-gather.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>


PMATH_PRIVATE pmath_t builtin_names(pmath_expr_t expr){
/* Names("wildcardstring")
   Names()
 */
  if(pmath_expr_length(expr) == 1){
    pmath_string_t pattern = pmath_expr_get_item(expr, 1);
    
    if(pmath_is_string(pattern)){
      pmath_unref(expr);
      expr = pmath_parse_string_args(
          "Local("
            "{System`Private`p:= StringReplace(`1`, {\"*\" -> Except(\"`\")***, \"@\" -> RegularExpression(\"\\\\p{Ll}+\")})},"
            "If(StringPosition(`1`,\"`\") === {},"
              "System`Private`p:= Prepend($NamespacePath, $Namespace) ++ System`Private`p);"
            "Names().Select(StringMatch(#, StartOfString ++ System`Private`p ++ EndOfString)&)"
            ")", 
        "(o)", pattern);
    }
    else
      pmath_unref(pattern);
    
    return expr;
  }
  
  if(pmath_expr_length(expr) != 0){
    pmath_message_argxxx(pmath_expr_length(expr), 0, 0);
    return expr;
  }

  pmath_unref(expr);

  pmath_gather_begin(NULL);

  {
    pmath_symbol_t sym = pmath_ref(PMATH_SYMBOL_LIST);
    do{
      pmath_symbol_attributes_t attr = pmath_symbol_get_attributes(sym);
      
      if(attr & PMATH_SYMBOL_ATTRIBUTE_REMOVED){
        pmath_debug_print_object("removed: ", sym, "\n");
      }
      else{
        pmath_emit(pmath_symbol_name(sym), NULL);
      }
      
      sym = pmath_symbol_iter_next(sym);
    }while(sym && !pmath_same(sym, PMATH_SYMBOL_LIST));
    pmath_unref(sym);
  }

  return pmath_gather_end();
}
