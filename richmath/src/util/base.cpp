#include <util/base.h>

#include <cmath>
#include <cstdio>

#include <new> // placement new

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
        debug_check_leaks_after(0);
      }
      
      void debug_check_leaks_after(intptr_t min_timer) {
        if(auto num_alive = pmath_atomic_read_aquire(&count)) {
          intptr_t num_alive_after_min_timer = num_alive;
          if(min_timer > 0) {
            num_alive_after_min_timer = 0;
            for(Base *obj = non_freed_objects_list; obj; obj = obj->debug_next) {
              if(obj->debug_alloc_time >= min_timer)
                ++num_alive_after_min_timer;
            }
          }
          
          if(num_alive_after_min_timer > 0) {
            if(min_timer > 0) {
              printf("\n%d objects alive. %d POSSIBLE LEAKS (created after time %d but still alive):",
                (int)num_alive, (int)num_alive_after_min_timer, (int)min_timer);
            }
            else
              printf("\n%d OBJECTS LEAKED:", (int)num_alive);
            
            Base *obj = non_freed_objects_list;
            for(int max_count = 2000; obj && max_count > 0; obj = obj->debug_next) {
              if(obj->debug_alloc_time >= min_timer) {
                --max_count;
                
                printf("\n  AT %p (time %d): %s", obj, (int)obj->debug_alloc_time, obj->debug_tag);
              }
            }
            if(obj)
              printf("\n  ...and more");
            
            printf("\n");
          }
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
  static alignas(BaseDebugImpl) char TheCounter_Buffer[sizeof(BaseDebugImpl)];
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

#ifdef RICHMATH_DEBUG_MEMORY
intptr_t Base::debug_alloc_clock() {
  return pmath_atomic_read_aquire(&TheCounter.timer);
}

void Base::debug_check_leaks_after(intptr_t alloc_clock) {
  TheCounter.debug_check_leaks_after(alloc_clock);
}
#endif

//} ... class Base

//{ class Shareable ...

Shareable::Shareable()
  : Base()
{
  SET_BASE_DEBUG_TAG(typeid(*this).name());
  pmath_atomic_write_release(&_refcount, 1);
}

Shareable::~Shareable() {
  RICHMATH_ASSERT(pmath_atomic_read_aquire(&_refcount) == 0);
}

void Shareable::ref() const {
  (void)pmath_atomic_fetch_add(&_refcount, 1);
}

void Shareable::unref() const {
  if(pmath_atomic_fetch_add(&_refcount, -1) == 1)
    delete this;
}

//} ... class Shareable
