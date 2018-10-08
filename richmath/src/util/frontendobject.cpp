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
        id._id = 0;
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
    result._id = PMATH_AS_INT32(expr.get());
    return result;
  }
  
  return FrontEndReference::None;
}

//} ... class FrontEndReference

//{ class FrontEndObject ...

static Hashtable<FrontEndReference, FrontEndObject*> front_end_object_cache;
static int next_front_end_object_id = 0;

FrontEndObject::FrontEndObject()
  : Base()
{
  _id._id = ++next_front_end_object_id;
  SET_BASE_DEBUG_TAG(typeid(*this).name());
  
  front_end_object_cache.set(_id, this);
}

FrontEndObject::~FrontEndObject() {
  front_end_object_cache.remove(_id);
}

FrontEndObject *FrontEndObject::find(FrontEndReference id) {
  return front_end_object_cache[id];
}

FrontEndObject *FrontEndObject::find(Expr frontendobject) {
  return find(FrontEndReference::from_pmath(frontendobject));
//  if( frontendobject.expr_length() == 1 &&
//      frontendobject[0] == richmath_System_FrontEndObject)
//  {
//    Expr num = frontendobject[1];
//
//    if(num.is_int32())
//      return find(PMATH_AS_INT32(num.get()));
//  }
//
//  return 0;
}

void FrontEndObject::swap_id(FrontEndObject *other) {
  if(other) {
    auto id = other->_id;
    other->_id = this->_id;
    this->_id  = id;
    front_end_object_cache.set(other->_id, other);
    front_end_object_cache.set(this->_id,  this);
  }
}

//} ... class FrontEndObject
