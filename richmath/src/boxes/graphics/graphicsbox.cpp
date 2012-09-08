#include <boxes/graphics/graphicsbox.h>
#include <boxes/graphics/axisticks.h>

#include <boxes/dynamicbox.h>
#include <boxes/fillbox.h>
#include <boxes/gridbox.h>
#include <boxes/inputfieldbox.h>
#include <boxes/section.h>
#include <boxes/stylebox.h>

#include <graphics/context.h>

#include <gui/document.h>
#include <gui/native-widget.h>

#include <util/spanexpr.h>

#include <algorithm>
#include <cmath>
#include <limits>

#ifdef max
#  undef max
#endif


#ifdef _MSC_VER
#  define isfinite(x)  (_finite(x))
#endif

#ifndef NAN
#  define NAN numeric_limits<double>::quiet_NaN()
#endif


using namespace richmath;
using namespace std;


enum SyntaxPosition {
  Alone,
  InsideList,
  InsideOther
};

template<typename T>
static T max(const T &a, const T &b, const T &c) {
  return std::max(std::max(a, b), c);
}

template<typename T>
static T max(const T &a, const T &b, const T &c, const T &d) {
  return std::max(std::max(a, b), std::max(c, d));
}

template<typename T>
static T clip(const T &x, const T &min, const T &max) {
  if(min < x) {
    if(x < max)
      return x;
      
    return max;
  }
  
  return min;
}


static enum SyntaxPosition find_syntax_position(Box *box, int index) {
  if(!box || dynamic_cast<Section *>(box))
    return Alone;
    
  if(MathSequence *seq = dynamic_cast<MathSequence *>(box)) {
    SpanExpr *expr = new SpanExpr(index, 0, seq);
    
    expr = expr->expand();
    
    bool inside_list = false;
    
    while(expr && expr->sequence() == seq) {
      if( FunctionCallSpan::is_list(expr) ||
          FunctionCallSpan::is_sequence(expr))
      {
        inside_list = true;
      }
      else if(!expr->is_box())
      {
        delete expr;
        return InsideOther;
      }
      
      expr = expr->expand();
    }
    
    if(expr)
      delete expr;
      
    enum SyntaxPosition pos = find_syntax_position(box->parent(), box->index());
    
    if(inside_list && pos < InsideList)
      return InsideList;
      
    return pos;
  }
  
  if( dynamic_cast<AbstractDynamicBox *>(box) ||
      dynamic_cast<AbstractStyleBox *>(box) ||
      dynamic_cast<GridItem *>(box) )
  {
    return find_syntax_position(box->parent(), box->index());
  }
  
  if( dynamic_cast<GridBox *>(box) ||
      dynamic_cast<OwnerBox *>(box))
  {
    enum SyntaxPosition pos = find_syntax_position(box->parent(), box->index());
    
    if(pos < InsideList)
      return InsideList;
      
    return pos;
  }
  
  return InsideOther;
}


//{ class GraphicsBox ...

GraphicsBox::GraphicsBox()
  : Box(),
    mouse_over_part(GraphicsPartNone),
    mouse_down_x(0),
    mouse_down_y(0),
    user_has_changed_size(false)
{
  reset_style();
  
  for(int part = 0; part < 6; ++part) {
    ticks[part] = new AxisTicks;
    adopt(ticks[part], part);
  }
  
  ticks[AxisIndexX     ]->label_direction_x =  0;
  ticks[AxisIndexBottom]->label_direction_x =  0;
  ticks[AxisIndexTop   ]->label_direction_x =  0;
  ticks[AxisIndexX     ]->label_direction_y =  1;
  ticks[AxisIndexBottom]->label_direction_y =  1;
  ticks[AxisIndexTop   ]->label_direction_y = -1;
  
  ticks[AxisIndexY     ]->label_direction_x = -1;
  ticks[AxisIndexLeft  ]->label_direction_x = -1;
  ticks[AxisIndexRight ]->label_direction_x =  1;
  ticks[AxisIndexY     ]->label_direction_y =  0;
  ticks[AxisIndexLeft  ]->label_direction_y =  0;
  ticks[AxisIndexRight ]->label_direction_y =  0;
}

GraphicsBox::~GraphicsBox() {
  for(int part = 0; part < 6; ++part)
    delete ticks[part];
}

bool GraphicsBox::try_load_from_object(Expr expr, int opts) {
  if(expr[0] != PMATH_SYMBOL_GRAPHICSBOX)
    return false;
    
  if(expr.expr_length() < 1)
    return false;
    
  Expr options(PMATH_UNDEFINED);
  
  if(expr.expr_length() > 1) {
    options = Expr(pmath_options_extract(expr.get(), 1));
    
    if(options.is_null())
      return false;
  }
  
  /* now success is guaranteed */
  
  Expr user_options;
  if(user_has_changed_size) {
    Gather g;
    
    style->emit_to_pmath(false);
    
    user_options = g.end();
    for(size_t i = user_options.expr_length(); i > 0; --i) {
      Expr rule = user_options[i];
      
      if(rule[1] == PMATH_SYMBOL_ASPECTRATIO)
        continue;
        
      if(rule[1] == PMATH_SYMBOL_IMAGESIZE)
        continue;
        
      user_options.set(0, Expr());
    }
  }
  
  reset_style();
  style->add_pmath(options);
  style->add_pmath(user_options);
  
  elements.load_from_object(expr[1], opts);
  invalidate();
  
  return true;
}

