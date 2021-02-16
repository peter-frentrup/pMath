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
      EvaluationPosition(FrontEndReference doc, FrontEndReference sect, FrontEndReference obj);
      explicit EvaluationPosition(FrontEndObject *obj = nullptr);
      
      friend void swap(EvaluationPosition &left, EvaluationPosition &right) {
        using std::swap;
        swap(left.document_id, right.document_id);
        swap(left.section_id,  right.section_id);
        swap(left.object_id,   right.object_id);
      }
      
    public:
      FrontEndReference document_id;
      FrontEndReference section_id;
      FrontEndReference object_id;
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
    
    protected:
      void set_context();
  };
  
  class EvaluationJob: public InputJob {
    public:
      explicit EvaluationJob(Expr expr, FrontEndObject *obj = nullptr);
      
      virtual bool start() override;
      virtual void end() override;
      
    protected:
      Expr _expr;
  };
  
  class DynamicEvaluationJob: public EvaluationJob {
    public:
      explicit DynamicEvaluationJob(Expr info, Expr expr, FrontEndObject *obj);
      
      virtual bool start() override;
      virtual void end() override;
      virtual void returned(Expr expr) override;
      
    protected:
      Expr              _info;
      FrontEndReference old_observer_id;
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
