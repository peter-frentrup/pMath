#include <eval/job.h>

#include <boxes/graphics/graphicsbox.h>
#include <boxes/mathsequence.h>
#include <boxes/section.h>

#include <eval/application.h>
#include <eval/dynamic.h>
#include <eval/eval-contexts.h>
#include <eval/server.h>

#include <gui/document.h>
#include <gui/native-widget.h>

using namespace richmath;

namespace richmath { namespace strings {
  extern String Output;
}}

extern pmath_symbol_t richmath_System_DollarDialogLevel;
extern pmath_symbol_t richmath_System_DollarFailed;
extern pmath_symbol_t richmath_System_DollarLine;
extern pmath_symbol_t richmath_System_Identity;

//{ class EvaluationPosition ...

EvaluationPosition::EvaluationPosition(FrontEndReference doc, FrontEndReference sect, FrontEndReference obj)
  : document_id(doc),
    section_id(sect),
    object_id(obj)
{
}

EvaluationPosition::EvaluationPosition(FrontEndObject *obj)
  : document_id(FrontEndReference::None),
    section_id(FrontEndReference::None),
    object_id(FrontEndReference::None)
{
  if(obj) {
    object_id = obj->id();

    if(Section *sect = Box::find_nearest_parent<Section>(obj)) {
      section_id = sect->id();
      
      if(Document *doc = sect->find_parent<Document>(false))
        document_id = doc->id();
    }
  }
}

//} ... class EvaluationPosition

//{ class Job ...

Job::Job()
  : Shareable()
{
  SET_BASE_DEBUG_TAG(typeid(*this).name());
}

void Job::remember_output_style(Section *old_sect) {
  if(default_graphics_options.is_null()) {
    if(auto seq_sect = dynamic_cast<AbstractSequenceSection *>(old_sect)) {
      if(seq_sect->content()->length() == 1 && seq_sect->content()->count() == 1) {
        if(GraphicsBox *gb = dynamic_cast<GraphicsBox *>(seq_sect->content()->item(0))) {
          default_graphics_options = gb->get_user_options();
        }
      }
    }
  }
  
  String base_style_name;
  if(old_sect && old_sect->style->get(BaseStyleName, &base_style_name) && !default_section_styles.lookup(base_style_name, Expr())) {
    if(default_section_styles.is_null()) {
      default_section_styles = List();
    }
    Gather g;
    old_sect->style->remove(SectionLabel);
    old_sect->style->emit_to_pmath();
    default_section_styles.append(Rule(base_style_name, g.end()));
  }
}

void Job::adjust_output_style(Section *sect) {
  apply_default_graphics_options(sect);
  apply_generated_section_styles(sect);
}

void Job::apply_default_graphics_options(Section *sect) {
  if(auto seq_sect = dynamic_cast<AbstractSequenceSection *>(sect)) {
    if(seq_sect->content()->length() == 1 && seq_sect->content()->count() == 1) {
      if(GraphicsBox *gb = dynamic_cast<GraphicsBox *>(seq_sect->content()->item(0))) {
        gb->set_user_default_options(default_graphics_options);
      }
    }
  }
}

void Job::apply_generated_section_styles(Section *sect) {
  String base_style_name;
  if(sect->style && sect->style->get(BaseStyleName, &base_style_name)) {
    if(Section *eval_sect = FrontEndObject::find_cast<Section>(_position.section_id)) {
      Expr rules = eval_sect->get_style(GeneratedSectionStyles);
      
      Expr base_style;
      if(rules.try_lookup(base_style_name, base_style)) {
        sect->style->remove(BaseStyleName);
        
        // TODO: Support short-hand notation for all directives., not only single "StyleName". 
        //       Should also allow {"StyleName", rules}, like Mma.
        sect->style->add_pmath(base_style); 
      }
    }
    
    if(sect->style->get(BaseStyleName, &base_style_name)) {
      Expr base_style;
      if(default_section_styles.try_lookup(base_style_name, base_style)) {
        sect->style->add_pmath(base_style); 
      }
    }
  }
}

//} ... class Job

//{ class InputJob ...

InputJob::InputJob(MathSection *section)
  : Job()
{
  SET_BASE_DEBUG_TAG(typeid(*this).name());
  _position = EvaluationPosition(section);
}

void InputJob::enqueued() {
  auto doc = dynamic_cast<Document*>(Box::find(_position.document_id));
  
  if(auto section = dynamic_cast<Section*>(Box::find(_position.section_id))) {
    section->evaluating++;
    if(section->evaluating == 1) {
      if(doc) {
        doc->move_to(doc, section->index() + 1);
      }
      
      section->invalidate(); // request_repaint_all doeas not always cover the section bracket ?!?
    }
  }
}