Box *GraphicsBox::item(int i) {
  assert(0 <= i && i < 6);
  
  return ticks[i];
}

int GraphicsBox::count() {
  return 6;
}

bool GraphicsBox::expand(const BoxSize &size) {
  MathSequence *seq = dynamic_cast<MathSequence *>(_parent);
  if(_parent && seq->length() == 1) {
    if(dynamic_cast<FillBox *>(seq->parent())) {
      calculate_size(&size.width);
      return true;
    }
  }
  
  return false;
}

void GraphicsBox::resize(Context *context) {
  em = context->canvas->get_font_size();
  
  resize_axes(context);
  
  margin_left   = 0;
  margin_right  = 0;
  margin_top    = 0;
  margin_bottom = 0;
  
  calculate_size();
}

static float calc_margin_width(float w, float lbl_w, double all_x, double other_x) {
  if(other_x > all_x)
    other_x = all_x;
    
  if(w <= 0)
    return lbl_w;
    
  double min_ox = (w - lbl_w) * all_x / w;
  if(other_x < min_ox)
    return 0;
    
  return w - (w - lbl_w) * all_x / other_x;
}

void GraphicsBox::calculate_size(const float *optional_expand_width) {
  GraphicsBounds bounds;
  bounds.xmin = ticks[AxisIndexX]->start_position;
  bounds.xmax = ticks[AxisIndexX]->end_position;
  bounds.ymin = ticks[AxisIndexY]->start_position;
  bounds.ymax = ticks[AxisIndexY]->end_position;
  
  double ox = 0, oy = 0;
  calculate_axes_origin(bounds, &ox, &oy);
  bool valid_ox = ticks[AxisIndexBottom]->is_visible(ox);
  bool valid_oy = ticks[AxisIndexLeft  ]->is_visible(oy);
  
  BoxSize label_sizes[6];
  for(int part = 0; part < 6; ++part) {
    label_sizes[part] = ticks[part]->all_labels_extents();
    
    ticks[part]->extra_offset = 0.75 * 6;
  }
  
  bool left, right, bottom, top;
  bool any_frame = have_frame(&left, &right, &bottom, &top);
  if(any_frame) {
    margin_left = max(
                    label_sizes[AxisIndexLeft].width + ticks[AxisIndexLeft]->extra_offset,
                    label_sizes[AxisIndexBottom].width / 2,
                    label_sizes[AxisIndexTop].width / 2);
                    
    margin_right = max(
                     label_sizes[AxisIndexRight].width + ticks[AxisIndexRight]->extra_offset,
                     label_sizes[AxisIndexBottom].width / 2,
                     label_sizes[AxisIndexTop].width / 2);
                     
    margin_bottom = max(
                      label_sizes[AxisIndexBottom].height() + ticks[AxisIndexBottom]->extra_offset,
                      label_sizes[AxisIndexLeft].height() / 2,
                      label_sizes[AxisIndexRight].height() / 2);
                      
    margin_top = max(
                   label_sizes[AxisIndexTop].height() + ticks[AxisIndexTop]->extra_offset,
                   label_sizes[AxisIndexLeft].height() / 2,
                   label_sizes[AxisIndexRight].height() / 2);
  }
  else {
    margin_left = ticks[AxisIndexLeft]->extra_offset;
    
    margin_bottom = ticks[AxisIndexBottom]->extra_offset;
    
    margin_right = max(
                     ticks[AxisIndexRight]->extra_offset,
                     label_sizes[AxisIndexX].width / 2);
                     
    margin_top = max(
                   ticks[AxisIndexTop]->extra_offset,
                   label_sizes[AxisIndexY].height() / 2);
  }
  
  float w     = get_own_style(ImageSizeHorizontal, ImageSizeAutomatic);
  float h     = get_own_style(ImageSizeVertical,   ImageSizeAutomatic);
  float ratio = get_own_style(AspectRatio, 1.0f); //0.61803f
  
  if(ratio <= 0)
    ratio = 1.0f; // 0.61803f;
    
  if(w <= 0 && h <= 0) {
    if(optional_expand_width) {
      w = *optional_expand_width;
    }
    else {
      enum SyntaxPosition pos = find_syntax_position(parent(), index());
      
      switch(pos) {
        case Alone:
          w = 24 * em;
          break;
          
        case InsideList:
          w = 20 * em;
          break;
          
        case InsideOther:
          w = 15 * em;
          break;
      }
    }
  }
  
  if(w > 0 && !any_frame) {
    margin_left = max(
                    label_sizes[AxisIndexX].width / 2,
                    calc_margin_width(
                      w - margin_right,
                      label_sizes[AxisIndexY].width + ticks[AxisIndexY]->extra_offset,
                      bounds.xmax - bounds.xmin,
                      bounds.xmax - ox));
  }
  
  if(h > 0 && !any_frame) {
    margin_bottom = max(
                      label_sizes[AxisIndexY].height() / 2,
                      calc_margin_width(
                        h - margin_top,
                        label_sizes[AxisIndexX].height() + ticks[AxisIndexX]->extra_offset,
                        bounds.ymax - bounds.ymin,
                        bounds.ymax - oy));
  }
  
  if(w <= 0) {
    float content_h = h - margin_top - margin_bottom;
    
    w = content_h / ratio + /*margin_left +*/ margin_right;
    
    if(!any_frame) {
      margin_left = max(
                      label_sizes[AxisIndexX].width / 2,
                      calc_margin_width(
                        w - margin_right,
                        label_sizes[AxisIndexY].width + ticks[AxisIndexY]->extra_offset,
                        bounds.xmax - bounds.xmin,
                        bounds.xmax - ox));
    }
  }
  
  if(h <= 0) {
    float content_w = w - margin_left - margin_right;
    
    h = content_w * ratio + margin_top/* + margin_bottom*/;
    
    if(!any_frame) {
      margin_bottom = max(
                        label_sizes[AxisIndexY].height() / 2,
                        calc_margin_width(
                          h - margin_top,
                          label_sizes[AxisIndexX].height() + ticks[AxisIndexX]->extra_offset,
                          bounds.ymax - bounds.ymin,
                          bounds.ymax - oy));
    }
  }
  
  _extents.width = w;
  _extents.ascent  = h / 2 + 0.25 * em;
  _extents.descent = h - _extents.ascent;
  
  
  ticks[AxisIndexLeft ]->start_y            = _extents.descent - margin_bottom;
  ticks[AxisIndexRight]->start_y            = _extents.descent - margin_bottom;
  ticks[AxisIndexY    ]->start_y            = _extents.descent - margin_bottom;
  ticks[AxisIndexLeft ]->end_y              = margin_top - _extents.ascent;
  ticks[AxisIndexRight]->end_y              = margin_top - _extents.ascent;
  ticks[AxisIndexY    ]->end_y              = margin_top - _extents.ascent;
  ticks[AxisIndexLeft ]->tick_length_factor = w - margin_left - margin_right;
  ticks[AxisIndexRight]->tick_length_factor = w - margin_left - margin_right;
  ticks[AxisIndexY    ]->tick_length_factor = w - margin_left - margin_right;
  
  ticks[AxisIndexLeft]->start_x = margin_left;
  ticks[AxisIndexLeft]->end_x   = margin_left;
  
  ticks[AxisIndexRight]->start_x = w - margin_right;
  ticks[AxisIndexRight]->end_x   = w - margin_right;
  
  
  ticks[AxisIndexBottom]->start_x            = margin_left;
  ticks[AxisIndexTop   ]->start_x            = margin_left;
  ticks[AxisIndexX     ]->start_x            = margin_left;
  ticks[AxisIndexBottom]->end_x              = w - margin_right;
  ticks[AxisIndexTop   ]->end_x              = w - margin_right;
  ticks[AxisIndexX     ]->end_x              = w - margin_right;
  ticks[AxisIndexBottom]->tick_length_factor = h - margin_bottom - margin_top;
  ticks[AxisIndexTop   ]->tick_length_factor = h - margin_bottom - margin_top;
  ticks[AxisIndexX     ]->tick_length_factor = h - margin_bottom - margin_top;
  
  ticks[AxisIndexBottom]->start_y = _extents.descent - margin_bottom;
  ticks[AxisIndexBottom]->end_y   = _extents.descent - margin_bottom;
  
  ticks[AxisIndexTop]->start_y = margin_top - _extents.ascent;
  ticks[AxisIndexTop]->end_y   = margin_top - _extents.ascent;
  
  float tx = 0;
  float ty = 0;
  float dummy;
  ticks[AxisIndexBottom]->get_tick_position(ox, &tx,    &dummy);
  ticks[AxisIndexLeft  ]->get_tick_position(oy, &dummy, &ty);
  
  ticks[AxisIndexY]->start_x     = tx;
  ticks[AxisIndexY]->end_x       = tx;
  
  ticks[AxisIndexX]->start_y     = ty;
  ticks[AxisIndexX]->end_y       = ty;
  
  if(valid_ox) {
    ticks[AxisIndexX]->ignore_label_position = ox;
  }
  else {
    ticks[AxisIndexX]->ignore_label_position = NAN;
    
    ticks[AxisIndexY]->axis_hidden = true;
  }
  
  if(valid_oy) {
    ticks[AxisIndexY]->ignore_label_position = oy;
  }
  else {
    ticks[AxisIndexY]->ignore_label_position = NAN;
    
    ticks[AxisIndexX]->axis_hidden = true;
  }
  
  if(valid_ox && ty >= _extents.descent - margin_bottom - ticks[AxisIndexX]->extra_offset / 2)
    ticks[AxisIndexX]->ignore_label_position = NAN;
    
  if(valid_oy && tx <= margin_left + ticks[AxisIndexY]->extra_offset / 2)
    ticks[AxisIndexY]->ignore_label_position = NAN;
    
}

