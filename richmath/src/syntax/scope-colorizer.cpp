#include <syntax/scope-colorizer.h>
#include <syntax/spanexpr.h>

#include <boxes/mathsequence.h>
#include <boxes/subsuperscriptbox.h>
#include <boxes/underoverscriptbox.h>

#include <eval/application.h>
#include <eval/binding.h>

#include <climits>

extern pmath_symbol_t richmath_System_Alternatives;
extern pmath_symbol_t richmath_System_False;
extern pmath_symbol_t richmath_System_List;
extern pmath_symbol_t richmath_System_Options;
extern pmath_symbol_t richmath_System_Syntax;
extern pmath_symbol_t richmath_FE_SymbolInfo;

using namespace richmath;

namespace {
  class Painter {
    public:
      explicit Painter(MathSequence &seq);
      
      int index() const { return _iter.index(); }
      SyntaxGlyphStyle get_style() const { return _iter.get(); }
      
      void paint_until(SyntaxGlyphStyle style, int next_index);
      void move_to(int next_index);
      
    private:
      using iterator_type = SyntaxGlyphStylesArray::iterator_type;
      
      MathSequence               &_seq_debug;
      iterator_type               _iter;
  };
  
  class ScopeColorizerImpl {
    private:
      MathSequence  &sequence;
      SyntaxState   &state;
      Painter        painter;
      const String  &str;
      
    public:
      explicit ScopeColorizerImpl(MathSequence &sequence, SyntaxState &state);
      
      void colorize_spanexpr(SpanExpr *se);
      
    private:
      int length() { return str.length(); }
      
      bool prepare_symbol_colorization(int start, SymbolKind kind, int &end, SyntaxGlyphStyle &style);
      int symbol_colorize(int start, SymbolKind kind);
      void symdef_colorize_spanexpr(SpanExpr *se, SymbolKind kind); // "x"  "x:=y"  "{{x,y},z}:=w"
      void symlist_colorize_spanexpr(SpanExpr *se, SymbolKind kind); // "x"  "{x,y}"
      void symdeflist_colorize_spanexpr(SpanExpr *se, SymbolKind kind); // "{symdefs ...}"
      void replacement_colorize_spanexpr(SpanExpr *se, SymbolKind kind); // "x->value"
      void colorize_keyword(SpanExpr *se);
      void colorize_identifier(SpanExpr *se); // identifiers   #   ~
      void colorize_pure_argument(SpanExpr *se); // #x
      void colorize_pure_function(SpanExpr *se); // body &
      void colorize_mapsto_function(SpanExpr *se);
      void colorize_simple_pattern_name(SpanExpr *se); // ~x   ?x
      void colorize_pattern_name(SpanExpr *se); // name:pat
      void colorize_typed_pattern(SpanExpr *se); // ~name:type
      void colorize_optional_value_pattern(SpanExpr *se); // ?name:value
      void colorize_assignment_or_rule(SpanExpr *se, bool delayed);
      void colorize_tag_assignment(SpanExpr * se, bool delayed); // x/:y:=z   x/:y::=z
      bool colorize_integral(SpanExpr *se); // \[Integral] ... \[DifferentialD]...
      bool colorize_bigop(SpanExpr *se);
      void colorize_localspec_call(SpanExpr *se, const SyntaxInformation &info);
      void colorize_functionspec_call(SpanExpr *se, const SyntaxInformation &info);
      void colorize_tablespec_call(SpanExpr *se, const SyntaxInformation &info);
      void colorize_simple_call(SpanExpr *head_name, SpanExpr *se);
      void colorize_block_body_line(SpanExpr *se, SharedPtr<ScopePos> &scope_after_block);
      void colorize_block_body(SpanExpr *se);
      void symdef_local_colorize_spanexpr(SpanExpr *se);
      void symdef_parameter_colorize_spanexpr(SpanExpr *se);
      void replacement_special_colorize_spanexpr(SpanExpr *se);
      void colorize_scoping_block_head(FunctionCallSpan &head, SharedPtr<ScopePos> &scope_after_block, void (ScopeColorizerImpl::*colorize_def)(SpanExpr*));
      void colorize_scoping_block(FunctionCallSpan &head, SpanExpr *se, void (ScopeColorizerImpl::*colorize_def)(SpanExpr*));
      void colorize_if_blocks(FunctionCallSpan &head, SpanExpr *se);
      void colorize_block(SpanExpr *se);
  };
  
  class ErrorColorizerImpl {
    private:
      Painter painter;
      float   error_indicator_height;
      
    public:
      explicit ErrorColorizerImpl(MathSequence &sequence, float _error_indicator_height);
      
      void arglist_errors_colorize_spanexpr(SpanExpr *se);
      
    private:
      void unknown_option_colorize_spanexpr(SpanExpr *se, Expr options);
      void add_missing_indicator(SpanExpr *span_before);
      void mark_excess_args(FunctionCallSpan &call, int max_args);
      void colorize_name(SpanExpr *se, SyntaxGlyphStyle style);
      void arglist_errors_colorize_spanexpr_norecurse(SpanExpr *se);
      void get_block_head_argument_counts(SpanExpr *name, int &argmin, int &argmax);
      void colorize_block_body_line_errors(SpanExpr *se);
      void colorize_block_body_errors(SpanExpr *se);
      void colorize_block_errors(SpanExpr *se);
  };
  
  class SyntaxColorizerImpl {
    public:
      explicit SyntaxColorizerImpl(MathSequence &seq);
      
      void colorize_spanexpr(SpanExpr *se);
      void colorize_quoted_string(SpanExpr *se);
      bool colorize_single_token(SpanExpr *se);
      
      void comments_colorize_span(Span span);
      void comments_colorize_until(int end);
    
    private:
      Painter                    painter;
      const SpanArray           &spans;
      ArrayView<const uint16_t>  text;
  };
}

//{ class ScopeColorizer ...

ScopeColorizer::ScopeColorizer(MathSequence &sequence)
  : Base(),
    sequence(sequence)
{
  SET_BASE_DEBUG_TAG(typeid(*this).name());
}

void ScopeColorizer::scope_colorize_spanexpr(SyntaxState &state, SpanExpr *se) {
  ScopeColorizerImpl(sequence, state).colorize_spanexpr(se);
}


void ScopeColorizer::comments_colorize() {
  SyntaxColorizerImpl(sequence).comments_colorize_until(sequence.length());
}

void ScopeColorizer::syntax_colorize_spanexpr(SpanExpr *se) {
  SyntaxColorizerImpl(sequence).colorize_spanexpr(se);
}

