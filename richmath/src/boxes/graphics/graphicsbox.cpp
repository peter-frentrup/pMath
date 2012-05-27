#include <boxes/graphics/graphicsbox.h>
#include <boxes/graphics/axisticks.h>

#include <boxes/fillbox.h>
#include <boxes/gridbox.h>
#include <boxes/inputfieldbox.h>
#include <boxes/section.h>

#include <graphics/context.h>

#include <gui/document.h>
#include <gui/native-widget.h>

#include <util/spanexpr.h>

#include <algorithm>
#include <cmath>

#ifdef max
#  undef max
#endif


#ifdef _MSC_VER
#  define isfinite(x)  (_finite(x))
#endif


using namespace richmath;
using namespace std;


enum SyntaxPosition {
  Alone,
  InsideList,
  InsideOther
};


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
  
  if(dynamic_cast<GridBox *>(box) || dynamic_cast<OwnerBox *>(box)) {
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
  if(!style)
    style = new Style();
    
  style->set(BaseStyleName, "Graphics");
  
  x_axis_ticks = new AxisTicks;
  y_axis_ticks = new AxisTicks;
  
  adopt(x_axis_ticks, 0);
  adopt(y_axis_ticks, 1);
  
  x_axis_ticks->label_direction_x = 0;
  x_axis_ticks->label_direction_y = 1;
  
  y_axis_ticks->label_direction_x = -1;
  y_axis_ticks->label_direction_y =  0;
}

GraphicsBox::~GraphicsBox() {
  delete x_axis_ticks;
  delete y_axis_ticks;
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
  
  style->clear();
  style->add_pmath(options);
  style->add_pmath(user_options);
  
  elements.load_from_object(expr[1], opts);
  
  return true;
}

Box *GraphicsBox::item(int i) {
  switch(i) {
    case 0: return x_axis_ticks;
    case 1: return y_axis_ticks;
  }
  
  return 0;
}

int GraphicsBox::count() {
  return 2;
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
  
  BoxSize xlabels = x_axis_ticks->all_labels_extents();
  BoxSize ylabels = y_axis_ticks->all_labels_extents();
  
  x_axis_ticks->extra_offset = 0.75 * 6;
  y_axis_ticks->extra_offset = 0.75 * 6;
  
  margin_left   = std::max(ylabels.width    + x_axis_ticks->extra_offset, xlabels.width    / 2);
  margin_bottom = std::max(xlabels.height() + y_axis_ticks->extra_offset, ylabels.height() / 2);
  margin_right  = xlabels.width    / 2;
  margin_top    = ylabels.height() / 2;
  
  calculate_size();
}

void GraphicsBox::calculate_size(const float *optional_expand_width) {
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
          w = 30 * em;
          break;
          
        case InsideList:
          w = 15 * em;
          break;
          
        case InsideOther:
          w = 8 * em;
          break;
      }
    }
  }
  
  if(w <= 0) {
    float content_h = h - margin_top - margin_bottom;
    
    w = content_h / ratio + margin_left + margin_right;
  }
  
  if(h <= 0) {
    float content_w = w - margin_left - margin_right;
    
    h = content_w * ratio + margin_top  + margin_bottom;
  }
  
  _extents.width = w;
  _extents.ascent  = h / 2 + 0.25 * em;
  _extents.descent = h - _extents.ascent;
  
  
  x_axis_ticks->start_x = margin_left;
  x_axis_ticks->start_y = _extents.descent - margin_bottom;
  x_axis_ticks->end_x   = _extents.width   - margin_right;
  x_axis_ticks->end_y   = _extents.descent - margin_bottom;
  
  y_axis_ticks->start_x = margin_left;
  y_axis_ticks->start_y = _extents.descent - margin_bottom;
  y_axis_ticks->end_x   = margin_left;
  y_axis_ticks->end_y   = margin_top - _extents.ascent;
}

