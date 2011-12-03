#include <boxes/graphics/axisticks.h>

#include <boxes/mathsequence.h>

#include <graphics/context.h>

#include <cmath>


using namespace richmath;

//{ class AxisTicks ...

AxisTicks::AxisTicks()
  : Box(),
  start_x(0),
  start_y(0),
  end_x(0),
  end_y(0),
  label_direction_x(0),
  label_direction_y(0),
  label_center_distance_min(0),
  extra_offset(0),
  start_position(0),
  end_position(0)
{
}

AxisTicks::~AxisTicks() {
  set_count(0);
}

void AxisTicks::load_from_object(Expr expr, int options) { // BoxOptionXXX
  /* {{pos1, label1, __ignored_rest__}, {pos2, label2, __ignored_rest__}, ...}
   */
  if(expr[0] != PMATH_SYMBOL_LIST)
    return;
    
  set_count((int)expr.expr_length());
  
  for(int i = 0; i < count(); ++i) {
    AbstractSequence *seq = label(i);
    Expr tick = expr[i + 1];
    
    if( tick[0] == PMATH_SYMBOL_LIST &&
        tick.expr_length() >= 2 &&
        tick[1].is_number())
    {
      position(i) = tick[1].to_double();
      seq->load_from_object(tick[2], options);
      continue;
    }
    
    seq->remove(0, seq->length());
    position(i) = 0.0;
  }
}

void AxisTicks::resize(Context *context) {
  float old_width = context->width;
  context->width = HUGE_VAL;
  
  for(int i = 0; i < _labels.length(); ++i)
    _labels[i]->resize(context);
    
  _extents.width   = 0;
  _extents.ascent  = 0;
  _extents.descent = 0;
  
  context->width = old_width;
}

void AxisTicks::paint(Context *context) {
  float x, y;
  
  context->canvas->current_pos(&x, &y);
  
  for(int i = 0; i < count(); ++i) {
    if(is_visible(i)) {
      float lx, ly;
      get_label_position(i, &lx, &ly);
      
      context->canvas->move_to(x + lx, y + ly);
      label(i)->paint(context);
      
      float tx, ty;
      get_tick_position(position(i), &tx, &ty);
      
      draw_tick(context->canvas, x + tx, y + ty, extra_offset / 2);
    }
  }
}

void AxisTicks::calc_bounds(float *x1, float *y1, float *x2, float *y2) {
  if(start_x < end_x) {
    *x1 = start_x;
    *x2 = end_x;
  }
  else {
    *x1 = end_x;
    *x2 = start_x;
  }
  
  if(start_y < end_y) {
    *y1 = start_y;
    *y2 = end_y;
  }
  else {
    *y1 = end_y;
    *y2 = start_y;
  }
  
  for(int i = 0; i < count(); ++i) {
    if(is_visible(i)) {
      float lx, ly;
      get_label_position(i, &lx, &ly);
      
      const BoxSize &size = label(i)->extents();
      
      if(*x1 > lx)
        *x1 = lx;
        
      if(*x2 < lx + size.width)
        *x2 = lx + size.width;
        
      if(*y1 > ly - size.ascent)
        *y1 = ly - size.ascent;
        
      if(*y2 < ly + size.descent)
        *y2 = ly + size.descent;
    }
  }
}

BoxSize AxisTicks::all_labels_extents() {
  BoxSize size;
  
  for(int i = 0; i < count(); ++i)
    size.merge(label(i)->extents());
    
  return size;
}

Box *AxisTicks::remove(int *index) {
  if(_parent) {
    *index = _index;
    return _parent->remove(index);
  }
  
  return this;
}

Expr AxisTicks::to_pmath(int flags) { // BoxFlagXXX
  Gather g;
  
  for(int i = 0; i < count(); ++i) {
    Gather::emit(
      List(position(i), label(i)->to_pmath(flags)));
  }
  
  return g.end();
}

