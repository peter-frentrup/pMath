#include <boxes/transformationbox.h>

#include <cmath>

#include <boxes/mathsequence.h>
#include <graphics/context.h>

using namespace richmath;

//{ class AbstractTransformationBox ...

extern pmath_symbol_t richmath_System_BoxRotation;
extern pmath_symbol_t richmath_System_BoxTransformation;
extern pmath_symbol_t richmath_System_RotationBox;
extern pmath_symbol_t richmath_System_TransformationBox;

AbstractTransformationBox::AbstractTransformationBox()
  : OwnerBox(0)
{
  mat.xx = 1;
  mat.xy = 0;
  mat.yx = 0;
  mat.yy = 1;
}

void AbstractTransformationBox::resize_default_baseline(Context *context) {
  context->canvas->save();
  context->canvas->transform(mat);
  float w = context->width;
  context->width = Infinity;
  
  OwnerBox::resize_default_baseline(context);
  
  context->width = w;
  context->canvas->restore();
  
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

void AbstractTransformationBox::paint(Context *context) {
  update_dynamic_styles(context);
  
  float x, y;
  context->canvas->current_pos(&x, &y);
  
  context->canvas->save();
  
  context->canvas->translate(x, y);
  context->canvas->transform(mat);
  
  context->canvas->move_to(0, 0);
  
  _content->paint(context);
  
  context->canvas->restore();
}

Box *AbstractTransformationBox::mouse_selection(
  float  x,
  float  y,
  int   *start,
  int   *end,
  bool  *was_inside_start
) {
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
           was_inside_start);
}

void AbstractTransformationBox::child_transformation(
  int             index,
  cairo_matrix_t *matrix
) {
  cairo_matrix_multiply(matrix, &mat, matrix);
}

//} ... class AbstractTransformationBox

//{ class RotationBox ...

RotationBox::RotationBox()
  : AbstractTransformationBox(),
    _angle(0)
{
  if(!style)
    style = new Style;
}

bool RotationBox::try_load_from_object(Expr expr, BoxInputFlags opts) {
  if(expr[0] != richmath_System_RotationBox)
    return false;
    
  if(expr.expr_length() < 1)
    return false;
    
  Expr options(pmath_options_extract_ex(expr.get(), 1, PMATH_OPTIONS_EXTRACT_UNKNOWN_WARNONLY));
  if(options.is_null())
    return false;
    
  /* now success is guaranteed */
  
  _content->load_from_object(expr[1], opts);
  reset_style();
  style->add_pmath(options);
  angle(
    Expr(pmath_option_value(
           richmath_System_RotationBox,
           richmath_System_BoxRotation,
           options.get())));
  
  finish_load_from_object(std::move(expr));
  return true;
}

bool RotationBox::angle(Expr a) {
  _angle = a;
  double angle = a.to_double();
  
  mat.xx = cos(angle);
  mat.xy = sin(angle);
  mat.yx = -mat.xy;
  mat.yy = mat.xx;
  
  return true;
}

void RotationBox::paint(Context *context) {
  bool have_dynamic = update_dynamic_styles(context);
  
  AbstractTransformationBox::paint(context);
  
  if(have_dynamic) {
    Expr e = get_style(BoxRotation, Expr());
    if(e.is_valid())
      angle(e);
  }
}

Expr RotationBox::to_pmath_symbol() {
  return Symbol(richmath_System_RotationBox);
}

Expr RotationBox::to_pmath(BoxOutputFlags flags) {
  return Call(
           Symbol(richmath_System_RotationBox),
           _content->to_pmath(flags),
           Rule(
             Symbol(richmath_System_BoxRotation),
             _angle));
}

//} ... class RotationBox

//{ class TransformationBox ...

TransformationBox::TransformationBox()
  : AbstractTransformationBox(),
    _matrix(0)
{
}

bool TransformationBox::try_load_from_object(Expr expr, BoxInputFlags opts) {
  if(expr[0] != richmath_System_TransformationBox)
    return false;
    
  if(expr.expr_length() < 1)
    return false;
    
  Expr options(pmath_options_extract_ex(expr.get(), 1, PMATH_OPTIONS_EXTRACT_UNKNOWN_WARNONLY));
  if(options.is_null())
    return false;
    
  if(!matrix(Expr(pmath_option_value(
                    richmath_System_TransformationBox,
                    richmath_System_BoxTransformation,
                    options.get()))))
  {
    return false;
  }
  
  /* now success is guaranteed */
  
  reset_style();
  if(style)
    style->add_pmath(options);
  else
    style = new Style(options);
    
  _content->load_from_object(expr[1], opts);
  
  finish_load_from_object(std::move(expr));
  return true;
}

bool TransformationBox::matrix(Expr m) {
  if(m.expr_length() == 2         &&
      m[0] == PMATH_SYMBOL_LIST    &&
      m[1].expr_length() == 2      &&
      m[1][0] == PMATH_SYMBOL_LIST &&
      m[2].expr_length() == 2      &&
      m[2][0] == PMATH_SYMBOL_LIST)
  {
    _matrix = m;
    mat.xx =   _matrix[1][1].to_double();
    mat.xy = - _matrix[1][2].to_double();
    mat.yx = - _matrix[2][1].to_double();
    mat.yy =   _matrix[2][2].to_double();
    
    cairo_matrix_t m2;
    memcpy(&m2, &mat, sizeof(m2));
    if(cairo_matrix_invert(&m2) != CAIRO_STATUS_SUCCESS)
      cairo_matrix_init_identity(&mat);
      
    return true;
  }
  return false;
}

void TransformationBox::paint(Context *context) {
  bool have_dynamic = update_dynamic_styles(context);
  
  AbstractTransformationBox::paint(context);
  
  if(have_dynamic) {
    Expr e = get_style(BoxTransformation, Expr());
    if(e.is_valid())
      matrix(e);
  }
}

Expr TransformationBox::to_pmath_symbol() {
  return Symbol(richmath_System_TransformationBox);
}

Expr TransformationBox::to_pmath(BoxOutputFlags flags) {
  return Call(
           Symbol(richmath_System_TransformationBox),
           _content->to_pmath(flags),
           Rule(
             Symbol(richmath_System_BoxTransformation),
             _matrix));
}

//} ... class TransformationBox
