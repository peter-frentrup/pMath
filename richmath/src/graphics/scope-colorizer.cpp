#include <graphics/scope-colorizer.h>

#include <util/spanexpr.h>

#include <boxes/mathsequence.h>
#include <boxes/subsuperscriptbox.h>
#include <boxes/underoverscriptbox.h>

#include <eval/application.h>
#include <eval/binding.h>

#include <climits>


using namespace richmath;

//{ class ScopeColorizer ...

ScopeColorizer::ScopeColorizer(MathSequence *_sequence)
  : Base(),
  sequence(_sequence)
{
}

int ScopeColorizer::symbol_colorize(
  SyntaxState *state,
  int          start,
  SymbolKind   kind
) {
  const Array<GlyphInfo> &glyphs = sequence->glyph_array();
  const SpanArray        &spans  = sequence->span_array();
  const String           &str    = sequence->text();
  
  int end = start;
  while(end < glyphs.length() && !spans.is_token_end(end))
    ++end;
  ++end;
  
  if(sequence->is_placeholder(start) && start + 1 == end)
    return end;
    
  if( glyphs[start].style != GlyphStyleNone      &&
      glyphs[start].style != GlyphStyleParameter &&
      glyphs[start].style != GylphStyleLocal     &&
      glyphs[start].style != GlyphStyleNewSymbol)
  {
    return end;
  }
  
  String name(str.part(start, end - start));
  
  SharedPtr<SymbolInfo> info = state->local_symbols[name];
  uint8_t style = GlyphStyleNone;
  
  if(kind == Global) {
    while(info) {
      if(info->pos->contains(state->current_pos)) {
        switch(info->kind) {
          case LocalSymbol:  style = GylphStyleLocal;      break;
          case Special:      style = GlyphStyleSpecialUse; break;
          case Parameter:    style = GlyphStyleParameter;  break;
          case Error:        style = GylphStyleScopeError; break;
          default: ;
        }
        
        break;
      }
      
      info = info->next;
    };
    
    if(!info) {
      Expr syminfo = Application::interrupt_cached(Call(
                       GetSymbol(SymbolInfoSymbol),
                       name));
                       
      if(syminfo == PMATH_SYMBOL_FALSE)
        style = GlyphStyleNewSymbol;
      else if(syminfo == PMATH_SYMBOL_ALTERNATIVES)
        style = GlyphStyleShadowError;
      else if(syminfo == PMATH_SYMBOL_SYNTAX)
        style = GlyphStyleSyntaxError;
    }
  }
  else if(info) {
    SharedPtr<SymbolInfo> si = info;
    
    do {
      if(info->pos.ptr() == state->current_pos.ptr()) {
        if(info->kind == kind)
          break;
          
        if(info->kind == LocalSymbol && kind == Special) {
          si->add(kind, state->current_pos);
          break;
        }
        
        if(kind == Special) {
          kind = info->kind;
          break;
        }
        
        si->add(Error, state->current_pos);
        style = GylphStyleScopeError;
        break;
      }
      
      if(info->pos->contains(state->current_pos)) {
        if(info->kind == LocalSymbol && kind != LocalSymbol/* && kind == Special*/) {
          si->add(kind, state->current_pos);
          break;
        }
        
        if(kind == Special) {
          kind = info->kind;
          break;
        }
        
        si->add(Error, state->current_pos);
        style = GylphStyleScopeError;
        break;
      }
      
      info = info->next;
    } while(info);
    
    if(!info) {
      si->kind = kind;
      si->pos = state->current_pos;
      si->next = 0;
    }
  }
  else
    state->local_symbols.set(name, new SymbolInfo(kind, state->current_pos));
    
  if(!style) {
    switch(kind) {
      case LocalSymbol:  style = GylphStyleLocal;       break;
      case Special:      style = GlyphStyleSpecialUse; break;
      case Parameter:    style = GlyphStyleParameter;   break;
      default:           return end;
    }
  }
  
  for(int i = start; i < end; ++i)
    glyphs[i].style = style;
    
//  if(style == GlyphStyleParameter)
//    for(int i = start;i < end;++i)
//      glyphs[i].slant = FontSlantItalic;

  return end;
}

