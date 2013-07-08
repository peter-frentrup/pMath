#include <pmath-core/packed-arrays-private.h>
#include <pmath-core/expressions-private.h>
#include <pmath-core/numbers-private.h>
#include <pmath-core/strings-private.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/lists-private.h>

#include <pmath-util/debug.h>
#include <pmath-util/hashtables-private.h>
#include <pmath-util/helpers.h>
#include <pmath-util/incremental-hash-private.h>
#include <pmath-util/memory.h>

#include <stdio.h>

#ifdef _MSC_VER
#  define snprintf sprintf_s

#  ifndef NAN
    static const uint64_t nan_as_uint64 = 0x7fffffffffffffff;
#    define NAN (*(const double*)nan_as_uint64)
#  endif
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
    _pmath_unref_ptr((void *)_array->blob);
    
  pmath_mem_free(_array);
}

static unsigned int hash_double(double d) {
  if(isfinite(d)) {
    if(d == 0)
      d = +0.0;
      
    return incremental_hash(&d, sizeof(double), 0); // pmath_hash(PMATH_FROM_DOUBLE(d))
  }
  
  if(d > 0)
    return pmath_hash(_pmath_object_pos_infinity);
    
  if(d < 0)
    return pmath_hash(_pmath_object_neg_infinity);
    
  return pmath_hash(PMATH_SYMBOL_UNDEFINED);
}

static unsigned int hash_packed_array_of_double(
  struct _pmath_packed_array_t *array,
  const void                   *data
) {
  size_t nums = ARRAY_SIZES(array)[array->dimensions - 1];
  size_t step = ARRAY_STEPS(array)[array->dimensions - 1];
  
  const uint8_t *ptr = data;
  
  unsigned hash = pmath_hash(PMATH_SYMBOL_LIST);
  
  for(; nums > 0; --nums, ptr += step) {
    unsigned h = hash_double(*(double *)ptr);
    
    hash = incremental_hash(&h, sizeof(h), hash);
  }
  
  return hash;
}

