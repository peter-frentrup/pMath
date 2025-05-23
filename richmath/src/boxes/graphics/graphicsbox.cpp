#include <boxes/graphics/graphicsbox.h>

#include <boxes/graphics/axisticks.h>
#include <boxes/graphics/graphicsdirective.h>
#include <boxes/graphics/graphicsdrawingcontext.h>

#include <boxes/dynamicbox.h>
#include <boxes/fillbox.h>
#include <boxes/gridbox.h>
#include <boxes/inputfieldbox.h>
#include <boxes/paneselectorbox.h>
#include <boxes/section.h>
#include <boxes/stylebox.h>

#include <graphics/context.h>

#include <gui/document.h>
#include <gui/native-widget.h>

#include <syntax/spanexpr.h>

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
#  define NAN  (std::numeric_limits<double>::quiet_NaN())
#endif


using namespace richmath;
using namespace std;

extern pmath_symbol_t richmath_System_All;
extern pmath_symbol_t richmath_System_Automatic;
extern pmath_symbol_t richmath_System_Axis;
extern pmath_symbol_t richmath_System_Baseline;
extern pmath_symbol_t richmath_System_Bottom;
extern pmath_symbol_t richmath_System_Center;
extern pmath_symbol_t richmath_System_False;
extern pmath_symbol_t richmath_System_GraphicsBox;
extern pmath_symbol_t richmath_System_List;
extern pmath_symbol_t richmath_System_NCache;
extern pmath_symbol_t richmath_System_None;
extern pmath_symbol_t richmath_System_Range;
extern pmath_symbol_t richmath_System_Scaled;
extern pmath_symbol_t richmath_System_Top;
extern pmath_symbol_t richmath_System_True;

extern pmath_symbol_t richmath_FE_Graphics_DefaultAxesOrigin;
extern pmath_symbol_t richmath_FE_Graphics_DefaultTickBoxes;

namespace richmath { namespace strings {
  extern String Graphics;
}}

template<typename T>
static T max(const T &a, const T &b, const T &c) {
  return std::max(std::max(a, b), c);
}

template<typename T>
static T max(const T &a, const T &b, const T &c, const T &d) {
  return std::max(std::max(a, b), std::max(c, d));
}


enum SyntaxPosition {
  Alone,
  InsideList,
  InsideOther
};

class GraphicsBox::Impl {
  public:
    Impl(GraphicsBox &_self) : self(_self) {}
    
    static enum SyntaxPosition find_syntax_position(Box *box, int index);
    
    float calculate_ascent_for_baseline_position(float em, Expr baseline_pos) const;
    
    static float calc_margin_width(float w, float lbl_w, double all_x, double other_x);
    void calculate_size(Vector2F max_auto_size, const float *optional_expand_width = nullptr);
    
    void try_get_axes_origin(const GraphicsBounds &bounds, double *ox, double *oy);
    void calculate_axes_origin(const GraphicsBounds &bounds, double *ox, double *oy);
    
    GraphicsBounds calculate_plotrange();
    static bool get_range_from_pmath(Interval<double> &result, Expr expr);
    static bool get_double_from_pmath(double &result, Expr expr);
    bool have_frame(bool *left, bool *right, bool *bottom, bool *top);
    bool have_axes(bool *x, bool *y);
    
    static Expr generate_default_ticks(const Interval<double> &range, bool with_labels);
    Expr generate_ticks(const GraphicsBounds &bounds, AxisIndex part);
    Expr get_ticks(const GraphicsBounds &bounds, AxisIndex part);
    
    bool set_axis_ends(AxisIndex part, const GraphicsBounds &bounds); // true if ends changed
    
    void resize_axes(Context &context);
      
  private:
    GraphicsBox &self;
};

//{ class GraphicsBox ...

GraphicsBox::GraphicsBox()
  : Box(),
    mouse_over_part(GraphicsPartNone),
    mouse_down_x(0),
    mouse_down_y(0),
    elements(this)
{
  reset_style();
  
  for(int part = 0; part < 6; ++part) {
    ticks[part] = new AxisTicks;
    adopt(ticks[part], part);
  }
  
  ticks[AxisIndexX     ]->label_direction.x =  0;
  ticks[AxisIndexBottom]->label_direction.x =  0;
  ticks[AxisIndexTop   ]->label_direction.x =  0;
  ticks[AxisIndexX     ]->label_direction.y =  1;
  ticks[AxisIndexBottom]->label_direction.y =  1;
  ticks[AxisIndexTop   ]->label_direction.y = -1;
  
  ticks[AxisIndexY     ]->label_direction.x = -1;
  ticks[AxisIndexLeft  ]->label_direction.x = -1;
  ticks[AxisIndexRight ]->label_direction.x =  1;
  ticks[AxisIndexY     ]->label_direction.y =  0;
  ticks[AxisIndexLeft  ]->label_direction.y =  0;
  ticks[AxisIndexRight ]->label_direction.y =  0;
}

GraphicsBox::~GraphicsBox() {
  for(int part = 0; part < 6; ++part)
    ticks[part]->safe_destroy();
}

bool GraphicsBox::try_load_from_object(Expr expr, BoxInputFlags opts) {
  if(!expr.item_equals(0, richmath_System_GraphicsBox))
    return false;
    
  if(expr.expr_length() < 1)
    return false;
    
  Expr options(PMATH_UNDEFINED);
  
  if(expr.expr_length() > 1) {
    options = Expr(pmath_options_extract_ex(expr.get(), 1, PMATH_OPTIONS_EXTRACT_UNKNOWN_WARNONLY));
    
    if(options.is_null())
      return false;
  }
  
  /* now success is guaranteed */
  
  Expr user_options = get_user_options();
  
  reset_style();
  style.add_pmath(options);
  style.add_pmath(user_options);
  
  elements.load_from_object(expr[1], opts);
  invalidate();
  
  finish_load_from_object(PMATH_CPP_MOVE(expr));
  return true;
}

