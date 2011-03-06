#include <pmath-core/custom-private.h>
#include <pmath-core/custom.h>
#include <pmath-core/objects-private.h>

#include <pmath-util/hashtables-private.h>
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
  
  custom = (struct custom_t*)PMATH_AS_PTR(_pmath_create_stub(
      PMATH_TYPE_SHIFT_CUSTOM, 
      sizeof(struct custom_t)));
  
  if(PMATH_UNLIKELY(!custom)){
    destructor(data);
    return PMATH_NULL;
  }
  
  custom->data = data;
  custom->destructor = destructor;
  return PMATH_FROM_PTR(custom);
}

PMATH_API void *pmath_custom_get_data(pmath_custom_t custom){
  if(PMATH_UNLIKELY(pmath_is_null(custom)))
    return NULL;
  
  assert(pmath_is_custom(custom));
  
  return ((struct custom_t*)PMATH_AS_PTR(custom))->data;
}

PMATH_API pmath_bool_t pmath_custom_has_destructor(
  pmath_custom_t    custom,
  pmath_callback_t  dtor
){
  if(PMATH_UNLIKELY(pmath_is_null(custom)))
    return FALSE;
  
  assert(pmath_is_custom(custom));
  
  return ((struct custom_t*)PMATH_AS_PTR(custom))->destructor == dtor;
}

//{ pMath object functions ...

static int compare_custom(pmath_t a, pmath_t b){
  return (uintptr_t)PMATH_AS_PTR(a) < (uintptr_t)PMATH_AS_PTR(b) ? -1 : 1;
}

static unsigned int hash_custom(pmath_t a){
  return _pmath_hash_pointer(PMATH_AS_PTR(a));
}

static void destroy_custom(pmath_t a){
  struct custom_t *custom = (struct custom_t*)PMATH_AS_PTR(a);
  
  custom->destructor(custom->data);
  pmath_mem_free(custom);
}

//} ... pMath object functions

//{ module handling functions ...

PMATH_PRIVATE pmath_bool_t _pmath_custom_objects_init(void){
  _pmath_init_special_type(
    PMATH_TYPE_SHIFT_CUSTOM,
    compare_custom,
    hash_custom,
    destroy_custom,
    NULL,
    NULL);
    
  return TRUE;
}

PMATH_PRIVATE void _pmath_custom_objects_done(void){
}

//} ... module handling functions
