#include <util/syntax-state.h>

#include <climits>

#include <eval/binding.h>
#include <eval/application.h>
#include <graphics/shapers.h>

using namespace richmath;

//{ class GeneralSyntaxInfo ...

SharedPtr<GeneralSyntaxInfo> GeneralSyntaxInfo::std;

GeneralSyntaxInfo::GeneralSyntaxInfo()
  : Shareable()
{
  SET_BASE_DEBUG_TAG(typeid(*this).name());
  memset(&glyph_style_colors, 0, sizeof(glyph_style_colors));
  
  glyph_style_colors[GlyphStyleImplicit]           = 0x999999;
  glyph_style_colors[GlyphStyleString]             = 0x808080;//0xFF0080;
  glyph_style_colors[GlyphStyleComment]            = 0x008000;
  glyph_style_colors[GlyphStyleParameter]          = 0x438958;
  glyph_style_colors[GylphStyleLocal]              = 0x438958;
  glyph_style_colors[GylphStyleScopeError]         = 0xCC0000;
  glyph_style_colors[GlyphStyleNewSymbol]          = 0x002CC3;
  glyph_style_colors[GlyphStyleShadowError]        = 0xFF3333;
  glyph_style_colors[GlyphStyleSyntaxError]        = 0xFF0000;
  glyph_style_colors[GlyphStyleSpecialUse]         = 0x3C7D91;
  glyph_style_colors[GlyphStyleExcessOrMissingArg] = 0xFF3333;
  glyph_style_colors[GlyphStyleInvalidOption]      = 0xFF3333;
  glyph_style_colors[GlyphStyleSpecialStringPart]  = 0xD95355;
  glyph_style_colors[GlyphStyleKeyword]            = 0xAF00DB;
  glyph_style_colors[GlyphStyleFunctionCall]       = 0x795E26;
}

GeneralSyntaxInfo::~GeneralSyntaxInfo() {
}

//} ... class GeneralSyntaxInfo

//{ class ScopePos ...

ScopePos::ScopePos(SharedPtr<ScopePos> super)
  : _super(super)
{
}

bool ScopePos::contains(SharedPtr<ScopePos> sub) {
  ScopePos *pos = sub.ptr();
  
  while(pos) {
    if(this == pos)
      return true;
      
    pos = pos->_super.ptr();
  }
  
  return false;
}

static SharedPtr<ScopePos> dummy_pos;

//} ... class ScopePos

//{ class SymbolInfo ...

SymbolInfo::SymbolInfo(
  SymbolKind            _kind,
  SharedPtr<ScopePos>   _pos,
  SharedPtr<SymbolInfo> _next)
  : Shareable(),
    kind(_kind),
    pos(_pos ? _pos : dummy_pos),
    next(_next)
{
  SET_BASE_DEBUG_TAG(typeid(*this).name());
}

SymbolInfo::~SymbolInfo() {
}

void SymbolInfo::add(SymbolKind _kind, SharedPtr<ScopePos> _pos) {
  if(kind != _kind)
    next = new SymbolInfo(kind, pos, next);
    
  kind = _kind;
  pos = _pos ? _pos : dummy_pos;
}

//} ... class SymbolInfo

class SyntaxInformationImpl {
  private:
    SyntaxInformation &self;
    
  public:
    SyntaxInformationImpl(SyntaxInformation &_self)
      : self(_self)
    {
    }
    
    void init_argument_count(Expr value) {
      if(value.is_int32() && PMATH_AS_INT32(value.get()) >= 0) {
        self.minargs = self.maxargs = PMATH_AS_INT32(value.get());
      }
      else if( value.is_expr()                &&
               value[0] == PMATH_SYMBOL_RANGE &&
               value.expr_length() == 2)
      {
        if( value[1].is_int32() &&
            PMATH_AS_INT32(value[1].get()) >= 0)
        {
          self.minargs = PMATH_AS_INT32(value[1].get());
        }
        
        if( value[2].is_int32() &&
            PMATH_AS_INT32(value[2].get()) >= 0)
        {
          self.maxargs = PMATH_AS_INT32(value[2].get());
        }
      }
    }
    
    void init_local_variables(Expr value) {
      if( !value.is_expr() || value.expr_length() != 2 || value[0] != PMATH_SYMBOL_LIST)
        return;
        
      String form(value[1]);
      if(form.equals("Function"))
        self.locals_form = FunctionSpec;
      else if(form.equals("Local"))
        self.locals_form = LocalSpec;
      else if(form.equals("Table"))
        self.locals_form = TableSpec;
        
      if(self.locals_form != NoSpec) {
        value = value[2];
        
        if( value.is_int32() &&
            PMATH_AS_INT32(value.get()) >= 0)
        {
          self.locals_min = self.locals_max = PMATH_AS_INT32(value.get());
        }
        else if( value.is_expr()                &&
                 value[0] == PMATH_SYMBOL_RANGE &&
                 value.expr_length() == 2)
        {
          if( value[1].is_int32() &&
              PMATH_AS_INT32(value[1].get()) >= 0)
          {
            self.locals_min = PMATH_AS_INT32(value[1].get());
          }
          
          if( value[2].is_int32() &&
              PMATH_AS_INT32(value[2].get()) >= 0)
          {
            self.locals_max = PMATH_AS_INT32(value[2].get());
          }
        }
      }
    }
    
    void init_category(Expr value) {
      String str(value);
      if(str.equals("Keyword"))
        self.is_keyword = true;
    }
};

//{ class SyntaxInformation ...

SyntaxInformation::SyntaxInformation(Expr name)
  : minargs(0),
    maxargs(INT_MAX),
    locals_form(NoSpec),
    locals_min(1),
    locals_max(INT_MAX),
    is_keyword(false)
{
  Expr expr = Application::interrupt_wait_cached(
                Call(
                  Symbol(PMATH_SYMBOL_SYNTAXINFORMATION),
                  name));
                  
  if( expr.is_expr() &&
      expr[0] == PMATH_SYMBOL_LIST)
  {
    for(size_t i = 1; i <= expr.expr_length(); ++i) {
      Expr opt = expr[i];
      
      if(opt.is_rule()) {
        String key(opt[1]);
        
        if(key.equals("ArgumentCount")) {
          SyntaxInformationImpl(*this).init_argument_count(opt[2]);
        }
        else if(key.equals("LocalVariables")) {
          SyntaxInformationImpl(*this).init_local_variables(opt[2]);
        }
        else if(key.equals("Category")) {
          SyntaxInformationImpl(*this).init_category(opt[2]);
        }
      }
    }
  }
}

//} ... class SyntaxInformation

//{ class SyntaxState ...

SyntaxState::SyntaxState()
  : Base(),
    in_pattern(false),
    in_function(false)
{
  SET_BASE_DEBUG_TAG(typeid(*this).name());
}

SyntaxState::~SyntaxState() {
}

void SyntaxState::clear() {
  in_pattern = false;
  in_function = false;
  current_pos = 0;
  local_symbols.clear();
}

//} ... class SyntaxState
