#ifndef __RICHMATH__EVAL__OBSERVABLE_H__
#define __RICHMATH__EVAL__OBSERVABLE_H__

#include <util/base.h>
#include <utility>

namespace richmath {
  class Observable: public Base {
    public:
      Observable();
      ~Observable();
      
      void register_observer();
      void register_observer(int id);
      static void unregister_oberserver(int id);
      void notify_all();
  };
  
  template<class T>
  class ObservableValue : public Observable {
    private:
      typedef ObservableValue<T> self_t;
    public:
      ObservableValue() = default;
      explicit ObservableValue(const T &value) : Observable(), _value(value) {}
      explicit ObservableValue(T &&value) : Observable(), _value(std::move(value)) {}
      
      operator T() {
        register_observer(); 
        return _value; 
      }
      
      const T &operator=(const T &new_value) {
        _value = new_value;
        notify_all();
        return _value;
      }
      
      const T &operator=(T &&new_value) {
        _value = std::move(new_value);
        notify_all();
        return _value;
      }
      
    private:
      T _value;
  };
}

#endif // __RICHMATH__EVAL__OBSERVABLE_H__
