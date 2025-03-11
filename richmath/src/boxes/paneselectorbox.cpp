#include <boxes/paneselectorbox.h>
#include <boxes/mathsequence.h>

#include <graphics/context.h>

#include <util/alignment.h>

#include <algorithm>


#ifdef max
#  undef max
#endif

using namespace richmath;
using namespace pmath;

extern pmath_symbol_t richmath_System_HoldPattern;
extern pmath_symbol_t richmath_System_List;
extern pmath_symbol_t richmath_System_PaneSelectorBox;

namespace richmath { namespace strings {
  extern String PaneSelector;
}}

//{ class PaneSelectorBox ...

PaneSelectorBox::PaneSelectorBox() 
  : Box(),
    _dynamic(this, Expr()),
    _current_offset(0.0f, 0.0f),
    _current_selection(-1)
{
}

PaneSelectorBox::~PaneSelectorBox() {
  for(auto box : _panes)
    box->safe_destroy();
}

bool PaneSelectorBox::try_load_from_object(Expr expr, BoxInputFlags opts) {
  /* PaneSelectorBox({v1 -> pane1, v2 -> pane2, ...}, v)
     PaneSelectorBox({v1 -> pane1, v2 -> pane2, ...}, v, def)
   */
  if(!expr.item_equals(0, richmath_System_PaneSelectorBox))
    return false;
  
  if(expr.expr_length() < 2)
    return false;
  
  Expr pane_rules = expr[1];
  if(!pane_rules.item_equals(0, richmath_System_List))
    return false;
  
  if(pane_rules.expr_length() >= INT_MAX)
    return false;
  
  for(auto rule : pane_rules.items())
    if(!rule.is_rule())
      return false;
  
  Expr default_pane;
  Expr options;
  if(expr.expr_length() >= 3) {
    default_pane = expr[3];
    if(default_pane.is_rule()) {
      default_pane = Expr();
      options = Expr(pmath_options_extract_ex(expr.get(), 2, PMATH_OPTIONS_EXTRACT_UNKNOWN_WARNONLY));
    }
    else {
      options = Expr(pmath_options_extract_ex(expr.get(), 3, PMATH_OPTIONS_EXTRACT_UNKNOWN_WARNONLY));
    }
    
    if(options.is_null())
      return false;
  }
  
  // now success is guaranteed
  
  reset_style();
  style.add_pmath(options);
  
  int total_num_panes = (int)pane_rules.expr_length();
  if(!default_pane.is_null())
    ++total_num_panes;
  
  int old_num_panes = _panes.length();
  if(old_num_panes > total_num_panes) {
    for(int i = total_num_panes + 1; i < old_num_panes; ++i)
      _panes[i]->safe_destroy();
    _panes.length(total_num_panes);
  }
  else {
    _panes.length(total_num_panes, nullptr);
    for(int i = old_num_panes; i < total_num_panes; ++i) {
      auto *seq = new MathSequence();
      adopt(seq, i);
      _panes[i] = seq;
    }
  }
  
  _cases.length((int)pane_rules.expr_length());
  for(int i = _cases.length(); i > 0; --i) {
    Expr rule = pane_rules[i];
    Expr val = rule[1];
    if(val.expr_length() == 1 && val.item_equals(0, richmath_System_HoldPattern))
      val = val[1];
    
    if(i - 1 == _current_selection && _cases[i - 1] != val)
      must_update(true);
    
    _cases[i - 1] = PMATH_CPP_MOVE(val);
    _panes[i - 1]->load_from_object(rule[2], opts);
  }
  
  if(total_num_panes > _cases.length())
    _panes[total_num_panes - 1]->load_from_object(default_pane, opts);
  
  Expr selector = expr[2];
  if(_dynamic.expr() != selector || has(opts, BoxInputFlags::ForceResetDynamic)){
    must_update(true);
    _dynamic     = selector;
  }
  
  if(_current_selection < 0 || _current_selection >= _panes.length()) {
    _current_selection = _cases.length();
    must_update(true);
  }
  
  finish_load_from_object(PMATH_CPP_MOVE(expr));
  return true;
}

Box *PaneSelectorBox::item(int i) {
  return _panes[i]; 
}