void ScopeColorizer::arglist_errors_colorize_spanexpr(SpanExpr *se, float error_indicator_height) {
  ErrorColorizerImpl(sequence, error_indicator_height).arglist_errors_colorize_spanexpr(se);
}

//} ... class ScopeColorizer

//{ class ScopeColorizerImpl ...

inline ScopeColorizerImpl::ScopeColorizerImpl(MathSequence &sequence, SyntaxState &state)
  : sequence(sequence),
    state(state),
    painter(sequence),
    str(sequence.text())
{
}

void ScopeColorizerImpl::colorize_spanexpr(SpanExpr *se) {
  RICHMATH_ASSERT(se != 0);
  
  if(se->count() == 0) { // identifiers   #   ~
    colorize_identifier(se);
    return;
  }
  
  if(se->count() == 2) { // #x   ~x   ?x   x&   <<x
  
    if( se->item_first_char(0) == '#' &&
        pmath_char_is_digit(se->item_first_char(1)))
    {
      colorize_pure_argument(se);
      return;
    }
    
    if((se->item_first_char(0) == '~' ||
        se->item_as_char(0)    == '?') &&
        pmath_char_is_name(se->item_first_char(1)))
    {
      colorize_simple_pattern_name(se);
      return;
    }
    
    if(se->item_as_char(1) == '&') {
      colorize_pure_function(se);
      return;
    }
    
    if( se->item_equals(0, "<<") ||
        se->item_equals(0, "??"))
    {
      return;
    }
  }
  
  if(se->count() >= 2 && se->count() <= 3) { // ~:t   x:p   x->y   x:=y   integrals   bigops
    if(se->item_as_char(1) == ':') {
      if(se->item_first_char(0) == '~') {
        painter.move_to(se->start());
        painter.paint_until(GlyphStyleParameter, se->end() + 1);
        return;
      }
      
      if(pmath_char_is_name(se->item_first_char(0))) {
        colorize_pattern_name(se);
        return;
      }
    }
    
    if( se->item_as_char(1) == 0x21A6 || // mapsto
        se->item_equals(1, "|->"))
    {
      colorize_mapsto_function(se);
      return;
    }
    
    if( se->item_as_char(1) == PMATH_CHAR_RULE   ||
        se->item_as_char(1) == PMATH_CHAR_ASSIGN ||
        se->item_equals(1, "->")                 ||
        se->item_equals(1, ":="))
    {
      colorize_assignment_or_rule(se, false);
      return;
    }
    
    if( se->item_as_char(1) == PMATH_CHAR_RULEDELAYED   ||
        se->item_as_char(1) == PMATH_CHAR_ASSIGNDELAYED ||
        se->item_equals(1, ":>")                        ||
        se->item_equals(1, "::="))
    {
      colorize_assignment_or_rule(se, true);
      return;
    }
    
    if(colorize_integral(se))
      return;
      
    if(colorize_bigop(se))
      return;
  }
  
  if(se->count() >= 3 && se->count() <= 4) { // F(x)   ~x:t
    if( se->item_as_char(1) == '(') {
      if(SpanExpr *head_name = span_as_name(se->item(0))) {
        colorize_simple_call(head_name, se);
        return;
      }
    }
    
    if( se->item_as_char(2)    == ':' &&
        se->item_first_char(0) == '~' &&
        pmath_char_is_name(se->item_first_char(1)))
    {
      colorize_typed_pattern(se);
      return;
    }
    
    if( se->item_as_char(2)    == ':' &&
        se->item_first_char(0) == '?' &&
        pmath_char_is_name(se->item_first_char(1)))
    {
      colorize_optional_value_pattern(se);
      return;
    }
    
  }
  
  if(se->count() >= 4 && se->count() <= 5) { // x/:y:=z   x/:y::=z
    if(se->item_equals(1, "/:")) {
      if( se->item_as_char(3) == PMATH_CHAR_ASSIGN ||
          se->item_equals(3, ":="))
      {
        colorize_tag_assignment(se, false);
        return;
      }
      
      if( se->item_as_char(3) == PMATH_CHAR_ASSIGNDELAYED ||
          se->item_equals(3, "::="))
      {
        colorize_tag_assignment(se, true);
        return;
      }
    }
  }
  
  if(BlockSpan::maybe_block(se)) {
    colorize_block(se);
    return;
  }
  
  for(int i = 0; i < se->count(); ++i) {
    SpanExpr *sub = se->item(i);
    
    colorize_spanexpr(sub);
  }
}

int ScopeColorizerImpl::symbol_colorize(int start, SymbolKind kind) {
  int end;
  SyntaxGlyphStyle style;
  if(prepare_symbol_colorization(start, kind, end, style)) {
    painter.move_to(start);
    painter.paint_until(style, end);
  }
  return end;
}

bool ScopeColorizerImpl::prepare_symbol_colorization(int start, SymbolKind kind, int &end, SyntaxGlyphStyle &style) {
  const SpanArray &spans = sequence.span_array();
  
  style = GlyphStyleNone;
  end = start;
  while(end < length() && !spans.is_token_end(end))
    ++end;
  ++end;
  
  if(sequence.is_placeholder(start) && start + 1 == end)
    return false;
    
  painter.move_to(start);
  switch(painter.get_style().kind()) {
    case GlyphStyleNone:
    case GlyphStyleParameter:
    case GlyphStyleLocal:
    case GlyphStyleNewSymbol:
    case GlyphStyleFunctionCall:
      break;
    default:
      return false;
  }
  
  String name(str.part(start, end - start));
  
  SharedPtr<SymbolInfo> info = state.local_symbols[name];
  
  if(kind == SymbolKind::Global) {
    while(info) {
      if(info->pos->contains(state.current_pos)) {
        switch(info->kind) {
          case SymbolKind::LocalSymbol:  style = GlyphStyleLocal;      break;
          case SymbolKind::Special:      style = GlyphStyleSpecialUse; break;
          case SymbolKind::Parameter:    style = GlyphStyleParameter;  break;
          case SymbolKind::Error:        style = GlyphStyleScopeError; break;
          default: break;
        }
        
        break;
      }
      
      info = info->next;
    };
    
    if(!info) {
      Expr syminfo = Application::interrupt_wait_cached(Call(
                       Symbol(richmath_FE_SymbolInfo),
                       name));
                       
      if(syminfo == richmath_System_False)
        style = GlyphStyleNewSymbol;
      else if(syminfo == richmath_System_Alternatives)
        style = GlyphStyleShadowError;
      else if(syminfo == richmath_System_Syntax)
        style = GlyphStyleSyntaxError;
      // True, Function
    }
  }
  else if(info) {
    SharedPtr<SymbolInfo> si = info;
    
    do {
      if(info->pos.ptr() == state.current_pos.ptr()) {
        if(info->kind == kind)
          break;
          
        if(info->kind == SymbolKind::LocalSymbol && kind == SymbolKind::Special) {
          si->add(kind, state.current_pos);
          break;
        }
        
        if(kind == SymbolKind::Special) {
          kind = info->kind;
          break;
        }
        
        si->add(SymbolKind::Error, state.current_pos);
        style = GlyphStyleScopeError;
        break;
      }
      
      if(info->pos->contains(state.current_pos)) {
        if(info->kind == SymbolKind::LocalSymbol && kind != SymbolKind::LocalSymbol/* && kind == SymbolKind::Special*/) {
          si->add(kind, state.current_pos);
          break;
        }
        
        if(kind == SymbolKind::Special) {
          kind = info->kind;
          break;
        }
        
        si->add(SymbolKind::Error, state.current_pos);
        style = GlyphStyleScopeError;
        break;
      }
      
      info = info->next;
    } while(info);
    
    if(!info) {
      si->kind = kind;
      si->pos  = state.current_pos;
      si->next = nullptr;
    }
  }
  else
    state.local_symbols.set(name, new SymbolInfo(kind, state.current_pos));
    
  if(!style) {
    switch(kind) {
      case SymbolKind::LocalSymbol:  style = GlyphStyleLocal;      break;
      case SymbolKind::Special:      style = GlyphStyleSpecialUse; break;
      case SymbolKind::Parameter:    style = GlyphStyleParameter;  break;
      default: return false;
    }
  }
  
  return true;
}

