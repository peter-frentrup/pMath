#ifndef RICHMATH__GRAPHICS__SYNTAX_STYLES_H__INCLUDED
#define RICHMATH__GRAPHICS__SYNTAX_STYLES_H__INCLUDED


#include <graphics/color.h>

#include <util/base.h>
#include <util/sharedptr.h>
#include <util/rle-array.h>


namespace richmath {
  enum SyntaxGlyphStyleEnum {
    GlyphStyleNone,
    GlyphStyleImplicit,
    GlyphStyleString,
    GlyphStyleComment,
    GlyphStyleParameter,
    GlyphStyleLocal,
    GlyphStyleScopeError,
    GlyphStyleNewSymbol,
    GlyphStyleShadowError,
    GlyphStyleSyntaxError,
    GlyphStyleSpecialUse,
    GlyphStyleExcessOrMissingArg,
    GlyphStyleInvalidOption,
    GlyphStyleSpecialStringPart,
    GlyphStyleKeyword,
    GlyphStyleFunctionCall
  };
  
  struct SyntaxGlyphStyle {
    SyntaxGlyphStyle() = default;
    SyntaxGlyphStyle(enum SyntaxGlyphStyleEnum kind) : _kind{(uint8_t)kind}, _is_mising_after{0} {}
    
    SyntaxGlyphStyleEnum kind() const { return (SyntaxGlyphStyleEnum)_kind; }
    void kind(SyntaxGlyphStyleEnum value) { _kind = value; }
    
    bool is_missing_after() const { return _is_mising_after; }
    void is_missing_after(bool value) { _is_mising_after = value; }
    
    SyntaxGlyphStyle with_missing_after(bool value) { SyntaxGlyphStyle ret = *this; ret._is_mising_after = value; return ret; }
    
    explicit operator bool() const { return _kind != GlyphStyleNone; }
    friend bool operator==(const SyntaxGlyphStyle &left, const SyntaxGlyphStyle &right) { return left._kind == right._kind && left._is_mising_after == right._is_mising_after; }
    friend bool operator!=(const SyntaxGlyphStyle &left, const SyntaxGlyphStyle &right) { return !(left == right); }
  
  private:
    uint8_t _kind;
    uint8_t _is_mising_after : 1;
  };
  
  struct SyntaxGlyphStylePredictor {
    static SyntaxGlyphStyle predict(SyntaxGlyphStyle initial, int steps) { 
      if(steps == 0)
        return initial; 
      
      initial.is_missing_after(false);
      return initial;
    }
    
    enum { _debug_predictor_kind = ConstPredictor<SyntaxGlyphStyle>::_debug_predictor_kind };
  };
  
  using SyntaxGlyphStylesArray = RleArray<SyntaxGlyphStyle, SyntaxGlyphStylePredictor>;
  
  class GeneralSyntaxInfo: public Shareable {
    public:
      static SharedPtr<GeneralSyntaxInfo> std;
      
    public:
      GeneralSyntaxInfo();
      virtual ~GeneralSyntaxInfo();
      
    public:
      Color glyph_style_colors[16];
  };
}

#endif // RICHMATH__GRAPHICS__SYNTAX_STYLES_H__INCLUDED
