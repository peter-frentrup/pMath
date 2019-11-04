#include <boxes/gridbox.h>

#include <boxes/fillbox.h>
#include <boxes/mathsequence.h>
#include <graphics/context.h>

#include <math.h>
#include <limits>


#ifdef _MSC_VER
#  define isfinite(x)  (_finite(x))
#endif

#ifndef NAN
#  define NAN  (std::numeric_limits<double>::quiet_NaN())
#endif

using namespace richmath;
using namespace std;

extern pmath_symbol_t richmath_System_Axis;
extern pmath_symbol_t richmath_System_Baseline;
extern pmath_symbol_t richmath_System_Bottom;
extern pmath_symbol_t richmath_System_Center;
extern pmath_symbol_t richmath_System_GridBox;
extern pmath_symbol_t richmath_System_Scaled;
extern pmath_symbol_t richmath_System_Top;

namespace richmath {
  class GridBoxImpl {
    public:
      GridBoxImpl(GridBox &_self) : self(_self) {}
      
      int resize_items(Context *context);
      void simple_spacing(float em);
      void expand_colspans(int span_count);
      void expand_rowspans(int span_count);
      
      float calculate_ascent_for_baseline_position(float em, Expr baseline_pos) const;
      void adjust_baseline(float em);
      
      bool has_any_fillbox();
      static FillBox *as_fillbox(GridItem *gi);
    
    private:
      GridBox &self;
  };
}

//{ class GridItem ...

GridItem::GridItem()
  : OwnerBox(),
    _span_right(0),
    _span_down(0),
    _really_span_from_left(false),
    _really_span_from_above(false)
{
  _content->insert(0, PMATH_CHAR_PLACEHOLDER);
}

GridItem::~GridItem() {
}

bool GridItem::expand(const BoxSize &size) {
  _content->expand(size);
  _extents = size;
  return true;
}

void GridItem::resize_default_baseline(Context *context) {
  bool smf = context->smaller_fraction_parts;
  context->smaller_fraction_parts = true;
  
  OwnerBox::resize_default_baseline(context);
  
  context->smaller_fraction_parts = smf;
  _span_right = 0;
  _span_down = 0;
  _really_span_from_left = false;
  _really_span_from_above = false;
}

Expr GridItem::to_pmath(BoxOutputFlags flags) {
  return _content->to_pmath(flags);
}

bool GridItem::try_load_from_object(Expr object, BoxInputFlags options) {
  load_from_object(object, options);
  return true;
}

void GridItem::load_from_object(const Expr object, BoxInputFlags opts) {
  _content->load_from_object(object, opts);
  finish_load_from_object(std::move(object));
}

bool GridItem::span_from_left() {
  return content()->length() == 1 &&
         (_content->text()[0] == 0xF3BA ||
          _content->text()[0] == 0xF3BC);
}

bool GridItem::span_from_above() {
  return _content->length() == 1 &&
         (_content->text()[0] == 0xF3BB ||
          _content->text()[0] == 0xF3BC);
}

bool GridItem::span_from_both() {
  return _content->length() == 1 &&
         _content->text()[0] == 0xF3BC;
}

bool GridItem::span_from_any() {
  return _content->length() == 1 &&
         (_content->text()[0] == 0xF3BA ||
          _content->text()[0] == 0xF3BB ||
          _content->text()[0] == 0xF3BC);
}

//} ... class GridItem

//{ class GridBox ...

GridBox::GridBox()
  : Box(),
    items(1, 1)
{
  items[0] = new GridItem;
  ensure_valid_boxes();
}

GridBox::GridBox(int rows, int cols)
  : Box(),
    items(rows > 0 ? rows : 1, cols > 0 ? cols : 1)
{
  for(int i = 0; i < items.length(); ++i) 
    items[i] = new GridItem();
  
  ensure_valid_boxes();
}

GridBox::~GridBox() {
  for(int i = 0; i < items.length(); ++i)
    delete items[i];
}