void GraphicsBox::try_get_axes_origin(const GraphicsBounds &bounds, double *ox, double *oy) {
  Expr e = get_own_style(AxesOrigin);
  
  if(e[0] == PMATH_SYMBOL_NCACHE)
    e = e[2];
    
  if(e[0] == PMATH_SYMBOL_LIST && e.expr_length() == 2) {
    Expr sub = e[1];
    if(sub[0] == PMATH_SYMBOL_NCACHE)
      sub = sub[2];
      
    if(sub.is_number())
      *ox = sub.to_double();
      
    sub = e[2];
    if(sub[0] == PMATH_SYMBOL_NCACHE)
      sub = sub[2];
      
    if(sub.is_number())
      *oy = sub.to_double();
  }
}

void GraphicsBox::calculate_axes_origin(const GraphicsBounds &bounds, double *ox, double *oy) {
  *ox = NAN;
  *oy = NAN;
  
  try_get_axes_origin(bounds, ox, oy);
  
  if(!isfinite(*ox)) {
    Expr e = Evaluate(
               Parse(
                 "FE`Graphics`DefaultAxesOrigin(`1`, `2`)",
                 bounds.xmin,
                 bounds.xmax));
    *ox = e.to_double();
  }
  
  if(!isfinite(*oy)) {
    Expr e = Evaluate(
               Parse(
                 "FE`Graphics`DefaultAxesOrigin(`1`, `2`)",
                 bounds.ymin,
                 bounds.ymax));
    *oy = e.to_double();
  }
  
  if(!isfinite(*ox))
    *ox = clip(0.0, bounds.xmin, bounds.xmax);
    
  if(!isfinite(*oy))
    *oy = clip(0.0, bounds.ymin, bounds.ymax);
}