void ScopeColorizerImpl::symdef_colorize_spanexpr(SpanExpr *se, SymbolKind kind) { // "x"  "x:=y"
  if( se->count() >= 2 && se->count() <= 3) {
    if( se->item_as_char(1) == PMATH_CHAR_ASSIGN        ||
        se->item_as_char(1) == PMATH_CHAR_ASSIGNDELAYED ||
        se->item_equals(1, ":=")                        ||
        se->item_equals(1, "::="))
    {
      symlist_colorize_spanexpr(se->item(0), kind);
      return;
    }
  }
  
  if(pmath_char_is_name(se->first_char()))
    symbol_colorize(se->start(), kind);
}

void ScopeColorizerImpl::symlist_colorize_spanexpr(SpanExpr *se, SymbolKind kind) {
  if(FunctionCallSpan::is_list(se)) { // "{x, ...}"
    FunctionCallSpan list(se);
    
    for(int i = 1; i <= list.list_length(); ++i) {
      symlist_colorize_spanexpr(list.list_element(i), kind);
    }
  }
  else if(pmath_char_is_name(se->first_char())) 
    symbol_colorize(se->start(), kind);
}

void ScopeColorizerImpl::symdeflist_colorize_spanexpr(SpanExpr *se, SymbolKind kind) { // "{symdefs ...}"
  if(se->count() != 3 || se->item_as_char(0) != '{')
    return;
    
  se = se->item(1);
  if(se->count() >= 2 && se->item_as_char(1) == ',') {
    for(int i = 0; i < se->count(); ++i)
      symdef_colorize_spanexpr(se->item(i), kind);
  }
  else
    symdef_colorize_spanexpr(se, kind);
}

void ScopeColorizerImpl::replacement_colorize_spanexpr(SpanExpr *se, SymbolKind kind) { // "x->value"
  if( se->count() >= 2 &&
      (se->item_as_char(1) == PMATH_CHAR_RULE ||
       se->item_equals(1, "->")) &&
      pmath_char_is_name(se->item_first_char(0)))
  {
    symbol_colorize(se->item_pos(0), kind);
  }
}

void ScopeColorizerImpl::colorize_keyword(SpanExpr *se) {
  se = span_as_name(se);
  if(!se)
    return;
  
  painter.move_to(se->start());
  painter.paint_until(GlyphStyleKeyword, se->end() + 1);
}

void ScopeColorizerImpl::colorize_identifier(SpanExpr *se) { // identifiers   #   ~
  RICHMATH_ASSERT(se->count() == 0);
  
  painter.move_to(se->start());
  switch(painter.get_style().kind()) {
    case GlyphStyleNone:
    case GlyphStyleFunctionCall:
      break;
    
    default:
      return;
  }
  
  if(se->is_box()) {
    se->as_box()->colorize_scope(state);
    return;
  }
  
  if(pmath_char_is_name(se->first_char())) {
    symbol_colorize(se->start(), SymbolKind::Global);
    return;
  }
  
  if(se->first_char() == '#') {
    if(state.in_function)
      painter.paint_until(GlyphStyleParameter, se->end() + 1);
    else
      painter.paint_until(GlyphStyleScopeError, se->end() + 1);
    
    return;
  }
  
  if(se->first_char() == '~' && state.in_pattern) {
    painter.paint_until(GlyphStyleParameter, se->end() + 1);
    return;
  }
}

void ScopeColorizerImpl::colorize_pure_argument(SpanExpr *se) { // #x
  RICHMATH_ASSERT(se->count() == 2);
  
  painter.move_to(se->start());
  if(state.in_function)
    painter.paint_until(GlyphStyleParameter, se->end() + 1);
  else
    painter.paint_until(GlyphStyleScopeError, se->end() + 1);
}

void ScopeColorizerImpl::colorize_pure_function(SpanExpr *se) { // body &
  RICHMATH_ASSERT(se->count() == 2);
  
  bool old_in_function = state.in_function;
  state.in_function = true;
  
  colorize_spanexpr(se->item(0));
  
  state.in_function = old_in_function;
}

void ScopeColorizerImpl::colorize_mapsto_function(SpanExpr *se) {
  // args \[Function]
  // args \[Function] body
  RICHMATH_ASSERT(se->count() == 2 || se->count() == 3);
  
  SharedPtr<ScopePos> next_scope = state.new_scope();
  state.new_scope();
  
  SpanExpr *sub = se->item(0);
  colorize_spanexpr(sub);
  
  if( sub->count() == 3 &&
      sub->item_as_char(0) == '(' &&
      sub->item_as_char(2) == ')')
  {
    sub = sub->item(1);
  }
  
  if( sub->count() == 0 &&
      pmath_char_is_name(sub->first_char()))
  {
    symbol_colorize(sub->start(), SymbolKind::Parameter);
  }
  else if(sub->count() > 0 &&
          sub->item_as_char(1) == ',')
  {
    const uint16_t *buf = str.buffer();
    
    for(int i = 0; i < sub->count(); ++i)
      if(pmath_char_is_name(buf[sub->item_pos(i)]))
        symbol_colorize(sub->item_pos(i), SymbolKind::Parameter);
  }
  
  if(se->count() == 3) {
    colorize_spanexpr(se->item(2));
  }
  else {
    painter.move_to(se->item_pos(1));
    painter.paint_until(GlyphStyleSyntaxError, se->item(1)->end() + 1);
  }
  
  state.current_pos = next_scope;
}

