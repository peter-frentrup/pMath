#include <pmath-core/custom-private.h>
#include <pmath-core/custom.h>
#include <pmath-core/objects-private.h>
#include <pmath-util/memory.h>

struct custom_t{
  struct _pmath_t    inherited;
  void              *data;
  pmath_callback_t   destructor;
};

PMATH_API pmath_custom_t pmath_custom_new(
  void              *data,
  pmath_callback_t   destructor
){
  struct custom_t *custom;
  
  if(PMATH_UNLIKELY(!destructor))
    return PMATH_NULL;
  
  custom = (struct custom_t*)
    _pmath_create_stub(
      PMATH_TYPE_SHIFT_CUSTOM, 
      sizeof(struct custom_t));
  
  if(PMATH_UNLIKELY(!custom)){
    destructor(data);
    return PMATH_NULL;
  }
  
  custom->data = data;
  custom->destructor = destructor;
  return (pmath_custom_t)custom;
}

PMATH_API void *pmath_custom_get_data(pmath_custom_t custom){
  if(PMATH_UNLIKELY(!custom))
    return PMATH_NULL;
  
  assert(pmath_is_custom(custom));
  
  return ((struct custom_t*)custom)->data;
}

PMATH_API pmath_bool_t pmath_custom_has_destructor(
  pmath_custom_t    custom,
  pmath_callback_t  dtor
){
  if(PMATH_UNLIKELY(!custom))
    return FALSE;
  
  assert(pmath_is_custom(custom));
  
  return ((struct custom_t*)custom)->destructor == dtor;
}

//{ pMath object functions ...

static int compare_custom(
  struct custom_t *customA,
  struct custom_t *customB
){
  return (uintptr_t)customA < (uintptr_t)customB ? -1 : 1;
}

static unsigned int hash_custom(
  struct custom_t *custom
){
  return 0;
}

static void destroy_custom(
  struct custom_t *custom
){
  custom->destructor(custom->data);
  pmath_mem_free(custom);
}

//} ... pMath object functions

//{ module handling functions ...

PMATH_PRIVATE pmath_bool_t _pmath_custom_objects_init(void){
  _pmath_init_special_type(
    PMATH_TYPE_SHIFT_CUSTOM,
    (pmath_compare_func_t)        compare_custom,
    (pmath_hash_func_t)           hash_custom,
    (pmath_proc_t)                destroy_custom,
    (pmath_equal_func_t)          PMATH_NULL,
    (_pmath_object_write_func_t)  PMATH_NULL);
    
  return TRUE;
}

PMATH_PRIVATE void _pmath_custom_objects_done(void){
}

//} ... module handling functions
