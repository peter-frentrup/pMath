#include <util/base.h>

#include <cmath>
#include <cstdio>
#include <inttypes.h>

#include <new>         // placement new
#include <type_traits> // aligned_storage

#include <pmath-util/concurrency/atomic.h>


using namespace richmath;

void richmath::assert_failed() {
  fprintf(stderr, "\n\n\tASSERTION FAILED\n\n");
}

const double richmath::Infinity = HUGE_VAL;

#ifdef RICHMATH_DEBUG_MEMORY
namespace richmath {
  class BaseDebugImpl {
    public:
      BaseDebugImpl() {
        pmath_atomic_write_release(&count, 0);
        pmath_atomic_write_release(&lock, 0);
        pmath_atomic_write_release(&timer, 0);
        non_freed_objects_list = nullptr;
      }
      ~BaseDebugImpl() {
        if(pmath_atomic_read_aquire(&count) != 0) {
          printf("%" PRIdPTR " OBJECTS NOT FREED\n", pmath_atomic_read_aquire(&count));
          
          Base *obj = non_freed_objects_list;
          for(int max_count = 10; obj && max_count > 0; --max_count, obj = obj->debug_next) {
            printf("  AT %p (time %" PRIdPTR "): %s\n", obj, obj->debug_alloc_time, obj->debug_tag);
          }
          if(obj)
            printf("  ...and more\n");
        }
      }
      
      pmath_atomic_t count;
      pmath_atomic_t lock;
      pmath_atomic_t timer;
      Base *non_freed_objects_list;
  };
}

namespace {
  class Locked {
    public:
      Locked(pmath_atomic_t *atom_ptr)
        : _atom_ptr(atom_ptr)
      {
        pmath_atomic_lock(_atom_ptr);
      }
      ~Locked() {
        pmath_atomic_unlock(_atom_ptr);
      }
      
      Locked() = delete;
      Locked(const Locked &src) = delete;
      Locked& operator=(const Locked&) = delete;
    private:
      pmath_atomic_t *_atom_ptr;
  };
  
  static int NiftyBaseInitializerCounter; // zero initialized at load time
  static typename std::aligned_storage<sizeof(BaseDebugImpl), alignof(BaseDebugImpl)>::type TheCounter_Buffer;
  static BaseDebugImpl &TheCounter = reinterpret_cast<BaseDebugImpl&>(TheCounter_Buffer);
}

//{ struct BaseInitializer ...

BaseInitializer::BaseInitializer() {
  /* All static objects are created in the same thread, in arbitrary order.
     BaseInitializer only exists as static objects.
     So, no locking is needed .
   */
  if(NiftyBaseInitializerCounter++ == 0)
    new(&TheCounter) BaseDebugImpl();
}

BaseInitializer::~BaseInitializer() {
  /* All static objects are destructed in the same thread, in arbitrary order.
     BaseInitializer only exists as static objects.
     So, no locking is needed .
   */
  if(--NiftyBaseInitializerCounter == 0)
    (&TheCounter)->~BaseDebugImpl();
}

//} ... struct BaseInitializer

#endif

//{ class Base ...

Base::Base() {
#ifdef RICHMATH_DEBUG_MEMORY
  SET_BASE_DEBUG_TAG(typeid(*this).name());
  debug_alloc_time = pmath_atomic_fetch_add(&TheCounter.timer, 1);
  
  (void)pmath_atomic_fetch_add(&TheCounter.count, 1);
  {
    Locked guard(&TheCounter.lock);
    
    debug_prev = nullptr;
    debug_next = TheCounter.non_freed_objects_list;
    
    if(TheCounter.non_freed_objects_list)
      TheCounter.non_freed_objects_list->debug_prev = this;
      
    TheCounter.non_freed_objects_list = this;
  }
#endif
}

Base::~Base() {
#ifdef RICHMATH_DEBUG_MEMORY
  {
    Locked guard(&TheCounter.lock);
    
    if(debug_prev)
      debug_prev->debug_next = debug_next;
    if(debug_next)
      debug_next->debug_prev = debug_prev;
      
    if(this == TheCounter.non_freed_objects_list)
      TheCounter.non_freed_objects_list = debug_next ? debug_next : debug_prev;
      
  }
  (void)pmath_atomic_fetch_add(&TheCounter.count, -1);
#endif
}

//} ... class Base

//{ class Shareable ...

Shareable::Shareable()
  : Base()
{
  SET_BASE_DEBUG_TAG(typeid(*this).name());
  pmath_atomic_write_release(&_refcount, 1);
}

Shareable::~Shareable() {
  assert(pmath_atomic_read_aquire(&_refcount) == 0);
}

void Shareable::ref() const {
  (void)pmath_atomic_fetch_add(&_refcount, 1);
}

void Shareable::unref() const {
  if(pmath_atomic_fetch_add(&_refcount, -1) == 1)
    delete this;
}

//} ... class Shareable
