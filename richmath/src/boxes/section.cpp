#include <boxes/section.h>

#include <cmath>

#include <boxes/sectionlist.h>
#include <boxes/mathsequence.h>
#include <boxes/textsequence.h>
#include <eval/application.h>
#include <graphics/context.h>
#include <graphics/rectangle.h>


using namespace richmath;

//{ class Section ...

Section::Section(SharedPtr<Style> _style)
  : Box(),
    y_offset(0),
    top_margin(3),
    bottom_margin(3),
    unfilled_width(0),
    must_resize(true),
    visible(true),
    evaluating(false),
    dialog_start(false)
{
  style = _style.release();
}

Section::~Section() {
}

Section *Section::create_from_object(const Expr expr) {
  if(expr[0] == PMATH_SYMBOL_SECTION) {
    Expr content = expr[1];
    
    Section *section = 0;
    
    if(content.expr_length() == 1 && content[0] == PMATH_SYMBOL_BOXDATA)
      section = Box::try_create<MathSection>(expr, BoxOptionDefault);
    else
      section = Box::try_create<TextSection>(expr, BoxOptionDefault);
      
    if(section)
      return section;
  }
  
  return new ErrorSection(expr);
}

float Section::label_width() {
  if(label_glyphs.length() == 0)
    return 0;
    
  return label_glyphs[label_glyphs.length() - 1].right;
}

void Section::resize_label(Context *context) {
  String lbl = get_style(SectionLabel);
  if(lbl.is_null() || label_string == lbl)
    return;
    
  label_string = lbl;
  
  SharedPtr<TextShaper> shaper = TextShaper::find("Arial", NoStyle);
  
  float fs = context->canvas->get_font_size();
  context->canvas->set_font_size(8/* * 4/3. */);
  
  label_glyphs.length(lbl.length());
  label_glyphs.zeromem();
  shaper->decode_token(
    context,
    label_glyphs.length(),
    lbl.buffer(),
    label_glyphs.items());
    
  float x = 0;
  for(int i = 0; i < label_glyphs.length(); ++i)
    label_glyphs[i].right = x += label_glyphs[i].right;
    
  context->canvas->set_font_size(fs);
}

void Section::paint_label(Context *context) {
  if(label_glyphs.length() == 0)
    return;
    
  String lbl = get_style(SectionLabel);
  if(lbl.length() != label_glyphs.length())
    return;
    
  SharedPtr<TextShaper> shaper = TextShaper::find("Arial", NoStyle);
  
  float x, y;
  context->canvas->current_pos(&x, &y);
  
  float fs = context->canvas->get_font_size();
  context->canvas->set_font_size(8/* * 4/3. */);
  
  int col = context->canvas->get_color();
  context->canvas->set_color(0x454E99);
  
  x -= label_glyphs[label_glyphs.length() - 1].right;
  float xx = x;
  const uint16_t *buf = lbl.buffer();
  for(int i = 0; i < label_glyphs.length(); ++i) {
    shaper->show_glyph(
      context,
      xx, y,
      buf[i],
      label_glyphs[i]);
    xx = x + label_glyphs[i].right;
  }
  
  context->canvas->set_font_size(fs);
  context->canvas->set_color(col);
}

Box *Section::move_vertical(
  LogicalDirection  direction,
  float            *index_rel_x,
  int              *index,
  bool              called_from_child
) {
  if(_parent) {
    if(index < 0) { // called from parent
      if(direction == Forward)
        *index = _index + 1;
      else
        *index = _index;
      return _parent;
    }
    *index = _index;
    return _parent->move_vertical(direction, index_rel_x, index, true);
  }
  
  return this;
}

bool Section::selectable(int i) {
  if(i < 0)
    return false;
  return Box::selectable(i);
}

Box *Section::get_highlight_child(Box *src, int *start, int *end) {
  if(src == this)
    return this;
    
  if(!visible)
    return 0;
    
  return Box::get_highlight_child(src, start, end);
}

bool Section::request_repaint(float x, float y, float w, float h) {
  if(visible)
    return Box::request_repaint(x, y, w, h);
  return false;
}

void Section::invalidate() {
  must_resize = true;
  Box::invalidate();
}

