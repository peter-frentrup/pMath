#include <boxes/graphics/graphicsbox.h>
#include <boxes/graphics/axisticks.h>

#include <boxes/gridbox.h>
#include <boxes/inputfieldbox.h>
#include <boxes/section.h>

#include <graphics/context.h>

#include <gui/document.h>
#include <gui/native-widget.h>

#include <util/spanexpr.h>

#include <algorithm>

#ifdef max
#undef max
#endif


using namespace richmath;


enum SyntaxPosition {
  Alone,
  InsideList,
  InsideOther
};


static enum SyntaxPosition find_syntax_position(Box *box, int index) {
  if(!box || dynamic_cast<Section*>(box))
    return Alone;
    
  if(MathSequence *seq = dynamic_cast<MathSequence*>(box)) {
    SpanExpr *expr = new SpanExpr(index, 0, seq);
    
    expr = expr->expand();
    
    bool inside_list = false;
    
    while(expr && expr->sequence() == seq) {
      FunctionCallSpan call(expr);
      
      if( call.is_simple_list() ||
          call.is_sequence())
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
  
  if(dynamic_cast<GridBox*>(box) || dynamic_cast<OwnerBox*>(box)) {
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
  mouse_down_y(0)
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

GraphicsBox *GraphicsBox::create(Expr expr, int opts) {
  GraphicsBox *box = new GraphicsBox;
  
  if(expr.expr_length() > 1) {
    Expr options(pmath_options_extract(expr.get(), 1));
    
    if(options.is_null()) {
      delete box;
      return 0;
    }
    
    box->style->add_pmath(options);
  }
  
  box->elements.load_from_object(expr[1], opts);
  
  return box;
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

void GraphicsBox::resize(Context *context) {
  float em = context->canvas->get_font_size();
  
  float w = get_style(ImageSizeHorizontal, ImageSizeAutomatic);
  float h = get_style(ImageSizeVertical,   ImageSizeAutomatic);
  
  ContextState cc(context);
  cc.begin(style);
  {
    double xmin = -1;
    double xmax =  1;
    double ymin = -1;
    double ymax =  1;
    
    if( x_axis_ticks->start_position != xmin ||
        x_axis_ticks->end_position   != xmax)
    {
      x_axis_ticks->load_from_object(
        Evaluate(Parse(
                   "FE`Graphics`TickPositions(`1`, `2`).Map({#, ToBoxes(#)}&)",
                   xmin,
                   xmax)),
        BoxOptionFormatNumbers);
        
      x_axis_ticks->start_position = xmin;
      x_axis_ticks->end_position   = xmax;
    }
    
    if( y_axis_ticks->start_position != ymin ||
        y_axis_ticks->end_position   != ymax)
    {
      y_axis_ticks->load_from_object(
        Evaluate(Parse(
                   "FE`Graphics`TickPositions(`1`, `2`).Map({#, ToBoxes(#)}&)",
                   ymin,
                   ymax)),
        BoxOptionFormatNumbers);
        
      y_axis_ticks->start_position = ymin;
      y_axis_ticks->end_position   = ymax;
    }
    
    x_axis_ticks->resize(context);
    y_axis_ticks->resize(context);
    
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
  }
  cc.end();
  
  
  if(w <= 0 && h <= 0) {
    enum SyntaxPosition pos = find_syntax_position(parent(), index());
    
    switch(pos) {
      case Alone:
        w = 16 * em;
        break;
        
      case InsideList:
        w = 12 * em;
        break;
        
      case InsideOther:
        w = 8 * em;
        break;
    }
  }
  
  if(w <= 0) {
    float content_h = h - margin_top - margin_bottom;
    
    double dy = y_axis_ticks->end_position - y_axis_ticks->start_position;
    
    if(content_h > 0 && dy > 0) {
      double dx = x_axis_ticks->end_position - x_axis_ticks->start_position;
      
      w = content_h * dx / dy + margin_left + margin_right;
    }
    else
      w = margin_left + margin_right;
  }
  
  if(h <= 0) {
    float content_w = w - margin_left - margin_right;
    
    double dx = x_axis_ticks->end_position - x_axis_ticks->start_position;
    
    if(content_w > 0 && dx > 0) {
      double dy = y_axis_ticks->end_position - y_axis_ticks->start_position;
      
      h = content_w * dy / dx + margin_top + margin_bottom;
    }
    else
      h = margin_top + margin_bottom;
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
      context->canvas->pixrect(x, y, x + w, y + h, false);
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
      context->canvas->pixrect(x, y, x + w, y + h, false);
      context->canvas->clip();
      
      context->canvas->pixrect(
        x + margin_left,
        y + margin_top,
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
  style->emit_to_pmath(false, false);
  
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
  
  if(box && !dynamic_cast<Document*>(box) && !dynamic_cast<InputFieldBox*>(box))
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
