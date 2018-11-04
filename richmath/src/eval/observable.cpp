#include <eval/observable.h>
#include <eval/application.h>
#include <eval/dynamic.h>

using namespace richmath;
using namespace pmath;

//{ class Observable ...

static Hashtable<Observable*, Expr>                                 all_observers;
static Hashtable<FrontEndReference, Hashtable<Observable*, Void> >  all_observed_values;

Observable::Observable()
  : Base()
{
  SET_BASE_DEBUG_TAG(typeid(*this).name());
}

Observable::~Observable() {
  auto my_observers = all_observers[this];
  all_observers.remove(this);
  
  for(size_t i = my_observers.expr_length(); i > 0; --i) {
    auto id = FrontEndReference::from_pmath(my_observers[i]);
    if(!id)
      continue;
      
    auto observed = all_observed_values.search(id);
    if(observed) {
      observed->remove(this);
      if(observed->size() == 0) {
        all_observed_values.remove(id);
      }
    }
  }
}

void Observable::register_observer() {
  register_observer(Dynamic::current_evaluation_box_id);
}

void Observable::register_observer(FrontEndReference id) {
  if(!id)
    return;
  
  auto observed = all_observed_values.search(id);
  if(!observed) {
    all_observed_values.set(id, Hashtable<Observable*, Void> {});
    observed = all_observed_values.search(id);
    HASHTABLE_ASSERT(observed != nullptr);
  }
  bool known = observed->search(this) != nullptr;
  if(known)
    return;
  
  observed->set(this, Void{});
  
  Expr id_obj = id.to_pmath_raw();
  
  Expr *observer_ids = all_observers.search(this);
  if(!observer_ids) {
    all_observers.set(this, List(id_obj));
    return;
  }
  observer_ids->append(id_obj);
}

void Observable::unregister_oberserver(FrontEndReference id) {
  auto observed = all_observed_values.search(id);
  if(!observed)
    return;
    
  Expr id_obj = id.to_pmath_raw();
  
  for(auto &e : observed->entries()) {
    Observable *o = e.key;
    Expr &observers_of_o = all_observers[o];
    if(observers_of_o.is_valid()) {
      observers_of_o.expr_remove_all(id_obj);
      if(observers_of_o.expr_length() == 0)
        all_observers.remove(o);
    } 
  }
  all_observed_values.remove(id);
}

void Observable::notify_all() {
  auto &my_observers = all_observers[this];
  
  for(size_t i = my_observers.expr_length(); i > 0; --i) {
    auto id = FrontEndReference::from_pmath(my_observers[i]);
    if(id)
      all_observed_values.remove(id);
  }
  
  Application::notify(ClientNotification::DynamicUpdate, my_observers);
  all_observers.remove(this);
}

//} ... class Observable
