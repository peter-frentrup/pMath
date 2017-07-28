#ifndef __BOXES__UNDEROVERSCRIPTBOX_H__
#define __BOXES__UNDEROVERSCRIPTBOX_H__

#include <boxes/box.h>


namespace richmath {
  class MathSequence;
  
  class UnderoverscriptBox: public Box {
    public:
      UnderoverscriptBox();
      UnderoverscriptBox(MathSequence *base, MathSequence *under, MathSequence *over);
      virtual ~UnderoverscriptBox();
      
      // Box::try_create<UnderoverscriptBox>(expr, opts);
      virtual bool try_load_from_object(Expr expr, int opts) override;
      
      MathSequence *base() {        return _base; }
      MathSequence *underscript() { return _underscript; }
      MathSequence *overscript() {  return _overscript; }
      
      virtual Box *item(int i) override;
      virtual int count() override;
      
      virtual void resize(Context *context) override;
      void after_items_resize(Context *context);
      virtual void colorize_scope(SyntaxState *state) override;
      virtual void paint(Context *context) override;
      
      virtual Box *remove(int *index) override;
      
      void complete();
      
      virtual Expr to_pmath_symbol() override;
      virtual Expr to_pmath(BoxFlags flags) override;
      
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
        cairo_matrix_t *matrixn) override;
        
    private:
      MathSequence *_base;
      MathSequence *_underscript;
      MathSequence *_overscript;
      
      float base_x, under_x, under_y, over_x, over_y;
      
//      float ou_displacement;
      bool _overscript_is_stretched;
      bool _underscript_is_stretched;
  };
}

#endif // __BOXES__UNDEROVERSCRIPTBOX_H__
