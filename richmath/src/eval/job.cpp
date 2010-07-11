#include <eval/job.h>

#include <boxes/section.h>
#include <boxes/mathsequence.h>
#include <eval/client.h>
#include <eval/server.h>
#include <gui/document.h>
#include <gui/native-widget.h>

using namespace richmath;

//{ class EvaluationPosition ...

EvaluationPosition::EvaluationPosition(Box *box)
: document_id(0),
  section_id(0),
  box_id(0)
{
  if(box){
    box_id = box->id();
    
    Document *doc = box->find_parent<Document>(true);
    document_id = doc ? doc->id() : 0;
    
    Section *sect= box->find_parent<Section>(true);
    section_id = sect ? sect->id() : 0;
  }
}

//} ... class EvaluationPosition

//{ class Job ...

Job::Job()
: Shareable(),
  have_printed(false)
{
}

//} ... class Job

//{ class InputJob ...

InputJob::InputJob(MathSection *section)
: Job()
{
  if(section && !section->evaluating){
    pos = EvaluationPosition(section);
  }
}

void InputJob::enqueued(){
  Document *doc = dynamic_cast<Document*>(
    Box::find(pos.document_id));
    
  MathSection *section = dynamic_cast<MathSection*>(
    Box::find(pos.section_id));
  
  if(section){
    if(section->evaluating){
      pos = EvaluationPosition(0);
    }
    else{
      if(doc){
        doc->move_to(doc, section->index() + 1);
      }
      
      section->evaluating = true;
      section->invalidate();
    }
  }
}

bool InputJob::start(){
  MathSection *section = dynamic_cast<MathSection*>(
    Box::find(pos.section_id));
    
  Document *doc = dynamic_cast<Document*>(
    Box::find(pos.document_id));
  
  if(!section || !doc || section->parent() != doc){
    if(section){
      section->evaluating = false;
      section->invalidate();
    }
      
    return false;
  }
  
  int i = section->index() + 1;
  while(i < doc->count()){
    Section *s = doc->section(i);
    if(!s || !s->get_style(SectionGenerated))
      break;
    
    ++i;
    //doc->remove(i, i + 1);
  }
  
  doc->move_to(doc, i);
  
  Expr line = Client::interrupt(Expr(
    pmath_expr_new_extended(
      pmath_ref(PMATH_SYMBOL_PLUS), 2,
      pmath_ref(PMATH_SYMBOL_LINE),
      pmath_integer_new_si(1))));
  
  Expr dlvl = Client::interrupt(Symbol(PMATH_SYMBOL_DIALOGLEVEL));
  
  String label = String("in [");
  if(dlvl.instance_of(PMATH_TYPE_INTEGER)){
    if(pmath_integer_fits_ui(dlvl.get())){
      unsigned long ui = pmath_integer_get_ui(dlvl.get());
      
      if(ui == 1)
        label = String("(Dialog) ") + label;
      else if(ui != 0)
        label = String("(Dialog ") + dlvl.to_string() + String(") ") + label;
    }
    else
      label = String("(Dialog ") + dlvl.to_string() + String(") ") + label;
  }
  else
    label = String("(Dialog ") + dlvl.to_string() + String(") ") + label;
  
  section->label(label + line.to_string() + String("]:"));
  section->invalidate();
  
  Server::local_server->run_boxes(
    Expr(section->content()->to_pmath(true)));
  
  doc->native()->running_state_changed();
  
  return true;
}

void InputJob::end(){
//  Document *doc = dynamic_cast<Document*>(
//    BoxReference::find(pos.document_id));
//          
//  Section *sect = dynamic_cast<Section*>(
//    BoxReference::find(pos.section_id));
//  
//  if(doc && sect && sect->parent() == doc){
//    int index = sect->index() + 1;
//            
//    while(index < doc->count()){
//      Section *s = doc->section(index);
//      if(!s || !s->get_style(SectionGenerated))
//        break;
//      
//      doc->remove(index, index + 1);
//    }
//  }
  Document *doc = dynamic_cast<Document*>(
    Box::find(pos.document_id));
    
  MathSection *section = dynamic_cast<MathSection*>(
    Box::find(pos.section_id));
    
  if(section){
    section->evaluating = false;
    section->invalidate();
  }
  
  if(doc)
    doc->native()->running_state_changed();
}

