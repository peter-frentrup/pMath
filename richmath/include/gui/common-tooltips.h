#ifndef RICHMATH__GUI__TOOLTIPS_H__INCLUDED
#define RICHMATH__GUI__TOOLTIPS_H__INCLUDED

#include <util/sharedptr.h>

namespace pmath {
  class Expr;
}

namespace richmath {
  class Document;
  class Stylesheet;
  
  namespace CommonTooltips {
    void load_content(Document *doc, const pmath::Expr &boxes, SharedPtr<Stylesheet> stylesheet);
  }
}

#endif // RICHMATH__GUI__TOOLTIPS_H__INCLUDED
