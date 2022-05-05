#ifndef RICHMATH__BOXES__TRANSFORMATIONBOX_H__INCLUDED
#define RICHMATH__BOXES__TRANSFORMATIONBOX_H__INCLUDED

#include <boxes/ownerbox.h>


namespace richmath {
  class AbstractTransformationBox: public OwnerBox {
      using base = OwnerBox;
    public:
      explicit AbstractTransformationBox(AbstractSequence *content);
      
      virtual void paint_content(Context &context) override;
      
      virtual VolatileSelection mouse_selection(Point pos, bool *was_inside_start) override;
        
      virtual void child_transformation(
        int             index,
        cairo_matrix_t *matrix) override;
        
      const cairo_matrix_t &cairo_matrix() { return mat; }
      
    protected:
      virtual void resize_default_baseline(Context &context) override;
      virtual float allowed_content_width(const Context &context) { return Infinity; }
      virtual void adjust_baseline_after_resize(Context &context) override;
    
    protected:
      cairo_matrix_t mat;
  };
  
  class RotationBox final : public AbstractTransformationBox {
      using base = AbstractTransformationBox;
    public:
      explicit RotationBox(AbstractSequence *content);
      
      // Box::try_create<RotationBox>(expr, opts);
      virtual bool try_load_from_object(Expr expr, BoxInputFlags opts) override;
      
      Expr angle() { return _angle; }
      bool angle(Expr a);
      
      virtual void paint(Context &context) override;
      
      virtual Expr to_pmath_symbol() override;
    
    protected:
      virtual Expr to_pmath_impl(BoxOutputFlags flags) override;
      
    private:
      Expr _angle;
  };
  
  class TransformationBox final : public AbstractTransformationBox {
      using base = AbstractTransformationBox;
    public:
      explicit TransformationBox(AbstractSequence *content);
      
      // Box::try_create<TransformationBox>(expr, opts);
      virtual bool try_load_from_object(Expr expr, BoxInputFlags opts) override;
      
      Expr matrix() { return _matrix; }
      bool matrix(Expr m);
      
      virtual void paint(Context &context) override;
      
      virtual Expr to_pmath_symbol() override;
    
    protected:
      virtual Expr to_pmath_impl(BoxOutputFlags flags) override;
      
    private:
      Expr _matrix;
  };
}

#endif // RICHMATH__BOXES__TRANSFORMATIONBOX_H__INCLUDED
