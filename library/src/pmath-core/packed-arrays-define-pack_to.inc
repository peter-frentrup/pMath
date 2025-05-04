#ifndef TO_TYPE
#  error TO_TYPE not defined
#endif

#ifndef NAME_pack_to
#  error NAME_pack_to not defined
#endif

#ifndef NAME_pack_element_to
#  error NAME_pack_element_to not defined
#endif

extern pmath_symbol_t pmath_System_List;


// Returns TRUE if the expr was indeed rectangular
static pmath_bool_t NAME_pack_to(TO_TYPE)(pmath_t expr, size_t dims, TO_TYPE **location) {
  if(pmath_is_packed_array(expr)) {
    struct _pmath_packed_array_t *_array;
    
    _array = (void *)PMATH_AS_PTR(expr);

    if(_array->dimensions != dims)
      return FALSE;
    
    switch(_array->element_type) {
      case PMATH_PACKED_INT32:
        NAME_pack_array_from_to(int32_t, TO_TYPE)(_array, 0, (void*)((const uint8_t*)_array->blob->data + _array->offset), location);
        break;
      
      case PMATH_PACKED_DOUBLE:
        NAME_pack_array_from_to(double, TO_TYPE)(_array, 0, (void*)((const uint8_t*)_array->blob->data + _array->offset), location);
        break;
      
      default:
        assert(0 && "bad element type");
        return FALSE;
    }
    
    return TRUE;
  }
  
  if(pmath_is_expr_of(expr, pmath_System_List)) {
    if(dims < 1)
      return FALSE;
    
    size_t len = pmath_expr_length(expr);
    
    for(size_t i = 1; i <= len; ++i) {
      pmath_t item = pmath_expr_get_item(expr, i);
      
      pmath_bool_t success = NAME_pack_to(TO_TYPE)(item, dims - 1, location);

      pmath_unref(item);
      if(!success)
        return FALSE;
    }
    
    return TRUE;
  }

  if(dims > 0)
    return FALSE;
  
  NAME_pack_element_to(TO_TYPE)(expr, *location);
  ++*location;

  return TRUE;
}
