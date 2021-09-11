#ifndef RICHMATH__GRAPHICS__GLYPHS_H__INCLUDED
#define RICHMATH__GRAPHICS__GLYPHS_H__INCLUDED


#include <util/array.h>
#include <util/frontendobject.h>


namespace richmath {
  enum SyntaxGlyphStyle {
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
  
  enum {
    FontSlantPlain = 1,
    FontSlantItalic = 2
  };
  
  enum {
    NumFontsPerGlyph = (1 << 5)
  };
  
  struct GlyphInfo {
    enum {
      MaxRelOverlap = ((1 << 4) - 1)
    };
    struct ExtendersAndOverlap {
      unsigned num_extenders: 12;
      unsigned rel_overlap: 4;
    };
    
    float right;
    float x_offset;
    union {
      uint16_t            index;
      ExtendersAndOverlap ext;
    };
    unsigned _unused:            4;
    
    unsigned fontinfo:           5;
    
    unsigned slant:              2; // 0=default, otherwise FontSlantXXX
    
    unsigned composed:           1;
    unsigned horizontal_stretch: 1;
    unsigned is_normal_text:     1;
    unsigned missing_after:      1;
    unsigned vertical_centered:  1; // glyph ink center = math_axis above baseline; does not work when composed=1
  };
  
  const uint16_t IgnoreGlyph = 0x0000;
  const uint16_t UnknownGlyph = 0xFFFF;
}

#endif // RICHMATH__GRAPHICS__GLYPHS_H__INCLUDED
