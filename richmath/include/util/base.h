#ifndef RICHMATH__UTIL__BASE_H__INCLUDED
#define RICHMATH__UTIL__BASE_H__INCLUDED

#include <pmath-util/concurrency/atomic.h>
#include <stdint.h>

#include <typeinfo>


#ifndef NDEBUG
#  define RICHMATH_DEBUG_MEMORY
#endif

#define RICHMATH_ASSERT(a) \
  do{if(!(a)){ \
      assert_failed(); \
      assert(a); \
    }}while(0)

namespace richmath {
  void assert_failed();
  
  static inline int boole(bool b) { return b ? 1 : 0; }
  
  extern const double Infinity;
  
  class Base {
#ifdef RICHMATH_DEBUG_MEMORY
      friend class BaseDebugImpl;
    private:
      const char   *debug_tag;
      intptr_t      debug_alloc_time;
      mutable Base *debug_prev;
      mutable Base *debug_next;
    protected:
      const char *get_debug_tag() const { return debug_tag; }
      void SET_BASE_DEBUG_TAG(const char *tag) { debug_tag = tag; }
    #define SET_EXPLICIT_BASE_DEBUG_TAG(CLASS, TAG_NAME)  CLASS::SET_BASE_DEBUG_TAG(TAG_NAME)
#else
    #define SET_BASE_DEBUG_TAG(TAG_NAME) ((void)0)
    #define SET_EXPLICIT_BASE_DEBUG_TAG(CLASS, TAG_NAME)   ((void)0)
#endif
    public:
      Base();
      ~Base();
      
    private:
      Base(const Base &src) = delete;
      Base const &operator=(Base const &src) = delete;
  };
  
  class Shareable: public Base {
    public:
      Shareable();
      virtual ~Shareable();
      
      void ref() const;
      void unref() const;
      
      intptr_t refcount() const { return pmath_atomic_read_aquire(&_refcount); };
    private:
      mutable pmath_atomic_t _refcount;
  };
  
  template <
    typename T,
    T *(*ref_func)(T *),
    void (*unref_func)(T *)
    >
  class AutoRefBase {
    public:
      AutoRefBase(T *ptr = 0)
        : _ptr(ptr)
      { }
      
      AutoRefBase(const AutoRefBase<T, ref_func, unref_func> &src)
        : _ptr(src._ptr ? ref_func(src._ptr) : 0)
      { }
      
      ~AutoRefBase() {
        if(_ptr)
          unref_func(_ptr);
      }
      
      AutoRefBase &operator=(const AutoRefBase<T, ref_func, unref_func> &src) {
        T *old = _ptr;
        
        _ptr = src._ptr ? ref_func(src._ptr) : 0;
        
        if(old)
          unref_func(old);
          
        return *this;
      }
      
      T *ptr() { return _ptr; }
      
    private:
      T *_ptr;
  };
  
#ifdef RICHMATH_DEBUG_MEMORY
  // http://en.wikibooks.org/wiki/More_C%2B%2B_Idioms/Nifty_Counter
  static struct BaseInitializer {
    BaseInitializer();
    ~BaseInitializer();
  } TheBaseInitializer;
#endif
}

#endif // RICHMATH__UTIL__BASE_H__INCLUDED
