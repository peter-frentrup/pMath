#ifndef RICHMATH__BOXES__SECTIONORNAMENT_H__INCLUDED
#define RICHMATH__BOXES__SECTIONORNAMENT_H__INCLUDED

#include <boxes/box.h>

namespace richmath {
  class SectionOrnament final : public Base {
    public:
      SectionOrnament();
      ~SectionOrnament();
      
      bool has_index(int i) const { return _box && _box->index() == i; }
      bool reload_if_necessary(BoxAdopter owner, Expr expr, BoxInputFlags flags);
      
      Box *box_or_null() const { return _box; }
      Expr to_pmath() const { return _expr; }
      
    private:
      Box *_box;
      Expr _expr;
  };
}

#endif // RICHMATH__BOXES__SECTIONORNAMENT_H__INCLUDED
