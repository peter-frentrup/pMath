#include <boxes/ownerbox.h>

#include <boxes/mathsequence.h>
#include <graphics/context.h>
#include <util/alignment.h>

#include <math.h>


using namespace richmath;

extern pmath_symbol_t richmath_System_Axis;
extern pmath_symbol_t richmath_System_Baseline;
extern pmath_symbol_t richmath_System_Bottom;
extern pmath_symbol_t richmath_System_BoxData;
extern pmath_symbol_t richmath_System_Center;
extern pmath_symbol_t richmath_System_GridBox;
extern pmath_symbol_t richmath_System_List;
extern pmath_symbol_t richmath_System_Scaled;
extern pmath_symbol_t richmath_System_String;
extern pmath_symbol_t richmath_System_TextData;
extern pmath_symbol_t richmath_System_Top;

class OwnerBox::Impl {
  public:
    Impl(OwnerBox &_self) : self(_self) {}
    
    void adjust_baseline(float em);
  
  private:
    OwnerBox &self;
};

//{ class OwnerBox ...

OwnerBox::OwnerBox(AbstractSequence *content)
  : base(),
  _content(content)
{
  if(!_content)
    _content = new MathSequence;
    
  adopt(_content, 0);
}

OwnerBox::~OwnerBox() {
  delete_owned(_content);
}

Box *OwnerBox::item(int i) {
  return _content;
}

void OwnerBox::resize_inline(Context &context) {
  cx = 0;
  cy = 0;
  base::resize_inline(context);
}

void OwnerBox::resize_default_baseline(Context &context) {
  _content->resize(context);
  _extents = _content->extents();
  cx = 0;
  cy = 0;
}

void OwnerBox::adjust_baseline_after_resize(Context &context) {
  Impl(*this).adjust_baseline(context.canvas().get_font_size());
}

float OwnerBox::calculate_scaled_baseline(double scale) const {
  //return -_extents.descent * (1 - scale) + _extents.ascent * scale
  return (float)(-(double)_extents.descent + scale * (double)(_extents.ascent + _extents.descent));
}

void OwnerBox::resize(Context &context) {
  update_simple_dynamic_styles_on_resize(context);
  resize_default_baseline(context);
  adjust_baseline_after_resize(context);
}

void OwnerBox::before_paint_inline(Context &context) {
  update_dynamic_styles_on_paint(context);
  
  base::before_paint_inline(context);
}

void OwnerBox::paint(Context &context) {
  update_dynamic_styles_on_paint(context);
    
  paint_content(context);
}

void OwnerBox::paint_content(Context &context) {
  context.canvas().rel_move_to(cx, cy);
  
  Expr expr;
  if(style && context.stylesheet->get(style, TextShadow, &expr))
    context.draw_with_text_shadows(_content, expr);
  else
    _content->paint(context);
}

Box *OwnerBox::remove(int *index) {
  if(auto par = parent()) {
    *index = _index;
    VolatileSelection sel = par->normalize_selection(_index, _index + 1);
    if(auto seq = dynamic_cast<AbstractSequence*>(sel.box)) {
      if(!is_parent_of(seq))
        seq->insert(sel.end, _content, 0, _content->length());
    }
    return par->remove(index);
  }
  *index = 0;
  return _content;
}

Expr OwnerBox::to_pmath_impl(BoxOutputFlags flags) {
  return _content->to_pmath(flags);
}

Box *OwnerBox::move_vertical(
  LogicalDirection  direction,
  float            *index_rel_x,
  int              *index,
  bool              called_from_child
) {
  if(*index < 0) { // called from parent
    if(style && get_own_style(Selectable, AutoBoolAutomatic) == AutoBoolFalse) {
      if(*index_rel_x <= _extents.width)
        *index = _index;
      else
        *index = _index + 1;
        
      return parent();
    }
    
    *index_rel_x -= cx;
    return _content->move_vertical(direction, index_rel_x, index, false);
  }
  
  *index_rel_x+= cx;
  return base::move_vertical(direction, index_rel_x, index, called_from_child);
}

VolatileSelection OwnerBox::mouse_selection(Point pos, bool *was_inside_start) {
  pos -= {cx, cy};
  
  if(get_own_style(Selectable, AutoBoolAutomatic) == AutoBoolFalse) {
    auto sel = _content->mouse_selection(pos, was_inside_start);
    if(sel && sel.box->mouse_sensitive())
      return sel;
    
    *was_inside_start = 0 <= pos.x && pos.x <= _extents.width;
    return { parent(), _index, _index + 1 };
  }

  return _content->mouse_selection(pos, was_inside_start);
}

void OwnerBox::child_transformation(
  int             index,
  cairo_matrix_t *matrix
) {
  cairo_matrix_translate(matrix,
                         cx,
                         cy/* + _extents.ascent*/);
}

bool OwnerBox::edit_selection(SelectionReference &selection, EditAction action) {
  if(!base::edit_selection(selection, action))
    return false;
  
  if(action == EditAction::DryRun)
    return true;
  
  bool auto_delete = 0 != get_own_style(AutoDelete, false);
  
  Box *selbox = selection.get();
  if(auto_delete && selbox != this) {
    if(auto seq = dynamic_cast<AbstractSequence*>(parent())) {
      if(selbox == _content) {
        selection.set(seq,
                      selection.start + _index,
                      selection.end   + _index);
      }
      
      seq->insert(_index + 1, _content, 0, _content->length());
      seq->remove(_index, _index + 1);
    }
  }
  
  return true;
}

