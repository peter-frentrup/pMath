#include <eval/job.h>

#include <boxes/section.h>
#include <boxes/mathsequence.h>

#include <eval/application.h>
#include <eval/dynamic.h>
#include <eval/server.h>

#include <gui/document.h>
#include <gui/native-widget.h>

using namespace richmath;

extern pmath_symbol_t richmath_System_DollarLine;

//{ class EvaluationPosition ...

EvaluationPosition::EvaluationPosition(FrontEndReference _doc, FrontEndReference _sect, FrontEndReference _box)
  : document_id(_doc),
    section_id(_sect),
    box_id(_box)
{
}

EvaluationPosition::EvaluationPosition(Box *box)
  : document_id(FrontEndReference::None),
    section_id(FrontEndReference::None),
    box_id(FrontEndReference::None)
{
  if(box) {
    box_id = box->id();
    
    Document *doc = box->find_parent<Document>(true);
    document_id = doc ? doc->id() : FrontEndReference::None;
    
    Section *sect = box->find_parent<Section>(true);
    section_id = sect ? sect->id() : FrontEndReference::None;
  }
}

//} ... class EvaluationPosition

//{ class Job ...

Job::Job()
  : Shareable()
{
  SET_BASE_DEBUG_TAG(typeid(*this).name());
}

//} ... class Job

//{ class InputJob ...

InputJob::InputJob(MathSection *section)
  : Job()
{
  SET_BASE_DEBUG_TAG(typeid(*this).name());
  if(section && !section->evaluating) {
    _position = EvaluationPosition(section);
  }
}

void InputJob::enqueued() {
  auto doc = dynamic_cast<Document*>(Box::find(_position.document_id));
  
  if(auto section = dynamic_cast<Section*>(Box::find(_position.section_id))) {
    if(section->evaluating) {
      _position = EvaluationPosition(nullptr);
    }
    else {
      if(doc) {
        doc->move_to(doc, section->index() + 1);
      }
      
      section->evaluating = true;
      section->invalidate();
    }
  }
}

bool InputJob::start() {
  auto section = dynamic_cast<MathSection*>(Box::find(_position.section_id));
  auto doc = dynamic_cast<Document*>(Box::find(_position.document_id));
  
  if(!section || !doc || section->parent() != doc) {
    if(section) {
      section->evaluating = false;
      section->invalidate();
    }
    
    return false;
  }
  
  int i = section->index() + 1;
  while(i < doc->count()) {
    Section *s = doc->section(i);
    if(!s || !s->get_style(SectionGenerated))
      break;
      
    ++i;
    //doc->remove(i, i + 1);
  }
  
  doc->move_to(doc, i);
  
  {
    Expr line = Application::interrupt_wait(Plus(Symbol(richmath_System_DollarLine), 1));
    Expr dlvl = Application::interrupt_wait(Symbol(PMATH_SYMBOL_DIALOGLEVEL));
    
    String label = String("in [");
    if(dlvl == PMATH_FROM_INT32(1))
      label = String("(Dialog) ") + label;
    else if(dlvl != PMATH_FROM_INT32(0))
      label = String("(Dialog ") + dlvl.to_string() + String(") ") + label;
      
    if(!section->style)
      section->style = new Style;
    section->style->set(SectionLabel, label + line.to_string() + String("]:"));
    section->invalidate();
  }
  
  Expr boxes = section->content()->to_pmath(BoxOutputFlags::Parseable | BoxOutputFlags::WithDebugInfo);
  Expr eval_fun = section->get_style(SectionEvaluationFunction);
  if(eval_fun != PMATH_SYMBOL_IDENTITY)
    boxes = Call(std::move(eval_fun), std::move(boxes));
  
  Server::local_server->run_boxes(std::move(boxes));
  
  doc->native()->running_state_changed();
  
  return true;
}

void InputJob::returned(Expr expr) {
  returned_boxes(to_boxes(expr));
}

void InputJob::returned_boxes(Expr expr) {
  if(!String(expr).equals("/\\/")) {
    Application::gui_print_section(generate_section("Output", expr));
  }
}

void InputJob::end() {
  if(auto doc = dynamic_cast<Document *>(Box::find(_position.document_id)))
    doc->native()->running_state_changed();
}

