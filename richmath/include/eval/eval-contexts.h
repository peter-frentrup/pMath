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
      static Section *find_section_group_header(Section *section);
      static String resolve_context(StyledObject *obj);
  };
}

#endif // RICHMATH__EVAL__EVAL_CONTEXT_H__INCLUDED
