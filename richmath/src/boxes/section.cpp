#include <boxes/section.h>

#include <cmath>

#include <boxes/sectionlist.h>
#include <boxes/mathsequence.h>
#include <boxes/textsequence.h>
#include <eval/client.h>
#include <graphics/context.h>

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

Section::~Section(){
}

Section *Section::create_from_object(const Expr object){
  if(object.instance_of(PMATH_TYPE_EXPRESSION)){
    if(object[0] == PMATH_SYMBOL_SECTION){
      Expr options(pmath_options_extract(object.get(), 2));
        
      if(options.get()){
        int opts = BoxOptionDefault;
        
        Expr content = object[1];
        Expr stylename = object[2];
        if(!stylename.instance_of(PMATH_TYPE_STRING))
          stylename = String("Input");
        
        SharedPtr<Style> style = new Style(options);
        style->set(BaseStyleName, stylename);
        
        AbstractSequenceSection *result = 0;
        
        if(content.expr_length() == 1 && content[0] == PMATH_SYMBOL_BOXDATA){
          content = content[1];
          
          result = new MathSection(style);
        }
        else if(content.instance_of(PMATH_TYPE_STRING) || content[0] == PMATH_SYMBOL_LIST){
          result = new TextSection(style);
        }
        
        if(result){
          Expr label = Expr(pmath_option_value(
            PMATH_SYMBOL_SECTION,
            PMATH_SYMBOL_SECTIONLABEL,
            options.get()));
          
          if(label.instance_of(PMATH_TYPE_STRING))
            result->label(label);
          
          if(result->get_own_style(AutoNumberFormating))
            opts|= BoxOptionFormatNumbers;
          
          ((AbstractSequence*)result->item(0))->load_from_object(content, opts);
          
          return result;
        }
      }
    }
  }
  
  return new ErrorSection(object);
}

void Section::label(const String str){
  if(str.is_valid()){
    if(!style)
      style = new Style;
    
    style->set(SectionLabel, str);
    label_glyphs.length(0);
  }
  else if(style)
    style->remove(SectionLabel);
}

float Section::label_width(){
  if(label_glyphs.length() == 0)
    return 0;
  
  return label_glyphs[label_glyphs.length() - 1].right;
}

void Section::resize_label(Context *context){
  String lbl = get_style(SectionLabel);
  if(!lbl.is_valid() || label_glyphs.length() == lbl.length())
    return;
  
  SharedPtr<TextShaper> shaper = TextShaper::find("Arial", NoStyle);
  
  float fs = context->canvas->get_font_size();
  context->canvas->set_font_size(8/* * 4/3. */);
  
  label_glyphs.length(lbl.length());
  shaper->decode_token(
    context,
    label_glyphs.length(),
    lbl.buffer(),
    label_glyphs.items());
  
  float x = 0;
  for(int i = 0;i < label_glyphs.length();++i)
    label_glyphs[i].right = x += label_glyphs[i].right;
  
  context->canvas->set_font_size(fs);
}

