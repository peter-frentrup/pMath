#ifndef __PMATH_CORE__PACKED_ARRAYS_H__
#define __PMATH_CORE__PACKED_ARRAYS_H__

#include <pmath-core/expressions.h>

/**\defgroup packed_arrays Packed Arrays
   \brief Compact arrays of numerical type.
   
   From the outside (i.e. the pMath language side), a packed array is a 
   retangular list of lists of ... of reals/integers/...
   But packed arrays provide a compact representation to save space and the 
   knowledge of their content (no arbitrary expressions/symbols/strings) enables
   several time-saving optimizations. This type also simplifies access to 
   external libraries such as Euler (linear algebra) or OpenCV 
   (image processing).
   
   
 */

/**\brief Internal representation of a flat array of bytes.
   
   These objects are non-evaluatable (like pmath_custom_t). 
 */
typedef pmath_t pmath_blob_t;

/**\brief A packed array.
   
   This can be used anywhere, where a pmath_expr_t is needed. But their internal 
   representations differ radically.
 */
typedef pmath_expr_t pmath_packed_array_t;

/**\brief Create a new blob of bytes. pMath allocates the bytes.
   \memberof pmath_blob_t
   \param size      The size in bytes of the to be allocated blob.
   \param zeroinit  Whether to initialize the data with 0 bytes.
   \return A new pmath_blob_t.
 */
PMATH_API
PMATH_ATTRIBUTE_USE_RESULT
pmath_blob_t pmath_blob_new(size_t size, pmath_bool_t zeroinit);

/**\brief Create a blob-view to existing bytes.
   \memberof pmath_blob_t
   \param size              The size in bytes of the data.
   \param data              An array of at least \a size bytes. 
   \param data_destructor   An optional function that will be called when the
                            new blob's reference count drops to zero.
   \param is_readonly       Whether the data only readonly (const).
   \return A new pmath_blob_t.
   
   The data must be accessible during the whole lifetime of the blob object. 
   Use \a data_destructor to do cleanup if needed.
 */
PMATH_API
PMATH_ATTRIBUTE_USE_RESULT
pmath_blob_t pmath_blob_new_with_data(
  size_t            size, 
  void             *data, 
  pmath_callback_t  data_destructor,
  pmath_bool_t      is_readonly);

/**\brief Make an existing blob writeable if needed.
   \memberof pmath_blob_t
   \param blob  A blob object. It will be freed.
   \return A writeable blob containing equal data. This will be \a blob itself
           if it has reference count 1 and pmath_blob_is_always_readonly(blob) 
           gives FALSE. Otherwise, a new blob with a copy of the data will be 
           created.
 */
PMATH_API
PMATH_ATTRIBUTE_USE_RESULT
pmath_blob_t pmath_blob_make_writeable(pmath_blob_t blob);

/**\brief Test whether a blob is only readonly. 
   \memberof pmath_blob_t
   \param blob  A blob object. It wont be freed.
   \return FALSE if writing to a blob is allowed, TRUE otherwise.
 */
PMATH_API
pmath_bool_t pmath_blob_is_always_readonly(pmath_blob_t blob);

/**\brief Request the size in bytes of a blob.
   \memberof pmath_blob_t
   \param blob  A blob object. It wont be freed.
   \return The size of the blob or 0 if it is PMATH_NULL.
 */
PMATH_API
size_t pmath_blob_get_size(pmath_blob_t blob);

/**\brief Access the internal data of the blob for reading.
   \memberof pmath_blob_t
   \param blob  A blob object. It wont be freed.
   \return A read-only pointer to the internal data. This is the \a data 
   parameter of the pmath_blob_new_with_data() thatcreated the blob.
 */
PMATH_API
const void *pmath_blob_read(pmath_blob_t blob);

/**\brief Try to access the internal data of the blob for reading and writing.
   \memberof pmath_blob_t
   \param blob  A blob object. It wont be freed.
   \return A pointer to the internal data or NULL in any of the two cases:
           - The reference count is greater than one.
           - The blob is always readonly (see pmath_blob_is_always_readonly()).
 */
PMATH_API
void *pmath_blob_try_write(pmath_blob_t blob);


/**\brief The element type of the data a packed array holds. 
   
   More types may be added in the future.
 */
enum pmath_packed_type_t {
  PMATH_PACKED_DOUBLE = 1, //< A <tt>double</tt>
  PMATH_PACKED_INT32  = 2  //< An <tt>int32_t</tt>
};

/**\brief Get the size in bytes of an element type.
 */
PMATH_API
size_t pmath_packed_element_size(enum pmath_packed_type_t element_type);

/**\brief Create a packed array.
   \memberof pmath_packed_array_t
   \param blob          The actual data. It will be freed.
   \param element_type  The element type.
   \param dimensions    The number of dimensions.
   \param sizes         The number of elements in each dimension. An array of 
                        \a dimensions sizes. 
   \param steps         (optional) How to compute the address of an element.
   \param offset        An optional offset into the blob.
   \return A new packed array or PMATH_NULL on error.
   
   The data must be properly aligned.
   Let M be the new array. The address of M[i1,i2,...,iN] for N = dimensions
   and 1 <= ik <= sizes[k-1], k=1...,N
   is computed as
   <tt>
    address(M[i1,...,iN]) = blob.data + offset + steps[0] * (i1 - 1)
                                               + steps[1] * (i2 - 1)
                                               + ... 
                                               + steps[N - 1] * (iN - 1).
   </tt>
   
   Additionally, steps[i] >= steps[i+1] * sizes[i+1] for i < N-1 and 
   steps[N-1] must exactly equal the element size. (as in e.g. OpenCV).
   
   When you specify NULL for steps, pMath will use steps[N-1] = element size
   and steps[i-1] = steps[i] * sizes[i] for i < N.
 */
