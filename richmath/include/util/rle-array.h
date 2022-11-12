#ifndef RICHMATH__UTIL__RLE_ARRAY_H__INCLUDED
#define RICHMATH__UTIL__RLE_ARRAY_H__INCLUDED

#include <util/array.h>

namespace richmath {
  template<typename T>
  struct DefaultValue {
    static T initial() { return value; }
    static const T value;
  };
  
  template<typename T>
  const T DefaultValue<T>::value = T{};
  
  template<typename T>
  struct ConstPredictor {
    static T predict(T initial, int steps) { return initial; }
    
    enum { _debug_predictor_kind = 0 };
  };
  
  template<typename T>
  struct LinearPredictor {
    static T predict(T initial, int steps) { return initial + steps; }
    
    enum { _debug_predictor_kind = 1 };
  };
  
  template<typename T, typename Def=DefaultValue<T>>
  struct EventPredictor {
    using value_default_type = Def;
    
    static T predict(T initial, int steps) { return steps == 0 ? initial : Def::initial(); }
    
    enum { _debug_predictor_kind = 2 };
  };
  
  template<typename T>
  struct RleArrayEntry{
    T next_value;
    int first_index;
  
    friend void swap(RleArrayEntry &left, RleArrayEntry &right) {
      using std::swap;
      
      swap(left.next_value,  right.next_value);
      swap(left.first_index, right.first_index);
    }
  };
  
  /// Iterator through a run-length encoded array (RleArray).
  template<typename A>
  class RleArrayIterator {
      friend A;
      using rle_array_type = A;
      using value_default_type = typename rle_array_type::value_default_type;
      using value_type         = typename rle_array_type::value_type;
      using entry_type         = typename rle_array_type::entry_type;
      using predictor_type     = typename rle_array_type::predictor_type;
    public:
      int index() const { return _index; }
      value_type get() const { return predictor_type::predict(_array->group_start_value(_group), _index - _array->group_start_index(_group)); }
      void reset_rest(value_type val); // invalidates other iterators
      void reset_range(value_type val, int length); // invalidates other iterators
      
      value_type operator*() const { return get(); }
      friend RleArrayIterator operator+(RleArrayIterator left, int steps) { return left+= steps; }
      friend RleArrayIterator operator-(RleArrayIterator left, int steps) { return left-= steps; }
      const RleArrayIterator &operator+=(int steps) { rewind_to(_index + steps); return *this; }
      const RleArrayIterator &operator-=(int steps) { rewind_to(_index - steps); return *this; }
      const RleArrayIterator &operator++() { increment(1); return *this; }
      const RleArrayIterator &operator--() { decrement(1); return *this; }
      
      void increment(int steps) { increment_to(_index + steps); }
      void increment_to(int new_index);
      void decrement(int steps) { increment_to(_index - steps); }
      void decrement_to(int new_index);
      void rewind_to(int new_index);
      
      bool find_next_run(int &next_run_index);
      
      rle_array_type &array() { return *_array; }
      
    private:
      RleArrayIterator(rle_array_type &array, int group, int index) : _array{&array}, _group{group}, _index{index} {
        ARRAY_ASSERT(group >= -1);
        ARRAY_ASSERT(index >= 0);
        ARRAY_ASSERT(group < array.groups.length());
      }
      
    private:
      rle_array_type *_array;
      int _group;
      int _index;
  };
  
  /// A run-length encoded array.
  template<typename T, typename P=ConstPredictor<T>, typename Def=DefaultValue<T>>
  class RleArray {
    public:
      using self_type = RleArray<T, P, Def>;
      using value_default_type = Def;
      using value_type = T;
      using predictor_type = P;
      using entry_type = RleArrayEntry<T>;
      using iterator_type       = RleArrayIterator<self_type>;
      using const_iterator_type = RleArrayIterator<const self_type>;
      
    public:
      iterator_type       begin()       {  return       iterator_type(*this, starts_non_default() ? 0 : -1, 0); }
      const_iterator_type begin() const {  return const_iterator_type(*this, starts_non_default() ? 0 : -1, 0); }
      const_iterator_type cbegin() const { return const_iterator_type(*this, starts_non_default() ? 0 : -1, 0); }
      iterator_type       find(int index)       { return begin() + index; }
      const_iterator_type find(int index) const { return begin() + index; }
      void clear() { groups.length(0); }
    