void Section::paint_label(Context *context){
  if(label_glyphs.length() == 0)
    return;
  
  String lbl = get_style(SectionLabel);
  if(lbl.length() != label_glyphs.length())
    return;
    
  SharedPtr<TextShaper> shaper = TextShaper::find("Arial", NoStyle);
    
  float x,y;
  context->canvas->current_pos(&x, &y);
  
  float fs = context->canvas->get_font_size();
  context->canvas->set_font_size(8/* * 4/3. */);
  
  int col = context->canvas->get_color();
  context->canvas->set_color(0x454E99);
  
  x-= label_glyphs[label_glyphs.length() - 1].right;
  float xx = x;
  const uint16_t *buf = lbl.buffer();
  for(int i = 0;i < label_glyphs.length();++i){
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
  int              *index
){
  if(_parent){
    if(direction == Forward)
      *index = _index + 1;
    else
      *index = _index;
    return _parent;
  }
  
  return this;
}

bool Section::selectable(int i){
  if(i < 0)
    return false;
  return Box::selectable(i);
}

bool Section::request_repaint(float x, float y, float w, float h){
  if(visible)
    return Box::request_repaint(x,y,w,h);
  return false;
}

void Section::invalidate(){
  must_resize = true;
  Box::invalidate();
}

bool Section::edit_selection(Context *context){
  if(!Box::edit_selection(context))
    return false;

  if(get_style(SectionLabel).length() > 0
  && get_style(SectionLabelAutoDelete)){
    if(style){
      String s;
      
      if(style->get(SectionLabel, &s)){
        style->remove(SectionLabel);
        if(get_style(SectionLabel).length() > 0)
          style->set(SectionLabel, String());
      }
      else
        style->set(SectionLabel, String());
    }
    else{
      style = new Style;
      style->set(SectionLabel, String());
    }
    
    invalidate();
  }

  if(style && get_style(BaseStyleName).equals("Output")){
    style->set(BaseStyleName, "Input");
    invalidate();
  }

  if(get_style(SectionGenerated)){
    if(style){
      int i;
      
      if(style->get(SectionGenerated, &i)){
        style->remove(SectionGenerated);
        if(get_style(SectionGenerated))
          style->set(SectionGenerated, false);
      }
      else
        style->set(SectionGenerated, false);
    }
    else{
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

void ErrorSection::resize(Context *context){
  must_resize = false;
  
  top_margin    = get_style(SectionMarginTop);
  bottom_margin = get_style(SectionMarginBottom);
  
  float em = get_style(FontSize);
  _extents.ascent  = 0;
  _extents.descent =     em + top_margin + bottom_margin;
  _extents.width   = 2 * em + get_style(SectionMarginLeft) + get_style(SectionMarginRight);
  
  unfilled_width = _extents.width;
}

void ErrorSection::paint(Context *context){
  float x, y;
  context->canvas->current_pos(&x, &y);
  
  context->canvas->save();
  
  context->canvas->pixrect(
    x + get_style(SectionMarginLeft),
    y + get_style(SectionMarginTop),
    x + _extents.width   - get_style(SectionMarginRight),
    y + _extents.descent - get_style(SectionMarginBottom),
    true);
  
  context->canvas->set_color(0xFFE6E6);
  context->canvas->fill_preserve();
  context->canvas->set_color(0xFF5454);
  context->canvas->hair_stroke();
  context->canvas->restore();
}

Box *ErrorSection::mouse_selection(
  float x,
  float y,
  int   *start,
  int   *end,
  bool  *eol
){
  *start = *end = 0;
  return this;
}
  
//} ... class ErrorSection

//{ class AbstractSequenceSection ...

AbstractSequenceSection::AbstractSequenceSection(AbstractSequence *content, SharedPtr<Style> style)
: Section(style),
  _content(content)
{
  adopt(_content, 0);
}

AbstractSequenceSection::~AbstractSequenceSection(){
  delete _content;
}

Box *AbstractSequenceSection::item(int i){
  return _content;
}

void AbstractSequenceSection::resize(Context *context){
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
  
  bool have_frame = get_style(Background) >= 0 
    || l != 0 || r != 0 || t != 0 || b != 0;
  if(have_frame){
    cx+= l;
    cx+= get_style(SectionFrameMarginLeft);
    
    cy+= t;
    cy+= get_style(SectionFrameMarginTop);
    
    horz_border+= r;
    horz_border+= get_style(SectionFrameMarginRight);
  }
  
  horz_border+= cx;
  context->width-= horz_border;
  context->section_content_window_width-= horz_border;
    
  _content->resize(context);
  
  unfilled_width = context->sequence_unfilled_width + horz_border;
  
  if(context->show_auto_styles){
    SyntaxState syntax;
    _content->colorize_scope(&syntax);
  }
  
  if(_content->var_extents().ascent < 0.75 * _content->get_em())
     _content->var_extents().ascent = 0.75 * _content->get_em();
  if(_content->var_extents().descent < 0.25 * _content->get_em())
     _content->var_extents().descent = 0.25 * _content->get_em();
  
  cy+= _content->extents().ascent;
  
  _extents.ascent = 0;
  _extents.descent = cy + _content->extents().descent + bottom_margin;
  
  if(context->width < HUGE_VAL
  && _content->var_extents().width < context->width)
     _content->var_extents().width = context->width;
  
  context->section_content_window_width = old_scww;
  cc.end();
  
  _extents.width = cx + _content->extents().width;
  
  if(have_frame){
    _extents.descent+= b;
    _extents.descent+= get_style(SectionFrameMarginBottom);
    
    _extents.width+= r + get_style(SectionFrameMarginRight);
  }
}

void AbstractSequenceSection::paint(Context *context){
  float x, y;
  context->canvas->current_pos(&x, &y);
  
  ContextState cc(context);
  cc.begin(style);
  
  float left_margin = get_style(SectionMarginLeft);
  int   background  = get_style(Background);
  
  if(background >= 0){
    if(context->canvas->show_only_text)
      return;
    
    float x1 = x + left_margin;
    float x2 = x + _extents.width;
    float y1 = y + top_margin;
    float y2 = y + _extents.descent - bottom_margin;
    
    context->canvas->pixrect(x1, y1, x2, y2, false);
    context->canvas->set_color(background);
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
  
  float l = get_style(SectionFrameLeft);
  float r = get_style(SectionFrameRight);
  float t = get_style(SectionFrameTop);
  float b = get_style(SectionFrameBottom);
  
  if(background >= 0 || l != 0 || r != 0 || t != 0 || b != 0){
    float x1 = x + left_margin;
    float x2 = x + _extents.width;
    float y1 = y + top_margin;
    float y2 = y + _extents.descent - bottom_margin;
    
    context->canvas->align_point(&x1, &y1, false);
    context->canvas->align_point(&x2, &y2, false);
    
    context->canvas->move_to(x1, y1);
    context->canvas->line_to(x2, y1);
    context->canvas->line_to(x2, y2);
    context->canvas->line_to(x1, y2);
    
    x1+= l;
    x2-= r;
    y1+= t;
    y2-= b;
    
    context->canvas->move_to(x1, y1);
    context->canvas->line_to(x1, y2);
    context->canvas->line_to(x2, y2);
    context->canvas->line_to(x2, y1);
    
    context->canvas->set_color(get_style(SectionFrameColor));
    
    context->canvas->fill();
  }

  cc.end();
}

Box *AbstractSequenceSection::remove(int *index){
  *index = 0;
  return _content;
}

Expr AbstractSequenceSection::to_pmath(bool parseable){
  Gather g;
  
  Expr cont = _content->to_pmath(false);
  if(dynamic_cast<MathSequence*>(_content))
    cont = Call(Symbol(PMATH_SYMBOL_BOXDATA), cont);
  
  g.emit(cont);
  
  String s;
  
  if(style && style->get(BaseStyleName, &s))
    g.emit(s);
  else
    g.emit(String(""));
  
  if(style)
    style->emit_to_pmath();
  
  Expr e = g.end();
  e.set(0, Symbol(PMATH_SYMBOL_SECTION));
  return e;
}

Box *AbstractSequenceSection::move_vertical(
  LogicalDirection  direction, 
  float            *index_rel_x,
  int              *index
){
  if(*index < 0){
    *index_rel_x-= cx;
    return _content->move_vertical(direction, index_rel_x, index);
  }
  
  if(_parent){
    *index_rel_x+= cx;
    if(direction == Forward)
      *index = _index + 1;
    else
      *index = _index;
    return _parent;
  }
  
  return this;
}
  
Box *AbstractSequenceSection::mouse_selection(
  float x,
  float y,
  int   *start,
  int   *end,
  bool  *eol
){
  return _content->mouse_selection(x - cx, y - cy, start, end, eol);
}

void AbstractSequenceSection::child_transformation(
  int             index,
  cairo_matrix_t *matrix
){
  cairo_matrix_translate(matrix, cx, cy);
}

//} ... class AbstractSequenceSection

//{ class MathSection ...

MathSection::MathSection(SharedPtr<Style> style)
: AbstractSequenceSection(new MathSequence, style)
{
}

//} ... class MathSection

//{ class TextSection ...

TextSection::TextSection(SharedPtr<Style> style)
: AbstractSequenceSection(new TextSequence, style)
{
}

//} ... class TextSection

//{ class EditSection ...

EditSection::EditSection()
: MathSection(new Style(String("Edit"))),
  original(0)
{
}

EditSection::~EditSection(){
  delete original;
}

Expr EditSection::to_pmath(bool parseable){
  Expr result = content()->to_pmath(true);
  
  result = Client::interrupt(Call(
    Symbol(PMATH_SYMBOL_MAKEEXPRESSION), 
    result));
  
  if(result.expr_length() == 1
  && result[0] == PMATH_SYMBOL_HOLDCOMPLETE){
    return result[1];
  }
  
  return Expr();
}

//} ... class EditSection
