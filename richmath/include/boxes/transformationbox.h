#ifndef __BOXES__TRANSFORMATIONBOX_H__
#define __BOXES__TRANSFORMATIONBOX_H__

#include <boxes/ownerbox.h>


namespace richmath {
  class AbstractTransformationBox: public OwnerBox {
    public:
      AbstractTransformationBox();
      
      virtual void resize(Context *context) override;
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
      cairo_matrix_t mat;
  };
  
  class RotationBox: public AbstractTransformationBox {
    public:
      RotationBox();
      
      // Box::try_create<RotationBox>(expr, opts);
      virtual bool try_load_from_object(Expr expr, int opts) override;
      
      Expr angle() { return _angle; }
      bool angle(Expr a);
      
      virtual void paint(Context *context) override;
      
      virtual Expr to_pmath_symbol() override { return Symbol(PMATH_SYMBOL_ROTATIONBOX); }
      virtual Expr to_pmath(int flags) override;
      
    private:
      Expr _angle;
  };
  
  class TransformationBox: public AbstractTransformationBox {
    public:
      TransformationBox();
      
      // Box::try_create<TransformationBox>(expr, opts);
      virtual bool try_load_from_object(Expr expr, int opts) override;
      
      Expr matrix() { return _matrix; }
      bool matrix(Expr m);
      
      virtual void paint(Context *context) override;
      
      virtual Expr to_pmath_symbol() override { return Symbol(PMATH_SYMBOL_TRANSFORMATIONBOX); }
      virtual Expr to_pmath(int flags) override;
      
    private:
      Expr _matrix;
  };
}

#endif // __BOXES__TRANSFORMATIONBOX_H__
