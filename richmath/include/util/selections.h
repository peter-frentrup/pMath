#ifndef __UTIL__SELECTIONS_H__
#define __UTIL__SELECTIONS_H__


namespace richmath {
  class Box;
  
  class SelectionReference {
    public:
      explicit SelectionReference(int _id = 0, int _start = 0, int _end = 0);
      
      Box *get();
      void set(Box *box, int _start, int _end);
      void set_raw(Box *box, int _start, int _end);
      void reset() { set(0, 0, 0); }
      
      bool equals(Box *box, int _start, int _end) const;
      bool equals(const SelectionReference &other) const;
      
      bool operator==(const SelectionReference &other) const {
        return other.id == id && other.start == start && other.end == end;
      }
      
      bool operator!=(const SelectionReference &other) const {
        return !(*this == other);
      }
      
    public:
      int id;
      int start;
      int end;
  };
}

#endif // __UTIL__SELECTIONS_H__
