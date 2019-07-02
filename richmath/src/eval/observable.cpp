#include <eval/observable.h>
#include <eval/application.h>
#include <eval/dynamic.h>

using namespace richmath;
using namespace pmath;

//{ class Observable ...

static Hashtable<Observable*, Expr>                        all_observers;
static Hashtable<FrontEndReference, Hashset<Observable*>>  all_observed_values;

Observable::Observable()
  : Base()
{
  SET_BASE_DEBUG_TAG(typeid(*this).name());
}

Observable::~Observable() {
  notify_all();
  assert(!all_observers.search(this));
}

void Observable::register_observer() const {
  register_observer(Dynamic::current_evaluation_box_id);
}

void Observable::register_observer(FrontEndReference id) const {
  if(!id)
    return;
  
  auto observed = all_observed_values.search(id);
  if(!observed) {
    all_observed_values.set(id, Hashset<Observable*> {});
    observed = all_observed_values.search(id);
    HASHTABLE_ASSERT(observed != nullptr);
  }
  bool known = observed->search(const_cast<Observable*>(this)) != nullptr;
  if(known)
    return;
  
  observed->add(const_cast<Observable*>(this));
  
  Expr id_obj = id.to_pmath_raw();
  
  Expr *observer_ids = all_observers.search(const_cast<Observable*>(this));
  if(!observer_ids) {
    all_observers.set(const_cast<Observable*>(this), List(id_obj));
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
    auto id = FrontEndReference::from_pmath_raw(my_observers[i]);
    if(id)
      all_observed_values.remove(id);
  }
  
  if(!my_observers.is_null())
    Application::notify(ClientNotification::DynamicUpdate, my_observers);
  
  all_observers.remove(this);
}

//} ... class Observable
