#ifndef RICHMATH__GRAPHICS__SYNTAX_STYLES_H__INCLUDED
#define RICHMATH__GRAPHICS__SYNTAX_STYLES_H__INCLUDED


#include <graphics/color.h>

#include <util/base.h>
#include <util/sharedptr.h>


namespace richmath {
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
