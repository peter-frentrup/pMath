#include <util/frontendobject.h>
#include <util/hashtable.h>


using namespace richmath;
using namespace pmath;

//{ class FrontEndObject ...

static Hashtable<int, FrontEndObject*, cast_hash> front_end_object_cache;
static int next_front_end_object_id = 0;

FrontEndObject::FrontEndObject()
  : Base(),
  _id(++next_front_end_object_id)
{
  front_end_object_cache.set(_id, this);
}

FrontEndObject::~FrontEndObject() {
  front_end_object_cache.remove(_id);
}

FrontEndObject *FrontEndObject::find(int id) {
  return front_end_object_cache[id];
}

FrontEndObject *FrontEndObject::find(Expr frontendobject) {
  if( frontendobject.expr_length() == 1 && 
      frontendobject[0] == PMATH_SYMBOL_FRONTENDOBJECT) 
  {
    Expr num = frontendobject[1];
    
    if(num.is_int32())
      return find(PMATH_AS_INT32(num.get()));
  }
  
  return 0;
}

void FrontEndObject::swap_id(FrontEndObject *other) {
  if(other) {
    int id = other->_id;
    other->_id = this->_id;
    this->_id  = id;
    front_end_object_cache.set(other->_id, other);
    front_end_object_cache.set(this->_id,  this);
  }
}

//} ... class FrontEndObject