bool Section::edit_selection(Context *context) {
  if(!Box::edit_selection(context))
    return false;
    
  if( get_style(SectionLabel).length() > 0 &&
      get_style(SectionLabelAutoDelete))
  {
    if(style) {
      String s;
      
      if(style->get(SectionLabel, &s)) {
        style->remove(SectionLabel);
        if(get_style(SectionLabel).length() > 0)
          style->set(SectionLabel, String());
      }
      else
        style->set(SectionLabel, String());
    }
    else {
      style = new Style;
      style->set(SectionLabel, String());
    }
    
    invalidate();
  }
  
  if(style && get_style(SectionEditDuplicate)) {
    SectionList *slist = dynamic_cast<SectionList *>(parent());
    if(slist) {
      slist->set_open_close_group(index(), true);
      
      if(get_style(SectionEditDuplicateMakesCopy)) {
        slist->insert(index(), Section::create_from_object(to_pmath(BoxOptionDefault)));
      }
    }
    
    Expr style_expr = get_style(DefaultDuplicateSectionStyle, Expr());
    if(style_expr.is_null() && parent())
      style_expr = parent()->get_style(DefaultDuplicateSectionStyle);
      
    style->add_pmath(style_expr);
    invalidate();
  }
  
  if(get_style(SectionGenerated)) {
    if(style) {
      int i;
      
      if(style->get(SectionGenerated, &i)) {
        style->remove(SectionGenerated);
        if(get_style(SectionGenerated))
          style->set(SectionGenerated, false);
      }
      else
        style->set(SectionGenerated, false);
    }
    else {
      style = new Style;
      style->set(SectionGenerated, false);
    }
    
    invalidate();
  }
  
  return true;
}

//} ... class Section

//{ class ErrorSection ...

ErrorSection::ErrorSection(const Expr object)
  : Section(0),
    _object(object)
{
}

bool ErrorSection::try_load_from_object(Expr expr, int opts) {
  return false;
}

void ErrorSection::resize(Context *context) {
  must_resize = false;
  
  top_margin    = get_style(SectionMarginTop);
  bottom_margin = get_style(SectionMarginBottom);
  
  float em = get_style(FontSize);
  _extents.ascent  = 0;
  _extents.descent =     em + top_margin + bottom_margin;
  _extents.width   = 2 * em + get_style(SectionMarginLeft) + get_style(SectionMarginRight);
  
  unfilled_width = _extents.width;
}

void ErrorSection::paint(Context *context) {
  if(style)
    style->update_dynamic(this);
    
  float x, y;
  context->canvas->current_pos(&x, &y);
  
  context->draw_error_rect(
    x +                    get_style(SectionMarginLeft),
    y +                    get_style(SectionMarginTop),
    x + _extents.width   - get_style(SectionMarginRight),
    y + _extents.descent - get_style(SectionMarginBottom));
}

Box *ErrorSection::mouse_selection(
  float  x,
  float  y,
  int   *start,
  int   *end,
  bool  *was_inside_start
) {
  *was_inside_start = true;
  *start = *end = 0;
  return this;
}

//} ... class ErrorSection

//{ class AbstractSequenceSection ...

AbstractSequenceSection::AbstractSequenceSection(AbstractSequence *content, SharedPtr<Style> _style)
  : Section(_style),
    _content(content)
{
  adopt(_content, 0);
}

AbstractSequenceSection::~AbstractSequenceSection() {
  delete _content;
}

Box *AbstractSequenceSection::item(int i) {
  return _content;
}

void AbstractSequenceSection::resize(Context *context) {
  must_resize = false;
  
  float old_scww = context->section_content_window_width;
  ContextState cc(context);
  cc.begin(style);
  
  // take document option if not set for this Section alone:
  context->show_auto_styles = get_style(ShowAutoStyles);
  
  context->script_indent = 0;
  
  top_margin    = get_style(SectionMarginTop);
  bottom_margin = get_style(SectionMarginBottom);
  
  resize_label(context);
  
  cx = get_style(SectionMarginLeft);
  cy = top_margin;
  
  float horz_border = get_style(SectionMarginRight);
  
  float l = get_style(SectionFrameLeft);
  float r = get_style(SectionFrameRight);
  float t = get_style(SectionFrameTop);
  float b = get_style(SectionFrameBottom);
  
  bool have_frame = get_style(Background) >= 0 || l != 0 || r != 0 || t != 0 || b != 0;
  if(have_frame) {
    cx += l;
    cx += get_style(SectionFrameMarginLeft);
    
    cy += t;
    cy += get_style(SectionFrameMarginTop);
    
    horz_border += r;
    horz_border += get_style(SectionFrameMarginRight);
  }
  
  horz_border += cx;
  context->width -= horz_border;
  context->section_content_window_width -= horz_border;
  
  _content->resize(context);
  
  unfilled_width = context->sequence_unfilled_width + horz_border;
  
  if(context->show_auto_styles) {
    SyntaxState syntax;
    _content->colorize_scope(&syntax);
  }
  
  if(_content->var_extents().ascent < 0.75 * _content->get_em())
    _content->var_extents().ascent = 0.75 * _content->get_em();
  if(_content->var_extents().descent < 0.25 * _content->get_em())
    _content->var_extents().descent = 0.25 * _content->get_em();
    
  cy += _content->extents().ascent;
  
  _extents.ascent = 0;
  _extents.descent = cy + _content->extents().descent + bottom_margin;
  
  if( context->width < HUGE_VAL &&
      _content->var_extents().width < context->width)
  {
    _content->var_extents().width = context->width;
  }
  
  context->section_content_window_width = old_scww;
  cc.end();
  
  _extents.width = cx + _content->extents().width;
  
  if(have_frame) {
    _extents.descent += b;
    _extents.descent += get_style(SectionFrameMarginBottom);
    
    _extents.width += r + get_style(SectionFrameMarginRight);
  }
}

