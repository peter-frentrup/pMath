#ifndef RICHMATH__EVAL__EVAL_CONTEXT_H__INCLUDED
#define RICHMATH__EVAL__EVAL_CONTEXT_H__INCLUDED

#include <util/styled-object.h>

namespace richmath {
  class Section;
  
  class EvaluationContexts {
      class Impl;
    public:
      static void init();
      static void done();
      
      static void context_source_deleted(StyledObject *obj);
      static void set_context(String context);
      static Expr prepare_set_context(String context);
      static Expr make_context_block(Expr expr, String context);
      
      static Section *find_section_group_header(Section *section);
      static String resolve_context(StyledObject *obj);
      
      static Expr prepare_namespace_for_current_context(Expr expr);
      static Expr prepare_namespace_for(Expr expr, StyledObject *obj);
      
      static String current();
      static Expr replace_symbol_namespace(Expr expr, String old_ns, String new_ns);
  };
}

#endif // RICHMATH__EVAL__EVAL_CONTEXT_H__INCLUDED
