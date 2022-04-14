#include <boxes/gridbox.h>

#include <boxes/box-factory.h>
#include <graphics/context.h>

#include <algorithm>
#include <math.h>
#include <limits>


#ifdef _MSC_VER
#  define isfinite(x)  (_finite(x))
#endif

#ifndef NAN
#  define NAN  (std::numeric_limits<double>::quiet_NaN())
#endif

#ifdef min
#  undef min
#endif
#ifdef max
#  undef max
#endif

using namespace richmath;
using namespace std;

namespace richmath {
  class GridBox::Impl {
    public:
      Impl(GridBox &_self) : self(_self) {}
      
      int resize_items(Context &context);
      void simple_spacing(float em);
      void expand_colspans(int span_count);
      void expand_rowspans(int span_count);
      
      float calculate_ascent_for_baseline_position(float em, Expr baseline_pos) const;
      void adjust_baseline(float em);
      
      GridItem *create_item();
      
    private:
      GridBox &self;
  };
}

extern pmath_symbol_t richmath_System_Axis;
extern pmath_symbol_t richmath_System_Automatic;
extern pmath_symbol_t richmath_System_Baseline;
extern pmath_symbol_t richmath_System_Bottom;
extern pmath_symbol_t richmath_System_Center;
extern pmath_symbol_t richmath_System_GridBox;
extern pmath_symbol_t richmath_System_List;
extern pmath_symbol_t richmath_System_Scaled;
extern pmath_symbol_t richmath_System_Top;

//{ class GridItem ...

GridItem::GridItem(AbstractSequence *content)
  : base(content),
    _span_right(0),
    _span_down(0)
{
  _content->insert(0, PMATH_CHAR_PLACEHOLDER);
}

GridItem::~GridItem() {
}

float GridItem::fill_weight() {
  return content()->fill_weight();
}

bool GridItem::expand(const BoxSize &size) {
  _content->expand(size);
  _extents = size;
  return true;
}

