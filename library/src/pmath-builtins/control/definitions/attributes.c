#include <pmath-core/numbers.h>
#include <pmath-core/symbols-private.h>

#include <pmath-util/concurrency/threads-private.h>
#include <pmath-util/dynamic-private.h>
#include <pmath-util/emit-and-gather.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/control/definitions-private.h>
#include <pmath-builtins/lists-private.h>


extern pmath_symbol_t pmath_System_DollarFailed;
extern pmath_symbol_t pmath_System_Associative;
extern pmath_symbol_t pmath_System_Attributes;
extern pmath_symbol_t pmath_System_DeepHoldAll;
extern pmath_symbol_t pmath_System_DefiniteFunction;
extern pmath_symbol_t pmath_System_Function;
extern pmath_symbol_t pmath_System_HoldAll;
extern pmath_symbol_t pmath_System_HoldAllComplete;
extern pmath_symbol_t pmath_System_HoldFirst;
extern pmath_symbol_t pmath_System_HoldRest;
extern pmath_symbol_t pmath_System_List;
extern pmath_symbol_t pmath_System_Listable;
extern pmath_symbol_t pmath_System_NHoldAll;
extern pmath_symbol_t pmath_System_NHoldFirst;
extern pmath_symbol_t pmath_System_NHoldRest;
extern pmath_symbol_t pmath_System_NumericFunction;
extern pmath_symbol_t pmath_System_OneIdentity;
extern pmath_symbol_t pmath_System_Protected;
extern pmath_symbol_t pmath_System_ReadProtected;
extern pmath_symbol_t pmath_System_SequenceHold;
extern pmath_symbol_t pmath_System_Symmetric;
extern pmath_symbol_t pmath_System_Temporary;
extern pmath_symbol_t pmath_System_ThreadLocal;

PMATH_PRIVATE pmath_bool_t _pmath_get_attributes(
  pmath_symbol_attributes_t *attr,
  pmath_t obj // wont be freed
) {
  *attr = 0;
  if(pmath_is_expr(obj)) {
    size_t i;
    pmath_t item = pmath_expr_get_item(obj, 0);
    pmath_unref(item);
    if(!pmath_same(item, pmath_System_List)) {
      pmath_message(pmath_System_Attributes, "noattr", 1, pmath_ref(obj));
      return FALSE;
    }
    
    for(i = 1; i <= pmath_expr_length(obj); ++i) {
      pmath_symbol_attributes_t a;
      
      item = pmath_expr_get_item(obj, i);
      if(!_pmath_get_attributes(&a, item)) {
        pmath_unref(item);
        return FALSE;
      }
      
      pmath_unref(item);
      *attr |= a;
    }
    
    return TRUE;
  }
  
  if(     pmath_same(obj, pmath_System_NHoldAll))             *attr = PMATH_SYMBOL_ATTRIBUTE_NHOLDALL;
  else if(pmath_same(obj, pmath_System_NHoldFirst))           *attr = PMATH_SYMBOL_ATTRIBUTE_NHOLDFIRST;
  else if(pmath_same(obj, pmath_System_NHoldRest))            *attr = PMATH_SYMBOL_ATTRIBUTE_NHOLDREST;
  else if(pmath_same(obj, pmath_System_Associative))          *attr = PMATH_SYMBOL_ATTRIBUTE_ASSOCIATIVE;
  else if(pmath_same(obj, pmath_System_DeepHoldAll))          *attr = PMATH_SYMBOL_ATTRIBUTE_DEEPHOLDALL;
  else if(pmath_same(obj, pmath_System_DefiniteFunction))     *attr = PMATH_SYMBOL_ATTRIBUTE_DEFINITEFUNCTION;
  else if(pmath_same(obj, pmath_System_HoldAll))              *attr = PMATH_SYMBOL_ATTRIBUTE_HOLDALL;
  else if(pmath_same(obj, pmath_System_HoldAllComplete))      *attr = PMATH_SYMBOL_ATTRIBUTE_HOLDALLCOMPLETE;
  else if(pmath_same(obj, pmath_System_HoldFirst))            *attr = PMATH_SYMBOL_ATTRIBUTE_HOLDFIRST;
  else if(pmath_same(obj, pmath_System_HoldRest))             *attr = PMATH_SYMBOL_ATTRIBUTE_HOLDREST;
  else if(pmath_same(obj, pmath_System_Listable))             *attr = PMATH_SYMBOL_ATTRIBUTE_LISTABLE;
  else if(pmath_same(obj, pmath_System_NumericFunction))      *attr = PMATH_SYMBOL_ATTRIBUTE_NUMERICFUNCTION;
  else if(pmath_same(obj, pmath_System_OneIdentity))          *attr = PMATH_SYMBOL_ATTRIBUTE_ONEIDENTITY;
  else if(pmath_same(obj, pmath_System_Protected))            *attr = PMATH_SYMBOL_ATTRIBUTE_PROTECTED;
  else if(pmath_same(obj, pmath_System_ReadProtected))        *attr = PMATH_SYMBOL_ATTRIBUTE_READPROTECTED;
  else if(pmath_same(obj, pmath_System_SequenceHold))         *attr = PMATH_SYMBOL_ATTRIBUTE_SEQUENCEHOLD;
  else if(pmath_same(obj, pmath_System_Symmetric))            *attr = PMATH_SYMBOL_ATTRIBUTE_SYMMETRIC;
  else if(pmath_same(obj, pmath_System_Temporary))            *attr = PMATH_SYMBOL_ATTRIBUTE_TEMPORARY;
  else if(pmath_same(obj, pmath_System_ThreadLocal))          *attr = PMATH_SYMBOL_ATTRIBUTE_THREADLOCAL;
  else {
    pmath_message(pmath_System_Attributes, "noattr", 1, pmath_ref(obj));
    return FALSE;
  }
  
  return TRUE;
}