void ScopeColorizerImpl::colorize_simple_pattern_name(SpanExpr *se) { // ~x   ?x
  RICHMATH_ASSERT(se->count() == 2);
  
  if(!state.in_pattern)
    return;
  
  int end;
  SyntaxGlyphStyle style;
  if(prepare_symbol_colorization(se->item_pos(1), SymbolKind::Parameter, end, style)) {
    painter.move_to(se->item_pos(0));
    painter.paint_until(GlyphStyleParameter, end);
  }
}

void ScopeColorizerImpl::colorize_pattern_name(SpanExpr *se) { // name:pat
  RICHMATH_ASSERT(se->count() >= 2);
  
  if(!state.in_pattern)
    return;
    
  for(int i = 0; i < se->count(); ++i) {
    SpanExpr *sub = se->item(i);
    
    colorize_spanexpr(sub);
  }
  
  symbol_colorize(se->item_pos(0), SymbolKind::Parameter);
}

void ScopeColorizerImpl::colorize_typed_pattern(SpanExpr *se) { // ~name:type
  RICHMATH_ASSERT(se->count() >= 3);
  
  if(!state.in_pattern)
    return;
  
  int end;
  SyntaxGlyphStyle style;
  if(prepare_symbol_colorization(se->item_pos(1), SymbolKind::Parameter, end, style)) {
    painter.move_to(se->item_pos(0));
    painter.paint_until(GlyphStyleParameter, se->end() + 1);
  }
}

void ScopeColorizerImpl::colorize_optional_value_pattern(SpanExpr *se) { // ?name:value
  RICHMATH_ASSERT(se->count() >= 3);
  
  if(!state.in_pattern)
    return;
    
  for(int i = 0; i < se->count(); ++i) {
    SpanExpr *sub = se->item(i);
    
    colorize_spanexpr(sub);
  }
  
  int end;
  SyntaxGlyphStyle style;
  if(prepare_symbol_colorization(se->item_pos(1), SymbolKind::Parameter, end, style)) {
    painter.move_to(se->item_pos(0));
    painter.paint_until(GlyphStyleParameter, end);
  }
}

void ScopeColorizerImpl::colorize_assignment_or_rule(SpanExpr *se, bool delayed) {
  // lhs:=
  // lhs:=rhs
  // lhs->
  // lhs->rhs
  RICHMATH_ASSERT(se->count() == 2 || se->count() == 3);
  
  SharedPtr<ScopePos> next_scope = state.new_scope();
  state.new_scope();
  
  bool old_in_pattern = state.in_pattern;
  state.in_pattern = true;
  
  colorize_spanexpr(se->item(0));
  
  state.in_pattern = old_in_pattern;
  
  if(!delayed)
    state.current_pos = next_scope;
    
  if(se->count() == 3) {
    colorize_spanexpr(se->item(2));
  }
  else {
    painter.move_to(se->item_pos(1));
    painter.paint_until(GlyphStyleSyntaxError, se->item(1)->end() + 1);
  }
  
  if(delayed)
    state.current_pos = next_scope;
}

void ScopeColorizerImpl::colorize_tag_assignment(SpanExpr * se, bool delayed) { // x/:y:=z   x/:y::=z
  RICHMATH_ASSERT(se->count() >= 4 && se->count() <= 5);
  
  colorize_spanexpr(se->item(0));
  
  SharedPtr<ScopePos> next_scope = state.new_scope();
  state.new_scope();
  
  bool old_in_pattern = state.in_pattern;
  state.in_pattern = true;
  
  colorize_spanexpr(se->item(2));
  
  state.in_pattern = old_in_pattern;
  
  if(!delayed)
    state.current_pos = next_scope;
    
  if(se->count() == 5) {
    colorize_spanexpr(se->item(4));
  }
  else {
    painter.move_to(se->item_pos(3));
    painter.paint_until(GlyphStyleSyntaxError, se->item(3)->end() + 1);
  }
  
  if(delayed)
    state.current_pos = next_scope;
}

bool ScopeColorizerImpl::colorize_integral(SpanExpr *se) { // \[Integral] ... \[DifferentialD]...
  RICHMATH_ASSERT(se->count() >= 2);
  
  bool have_integral = false;
  if(pmath_char_is_integral(se->item_as_char(0))) {
    have_integral = true;
  }
  else {
    auto uo = dynamic_cast<UnderoverscriptBox*>(se->item_as_box(0));
    if( uo &&
        uo->base()->length() == 1 &&
        pmath_char_is_integral(uo->base()->text()[0]))
    {
      have_integral = true;
    }
  }
  
  if(!have_integral)
    return false;
    
  SpanExpr *integrand = se->item(se->count() - 1);
  
  if(integrand && integrand->count() >= 1) {
    SpanExpr *dx = integrand->item(integrand->count() - 1);
    
    if( dx                                           &&
        dx->count() == 2                             &&
        dx->item_as_char(0) == PMATH_CHAR_INTEGRAL_D &&
        pmath_char_is_name(dx->item_first_char(1)))
    {
      for(int i = 0; i < se->count() - 1; ++i)
        colorize_spanexpr(se->item(i));
        
      SharedPtr<ScopePos> next_scope = state.new_scope();
      state.new_scope();
      
      symbol_colorize(dx->item_pos(1), SymbolKind::Special);
      
      for(int i = 0; i < integrand->count() - 1; ++i)
        colorize_spanexpr(integrand->item(i));
        
      state.current_pos = next_scope;
      return true;
    }
  }
  
  return false;
}

