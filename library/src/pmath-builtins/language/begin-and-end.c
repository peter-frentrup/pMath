#include <pmath-core/expressions-private.h>
#include <pmath-core/numbers.h>

#include <pmath-language/scanner.h>
#include <pmath-language/tokens.h>

#include <pmath-util/concurrency/threads.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/control/definitions-private.h>
#include <pmath-builtins/control/messages-private.h>

#include <limits.h>


PMATH_PRIVATE pmath_bool_t _pmath_is_namespace(pmath_t name){
  const uint16_t *buf;
  int len, i;
  pmath_token_t tok;
  
  if(!pmath_instance_of(name, PMATH_TYPE_STRING))
    return FALSE;
  
  len = pmath_string_length(name);
  buf = pmath_string_buffer(name);
  
  if(len < 2 || buf[len-1] != '`' || buf[0] == '`')
    return FALSE;
  
  for(i = 0;i < len-1;++i){
    if(buf[i] == '`'){
      ++i;
      tok = pmath_token_analyse(buf + i, 1, NULL);
      if(tok != PMATH_TOK_NAME)
        return FALSE;
    }
    else{
      tok = pmath_token_analyse(buf + i, 1, NULL);
      if(tok != PMATH_TOK_DIGIT && tok != PMATH_TOK_NAME)
        return FALSE;
    }
  }
  
  return TRUE;
}

PMATH_PRIVATE pmath_bool_t _pmath_is_namespace_list(pmath_t list){
  size_t i;
  
  if(!pmath_is_expr_of(list, PMATH_SYMBOL_LIST))
    return FALSE;
  
  for(i = pmath_expr_length(list);i > 0;--i){
    pmath_t name = pmath_expr_get_item(list, i);
    
    if(!_pmath_is_namespace(name)){
      pmath_unref(name);
      return FALSE;
    }
    
    pmath_unref(name);
  }
  
  return TRUE;
}

static pmath_bool_t is_namespace_listlist(pmath_t list){
  size_t i;
  
  if(!pmath_is_expr_of(list, PMATH_SYMBOL_LIST))
    return FALSE;
  
  for(i = pmath_expr_length(list);i > 0;--i){
    pmath_t name = pmath_expr_get_item(list, i);
    
    if(!_pmath_is_namespace_list(name)){
      pmath_unref(name);
      return FALSE;
    }
    
    pmath_unref(name);
  }
  
  return TRUE;
}



PMATH_PRIVATE pmath_t builtin_assign_namespace(pmath_expr_t expr){
  pmath_t tag;
  pmath_t lhs;
  pmath_t rhs;
  int kind = _pmath_is_assignment(expr, &tag, &lhs, &rhs);
  
  if(!kind)
    return expr;
  
  if(lhs != PMATH_SYMBOL_CURRENTNAMESPACE){
    pmath_unref(tag);
    pmath_unref(lhs);
    pmath_unref(rhs);
    return expr;
  }
  
  if(tag != PMATH_UNDEFINED && tag != lhs){
    pmath_message(NULL, "tag", 2, tag, lhs);
    
    pmath_unref(expr);
    pmath_unref(rhs);
    
    if(kind < 0)
      return NULL;
    
    if(rhs == PMATH_UNDEFINED)
      return pmath_ref(PMATH_SYMBOL_FAILED);
    return rhs;
  }
  
  pmath_unref(tag);
  pmath_unref(lhs);
  
  if(rhs == PMATH_UNDEFINED){
    pmath_unref(expr);
    return NULL;
  }
  
  if(!_pmath_is_namespace(rhs)){
    pmath_message(PMATH_SYMBOL_CURRENTNAMESPACE, "nsset", 1, pmath_ref(rhs));
    pmath_unref(expr);
    
    if(kind < 0)
      return NULL;
    
    return rhs;
  }
  
  pmath_unref(rhs);
  return expr;
}

PMATH_PRIVATE pmath_t builtin_assign_namespacepath(pmath_expr_t expr){
  pmath_t tag;
  pmath_t lhs;
  pmath_t rhs;
  int kind = _pmath_is_assignment(expr, &tag, &lhs, &rhs);
  
  if(!kind)
    return expr;
  
  if(lhs != PMATH_SYMBOL_NAMESPACEPATH 
  && lhs != PMATH_SYMBOL_INTERNAL_NAMESPACESTACK){
    pmath_unref(tag);
    pmath_unref(lhs);
    pmath_unref(rhs);
    return expr;
  }
  
  if(tag != PMATH_UNDEFINED && tag != lhs){
    pmath_message(NULL, "tag", 2, tag, lhs);
    
    pmath_unref(expr);
    pmath_unref(rhs);
    
    if(kind < 0)
      return NULL;
    
    if(rhs == PMATH_UNDEFINED)
      return pmath_ref(PMATH_SYMBOL_FAILED);
    return rhs;
  }
  
  pmath_unref(tag);
  
  if(rhs == PMATH_UNDEFINED){
    pmath_unref(lhs);
    pmath_unref(expr);
    return NULL;
  }
  
  if(!_pmath_is_namespace_list(rhs)){
    pmath_message(lhs, "nslist", 1, pmath_ref(rhs));
    pmath_unref(lhs);
    pmath_unref(expr);
    
    if(kind < 0){
      pmath_unref(rhs);
      return NULL;
    }
    
    return rhs;
  }
  
  pmath_unref(lhs);
  pmath_unref(rhs);
  return expr;
}