GraphicsBounds GraphicsBox::calculate_plotrange() {
  GraphicsBounds bounds;
  
  Expr plot_range = get_own_style(PlotRange, Symbol(PMATH_SYMBOL_AUTOMATIC));
  
  if(plot_range[0] == PMATH_SYMBOL_NCACHE)
    plot_range = plot_range[2];
    
  if( plot_range[0] == PMATH_SYMBOL_LIST &&
      plot_range.expr_length() == 2)
  {
    Expr xrange = plot_range[1];
    Expr yrange = plot_range[2];
    
    if( xrange[0] == PMATH_SYMBOL_RANGE &&
        xrange.expr_length() == 2)
    {
      Expr xmin = xrange[1];
      if(xmin[0] == PMATH_SYMBOL_NCACHE)
        xmin = xmin[2];
        
      Expr xmax = xrange[2];
      if(xmax[0] == PMATH_SYMBOL_NCACHE)
        xmax = xmax[2];
        
      if(xmin.is_number() && xmax.is_number()) {
        bounds.xmin = xmin.to_double();
        bounds.xmax = xmax.to_double();
      }
    }
    
    if( yrange[0] == PMATH_SYMBOL_RANGE &&
        yrange.expr_length() == 2)
    {
      Expr ymin = yrange[1];
      if(ymin[0] == PMATH_SYMBOL_NCACHE)
        ymin = ymin[2];
        
      Expr ymax = yrange[2];
      if(ymax[0] == PMATH_SYMBOL_NCACHE)
        ymax = ymax[2];
        
      if(ymin.is_number() && ymax.is_number()) {
        bounds.ymin = ymin.to_double();
        bounds.ymax = ymax.to_double();
      }
    }
  }
  
  if(!bounds.is_finite()) {
    GraphicsBounds auto_bounds;
    elements.find_extends(auto_bounds);
    
    double ox = clip(0.0, bounds.xmin, bounds.xmax);
    double oy = clip(0.0, bounds.ymin, bounds.ymax);
    try_get_axes_origin(auto_bounds, &ox, &oy);
    
    auto_bounds.add_point(ox, oy);
    
    if(!isfinite(bounds.xmin)) bounds.xmin = auto_bounds.xmin;
    if(!isfinite(bounds.xmax)) bounds.xmax = auto_bounds.xmax;
    if(!isfinite(bounds.ymin)) bounds.ymin = auto_bounds.ymin;
    if(!isfinite(bounds.ymax)) bounds.ymax = auto_bounds.ymax;
  }
  
  if( !isfinite(bounds.xmin) ||
      !isfinite(bounds.xmax) ||
      bounds.xmin > bounds.xmax)
  {
    bounds.xmin = -1;
    bounds.xmax = 1;
  }
  
  if( !isfinite(bounds.ymin) ||
      !isfinite(bounds.ymax) ||
      bounds.ymin > bounds.ymax)
  {
    bounds.ymin = -1;
    bounds.ymax = 1;
  }
  
  if(bounds.xmin == bounds.xmax) {
    double dist = 0.5 * max(fabs(bounds.xmin), fabs(0.5 * bounds.ymin + 0.5 * bounds.ymax));
    
    bounds.xmin -= dist;
    bounds.xmax += dist;
    
    if(bounds.xmin == bounds.xmax) {
      bounds.xmin -= 1;
      bounds.xmax += 1;
    }
  }
  
  
  if(bounds.ymin == bounds.ymax) {
    double dist = 0.5 * max(fabs(bounds.ymin), fabs(0.5 * bounds.xmin + 0.5 * bounds.xmax));
    
    bounds.ymin -= dist;
    bounds.ymax += dist;
    
    if(bounds.ymin == bounds.ymax) {
      bounds.ymin -= 1;
      bounds.ymax += 1;
    }
  }
  return bounds;
}