bool GridBox::try_load_from_object(Expr expr, BoxInputFlags opts) {
  if(expr[0] != richmath_System_GridBox)
    return false;
    
  if(expr.expr_length() < 1)
    return false;
    
  Expr options(pmath_options_extract_ex(expr.get(), 1, PMATH_OPTIONS_EXTRACT_UNKNOWN_WARNONLY));
  
  if(options.is_null())
    return false;
    
  Expr matrix = expr[1];
  
  if(matrix[0] != PMATH_SYMBOL_LIST)
    return false;
    
  if(matrix.expr_length() < 1)
    return false;
    
  int n_rows = (int)matrix.expr_length();
  if(n_rows <= 0)
    return false;
    
  if(matrix[1][0] != PMATH_SYMBOL_LIST)
    return false;
    
  int n_cols = (int)matrix[1].expr_length();
  if(n_cols <= 0)
    return false;
    
  for(int r = 2; r <= n_rows; ++r) {
    Expr row = matrix[r];
    
    // if we directly write matrix[row] in the following lines, and
    // compile with gcc -O1 ..., the app crashes, because matrix[row]._obj
    // is freed 1 time too often.
    
    if(row[0] != PMATH_SYMBOL_LIST)
      return false;
      
    if(row.expr_length() != (size_t)n_cols)
      return false;
  }
  
  /* now success is guaranteed */
  
  if(n_cols < cols())
    remove_cols(n_cols, cols() - n_cols);
  else if(n_cols > cols())
    insert_cols(cols(), n_cols - cols());
    
  if(n_rows < rows())
    remove_rows(n_rows, rows() - n_rows);
  else if(n_rows > rows())
    insert_rows(rows(), n_rows - rows());
    
  for(int r = 0; r < n_rows; ++r) {
    for(int c = 0; c < n_cols; ++c) {
      item(r, c)->load_from_object(matrix[r + 1][c + 1], opts);
    }
  }
  
  if(style) {
    reset_style();
    style->add_pmath(options);
  }
  else if(options != PMATH_UNDEFINED)
    style = new Style(options);
    
  finish_load_from_object(std::move(expr));
  return true;
}

void GridBox::insert_rows(int yindex, int count) {
  if(count <= 0)
    return;
    
  if(yindex < 0)
    yindex = 0;
  else if(yindex > items.rows())
    yindex = items.rows();
    
  items.insert_rows(yindex, count);
  for(int x = 0; x < items.cols(); ++x)
    for(int y = 0; y < count; ++y)
      items.set(yindex + y, x, new GridItem);
      
  ensure_valid_boxes();
  
  invalidate();
}

void GridBox::insert_cols(int xindex, int count) {
  if(count <= 0)
    return;
    
  if(xindex < 0)
    xindex = 0;
  else if(xindex > items.cols())
    xindex = items.cols();
    
  items.insert_cols(xindex, count);
  for(int x = 0; x < count; ++x)
    for(int y = 0; y < rows(); ++y)
      items.set(y, xindex + x, new GridItem);
      
  ensure_valid_boxes();
    
  invalidate();
}

void GridBox::remove_rows(int yindex, int count) {
  if(count <= 0)
    return;
    
  if(yindex < 0 || yindex >= items.rows() || items.rows() <= 1)
    return;
    
  for(int x = 0; x < items.cols(); ++x)
    for(int y = 0; y < count; ++y)
      item(yindex + y, x)->safe_destroy();
      
  items.remove_rows(yindex, count);
  ensure_valid_boxes();
    
  invalidate();
}

void GridBox::remove_cols(int xindex, int count) {
  if(count <= 0)
    return;
    
  if(xindex < 0 || xindex >= items.cols() || items.cols() <= 1)
    return;
    
  for(int x = 0; x < count; ++x)
    for(int y = 0; y < rows(); ++y)
      item(y, xindex + x)->safe_destroy();
      
  items.remove_cols(xindex, count);
  ensure_valid_boxes();
    
  invalidate();
}