void GraphicsBox::reset_user_options() {
  user_has_changed_size(false);
  if(style) {
    style.remove(ImageSizeHorizontal);
    style.remove(ImageSizeVertical);
    invalidate();
  }
}

void GraphicsBox::set_user_default_options(Expr rules) {
  if(rules.expr_length() > 0) {
    user_has_changed_size(true);
    
    Style old_style = style;
    style.reset_new();
    reset_style();
    style.add_pmath(rules);
    style.merge(old_style);
  }
}

Expr GraphicsBox::get_user_options() {
  if(!user_has_changed_size())
    return List();
    
  Gather g;
  
  style.emit_pmath(AspectRatio);
  style.emit_pmath(ImageSizeCommon);
  
  return g.end();
}

Box *GraphicsBox::item(int i) {
  RICHMATH_ASSERT(0 <= i && i < 6);
  
  return ticks[i];
}

int GraphicsBox::count() {
  return 6;
}

bool GraphicsBox::expand(Context &context, const BoxSize &size) {
  auto seq = dynamic_cast<MathSequence*>(parent());
  if(seq && seq->length() == 1) {
    if(dynamic_cast<FillBox *>(seq->parent())) {
      BoxSize old_size = _extents;
      Impl(*this).calculate_size(Vector2F(Infinity, Infinity), &size.width);
      
      if(old_size != _extents)
        cached_bitmap = nullptr;
        
      return true;
    }
  }
  
  return false;
}

void GraphicsBox::invalidate() {
  if(!is_currently_resizing())
    Box::invalidate();
}

bool GraphicsBox::request_repaint(const RectangleF &rect) {
  if(is_currently_resizing())
    return false;
    
  cached_bitmap = nullptr;
  return Box::request_repaint(rect);
}

void GraphicsBox::resize(Context &context) {
  update_simple_dynamic_styles_on_resize(context);
  
  is_currently_resizing(true);
  cached_bitmap = nullptr;
  
  em = context.canvas().get_font_size();
  
  Impl(*this).resize_axes(context);
  
  margin_left   = 0;
  margin_right  = 0;
  margin_top    = 0;
  margin_bottom = 0;
  
  Impl(*this).calculate_size({context.width, 0.5f * context.page_or_monitor_height});
  
  is_currently_resizing(false);
}

void GraphicsBox::paint(Context &context) {
  error_boxes_expr = Expr();
  
  update_dynamic_styles_on_paint(context);
  
  float w = _extents.width;
  float h = _extents.height();
  
  Point p = context.canvas().current_pos();
  
  if(Color bg = get_own_style(Background)) {
    Color c = context.canvas().get_color();
    context.canvas().set_color(bg);
    context.canvas().pixrect(_extents.to_rectangle(p), false);
    context.canvas().fill();
    context.canvas().set_color(c);
    context.canvas().move_to(p);
  }
  
  if(!cached_bitmap.is_valid() || !cached_bitmap->is_compatible(context.canvas())) {
    cached_bitmap = new Buffer(context.canvas(), CAIRO_FORMAT_ARGB32, _extents);
    if(!cached_bitmap->canvas()) 
      cached_bitmap = nullptr;
    
    context.with_canvas(cached_bitmap ? *cached_bitmap->canvas() : context.canvas(), [&]() {
    
      ContextState cc(context);
      cc.begin(style);
      {
        Point p0 = context.canvas().current_pos();
        p0.y -= _extents.ascent;
        p0 = context.canvas().align_point(p0, false);
        
        if(error_boxes_expr.is_valid())
          context.draw_error_rect({p0, Vector2F{w, h}});
          
        context.canvas().save();
        {
          GraphicsDrawingContext gc{*this, context};
          gc.plot_range_width = w - margin_left - margin_right;
          
          gc.init_edgeform_from_style();
          gc.apply_thickness(SymbolicSize::Automatic);
          
          context.canvas().pixrect(
            p0.x +     margin_left   - 0.75f,
            p0.y +     margin_top    - 0.75f,
            p0.x + w - margin_right  + 0.75f,
            p0.y + h - margin_bottom + 0.75f,
            false);
          context.canvas().clip();
          
          cairo_matrix_t m;
          cairo_matrix_init_identity(&m);
          
          cairo_matrix_translate(&m, p0.x, p0.y + _extents.ascent);
          transform_inner_to_outer(&m);
          context.canvas().transform(m);
          
          Color old_color = context.canvas().get_color();
          
          //context.canvas().set_color(0xff0000);
          //context.canvas().move_to(0, 0);
          //context.canvas().line_to(1, 1);
          //context.canvas().hair_stroke();
          
          context.canvas().set_color(Color::Black);
          elements.paint(gc);
          
          context.canvas().set_color(old_color);
        }
        context.canvas().restore();
        
        
        context.canvas().save();
        {
          Color old_color = context.canvas().get_color();
          context.canvas().set_color(Color::Black);
          
          for(int axis = 0; axis < 6; ++axis) {
            if(!ticks[axis]->axis_hidden()) {
              Point p1 = p0 + Vector2F{ticks[axis]->start_pos};
              Point p2 = p0 + Vector2F{ticks[axis]->end_pos};
              
              p1 = context.canvas().align_point(p1, true);
              p2 = context.canvas().align_point(p2, true);
              
              context.canvas().move_to(p1);
              context.canvas().line_to(p2);
              context.canvas().hair_stroke();
            }
          }
          
          for(int axis = 0; axis < 6; ++axis) {
            if(!ticks[axis]->axis_hidden()) {
              context.canvas().move_to(p0);
              ticks[axis]->paint(context);
            }
          }
          
          context.canvas().set_color(old_color);
        }
        context.canvas().restore();
      }
      cc.end();
    });
  }
  
  if(cached_bitmap.is_valid())
    cached_bitmap->paint(context.canvas());
  
  p.y -= _extents.ascent;
  
  bool has_sel = false;
  context.for_each_selection_at(this, [&](const VolatileSelection &sel) {
    if(has_sel) 
      return;
    
    has_sel = true;
    
    context.canvas().save();
    context.canvas().pixrect(p.x, p.y, p.x + w, p.y + h, true);
    
    context.canvas().set_color(Color::from_rgb24(0xFF8000));
    context.canvas().hair_stroke();
    
    context.canvas().pixrect(
      p.x + w - 2.25,
      p.y + h / 2 - 1.5,
      p.x + w,
      p.y + h / 2 + 0.75,
      false);
      
    context.canvas().pixrect(
      p.x + w - 2.25,
      p.y + h - 2.25,
      p.x + w,
      p.y + h,
      false);
      
    context.canvas().pixrect(
      p.x + w / 2 - 1.5,
      p.y + h - 2.25,
      p.x + w / 2 + 0.75,
      p.y + h,
      false);
      
    context.canvas().fill();
    
    context.canvas().restore();
  });
}

