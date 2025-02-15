#include <util/frontendobject.h>
#include <util/hashtable.h>

#include <eval/application.h>
#include <util/styled-object.h>

#include <cstdio>
#include <new> // placement new


using namespace richmath;
using namespace pmath;

extern pmath_symbol_t richmath_System_DollarFailed;
extern pmath_symbol_t richmath_System_DocumentObject;
extern pmath_symbol_t richmath_System_FrontEndObject;
extern pmath_symbol_t richmath_System_List;
extern pmath_symbol_t richmath_System_None;

extern pmath_symbol_t richmath_FE_BoxReference;

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
  static char TheCache_Buffer[sizeof(FrontEndReferenceImpl)] alignas(FrontEndReferenceImpl);
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
  if(expr.expr_length() == 1 && expr.item_equals(0, richmath_System_DocumentObject))
    expr = expr[1];
  
  if(expr.expr_length() == 2 && expr.item_equals(0, richmath_System_FrontEndObject)) {
    if(expr.item_equals(1, Application::session_id))
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

FrontEndObject *FrontEndObject::find_box_reference(Expr boxref) {
  if(!boxref.item_equals(0, richmath_FE_BoxReference))
    return nullptr;
  
  auto basis_id = FrontEndReference::from_pmath(boxref[1]);
  if(!basis_id)
    return nullptr;
  
  auto basis = find(basis_id);
  if(!basis)
    return nullptr;
  
  Expr boxid = boxref[2];
  if(boxid.item_equals(0, richmath_System_List))
    boxid = boxid[1];
  
  if(!boxid || boxid == richmath_System_None)
    return nullptr;
  
  auto iterable = Stylesheet::find_registered_box(boxid);
  for(FrontEndReference ref : iterable) {
    if(auto obj = find_cast<StyledObject>(ref)) {
      if(obj->get_own_style(BoxID) != boxid) {
        pmath_debug_print("[box #%d = %p does not have ", (int)(intptr_t)FrontEndReference::unsafe_cast_to_pointer(obj->id()), obj);
        pmath_debug_print_object(" BoxID -> ", boxid.get(), ", but has ");
        pmath_debug_print_object(" BoxID -> ", obj->get_own_style(BoxID).get(), "]\n");
        continue;
      }
      
      for(auto tmp = obj; tmp; tmp = tmp->style_parent()) {
        if(tmp == basis)
          return obj;
      }
    }
  }
  
  return nullptr;
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

Expr richmath_eval_FrontEnd_BoxReferenceBoxObject(Expr expr) {
  if(expr.expr_length() != 1)
    return Symbol(richmath_System_DollarFailed);
  
  FrontEndObject *res = FrontEndObject::find_box_reference(expr[1]);
  if(!res)
    return Symbol(richmath_System_DollarFailed);
  
  return res->to_pmath_id();
}
