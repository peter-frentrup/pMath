#ifndef RICHMATH__GUI__WIN32__OLE__COMBASE_H__INCLUDED
#define RICHMATH__GUI__WIN32__OLE__COMBASE_H__INCLUDED

#ifndef RICHMATH_USE_WIN32_GUI
#  error this header is win32 specific
#endif

#include <utility>
#include <cassert>
#include <cstdio>

#include <util/base.h>

#include <windows.h>

namespace richmath {
  #define COM_ASSERT(a) \
  do{if(!(a)){ \
      assert_failed(); \
      assert(a); \
    }}while(0)
  
  void assert_failed();
  
  /* See
     Kenny Kerr: Windows with C++ - COM Smart Pointers Revisited
     (https://msdn.microsoft.com/en-us/magazine/dn904668.aspx)
   */
  
  template <typename Interface>
  class RemoveAddRefRelease: public Interface {
      ULONG WINAPI AddRef() override;
      ULONG WINAPI Release() override;
  };
  
  template <typename Interface>
  class ComBase {
      template <typename T>
      friend class ComBase;
    public:
      ComBase() noexcept = default;
      
      ~ComBase() noexcept {
        internal_release();
      }
      
      ComBase(const ComBase &other) noexcept
        : ptr(other.ptr)
      {
        internal_add_reference();
      }
      
      template <typename T>
      ComBase(const ComBase<T> &other) noexcept
        : ptr(other.ptr)
      {
        internal_add_reference();
      }
      
      ComBase(ComBase &&other) noexcept
        : ptr(other.ptr)
      {
        other.ptr = nullptr;
      }
      
      template <typename T>
      ComBase(ComBase<T> &&other) noexcept
        : ptr(other.ptr)
      {
        other.ptr = nullptr;
      }
      
      ComBase &operator=(ComBase const &other) noexcept {
        internal_copy(other.ptr);
        return *this;
      }
      
      template <typename T>
      ComBase &operator=(ComBase<T> const &other) noexcept {
        internal_copy(other.ptr);
        return *this;
      }
      
      ComBase &operator=(ComBase &&other) noexcept {
        internal_move(other);
        return *this;
      }
      
      template <typename T>
      ComBase &operator=(ComBase<T> &&other) noexcept {
        internal_move(other);
        return *this;
      }
      
      friend void swap(ComBase &left, ComBase &right) noexcept {
        using std::swap;
        swap(left.ptr, right.ptr);
      }
      
      explicit operator bool() const noexcept {
        return is_available();
      }
      
      bool is_available() const noexcept {
        return ptr != nullptr;
      }
      
      Interface *get() const noexcept {
        return ptr;
      }
      
      void reset() noexcept {
        internal_release();
      }
      
      Interface *detach() noexcept {
        Interface * temp = ptr;
        ptr = nullptr;
        return temp;
      }
      
      void copy(Interface *other) noexcept {
        internal_copy(other);
      }
      
      void copy_to(Interface ** other) const noexcept {
        COM_ASSERT(other != nullptr);
        internal_add_reference();
        *other = ptr;
      }
      
      void attach(Interface *other) noexcept {
        internal_release();
        ptr = other;
      }
      
      RemoveAddRefRelease<Interface> *operator->() const noexcept {
        COM_ASSERT(ptr != nullptr);
        return static_cast<RemoveAddRefRelease<Interface> *>(ptr);
      }
      
      Interface **get_address_of() noexcept {
        COM_ASSERT(ptr == nullptr);
        return &ptr;
      }
      
      // this requires __uuid() support
      template <typename T>
      ComBase<T> as() const noexcept {
        ComBase<T> temp;
        if(ptr) {
          ptr->QueryInterface(temp.get_address_of());
        }
        return temp;
      }
      
    private:
      Interface *ptr = nullptr;
      
      void internal_add_reference() const noexcept {
        if(ptr)
          ptr->AddRef();
      }
      
      void internal_release() noexcept {
        Interface * temp = ptr;
        if (temp) {
          ptr = nullptr;
          temp->Release();
        }
      }
      
      void internal_copy(Interface *other) noexcept {
        if (ptr != other) {
          internal_release();
          ptr = other;
          internal_add_reference();
        }
      }
      
      template <typename T>
      void internal_move(ComBase<T> &other) noexcept {
        if (ptr != other.ptr) {
          internal_release();
          ptr = other.ptr;
          other.ptr = nullptr;
        }
      }
  };

  #define HRbool(call) SUCCEEDED(check_HRESULT((call), #call, __FILE__, __LINE__))
  #define HR(call) do { HRESULT _temp_hr = (call); if(!SUCCEEDED(_temp_hr)) return check_HRESULT(_temp_hr, #call, __FILE__, __LINE__); } while(0)
  static inline HRESULT check_HRESULT(HRESULT hr, const char *call, const char *file, int line) {
    if(FAILED(hr))
      fprintf(stderr, "%s:%d: call %s failed with %x\n", file, line, call, (unsigned)hr);
    return hr;
  }
}

#endif // RICHMATH__GUI__WIN32__OLE__COMBASE_H__INCLUDED
