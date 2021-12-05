#ifndef RICHMATH__UTIL__SELECTIONS_H__INCLUDED
#define RICHMATH__UTIL__SELECTIONS_H__INCLUDED

#include <util/frontendobject.h>
#include <pmath-cpp.h>

namespace richmath {
  class Box;
  class Canvas;
  enum class BoxOutputFlags;
  
  /** Represents a weak reference to a position inside a Box.
      
      Weak means that when the Box gets destroyed, this object will notice and give 
      nullptr instead of a dangling reference.
   */
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
  
  /** Represents a (volatile) reference to a position inside a Box.
  
      Volatile means that this object is only valid as long as the Box is valid.
      When the Box gets destroyed, this object holds a dangling reference without the possibility
      to detect the destruction of the Box. 
      
      \see AutoMemorySuspension
      \see Box::safe_destroy()
   */
  struct VolatileSelection {
    Box *box;
    int  start;
    int  end;
    
    VolatileSelection() : box(nullptr), start(0), end(0) {}
    VolatileSelection(Box *box, int index) : box(box), start(index), end(index) {}
    VolatileSelection(Box *box, int start, int end) : box(box), start(start), end(end) {}
    
    VolatileSelection start_only() const { return VolatileSelection(box, start, start); }
    VolatileSelection end_only() const { return VolatileSelection(box, end, end); }
    
    int length() { return end - start; }
    
    explicit operator bool() const { return box != nullptr; }
    bool is_empty() const { return !box || start == end; }
    
    bool visually_contains(VolatileSelection other) const;
    bool logically_contains(VolatileSelection other) const;
    bool directly_contains(const VolatileSelection &other) const { return box == other.box && start <= other.start && other.end <= end; }
    bool same_box_but_disjoint(const VolatileSelection &other) const { return box == other.box && (other.end <= start || end <= other.start); }
    
    bool null_or_selectable() const;
    bool selectable() const;
    bool is_name() const;
    bool is_inside_string() const;
    
    Box *contained_box() const;
        
    bool operator==(const VolatileSelection &other) const {
      return other.box == box && other.start == start && other.end == end;
    }
    
    bool operator!=(const VolatileSelection &other) const {
      return !(*this == other);
    }
    
    pmath::Expr to_pmath(BoxOutputFlags flags) const;
    
    void add_path(Canvas &canvas) const;
    
    void expand();
    void expand_to_parent();
    void expand_up_to_sibling(const VolatileSelection &sibling, int max_steps = INT_MAX);
    void normalize();
    void dynamic_to_literal();
    
    PMATH_ATTRIBUTE_USE_RESULT VolatileSelection expanded() const {
      VolatileSelection ret = *this;
      ret.expand();
      return ret; 
    }
    PMATH_ATTRIBUTE_USE_RESULT VolatileSelection expanded_to_parent() const {
      VolatileSelection ret = *this;
      ret.expand_to_parent();
      return ret;
    }
    PMATH_ATTRIBUTE_USE_RESULT VolatileSelection expanded_up_to_sibling(const VolatileSelection &sibling, int max_steps = INT_MAX) const {
      VolatileSelection ret = *this;
      ret.expand_up_to_sibling(sibling, max_steps);
      return ret;
    }
    PMATH_ATTRIBUTE_USE_RESULT VolatileSelection normalized() const {
      VolatileSelection ret = *this;
      ret.normalize();
      return ret;
    }
  };
  
  /** Represents a weak reference to a range of positions inside a Box.
      
      Weak means that when the Box gets destroyed, this object will notice and give 
      nullptr instead of a dangling reference.
   */
  class SelectionReference {
    public:
      explicit SelectionReference();
      explicit SelectionReference(const VolatileSelection &box_with_range) : SelectionReference(box_with_range.box, box_with_range.start, box_with_range.end) {}
      explicit SelectionReference(Box *box, int start, int end);
      explicit SelectionReference(FrontEndReference id, int start, int end);
      
      explicit operator bool() const { return id.is_valid(); }
      
      Box *get(); // may change the stored start and end fields
      VolatileSelection get_all(); // may change the stored start and end fields
      
      void set(const VolatileSelection &box_with_range) { set(box_with_range.box, box_with_range.start, box_with_range.end); }
      void set(Box *new_box, int new_start, int new_end);
      void set_raw(const VolatileSelection &box_with_range) { set_raw(box_with_range.box, box_with_range.start, box_with_range.end); }
      void set_raw(Box *new_box, int new_start, int new_end);
      void reset() { set(nullptr, 0, 0); }
      
      int length() const { return end - start; }
      
      // TODO: GridBox needs special handling before and after the edit.
      void move_after_edit(const SelectionReference &before_edit, const SelectionReference &after_edit);
      
      bool equals(Box *other_box, int other_start, int other_end) const;
      bool equals(const VolatileSelection &other) const { return equals(other.box, other.start, other.end); }
      bool equals(const SelectionReference &other) const {
        return other.id == id && other.start == start && other.end == end;
      }
      
      bool operator==(const VolatileSelection &other) const { return equals(other); }
      bool operator!=(const VolatileSelection &other) const { return !equals(other); }
      
      bool operator==(const SelectionReference &other) const { return equals(other); }
      bool operator!=(const SelectionReference &other) const { return !equals(other); }
      
      int cmp_lexicographic(const SelectionReference &other) const;
      friend bool operator<(const SelectionReference &left, const SelectionReference &right) {
        return left.cmp_lexicographic(right) < 0;
      }
      friend bool operator<=(const SelectionReference &left, const SelectionReference &right) {
        return left.cmp_lexicographic(right) <= 0;
      }
      friend bool operator>(const SelectionReference &left, const SelectionReference &right) {
        return left.cmp_lexicographic(right) > 0;
      }
      friend bool operator>=(const SelectionReference &left, const SelectionReference &right) {
        return left.cmp_lexicographic(right) >= 0;
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
  
  extern int box_depth(Box *box);
  extern int box_order(Box *b1, int i1, Box *b2, int i2);
}

#endif // RICHMATH__UTIL__SELECTIONS_H__INCLUDED
