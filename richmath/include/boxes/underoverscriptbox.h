#ifndef __BOXES__UNDEROVERSCRIPTBOX_H__
#define __BOXES__UNDEROVERSCRIPTBOX_H__

#include <boxes/box.h>

namespace richmath{
  class MathSequence;
  
  class UnderoverscriptBox: public Box{
    public:
      UnderoverscriptBox(MathSequence *base, MathSequence *under, MathSequence *over);
      virtual ~UnderoverscriptBox();
      
      MathSequence *base(){        return _base; }
      MathSequence *underscript(){ return _underscript; }
      MathSequence *overscript(){  return _overscript; }
      
      virtual Box *item(int i);
      virtual int count();
      
      virtual void resize(Context *context);
      void after_items_resize(Context *context);
      virtual void colorize_scope(SyntaxState *state);
      virtual void paint(Context *context);
      
      virtual Box *remove(int *index);
      
      virtual void complete();
      
      virtual Expr to_pmath_symbol();
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
        cairo_matrix_t *matrixn);
      
    private:
      MathSequence *_base;
      MathSequence *_underscript;
      MathSequence *_overscript;
      
      float base_x, under_x, under_y, over_x, over_y;
      
//      float ou_displacement;
      bool o_stretched, u_stretched;
  };
}

#endif // __BOXES__UNDEROVERSCRIPTBOX_H__
