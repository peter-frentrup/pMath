#ifndef __BOXES__TRANSFORMATIONBOX_H__
#define __BOXES__TRANSFORMATIONBOX_H__

#include <boxes/ownerbox.h>

namespace richmath{
  class AbstractTransformationBox: public OwnerBox{
    public:
      AbstractTransformationBox();
      
      void resize(Context *context);
      void paint(Context *context);
      
      Box *mouse_selection(
        float x,
        float y,
        int   *start,
        int   *end,
        bool  *eol);
      
      void child_transformation(
        int             index,
        cairo_matrix_t *matrix);
        
      const cairo_matrix_t &cairo_matrix(){ return mat; }
    protected:
      cairo_matrix_t mat;
  };
  
  class RotationBox: public AbstractTransformationBox{
    public:
      RotationBox();
      
      static RotationBox *create(Expr expr, int opts);
      
      Expr angle(){ return _angle; }
      bool angle(Expr a);
      
      pmath_t to_pmath(bool parseable);
    
    private:
      Expr _angle;
  };
  
  class TransformationBox: public AbstractTransformationBox{
    public:
      TransformationBox();
      
      static TransformationBox *create(Expr expr, int opts);
      
      Expr matrix(){ return _matrix; }
      bool matrix(Expr m);
      
      pmath_t to_pmath(bool parseable);
      
    private:
      Expr _matrix;
  };
}

#endif // __BOXES__TRANSFORMATIONBOX_H__
