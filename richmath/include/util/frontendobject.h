#ifndef RICHMATH__UTIL__FRONTENDOBJECT_H__INCLUDED
#define RICHMATH__UTIL__FRONTENDOBJECT_H__INCLUDED

#include <pmath-cpp.h>

#include <util/base.h>


namespace richmath {
  class FrontEndReference {
    friend class FrontEndObject;
    friend class FrontEndReferenceImpl;
    public:
      int hash() const { return _id ^ 0xBADC0DE; }
      
      bool is_valid() const { return _id != 0; }
      explicit operator bool() const { return is_valid(); }
      
      static FrontEndReference from_pmath(pmath::Expr expr);
      static FrontEndReference from_pmath_raw(pmath::Expr expr);
      
      pmath::Expr to_pmath() const;
      pmath::Expr to_pmath_raw() const { return pmath::Expr(_id); }
      
      friend bool operator==(const FrontEndReference &left, const FrontEndReference &right) {
        return left._id == right._id;
      } 
      friend bool operator!=(const FrontEndReference &left, const FrontEndReference &right) {
        return left._id != right._id;
      }
      
      static const FrontEndReference None;
    
    private:
      int _id;
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
      
      void swap_id(FrontEndObject *other);
      
      virtual void dynamic_updated() = 0;
      
    private:
      FrontEndReference _id;
  };
  
}

#endif // RICHMATH__UTIL__FRONTENDOBJECT_H__INCLUDED
