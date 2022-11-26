#ifndef RICHMATH__UTIL__INTERVAL_H__INCLUDED
#define RICHMATH__UTIL__INTERVAL_H__INCLUDED

namespace richmath {
  template<typename T>
  struct Interval {
    using value_type = T;
    
    T from;
    T to;
    
    Interval(T from, T to) : from{from}, to{to} {}
    
    template<typename U>
    Interval(const Interval<U> &other) : from(other.from), to(other.to) {}
    
    template<typename U>
    Interval &operator=(const Interval<U> &other) { from = other.from; to = other.to; return *this; }
    
    Interval &operator+=(T val) { from+= val; to+= val; return *this; }
    Interval &operator-=(T val) { from-= val; to-= val; return *this; }
    
    bool is_singleton() const { return from == to; }
    auto length() const -> decltype(to - from) { return to - from; }
    bool contains(T x) const { return from <= x && x <= to; }
    bool contains(const Interval<T> &other) const { return from <= other.from && other.to <= to; }
    T nearest(T x) const { return (from < x) ? ((x < to) ? x : to) : from; }
    void grow_by(T delta) { from -= delta; to += delta; }
    Interval<T> grown_by(T delta) const { auto res = *this; res.grow_by(delta); return res; }
    Interval<T> snap(Interval<T> inner) const;
    Interval<T> intersect(Interval<T> other) const;
    Interval<T> union_hull(Interval<T> other) const;
    Interval<T> singleton_start() const { return {from, from}; }
    Interval<T> singleton_end() const { return {to, to}; }
  };
  
  template<typename T>
  inline Interval<T> Interval<T>::snap(Interval<T> inner) const {
    if(inner.from < from) {
      inner.to = from + inner.length();
      inner.from = from;
      
      if(to < inner.to)
        inner.to = to;
      
      return inner;
    }
    
    if(to < inner.to) {
      inner.from = to - inner.length();
      inner.to = to;
      
      if(inner.from < from)
        inner.from = from;
      
      return inner;
    }
    
    return inner;
  }
  
  template<typename T>
  inline Interval<T> Interval<T>::intersect(Interval<T> other) const {
    if(other.from < from)
      other.from = from;
    if(to < other.to)
      other.to = to;
    return other;
  }
  
  template<typename T>
  inline Interval<T> Interval<T>::union_hull(Interval<T> other) const {
    if(from < other.from)
      other.from = from;
    if(other.to < to)
      other.to = to;
    return other;
  }
}

#endif // RICHMATH__UTIL__INTERVAL_H__INCLUDED
