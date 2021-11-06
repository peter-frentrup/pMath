#ifndef RICHMATH__BOXES__BOX_FACTORY_H__INCLUDED
#define RICHMATH__BOXES__BOX_FACTORY_H__INCLUDED

#include <boxes/abstractsequence.h>
#include <boxes/section.h>


namespace richmath {
  struct BoxFactory {
    static Box              *create_box(LayoutKind layout_kind, Expr expr, BoxInputFlags options);
    static AbstractSequence *create_sequence(LayoutKind kind);
    static Section          *create_section(Expr expr);
  };
}

#endif // RICHMATH__BOXES__BOX_FACTORY_H__INCLUDED