void GraphicsBox::reset_style() {
  style.reset(strings::Graphics);
}

Expr GraphicsBox::to_pmath_symbol() {
  return Symbol(richmath_System_GraphicsBox);
}

Expr GraphicsBox::to_pmath_impl(BoxOutputFlags flags) {
  Gather g;
  
  Gather::emit(elements.to_pmath(flags));
  style.emit_to_pmath(false);
  
  Expr result = g.end();
  result.set(0, Symbol(richmath_System_GraphicsBox));
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
  
  sx = ticks[AxisIndexBottom]->range.length();
  sy = ticks[AxisIndexLeft  ]->range.length();
  if(sx == 0) sx = 1;
  if(sy == 0) sy = 1;
  
  cairo_matrix_scale(mat, 1 / sx, 1 / sy);
  
  cairo_matrix_translate(
    mat,
    - ticks[AxisIndexBottom]->range.from,
    - ticks[AxisIndexLeft  ]->range.from);
}

VolatileSelection GraphicsBox::mouse_selection(Point pos, bool *was_inside_start) {
  for(int axis = 0; axis < 6; ++axis) {
    VolatileSelection tmp = ticks[axis]->mouse_selection(pos, was_inside_start);
    
    if(tmp.box != ticks[axis])
      return tmp;
  }
  
  *was_inside_start = false;
  return { this, 0, 0 };
}

bool GraphicsBox::selectable(int i) {
  if(i < 0)
    return Box::selectable(i);
    
  return false;
}

VolatileSelection GraphicsBox::normalize_selection(int start, int end) {
  return {this, 0, 0};
}

Box *GraphicsBox::mouse_sensitive() {
  Box *box = Box::mouse_sensitive();
  if(!box)
    return this;
  
  if(!selectable())
    return box;
    
  if(!dynamic_cast<Document *>(box) && !dynamic_cast<InputFieldBox *>(box))
    return box;
    
  return this;
}

void GraphicsBox::on_mouse_enter() {
  if(error_boxes_expr.is_valid()) {
    if(auto doc = find_parent<Document>(false))
      doc->native()->show_tooltip(this, error_boxes_expr);
  }
}

void GraphicsBox::on_mouse_exit() {
  if(error_boxes_expr.is_valid()) {
    if(auto doc = find_parent<Document>(false))
      doc->native()->hide_tooltip();
  }
}

void GraphicsBox::on_mouse_down(MouseEvent &event) {
  event.set_origin(this);
  
  int part = calc_mouse_over_part(event.position.x, event.position.y);
  
  mouse_over_part = part == GraphicsPartNone ? GraphicsPartNone : GraphicsPartBackground;
  
  if(auto doc = find_parent<Document>(false)) {
    if(doc->selection_box() != this)
      doc->select(this, 0, 0);
    else
      mouse_over_part = part;
  }
  
  mouse_down_x = event.position.x;
  mouse_down_y = event.position.y + _extents.ascent;
  
  Box::on_mouse_down(event);
}

void GraphicsBox::on_mouse_move(MouseEvent &event) {

  if(event.left) {
    event.set_origin(this);
    
    float dx = 0;
    float dy = 0;
    
    switch(mouse_over_part) {
      case GraphicsPartSizeRight:
        dx = event.position.x - mouse_down_x;
        break;
        
      case GraphicsPartSizeBottom:
        dy = event.position.y + _extents.ascent - mouse_down_y;
        break;
        
      case GraphicsPartSizeBottomRight:
        dx = event.position.x                   - mouse_down_x;
        dy = event.position.y + _extents.ascent - mouse_down_y;
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
        style.set(ImageSizeHorizontal, SymbolicSize::Automatic);
        style.set(ImageSizeVertical,   Length(h));
      }
      else {
        style.set(ImageSizeHorizontal, Length(w));
        style.set(ImageSizeVertical,   SymbolicSize::Automatic);
      }
      
      user_has_changed_size(true);
      invalidate();
      
      mouse_down_x += w - _extents.width;
      mouse_down_y += h - _extents.height();
    }
  }
  
  if( !event.left &&
      !event.middle &&
      !event.right)
  {
    event.set_origin(this);
    
    int part = calc_mouse_over_part(event.position.x, event.position.y);
    mouse_over_part = part == GraphicsPartNone ? GraphicsPartNone : GraphicsPartBackground;
    
    if(auto doc = find_parent<Document>(false)) {
      if(doc->selection_box() == this) {
        mouse_over_part = part;
      }
      
      switch(mouse_over_part) {
        case GraphicsPartSizeRight:
          doc->native()->set_cursor(NativeWidget::size_cursor(this, CursorType::SizeE));
          break;
          
        case GraphicsPartSizeBottom:
          doc->native()->set_cursor(NativeWidget::size_cursor(this, CursorType::SizeS));
          break;
          
        case GraphicsPartSizeBottomRight:
          doc->native()->set_cursor(NativeWidget::size_cursor(this, CursorType::SizeSE));
          break;
          
        default:
          doc->native()->set_cursor(CursorType::Default);
      }
    }
  }
  
  Box::on_mouse_move(event);
}