bool GraphicsBox::have_frame(bool *left, bool *right, bool *bottom, bool *top) {
  *left   = false;
  *right  = false;
  *bottom = false;
  *top    = false;
  
  Expr e = get_own_style(Frame);
  
  if(e == PMATH_SYMBOL_FALSE || e == PMATH_SYMBOL_NONE)
    return false;
    
  if(e == PMATH_SYMBOL_TRUE || e == PMATH_SYMBOL_ALL) {
    *left   = true;
    *right  = true;
    *bottom = true;
    *top    = true;
    return true;
  }
  
  if(e[0] == PMATH_SYMBOL_LIST && e.expr_length() == 2) {
    Expr sub = e[1];
    if(sub == PMATH_SYMBOL_TRUE) {
      *left   = true;
      *right  = true;
    }
    else if(sub[0] == PMATH_SYMBOL_LIST && sub.expr_length() == 2) {
      if(sub[1] == PMATH_SYMBOL_TRUE)
        *left = true;
        
      if(sub[2] == PMATH_SYMBOL_TRUE)
        *right = true;
    }
    
    sub = e[2];
    if(sub == PMATH_SYMBOL_TRUE) {
      *bottom = true;
      *top    = true;
    }
    else if(sub[0] == PMATH_SYMBOL_LIST && sub.expr_length() == 2) {
      if(sub[1] == PMATH_SYMBOL_TRUE)
        *bottom = true;
        
      if(sub[2] == PMATH_SYMBOL_TRUE)
        *top = true;
    }
    
    return *left || *right || *bottom || *top;
  }
  
  return false;
}

bool GraphicsBox::have_axes(bool *x, bool *y) {
  *x = false;
  *y = false;
  
  Expr e = get_own_style(Axes);
  
  if(e == PMATH_SYMBOL_FALSE)
    return false;
    
  if(e == PMATH_SYMBOL_TRUE) {
    *x = true;
    *y = true;
    return true;
  }
  
  if(e[0] == PMATH_SYMBOL_LIST && e.expr_length() == 2) {
    if(e[1] == PMATH_SYMBOL_TRUE)
      *x = true;
      
    if(e[2] == PMATH_SYMBOL_TRUE)
      *y = true;
      
    return *x || *y;
  }
  
  return false;
}

Expr GraphicsBox::generate_default_ticks(double min, double max, bool with_labels) {
  return Evaluate(
           Parse(
             "FE`Graphics`DefaultTickBoxes(`1`, `2`, `3`)",
             min,
             max,
             Symbol(with_labels ? PMATH_SYMBOL_TRUE : PMATH_SYMBOL_FALSE)));
}

Expr GraphicsBox::generate_ticks(const GraphicsBounds &bounds, enum AxisIndex part) {
  Expr ticks = get_ticks(bounds, part);
  
  if(ticks[0] == PMATH_SYMBOL_LIST)
    return ticks;
    
  if(ticks == PMATH_SYMBOL_NONE)
    return List();
    
  if(ticks == PMATH_SYMBOL_ALL) {
    switch(part) {
      case AxisIndexX:
      case AxisIndexBottom:
      case AxisIndexTop:
        return generate_default_ticks(bounds.xmin, bounds.xmax, true);
        
      case AxisIndexY:
      case AxisIndexLeft:
      case AxisIndexRight:
        return generate_default_ticks(bounds.ymin, bounds.ymax, true);
    }
  }
  
  if(ticks == PMATH_SYMBOL_AUTOMATIC) {
    switch(part) {
      case AxisIndexX:
      case AxisIndexBottom:
        return generate_default_ticks(bounds.xmin, bounds.xmax, true);
        
      case AxisIndexTop:
        return generate_default_ticks(bounds.xmin, bounds.xmax, false);
        
      case AxisIndexY:
      case AxisIndexLeft:
        return generate_default_ticks(bounds.ymin, bounds.ymax, true);
        
      case AxisIndexRight:
        return generate_default_ticks(bounds.ymin, bounds.ymax, false);
    }
  }
  
  return List();
}

