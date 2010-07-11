#ifndef __BOXES__RADICALBOX_H__
#define __BOXES__RADICALBOX_H__

#include <boxes/box.h>

namespace richmath{
  class MathSequence;
  
  class RadicalBox: public Box{
    public:
      RadicalBox(MathSequence *radicand = 0, MathSequence *exponent = 0);
      ~RadicalBox();
      
      MathSequence *radicand(){ return _radicand; }
      MathSequence *exponent(){ return _exponent; }
      
      Box *item(int i);
      int count();
      
      void resize(Context *context);
      void paint(Context *context);
      
      Box *remove(int *index);
      
      void complete();
      
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
      MathSequence *_radicand;
      MathSequence *_exponent;
      
      RadicalShapeInfo info;
      float small_em;
      float rx;
      float ex;
      float ey;
  };
}

#endif // __BOXES__RADICALBOX_H__
