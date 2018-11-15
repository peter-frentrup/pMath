#include <gui/common-tooltips.h>

#include <boxes/section.h>
#include <gui/document.h>
#include <util/style.h>


using namespace pmath;
using namespace richmath;

extern pmath_symbol_t richmath_System_BoxData;
extern pmath_symbol_t richmath_System_ButtonBox;
extern pmath_symbol_t richmath_System_ButtonFrame;
extern pmath_symbol_t richmath_System_Section;
extern pmath_symbol_t richmath_System_TemplateBox;

void CommonTooltips::load_content(
  Document              *doc, 
  const Expr            &boxes, 
  SharedPtr<Stylesheet>  stylesheet
) {
  if(stylesheet)
    doc->stylesheet(stylesheet);
  
  Section *section = nullptr;
  if(doc->length() > 0) {
    doc->remove(1, doc->length());
    section = doc->section(0);
  }
  
  Expr section_boxes = Call(
    Symbol(richmath_System_Section),
    Call(
      Symbol(richmath_System_BoxData),
      Call(
        Symbol(richmath_System_TemplateBox),
        List(boxes),
        String("TooltipContent"))),
    String("TooltipWindowSection"));
  
  if(!section) {
    section = Section::create_from_object(section_boxes);
    doc->insert(0, section);
  }
  else if(!section->try_load_from_object(section_boxes, BoxInputFlags::Default)) {
    section = Section::create_from_object(section_boxes);
    doc->swap(0, section)->safe_destroy();
  }
}