bool GridBox::expand(const BoxSize &size) {
  if(size.width < _extents.width)
    return false;
    
  if(!isfinite(size.width))
    return false;
  
  if(!GridBoxImpl(*this).has_any_fillbox())
    return false;
  
  Array<float> col_weights;
  col_weights.length(cols(), 0.0f);
  
  int span_count = 0;
  for(int x = cols() - 1; x >= 0; --x) {
    for(int y = rows() - 1; y >= 0; --y) {
      GridItem *gi = item(y, x);
      if(FillBox *fillbox = GridBoxImpl::as_fillbox(gi)) {
        float w = fillbox->weight() / (1.0f + gi->_span_right);
        
        for(int dx = 0; dx <= gi->_span_right; ++dx)
          col_weights[x + dx] = max(col_weights[x + dx], w);
      }
      if(gi->_span_down || gi->_span_right)
        span_count+= 1;
    }
  }
  
  float right = _extents.width;
  float total_weight = 0.0f;
  float total_fill_width = size.width - _extents.width;
  for(int x = cols() - 1; x >= 0; --x) {
    float dw = col_weights[x];
    if(dw > 0) {
      total_weight+= dw;
      total_fill_width+= right - xpos[x];
    }
    right = xpos[x] - colspacing;
  }
  
  if(!(total_weight > 0)) // should not happen; e.g. overflow
    return false;
  
  for(int i = count() - 1; i >= 0; --i) {
    GridItem *gi = items[i];
    if(FillBox *fillbox = GridBoxImpl::as_fillbox(gi)) {
      BoxSize new_item_size = gi->extents();
      float new_width = total_fill_width * fillbox->weight() / total_weight + gi->_span_right * colspacing;
      if(new_item_size.width < new_width) {
        new_item_size.width = new_width;
        gi->expand(new_item_size);
      }
      else {
        // TODO: Cannot shrink box. Probably need to reduce total_fill_width for remaining FillBox'es
      }
    } 
  }
  
  float em = 0.0f;
  if(auto seq = dynamic_cast<MathSequence*>(parent()))
    em = seq->font_size();
  
  GridBoxImpl(*this).simple_spacing(em);
  GridBoxImpl(*this).expand_colspans(span_count);
  GridBoxImpl(*this).expand_rowspans(span_count);
  GridBoxImpl(*this).adjust_baseline(em);
  return true;
}

void GridBox::resize(Context *context) {
  float em = context->canvas->get_font_size();
  rowspacing = em;
  colspacing = em;
  
  rowspacing *= get_style(GridBoxRowSpacing);
  colspacing *= get_style(GridBoxColumnSpacing);
  
  float w = context->width;
  context->width = Infinity;
  int span_count = GridBoxImpl(*this).resize_items(context);
  context->width = w;
  
  GridBoxImpl(*this).simple_spacing(em);
  GridBoxImpl(*this).expand_colspans(span_count);
  GridBoxImpl(*this).expand_rowspans(span_count);
  
  GridBoxImpl(*this).adjust_baseline(em);
}

void GridBox::paint(Context *context) {
  using std::swap;
  
  update_dynamic_styles(context);
  
  float x, y;
  context->canvas->current_pos(&x, &y);
  
  for(int ix = 0; ix < items.cols(); ++ix) {
    for(int iy = 0; iy < items.rows(); ++iy) {
      GridItem *gi = item(iy, ix);
      
      if(!gi->_really_span_from_left && !gi->_really_span_from_above) {
        context->canvas->move_to(
          x + xpos[ix],
          y + ypos[iy] + gi->extents().ascent - _extents.ascent);
        gi->paint(context);
      }
    }
  }
  
  if(context->selection.get() == this) {
    auto rect = get_enclosing_range(context->selection.start, context->selection.end - 1);
    
    float x1, x2, y1, y2;
    x1 = x + xpos[rect.x.start.primary_value()];
    if(rect.x.end.primary_value() < cols() - 1)
      x2 = x + xpos[rect.x.end.primary_value()] + item(rect.y.end, rect.x.end)->extents().width;
    else
      x2 = x + _extents.width;
      
    y1 = y - _extents.ascent + ypos[rect.y.start.primary_value()];// - item(ay, ax)->extents().ascent;
    if(rect.y.end.primary_value() < rows() - 1)
      y2 = y - _extents.ascent + ypos[rect.y.end.primary_value()] + item(rect.y.end, rect.x.end)->extents().height();
    else
      y2 = y + _extents.descent;
      
    Color c = context->canvas->get_color();
    context->canvas->pixrect(x1, y1, x2, y2, false);
    context->draw_selection_path();
//    context->canvas->paint_selection(x1, y1, x2, y2);
    context->canvas->set_color(c);
  }
}

