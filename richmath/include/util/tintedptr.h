#ifndef RICHMATH__UTIL__TINTEDPTR_H__INCLUDED
#define RICHMATH__UTIL__TINTEDPTR_H__INCLUDED

#define TintedPtr_ASSERT(a) \
  do{if(!(a)){ \
      assert_failed(); \
      assert(a); \
    }}while(0)

namespace richmath {
  void assert_failed();
  
  template<typename Normal, typename Tinted>
  class TintedPtr final {
      //static_assert(alignof(Normal) > 1, "Cannot tint byte-aligned pointers");
      //static_assert(alignof(Tinted) > 1, "Cannot tint byte-aligned pointers");
    
    private:
      explicit TintedPtr(void *ptr, bool normal) 
        : _data(normal ? (uintptr_t)ptr : (uintptr_t)ptr | (uintptr_t)1) 
      { TintedPtr_ASSERT(((uintptr_t)ptr & 1U) == 0U); }
      
      void *pointer() const { return (void*)(_data & ~(uintptr_t)1); }
    
    public:
      TintedPtr() : TintedPtr(nullptr, true) {}
      explicit TintedPtr(Normal *normal) : TintedPtr(normal, true) {}
      static TintedPtr FromTinted(Tinted *tinted) { return TintedPtr(tinted, false); }
      
      bool is_normal() const { return (_data & 1U) == 0U; }
      bool is_tinted() const { return (_data & 1U); }
      
      Normal *as_normal() const { return is_normal() ? (Normal*)pointer() : nullptr; }
      Tinted *as_tinted() const { return is_tinted() ? (Tinted*)pointer() : nullptr; }
      
      void set_to_normal(Normal *normal) { *this = TintedPtr(normal); }
      void set_to_tinted(Tinted *tinted) { *this = FromTinted(tinted); }
      
    private:
      uintptr_t _data;
  };
}

#endif // RICHMATH__UTIL__TINTEDPTR_H__INCLUDED
