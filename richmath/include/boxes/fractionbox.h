#ifndef __BOXES__FRACTIONBOX_H__
#define __BOXES__FRACTIONBOX_H__

#include <boxes/box.h>


namespace richmath {
  class MathSequence;
  
  class FractionBox: public Box {
    public:
      FractionBox();
      FractionBox(MathSequence *num, MathSequence *den);
      virtual ~FractionBox();
      
      // Box::try_create<FractionBox>(expr, opts);
      virtual bool try_load_from_object(Expr expr, int opts);
      
      MathSequence *numerator() {   return _numerator; }
      MathSequence *denominator() { return _denominator; }
      
      virtual Box *item(int i);
      virtual int count() { return 2; }
      
      virtual void resize(Context *context);
      virtual void paint(Context *context);
      
      virtual Box *remove(int *index);
      
      virtual Expr to_pmath_symbol() { return Symbol(PMATH_SYMBOL_FRACTIONBOX); }
      virtual Expr to_pmath(int flags);
      
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
        
    private:
      MathSequence *_numerator;
      MathSequence *_denominator;
      
      float num_y, den_y;
  };
}

#endif // __BOXES__FRACTIONBOX_H__
