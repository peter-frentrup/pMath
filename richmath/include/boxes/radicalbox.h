#ifndef __BOXES__RADICALBOX_H__
#define __BOXES__RADICALBOX_H__

#include <boxes/box.h>

namespace richmath{
  class MathSequence;
  
  class RadicalBox: public Box{
    public:
      RadicalBox(MathSequence *radicand = 0, MathSequence *exponent = 0);
      virtual ~RadicalBox();
      
      MathSequence *radicand(){ return _radicand; }
      MathSequence *exponent(){ return _exponent; }
      
      virtual Box *item(int i);
      virtual int count();
      
      virtual void resize(Context *context);
      virtual void paint(Context *context);
      
      virtual Box *remove(int *index);
      
      void complete();
      
      virtual Expr to_pmath(bool parseable);
      
      virtual Box *move_vertical(
        LogicalDirection  direction, 
        float            *index_rel_x,
        int              *index);
      
      virtual Box *mouse_selection(
        float x,
        float y,
        int   *start,
        int   *end,
        bool  *eol);
        
      virtual void child_transformation(
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
