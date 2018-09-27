#include <eval/observable.h>
#include <eval/application.h>
#include <eval/dynamic.h>

using namespace richmath;
using namespace pmath;

//{ class Observable ...

static Hashtable<Observable*, Expr, cast_hash>                             all_observers;
static Hashtable<int, Hashtable<Observable*, Void, cast_hash>, cast_hash>  all_observed_values;

Observable::Observable()
  : Base()
{
  SET_BASE_DEBUG_TAG(typeid(*this).name());
}

Observable::~Observable() {
  auto my_observers = all_observers[this];
  all_observers.remove(this);
  
  for(size_t i = my_observers.expr_length(); i > 0; --i) {
    auto item = my_observers[i];
    int id;
    if(item.is_int32())
      id = PMATH_AS_INT32(item.get());
    else
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

void Observable::register_observer(int id) {
  if(!id)
    return;
  
  auto observed = all_observed_values.search(id);
  if(!observed) {
    all_observed_values.set(id, Hashtable<Observable*, Void, cast_hash> {});
    observed = all_observed_values.search(id);
    HASHTABLE_ASSERT(observed != nullptr);
  }
  bool known = observed->search(this) != nullptr;
  if(known)
    return;
  
  observed->set(this, Void{});
  
  Expr id_obj {id};
  
  Expr *observer_ids = all_observers.search(this);
  if(!observer_ids) {
    all_observers.set(this, List(id_obj));
    return;
  }
  observer_ids->append(id_obj);
}

void Observable::unregister_oberserver(int id) {
  auto observed = all_observed_values.search(id);
  if(!observed)
    return;
    
  Expr id_obj {id};
  
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
    auto item = my_observers[i];
    int id;
    if(item.is_int32())
      id = PMATH_AS_INT32(item.get());
    else
      continue;
      
    all_observed_values.remove(id);
  }
  
  Application::notify(ClientNotification::DynamicUpdate, my_observers);
  all_observers.remove(this);
}

//} ... class Observable
