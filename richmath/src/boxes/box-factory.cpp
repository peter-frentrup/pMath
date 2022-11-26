#include <boxes/box-factory.h>

#include <boxes/graphics/graphicsbox.h>
#include <boxes/buttonbox.h>
#include <boxes/checkboxbox.h>
#include <boxes/dynamicbox.h>
#include <boxes/dynamiclocalbox.h>
#include <boxes/errorbox.h>
#include <boxes/fillbox.h>
#include <boxes/fractionbox.h>
#include <boxes/framebox.h>
#include <boxes/gridbox.h>
#include <boxes/inputfieldbox.h>
#include <boxes/interpretationbox.h>
#include <boxes/mathsequence.h>
#include <boxes/numberbox.h>
#include <boxes/openerbox.h>
#include <boxes/ownerbox.h>
#include <boxes/panebox.h>
#include <boxes/panelbox.h>
#include <boxes/paneselectorbox.h>
#include <boxes/progressindicatorbox.h>
#include <boxes/radicalbox.h>
#include <boxes/radiobuttonbox.h>
#include <boxes/section.h>
#include <boxes/setterbox.h>
#include <boxes/sliderbox.h>
#include <boxes/stylebox.h>
#include <boxes/subsuperscriptbox.h>
#include <boxes/templatebox.h>
#include <boxes/textsequence.h>
#include <boxes/tooltipbox.h>
#include <boxes/transformationbox.h>
#include <boxes/underoverscriptbox.h>


using namespace richmath;

extern pmath_symbol_t richmath_System_BoxData;
extern pmath_symbol_t richmath_System_ButtonBox;
extern pmath_symbol_t richmath_System_CheckboxBox;
extern pmath_symbol_t richmath_System_DynamicBox;
extern pmath_symbol_t richmath_System_DynamicLocalBox;
extern pmath_symbol_t richmath_System_FillBox;
extern pmath_symbol_t richmath_System_FractionBox;
extern pmath_symbol_t richmath_System_FrameBox;
extern pmath_symbol_t richmath_System_GraphicsBox;
extern pmath_symbol_t richmath_System_GridBox;
extern pmath_symbol_t richmath_System_InputFieldBox;
extern pmath_symbol_t richmath_System_InterpretationBox;
extern pmath_symbol_t richmath_System_List;
extern pmath_symbol_t richmath_System_OpenerBox;
extern pmath_symbol_t richmath_System_OverscriptBox;
extern pmath_symbol_t richmath_System_PaneBox;
extern pmath_symbol_t richmath_System_PanelBox;
extern pmath_symbol_t richmath_System_PaneSelectorBox;
extern pmath_symbol_t richmath_System_ProgressIndicatorBox;
extern pmath_symbol_t richmath_System_RadicalBox;
extern pmath_symbol_t richmath_System_RadioButtonBox;
extern pmath_symbol_t richmath_System_RotationBox;
extern pmath_symbol_t richmath_System_Section;
extern pmath_symbol_t richmath_System_SetterBox;
extern pmath_symbol_t richmath_System_SliderBox;
extern pmath_symbol_t richmath_System_SqrtBox;
extern pmath_symbol_t richmath_System_StringBox;
extern pmath_symbol_t richmath_System_StyleBox;
extern pmath_symbol_t richmath_System_StyleData;
extern pmath_symbol_t richmath_System_SubscriptBox;
extern pmath_symbol_t richmath_System_SubsuperscriptBox;
extern pmath_symbol_t richmath_System_SuperscriptBox;
extern pmath_symbol_t richmath_System_TagBox;
extern pmath_symbol_t richmath_System_TemplateBox;
extern pmath_symbol_t richmath_System_TemplateSlot;
extern pmath_symbol_t richmath_System_TextData;
extern pmath_symbol_t richmath_System_TooltipBox;
extern pmath_symbol_t richmath_System_TransformationBox;
extern pmath_symbol_t richmath_System_UnderscriptBox;
extern pmath_symbol_t richmath_System_UnderoverscriptBox;


static Box *finish_create_or_error(Box *box, Expr expr, BoxInputFlags options) {
  if(!box->try_load_from_object(expr, options)) {
    box->safe_destroy();
    return new ErrorBox(expr);
  }
  else
    return box;
}

//{ class BoxFactory ...