void GraphicsBox::on_mouse_up(MouseEvent &event) {

  Box::on_mouse_up(event);
}

//} ... class GraphicsBox

//{ class GraphicsBox::Impl ...

enum SyntaxPosition GraphicsBox::Impl::find_syntax_position(Box *box, int index) {
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
      dynamic_cast<GridItem *>(box) ||
      dynamic_cast<PaneSelectorBox *>(box) )
  {
    return find_syntax_position(box->parent(), box->index());
  }
  
  if(dynamic_cast<GridBox*>(box)) {
    if(!box->get_own_style(AllowScriptLevelChange, true))
      return find_syntax_position(box->parent(), box->index());
    
    enum SyntaxPosition pos = find_syntax_position(box->parent(), box->index());
    
    if(pos < InsideList)
      return InsideList;
      
    return pos;
  }
  
  if(dynamic_cast<OwnerBox *>(box)) {
    enum SyntaxPosition pos = find_syntax_position(box->parent(), box->index());
    
    if(pos < InsideList)
      return InsideList;
      
    return pos;
  }
  
  return InsideOther;
}

float GraphicsBox::Impl::calculate_ascent_for_baseline_position(float em, Expr baseline_pos) const {
  float height = self._extents.height();
  
  if(baseline_pos == richmath_System_Bottom) 
    return height;
  
  if(baseline_pos == richmath_System_Top) 
    return 0;
  
  if(baseline_pos == richmath_System_Center) 
    return 0.5f * height;
  
  if(baseline_pos == richmath_System_Axis) 
    return self.ticks[AxisIndexX]->start_pos.y;
  
  if(baseline_pos.item_equals(0, richmath_System_Scaled)) {
    double factor = 0.0;
    if(get_factor_of_scaled(baseline_pos, &factor) && isfinite(factor)) {
      return height - height * factor;
    }
  }
  else if(baseline_pos.is_rule()) {
    float lhs_ascent = calculate_ascent_for_baseline_position(em, baseline_pos[1]);
    Expr rhs = baseline_pos[2];
    
    if(rhs == richmath_System_Axis) {
      float ref_pos = 0.25f * em; // TODO: use actual math axis from font
      return lhs_ascent + ref_pos;
    }
    else if(rhs == richmath_System_Baseline) {
      float ref_pos = 0.0f;
      return lhs_ascent + ref_pos;
    }
    else if(rhs == richmath_System_Bottom) {
      float ref_pos = -0.25f * em;
      return lhs_ascent + ref_pos;
    }
    else if(rhs == richmath_System_Center) {
      float ref_pos = 0.25f * em;
      return lhs_ascent + ref_pos;
    }
    else if(rhs == richmath_System_Top) {
      float ref_pos = 0.75f * em;
      return lhs_ascent + ref_pos;
    }
    else if(rhs.item_equals(0, richmath_System_Scaled)) {
      double factor = 0.0;
      if(get_factor_of_scaled(rhs, &factor) && isfinite(factor)) {
        //float ref_pos = 0.75 * em * factor - 0.25 * em * (1 - factor);
        float ref_pos = (factor - 0.25) * em;
        return lhs_ascent + ref_pos;
      }
    }
  }
  
  //if(baseline_pos == richmath_System_Automatic) 
  //  return 0.5f * height + 0.25f * em; // TODO: use actual math axis from font
  //
  return self._extents.ascent;
}

float GraphicsBox::Impl::calc_margin_width(float w, float lbl_w, double all_x, double other_x) {
  if(other_x > all_x)
    other_x = all_x;
    
  if(w <= 0)
    return lbl_w;
    
  double min_ox = (w - lbl_w) * all_x / w;
  if(other_x < min_ox)
    return 0;
    
  return w - (w - lbl_w) * all_x / other_x;
}

