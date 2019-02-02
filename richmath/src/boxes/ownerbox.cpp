#include <boxes/ownerbox.h>

#include <boxes/mathsequence.h>
#include <graphics/context.h>

using namespace richmath;

extern pmath_symbol_t richmath_System_Axis;
extern pmath_symbol_t richmath_System_Baseline;
extern pmath_symbol_t richmath_System_Bottom;
extern pmath_symbol_t richmath_System_Center;
extern pmath_symbol_t richmath_System_GridBox;
extern pmath_symbol_t richmath_System_Scaled;
extern pmath_symbol_t richmath_System_Top;

class OwnerBox::Impl {
  public:
    Impl(OwnerBox &_self) : self(_self) {}
    
    float calculate_baseline(float em, Expr baseline_pos) const;
    void adjust_baseline(float em);
  
  private:
    OwnerBox &self;
};

//{ class OwnerBox ...

OwnerBox::OwnerBox(MathSequence *content)
  : Box(),
  _content(content)
{
  if(!_content)
    _content = new MathSequence;
    
  adopt(_content, 0);
}

OwnerBox::~OwnerBox() {
  delete _content;
}

Box *OwnerBox::item(int i) {
  return _content;
}

void OwnerBox::resize_default_baseline(Context *context) {
  _content->resize(context);
  _extents = _content->extents();
  cx = 0;
  cy = 0;
}

void OwnerBox::adjust_baseline_after_resize(Context *context) {
  Impl(*this).adjust_baseline(context->canvas->get_font_size());
}

float OwnerBox::calculate_scaled_baseline(double scale) const {
  //return -_extents.descent * (1 - scale) + _extents.ascent * scale
  return (float)(-(double)_extents.descent + scale * (double)(_extents.ascent + _extents.descent));
}

void OwnerBox::paint(Context *context) {
  update_dynamic_styles(context);
    
  paint_content(context);
}

void OwnerBox::paint_content(Context *context) {
  context->canvas->rel_move_to(cx, cy);
  
  Expr expr;
  if(style && context->stylesheet->get(style, TextShadow, &expr))
    context->draw_with_text_shadows(_content, expr);
  else
    _content->paint(context);
}

Box *OwnerBox::remove(int *index) {
  if(_parent) {
    *index = _index;
    if(auto seq = dynamic_cast<AbstractSequence*>(_parent)) {
      int s = _index;
      int e = _index + 1;
      seq->normalize_selection(&s, &e);
      seq->insert(e, _content, 0, _content->length());
    }
    return _parent->remove(index);
  }
  *index = 0;
  return _content;
}

Expr OwnerBox::to_pmath(BoxOutputFlags flags) {
  return _content->to_pmath(flags);
}

Box *OwnerBox::move_vertical(
  LogicalDirection  direction,
  float            *index_rel_x,
  int              *index,
  bool              called_from_child
) {
  if(*index < 0) {
    *index_rel_x -= cx;
    return _content->move_vertical(direction, index_rel_x, index, false);
  }
  
  *index_rel_x+= cx;
  return Box::move_vertical(direction, index_rel_x, index, called_from_child);
}

Box *OwnerBox::mouse_selection(
  float  x,
  float  y,
  int   *start,
  int   *end,
  bool  *was_inside_start
) {
  x -= cx;
  y -= cy;
  return _content->mouse_selection(x, y, start, end, was_inside_start);
}

void OwnerBox::child_transformation(
  int             index,
  cairo_matrix_t *matrix
) {
  cairo_matrix_translate(matrix,
                         cx,
                         cy/* + _extents.ascent*/);
}

bool OwnerBox::edit_selection(Context *context) {
  if(Box::edit_selection(context)) {
    int auto_delete;
    
    if(context->stylesheet) {
      if(!context->stylesheet->get(style, AutoDelete, &auto_delete))
        auto_delete = 0;
    }
    else if(!style || !style->get(AutoDelete, &auto_delete))
      auto_delete = 0;
      
    Box *selbox = context->selection.get();
    if(auto_delete && selbox != this) {
      if(auto seq = dynamic_cast<MathSequence*>(_parent)) {
        if(selbox == _content) {
          context->selection.set(seq,
                                 context->selection.start + _index,
                                 context->selection.end   + _index);
        }
        
        seq->insert(_index + 1, _content, 0, _content->length());
        seq->remove(_index, _index + 1);
      }
    }
    
    return true;
  }
  
  return false;
}

//} ... class OwnerBox

//{ ... class InlineSequenceBox

