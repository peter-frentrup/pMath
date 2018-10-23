#ifndef __UTIL__SELECTIONS_H__
#define __UTIL__SELECTIONS_H__

#include <util/frontendobject.h>
#include <pmath-cpp.h>

namespace richmath {
  class Box;
  
  class SelectionReference {
    public:
      explicit SelectionReference();
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
      
    public:
      FrontEndReference id;
      int start;
      int end;
  };
}

#endif // __UTIL__SELECTIONS_H__
