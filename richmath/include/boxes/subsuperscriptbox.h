#ifndef __BOXES__SUBSUPERSCRIPTBOX_H__
#define __BOXES__SUBSUPERSCRIPTBOX_H__

#include <boxes/box.h>


namespace richmath {
  class MathSequence;
  
  class SubsuperscriptBox: public Box {
    public:
      SubsuperscriptBox();
      SubsuperscriptBox(MathSequence *sub, MathSequence *super);
      virtual ~SubsuperscriptBox();
      
      // Box::try_create<SubsuperscriptBox>(expr, opts);
      virtual bool try_load_from_object(Expr expr, int opts);
      
      MathSequence *subscript() {   return _subscript; }
      MathSequence *superscript() { return _superscript; }
      
      virtual Box *item(int i);
      virtual int count();
      
      virtual void resize(Context *context);
      virtual void stretch(Context *context, const BoxSize &base);
      virtual void adjust_x(
        Context           *context,
        uint16_t           base_char,
        const GlyphInfo   &base_info);
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
        float x,
        float y,
        int   *start,
        int   *end,
        bool  *was_inside_start);
        
      virtual void child_transformation(
        int             index,
        cairo_matrix_t *matrix);
        
    private:
      MathSequence *_subscript;
      MathSequence *_superscript;
      
      float sub_x, super_x;
      float em, sub_y, super_y;
  };
}

#endif // __BOXES__SUBSUPERSCRIPTBOX_H__