PMATH_PRIVATE
pmath_symbol_attributes_t _pmath_get_function_attributes(pmath_t head) 
{
  pmath_symbol_t head_sym;
  pmath_symbol_attributes_t attrib;
  
  if(pmath_is_symbol(head))
    return pmath_symbol_get_attributes(head);
  
  if(pmath_is_expr_of_len(head, pmath_System_Function, 3)) {
    pmath_t attrib_obj = pmath_expr_get_item(head, 3);
        
    if(_pmath_get_attributes(&attrib, attrib_obj)) {
      pmath_unref(attrib_obj);
      return attrib;
    }
    
    pmath_unref(attrib_obj);
  }
  
  head_sym = _pmath_topmost_symbol(head);
  attrib = pmath_symbol_get_attributes(head_sym);
  pmath_unref(head_sym);
  
  if(attrib & PMATH_SYMBOL_ATTRIBUTE_DEEPHOLDALL) {
    if(attrib & PMATH_SYMBOL_ATTRIBUTE_HOLDALLCOMPLETE)
      return PMATH_SYMBOL_ATTRIBUTE_HOLDALLCOMPLETE;
      
    return PMATH_SYMBOL_ATTRIBUTE_HOLDALL;
  }
  
  return 0;
}

PMATH_PRIVATE pmath_t builtin_assign_attributes(pmath_expr_t expr) {
  pmath_t            tag;
  pmath_t            lhs;
  pmath_t            rhs;
  pmath_t            sym;
  pmath_symbol_attributes_t attr;
  
  if(!_pmath_is_assignment(expr, &tag, &lhs, &rhs))
    return expr;
    
  if(!pmath_is_expr_of_len(lhs, pmath_System_Attributes, 1)) {
    pmath_unref(tag);
    pmath_unref(lhs);
    pmath_unref(rhs);
    return expr;
  }
  
  sym = pmath_expr_get_item(lhs, 1);
  
  if( !pmath_same(tag, PMATH_UNDEFINED) &&
      !pmath_same(tag, sym))
  {
    pmath_message(PMATH_NULL, "tag", 3, tag, lhs, sym);
    
    pmath_unref(expr);
    if(pmath_same(rhs, PMATH_UNDEFINED))
      return pmath_ref(pmath_System_DollarFailed);
    return rhs;
  }
  
  pmath_unref(tag);
  pmath_unref(expr);
  
  if(pmath_is_string(sym))
    sym = pmath_symbol_find(sym, FALSE);
    
  if(!pmath_is_symbol(sym)) {
    pmath_message(PMATH_NULL, "fnsym", 1, lhs);
    
    pmath_unref(sym);
    if(pmath_same(rhs, PMATH_UNDEFINED))
      return pmath_ref(pmath_System_DollarFailed);
    return rhs;
  }
  
  pmath_unref(lhs);
  
  if(pmath_same(rhs, PMATH_UNDEFINED)) {
    attr = 0;
  }
  else if(!_pmath_get_attributes(&attr, rhs)) {
    pmath_unref(sym);
    return rhs;
  }
  
  pmath_symbol_set_attributes(sym, attr);
  
  pmath_unref(sym);
  if(pmath_same(rhs, PMATH_UNDEFINED))
    return PMATH_NULL;
  return rhs;
}