bool ScopeColorizerImpl::colorize_bigop(SpanExpr *se) {
  RICHMATH_ASSERT(se->count() >= 2);
  
  MathSequence *bigop_init = nullptr;
  int next_item = 1;
  if(pmath_char_maybe_bigop(se->item_as_char(0))) {
    if(auto subsup = dynamic_cast<SubsuperscriptBox *>(se->item_as_box(1))) {
      bigop_init = subsup->subscript();
      ++next_item;
    }
  }
  else {
    auto uo = dynamic_cast<UnderoverscriptBox*>(se->item_as_box(0));
    if( uo &&
        uo->base()->length() == 1 &&
        pmath_char_maybe_bigop(uo->base()->text()[0]))
    {
      bigop_init = uo->underscript();
    }
  }
  
  if( bigop_init                 &&
      bigop_init->length() > 0   &&
      bigop_init->span_array()[0])
  {
    SpanExpr *init = new SpanExpr(0, bigop_init->span_array()[0], bigop_init);
    
    if( init->end() + 1 == bigop_init->length()     &&
        init->count() >= 2                          &&
        init->item_as_char(1) == '='                &&
        pmath_char_is_name(init->item_first_char(0)))
    {
      colorize_spanexpr(se->item(next_item - 1));
      
      SharedPtr<ScopePos> next_scope = state.new_scope();
      state.new_scope();
      
      ScopeColorizerImpl(*bigop_init, state).symbol_colorize(init->item_pos(0), SymbolKind::Special);
      
      for(int i = next_item; i < se->count(); ++i)
        colorize_spanexpr(se->item(i));
        
      state.current_pos = next_scope;
      delete init;
      return true;
    }
    
    delete init;
  }
  
  return false;
}

void ScopeColorizerImpl::colorize_localspec_call(SpanExpr *se, const SyntaxInformation &info) {
  /* This currently ignores info.locals_min and info.locals_max. */
  FunctionCallSpan call = se;
  const int arg_count = call.function_argument_count();
  
  if(arg_count < 1)
    return;
    
  colorize_spanexpr(call.function_argument(1));
  
  SharedPtr<ScopePos> next_scope = state.new_scope();
  state.new_scope();
  
  symdeflist_colorize_spanexpr(call.function_argument(1), SymbolKind::LocalSymbol);
  
  if(arg_count >= 2) {
    colorize_spanexpr(call.function_argument(2));
  }
  
  state.current_pos = next_scope;
  
  for(int i = 3; i <= arg_count; ++i)
    colorize_spanexpr(call.function_argument(i));
}

void ScopeColorizerImpl::colorize_functionspec_call(SpanExpr *se, const SyntaxInformation &info) {
  FunctionCallSpan call = se;
  const int arg_count = call.function_argument_count();
  
  if(arg_count < 1)
    return;
    
  if(arg_count == 1) {
    bool old_in_function = state.in_function;
    state.in_function = true;
    
    colorize_spanexpr(call.function_argument(1));
    
    state.in_function = old_in_function;
    return;
  }
  
  colorize_spanexpr(call.function_argument(1));
  
  SharedPtr<ScopePos> next_scope = state.new_scope();
  state.new_scope();
  
  if(pmath_char_is_name(call.function_argument(1)->first_char()))
    symbol_colorize(call.function_argument(1)->start(), SymbolKind::Parameter);
  else
    symdeflist_colorize_spanexpr(call.function_argument(1), SymbolKind::Parameter);
    
  colorize_spanexpr(call.function_argument(2));
  
  state.current_pos = next_scope;
  
  for(int i = 3; i < arg_count; ++i)
    colorize_spanexpr(call.function_argument(i));
}

void ScopeColorizerImpl::colorize_tablespec_call(SpanExpr *se, const SyntaxInformation &info) {
  FunctionCallSpan call = se;
  const int arg_count = call.function_argument_count();
  
  if(arg_count < 1)
    return;
    
  SharedPtr<ScopePos> next_scope = state.new_scope();
  state.new_scope();
  
  for(int i = info.locals_min; i <= info.locals_max && i <= arg_count; ++i) {
    replacement_colorize_spanexpr(call.function_argument(i), SymbolKind::Special);
    
    for(int j = i + 1; j <= info.locals_max && j <= arg_count; ++j) {
      SpanExpr *arg_j = call.function_argument(j);
      
      painter.move_to(arg_j->start());
      painter.paint_until(GlyphStyleNone, arg_j->end());
        
      colorize_spanexpr(arg_j);
    }
  }
  
  for(int i = 1; i < info.locals_min && i <= arg_count; ++i)
    colorize_spanexpr(call.function_argument(i));
    
  for(int i = info.locals_max; i < arg_count; ++i)
    colorize_spanexpr(call.function_argument(i + 1));
    
  state.current_pos = next_scope;
}

void ScopeColorizerImpl::colorize_simple_call(SpanExpr *head_name, SpanExpr *se) {
  RICHMATH_ASSERT(se->count() >= 3 && se->item_as_char(1) == '(');
  
  String name = head_name->as_text();
  SyntaxInformation info(name);
  
  colorize_spanexpr(se->item(0));
  
  switch(info.locals_form) {
    case LocalVariableForm::LocalSpec:
      colorize_localspec_call(se, info);
      return;
      
    case LocalVariableForm::FunctionSpec:
      colorize_functionspec_call(se, info);
      return;
      
    case LocalVariableForm::TableSpec:
      colorize_tablespec_call(se, info);
      return;
      
    case LocalVariableForm::NoSpec:
      break;
  }
  
  for(int i = 1; i < se->count(); ++i) {
    SpanExpr *sub = se->item(i);
    
    colorize_spanexpr(sub);
  }
}

void ScopeColorizerImpl::colorize_block_body_line(SpanExpr *se, SharedPtr<ScopePos> &scope_after_block) {
  while(se->count() == 1)
    se = se->item(0);
    
  if(FunctionCallSpan::is_simple_call(se)) {
    FunctionCallSpan call(se);
    if(SpanExpr *name = span_as_name(call.function_head())) {
      if(name->equals("Local") || name->equals("With")) {
        colorize_scoping_block_head(call, scope_after_block, &ScopeColorizerImpl::symdef_local_colorize_spanexpr);
        return;
      }
    }
  }
  
  if(se->count() < 2) {
    colorize_spanexpr(se);
    return;
  }
  
  uint16_t firstchar = se->item_as_char(0);
  uint16_t secondchar = se->item_as_char(1);
  if( firstchar  == ';' || firstchar  == '\n' || firstchar  == ',' ||
      secondchar == ';' || secondchar == '\n' || secondchar == ',')
  {
    for(int i = 0; i < se->count(); ++i) {
      colorize_block_body_line(se->item(i), scope_after_block);
    }
    
    return;
  }
  
  colorize_spanexpr(se);
}

void ScopeColorizerImpl::colorize_block_body(SpanExpr *se) {
  if(se->count() < 2 || !FunctionCallSpan::is_list(se)) {
    colorize_spanexpr(se);
    return;
  }
  
  SharedPtr<ScopePos> next_scope;
  colorize_block_body_line(se->item(1), next_scope);
  if(next_scope)
    state.current_pos = next_scope;
}

void ScopeColorizerImpl::symdef_local_colorize_spanexpr(SpanExpr *se) {
  symdef_colorize_spanexpr(se, SymbolKind::LocalSymbol);
}

