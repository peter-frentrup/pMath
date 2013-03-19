#include <pmath-core/packed-arrays-private.h>
#include <pmath-core/strings-private.h>

#include <pmath-util/hashtables-private.h>
#include <pmath-util/memory.h>
#include <stdio.h>

#ifdef _MSC_VER
#  define snprintf sprintf_s
#endif


/* -------------------------------------------------------------------------- */

struct _pmath_blob_t {
  struct _pmath_t    inherited;
  size_t             data_size;
  void              *data;
  pmath_callback_t   destructor;
  pmath_bool_t       is_readonly;
};

static int compare_blob(pmath_t a, pmath_t b) {
  return (uintptr_t)PMATH_AS_PTR(a) < (uintptr_t)PMATH_AS_PTR(b) ? -1 : 1;
}

static unsigned int hash_blob(pmath_t a) {
  return _pmath_hash_pointer(PMATH_AS_PTR(a));
}

static void destroy_blob(pmath_t a) {
  struct _pmath_blob_t *_blob = (struct _pmath_blob_t *)PMATH_AS_PTR(a);
  
  if(_blob->destructor)
    _blob->destructor(_blob->data);
    
  pmath_mem_free(_blob);
}

static void write_blob(struct pmath_write_ex_t *info, pmath_t a) {
  struct _pmath_blob_t *_blob = (void *)PMATH_AS_PTR(a);
  char s[100];
  
  snprintf(s, sizeof(s), "(/\\/ /* blob of %"PRIuPTR" bytes at 16^^%"PRIxPTR" */)",
           (uintptr_t)_blob->data_size,
           (uintptr_t)_blob->data);
           
  _pmath_write_cstr(s, info->write, info->user);
}

PMATH_API
pmath_blob_t pmath_blob_new(size_t size, pmath_bool_t zeroinit) {
  struct _pmath_blob_t *_blob;
  size_t header_size = ROUND_UP(sizeof(struct _pmath_blob_t), 2 * sizeof(void *));
  
  if(PMATH_UNLIKELY(size >= SIZE_MAX - header_size))
    return PMATH_NULL;
    
  _blob = (void *)PMATH_AS_PTR(_pmath_create_stub(
                                 PMATH_TYPE_SHIFT_BLOB,
                                 header_size + size));
                                 
  if(PMATH_UNLIKELY(!_blob))
    return PMATH_NULL;
    
  _blob->data_size   = size;
  _blob->data        = (void *)((uint8_t *)_blob + header_size);
  _blob->destructor  = NULL;
  _blob->is_readonly = FALSE;
  
  if(zeroinit)
    memset(_blob->data, 0, size);
    
  return PMATH_FROM_PTR(_blob);
}

PMATH_API
pmath_blob_t pmath_blob_new_with_data(
  size_t            size,
  void             *data,
  pmath_callback_t  data_destructor,
  pmath_bool_t      is_readonly
) {
  struct _pmath_blob_t *_blob;
  
  _blob = (void *)PMATH_AS_PTR(_pmath_create_stub(
                                 PMATH_TYPE_SHIFT_BLOB,
                                 sizeof(struct _pmath_blob_t)));
                                 
  if(PMATH_UNLIKELY(!_blob)) {
    if(data_destructor)
      data_destructor(data);
      
    return PMATH_NULL;
  }
  
  _blob->data_size   = size;
  _blob->data        = data;
  _blob->destructor  = data_destructor;
  _blob->is_readonly = is_readonly;
  return PMATH_FROM_PTR(_blob);
}

PMATH_API
pmath_blob_t pmath_blob_make_writeable(pmath_blob_t blob) {
  struct _pmath_blob_t *_blob;
  struct _pmath_blob_t *_newblob;
  pmath_blob_t newblob;
  
  if(PMATH_UNLIKELY(pmath_is_null(blob)))
    return blob;
    
  assert(pmath_is_blob(blob));
  
  _blob = (void *)PMATH_AS_PTR(blob);
  
  if(_pmath_refcount_ptr((void *)_blob) == 1 && !_blob->is_readonly)
    return blob;
    
  newblob = pmath_blob_new(_blob->data_size, FALSE);
  
  if(PMATH_LIKELY(!pmath_is_null(blob))) {
    _newblob = (void *)PMATH_AS_PTR(newblob);
    memcpy(_newblob->data, _blob->data, _blob->data_size);
  }
  
  pmath_unref(blob);
  return newblob;
}

