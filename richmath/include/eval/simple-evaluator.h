#ifndef RICHMATH__SIMPLE_EVALUATOR_H__INCLUDED
#define RICHMATH__SIMPLE_EVALUATOR_H__INCLUDED

#include <util/pmath-extra.h>

namespace richmath {
  class FrontEndObject;
  class SimpleEvaluator {
      class Impl;
    public:
      static bool try_eval(FrontEndObject *scope, Expr *result, Expr call);
      
      static void done();
  };
}

#endif // RICHMATH__SIMPLE_EVALUATOR_H__INCLUDED