void GraphicsBox::Impl::calculate_size(Vector2F max_auto_size, const float *optional_expand_width) {
  GraphicsBounds bounds;
  bounds.x_range = self.ticks[AxisIndexX]->range;
  bounds.y_range = self.ticks[AxisIndexY]->range;
  
  double ox = 0, oy = 0;
  calculate_axes_origin(bounds, &ox, &oy);
  bool valid_ox = self.ticks[AxisIndexBottom]->is_visible(ox);
  bool valid_oy = self.ticks[AxisIndexLeft  ]->is_visible(oy);
  
  BoxSize label_sizes[6];
  for(int part = 0; part < 6; ++part) {
    if(self.ticks[part]->axis_hidden()) {
      label_sizes[part] = BoxSize(0, 0, 0);
      
      self.ticks[part]->extra_offset = 0;
    }
    else {
      label_sizes[part] = self.ticks[part]->all_labels_extents();
      
      self.ticks[part]->extra_offset = 0.75 * 6;
    }
  }
  
  bool left, right, bottom, top;
  bool any_frame = have_frame(&left, &right, &bottom, &top);
  if(any_frame) {
    self.margin_left = max(
                         label_sizes[AxisIndexLeft].width + self.ticks[AxisIndexLeft]->extra_offset,
                         label_sizes[AxisIndexBottom].width / 2,
                         label_sizes[AxisIndexTop].width / 2);
                    
    self.margin_right = max(
                          label_sizes[AxisIndexRight].width + self.ticks[AxisIndexRight]->extra_offset,
                          label_sizes[AxisIndexBottom].width / 2,
                          label_sizes[AxisIndexTop].width / 2);
                     
    self.margin_bottom = max(
                           label_sizes[AxisIndexBottom].height() + self.ticks[AxisIndexBottom]->extra_offset,
                           label_sizes[AxisIndexLeft].height() / 2,
                           label_sizes[AxisIndexRight].height() / 2);
                      
    self.margin_top = max(
                        label_sizes[AxisIndexTop].height() + self.ticks[AxisIndexTop]->extra_offset,
                        label_sizes[AxisIndexLeft].height() / 2,
                        label_sizes[AxisIndexRight].height() / 2);
  }
  else {
    self.margin_left   = self.margin_right = label_sizes[AxisIndexX].width / 2;
    self.margin_bottom = self.margin_top   = label_sizes[AxisIndexY].height() / 2;
  }
  
  Length w    = self.get_own_style(ImageSizeHorizontal, SymbolicSize::Automatic);
  Length h    = self.get_own_style(ImageSizeVertical,   SymbolicSize::Automatic);
  float ratio = self.get_own_style(AspectRatio, NAN);
  
  if(!isfinite(ratio) || ratio <= 0) {
    ratio = bounds.y_range.length() / bounds.x_range.length();
    
    if(!isfinite(ratio) || ratio <= 0)
      ratio = 1.0f; // 0.61803f;
  }
  
  bool check_max_auto_width = false;
  float max_height = Infinity;
  
  if(w == SymbolicSize::Automatic && h == SymbolicSize::Automatic) {
    if(optional_expand_width) {
      w = Length::Absolute(*optional_expand_width); // Length::Relative(1.0f)
    }
    else {
      check_max_auto_width = true;
      enum SyntaxPosition pos = find_syntax_position(self.parent(), self.index());
      
      max_height = max_auto_size.y;
      
      switch(pos) {
        case Alone:       w = SymbolicSize::Medium; break;
        case InsideList:  w = SymbolicSize::Small;  max_height*= 0.5f; break; 
        case InsideOther: w = SymbolicSize::Tiny;   max_height*= 0.25f; break;
      }
    }
  }
  
  float expand_width = optional_expand_width ? *optional_expand_width : max_auto_size.x;
  
  if(!w.is_explicit_abs_positive()) {
    if(w != SymbolicSize::Automatic) {
      w = Length::Absolute(w.resolve(self.em, LengthConversionFactors::GraphicsSize, expand_width));
      if(w.is_explicit_abs_positive()) {
        if(check_max_auto_width && isfinite(max_auto_size.x) && max_auto_size.x > 0 && w.explicit_abs_value() > max_auto_size.x)
          w = Length::Absolute(max_auto_size.x);
      }
      else
        w = SymbolicSize::Automatic;
    }
  }
  
  if(!h.is_explicit_abs_positive()) {
    if(h != SymbolicSize::Automatic) {
      h = Length::Absolute(h.resolve(self.em, LengthConversionFactors::GraphicsSize, expand_width));
      if(!h.is_explicit_abs_positive())
        h = SymbolicSize::Automatic;
    }
  }
  
  if(w.is_explicit_abs_positive() && !any_frame) {
    self.margin_left = max(
                         label_sizes[AxisIndexX].width / 2,
                         calc_margin_width(
                           w.explicit_abs_value() - self.margin_right,
                           label_sizes[AxisIndexY].width + self.ticks[AxisIndexY]->extra_offset,
                           bounds.x_range.length(),
                           bounds.x_range.to - ox));
  }
  
  if(h.is_explicit_abs_positive() && !any_frame) {
    self.margin_bottom = max(
                           label_sizes[AxisIndexY].height() / 2,
                           calc_margin_width(
                             h.explicit_abs_value() - self.margin_top,
                             label_sizes[AxisIndexX].height() + self.ticks[AxisIndexX]->extra_offset,
                             bounds.y_range.length(),
                             bounds.y_range.to - oy));
  }
  
  if(w == SymbolicSize::Automatic) {
    float content_h = max(0.0f, h.explicit_abs_value() - self.margin_top - self.margin_bottom);
    float content_w = content_h / ratio;

    if(!any_frame) { // No frame, but an Y-axis whose labels could extend the left margin
      self.margin_left = max(
                           label_sizes[AxisIndexX].width / 2,
                           calc_margin_width(
                             content_w,
                             label_sizes[AxisIndexY].width + self.ticks[AxisIndexY]->extra_offset,
                             bounds.x_range.length(),
                             bounds.x_range.to - ox));
    }
    
    w = Length::Absolute(content_w + self.margin_left + self.margin_right);
  }
  
  if(h == SymbolicSize::Automatic) {
    float content_w = max(0.0f, w.explicit_abs_value() - self.margin_left - self.margin_right);
    float content_h = content_w * ratio;

    if(!any_frame) { // No frame, but an X-axis whose labels could extend the bottom margin
      self.margin_bottom = max(
                             label_sizes[AxisIndexY].height() / 2,
                             calc_margin_width(
                               content_h,
                               label_sizes[AxisIndexX].height() + self.ticks[AxisIndexX]->extra_offset,
                               bounds.y_range.length(),
                               bounds.y_range.to - oy));
    }
    
    if(check_max_auto_width) { // Original setting was ImageSize->{Automatic, Automatic}
      if(content_h + self.margin_top + self.margin_bottom > max_height) {
        content_h = max(0.0f, max_height - (self.margin_top + self.margin_bottom));
        content_w = content_h / ratio;
        w = Length::Absolute(content_w + self.margin_left + self.margin_right);
      }
    }
    
    h = Length::Absolute(content_h + self.margin_top + self.margin_bottom);
  }
  
  self._extents.width = w.explicit_abs_value();
  self._extents.ascent  = h.explicit_abs_value() / 2 + 0.25 * self.em;
  self._extents.descent = h.explicit_abs_value() - self._extents.ascent;
  
  float ascent = calculate_ascent_for_baseline_position(self.em, self.get_style(BaselinePosition));
  self._extents.ascent = ascent;
  self._extents.descent = h.explicit_abs_value() - ascent;
  
  //float plot_range_height = h.explicit_abs_value() - self.margin_top - self.margin_bottom;
  float plot_range_width = w.explicit_abs_value() - self.margin_left - self.margin_right;
  
  self.ticks[AxisIndexLeft ]->start_pos.y        = h.explicit_abs_value() - self.margin_bottom;
  self.ticks[AxisIndexRight]->start_pos.y        = h.explicit_abs_value() - self.margin_bottom;
  self.ticks[AxisIndexY    ]->start_pos.y        = h.explicit_abs_value() - self.margin_bottom;
  self.ticks[AxisIndexLeft ]->end_pos.y          = self.margin_top;
  self.ticks[AxisIndexRight]->end_pos.y          = self.margin_top;
  self.ticks[AxisIndexY    ]->end_pos.y          = self.margin_top;
  self.ticks[AxisIndexLeft ]->tick_length_factor = plot_range_width;
  self.ticks[AxisIndexRight]->tick_length_factor = plot_range_width;
  self.ticks[AxisIndexY    ]->tick_length_factor = plot_range_width;
  
  self.ticks[AxisIndexLeft]->start_pos.x = self.margin_left;
  self.ticks[AxisIndexLeft]->end_pos.x   = self.margin_left;
  
  self.ticks[AxisIndexRight]->start_pos.x = w.explicit_abs_value() - self.margin_right;
  self.ticks[AxisIndexRight]->end_pos.x   = w.explicit_abs_value() - self.margin_right;
  
  
  self.ticks[AxisIndexBottom]->start_pos.x        = self.margin_left;
  self.ticks[AxisIndexTop   ]->start_pos.x        = self.margin_left;
  self.ticks[AxisIndexX     ]->start_pos.x        = self.margin_left;
  self.ticks[AxisIndexBottom]->end_pos.x          = w.explicit_abs_value() - self.margin_right;
  self.ticks[AxisIndexTop   ]->end_pos.x          = w.explicit_abs_value() - self.margin_right;
  self.ticks[AxisIndexX     ]->end_pos.x          = w.explicit_abs_value() - self.margin_right;
  self.ticks[AxisIndexBottom]->tick_length_factor = plot_range_width;
  self.ticks[AxisIndexTop   ]->tick_length_factor = plot_range_width;
  self.ticks[AxisIndexX     ]->tick_length_factor = plot_range_width;
  
  self.ticks[AxisIndexBottom]->start_pos.y = h.explicit_abs_value() - self.margin_bottom;
  self.ticks[AxisIndexBottom]->end_pos.y   = h.explicit_abs_value() - self.margin_bottom;
  
  self.ticks[AxisIndexTop]->start_pos.y = self.margin_top;
  self.ticks[AxisIndexTop]->end_pos.y   = self.margin_top;
  
  Point origin_pos;
  origin_pos.x = self.ticks[AxisIndexBottom]->get_tick_position(ox).x;
  origin_pos.y = self.ticks[AxisIndexLeft  ]->get_tick_position(oy).y;
  
  self.ticks[AxisIndexY]->start_pos.x = origin_pos.x;
  self.ticks[AxisIndexY]->end_pos.x   = origin_pos.x;
  
  self.ticks[AxisIndexX]->start_pos.y = origin_pos.y;
  self.ticks[AxisIndexX]->end_pos.y   = origin_pos.y;
  
  if(valid_ox) {
    self.ticks[AxisIndexX]->ignore_label_position = ox;
  }
  else {
    self.ticks[AxisIndexX]->ignore_label_position = NAN;
    
    self.ticks[AxisIndexY]->axis_hidden(true);
  }
  
  if(valid_oy) {
    self.ticks[AxisIndexY]->ignore_label_position = oy;
  }
  else {
    self.ticks[AxisIndexY]->ignore_label_position = NAN;
    
    self.ticks[AxisIndexX]->axis_hidden(true);
  }
  
  if(valid_ox && origin_pos.y >= h.explicit_abs_value() - self.margin_bottom - self.ticks[AxisIndexX]->extra_offset / 2)
    self.ticks[AxisIndexX]->ignore_label_position = NAN;
    
  if(valid_oy && origin_pos.x <= self.margin_left + self.ticks[AxisIndexY]->extra_offset / 2)
    self.ticks[AxisIndexY]->ignore_label_position = NAN;
    
}

