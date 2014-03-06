#ifndef FROM_TYPE
#  error FROM_TYPE not defined
#endif

#ifndef TO_TYPE
#  error TO_TYPE not defined
#endif

#ifndef NAME_pack_array_from_to
#  error NAME_pack_array_from_to not defined
#endif

static void NAME_pack_array_from_to(FROM_TYPE, TO_TYPE)
( struct _pmath_packed_array_t  *array,
  size_t                         level,
  const FROM_TYPE               *array_data,
  TO_TYPE                      **location
) {                                                     
  assert(0 && "invalid conversion");                    
}
