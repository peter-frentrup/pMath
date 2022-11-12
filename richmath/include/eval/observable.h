#ifndef __RICHMATHRICHMATH__EVAL__OBSERVABLE_H__INCLUDED
#define __RICHMATHRICHMATH__EVAL__OBSERVABLE_H__INCLUDED

#include <util/frontendobject.h>
#include <utility>

namespace richmath {
  class Observable: public Base {
    public:
      Observable();
      ~Observable();
      Observable(Observable &&src);
      Observable &operator=(Observable &&src);
      
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
      using self_t = ObservableValue<T>;
    public:
      ObservableValue() = default;
      explicit ObservableValue(const T &value) : Observable(), _value(value) {}
      explicit ObservableValue(T &&value) : Observable(), _value(PMATH_CPP_MOVE(value)) {}
      
      bool unobserved_equals(const T &other) {
        return _value == other;
      }
      
      T get() const {
        register_observer(); 
        return _value; 
      }
      
      operator T() const {
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
          _value = PMATH_CPP_MOVE(new_value);
          notify_all();
        }
        return _value;
      }
      
    private:
      T _value;
  };
  
  class Observatory {
    public:
      static void shutdown();
  };
  
  // http://en.wikibooks.org/wiki/More_C%2B%2B_Idioms/Nifty_Counter
  static struct ObservatoryInitializer {
    ObservatoryInitializer();
    ~ObservatoryInitializer();
  } TheObservatoryInitializer;
}

#endif // __RICHMATHRICHMATH__EVAL__OBSERVABLE_H__INCLUDED
