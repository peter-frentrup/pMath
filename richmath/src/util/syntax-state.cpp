#include <util/syntax-state.h>

#include <climits>

#include <eval/binding.h>
#include <eval/client.h>
#include <graphics/shapers.h>

using namespace richmath;

//{ class GeneralSyntaxInfo ...

SharedPtr<GeneralSyntaxInfo> GeneralSyntaxInfo::std;

GeneralSyntaxInfo::GeneralSyntaxInfo()
: Shareable()
{
  memset(&glyph_style_colors, 0, sizeof(glyph_style_colors));
  
  glyph_style_colors[GlyphStyleImplicit]      = 0x999999;
  glyph_style_colors[GlyphStyleString]        = 0x808080;//0xFF0080;
  glyph_style_colors[GlyphStyleComment]       = 0x008000;
  glyph_style_colors[GlyphStyleParameter]     = 0x438958;
  glyph_style_colors[GylphStyleLocal]         = 0x438958;
  glyph_style_colors[GylphStyleScopeError]    = 0xCC0000;
  glyph_style_colors[GlyphStyleNewSymbol]     = 0x002CC3;
  glyph_style_colors[GlyphStyleShadowError]   = 0xFF3333;
  glyph_style_colors[GlyphStyleSyntaxError]   = 0xC254CC;
  glyph_style_colors[GlyphStyleSpecialUse]    = 0x3C7D91;
  glyph_style_colors[GlyphStyleExcessArg]     = 0xFF3333;
  glyph_style_colors[GlyphStyleMissingArg]    = 0xFF3333;
  glyph_style_colors[GlyphStyleInvalidOption] = 0xFF3333;
}

GeneralSyntaxInfo::~GeneralSyntaxInfo(){
}

//} ... class GeneralSyntaxInfo

//{ class ScopePos ...

ScopePos::ScopePos(SharedPtr<ScopePos> super)
: _super(super)
{
}
      
bool ScopePos::contains(SharedPtr<ScopePos> sub){
  ScopePos *pos = sub.ptr();
  
  while(pos){
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
}

SymbolInfo::~SymbolInfo(){
}

void SymbolInfo::add(SymbolKind _kind, SharedPtr<ScopePos> _pos){
  if(kind != _kind)
    next = new SymbolInfo(kind, pos, next);
  
  kind = _kind;
  pos = _pos ? _pos : dummy_pos;
}

//} ... class SymbolInfo

//{ class SyntaxInformation ...

SyntaxInformation::SyntaxInformation(int min, int max)
: minargs(min),
  maxargs(max < 0 ? INT_MAX : max),
  locals_form(NoSpec),
  locals_min(1),
  locals_max(INT_MAX)
{
}

SyntaxInformation::SyntaxInformation(Expr name)
: minargs(0),
  maxargs(INT_MAX),
  locals_form(NoSpec),
  locals_min(1),
  locals_max(INT_MAX)
{
  Expr expr = Client::interrupt_cached(Expr(
    pmath_expr_new_extended(
      pmath_ref(PMATH_SYMBOL_SYNTAXINFORMATION), 1,
      pmath_ref(name.get()))));
  
  if(expr.instance_of(PMATH_TYPE_EXPRESSION)
  && expr[0] == PMATH_SYMBOL_LIST){
    for(size_t i = 1;i <= expr.expr_length();++i){
      Expr opt = expr[i];
      
      if(opt.instance_of(PMATH_TYPE_EXPRESSION)
      && opt[0] == PMATH_SYMBOL_RULE
      && opt.expr_length() == 2){
        String key(opt[1]);
        
        if(key.equals("ArgumentCount")){
          Expr value = opt[2];
          
          if(value.instance_of(PMATH_TYPE_INTEGER)
          && pmath_integer_fits_ui(value.get())){
            unsigned long n = pmath_integer_get_ui(value.get());
            if(n <= INT_MAX)
              minargs = maxargs = (int)n;
          }
          else if(value.instance_of(PMATH_TYPE_EXPRESSION)
          && value[0] == PMATH_SYMBOL_RANGE
          && value.expr_length() == 2){
            if(value[1].instance_of(PMATH_TYPE_INTEGER)
            && pmath_integer_fits_ui(value[1].get())){
              unsigned long n = pmath_integer_get_ui(value[1].get());
              if(n <= INT_MAX)
                minargs = (int)n;
            }
            
            if(value[2].instance_of(PMATH_TYPE_INTEGER)
            && pmath_integer_fits_ui(value[2].get())){
              unsigned long n = pmath_integer_get_ui(value[2].get());
              if(n <= INT_MAX)
                maxargs = (int)n;
            }
          }
        }
        else if(key.equals("LocalVariables")){
          Expr value = opt[2];
          
          if(value.instance_of(PMATH_TYPE_EXPRESSION)
          && value.expr_length() == 2
          && value[0] == PMATH_SYMBOL_LIST){
            String form(value[1]);
            
            if(form.equals("Function"))
              locals_form = FunctionSpec;
            else if(form.equals("Local"))
              locals_form = LocalSpec;
            else if(form.equals("Table"))
              locals_form = TableSpec;
            
            if(locals_form != NoSpec){
              value = value[2];
              
              if(value.instance_of(PMATH_TYPE_INTEGER)
              && pmath_integer_fits_ui(value.get())){
                unsigned long n = pmath_integer_get_ui(value.get());
                if(n <= INT_MAX)
                  locals_min = locals_max = (int)n;
              }
              else if(value.instance_of(PMATH_TYPE_EXPRESSION)
              && value[0] == PMATH_SYMBOL_RANGE
              && value.expr_length() == 2){
                if(value[1].instance_of(PMATH_TYPE_INTEGER)
                && pmath_integer_fits_ui(value[1].get())){
                  unsigned long n = pmath_integer_get_ui(value[1].get());
                  if(n <= INT_MAX)
                    locals_min = (int)n;
                }
                
                if(value[2].instance_of(PMATH_TYPE_INTEGER)
                && pmath_integer_fits_ui(value[2].get())){
                  unsigned long n = pmath_integer_get_ui(value[2].get());
                  if(n <= INT_MAX)
                    locals_max = (int)n;
                }
              }
            }
          }
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
}

SyntaxState::~SyntaxState(){
}
      
void SyntaxState::clear(){
  in_pattern = false;
  in_function = false;
  current_pos = 0;
  local_symbols.clear();
}

//} ... class SyntaxState
