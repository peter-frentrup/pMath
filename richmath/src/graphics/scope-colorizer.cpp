#include <graphics/scope-colorizer.h>

#include <util/spanexpr.h>

#include <boxes/mathsequence.h>
#include <boxes/subsuperscriptbox.h>
#include <boxes/underoverscriptbox.h>

#include <eval/application.h>
#include <eval/binding.h>

#include <climits>


using namespace richmath;

namespace richmath {
  class ScopeColorizerImpl {
    private:
      MathSequence           *sequence;
      SyntaxState            *state;
      const Array<GlyphInfo> &glyphs;
      const String           &str;
      
    public:
      ScopeColorizerImpl(MathSequence *_sequence, SyntaxState *_state)
        : sequence(_sequence),
          state(_state),
          glyphs(sequence->glyph_array()),
          str(sequence->text())
      {
      }
      
    private:
      int symbol_colorize(
        int          start,
        SymbolKind   kind
      ) {
        const SpanArray &spans = sequence->span_array();
        
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
            // True, Function
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
            case LocalSymbol:  style = GylphStyleLocal;      break;
            case Special:      style = GlyphStyleSpecialUse; break;
            case Parameter:    style = GlyphStyleParameter;  break;
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
      
      void symdef_colorize_spanexpr(
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
            symbol_colorize(se->item_pos(0), kind);
            return;
          }
        }
        
        symbol_colorize(se->start(), kind);
      }
      