void PaneSelectorBox::resize(Context &context) {
  update_simple_dynamic_styles_on_resize(context);
  // TODO: try to evaluate _dynamic already now ... ?
  
  float rel_scale = context.width;
  
  ContextState cc(context);
  cc.begin(style);
  
  float em = context.canvas().get_font_size();
  
  Length w = get_own_style(ImageSizeHorizontal, SymbolicSize::Automatic);
  Length h = get_own_style(ImageSizeVertical,   SymbolicSize::Automatic);
  
  float resolved_width = w.resolve(em, LengthConversionFactors::GraphicsSize, rel_scale);
  if(w != SymbolicSize::All && w != SymbolicSize::Automatic) {
    context.width = resolved_width;
  }
  
  if(w == SymbolicSize::All || h == SymbolicSize::All) {
    for(auto box : _panes)
      box->resize(context);
  }
  else {
    if(0 <= _current_selection && _current_selection < _panes.length())
      _panes[_current_selection]->resize(context);
  }
  
  cc.end();
  
  BoxSize current_pane_extents;
  if(0 <= _current_selection && _current_selection < _panes.length())
    current_pane_extents = _panes[_current_selection]->extents();
  else
    current_pane_extents = BoxSize();
  
  if(w == SymbolicSize::All) {
    _extents = BoxSize();
    for(auto box : _panes)
      _extents.width = std::max(_extents.width, box->extents().width);
  }
  else if(w == SymbolicSize::Automatic)
    _extents.width = current_pane_extents.width;
  else
    _extents.width = resolved_width;
  
  float total_height = 0.0f;
  if(h == SymbolicSize::All) {
    for(auto box : _panes)
      total_height = std::max(total_height, box->extents().height());
  }
  else if(h == SymbolicSize::Automatic)
    total_height = current_pane_extents.height();
  else
    total_height = h.resolve(em, LengthConversionFactors::GraphicsSize, rel_scale);
  
  SimpleAlignment alignment = SimpleAlignment::from_pmath(get_own_style(Alignment), SimpleAlignment::TopLeft);
  
  _extents.ascent  = alignment.interpolate_bottom_to_top(total_height - current_pane_extents.descent,                current_pane_extents.ascent);
  _extents.descent = alignment.interpolate_bottom_to_top(               current_pane_extents.descent, total_height - current_pane_extents.ascent);
  
  _current_offset.x = alignment.interpolate_left_to_right(0.0f, _extents.width - current_pane_extents.width);
  _current_offset.y = 0; // TODO: incorporate BaselinePosition
}

void PaneSelectorBox::paint(Context &context) {
  context.canvas().rel_move_to(_current_offset);
  Point p0 = context.canvas().current_pos();
  
  update_dynamic_styles_on_paint(context);
  
  if(_current_selection >= 0 && _current_selection < _panes.length()) {
    ContextState cc(context);
    cc.begin(style);
    
    if(Color c = context.stylesheet->get_or_default(style, Background)) {
      if(context.canvas().show_only_text) 
        goto AFTER_PAINT;
      
      RectangleF rect = _extents.to_rectangle(p0);
      BoxRadius radii;
      
      if(Expr radii_expr = context.stylesheet->get_or_default(style, BorderRadius)) 
        radii = BoxRadius(PMATH_CPP_MOVE(radii_expr));
      
      rect.normalize();
      rect.pixel_align(context.canvas(), false, +1);
      
      radii.normalize(rect.width, rect.height);
      rect.add_round_rect_path(context.canvas(), radii, false);
      
      context.canvas().set_color(c);
      context.canvas().fill();
    }
    
    if(Color c = context.stylesheet->get_or_default(style, FontColor))
      context.canvas().set_color(c);
    
    context.canvas().move_to(p0);
    _panes[_current_selection]->paint(context);
    
  AFTER_PAINT:
    cc.end();
  }
  
  if(must_update()) {
    must_update(false);
    
    Expr result;
    if(_dynamic.get_value(&result)) 
      dynamic_finished(Expr(), result);
  }
}

void PaneSelectorBox::reset_style() {
  style.reset(strings::PaneSelector);
}

Box *PaneSelectorBox::remove(int *index) {
  return move_logical(LogicalDirection::Backward, false, index);
}

VolatileSelection PaneSelectorBox::dynamic_to_literal(int start, int end) {
  _dynamic = to_literal();
  return {this, start, end};
}

void PaneSelectorBox::dynamic_updated() {
  if(must_update())
    return;
    
  must_update(true);
  request_repaint_all();
}

