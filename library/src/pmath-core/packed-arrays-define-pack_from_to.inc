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
  size_t ncdc = array->non_continuous_dimensions_count;
  size_t i;
  size_t *sizes = ARRAY_SIZES(array);
  size_t *steps = ARRAY_STEPS(array);
  
  if(level == ncdc) {
    size_t count = sizes[ncdc] * steps[ncdc] / sizeof(FROM_TYPE);
    
    for(i = 0; i < count; ++i)
      (*location)[i] = (TO_TYPE)array_data[i];
      
    *location += count;
    return;
  }
  
  assert(level < ncdc);
  assert(ncdc < array->dimensions);
  
  for(i = 0; i < sizes[level]; ++i) {
    NAME_pack_array_from_to(FROM_TYPE, TO_TYPE)(array, level + 1, array_data, location);
    array_data = (void *)((uint8_t *)array_data + steps[level]);
  }
}