PMATH_API
pmath_bool_t pmath_blob_is_always_readonly(pmath_blob_t blob) {
  struct _pmath_blob_t *_blob;
  
  if(PMATH_UNLIKELY(pmath_is_null(blob)))
    return FALSE;
    
  assert(pmath_is_blob(blob));
  
  _blob = (void *)PMATH_AS_PTR(blob);
  
  return _blob->is_readonly;
}

PMATH_API
size_t pmath_blob_get_size(pmath_blob_t blob) {
  struct _pmath_blob_t *_blob;
  
  if(PMATH_UNLIKELY(pmath_is_null(blob)))
    return 0;
    
  assert(pmath_is_blob(blob));
  
  _blob = (void *)PMATH_AS_PTR(blob);
  
  return _blob->data_size;
}

PMATH_API
const void *pmath_blob_read(pmath_blob_t blob) {
  struct _pmath_blob_t *_blob;
  
  if(PMATH_UNLIKELY(pmath_is_null(blob)))
    return NULL;
    
  assert(pmath_is_blob(blob));
  
  _blob = (void *)PMATH_AS_PTR(blob);
  
  return _blob->data;
}

PMATH_API
void *pmath_blob_try_write(pmath_blob_t blob) {
  struct _pmath_blob_t *_blob;
  
  if(PMATH_UNLIKELY(pmath_is_null(blob)))
    return NULL;
    
  assert(pmath_is_blob(blob));
  
  _blob = (void *)PMATH_AS_PTR(blob);
  
  if(_pmath_refcount_ptr((void *)_blob) != 1 || _blob->is_readonly)
    return NULL;
    
  return _blob->data;
}

/* -------------------------------------------------------------------------- */

struct _pmath_packed_array_t {
  struct _pmath_t           inherited;
  struct _pmath_blob_t     *blob;
  size_t                    offset;
  size_t                    total_size;
  
  uint8_t                   element_type;
  uint8_t                   dimensions;
  uint8_t                   non_continuous_dimensions_count;
  
  unsigned                  cached_hash;
  
  size_t                    sizes_and_steps[1];
};

#define ARRAY_SIZES(_arr)  (&(_arr)->sizes_and_steps[0])
#define ARRAY_STEPS(_arr)  (&(_arr)->sizes_and_steps[(_arr)->dimensions])

static void destroy_packed_array(pmath_t a) {
  struct _pmath_packed_array_t *_array = (void *)PMATH_AS_PTR(a);
  
  if(PMATH_LIKELY(_array->blob != NULL))
    _pmath_unref_ptr(_array->blob);
    
  pmath_mem_free(_array);
}

// This must return the same as for a normal pmath_expr_t List of Lists
// See hash_expression() in core/expressions.c
static unsigned int hash_packed_array(pmath_t a) {
  struct _pmath_packed_array_t *_array = (void *)PMATH_AS_PTR(a);
  
  .....
  
//  unsigned int next = 0;
//  size_t i;
//
//  for(i = 0; i <= pmath_expr_length(expr); i++) {
//    pmath_t item = pmath_expr_get_item(expr, i);
//    unsigned int h = pmath_hash(item);
//    pmath_unref(item);
//    next = incremental_hash(&h, sizeof(h), next);
//  }
//  return next;
}

static pmath_bool_t check_sizes(
  size_t        dimensions,
  const size_t *sizes
) {
  size_t i;
  if(dimensions < 1 || dimensions > 127)
    return FALSE;
    
  for(i = 0; i < dimensions; ++i)
    if(sizes[i] < 1)
      return FALSE;
      
  return TRUE;
}

static pmath_bool_t check_steps(
  enum pmath_packed_type_t  element_type,
  size_t                    dimensions,
  const size_t             *sizes,
  const size_t             *steps,
  size_t                   *total_size
) {
  size_t elem_size = pmath_packed_element_size(element_type);
  size_t i;
  
  if(elem_size < 1)
    return FALSE;
    
  assert(dimensions >= 1);
  
  *total_size = 0;
  
  if(elem_size > steps[dimensions - 1])
    return FALSE;
    
  for(i = dimensions - 1; i > 0; --i) {
    assert(sizes[i] > 0);
    
    if(steps[i - 1] / sizes[i] < steps[i])
      return FALSE;
  }
  
  assert(sizes[0] > 0);
  
  if(steps[0] > SIZE_MAX / sizes[0])
    return FALSE;
    
  *total_size = steps[0] * sizes[0];
  return TRUE;
}