static unsigned int hash_packed_array_of_int32(
  struct _pmath_packed_array_t *array,
  const void                   *data
) {
  size_t nums = ARRAY_SIZES(array)[array->dimensions - 1];
  size_t step = ARRAY_STEPS(array)[array->dimensions - 1];
  
  const uint8_t *ptr = data;
  
  unsigned hash = pmath_hash(PMATH_SYMBOL_LIST);
  pmath_t dummy = PMATH_FROM_INT32(0);
  
  for(; nums > 0; --nums, ptr += step) {
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
    
    for(; nums > 0; --nums, ptr += step) {
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
  
  /*  unsigned int next = 0;
   *  size_t i;
   *
   *  for(i = 0; i <= pmath_expr_length(expr); i++) {
   *    pmath_t item = pmath_expr_get_item(expr, i);
   *    unsigned int h = pmath_hash(item);
   *    pmath_unref(item);
   *    next = incremental_hash(&h, sizeof(h), next);
   *  }
   *  return next;
   */
}

static int compare_array_sizes(
  const size_t *sizes_a,
  const size_t *sizes_b,
  size_t        dim_a,
  size_t        dim_b
) {
  size_t dim = dim_a;
  size_t i;
  
  if(dim > dim_b)
    dim = dim_b;
    
  for(i = 0; i < dim; ++i) {
    size_t len_a = sizes_a[i];
    size_t len_b = sizes_b[i];
    
    if(len_a < len_b)
      return -1;
      
    if(len_a > len_b)
      return 1;
  }
  
  if(dim_a < dim)
    return -1;
    
  if(dim_b < dim)
    return 1;
    
  return 0;
}

/* TODO: Make comparision of +/- Infinity compatible with non-packed arrays
         i.e. -HUGE_VAL (= -Infinity) comes *after* all numbers
 */
static int compare_arrays_of_double(const double *a, const double *b, size_t len) {
  size_t i;
  
  for(i = 0; i < len; ++i) {
    if(a[i] < b[i])
      return -1;
      
    if(a[i] > b[i])
      return 1;
      
    if(!(a[i] == b[i])) {
      pmath_t a_bad = _pmath_packed_element_unbox(&a[i], PMATH_PACKED_DOUBLE);
      pmath_t b_bad = _pmath_packed_element_unbox(&b[i], PMATH_PACKED_DOUBLE);
      
      int cmp = pmath_compare(a_bad, b_bad);
      
      pmath_unref(a_bad);
      pmath_unref(b_bad);
      
      if(cmp < 0)
        return -1;
        
      if(cmp > 0)
        return +1;
    }
  }
  
  return 0;
}

static int compare_arrays_of_int32(const int32_t *a, const int32_t *b, size_t len) {
  size_t i;
  
  for(i = 0; i < len; ++i) {
    if(a[i] < b[i])
      return -1;
      
    if(a[i] > b[i])
      return 1;
  }
  
  return 0;
}

static int compare_array_parts(
  const size_t             *sizes,
  const size_t             *steps_a,
  const size_t             *steps_b,
  size_t                    dims,
  const void               *data_a,
  const void               *data_b,
  enum pmath_packed_type_t  element_type
) {
  --dims;
  
  if(dims) {
    size_t len = sizes[0];
    size_t step_a = steps_a[0];
    size_t step_b = steps_b[0];
    const uint8_t *ptr_a = data_a;
    const uint8_t *ptr_b = data_b;
    
    ++sizes;
    ++steps_a;
    ++steps_b;
    
    for(; len > 0; --len, ptr_a += step_a, ptr_b += step_b) {
      int cmp = compare_array_parts(
                  sizes,
                  steps_a,
                  steps_b,
                  dims,
                  ptr_a,
                  ptr_b,
                  element_type);
                  
      if(cmp != 0)
        return cmp;
    }
    
    return 0;
  }
  
  assert(dims == 0);
  
  switch(element_type) {
    case PMATH_PACKED_DOUBLE: return compare_arrays_of_double(data_a, data_b, sizes[0]);
    case PMATH_PACKED_INT32:  return compare_arrays_of_int32( data_a, data_b, sizes[0]);
  }
  
  assert(!"reached");
  return 0;
}

PMATH_PRIVATE
int _pmath_packed_array_compare(pmath_packed_array_t a, pmath_packed_array_t b) {
  struct _pmath_packed_array_t *array_a;
  struct _pmath_packed_array_t *array_b;
  const size_t *sizes_a;
  const size_t *sizes_b;
  int cmp;
  const void *data_a;
  const void *data_b;
  
  array_a = (void *)PMATH_AS_PTR(a);
  array_b = (void *)PMATH_AS_PTR(b);
  
  sizes_a = ARRAY_SIZES(array_a);
  sizes_b = ARRAY_SIZES(array_b);
  
  cmp = compare_array_sizes(sizes_a, sizes_b, array_a->dimensions, array_b->dimensions);
  if(cmp != 0)
    return cmp;
    
  if(array_a->element_type != array_b->element_type)
    return PMATH_ARRAYS_INCOMPATIBLE_CMP;
    
  data_a = (const void *)((const uint8_t *)array_a->blob->data + array_a->offset);
  data_b = (const void *)((const uint8_t *)array_b->blob->data + array_b->offset);
  
  return compare_array_parts(
           sizes_a,
           ARRAY_STEPS(array_a),
           ARRAY_STEPS(array_b),
           array_a->dimensions,
           data_a,
           data_b,
           array_a->element_type);
}


PMATH_PRIVATE pmath_bool_t _pmath_packed_array_equal(
  pmath_packed_array_t a,
  pmath_packed_array_t b
) {
  int cmp = _pmath_packed_array_compare(a, b);
  
  return cmp == 0;
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
  
  if(elem_size != steps[dimensions - 1]) {
    pmath_debug_print("[bad packed-array steps]\n");
    return FALSE;
  }
  
  for(i = dimensions - 1; i > 0; --i) {
    assert(sizes[i] > 0);
    
    if(steps[i - 1] / sizes[i] < steps[i]) {
      pmath_debug_print("[bad packed-array steps]\n");
      return FALSE;
    }
  }
  
  assert(sizes[0] > 0);
  
  if(steps[0] > SIZE_MAX / sizes[0]) {
    pmath_debug_print("[bad packed-array steps]\n");
    return FALSE;
  }
  
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
  size_t        dimensions,
  const size_t *sizes,
  size_t       *steps
) {
  size_t i;
  
  assert(dimensions >= 1);
  
  for(i = dimensions - 1; i > 0; --i) {
    assert(sizes[i] > 0);
    
    if(steps[i - 1] != steps[i] * sizes[i])
      return i;
  }
  
  return 0;
}

static void copy_array(
  void         *dst_data,
  const size_t *dst_steps,
  const void   *src_data,
  const size_t *src_steps,
  const size_t *src_sizes,
  size_t        depth,
  size_t        end_size
) {
  size_t i;
  
  if(depth == 0) {
    memcpy(dst_data, src_data, end_size);
    return;
  }
  
  for(i = 0; i < *src_sizes; ++i) {
    copy_array(dst_data, dst_steps + 1, src_data, src_steps + 1, src_sizes + 1, depth - 1, end_size);
    
    dst_data = (int8_t *)dst_data + *dst_steps;
    src_data = (int8_t *)src_data + *src_steps;
  }
}

static void packed_array_copy(
  void                         *dst,
  const size_t                 *dst_steps,
  struct _pmath_packed_array_t *src
) {
  size_t cont_level;
  const size_t *src_sizes;
  const size_t *src_steps;
  size_t end_size;
  
  assert(dst       != NULL);
  assert(dst_steps != NULL);
  assert(src       != NULL);
  
  src_sizes = ARRAY_SIZES(src);
  src_steps = ARRAY_STEPS(src);
  
  cont_level = src->dimensions;
  while(cont_level > 0 && dst_steps[cont_level - 1] == src_steps[cont_level - 1])
    --cont_level;
    
  if(cont_level < src->dimensions) {
    end_size = src_steps[cont_level] * src_sizes[cont_level];
  }
  else
    end_size = pmath_packed_element_size(src->element_type);
    
  copy_array(
    dst,
    dst_steps,
    (const uint8_t *)src->blob->data + src->offset,
    src_steps,
    src_sizes,
    cont_level,
    end_size);
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
  
  memcpy(ARRAY_SIZES(_array), sizes, dimensions * sizeof(size_t));
  
  if(steps) {
    if(!check_steps(element_type, dimensions, sizes, steps, &_array->total_size)) {
      _pmath_unref_ptr((void *)_array);
      return PMATH_NULL;
    }
    
    memcpy(ARRAY_STEPS(_array), steps, dimensions * sizeof(size_t));
    
    _array->non_continuous_dimensions_count = count_non_continuous_dimensions(
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
  
  new_array->blob = (void *)PMATH_AS_PTR(pmath_blob_new(new_array->total_size, FALSE));
  if(new_array->blob == NULL) {
    destroy_packed_array(PMATH_FROM_PTR((void *)new_array));
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
    
  assert(pmath_is_packed_array(array));
  
  _array = (void *)PMATH_AS_PTR(array);
  return _array->dimensions;
}

PMATH_API
const size_t *pmath_packed_array_get_sizes(pmath_packed_array_t array) {
  struct _pmath_packed_array_t *_array;
  
  if(PMATH_UNLIKELY(pmath_is_null(array)))
    return NULL;
    
  assert(pmath_is_packed_array(array));
  
  _array = (void *)PMATH_AS_PTR(array);
  return ARRAY_SIZES(_array);
}

PMATH_API
const size_t *pmath_packed_array_get_steps(pmath_packed_array_t array) {
  struct _pmath_packed_array_t *_array;
  
  if(PMATH_UNLIKELY(pmath_is_null(array)))
    return NULL;
    
  assert(pmath_is_packed_array(array));
  
  _array = (void *)PMATH_AS_PTR(array);
  return ARRAY_STEPS(_array);
}

PMATH_API
enum pmath_packed_type_t pmath_packed_array_get_element_type(pmath_packed_array_t array) {
  struct _pmath_packed_array_t *_array;
  
  if(PMATH_UNLIKELY(pmath_is_null(array)))
    return 0;
    
  assert(pmath_is_packed_array(array));
  
  _array = (void *)PMATH_AS_PTR(array);
  return _array->element_type;
}

PMATH_API
const void *pmath_packed_array_read(
  pmath_packed_array_t  array,
  const size_t         *indices,
  size_t                num_indices
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
  
  assert(num_indices <= _array->dimensions);
  
  offset = _array->offset;
  for(k = 0; k < num_indices; ++k) {
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
  const size_t         *indices,
  size_t                num_indices
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
  
  assert(num_indices <= _array->dimensions);
  
  offset = _array->offset;
  for(k = 0; k < num_indices; ++k) {
    size_t i = indices[k];
    
    if(i < 1 || i > sizes[k])
      return NULL;
      
    offset += (i - 1) * steps[k];
  }
  
  if(pmath_refcount(*array) == 1) {
    data = pmath_blob_try_write(PMATH_FROM_PTR(_array->blob));
    
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
        
      _pmath_unref_ptr((void *)_array->blob);
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
        return pmath_ref(_pmath_object_pos_infinity);
        
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
  
  if(index > sizes[0])
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
    
    _pmath_ref_ptr((void *)_array->blob);
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
  
  if(start == 1 && length >= sizes[0])
    return pmath_ref(array);
    
  if(start > sizes[0] || length == 0)
    return pmath_ref(_pmath_object_emptylist);
    
  if(length > sizes[0] || start + length > sizes[0] + 1)
    length = sizes[0] + 1 - start;
    
  if(start < 1) {
    struct _pmath_expr_t *expr = _pmath_expr_new_noinit(length);
    size_t i;
    
    if(expr == NULL)
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
      
    memcpy(new_array->sizes_and_steps, _array->sizes_and_steps, 2 * _array->dimensions * sizeof(size_t));
    ARRAY_SIZES(new_array)[0] = length;
    
    _pmath_ref_ptr((void *)_array->blob);
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
  
  if(PMATH_UNLIKELY(pmath_is_null(array)))
    return array;
    
  _array = (void *)PMATH_AS_PTR(array);
  if(index == 0) {
    if(pmath_same(item, PMATH_SYMBOL_LIST)) {
      pmath_unref(item);
      return array;
    }
    
    pmath_debug_print("[unpack array: new head != List]\n");
    
    return pmath_expr_set_item(
             _pmath_expr_unpack_array(array),
             index,
             item);
  }
  
  sizes = ARRAY_SIZES(_array);
  steps = ARRAY_STEPS(_array);
  
  old_data = (const uint8_t *)_array->blob->data + _array->offset + (index - 1) * steps[0];
  
  if(index > sizes[0]) {
    pmath_unref(item);
    return array;
  }
  
  if(_array->dimensions == 1) {
    switch(_array->element_type) {
    
      case PMATH_PACKED_DOUBLE: {
          double as_double;
          void *new_data;
          
          if(pmath_is_double(item)) {
            as_double = PMATH_AS_DOUBLE(item);
          }
          else if(pmath_same(item, PMATH_SYMBOL_UNDEFINED)) {
            as_double = NAN;
          }
          else if(pmath_equals(item, _pmath_object_pos_infinity)) {
            as_double = HUGE_VAL;
            pmath_unref(item);
            item = PMATH_NULL;
          }
          else if(pmath_equals(item, _pmath_object_neg_infinity)) {
            as_double = -HUGE_VAL;
            pmath_unref(item);
            item = PMATH_NULL;
          }
          else {
            pmath_debug_print_object("[unpack array: DOUBLE expected, but ", item, " given]\n");
            return pmath_expr_set_item(
                     _pmath_expr_unpack_array(array),
                     index,
                     item);
          }
          
          if(memcmp(old_data, &as_double, sizeof(as_double)) == 0)
            return array;
            
          new_data = pmath_packed_array_begin_write(&array, &index, 1);
          if(new_data == NULL) {
            pmath_unref(array);
            return PMATH_NULL;
          }
          
          memcpy(new_data, &as_double, sizeof(as_double));
        }
        return array;
        
      case PMATH_PACKED_INT32: {
          int32_t as_int32;
          void *new_data;
          
          if(pmath_is_int32(item)) {
            as_int32 = PMATH_AS_INT32(item);
          }
          else {
            pmath_debug_print_object("[unpack array: INT32 expected, but ", item, " given]\n");
            return pmath_expr_set_item(
                     _pmath_expr_unpack_array(array),
                     index,
                     item);
          }
          
          if(memcmp(old_data, &as_int32, sizeof(as_int32)) == 0)
            return array;
            
          new_data = pmath_packed_array_begin_write(&array, &index, 1);
          if(new_data == NULL) {
            pmath_unref(array);
            return PMATH_NULL;
          }
          
          memcpy(new_data, &as_int32, sizeof(as_int32));
        }
        return array;
    }
    
    assert(!"bad element type");
  }
  
  if(!pmath_is_packed_array(item) &&
      pmath_is_expr_of_len(item, PMATH_SYMBOL_LIST, sizes[0]))
  {
    item = _pmath_expr_pack_array(item);
  }
  
  if(pmath_is_packed_array(item)) {
    struct _pmath_packed_array_t *item_array;
    const void *item_data;
    void *new_data;
    
    item_array = (void *)PMATH_AS_PTR(item);
    
    if(item_array->element_type != _array->element_type) {
      pmath_debug_print("[unpack array: incompatible packed arrays]\n");
      return pmath_expr_set_item(
               _pmath_expr_unpack_array(array),
               index,
               item);
    }
    
    if(item_array->dimensions != _array->dimensions + 1) {
      pmath_debug_print("[unpack array: incompatible packed array depths]\n");
      return pmath_expr_set_item(
               _pmath_expr_unpack_array(array),
               index,
               item);
    }
    
    if(0 != memcmp(sizes + 1, ARRAY_SIZES(item_array), item_array->dimensions)) {
      pmath_debug_print("[unpack array: incompatible packed array sizes]\n");
      return pmath_expr_set_item(
               _pmath_expr_unpack_array(array),
               index,
               item);
    }
    
    item_data = (const uint8_t *)item_array->blob->data + item_array->offset;
    
    if(0 == memcmp(steps + 1, ARRAY_STEPS(item_array), item_array->dimensions)) {
    
      assert(item_array->total_size <= steps[0]);
      
      if(item_data == old_data) {
        pmath_unref(item);
        return array;
      }
      
      // Same blob, referenced nowhere else? Try inplace copying
      if( item_array->blob == _array->blob &&
          !item_array->blob->is_readonly &&
          1 == _pmath_refcount_ptr((void *)item_array) &&
          2 == _pmath_refcount_ptr((void *)item_array->blob))
      {
        _pmath_unref_ptr((void *)item_array->blob);
        item_array->blob = NULL;
      }
      
      new_data = pmath_packed_array_begin_write(&array, &index, 1);
      if(new_data == NULL) {
        pmath_unref(array);
        pmath_unref(item);
        return PMATH_NULL;
      }
      
      memcpy(new_data, item_data, item_array->total_size);
      pmath_unref(item);
      return array;
    }
    
    pmath_debug_print("[incompatiple steps]\n");
    
    new_data = pmath_packed_array_begin_write(&array, &index, 1);
    if(new_data == NULL) {
      pmath_unref(array);
      pmath_unref(item);
      return PMATH_NULL;
    }
    
    packed_array_copy(new_data, steps + 1, item_array);
    
    pmath_unref(item);
    return array;
  }
  
  pmath_debug_print_object("[unpack array: List expected, but ", item, " given]\n");
  
  return pmath_expr_set_item(
           _pmath_expr_unpack_array(array),
           index,
           item);
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
  if(PMATH_UNLIKELY(expr == NULL)) {
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
  if(pmath_is_packed_array(expr)) {
    struct _pmath_packed_array_t *_array;
    
    _array = (void *)PMATH_AS_PTR(expr);
    
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
      
    for(--len; len > 0; --len) {
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

static size_t packable_dimensions(pmath_t expr) {
  if(pmath_is_packed_array(expr)) {
    struct _pmath_packed_array_t *_array;
    
    _array = (void *)PMATH_AS_PTR(expr);
    
    return _array->dimensions;
  }
  
  if(pmath_is_expr(expr)) {
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
  void                         **location
) {
  size_t ncdc = array->non_continuous_dimensions_count;
  size_t i;
  size_t *sizes = ARRAY_SIZES(array);
  size_t *steps = ARRAY_STEPS(array);
  
  if(level == ncdc) {
    size_t size = sizes[ncdc] * steps[ncdc];
    
    memcpy(*location, array_data, size);
    *location = (uint8_t *)location + size;
    return;
  }
  
  assert(level < ncdc);
  assert(ncdc < array->dimensions);
  
  for(i = 0; i < sizes[level]; ++i) {
    pack_array(array, level + 1, array_data, location);
    array_data = (uint8_t *)array_data + steps[level];
  }
}

static void pack_and_free_int32(pmath_t expr, int32_t **location) {
  if(pmath_is_packed_array(expr)) {
    struct _pmath_packed_array_t *_array;
    
    _array = (void *)PMATH_AS_PTR(expr);
    
    assert(_array->element_type == PMATH_PACKED_INT32);
    
    pack_array(_array, 0, _array->blob->data, (void **)location);
    _pmath_unref_ptr((void *)_array);
    return;
  }
  
  if(pmath_is_expr(expr)) {
    size_t len = pmath_expr_length(expr);
    size_t i;
    
    for(i = 1; i <= len; ++i) {
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

static void pack_and_free_double(pmath_t expr, double **location) {
  if(pmath_is_packed_array(expr)) {
    struct _pmath_packed_array_t *_array;
    
    _array = (void *)PMATH_AS_PTR(expr);
    
    assert(_array->element_type == PMATH_PACKED_DOUBLE);
    
    pack_array(_array, 0, _array->blob->data, (void **)location);
    _pmath_unref_ptr((void *)_array);
    return;
  }
  
  if(pmath_is_expr(expr)) {
    size_t len = pmath_expr_length(expr);
    size_t i;
    
    for(i = 1; i <= len; ++i) {
      pmath_t item = pmath_expr_get_item(expr, i);
      pack_and_free_double(item, location);
    }
    
    pmath_unref(expr);
    return;
  }
  
  if(pmath_is_double(expr)) {
    **location = PMATH_AS_DOUBLE(expr);
    ++*location;
    return;
  }
  
  if(pmath_same(expr, PMATH_SYMBOL_UNDEFINED)) {
    **location = NAN;
    ++*location;
    pmath_unref(expr);
    return;
  }
  
  if(pmath_equals(expr, _pmath_object_pos_infinity)) {
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
  int                   elem_type;
  pmath_t               dims_expr;
  size_t                dims;
  size_t                i;
  size_t                elem_size;
  size_t                total_size;
  size_t               *sizes;
  pmath_blob_t          blob;
  struct _pmath_blob_t *_blob;
  void                 *data;
  pmath_packed_array_t  array;
  
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
  if(pmath_expr_length(dims_expr) != dims) {
    pmath_unref(dims_expr);
    return expr;
  }
  
  elem_size  = pmath_packed_element_size(elem_type);
  total_size = elem_size;
  
  sizes = pmath_mem_alloc(dims * sizeof(size_t));
  if(!sizes) {
    pmath_unref(dims_expr);
    return expr;
  }
  
  for(i = 0; i < dims; ++i) {
    pmath_t n_expr = pmath_expr_get_item(dims_expr, i + 1);
    
    sizes[i] = pmath_integer_get_uiptr(n_expr);
    pmath_unref(n_expr);
    
    if(total_size > SIZE_MAX / sizes[i]) {
      pmath_mem_free(sizes);
      pmath_unref(dims_expr);
      return expr;
    }
    
    total_size *= sizes[i];
  }
  
  pmath_unref(dims_expr);
  
  blob = pmath_blob_new(total_size, FALSE);
  if(pmath_is_null(blob)) {
    pmath_mem_free(sizes);
    return expr;
  }
  
  _blob = (void *)PMATH_AS_PTR(blob);
  data = _blob->data;
  switch(elem_type) {
    case PMATH_PACKED_DOUBLE:
      pack_and_free_double(expr, (double **)&data);
      break;
      
    case PMATH_PACKED_INT32:
      pack_and_free_int32(expr, (int32_t **)&data);
      break;
  }
  
  assert(data == (uint8_t *)_blob->data + _blob->data_size);
  
  array = pmath_packed_array_new(blob, elem_type, dims, sizes, NULL, 0);
  
  pmath_mem_free(sizes);
  return array;
}

/* -------------------------------------------------------------------------- */

static int cmp_double(const void *key, const void *elem) {
  pmath_t bad_key;
  pmath_t bad_elem;
  int cmp;
  
  double d_key  = *(double *)key;
  double d_elem = *(double *)elem;
  
  if(isfinite(d_key) && isfinite(d_elem)) {
    if(d_key < d_elem)
      return -1;
      
    if(d_key > d_elem)
      return -1;
      
    return 0;
  }
  
  bad_key  = _pmath_packed_element_unbox(key, PMATH_PACKED_DOUBLE);
  bad_elem = _pmath_packed_element_unbox(elem, PMATH_PACKED_DOUBLE);
  
  cmp = pmath_compare(bad_key, bad_elem);
  
  pmath_unref(bad_key);
  pmath_unref(bad_elem);
  
  return cmp;
}

static int cmp_int32(const void *key, const void *elem) {

  int32_t i_key  = *(int32_t *)key;
  int32_t i_elem = *(int32_t *)elem;
  
  if(i_key < i_elem)
    return -1;
    
  if(i_key > i_elem)
    return 1;
    
  return 0;
}

PMATH_PRIVATE
size_t _pmath_packed_array_find_sorted(
  pmath_packed_array_t sorted_array, // wont be freed
  pmath_t              item          // wont be freed
) {
  struct _pmath_packed_array_t *_array;
  const size_t *sizes;
  const size_t *steps;
  size_t i;
  
  assert(pmath_is_packed_array(sorted_array));
  
  _array = (void *)PMATH_AS_PTR(sorted_array);
  
  sizes = ARRAY_SIZES(_array);
  steps = ARRAY_STEPS(_array);
  
  if(_array->dimensions == 1) {
    const void *data = (const uint8_t *)_array->blob->data + _array->offset;
    
    switch(_array->element_type) {
    
      case PMATH_PACKED_DOUBLE:
        if(pmath_is_double(item)) {
          const void *found_elem;
          
          double key = PMATH_AS_DOUBLE(item);
          
          found_elem = bsearch(
                         &key,
                         data,
                         sizes[0],
                         steps[0],
                         cmp_double);
                         
          if(found_elem)
            return 1 + ((size_t)found_elem - (size_t)data) / steps[0];
            
          return 0;
        }
        break;
        
      case PMATH_PACKED_INT32:
        if(pmath_is_int32(item)) {
          const void *found_elem;
          
          int32_t key = PMATH_AS_INT32(item);
          
          found_elem = bsearch(
                         &key,
                         data,
                         sizes[0],
                         steps[0],
                         cmp_int32);
                         
          if(found_elem)
            return 1 + ((size_t)found_elem - (size_t)data) / steps[0];
        }
        return 0;
    }
  }
  else {
    if(!pmath_is_expr_of_len(item, PMATH_SYMBOL_LIST, sizes[0]))
      return 0;
  }
  
  // linear search
  for(i = 1; i <= sizes[0]; ++i) {
    pmath_t array_item = _pmath_packed_array_get_item(sorted_array, i);
    
    int cmp = pmath_compare(item, array_item);
    pmath_unref(array_item);
    
    if(cmp == 0)
      return i;
      
    if(cmp > 0)
      return 0;
  }
  
  return 0;
}

/* -------------------------------------------------------------------------- */

PMATH_PRIVATE
pmath_packed_array_t _pmath_packed_array_resize(
  pmath_packed_array_t array,
  size_t               new_length
) {
  pmath_packed_array_t new_array;
  size_t *new_sizes;
  
  struct _pmath_packed_array_t *_array;
  
  const size_t *sizes;
  const size_t *steps;
  
  assert(pmath_is_packed_array(array));
  
  _array = (void *)PMATH_AS_PTR(array);
  
  sizes = ARRAY_SIZES(_array);
  steps = ARRAY_STEPS(_array);
  
  if(sizes[0] == new_length)
    return array;
    
  if(new_length == 0) {
    pmath_unref(array);
    return pmath_ref(_pmath_object_emptylist);
  }
  
  new_sizes = pmath_mem_alloc(_array->dimensions * sizeof(size_t));
  if(new_sizes == NULL) {
    pmath_mem_free(new_sizes);
    return PMATH_NULL;
  }
  
  memcpy(new_sizes, sizes, _array->dimensions * sizeof(size_t));
  new_sizes[0] = new_length;
  
  if( //sizes[0] > new_length ||
    _array->blob->data_size - _array->offset >= new_sizes[0] * steps[0])
  {
    _pmath_ref_ptr((void *)_array->blob);
    new_array = pmath_packed_array_new(
                  PMATH_FROM_PTR(_array->blob),
                  _array->element_type,
                  _array->dimensions,
                  new_sizes,
                  steps,
                  _array->offset);
                  
    pmath_mem_free(new_sizes);
    pmath_unref(array);
    return new_array;
  }
  
  new_array = pmath_packed_array_new(
                PMATH_NULL,
                _array->element_type,
                _array->dimensions,
                new_sizes,
                NULL,
                0);
                
  if(!pmath_is_null(new_array)) {
    struct _pmath_packed_array_t *_new_array = (void *)PMATH_AS_PTR(new_array);
    
    packed_array_copy(
      _new_array->blob->data,
      ARRAY_STEPS(_new_array),
      _array);
  }
  
  pmath_mem_free(new_sizes);
  pmath_unref(array);
  return new_array;
}

/* -------------------------------------------------------------------------- */

PMATH_PRIVATE pmath_expr_t _pmath_packed_array_sort(pmath_packed_array_t array) {
  struct _pmath_packed_array_t *_array;
  
  const size_t *sizes;
  const size_t *steps;
  
  assert(pmath_is_packed_array(array));
  
  _array = (void *)PMATH_AS_PTR(array);
  
  if(_array->dimensions == 1) {
    void *data = pmath_packed_array_begin_write(&array, NULL, 0);
    
    int (*cmp)(const void *, const void *) = NULL;
    
    if(data == NULL)
      return PMATH_NULL;
      
    _array = (void *)PMATH_AS_PTR(array);
    
    sizes = ARRAY_SIZES(_array);
    steps = ARRAY_STEPS(_array);
    
    switch(_array->element_type) {
      case PMATH_PACKED_DOUBLE:
        cmp = cmp_double;
        break;
        
      case PMATH_PACKED_INT32:
        cmp = cmp_int32;
        break;
        
      default:
        assert(!"reached");
        pmath_unref(array);
        return PMATH_NULL;
    }
    
    qsort(data, sizes[0], steps[0], cmp);
    return array;
  }
  
  return pmath_expr_sort(_pmath_expr_unpack_array(array));
}

/* -------------------------------------------------------------------------- */

PMATH_PRIVATE
pmath_expr_t _pmath_packed_array_map(
  pmath_packed_array_t  array, // will be freed
  size_t                start,
  size_t                end,
  pmath_t             (*func)(pmath_t, size_t, void *),
  void                 *context
) {
  struct _pmath_packed_array_t *_array;
  
  const uint8_t *old_data;
  size_t elem_size;
  
  assert(pmath_is_packed_array(array));
  
  _array = (void *)PMATH_AS_PTR(array);
  
  if(end > *ARRAY_SIZES(_array))
    end  = *ARRAY_SIZES(_array);
    
  if(start == 0) {
    pmath_t item;
    
    start = 1;
    
    item = (*func)(pmath_ref(PMATH_SYMBOL_LIST), 0, context);
    if(!pmath_same(item, PMATH_SYMBOL_LIST)) {
      pmath_t expr = pmath_expr_set_item(array, 0, item);
      
      return _pmath_expr_map(expr, start, end, func, context);
    }
    
    pmath_unref(item);
  }
  
  elem_size = ARRAY_STEPS(_array)[_array->dimensions - 1];
  old_data = pmath_packed_array_read(array, &start, 1);
  
  for(; start <= end; ++start, old_data += elem_size) {
    pmath_t old_item;
    pmath_t new_item;
    pmath_t expr;
    
    old_item = _pmath_packed_element_unbox(old_data, _array->element_type);
    new_item = func(old_item, start, context); // frees old_item
    
    if(pmath_same(new_item, old_item)) {
      pmath_unref(new_item);
      continue;
    }
    
    expr = pmath_expr_set_item(array, start, new_item);
    
    if(pmath_is_packed_array(expr)) {
      array = expr;
      _array = (void *)PMATH_AS_PTR(array);
      old_data = pmath_packed_array_read(array, &start, 1);
      continue;
    }
    
    return _pmath_expr_map(
             expr,
             start,
             end,
             func,
             context);
  }
  
  return array;
}

/* -------------------------------------------------------------------------- */

static void *move_repacked_array_internal(
  void          *dst_data,
  const void    *src_data,
  const size_t  *src_steps,
  const size_t  *src_sizes,
  size_t         depth,
  size_t         end_size
) {
  size_t i;
  
  if(depth == 0) {
    if(dst_data != src_data)
      memmove(dst_data, src_data, end_size);
      
    return (uint8_t *)dst_data + end_size;
  }
  
  for(i = 0; i < *src_sizes; ++i) {
    dst_data = move_repacked_array_internal(
                 dst_data,
                 src_data,
                 src_steps + 1,
                 src_sizes + 1,
                 depth - 1,
                 end_size);
                 
    src_data = (int8_t *)src_data + *src_steps;
  }
  
  return dst_data;
}

static void *move_repacked_array(
  void                               *dst,
  const struct _pmath_packed_array_t *src
) {
  const size_t *src_sizes;
  const size_t *src_steps;
  size_t end_size;
  
  assert(dst != NULL);
  assert(src != NULL);
  
  src_sizes = ARRAY_SIZES(src);
  src_steps = ARRAY_STEPS(src);
  
  end_size = src_steps[src->non_continuous_dimensions_count] * src_sizes[src->non_continuous_dimensions_count];
  
  dst = move_repacked_array_internal(
          dst,
          (const uint8_t *)src->blob->data + src->offset,
          src_steps,
          src_sizes,
          src->non_continuous_dimensions_count,
          end_size);
          
  return dst;
}


PMATH_API
pmath_packed_array_t pmath_packed_array_reshape(
  pmath_packed_array_t  array,
  size_t                new_dimensions,
  const size_t         *new_sizes
) {
  const struct _pmath_packed_array_t *_array;
  struct _pmath_packed_array_t *new_array;
  size_t header_size;
  size_t old_total_count;
  size_t new_total_count;
  size_t i;
  const size_t *old_sizes;
  
  _array = (void *)PMATH_AS_PTR(array);
  
  if(!_array)
    return PMATH_NULL;
    
  assert(pmath_is_packed_array(array));
  
  if(!check_sizes(new_dimensions, new_sizes)) {
    _pmath_unref_ptr((void *)_array);
    return PMATH_NULL;
  }
  
  old_sizes = ARRAY_SIZES(_array);
  
  old_total_count = 1;
  for(i = 0; i < _array->dimensions; ++i)
    old_total_count *= old_sizes[i];
    
  new_total_count = 1;
  for(i = 0; i < new_dimensions; ++i)
    new_total_count *= new_sizes[i];
    
  if(old_total_count != new_total_count) {
    _pmath_unref_ptr((void *)_array);
    return PMATH_NULL;
  }
  
  header_size = sizeof(struct _pmath_packed_array_t) - sizeof(size_t) + 2 * new_dimensions * sizeof(size_t);
  
  new_array = (void *)PMATH_AS_PTR(_pmath_create_stub(
                                     PMATH_TYPE_SHIFT_PACKED_ARRAY,
                                     header_size));
                                     
  if(PMATH_UNLIKELY(!new_array)) {
    _pmath_unref_ptr((void *)_array);
    return PMATH_NULL;
  }
  
  new_array->blob   = NULL;
  new_array->offset = 0;
  //new_array->blob         = (void *)PMATH_AS_PTR(blob);
  //new_array->offset       = offset;
  new_array->element_type = _array->element_type;
  new_array->dimensions   = new_dimensions;
  new_array->cached_hash  = 0;
  
  memcpy(ARRAY_SIZES(new_array), new_sizes, new_dimensions);
  
  if(!init_default_steps(new_array->element_type, new_dimensions, new_sizes, ARRAY_STEPS(new_array), &new_array->total_size)) {
    _pmath_unref_ptr((void *)new_array);
    _pmath_unref_ptr((void *)_array);
    return PMATH_NULL;
  }
  
  new_array->non_continuous_dimensions_count = 0;
  
  if(_array->non_continuous_dimensions_count == 0) {
    _pmath_ref_ptr((void *)_array->blob);
    new_array->blob = _array->blob;
    new_array->offset = _array->offset;
    
    _pmath_unref_ptr((void *)_array);
    return PMATH_FROM_PTR(new_array);
  }
  
  if( _pmath_refcount_ptr((void *)_array) == 1 &&
      _pmath_refcount_ptr((void *)_array->blob) == 1 &&
      !_array->blob->is_readonly)
  {
    _pmath_ref_ptr((void *)_array->blob);
    new_array->blob = _array->blob;
    new_array->offset = _array->offset;
  }
  else {
    new_array->blob = (void *)PMATH_AS_PTR(pmath_blob_new(new_array->total_size, FALSE));
    new_array->offset = 0;
    
    if(new_array->blob == NULL) {
      _pmath_unref_ptr((void *)new_array);
      _pmath_unref_ptr((void *)_array);
      return PMATH_NULL;
    }
  }
  
  move_repacked_array(
    (uint8_t *)new_array->blob->data + new_array->offset,
    _array);
    
  _pmath_unref_ptr((void *)_array);
  return PMATH_FROM_PTR(new_array);
}

PMATH_PRIVATE
pmath_packed_array_t _pmath_packed_array_flatten(
  pmath_packed_array_t  array,
  size_t                depth
) {
  struct _pmath_packed_array_t *_array;
  size_t short_new_sizes[4];
  size_t *new_sizes;
  size_t new_dims;
  const size_t *sizes;
  size_t i;
  
  assert(pmath_is_packed_array(array));
  
  if(depth == 0)
    return array;
    
  _array = (void *)PMATH_AS_PTR(array);
  sizes = ARRAY_SIZES(_array);
  
  if(_array->dimensions <= depth)  {
    new_dims = 1;
  }
  else {
    new_dims = _array->dimensions - depth + 1;
  }
  
  if(new_dims < sizeof(short_new_sizes) / sizeof(short_new_sizes[0])) {
    new_sizes = short_new_sizes;
  }
  else {
    new_sizes = pmath_mem_alloc(sizeof(new_sizes[0]) * new_dims);
    if(!new_sizes) {
      pmath_unref(array);
      return PMATH_NULL;
    }
  }
  
  new_sizes[0] = 1;
  for(i = 0; i < depth; ++i)
    new_sizes[0] *= sizes[i];
    
  for(i = depth; i < _array->dimensions; ++i)
    new_sizes[i - depth + 1] = sizes[i];
    
  array = pmath_packed_array_reshape(
            array,
            new_dims,
            new_sizes);
            
  if(new_sizes != short_new_sizes)
    pmath_mem_free(new_sizes);
    
  return array;
}

PMATH_PRIVATE
size_t _pmath_packed_array_bytecount(pmath_packed_array_t array) {
  struct _pmath_packed_array_t *_array;
  
  assert(pmath_is_packed_array(array));
  
  _array = (void *)PMATH_AS_PTR(array);
  return sizeof(struct _pmath_packed_array_t) - sizeof(size_t) + sizeof(size_t) * 2 * _array->dimensions
    + _array->total_size 
    + sizeof(struct _pmath_blob_t);
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
    
  _pmath_init_special_type(
    PMATH_TYPE_SHIFT_PACKED_ARRAY,
    _pmath_compare_exprsym,
    hash_packed_array,
    destroy_packed_array,
    _pmath_expr_equal,
    _pmath_expr_write);
    
  return TRUE;
}

PMATH_PRIVATE void _pmath_packed_arrays_done(void) {

}
