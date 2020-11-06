#include <graphics/syntax-styles.h>

#include <graphics/shapers.h>


using namespace richmath;


//{ class GeneralSyntaxInfo ...

SharedPtr<GeneralSyntaxInfo> GeneralSyntaxInfo::std;

GeneralSyntaxInfo::GeneralSyntaxInfo()
  : Shareable()
{
  SET_BASE_DEBUG_TAG(typeid(*this).name());
  
  for(auto &c : glyph_style_colors)
    c = Color::None;
}

GeneralSyntaxInfo::~GeneralSyntaxInfo() {
}

//} ... class GeneralSyntaxInfo