void PaneSelectorBox::dynamic_finished(Expr info, Expr result) {
  int old_sel = _current_selection;
  for(_current_selection = 0; _current_selection < _cases.length(); ++_current_selection)
    if(_cases[_current_selection] == result)
      break;
    
  if(old_sel != _current_selection)
    invalidate();
}

Expr PaneSelectorBox::to_pmath_symbol() {
  return Symbol(richmath_System_PaneSelectorBox);
}

Expr PaneSelectorBox::to_pmath_impl(BoxOutputFlags flags) {
  Expr rules = MakeCall(Symbol(richmath_System_List), (size_t)_cases.length());
  for(int i = 0;i < _cases.length();++i) {
    Expr val = _cases[i];
    if(val.is_symbol() || val.is_expr())
      val = Call(Symbol(richmath_System_HoldPattern), val);
    
    rules.set(i + 1, Rule(val, _panes[i]->to_pmath(flags)));
  }
  
  Gather g;
  Gather::emit(PMATH_CPP_MOVE(rules));
  
  if(has(flags, BoxOutputFlags::Literal))
    Gather::emit(to_literal());
  else
    Gather::emit(_dynamic.expr());
  
  if(_panes.length() > _cases.length())
    Gather::emit(_panes[_panes.length() - 1]->to_pmath(flags));
  
  if(style) {
    bool with_inherited = true;
    
    String s;
    if(style.get(BaseStyleName, &s) && s == strings::PaneSelector)
      with_inherited = false;
    
    style.emit_to_pmath(with_inherited);
  }
  
  Expr expr = g.end();
  expr.set(0, Symbol(richmath_System_PaneSelectorBox));
  return expr;
}

Box *PaneSelectorBox::move_logical(
  LogicalDirection  direction,
  bool              jumping,
  int              *index
) {
  if(direction == LogicalDirection::Forward) {
    if(*index < _current_selection && !jumping && _current_selection >= 0 && _current_selection < _panes.length()) {
      *index = -1;
      return _panes[_current_selection]->move_logical(LogicalDirection::Forward, jumping, index);
    }
    
    if(auto par = parent()) {
      *index = _index;
      return par->move_logical(LogicalDirection::Forward, true, index);
    }
    
    *index = _panes.length();
    return this;
  }
  else {
    if(*index > _current_selection && !jumping && _current_selection >= 0 && _current_selection < _panes.length()) {
      auto pane = _panes[_current_selection];
      *index = pane->length() + 1;
      return pane->move_logical(LogicalDirection::Backward, jumping, index);
    }
    
    if(auto par = parent()) {
      *index = _index + 1;
      return par->move_logical(LogicalDirection::Backward, true, index);
    }
    
    *index = 0;
    return this;
  }
}

Box *PaneSelectorBox::move_vertical(
  LogicalDirection  direction,
  float            *index_rel_x,
  int              *index,
  bool              called_from_child
) {
  if(*index < 0) {
    if(_current_selection >= 0 && _current_selection < _cases.length()) {
      *index_rel_x -= _current_offset.x;
      return _panes[_current_selection]->move_vertical(direction, index_rel_x, index, false);
    }
  }
  else {
    *index_rel_x += _current_offset.x;
  }
  
  return base::move_vertical(direction, index_rel_x, index, called_from_child);
}

VolatileSelection PaneSelectorBox::mouse_selection(Point pos, bool *was_inside_start) {
  if(_current_selection >= 0 && _current_selection < _cases.length())
    return _panes[_current_selection]->mouse_selection(pos - _current_offset, was_inside_start);
  
  return base::mouse_selection(pos, was_inside_start);
}

void PaneSelectorBox::child_transformation(int index, cairo_matrix_t *matrix) {
  cairo_matrix_translate(matrix,
                         _current_offset.x,
                         _current_offset.y);
}

bool PaneSelectorBox::edit_selection(SelectionReference &selection, EditAction action) {
  if(_current_selection < 0 || _current_selection >= _cases.length())
    return false;
  
  Box *b = selection.get();
  if(b == this)
    return false;
  
  int i = selection.start;
  while(b) {
    i = b->index();
    b = b->parent();
    if(b == this)
      if(i != _current_selection)
        return false;
  }
  
  return base::edit_selection(selection, action);
}

Expr PaneSelectorBox::to_literal() {
  if(!_dynamic.is_dynamic())
    return _dynamic.expr();
  
  if(_current_selection >= 0 && _current_selection < _cases.length())
    return _cases[_current_selection];
  
  return _dynamic.get_value_now();
}
    
//} ... class PaneSelectorBox
