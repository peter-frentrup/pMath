#include <boxes/section.h>

#include <cmath>

#include <boxes/sectionlist.h>
#include <boxes/mathsequence.h>
#include <boxes/textsequence.h>
#include <eval/application.h>
#include <graphics/context.h>
#include <graphics/rectangle.h>


using namespace richmath;
using namespace std;

extern pmath_symbol_t richmath_System_BoxData;
extern pmath_symbol_t richmath_System_Section;
extern pmath_symbol_t richmath_System_StyleData;


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
  if(expr[0] == richmath_System_Section) {
    Expr content = expr[1];
    
    Section *section = 0;
    
    if(content[0] == richmath_System_BoxData)
      section = Box::try_create<MathSection>(expr, BoxInputFlags::Default);
    else if(content[0] == richmath_System_StyleData)
      section = Box::try_create<StyleDataSection>(expr, BoxInputFlags::Default);
    else
      section = Box::try_create<TextSection>(expr, BoxInputFlags::Default);
      
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
  for(auto &glyph : label_glyphs)
    glyph.right = x += glyph.right;
    
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
  
  Color col = context->canvas->get_color();
  context->canvas->set_color(Color::from_rgb24(0x454E99));
  
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
      if(direction == LogicalDirection::Forward)
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

Box *Section::normalize_selection(int *start, int *end) {
  if(_parent) {
    *start = _index;
    *end = _index + 1;
    return _parent->normalize_selection(start, end);
  }
  
  return this;
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
    if(auto slist = dynamic_cast<SectionList *>(parent())) {
      slist->set_open_close_group(index(), true);
      
      if(get_style(SectionEditDuplicateMakesCopy)) {
        slist->insert(index(), Section::create_from_object(to_pmath(BoxOutputFlags::Default)));
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

void Section::reset_style() { 
  if(style) {
    style->clear(); 
    style->set(InternalHasPendingDynamic, true);
  }
}

//} ... class Section

//{ class ErrorSection ...

ErrorSection::ErrorSection(const Expr object)
  : Section(0),
    _object(object)
{
}

bool ErrorSection::try_load_from_object(Expr expr, BoxInputFlags opts) {
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
  update_dynamic_styles(context);
  
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

void AbstractSequenceSection::adopt_all() {
  int i = 0;
  adopt(_content, i++);
  
  if(Box *box = _dingbat.box_or_null())
    adopt(box, i++);
}

Box *AbstractSequenceSection::item(int i) {
  if(i == 0)
    return _content;
  
  if(_dingbat.has_index(i))
    return _dingbat.box_or_null();
  
  assert(0 && "invalid index");
  return nullptr;
}

int AbstractSequenceSection::count() {
  int i = 1;
  if(_dingbat.box_or_null())
    ++i;
  
  return i;
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
  
  float left_margin = get_style(SectionMarginLeft);
  cx = left_margin;
  cy = top_margin;
  
  float horz_border = get_style(SectionMarginRight);
  
  float l = get_style(SectionFrameLeft);
  float r = get_style(SectionFrameRight);
  float t = get_style(SectionFrameTop);
  float b = get_style(SectionFrameBottom);
  
  bool have_frame = get_style(Background).is_valid() || l != 0 || r != 0 || t != 0 || b != 0;
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
  
  if(Box *dingbat = _dingbat.box_or_null()) {
    dingbat->resize(context);
    float dist = get_style(SectionFrameLabelMarginLeft);
    auto extra_indent = dingbat->extents().width + dist - cx;
    if(extra_indent > 0) {
      horz_border                           += extra_indent;
      left_margin                           += extra_indent;
      cx                                    += extra_indent;
      context->width                        -= extra_indent;
      context->section_content_window_width -= extra_indent;
    }
  }
  
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
  
  float w = min(context->width, context->section_content_window_width);
  if(w < HUGE_VAL && _content->var_extents().width < w) {
    _content->var_extents().width = w;
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
  float x, y;
  context->canvas->current_pos(&x, &y);
  
  ContextState cc(context);
  //cc.begin(style);
  cc.begin(nullptr);
  
  cc.apply_layout_styles(style);
  
  float left_margin = get_style(SectionMarginLeft);
  Color background  = get_style(Background);
  
  float l = get_style(SectionFrameLeft);
  float r = get_style(SectionFrameRight);
  float t = get_style(SectionFrameTop);
  float b = get_style(SectionFrameBottom);
  
  // TODO: suppress request_repaint_all if only non-layout styles changed during update_dynamic_styles()
  update_dynamic_styles(context);
  cc.apply_non_layout_styles(style);
  
  if(_dingbat.reload_if_necessary(get_own_style(SectionDingbat), BoxInputFlags::Default)) {
    adopt_all();
    invalidate();
  }
  
  if(background.is_valid() || l != 0 || r != 0 || t != 0 || b != 0) {
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
    
    if(background.is_valid() && !context->canvas->show_only_text) {
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
  
  Expr textshadow;
  context->stylesheet->get(style, TextShadow, &textshadow);
  context->canvas->set_color(get_style(FontColor));
  
  float xx, yy;
  
  if(Box *dingbat = _dingbat.box_or_null()) {
    float dist = get_style(SectionFrameLabelMarginLeft);
    xx = x + cx - dist - dingbat->extents().width;
    yy = y + cy;
    context->canvas->align_point(&xx, &yy, false);
    context->canvas->move_to(xx, yy);
    
    context->draw_with_text_shadows(dingbat, textshadow);
  }
  
  xx = x + cx;
  yy = y + cy;
  context->canvas->align_point(&xx, &yy, false);
  context->canvas->move_to(xx, yy);
  
  context->draw_with_text_shadows(_content, textshadow);
  
  cc.end();
}

Box *AbstractSequenceSection::remove(int *index) {
  *index = 0;
  return _content;
}

Expr AbstractSequenceSection::to_pmath_symbol() {
  return Symbol(richmath_System_Section);
}

Expr AbstractSequenceSection::to_pmath(BoxOutputFlags flags) {
  Gather g;
  
  Expr cont = _content->to_pmath(flags/* & ~BoxOutputFlags::Parseable*/);
  if(dynamic_cast<MathSequence *>(_content))
    cont = Call(Symbol(richmath_System_BoxData), cont);
    
  Gather::emit(cont);
  
  String s;
  
  if(style && style->get(BaseStyleName, &s))
    Gather::emit(s);
  else
    Gather::emit(String(""));
    
  if(style)
    style->emit_to_pmath();
    
  Expr e = g.end();
  e.set(0, Symbol(richmath_System_Section));
  return e;
}

Box *AbstractSequenceSection::move_logical(
  LogicalDirection  direction,
  bool              jumping,
  int              *index
) {
  if(direction == LogicalDirection::Forward) {
    if(*index >= 0)
      *index = count();
  }
  else {
    if(*index > 1) {
      *index = 1;
      if(jumping)
        ++*index;
    }
  }
  
  return Section::move_logical(direction, jumping, index);
}
        
Box *AbstractSequenceSection::move_vertical(
  LogicalDirection  direction,
  float            *index_rel_x,
  int              *index,
  bool              called_from_child
) {
  if(*index < 0) {
    if(!can_enter_content() || !get_own_style(Selectable, true))
      return Section::move_vertical(direction, index_rel_x, index, called_from_child);
    *index_rel_x -= cx;
    return _content->move_vertical(direction, index_rel_x, index, false);
  }
  
  if(_parent) {
    *index_rel_x += cx;
//    if(direction == LogicalDirection::Forward)
//      *index = _index + 1;
//    else
//      *index = _index;
//    return _parent;
    *index = _index;
    return _parent->move_vertical(direction, index_rel_x, index, true);
  }
  
  return this;
}

Box *AbstractSequenceSection::mouse_sensitive() {
  if(SectionList *slist = dynamic_cast<SectionList*>(parent())) {
    if(get_own_style(WholeSectionGroupOpener)) 
      return this;
  }
  
  return base::mouse_sensitive();
}

void AbstractSequenceSection::on_mouse_up(MouseEvent &event) {
  if(SectionList *slist = dynamic_cast<SectionList*>(parent())) {
    if(get_own_style(WholeSectionGroupOpener)) {
      if(slist->group_info(_index).end > _index)
        slist->toggle_open_close_group(_index);
      
      return;
    }
  }
  base::on_mouse_up(event);
}

Box *AbstractSequenceSection::mouse_selection(
  float  x,
  float  y,
  int   *start,
  int   *end,
  bool  *was_inside_start
) {
  if(Box *dingbat = _dingbat.box_or_null()) {
    cairo_matrix_t mat;
    cairo_matrix_init_identity(&mat);
    child_transformation(dingbat->index(), &mat);
    if( mat.x0 <= x && 
        x <= mat.x0 + dingbat->extents().width &&
        mat.y0 - dingbat->extents().ascent <= y &&
        y <= mat.y0 + dingbat->extents().descent)
    {
      if(Box *box = dingbat->mouse_selection(x - mat.x0, y - y - mat.y0, start, end, was_inside_start)) {
        //Box *tmp = box->mouse_sensitive();
        //while(tmp && tmp->parent() != this)
        //  tmp = tmp->parent();
        //
        //if(tmp)
          return box;
      }
    }
  }
  
  return _content->mouse_selection(x - cx, y - cy, start, end, was_inside_start);
}

void AbstractSequenceSection::child_transformation(
  int             index,
  cairo_matrix_t *matrix
) {
  if(_dingbat.has_index(index)) {
    float dist = get_style(SectionFrameLabelMarginLeft);
    float dingbat_width = _dingbat.box_or_null()->extents().width;
    
    cairo_matrix_translate(matrix, cx - dist - dingbat_width, cy);
    return;
  }
  
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

bool MathSection::try_load_from_object(Expr expr, BoxInputFlags opts) {
  if(expr[0] != richmath_System_Section)
    return false;
    
  Expr content = expr[1];
  if(content.expr_length() != 1 || content[0] != richmath_System_BoxData)
    return false;
    
  content = content[1];
  
  Expr options(pmath_options_extract_ex(expr.get(), 2, PMATH_OPTIONS_EXTRACT_UNKNOWN_WARNONLY));
  if(options.is_null())
    return false;
    
  Expr stylename = expr[2];
  if(!stylename.is_string())
    stylename = String("Input");
    
  reset_style();
  style->add_pmath(options);
  style->set(BaseStyleName, stylename);
  
  opts = BoxInputFlags::Default;
  if(get_own_style(AutoNumberFormating))
    opts |= BoxInputFlags::FormatNumbers;
    
  _content->load_from_object(content, opts);
  
  must_resize = true;
  finish_load_from_object(std::move(expr));
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

bool TextSection::try_load_from_object(Expr expr, BoxInputFlags opts) {
  if(expr[0] != richmath_System_Section)
    return false;
    
  Expr content = expr[1];
  if(!content.is_string() && content[0] != PMATH_SYMBOL_LIST)
    return false;
    
  Expr options(pmath_options_extract_ex(expr.get(), 2, PMATH_OPTIONS_EXTRACT_UNKNOWN_WARNONLY));
  if(options.is_null())
    return false;
    
  Expr stylename = expr[2];
  if(!stylename.is_string())
    stylename = String("Input");
    
  reset_style();
  style->add_pmath(options);
  style->set(BaseStyleName, stylename);
  
  opts = BoxInputFlags::Default;
  if(get_own_style(AutoNumberFormating))
    opts |= BoxInputFlags::FormatNumbers;
    
  _content->load_from_object(content, opts);
  
  must_resize = true;
  finish_load_from_object(std::move(expr));
  return true;
}

//} ... class TextSection

//{ class EditSection ...

EditSection::EditSection()
  : MathSection(new Style(String("Edit"))),
    original(nullptr)
{
}

EditSection::~EditSection() {
  delete original;
}

bool EditSection::try_load_from_object(Expr expr, BoxInputFlags opts) {
  return false;
}

Expr EditSection::to_pmath(BoxOutputFlags flags) {
  Expr result = content()->to_pmath(BoxOutputFlags::Parseable | flags);
  
  if(false && has(flags, BoxOutputFlags::NoNewSymbols)) {
    // todo: wrap in Quiet(...)
    result = Call(
               Symbol(PMATH_SYMBOL_MAKEEXPRESSION),
               std::move(result),
               Rule(Symbol(PMATH_SYMBOL_PARSESYMBOLS), Symbol(PMATH_SYMBOL_FALSE)));
    
    if(original) {
      result = Call(
                 Symbol(PMATH_SYMBOL_TRY), 
                 std::move(result),
                 Symbol(PMATH_SYMBOL_FAILED),
                 List(
                   Call(
                     Symbol(PMATH_SYMBOL_MESSAGENAME), 
                     Symbol(PMATH_SYMBOL_MAKEEXPRESSION),
                     String("nonewsym"))));
    }
  }
  else{
    result = Call(
               Symbol(PMATH_SYMBOL_MAKEEXPRESSION),
               std::move(result));
  }
  
  result = Application::interrupt_wait(
             result,
             Application::edit_interrupt_timeout);
  
  if(original) {
    if(result == PMATH_SYMBOL_FAILED) 
      result = original->to_pmath(flags);
  }
  
  if(result.expr_length() == 1 && result[0] == PMATH_SYMBOL_HOLDCOMPLETE) {
    return result[1];
  }
  
  return Expr();
}

//} ... class EditSection

//{ class StyleDataSection ...

StyleDataSection::StyleDataSection()
  : AbstractSequenceSection(new MathSequence, new Style)
{
}

/* FIXME: The StyleDataSection should not use its parent document stylesheet for display,
   but all the style definitions above itself.

   One possibility is that each StyleDataSection `sds` has a Stylesheet that represents the all
   style definitions upto and including `sds`.
   When the `sds` needs to recalculate a style, it searches the previous StyleDataSection`s
   Stylesheet, obtains its previous Style `old_style` of the same name, creates a new merged
   copy of `old_style` and its own local definitions (Box::style) and stores that in its own
   Stylesheet.
   The problem is to know when to recalculate a style. Maybe styles should be observable
   (like symbols are by `FrontEndObject`s).
 */
bool StyleDataSection::try_load_from_object(Expr expr, BoxInputFlags opts) {
  if(expr[0] != richmath_System_Section)
    return false;
    
  Expr style_data = expr[1];
  if(style_data[0] != richmath_System_StyleData)
    return false;
    
  Expr options(pmath_options_extract_ex(expr.get(), 1, PMATH_OPTIONS_EXTRACT_UNKNOWN_WARNONLY));
  if(options.is_null())
    return false;
    
  _style_data = style_data;
  
  reset_style();
  style->add_pmath(options);
  
  opts = BoxInputFlags::Default;
  if(get_own_style(AutoNumberFormating))
    opts |= BoxInputFlags::FormatNumbers;
    
  Expr boxes = Application::interrupt_wait(
                 Parse("FE`Styles`MakeStyleDataBoxes(HoldComplete(`1`))", style_data),
                 Application::button_timeout);
  _content->load_from_object(boxes, opts);
  
  must_resize = true;
  finish_load_from_object(std::move(expr));
  return true;
}

Expr StyleDataSection::to_pmath(BoxOutputFlags flags) {
  Gather g;
  
  Gather::emit(_style_data);
  style->emit_to_pmath(true);
  
  Expr e = g.end();
  e.set(0, Symbol(richmath_System_Section));
  return e;
}

Box *StyleDataSection::mouse_selection(
  float  x,
  float  y,
  int   *start,
  int   *end,
  bool  *was_inside_start
) {
  Box *box = AbstractSequenceSection::mouse_selection(x, y, start, end, was_inside_start);
  if(box) {
    Box *mouse = box->mouse_sensitive();
    if(mouse && !mouse->is_parent_of(this)) 
      return box;
  }
  
  *was_inside_start = true;
//  if(_parent) {
//    *start = _index;
//    *end = *start + 1;
//    return _parent;
//  }
  *start = 0;
  *end = length();
  return this;
}


//} ... class StyleDataSection
