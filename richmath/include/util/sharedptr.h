#ifndef RICHMATH__UTIL__SHAREDPTR_H__INCLUDED
#define RICHMATH__UTIL__SHAREDPTR_H__INCLUDED

#include <stdint.h>


namespace richmath {
  void assert_failed();

  /* T must provide public methods void ref() and void unref().
     The constructor of T should set its reference counter to 1. */
  template<class T> class SharedPtr {
    public:
      SharedPtr(T *ptr = nullptr)
        : _ptr(ptr)
      {
      }

      SharedPtr(const SharedPtr<T> &src)
        : _ptr(src._ptr)
      {
        if(_ptr)
          _ptr->ref();
      }

      template<class O> SharedPtr(const SharedPtr<O> &src)
        : _ptr(dynamic_cast<T*>(src.ptr()))
      {
        if(_ptr)
          _ptr->ref();
      }

      ~SharedPtr() {
        T *old_ptr = _ptr;
        _ptr = nullptr;
        if(old_ptr)
          old_ptr->unref();
      }

      SharedPtr &operator=(T *ptr) {
        T *old_ptr = _ptr;
        _ptr = ptr;
        if(old_ptr)
          old_ptr->unref();
        return *this;
      }

      SharedPtr &operator=(const SharedPtr &src) {
        /* In bla = bla->next, where bla._ptr.refcount = 1, _ptr would be
           deleted and thus src === &bla->next === &bla._ptr->next is lost, so
           we need to preserve the value in a local variable.
         */
        T *p = src._ptr;

        if(p)
          p->ref();
          
        T *old_ptr = _ptr;
        _ptr = p;
        if(old_ptr)
          old_ptr->unref();
        return *this;
      }

      //const T *ptr() const { return _ptr; }

      T *ptr() const { return _ptr; }

      T *release() {
        T *result = _ptr;
        _ptr = nullptr;
        return result;
      }

      const T *operator->() const {
        return _ptr;
      }

      T *operator->() {
        return _ptr;
      }

      template<class O> bool operator==(const SharedPtr<O> &src) const {
        return _ptr == src._ptr;
      }

      template<class O> bool operator!=(const SharedPtr<O> &src) const {
        return _ptr != src._ptr;
      }

      bool is_valid() const {
        return _ptr != nullptr;
      }

      operator bool() const {
        return is_valid();
      }

      unsigned int hash() const {
        return default_hash(_ptr);
      }

    protected:
      T *_ptr;
  };
}

#endif // RICHMATH__UTIL__SHAREDPTR_H__INCLUDED
