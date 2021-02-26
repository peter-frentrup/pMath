#ifndef RICHMATH__BOXES__GRIDBOX_H__INCLUDED
#define RICHMATH__BOXES__GRIDBOX_H__INCLUDED

#include <boxes/ownerbox.h>
#include <util/matrix.h>


namespace richmath {
  class GridBox;
  
  class GridIndex {
    protected:
      int _value;
      GridIndex() = default;
      explicit GridIndex(int primary_value) : _value(primary_value) {}
      
      friend int operator-(GridIndex left, GridIndex right) { return left._value - right._value; }
      
    public:
      int primary_value() const { return _value; }
  };
  
  class GridXIndex : public GridIndex {
    public:
      GridXIndex() = default;
      explicit GridXIndex(int primary_value) : GridIndex(primary_value) {}
      
      friend bool operator==(GridXIndex left, GridXIndex right) { return left._value == right._value; }
      friend bool operator!=(GridXIndex left, GridXIndex right) { return left._value != right._value; }
      friend bool operator< (GridXIndex left, GridXIndex right) { return left._value <  right._value; }
      friend bool operator<=(GridXIndex left, GridXIndex right) { return left._value <= right._value; }
      friend bool operator> (GridXIndex left, GridXIndex right) { return left._value >  right._value; }
      friend bool operator>=(GridXIndex left, GridXIndex right) { return left._value >= right._value; }
      
      friend GridXIndex operator+(GridXIndex left, int right) { return GridXIndex(left._value + right); }
      friend GridXIndex operator-(GridXIndex left, int right) { return GridXIndex(left._value - right); }
      
      GridXIndex &operator+=(int delta) { return *this = *this + delta; }
      GridXIndex &operator-=(int delta) { return *this = *this - delta; }
      
      GridXIndex &operator++() { return *this+= 1; }
      GridXIndex &operator--() { return *this-= 1; }
      GridXIndex operator++(int) { auto old = *this; *this+= 1; return old; }
      GridXIndex operator--(int) { auto old = *this; *this-= 1; return old; }
  };
  
  class GridYIndex : public GridIndex {
    public:
      GridYIndex() = default;
      explicit GridYIndex(int primary_value) : GridIndex(primary_value) {}
      
      friend bool operator==(GridYIndex left, GridYIndex right) { return left._value == right._value; }
      friend bool operator!=(GridYIndex left, GridYIndex right) { return left._value != right._value; }
      friend bool operator< (GridYIndex left, GridYIndex right) { return left._value <  right._value; }
      friend bool operator<=(GridYIndex left, GridYIndex right) { return left._value <= right._value; }
      friend bool operator> (GridYIndex left, GridYIndex right) { return left._value >  right._value; }
      friend bool operator>=(GridYIndex left, GridYIndex right) { return left._value >= right._value; }
      
      friend GridYIndex operator+(GridYIndex left, int right) { return GridYIndex(left._value + right); }
      friend GridYIndex operator-(GridYIndex left, int right) { return GridYIndex(left._value - right); }
      
      GridYIndex &operator+=(int delta) { return *this = *this + delta; }
      GridYIndex &operator-=(int delta) { return *this = *this - delta; }
      
      GridYIndex &operator++() { return *this+= 1; }
      GridYIndex &operator--() { return *this-= 1; }
      GridYIndex operator++(int) { auto old = *this; *this+= 1; return old; }
      GridYIndex operator--(int) { auto old = *this; *this-= 1; return old; }
  };
  
  template<typename Index>
  class IndexIterator {
    public:
      IndexIterator(Index index) : index(index) {}
      
      friend bool operator==(IndexIterator left, IndexIterator right) { return left.index == right.index; }
      friend bool operator!=(IndexIterator left, IndexIterator right) { return left.index != right.index; }
      
      Index operator*() const { return index; }
      IndexIterator &operator++() { ++index; return *this; }
      IndexIterator &operator--() { --index; return *this; }
      
    private:
      Index index;
  };
  
  template<typename T>
  class GridAxisRange {
    private:
      GridAxisRange(T start, T end) : start(start), end(end) {}
      
    public:
      static GridAxisRange InclusiveHull(T a, T b) { return a <= b ? GridAxisRange(a, b+1) : GridAxisRange(b, a+1); }
      static GridAxisRange Hull(T a, T b) {          return a <= b ? GridAxisRange(a, b) :   GridAxisRange(b, a); }
      
