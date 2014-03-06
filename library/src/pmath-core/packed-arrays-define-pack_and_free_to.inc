#ifndef TO_TYPE
#  error TO_TYPE not defined
#endif

#ifndef NAME_pack_and_free_to
#  error NAME_pack_and_free_to not defined
#endif


static void NAME_pack_and_free_to(TO_TYPE)(pmath_t expr, TO_TYPE **location) {              
  if(pmath_is_packed_array(expr)) {                                                         
    struct _pmath_packed_array_t *_array;                                                   
                                                                                            
    _array = (void *)PMATH_AS_PTR(expr);                                                    
                                                                                            
    switch(_array->element_type) {                                                          
      case PMATH_PACKED_INT32:                                                              
        NAME_pack_array_from_to(int32_t, TO_TYPE)(_array, 0, _array->blob->data, location); 
        break;                                                                              
                                                                                            
      case PMATH_PACKED_DOUBLE:                                                             
        NAME_pack_array_from_to(double, TO_TYPE)(_array, 0, _array->blob->data, location);  
        break;                                                                              
                                                                                            
      default:                                                                              
        assert(0 && "bad element type");                                                    
    }                                                                                       
                                                                                            
    _pmath_unref_ptr((void *)_array);                                                       
    return;                                                                                 
  }                                                                                         
                                                                                            
  if(pmath_is_expr(expr)) {                                                                 
    size_t len = pmath_expr_length(expr);                                                   
    size_t i;                                                                               
                                                                                            
    for(i = 1; i <= len; ++i) {                                                             
      pmath_t item = pmath_expr_get_item(expr, i);                                          
                                                                                            
      NAME_pack_and_free_to(TO_TYPE)(item, location);                                       
    }                                                                                       
                                                                                            
    pmath_unref(expr);                                                                      
    return;                                                                                 
  }                                                                                         
                                                                                            
  NAME_pack_and_free_element_to(TO_TYPE)(expr, *location);
  ++*location;
}
