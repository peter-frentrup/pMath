#ifndef RICHMATH__BOXES__FRACTIONBOX_H__INCLUDED
#define RICHMATH__BOXES__FRACTIONBOX_H__INCLUDED

#include <boxes/box.h>


namespace richmath {
  class MathSequence;
  
  class FractionBox final : public Box {
    protected:
      virtual ~FractionBox();
    public:
      FractionBox();
      FractionBox(MathSequence *num, MathSequence *den);
      
      // Box::try_create<FractionBox>(expr, opts);
      virtual bool try_load_from_object(Expr expr, BoxInputFlags opts) override;
      
      MathSequence *numerator() {   return _numerator; }
      MathSequence *denominator() { return _denominator; }
      
      virtual Box *item(int i) override;
      virtual int count() override { return 2; }
      
      virtual int child_script_level(int index, const int *opt_ambient_script_level) final override;
      virtual void resize(Context &context) override;
      virtual void paint(Context &context) override;
      
      virtual Box *remove(int *index) override;
      
      virtual Expr to_pmath_symbol() override;
      virtual Expr to_pmath(BoxOutputFlags flags) override;
      
      virtual Box *move_vertical(
        LogicalDirection  direction,
        float            *index_rel_x,
        int              *index,
        bool              called_from_child) override;
        
      virtual VolatileSelection mouse_selection(Point pos, bool *was_inside_start) override;
        
      virtual void child_transformation(
        int             index,
        cairo_matrix_t *matrix) override;
    
    protected:
        virtual DefaultStyleOptionOffsets get_default_styles_offset() override { return DefaultStyleOptionOffsets::FractionBox; }

    private:
      MathSequence *_numerator;
      MathSequence *_denominator;
      
      float num_y, den_y;
  };
}

#endif // RICHMATH__BOXES__FRACTIONBOX_H__INCLUDED