PMATH_API
PMATH_ATTRIBUTE_USE_RESULT
pmath_packed_array_t pmath_packed_array_new(
  pmath_blob_t               blob,
  enum pmath_packed_type_t   element_type,
  size_t                     dimensions,
  const size_t              *sizes,
  const size_t              *steps,
  size_t                     offset);

/**\brief Get the number of dimensions of a packed array.
   \memberof pmath_packed_array_t
   \param array  A packed array. It wont be freed.
   \return The number of dimensions or 0 if the array was PMATH_NULL.
 */
PMATH_API
size_t pmath_packed_array_get_dimensions(pmath_packed_array_t array);

/**\brief Get the sizes of a packed array.
   \memberof pmath_packed_array_t
   \param array  A packed array. It wont be freed.
   \return The array of sizes. Its length it the array dimension.
 */
PMATH_API
const size_t *pmath_packed_array_get_sizes(pmath_packed_array_t array);

/**\brief Get the element data type of a packed array.
   \memberof pmath_packed_array_t
   \param array  A packed array. It wont be freed.
   \return The array element data type.
 */
PMATH_API
enum pmath_packed_type_t pmath_packed_array_get_element_type(pmath_packed_array_t array);

/**\brief Get the number of dimensions that contain holes.
   \memberof pmath_packed_array_t
   \param array  A packed array. It wont be freed.
   \return A number between 0 and pmath_packed_array_get_dimensions(array)-1 
           (inclusive).
   
   Let <tt>K</tt> be the returned number and <tt>N</tt> the number of 
   dimensions. Then memberwise operations can be speed up. Instead of:
   \code
for(i1 = 1;i1 <= sizes[0];++i1)
  for(i2 = 1;i2 <= sizes[1];++i2)
    ...
    for(iK = 1;iK <= sizes[K-1];++iK)
      ...
      for(iN = 1;iN <= sizes[N-1];++iN)
        some_operation(array[i1, i2, ..., iK, ... iN])
    \endcode
    you can use a single for loop and C pointer arithmetic for all dimensions
    above K:
    \code
for(i1 = 1;i1 <= sizes[0];++i1)
  for(i2 = 1;i2 <= sizes[1];++i2)
    ...
    for(iK = 1;iK <= sizes[K-1];++iK) {
      T *ptr = address(array, [i1, i2, ..., iK], K)
      for(j = 0;j < sizes[K] * steps[K];++j)
        some_operation(ptr[j]);
    }
    \endcode
    where <tt>T</tt> is the element type (double/int32_t/...) and 
    <tt>address</tt> is either <tt>pmath_packed_array_read</tt> or
    <tt>pmath_packed_array_begin_write</tt>.
 */
PMATH_API
size_t pmath_packed_array_get_non_continuous_dimensions(pmath_packed_array_t array);

/**\brief Get the data address of an array element.
   \memberof pmath_packed_array_t
   \param array       A packed array. It wont be freed.
   \param indices     An array of 1-based indices. Its length is the array 
                      dimension. Every entry must satisfy 
                      1 <= indices[k] <= sizes[k]
   \param num_indices The number of indices given. Tis must not exceeed the 
                      array's dimension. A value of 1 is assumed for the 
                      remaining indices.
   \return The (readonly) address of the element.
 */
PMATH_API
const void *pmath_packed_array_read(
  pmath_packed_array_t  array,
  const size_t         *indices,
  size_t                num_indices);

/**\brief Get the writeable data address of an array element.
   \memberof pmath_packed_array_t
   \param array       Pointer to a packed array. 
   \param indices     An array of 1-based indices. Its length is the array 
                      dimension. Every entry must satisfy 
                      1 <= indices[k] <= sizes[k]
   \param num_indices The number of indices given. Tis must not exceeed the 
                      array's dimension. A value of 1 is assumed for the 
                      remaining indices.
   \return The (writeable) address of the element or NULL on error.
   
   This funcition duplicates *array if needed (if the underlying blob is not 
   writeable or *array has refcount > 1). Then it clears all caches in the new
   *array (i.e. cached hash value) and finally returns the requested location.
 */  
PMATH_API
void *pmath_packed_array_begin_write(
  pmath_packed_array_t *array,
  const size_t         *indices,
  size_t                num_indices);

/**\brief change the shape of an array.
   \memberof pmath_packed_array_t
   \param array           A packed array. It will be freed.
   \param new_dimensions  The new number of dimensions.
   \param new_sizes       An array of new lengths for each of the \a new_dimensions
                          dimensions. 
   \return The reshaped array of PMATH_NULL on error.
   
   The product of all new sizes must equal the product of all old sizes of the 
   array.
 */
PMATH_API
pmath_packed_array_t pmath_packed_array_reshape(
  pmath_packed_array_t  array,
  size_t                new_dimensions,
  const size_t         *new_sizes);

#endif // __PMATH_CORE__PACKED_ARRAYS_H__