void GraphicsBox::resize_axes(Context *context) {
  ContextState cc(context);
  cc.begin(style);
  {
    GraphicsBounds bounds;
    
    Expr plot_range = get_own_style(PlotRange, Symbol(PMATH_SYMBOL_AUTOMATIC));
    
    if(plot_range[0] == PMATH_SYMBOL_NCACHE)
      plot_range = plot_range[2];
    
    if(plot_range == PMATH_SYMBOL_AUTOMATIC) {
      elements.find_extends(bounds);
    }
    else if(plot_range[0] == PMATH_SYMBOL_LIST &&
            plot_range.expr_length() == 2)
    {
      Expr xrange = plot_range[1];
      Expr yrange = plot_range[2];
      
      if(xrange == PMATH_SYMBOL_AUTOMATIC || yrange == PMATH_SYMBOL_AUTOMATIC)
        elements.find_extends(bounds);
        
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
    
    if( !isfinite(bounds.xmin) ||
        !isfinite(bounds.xmax) ||
        bounds.xmin >= bounds.xmax)
    {
      bounds.xmin = -1;
      bounds.xmax = 1;
    }
    else if(bounds.xmin == bounds.xmax) {
      bounds.xmin -= 1;
      bounds.xmax += 1;
    }
    
    if( !isfinite(bounds.ymin) ||
        !isfinite(bounds.ymax) ||
        bounds.ymin >= bounds.ymax)
    {
      bounds.ymin = -1;
      bounds.ymax = 1;
    }
    else if(bounds.ymin == bounds.ymax) {
      bounds.ymin -= 1;
      bounds.ymax += 1;
    }
    
    if( x_axis_ticks->start_position != bounds.xmin ||
        x_axis_ticks->end_position   != bounds.xmax)
    {
      x_axis_ticks->load_from_object(
        Evaluate(Parse(
                   "FE`Graphics`TickPositions(`1`, `2`).Map({#, ToBoxes(#)}&)",
                   bounds.xmin,
                   bounds.xmax)),
        BoxOptionFormatNumbers);
        
      x_axis_ticks->start_position = bounds.xmin;
      x_axis_ticks->end_position   = bounds.xmax;
    }
    
    if( y_axis_ticks->start_position != bounds.ymin ||
        y_axis_ticks->end_position   != bounds.ymax)
    {
      y_axis_ticks->load_from_object(
        Evaluate(Parse(
                   "FE`Graphics`TickPositions(`1`, `2`).Map({#, ToBoxes(#)}&)",
                   bounds.ymin,
                   bounds.ymax)),
        BoxOptionFormatNumbers);
        
      y_axis_ticks->start_position = bounds.ymin;
      y_axis_ticks->end_position   = bounds.ymax;
    }
    
    x_axis_ticks->resize(context);
    y_axis_ticks->resize(context);
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
    if(error_boxes_expr.is_valid())
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
      context->canvas->pixrect(
        x     - 0.75f,
        y     - 0.75f,
        x + w + 0.75f,
        y + h + 0.75f,
        false);
      context->canvas->clip();
      
      if(x_axis_ticks->is_visible(0.0)) {
        float zero_x, zero_y1, zero_y2;
        x_axis_ticks->get_tick_position(0.0, &zero_x, &zero_y1);
        
        zero_x += x;
        zero_y1 = y + margin_top;
        zero_y2 = y + h - margin_bottom;
        
        context->canvas->align_point(&zero_x, &zero_y1, true);
        context->canvas->align_point(&zero_x, &zero_y2, true);
        
        context->canvas->move_to(zero_x, zero_y1);
        context->canvas->line_to(zero_x, zero_y2);
        context->canvas->hair_stroke();
      }
      
      if(y_axis_ticks->is_visible(0.0)) {
        float zero_x1, zero_x2, zero_y;
        y_axis_ticks->get_tick_position(0.0, &zero_x1, &zero_y);
        
        zero_x1 = x + margin_left;
        zero_x2 = x + w - margin_right;
        zero_y += y + _extents.ascent;
        
        context->canvas->align_point(&zero_x1, &zero_y, true);
        context->canvas->align_point(&zero_x2, &zero_y, true);
        
        context->canvas->move_to(zero_x1, zero_y);
        context->canvas->line_to(zero_x2, zero_y);
        context->canvas->hair_stroke();
      }
      
      context->canvas->pixrect(
        x +     margin_left,
        y +     margin_top,
        x + w - margin_right,
        y + h - margin_bottom,
        true);
        
      context->canvas->hair_stroke();
      
      context->canvas->move_to(x, y + _extents.ascent);
      x_axis_ticks->paint(context);
      
      context->canvas->move_to(x, y + _extents.ascent);
      y_axis_ticks->paint(context);
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
  
  sx = x_axis_ticks->end_position - x_axis_ticks->start_position;
  sy = y_axis_ticks->end_position - y_axis_ticks->start_position;
  if(sx == 0) sx = 1;
  if(sy == 0) sy = 1;
  
  cairo_matrix_scale(mat, 1 / sx, 1 / sy);
  
  cairo_matrix_translate(
    mat,
    - x_axis_ticks->start_position,
    - y_axis_ticks->start_position);
}

Box *GraphicsBox::mouse_selection(
  float  x,
  float  y,
  int   *start,
  int   *end,
  bool  *was_inside_start
) {
  Box *tmp = x_axis_ticks->mouse_selection(x, y, start, end, was_inside_start);
  
  if(tmp != x_axis_ticks)
    return tmp;
    
  tmp = y_axis_ticks->mouse_selection(x, y, start, end, was_inside_start);
  
  if(tmp != y_axis_ticks)
    return tmp;
    
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
  if(error_boxes_expr.is_valid()) {
    Document *doc = find_parent<Document>(false);
    
    if(doc)
      doc->native()->show_tooltip(error_boxes_expr);
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
