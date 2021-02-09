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


extern pmath_symbol_t pmath_System_DollarFailed;
extern pmath_symbol_t pmath_System_DollarNamespace;
extern pmath_symbol_t pmath_System_DollarNamespacePath;
extern pmath_symbol_t pmath_System_General;
extern pmath_symbol_t pmath_System_List;
extern pmath_symbol_t pmath_System_MessageName;
extern pmath_symbol_t pmath_Internal_DollarNamespacePathStack;
extern pmath_symbol_t pmath_Internal_DollarNamespaceStack;

static pmath_bool_t is_namespace_listlist(pmath_t list) {
  size_t i;

  if(!pmath_is_expr_of(list, pmath_System_List))
    return FALSE;

  for(i = pmath_expr_length(list); i > 0; --i) {
    pmath_t name = pmath_expr_get_item(list, i);

    if(!pmath_is_namespace_list(name)) {
      pmath_unref(name);
      return FALSE;
    }

    pmath_unref(name);
  }

  return TRUE;
}



PMATH_PRIVATE pmath_t builtin_assign_namespace(pmath_expr_t expr) {
  pmath_t tag;
  pmath_t lhs;
  pmath_t rhs;
  int kind = _pmath_is_assignment(expr, &tag, &lhs, &rhs);

  if(!kind)
    return expr;

  if(!pmath_same(lhs, pmath_System_DollarNamespace)) {
    pmath_unref(tag);
    pmath_unref(lhs);
    pmath_unref(rhs);
    return expr;
  }

  if(!pmath_same(tag, PMATH_UNDEFINED) && !pmath_same(tag, lhs)) {
    pmath_message(PMATH_NULL, "tag", 3, tag, pmath_ref(lhs), lhs);

    pmath_unref(expr);
    pmath_unref(rhs);

    if(kind < 0)
      return PMATH_NULL;

    if(pmath_same(rhs, PMATH_UNDEFINED))
      return pmath_ref(pmath_System_DollarFailed);
    return rhs;
  }

  pmath_unref(tag);
  pmath_unref(lhs);

  if(pmath_same(rhs, PMATH_UNDEFINED)) {
    pmath_unref(expr);
    return PMATH_NULL;
  }

  if(!pmath_is_namespace(rhs)) {
    pmath_message(pmath_System_DollarNamespace, "nsset", 1, pmath_ref(rhs));
    pmath_unref(expr);

    if(kind < 0)
      return PMATH_NULL;

    return rhs;
  }

  pmath_unref(rhs);
  return expr;
}

PMATH_PRIVATE pmath_t builtin_assign_namespacepath(pmath_expr_t expr) {
  pmath_t tag;
  pmath_t lhs;
  pmath_t rhs;
  int kind = _pmath_is_assignment(expr, &tag, &lhs, &rhs);

  if(!kind)
    return expr;

  if( !pmath_same(lhs, pmath_System_DollarNamespacePath) &&
      !pmath_same(lhs, pmath_Internal_DollarNamespaceStack))
  {
    pmath_unref(tag);
    pmath_unref(lhs);
    pmath_unref(rhs);
    return expr;
  }

  if(!pmath_same(tag, PMATH_UNDEFINED) && !pmath_same(tag, lhs)) {
    pmath_message(PMATH_NULL, "tag", 3, tag, pmath_ref(lhs), lhs);

    pmath_unref(expr);
    pmath_unref(rhs);

    if(kind < 0)
      return PMATH_NULL;

    if(pmath_same(rhs, PMATH_UNDEFINED))
      return pmath_ref(pmath_System_DollarFailed);
    return rhs;
  }

  pmath_unref(tag);

  if(pmath_same(rhs, PMATH_UNDEFINED)) {
    pmath_unref(lhs);
    pmath_unref(expr);
    return PMATH_NULL;
  }

  if(!pmath_is_namespace_list(rhs)) {
    pmath_message(lhs, "nslist", 1, pmath_ref(rhs));
    pmath_unref(lhs);
    pmath_unref(expr);

    if(kind < 0) {
      pmath_unref(rhs);
      return PMATH_NULL;
    }

    return rhs;
  }

  pmath_unref(lhs);
  pmath_unref(rhs);
  return expr;
}



