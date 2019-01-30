#ifndef RICHMATH__BOXES__TRANSFORMATIONBOX_H__INCLUDED
#define RICHMATH__BOXES__TRANSFORMATIONBOX_H__INCLUDED

#include <boxes/ownerbox.h>


namespace richmath {
  class AbstractTransformationBox: public OwnerBox {
    public:
      AbstractTransformationBox();
      
      virtual void paint(Context *context) override;
      
      virtual Box *mouse_selection(
        float  x,
        float  y,
        int   *start,
        int   *end,
        bool  *was_inside_start) override;
        
      virtual void child_transformation(
        int             index,
        cairo_matrix_t *matrix) override;
        
      const cairo_matrix_t &cairo_matrix() { return mat; }
      
    protected:
      virtual void resize_no_baseline(Context *context) override;
    
    protected:
      cairo_matrix_t mat;
  };
  
  class RotationBox: public AbstractTransformationBox {
    public:
      RotationBox();
      
      // Box::try_create<RotationBox>(expr, opts);
      virtual bool try_load_from_object(Expr expr, BoxInputFlags opts) override;
      
      Expr angle() { return _angle; }
      bool angle(Expr a);
      
      virtual void paint(Context *context) override;
      
      virtual Expr to_pmath_symbol() override;
      virtual Expr to_pmath(BoxOutputFlags flags) override;
      
    private:
      Expr _angle;
  };
  
  class TransformationBox: public AbstractTransformationBox {
    public:
      TransformationBox();
      
      // Box::try_create<TransformationBox>(expr, opts);
      virtual bool try_load_from_object(Expr expr, BoxInputFlags opts) override;
      
      Expr matrix() { return _matrix; }
      bool matrix(Expr m);
      
      virtual void paint(Context *context) override;
      
      virtual Expr to_pmath_symbol() override;
      virtual Expr to_pmath(BoxOutputFlags flags) override;
      
    private:
      Expr _matrix;
  };
}

#endif // RICHMATH__BOXES__TRANSFORMATIONBOX_H__INCLUDED