void GridBox::selection_path(Canvas *canvas, int start, int end) {
  auto rect = get_enclosing_range(start, end - 1);
  
  float x1, x2, y1, y2;
  x1 = xpos[rect.x.start.primary_value()];
  if(rect.x.end.primary_value() < cols() - 1)
    x2 = xpos[rect.x.end.primary_value()] + item(rect.y.end, rect.x.end)->extents().width;
  else
    x2 = _extents.width;
    
  y1 = - _extents.ascent + ypos[rect.y.start.primary_value()];
  if(rect.y.end.primary_value() < rows() - 1)
    y2 = - _extents.ascent + ypos[rect.y.end.primary_value()] + item(rect.y.end, rect.x.end)->extents().height();
  else
    y2 = _extents.descent;
    
  float x0, y0;
  canvas->current_pos(&x0, &y0);
  
  x1 += x0;
  y1 += y0;
  x2 += x0;
  y2 += y0;
  
  float px1 = x1;
  float py1 = y1;
  float px2 = x2;
  float py2 = y1;
  float px3 = x2;
  float py3 = y2;
  float px4 = x1;
  float py4 = y2;
  
  canvas->align_point(&px1, &py1, false);
  canvas->align_point(&px2, &py2, false);
  canvas->align_point(&px3, &py3, false);
  canvas->align_point(&px4, &py4, false);
  
  canvas->move_to(px1, py1);
  canvas->line_to(px2, py2);
  canvas->line_to(px3, py3);
  canvas->line_to(px4, py4);
  canvas->close_path();
}

Box *GridBox::remove_range(int *start, int end) {
  using std::swap;
  
  if(*start >= end) {
    if(_parent) {
      *start = _index + 1;
      return _parent->move_logical(LogicalDirection::Backward, true, start);
    }
    
    return this;
  }
  
  auto rect = get_enclosing_range(*start, end - 1);
  
  int ax_val = rect.x.start.primary_value();
  int ay_val = rect.y.start.primary_value();
  int bx_val = rect.x.end.primary_value();
  int by_val = rect.y.end.primary_value();
  
  if( ax_val == 0 && ay_val == 0 && bx_val == cols() - 1 && by_val == rows() - 1) {
    if(_parent) {
      *start = _index;
      return _parent->remove(start);
    }
    
    *start = 0;
    return items[0]->content();
  }
  
  if(_parent) {
    if(cols() == 1) {
      if(ay_val == 1 && by_val == rows() - 1) {
        *start = _index;
        if(MathSequence *seq = dynamic_cast<MathSequence *>(_parent)) {
          MathSequence *content = items[0]->content();
          seq->insert(
            _index,
            content,
            0,
            content->length());
          *start += content->length();
        }
        
        return _parent->remove(start);
      }
      else if(ay_val == 0 && by_val == rows() - 2) {
        *start = _index;
        if(MathSequence *seq = dynamic_cast<MathSequence *>(_parent)) {
          MathSequence *content = items[items.length() - 1]->content();
          seq->insert(
            _index,
            content,
            0,
            content->length());
          *start += content->length();
        }
        
        return _parent->remove(start);
      }
      else if(ay_val == 0 && by_val == rows() - 1) {
        *start = _index;
        return _parent->remove(start);
      }
    }
    
    if(rows() == 1) {
      if(ax_val == 1 && bx_val == cols() - 1) {
        *start = _index;
        if(MathSequence *seq = dynamic_cast<MathSequence *>(_parent)) {
          MathSequence *content = items[0]->content();
          seq->insert(
            _index,
            content,
            0,
            content->length());
          *start += content->length();
        }
        
        return _parent->remove(start);
      }
      else if(ax_val == 0 && bx_val == cols() - 2) {
        *start = _index;
        if(MathSequence *seq = dynamic_cast<MathSequence *>(_parent)) {
          MathSequence *content = items[items.length() - 1]->content();
          seq->insert(
            _index,
            content,
            0,
            content->length());
          *start += content->length();
        }
        
        return _parent->remove(start);
      }
      else if(ax_val == 0 && bx_val == cols() - 1) {
        *start = _index;
        return _parent->remove(start);
      }
    }
  }
  
  if(rect.cols() == cols()) {
    remove_rows(rect.y.start, rect.rows());
    if(ay_val > 0) {
      MathSequence *result = item(ay_val - 1, cols() - 1)->content();
      *start = result->length();
      return result;
    }
    
    if(_parent) {
      *start = _index;
      return _parent;
    }
    
    *start = items[0]->content()->length();
    return items[0]->content();
  }
  
  if(rect.rows() == rows()) {
    remove_cols(rect.x.start, rect.cols());
    if(ax_val > 0) {
      MathSequence *result = item(0, ax_val - 1)->content();
      *start = result->length();
      return result;
    }
    
    if(_parent) {
      *start = _index;
      return _parent;
    }
    
    *start = items[0]->content()->length();
    return items[0]->content();
  }
  
  for(int x = ax_val; x <= bx_val; ++x)
    for(int y = ay_val; y <= by_val; ++y) {
      item(y, x)->content()->remove(0, item(y, x)->content()->length());
      item(y, x)->content()->insert(0, PMATH_CHAR_PLACEHOLDER);
    }
    
  if(ay_val == 0 && by_val == 0 && rect.cols() == 1) {
    bool all_empty = true;
    for(int y = 1; y < rows(); ++y)
      if( item(y, ax_val)->content()->length() > 0 &&
          !item(y, ax_val)->content()->is_placeholder())
      {
        all_empty = false;
        break;
      }
      
    if(all_empty) {
      remove_cols(rect.x.start, 1);
      if(ax_val > 0) {
        MathSequence *result = item(ay_val, ax_val - 1)->content();
        *start = result->length();
        return result;
      }
      
      if(_parent) {
        *start = _index;
        return _parent;
      }
      
      *start = items[0]->content()->length();
      return items[0]->content();
    }
  }
  
  if(ax_val == 0 && bx_val == 0 && rect.rows() == 1) {
    bool all_empty = true;
    for(int x = 1; x < cols(); ++x)
      if( item(ay_val, x)->content()->length() > 0 &&
          !item(ay_val, x)->content()->is_placeholder())
      {
        all_empty = false;
        break;
      }
      
    if(all_empty) {
      remove_rows(rect.y.start, 1);
      if(ay_val > 0) {
        MathSequence *result = item(ay_val - 1, cols() - 1)->content();
        *start = result->length();
        return result;
      }
      
      if(_parent) {
        *start = _index;
        return _parent;
      }
      
      *start = items[0]->content()->length();
      return items[0]->content();
    }
  }
  
  *start = yx_to_index(rect.y.start, rect.x.start);
  if(*start > 0) {
    MathSequence *result = items[*start - 1]->content();
    *start = result->length();
    return result;
  }
  
  if(_parent) {
    *start = _index;
    return _parent;
  }
  
  *start = items[0]->content()->length();
  return items[0]->content();
}