PMATH_PRIVATE pmath_t builtin_begin(pmath_expr_t expr) {
  pmath_t ns;

  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }

  ns = pmath_expr_get_item(expr, 1);
  if(!pmath_is_namespace(ns)) {
    pmath_unref(ns);
    pmath_message(PMATH_NULL, "ns", 2, PMATH_FROM_INT32(1), pmath_ref(expr));
    return expr;
  }
  pmath_unref(expr);

  PMATH_RUN_ARGS(
    "Internal`$NamespaceStack:= Append("
    "  Internal`$NamespaceStack, $Namespace);"
    "$Namespace:= `1`",
    "(o)", pmath_ref(ns));

  return ns;
}

PMATH_PRIVATE pmath_t builtin_end(pmath_expr_t expr) {
  pmath_t oldns, ns, nsstack;
  size_t len;

  if(pmath_expr_length(expr) != 0) {
    pmath_message_argxxx(pmath_expr_length(expr), 0, 0);
    return expr;
  }
  pmath_unref(expr);

  oldns   = pmath_thread_local_load(pmath_System_DollarNamespace);
  nsstack = pmath_thread_local_load(pmath_Internal_DollarNamespaceStack);

  if( !pmath_is_namespace(oldns)        ||
      !pmath_is_namespace_list(nsstack) ||
      pmath_expr_length(nsstack) == 0)
  {
    pmath_unref(oldns);
    pmath_unref(nsstack);
    pmath_message(PMATH_NULL, "nons", 0);
    return oldns;
  }

  len = pmath_expr_length(nsstack);

  ns = pmath_expr_get_item(nsstack, len);
  expr = nsstack;
  nsstack = pmath_expr_get_item_range(expr, 1, len - 1);
  pmath_unref(expr);

  if(pmath_is_null(ns) || pmath_is_null(nsstack)) {
    pmath_unref(ns);
    pmath_unref(nsstack);
    return oldns;
  }

  pmath_unref(
    pmath_thread_local_save(
      pmath_System_DollarNamespace,
      ns));

  pmath_unref(
    pmath_thread_local_save(
      pmath_Internal_DollarNamespaceStack,
      nsstack));

  return oldns;
}



PMATH_PRIVATE pmath_t builtin_beginpackage(pmath_expr_t expr) {
  pmath_t package, nspath;
  size_t exprlen = pmath_expr_length(expr);

  if(exprlen < 1 || exprlen > 2) {
    pmath_message_argxxx(exprlen, 1, 2);
    return expr;
  }

  package = pmath_expr_get_item(expr, 1);
  if(!pmath_is_namespace(package)) {
    pmath_unref(package);
    pmath_message(PMATH_NULL, "ns", 2, PMATH_FROM_INT32(1), pmath_ref(expr));
    return expr;
  }

  if(exprlen == 2) {
    nspath = pmath_expr_get_item(expr, 2);

    if(pmath_is_namespace(nspath)) {
      nspath = pmath_build_value("(o)", nspath);
    }
    else if(!pmath_is_namespace_list(nspath)) {
      pmath_unref(package);
      pmath_unref(nspath);
      pmath_message(PMATH_NULL, "nsls", 2, PMATH_FROM_INT32(2), pmath_ref(expr));
      return expr;
    }
  }
  else
    nspath = pmath_ref(_pmath_object_emptylist);

  PMATH_RUN_ARGS(
    "Internal`$NamespacePathStack:= Append("
    "  Internal`$NamespacePathStack, $NamespacePath);"
    "Internal`$NamespaceStack:= Append("
    "  Internal`$NamespaceStack, $Namespace);"
    "$Namespace:= `1`;"
    "$NamespacePath:= Join({`1`}, `2`, {\"System`\"});"
    "Synchronize($Packages,"
    "  If(IsFreeOf($Packages, `1`),"
    "    Unprotect($Packages);"
    "    $Packages:= Append($Packages, `1`);"
    "    Protect($Packages)));"
    "Scan(`2`, Get)",
    "(oo)", package, nspath);

  pmath_unref(expr);
  return PMATH_NULL;
}