PMATH_PRIVATE pmath_t builtin_attributes(pmath_expr_t expr) {
  pmath_symbol_t             sym;
  pmath_symbol_attributes_t  attr;
  
  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  sym = pmath_expr_get_item(expr, 1);
  
  if(pmath_is_string(sym))
    sym = pmath_symbol_find(sym, FALSE);
    
  if(!pmath_is_symbol(sym)) {
    pmath_message(PMATH_NULL, "sym", 2, sym, PMATH_FROM_INT32(1));
    return expr;
  }
  
  attr = pmath_symbol_get_attributes(sym);
  
  if(pmath_atomic_read_aquire(&_pmath_dynamic_trackers)) {
    pmath_thread_t thread = pmath_thread_get_current();
    
    if(thread->current_dynamic_id) {
      pmath_symbol_track_dynamic(sym, thread->current_dynamic_id);
    }
  }
  
  pmath_unref(expr);
  pmath_unref(sym);
  
  pmath_gather_begin(PMATH_NULL);
  
  //{ emit attributes ...
  if(attr & PMATH_SYMBOL_ATTRIBUTE_ASSOCIATIVE)
    pmath_emit(pmath_ref(pmath_System_Associative), PMATH_NULL);
    
  if(attr & PMATH_SYMBOL_ATTRIBUTE_DEEPHOLDALL)
    pmath_emit(pmath_ref(pmath_System_DeepHoldAll), PMATH_NULL);
    
  if(attr & PMATH_SYMBOL_ATTRIBUTE_DEFINITEFUNCTION)
    pmath_emit(pmath_ref(pmath_System_DefiniteFunction), PMATH_NULL);
    
  if(attr & PMATH_SYMBOL_ATTRIBUTE_HOLDFIRST) {
    if(attr & PMATH_SYMBOL_ATTRIBUTE_HOLDREST)
      pmath_emit(pmath_ref(pmath_System_HoldAll), PMATH_NULL);
    else
      pmath_emit(pmath_ref(pmath_System_HoldFirst), PMATH_NULL);
  }
  else if(attr & PMATH_SYMBOL_ATTRIBUTE_HOLDREST)
    pmath_emit(pmath_ref(pmath_System_HoldRest), PMATH_NULL);
    
  if(attr & PMATH_SYMBOL_ATTRIBUTE_HOLDALLCOMPLETE)
    pmath_emit(pmath_ref(pmath_System_HoldAllComplete), PMATH_NULL);
    
  if(attr & PMATH_SYMBOL_ATTRIBUTE_LISTABLE)
    pmath_emit(pmath_ref(pmath_System_Listable), PMATH_NULL);
    
  if(attr & PMATH_SYMBOL_ATTRIBUTE_NHOLDFIRST) {
    if(attr & PMATH_SYMBOL_ATTRIBUTE_NHOLDREST)
      pmath_emit(pmath_ref(pmath_System_NHoldAll), PMATH_NULL);
    else
      pmath_emit(pmath_ref(pmath_System_NHoldFirst), PMATH_NULL);
  }
  else if(attr & PMATH_SYMBOL_ATTRIBUTE_NHOLDREST)
    pmath_emit(pmath_ref(pmath_System_NHoldRest), PMATH_NULL);
    
  if(attr & PMATH_SYMBOL_ATTRIBUTE_NUMERICFUNCTION)
    pmath_emit(pmath_ref(pmath_System_NumericFunction), PMATH_NULL);
    
  if(attr & PMATH_SYMBOL_ATTRIBUTE_ONEIDENTITY)
    pmath_emit(pmath_ref(pmath_System_OneIdentity), PMATH_NULL);
    
  if(attr & PMATH_SYMBOL_ATTRIBUTE_PROTECTED)
    pmath_emit(pmath_ref(pmath_System_Protected), PMATH_NULL);
    
  if(attr & PMATH_SYMBOL_ATTRIBUTE_READPROTECTED)
    pmath_emit(pmath_ref(pmath_System_ReadProtected), PMATH_NULL);
    
  if(attr & PMATH_SYMBOL_ATTRIBUTE_SEQUENCEHOLD)
    pmath_emit(pmath_ref(pmath_System_SequenceHold), PMATH_NULL);
    
  if(attr & PMATH_SYMBOL_ATTRIBUTE_SYMMETRIC)
    pmath_emit(pmath_ref(pmath_System_Symmetric), PMATH_NULL);
    
  if(attr & PMATH_SYMBOL_ATTRIBUTE_TEMPORARY)
    pmath_emit(pmath_ref(pmath_System_Temporary), PMATH_NULL);
    
  if(attr & PMATH_SYMBOL_ATTRIBUTE_THREADLOCAL)
    pmath_emit(pmath_ref(pmath_System_ThreadLocal), PMATH_NULL);
  //}
  
  return pmath_gather_end();
}