Box *GridBox::remove(int *index) {
  if( items[*index]->content()->length() == 0 ||
      items[*index]->content()->is_placeholder())
  {
    return remove_range(index, *index + 1);
  }
  
  return move_logical(LogicalDirection::Backward, false, index);
}

Expr GridBox::to_pmath_symbol() {
  return Symbol(richmath_System_GridBox);
}

Expr GridBox::to_pmath(BoxOutputFlags flags) {
  return to_pmath(flags, 0, count());
}

Expr GridBox::to_pmath(BoxOutputFlags flags, int start, int end) {
  auto rect = get_enclosing_range(start, end - 1);
  
  int ax_val = rect.x.start.primary_value();
  int ay_val = rect.y.start.primary_value();
  int bx_val = rect.x.end.primary_value();
  int by_val = rect.y.end.primary_value();
  
  Expr mat = MakeList(rect.rows());
  
  for(int y = ay_val; y <= by_val; ++y) {
    Expr row = MakeList(rect.cols());
    
    for(int x = ax_val; x <= bx_val; ++x) 
      row.set(x - ax_val + 1, item(y, x)->to_pmath(flags));
    
    mat.set(y - ay_val + 1, row);
  }
  
  Gather g;
  g.emit(mat);
  
  if(style)
    style->emit_to_pmath();
    
  Expr e = g.end();
  e.set(0, Symbol(richmath_System_GridBox));
  return e;
}

Box *GridBox::move_vertical(
  LogicalDirection  direction,
  float            *index_rel_x,
  int              *index,
  bool              called_from_child
) {
  int row, col;
  need_pos_vectors();
  
  if(*index < 0) {
    if(direction == LogicalDirection::Forward)
      row = 0;
    else
      row = rows() - 1;
      
    col = 0;
    while(col < cols() - 1 && *index_rel_x > xpos[col + 1])
      ++col;
      
    *index_rel_x -= xpos[col];
  }
  else {
    GridYIndex y;
    GridXIndex x;
    index_to_yx(*index, &y, &x);
    row = y.primary_value();
    col = x.primary_value();
    if(direction == LogicalDirection::Forward)
      ++row;
    else
      --row;
  }
  
  if(row < 0 || row >= rows()) {
    if(_parent) {
      *index_rel_x += xpos[col];
      *index = _index;
      return _parent->move_vertical(direction, index_rel_x, index, true);
    }
    
    return this;
  }
  
  *index = -1;
  return item(row, col)->move_vertical(direction, index_rel_x, index, false);
}