Expr GraphicsBox::get_ticks(const GraphicsBounds &bounds, enum AxisIndex part) {
  Expr e;
  switch(part) {
    case AxisIndexX:
    case AxisIndexY:
      e = get_own_style(Ticks, Symbol(PMATH_SYMBOL_AUTOMATIC));
      break;
      
    case AxisIndexLeft:
    case AxisIndexRight:
    case AxisIndexBottom:
    case AxisIndexTop:
      e = get_own_style(FrameTicks, Symbol(PMATH_SYMBOL_AUTOMATIC));
      break;
  }
  
  if(e[0] == PMATH_SYMBOL_NCACHE)
    e = e[2];
    
  if(e == PMATH_SYMBOL_TRUE)
    return Symbol(PMATH_SYMBOL_AUTOMATIC);
    
  if(e[0] == PMATH_SYMBOL_LIST) {
    if(e.expr_length() != 2)
      return List();
      
    Expr sub;
    switch(part) {
      case AxisIndexX: return e[1];
      case AxisIndexY: return e[2];
      
      case AxisIndexLeft:
      case AxisIndexRight:
        sub = e[1];
        break;
        
      case AxisIndexBottom:
      case AxisIndexTop:
        sub = e[2];
        break;
    }
    
    if(sub[0] == PMATH_SYMBOL_NCACHE)
      sub = sub[2];
      
    if(sub[0] == PMATH_SYMBOL_LIST) {
      if(sub.expr_length() != 2)
        return List();
        
      switch(part) {
        case AxisIndexLeft:
        case AxisIndexBottom:
          return sub[1];
          
        case AxisIndexRight:
        case AxisIndexTop:
          return sub[2];
          
        case AxisIndexX:
        case AxisIndexY:
          assert(!"not reached");
      }
    }
    
    return sub;
  }
  
  return e;
}

bool GraphicsBox::set_axis_ends(enum AxisIndex part, const GraphicsBounds &bounds) { // true if ends changed
  double min, max;
  
  if(part == AxisIndexX || part == AxisIndexBottom || part == AxisIndexTop) {
    min = bounds.xmin;
    max = bounds.xmax;
  }
  else {
    min = bounds.ymin;
    max = bounds.ymax;
  }
  
  if(ticks[part]->start_position != min || ticks[part]->end_position != max) {
    ticks[part]->start_position = min;
    ticks[part]->end_position   = max;
    return true;
  }
  
  return false;
}

void GraphicsBox::resize_axes(Context *context) {
  ContextState cc(context);
  cc.begin(style);
  {
    GraphicsBounds bounds = calculate_plotrange();
    
    bool have_axis[6];
    
    bool any_frame = have_frame(
                       &have_axis[AxisIndexLeft],
                       &have_axis[AxisIndexRight],
                       &have_axis[AxisIndexBottom],
                       &have_axis[AxisIndexTop]);
    have_axes(
      &have_axis[AxisIndexX],
      &have_axis[AxisIndexY]);
      
    for(int part = 0; part < 6; ++part)
      ticks[part]->axis_hidden = !have_axis[part];
      
    if(any_frame) {
      have_axis[AxisIndexX] = false;
      have_axis[AxisIndexY] = false;
    }
    
    for(int part = 0; part < 6; ++part) {
      set_axis_ends((AxisIndex)part, bounds);
      
      if(have_axis[part])
        ticks[part]->load_from_object(generate_ticks(bounds, (AxisIndex)part), BoxOptionFormatNumbers);
      else
        ticks[part]->load_from_object(List(), BoxOptionDefault);
        
      ticks[part]->resize(context);
    }
  }
  cc.end();
}

