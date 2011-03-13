#include <pmath-core/numbers.h>

#include <pmath-util/emit-and-gather.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/control/definitions-private.h>

PMATH_PRIVATE pmath_bool_t _pmath_get_attributes(
  pmath_symbol_attributes_t *attr,
  pmath_t obj // wont be freed
){
  *attr = 0;
  if(pmath_is_expr(obj)){
    size_t i;
    pmath_t item = pmath_expr_get_item(obj, 0);
    pmath_unref(item);
    if(!pmath_same(item, PMATH_SYMBOL_LIST)){
      pmath_message(PMATH_SYMBOL_ATTRIBUTES, "noattr", 1, pmath_ref(obj));
      return FALSE;
    }

    for(i = 1;i <= pmath_expr_length(obj);++i){
      pmath_symbol_attributes_t a;
      
      item = pmath_expr_get_item(obj, i);
      if(!_pmath_get_attributes(&a, item)){
        pmath_unref(item);
        return FALSE;
      }
      
      pmath_unref(item);
      *attr |= a;
    }
    
    return TRUE;
  }

       if(pmath_same(obj, PMATH_SYMBOL_NHOLDALL))             *attr = PMATH_SYMBOL_ATTRIBUTE_NHOLDALL;
  else if(pmath_same(obj, PMATH_SYMBOL_NHOLDFIRST))           *attr = PMATH_SYMBOL_ATTRIBUTE_NHOLDFIRST;
  else if(pmath_same(obj, PMATH_SYMBOL_NHOLDREST))            *attr = PMATH_SYMBOL_ATTRIBUTE_NHOLDREST;
  else if(pmath_same(obj, PMATH_SYMBOL_ASSOCIATIVE))          *attr = PMATH_SYMBOL_ATTRIBUTE_ASSOCIATIVE;
  else if(pmath_same(obj, PMATH_SYMBOL_DEEPHOLDALL))          *attr = PMATH_SYMBOL_ATTRIBUTE_DEEPHOLDALL;
  else if(pmath_same(obj, PMATH_SYMBOL_DEFINITEFUNCTION))     *attr = PMATH_SYMBOL_ATTRIBUTE_DEFINITEFUNCTION;
  else if(pmath_same(obj, PMATH_SYMBOL_HOLDALL))              *attr = PMATH_SYMBOL_ATTRIBUTE_HOLDALL;
  else if(pmath_same(obj, PMATH_SYMBOL_HOLDALLCOMPLETE))      *attr = PMATH_SYMBOL_ATTRIBUTE_HOLDALLCOMPLETE;
  else if(pmath_same(obj, PMATH_SYMBOL_HOLDFIRST))            *attr = PMATH_SYMBOL_ATTRIBUTE_HOLDFIRST;
  else if(pmath_same(obj, PMATH_SYMBOL_HOLDREST))             *attr = PMATH_SYMBOL_ATTRIBUTE_HOLDREST;
  else if(pmath_same(obj, PMATH_SYMBOL_LISTABLE))             *attr = PMATH_SYMBOL_ATTRIBUTE_LISTABLE;
  else if(pmath_same(obj, PMATH_SYMBOL_NUMERICFUNCTION))      *attr = PMATH_SYMBOL_ATTRIBUTE_NUMERICFUNCTION;
  else if(pmath_same(obj, PMATH_SYMBOL_ONEIDENTITY))          *attr = PMATH_SYMBOL_ATTRIBUTE_ONEIDENTITY;
  else if(pmath_same(obj, PMATH_SYMBOL_PROTECTED))            *attr = PMATH_SYMBOL_ATTRIBUTE_PROTECTED;
  else if(pmath_same(obj, PMATH_SYMBOL_READPROTECTED))        *attr = PMATH_SYMBOL_ATTRIBUTE_READPROTECTED;
  else if(pmath_same(obj, PMATH_SYMBOL_SEQUENCEHOLD))         *attr = PMATH_SYMBOL_ATTRIBUTE_SEQUENCEHOLD;
  else if(pmath_same(obj, PMATH_SYMBOL_SYMMETRIC))            *attr = PMATH_SYMBOL_ATTRIBUTE_SYMMETRIC;
  else if(pmath_same(obj, PMATH_SYMBOL_TEMPORARY))            *attr = PMATH_SYMBOL_ATTRIBUTE_TEMPORARY;
  else if(pmath_same(obj, PMATH_SYMBOL_THREADLOCAL))          *attr = PMATH_SYMBOL_ATTRIBUTE_THREADLOCAL;
  else{
    pmath_message(PMATH_SYMBOL_ATTRIBUTES, "noattr", 1, pmath_ref(obj));
    return FALSE;
  }

  return TRUE;
}