void GraphicsBox::Impl::try_get_axes_origin(const GraphicsBounds &bounds, double *ox, double *oy) {
  Expr e = self.get_own_style(AxesOrigin);
  
  if(e.item_equals(0, richmath_System_NCache))
    e = e[2];
    
  if(e.item_equals(0, richmath_System_List) && e.expr_length() == 2) {
    Expr sub = e[1];
    if(sub.item_equals(0, richmath_System_NCache))
      sub = sub[2];
      
    if(sub.is_number())
      *ox = sub.to_double();
      
    sub = e[2];
    if(sub.item_equals(0, richmath_System_NCache))
      sub = sub[2];
      
    if(sub.is_number())
      *oy = sub.to_double();
  }
}

void GraphicsBox::Impl::calculate_axes_origin(const GraphicsBounds &bounds, double *ox, double *oy) {
  *ox = NAN;
  *oy = NAN;
  
  try_get_axes_origin(bounds, ox, oy);
  
  if(!isfinite(*ox)) {
    Expr e = Evaluate(Call(Symbol(richmath_FE_Graphics_DefaultAxesOrigin), bounds.x_range.from, bounds.x_range.to));
    *ox = e.to_double();
  }
  
  if(!isfinite(*oy)) {
    Expr e = Evaluate(Call(Symbol(richmath_FE_Graphics_DefaultAxesOrigin), bounds.y_range.from, bounds.y_range.to));
    *oy = e.to_double();
  }
  
  if(!isfinite(*ox))
    *ox = bounds.x_range.nearest(0.0);
    
  if(!isfinite(*oy))
    *oy = bounds.y_range.nearest(0.0);
}