void GraphicsBox::paint(Context *context) {
  style->update_dynamic(this);
  
  float x, y;
  context->canvas->current_pos(&x, &y);
  
  y -= _extents.ascent;
  
  float w = _extents.width;
  float h = _extents.height();
  
  ContextState cc(context);
  cc.begin(style);
  {
    if(static_error_boxes.is_valid() || dynamic_error_boxes.is_valid())
      context->draw_error_rect(x, y, x + w, y + h);
      
    context->canvas->save();
    {
      context->canvas->pixrect(
        x +     margin_left   - 0.75f,
        y +     margin_top    - 0.75f,
        x + w - margin_right  + 0.75f,
        y + h - margin_bottom + 0.75f,
        false);
      context->canvas->clip();
      
      cairo_matrix_t m;
      cairo_matrix_init_identity(&m);
      
      cairo_matrix_translate(&m, x, y + _extents.ascent);
      transform_inner_to_outer(&m);
      context->canvas->transform(m);
      
      int old_color = context->canvas->get_color();
      
      //context->canvas->set_color(0xff0000);
      //context->canvas->move_to(0, 0);
      //context->canvas->line_to(1, 1);
      //context->canvas->hair_stroke();
      
      context->canvas->set_color(0x000000);
      elements.paint(context);
      
      context->canvas->set_color(old_color);
    }
    context->canvas->restore();
    
    
    context->canvas->save();
    {
      int old_color = context->canvas->get_color();
      context->canvas->set_color(0x000000);
      
      for(int axis = 0; axis < 6; ++axis) {
        if(!ticks[axis]->axis_hidden) {
          float x1 = ticks[axis]->start_x + x;
          float y1 = ticks[axis]->start_y + y + _extents.ascent;
          float x2 = ticks[axis]->end_x   + x;
          float y2 = ticks[axis]->end_y   + y + _extents.ascent;
          
          context->canvas->align_point(&x1, &y1, true);
          context->canvas->align_point(&x2, &y2, true);
          
          context->canvas->move_to(x1, y1);
          context->canvas->line_to(x2, y2);
          context->canvas->hair_stroke();
          
        }
      }
      
      for(int axis = 0; axis < 6; ++axis) {
        if(!ticks[axis]->axis_hidden) {
          context->canvas->move_to(x, y + _extents.ascent);
          ticks[axis]->paint(context);
        }
      }
      
      context->canvas->set_color(old_color);
    }
    context->canvas->restore();
    
    if(context->selection.equals(this, 0, 0)) {
      context->canvas->save();
      
      context->canvas->pixrect(x, y, x + w, y + h, true);
      
      context->canvas->set_color(0xFF8000);
      context->canvas->hair_stroke();
      
      context->canvas->pixrect(
        x + w - 2.25,
        y + h / 2 - 1.5,
        x + w,
        y + h / 2 + 0.75,
        false);
        
      context->canvas->pixrect(
        x + w - 2.25,
        y + h - 2.25,
        x + w,
        y + h,
        false);
        
      context->canvas->pixrect(
        x + w / 2 - 1.5,
        y + h - 2.25,
        x + w / 2 + 0.75,
        y + h,
        false);
        
      context->canvas->fill();
      
      context->canvas->restore();
    }
  }
  cc.end();
}

void GraphicsBox::reset_style() {
  if(style)
    style->clear();
  else
    style = new Style();
    
  style->set(BaseStyleName, "Graphics");
}

Expr GraphicsBox::to_pmath(int flags) {
  Gather g;
  
  Gather::emit(elements.to_pmath(flags));
  style->emit_to_pmath(false);
  
  Expr result = g.end();
  result.set(0, Symbol(PMATH_SYMBOL_GRAPHICSBOX));
  return result;
}

int GraphicsBox::calc_mouse_over_part(float x, float y) {
  y += _extents.ascent;
  
  float w = _extents.width;
  float h = _extents.height();
  
  if(x < 0 || x > w || y < 0 || y > h)
    return GraphicsPartNone;
    
  if(x >= w - 4.5) {
    if(y >= h - 4.5)
      return GraphicsPartSizeBottomRight;
      
    if(y >= h / 2 - 2.25 && y <= h / 2 + 2.25)
      return GraphicsPartSizeRight;
  }
  else if(y >= h - 2.25) {
    if(x >= w / 2 - 2.25 && x <= w / 2 + 2.25)
      return GraphicsPartSizeBottom;
  }
  
  return GraphicsPartBackground;
}

void GraphicsBox::transform_inner_to_outer(cairo_matrix_t *mat) {
  cairo_matrix_scale(mat, 1, -1);
  
  cairo_matrix_translate(
    mat,
    margin_left,
    margin_bottom - _extents.descent);
    
  double sx = _extents.width    - margin_left - margin_right;
  double sy = _extents.height() - margin_top  - margin_bottom;
  if(sx == 0) sx = 1;
  if(sy == 0) sy = 1;
  
  cairo_matrix_scale(mat, sx, sy);
  
  sx = ticks[AxisIndexBottom]->end_position - ticks[AxisIndexBottom]->start_position;
  sy = ticks[AxisIndexLeft  ]->end_position - ticks[AxisIndexLeft  ]->start_position;
  if(sx == 0) sx = 1;
  if(sy == 0) sy = 1;
  
  cairo_matrix_scale(mat, 1 / sx, 1 / sy);
  
  cairo_matrix_translate(
    mat,
    - ticks[AxisIndexBottom]->start_position,
    - ticks[AxisIndexLeft  ]->start_position);
}

Box *GraphicsBox::mouse_selection(
  float  x,
  float  y,
  int   *start,
  int   *end,
  bool  *was_inside_start
) {
  for(int axis = 0; axis < 6; ++axis) {
    Box *tmp = ticks[axis]->mouse_selection(x, y, start, end, was_inside_start);
    
    if(tmp != ticks[axis])
      return tmp;
  }
  
  //if(!selectable())
  //  return Box::mouse_selection(x, y, start, end, was_inside_start);
  
  *was_inside_start = false;
  *start = *end = 0;
  return this;
}

bool GraphicsBox::selectable(int i) {
  if(i < 0)
    return Box::selectable(i);
    
  return false;
}