PMATH_PRIVATE pmath_t builtin_assign_attributes(pmath_expr_t expr){
  pmath_t            tag;
  pmath_t            lhs;
  pmath_t            rhs;
  pmath_t            sym;
  pmath_symbol_attributes_t attr;
  
  if(!_pmath_is_assignment(expr, &tag, &lhs, &rhs))
    return expr;
  
  if(!pmath_is_expr_of_len(lhs, PMATH_SYMBOL_ATTRIBUTES, 1)){
    pmath_unref(tag);
    pmath_unref(lhs);
    pmath_unref(rhs);
    return expr;
  }
  
  sym = pmath_expr_get_item(lhs, 1);
  
  if(!pmath_same(tag, PMATH_UNDEFINED)
  && !pmath_same(tag, sym)){
    pmath_message(PMATH_NULL, "tag", 3, tag, lhs, sym);
    
    pmath_unref(expr);
    if(pmath_same(rhs, PMATH_UNDEFINED))
      return pmath_ref(PMATH_SYMBOL_FAILED);
    return rhs;
  }
  
  pmath_unref(tag);
  pmath_unref(expr);
  
  if(pmath_is_string(sym))
    sym = pmath_symbol_find(sym, FALSE);
  
  if(!pmath_is_symbol(sym)){
    pmath_message(PMATH_NULL, "fnsym", 1, lhs);
    
    pmath_unref(sym);
    if(pmath_same(rhs, PMATH_UNDEFINED))
      return pmath_ref(PMATH_SYMBOL_FAILED);
    return rhs;
  }
  
  pmath_unref(lhs);
  
  if(pmath_same(rhs, PMATH_UNDEFINED)){
    attr = 0;
  }
  else if(!_pmath_get_attributes(&attr, rhs)){
    pmath_unref(sym);
    return rhs;
  }
  
  pmath_symbol_set_attributes(sym, attr);
  
  pmath_unref(sym);
  if(pmath_same(rhs, PMATH_UNDEFINED))
    return PMATH_NULL;
  return rhs;
}