      T group_start_value(int group) const { return group < 0 ? Def::initial() : groups[group].next_value; }
      int group_start_index(int group) const { return group < 0 ? 0 : groups[group].first_index; }
      
      bool starts_non_default() const { return groups.length() > 0 && groups[0].first_index == 0; }
    
    public:
      Array<entry_type> groups;
  };
  
  template<typename T, typename Def=DefaultValue<T>>
  using RleLinearPredictorArray = RleArray<T, LinearPredictor<T>, Def>;

  template<typename A>
  void RleArrayIterator<A>::increment_to(int new_index) {
    ARRAY_ASSERT(new_index >= _index);
    
    _index = new_index;
    while(_group + 1 < _array->groups.length()) {
      int next_group_start = _array->groups[_group + 1].first_index;
      if(_index < next_group_start)
        break;
      
      ++_group;
    }
  }
  
  template<typename A>
  void RleArrayIterator<A>::decrement_to(int new_index) {
    ARRAY_ASSERT(new_index >= 0);
    
    _index = new_index;
    while(_group >= 0 && _index < _array->groups[_group].first_index)
      --_group;
  }
  
  template<typename A>
  inline void RleArrayIterator<A>::rewind_to(int new_index) {
    if(new_index < _index) {
      //*this = _array->find(new_index);
      decrement_to(new_index);
    }
    else {
      increment_to(new_index);
    }
  }
  
  template<typename A>
  inline bool RleArrayIterator<A>::find_next_run(int &next_run_index) {
    if(_group + 1 < _array->groups.length()) {
      next_run_index = _array->groups[_group + 1].first_index;
      return true;
    }
    else
      return false;
  }
  
  /// Invalidates other iterators for the array.
  template<typename A>
  void RleArrayIterator<A>::reset_rest(value_type val) {
    if(get() == val)
      return;
    
    _array->groups.length(_group + 2);
    ++_group;
    _array->groups[_group] = {PMATH_CPP_MOVE(val), _index };
  }
  
  /// Invalidates other iterators for the array.
  template<typename A>
  void RleArrayIterator<A>::reset_range(value_type val, int length) {
    ARRAY_ASSERT(length >= 0);
    
    if(length == 0)
      return;
    
    auto next = *this + length;
    
    entry_type next_entry = {next.get(), next.index()};
    
    bool remove_current_group = false;
    bool need_new_group = true;
    if(_group >= 0) {
      if(_array->groups[_group].first_index == _index) {
        if(val == predictor_type::predict(_array->group_start_value(_group - 1), _index - _array->group_start_index(_group - 1))) {
          need_new_group = false;
          remove_current_group = true;
        }
        else if(get() == val)
          need_new_group = false;
      }
      else if(get() == val)
        need_new_group = false;
    }
    else if(get() == val)
      need_new_group = false;
    
    bool need_group_after = true;
    if(predictor_type::predict(val, length) == next_entry.next_value)
      need_group_after = false;
    
    if(need_new_group)
      ++_group;
    
    Range garbage {
      remove_current_group ? _group : _group + 1, 
      need_group_after ? next._group - 1 : next._group
    };
    if(garbage.end < garbage.start - 1) {
      _array->groups.insert(garbage.start - 1, garbage.start - 1 - garbage.end);
      garbage.end = garbage.start - 1;
    }
    
    if(need_new_group) 
      _array->groups.set(_group, {PMATH_CPP_MOVE(val), _index});
    
    if(need_group_after)
      _array->groups.set(garbage.end + 1, PMATH_CPP_MOVE(next_entry));
    
    if(garbage.start <= garbage.end)
      _array->groups.remove(garbage);
  }

#ifdef NDEBUG
#  define debug_test_rle_array()  ((void)0)
#else
  void debug_test_rle_array();
#endif
}

#endif // RICHMATH__UTIL__RLEARRAY_H__INCLUDED
