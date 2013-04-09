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

static unsigned int hash_double(double d) {
  if(isfinite(d)) {
    if(d == 0)
      d = +0.0;
      
    return h = incremental_hash(&d, sizeof(double), 0); // pmath_hash(PMATH_FROM_DOUBLE(d))
  }
  
  if(d > 0)
    return pmath_hash(_pmath_object_pos_infinity);
    
  if(d < 0)
    return pmath_hash(_pmath_object_neg_infinity);
    
  return pmath_hash(PMATH_SYMBOL_UNDEFINED);
}

static unsigned int hash_packed_array_of_double(
  struct _pmath_packed_array_t *array,
  const void                   *data,
) {
  size_t nums = ARRAY_SIZES(array)[array->dimensions - 1];
  size_t step = ARRAY_STEPS(array)[array->dimensions - 1];
  
  const uint8_t *ptr = data;
  
  unsigned hash = pmath_hash(PMATH_SYMBOL_LIST);
  
  for(; nums > 0; --num, ptr += step) {
    unsigned h = hash_double(*(double *)ptr);
    
    hash = incremental_hash(&h, sizeof(h), hash);
  }
  
  return hash;
}

static unsigned int hash_packed_array_of_int32(
  struct _pmath_packed_array_t *array,
  const void                   *data,
) {
  size_t nums = ARRAY_SIZES(array)[array->dimensions - 1];
  size_t step = ARRAY_STEPS(array)[array->dimensions - 1];
  
  const uint8_t *ptr = data;
  
  unsigned hash = pmath_hash(PMATH_SYMBOL_LIST);
  pmath_t dummy = PMATH_FROM_INT32(0);
  
  for(; nums > 0; --num, ptr += step) {
    unsigned h;
    
    dummy.s.u.as_int32 = *(int32_t *)ptr;
    
    h = incremental_hash(&dummy, sizeof(pmath_t), 0); // = pmath_hash(dummy)
    
    hash = incremental_hash(&h, sizeof(h), hash);
  }
  
  return hash;
}

static unsigned int hash_packed_array_part(
  struct _pmath_packed_array_t *array,
  const void                   *data,
  size_t                        level
) {
  if(level + 1 < array->dimensions) {
    size_t nums = ARRAY_SIZES(array)[level];
    size_t step = ARRAY_STEPS(array)[level];
    const uint8_t *ptr = data;
    
    unsigned hash = pmath_hash(PMATH_SYMBOL_LIST);
    
    for(; nums > 0; --num, ptr += step) {
      unsigned h = hash_packed_array_part(array, ptr, level + 1);
      
      hash = incremental_hash(&h, sizeof(h), hash);
    }
    
    return hash;
  }
  
  switch(array->element_type) {
    case PMATH_PACKED_DOUBLE: return hash_packed_array_of_double(array, data);
    case PMATH_PACKED_INT32:  return hash_packed_array_of_int32(array, data);
  }
  
  assert(!"reached");
  return 0;
}