PMATH_PRIVATE pmath_t builtin_attributes(pmath_expr_t expr){
  pmath_symbol_t             sym;
  pmath_symbol_attributes_t  attr;
  
  if(pmath_expr_length(expr) != 1){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }

  sym = pmath_expr_get_item(expr, 1);
  
  if(pmath_is_string(sym))
    sym = pmath_symbol_find(sym, FALSE);
    
  if(!pmath_is_symbol(sym)){
    pmath_message(PMATH_NULL, "sym", 2, sym, PMATH_FROM_INT32(1));
    return expr;
  }

  attr = pmath_symbol_get_attributes(sym);
  pmath_unref(expr);
  pmath_unref(sym);

  pmath_gather_begin(PMATH_NULL);

  //{ emit attributes ...
    if(attr & PMATH_SYMBOL_ATTRIBUTE_ASSOCIATIVE)
      pmath_emit(pmath_ref(PMATH_SYMBOL_ASSOCIATIVE), PMATH_NULL);

    if(attr & PMATH_SYMBOL_ATTRIBUTE_DEEPHOLDALL)
      pmath_emit(pmath_ref(PMATH_SYMBOL_DEEPHOLDALL), PMATH_NULL);

    if(attr & PMATH_SYMBOL_ATTRIBUTE_DEFINITEFUNCTION)
      pmath_emit(pmath_ref(PMATH_SYMBOL_DEFINITEFUNCTION), PMATH_NULL);

    if(attr & PMATH_SYMBOL_ATTRIBUTE_HOLDFIRST){
      if(attr & PMATH_SYMBOL_ATTRIBUTE_HOLDREST)
        pmath_emit(pmath_ref(PMATH_SYMBOL_HOLDALL), PMATH_NULL);
      else
        pmath_emit(pmath_ref(PMATH_SYMBOL_HOLDFIRST), PMATH_NULL);
    }
    else if(attr & PMATH_SYMBOL_ATTRIBUTE_HOLDREST)
      pmath_emit(pmath_ref(PMATH_SYMBOL_HOLDREST), PMATH_NULL);

    if(attr & PMATH_SYMBOL_ATTRIBUTE_HOLDALLCOMPLETE)
      pmath_emit(pmath_ref(PMATH_SYMBOL_HOLDALLCOMPLETE), PMATH_NULL);

    if(attr & PMATH_SYMBOL_ATTRIBUTE_LISTABLE)
      pmath_emit(pmath_ref(PMATH_SYMBOL_LISTABLE), PMATH_NULL);

    if(attr & PMATH_SYMBOL_ATTRIBUTE_NHOLDFIRST){
      if(attr & PMATH_SYMBOL_ATTRIBUTE_NHOLDREST)
        pmath_emit(pmath_ref(PMATH_SYMBOL_NHOLDALL), PMATH_NULL);
      else
        pmath_emit(pmath_ref(PMATH_SYMBOL_NHOLDFIRST), PMATH_NULL);
    }
    else if(attr & PMATH_SYMBOL_ATTRIBUTE_NHOLDREST)
      pmath_emit(pmath_ref(PMATH_SYMBOL_NHOLDREST), PMATH_NULL);

    if(attr & PMATH_SYMBOL_ATTRIBUTE_NUMERICFUNCTION)
      pmath_emit(pmath_ref(PMATH_SYMBOL_NUMERICFUNCTION), PMATH_NULL);

    if(attr & PMATH_SYMBOL_ATTRIBUTE_ONEIDENTITY)
      pmath_emit(pmath_ref(PMATH_SYMBOL_ONEIDENTITY), PMATH_NULL);

    if(attr & PMATH_SYMBOL_ATTRIBUTE_PROTECTED)
      pmath_emit(pmath_ref(PMATH_SYMBOL_PROTECTED), PMATH_NULL);

    if(attr & PMATH_SYMBOL_ATTRIBUTE_READPROTECTED)
      pmath_emit(pmath_ref(PMATH_SYMBOL_READPROTECTED), PMATH_NULL);

    if(attr & PMATH_SYMBOL_ATTRIBUTE_SEQUENCEHOLD)
      pmath_emit(pmath_ref(PMATH_SYMBOL_SEQUENCEHOLD), PMATH_NULL);

    if(attr & PMATH_SYMBOL_ATTRIBUTE_SYMMETRIC)
      pmath_emit(pmath_ref(PMATH_SYMBOL_SYMMETRIC), PMATH_NULL);

    if(attr & PMATH_SYMBOL_ATTRIBUTE_TEMPORARY)
      pmath_emit(pmath_ref(PMATH_SYMBOL_TEMPORARY), PMATH_NULL);

    if(attr & PMATH_SYMBOL_ATTRIBUTE_THREADLOCAL)
      pmath_emit(pmath_ref(PMATH_SYMBOL_THREADLOCAL), PMATH_NULL);
  //}

  return pmath_gather_end();
}
