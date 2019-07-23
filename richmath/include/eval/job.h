#ifndef RICHMATH__EVAL__JOB_H__INCLUDED
#define RICHMATH__EVAL__JOB_H__INCLUDED

#include <util/frontendobject.h>
#include <util/pmath-extra.h>
#include <util/sharedptr.h>


namespace richmath {
  class Box;
  class Document;
  class MathSection;
  class MathSequence;
  
  class EvaluationPosition {
    public:
      EvaluationPosition(FrontEndReference _doc, FrontEndReference _sect, FrontEndReference _box);
      explicit EvaluationPosition(Box *box = nullptr);
      
    public:
      FrontEndReference document_id;
      FrontEndReference section_id;
      FrontEndReference box_id;
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
      
      const EvaluationPosition &position() { return _position; }
      
    public:
      Expr default_graphics_options;
      
    protected:
      EvaluationPosition _position;
  };
  
  class InputJob: public Job {
    public:
      explicit InputJob(MathSection *section);
      
      virtual void enqueued() override;
      virtual bool start() override;
      virtual void returned(Expr expr) override;
      virtual void returned_boxes(Expr expr) override;
      virtual void end() override;
      virtual void dequeued() override;
  };
  
  class EvaluationJob: public InputJob {
    public:
      explicit EvaluationJob(Expr expr, Box *box = nullptr);
      
      virtual bool start() override;
      virtual void end() override;
      
    protected:
      Expr _expr;
  };
  
  class DynamicEvaluationJob: public EvaluationJob {
    public:
      explicit DynamicEvaluationJob(Expr info, Expr expr, Box *box);
      
      virtual bool start() override;
      virtual void end() override;
      virtual void returned(Expr expr) override;
      
    protected:
      Expr              _info;
      FrontEndReference old_eval_id;
  };
  
  class ReplacementJob: public InputJob {
    public:
      explicit ReplacementJob(MathSequence *seq, int start, int end);
      
      virtual bool start() override;
      virtual void returned_boxes(Expr expr) override;
      virtual void end() override;
      
    public:
      bool have_result;
      Expr result;
      
    protected:
      int selection_start;
      int selection_end;
  };
}

#endif // RICHMATH__EVAL__JOB_H__INCLUDED