Box *GridBox::mouse_selection(
  float  x,
  float  y,
  int   *start,
  int   *end,
  bool  *was_inside_start
) {
  need_pos_vectors();
  
  int col = 0;
  while(col < cols() - 1 && x > xpos[col + 1])
    ++col;
    
  y += _extents.ascent;
  
  int row = 0;
  while(row < rows() - 1 && y > ypos[row + 1])
    ++row;
    
  while(row > 0 && item(row, col)->_really_span_from_above)
    --row;
    
  while(col > 0 && item(row, col)->_really_span_from_left)
    --col;
    
  return item(row, col)->mouse_selection(
           x - xpos[col],
           y - ypos[row] - item(row, col)->extents().ascent,
           start,
           end,
           was_inside_start);
}

void GridBox::child_transformation(
  int             index,
  cairo_matrix_t *matrix
) {
  need_pos_vectors();
  
  GridYIndex row;
  GridXIndex col;
  index_to_yx(index, &row, &col);
  
  float x = xpos[col.primary_value()];
  float y = ypos[row.primary_value()] + item(index)->extents().ascent - _extents.ascent;
  
  cairo_matrix_translate(matrix, x, y);
}

GridIndexRect GridBox::get_enclosing_range(int start, int end) {
  GridXIndex ax, bx;
  GridYIndex ay, by;
  index_to_yx(start, &ay, &ax);
  index_to_yx(end,   &by, &bx); 
  
  return GridIndexRect::FromYX(GridYRange{ay, by}, GridXRange{ax, bx});
}

Box *GridBox::normalize_selection(int *start, int *end) {
  if(*start == *end) {
    if(*start == count())
      --*start;
    else
      ++*end;
  }
  
  auto rect = get_enclosing_range(*start, *end - 1);
  
  *start = yx_to_index(rect.y.start, rect.x.start);
  *end   = yx_to_index(rect.y.end, rect.x.end) + 1;
  
  if(*start + 1 == *end) {
    *start = 0;
    *end = items[*start]->content()->length();
    return items[*start]->content();
  }
  
  if(*start == 0 && *end == count())
    return Box::normalize_selection(start, end);
    
  return this;
}

void GridBox::need_pos_vectors() {
  if(xpos.length() != cols())
    xpos.length(cols());
    
  if(ypos.length() != rows())
    ypos.length(rows());
}

void GridBox::ensure_valid_boxes() {
  for(int i = 0; i < items.length(); ++i)
    adopt(items[i], i);
}

//} ... class GridBox

//{ class GridBoxImpl ...

int GridBoxImpl::resize_items(Context *context) {
  int span_count = 0;
  
  for(int i = 0; i < self.count(); ++i)
    self.items[i]->resize(context);
    
  for(int y = 0; y < self.rows(); ++y) {
    for(int x = 0; x < self.cols(); ++x) {
      GridItem *gi = self.item(y, x);
      
      if(!gi->span_from_any()) {
        gi->_span_right = 1;
        while(x + gi->_span_right < self.cols() &&
              self.item(y, x + gi->_span_right)->span_from_left())
        {
          self.item(y, x + gi->_span_right)->_really_span_from_left = true;
          gi->_span_right++;
        }
        gi->_span_right--;
        
        gi->_span_down = 1;
        while(y + gi->_span_down < self.rows() &&
              self.item(y + gi->_span_down, x)->span_from_above())
        {
          for(int i = x + gi->_span_right; i > x; --i)
            if(!self.item(y + gi->_span_down, i)->span_from_both())
              goto END_DOWN;
              
          for(int i = x + gi->_span_right; i >= x; --i)
            self.item(y + gi->_span_down, i)->_really_span_from_above = true;
            
          gi->_span_down++;
        }
        gi->_span_down--;
        
      END_DOWN:
        if(gi->_span_right > 0 || gi->_span_down > 0)
          ++span_count;
      }
    }
  }
  
  return span_count;
}

