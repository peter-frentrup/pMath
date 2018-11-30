#ifndef RICHMATH__UTIL__FRONTENDOBJECT_H__INCLUDED
#define RICHMATH__UTIL__FRONTENDOBJECT_H__INCLUDED

#include <pmath-cpp.h>

#include <util/hashtable.h>


namespace richmath {
  class FrontEndReference {
    friend class FrontEndObject;
    friend class FrontEndReferenceImpl;
    public:
      int hash() const { return default_hash(_id) ^ 0xBADC0DE; }
      
      bool is_valid() const { return !!_id; }
      explicit operator bool() const { return is_valid(); }
      
      static FrontEndReference from_pmath(pmath::Expr expr);
      static FrontEndReference from_pmath_raw(pmath::Expr expr);
      
      pmath::Expr to_pmath() const;
      pmath::Expr to_pmath_raw() const { return pmath::Expr((int32_t)_id); }
      
      friend bool operator==(const FrontEndReference &left, const FrontEndReference &right) {
        return left._id == right._id;
      } 
      friend bool operator!=(const FrontEndReference &left, const FrontEndReference &right) {
        return left._id != right._id;
      }
      
      static const FrontEndReference None;
      
      static void *unsafe_cast_to_pointer(const FrontEndReference &ref) {
        return (void*)(uintptr_t)ref._id;
      }
      
      static FrontEndReference unsafe_cast_from_pointer(void *p) {
        FrontEndReference ref;
        ref._id = (uint32_t)(uintptr_t)p;
        return ref;
      }
    
    private:
      uint32_t _id;
  };

  class FrontEndObject: public virtual Base {
    public:
      FrontEndObject();
      virtual ~FrontEndObject();
      
      FrontEndReference id() { return _id; }
      
      static FrontEndObject *find(FrontEndReference id);
      
      template<class T>
      static T *find_cast(FrontEndReference id) {
        return dynamic_cast<T*>(find(id));
      }
      
      virtual void dynamic_updated() = 0;
    
    private:
      FrontEndReference _id;
  };
  
  // http://en.wikibooks.org/wiki/More_C%2B%2B_Idioms/Nifty_Counter
  static struct FrontEndObjectInitializer {
    FrontEndObjectInitializer();
    ~FrontEndObjectInitializer();
  } TheFrontEndObjectInitializer;
}

#endif // RICHMATH__UTIL__FRONTENDOBJECT_H__INCLUDED