GraphicsBounds GraphicsBox::Impl::calculate_plotrange() {
  GraphicsBounds bounds;
  
  Expr plot_range = self.get_own_style(PlotRange, Symbol(richmath_System_Automatic));
  
  if(plot_range.item_equals(0, richmath_System_NCache))
    plot_range = plot_range[2];
  
  if(plot_range.item_equals(0, richmath_System_List) && plot_range.expr_length() == 2) {
    get_range_from_pmath(bounds.x_range, plot_range[1]);
    get_range_from_pmath(bounds.y_range, plot_range[2]);
  }
  else if(get_range_from_pmath(bounds.x_range, plot_range)) {
    bounds.y_range = bounds.x_range;
  }
  
  Length pad_left   = self.get_own_style(PlotRangePaddingLeft,   SymbolicSize::Automatic);
  Length pad_right  = self.get_own_style(PlotRangePaddingRight,  SymbolicSize::Automatic);
  Length pad_top    = self.get_own_style(PlotRangePaddingTop,    SymbolicSize::Automatic);
  Length pad_bottom = self.get_own_style(PlotRangePaddingBottom, SymbolicSize::Automatic);
  
  if(pad_left   == SymbolicSize::Automatic && isfinite(bounds.x_range.from)) pad_left   = SymbolicSize::None;
  if(pad_right  == SymbolicSize::Automatic && isfinite(bounds.x_range.to))   pad_right  = SymbolicSize::None;
  if(pad_top    == SymbolicSize::Automatic && isfinite(bounds.y_range.from)) pad_top    = SymbolicSize::None;
  if(pad_bottom == SymbolicSize::Automatic && isfinite(bounds.y_range.to))   pad_bottom = SymbolicSize::None;
  
  if(!bounds.is_finite()) {
    GraphicsBounds auto_bounds;
    self.elements.find_extends(auto_bounds);
    
    double ox = bounds.x_range.nearest(0.0);
    double oy = bounds.y_range.nearest(0.0);
    try_get_axes_origin(auto_bounds, &ox, &oy);
    
    auto_bounds.add_point(ox, oy);
    
    if(!isfinite(bounds.x_range.from)) bounds.x_range.from = auto_bounds.x_range.from;
    if(!isfinite(bounds.x_range.to))   bounds.x_range.to   = auto_bounds.x_range.to;
    if(!isfinite(bounds.y_range.from)) bounds.y_range.from = auto_bounds.y_range.from;
    if(!isfinite(bounds.y_range.to))   bounds.y_range.to   = auto_bounds.y_range.to;
  }
  
  if(!isfinite(bounds.x_range.from) || !isfinite(bounds.x_range.to) || bounds.x_range.from > bounds.x_range.to) {
    bounds.x_range.from = -1;
    bounds.x_range.to   = 1;
  }
  
  if(!isfinite(bounds.y_range.from) || !isfinite(bounds.y_range.to) || bounds.y_range.from > bounds.y_range.to) {
    bounds.y_range.from = -1;
    bounds.y_range.to   = 1;
  }
  
  float w = bounds.x_range.length();
  float h = bounds.y_range.length();
  
  bounds.x_range.from -= pad_left.resolve(   w, LengthConversionFactors::PlotRangePadding, w);
  bounds.x_range.to   += pad_right.resolve(  w, LengthConversionFactors::PlotRangePadding, w);
  bounds.y_range.from -= pad_bottom.resolve( h, LengthConversionFactors::PlotRangePadding, h);
  bounds.y_range.to   += pad_top.resolve(    h, LengthConversionFactors::PlotRangePadding, h);
  
  if(bounds.x_range.is_singleton()) {
    double dist = 0.5 * max(fabs(bounds.x_range.from), fabs(0.5 * bounds.y_range.from + 0.5 * bounds.y_range.to));
    
    bounds.x_range.grow_by(dist);
    if(bounds.x_range.is_singleton())
      bounds.x_range.grow_by(1);
  }
  
  if(bounds.y_range.is_singleton()) {
    double dist = 0.5 * max(fabs(bounds.y_range.from), fabs(0.5 * bounds.x_range.from + 0.5 * bounds.x_range.to));
    
    bounds.y_range.grow_by(dist);
    if(bounds.y_range.is_singleton())
      bounds.y_range.grow_by(1);
  }
  return bounds;
}

bool GraphicsBox::Impl::get_range_from_pmath(Interval<double> &result, Expr expr) {
  if(expr.item_equals(0, richmath_System_Range) && expr.expr_length() == 2)
    return get_double_from_pmath(result.from, expr[1]) && get_double_from_pmath(result.to, expr[2]);
    
  double d;
  if(get_double_from_pmath(d, expr)) {
    d = fabs(d);
    result.from = -d;
    result.to   =  d;
    return true; 
  }
  
  return false;
}

bool GraphicsBox::Impl::get_double_from_pmath(double &result, Expr expr) {
  if(expr.item_equals(0, richmath_System_NCache))
    expr = expr[2];
  
  if(expr.is_number()) {
    result = expr.to_double();
    return true;
  }
  
  return false;
}

