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
      
    public:
      int primary_value() { return _value; }
  };
  
  class GridXIndex : public GridIndex {
    public:
      GridXIndex() = default;
      explicit GridXIndex(int primary_value) : GridIndex(primary_value) {}
      
      int primary_value() { return _value; }
      
      friend bool operator==(GridXIndex left, GridXIndex right) {
        return left._value == right._value;
      }
      friend bool operator!=(GridXIndex left, GridXIndex right) {
        return left._value != right._value;
      }
      friend bool operator<(GridXIndex left, GridXIndex right) {
        return left._value < right._value;
      }
  };
  
  class GridYIndex : public GridIndex {
    public:
      GridYIndex() = default;
      explicit GridYIndex(int primary_value) : GridIndex(primary_value) {}
      
      friend bool operator==(GridYIndex left, GridYIndex right) {
        return left._value == right._value;
      }
      friend bool operator!=(GridYIndex left, GridYIndex right) {
        return left._value != right._value;
      }
      friend bool operator<(GridYIndex left, GridYIndex right) {
        return left._value < right._value;
      }
  };
  
  template<typename T>
  struct GridAxisRange {
    T start;
    T end;
    
    GridAxisRange(T a, T b) : start(a < b ? a : b), end(a < b ? b : a) {}
    
    int primary_length() { return end.primary_value() - start.primary_value() + 1; }
  };
  using GridXRange = GridAxisRange<GridXIndex>;
  using GridYRange = GridAxisRange<GridYIndex>;
  
  struct GridIndexRect {
    GridYRange y;
    GridXRange x;
    
    int rows() { return y.primary_length(); }
    int cols() { return x.primary_length(); }
    
    static GridIndexRect FromYX(const GridYRange &y, const GridXRange &x) {
      return GridIndexRect{y, x};
    }
    
  private:
    GridIndexRect(const GridYRange &_y, const GridXRange &_x) : y(_y), x(_x) {}
  };
  
  class GridItem: public OwnerBox {
      friend class GridBoxImpl;
      friend class GridBox;
    protected:
      virtual ~GridItem();
    public:
      GridBox *grid() { return (GridBox*)_parent; }
      
      virtual float fill_weight() override;
      virtual bool expand(const BoxSize &size) override;
      
      virtual Expr to_pmath_symbol() override { return Expr(); }
      virtual Expr to_pmath(BoxOutputFlags flags) override;
      
      virtual bool try_load_from_object(Expr object, BoxInputFlags options) override;
      void             load_from_object(Expr object, BoxInputFlags options);
      
      bool span_from_left();
      bool span_from_above();
      bool span_from_both();
      bool span_from_any();
      
    protected:
      GridItem();
      virtual void resize_default_baseline(Context *context) override;
      
    protected:
      int _span_right;
      int _span_down;
      bool _really_span_from_left;
      bool _really_span_from_above;
  };
  
  class GridBox: public Box {
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
      
      virtual Box *item(int i) override { return items[i]; }
      virtual int count() override { return items.length(); }
      GridItem *item(int row, int col) { return items.get(row, col); }
      GridItem *item(GridYIndex row, GridXIndex col) { return item(row.primary_value(), col.primary_value()); }
      
      int yx_to_index(GridYIndex y, GridXIndex x) {
        return items.yx_to_index(y.primary_value(), x.primary_value());
      }
      void index_to_yx(int index, GridYIndex *y, GridXIndex *x) {
        int y_primary;
        int x_primary;
        items.index_to_yx(index, &y_primary, &x_primary);
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
      
      virtual bool expand(const BoxSize &size) override;
      virtual void resize(Context *context) override;
      virtual void paint(Context *context) override;
      virtual void selection_path(Canvas *canvas, int start, int end) override;
      
      Box *remove_range(int *start, int end);
      virtual Box *remove(int *index) override;
      
      virtual Expr to_pmath_symbol() override;
      virtual Expr to_pmath(BoxOutputFlags flags) override;
      virtual Expr to_pmath(BoxOutputFlags flags, int start, int end) override;
      
      virtual Box *move_vertical(
        LogicalDirection  direction,
        float            *index_rel_x,
        int              *index,
        bool              called_from_child) override;
        
      virtual VolatileSelection mouse_selection(float x, float y, bool *was_inside_start) override;
        
      virtual void child_transformation(
        int             index,
        cairo_matrix_t *matrix) override;
      
      GridIndexRect get_enclosing_range(int start, int end);
      virtual Box *normalize_selection(int *start, int *end) override;
      
    protected:
      float rowspacing;
      float colspacing;
      
    private:
      void need_pos_vectors();
      void ensure_valid_boxes();
      
    private:
      Matrix<GridItem*> items;
      Array<float> xpos;
      Array<float> ypos;
  };
}

#endif // RICHMATH__BOXES__GRIDBOX_H__INCLUDED