bool InputJob::start() {
  auto section = dynamic_cast<MathSection*>(Box::find(_position.section_id));
  auto doc = dynamic_cast<Document*>(Box::find(_position.document_id));
  
  if(!section || !doc || section->parent() != doc) {
    if(section) {
      section->evaluating--;
      if(!section->evaluating)
        section->invalidate(); // request_repaint_all doeas not always cover the section bracket ?!?
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
  
  set_context();
  {
    Expr line_and_dlvl = Application::interrupt_wait(
      List(
        Plus(Symbol(richmath_System_DollarLine), 1),
        Symbol(richmath_System_DollarDialogLevel)));
    Expr line = line_and_dlvl[1];
    Expr dlvl = line_and_dlvl[2];
    
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
  
  Expr boxes = section->content()->to_pmath(BoxOutputFlags::Parseable | BoxOutputFlags::WithDebugMetadata);
  Expr eval_fun = section->get_style(SectionEvaluationFunction);
  if(eval_fun != richmath_System_Identity)
    boxes = Call(PMATH_CPP_MOVE(eval_fun), PMATH_CPP_MOVE(boxes));
  
  boxes = EvaluationContexts::prepare_namespace_for_current_context(PMATH_CPP_MOVE(boxes));
  Server::local_server->run_boxes(PMATH_CPP_MOVE(boxes));
  
  doc->native()->running_state_changed();
  
  return true;
}

void InputJob::returned(Expr expr) {
  returned_boxes(evaluate_to_boxes(expr));
}

void InputJob::returned_boxes(Expr expr) {
  if(!String(expr).equals("/\\/")) {
    Application::gui_print_section(generate_section(strings::Output, expr));
  }
}

void InputJob::end() {
  if(auto doc = dynamic_cast<Document *>(Box::find(_position.document_id)))
    doc->native()->running_state_changed();
}

void InputJob::dequeued() {
  if(auto section = dynamic_cast<MathSection *>(Box::find(_position.section_id))) {
    section->evaluating--;
    if(!section->evaluating)
      section->invalidate(); // request_repaint_all doeas not always cover the section bracket ?!?
  }
}

void InputJob::set_context() {
  auto obj = FrontEndObject::find(_position.object_id);
  if(!obj)
    obj = FrontEndObject::find(_position.section_id);
  if(!obj)
    obj = FrontEndObject::find(_position.document_id);
  
  EvaluationContexts::set_context(EvaluationContexts::resolve_context(dynamic_cast<StyledObject*>(obj)));
}

//} ... class InputJob

//{ class EvaluationJob ...

EvaluationJob::EvaluationJob(Expr expr, FrontEndObject *obj)
  : InputJob(nullptr),
    _expr(expr)
{
  SET_BASE_DEBUG_TAG(typeid(*this).name());
  _position = EvaluationPosition(obj);
}

bool EvaluationJob::start() {
  auto doc = dynamic_cast<Document*>(Box::find(_position.document_id));
  
  set_context();
  
  //Server::local_server->run(EvaluationContexts::prepare_evaluation(_expr));
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

DynamicEvaluationJob::DynamicEvaluationJob(Expr info, Expr expr, FrontEndObject *obj)
  : EvaluationJob(expr, obj),
    _info(info),
    old_observer_id(FrontEndReference::None)
{
  SET_BASE_DEBUG_TAG(typeid(*this).name());
}

bool DynamicEvaluationJob::start() {
  set_context();
  
  old_observer_id = Dynamic::current_observer_id;
  Dynamic::current_observer_id = _position.object_id;
  return EvaluationJob::start();
}

void DynamicEvaluationJob::end() {
  EvaluationJob::end();
  Dynamic::current_observer_id = old_observer_id;
}

void DynamicEvaluationJob::returned(Expr expr) {
  if(auto obj = FrontEndObject::find(_position.object_id))
    obj->dynamic_finished(_info, expr);
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
  
  RICHMATH_ASSERT(0 <= start);
  RICHMATH_ASSERT(start <= end);
  
  _position = EvaluationPosition(seq);
}

bool ReplacementJob::start() {
  auto doc = dynamic_cast<Document*>(Box::find(_position.document_id));
  auto section = dynamic_cast<Section*>(Box::find(_position.section_id));
  auto sequence = dynamic_cast<MathSequence*>(Box::find(_position.object_id));
  
  if( !section                           ||
      !doc                               ||
      !sequence                          ||
      section->parent() != doc           ||
      selection_end > sequence->length() ||
      !sequence->editable())
  {
    if(section) {
      section->evaluating--;
      if(!section->evaluating)
        section->invalidate(); // request_repaint_all doeas not always cover the section bracket ?!?
    }
    
    return false;
  }
  
  set_context();
  
  Expr boxes = sequence->to_pmath(BoxOutputFlags::Parseable | BoxOutputFlags::WithDebugMetadata, selection_start, selection_end);
  Expr eval_fun = section->get_style(SectionEvaluationFunction);
  if(eval_fun != richmath_System_Identity)
    boxes = Call(PMATH_CPP_MOVE(eval_fun), PMATH_CPP_MOVE(boxes));
  
  boxes = EvaluationContexts::prepare_namespace_for_current_context(PMATH_CPP_MOVE(boxes));
  Server::local_server->run_boxes(PMATH_CPP_MOVE(boxes));
    
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
  auto sequence = dynamic_cast<MathSequence*>(Box::find(_position.object_id));
  
  if(section) {
    section->evaluating--;
    if(!section->evaluating)
      section->invalidate(); // request_repaint_all doeas not always cover the section bracket ?!?
  }
  
  if( have_result                            &&
      result != richmath_System_DollarFailed &&
      doc                                    &&
      section                                &&
      sequence                               &&
      section->parent() == doc               &&
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
