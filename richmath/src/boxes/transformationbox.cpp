#include <boxes/transformationbox.h>

#include <cmath>

#include <boxes/mathsequence.h>
#include <graphics/context.h>

using namespace richmath;

//{ class AbstractTransformationBox ...

AbstractTransformationBox::AbstractTransformationBox()
: OwnerBox(0)
{
  mat.xx = 1;
  mat.xy = 0;
  mat.yx = 0;
  mat.yy = 1;
}

void AbstractTransformationBox::resize(Context *context){
  context->canvas->save();
  context->canvas->transform(mat);
  context->canvas->reset_font_cache();
  
  OwnerBox::resize(context);
  
  context->canvas->restore();
  context->canvas->reset_font_cache();
  
  double mx = 0;//_content->extents().width / 2;
  double my = _content->extents().ascent;//_content->extents().height() / 2;
  
  double x, y;
  
  x = _extents.width - mx;
  y = _extents.height() - my;
  
  double x1 = mat.xx * x + mat.xy * y;
  double y1 = mat.yx * x + mat.yy * y;
  
  y = -my;
  
  double x2 = mat.xx * x + mat.xy * y;
  double y2 = mat.yx * x + mat.yy * y;
  
  x = -mx;
  
  double x3 = mat.xx * x + mat.xy * y;
  double y3 = mat.yx * x + mat.yy * y;
  
  y = _extents.height() - my;
  
  double x4 = mat.xx * x + mat.xy * y;
  double y4 = mat.yx * x + mat.yy * y;
  
  double xa = x1;
  if(xa > x2) xa = x2;
  if(xa > x3) xa = x3;
  if(xa > x4) xa = x4;
  
  double xb = x1;
  if(xb < x2) xb = x2;
  if(xb < x3) xb = x3;
  if(xb < x4) xb = x4;
  
  double ya = y1;
  if(ya > y2) ya = y2;
  if(ya > y3) ya = y3;
  if(ya > y4) ya = y4;
  
  double yb = y1;
  if(yb < y2) yb = y2;
  if(yb < y3) yb = y3;
  if(yb < y4) yb = y4;
  
  mat.x0 = -xa;
  mat.y0 = 0;
  
  _extents.width = xb - xa;
  _extents.ascent = -ya;
  _extents.descent = yb;
}

void AbstractTransformationBox::paint(Context *context){
  float x,y;
  context->canvas->current_pos(&x, &y);
  
  context->canvas->save();
  
  cairo_translate(context->canvas->cairo(), x, y);
  cairo_transform(context->canvas->cairo(), &mat);
  context->canvas->reset_font_cache();
  
  context->canvas->move_to(0, 0);
  
  _content->paint(context);
  
  context->canvas->restore();
  context->canvas->reset_font_cache();
}

Box *AbstractTransformationBox::mouse_selection(
  float x,
  float y,
  int   *start,
  int   *end,
  bool  *eol
){
  double mx = 0;//_content->extents().width / 2;
  double my = _content->extents().ascent;//_content->extents().height() / 2;
  
  cairo_matrix_t inv_mat = mat;
  cairo_matrix_translate(&inv_mat, -mx, _content->extents().ascent - my);
  
  cairo_matrix_invert(&inv_mat);
  cairo_matrix_translate(&inv_mat, x, y);
  x = inv_mat.x0;
  y = inv_mat.y0;
        
  return _content->mouse_selection(
    x,
    y,
    start,
    end,
    eol);
}

void AbstractTransformationBox::child_transformation(
  int             index,
  cairo_matrix_t *matrix
){
  cairo_matrix_multiply(matrix, &mat, matrix);
}

//} ... class AbstractTransformationBox

//{ class RotationBox ...

RotationBox::RotationBox()
: AbstractTransformationBox(),
  _angle(0)
{
}

RotationBox *RotationBox::create(Expr expr, int opts){
  if(!expr.instance_of(PMATH_TYPE_EXPRESSION)
  || expr.expr_length() < 1)
    return 0;
  
  pmath_t options = pmath_options_extract(expr.get(), 1);
  if(!options)
    return 0;
  
  RotationBox *result = new RotationBox();
  result->_content->load_from_object(expr[1], opts);
  result->angle(
    Expr(pmath_option_value(
      PMATH_SYMBOL_ROTATIONBOX, 
      PMATH_SYMBOL_BOXROTATION,
      options)));
  
  pmath_unref(options);
  return result;
}

bool RotationBox::angle(Expr a){
  _angle = a;
  double angle = a.to_double();
  
  mat.xx = cos(angle);
  mat.xy = sin(angle);
  mat.yx = -mat.xy;
  mat.yy = mat.xx;
  
  return true;
}

Expr RotationBox::to_pmath(bool parseable){
  return Call(
    Symbol(PMATH_SYMBOL_ROTATIONBOX),
    _content->to_pmath(parseable),
    Rule(
      Symbol(PMATH_SYMBOL_BOXROTATION),
      _angle));
}

//} ... class FrameBox

//{ class TransformationBox ...

TransformationBox::TransformationBox()
: AbstractTransformationBox(),
  _matrix(0)
{
}

TransformationBox *TransformationBox::create(Expr expr, int opts){
  if(!expr.instance_of(PMATH_TYPE_EXPRESSION)
  || expr.expr_length() < 1)
    return 0;
  
  pmath_t options = pmath_options_extract(expr.get(), 1);
  if(!options)
    return 0;
  
  TransformationBox *result = new TransformationBox();
  
  if(result->matrix(Expr(pmath_option_value(
      PMATH_SYMBOL_TRANSFORMATIONBOX, 
      PMATH_SYMBOL_BOXTRANSFORMATION,
      options)))
  ){
    result->_content->load_from_object(expr[1], opts);
    pmath_unref(options);
    return result;
  }
  
  pmath_unref(options);
  delete result;
  return 0;
}

bool TransformationBox::matrix(Expr m){
  if(m.expr_length() == 2
  && m[0] == PMATH_SYMBOL_LIST
  && m[1].expr_length() == 2
  && m[1][0] == PMATH_SYMBOL_LIST
  && m[2].expr_length() == 2
  && m[2][0] == PMATH_SYMBOL_LIST){
    _matrix = m;
    mat.xx =   _matrix[1][1].to_double();
    mat.xy = - _matrix[1][2].to_double();
    mat.yx = - _matrix[2][1].to_double();
    mat.yy =   _matrix[2][2].to_double();
    return true;
  }
  return false;
}

Expr TransformationBox::to_pmath(bool parseable){
  return Call(
    Symbol(PMATH_SYMBOL_TRANSFORMATIONBOX), 
    _content->to_pmath(parseable),
    Rule(
      Symbol(PMATH_SYMBOL_BOXTRANSFORMATION),
      _matrix));
}

//} ... class TransformationBox