// This must return the same as for a normal pmath_expr_t List of Lists
// See hash_expression() in core/expressions.c
static unsigned int hash_packed_array(pmath_t a) {
  struct _pmath_packed_array_t *_array = (void *)PMATH_AS_PTR(a);
  
  if(_array->cached_hash != 0)
    return _array->cached_hash;
    
  _array->cached_hash = hash_packed_array_part(
                          _array,
                          (const void *)((const uint8_t *)_array->blob->data + _array->offset),
                          0);
                          
  return _array->cached_hash;
  
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

static struct _pmath_packed_array_t *duplicate_packed_array(
  struct _pmath_packed_array_t *old_array
) {
  struct _pmath_packed_array_t *new_array;
  size_t size;
  
  if(old_array == NULL)
    return NULL;
    
  size = sizeof(struct _pmath_packed_array_t) - sizeof(size_t) + 2 * old_array->dimensions * sizeof(size_t);
  
  new_array = (void *)PMATH_AS_PTR(_pmath_create_stub(
                                     PMATH_TYPE_SHIFT_PACKED_ARRAY,
                                     size));
                                     
  if(new_array == NULL)
    return NULL;
    
  new_array->offset                          = 0;
  new_array->total_size                      = old_array->total_size;
  new_array->element_type                    = old_array->element_type;
  new_array->dimensions                      = old_array->dimensions;
  new_array->non_continuous_dimensions_count = old_array->non_continuous_dimensions_count;
  new_array->cached_hash                     = 0;//old_array->cached_hash;
  
  memcpy(new_array->sizes_and_steps, old_array->sizes_and_steps, 2 * old_array->dimensions * sizeof(size_t));
  
  new_array->blob = PMATH_AS_PTR(pmath_blob_new(new_array->total_size, FALSE));
  if(new_array->blob == NULL) {
    destroy_packed_array(new_array);
    return NULL;
  }
  
  memcpy(
    new_array->blob->data,
    (const uint8_t *)old_array->blob->data + old_array->offset,
    new_array->total_size);
    
  return new_array;
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

PMATH_API
void *pmath_packed_array_begin_write(
  pmath_packed_array_t *array,
  const size_t         *indices
) {
  struct _pmath_packed_array_t *_array;
  size_t k, offset;
  const size_t *sizes;
  const size_t *steps;
  void *data;
  
  assert(array != NULL);
  
  if(PMATH_UNLIKELY(pmath_is_null(*array)))
    return NULL;
    
  _array = (void *)PMATH_AS_PTR(*array);
  
  sizes = ARRAY_SIZES(_array);
  steps = ARRAY_STEPS(_array);
  
  offset = _array->offset;
  for(k = 0; k < _array->dimensions; ++k) {
    size_t i = indices[k];
    
    if(i < 1 || i > sizes[k])
      return NULL;
      
    offset += (i - 1) * steps[k];
  }
  
  if(pmath_refcount(*array) == 1) {
    data = pmath_blob_try_write(_array->blob);
    
    if(!data) {
      struct _pmath_blob_t *_newblob;
      pmath_blob_t newblob = pmath_blob_new(_array->total_size, FALSE);
      
      if(pmath_is_null(newblob))
        return NULL;
        
      _newblob = (void *)PMATH_AS_PTR(newblob);
      memcpy(
        _newblob->data,
        (const uint8_t *)_array->blob->data + _array->offset,
        _array->total_size);
        
      _pmath_unref_ptr(_array->blob);
      _array->blob = _newblob;
      _array->offset = 0;
      
      offset -= _array->offset;
      data = _newblob->data;
    }
  }
  else {
    _array = duplicate_packed_array(_array);
    
    if(!_array)
      return NULL;
      
    pmath_unref(*array);
    *array = PMATH_FROM_PTR(_array);
    
    offset -= _array->offset;
    data = _array->blob->data;
  }
  
  _array->cached_hash = 0;
  
  return (void *)((uint8_t *)data + offset);
}

PMATH_PRIVATE
pmath_t _pmath_packed_element_unbox(const void *data, enum pmath_packed_type_t type) {
  double d;
  
  switch(type) {
    case PMATH_PACKED_DOUBLE:
      d = *(double *)data;
      
      if(isfinite(d))
        return PMATH_FROM_DOUBLE(d == 0 ? +0.0 : d); // convert -0.0 to +0.0
        
      if(d > 0)
        return pmath_ref(_pmath_object_infinity);
        
      if(d < 0)
        return pmath_ref(_pmath_object_neg_infinity);
        
      return pmath_ref(PMATH_SYMBOL_UNDEFINED);
      
    case PMATH_PACKED_INT32:
      return PMATH_FROM_INT32(*(int32_t *)data);
  }
  
  return PMATH_NULL;
}

PMATH_PRIVATE
PMATH_ATTRIBUTE_PURE
PMATH_ATTRIBUTE_USE_RESULT
pmath_t _pmath_packed_array_get_item(
  pmath_packed_array_t array,
  size_t               index
) {
  struct _pmath_packed_array_t *_array;
  const size_t *sizes;
  const size_t *steps;
  
  if(PMATH_UNLIKELY(pmath_is_null(array)))
    return PMATH_NULL;
    
  _array = (void *)PMATH_AS_PTR(array);
  if(index == 0)
    return pmath_ref(PMATH_SYMBOL_LIST);
    
  sizes = ARRAY_SIZES(_array);
  steps = ARRAY_STEPS(_array);
  
  if(index > _array->sizes[0])
    return PMATH_NULL;
    
  if(_array->dimensions > 1) {
    struct _pmath_packed_array_t *new_array;
    size_t size;
    
    size = sizeof(struct _pmath_packed_array_t) - sizeof(size_t) + 2 * (_array->dimensions - 1) * sizeof(size_t);
    
    new_array = (void *)PMATH_AS_PTR(_pmath_create_stub(
                                       PMATH_TYPE_SHIFT_PACKED_ARRAY,
                                       size));
                                       
    if(new_array == NULL)
      return PMATH_NULL;
      
    new_array->offset                          = _array->offset + (index - 1) * steps[0];
    new_array->total_size                      = _array->total_size / sizes[0];
    new_array->element_type                    = _array->element_type;
    new_array->dimensions                      = _array->dimensions - 1;
    new_array->cached_hash                     = 0;
    
    if(_array->non_continuous_dimensions_count == 0)
      new_array->non_continuous_dimensions_count = 0;
    else
      new_array->non_continuous_dimensions_count = _array->non_continuous_dimensions_count - 1;
      
    memcpy(ARRAY_SIZES(new_array), sizes + 1, new_array->dimensions * sizeof(size_t));
    memcpy(ARRAY_STEPS(new_array), steps + 1, new_array->dimensions * sizeof(size_t));
    
    _pmath_ref_ptr(_array->blob);
    new_array->blob = _array->blob;
    
    return PMATH_FROM_PTR((void *)new_array);
  }
  
  return _pmath_packed_element_unbox(
           (const uint8_t *)_array->blob->data + _array->offset + (index - 1) * steps[0],
           _array->element_type);
}

PMATH_PRIVATE
PMATH_ATTRIBUTE_PURE
PMATH_ATTRIBUTE_USE_RESULT
pmath_expr_t _pmath_packed_array_get_item_range(
  pmath_packed_array_t array,
  size_t               start,
  size_t               length
) {
  struct _pmath_packed_array_t *_array;
  const size_t *sizes;
  const size_t *steps;
  
  if(PMATH_UNLIKELY(pmath_is_null(array)))
    return PMATH_NULL;
    
  _array = (void *)PMATH_AS_PTR(array);
  
  sizes = ARRAY_SIZES(_array);
  steps = ARRAY_STEPS(_array);
  
  if(start == 1 && length >= exprlen)
    return pmath_ref(array);
    
  if(start > exprlen || length == 0)
    return pmath_ref(_pmath_object_emptylist);
    
  if(length > exprlen || start + length > exprlen + 1)
    length = exprlen + 1 - start;
    
  if(start < 1) {
    struct _pmath_expr_t *expr = _pmath_expr_new_noinit(length);
    size_t i;
    
    if(pmath_is_null(expr))
      return PMATH_NULL;
      
    expr->items[0] = pmath_ref(PMATH_SYMBOL_LIST);
    expr->items[1] = pmath_ref(PMATH_SYMBOL_LIST);
    for(i = 1; i < length; ++i)
      expr->items[i + 1] = _pmath_packed_array_get_item(array, i);
      
    return PMATH_FROM_PTR(expr);
  }
  else {
    struct _pmath_packed_array_t *new_array;
    size_t size;
    
    size = sizeof(struct _pmath_packed_array_t) - sizeof(size_t) + 2 * _array->dimensions * sizeof(size_t);
    
    new_array = (void *)PMATH_AS_PTR(_pmath_create_stub(
                                       PMATH_TYPE_SHIFT_PACKED_ARRAY,
                                       size));
                                       
    if(new_array == NULL)
      return PMATH_NULL;
      
    new_array->offset                          = _array->offset + (start - 1) * steps[0];
    new_array->total_size                      = _array->total_size / sizes[0] * length;
    new_array->element_type                    = _array->element_type;
    new_array->dimensions                      = _array->dimensions;
    new_array->cached_hash                     = 0;
    
    if(_array->non_continuous_dimensions_count == 0)
      new_array->non_continuous_dimensions_count = 0;
    else
      new_array->non_continuous_dimensions_count = _array->non_continuous_dimensions_count - 1;
      
    memcpy(new_array->sizes_and_steps, old_array->sizes_and_steps, 2 * old_array->dimensions * sizeof(size_t));
    ARRAY_SIZES(new_array)[0] = length;
    
    _pmath_ref_ptr(_array->blob);
    new_array->blob = _array->blob;
    
    return PMATH_FROM_PTR((void *)new_array);
  }
}

PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
pmath_expr_t _pmath_packed_array_set_item(
  pmath_packed_array_t array,
  size_t               index,
  pmath_t              item
) {
  struct _pmath_packed_array_t *_array;
  const size_t *sizes;
  const size_t *steps;
  const void *old_data;
  void *new_data;
  
  union{
    double  d;
    int32_t i;
  } new_data_store;
  
  if(PMATH_UNLIKELY(pmath_is_null(array)))
    return array;
    
  _array = (void *)PMATH_AS_PTR(array);
  if(index == 0) {
    if(pmath_same(item, PMATH_SYMBOL_LIST)) {
      pmath_unref(item);
      return array;
    }
    
    pmath_debug_print("[unpack array (new head != List)]\n");
    
    return pmath_expr_set_item(
             _pmath_expr_unpack_array(array),
             index,
             item);
  }
  
  sizes = ARRAY_SIZES(_array);
  steps = ARRAY_STEPS(_array);
  
  old_data = (const uint8_t*)_array->blob->data + _array->offset + (index - 1) * steps[0];
  
  if(index > _array->sizes[0]) {
    pmath_unref(item);
    return array;
  }
  
  new_data = NULL;
  
  if(_array->dimensions == 1){
    switch(_array->element_type){
        
      case PMATH_PACKED_DOUBLE:
        new_data = &new_data_store.d;
        if(pmath_is_double(item)) {
          new_data_store.d = PMATH_AS_DOUBLE(item);
        }
        else if(pmath_same(item, PMATH_SYMBOL_UNDEFINED)) {
          new_data_store.d = NAN;
        }
        else if(pmath_equals(item, _pmath_object_pos_infinity)) {
          new_data_store.d = HUGE_VAL;
        }
        else if(pmath_equals(item, _pmath_object_neg_infinity)) {
          new_data_store.d = -HUGE_VAL;
        }
        else{
          return pmath_expr_set_item(
                   _pmath_expr_unpack_array(array),
                   index,
                   item);
        }
        break;
        
      case PMATH_PACKED_INT32:
        new_data = &new_data_store.i;
        if(pmath_is_int32(item)) {
          new_data_store.i = PMATH_AS_INT32(item);
        }
        else{
          return pmath_expr_set_item(
                   _pmath_expr_unpack_array(array),
                   index,
                   item);
        }
        break;
    }
  }
  else if(pmath_is_packed_array(item)){
    struct _pmath_packed_array_t *item_array;
    
    item_array = (void*)PMATH_AS_PTR(item);
    
    if(item_array->element_type != _array->element_type){
      return pmath_expr_set_item(
               _pmath_expr_unpack_array(array),
               index,
               item);
    }
    
    if(item_array->blob == _array->blob){
      new_data = (const uint8_t*)item_array->blob->data + item_array->offset;
      
      if(new_data == old_data){
        pmath_unref(item);
        return array;
      }
      
      ....
    }
    
  }
  else if(pmath_is_expr_of(item)){
  }
  else {
    return pmath_expr_set_item(
             _pmath_expr_unpack_array(array),
             index,
             item);
  }
  
  ......
  
  if(_array->dimensions > 1) {
    struct _pmath_packed_array_t *new_array;
    size_t size;
    
    size = sizeof(struct _pmath_packed_array_t) - sizeof(size_t) + 2 * (_array->dimensions - 1) * sizeof(size_t);
    
    new_array = (void *)PMATH_AS_PTR(_pmath_create_stub(
                                       PMATH_TYPE_SHIFT_PACKED_ARRAY,
                                       size));
                                       
    if(new_array == NULL)
      return PMATH_NULL;
      
    new_array->offset                          = _array->offset + (index - 1) * steps[0];
    new_array->total_size                      = _array->total_size / sizes[0];
    new_array->element_type                    = _array->element_type;
    new_array->dimensions                      = _array->dimensions - 1;
    new_array->cached_hash                     = 0;
    
    if(_array->non_continuous_dimensions_count == 0)
      new_array->non_continuous_dimensions_count = 0;
    else
      new_array->non_continuous_dimensions_count = _array->non_continuous_dimensions_count - 1;
      
    memcpy(ARRAY_SIZES(new_array), sizes + 1, new_array->dimensions * sizeof(size_t));
    memcpy(ARRAY_STEPS(new_array), steps + 1, new_array->dimensions * sizeof(size_t));
    
    _pmath_ref_ptr(_array->blob);
    new_array->blob = _array->blob;
    
    return PMATH_FROM_PTR((void *)new_array);
  }
  
  return _pmath_packed_element_unbox(
           (const uint8_t *)_array->blob->data + _array->offset + (index - 1) * steps[0],
           _array->element_type);
}

/* -------------------------------------------------------------------------- */

PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
pmath_expr_t _pmath_expr_unpack_array(pmath_packed_array_t array) {
  struct _pmath_packed_array_t *_array;
  struct _pmath_expr_t *expr;
  size_t i, length;
  
  if(PMATH_UNLIKELY(pmath_is_null(array)))
    return PMATH_NULL;
    
  _array = (void *)PMATH_AS_PTR(array);
  
  length = ARRAY_SIZES(_array)[0];
  
  expr = _pmath_expr_new_noinit(length);
  if(PMATH_UNLIKELY(pmath_is_null(expr))) {
    pmath_unref(array);
    return PMATH_NULL;
  }
  
  expr->items[0] = pmath_ref(PMATH_SYMBOL_LIST);
  for(i = 1; i <= length; ++i)
    expr->items[i] = _pmath_packed_array_get_item(array, i);
    
  pmath_unref(array);
  return PMATH_FROM_PTR(expr);
}

/* -------------------------------------------------------------------------- */

static int packable_element_type(pmath_t expr) {
  if(pmath_is_packed_array(expr)){
    struct _pmath_packed_array_t *_array;
    
    _array = (void*)PMATH_AS_PTR(expr);
    
    return _array->element_type;
  }
  
  if(pmath_is_expr(expr)) {
    pmath_t item;
    size_t len;
    int result;
    
    item = pmath_expr_get_item(expr, 0);
    pmath_unref(item);
    
    if(!pmath_same(item, PMATH_SYMBOL_LIST))
      return -1;
      
    len = pmath_expr_length(expr);
    if(len == 0)
      return 0;
      
    item = pmath_expr_get_item(expr, len);
    result = packable_element_type(item);
    pmath_unref(item);
    
    if(result < 0)
      return -1;
      
    for(; len > 0; --len) {
      int typ;
      
      item = pmath_expr_get_item(expr, len);
      typ = packable_element_type(item);
      pmath_unref(item);
      
      if(typ != result)
        return -1;
    }
    
    return result;
  }
  
  if(pmath_is_int32(expr))
    return PMATH_PACKED_INT32;
    
  if(pmath_is_double(expr) ||
      pmath_same(expr, PMATH_SYMBOL_UNDEFINED) ||
      pmath_equals(expr, _pmath_object_pos_infinity) ||
      pmath_equals(expr, _pmath_object_neg_infinity))
  {
    return PMATH_PACKED_DOUBLE;
  }
  
  return -1;
}

static size_t packable_dimensions(pmath_t expr){
  if(pmath_is_packed_array(expr)){
    struct _pmath_packed_array_t *_array;
    
    _array = (void*)PMATH_AS_PTR(expr);
    
    return _array->dimensions;
  }
  
  if(pmath_is_expr(expr)){
    pmath_t item;
    size_t result;
    
    if(pmath_expr_length(expr) == 0)
      return 1;
    
    item = pmath_expr_get_item(expr, 1);
    result = 1 + packable_dimensions(item);
    pmath_unref(item);
    
    return result;
  }
  
  return 0;
}

static void pack_array(
  struct _pmath_packed_array_t  *array, 
  size_t                         level, 
  const void                    *array_data,
  void                         **location,
  size_t                         elem_size
){
  size_t ncdc = array->non_continuous_dimensions_count;
  size_t i;
  size_t *sizes = ARRAY_SIZES(array);
  size_t *steps = ARRAY_STEPS(array);
  
  if(level == array->dimensions) {
    memcpy(*location, array_data, elem_size);
    *location = (uint8_t*)location + elem_size;
    return;
  }
  
  if(level == ncdc){
    size_t size = sizes[ncdc] * steps[ncdc];
    
    memcpy(*location, array_data, size);
    *location = (uint8_t*)location + size;
    return;
  }
  
  assert(level < ncdc);
  
  for(i = 0;i < sizes[level];++i){
    pack_array(array, level + 1, array_data, location, elem_size);
    array_data = (uint8_t*)array_data + steps[level];
  }
}

static void pack_and_free_int32(pmath_t expr, int32_t **location){
  if(pmath_is_packed_array(expr)) {
    struct _pmath_packed_array_t *_array;
    
    _array = (void*)PMATH_AS_PTR(expr);
    
    assert(_array->element_type == PMATH_PACKED_INT32);
    
    pack_array(_array, 0, _array->blob->data, location, sizeof(**location));
    pmath_unref(array);
    return;
  }
  
  if(pmath_is_expr(expr)){
    size_t len = pmath_expr_length(expr);
    size_t i;
    
    for(i = 1;i <= len;++i) {
      pmath_t item = pmath_expr_get_item(expr, i);
      pack_and_free_int32(item, location);
    }
    
    pmath_unref(expr);
    return;
  }
  
  assert(pmath_is_int32(expr));
  
  **location = PMATH_AS_INT32(expr);
  ++*location;
}

static void pack_and_free_double(pmath_t expr, double **location){
  if(pmath_is_packed_array(expr)) {
    struct _pmath_packed_array_t *_array;
    
    _array = (void*)PMATH_AS_PTR(expr);
    
    assert(_array->element_type == PMATH_PACKED_DOUBLE);
    
    pack_array(_array, 0, _array->blob->data, location, sizeof(**location));
    pmath_unref(array);
    return;
  }
  
  if(pmath_is_expr(expr)){
    size_t len = pmath_expr_length(expr);
    size_t i;
    
    for(i = 1;i <= len;++i) {
      pmath_t item = pmath_expr_get_item(expr, i);
      pack_and_free_double(item, location);
    }
    
    pmath_unref(expr);
    return;
  }
  
  if(pmath_is_double(expr)){
    **location = PMATH_AS_DOUBLE(expr);
    ++*location;
    return;
  }
  
  if(pmath_same(expr, PMATH_SYMBOL_UNDEFINED)){
    **location = NAN;
    ++*location;
    pmath_unref(expr);
    return;
  }
  
  if(pmath_equals(expr, _pmath_object_pos_infinity)){
    **location = HUGE_VAL;
    ++*location;
    pmath_unref(expr);
    return;
  }
  
  assert(pmath_equals(expr, _pmath_object_neg_infinity));
  
  **location = -HUGE_VAL;
  ++*location;
  pmath_unref(expr);
}

PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
pmath_expr_t _pmath_expr_pack_array(pmath_expr_t expr) {
  int elem_type;
  size_t dims, i;
  pmath_t dims_expr;
  size_t elem_size, total_size;
  size_t *sizes;
  pmath_blob_t blob;
  struct _pmath_blob_t *_blob;
  void *data;
  pmath_packed_array_t *array;
  
  if(pmath_is_null(expr))
    return expr;
    
  if(pmath_is_packed_array(expr))
    return expr;
    
  assert(pmath_is_expr(expr));
  
  elem_type = packable_element_type(expr);
  if(elem_type < 0)
    return expr;
  
  if(elem_type == 0)
    elem_type = PMATH_PACKED_INT32;
  
  dims = packable_dimensions(expr);
  dims_expr = _pmath_dimensions(expr, dims);
  if(pmath_expr_length(dims_expr) != dims){
    pmath_unref(dims_expr);
    return expr;
  }
  
  elem_size  = pmath_packed_element_size(elem_type);
  total_size = elem_size;
  
  sizes = pmath_mem_alloc(dims * sizeof(size_t));
  if(!sizes){
    pmath_unref(dims_expr);
    return expr;
  }
  
  for(i = 0;i < dims;++i){
    pmath_t n_expr = pmath_expr_get_item(dims_expr, i + 1);
    
    sizes[i] = pmath_integer_get_uiptr(n_expr);
    pmath_unref(n_expr);
    
    if(total_size > SIZE_MAX / sizes[i]) {
      pmath_mem_free(sizes);
      pmath_unref(dims_expr);
      return expr;
    }
    
    total_size*= sizes[i];
  }
  
  pmath_unref(dims_expr);
  
  blob = pmath_blob_new(total_size, FALSE);
  if(pmath_is_null(blob)) {
    pmath_mem_free(sizes);
    return expr;
  }
  
  _blob = (void*)PMATH_AS_PTR(blob);
  data = _blob->data;
  switch(elem_type){
    case PMATH_PACKED_DOUBLE: 
      pack_and_free_double(expr, (double**)&data);
      break;
      
    case PMATH_PACKED_INT32: 
      pack_and_free_int32(expr, (int32_t**)&data);
      break;
  }
  
  assert(data == (uint8_t*)_blob->data + _blob->data_size);
  
  array = pmath_packed_array_new(blob, elem_type, dims, sizes, NULL, 0);
  
  pmath_mem_free(sizes);
  return array;
}

/* -------------------------------------------------------------------------- */

static void copy_array(
  void         *dst_data,
  const size_t *dst_steps,
  const void   *src_data,
  const size_t *src_steps,
  const size_t *src_sizes,
  size_t        depth,
  size_t        end_size
){
  size_t i;
  
  if(depth == 0){
    memcpy(dst_data, src_data, end_size);
    return;
  }
  
  for(i = 0;i < *src_sizes;++i){
    copy_array(dst_data, dst_steps + 1, src_data, src_steps + 1, src_sizes + 1, depth - 1, end_size);
    
    dst_data+= *dst_steps;
    src_data+= *src_steps;
  }
}

PMATH_PRIVATE 
void _pmath_packed_array_copy(
  void                         *dst,
  const size_t                 *dst_steps,
  struct _pmath_packed_array_t *src
) {
  size_t cont_level;
  const size_t *src_steps;
  size_t end_size;
  
  assert(dst       != NULL);
  assert(dst_steps != NULL);
  assert(src       != NULL);
  
  const size_t *src_sizes = ARRAY_SIZES(src);
  const size_t *src_steps = ARRAY_STEPS(src);
  
  cont_level = src->dimensions;
  while(cont_level > 0 && dst_steps[cont_level-1] == src->src_steps[cont_level-1])
    --cont_level;
  
  if(cont_level < src->dimensions){
    end_size = src_steps[cont_level] * src_sizes[cont_level];
  }
  else
    end_size = pmath_packed_element_size(src->element_type);
  
  copy_array(
    dst,
    dst_steps,
    (const uint8_t*)src->blob->data + src->offset,
    src_steps,
    src_sizes,
    cont_level,
    end_size);
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
    
  //.....
  
  return TRUE;
}

PMATH_PRIVATE void _pmath_packed_arrays_done(void) {

}