void ScopeColorizer::symdef_colorize_spanexpr(
  SyntaxState *state,
  SpanExpr    *se,    // "x"  "x:=y"
  SymbolKind   kind
) {
  if( se->count() >= 2 &&
      se->count() <= 3 &&
      pmath_char_is_name(se->item_first_char(0)))
  {
    if(se->item_as_char(1) == PMATH_CHAR_ASSIGN        ||
        se->item_as_char(1) == PMATH_CHAR_ASSIGNDELAYED ||
        se->item_equals(1, ":=")                        ||
        se->item_equals(1, "::="))
    {
      symbol_colorize(state, se->item_pos(0), kind);
      return;
    }
  }
  
  symbol_colorize(state, se->start(), kind);
}

void ScopeColorizer::symdeflist_colorize_spanexpr(
  SyntaxState *state,
  SpanExpr    *se,    // "{symdefs ...}"
  SymbolKind   kind
) {
  if(se->count() < 2 || se->count() > 3 || se->item_as_char(0) != '{')
    return;
    
  se = se->item(1);
  if(se->count() >= 2 && se->item_as_char(1) == ',') {
    for(int i = 0; i < se->count(); ++i)
      symdef_colorize_spanexpr(state, se->item(i), kind);
  }
  else
    symdef_colorize_spanexpr(state, se, kind);
}

void ScopeColorizer::replacement_colorize_spanexpr(
  SyntaxState *state,
  SpanExpr    *se,    // "x->value"
  SymbolKind   kind
) {
  if(se->count() >= 2
      && (se->item_as_char(1) == PMATH_CHAR_RULE
          || se->item_equals(1, "->"))
      && pmath_char_is_name(se->item_first_char(0))) {
    symbol_colorize(state, se->item_pos(0), kind);
  }
}

