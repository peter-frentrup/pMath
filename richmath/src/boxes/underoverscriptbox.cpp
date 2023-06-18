#include <boxes/underoverscriptbox.h>

#include <cmath>

#include <boxes/mathsequence.h>
#include <graphics/context.h>

using namespace richmath;

extern pmath_symbol_t richmath_System_OverscriptBox;
extern pmath_symbol_t richmath_System_UnderoverscriptBox;
extern pmath_symbol_t richmath_System_UnderscriptBox;

namespace richmath {
  namespace strings {
    extern String Overscript;
    extern String Underoverscript;
    extern String Underscript;
  }
}

namespace richmath {
  class UnderoverscriptBox::Impl {
    public:
      explicit Impl(UnderoverscriptBox &self);
      
      bool base_style_name_is_default();
      String default_base_style_name();
      
    private:
      UnderoverscriptBox &self;
  };
}

static inline bool char_is_vertical_paren(uint16_t ch) {
  return ch == 0x23B4
         || ch == 0x23B5
         || (ch >= 0x23DC && ch <= 0x23E1);
}

//{ class UnderoverscriptBox ...

UnderoverscriptBox::UnderoverscriptBox()
  : Box(),
  _base(       new MathSequence),
  _underscript(new MathSequence),
  _overscript( new MathSequence)
{
  adopt(_base,        0);
  adopt(_underscript, 1);
  adopt(_overscript,  2);
}

UnderoverscriptBox::UnderoverscriptBox(
  MathSequence *base,
  MathSequence *under,
  MathSequence *over)
  : Box(),
  _base(       base),
  _underscript(under),
  _overscript( over)
{
  RICHMATH_ASSERT(_base && (_underscript || _overscript));
  adopt(_base, 0);
  
  int i = 1;
  if(_underscript)
    adopt(_underscript, i++);
  if(_overscript)
    adopt(_overscript, i);
}

UnderoverscriptBox::~UnderoverscriptBox() {
  delete_owned(_base);
  delete_owned(_underscript);
  delete_owned(_overscript);
}

bool UnderoverscriptBox::try_load_from_object(Expr expr, BoxInputFlags opts) {
  if(expr[0] == richmath_System_OverscriptBox) {
    Expr options = Expr(pmath_options_extract_ex(expr.get(), 2, PMATH_OPTIONS_EXTRACT_UNKNOWN_WARNONLY));
    if(options.is_null())
      return false;
      
    if(_underscript) {
      _underscript->safe_destroy();
      _underscript = nullptr;
    }
    
    if(!_overscript)
      _overscript = new MathSequence;
      
    adopt(_overscript, 1);
    
    _base->load_from_object(      expr[1], opts);
    _overscript->load_from_object(expr[2], opts);
    
    reset_style();
    style.add_pmath(options);
    
    finish_load_from_object(PMATH_CPP_MOVE(expr));
    return true;
  }
  
  if(expr[0] == richmath_System_UnderscriptBox) {
    Expr options = Expr(pmath_options_extract_ex(expr.get(), 2, PMATH_OPTIONS_EXTRACT_UNKNOWN_WARNONLY));
    if(options.is_null())
      return false;
      
    if(_overscript) {
      _overscript->safe_destroy();
      _overscript = nullptr;
    }
    
    if(!_underscript)
      _underscript = new MathSequence;
      
    adopt(_underscript, 1);
    
    _base->load_from_object(       expr[1], opts);
    _underscript->load_from_object(expr[2], opts);
    
    reset_style();
    style.add_pmath(options);
    
    finish_load_from_object(PMATH_CPP_MOVE(expr));
    return true;
  }
  
  if(expr[0] == richmath_System_UnderoverscriptBox) {
    Expr options = Expr(pmath_options_extract_ex(expr.get(), 3, PMATH_OPTIONS_EXTRACT_UNKNOWN_WARNONLY));
    if(options.is_null())
      return false;
      
    if(!_underscript)
      _underscript = new MathSequence;
      
    if(!_overscript)
      _overscript = new MathSequence;
      
    adopt(_underscript, 1);
    adopt(_overscript, 2);
    
    _base->load_from_object(       expr[1], opts);
    _underscript->load_from_object(expr[2], opts);
    _overscript->load_from_object( expr[3], opts);
    
    reset_style();
    style.add_pmath(options);
    
    finish_load_from_object(PMATH_CPP_MOVE(expr));
    return true;
  }
  
  return false;
}

