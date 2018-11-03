#ifndef __UTIL__SELECTIONS_H__
#define __UTIL__SELECTIONS_H__

#include <util/frontendobject.h>
#include <pmath-cpp.h>

namespace richmath {
  class Box;
  
  class LocationReference {
    public:
      explicit LocationReference();
      explicit LocationReference(Box *_box, int _index);
      explicit LocationReference(FrontEndReference _id, int _index);
      
      explicit operator bool() const { return id.is_valid(); }
      
      Box *get();
      void set_raw(Box *box, int _index);
      void reset() { set_raw(nullptr, 0); }
      
      bool equals(Box *box, int _index) const;
      bool equals(const LocationReference &other) const {
        return *this == other;
      }
      
      bool operator==(const LocationReference &other) const {
        return other.id == id && other.index == index;
      }
      
      bool operator!=(const LocationReference &other) const {
        return !(*this == other);
      }
      
      unsigned int hash() const {
        unsigned int h = 5381;
        h = ((h << 5) + h) + id.hash();
        h = ((h << 5) + h) + (unsigned)index;
        return h;
      }
      
    public:
      FrontEndReference id;
      int index;
  };
  
  class SelectionReference {
    public:
      explicit SelectionReference();
      explicit SelectionReference(Box *_box, int _start, int _end);
      explicit SelectionReference(FrontEndReference _id, int _start, int _end);
      
      explicit operator bool() const { return id.is_valid(); }
      
      Box *get();
      void set(Box *box, int _start, int _end);
      void set_raw(Box *box, int _start, int _end);
      void reset() { set(nullptr, 0, 0); }
      
      bool equals(Box *box, int _start, int _end) const;
      bool equals(const SelectionReference &other) const;
      
      bool operator==(const SelectionReference &other) const {
        return other.id == id && other.start == start && other.end == end;
      }
      
      bool operator!=(const SelectionReference &other) const {
        return !(*this == other);
      }
      
      pmath::Expr to_debug_info() const;
      static SelectionReference from_debug_info(pmath::Expr expr);
      static SelectionReference from_debug_info_of(pmath::Expr expr);
      static SelectionReference from_debug_info_of(pmath_t expr); // does not free expr
      
      LocationReference start_reference() const {
        return LocationReference{id, start};
      }
      LocationReference end_reference() const {
        return LocationReference{id, end};
      }
      
      unsigned int hash() const {
        unsigned int h = 5381;
        h = ((h << 5) + h) + id.hash();
        h = ((h << 5) + h) + (unsigned)start;
        h = ((h << 5) + h) + (unsigned)end;
        return h;
      }
      
    public:
      FrontEndReference id;
      int start;
      int end;
  };
}

#endif // __UTIL__SELECTIONS_H__
