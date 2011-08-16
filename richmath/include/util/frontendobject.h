#ifndef __UTIL__FRONTENDOBJECT_H__
#define __UTIL__FRONTENDOBJECT_H__

#include <pmath-cpp.h>

#include <util/base.h>


namespace richmath {

  class FrontEndObject: public Base {
    public:
      FrontEndObject();
      virtual ~FrontEndObject();
      
      int id(){ return _id; }
      
      static FrontEndObject *find(int id);
      static FrontEndObject *find(pmath::Expr frontendobject);
      
      template<class T>
      static T *find_cast(int id){ 
        return dynamic_cast<T*>(find(id)); 
      }
      
      template<class T>
      static T *find_cast(pmath::Expr frontendobject){ 
        return dynamic_cast<T*>(find(frontendobject));
      }
      
      void swap_id(FrontEndObject *other);
      
      virtual void dynamic_updated() = 0;
      
    private:
      int _id;
  };
  
}

#endif // __UTIL__FRONTENDOBJECT_H__