Box *UnderoverscriptBox::item(int i) {
  if(i == 0)
    return _base;
    
  if(i == 1 && _underscript)
    return _underscript;
    
  return _overscript;
}

int UnderoverscriptBox::count() {
  return 1 + (_underscript ? 1 : 0) + (_overscript ? 1 : 0);
}

int UnderoverscriptBox::child_script_level(int index, const int *opt_ambient_script_level) {
  if(index <= 0)
    return Box::child_script_level(index, opt_ambient_script_level);
  
  // For underscript or overscript:
  int ambient_script_level = Box::child_script_level(-1, opt_ambient_script_level);
  
  if(ambient_script_level < 1)
    ambient_script_level = 1;
  
  return ambient_script_level + 1;
}

void UnderoverscriptBox::resize(Context &context) {
  float old_w = context.width;
  context.width = HUGE_VAL;
  
  _base->resize(context);
  
  int old_script_level = context.script_level;
  context.script_level = child_script_level(1, &context.script_level);
  
//  float flac_min = context.math_shaper->flattened_accent_base_height(context);
  
  float old_fs = context.canvas().get_font_size();
  float em = context.get_script_size(old_fs);
  context.canvas().set_font_size(em);
  
  float w = 0;
  float min_stretch_width = _base->extents().width;
  min_stretch_width -= _base->first_glyph_width() / 3;
  min_stretch_width -= _base->last_glyph_width() / 3;
  
  underscript_is_stretched(false);
  overscript_is_stretched(false);
  
  if(_underscript) {
    _underscript->resize(context);
    
    underscript_is_stretched(
      _underscript->stretch_horizontal(context, min_stretch_width));
                                  
    w = _underscript->extents().width;
  }
  
  if(_overscript) {
//    if(_base->extents().ascent > flac_min) {
//      int old_flac_feature_value = context.fontfeatures.feature_value(FontFeatureSet::TAG_flac);
//      context.fontfeatures.set_feature(FontFeatureSet::TAG_flac, 1);
//      
//      _overscript->resize(context);
//      
//      context.fontfeatures.set_feature(FontFeatureSet::TAG_flac, old_flac_feature_value);
//    }
//    else {
      _overscript->resize(context);
//    }
    
    overscript_is_stretched(
      _overscript->stretch_horizontal(context, min_stretch_width));
                                 
    if(w < _overscript->extents().width)
      w = _overscript->extents().width;
  }
  
  context.canvas().set_font_size(old_fs);
  context.width = old_w;
  
  if( !underscript_is_stretched() &&
      !overscript_is_stretched() &&
      _base->length() == 1)
  {
    if(auto par = parent()) {
      if(par->length() == 1) {
        if(auto uo = dynamic_cast<UnderoverscriptBox*>(par->parent())) {
          for(int i = 0; i < uo->count(); ++i)
            if(i != par->index()) {
              float wi = uo->item(i)->extents().width;
              if(w < wi)
                w = wi;
            }
        }
      }
    }
    
    _base->stretch_horizontal(context, w + 0.6f * em);
  }
  
  context.script_level = old_script_level;
  after_items_resize(context);
}

void UnderoverscriptBox::after_items_resize(Context &context) {
  context.math_shaper->accent_positions(
    context,
    _base,
    _underscript,
    _overscript,
    &_base_offset_x,
    &_underscript_offset,
    &_overscript_offset,
    underscript_is_stretched(),
    overscript_is_stretched(),
    (AutoBoolValues)get_own_style(LimitsPositioning, AutoBoolAutomatic));
    
  _extents = _base->extents();
  if(_base_offset_x > 0)
    _extents.width += _base_offset_x;
    
  if(_underscript) {
    if(_extents.descent < _underscript_offset.y + _underscript->extents().descent)
      _extents.descent = _underscript_offset.y + _underscript->extents().descent;
      
    if(_extents.width < _underscript_offset.x + _underscript->extents().width)
      _extents.width = _underscript_offset.x + _underscript->extents().width;
  }
  
  if(_overscript) {
    if(_extents.ascent < -_overscript_offset.y + _overscript->extents().ascent)
      _extents.ascent = -_overscript_offset.y + _overscript->extents().ascent;
      
    if(_extents.width < _overscript_offset.x + _overscript->extents().width)
      _extents.width = _overscript_offset.x + _overscript->extents().width;
  }
}

void UnderoverscriptBox::colorize_scope(SyntaxState &state) {
  _base->colorize_scope(state);
  
  if(_underscript && !underscript_is_stretched())
    _underscript->colorize_scope(state);
    
  if(_overscript && !overscript_is_stretched())
    _overscript->colorize_scope(state);
}