void GridItem::resize_default_baseline(Context &context) {
  _span_right = 0;
  _span_down = 0;
  really_span_from_left(false);
  really_span_from_above(false);
  
  base::resize_default_baseline(context);
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

void GridItem::swap_content(GridItem *other) {
  using std::swap;
  assert(other != nullptr);
  abandon(_content);
  other->abandon(other->_content);
  swap(_content, other->_content);
  adopt(_content, 0);
  other->adopt(other->_content, 0);
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

//{ class GridSelectionStrategy ...

GridSelectionStrategy GridSelectionStrategy::ContentsOnly = GridSelectionStrategy(GridSelectionStrategy::Kind::ContentsOnly);

GridSelectionStrategy::GridSelectionStrategy(Kind kind)
  : kind(kind),
    expected_rows(0),
    expected_cols(0)
{
}

GridSelectionStrategy::GridSelectionStrategy(Kind kind, const GridIndexRect &expected_size)
  : kind(kind),
    expected_rows(expected_size.rows()),
    expected_cols(expected_size.cols())
{
}

GridSelectionStrategy::GridSelectionStrategy(Kind kind, int expected_rows, int expected_cols)
  : kind(kind),
    expected_rows(expected_rows),
    expected_cols(expected_cols)
{
}

//} ... class GridSelectionStrategy

//{ class GridBox ...

GridSelectionStrategy GridBox::selection_strategy = GridSelectionStrategy(GridSelectionStrategy::Kind::ContentsOnly);

GridBox::GridBox(LayoutKind kind)
  : base(),
    items(1, 1)
{
  use_text_layout(kind == LayoutKind::Text);
  items[0] = Impl(*this).create_item();
  ensure_valid_boxes();
}

GridBox::GridBox(LayoutKind kind, int rows, int cols)
  : base(),
    items(rows > 0 ? rows : 1, cols > 0 ? cols : 1)
{
  use_text_layout(kind == LayoutKind::Text);
  for(int i = 0; i < items.length(); ++i) 
    items[i] = Impl(*this).create_item();
  
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
  
  if(matrix[0] != richmath_System_List)
    return false;
    
  if(matrix.expr_length() < 1)
    return false;
    
  int n_rows = (int)matrix.expr_length();
  if(n_rows <= 0)
    return false;
    
  if(matrix[1][0] != richmath_System_List)
    return false;
    
  int n_cols = (int)matrix[1].expr_length();
  if(n_cols <= 0)
    return false;
    
  for(int r = 2; r <= n_rows; ++r) {
    Expr row = matrix[r];
    
    // if we directly write matrix[row] in the following lines, and
    // compile with gcc -O1 ..., the app crashes, because matrix[row]._obj
    // is freed 1 time too often.
    
    if(row[0] != richmath_System_List)
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
      items.set(yindex + y, x, Impl(*this).create_item());
      
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
      items.set(y, xindex + x, Impl(*this).create_item());
      
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

int GridBox::child_script_level(int index, const int *opt_ambient_script_level) {
  int ambient_level = Box::child_script_level(0, opt_ambient_script_level);
  
  // TODO: implement AllowScriptLevelChange style
  if(ambient_level < 1)
    ambient_level = 1;
  
  return ambient_level;
}

bool GridBox::expand(const BoxSize &size) {
  if(size.width < _extents.width)
    return false;
    
  if(!isfinite(size.width))
    return false;
  
  Array<float> col_weights;
  col_weights.length(cols(), 0.0f);
  
  int span_count = 0;
  for(int x = cols() - 1; x >= 0; --x) {
    for(int y = rows() - 1; y >= 0; --y) {
      GridItem *gi = item(y, x);
      float w = gi->fill_weight() / (1.0f + gi->_span_right);
      if(w > 0) {
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
    float w = gi->fill_weight();
    if(w > 0) {
      BoxSize new_item_size = gi->extents();
      float new_width = total_fill_width * w / total_weight + gi->_span_right * colspacing;
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
  if(auto seq = dynamic_cast<AbstractSequence*>(parent()))
    em = seq->get_em();
  
  Impl(*this).simple_spacing(em);
  Impl(*this).expand_colspans(span_count);
  Impl(*this).expand_rowspans(span_count);
  Impl(*this).adjust_baseline(em);
  return true;
}

void GridBox::resize(Context &context) {
  float em = context.canvas().get_font_size();
  rowspacing = em;
  colspacing = em;
  
  rowspacing *= get_style(GridBoxRowSpacing);
  colspacing *= get_style(GridBoxColumnSpacing);
  
  int old_script_level = context.script_level;
  context.script_level = child_script_level(0, &context.script_level);
  
  float w = context.width;
  context.width = Infinity;
  int span_count = Impl(*this).resize_items(context);
  context.width = w;
  
  Impl(*this).simple_spacing(em);
  Impl(*this).expand_colspans(span_count);
  Impl(*this).expand_rowspans(span_count);
  
  Impl(*this).adjust_baseline(em);
  
  context.script_level = old_script_level;
}

void GridBox::paint(Context &context) {
  need_pos_vectors();
  
  update_dynamic_styles(context);
  
  float x, y;
  context.canvas().current_pos(&x, &y);
  
  for(int ix = 0; ix < items.cols(); ++ix) {
    for(int iy = 0; iy < items.rows(); ++iy) {
      GridItem *gi = item(iy, ix);
      
      if(!gi->really_span_from_left() && !gi->really_span_from_above()) {
        context.canvas().move_to(
          x + xpos[ix],
          y + ypos[iy] + gi->extents().ascent - _extents.ascent);
        gi->paint(context);
      }
    }
  }
  
  if(context.selection.get() == this) {
    Color c = context.canvas().get_color();
    context.canvas().move_to(x, y);
    selection_path(context.canvas(), context.selection.start, context.selection.end);
    if(context.selection.start >= items.length() && context.selection.end >= items.length()) {
      context.canvas().save();
      context.canvas().set_color(context.cursor_color);
      context.canvas().reset_matrix();
      cairo_set_line_width(context.canvas().cairo(), 2.0);
      cairo_set_line_cap(context.canvas().cairo(), CAIRO_LINE_CAP_BUTT);
      cairo_set_line_join(context.canvas().cairo(), CAIRO_LINE_JOIN_MITER);
      const double dashes[] = {1.0, 1.0};
      cairo_set_dash(context.canvas().cairo(), dashes, int(sizeof(dashes)/sizeof(dashes[0])), 0.0);
      context.canvas().clip_preserve();
      context.canvas().stroke();
      context.canvas().restore();
    }
    else {
      context.draw_selection_path();
    }
    context.canvas().set_color(c);
  }
}

float GridBox::get_gap_x(GridXIndex col, int gap_side) {
  if(col < GridXIndex(cols())) {
    if(col == GridXIndex(0))
      return xpos[0];
    
    if(gap_side > 0)
      return xpos[col.primary_value()];
    
    float x_before = xpos[col.primary_value() - 1] + item(GridYIndex(0), col-1)->extents().width;
    if(gap_side == 0)
      return x_before + (xpos[col.primary_value()] - x_before)/2;
      
    return x_before;
  }
  
  return _extents.width;
}

float GridBox::get_gap_y(GridYIndex row, int gap_side) {
  if(row < GridYIndex(rows())) {
    if(row == GridYIndex(0))
      return ypos[0] - _extents.ascent;
    
    if(gap_side > 0)
      return ypos[row.primary_value()] - _extents.ascent;
    
    float y_before = ypos[row.primary_value() - 1] + item(row-1, GridXIndex(0))->extents().height() - _extents.ascent;
    if(gap_side == 0)
      return y_before + (ypos[row.primary_value()] - _extents.ascent - y_before)/2;
    
    return y_before;
  }
  
  return _extents.descent;
}

void GridBox::selection_path(Canvas &canvas, int start, int end) {
  auto rect = get_enclosing_range(start, end);
  
  float x1 = get_gap_x(rect.x.start, +1);
  float x2 = get_gap_x(rect.x.end,   -1);
  
  if(abs(x2 - x1) < 1e-4) {
    x1-= 0.75f;
    x2+= 0.75f;
  }
  
  float y1 = get_gap_y(rect.y.start, +1);
  float y2 = get_gap_y(rect.y.end,   -1);
  
  if(abs(y2 - y1) < 1e-4) {
    y1-= 0.75f;
    y2+= 0.75f;
  }
  
  float x0, y0;
  canvas.current_pos(&x0, &y0);
  
  x1 += x0;
  y1 += y0;
  x2 += x0;
  y2 += y0;
  
  canvas.pixrect(x1, y1, x2, y2, false);
}

Box *GridBox::remove_range(int *start, int end) {
  using std::swap;
  
  if(*start >= end) {
    if(auto par = parent()) {
      *start = _index + 1;
      return par->move_logical(LogicalDirection::Backward, true, start);
    }
    
    return this;
  }
  
  auto rect = get_enclosing_range(*start, end);
  
  if( rect.x.start == GridXIndex(0) && 
      rect.y.start == GridYIndex(0) && 
      rect.x.end == GridXIndex(cols()) && 
      rect.y.end == GridYIndex(rows())) 
  {
    if(auto par = parent()) {
      *start = _index;
      return par->remove(start);
    }
    
    *start = 0;
    return items[0]->content();
  }
  
  if(auto par = parent()) {
    if(cols() == 1) {
      if(rect.y.start == GridYIndex(1) && rect.y.end == GridYIndex(rows())) {
        *start = _index;
        if(auto seq = dynamic_cast<AbstractSequence *>(par)) {
          auto content = items[0]->content();
          *start = seq->insert(_index, content, 0, content->length());
        }
        
        return par->remove(start);
      }
      else if(rect.y.start == GridYIndex(0) && rect.y.end == GridYIndex(rows() - 1)) {
        *start = _index;
        if(auto seq = dynamic_cast<AbstractSequence *>(par)) {
          auto content = items[items.length() - 1]->content();
          *start = seq->insert(_index, content, 0, content->length());
        }
        
        return par->remove(start);
      }
      else if(rect.y.start == GridYIndex(0) && rect.y.end == GridYIndex(rows())) {
        *start = _index;
        return par->remove(start);
      }
    }
    
    if(rows() == 1) {
      if(rect.x.start == GridXIndex(1) && rect.x.end == GridXIndex(cols())) {
        *start = _index;
        if(auto seq = dynamic_cast<AbstractSequence *>(par)) {
          auto content = items[0]->content();
          *start = seq->insert(_index, content, 0, content->length());
        }
        
        return par->remove(start);
      }
      else if(rect.x.start == GridXIndex(0) && rect.x.end == GridXIndex(cols() - 1)) {
        *start = _index;
        if(auto seq = dynamic_cast<AbstractSequence *>(par)) {
          auto content = items[items.length() - 1]->content();
          *start = seq->insert(_index, content, 0, content->length());
        }
        
        return par->remove(start);
      }
      else if(rect.x.start == GridXIndex(0) && rect.x.end == GridXIndex(cols())) {
        *start = _index;
        return par->remove(start);
      }
    }
  }
  
  if(rect.cols() == cols()) {
    remove_rows(rect.y.start, rect.rows());
    if(rect.y.start > GridYIndex(0)) {
      auto result = item(rect.y.start - 1, GridXIndex(cols() - 1))->content();
      *start = result->length();
      return result;
    }
    
    if(auto par = parent()) {
      *start = _index;
      return par;
    }
    
    *start = items[0]->content()->length();
    return items[0]->content();
  }
  
  if(rect.rows() == rows()) {
    remove_cols(rect.x.start, rect.cols());
    if(rect.x.start > GridXIndex(0)) {
      auto result = item(GridYIndex(0), rect.x.start - 1)->content();
      *start = result->length();
      return result;
    }
    
    if(auto par = parent()) {
      *start = _index;
      return par;
    }
    
    *start = items[0]->content()->length();
    return items[0]->content();
  }
  
  for(GridXIndex x : rect.x)
    for(GridYIndex y : rect.y) {
      item(y, x)->content()->remove(0, item(y, x)->content()->length());
      item(y, x)->content()->insert(0, PMATH_CHAR_PLACEHOLDER);
    }
    
  if(rect.y.start == GridYIndex(0) && rect.rows() == 1 && rect.cols() == 1) {
    bool all_empty = true;
    for(GridYIndex y = GridYIndex(1); y < GridYIndex(rows()); ++y) {
      if( item(y, rect.x.start)->content()->length() > 0 &&
          !item(y, rect.x.start)->content()->is_placeholder())
      {
        all_empty = false;
        break;
      }
    }
    
    if(all_empty) {
      remove_cols(rect.x.start, 1);
      if(rect.x.start > GridXIndex(0)) {
        auto result = item(rect.y.start, rect.x.start - 1)->content();
        *start = result->length();
        return result;
      }
      
      if(auto par = parent()) {
        *start = _index;
        return par;
      }
      
      *start = items[0]->content()->length();
      return items[0]->content();
    }
  }
  
  if(rect.x.start == GridXIndex(0) && rect.cols() == 1 && rect.rows() == 1) {
    bool all_empty = true;
    for(GridXIndex x = GridXIndex(1); x < GridXIndex(cols()); ++x) {
      if( item(rect.y.start, x)->content()->length() > 0 &&
          !item(rect.y.start, x)->content()->is_placeholder())
      {
        all_empty = false;
        break;
      }
    }
    
    if(all_empty) {
      remove_rows(rect.y.start, 1);
      if(rect.y.start > GridYIndex(0)) {
        auto result = item(rect.y.start - 1, GridXIndex(cols() - 1))->content();
        *start = result->length();
        return result;
      }
      
      if(auto par = parent()) {
        *start = _index;
        return par;
      }
      
      *start = items[0]->content()->length();
      return items[0]->content();
    }
  }
  
  *start = yx_to_index(rect.y.start, rect.x.start);
  if(*start > 0) {
    auto result = items[*start - 1]->content();
    *start = result->length();
    return result;
  }
  
  if(auto par = parent()) {
    *start = _index;
    return par;
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
  auto rect = get_enclosing_range(start, end);
  
  Expr mat = MakeCall(Symbol(richmath_System_List), rect.rows());
  
  for(GridYIndex y : rect.y) {
    Expr row = MakeCall(Symbol(richmath_System_List), rect.cols());
    
    for(GridXIndex x : rect.x) 
      row.set(x - rect.x.start + 1, item(y, x)->to_pmath(flags));
    
    mat.set(y - rect.y.start + 1, row);
  }
  
  Gather g;
  g.emit(mat);
  
  if(style)
    style->emit_to_pmath();
    
  Expr e = g.end();
  e.set(0, Symbol(richmath_System_GridBox));
  return e;
}

Box *GridBox::move_logical(LogicalDirection direction, bool jumping, int *index) {
  if(*index < count() || (*index == count() && direction == LogicalDirection::Backward))
    return base::move_logical(direction, jumping, index);
  
  if(*index > length() && direction == LogicalDirection::Backward) { // from outside backwards
    //// for debugging: select gaps instead of last item:
    //*index = length();
    //return this;
    *index = count() + 1;
    return base::move_logical(direction, jumping, index);
  }
  
  GridYIndex row;
  GridXIndex col;
  index_to_yx(*index, &row, &col);
  
  if(direction == LogicalDirection::Forward) {
    if(col < GridXIndex(cols()))
      ++col;
  }
  else {
    if(col > GridXIndex(0))
      --col;
  }
  *index = yx_to_gap_index(row, col);
  return this;
}

Box *GridBox::move_vertical(
  LogicalDirection  direction,
  float            *index_rel_x,
  int              *index,
  bool              called_from_child
) {
  GridYIndex row;
  GridXIndex col;
  need_pos_vectors();
  
  if(*index < 0) {
    if(direction == LogicalDirection::Forward)
      row = GridYIndex(0);
    else
      row = GridYIndex(rows() - 1);
      
    col = GridXIndex(0);
    while(col < GridXIndex(cols() - 1) && *index_rel_x > xpos[col.primary_value() + 1])
      ++col;
      
    *index_rel_x -= xpos[col.primary_value()];
  }
  else {
    index_to_yx(*index, &row, &col);
    if(direction == LogicalDirection::Forward)
      ++row;
    else
      --row;
  }
  
  if(*index >= items.length()) {
    if(row < GridYIndex(0) || row > GridYIndex(rows())) {
      if(auto par = parent()) {
        *index_rel_x += col < GridXIndex(cols()) ? xpos[col.primary_value()] : _extents.width;
        *index = _index;
        return par->move_vertical(direction, index_rel_x, index, true);
      }
      
      return this;
    }
    
    *index = yx_to_gap_index(row, col);
    return this;
  }
  
  if(row < GridYIndex(0) || row >= GridYIndex(rows())) {
    if(auto par = parent()) {
      *index_rel_x += xpos[col.primary_value()];
      *index = _index;
      return par->move_vertical(direction, index_rel_x, index, true);
    }
    
    return this;
  }
  
  *index = -1;
  return item(row, col)->move_vertical(direction, index_rel_x, index, false);
}

VolatileSelection GridBox::mouse_selection(Point pos, bool *was_inside_start) {
  need_pos_vectors();
  
  const int num_cols = cols();
  const int num_rows = rows();
  
  int col = 0;
  while(col < num_cols - 1 && xpos[col + 1] <= pos.x)
    ++col;
  
  pos.y += _extents.ascent;
  
  int row = 0;
  while(row < num_rows - 1 && ypos[row + 1] <= pos.y)
    ++row;
    
  while(row > 0 && item(row, col)->really_span_from_above())
    --row;
    
  while(col > 0 && item(row, col)->really_span_from_left())
    --col;
    
  GridItem *gi = item(row, col);
  pos -= Vector2F{xpos[col], ypos[row] + gi->extents().ascent};
  
  switch(selection_strategy.kind) {
    case GridSelectionStrategy::Kind::ContentsOrGaps:
    case GridSelectionStrategy::Kind::ContentsOrColumnGaps:
    case GridSelectionStrategy::Kind::ContentsOrRowGaps: {
      enum {TopLeft, Top, TopRight, Left, Center, Right, BottomLeft, Bottom, BottomRight} edge = Center;
      int dx = 1 + gi->span_right();
      int dy = 1 + gi->span_down();
      int NA = 0;
      int start_dcol[9] = { 0,  0, dx,
                            0, NA, dx,
                            0,  0, dx};
      int end_dcol[9]   = { 0, dx, dx, 
                            0, NA, dx, 
                            0, dx, dx};
      int start_drow[9] = { 0,  0,  0, 
                            0, NA,  0,
                           dy, dy, dy};
      int end_drow[9]   = { 0,  0,  0, 
                           dy, NA, dy,
                           dy, dy, dy};
      
      switch(selection_strategy.kind) {
        case GridSelectionStrategy::Kind::ContentsOrGaps: {
            if(pos.x < 0) {
              if(     pos.y < -gi->extents().ascent) edge = TopLeft;
              else if(pos.y > gi->extents().descent) edge = BottomLeft;
              else                                   edge = Left;
            }
            else if(pos.x > gi->extents().width) {
              if(     pos.y < -gi->extents().ascent) edge = TopRight;
              else if(pos.y > gi->extents().descent) edge = BottomRight;
              else                                   edge = Right;
            }
            else {
              if(     pos.y < -gi->extents().ascent) edge = Top;
              else if(pos.y > gi->extents().descent) edge = Bottom;
              else                                   edge = Center;
            }
          } break;
        
        case GridSelectionStrategy::Kind::ContentsOrColumnGaps: {
            if(gi->extents().to_rectangle().contains(pos)) edge = Center;
            else if(pos.x < 0.5f * gi->extents().width)    edge = Left;
            else                                           edge = Right;
          } break;
        
        case GridSelectionStrategy::Kind::ContentsOrRowGaps: {
            if(gi->extents().to_rectangle().contains(pos))                        edge = Center;
            else if(pos.y + gi->extents().ascent < 0.5f * gi->extents().height()) edge = Top;
            else                                                                  edge = Bottom;
          } break;
        
        default: break; // should not happen
      }
      
      if(edge != Center) {
        auto rect = GridIndexRect::FromYX(
          GridYRange::Hull(GridYIndex{row} + start_drow[edge], GridYIndex{row} + end_drow[edge]),
          GridXRange::Hull(GridXIndex{col} + start_dcol[edge], GridXIndex{col} + end_dcol[edge]));
        
        if(rect.rows() == 0 && rect.cols() < selection_strategy.expected_cols) {
          if(rect.x.start + selection_strategy.expected_cols <= GridXIndex{num_cols})
            rect.x = GridXRange::Hull(rect.x.start, rect.x.start + selection_strategy.expected_cols);
          else if(selection_strategy.expected_cols <= num_cols)
            rect.x = GridXRange::Hull(GridXIndex{num_cols} - selection_strategy.expected_cols, GridXIndex{num_cols});
          else
            rect.x = GridXRange::Hull(GridXIndex{0}, GridXIndex{num_cols});
        }
        else if(rect.cols() == 0) {
          if(rect.y.start + selection_strategy.expected_rows <= GridYIndex{num_rows})
            rect.y = GridYRange::Hull(rect.y.start, rect.y.start + selection_strategy.expected_rows);
          else if(selection_strategy.expected_rows <= num_rows)
            rect.y = GridYRange::Hull(GridYIndex{num_rows} - selection_strategy.expected_rows, GridYIndex{num_rows});
          else
            rect.y = GridYRange::Hull(GridYIndex{0}, GridYIndex{num_rows});
        }
        
        return gap_selection(rect);
      }
    } break;
    
    case GridSelectionStrategy::Kind::ContentsOnly:
      break;
  }
  
  if( selection_strategy.expected_cols > 0 && 
      selection_strategy.expected_rows > 0 &&
      selection_strategy.expected_cols <= num_cols &&
      selection_strategy.expected_rows <= num_rows &&
      gi->content()->is_placeholder())
  {
    // TODO: properly support \[SpanFromLeft], etc.
    
    //GridXIndex last_col = std::min(GridXIndex{col} + selection_strategy.expected_cols/2, GridXIndex{num_cols});
    GridXIndex last_col = std::min(GridXIndex{col} + selection_strategy.expected_cols, GridXIndex{num_cols});
    for(auto x = GridXIndex{col} + 1; x < last_col; ++x) {
      if(!item(GridYIndex{row}, x)->content()->is_placeholder()) {
        last_col = x;
        break;
      }
    }
    
    GridXIndex first_col = std::max(GridXIndex{0}, last_col - selection_strategy.expected_cols);
    for(auto x = GridXIndex{col} - 1; x >= first_col; --x) {
      if(!item(GridYIndex{row}, x)->content()->is_placeholder()) {
        first_col = x + 1;
        last_col = std::min(first_col + selection_strategy.expected_cols, GridXIndex{num_cols});
        break;
      }
    }
    
    //GridYIndex last_row = std::min(GridYIndex{row} + selection_strategy.expected_rows/2, GridYIndex{num_rows});
    GridYIndex last_row = std::min(GridYIndex{row} + selection_strategy.expected_rows, GridYIndex{num_rows});
    for(auto y = GridYIndex{row} + 1; y < last_row; ++y) {
      if(!item(y, GridXIndex{col})->content()->is_placeholder()) {
        last_row = y;
        break;
      }
    }
    
    GridYIndex first_row = std::max(GridYIndex{0}, last_row - selection_strategy.expected_rows);
    for(auto y = GridYIndex{row} - 1; y >= first_row; --y) {
      if(!item(y, GridXIndex{col})->content()->is_placeholder()) {
        first_row = y + 1;
        last_row = std::min(first_row + selection_strategy.expected_rows, GridYIndex{num_rows});
        break;
      }
    }
    
    auto rect = GridIndexRect::FromYX(
                  GridYRange::Hull(first_row, last_row),
                  GridXRange::Hull(first_col, last_col));
    
    if( rect.rows() == selection_strategy.expected_rows && 
        rect.cols() == selection_strategy.expected_cols)
    {
      bool can_overwrite = true;
      for(auto x : rect.x)
        for(auto y : rect.y)
          if(!item(y, x)->content()->is_placeholder()) {
            can_overwrite = false;
            break;
          }
      
      if(can_overwrite) 
        return gap_selection(rect);
    }
  }
  
  return gi->mouse_selection(pos, was_inside_start);
}

void GridBox::child_transformation(
  int             index,
  cairo_matrix_t *matrix
) {
  need_pos_vectors();
  
  GridYIndex row;
  GridXIndex col;
  index_to_yx(index, &row, &col);
  
  float x, y;
  if(index < items.length()) {
    x = xpos[col.primary_value()];
    y = ypos[row.primary_value()] + item(index)->extents().ascent - _extents.ascent;
  }
  else {
    x = col.primary_value() < cols() ? xpos[col.primary_value()] : _extents.width;
    y = row.primary_value() < rows() ? ypos[row.primary_value()] : _extents.height();
    y-= _extents.ascent;
  }
  cairo_matrix_translate(matrix, x, y);
}

GridIndexRect GridBox::get_enclosing_range(int start, int end) {
  using std::swap;
  if(end < start)
    swap(start, end);

  if(0 < end && end <= items.length() && start < items.length()) {
    end-= 1;
  }
  
  GridXIndex ax, bx;
  GridYIndex ay, by;
  index_to_yx(start, &ay, &ax);
  index_to_yx(end,   &by, &bx); 
  
  if(start < items.length())
    return GridIndexRect::FromYX(GridYRange::InclusiveHull(ay, by), GridXRange::InclusiveHull(ax, bx));
  else
    return GridIndexRect::FromYX(GridYRange::Hull(ay, by), GridXRange::Hull(ax, bx));
}

VolatileSelection GridBox::normalize_selection(int start, int end) {
  using std::swap;
  if(end < start) 
    swap(start, end);
  
  auto rect = get_enclosing_range(start, end);
  
  if(start >= items.length()) {
    //if(rect.cols() == 0 || rect.rows() == 0) 
      return {this, start, end};
  }
  
  if(start == end) {
    if(start == items.length())
      --start;
    else if(start < items.length())
      ++end;
  }
  
  start = yx_to_index(rect.y.start, rect.x.start);
  end   = yx_to_index(rect.y.end - 1, rect.x.end - 1) + 1;
  
  if(start + 1 == end) {
    auto content = items[start]->content();
    return {content, 0, content->length()};
  }
  
  if(start == 0 && end == count())
    return base::normalize_selection(start, end);
    
  return {this, start, end};
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

GridSelectionStrategy GridBox::best_selection_strategy_for_drag_source(const VolatileSelection &sel) {
  if(dynamic_cast<AbstractSequence*>(sel.box)) {
    if(auto grid = dynamic_cast<GridBox*>(sel.contained_box())) {
      return GridSelectionStrategy(GridSelectionStrategy::Kind::ContentsOrGaps, grid->rows(), grid->cols());
    }
  }
  
  if(auto grid = dynamic_cast<GridBox*>(sel.box)) {
    GridIndexRect rect = grid->get_enclosing_range(sel.start, sel.end);
    if(rect.rows() == grid->rows())
      return GridSelectionStrategy(GridSelectionStrategy::Kind::ContentsOrColumnGaps, rect);
    else if(rect.cols() == grid->cols())
      return GridSelectionStrategy(GridSelectionStrategy::Kind::ContentsOrRowGaps, rect);
    else if(rect.rows() > rect.cols())
      return GridSelectionStrategy(GridSelectionStrategy::Kind::ContentsOrColumnGaps, rect);
    else if(rect.rows() < rect.cols())
      return GridSelectionStrategy(GridSelectionStrategy::Kind::ContentsOrRowGaps, rect);
    else
      return GridSelectionStrategy(GridSelectionStrategy::Kind::ContentsOrGaps, rect);
  }
  return GridSelectionStrategy::ContentsOnly;
}

//} ... class GridBox

//{ class GridBox::Impl ...

int GridBox::Impl::resize_items(Context &context) {
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
          self.item(y, x + gi->_span_right)->really_span_from_left(true);
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
            self.item(y + gi->_span_down, i)->really_span_from_above(true);
            
          gi->_span_down++;
        }
        gi->_span_down--;
        
      END_DOWN:
        if(gi->_span_right > 0 || gi->_span_down > 0)
          ++span_count;
      }
      else {
        gi->_span_right = 0;
        gi->_span_down = 0;
      }
    }
  }
  
  return span_count;
}

void GridBox::Impl::simple_spacing(float em) {
  float right = 0;//colspacing / 2;
  self.xpos.length(self.items.cols());
  
  for(int x = 0; x < self.xpos.length(); ++x) {
    float w = 0;
    for(int y = 0; y < self.rows(); ++y) {
      GridItem *gi = self.item(y, x);
      
      if(gi->really_span_from_left()) {
        if(x + 1 == self.cols() || !self.item(y, x + 1)->really_span_from_left()) {
          int basex = x - 1;
          while(self.item(y, basex)->really_span_from_left()) {
            --basex;
          }
          
          float rest_w = self.item(y, basex)->extents().width - right + self.xpos[basex];
          if(w < rest_w)
            w = rest_w;
        }
      }
      else if(!gi->_span_right &&
              !gi->really_span_from_above() &&
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
          !gi->really_span_from_left() &&
          !gi->really_span_from_above())
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

void GridBox::Impl::expand_colspans(int span_count) {
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
              
              if( !gi2->_span_right             &&
                  !gi2->really_span_from_left() &&
                  !gi2->really_span_from_above())
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

void GridBox::Impl::expand_rowspans(int span_count) {
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
              
              if( !gi2->_span_right             &&
                  !gi2->_span_down              &&
                  !gi2->really_span_from_left() &&
                  !gi2->really_span_from_above())
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

float GridBox::Impl::calculate_ascent_for_baseline_position(float em, Expr baseline_pos) const {
  float height = self._extents.height();
  
  if(baseline_pos == richmath_System_Bottom) 
    return height;
  
  if(baseline_pos == richmath_System_Top) 
    return 0;
  
  if(baseline_pos == richmath_System_Center) 
    return 0.5f * height;
  
  if(baseline_pos == richmath_System_Axis || baseline_pos == richmath_System_Automatic) 
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
  else if(baseline_pos[0] == richmath_System_List) {
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

void GridBox::Impl::adjust_baseline(float em) {
  float height = self._extents.height();
  
  float ascent = calculate_ascent_for_baseline_position(em, self.get_style(BaselinePosition));
  self._extents.ascent = ascent;
  self._extents.descent = height - ascent;
}

GridItem *GridBox::Impl::create_item() {
  return new GridItem(BoxFactory::create_sequence(self.layout_kind()));
}

//} ... class GridBox::Impl