Box *AxisTicks::mouse_selection(
  float  x,
  float  y,
  int   *start,
  int   *end,
  bool  *was_inside_start
) {
  for(int i = 0; i < count(); ++i) {
    float cx, cy;
    
    get_label_position(i, &cx, &cy);
    
    const BoxSize &size = label(i)->extents();
    
    if( cx               <= x && x <= cx + size.width &&
        cy - size.ascent <= y && y <= cy + size.descent)
    {
      x -= cx;
      y -= cy;
      return label(i)->mouse_selection(x, y, start, end, was_inside_start);
    }
  }
  
  *start = 0;
  *end = count();
  return this;
}

void AxisTicks::child_transformation(
  int             index,
  cairo_matrix_t *matrix
) {
  float cx, cy;
  
  get_label_position(index, &cx, &cy);
  
  cairo_matrix_translate(matrix, cx, cy);
}

void AxisTicks::set_count(int new_count) {
  int old_count = _labels.length();
  
  for(int i = new_count; i < old_count; ++i) {
    delete _labels[i];
    _labels[i] = 0;
  }
  
  _labels.length(new_count, 0);
  _positions.length(new_count, 0.0);
  
  for(int i = old_count; i < new_count; ++i) {
    AbstractSequence *label = new MathSequence();
    
    adopt(label, i);
    
    _labels[i] = label;
  }
}

void AxisTicks::draw_tick(Canvas *canvas, float x, float y, float length) {
  if(length == 0)
    return;
  
  float factor = label_direction_x * label_direction_x + label_direction_y * label_direction_y;
  
  if(factor == 0)
    return;
  
  factor = 1/sqrt(factor);
  
  float x1 = x;
  float y1 = y;
  
  float x2 = x + length * label_direction_x * factor;
  float y2 = y + length * label_direction_y * factor;
  
  if(label_direction_x == 0 || label_direction_y == 0){
    canvas->align_point(&x1, &y1, true);
    canvas->align_point(&x2, &y2, true);
  }
  
  canvas->move_to(x1, y1);
  canvas->line_to(x2, y2);
  canvas->hair_stroke();
}

void AxisTicks::get_tick_position(
  double  t,
  float  *x,
  float  *y
) {
  if(end_position == start_position) {
    *x = start_x;
    *y = start_y;
    return;
  }
  
  double relative = (t - start_position) / (end_position - start_position);
  
  *x = start_x + (end_x - start_x) * relative;
  *y = start_y + (end_y - start_y) * relative;
}

void AxisTicks::get_label_center(
  double  t,
  float   label_width,
  float   label_height,
  float  *x,
  float  *y
) {
  double square_distance = get_square_rect_radius(
                             label_width,
                             label_height,
                             label_direction_x,
                             label_direction_y);
                             
  double distance = sqrt(square_distance);
  if(distance < label_center_distance_min)
    distance = label_center_distance_min;
    
  distance += extra_offset;
  square_distance = distance * distance;
  
  double dx, dy;
  double square_dx = label_direction_x * label_direction_x;
  double square_dy = label_direction_y * label_direction_y;
  
  if(square_dx != 0) {
    square_dx = square_distance / (1 + square_dy / square_dx);
    
    dx = sqrt(square_dx);
    
    if(label_direction_x < 0)
      dx = -dx;
      
    dy = dx * square_dy / square_dx;
  }
  else {
    dx = 0;
    dy = sqrt(square_distance);
    
    if(label_direction_y < 0)
      dy = -dy;
  }
  
  get_tick_position(t, x, y);
  *x += dx;
  *y += dy;
}

void AxisTicks::get_label_position(int i, float *x, float *y) {
  const BoxSize &size = label(i)->extents();
  
  get_label_center(
    position(i),
    size.width,
    size.height(),
    x,
    y);
    
  *x -= size.width / 2;
  *y += size.ascent - size.height() / 2;
}

double AxisTicks::get_square_rect_radius(
  float width,
  float height,
  float dx,
  float dy
) {
  if(width == 0) {
    if(dx == 0)
      return height / 2;
      
    return 0;
  }
  
  if(height == 0) {
    if(dy == 0)
      return width / 2;
      
    return 0;
  }
  
  double w, h;
  
  if(fabs(height * dx) > fabs(width * dy)) {
    w = width / 2;
    h = w * dy / dx;
  }
  else {
    h = height / 2;
    w = h * dx / dy;
  }
  
  return h * h + w * w;
}

//} ... class AxisTicks