      GridAxisRange &operator+=(int delta) { start+= delta; end+= delta; return *this; }
      GridAxisRange &operator-=(int delta) { start-= delta; end-= delta; return *this; }
      
      int length() const { return end.primary_value() - start.primary_value(); }
      friend bool disjoint(const GridAxisRange &a, const GridAxisRange &b) { return a.end <= b.start || b.end <= a.start; }
      bool contains(const GridAxisRange &other) const { return start <= other.start && other.end <= end; }
      
      friend IndexIterator<T> begin(const GridAxisRange &range) { return range.start; }
      friend IndexIterator<T> end(const GridAxisRange &range) {   return range.end; }
    public:
      T start;
      T end;
  };
  using GridXRange = GridAxisRange<GridXIndex>;
  using GridYRange = GridAxisRange<GridYIndex>;
  
  class GridIndexRect {
    public:
      GridYRange y;
      GridXRange x;
      
      int rows() const { return y.length(); }
      int cols() const { return x.length(); }
      
      static GridIndexRect FromYX(const GridYRange &y, const GridXRange &x) {
        return GridIndexRect{y, x};
      }
      
      bool contains(const GridIndexRect &other) const { return y.contains(other.y) && x.contains(other.x); }
      
    private:
      GridIndexRect(const GridYRange &_y, const GridXRange &_x) : y(_y), x(_x) {}
  };
  
  class GridItem final : public OwnerBox {
      using base = OwnerBox;
      friend class GridBoxImpl;
      friend class GridBox;
    protected:
      virtual ~GridItem();
    public:
      GridBox *grid() { return (GridBox*)parent(); }
      
      virtual float fill_weight() override;
      virtual bool expand(const BoxSize &size) override;
      
      virtual Expr to_pmath_symbol() override { return Expr(); }
      virtual Expr to_pmath(BoxOutputFlags flags) override;
      
      virtual bool try_load_from_object(Expr object, BoxInputFlags options) override;
      void             load_from_object(Expr object, BoxInputFlags options);
      void swap_content(GridItem *other);
      
      bool span_from_left();
      bool span_from_above();
      bool span_from_both();
      bool span_from_any();
      
      int span_right() const { return _span_right; }
      int span_down()  const { return _span_down; }
      
    protected:
      GridItem();
      virtual void resize_default_baseline(Context &context) override;
      
    protected:
      enum {
        ReallySpanFromLeftBit = base::NumFlagsBits,
        ReallySpanFromAboveBit,
        
        NumFlagsBits
      };
      static_assert(NumFlagsBits <= MaximumFlagsBits, "");
      
      bool really_span_from_left() {        return get_flag(ReallySpanFromLeftBit); }
      void really_span_from_left(bool value) {  change_flag(ReallySpanFromLeftBit, value); }
      bool really_span_from_above() {       return get_flag(ReallySpanFromAboveBit); }
      void really_span_from_above(bool value) { change_flag(ReallySpanFromAboveBit, value); }
    
    protected:
      int _span_right;
      int _span_down;
  };
  
  class GridSelectionStrategy {
    public:
      enum class Kind { 
        ContentsOnly,
        ContentsOrGaps,
        ContentsOrColumnGaps,
        ContentsOrRowGaps,
     };
     
     Kind kind;
     int expected_rows;
     int expected_cols;
     
     GridSelectionStrategy(Kind kind);
     GridSelectionStrategy(Kind kind, const GridIndexRect &expected_size);
     GridSelectionStrategy(Kind kind, int expected_rows, int expected_cols);
     
     static GridSelectionStrategy ContentsOnly;
  };
  
  class GridBox final : public Box {
      using base = Box;
      class Impl;
      friend class GridItem;
    protected:
      virtual ~GridBox();
    public:
      GridBox();
      GridBox(int rows, int cols);
      
      // Box::try_create<GridBox>(expr, opts);
      virtual bool try_load_from_object(Expr expr, BoxInputFlags opts) override;
      
      const Array<float> &xpos_array() { need_pos_vectors(); return xpos; }
      const Array<float> &ypos_array() { need_pos_vectors(); return ypos; }
      
      float get_gap_x(GridXIndex col, int gap_side);
      float get_gap_y(GridYIndex row, int gap_side);
      