void ScopeColorizer::scope_colorize_spanexpr(
  SyntaxState *state,
  SpanExpr    *se
) {
  const Array<GlyphInfo> &glyphs = sequence->glyph_array();
  const String           &str    = sequence->text();
  
  assert(se != 0);
  
  if(se->count() == 0) { // identifiers   #   ~
    if(glyphs[se->start()].style)
      return;
      
    if(se->is_box()) {
      se->as_box()->colorize_scope(state);
      return;
    }
    
    if(pmath_char_is_name(se->first_char())) {
      symbol_colorize(state, se->start(), Global);
      return;
    }
    
    if(se->first_char() == '#') {
      uint8_t style;
      if(state->in_function)
        style = GlyphStyleParameter;
      else
        style = GylphStyleScopeError;
        
      for(int i = se->start(); i <= se->end(); ++i)
        glyphs[i].style = style;
        
      return;
    }
    
    if(se->first_char() == '~' && state->in_pattern) {
      for(int i = se->start(); i <= se->end(); ++i)
        glyphs[i].style = GlyphStyleParameter;
        
      return;
    }
  }
  
  if(se->count() == 2) { // #x   ~x   ?x   x&   <<x
  
    if( se->item_first_char(0) == '#' &&
        pmath_char_is_digit(se->item_first_char(1)))
    {
      uint8_t style;
      if(state->in_function)
        style = GlyphStyleParameter;
      else
        style = GylphStyleScopeError;
        
      for(int i = se->start(); i <= se->end(); ++i)
        glyphs[i].style = style;
        
      return;
    }
    
    if((se->item_first_char(0) == '~' ||
        se->item_as_char(0)    == '?') &&
        pmath_char_is_name(se->item_first_char(1)))
    {
      if(!state->in_pattern)
        return;
        
      symbol_colorize(state, se->item_pos(1), Parameter);
      
      for(int i = se->item_pos(0); i < se->item_pos(1); ++i)
        glyphs[i].style = glyphs[se->item_pos(1)].style;
        
      return;
    }
    
    if(se->item_as_char(1) == '&') {
      bool old_in_function = state->in_function;
      state->in_function = true;
      
      scope_colorize_spanexpr(state, se->item(0));
      
      state->in_function = old_in_function;
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
        for(int i = se->start(); i <= se->end(); ++i)
          glyphs[i].style = GlyphStyleParameter;
          
        return;
      }
      
      if(pmath_char_is_name(se->item_first_char(0))) {
        if(!state->in_pattern)
          return;
          
        for(int i = 0; i < se->count(); ++i) {
          SpanExpr *sub = se->item(i);
          
          scope_colorize_spanexpr(state, sub);
        }
        
        symbol_colorize(state, se->item_pos(0), Parameter);
        return;
      }
    }
    
    if(se->item_as_char(1) == 0x21A6) { // mapsto
      SharedPtr<ScopePos> next_scope = state->new_scope();
      state->new_scope();
      
      SpanExpr *sub = se->item(0);
      scope_colorize_spanexpr(state, sub);
      
      if( sub->count() == 3 &&
          sub->item_as_char(0) == '(' &&
          sub->item_as_char(2) == ')')
      {
        sub = sub->item(1);
      }
      
      if( sub->count() == 0 &&
          pmath_char_is_name(sub->first_char()))
      {
        symbol_colorize(state, sub->start(), Parameter);
      }
      else if(sub->count() > 0 &&
              sub->item_as_char(1) == ',')
      {
        const uint16_t *buf = str.buffer();
        
        for(int i = 0; i < sub->count(); ++i)
          if(pmath_char_is_name(buf[sub->item_pos(i)]))
            symbol_colorize(state, sub->item_pos(i), Parameter);
      }
      
      if(se->count() == 3) {
        scope_colorize_spanexpr(state, se->item(2));
      }
      else {
        for(int i = se->item_pos(1); i <= se->item(1)->end(); ++i)
          glyphs[i].style = GlyphStyleSyntaxError;
      }
      
      state->current_pos = next_scope;
      return;
    }
    
    if( se->item_as_char(1) == PMATH_CHAR_RULE   ||
        se->item_as_char(1) == PMATH_CHAR_ASSIGN ||
        se->item_equals(1, "->")                 ||
        se->item_equals(1, ":="))
    {
      SharedPtr<ScopePos> next_scope = state->new_scope();
      state->new_scope();
      
      bool old_in_pattern = state->in_pattern;
      state->in_pattern = true;
      
      scope_colorize_spanexpr(state, se->item(0));
      
      state->in_pattern = old_in_pattern;
      state->current_pos = next_scope;
      
      if(se->count() == 3) {
        scope_colorize_spanexpr(state, se->item(2));
      }
      else {
        for(int i = se->item_pos(1); i <= se->item(1)->end(); ++i)
          glyphs[i].style = GlyphStyleSyntaxError;
      }
      
      return;
    }
    
    if( se->item_as_char(1) == PMATH_CHAR_RULEDELAYED   ||
        se->item_as_char(1) == PMATH_CHAR_ASSIGNDELAYED ||
        se->item_equals(1, ":>")                        ||
        se->item_equals(1, "::="))
    {
      SharedPtr<ScopePos> next_scope = state->new_scope();
      state->new_scope();
      
      bool old_in_pattern = state->in_pattern;
      state->in_pattern = true;
      
      scope_colorize_spanexpr(state, se->item(0));
      
      state->in_pattern = old_in_pattern;
      
      if(se->count() == 3) {
        scope_colorize_spanexpr(state, se->item(2));
      }
      else {
        for(int i = se->item_pos(1); i <= se->item(1)->end(); ++i)
          glyphs[i].style = GlyphStyleSyntaxError;
      }
      
      state->current_pos = next_scope;
      return;
    }
    
    bool have_integral = false;
    if(pmath_char_is_integral(se->item_as_char(0))) {
      have_integral = true;
    }
    else {
      UnderoverscriptBox *uo = dynamic_cast<UnderoverscriptBox*>(se->item_as_box(0));
      
      if(uo
          && uo->base()->length() == 1
          && pmath_char_is_integral(uo->base()->text()[0])) {
        have_integral = true;
      }
    }
    
    if(have_integral) {
      SpanExpr *integrand = se->item(se->count() - 1);
      
      if(integrand && integrand->count() >= 1) {
        SpanExpr *dx = integrand->item(integrand->count() - 1);
        
        if( dx &&
            dx->count() == 2 &&
            dx->item_as_char(0) == PMATH_CHAR_INTEGRAL_D &&
            pmath_char_is_name(dx->item_first_char(1)))
        {
          for(int i = 0; i < se->count() - 1; ++i)
            scope_colorize_spanexpr(state, se->item(i));
            
          SharedPtr<ScopePos> next_scope = state->new_scope();
          state->new_scope();
          
          symbol_colorize(state, dx->item_pos(1), Special);
          
          for(int i = 0; i < integrand->count() - 1; ++i)
            scope_colorize_spanexpr(state, integrand->item(i));
            
          state->current_pos = next_scope;
          return;
        }
      }
    }
    
    MathSequence *bigop_init = 0;
    int next_item = 1;
    if(pmath_char_maybe_bigop(se->item_as_char(0))) {
      SubsuperscriptBox *subsup = dynamic_cast<SubsuperscriptBox*>(se->item_as_box(1));
      
      if(subsup) {
        bigop_init = subsup->subscript();
        ++next_item;
      }
    }
    else {
      UnderoverscriptBox *uo = dynamic_cast<UnderoverscriptBox*>(se->item_as_box(0));
      
      if( uo &&
          uo->base()->length() == 1 &&
          pmath_char_maybe_bigop(uo->base()->text()[0]))
      {
        bigop_init = uo->underscript();
      }
    }
    
    if(bigop_init
        && bigop_init->length() > 0
        && bigop_init->span_array()[0]) {
      SpanExpr *init = new SpanExpr(0, bigop_init->span_array()[0], bigop_init);
      
      if(init->end() + 1 == bigop_init->length()
          && init->count() >= 2
          && init->item_as_char(1) == '='
          && pmath_char_is_name(init->item_first_char(0))) {
        scope_colorize_spanexpr(state, se->item(next_item - 1));
        
        SharedPtr<ScopePos> next_scope = state->new_scope();
        state->new_scope();
        
        ScopeColorizer(bigop_init).symbol_colorize(
          state,
          init->item_pos(0),
          Special);
          
        for(int i = next_item; i < se->count(); ++i)
          scope_colorize_spanexpr(state, se->item(i));
          
        state->current_pos = next_scope;
        delete init;
        return;
      }
      
      delete init;
    }
  }
  
  if(se->count() >= 3 && se->count() <= 4) { // F(x)   ~x:t
    if( se->item_as_char(1) == '(' &&
        pmath_char_is_name(se->item_first_char(0)))
    {
      String name = se->item_as_text(0);
      SyntaxInformation info(name);
      
      SpanExpr *args = se->item(2);
      bool multiargs = args->count() >= 2 && args->item_as_char(1) == ',';
      
      scope_colorize_spanexpr(state, se->item(0));
      
      switch(info.locals_form) {
        case LocalSpec: {
            if(multiargs)
              scope_colorize_spanexpr(state, args->item(0));
            else
              scope_colorize_spanexpr(state, args);
              
            SharedPtr<ScopePos> next_scope = state->new_scope();
            state->new_scope();
            
            symdeflist_colorize_spanexpr(state, multiargs ? args->item(0) : args, LocalSymbol);
            
            if(multiargs && args->count() >= 3) {
              scope_colorize_spanexpr(state, args->item(2));
            }
            
            state->current_pos = next_scope;
            
            if(multiargs) {
              for(int i = 3; i < args->count(); ++i)
                scope_colorize_spanexpr(state, args->item(i));
            }
            
          }
          return;
          
        case FunctionSpec: {
            if(multiargs) {
              scope_colorize_spanexpr(state, args->item(0));
              
              SharedPtr<ScopePos> next_scope = state->new_scope();
              state->new_scope();
              
              if(multiargs) {
                if(pmath_char_is_name(args->item(0)->first_char()))
                  symbol_colorize(state, args->item_pos(0), Parameter);
                else
                  symdeflist_colorize_spanexpr(state, args->item(0), Parameter);
              }
              else if(pmath_char_is_name(args->first_char()))
                symbol_colorize(state, args->start(), Parameter);
              else
                symdeflist_colorize_spanexpr(state, args, Parameter);
                
              if(multiargs && args->count() >= 3) {
                scope_colorize_spanexpr(state, args->item(2));
              }
              
              state->current_pos = next_scope;
              
              if(multiargs) {
                for(int i = 3; i < args->count(); ++i)
                  scope_colorize_spanexpr(state, args->item(i));
              }
              
              return;
            }
            
            bool old_in_function = state->in_function;
            state->in_function = true;
            
            scope_colorize_spanexpr(state, args);
            
            state->in_function = old_in_function;
          }
          return;
          
        case TableSpec: {
            int locals_min_item = 0;
            int locals_max_item = 0;
            
            if(multiargs) {
              int argpos = 1;
              int i;
              
              for(i = 0; i < args->count() && argpos <= info.locals_max; ++i) {
                if(args->item_as_char(i) == ',') {
                  ++argpos;
                  
                  if(argpos == info.locals_min)
                    locals_min_item = i + 1;
                }
                else if(argpos >= info.locals_min)
                  scope_colorize_spanexpr(state, args->item(i));
              }
              
              locals_max_item = i - 1;
            }
            else if(info.locals_min <= 1 && info.locals_max >= 1) {
              scope_colorize_spanexpr(state, args);
            }
            else
              goto COLORIZE_ITEMS;
              
            SharedPtr<ScopePos> next_scope = state->new_scope();
            state->new_scope();
            
            if(multiargs) {
              for(int i = locals_min_item; i <= locals_max_item; ++i) {
                replacement_colorize_spanexpr(state, args->item(i), Special);
                
                for(int j = i + 1; j <= locals_max_item; ++j) {
                  for(int p = args->item(j)->start(); p <= args->item(j)->end(); ++p)
                    glyphs[p].style = GlyphStyleNone;
                    
                  scope_colorize_spanexpr(state, args->item(j));
                }
              }
              
              for(int i = 0; i < locals_min_item; ++i)
                scope_colorize_spanexpr(state, args->item(i));
                
              for(int i = locals_max_item + 1; i < args->count(); ++i)
                scope_colorize_spanexpr(state, args->item(i));
            }
            else
              replacement_colorize_spanexpr(state, args, Special);
              
            state->current_pos = next_scope;
            
          }
          return;
          
        case NoSpec:
          break;
      }
    }
    
    if( se->item_as_char(2)    == ':' &&
        se->item_first_char(0) == '~' &&
        pmath_char_is_name(se->item_first_char(1)))
    {
      if(!state->in_pattern)
        return;
        
      symbol_colorize(state, se->item_pos(1), Parameter);
      
      for(int i = se->item_pos(0); i <= se->end(); ++i)
        glyphs[i].style = glyphs[se->item_pos(1)].style;
        
      return;
    }
    
  }
  
  if(se->count() >= 4 && se->count() <= 5) { // x/:y:=z   x/:y::=z
    if(se->item_equals(1, "/:")) {
      if( se->item_as_char(3) == PMATH_CHAR_ASSIGN ||
          se->item_equals(3, ":="))
      {
        scope_colorize_spanexpr(state, se->item(0));
        
        SharedPtr<ScopePos> next_scope = state->new_scope();
        state->new_scope();
        
        bool old_in_pattern = state->in_pattern;
        state->in_pattern = true;
        
        scope_colorize_spanexpr(state, se->item(2));
        
        state->in_pattern = old_in_pattern;
        state->current_pos = next_scope;
        
        if(se->count() == 5) {
          scope_colorize_spanexpr(state, se->item(4));
        }
        else {
          for(int i = se->item_pos(3); i <= se->item(3)->end(); ++i)
            glyphs[i].style = GlyphStyleSyntaxError;
        }
        
        return;
      }
      
      if( se->item_as_char(3) == PMATH_CHAR_ASSIGNDELAYED ||
          se->item_equals(3, "::="))
      {
        scope_colorize_spanexpr(state, se->item(0));
        
        SharedPtr<ScopePos> next_scope = state->new_scope();
        state->new_scope();
        
        bool old_in_pattern = state->in_pattern;
        state->in_pattern = true;
        
        scope_colorize_spanexpr(state, se->item(2));
        
        state->in_pattern = old_in_pattern;
        
        if(se->count() == 5) {
          scope_colorize_spanexpr(state, se->item(4));
        }
        else {
          for(int i = se->item_pos(3); i <= se->item(3)->end(); ++i)
            glyphs[i].style = GlyphStyleSyntaxError;
        }
        
        state->current_pos = next_scope;
        return;
      }
    }
  }
  
