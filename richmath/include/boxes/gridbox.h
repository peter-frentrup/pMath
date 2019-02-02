#ifndef RICHMATH__BOXES__GRIDBOX_H__INCLUDED
#define RICHMATH__BOXES__GRIDBOX_H__INCLUDED

#include <boxes/ownerbox.h>
#include <util/matrix.h>


namespace richmath {
  class GridBox;
  
  class GridItem: public OwnerBox {
      friend class GridBoxImpl;
      friend class GridBox;
    public:
      virtual ~GridItem();
      
      GridBox *grid() { return (GridBox*)_parent; }
      
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
      friend class GridBoxImpl;
      friend class GridItem;
    public:
      GridBox();
      GridBox(int rows, int cols);
      virtual ~GridBox();
      
      // Box::try_create<GridBox>(expr, opts);
      virtual bool try_load_from_object(Expr expr, BoxInputFlags opts) override;
      
      const Matrix<GridItem*> &matrix() { return items; }
      const Array<float> &xpos_array() { need_pos_vectors(); return xpos; }
      const Array<float> &ypos_array() { need_pos_vectors(); return ypos; }
      
      virtual Box *item(int i) override { return items[i]; }
      virtual int count() override { return items.length(); }
      GridItem *item(int row, int col) { return items.get(row, col); }
      
      int rows() { return items.rows(); }
      int cols() { return items.cols(); }
      
      void insert_rows(int yindex, int count);
      void insert_cols(int xindex, int count);
      
      void remove_rows(int yindex, int count);
      void remove_cols(int xindex, int count);
      
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
        
      virtual Box *mouse_selection(
        float  x,
        float  y,
        int   *start,
        int   *end,
        bool  *was_inside_start) override;
        
      virtual void child_transformation(
        int             index,
        cairo_matrix_t *matrix) override;
        
      virtual Box *normalize_selection(int *start, int *end) override;
      
    protected:
      float rowspacing;
      float colspacing;
      
    private:
      void need_pos_vectors();
      
    private:
      Matrix<GridItem*> items;
      Array<float> xpos;
      Array<float> ypos;
  };
}

#endif // RICHMATH__BOXES__GRIDBOX_H__INCLUDED
