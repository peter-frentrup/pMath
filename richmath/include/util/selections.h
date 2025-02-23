#ifndef RICHMATH__UTIL__SELECTIONS_H__INCLUDED
#define RICHMATH__UTIL__SELECTIONS_H__INCLUDED

#include <util/frontendobject.h>
#include <pmath-cpp.h>

namespace richmath {
  class Box;
  class Canvas;
  class RectangleF;
  class Point;
  enum class BoxOutputFlags;
  enum class SelectionDisplayFlags;
  
  enum class LogicalDirection {
    Forward,
    Backward
  };
  
  inline LogicalDirection opposite_direction(LogicalDirection dir) { return (dir == LogicalDirection::Forward) ? LogicalDirection::Backward : LogicalDirection::Forward; }
  
  /** Represents a (volatile) reference to a single position inside a Box.
  
      Volatile means that this object is only valid as long as the Box is valid.
      When the Box gets destroyed, this object holds a dangling reference without the possibility
      to detect the destruction of the Box. 
      
      \see AutoMemorySuspension
      \see Box::safe_destroy()
   */
  struct VolatileLocation {
    Box *box;
    int  index;
    
    VolatileLocation(): box(nullptr), index(0) {}
    VolatileLocation(Box *box, int index) : box(box), index(index) {}
    
    explicit operator bool() const { return box != nullptr; }
    
    bool operator==(const VolatileLocation &other) const { 
      return other.box == box && other.index == index;
    }
    
    bool operator!=(const VolatileLocation &other) const {
      return !(*this == other);
    }
    
    friend int document_order(VolatileLocation left, VolatileLocation right);
    
    bool selectable() const;
    bool exitable() const;
    bool selection_exitable() const;
    
    VolatileLocation parent() const { return parent(LogicalDirection::Backward); }
    VolatileLocation parent(LogicalDirection direction) const;
    
    VolatileLocation move_logical(        LogicalDirection direction, bool jumping) const; 
    void             move_logical_inplace(LogicalDirection direction, bool jumping); 
    
    VolatileLocation move_vertical(        LogicalDirection direction, float *index_rel_x) const;
    void             move_vertical_inplace(LogicalDirection direction, float *index_rel_x);
    
    bool find_next(String string, bool complete_token, const VolatileLocation &stop);
    bool find_selection_placeholder(LogicalDirection direction, const VolatileLocation &stop, bool stop_early = false);
  };
  
  /** Represents a (volatile) reference to a position range inside a Box.
  
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
    VolatileSelection(const VolatileLocation &loc) : box(loc.box), start(loc.index), end(loc.index) {}
    VolatileSelection(Box *box, int index) : box(box), start(index), end(index) {}
    VolatileSelection(Box *box, int start, int end) : box(box), start(start), end(end) {}

    static VolatileSelection all_of(Box *box);
    
    VolatileLocation start_only() const { return VolatileLocation(box, start); }
    VolatileLocation end_only() const {   return VolatileLocation(box, end); }
    VolatileLocation start_end_only(LogicalDirection dir) const {
      return VolatileLocation(box, (dir == LogicalDirection::Forward) ? end : start ); 
    }
    
    int length() const { return end - start; }
    
    explicit operator bool() const { return box != nullptr; }
    bool is_empty() const { return !box || start == end; }
    
    bool visually_contains(VolatileSelection other) const;
    bool logically_contains(VolatileSelection other) const;
    bool directly_contains(const VolatileSelection &other) const { return box == other.box && start <= other.start && other.end <= end; }
    bool same_box_but_disjoint(const VolatileSelection &other) const { return box == other.box && (other.end <= start || end <= other.start); }
    bool request_repaint() const;
    
    bool null_or_selectable() const;
    bool selectable() const;
    bool is_name() const;
    bool is_inside_string() const;
    
    Box *contained_box() const;

    /// Convert to a single token or SyntaxForm. NULL if not a single token.
    String syntax_form() const;
    
    bool operator==(const VolatileSelection &other) const { 
      return other.box == box && other.start == start && other.end == end;
    }
    
    bool operator!=(const VolatileSelection &other) const {
      return !(*this == other);
    }
    
    pmath::Expr to_pmath(BoxOutputFlags flags) const;
    
    void add_path(Canvas &canvas) const;
    void add_rectangles(Array<RectangleF> &rects, SelectionDisplayFlags flags, Point p0) const;
    
    void move(const VolatileSelection &del, int ins_len);
    
    void expand();
    void expand_to_parent();
    void expand_up_to_sibling(const VolatileSelection &sibling, int max_steps = INT_MAX);
    void expand_nearby_placeholder(float *index_rel_x);
    
    void expand_to_cover(VolatileSelection &other, bool restrict_from_exits = true);
    
    void normalize();
    void dynamic_to_literal();
    
    VolatileLocation find_selection_placeholder(LogicalDirection direction, bool stop_early = false);
    
    PMATH_ATTRIBUTE_USE_RESULT VolatileSelection moved(const VolatileSelection &del, int ins_len) const {
      VolatileSelection ret = *this;
      ret.move(del, ins_len);
      return ret; 
    }
    
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
  
  /** Represents a weak reference to a position inside a Box.
      
      Weak means that when the Box gets destroyed, this object will notice and give 
      nullptr instead of a dangling reference.
   */
  class LocationReference {
    public:
      explicit LocationReference();
      explicit LocationReference(const VolatileLocation &box_width_index) : LocationReference(box_width_index.box, box_width_index.index) {}
      explicit LocationReference(Box *_box, int _index);
      explicit LocationReference(FrontEndReference _id, int _index);
      
      explicit operator bool() const { return id.is_valid(); }
      
      Box *get();
      VolatileLocation get_all(); // may change the stored start and end fields
      void set_raw(const VolatileLocation &box_with_index) { set_raw(box_with_index.box, box_with_index.index); }
      void set_raw(Box *box, int _index);
      void reset() { set_raw(nullptr, 0); }
      
      bool equals(const VolatileLocation &box_with_index) const { return equals(box_with_index.box, box_with_index.index); }
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
      explicit SelectionReference(const LocationReference &loc) : SelectionReference(loc.id, loc.index, loc.index) {}
      
      explicit operator bool() const { return id.is_valid(); }
      
      Box *get(); // may change the stored start and end fields
      VolatileSelection get_all(); // may change the stored start and end fields
      
      void set(const VolatileLocation  &box_with_index) { set(box_with_index.box, box_with_index.index, box_with_index.index); }
      void set(const VolatileSelection &box_with_range) { set(box_with_range.box, box_with_range.start, box_with_range.end); }
      void set(Box *new_box, int new_start, int new_end);
      void set_raw(const VolatileLocation  &box_with_index) { set_raw(box_with_index.box, box_with_index.index, box_with_index.index); }
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
      
      pmath::Expr to_pmath() const;
      static SelectionReference from_pmath(pmath::Expr expr);
      static SelectionReference from_debug_metadata_of(pmath::Expr expr);
      static SelectionReference from_debug_metadata_of(pmath_t expr); // does not free expr
      
      LocationReference start_reference() const { return LocationReference{id, start}; }
      LocationReference end_reference() const {   return LocationReference{id, end};   }
      LocationReference start_end_reference(LogicalDirection dir) const {
        return LocationReference{id, (dir == LogicalDirection::Forward) ? end : start };
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

#endif // RICHMATH__UTIL__SELECTIONS_H__INCLUDED