      /*  A 2x3 grid has 6 items, so count()=6, positions after the last item are the 3*4 corners
          ("gap indices"). Since Box::length() is an allowed position, we get length()=17.
          6-----7-----8-----9
          |  0  |  1  |  2  |
          10---11----12----13
          |  3  |  4  |  5  |
          14---15----16----17
       */

      virtual Box *item(int i) override { return items[i]; }
      virtual int count() override { return items.length(); }
      virtual int length() override { return items.length() + (items.rows() + 1) * (items.cols() + 1) - 1; }
      GridItem *item(int row, int col) { return items.get(row, col); }
      GridItem *item(GridYIndex row, GridXIndex col) { return item(row.primary_value(), col.primary_value()); }
      
      int yx_to_index(GridYIndex y, GridXIndex x) {
        return items.yx_to_index(y.primary_value(), x.primary_value());
      }
      int yx_to_gap_index(GridYIndex y, GridXIndex x) {
        return items.length() + x.primary_value() + y.primary_value() * (items.cols() + 1);
      }
      VolatileSelection selection(const GridIndexRect &rect) {
        return {this, yx_to_index(rect.y.start, rect.x.start), yx_to_index(rect.y.end, rect.x.end)};
      }
      VolatileSelection gap_selection(const GridIndexRect &rect) {
        return {this, yx_to_gap_index(rect.y.start, rect.x.start), yx_to_gap_index(rect.y.end, rect.x.end)};
      }
      void index_to_yx(int index, GridYIndex *y, GridXIndex *x) {
        int y_primary;
        int x_primary;
        if(index < items.length()) {
          items.index_to_yx(index, &y_primary, &x_primary);
        }
        else {
          int i = index - items.length();
          x_primary = i % (items.cols() + 1);
          y_primary = i / (items.cols() + 1);
        }
        *y = GridYIndex{y_primary};
        *x = GridXIndex{x_primary};
      }
      
      int rows() { return items.rows(); }
      int cols() { return items.cols(); }
      
      void insert_rows(GridYIndex yindex, int count) { insert_rows(yindex.primary_value(), count); }
      void insert_cols(GridXIndex xindex, int count) { insert_cols(xindex.primary_value(), count); }
      void insert_rows(int yindex, int count);
      void insert_cols(int xindex, int count);
      
      void remove_rows(GridYIndex yindex, int count) { remove_rows(yindex.primary_value(), count); }
      void remove_cols(GridXIndex xindex, int count) { remove_cols(xindex.primary_value(), count); }
      void remove_rows(int yindex, int count);
      void remove_cols(int xindex, int count);
      
      virtual int child_script_level(int index, const int *opt_ambient_script_level) final override;
      virtual bool expand(const BoxSize &size) override;
      virtual void resize(Context &context) override;
      virtual void paint(Context &context) override;
      virtual void selection_path(Canvas &canvas, int start, int end) override;
      
      Box *remove_range(int *start, int end);
      virtual Box *remove(int *index) override;
      
      virtual Expr to_pmath_symbol() override;
      virtual Expr to_pmath(BoxOutputFlags flags) override;
      virtual Expr to_pmath(BoxOutputFlags flags, int start, int end) override;
      
      virtual Box *move_logical(
        LogicalDirection  direction,
        bool              jumping,
        int              *index) override;
      
      virtual Box *move_vertical(
        LogicalDirection  direction,
        float            *index_rel_x,
        int              *index,
        bool              called_from_child) override;
        
      virtual VolatileSelection mouse_selection(Point pos, bool *was_inside_start) override;
        
      virtual void child_transformation(
        int             index,
        cairo_matrix_t *matrix) override;
      
      GridIndexRect get_enclosing_range(int start, int end);
      virtual VolatileSelection normalize_selection(int start, int end) override;
      
    public:
      static GridSelectionStrategy selection_strategy;
      static GridSelectionStrategy best_selection_strategy_for_drag_source(const VolatileSelection &sel);
      
    private:
      void need_pos_vectors();
      void ensure_valid_boxes();
      
    protected:
      float rowspacing;
      float colspacing;
    
    private:
      Matrix<GridItem*> items;
      Array<float> xpos;
      Array<float> ypos;
  };
}

#endif // RICHMATH__BOXES__GRIDBOX_H__INCLUDED
