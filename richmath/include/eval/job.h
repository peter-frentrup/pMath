#ifndef __EVAL__JOB_H__
#define __EVAL__JOB_H__

#include <util/pmath-extra.h>
#include <util/sharedptr.h>

namespace richmath{
  class Box;
  class Document;
  class MathSection;
  class MathSequence;
  
  class EvaluationPosition{
    public:
      explicit EvaluationPosition(Box *box = 0);
      
    public:
      int document_id;
      int section_id;
      int box_id;
  };
  
  class Job: public Shareable{
    public:
      Job();
      
      virtual void enqueued() = 0;
      virtual bool start() = 0;
      virtual void end() = 0;
      
      Document *prepare_print(int *output_index);
      
    public:
      EvaluationPosition pos;
      bool have_printed;
  };
  
  class InputJob: public Job{
    public:
      explicit InputJob(MathSection *section);
      
      virtual void enqueued();
      virtual bool start();
      virtual void end();
  };
  
  class EvaluationJob: public InputJob{
    public:
      explicit EvaluationJob(Expr expr, Box *box = 0);
      
      virtual bool start();
      virtual void end();
    
    protected:
      Expr _expr;
  };
  
  class ReplacementJob: public InputJob{
    public:
      explicit ReplacementJob(MathSequence *seq, int start, int end);
      
      virtual bool start();
      virtual void end();
      
    public:
      bool have_result;
      Expr result;
      
    protected:
      int selection_start;
      int selection_end;
  };
}

#endif // __EVAL__JOB_H__
