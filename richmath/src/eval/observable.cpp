#include <eval/observable.h>
#include <eval/application.h>
#include <eval/dynamic.h>

#include <new> // placement new


using namespace richmath;
using namespace pmath;


namespace richmath {
  class ObservatoryImpl {
    public:
      ObservatoryImpl() : _quitting(false) {
      }
      
      void shutdown() { _quitting = true; }
      bool is_quitting() { return _quitting; }
      
    public:
      Hashtable<Observable*, Expr>                        all_observers;
      Hashtable<FrontEndReference, Hashset<Observable*>>  all_observed_values;
    
    private:
      bool _quitting;
  }; 
}

namespace {
  static int NiftyObservatoryInitializerCounter; // zero initialized at load time
  static char TheObservatory_Buffer[sizeof(ObservatoryImpl)] alignas(ObservatoryImpl);
  static ObservatoryImpl &TheObservatory = reinterpret_cast<ObservatoryImpl&>(TheObservatory_Buffer);
};

//{ class ObservatoryInitializer ...

ObservatoryInitializer::ObservatoryInitializer() {
  /* All static objects are created in the same thread, in arbitrary order.
     ObservatoryInitializer only exists as static objects.
     So, no locking is needed .
   */
  if(NiftyObservatoryInitializerCounter++ == 0)
    new(&TheObservatory) ObservatoryImpl();
}

ObservatoryInitializer::~ObservatoryInitializer() {
  /* All static objects are destructed in the same thread, in arbitrary order.
     ObservatoryInitializer only exists as static objects.
     So, no locking is needed .
   */
  if(--NiftyObservatoryInitializerCounter == 0)
    (&TheObservatory)->~ObservatoryImpl();
}

//} ... class ObservatoryInitializer

//{ class Observatory ...

void Observatory::shutdown() {
  TheObservatory.all_observed_values.clear();
  TheObservatory.all_observers.clear();
  TheObservatory.shutdown();
}

//} ... class Observatory

//{ class Observable ...

static void swap_observers(Observable *left, Observable *right) {
  RICHMATH_ASSERT(left != nullptr);
  RICHMATH_ASSERT(right != nullptr);
  
  Expr left_observers = TheObservatory.all_observers[left];
  Expr right_observers = TheObservatory.all_observers[right];
  
  size_t left_count  = left_observers.expr_length();
  size_t right_count = right_observers.expr_length();
  
  if(left_count == 0 && right_count == 0)
    return;
  
  for(size_t i = left_count; i > 0; --i) {
    auto id = FrontEndReference::from_pmath_raw(left_observers[i]);
    if(id) {
      auto observed = TheObservatory.all_observed_values.search(id);
      if(!observed) {
        TheObservatory.all_observed_values.set(id, Hashset<Observable*> {});
        observed = TheObservatory.all_observed_values.search(id);
        HASHTABLE_ASSERT(observed != nullptr);
      }
      observed->add(right);
    }
  }
  
  for(size_t i = right_count; i > 0; --i) {
    auto id = FrontEndReference::from_pmath_raw(right_observers[i]);
    if(id) {
      auto observed = TheObservatory.all_observed_values.search(id);
      if(!observed) {
        TheObservatory.all_observed_values.set(id, Hashset<Observable*> {});
        observed = TheObservatory.all_observed_values.search(id);
        HASHTABLE_ASSERT(observed != nullptr);
      }
      observed->add(left);
    }
  }
  
  TheObservatory.all_observers.set(right, PMATH_CPP_MOVE(left_observers));
  TheObservatory.all_observers.set(left, PMATH_CPP_MOVE(right_observers));
}

Observable::Observable()
  : Base()
{
  SET_BASE_DEBUG_TAG(typeid(*this).name());
}

Observable::~Observable() {
  notify_all();
  RICHMATH_ASSERT(!TheObservatory.all_observers.search(this));
}

Observable::Observable(Observable &&src)
  : Base()
{
  swap_observers(this, &src);
}

Observable &Observable::operator=(Observable &&src) {
  //Base::operator=(src);
  swap_observers(this, &src);
  return *this;
}

void Observable::register_observer() const {
  register_observer(Dynamic::current_observer_id);
}

void Observable::register_observer(FrontEndReference id) const {
  if(!id)
    return;
  
  if(TheObservatory.is_quitting())
    return;
  
  auto observed = TheObservatory.all_observed_values.search(id);
  if(!observed) {
    TheObservatory.all_observed_values.set(id, Hashset<Observable*> {});
    observed = TheObservatory.all_observed_values.search(id);
    HASHTABLE_ASSERT(observed != nullptr);
  }
  bool known = observed->search(const_cast<Observable*>(this)) != nullptr;
  if(known)
    return;
  
  observed->add(const_cast<Observable*>(this));
  
  Expr id_obj = id.to_pmath_raw();
  
  Expr *observer_ids = TheObservatory.all_observers.search(const_cast<Observable*>(this));
  if(!observer_ids) {
    TheObservatory.all_observers.set(const_cast<Observable*>(this), List(id_obj));
    return;
  }
  observer_ids->append(id_obj);
}

void Observable::unregister_oberserver(FrontEndReference id) {
  if(TheObservatory.is_quitting())
    return;
  
  auto observed = TheObservatory.all_observed_values.search(id);
  if(!observed)
    return;
    
  Expr id_obj = id.to_pmath_raw();
  
  for(auto &e : observed->entries()) {
    Observable *o = e.key;
    Expr &observers_of_o = TheObservatory.all_observers[o];
    if(observers_of_o.is_valid()) {
      observers_of_o.expr_remove_all(id_obj);
      if(observers_of_o.expr_length() == 0)
        TheObservatory.all_observers.remove(o);
    } 
  }
  TheObservatory.all_observed_values.remove(id);
}

void Observable::notify_all() {
  if(TheObservatory.is_quitting()) 
    return;
    
  auto &my_observers = TheObservatory.all_observers[this];
  
  for(size_t i = my_observers.expr_length(); i > 0; --i) {
    auto id = FrontEndReference::from_pmath_raw(my_observers[i]);
    if(id)
      TheObservatory.all_observed_values.remove(id);
  }
  
  if(!my_observers.is_null())
    Application::notify(ClientNotification::DynamicUpdate, my_observers);
  
  TheObservatory.all_observers.remove(this);
}

//} ... class Observable
