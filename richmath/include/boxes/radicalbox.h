#ifndef RICHMATH__BOXES__RADICALBOX_H__INCLUDED
#define RICHMATH__BOXES__RADICALBOX_H__INCLUDED

#include <boxes/box.h>


namespace richmath {
  class MathSequence;
  
  class RadicalBox: public Box {
    protected:
      virtual ~RadicalBox();
    public:
      RadicalBox(MathSequence *radicand = 0, MathSequence *exponent = 0);
      
      // Box::try_create<RadicalBox>(expr, opts);
      virtual bool try_load_from_object(Expr expr, BoxInputFlags opts) override;
      
      MathSequence *radicand() { return _radicand; }
      MathSequence *exponent() { return _exponent; }
      
      virtual Box *item(int i) override;
      virtual int count() override;
      
      virtual void resize(Context &context) override;
      virtual void paint(Context &context) override;
      
      virtual Box *remove(int *index) override;
      
      void complete();
      
      virtual Expr to_pmath_symbol() override;
      virtual Expr to_pmath(BoxOutputFlags flags) override;
      
      virtual Box *move_vertical(
        LogicalDirection  direction,
        float            *index_rel_x,
        int              *index,
        bool              called_from_child) override;
        
      virtual VolatileSelection mouse_selection(float x, float y, bool *was_inside_start) override;
        
      virtual void child_transformation(
        int             index,
        cairo_matrix_t *matrix) override;
        
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

#endif // RICHMATH__BOXES__RADICALBOX_H__INCLUDED
