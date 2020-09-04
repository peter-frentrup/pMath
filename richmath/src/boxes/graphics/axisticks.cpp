#include <boxes/graphics/axisticks.h>

#include <boxes/mathsequence.h>

#include <graphics/context.h>

#include <cmath>
#include <limits>

#ifndef NAN
#  define NAN  (std::numeric_limits<double>::quiet_NaN())
#endif


using namespace richmath;

template<typename T>
static T clip(const T &x, const T &min, const T &max) {
  if(min < x) {
    if(x < max)
      return x;
      
    return max;
  }
  
  return min;
}

static void get_tick_length(Expr expr, float *plen, float *nlen) {
  *plen = 0.0;
  *nlen = 0.02;
  
  if(expr[0] == PMATH_SYMBOL_NCACHE)
    expr = expr[2];
    
  if(expr.is_number()) {
    *plen = clip((float)expr.to_double(), -1000.0f, 1000.0f);
    *nlen = *plen;
    return;
  }
  
  if(expr[0] == PMATH_SYMBOL_LIST && expr.expr_length() == 2) {
    Expr sub = expr[1];
    
    if(sub[0] == PMATH_SYMBOL_NCACHE)
      sub = sub[2];
      
    if(sub.is_number())
      *plen = clip((float)sub.to_double(), -1000.0f, 1000.0f);
      
    sub = expr[2];
    
    if(sub[0] == PMATH_SYMBOL_NCACHE)
      sub = sub[2];
      
    if(sub.is_number())
      *nlen = clip((float)sub.to_double(), -1000.0f, 1000.0f);
      
    return;
  }
  
}

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
    tick_length_factor(0),
    extra_offset(0),
    start_position(0),
    end_position(0),
    ignore_label_position(NAN),
    axis_hidden(false)
{
}

AxisTicks::~AxisTicks() {
  set_count(0);
}

bool AxisTicks::try_load_from_object(Expr object, BoxInputFlags options) {
  load_from_object(object, options);
  return true;
}

void AxisTicks::load_from_object(Expr expr, BoxInputFlags options) {
  /* {{pos1, label1, __ignored_rest__}, {pos2, label2, __ignored_rest__}, ...}
   */
  if(expr[0] != PMATH_SYMBOL_LIST)
    return;
    
  set_count((int)expr.expr_length());
  _max_rel_tick = 0.0;
  
  for(int i = 0; i < count(); ++i) {
    AbstractSequence *seq = label(i);
    Expr tick = expr[i + 1];
    
    if( tick[0] == PMATH_SYMBOL_LIST &&
        tick.expr_length() >= 2)
    {
      Expr pos_expr = tick[1];
      if(pos_expr[0] == PMATH_SYMBOL_NCACHE)
        pos_expr = pos_expr[2];
        
      if(pos_expr.is_number())
        position(i) = pos_expr.to_double();
      else
        position(i) = 0.0;
        
      seq->load_from_object(tick[2], options);
      
      float ptic, ntic;
      get_tick_length(tick[3], &ptic, &ntic);
      
      _rel_tick_pos[i] = ptic;
      _rel_tick_neg[i] = ntic;
      
      if(_max_rel_tick < ptic)
        _max_rel_tick = ptic;
        
      continue;
    }
    
    seq->remove(0, seq->length());
    position(i) = 0.0;
    _rel_tick_pos[i] = 0;
    _rel_tick_neg[i] = 0;
  }
  
  finish_load_from_object(std::move(expr));
}

bool AxisTicks::is_visible(double t) {
  double err = (end_position - start_position) * 1.0e-4;
  
  return start_position - err <= t && t <= end_position + err;
}

void AxisTicks::resize(Context &context) {
  float old_w = context.width;
  float old_fs = context.canvas().get_font_size();
  
  context.width = HUGE_VAL;
  
  context.canvas().set_font_size(0.8 * old_fs);
  
  for(auto label : _labels)
    label->resize(context);
  
  context.canvas().set_font_size(old_fs);
  context.width = old_w;
  
  _extents.width   = 0;
  _extents.ascent  = 0;
  _extents.descent = 0;
  
  context.width = old_w;
}

void AxisTicks::paint(Context &context) {
  float x, y;
  
  context.canvas().current_pos(&x, &y);
  
  float old_fs = context.canvas().get_font_size();
  context.canvas().set_font_size(0.8 * old_fs);
  
  bool have_ilp = is_visible(ignore_label_position);
  
  for(int i = 0; i < count(); ++i) {
    if(is_visible(position(i))) {
      Point p;
      get_tick_position(position(i), &p.x, &p.y);
      
      draw_tick(context.canvas(), x + p.x, y + p.y,   _rel_tick_pos[i] * tick_length_factor);
      draw_tick(context.canvas(), x + p.x, y + p.y, - _rel_tick_neg[i] * tick_length_factor);
      
      
      get_label_position(i, &p.x, &p.y);
      
      Box *lbl = label(i);
      
      if(have_ilp) {
        Point ign;
        
        get_label_center(
          ignore_label_position,
          lbl->extents().width,
          lbl->extents().height(),
          &ign.x, &ign.y);
          
        if(lbl->extents().to_rectangle(p).contains(ign))
          continue;
      }
      
      context.canvas().move_to(x + p.x, y + p.y);
      lbl->paint(context);
    }
  }
  
  context.canvas().set_font_size(old_fs);
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
    if(is_visible(position(i))) {
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

Expr AxisTicks::to_pmath(BoxOutputFlags flags) {
  Gather g;
  
  for(int i = 0; i < count(); ++i) {
    Gather::emit(
      List(position(i), label(i)->to_pmath(flags)));
  }
  
  return g.end();
}

VolatileSelection AxisTicks::mouse_selection(float x, float y, bool *was_inside_start) {
  for(int i = 0; i < count(); ++i) {
    float cx, cy;
    
    get_label_position(i, &cx, &cy);
    
    const BoxSize &size = label(i)->extents();
    
    if( cx               <= x && x <= cx + size.width &&
        cy - size.ascent <= y && y <= cy + size.descent)
    {
      x -= cx;
      y -= cy;
      return label(i)->mouse_selection(x, y, was_inside_start);
    }
  }
  
  return { this, 0, count() };
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
    _labels[i]->safe_destroy();
    _labels[i] = nullptr;
  }
  
  _labels.length(new_count, 0);
  _positions.length(new_count, 0.0);
  _rel_tick_pos.length(new_count, 0.0);
  _rel_tick_neg.length(new_count, 0.0);
  
  for(int i = old_count; i < new_count; ++i) {
    AbstractSequence *label = new MathSequence();
    
    adopt(label, i);
    
    _labels[i] = label;
  }
}

void AxisTicks::draw_tick(Canvas &canvas, float x, float y, float length) {
  if(length == 0)
    return;
    
  float factor = hypot(label_direction_x, label_direction_y);
  if(factor == 0)
    return;
    
  factor = 1 / factor;
  
  float x1 = x;
  float y1 = y;
  
  float x2 = x + length * label_direction_x * factor;
  float y2 = y + length * label_direction_y * factor;
  
  if(label_direction_x == 0 || label_direction_y == 0) {
    canvas.align_point(&x1, &y1, true);
    canvas.align_point(&x2, &y2, true);
  }
  
  canvas.save();
  {
    cairo_set_line_cap(canvas.cairo(), CAIRO_LINE_CAP_SQUARE);
    
    canvas.move_to(x1, y1);
    canvas.line_to(x2, y2);
    
    canvas.hair_stroke();
  }
  canvas.restore();
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