void GridBoxImpl::simple_spacing(float em) {
  float right = 0;//colspacing / 2;
  self.xpos.length(self.items.cols());
  
  for(int x = 0; x < self.xpos.length(); ++x) {
    float w = 0;
    for(int y = 0; y < self.rows(); ++y) {
      GridItem *gi = self.item(y, x);
      
      if(gi->_really_span_from_left) {
        if(x + 1 == self.cols() || !self.item(y, x + 1)->_really_span_from_left) {
          int basex = x - 1;
          while(self.item(y, basex)->_really_span_from_left) {
            --basex;
          }
          
          float rest_w = self.item(y, basex)->extents().width - right + self.xpos[basex];
          if(w < rest_w)
            w = rest_w;
        }
      }
      else if(!gi->_span_right &&
              !gi->_really_span_from_above &&
              w < gi->extents().width)
      {
        w = gi->extents().width;
      }
    }
    
    self.xpos[x] = right;
    right += w + self.colspacing;
  }
  
  self._extents.width = right - self.colspacing;
  
  int num_span_down_items = 0;
  
  float bottom = 0;
  self.ypos.length(self.items.rows());
  for(int y = 0; y < self.ypos.length(); ++y) {
    float a = 0;
    float d = 0;
    for(int x = 0; x < self.cols(); ++x) {
      GridItem *gi = self.item(y, x);
      
      if( !gi->_span_down &&
          !gi->_really_span_from_left &&
          !gi->_really_span_from_above)
      {
        gi->extents().bigger_y(&a, &d);
      }
    }
    
    BoxSize size(0, a, d);
    for(int x = 0; x < self.cols(); ++x) {
      GridItem *gi = self.item(y, x);
      
      if(gi->_span_down > 0)
        ++num_span_down_items;
        
      if(x + 1 + gi->_span_right < self.cols())
        right = self.xpos[x + 1 + gi->_span_right] - self.colspacing;
      else
        right = self._extents.width;// - colspacing;
        
      size.width = right - self.xpos[x];
      gi->expand(size);
    }
    
    self.ypos[y] = bottom;
    bottom += a + d + self.rowspacing;
  }
  
  bottom -= self.rowspacing;
  self._extents.ascent = bottom / 2 + 0.25f * em;
  self._extents.descent = bottom - self._extents.ascent;
  
  if(num_span_down_items > 0) {
    for(int y = 0; y < self.rows(); ++y) {
      for(int x = 0; x < self.cols(); ++x) {
        GridItem *gi = self.item(y, x);
        
        if(gi->_span_down > 0) {
          BoxSize size = gi->extents();
          
          if(y + 1 + gi->_span_down < self.rows())
            bottom = self.ypos[y + 1 + gi->_span_down];
          else
            bottom = self._extents.height() + self.rowspacing;
            
          size.descent += bottom - self.ypos[y + 1];
          
          gi->expand(size);
          
          if(--num_span_down_items == 0)
            return;
        }
      }
    }
  }
}

void GridBoxImpl::expand_colspans(int span_count) {
  int first_span = 0;
  while(span_count-- > 0 && first_span < self.count()) {
    for(int x = 0; x < self.cols(); ++x) {
      for(int y = 0; y < self.rows(); ++y) {
        GridItem *gi = self.item(y, x);
        
        if(gi->_span_right) {
          float w;
          if(x + gi->_span_right + 1 < self.cols())
            w = self.xpos[x + gi->_span_right + 1] - self.colspacing;
          else
            w = self._extents.width;
            
          w -= self.xpos[x];
          
          if(w > gi->extents().width) {
            BoxSize size = gi->extents();
            size.width = w;
            
            gi->expand(size);
          }
          else if(gi->index() >= first_span && w < gi->extents().width) {
            float delta = gi->extents().width - w;
            
            for(int x2 = x + gi->_span_right + 1; x2 < self.cols(); ++x2) {
              self.xpos[x2] += delta;
            }
            self._extents.width += delta;
            
            for(int y2 = 0; y2 < self.rows(); ++y2) {
              GridItem *gi2 = self.item(y2, x + gi->_span_right);
              
              if( !gi2->_span_right            &&
                  !gi2->_really_span_from_left &&
                  !gi2->_really_span_from_above)
              {
                BoxSize size = gi2->extents();
                size.width += delta;
                
                gi2->expand(size);
              }
            }
            
            first_span = gi->index();
            goto NEXT_X_SPAN;
          }
        }
      }
    }
    
  NEXT_X_SPAN:
    ;
  }
}

