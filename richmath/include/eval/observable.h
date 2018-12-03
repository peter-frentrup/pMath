#ifndef __RICHMATHRICHMATH__EVAL__OBSERVABLE_H__INCLUDED
#define __RICHMATHRICHMATH__EVAL__OBSERVABLE_H__INCLUDED

#include <util/frontendobject.h>
#include <utility>

namespace richmath {
  class Observable: public Base {
    public:
      Observable();
      ~Observable();
      
      void register_observer() const;
      void register_observer(FrontEndReference id) const;
      static void unregister_oberserver(FrontEndReference id);
      void notify_all();
  };
  
  /** A variable, whose changes are observable by Dynamic() objects
   */
  template<class T>
  class ObservableValue : public Observable {
    private:
      typedef ObservableValue<T> self_t;
    public:
      ObservableValue() = default;
      explicit ObservableValue(const T &value) : Observable(), _value(value) {}
      explicit ObservableValue(T &&value) : Observable(), _value(std::move(value)) {}
      
      bool unobserved_equals(const T &other) {
        return _value == other;
      }
      
      operator T() {
        register_observer(); 
        return _value; 
      }
      
      const T &operator=(const T &new_value) {
        if(_value != new_value) {
          _value = new_value;
          notify_all();
        }
        return _value;
      }
      
      const T &operator=(T &&new_value) {
        if(_value != new_value) {
          _value = std::move(new_value);
          notify_all();
        }
        return _value;
      }
      
    private:
      T _value;
  };
}

#endif // __RICHMATHRICHMATH__EVAL__OBSERVABLE_H__INCLUDED