COLORIZE_ITEMS:
  for(int i = 0; i < se->count(); ++i) {
    SpanExpr *sub = se->item(i);
    
    scope_colorize_spanexpr(state, sub);
  }
}



void ScopeColorizer::comments_colorize_span(Span span, int *pos) {
  const uint16_t         *buf    = sequence->text().buffer();
  const Array<GlyphInfo> &glyphs = sequence->glyph_array();
  const SpanArray        &spans  = sequence->span_array();
  
  if(!span) {
    while(*pos < glyphs.length() && !spans.is_token_end(*pos))
      ++*pos;
      
    ++*pos;
    return;
  }
  
  
  if(!span.next()) {
    if(*pos + 1 < span.end() &&
        buf[*pos]     == '/' &&
        buf[*pos + 1] == '*')
    {
      for(; *pos <= span.end(); ++*pos)
        glyphs[*pos].style = GlyphStyleComment;
        
      return;
    }
  }
  
  comments_colorize_span(span.next(), pos);
  
  while(*pos <= span.end())
    comments_colorize_span(spans[*pos], pos);
}

void ScopeColorizer::syntax_colorize_spanexpr(SpanExpr *se) {
  const Array<GlyphInfo> &glyphs = se->sequence()->glyph_array();
  
  if(se->count() == 0) {
    if(pmath_char_is_left( se->as_char())) {
      SpanExpr *parent = se->parent();
      
      if( !parent ||
          !pmath_char_is_right(parent->item_as_char(parent->count() - 1)))
      {
        glyphs[se->start()].style = GlyphStyleSyntaxError;
        return;
      }
      
      return;
    }
    
    if(pmath_char_is_right(se->as_char())) {
      SpanExpr *parent = se->parent();
      
      if(!parent) {
        glyphs[se->start()].style = GlyphStyleSyntaxError;
        return;
      }
      
      for(int i = parent->count() - 1; i >= 0; --i)
        if(pmath_char_is_left(parent->item_as_char(i)))
          return;
          
      glyphs[se->start()].style = GlyphStyleSyntaxError;
      return;
    }
    
    if(se->first_char() == '"') {
      for(int i = 1 + se->start(); i < se->end(); ++i)
        glyphs[i].style = GlyphStyleString;
        
      if(se->sequence()->text()[se->end()] != '"')
        glyphs[se->end()].style = GlyphStyleString;
    }
    
    return;
  }
  
  if( se->count() == 3 &&
      se->item_as_text(1).equals("::") &&
      se->item(2)->count() == 0)
  {
    for(int i = se->item_pos(2); i <= se->end(); ++i)
      glyphs[i].style = GlyphStyleString;
      
    syntax_colorize_spanexpr(se->item(0));
    return;
  }
  
  for(int i = 0; i < se->count(); ++i)
    syntax_colorize_spanexpr(se->item(i));
}