void ScopeColorizerImpl::symdef_parameter_colorize_spanexpr(SpanExpr *se) {
  symdef_colorize_spanexpr(se, SymbolKind::Parameter);
}

void ScopeColorizerImpl::replacement_special_colorize_spanexpr(SpanExpr *se) {
  replacement_colorize_spanexpr(se, SymbolKind::Special);
}

void ScopeColorizerImpl::colorize_scoping_block_head(FunctionCallSpan &head, SharedPtr<ScopePos> &scope_after_block, void (ScopeColorizerImpl::*colorize_def)(SpanExpr*)) {
  for(int i = 0; i < head.span()->count(); ++i) {
    colorize_spanexpr(head.span()->item(i));
  }
  colorize_keyword(head.function_head());
  
  if(!scope_after_block)
    scope_after_block = state.new_scope();
    
  state.new_scope();
  
  const int head_arg_count = head.function_argument_count();
  for(int i = 1; i <= head_arg_count; ++i)
    (this->*colorize_def)(head.function_argument(i));
}

void ScopeColorizerImpl::colorize_scoping_block(FunctionCallSpan &head, SpanExpr *se, void (ScopeColorizerImpl::*colorize_def)(SpanExpr*)) {
  RICHMATH_ASSERT(se->count() == 2);
  
  SharedPtr<ScopePos> next_scope;
  colorize_scoping_block_head(head, next_scope, colorize_def);
  RICHMATH_ASSERT(next_scope);
  
  colorize_block_body(se->item(1));
  
  state.current_pos = next_scope;
}

void ScopeColorizerImpl::colorize_if_blocks(FunctionCallSpan &head, SpanExpr *se) {
  colorize_spanexpr(head.span());
  colorize_keyword(head.function_head());
  
  for(int i = 1; i < se->count(); ++i) {
    SpanExpr *item = se->item(i);
    
    SpanExpr *name = span_as_name(item);
    if(name && name->equals("Else")) {
      colorize_keyword(name);
      continue;
    }
    
    if(FunctionCallSpan::is_simple_call(item)) {
      FunctionCallSpan else_if(item);
      name = span_as_name(else_if.function_head());
      if(name && name->equals("If")) {
        colorize_spanexpr(item);
        colorize_keyword(name);
        continue;
      }
    }
    
    colorize_block_body(item);
  }
}

void ScopeColorizerImpl::colorize_block(SpanExpr *se) {
  RICHMATH_ASSERT(se->count() >= 2);
  
  if(FunctionCallSpan::is_simple_call(se->item(0))) {
    FunctionCallSpan head_call(se->item(0));
    if(SpanExpr *name = span_as_name(head_call.function_head())) {
      if(se->count() == 2) {
        if(name->equals("Local") || name->equals("With")) {
          colorize_scoping_block(head_call, se, &ScopeColorizerImpl::symdef_local_colorize_spanexpr);
          return;
        }
        if(name->equals("Do")) {
          colorize_scoping_block(head_call, se, &ScopeColorizerImpl::replacement_special_colorize_spanexpr);
          return;
        }
        if(name->equals("Function")) {
          // Function(args) { ... }
          colorize_scoping_block(head_call, se, &ScopeColorizerImpl::symdef_parameter_colorize_spanexpr);
          return;
        }
        if(name->equals("While") || name->equals("Switch") || name->equals("If")) {
          colorize_spanexpr(head_call.span());
          colorize_keyword(name);
          colorize_block_body(se->item(1));
          return;
        }
        if(name->equals("Case")) {
          // Case(...) { ... }  is syntactic sugar for  ... :> Block { ... }  
          // TODO: Case is only valid directly inside a Switch(...) {...} block
          
          SharedPtr<ScopePos> next_scope = state.new_scope();
          state.new_scope();
          
          bool old_in_pattern = state.in_pattern;
          state.in_pattern = true;
          
          colorize_spanexpr(head_call.span()); // Case(...)
          colorize_keyword(name);
          
          state.in_pattern = old_in_pattern;
          
          colorize_block_body(se->item(1));
          state.current_pos = next_scope;
          return;
        }
      }
      
      if(name->equals("If")) {
        colorize_if_blocks(head_call, se);
        return;
      }
    }
  }
  
  SpanExpr *name = span_as_name(se->item(0));
  if(name && name->equals("Function")) {
    if(se->count() == 2) {
      // Function { ... }
      colorize_keyword(name);
      
      bool old_in_function = state.in_function;
      state.in_function = true;
      
      colorize_block_body(se->item(1)); // {...}
      
      state.in_function = old_in_function;
      return;
    }
    
    if(se->count() >= 3) {
      // Function name(...) {...}  is syntactic suggar for  name(...)::=...
      // Function name(...) Where(...) {...} is abbrev. of  name(...) /? ... ::= ...
      colorize_keyword(name);
      
      SharedPtr<ScopePos> next_scope = state.new_scope();
      state.new_scope();
      
      bool old_in_pattern = state.in_pattern;
      state.in_pattern = true;
      
      colorize_spanexpr(se->item(1)); // name(...)
      
      state.in_pattern = old_in_pattern;
      
      for(int i = 2; i < se->count() - 1; ++i) {
        SpanExpr *item = se->item(i);
        colorize_spanexpr(item); // Where(...)
        
        if(FunctionCallSpan::is_simple_call(item)) {
          FunctionCallSpan more_call(item);
          SpanExpr *more_name = span_as_name(more_call.function_head());
          if(more_name && more_name->equals("Where"))
            colorize_keyword(more_name);
        }
      }
      
      colorize_block_body(se->item(se->count() - 1)); // {...}
      
      state.current_pos = next_scope;
      return;
    }
  }
  
  if(name && se->count() == 2 && name->equals("Block")) { // Block { ... }
    colorize_keyword(name);
    colorize_block_body(se->item(1));
    return;
  }
  
  if(name && se->count() == 4 && name->equals("Try")) { // Try { ... } Finally { ... }
    SpanExpr *finally = span_as_name(se->item(2));
    if(finally && finally->equals("Finally")) {
      colorize_keyword(name);
      colorize_block_body(se->item(1));
      colorize_keyword(finally);
      colorize_block_body(se->item(3));
      return;
    }
  }
  
  for(int i = 0; i < se->count(); ++i) {
    colorize_block_body(se->item(i));
  }
}

//} ... class ScopeColorizerImpl

//{ class ErrorColorizerImpl ...

inline ErrorColorizerImpl::ErrorColorizerImpl(MathSequence &sequence, float _error_indicator_height)
  : painter(sequence),
    error_indicator_height(_error_indicator_height)
{
}

