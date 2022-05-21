#include <util/frontendobject.h>
#include <util/hashtable.h>

#include <eval/application.h>

#include <new>         // placement new
#include <type_traits> // aligned_storage


using namespace richmath;
using namespace pmath;

extern pmath_symbol_t richmath_System_FrontEndObject;
extern pmath_symbol_t richmath_System_DocumentObject;

namespace {
  class ObjectWithLimboEnd final : public ObjectWithLimbo {
    virtual void safe_destroy() override {}
    virtual ObjectWithLimbo *next_in_limbo() override { return this; };
    virtual void next_in_limbo(ObjectWithLimbo *next) override { RICHMATH_ASSERT(next == this); }
  };
}

namespace richmath {
  class FrontEndReferenceImpl {
    public:
      FrontEndReferenceImpl() {
        _next_id._id = 1;
        object_limbo = &object_limbo_end;
        deletion_suspensions = 0;
      }
      
      static FrontEndReference init_none() {
        FrontEndReference id;
        id._id = 0;
        return id;
      }
      
      FrontEndReference generate_next_id() {
        FrontEndReference result;
        do {
          result = _next_id;
          _next_id._id++; // _id is unsigned, so increment has wrap-around semantics
          if(!_next_id._id)
            _next_id._id++;
        } while(table.search(_next_id));
        
        RICHMATH_ASSERT(result.is_valid());
        return result;
      }
    
    public:
      Hashtable<FrontEndReference, FrontEndObject*> table;
      
      // Actually for AutoMemorySuspension, not for FrontEndReference
      ObjectWithLimboEnd object_limbo_end;

      int deletion_suspensions;
      ObjectWithLimbo *object_limbo;

    private:
      FrontEndReference  _next_id;
  };
}

namespace {
  static int NiftyFrontEndObjectInitializerCounter; // zero initialized at load time
  static typename std::aligned_storage<
    sizeof(FrontEndReferenceImpl), 
    alignof(FrontEndReferenceImpl)
  >::type TheCache_Buffer;
  static FrontEndReferenceImpl &TheCache = reinterpret_cast<FrontEndReferenceImpl&>(TheCache_Buffer);
};

//{ struct FrontEndObjectInitializer ...

FrontEndObjectInitializer::FrontEndObjectInitializer() {
  /* All static objects are created in the same thread, in arbitrary order.
     FrontEndObjectInitializer only exists as static objects.
     So, no locking is needed .
   */
  if(NiftyFrontEndObjectInitializerCounter++ == 0)
    new(&TheCache) FrontEndReferenceImpl();
}

FrontEndObjectInitializer::~FrontEndObjectInitializer() {
  /* All static objects are destructed in the same thread, in arbitrary order.
     FrontEndObjectInitializer only exists as static objects.
     So, no locking is needed .
   */
  if(--NiftyFrontEndObjectInitializerCounter == 0)
    (&TheCache)->~FrontEndReferenceImpl();
}

//} ... struct FrontEndObjectInitializer

//{ class FrontEndReference ...
const FrontEndReference FrontEndReference::None = FrontEndReferenceImpl::init_none();

FrontEndReference FrontEndReference::from_pmath(pmath::Expr expr) {
  if(expr.expr_length() == 1 && expr[0] == richmath_System_DocumentObject)
    expr = expr[1];
  
  if(expr.expr_length() == 2 && expr[0] == richmath_System_FrontEndObject) {
    if(expr[1] == Application::session_id)
      return from_pmath_raw(expr[2]);
  }
  
  return FrontEndReference::None;
}

FrontEndReference FrontEndReference::from_pmath_raw(pmath::Expr expr) {
  if(expr.is_int32()) {
    FrontEndReference result;
    result._id = (uint32_t)PMATH_AS_INT32(expr.get());
    return result;
  }
  
  return FrontEndReference::None;
}

Expr FrontEndReference::to_pmath() const {
  return Call(Symbol(richmath_System_FrontEndObject), Application::session_id, to_pmath_raw());
}

//} ... class FrontEndReference

//{ class AutoMemorySuspension ...

bool AutoMemorySuspension::are_deletions_suspended() {
  return TheCache.deletion_suspensions > 0;
}

void AutoMemorySuspension::suspend_deletions() {
  ++TheCache.deletion_suspensions;
}

void AutoMemorySuspension::resume_deletions() {
  if(--TheCache.deletion_suspensions > 0)
    return;
    
  int count = 0;
  while(TheCache.object_limbo != &TheCache.object_limbo_end) {
    ObjectWithLimbo *tmp = TheCache.object_limbo;
    TheCache.object_limbo = tmp->next_in_limbo();
    
    delete tmp;
    ++count;
  }
  
  if(count > 0)
    fprintf(stderr, "[deleted %d objects from limbo]\n", count);
}

//} ... class AutoMemorySuspension

//{ class ObjectWithLimbo ...

ObjectWithLimbo::~ObjectWithLimbo() {
#ifdef RICHMATH_DEBUG_MEMORY
  if(AutoMemorySuspension::are_deletions_suspended()) {
    fprintf(stderr, "[warning: delete %s during memory suspension]\n", get_debug_tag());
  }
#endif
}

void ObjectWithLimbo::safe_destroy() {
  fprintf(stderr, "[safe_destroy %p %d next=%p, limbo=%p]\n", 
    this, 
    AutoMemorySuspension::are_deletions_suspended(),
    next_in_limbo(),
    TheCache.object_limbo);
  if(AutoMemorySuspension::are_deletions_suspended()) {
    if(next_in_limbo() != nullptr) {
      fprintf(stderr, "[warning: duplicate safe_destroy() for %p]\n", this);
      return;
    }
    next_in_limbo(TheCache.object_limbo);
    RICHMATH_ASSERT( next_in_limbo() == TheCache.object_limbo );
    TheCache.object_limbo = this;
    return;
  }
  if(next_in_limbo() != nullptr) {
    fprintf(stderr, "[error: duplicate safe_destroy() for %p, without memory suspensions now]\n", this);
    return;
  }
  delete this;
}

//} ... class ObjectWithLimbo

//{ class FrontEndObject ...

FrontEndObject::FrontEndObject()
  : ObjectWithLimbo(),
    _id{ TheCache.generate_next_id() },
    _flags{ 0 }
{
  SET_BASE_DEBUG_TAG(typeid(*this).name());
  
  TheCache.table.set(_id, this);
}

FrontEndObject::~FrontEndObject() {
  TheCache.table.remove(_id);
}

FrontEndObject *FrontEndObject::find(FrontEndReference id) {
  FrontEndObject **obj = TheCache.table.search(id);
  if(!obj)
    return nullptr;
  return *obj;
}

void FrontEndObject::swap_id(FrontEndObject *other) {
  if(other) {
    auto id = other->_id;
    other->_id = this->_id;
    this->_id  = id;
    TheCache.table.set(other->_id, other);
    TheCache.table.set(this->_id,  this);
  }
}

//} ... class FrontEndObject