PMATH_PRIVATE pmath_t builtin_begin(pmath_expr_t expr){
  pmath_t ns;
  
  if(pmath_expr_length(expr) != 1){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  ns = pmath_expr_get_item(expr, 1);
  if(!_pmath_is_namespace(ns)){
    pmath_unref(ns);
    pmath_message(NULL, "ns", 2, pmath_integer_new_si(1), pmath_ref(expr));
    return expr;
  }
  pmath_unref(expr);
  
  PMATH_RUN_ARGS(
      "Internal`$NamespaceStack:= Append("
        "Internal`$NamespaceStack, $Namespace);"
      "$Namespace:= `1`",
    "(o)", pmath_ref(ns));
  
  return ns;
}

PMATH_PRIVATE pmath_t builtin_end(pmath_expr_t expr){
  pmath_t oldns, ns, nsstack;
  size_t len;
  
  if(pmath_expr_length(expr) != 0){
    pmath_message_argxxx(pmath_expr_length(expr), 0, 0);
    return expr;
  }
  pmath_unref(expr);
  
  oldns   = pmath_thread_local_load(PMATH_SYMBOL_CURRENTNAMESPACE);
  nsstack = pmath_thread_local_load(PMATH_SYMBOL_INTERNAL_NAMESPACESTACK);
  
  if(!_pmath_is_namespace(oldns) 
  || !_pmath_is_namespace_list(nsstack)
  || pmath_expr_length(nsstack) == 0){
    pmath_unref(oldns);
    pmath_unref(nsstack);
    pmath_message(NULL, "nons", 0);
    return oldns;
  }
  
  len = pmath_expr_length(nsstack);
  
  ns = pmath_expr_get_item(nsstack, len);
  expr = nsstack;
  nsstack = pmath_expr_get_item_range(expr, 1, len-1);
  pmath_unref(expr);
  
  if(!ns || !nsstack){
    pmath_unref(ns);
    pmath_unref(nsstack);
    return oldns;
  }
  
  pmath_unref(
    pmath_thread_local_save(
      PMATH_SYMBOL_CURRENTNAMESPACE, 
      ns));
  
  pmath_unref(
    pmath_thread_local_save(
      PMATH_SYMBOL_INTERNAL_NAMESPACESTACK, 
      nsstack));
  
  return oldns;
}



PMATH_PRIVATE pmath_t builtin_beginpackage(pmath_expr_t expr){
  pmath_t package, nspath;
  size_t exprlen = pmath_expr_length(expr);
  
  if(exprlen < 1 || exprlen > 2){
    pmath_message_argxxx(exprlen, 1, 2);
    return expr;
  }
  
  package = pmath_expr_get_item(expr, 1);
  if(!_pmath_is_namespace(package)){
    pmath_unref(package);
    pmath_message(NULL, "ns", 2, pmath_integer_new_si(1), pmath_ref(expr));
    return expr;
  }
  
  if(exprlen == 2){
    nspath = pmath_expr_get_item(expr, 2);
    
    if(_pmath_is_namespace(nspath)){
      nspath = pmath_build_value("(o)", nspath);
    }
    else if(!_pmath_is_namespace_list(nspath)){
      pmath_unref(package);
      pmath_unref(nspath);
      pmath_message(NULL, "nsls", 2, pmath_integer_new_si(2), pmath_ref(expr));
      return expr;
    }
  }
  else
    nspath = pmath_ref(_pmath_object_emptylist);
  
  PMATH_RUN_ARGS(
      "Internal`$NamespacePathStack:= Append("
        "Internal`$NamespacePathStack, $NamespacePath);"
      "Internal`$NamespaceStack:= Append("
        "Internal`$NamespaceStack, $Namespace);"
      "$Namespace:= `1`;"
      "$NamespacePath:= Join({`1`}, `2`, {\"System`\"});"
      "Synchronize($Packages,"
        "If(Length(Position($Packages, `1`)) === 0,"
          "Unprotect($Packages);"
          "$Packages:= Append($Packages, `1`);"
          "Protect($Packages)));"
      "Scan(`2`, Get)",
    "(oo)", package, nspath);
  
  pmath_unref(expr);
  return NULL;
}
  
  static pmath_bool_t starts_with(pmath_string_t s, pmath_string_t sub){
    const uint16_t *sbuf   = pmath_string_buffer(s);
    const uint16_t *subbuf = pmath_string_buffer(sub);
    int slen   = pmath_string_length(s);
    int sublen = pmath_string_length(sub);
    
    if(slen < sublen)
      return FALSE;
    
    while(sublen-- > 0){
      if(*sbuf++ != *subbuf++)
        return FALSE;
    }
    
    return TRUE;
  }
  
  static void check_name_clashes(pmath_string_t new_namespace){
    pmath_t msg;
    pmath_symbol_t current;
    
    msg = pmath_expr_new_extended(
      pmath_ref(PMATH_SYMBOL_MESSAGENAME), 2,
      pmath_ref(PMATH_SYMBOL_GENERAL),
      PMATH_C_STRING("shdw"));
    
    if(!_pmath_message_is_on(msg)){
      pmath_unref(msg);
      return;
    }
    
    pmath_unref(msg);
    current = pmath_ref(PMATH_SYMBOL_LIST);
    do{
      pmath_string_t name = pmath_symbol_name(current);
      
      if(starts_with(name, new_namespace)){
        pmath_symbol_t other;
        
        name = pmath_string_part(name, pmath_string_length(new_namespace), INT_MAX);
        
        other = pmath_symbol_find(name, FALSE);
        if(other && other != current){
          const uint16_t *buf = pmath_string_buffer(name);
          int             len = pmath_string_length(name);
          
          --len;
          while(len > 0 && buf[len] != '`')
            --len;
          
          pmath_message(PMATH_SYMBOL_GENERAL, "shdw", 3,
            pmath_ref(name),
            pmath_build_value("(oo)", 
              pmath_string_part(name, 0, len+1), 
              pmath_ref(new_namespace)),
            pmath_ref(new_namespace));
        }
        
        pmath_unref(other);
      }
      
      pmath_unref(name);
      
      current = pmath_symbol_iter_next(current);
    }while(current && current != PMATH_SYMBOL_LIST);
    
    pmath_unref(current);
  }

PMATH_PRIVATE pmath_t builtin_endpackage(pmath_expr_t expr){
  pmath_t oldns, ns, nspath, nspathstack, nsstack;
  size_t nslen, nspathlen;
  
  if(pmath_expr_length(expr) != 0){
    pmath_message_argxxx(pmath_expr_length(expr), 0, 0);
    return expr;
  }
  pmath_unref(expr);
  
  oldns       = pmath_thread_local_load(PMATH_SYMBOL_CURRENTNAMESPACE);
  nspathstack = pmath_thread_local_load(PMATH_SYMBOL_INTERNAL_NAMESPACEPATHSTACK);
  nsstack     = pmath_thread_local_load(PMATH_SYMBOL_INTERNAL_NAMESPACESTACK);
  
  if(!_pmath_is_namespace(oldns) 
  || !is_namespace_listlist(nspathstack)
  || pmath_expr_length(nspathstack) == 0
  || !_pmath_is_namespace_list(nsstack)
  || pmath_expr_length(nsstack) == 0){
    pmath_unref(oldns);
    pmath_unref(nsstack);
    pmath_unref(nspathstack);
    pmath_message(NULL, "nons", 0);
    return oldns;
  }
  
  nslen     = pmath_expr_length(nsstack);
  nspathlen = pmath_expr_length(nspathstack);
  
  ns     = pmath_expr_get_item(nsstack,     nslen);
  nspath = pmath_expr_get_item(nspathstack, nspathlen);
  
  expr = nsstack;
  nsstack     = pmath_expr_get_item_range(expr, 1, nslen - 1);
  pmath_unref(expr);
  
  expr = nspathstack;
  nspathstack = pmath_expr_get_item_range(expr, 1, nspathlen - 1);
  pmath_unref(expr);
  
  if(!ns || !nspath || !nsstack || !nspathstack){
    pmath_unref(oldns);
    pmath_unref(ns);
    pmath_unref(nsstack);
    pmath_unref(nspath);
    pmath_unref(nspathstack);
    return NULL;
  }
  
  check_name_clashes(oldns);
  
  pmath_unref(
    pmath_thread_local_save(
      PMATH_SYMBOL_CURRENTNAMESPACE, 
      ns));
  
  pmath_unref(
    pmath_thread_local_save(
      PMATH_SYMBOL_INTERNAL_NAMESPACEPATHSTACK, 
      nspathstack));
  
  pmath_unref(
    pmath_thread_local_save(
      PMATH_SYMBOL_INTERNAL_NAMESPACESTACK, 
      nsstack));
  
  PMATH_RUN_ARGS(
      "$NamespacePath:= Prepend(Select(`1`, # =!= `2` &), `2`)",
    "(oo)", nspath, oldns);
  
  return NULL;
}