bool InlineSequenceBox::try_load_from_object(Expr expr, BoxInputFlags options){
  if(expr[0] == PMATH_SYMBOL_LIST) {
    _content->load_from_object(expr, options);
    
    finish_load_from_object(std::move(expr));
    return true;
  }
  
  return false;
}

void InlineSequenceBox::resize_default_baseline(Context *context) {
  bool old_math_spacing = context->math_spacing;
  context->math_spacing = true;
  OwnerBox::resize_default_baseline(context);
  context->math_spacing = old_math_spacing;
}

void InlineSequenceBox::paint(Context *context) {
  bool old_math_spacing = context->math_spacing;
  context->math_spacing = true;
  
  Box *b = context->selection.get();
  while(b && b != this)
    b = b->parent();
    
  if(b == this) {
    float x, y;
    int c = context->canvas->get_color();
    context->canvas->current_pos(&x, &y);
    context->canvas->pixrect(
      x,
      y - _extents.ascent - 1,
      x + _extents.width,
      y + _extents.descent + 1,
      false);
      
    context->canvas->set_color(0xF6EDD6);
    context->canvas->fill();
    context->canvas->set_color(c);
    context->canvas->move_to(x, y);
  }
  
  OwnerBox::paint(context);
  context->math_spacing = old_math_spacing;
}

void InlineSequenceBox::on_enter() {
  request_repaint_all();
  
  OwnerBox::on_enter();
}

void InlineSequenceBox::on_exit() {
  request_repaint_all();
  
  OwnerBox::on_exit();
}

//} ... class InlineSequenceBox

static const float NormalTextAxisFactor = 0.25f; // TODO: use actual math axis from font
static const float NormalTextDescentFactor = 0.25f;
static const float NormalTextAscentFactor = 0.75f;

float OwnerBox::Impl::calculate_baseline(float em, Expr baseline_pos) const {
  if(baseline_pos == richmath_System_Bottom) 
    return self.calculate_scaled_baseline(0);
  
  if(baseline_pos == richmath_System_Top) 
    return self.calculate_scaled_baseline(1);
  
  if(baseline_pos == richmath_System_Center) 
    return self.calculate_scaled_baseline(0.5);
  
  if(baseline_pos == richmath_System_Axis) 
    return NormalTextAxisFactor * em - self.cy; // TODO: use actual math axis from font
    
  if(baseline_pos == richmath_System_Baseline) 
    return -self.cy;
    
  if(baseline_pos[0] == richmath_System_Scaled) {
    double factor = 0.0;
    if(get_factor_of_scaled(baseline_pos, &factor) && isfinite(factor)) 
      return self.calculate_scaled_baseline(factor);
  }
  else if(baseline_pos.is_rule()) {
    float lhs_y = calculate_baseline(em, baseline_pos[1]);
    Expr rhs = baseline_pos[2];
    
    if(rhs == richmath_System_Axis) {
      float ref_pos = NormalTextAxisFactor * em; // TODO: use actual math axis from font
      return lhs_y - ref_pos;
    }
    else if(rhs == richmath_System_Baseline) {
      float ref_pos = self.cy;
      return lhs_y - ref_pos;
    }
    else if(rhs == richmath_System_Bottom) {
      float ref_pos = - NormalTextDescentFactor * em;
      return lhs_y - ref_pos;
    }
    else if(rhs == richmath_System_Center) {
      float ref_pos = (NormalTextAscentFactor - NormalTextDescentFactor) * em; // (bottom + top)/2
      return lhs_y - ref_pos;
    }
    else if(rhs == richmath_System_Top) {
      float ref_pos = NormalTextAscentFactor * em;
      return lhs_y - ref_pos;
    }
    else if(rhs[0] == richmath_System_Scaled) {
      double factor = 0.0;
      if(get_factor_of_scaled(rhs, &factor) && isfinite(factor)) {
        //float ref_pos = NormalTextAscentFactor * em * factor - NormalTextDescentFactor * em * (1 - factor);
        float ref_pos = ((NormalTextAscentFactor + NormalTextDescentFactor) * factor - NormalTextDescentFactor) * em;
        return lhs_y - ref_pos;
      }
    }
  }
  
  // baseline_pos == PMATH_SYMBOL_AUTOMATIC
  return 0;
}

void OwnerBox::Impl::adjust_baseline(float em) {
  float y = calculate_baseline(em, self.get_style(BaselinePosition));
  self.cy += y;
  self._extents.ascent-= y;
  self._extents.descent+= y;
}