static pmath_bool_t init_default_steps(
  enum pmath_packed_type_t  element_type,
  size_t                    dimensions,
  const size_t             *sizes,
  size_t                   *steps,
  size_t                   *total_size
) {
  size_t elem_size = pmath_packed_element_size(element_type);
  size_t i;
  
  if(elem_size < 1)
    return FALSE;
    
  assert(dimensions >= 1);
  
  *total_size = 0;
  
  steps[dimensions - 1] = elem_size;
  for(i = dimensions - 1; i > 0; --i) {
    assert(sizes[i] > 0);
    
    if(steps[i] > SIZE_MAX / sizes[i])
      return FALSE;
      
    steps[i - 1] = steps[i] * sizes[i];
  }
  
  if(steps[0] > SIZE_MAX / sizes[0])
    return FALSE;
    
  *total_size = steps[0] * sizes[0];
  return TRUE;
}

static size_t count_non_continuous_dimensions(
  enum pmath_packed_type_t  element_type,
  size_t                    dimensions,
  const size_t             *sizes,
  size_t                   *steps,
) {
  size_t elem_size = pmath_packed_element_size(element_type);
  size_t i;
  
  assert(dimensions >= 1);
  
  if(elem_size < 1)
    return dimensions;
    
  if(elem_size != steps[dimensions - 1])
    return dimensions;
    
  for(i = dimensions - 1; i > 0; --i) {
    assert(sizes[i] > 0);
    
    if(steps[i - 1] != steps[i] * sizes[i])
      return i;
  }
  
  return 0;
}

PMATH_API
size_t pmath_packed_element_size(enum pmath_packed_type_t element_type) {

  switch(element_type) {
    case PMATH_PACKED_DOUBLE: return sizeof(double);
    case PMATH_PACKED_INT32:  return sizeof(int32_t);
  }
  
  return 0;
}

PMATH_API
PMATH_ATTRIBUTE_USE_RESULT
pmath_packed_array_t pmath_packed_array_new(
  pmath_blob_t               blob,
  enum pmath_packed_type_t   element_type,
  size_t                     dimensions,
  const size_t              *sizes,
  const size_t              *steps,
  size_t                     offset
) {
  struct _pmath_packed_array_t *_array;
  size_t size;
  
  if(PMATH_UNLIKELY(pmath_is_null(blob)))
    return PMATH_NULL;
    
  assert(pmath_is_blob(blob));
  
  if(!check_sizes(dimensions, sizes)) {
    pmath_unref(blob);
    return PMATH_NULL;
  }
  
  size = sizeof(struct _pmath_packed_array_t) - sizeof(size_t) + 2 * dimensions * sizeof(size_t);
  
  _array = (void *)PMATH_AS_PTR(_pmath_create_stub(
                                  PMATH_TYPE_SHIFT_PACKED_ARRAY,
                                  size));
                                  
  if(PMATH_UNLIKELY(!_array)) {
    pmath_unref(blob);
    return PMATH_NULL;
  }
  
  _array->blob         = (void *)PMATH_AS_PTR(blob);
  _array->offset       = offset;
  _array->element_type = element_type;
  _array->dimensions   = dimensions;
  _array->cached_hash  = 0;
  
  memcpy(ARRAY_SIZES(_array), sizes, dimensions);
  
  if(steps) {
    if(!check_steps(element_type, dimensions, sizes, steps, &_array->total_size)) {
      _pmath_unref_ptr((void *)_array);
      return PMATH_NULL;
    }
    
    memcpy(ARRAY_STEPS(_array), steps, dimensions);
    
    _array->non_continuous_dimensions_count = count_non_continuous_dimensions(
          element_type,
          dimensions,
          ARRAY_SIZES(_array),
          ARRAY_STEPS(_array));
          
  }
  else {
    if(!init_default_steps(element_type, dimensions, sizes, ARRAY_STEPS(_array), &_array->total_size)) {
      _pmath_unref_ptr((void *)_array);
      return PMATH_NULL;
    }
    
    _array->non_continuous_dimensions_count = 0;
  }
  
  if(_array->blob->data_size < offset) {
    _pmath_unref_ptr((void *)_array);
    return PMATH_NULL;
  }
  
  if(_array->blob->data_size - offset < _array->total_size) {
    _pmath_unref_ptr((void *)_array);
    return PMATH_NULL;
  }
  
  // check alignment
  if((((size_t)_array->blob->data + offset) % pmath_packed_element_size(element_type)) != 0) {
    _pmath_unref_ptr((void *)_array);
    return PMATH_NULL;
  }
  
  return PMATH_FROM_PTR((void *)_array);
}