void ScopeColorizer::arglist_errors_colorize_spanexpr(SpanExpr *se, float error_indicator_height) {
  arglist_errors_colorize_spanexpr_norecurse(se, error_indicator_height);
  
  for(int i = 0; i < se->count(); ++i)
    arglist_errors_colorize_spanexpr(se->item(i), error_indicator_height);
}


void ScopeColorizer::arglist_errors_colorize_spanexpr_norecurse(SpanExpr *se, float error_indicator_height) {
  if(!FunctionCallSpan::is_call(se))
    return;
    
  FunctionCallSpan        call   = se;
  SpanExpr               *head   = call.function_head();
  const Array<GlyphInfo> &glyphs = se->sequence()->glyph_array();
  
  if(head->count() != 0)
    return;
    
  if(GlyphStyleNone != glyphs[head->start()].style)
    return;
    
  if(head->as_token() != PMATH_TOK_NAME)
    return;
    
  String name = head->as_text();
  SyntaxInformation info(name);
  
  if(info.minargs == 0 && info.maxargs == INT_MAX)
    return;
    
  int arg_count = call.function_argument_count();
  
  if(arg_count > info.maxargs) {
    Expr options = Application::interrupt_cached(
                     Call(Symbol(PMATH_SYMBOL_OPTIONS), name));
                     
    if( options.expr_length() == 0 ||
        options[0] != PMATH_SYMBOL_LIST)
    {
      int end = call.arguments_span()->end();
      int start;
      
      if(info.maxargs == 0){
        start = call.arguments_span()->start();
        
        if(call.is_complex_call()){
          int arg1_start = call.function_argument(1)->start();
          int arg1_end   = call.function_argument(1)->end();
          
          for(int pos = arg1_start; pos <= arg1_end; ++pos)
            glyphs[pos].style = GlyphStyleExcessArg;
        }
      }
      else if(info.maxargs == 1 && call.is_complex_call()){
        start = call.arguments_span()->start();
      }
      else{
        start = call.function_argument(info.maxargs)->end() + 1;
      }
      
      for(int pos = start; pos <= end; ++pos)
        glyphs[pos].style = GlyphStyleExcessArg;
        
      return;
    }
    
    for(int i = info.maxargs + 1; i <= arg_count; ++i)
      unknown_option_colorize_spanexpr(call.function_argument(i), options);
  }
  
  if(arg_count < info.minargs) {
    int end = call.arguments_span()->end();
    
    glyphs[end].missing_after = 1;
    
    if(end + 1 < glyphs.length()) {
      glyphs[end + 1].x_offset += 2 * error_indicator_height;
      glyphs[end + 1].right    += 2 * error_indicator_height;
    }
    else
      glyphs[end].right += error_indicator_height;
  }
}

void ScopeColorizer::unknown_option_colorize_spanexpr(SpanExpr *se, Expr options) {
  FunctionCallSpan call(se);
  
  if(call.is_list()) {
    for(int i = call.list_length(); i >= 1; --i)
      unknown_option_colorize_spanexpr(call.list_element(i), options);
      
    return;
  }
  
  if(se->count() == 3) {
    if(se->item(0)->as_token() != PMATH_TOK_NAME)
      return;
      
    if( se->item_as_char(1) == PMATH_CHAR_RULE        ||
        se->item_as_char(1) == PMATH_CHAR_RULEDELAYED ||
        se->item_as_text(1).equals("->")              ||
        se->item_as_text(1).equals(":>"))
    {
      String          name = se->item_as_text(0);
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
      
      int name_start = se->item(0)->start();
      int name_end   = se->item(0)->end();
      
      const Array<GlyphInfo> &glyphs = se->sequence()->glyph_array();
      
      for(int i = name_start; i <= name_end; ++i)
        glyphs[i].style = GlyphStyleInvalidOption;
    }
  }
}

//} ... class ScopeColorizer