//} ... class InputJob

//{ class EvaluationJob ...

EvaluationJob::EvaluationJob(Expr expr, Box *box)
: InputJob(0),
  _expr(expr)
{
  pos = EvaluationPosition(box);
}

bool EvaluationJob::start(){
  Document *doc = dynamic_cast<Document*>(
    Box::find(pos.document_id));
  
  Server::local_server->run(_expr);
  
  if(doc)
    doc->native()->running_state_changed();
    
  return true;
}

void EvaluationJob::end(){
  InputJob::end();
}

//} ... class EvaluationJob

//{ class ReplacementJob ...

ReplacementJob::ReplacementJob(MathSequence *seq, int start, int end)
: InputJob(0),
  have_result(false),
//  selection_seq_id(0),
  selection_start(start),
  selection_end(end)
{
  assert(0 <= start);
  assert(start <= end);
  
  pos = EvaluationPosition(seq);
  
//  Box *box = seq;
//  while(box && !dynamic_cast<Section*>(box))
//    box = box->parent();
//  
////  Section *section = dynamic_cast<Section*>(box);
//  if(section && !section->evaluating){
//    SectionList *doc = dynamic_cast<SectionList*>(section->parent());
//    
//    if(doc){
//      document_id = doc->id();
//      section_id = section->id();
//      input_section_id = section_id;
//      selection_seq_id = seq->id();
//    }
//  }
}

bool ReplacementJob::start(){
  Document *doc = dynamic_cast<Document*>(
    Box::find(pos.document_id));
    
  MathSection *section = dynamic_cast<MathSection*>(
    Box::find(pos.section_id));
    
  MathSequence *sequence = dynamic_cast<MathSequence*>(
    Box::find(pos.box_id));
  
  if(!section || !doc || !sequence || section->parent() != doc
  || selection_end > sequence->length()
  || !sequence->get_style(Editable)){
    if(section){
      section->evaluating = false;
      section->invalidate();
    }
      
    return false;
  }
  
  Expr line = Client::interrupt(Expr(
    pmath_expr_new_extended(
      pmath_ref(PMATH_SYMBOL_PLUS), 2,
      pmath_ref(PMATH_SYMBOL_LINE),
      pmath_integer_new_si(1))));
  
  Server::local_server->run_boxes(
    Expr(sequence->to_pmath(true, selection_start, selection_end)));
  
  doc->native()->running_state_changed();
  
  return true;
}

void ReplacementJob::end(){
  Document *doc = dynamic_cast<Document*>(
    Box::find(pos.document_id));
  MathSection *section = dynamic_cast<MathSection*>(
    Box::find(pos.section_id));
  MathSequence *sequence = dynamic_cast<MathSequence*>(
    Box::find(pos.box_id));
    
  if(section){
    section->evaluating = false;
    section->invalidate();
  }
  
  if(have_result
  && result != PMATH_SYMBOL_FAILED
  && doc 
  && section
  && sequence
  && section->parent() == doc
  && selection_end <= sequence->length()){
    sequence->remove(selection_start, selection_end);
    
    int opts = BoxOptionDefault;
    if(sequence->get_style(AutoNumberFormating))
      opts|= BoxOptionFormatNumbers;
    
    MathSequence *insert_seq = new MathSequence;
    insert_seq->load_from_object(result, opts);
    int len = insert_seq->length();
    insert_seq->ensure_boxes_valid();
    sequence->insert(selection_start, insert_seq);
    
    doc->select(sequence, selection_start + len, selection_start + len);
  }
  
  if(doc)
    doc->native()->running_state_changed();
}

//} ... class InputJob