bool GraphicsBox::Impl::have_frame(bool *left, bool *right, bool *bottom, bool *top) {
  *left   = false;
  *right  = false;
  *bottom = false;
  *top    = false;
  
  Expr e = self.get_own_style(Frame);
  
  if(e == richmath_System_False || e == richmath_System_None)
    return false;
    
  if(e == richmath_System_True || e == richmath_System_All) {
    *left   = true;
    *right  = true;
    *bottom = true;
    *top    = true;
    return true;
  }
  
  if(e.item_equals(0, richmath_System_List) && e.expr_length() == 2) {
    Expr sub = e[1];
    if(sub == richmath_System_True) {
      *left   = true;
      *right  = true;
    }
    else if(sub.item_equals(0, richmath_System_List) && sub.expr_length() == 2) {
      if(sub.item_equals(1, richmath_System_True))
        *left = true;
        
      if(sub.item_equals(2, richmath_System_True))
        *right = true;
    }
    
    sub = e[2];
    if(sub == richmath_System_True) {
      *bottom = true;
      *top    = true;
    }
    else if(sub.item_equals(0, richmath_System_List) && sub.expr_length() == 2) {
      if(sub.item_equals(1, richmath_System_True))
        *bottom = true;
        
      if(sub.item_equals(2, richmath_System_True))
        *top = true;
    }
    
    return *left || *right || *bottom || *top;
  }
  
  return false;
}

bool GraphicsBox::Impl::have_axes(bool *x, bool *y) {
  *x = false;
  *y = false;
  
  Expr e = self.get_own_style(Axes);
  
  if(e == richmath_System_False)
    return false;
    
  if(e == richmath_System_True) {
    *x = true;
    *y = true;
    return true;
  }
  
  if(e.item_equals(0, richmath_System_List) && e.expr_length() == 2) {
    if(e.item_equals(1, richmath_System_True))
      *x = true;
      
    if(e.item_equals(2, richmath_System_True))
      *y = true;
      
    return *x || *y;
  }
  
  return false;
}

Expr GraphicsBox::Impl::generate_default_ticks(const Interval<double> &range, bool with_labels) {
  return Evaluate(Call(Symbol(richmath_FE_Graphics_DefaultTickBoxes),
             range.from, range.to,
             Symbol(with_labels ? richmath_System_True : richmath_System_False)));
}

Expr GraphicsBox::Impl::generate_ticks(const GraphicsBounds &bounds, enum AxisIndex part) {
  Expr ticks = get_ticks(bounds, part);
  
  if(ticks.item_equals(0, richmath_System_List))
    return ticks;
    
  if(ticks == richmath_System_None)
    return List();
    
  if(ticks == richmath_System_All) {
    switch(part) {
      case AxisIndexX:
      case AxisIndexBottom:
      case AxisIndexTop:
        return generate_default_ticks(bounds.x_range, true);
        
      case AxisIndexY:
      case AxisIndexLeft:
      case AxisIndexRight:
        return generate_default_ticks(bounds.y_range, true);
    }
  }
  
  if(ticks == richmath_System_Automatic) {
    switch(part) {
      case AxisIndexX:
      case AxisIndexBottom:
        return generate_default_ticks(bounds.x_range, true);
        
      case AxisIndexTop:
        return generate_default_ticks(bounds.x_range, false);
        
      case AxisIndexY:
      case AxisIndexLeft:
        return generate_default_ticks(bounds.y_range, true);
        
      case AxisIndexRight:
        return generate_default_ticks(bounds.y_range, false);
    }
  }
  
  return List();
}

Expr GraphicsBox::Impl::get_ticks(const GraphicsBounds &bounds, enum AxisIndex part) {
  Expr e;
  switch(part) {
    case AxisIndexX:
    case AxisIndexY:
      e = self.get_own_style(Ticks, Symbol(richmath_System_Automatic));
      break;
      
    case AxisIndexLeft:
    case AxisIndexRight:
    case AxisIndexBottom:
    case AxisIndexTop:
      e = self.get_own_style(FrameTicks, Symbol(richmath_System_Automatic));
      break;
  }
  
  if(e.item_equals(0, richmath_System_NCache))
    e = e[2];
    
  if(e == richmath_System_True)
    return Symbol(richmath_System_Automatic);
    
  if(e.item_equals(0, richmath_System_List)) {
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
    
    if(sub.item_equals(0, richmath_System_NCache))
      sub = sub[2];
      
    if(sub.item_equals(0, richmath_System_List)) {
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
          RICHMATH_ASSERT(0 && "not reached");
      }
    }
    
    return sub;
  }
  
  return e;
}

bool GraphicsBox::Impl::set_axis_ends(enum AxisIndex part, const GraphicsBounds &bounds) { // true if ends changed
  const Interval<double> *b_range;
  
  if(part == AxisIndexX || part == AxisIndexBottom || part == AxisIndexTop) {
    b_range = &bounds.x_range;
  }
  else {
    b_range = &bounds.y_range;
  }
  
  Interval<double> &a_range = self.ticks[part]->range;
  if(a_range != *b_range) {
    a_range = *b_range;
    return true;
  }
  
  return false;
}

void GraphicsBox::Impl::resize_axes(Context &context) {
  ContextState cc(context);
  cc.begin(self.style);
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
      self.ticks[part]->axis_hidden(!have_axis[part]);
      
    if(any_frame) {
      have_axis[AxisIndexX] = false;
      have_axis[AxisIndexY] = false;
    }
    
    for(int part = 0; part < 6; ++part) {
      set_axis_ends((AxisIndex)part, bounds);
      
      if(have_axis[part])
        self.ticks[part]->load_from_object(generate_ticks(bounds, (AxisIndex)part), BoxInputFlags::FormatNumbers);
      else
        self.ticks[part]->load_from_object(List(), BoxInputFlags::Default);
        
      self.ticks[part]->resize(context);
    }
  }
  cc.end();
}

//} ... class GraphicsBox::Impl