void GridBoxImpl::expand_rowspans(int span_count) {
  int first_span = 0;
  while(span_count-- > 0 && first_span < self.count()) {
    for(int y = 0; y < self.rows(); ++y) {
      for(int x = 0; x < self.cols(); ++x) {
        GridItem *gi = self.item(y, x);
        
        if(gi->_span_down) {
          float h;
          if(y + gi->_span_down + 1 < self.rows())
            h = self.ypos[y + gi->_span_down + 1] - self.rowspacing;
          else
            h = self._extents.height();
            
          h -= self.ypos[y];
          
          if(h > gi->extents().height()) {
            float half = (h - gi->extents().height()) / 2;
            BoxSize size = gi->extents();
            size.ascent +=  half;
            size.descent += half; // = h - size.ascent;
            
            gi->expand(size);
          }
          else if(gi->index() >= first_span &&
                  h < gi->extents().height())
          {
            float delta = gi->extents().height() - h;
            
            for(int y2 = y + gi->_span_down + 1; y2 < self.rows(); ++y2) {
              self.ypos[y2] += delta;
            }
            self._extents.ascent +=  delta / 2;
            self._extents.descent += delta / 2;
            
            for(int x2 = 0; x2 < self.cols(); ++x2) {
              GridItem *gi2 = self.item(y + gi->_span_down, x2);
              
              if( !gi2->_span_right            &&
                  !gi2->_span_down             &&
                  !gi2->_really_span_from_left &&
                  !gi2->_really_span_from_above)
              {
                BoxSize size = gi2->extents();
                size.ascent +=  delta / 2;
                size.descent += delta / 2;
                
                gi2->expand(size);
              }
            }
            
            first_span = gi->index();
            goto NEXT_Y_SPAN;
          }
        }
      }
    }
    
  NEXT_Y_SPAN:
    ;
  }
}

float GridBoxImpl::calculate_ascent_for_baseline_position(float em, Expr baseline_pos) const {
  float height = self._extents.height();
  
  if(baseline_pos == richmath_System_Bottom) 
    return height;
  
  if(baseline_pos == richmath_System_Top) 
    return 0;
  
  if(baseline_pos == richmath_System_Center) 
    return 0.5f * height;
  
  if(baseline_pos == richmath_System_Axis || baseline_pos == PMATH_SYMBOL_AUTOMATIC) 
    return 0.5f * height + 0.25f * em; // TODO: use actual math axis from font
  
  if(baseline_pos[0] == richmath_System_Scaled) {
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
    else if(rhs[0] == richmath_System_Scaled) {
      double factor = 0.0;
      if(get_factor_of_scaled(rhs, &factor) && isfinite(factor)) {
        //float ref_pos = 0.75 * em * factor - 0.25 * em * (1 - factor);
        float ref_pos = (factor - 0.25) * em;
        return lhs_ascent + ref_pos;
      }
    }
  }
  
  GridItem *gi = nullptr;
  if(baseline_pos.is_int32() && self.cols() > 0) {
    int row = PMATH_AS_INT32(baseline_pos.get());
    if(row < 0)
      row += self.rows() + 1;
      
    if(row >= 1 && row <= self.rows()) 
      gi = self.item(row - 1, 0);
  }
  else if(baseline_pos[0] == PMATH_SYMBOL_LIST) {
    if(baseline_pos.expr_length() == 2) {
      Expr row_expr = baseline_pos[1];
      Expr col_expr = baseline_pos[2];
      if(row_expr.is_int32() && col_expr.is_int32()) {
        int row = PMATH_AS_INT32(row_expr.get());
        int col = PMATH_AS_INT32(col_expr.get());
        
        if(row < 0)
          row += self.rows() + 1;
        if(col < 0)
          col += self.cols() + 1;
          
        if(row >= 1 && row <= self.rows() && col >= 1 && col <= self.cols())
          gi = self.item(row - 1, col - 1);
      }
      // TODO: BaselinePosition -> {{i,j}, pos}
    }
  }
  
  if(gi) {
    GridYIndex row;
    GridXIndex col;
    self.index_to_yx(gi->index(), &row, &col);
    
    return self.ypos[row.primary_value()] + gi->extents().ascent;
  }
  else
    return 0.5f * height + 0.25f * em; // TODO: use actual math axis from font
}

void GridBoxImpl::adjust_baseline(float em) {
  float height = self._extents.height();
  
  float ascent = calculate_ascent_for_baseline_position(em, self.get_style(BaselinePosition));
  self._extents.ascent = ascent;
  self._extents.descent = height - ascent;
}

bool GridBoxImpl::has_any_fillbox() {
  for(int i = self.count() - 1; i >= 0; --i) 
    if(as_fillbox(self.items[i])) 
      return true;
  
  return false;
}

FillBox *GridBoxImpl::as_fillbox(GridItem *gi) {
  auto seq = gi->content();
  if(seq->length() != 1 || seq->count() != 1)
    return nullptr;
  
  return dynamic_cast<FillBox*>(seq->item(0));
}

//} ... class GridBoxImpl