void InputJob::dequeued() {
  if(auto section = dynamic_cast<MathSection *>(Box::find(_position.section_id))) {
    section->evaluating = false;
    section->request_repaint_all();
  }
}

//} ... class InputJob

//{ class EvaluationJob ...

EvaluationJob::EvaluationJob(Expr expr, Box *box)
  : InputJob(0),
    _expr(expr)
{
  SET_BASE_DEBUG_TAG(typeid(*this).name());
  _position = EvaluationPosition(box);
}

bool EvaluationJob::start() {
  auto doc = dynamic_cast<Document*>(Box::find(_position.document_id));
  
  Server::local_server->run(_expr);
  
  if(doc)
    doc->native()->running_state_changed();
    
  return true;
}

void EvaluationJob::end() {
  InputJob::end();
}

//} ... class EvaluationJob

//{ class DynamicEvaluationJob ...

DynamicEvaluationJob::DynamicEvaluationJob(Expr info, Expr expr, Box *box)
  : EvaluationJob(expr, box),
    _info(info),
    old_observer_id(FrontEndReference::None)
{
  SET_BASE_DEBUG_TAG(typeid(*this).name());
}

bool DynamicEvaluationJob::start() {
  old_observer_id = Dynamic::current_observer_id;
  Dynamic::current_observer_id = _position.box_id;
  return EvaluationJob::start();
}

void DynamicEvaluationJob::end() {
  EvaluationJob::end();
  Dynamic::current_observer_id = old_observer_id;
}

void DynamicEvaluationJob::returned(Expr expr) {
  Box *box = FrontEndObject::find_cast<Box>(_position.box_id);
  
  if(box)
    box->dynamic_finished(_info, expr);
}

//} ... class DynamicEvaluationJob

//{ class ReplacementJob ...

ReplacementJob::ReplacementJob(MathSequence *seq, int start, int end)
  : InputJob(0),
    have_result(false),
    selection_start(start),
    selection_end(end)
{
  SET_BASE_DEBUG_TAG(typeid(*this).name());
  
  assert(0 <= start);
  assert(start <= end);
  
  _position = EvaluationPosition(seq);
}

bool ReplacementJob::start() {
  auto doc = dynamic_cast<Document*>(Box::find(_position.document_id));
  auto section = dynamic_cast<Section*>(Box::find(_position.section_id));
  auto sequence = dynamic_cast<MathSequence*>(Box::find(_position.box_id));
  
  if( !section                           ||
      !doc                               ||
      !sequence                          ||
      section->parent() != doc           ||
      selection_end > sequence->length() ||
      !sequence->get_style(Editable))
  {
    if(section) {
      section->evaluating = false;
      section->invalidate();
    }
    
    return false;
  }
  
  Expr boxes = sequence->to_pmath(BoxOutputFlags::Parseable | BoxOutputFlags::WithDebugInfo, selection_start, selection_end);
  Expr eval_fun = section->get_style(SectionEvaluationFunction);
  if(eval_fun != PMATH_SYMBOL_IDENTITY)
    boxes = Call(std::move(eval_fun), std::move(boxes));
  
  Server::local_server->run_boxes(std::move(boxes));
    
  doc->native()->running_state_changed();
  
  return true;
}

void ReplacementJob::returned_boxes(Expr expr) {
  if(!expr.is_null()) {
    have_result = true;
    result = expr;
  }
}

void ReplacementJob::end() {
  auto doc = dynamic_cast<Document*>(Box::find(_position.document_id));
  auto section = dynamic_cast<Section*>(Box::find(_position.section_id));
  auto sequence = dynamic_cast<MathSequence*>(Box::find(_position.box_id));
  
  if(section) {
    section->evaluating = false;
    section->invalidate();
  }
  
  if( have_result                   &&
      result != PMATH_SYMBOL_FAILED &&
      doc                           &&
      section                       &&
      sequence                      &&
      section->parent() == doc      &&
      selection_end <= sequence->length())
  {
    sequence->remove(selection_start, selection_end);
    
    BoxInputFlags opts = BoxInputFlags::Default;
    if(sequence->get_style(AutoNumberFormating))
      opts |= BoxInputFlags::FormatNumbers;
      
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