Box *GraphicsBox::normalize_selection(int *start, int *end) {
  *start = *end = 0;
  
  return this;
}

Box *GraphicsBox::mouse_sensitive() {
  Box *box = Box::mouse_sensitive();
  
  if(box && !dynamic_cast<Document *>(box) && !dynamic_cast<InputFieldBox *>(box))
    return box;
    
  return this;
}

void GraphicsBox::on_mouse_enter() {
  if(error_boxes_expr.is_valid() || dynamic_error_boxes.is_valid()) {
    Document *doc = find_parent<Document>(false);
    
    if(doc) {
      Expr error_boxes = error_boxes_expr;
      if(dynamic_error_boxes.is_valid())
        error_boxes = List(error_boxes, String("\n"), dynamic_error_boxes);
      
      doc->native()->show_tooltip(error_boxes);
    }
  }
}

void GraphicsBox::on_mouse_exit() {
  if(error_boxes_expr.is_valid()) {
    Document *doc = find_parent<Document>(false);
    
    if(doc)
      doc->native()->hide_tooltip();
  }
}

void GraphicsBox::on_mouse_down(MouseEvent &event) {
  event.set_source(this);
  
  int part = calc_mouse_over_part(event.x, event.y);
  
  mouse_over_part = part == GraphicsPartNone ? GraphicsPartNone : GraphicsPartBackground;
  
  Document *doc = find_parent<Document>(false);
  if(doc) {
    if(doc->selection_box() != this)
      doc->select(this, 0, 0);
    else
      mouse_over_part = part;
  }
  
  mouse_down_x = event.x;
  mouse_down_y = event.y + _extents.ascent;
  
  Box::on_mouse_down(event);
}

void GraphicsBox::on_mouse_move(MouseEvent &event) {

  if(event.left) {
    event.set_source(this);
    
    float dx = 0;
    float dy = 0;
    
    switch(mouse_over_part) {
      case GraphicsPartSizeRight:
        dx = event.x - mouse_down_x;
        
        //mouse_down_x = event.x;
        break;
        
      case GraphicsPartSizeBottom:
        dy = event.y + _extents.ascent - mouse_down_y;
        
        //mouse_down_y = event.y + _extents.ascent;
        break;
        
      case GraphicsPartSizeBottomRight:
        dx = event.x                   - mouse_down_x;
        dy = event.y + _extents.ascent - mouse_down_y;
        
        //mouse_down_x = event.x;
        //mouse_down_y = event.y + _extents.ascent;
        break;
    }
    
    if(dx != 0 || dy != 0) {
      float w = _extents.width    + dx;
      float h = _extents.height() + dy;
      
      if(w < 0.75)
        w = 0.75;
      if(h < 0.75)
        h = 0.75;
        
      if(_extents.height() > 0 && _extents.width > 0) {
        switch(mouse_over_part) {
          case GraphicsPartSizeRight:
            h = w / _extents.width * _extents.height();
            break;
            
          case GraphicsPartSizeBottom:
            w = h / _extents.height() * _extents.width;
            break;
            
          default:
            if(w > h)
              h = w / _extents.width * _extents.height();
            else
              w = h / _extents.height() * _extents.width;
        }
        
      }
      
      if(mouse_over_part == GraphicsPartSizeBottom) {
        style->set(ImageSizeHorizontal, ImageSizeAutomatic);
        style->set(ImageSizeVertical,   h);
      }
      else {
        style->set(ImageSizeHorizontal, w);
        style->set(ImageSizeVertical,   ImageSizeAutomatic);
      }
      
      user_has_changed_size = true;
      invalidate();
      
      mouse_down_x += w - _extents.width;
      mouse_down_y += h - _extents.height();
    }
  }
  
  if( !event.left &&
      !event.middle &&
      !event.right)
  {
    event.set_source(this);
    
    int part = calc_mouse_over_part(event.x, event.y);
    
    mouse_over_part = part == GraphicsPartNone ? GraphicsPartNone : GraphicsPartBackground;
    
    Document *doc = find_parent<Document>(false);
    if(doc) {
      if(doc->selection_box() == this) {
        mouse_over_part = part;
      }
      
      switch(mouse_over_part) {
        case GraphicsPartSizeRight:
          doc->native()->set_cursor(NativeWidget::size_cursor(this, SizeECursor));
          break;
          
        case GraphicsPartSizeBottom:
          doc->native()->set_cursor(NativeWidget::size_cursor(this, SizeSCursor));
          break;
          
        case GraphicsPartSizeBottomRight:
          doc->native()->set_cursor(NativeWidget::size_cursor(this, SizeSECursor));
          break;
          
        default:
          doc->native()->set_cursor(DefaultCursor);
      }
    }
  }
  
  Box::on_mouse_move(event);
}

void GraphicsBox::on_mouse_up(MouseEvent &event) {

  Box::on_mouse_up(event);
}

//} ... class GraphicsBox