static pmath_bool_t starts_with(pmath_string_t s, pmath_string_t sub) {
  const uint16_t *sbuf   = pmath_string_buffer(&s);
  const uint16_t *subbuf = pmath_string_buffer(&sub);
  int slen   = pmath_string_length(s);
  int sublen = pmath_string_length(sub);

  if(slen < sublen)
    return FALSE;

  while(sublen-- > 0) {
    if(*sbuf++ != *subbuf++)
      return FALSE;
  }

  return TRUE;
}

static void check_name_clashes(pmath_string_t new_namespace) {
  pmath_t msg;
  pmath_symbol_t current;

  msg = pmath_expr_new_extended(
          pmath_ref(pmath_System_MessageName), 2,
          pmath_ref(pmath_System_General),
          PMATH_C_STRING("shdw"));

  if(!_pmath_message_is_on(msg)) {
    pmath_unref(msg);
    return;
  }

  pmath_unref(msg);
  current = pmath_ref(pmath_System_List);
  do {
    pmath_string_t name = pmath_symbol_name(current);

    if(starts_with(name, new_namespace)) {
      pmath_symbol_t other;

      name = pmath_string_part(name, pmath_string_length(new_namespace), INT_MAX);

      other = pmath_symbol_find(pmath_ref(name), FALSE);
      if(!pmath_is_null(other) && !pmath_same(other, current)) {
        const uint16_t *buf = pmath_string_buffer(&name);
        int             len = pmath_string_length(name);

        --len;
        while(len > 0 && buf[len] != '`')
          --len;

        pmath_message(pmath_System_General, "shdw", 3,
                      pmath_ref(name),
                      pmath_build_value("(oo)",
                                        pmath_string_part(name, 0, len + 1),
                                        pmath_ref(new_namespace)),
                      pmath_ref(new_namespace));
      }

      pmath_unref(other);
    }

    pmath_unref(name);

    current = pmath_symbol_iter_next(current);
  } while(!pmath_is_null(current) && !pmath_same(current, pmath_System_List));

  pmath_unref(current);
}

PMATH_PRIVATE pmath_t builtin_endpackage(pmath_expr_t expr) {
  pmath_t oldns, ns, nspath, nspathstack, nsstack;
  size_t nslen, nspathlen;

  if(pmath_expr_length(expr) != 0) {
    pmath_message_argxxx(pmath_expr_length(expr), 0, 0);
    return expr;
  }
  pmath_unref(expr);

  oldns       = pmath_thread_local_load(pmath_System_DollarNamespace);
  nspathstack = pmath_thread_local_load(pmath_Internal_DollarNamespacePathStack);
  nsstack     = pmath_thread_local_load(pmath_Internal_DollarNamespaceStack);

  if( !pmath_is_namespace(oldns)         ||
      !is_namespace_listlist(nspathstack) ||
      pmath_expr_length(nspathstack) == 0 ||
      !pmath_is_namespace_list(nsstack)  ||
      pmath_expr_length(nsstack) == 0)
  {
    pmath_unref(oldns);
    pmath_unref(nsstack);
    pmath_unref(nspathstack);
    pmath_message(PMATH_NULL, "nons", 0);
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

  if( pmath_is_null(ns)      ||
      pmath_is_null(nspath)  ||
      pmath_is_null(nsstack) ||
      pmath_is_null(nspathstack))
  {
    pmath_unref(oldns);
    pmath_unref(ns);
    pmath_unref(nsstack);
    pmath_unref(nspath);
    pmath_unref(nspathstack);
    return PMATH_NULL;
  }

  check_name_clashes(oldns);

  pmath_unref(pmath_thread_local_save(pmath_System_DollarNamespace,            ns));
  pmath_unref(pmath_thread_local_save(pmath_Internal_DollarNamespacePathStack, nspathstack));
  pmath_unref(pmath_thread_local_save(pmath_Internal_DollarNamespaceStack,     nsstack));

  PMATH_RUN_ARGS(
    "$NamespacePath:= Prepend(Select(`1`, # =!= `2` &), `2`)",
    "(oo)", nspath, oldns);

  return PMATH_NULL;
}
