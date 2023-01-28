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

#ifdef NDEBUG
#  define COM_ASSERT(a) ((void)0)
#else
#  define COM_ASSERT(a) \
     do{if(!(a)){ \
         assert_failed(); \
         assert(a); \
       }}while(0)
#endif

namespace richmath {
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
      
      ComBase(nullptr_t) {}
      
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
      
      static const GUID &iid() noexcept {
        return __uuidof(Interface);
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
        if(Interface *temp = ptr) {
          ptr = nullptr;
          temp->Release();
        }
      }
      
      void internal_copy(Interface *other) noexcept {
        if(ptr != other) {
          internal_release();
          ptr = other;
          internal_add_reference();
        }
      }
      
      template <typename T>
      void internal_move(ComBase<T> &other) noexcept {
        if(ptr != other.ptr) {
          internal_release();
          ptr = other.ptr;
          other.ptr = nullptr;
        }
      }
  };
  
  template <typename Struct>
  class ComHashPtr {
    public:
      ComHashPtr(const ComHashPtr&) = delete;
      ComHashPtr &operator=(const ComHashPtr&) = delete;
      
      explicit ComHashPtr(Struct *p = nullptr) : ptr(p) {}
      ~ComHashPtr() { internal_release(); }
      
      ComHashPtr(ComHashPtr &&other) : ptr(nullptr) { swap(*this, other); }
      ComHashPtr &operator=(ComHashPtr &&other) { swap(*this, other); return *this; }
      
      friend void swap(ComHashPtr &left, ComHashPtr &right) noexcept {
        using std::swap;
        swap(left.ptr, right.ptr);
      }
      
      explicit operator bool() const noexcept { return ptr != nullptr; }
      
      Struct *get() const noexcept {
        COM_ASSERT(ptr != nullptr);
        return ptr;
      }
      
      Struct *operator->() const noexcept {
        COM_ASSERT(ptr != nullptr);
        return ptr;
      }
      
      Struct **get_address_of() noexcept {
        COM_ASSERT(ptr == nullptr);
        return &ptr;
      }
      
      void attach(Struct *p) noexcept {
        internal_release();
        ptr = p;
      }
      Struct *detach() noexcept {
        Struct *result = ptr;
        ptr = nullptr;
        return result;
      }
      
    private:
      Struct *ptr;
      
      void internal_release() noexcept {
        if(Struct *temp = ptr) {
          ptr = nullptr;
          CoTaskMemFree(temp);
        }
      }
  };
  
  #define HRreport(call) check_HRESULT((call), #call, __FILE__, __LINE__)
  #define HRbool(call) SUCCEEDED(check_HRESULT((call), #call, __FILE__, __LINE__))
  #define HR(call)       do { HRESULT _temp_hr = (call); if(!SUCCEEDED(_temp_hr)) return check_HRESULT(_temp_hr, #call, __FILE__, __LINE__); } while(0)
  #define HRquiet(call)  do { HRESULT _temp_hr = (call); if(!SUCCEEDED(_temp_hr)) return _temp_hr; } while(0)
  static inline HRESULT check_HRESULT(HRESULT hr, const char *call, const char *file, int line) {
    if(FAILED(hr))
      fprintf(stderr, "%s:%d: call %s failed with %x\n", file, line, call, (unsigned)hr);
    return hr;
  }
  
  extern IUnknown *get_canonical_iunknown(IUnknown *punk);
}

#endif // RICHMATH__GUI__WIN32__OLE__COMBASE_H__INCLUDED