void UnderoverscriptBox::paint(Context &context) {
  update_dynamic_styles(context);
    
  Point pos = context.canvas().current_pos();
  
  context.canvas().move_to(pos.x + _base_offset_x, pos.y);
  _base->paint(context);
  
  float old_fs = context.canvas().get_font_size();
  
  if(_underscript) {
    context.canvas().move_to(pos + _underscript_offset);
    
    context.canvas().set_font_size(_underscript->font_size());
    _underscript->paint(context);
  }
  
  if(_overscript) {
    context.canvas().move_to(pos + _overscript_offset);
    
    context.canvas().set_font_size(_overscript->font_size());
    _overscript->paint(context);
  }
  
  context.canvas().set_font_size(old_fs);
}

Box *UnderoverscriptBox::remove(int *index) {
  bool use_default_base_style_name = Impl(*this).base_style_name_is_default();
  
  if(*index == 0) {
    if(_base->length() == 0) {
      if(auto seq = dynamic_cast<AbstractSequence*>(parent())) {
        if(_underscript && !_overscript) {
          seq->insert(_index + 1, _underscript, 0, _underscript->length());
          *index = _index;
          return seq->remove(index);
        }
        if(!_underscript && _overscript) {
          seq->insert(_index + 1, _overscript, 0, _overscript->length());
          *index = _index;
          return seq->remove(index);
        }
      }
    }
    
    if(_base->length() == 0)
      _base->insert(0, PMATH_CHAR_PLACEHOLDER);
      
    return move_logical(LogicalDirection::Backward, false, index);
  }
  
  if(_underscript && _overscript) {
    if(*index == 1) {
      if(_underscript->length() == 0) {
        _underscript->safe_destroy();
        _underscript = nullptr;
        adopt(_overscript, 1);
        invalidate();
        
        if(use_default_base_style_name)
          style.set(BaseStyleName, Impl(*this).default_base_style_name());
      }
      
      return move_logical(LogicalDirection::Backward, false, index);
    }
    
    if(_overscript->length() == 0) {
      _overscript->safe_destroy();
      _overscript = nullptr;
      invalidate();
      
      if(use_default_base_style_name)
        style.set(BaseStyleName, Impl(*this).default_base_style_name());
    }
    return move_logical(LogicalDirection::Backward, false, index);
  }
  
  auto seq = dynamic_cast<AbstractSequence*>(parent());
  if( seq && 
      ((_underscript && _underscript->length() == 0) || 
       (_overscript  && _overscript->length()  == 0))) 
  {
    *index = _index + _base->length();
    seq->insert(_index + 1, _base, 0, _base->length());
    int i = _index;
    seq->remove(&i);
    return seq;
  }
  
  return move_logical(LogicalDirection::Backward, false, index);
}

void UnderoverscriptBox::complete() {
  bool use_default_base_style_name = Impl(*this).base_style_name_is_default();
  
  if(!_underscript) {
    _underscript = new MathSequence;
    adopt(_underscript, 1);
    adopt(_overscript, 2);
  }
  
  if(!_overscript) {
    _overscript = new MathSequence;
    adopt(_overscript, 2);
  }
  
  if(use_default_base_style_name)
    style.set(BaseStyleName, Impl(*this).default_base_style_name());
}

Expr UnderoverscriptBox::to_pmath_symbol() {
  if(_underscript) {
    if(_overscript)
      return Symbol(richmath_System_UnderoverscriptBox);
    return Symbol(richmath_System_UnderscriptBox);
  }
  
  return Symbol(richmath_System_OverscriptBox);
}

Expr UnderoverscriptBox::to_pmath_impl(BoxOutputFlags flags) {
  Gather g;
  
  Gather::emit(_base->to_pmath(flags));
  
  if(_underscript) Gather::emit(_underscript->to_pmath(flags));
  if(_overscript)  Gather::emit(_overscript->to_pmath(flags));
  
  style.emit_to_pmath(!Impl(*this).base_style_name_is_default());
  
  Expr result = g.end();
  result.set(0, to_pmath_symbol());
  return result;
}

DefaultStyleOptionOffsets UnderoverscriptBox::get_default_styles_offset() {
  if(_underscript && !_overscript) return DefaultStyleOptionOffsets::UnderoverscriptBox;
  if(_overscript && !_underscript) return DefaultStyleOptionOffsets::OverscriptBox;
  return DefaultStyleOptionOffsets::UnderoverscriptBox;
}