void ErrorColorizerImpl::arglist_errors_colorize_spanexpr(SpanExpr *se) {
  if(BlockSpan::maybe_block(se)) {
    colorize_block_errors(se);
    return;
  }
  
  arglist_errors_colorize_spanexpr_norecurse(se);
  
  for(int i = 0; i < se->count(); ++i)
    arglist_errors_colorize_spanexpr(se->item(i));
}

void ErrorColorizerImpl::unknown_option_colorize_spanexpr(SpanExpr *se, Expr options) {
  FunctionCallSpan call(se);
  
  if(call.is_list()) {
    for(int i = call.list_length(); i >= 1; --i)
      unknown_option_colorize_spanexpr(call.list_element(i), options);
      
    return;
  }
  
  if(se->count() == 3) {
    SpanExpr *name_span = span_as_name(se->item(0));
    if(!name_span)
      return;
      
    if( se->item_as_char(1) == PMATH_CHAR_RULE        ||
        se->item_as_char(1) == PMATH_CHAR_RULEDELAYED ||
        se->item_as_text(1).equals("->")              ||
        se->item_as_text(1).equals(":>"))
    {
      String          name = name_span->as_text();
      const uint16_t *buf  = name.buffer();
      int             len  = name.length();
      
      for(size_t i = options.expr_length(); i > 0; --i) {
        Expr rule = options[i];
        Expr lhs  = rule[1];
        
        if(!lhs.is_symbol())
          continue;
          
        String          lhs_name(pmath_symbol_name(lhs.get()));
        const uint16_t *lhs_buf = lhs_name.buffer();
        int             lhs_len = lhs_name.length();
        
        if(lhs_len < len)
          continue;
          
        if(0 != memcmp(buf, lhs_buf + lhs_len - len, len * sizeof(uint16_t)))
          continue;
          
        if(lhs_len == len || lhs_buf[lhs_len - len - 1] == '`')
          return;
      }
      
      painter.move_to(se->item(0)->start());
      painter.paint_until(GlyphStyleInvalidOption, se->item(0)->end() + 1);
    }
  }
}

void ErrorColorizerImpl::add_missing_indicator(SpanExpr *span_before) {
  int idx = span_before->end();
  painter.move_to(idx);
  auto style = painter.get_style();
  painter.paint_until(painter.get_style().with_missing_after(true), idx + 1);
}

void ErrorColorizerImpl::mark_excess_args(FunctionCallSpan &call, int max_args) {
  if(max_args == 0) {
    if(call.is_dot_call() || call.is_pipe_call()) {
      painter.move_to(call.function_argument(1)->start());
      painter.paint_until(GlyphStyleExcessOrMissingArg, call.function_argument(1)->end() + 1);
    }
    
    painter.move_to(call.arguments_span()->start());
  }
  else if(max_args == 1 && call.is_dot_call()) {
    painter.move_to(call.arguments_span()->start());
  }
  else if(max_args == 1 && call.is_pipe_call()) {
    painter.move_to(call.arguments_span()->start());
  }
  else {
    painter.move_to(call.function_argument(max_args)->end() + 1);
  }
  
  painter.paint_until(GlyphStyleExcessOrMissingArg, call.arguments_span()->end() + 1);
  return;
}

void ErrorColorizerImpl::colorize_name(SpanExpr *se, SyntaxGlyphStyle style) {
  if(!se)
    return;
    
  painter.move_to(se->start());
  painter.paint_until(style, se->end() + 1);
}

void ErrorColorizerImpl::arglist_errors_colorize_spanexpr_norecurse(SpanExpr *se) {
  if(!FunctionCallSpan::is_call(se))
    return;
    
  FunctionCallSpan  call      = se;
  SpanExpr         *head_name = span_as_name(call.function_head());
  
  if(!head_name)
    return;
  
  painter.move_to(head_name->start());
  if(painter.get_style().kind() != GlyphStyleNone)
    return;
    
  String name = head_name->as_text();
  SyntaxInformation info(name);
  
  if(info.is_keyword)
    colorize_name(head_name, GlyphStyleKeyword);
  else
    colorize_name(head_name, GlyphStyleFunctionCall);
    
  if(info.minargs == 0 && info.maxargs == INT_MAX)
    return;
    
  int arg_count = call.function_argument_count();
  
  if(arg_count > info.maxargs) {
    Expr options = Application::interrupt_wait_cached(
                     Call(Symbol(richmath_System_Options), name));
                     
    if( options.expr_length() == 0 ||
        !options.item_equals(0, richmath_System_List))
    {
      mark_excess_args(call, info.maxargs);
      return;
    }
    
    for(int i = info.maxargs + 1; i <= arg_count; ++i)
      unknown_option_colorize_spanexpr(call.function_argument(i), options);
  }
  
  if(arg_count < info.minargs) {
    add_missing_indicator(call.arguments_span());
  }
}

void ErrorColorizerImpl::get_block_head_argument_counts(SpanExpr *name, int &argmin, int &argmax) {
  argmin = 0;
  argmax = INT_MAX;
  
  if( name->equals("If") ||
      name->equals("While"))
  {
    argmin = 1;
    argmax = 1;
    return;
  }
  
  if( name->equals("Switch") ||
      name->equals("Case"))
  {
    argmin = 1;
    argmax = INT_MAX;
    return;
  }
  
  if( name->equals("Do") ||
      name->equals("With") ||
      name->equals("Local"))
  {
    argmin = 1;
    return;
  }
  
  if( name->equals("Block") ||
      name->equals("Try") ||
      name->equals("Finally"))
  {
    argmax = 0;
  }
}

void ErrorColorizerImpl::colorize_block_body_line_errors(SpanExpr *se) {
  while(se->count() == 1)
    se = se->item(0);
    
  if(FunctionCallSpan::is_simple_call(se)) {
    FunctionCallSpan call(se);
    if(SpanExpr *name = span_as_name(call.function_head())) {
      if(name->equals("Local") || name->equals("With")) {
        colorize_block_body_errors(se);
        return;
      }
    }
  }
  
  if(se->count() < 2) {
    arglist_errors_colorize_spanexpr(se);
    return;
  }
  
  uint16_t firstchar = se->item_as_char(0);
  uint16_t secondchar = se->item_as_char(1);
  if( firstchar  == ';' || firstchar  == '\n' || firstchar  == ',' ||
      secondchar == ';' || secondchar == '\n' || secondchar == ',')
  {
    for(int i = 0; i < se->count(); ++i) {
      colorize_block_body_line_errors(se->item(i));
    }
    
    return;
  }
  
  arglist_errors_colorize_spanexpr(se);
}

