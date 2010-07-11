#ifndef __BOXES__FRACTIONBOX_H__
#define __BOXES__FRACTIONBOX_H__

#include <boxes/box.h>

namespace richmath{
  class MathSequence;
  
  class FractionBox: public Box{
    public:
      FractionBox();
      FractionBox(MathSequence *num, MathSequence *den);
      ~FractionBox();
      
      MathSequence *numerator(){   return _numerator; }
      MathSequence *denominator(){ return _denominator; }
      
      Box *item(int i);
      int count(){ return 2; }
      
      void resize(Context *context);
      void paint(Context *context);
      
      Box *remove(int *index);
      
      pmath_t to_pmath(bool parseable);
      
      Box *move_vertical(
        LogicalDirection  direction, 
        float            *index_rel_x,
        int              *index);
      
      Box *mouse_selection(
        float x,
        float y,
        int   *start,
        int   *end,
        bool  *eol);
      
      void child_transformation(
        int             index,
        cairo_matrix_t *matrix);
      
    private:
      MathSequence *_numerator;
      MathSequence *_denominator;
      
//      GlyphInfo fraction_glyph;
      float num_y, den_y;
  };
}

#endif // __BOXES__FRACTIONBOX_H__
