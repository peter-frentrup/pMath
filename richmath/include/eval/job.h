#ifndef __EVAL__JOB_H__
#define __EVAL__JOB_H__

#include <util/pmath-extra.h>
#include <util/sharedptr.h>


namespace richmath {
  class Box;
  class Document;
  class MathSection;
  class MathSequence;
  
  class EvaluationPosition {
    public:
      EvaluationPosition(int _doc, int _sect, int _box);
      explicit EvaluationPosition(Box *box = 0);
      
    public:
      int document_id;
      int section_id;
      int box_id;
  };
  
  class Job: public Shareable {
    public:
      Job();
      
      virtual void enqueued() = 0;
      virtual bool start() = 0;
      virtual void returned(Expr expr) = 0;
      virtual void returned_boxes(Expr expr) = 0;
      virtual void end() = 0;
      virtual void dequeued() = 0;
      
      Document *prepare_print(int *output_index);
      
      const EvaluationPosition &position() { return _position; }
      
    protected:
      EvaluationPosition _position;
      bool have_printed;
  };
  
  class InputJob: public Job {
    public:
      explicit InputJob(MathSection *section);
      
      virtual void enqueued();
      virtual bool start();
      virtual void returned(Expr expr);
      virtual void returned_boxes(Expr expr);
      virtual void end();
      virtual void dequeued();
  };
  
  class EvaluationJob: public InputJob {
    public:
      explicit EvaluationJob(Expr expr, Box *box = 0);
      
      virtual bool start();
      virtual void end();
      
    protected:
      Expr _expr;
  };
  
  class DynamicEvaluationJob: public EvaluationJob {
    public:
      explicit DynamicEvaluationJob(Expr info, Expr expr, Box *box);
      
      virtual bool start();
      virtual void end();
      virtual void returned(Expr expr);
      
    protected:
      Expr _info;
      int old_eval_id;
  };
  
  class ReplacementJob: public InputJob {
    public:
      explicit ReplacementJob(MathSequence *seq, int start, int end);
      
      virtual bool start();
      virtual void returned_boxes(Expr expr);
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