PMATH_API
size_t pmath_packed_array_get_dimensions(pmath_packed_array_t array) {
  struct _pmath_packed_array_t *_array;
  
  if(PMATH_UNLIKELY(pmath_is_null(array)))
    return 0;
    
  assert(pmath_is_packed_array(array))
  
  _array = (void *)PMATH_AS_PTR(array);
  return _array->dimensions;
}

PMATH_API
const size_t *pmath_packed_array_get_sizes(pmath_packed_array_t array) {
  struct _pmath_packed_array_t *_array;
  
  if(PMATH_UNLIKELY(pmath_is_null(array)))
    return NULL;
    
  assert(pmath_is_packed_array(array))
  
  _array = (void *)PMATH_AS_PTR(array);
  return ARRAY_SIZES(_array);
}

PMATH_API
const size_t *pmath_packed_array_get_steps(pmath_packed_array_t array) {
  struct _pmath_packed_array_t *_array;
  
  if(PMATH_UNLIKELY(pmath_is_null(array)))
    return NULL;
    
  assert(pmath_is_packed_array(array))
  
  _array = (void *)PMATH_AS_PTR(array);
  return ARRAY_STEPS(_array);
}

PMATH_API
enum pmath_packed_type_t pmath_packed_array_get_element_type(pmath_packed_array_t array) {
  struct _pmath_packed_array_t *_array;
  
  if(PMATH_UNLIKELY(pmath_is_null(array)))
    return 0;
    
  assert(pmath_is_packed_array(array))
  
  _array = (void *)PMATH_AS_PTR(array);
  return _array->element_type;
}

PMATH_API
const void *pmath_packed_array_read(
  pmath_packed_array_t  array,
  const size_t         *indices
) {
  struct _pmath_packed_array_t *_array;
  size_t k, offset;
  const size_t *sizes;
  const size_t *steps;
  
  if(PMATH_UNLIKELY(pmath_is_null(array)))
    return NULL;
    
  _array = (void *)PMATH_AS_PTR(array);
  
  sizes = ARRAY_SIZES(_array);
  steps = ARRAY_STEPS(_array);
  
  offset = _array->offset;
  for(k = 0; k < _array->dimensions; ++k) {
    size_t i = indices[k];
    
    if(i < 1 || i > sizes[k])
      return NULL;
      
    offset += (i - 1) * steps[k];
  }
  
  return (const void *)((const uint8_t *)_array->blob->data + offset);
}

PMATH_PRIVATE
pmath_t _pmath_packed_element_unbox(const void *data, enum pmath_packed_type_t type) {
  switch(type){
    case PMATH_PACKED_DOUBLE: {
      double d = *(double*)data;
        
      if(isfinite(d))
        return PMATH_FROM_DOUBLE(d == 0 ? +0.0 : d); // convert -0.0 to +0.0
        
      if(d > 0)
        return pmath_ref(_pmath_object_infinity);
        
      if(d < 0)
        return pmath_expr_new_extended(
                 pmath_ref(PMATH_SYMBOL_DIRECTEDINFINITY), 1,
                 PMATH_FROM_INT32(-1));
                 
      return pmath_ref(PMATH_SYMBOL_UNDEFINED);
    }
    
    case PMATH_PACKED_INT32:
      return PMATH_FROM_INT32(*(int32_t*)data);
  }
  
  return PMATH_NULL;
}

/* -------------------------------------------------------------------------- */

PMATH_PRIVATE pmath_bool_t _pmath_packed_arrays_init(void) {
  _pmath_init_special_type(
    PMATH_TYPE_SHIFT_BLOB,
    compare_blob,
    hash_blob,
    destroy_blob,
    NULL,
    write_blob);
    
  .....
    
  return TRUE;
}

PMATH_PRIVATE void _pmath_packed_arrays_done(void) {

}
