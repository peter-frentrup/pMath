#ifndef __BOXES__GRIDBOX_H__
#define __BOXES__GRIDBOX_H__

#include <boxes/ownerbox.h>
#include <util/matrix.h>

namespace richmath{
  class GridBox;
  
  class GridItem: public OwnerBox{
    friend class GridBox;
    public:
      virtual ~GridItem();
      
      GridBox *grid(){ return (GridBox*)_parent; }
      
      virtual bool expand(const BoxSize &size);
      virtual void resize(Context *context);
      virtual void paint(Context *context);
      
      virtual Expr to_pmath(bool parseable);
      
      virtual Box *mouse_selection(
        float  x,
        float  y,
        int   *start,
        int   *end,
        bool  *was_inside_start);
      
      virtual void child_transformation(
        int             index,
        cairo_matrix_t *matrix);
        
      virtual void load_from_object(const Expr object, int opts);
      
      bool span_from_left();
      bool span_from_above();
      bool span_from_both();
      bool span_from_any();
      
    protected:
      GridItem();
    
    protected:
      int _span_right;
      int _span_down;
      bool _really_span_from_left;
      bool _really_span_from_above;
      
  };
  
  class GridBox: public Box {
    friend class GridItem;
    public:
      GridBox();
      GridBox(int rows, int cols);
      virtual ~GridBox();
      
      static GridBox *create(Expr expr, int opts);
      
      const Matrix<GridItem*> &matrix(){ return items; }
      const Array<float> &xpos_array(){ need_pos_vectors(); return xpos; }
      const Array<float> &ypos_array(){ need_pos_vectors(); return ypos; }
      
      virtual Box *item(int i){ return items[i]; }
      virtual int count(){ return items.length(); }
      GridItem *item(int row, int col){ return items.get(row, col); }
      
      int rows(){ return items.rows(); }
      int cols(){ return items.cols(); }
      
      void insert_rows(int yindex, int count);
      void insert_cols(int xindex, int count);
      
      void remove_rows(int yindex, int count);
      void remove_cols(int xindex, int count);
      
      virtual void resize(Context *context);
      virtual void paint(Context *context);
      virtual void selection_path(Canvas *canvas, int start, int end);
      
      Box *remove_range(int *start, int end);
      virtual Box *remove(int *index);
      
      virtual Expr to_pmath(bool parseable);
      virtual Expr to_pmath(bool parseable, int start, int end);
      
      virtual Box *move_vertical(
        LogicalDirection  direction, 
        float            *index_rel_x,
        int              *index);
      
      virtual Box *mouse_selection(
        float  x,
        float  y,
        int   *start,
        int   *end,
        bool  *was_inside_start);
        
      virtual void child_transformation(
        int             index,
        cairo_matrix_t *matrix);
        
      virtual Box *normalize_selection(int *start, int *end);
    
    protected:
      float rowspacing;
      float colspacing;
    
    private:
      void need_pos_vectors();
      
      int resize_items(Context *context);
      
      void simple_spacing(float em);
      
    private:
      Matrix<GridItem*> items;
      Array<float> xpos;
      Array<float> ypos;
  };
}

#endif // __BOXES__GRIDBOX_H__