      void symdeflist_colorize_spanexpr(
        SpanExpr    *se,    // "{symdefs ...}"
        SymbolKind   kind
      ) {
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
      
      void replacement_colorize_spanexpr(
        SpanExpr    *se,    // "x->value"
        SymbolKind   kind
      ) {
        if( se->count() >= 2 &&
            (se->item_as_char(1) == PMATH_CHAR_RULE ||
             se->item_equals(1, "->")) &&
            pmath_char_is_name(se->item_first_char(0)))
        {
          symbol_colorize(se->item_pos(0), kind);
        }
      }
      
      
      void colorize_identifier(SpanExpr *se) { // identifiers   #   ~
        assert(se->count() == 0);
        
        if(glyphs[se->start()].style)
          return;
          
        if(se->is_box()) {
          se->as_box()->colorize_scope(state);
          return;
        }
        
        if(pmath_char_is_name(se->first_char())) {
          symbol_colorize(se->start(), Global);
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
      
      void colorize_pure_argument(SpanExpr *se) { // #x
        assert(se->count() == 2);
        
        uint8_t style;
        if(state->in_function)
          style = GlyphStyleParameter;
        else
          style = GylphStyleScopeError;
          
        for(int i = se->start(); i <= se->end(); ++i)
          glyphs[i].style = style;
      }
      
      void colorize_pure_function(SpanExpr *se) { // body &
        assert(se->count() == 2);
        
        bool old_in_function = state->in_function;
        state->in_function = true;
        
        scope_colorize_spanexpr(se->item(0));
        
        state->in_function = old_in_function;
      }
      
      void colorize_mapsto_function(SpanExpr *se) {
        // args \[Function]
        // args \[Function] body
        assert(se->count() == 2 || se->count() == 3);
        
        SharedPtr<ScopePos> next_scope = state->new_scope();
        state->new_scope();
        
        SpanExpr *sub = se->item(0);
        scope_colorize_spanexpr(sub);
        
        if( sub->count() == 3 &&
            sub->item_as_char(0) == '(' &&
            sub->item_as_char(2) == ')')
        {
          sub = sub->item(1);
        }
        
        if( sub->count() == 0 &&
            pmath_char_is_name(sub->first_char()))
        {
          symbol_colorize(sub->start(), Parameter);
        }
        else if(sub->count() > 0 &&
                sub->item_as_char(1) == ',')
        {
          const uint16_t *buf = str.buffer();
          
          for(int i = 0; i < sub->count(); ++i)
            if(pmath_char_is_name(buf[sub->item_pos(i)]))
              symbol_colorize(sub->item_pos(i), Parameter);
        }
        
        if(se->count() == 3) {
          scope_colorize_spanexpr(se->item(2));
        }
        else {
          for(int i = se->item_pos(1); i <= se->item(1)->end(); ++i)
            glyphs[i].style = GlyphStyleSyntaxError;
        }
        
        state->current_pos = next_scope;
      }
      
      void colorize_simple_pattern_name(SpanExpr *se) { // ~x   ?x
        assert(se->count() == 2);
        
        if(!state->in_pattern)
          return;
          
        symbol_colorize(se->item_pos(1), Parameter);
        
        for(int i = se->item_pos(0); i < se->item_pos(1); ++i)
          glyphs[i].style = glyphs[se->item_pos(1)].style;
      }
      
      void colorize_pattern_name(SpanExpr *se) { // name:pat
        assert(se->count() >= 2);
        
        if(!state->in_pattern)
          return;
          
        for(int i = 0; i < se->count(); ++i) {
          SpanExpr *sub = se->item(i);
          
          scope_colorize_spanexpr(sub);
        }
        
        symbol_colorize(se->item_pos(0), Parameter);
      }
      
      void colorize_typed_pattern(SpanExpr *se) { // ~name:type
        assert(se->count() >= 3);
        
        if(!state->in_pattern)
          return;
          
        symbol_colorize(se->item_pos(1), Parameter);
        
        for(int i = se->item_pos(0); i <= se->end(); ++i)
          glyphs[i].style = glyphs[se->item_pos(1)].style;
      }
      
      void colorize_optional_value_pattern(SpanExpr *se) { // ?name:value
        assert(se->count() >= 3);
        
        if(!state->in_pattern)
          return;
          
        for(int i = 0; i < se->count(); ++i) {
          SpanExpr *sub = se->item(i);
          
          scope_colorize_spanexpr(sub);
        }
        
        symbol_colorize(se->item_pos(1), Parameter);
        
        int pos1 = se->item_pos(1);
        for(int i = se->item_pos(0); i < pos1; ++i)
          glyphs[i].style = glyphs[pos1].style;
      }
      
      void colorize_assignment_or_rule(SpanExpr *se, bool delayed) {
        // lhs:=
        // lhs:=rhs
        // lhs->
        // lhs->rhs
        assert(se->count() == 2 || se->count() == 3);
        
        SharedPtr<ScopePos> next_scope = state->new_scope();
        state->new_scope();
        
        bool old_in_pattern = state->in_pattern;
        state->in_pattern = true;
        
        scope_colorize_spanexpr(se->item(0));
        
        state->in_pattern = old_in_pattern;
        
        if(!delayed)
          state->current_pos = next_scope;
          
        if(se->count() == 3) {
          scope_colorize_spanexpr(se->item(2));
        }
        else {
          for(int i = se->item_pos(1); i <= se->item(1)->end(); ++i)
            glyphs[i].style = GlyphStyleSyntaxError;
        }
        
        if(delayed)
          state->current_pos = next_scope;
      }
      
      void colorize_tag_assignment(SpanExpr * se, bool delayed) { // x/:y:=z   x/:y::=z
        assert(se->count() >= 4 && se->count() <= 5);
        
        scope_colorize_spanexpr(se->item(0));
        
        SharedPtr<ScopePos> next_scope = state->new_scope();
        state->new_scope();
        
        bool old_in_pattern = state->in_pattern;
        state->in_pattern = true;
        
        scope_colorize_spanexpr(se->item(2));
        
        state->in_pattern = old_in_pattern;
        
        if(!delayed)
          state->current_pos = next_scope;
          
        if(se->count() == 5) {
          scope_colorize_spanexpr(se->item(4));
        }
        else {
          for(int i = se->item_pos(3); i <= se->item(3)->end(); ++i)
            glyphs[i].style = GlyphStyleSyntaxError;
        }
        
        if(delayed)
          state->current_pos = next_scope;
      }
      
      bool colorize_integral(SpanExpr *se) { // \[Integral] ... \[DifferentialD]...
        assert(se->count() >= 2);
        
        bool have_integral = false;
        if(pmath_char_is_integral(se->item_as_char(0))) {
          have_integral = true;
        }
        else {
          UnderoverscriptBox *uo = dynamic_cast<UnderoverscriptBox *>(se->item_as_box(0));
          
          if(uo &&
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
              scope_colorize_spanexpr(se->item(i));
              
            SharedPtr<ScopePos> next_scope = state->new_scope();
            state->new_scope();
            
            symbol_colorize(dx->item_pos(1), Special);
            
            for(int i = 0; i < integrand->count() - 1; ++i)
              scope_colorize_spanexpr(integrand->item(i));
              
            state->current_pos = next_scope;
            return true;
          }
        }
        
        return false;
      }
      
      bool colorize_bigop(SpanExpr *se) {
        assert(se->count() >= 2);
        
        MathSequence *bigop_init = 0;
        int next_item = 1;
        if(pmath_char_maybe_bigop(se->item_as_char(0))) {
          SubsuperscriptBox *subsup = dynamic_cast<SubsuperscriptBox *>(se->item_as_box(1));
          
          if(subsup) {
            bigop_init = subsup->subscript();
            ++next_item;
          }
        }
        else {
          UnderoverscriptBox *uo = dynamic_cast<UnderoverscriptBox *>(se->item_as_box(0));
          
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
            scope_colorize_spanexpr(se->item(next_item - 1));
            
            SharedPtr<ScopePos> next_scope = state->new_scope();
            state->new_scope();
            
            ScopeColorizerImpl(bigop_init, state).symbol_colorize(init->item_pos(0), Special);
            
            for(int i = next_item; i < se->count(); ++i)
              scope_colorize_spanexpr(se->item(i));
              
            state->current_pos = next_scope;
            delete init;
            return true;
          }
          
          delete init;
        }
        
        return false;
      }
      
      void colorize_localspec_call(SpanExpr *se, const SyntaxInformation &info) {
        /* This currently ignores info.locals_min and info.locals_max. */
        FunctionCallSpan call = se;
        const int arg_count = call.function_argument_count();
        
        if(arg_count < 1)
          return;
          
        scope_colorize_spanexpr(call.function_argument(1));
        
        SharedPtr<ScopePos> next_scope = state->new_scope();
        state->new_scope();
        
        symdeflist_colorize_spanexpr(call.function_argument(1), LocalSymbol);
        
        if(arg_count >= 2) {
          scope_colorize_spanexpr(call.function_argument(2));
        }
        
        state->current_pos = next_scope;
        
        for(int i = 3; i <= arg_count; ++i)
          scope_colorize_spanexpr(call.function_argument(i));
      }
      
      void colorize_functionspec_call(SpanExpr *se, const SyntaxInformation &info) {
        FunctionCallSpan call = se;
        const int arg_count = call.function_argument_count();
        
        if(arg_count < 1)
          return;
          
        if(arg_count == 1) {
          bool old_in_function = state->in_function;
          state->in_function = true;
          
          scope_colorize_spanexpr(call.function_argument(1));
          
          state->in_function = old_in_function;
          return;
        }
        
        scope_colorize_spanexpr(call.function_argument(1));
        
        SharedPtr<ScopePos> next_scope = state->new_scope();
        state->new_scope();
        
        if(pmath_char_is_name(call.function_argument(1)->first_char()))
          symbol_colorize(call.function_argument(1)->start(), Parameter);
        else
          symdeflist_colorize_spanexpr(call.function_argument(1), Parameter);
          
        scope_colorize_spanexpr(call.function_argument(2));
        
        state->current_pos = next_scope;
        
        for(int i = 3; i < arg_count; ++i)
          scope_colorize_spanexpr(call.function_argument(i));
      }
      
      void colorize_tablespec_call(SpanExpr *se, const SyntaxInformation &info) {
        FunctionCallSpan call = se;
        const int arg_count = call.function_argument_count();
        
        if(arg_count < 1)
          return;
          
        SharedPtr<ScopePos> next_scope = state->new_scope();
        state->new_scope();
        
        for(int i = info.locals_min; i <= info.locals_max && i <= arg_count; ++i) {
          replacement_colorize_spanexpr(call.function_argument(i), Special);
          
          for(int j = i + 1; j <= info.locals_max && j <= arg_count; ++j) {
            SpanExpr *arg_j = call.function_argument(j);
            
            for(int p = arg_j->start(); p <= arg_j->end(); ++p)
              glyphs[p].style = GlyphStyleNone;
              
            scope_colorize_spanexpr(arg_j);
          }
        }
        
        for(int i = 1; i < info.locals_min && i <= arg_count; ++i)
          scope_colorize_spanexpr(call.function_argument(i));
          
        for(int i = info.locals_max; i < arg_count; ++i)
          scope_colorize_spanexpr(call.function_argument(i + 1));
          
        state->current_pos = next_scope;
      }
      
      bool colorize_simple_call(SpanExpr *head_name, SpanExpr *se) {
        assert(se->count() >= 3 && se->item_as_char(1) == '(');
        
        String name = head_name->as_text();
        SyntaxInformation info(name);
        
        scope_colorize_spanexpr(se->item(0));
        
        switch(info.locals_form) {
          case LocalSpec:
            colorize_localspec_call(se, info);
            return true;
            
          case FunctionSpec:
            colorize_functionspec_call(se, info);
            return true;
            
          case TableSpec:
            colorize_tablespec_call(se, info);
            return true;
            
          case NoSpec:
            break;
        }
        
        return false;
      }
      
      void colorize_block_body_line(SpanExpr *se, SharedPtr<ScopePos> &scope_after_block) {
        while(se->count() == 1)
          se = se->item(0);
          
        if(FunctionCallSpan::is_simple_call(se)) {
          FunctionCallSpan call(se);
          
          SpanExpr *name = span_as_name(call.function_head());
          if(name) {
            if(name->equals("Local") || name->equals("With")) {
              colorize_scoping_block_head(call, scope_after_block, &ScopeColorizerImpl::symdef_local_colorize_spanexpr);
              return;
            }
          }
        }
        
        if(se->count() < 2) {
          scope_colorize_spanexpr(se);
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
        
        scope_colorize_spanexpr(se);
      }
      
      void colorize_block_body(SpanExpr *se) {
        if(se->count() < 2 || !FunctionCallSpan::is_list(se)) {
          scope_colorize_spanexpr(se);
          return;
        }
        
        SharedPtr<ScopePos> next_scope;
        colorize_block_body_line(se->item(1), next_scope);
        if(next_scope)
          state->current_pos = next_scope;
      }
      
      void symdef_local_colorize_spanexpr(SpanExpr *se) {
        symdef_colorize_spanexpr(se, LocalSymbol);
      }
      
      void replacement_special_colorize_spanexpr(SpanExpr *se) {
        replacement_colorize_spanexpr(se, Special);
      }
      
      void colorize_scoping_block_head(FunctionCallSpan &head, SharedPtr<ScopePos> &scope_after_block, void (ScopeColorizerImpl::*colorize_def)(SpanExpr*)) {
        scope_colorize_spanexpr(head.span());
        
        if(!scope_after_block)
          scope_after_block = state->new_scope();
          
        state->new_scope();
        
        const int head_arg_count = head.function_argument_count();
        for(int i = 1; i <= head_arg_count; ++i)
          (this->*colorize_def)(head.function_argument(i));
      }
      
      void colorize_scoping_block(FunctionCallSpan &head, SpanExpr *se, void (ScopeColorizerImpl::*colorize_def)(SpanExpr*)) {
        assert(se->count() == 2);
        
        SharedPtr<ScopePos> next_scope;
        colorize_scoping_block_head(head, next_scope, colorize_def);
        assert(next_scope);
        
        colorize_block_body(se->item(1));
        
        state->current_pos = next_scope;
      }
      
      void colorize_block(SpanExpr *se) {
        assert(se->count() >= 2);
        
        if(FunctionCallSpan::is_simple_call(se->item(0))) {
          FunctionCallSpan head_call(se->item(0));
          
          SpanExpr *name = span_as_name(head_call.function_head());
          if(name) {
            if(se->count() == 2) {
              if(name->equals("Local") || name->equals("With")) {
                colorize_scoping_block(head_call, se, &ScopeColorizerImpl::symdef_local_colorize_spanexpr);
                return;
              }
              if(name->equals("Do")) {
                colorize_scoping_block(head_call, se, &ScopeColorizerImpl::replacement_special_colorize_spanexpr);
                return;
              }
            }
          }
        }
        
        SpanExpr *name = span_as_name(se->item(0));
        if(name && name->equals("Function") && se->count() == 3) {
          /* Function name(...) {...}  is syntactic suggar for  name(...)::=...
           */
          scope_colorize_spanexpr(se->item(0));
          
          SharedPtr<ScopePos> next_scope = state->new_scope();
          state->new_scope();
          
          bool old_in_pattern = state->in_pattern;
          state->in_pattern = true;
          
          scope_colorize_spanexpr(se->item(1)); // name(...)
          
          state->in_pattern = old_in_pattern;
          
          colorize_block_body(se->item(2)); // {...}
          
          state->current_pos = next_scope;
          return;
        }
        
        for(int i = 0; i < se->count(); ++i) {
          colorize_block_body(se->item(i));
        }
      }
      
    public:
      void scope_colorize_spanexpr(SpanExpr *se) {
        assert(se != 0);
        
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
              for(int i = se->start(); i <= se->end(); ++i)
                glyphs[i].style = GlyphStyleParameter;
                
              return;
            }
            
            if(pmath_char_is_name(se->item_first_char(0))) {
              colorize_pattern_name(se);
              return;
            }
          }
          
          if(se->item_as_char(1) == 0x21A6) { // mapsto
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
            SpanExpr *head_name = span_as_name(se->item(0));
            if(head_name) {
              if(colorize_simple_call(head_name, se))
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
          
          scope_colorize_spanexpr(sub);
        }
      }
  };
  
  class ErrorColorizerImpl {
    private:
      float  error_indicator_height;
      
    public:
      ErrorColorizerImpl(float _error_indicator_height)
        : error_indicator_height(_error_indicator_height)
      {
      }
      
    private:
      void unknown_option_colorize_spanexpr(SpanExpr *se, Expr options) {
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
      
      void add_missing_indicator(SpanExpr *span_before) {
        const Array<GlyphInfo> &glyphs = span_before->sequence()->glyph_array();
        int end = span_before->end();
        
        glyphs[end].missing_after = 1;
        
        if(end + 1 < glyphs.length()) {
          glyphs[end + 1].x_offset += 2 * error_indicator_height;
          glyphs[end + 1].right    += 2 * error_indicator_height;
        }
        else
          glyphs[end].right += error_indicator_height;
      }
      
      void mark_excess_args(FunctionCallSpan &call, int max_args) {
        const Array<GlyphInfo> &glyphs = call.span()->sequence()->glyph_array();
        
        int end = call.arguments_span()->end();
        int start;
        
        if(max_args == 0) {
          start = call.arguments_span()->start();
          
          if(call.is_complex_call()) {
            int arg1_start = call.function_argument(1)->start();
            int arg1_end   = call.function_argument(1)->end();
            
            for(int pos = arg1_start; pos <= arg1_end; ++pos)
              glyphs[pos].style = GlyphStyleExcessArg;
          }
        }
        else if(max_args == 1 && call.is_complex_call()) {
          start = call.arguments_span()->start();
        }
        else {
          start = call.function_argument(max_args)->end() + 1;
        }
        
        for(int pos = start; pos <= end; ++pos)
          glyphs[pos].style = GlyphStyleExcessArg;
          
        return;
      }
      
      void arglist_errors_colorize_spanexpr_norecurse(SpanExpr *se) {
        if(!FunctionCallSpan::is_call(se))
          return;
          
        FunctionCallSpan        call      = se;
        SpanExpr               *head_name = span_as_name(call.function_head());
        const Array<GlyphInfo> &glyphs    = se->sequence()->glyph_array();
        
        if(!head_name)
          return;
          
        if(GlyphStyleNone != glyphs[head_name->start()].style)
          return;
          
        String name = head_name->as_text();
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
      
      void get_block_head_argument_counts(SpanExpr *name, int &argmin, int &argmax) {
        argmin = 0;
        argmax = INT_MAX;
        
        if( name->equals("If") ||
            name->equals("While") ||
            name->equals("Switch") ||
            name->equals("Case"))
        {
          argmin = 1;
          argmax = 1;
          return;
        }
        
        if( name->equals("Do") ||
            name->equals("With") ||
            name->equals("Local"))
        {
          argmin = 1;
          return;
        }
        
        
        if( name->equals("Function") ||
            name->equals("Block") ||
            name->equals("Try") ||
            name->equals("Finally"))
        {
          argmax = 0;
        }
      }
      
      void colorize_block_body_line_errors(SpanExpr *se) {
        while(se->count() == 1)
          se = se->item(0);
          
        if(FunctionCallSpan::is_simple_call(se)) {
          FunctionCallSpan call(se);
          
          SpanExpr *name = span_as_name(call.function_head());
          if(name) {
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
      
      void colorize_block_body_errors(SpanExpr *se) {
        while(se->count() == 1)
          se = se->item(0);
          
        if(FunctionCallSpan::is_simple_call(se)) {
          FunctionCallSpan call(se);
          SpanExpr *name = span_as_name(call.function_head());
          if(name) {
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
      
      void colorize_block_errors(SpanExpr *se) {
        assert(se->count() >= 2);
        
        for(int i = 0; i < se->count(); ++i)
          colorize_block_body_errors(se->item(i));
      }
      
    public:
      void arglist_errors_colorize_spanexpr(SpanExpr *se) {
        if(BlockSpan::maybe_block(se)) {
          colorize_block_errors(se);
          return;
        }
        
        arglist_errors_colorize_spanexpr_norecurse(se);
        
        for(int i = 0; i < se->count(); ++i)
          arglist_errors_colorize_spanexpr(se->item(i));
      }
  };
  
  class SyntaxColorizerImpl {
    public:
      static void colorize_quoted_string(SpanExpr *se) {
        if(se->first_char() != '"') 
          return;
        
        if(se->count() != 0 && se->item_pos(0) == se->start()) 
          return;
          
        const Array<GlyphInfo> &glyphs = se->sequence()->glyph_array();
        for(int i = 1 + se->start(); i < se->end(); ++i) {
          if(se->sequence()->text()[i] == '\\') {
            const uint16_t *buf = se->sequence()->text().buffer() + i;
            int maxlen = se->end() - i;
            uint32_t encoded_char;
            const uint16_t *next = pmath_char_parse(buf, maxlen, &encoded_char);
            
            int len = next - buf;
            for(int j = 0; j < len; ++j)
              glyphs[i + j].style = GlyphStyleSpecialStringPart;
              
            i += len - 1;
            
            continue;
          }
          
          glyphs[i].style = GlyphStyleString;
        }
        
        if(se->sequence()->text()[se->end()] != '"')
          glyphs[se->end()].style = GlyphStyleString;
      }
  
      static bool colorize_single_token(SpanExpr *se) {
        if(se->count() > 0) 
          return false;
          
        const Array<GlyphInfo> &glyphs = se->sequence()->glyph_array();
        
        uint16_t ch = se->as_char();
        if(pmath_char_is_left(ch)) {
          SpanExpr *parent = se->parent();
          
          if( !parent ||
              !pmath_char_is_right(parent->item_as_char(parent->count() - 1)))
          {
            if(pmath_right_fence(se->as_char()) != 0) {
              glyphs[se->start()].style = GlyphStyleSyntaxError;
              return true;
            }
          }
          
          return true;
        }
        
        if(pmath_char_is_right(ch)) {
          SpanExpr *parent = se->parent();
          
          if(!parent) {
            glyphs[se->start()].style = GlyphStyleSyntaxError;
            return true;
          }
          
          for(int i = parent->count() - 1; i >= 0; --i)
            if(pmath_right_fence(parent->item_as_char(i)) == ch) 
              return true;
              
          glyphs[se->start()].style = GlyphStyleSyntaxError;
          return true;
        }
        
//        if(se->first_char() == '\\') { /* outside a string or in a box in a string */
//          for(int i = se->start(); i <= se->end(); ++i)
//            glyphs[i].style = GlyphStyleSpecialStringPart;
//    
//          return true;
//        }

        return true;
      }
  };
}

//{ class ScopeColorizer ...

ScopeColorizer::ScopeColorizer(MathSequence *_sequence)
  : Base(),
    sequence(_sequence)
{
}

void ScopeColorizer::scope_colorize_spanexpr(SyntaxState *state, SpanExpr *se) {
  ScopeColorizerImpl(sequence, state).scope_colorize_spanexpr(se);
}


void ScopeColorizer::comments_colorize_span(Span span, int *pos) {
  const uint16_t         *buf     = sequence->text().buffer();
  const uint16_t         *buf_end = buf + sequence->text().length();
  const Array<GlyphInfo> &glyphs  = sequence->glyph_array();
  const SpanArray        &spans   = sequence->span_array();
  
  if(!span) {
    if(is_comment_start_at(buf + *pos, buf_end)) {
      while(*pos < glyphs.length() && !spans.is_token_end(*pos)) {
        glyphs[*pos].style = GlyphStyleComment;
        ++*pos;
      }
      
      glyphs[*pos].style = GlyphStyleComment;
      ++*pos;
    }
    else {
      while(*pos < glyphs.length() && !spans.is_token_end(*pos))
        ++*pos;
        
      ++*pos;
    }
    return;
  }
  
  if(!span.next()) {
    if(is_comment_start_at(buf + *pos, buf_end)) {
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
  
  SyntaxColorizerImpl::colorize_quoted_string(se);
  if(SyntaxColorizerImpl::colorize_single_token(se))
    return;
  
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
  ErrorColorizerImpl(error_indicator_height).arglist_errors_colorize_spanexpr(se);
}

//} ... class ScopeColorizer
