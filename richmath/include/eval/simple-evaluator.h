#ifndef RICHMATH__SIMPLE_EVALUATOR_H__INCLUDED
#define RICHMATH__SIMPLE_EVALUATOR_H__INCLUDED

#include <util/pmath-extra.h>

namespace richmath {
  class FrontEndObject;
  class SimpleEvaluator {
      class Impl;
    public:
      static bool try_eval(FrontEndObject *scope, Expr *result, Expr call);
      
      static Expr expand_compressed_data(Expr expr);
      
      static void done();
  };
}

#endif // RICHMATH__SIMPLE_EVALUATOR_H__INCLUDED
