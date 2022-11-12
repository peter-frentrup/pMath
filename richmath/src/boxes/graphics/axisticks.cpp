#include <boxes/graphics/axisticks.h>

#include <boxes/mathsequence.h>

#include <graphics/context.h>

#include <cmath>
#include <limits>

#ifndef NAN
#  define NAN  (std::numeric_limits<double>::quiet_NaN())
#endif


using namespace richmath;

extern pmath_symbol_t richmath_System_List;
extern pmath_symbol_t richmath_System_NCache;

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
  
  if(expr[0] == richmath_System_NCache)
    expr = expr[2];
    
  if(expr.is_number()) {
    *plen = clip((float)expr.to_double(), -1000.0f, 1000.0f);
    *nlen = *plen;
    return;
  }
  
  if(expr[0] == richmath_System_List && expr.expr_length() == 2) {
    Expr sub = expr[1];
    
    if(sub[0] == richmath_System_NCache)
      sub = sub[2];
      
    if(sub.is_number())
      *plen = clip((float)sub.to_double(), -1000.0f, 1000.0f);
      
    sub = expr[2];
    
    if(sub[0] == richmath_System_NCache)
      sub = sub[2];
      
    if(sub.is_number())
      *nlen = clip((float)sub.to_double(), -1000.0f, 1000.0f);
      
    return;
  }
  
}

//{ class AxisTicks ...

AxisTicks::AxisTicks()
  : base(),
    start_pos{0.0f, 0.0f},
    end_pos{0.0f, 0.0f},
    label_direction{0.0f, 0.0f},
    label_center_distance_min(0),
    tick_length_factor(0),
    extra_offset(0),
    start_position(0),
    end_position(0),
    ignore_label_position(NAN)
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
  if(expr[0] != richmath_System_List)
    return;
    
  set_count((int)expr.expr_length());
  _max_rel_tick = 0.0;
  
  for(int i = 0; i < count(); ++i) {
    AbstractSequence *seq = label(i);
    Expr tick = expr[i + 1];
    
    if( tick[0] == richmath_System_List &&
        tick.expr_length() >= 2)
    {
      Expr pos_expr = tick[1];
      if(pos_expr[0] == richmath_System_NCache)
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
  
  finish_load_from_object(PMATH_CPP_MOVE(expr));
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
  Point p0 = context.canvas().current_pos();
  
  float old_fs = context.canvas().get_font_size();
  context.canvas().set_font_size(0.8 * old_fs);
  
  bool have_ilp = is_visible(ignore_label_position);
  
  for(int i = 0; i < count(); ++i) {
    if(is_visible(position(i))) {
      Point p = p0 + Vector2F(get_tick_position(position(i)));
      
      draw_tick(context.canvas(), p,   _rel_tick_pos[i] * tick_length_factor);
      draw_tick(context.canvas(), p, - _rel_tick_neg[i] * tick_length_factor);
      
      p = get_label_position(i);
      
      Box *lbl = label(i);
      
      if(have_ilp) {
        Point ign = get_label_center(
          ignore_label_position,
          lbl->extents().width,
          lbl->extents().height());
          
        if(lbl->extents().to_rectangle(p).contains(ign))
          continue;
      }
      
      context.canvas().move_to(p0 + Vector2F(p));
      lbl->paint(context);
    }
  }
  
  context.canvas().set_font_size(old_fs);
}

BoxSize AxisTicks::all_labels_extents() {
  BoxSize size;
  
  for(int i = 0; i < count(); ++i)
    size.merge(label(i)->extents());
    
  return size;
}

Box *AxisTicks::remove(int *index) {
  if(auto par = parent()) {
    *index = _index;
    return par->remove(index);
  }
  
  return this;
}

Expr AxisTicks::to_pmath_symbol() {
  return Symbol(richmath_System_List);
}

Expr AxisTicks::to_pmath_impl(BoxOutputFlags flags) {
  Gather g;
  
  for(int i = 0; i < count(); ++i) {
    Gather::emit(
      List(position(i), label(i)->to_pmath(flags)));
  }
  
  return g.end();
}

VolatileSelection AxisTicks::mouse_selection(Point pos, bool *was_inside_start) {
  for(int i = 0; i < count(); ++i) {
    Vector2F lbl_delta{get_label_position(i)};
    
    if(label(i)->extents().to_rectangle().contains(pos - lbl_delta))
      return label(i)->mouse_selection(pos - lbl_delta, was_inside_start);
  }
  
  return { this, 0, count() };
}

void AxisTicks::child_transformation(
  int             index,
  cairo_matrix_t *matrix
) {
  Point label_pos = get_label_position(index);
  
  cairo_matrix_translate(matrix, label_pos.x, label_pos.y);
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

void AxisTicks::draw_tick(Canvas &canvas, Point p, float length) {
  if(length == 0)
    return;
    
  float factor = label_direction.length();
  if(factor == 0)
    return;
    
  factor = 1 / factor;
  
  Point p1 = p;
  Point p2 = p + (length * factor) * label_direction;
  
  if(label_direction.x == 0 || label_direction.y == 0) {
    p1 = canvas.align_point(p1, true);
    p2 = canvas.align_point(p2, true);
  }
  
  canvas.save();
  {
    cairo_set_line_cap(canvas.cairo(), CAIRO_LINE_CAP_SQUARE);
    
    canvas.move_to(p1);
    canvas.line_to(p2);
    
    canvas.hair_stroke();
  }
  canvas.restore();
}

Point AxisTicks::get_tick_position(double t) {
  if(end_position == start_position) 
    return start_pos;
  
  double relative = (t - start_position) / (end_position - start_position);
  
  return start_pos + (end_pos - start_pos) * relative;
}

Point AxisTicks::get_label_center(double t, float label_width, float label_height) {
  double square_distance = get_square_rect_radius(label_width, label_height, label_direction);
                             
  double distance = sqrt(square_distance);
  if(distance < label_center_distance_min)
    distance = label_center_distance_min;
    
  distance += extra_offset;
  square_distance = distance * distance;
  
  double dx, dy;
  double square_dx = label_direction.x * label_direction.x;
  double square_dy = label_direction.y * label_direction.y;
  
  if(square_dx != 0) {
    square_dx = square_distance / (1 + square_dy / square_dx);
    
    dx = sqrt(square_dx);
    
    if(label_direction.x < 0)
      dx = -dx;
      
    dy = dx * square_dy / square_dx;
  }
  else {
    dx = 0;
    dy = sqrt(square_distance);
    
    if(label_direction.y < 0)
      dy = -dy;
  }
  
  Point p = get_tick_position(t);
  return Point(p.x + dx, p.y + dy);
}

Point AxisTicks::get_label_position(int i) {
  const BoxSize &size = label(i)->extents();
  
  Point p = get_label_center(position(i), size.width, size.height());
    
  p.x -= size.width / 2;
  p.y += size.ascent - size.height() / 2;
  return p;
}

double AxisTicks::get_square_rect_radius(float width, float height, Vector2F offset) {
  if(width == 0) {
    if(offset.x == 0)
      return height / 2;
      
    return 0;
  }
  
  if(height == 0) {
    if(offset.y == 0)
      return width / 2;
      
    return 0;
  }
  
  double w, h;
  
  if(fabs(height * offset.x) > fabs(width * offset.y)) {
    w = width / 2;
    h = w * offset.y / offset.x;
  }
  else {
    h = height / 2;
    w = h * offset.x / offset.y;
  }
  
  return h * h + w * w;
}

//} ... class AxisTicks