void AbstractSequenceSection::paint(Context *context) {
  if(style)
    style->update_dynamic(this);
    
  float x, y;
  context->canvas->current_pos(&x, &y);
  
  ContextState cc(context);
  cc.begin(style);
  
  float left_margin = get_style(SectionMarginLeft);
  int   background  = get_style(Background);
  
  float l = get_style(SectionFrameLeft);
  float r = get_style(SectionFrameRight);
  float t = get_style(SectionFrameTop);
  float b = get_style(SectionFrameBottom);
  
  if(background >= 0 || l != 0 || r != 0 || t != 0 || b != 0) {
    /* Cairo 1.12.2 bug:
      With BorderRadius->0, and one of l,r,t,b != 0, e.g. SectionFrame->{0,0,1,0}
      The whole section is filled, not only the frame. Another BorderRadius
      fixes that
     */
    
    BoxRadius radii;
    Expr expr;
    if(context->stylesheet->get(style, BorderRadius, &expr))
      radii = BoxRadius(expr);
      
    Rectangle rect(Point(x + left_margin,
                         y + top_margin),
                   Point(x + _extents.width,
                         y + _extents.descent - bottom_margin));
    rect.pixel_align(*context->canvas, false);
    radii.normalize(rect.width, rect.height);
    
    // outer rounded rectangle
    rect.add_round_rect_path(*context->canvas, radii, false);
    
    if(background >= 0 && !context->canvas->show_only_text) {
      context->canvas->set_color(background);
      context->canvas->fill_preserve();
    }
    
    Point delta_tl(l, t);
    delta_tl.pixel_align_distance(*context->canvas);
    rect.x += delta_tl.x; rect.width -= delta_tl.x;
    rect.y += delta_tl.y; rect.height -= delta_tl.y;
    
    Point delta_br(r, b);
    delta_br.pixel_align_distance(*context->canvas);
    rect.width -= delta_br.x;
    rect.height -= delta_br.y;
    
    radii.top_left_x    -= delta_tl.x;
    radii.top_left_y    -= delta_tl.y;
    radii.top_right_x   -= delta_br.x;
    radii.top_right_y   -= delta_tl.y;
    radii.bottom_right_x -= delta_br.x;
    radii.bottom_right_y -= delta_br.y;
    radii.bottom_left_x -= delta_tl.x;
    radii.bottom_left_y -= delta_br.y;
    
    rect.normalize_to_zero();
    radii.normalize(rect.width, rect.height);
    
    // inner rounded rectangle
    rect.add_round_rect_path(*context->canvas, radii, true);
    
    context->canvas->set_color(get_style(SectionFrameColor));
    context->canvas->fill();
  }
  
  context->canvas->move_to(
    x + left_margin - 3,
    y + _content->extents().ascent + top_margin);
    
  paint_label(context);
  
  float xx = x + cx;
  float yy = y + cy;
  context->canvas->align_point(&xx, &yy, false);
  context->canvas->move_to(xx, yy);
  
  context->canvas->set_color(get_style(FontColor));
  
  Expr expr;
  context->stylesheet->get(style, TextShadow, &expr);
  context->draw_with_text_shadows(_content, expr);
  
  cc.end();
}

Box *AbstractSequenceSection::remove(int *index) {
  *index = 0;
  return _content;
}

