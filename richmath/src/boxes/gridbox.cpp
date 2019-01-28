#include <boxes/gridbox.h>

#include <boxes/mathsequence.h>
#include <graphics/context.h>

using namespace richmath;

extern pmath_symbol_t richmath_System_GridBox;

namespace richmath {
  class GridBoxImpl {
    public:
      GridBoxImpl(GridBox &_self) : self(_self) {}
      
      void simple_spacing(float em);
      int resize_items(Context *context);
      void adjust_baseline(float em);
    
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

void GridItem::resize(Context *context) {
  bool smf = context->smaller_fraction_parts;
  context->smaller_fraction_parts = true;
  
  OwnerBox::resize(context);
  
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
  adopt(items[0], 0);
}

GridBox::GridBox(int rows, int cols)
  : Box(),
    items(rows > 0 ? rows : 1, cols > 0 ? cols : 1)
{
  for(int i = 0; i < items.length(); ++i) {
    items[i] = new GridItem();
    adopt(items[i], i);
  }
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
      
  for(int i = 0; i < items.length(); ++i)
    adopt(items[i], i);
    
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
      
  for(int i = 0; i < items.length(); ++i)
    adopt(items[i], i);
    
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
  for(int i = 0; i < items.length(); ++i)
    adopt(items[i], i);
    
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
  for(int i = 0; i < items.length(); ++i)
    adopt(items[i], i);
    
  invalidate();
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
  
  int spans = span_count;
  int first_span = 0;
  while(spans-- > 0 && first_span < count()) {
    for(int x = 0; x < cols(); ++x) {
      for(int y = 0; y < rows(); ++y) {
        GridItem *gi = item(y, x);
        
        if(gi->_span_right) {
          float w;
          if(x + gi->_span_right + 1 < cols())
            w = xpos[x + gi->_span_right + 1] - colspacing;
          else
            w = _extents.width;
            
          w -= xpos[x];
          
          if(w > gi->extents().width) {
            BoxSize size = gi->extents();
            size.width = w;
            
            gi->expand(size);
          }
          else if(gi->index() >= first_span &&
                  w < gi->extents().width)
          {
            float delta = gi->extents().width - w;
            
            for(int x2 = x + gi->_span_right + 1; x2 < cols(); ++x2) {
              xpos[x2] += delta;
            }
            _extents.width += delta;
            
            for(int y2 = 0; y2 < rows(); ++y2) {
              GridItem *gi2 = item(y2, x + gi->_span_right);
              
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
  
  spans = span_count;
  first_span = 0;
  while(spans-- > 0 && first_span < count()) {
    for(int y = 0; y < rows(); ++y) {
      for(int x = 0; x < cols(); ++x) {
        GridItem *gi = item(y, x);
        
        if(gi->_span_down) {
          float h;
          if(y + gi->_span_down + 1 < rows())
            h = ypos[y + gi->_span_down + 1] - rowspacing;
          else
            h = _extents.height();
            
          h -= ypos[y];
          
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
            
            for(int y2 = y + gi->_span_down + 1; y2 < rows(); ++y2) {
              ypos[y2] += delta;
            }
            _extents.ascent +=  delta / 2;
            _extents.descent += delta / 2;
            
            for(int x2 = 0; x2 < cols(); ++x2) {
              GridItem *gi2 = item(y + gi->_span_down, x2);
              
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

  GridBoxImpl(*this).adjust_baseline(em);
}

void GridBox::paint(Context *context) {
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
    int ax, ay, bx, by;
    items.index_to_yx(context->selection.start, &ay, &ax);
    items.index_to_yx(context->selection.end - 1,   &by, &bx);
    
    if(bx < ax) {
      int tmp = ax;
      ax = bx;
      bx = tmp;
    }
    
    if(by < ay) {
      int tmp = ay;
      ay = by;
      by = tmp;
    }
    
    float x1, x2, y1, y2;
    x1 = x + xpos[ax];
    if(bx < cols() - 1)
      x2 = x + xpos[bx] + item(by, bx)->extents().width;
    else
      x2 = x + _extents.width;
      
    y1 = y - _extents.ascent + ypos[ay];// - item(ay, ax)->extents().ascent;
    if(by < rows() - 1)
      y2 = y - _extents.ascent + ypos[by] + item(by, bx)->extents().height();
    else
      y2 = y + _extents.descent;
      
    int c = context->canvas->get_color();
    context->canvas->pixrect(x1, y1, x2, y2, false);
    context->draw_selection_path();
//    context->canvas->paint_selection(x1, y1, x2, y2);
    context->canvas->set_color(c);
  }
}

void GridBox::selection_path(Canvas *canvas, int start, int end) {
  int ax, ay, bx, by;
  if(end > start)
    --end;
    
  items.index_to_yx(start, &ay, &ax);
  items.index_to_yx(end,   &by, &bx);
  
  if(bx < ax) {
    int tmp = ax;
    ax = bx;
    bx = tmp;
  }
  
  if(by < ay) {
    int tmp = ay;
    ay = by;
    by = tmp;
  }
  
  float x1, x2, y1, y2;
  x1 = xpos[ax];
  if(bx < cols() - 1)
    x2 = xpos[bx] + item(by, bx)->extents().width;
  else
    x2 = _extents.width;
    
  y1 = - _extents.ascent + ypos[ay];
  if(by < rows() - 1)
    y2 = - _extents.ascent + ypos[by] + item(by, bx)->extents().height();
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
  if(*start >= end) {
    if(_parent) {
      *start = _index + 1;
      return _parent->move_logical(LogicalDirection::Backward, true, start);
    }
    
    return this;
  }
  
  int ax, ay, bx, by;
  items.index_to_yx(*start,  &ay, &ax);
  items.index_to_yx(end - 1, &by, &bx);
  
  if(bx < ax) {
    int tmp = ax;
    ax = bx;
    bx = tmp;
  }
  
  if(by < ay) {
    int tmp = ay;
    ay = by;
    by = tmp;
  }
  
  if(ax == 0 && ay == 0 && bx == cols() - 1 && by == rows() - 1) {
    if(_parent) {
      *start = _index;
      return _parent->remove(start);
    }
    
    *start = 0;
    return items[0]->content();
  }
  
  if(_parent) {
    if(cols() == 1) {
      if(ay == 1 && by == rows() - 1) {
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
      else if(ay == 0 && by == rows() - 2) {
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
      else if(ay == 0 && by == rows() - 1) {
        *start = _index;
        return _parent->remove(start);
      }
    }
    
    if(rows() == 1) {
      if(ax == 1 && bx == cols() - 1) {
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
      else if(ax == 0 && bx == cols() - 2) {
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
      else if(ax == 0 && bx == cols() - 1) {
        *start = _index;
        return _parent->remove(start);
      }
    }
  }
  
  if(ax == 0 && bx == cols() - 1) {
    remove_rows(ay, by - ay + 1);
    if(ay > 0) {
      MathSequence *result = item(ay - 1, cols() - 1)->content();
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
  
  if(ay == 0 && by == rows() - 1) {
    remove_cols(ax, bx - ax + 1);
    if(ax > 0) {
      MathSequence *result = item(0, ax - 1)->content();
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
  
  for(int x = ax; x <= bx; ++x)
    for(int y = ay; y <= by; ++y) {
      item(y, x)->content()->remove(0, item(y, x)->content()->length());
      item(y, x)->content()->insert(0, PMATH_CHAR_PLACEHOLDER);
    }
    
  if(ay == 0 && by == 0 && ax == bx) {
    bool all_empty = true;
    for(int y = 1; y < rows(); ++y)
      if( item(y, ax)->content()->length() > 0 &&
          !item(y, ax)->content()->is_placeholder())
      {
        all_empty = false;
        break;
      }
      
    if(all_empty) {
      remove_cols(ax, 1);
      if(ax > 0) {
        MathSequence *result = item(ay, ax - 1)->content();
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
  
  if(ax == 0 && bx == 0 && ay == by) {
    bool all_empty = true;
    for(int x = 1; x < cols(); ++x)
      if( item(ay, x)->content()->length() > 0 &&
          !item(ay, x)->content()->is_placeholder())
      {
        all_empty = false;
        break;
      }
      
    if(all_empty) {
      remove_rows(ay, 1);
      if(ay > 0) {
        MathSequence *result = item(ay - 1, cols() - 1)->content();
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
  
  *start = items.yx_to_index(ay, ax);
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
  int ax, ay, bx, by;
  items.index_to_yx(start,   &ay, &ax);
  items.index_to_yx(end - 1, &by, &bx);
  
  if(bx < ax) {
    int tmp = ax;
    ax = bx;
    bx = tmp;
  }
  
  if(by < ay) {
    int tmp = ay;
    ay = by;
    by = tmp;
  }
  
  Expr mat = MakeList(by - ay + 1);
  
  for(int y = ay; y <= by; ++y) {
    Expr row = MakeList(bx - ax + 1);
    
    for(int x = ax; x <= bx; ++x) {
      row.set(x - ax + 1,
              item(y, x)->to_pmath(flags));
    }
    
    mat.set(y - ay + 1, row);
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
    items.index_to_yx(*index, &row, &col);
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
  
  int row, col;
  items.index_to_yx(index, &row, &col);
  
  cairo_matrix_translate(
    matrix,
    xpos[col],
    ypos[row] + item(index)->extents().ascent - _extents.ascent);
}

Box *GridBox::normalize_selection(int *start, int *end) {
  if(*start == *end) {
    if(*start == count())
      --*start;
    else
      ++*end;
  }
  
  int ax, ay, bx, by;
  items.index_to_yx(*start,   &ay, &ax);
  items.index_to_yx(*end - 1, &by, &bx);
  
  if(bx < ax) {
    int tmp = ax;
    ax = bx;
    bx = tmp;
  }
  
  if(by < ay) {
    int tmp = ay;
    ay = by;
    by = tmp;
  }
  
  *start = items.yx_to_index(ay, ax);
  *end   = items.yx_to_index(by, bx) + 1;
  
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

//} ... class GridBox


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

void GridBoxImpl::adjust_baseline(float em) {
  float height = self._extents.height();
  
  Expr baseline_pos = self.get_style(BaselinePosition);
  
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
    }
  }
  
  if(gi) {
    int row, col;
    self.items.index_to_yx(gi->index(), &row, &col);
    
    self._extents.ascent = self.ypos[row] + gi->extents().ascent;
    self._extents.descent = height - self._extents.ascent;
  }
}

