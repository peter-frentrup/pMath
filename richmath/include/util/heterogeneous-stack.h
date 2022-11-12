#ifndef RICHMATH__UTIL__HETEROGENEOUS_STACK_H__INCLUDED
#define RICHMATH__UTIL__HETEROGENEOUS_STACK_H__INCLUDED


#include <util/array.h>

namespace richmath {
  /** A stack for different objects, with semi-manual memory management.
      
      Every `push_new<T>(...)` must be accompanied by a corresponding `pop<T>()`.
      
      Pushed objects must be trivially relocatable: their storage will be moved 
      around without calling their move-constructor.
   */
  template<typename Unit> struct HeterogeneousStack {
    using unit_t = Unit;
    enum { granularity = sizeof(Unit) };
    
    Array<Unit> contents;
    
    template<typename T, typename... Args>
    T &push_new(Args&&... args) {
      void *ptr = push_new_object_place(sizeof(T));
      // Forward the arguments to the function without calling debug-harming std::forward
      T *ptr_as_t = new(ptr)T(static_cast<Args&&>(args)...);
      return *ptr_as_t;
    }
    
    template<typename T>
    T &get_top() {
      void *ptr = get_top_object_place(sizeof(T));
      return *(T*)ptr;
    }
    
    template<typename T>
    void pop() {
      void *ptr = pop_object_place(sizeof(T));
      ((T*)ptr)->~T();
    }
    
    void *push_new_object_place(size_t size) {
      int num_units = (int)((size + sizeof(Unit) - 1) / sizeof(Unit));
      
      ARRAY_ASSERT(num_units > 0);
      
      int oldlen = contents.length();
      contents.length(oldlen + num_units);
      return contents.items() + oldlen;
    }
    
    void *get_top_object_place(size_t size) {
      int num_units = (int)((size + sizeof(Unit) - 1) / sizeof(Unit));
      
      ARRAY_ASSERT(num_units > 0);
      
      int oldlen = contents.length();
      
      ARRAY_ASSERT(oldlen >= num_units);
      
      return contents.items() + oldlen - num_units;
    }
    
    void *pop_object_place(size_t size) {
      int num_units = (int)((size + sizeof(Unit) - 1) / sizeof(Unit));
      
      ARRAY_ASSERT(num_units > 0);
      
      int oldlen = contents.length();
      
      ARRAY_ASSERT(oldlen >= num_units);
      
      void *ret = contents.items() + oldlen - num_units;
      contents.length(oldlen - num_units);
     
      ARRAY_ASSERT(ret == contents.items() + contents.length());
      return ret;
    }
  };
  
  using BasicHeterogeneousStack = HeterogeneousStack<uintptr_t>;
  
#ifdef NDEBUG
#  define debug_test_heterogeneous_stack()  ((void)0)
#else
  void debug_test_heterogeneous_stack();
#endif
}

#endif // RICHMATH__UTIL__HETEROGENEOUS_STACK_H__INCLUDED