Expr AbstractSequenceSection::to_pmath(int flags) {
  Gather g;
  
  Expr cont = _content->to_pmath(flags/* & ~BoxFlagParseable*/);
  if(dynamic_cast<MathSequence *>(_content))
    cont = Call(Symbol(PMATH_SYMBOL_BOXDATA), cont);
    
  Gather::emit(cont);
  
  String s;
  
  if(style && style->get(BaseStyleName, &s))
    Gather::emit(s);
  else
    Gather::emit(String(""));
    
  if(style)
    style->emit_to_pmath();
    
  Expr e = g.end();
  e.set(0, Symbol(PMATH_SYMBOL_SECTION));
  return e;
}

Box *AbstractSequenceSection::move_vertical(
  LogicalDirection  direction,
  float            *index_rel_x,
  int              *index,
  bool              called_from_child
) {
  if(*index < 0) {
    *index_rel_x -= cx;
    return _content->move_vertical(direction, index_rel_x, index, false);
  }
  
  if(_parent) {
    *index_rel_x += cx;
//    if(direction == Forward)
//      *index = _index + 1;
//    else
//      *index = _index;
//    return _parent;
    *index = _index;
    return _parent->move_vertical(direction, index_rel_x, index, true);
  }
  
  return this;
}

Box *AbstractSequenceSection::mouse_selection(
  float  x,
  float  y,
  int   *start,
  int   *end,
  bool  *was_inside_start
) {
  return _content->mouse_selection(x - cx, y - cy, start, end, was_inside_start);
}

void AbstractSequenceSection::child_transformation(
  int             index,
  cairo_matrix_t *matrix
) {
  cairo_matrix_translate(matrix, cx, cy);
}

//} ... class AbstractSequenceSection

//{ class MathSection ...

MathSection::MathSection()
  : AbstractSequenceSection(new MathSequence, new Style)
{
}

MathSection::MathSection(SharedPtr<Style> _style)
  : AbstractSequenceSection(new MathSequence, _style)
{
  if(!style)
    style = new Style;
}

bool MathSection::try_load_from_object(Expr expr, int opts) {
  if(expr[0] != PMATH_SYMBOL_SECTION)
    return false;
    
  Expr content = expr[1];
  if(content[0] != PMATH_SYMBOL_BOXDATA)
    return false;
    
  content = content[1];
  
  Expr options(pmath_options_extract(expr.get(), 2));
  if(options.is_null())
    return false;
    
  Expr stylename = expr[2];
  if(!stylename.is_string())
    stylename = String("Input");
    
  reset_style();
  style->add_pmath(options);
  style->set(BaseStyleName, stylename);
  
  opts = BoxOptionDefault;
  if(get_own_style(AutoNumberFormating))
    opts |= BoxOptionFormatNumbers;
    
  _content->load_from_object(content, opts);
  return true;
}

//} ... class MathSection

//{ class TextSection ...

TextSection::TextSection()
  : AbstractSequenceSection(new TextSequence, new Style)
{
}

TextSection::TextSection(SharedPtr<Style> _style)
  : AbstractSequenceSection(new TextSequence, _style)
{
  if(!style)
    style = new Style;
}

bool TextSection::try_load_from_object(Expr expr, int opts) {
  if(expr[0] != PMATH_SYMBOL_SECTION)
    return false;
    
  Expr content = expr[1];
  if(!content.is_string() && content[0] != PMATH_SYMBOL_LIST)
    return false;
    
  Expr options(pmath_options_extract(expr.get(), 2));
  if(options.is_null())
    return false;
    
  Expr stylename = expr[2];
  if(!stylename.is_string())
    stylename = String("Input");
    
  reset_style();
  style->add_pmath(options);
  style->set(BaseStyleName, stylename);
  
  opts = BoxOptionDefault;
  if(get_own_style(AutoNumberFormating))
    opts |= BoxOptionFormatNumbers;
    
  _content->load_from_object(content, opts);
  return true;
}

//} ... class TextSection

//{ class EditSection ...

EditSection::EditSection()
  : MathSection(new Style(String("Edit"))),
    original(0)
{
}

EditSection::~EditSection() {
  delete original;
}

bool EditSection::try_load_from_object(Expr expr, int opts) {
  return false;
}

Expr EditSection::to_pmath(int flags) {
  Expr result = content()->to_pmath(BoxFlagParseable);
  
  result = Application::interrupt(Call(
                                    Symbol(PMATH_SYMBOL_MAKEEXPRESSION),
                                    result),
                                  Application::edit_interrupt_timeout);
                                  
  if(result.expr_length() == 1
      && result[0] == PMATH_SYMBOL_HOLDCOMPLETE) {
    return result[1];
  }
  
  return Expr();
}

//} ... class EditSection