Box *BoxFactory::create_empty_box(LayoutKind layout_kind, Expr expr) {
  if(String s = expr) {
    if(s.length() == 1 && s[0] == PMATH_CHAR_BOX)
      return new ErrorBox(s);
    
    return new InlineSequenceBox(create_sequence(layout_kind));
  }
  
  if(!expr.is_expr())
    return new ErrorBox(expr);
  
  Expr head = expr[0];
  
  if(head == richmath_System_List || head == richmath_System_StringBox || head == PMATH_NULL) {
    return new InlineSequenceBox(new MathSequence);
  }
  
  if(head == richmath_System_BoxData)              return new InlineSequenceBox(new MathSequence);
  if(head == richmath_System_TextData)             return new InlineSequenceBox(new TextSequence);
  if(head == richmath_System_ButtonBox)            return new ButtonBox(create_sequence(layout_kind));
  if(head == richmath_System_CheckboxBox)          return new CheckboxBox();
  if(head == richmath_System_DynamicBox)           return new DynamicBox(create_sequence(layout_kind));
  if(head == richmath_System_DynamicLocalBox)      return new DynamicLocalBox(create_sequence(layout_kind));
  if(head == richmath_System_FillBox)              return new FillBox(create_sequence(layout_kind));
  if(head == richmath_System_FractionBox)          return new FractionBox(); // TODO: consider layout_kind
  if(head == richmath_System_FrameBox)             return new FrameBox(create_sequence(layout_kind));
  if(head == richmath_System_GraphicsBox)          return new GraphicsBox();
  if(head == richmath_System_GridBox)              return new GridBox(layout_kind);
  if(head == richmath_System_InputFieldBox)        return new InputFieldBox(create_sequence(layout_kind));
  if(head == richmath_System_InterpretationBox)    return new InterpretationBox(create_sequence(layout_kind));
  if(head == richmath_System_PaneBox)              return new PaneBox(create_sequence(layout_kind));
  if(head == richmath_System_PanelBox)             return new PanelBox(create_sequence(layout_kind));
  if(head == richmath_System_PaneSelectorBox)      return new PaneSelectorBox();
  if(head == richmath_System_ProgressIndicatorBox) return new ProgressIndicatorBox();
  if(head == richmath_System_RadicalBox)           return new RadicalBox(create_sequence(layout_kind));
  if(head == richmath_System_RadioButtonBox)       return new RadioButtonBox();
  if(head == richmath_System_RotationBox)          return new RotationBox(create_sequence(layout_kind));
  if(head == richmath_System_SetterBox)            return new SetterBox(create_sequence(layout_kind));
  if(head == richmath_System_SliderBox)            return new SliderBox();
  if(head == richmath_System_SubscriptBox)         return new SubsuperscriptBox(); // TODO: consider layout_kind
  if(head == richmath_System_SubsuperscriptBox)    return new SubsuperscriptBox(); // TODO: consider layout_kind
  if(head == richmath_System_SuperscriptBox)       return new SubsuperscriptBox(); // TODO: consider layout_kind
  if(head == richmath_System_SqrtBox)              return new RadicalBox(create_sequence(layout_kind));
  if(head == richmath_System_StyleBox)             return new StyleBox(create_sequence(layout_kind));
  if(head == richmath_System_TagBox)               return new TagBox(create_sequence(layout_kind));
  if(head == richmath_System_TemplateBox)          return new TemplateBox(create_sequence(layout_kind));
  if(head == richmath_System_TooltipBox)           return new TooltipBox(create_sequence(layout_kind));
  if(head == richmath_System_TransformationBox)    return new TransformationBox(create_sequence(layout_kind));
  if(head == richmath_System_OpenerBox)            return new OpenerBox();
  if(head == richmath_System_OverscriptBox)        return new UnderoverscriptBox(new MathSequence, nullptr,          new MathSequence);
  if(head == richmath_System_UnderoverscriptBox)   return new UnderoverscriptBox(new MathSequence, new MathSequence, new MathSequence);
  if(head == richmath_System_UnderscriptBox)       return new UnderoverscriptBox(new MathSequence, new MathSequence, nullptr);
  if(head == richmath_FE_NumberBox)                return new NumberBox();
  if(head == richmath_System_TemplateSlot)         return new TemplateBoxSlot(create_sequence(layout_kind));
  
  return new ErrorBox(PMATH_CPP_MOVE(expr));
}

AbstractSequence *BoxFactory::create_sequence(LayoutKind kind) {
  switch(kind) {
    case LayoutKind::Math: return new MathSequence;
    case LayoutKind::Text: return new TextSequence;
  }
  
  RICHMATH_ASSERT(0 && "not reached");
  return new MathSequence;
}

Section *BoxFactory::create_empty_section(SectionKind kind) {
  switch(kind) {
    case SectionKind::Error: break;
    case SectionKind::Math:  return new MathSection();
    case SectionKind::Style: return new StyleDataSection();
    case SectionKind::Text:  return new TextSection();
  }
  return new ErrorSection(Expr());
}

SectionKind BoxFactory::kind_of_section(Expr expr) {
  if(expr[0] == richmath_System_Section) {
    Expr content = expr[1];
    
    if(content.is_string()) return SectionKind::Text;
    
    Expr content_head = content[0];
    if(content_head == richmath_System_BoxData)   return SectionKind::Math;
    if(content_head == richmath_System_StyleData) return SectionKind::Style;
    if(content_head == richmath_System_TextData)  return SectionKind::Text;
    if(content_head == richmath_System_List)      return SectionKind::Text;
  }
  
  return SectionKind::Error;
}

//} ... class BoxFactory