void UnderoverscriptBox::reset_style() {
  style.reset(Impl(*this).default_base_style_name());
}

Box *UnderoverscriptBox::move_vertical(
  LogicalDirection  direction,
  float            *index_rel_x,
  int              *index,
  bool              called_from_child
) {
  MathSequence *dst = nullptr;
  
  if(*index < 0) {
    if(direction == LogicalDirection::Forward) {
      if(_overscript) {
        dst = _overscript;
        *index_rel_x -= _overscript_offset.x;
      }
      else {
        dst = _base;
        *index_rel_x -= _base_offset_x;
      }
    }
    else if(_underscript) {
      dst = _underscript;
      *index_rel_x -= _underscript_offset.x;
    }
    else {
      dst = _base;
      *index_rel_x -= _base_offset_x;
    }
  }
  else if(*index == 0) { // comming from base
    *index_rel_x += _base_offset_x;
    
    if(direction == LogicalDirection::Backward) {
      dst = _overscript;
      *index_rel_x -= _overscript_offset.x;
    }
    else {
      dst = _underscript;
      *index_rel_x -= _underscript_offset.x;
    }
  }
  else if(*index == 1 && _underscript) { // comming from underscript
    *index_rel_x += _underscript_offset.x;
    
    if(direction == LogicalDirection::Backward) {
      dst = _base;
      *index_rel_x -= _base_offset_x;
    }
  }
  else { // comming from overscript
    *index_rel_x += _overscript_offset.x;
    
    if(direction == LogicalDirection::Forward) {
      dst = _base;
      *index_rel_x -= _base_offset_x;
    }
  }
  
  if(!dst) {
    if(auto par = parent()) {
      *index = _index;
      return par->move_vertical(direction, index_rel_x, index, true);
    }
    
    return this;
  }
  
  *index = -1;
  return dst->move_vertical(direction, index_rel_x, index, false);
}

VolatileSelection UnderoverscriptBox::mouse_selection(Point pos, bool *was_inside_start) {
  if(_underscript) {
    if(_underscript_offset.y - _underscript->extents().ascent > _base->extents().descent) {
      if(pos.y > _base->extents().descent) {
        return _underscript->mouse_selection(pos - _underscript_offset, was_inside_start);
      }
    }
    else if(pos.x >= _underscript_offset.x) {
      // TODO: What is this check for? Shouldn't it be pos.y>= 0.5 * (...) ?
      if(!_overscript || pos.y >= _underscript_offset.y - _underscript->extents().ascent + _overscript_offset.y + _overscript->extents().descent) {
        return _underscript->mouse_selection(pos - _underscript_offset, was_inside_start);
      }
    }
  }
  
  if(_overscript) {
    if(-_overscript_offset.y - _overscript->extents().descent > _base->extents().ascent) {
      if(pos.y < -_base->extents().ascent) 
        return _overscript->mouse_selection(pos - _overscript_offset, was_inside_start);
    }
    else if(pos.x >= _overscript_offset.x) 
      return _overscript->mouse_selection(pos - _overscript_offset, was_inside_start);
  }
  
  return _base->mouse_selection(pos - Vector2F(_base_offset_x, 0), was_inside_start);
}

void UnderoverscriptBox::child_transformation(
  int             index,
  cairo_matrix_t *matrix
) {
  if(index == 0) {
    cairo_matrix_translate(
      matrix,
      _base_offset_x,
      0);
  }
  else if(index == 1 && _underscript) {
    cairo_matrix_translate(
      matrix,
      _underscript_offset.x,
      _underscript_offset.y);
  }
  else {
    cairo_matrix_translate(
      matrix,
      _overscript_offset.x,
      _overscript_offset.y);
  }
}

//} ... class UnderoverscriptBox

//{ class UnderoverscriptBox::Impl ...

inline UnderoverscriptBox::Impl::Impl(UnderoverscriptBox &self)
: self{self}
{
}

bool UnderoverscriptBox::Impl::base_style_name_is_default() {
  String s;
  if(!self.style.get(BaseStyleName, &s)) 
    return false;
  
  return s == default_base_style_name();
}

String UnderoverscriptBox::Impl::default_base_style_name() {
  if(self._underscript && !self._overscript)  return strings::Underscript;
  if(self._overscript  && !self._underscript) return strings::Overscript;
  
  return strings::Underoverscript;
}

//} ... class UnderoverscriptBox::Impl
