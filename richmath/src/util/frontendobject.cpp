#include <util/frontendobject.h>
#include <util/hashtable.h>


using namespace richmath;
using namespace pmath;

extern pmath_symbol_t richmath_System_FrontEndObject;

namespace richmath {
  class FrontEndReferenceImpl {
    public:
      static FrontEndReference init_none() {
        FrontEndReference id;
        id._id = nullptr;
        return id;
      }
  };
}

//{ class FrontEndReference ...
const FrontEndReference FrontEndReference::None = FrontEndReferenceImpl::init_none();

FrontEndReference FrontEndReference::from_pmath(pmath::Expr expr) {
  if( expr.expr_length() == 1 &&
      expr[0] == richmath_System_FrontEndObject)
  {
    return from_pmath_raw(expr[1]);
  }
  
  return FrontEndReference::None;
}

FrontEndReference FrontEndReference::from_pmath_raw(pmath::Expr expr) {
  if(expr.is_int32()) {
    FrontEndReference result;
    result._id = (void*)(intptr_t)PMATH_AS_INT32(expr.get());
    return result;
  }
  else if(expr.is_integer() && pmath_integer_fits_si64(expr.get())) {
    FrontEndReference result;
    result._id = (void*)pmath_integer_get_siptr(expr.get());
    return result;
  }
  
  return FrontEndReference::None;
}

Expr FrontEndReference::to_pmath() const {
  return Call(Symbol(richmath_System_FrontEndObject), to_pmath_raw());
}

//} ... class FrontEndReference

//{ class FrontEndObject ...

static Hashtable<FrontEndReference, Void> front_end_object_cache;

FrontEndObject::FrontEndObject()
  : Base()
{
  SET_BASE_DEBUG_TAG(typeid(*this).name());
  
  front_end_object_cache.set(id(), Void{});
}

FrontEndObject::~FrontEndObject() {
  front_end_object_cache.remove(id());
}

FrontEndObject *FrontEndObject::find(FrontEndReference id) {
  if(front_end_object_cache.search(id))
    return static_cast<FrontEndObject*>(FrontEndReference::unsafe_cast_to_pointer(id));
  return nullptr;
}

//} ... class FrontEndObject