//} ... class OwnerBox

//{ class ExpandableOwnerBox ...

bool ExpandableOwnerBox::expand(Context &context, const BoxSize &size) {
  BoxSize size2 = size;
  float dw = _extents.width - _content->extents().width;
  float t = _extents.ascent  - _content->extents().ascent;
  float b = _extents.descent - _content->extents().descent;
  size2.width   -= dw;
  size2.ascent  -= t;
  size2.descent -= b;
  
  bool any_change = false;
  if(style) {
    ContextState cc(context);
    cc.begin(nullptr);
    cc.apply_layout_styles(style);

    any_change = _content->expand(context, size2);

    cc.end();
  }
  else
    any_change = _content->expand(context, size2);

  if(any_change) {
    _extents          = _content->extents();
    _extents.width   += dw;
    _extents.ascent  += t;
    _extents.descent += b;
  }
  
  return any_change;
}

//} ... class ExpandableOwnerBox

//{ ... class InlineSequenceBox

MathSequence *InlineSequenceBox::as_inline_span() {
  return dynamic_cast<MathSequence*>(content());
}

bool InlineSequenceBox::try_load_from_object(Expr expr, BoxInputFlags options) {
  if(expr.item_equals(0, richmath_System_BoxData)) {
    if(content()->kind() != LayoutKind::Math)
      return false;
    
    if(expr.expr_length() != 1)
      return false;
    
    _content->load_from_object(expr[1], options);
    has_explicit_head(true);
  }
  else if(expr.item_equals(0, richmath_System_TextData)) {
    if(content()->kind() != LayoutKind::Text)
      return false;
    
    if(expr.expr_length() != 1)
      return false;
    
    _content->load_from_object(expr[1], options);
    has_explicit_head(true);
  }
  else if(expr.item_equals(0, richmath_System_List) || expr.is_string()) {
    if(has_explicit_head())
      return false;
    
    _content->load_from_object(expr, options);
    has_explicit_head(false);
  }
  else { // StringBox, /\/
    if(has_explicit_head())
      return false;
    
    if(content()->kind() != LayoutKind::Math)
      return false;
      
    _content->load_from_object(expr, options);
    has_explicit_head(false);
  }
  
  finish_load_from_object(PMATH_CPP_MOVE(expr));
  return true;
}

void InlineSequenceBox::resize_default_baseline(Context &context) {
  bool old_math_spacing = context.math_spacing;
  context.math_spacing = true;
  OwnerBox::resize_default_baseline(context);
  context.math_spacing = old_math_spacing;
}

void InlineSequenceBox::paint(Context &context) {
  if(selectable()) {
    if(Color bg = get_style(InlineSectionEditingBackgroundColor, Color::None)) {
      float bg_alpha = get_style(InlineSectionEditingHighlightOpacity, 1.0f);
      if(0 < bg_alpha && bg_alpha <= 1.0) {
        bool contains_selection = false;
        context.for_each_selection_inside(this, [&](const VolatileSelection &sel) { contains_selection = true; });
        if(contains_selection) {
          float x, y;
          Color c = context.canvas().get_color();
          context.canvas().current_pos(&x, &y);
          context.canvas().pixrect(
            x,
            y - _extents.ascent - 1,
            x + _extents.width,
            y + _extents.descent + 1,
            false);
          
          context.canvas().set_color(bg, bg_alpha);
          context.canvas().fill();
          context.canvas().set_color(c);
          context.canvas().move_to(x, y);
        }
      }
    }
  }
  
  OwnerBox::paint(context);
}

void InlineSequenceBox::on_enter() {
  request_repaint_all();
  
  base::on_enter();
}

void InlineSequenceBox::on_exit() {
  request_repaint_all();
  
  base::on_exit();
}

Expr InlineSequenceBox::to_pmath_symbol() {
  if(has_explicit_head()) {
    switch(content()->kind()) {
      case LayoutKind::Math: return Symbol(richmath_System_BoxData);
      case LayoutKind::Text: return Symbol(richmath_System_TextData);
    }
  }
  else {
    switch(content()->kind()) {
      case LayoutKind::Math: return Symbol(richmath_System_List);
      case LayoutKind::Text: return Symbol(richmath_System_String);
    }
  }
  
  return base::to_pmath_symbol();
}

Expr InlineSequenceBox::to_pmath_impl(BoxOutputFlags flags) {
  if(has_explicit_head())
    return Call(to_pmath_symbol(), base::to_pmath_impl(flags));
  else
    return base::to_pmath_impl(flags);
}

//} ... class InlineSequenceBox

void OwnerBox::Impl::adjust_baseline(float em) {
  float y = SimpleBoxBaselinePositioning{self._extents.ascent, self._extents.descent, self.cy}.calculate_baseline(em, self.get_own_style(BaselinePosition));
  self.cy += y;
  self._extents.ascent-= y;
  self._extents.descent+= y;
}