void ErrorColorizerImpl::colorize_block_body_errors(SpanExpr *se) {
  while(se->count() == 1)
    se = se->item(0);
    
  if(FunctionCallSpan::is_simple_call(se)) {
    FunctionCallSpan call(se);
    if(SpanExpr *name = span_as_name(call.function_head())) {
      int arg_count = call.function_argument_count();
      int argmin, argmax;
      
      get_block_head_argument_counts(name, argmin, argmax);
      if(argmax > arg_count)
        argmax = arg_count;
        
      for(int i = 1; i <= argmax; ++i) {
        arglist_errors_colorize_spanexpr(call.function_argument(i));
      }
      
      if(arg_count > argmax)
        mark_excess_args(call, argmax);
      else if(arg_count < argmin)
        add_missing_indicator(call.arguments_span());
        
      return;
    }
  }
  
  if(se->as_token() == PMATH_TOK_NAME) {
    int argmin, argmax;
    get_block_head_argument_counts(se, argmin, argmax);
    if(argmin > 0) {
      add_missing_indicator(se);
    }
    
    return;
  }
  
  if(se->count() >= 2 && FunctionCallSpan::is_list(se)) {
    colorize_block_body_line_errors(se->item(1));
    return;
  }
  
  arglist_errors_colorize_spanexpr(se);
}

void ErrorColorizerImpl::colorize_block_errors(SpanExpr *se) {
  RICHMATH_ASSERT(se->count() >= 2);
  
  for(int i = 0; i < se->count(); ++i)
    colorize_block_body_errors(se->item(i));
}

//} ... class ErrorColorizerImpl

//{ class SyntaxColorizerImpl ...

SyntaxColorizerImpl::SyntaxColorizerImpl(MathSequence &seq)
  : painter(seq),
    spans(seq.span_array()),
    text(buffer_view(seq.text()))
{
}

void SyntaxColorizerImpl::colorize_spanexpr(SpanExpr *se) {
  colorize_quoted_string(se);
  if(colorize_single_token(se))
    return;
  
  if( se->count() == 3 &&
      se->item_as_text(1).equals("::") &&
      se->item(2)->count() == 0)
  {
    painter.move_to(se->item_pos(2));
    painter.paint_until(GlyphStyleString, se->end() + 1);
      
    colorize_spanexpr(se->item(0));
    return;
  }
  
  for(int i = 0; i < se->count(); ++i)
    colorize_spanexpr(se->item(i));
}

void SyntaxColorizerImpl::colorize_quoted_string(SpanExpr *se) {
  if(se->first_char() != '"')
    return;
    
  if(se->count() != 0 && se->item_pos(0) == se->start())
    return;
    
  auto text = buffer_view(se->sequence()->text());
  painter.move_to(1 + se->start());
  for(int i = 1 + se->start(); i < se->end();) {
    if(text[i] == '\\') {
      painter.paint_until(GlyphStyleString, i);
      
      auto rest_text = text.part(i);
      uint32_t encoded_char;
      const uint16_t *next = pmath_char_parse(rest_text.items(), rest_text.length(), &encoded_char);
      
      i += next - rest_text.items();
      painter.paint_until(GlyphStyleSpecialStringPart, i);
    }
    else
      ++i;
  }
  
  if(text[se->end()] == '"')
    painter.paint_until(GlyphStyleString, se->end());
  else
    painter.paint_until(GlyphStyleString, se->end()+1);
}

bool SyntaxColorizerImpl::colorize_single_token(SpanExpr *se) {
  if(se->count() > 0)
    return false;
  
  uint16_t ch = se->as_char();
  if(pmath_char_is_left(ch)) {
    SpanExpr *parent = se->parent();
    
    if( !parent ||
        (!pmath_char_is_right(parent->item_as_char(parent->count() - 1)) &&
        !Tokenizer::is_right_bracket(parent->item_as_syntax_form(parent->count() - 1))))
    {
      if(pmath_right_fence(ch) != 0) {
        painter.move_to(se->start());
        painter.paint_until(GlyphStyleSyntaxError, se->start() + 1);
        return true;
      }
    }
    
    return true;
  }
  
  if(pmath_char_is_right(ch)) {
    SpanExpr *parent = se->parent();
    
    if(!parent) {
      painter.move_to(se->start());
      painter.paint_until(GlyphStyleSyntaxError, se->start() + 1);
      return true;
    }
    
    for(int i = parent->count() - 1; i >= 0; --i) {
      if(pmath_right_fence(parent->item_as_char(i)) == ch)
        return true;
      
      String item_syntax_form = parent->item_as_syntax_form(i);
      if(item_syntax_form.length() == 1) {
        if(pmath_right_fence(item_syntax_form.buffer()[0]) == ch)
          return true;
      }
    }
    
    painter.move_to(se->start());
    painter.paint_until(GlyphStyleSyntaxError, se->start() + 1);
    return true;
  }
  
  return true;
}

void SyntaxColorizerImpl::comments_colorize_span(Span span) {
  if(!span) {
    int next_token = painter.index();
    while(next_token < text.length() && !spans.is_token_end(next_token)) {
      ++next_token;
    }
    ++next_token;

    if(is_comment_start_at(text.part(painter.index())))
      painter.paint_until(GlyphStyleComment, next_token);
    else 
      painter.move_to(next_token);
    
    return;
  }
  
  if(!span.next()) {
    if(is_comment_start_at(text.part(painter.index()))) {
      painter.paint_until(GlyphStyleComment, span.end() + 1);
      return;
    }
  }
  
  comments_colorize_span(span.next());
  comments_colorize_until(span.end() + 1);
}

void SyntaxColorizerImpl::comments_colorize_until(int end) {
  while(painter.index() < end)
    comments_colorize_span(spans[painter.index()]);
}

//} ... class SyntaxColorizerImpl

//{ class Painter ...

inline Painter::Painter(MathSequence &seq)
  : _seq_debug(seq), 
    _iter(seq.semantic_styles_array().find(0)) 
{
}

void Painter::paint_until(SyntaxGlyphStyle style, int next_index) {
  int length = next_index - index();
  if(length <= 0)
    return;
  
  _iter.reset_range(style, length);
  _iter+= length;
}

void Painter::move_to(int next_index) {
//  if(next_index < index()) {
//    pmath_debug_print("[Painter::move_to needs to rewind from %d to %d", index(), next_index);
//    pmath_debug_print_object(" in :", _seq_debug.text().get(), "]\n");
//  }
  
  _iter.rewind_to(next_index);
}

//} ... class Painter
