#ifndef RICHMATH__UTIL__STYLE_H__INCLUDED
#define RICHMATH__UTIL__STYLE_H__INCLUDED

#include <eval/observable.h>
#include <graphics/color.h>
#include <graphics/symbolic-length.h>
#include <util/autobool.h>
#include <util/multimap.h>
#include <util/pmath-extra.h>
#include <util/sharedptr.h>

namespace richmath {
  class StyledObject;
  
  bool get_factor_of_scaled(Expr expr, double *value);
  double convert_float_to_nice_double(float f);
  
  enum class DefaultStyleOptionOffsets {
    None = 0,
    ButtonBox            = 0x00100000,
    DynamicBox           = 0x00200000,
    DynamicLocalBox      = 0x00300000,
    FillBox              = 0x00400000,
    FractionBox          = 0x00500000,
    FrameBox             = 0x00600000,
    GraphicsBox          = 0x00700000,
    GridBox              = 0x00800000,
    InputFieldBox        = 0x00900000,
    OverscriptBox        = 0x00A00000,
    PaneBox              = 0x00B00000,
    PanelBox             = 0x00C00000,
    PaneSelectorBox      = 0x00D00000,
    ProgressIndicatorBox = 0x00E00000,
    SetterBox            = 0x00F00000,
    SliderBox            = 0x01000000,
    TemplateBox          = 0x01100000,
    UnderoverscriptBox   = 0x01200000,
    UnderscriptBox       = 0x01300000,
  };
  
  enum ClosingActionValues {
    ClosingActionDelete = 0,
    ClosingActionHide   = 1,
  };
  
  enum ControlPlacementKind {
    ControlPlacementKindBottom,
    ControlPlacementKindLeft,
    ControlPlacementKindRight,
    ControlPlacementKindTop,
  };
  
  enum ImageSizeActionValues {
    ImageSizeActionClip = 0,
    ImageSizeActionShrinkToFit = 1,
    ImageSizeActionResizeToFit = 2,
  };
  
  enum ObserverKind {
    ObserverKindNone  = 0x0,
    ObserverKindSelf  = 0x1,
    ObserverKindOther = 0x2,
    ObserverKindBoth  = ObserverKindSelf | ObserverKindOther,
  };
  
  enum DynamicUpdateKind: uint8_t {
    DynamicUpdateKindNone   = 0,
    DynamicUpdateKindPaint  = 1,
    DynamicUpdateKindLayout = 2,
  };
  
  enum Evaluator: uint8_t {
    Simple = 0,
    Full   = 1,
  };
  
  enum RemovalConditionFlags {
    RemovalConditionFlagSelectionExit          = 0x01,
    RemovalConditionFlagMouseExit              = 0x02,
    RemovalConditionFlagMouseClickOutside      = 0x04,
    RemovalConditionFlagMouseClickOutsidePopup = 0x08,
    RemovalConditionFlagParentChanged          = 0x10,
    RemovalConditionFlagWindowFocusLost        = 0x20,
  };
  
  enum ColorStyleOptionName {
    Background = 0x00000,
    ColorForGraphics, // TODO: rename to System`GraphicsColor
    EdgeColor,
    FontColor,
    SectionFrameColor,
    
    CharacterNameSyntaxColor,
    CommentSyntaxColor,
    ExcessOrMissingArgumentSyntaxColor,
    FunctionLocalVariableSyntaxColor,
    FunctionNameSyntaxColor,
    ImplicitOperatorSyntaxColor,
    KeywordSymbolSyntaxColor,
    LocalScopeConflictSyntaxColor,
    LocalVariableSyntaxColor,
    PatternVariableSyntaxColor,
    SectionInsertionPointColor,
    StringSyntaxColor,
    SymbolShadowingSyntaxColor,
    SyntaxErrorColor,
    UndefinedSymbolSyntaxColor,
    UnknownOptionSyntaxColor,
    
    ContainingCallBackgroundColor,
    InlineAutoCompletionBackgroundColor,
    InlineSectionEditingBackgroundColor,
    MatchingBracketBackgroundColor,
    OccurenceBackgroundColor,
    FrameBoxDefaultBackground    = Background + (int)DefaultStyleOptionOffsets::FrameBox,
    GraphicsBoxDefaultBackground = Background + (int)DefaultStyleOptionOffsets::GraphicsBox,
  };
  
  enum IntStyleOptionName {
    AllowScriptLevelChange = 0x10000, // bool
    Antialiasing, // AutoBoolXXX
    AutoDelete,
    AutoNumberFormating,
    AutoSpacing,
    CapForm,       // CapFormXXX
    ClosingAction, // ClosingActionXXX
    ContentPadding,
    ContinuousAction,
    ControlPlacement, // ControlPlacementKindXXX
    DebugColorizeChanges, // bool
    DebugFollowMouse,     // bool
    DebugSelectionBounds, // bool
    DrawEdges,            // bool
    Editable,
    Enabled, // AutoBoolXXX
    Evaluatable,
    ImageSizeAction, // ImageSizeActionXXX
    InternalDefinesEvaluationContext, // bool
    InternalHasModifiedWindowOption,
    InternalHasPendingDynamic,
    InternalHasNewBaseStyle,
    InternalRegisteredBoxReference, // FrontEndReference
    InternalRequiresChildResize,
    InternalUsesCurrentValueOfMouseOver, // ObserverKindXXX
    LimitsPositioning, // AutoBoolXXX
    LineBreakWithin,
    MenuCommandKey,
    MenuSortingValue,
    Placeholder,
    PrintPrecision,
    RemovalConditions, // 0 or more of RemovalConditionFlagXXX
    ReturnCreatesNewSection,
    Saveable,
    ScriptLevel,
    SectionEditDuplicate,
    SectionEditDuplicateMakesCopy,
    SectionGenerated,
    SectionLabelAutoDelete,
    Selectable, // AutoBoolXXX
    ShowAutoStyles,
    ShowContents,
    ShowSectionBracket, // AutoBoolXXX
    ShowStringCharacters,
    SingleLetterItalics,
    StripOnInput,
    SurdForm,
    SynchronousUpdating, // AutoBoolXXX
    Visible,
    WholeSectionGroupOpener,
    
    FontSlant,
    FontWeight,
    
    ButtonFrame, // -1 = Automatic,  other: ContainerType value
    ButtonSource, // ButtonSourceXXX
    WindowFrame, // WindowFrameType
    
    ButtonBoxDefaultContentPadding = ContentPadding + (int)DefaultStyleOptionOffsets::ButtonBox,
    ButtonBoxDefaultEnabled        = Enabled        + (int)DefaultStyleOptionOffsets::ButtonBox,
    ButtonBoxDefaultButtonFrame    = ButtonFrame    + (int)DefaultStyleOptionOffsets::ButtonBox,
    ButtonBoxDefaultButtonSource   = ButtonSource   + (int)DefaultStyleOptionOffsets::ButtonBox,
    
    DynamicBoxDefaultSelectable          = Selectable          + (int)DefaultStyleOptionOffsets::DynamicBox,
    DynamicBoxDefaultSynchronousUpdating = SynchronousUpdating + (int)DefaultStyleOptionOffsets::DynamicBox,
    
    FillBoxDefaultStripOnInput = StripOnInput + (int)DefaultStyleOptionOffsets::FillBox,
    
    FractionBoxDefaultAllowScriptLevelChange = AllowScriptLevelChange + (int)DefaultStyleOptionOffsets::FractionBox,
    
    FrameBoxDefaultContentPadding = ContentPadding + (int)DefaultStyleOptionOffsets::FrameBox,
    
    GridBoxDefaultAllowScriptLevelChange = AllowScriptLevelChange + (int)DefaultStyleOptionOffsets::GridBox,
    
    InputFieldBoxDefaultContentPadding   = ContentPadding   + (int)DefaultStyleOptionOffsets::InputFieldBox,
    InputFieldBoxDefaultContinuousAction = ContinuousAction + (int)DefaultStyleOptionOffsets::InputFieldBox,
    InputFieldBoxDefaultEnabled          = Enabled          + (int)DefaultStyleOptionOffsets::InputFieldBox,
    InputFieldBoxDefaultLineBreakWithin  = LineBreakWithin  + (int)DefaultStyleOptionOffsets::InputFieldBox,

    OverscriptBoxDefaultLimitsPositioning      = LimitsPositioning + (int)DefaultStyleOptionOffsets::OverscriptBox,
    UnderoverscriptBoxDefaultLimitsPositioning = LimitsPositioning + (int)DefaultStyleOptionOffsets::UnderoverscriptBox,
    UnderscriptBoxDefaultLimitsPositioning     = LimitsPositioning + (int)DefaultStyleOptionOffsets::UnderscriptBox,
    
    PaneBoxDefaultImageSizeAction = ImageSizeAction + (int)DefaultStyleOptionOffsets::PaneBox,
    PaneBoxDefaultLineBreakWithin = LineBreakWithin + (int)DefaultStyleOptionOffsets::PaneBox,
    
    PanelBoxDefaultContentPadding = ContentPadding + (int)DefaultStyleOptionOffsets::PanelBox,
    PanelBoxDefaultEnabled        = Enabled        + (int)DefaultStyleOptionOffsets::PanelBox,
    
    SetterBoxDefaultContentPadding = ContentPadding + (int)DefaultStyleOptionOffsets::SetterBox,
    SetterBoxDefaultEnabled        = Enabled        + (int)DefaultStyleOptionOffsets::SetterBox,
    SetterBoxDefaultButtonFrame    = ButtonFrame    + (int)DefaultStyleOptionOffsets::SetterBox,
    
    SliderBoxDefaultContinuousAction = ContinuousAction + (int)DefaultStyleOptionOffsets::SliderBox,
    SliderBoxDefaultEnabled          = Enabled          + (int)DefaultStyleOptionOffsets::SliderBox,
  };
  
  enum {
    ButtonSourceAutomatic      = 0,
    ButtonSourceButtonContents = 1,
    ButtonSourceButtonData     = 2,
    ButtonSourceButtonBox      = 3,
    ButtonSourceFrontEndObject = 4
  };
  
  enum {
    FontWeightPlain = 0,
    FontWeightBold  = 100
  };
  
  enum WindowFrameType {
    WindowFrameNormal      = 0,
    WindowFramePalette     = 1,
    WindowFrameDialog      = 2,
    WindowFrameNone        = 3,
    WindowFrameThin        = 4,
    WindowFrameThinCallout = 5,
  };
  
  enum FloatStyleOptionName {
    AspectRatio = 0x20000, // greater than any IntStyleOptionName value
    
    Magnification,
    
    FillBoxWeight, // > 0
    
    GridBoxColumnSpacing,
    GridBoxRowSpacing,
    
    ContainingCallHighlightOpacity,       // 0 .. 1
    InlineAutoCompletionHighlightOpacity, // 0 .. 1
    InlineSectionEditingHighlightOpacity, // 0 .. 1
    MatchingBracketHighlightOpacity,      // 0 .. 1
    OccurenceHighlightOpacity,            // 0 .. 1
    
    WindowProgress,
    
    SectionGroupPrecedence,
    
    FillBoxDefaultFillBoxWeight   = FillBoxWeight + (int)DefaultStyleOptionOffsets::FillBox,
    GraphicsBoxDefaultAspectRatio = AspectRatio   + (int)DefaultStyleOptionOffsets::GraphicsBox,
  };
  
  enum LengthStyleOptionName {
    FontSize = 0x30000, // greater than any FloatStyleOptionName value
  
    ImageSizeCommon,
    ImageSizeHorizontal,
    ImageSizeVertical,
    
    FrameMarginLeft,
    FrameMarginRight,
    FrameMarginTop,
    FrameMarginBottom,
    
    PlotRangePaddingLeft,
    PlotRangePaddingRight,
    PlotRangePaddingTop,
    PlotRangePaddingBottom,
    
    SectionMarginLeft,
    SectionMarginRight,
    SectionMarginTop,
    SectionMarginBottom,
    
    SectionFrameLeft,
    SectionFrameRight,
    SectionFrameTop,
    SectionFrameBottom,
    
    SectionFrameMarginLeft,
    SectionFrameMarginRight,
    SectionFrameMarginTop,
    SectionFrameMarginBottom,
    
    SectionFrameLabelMarginLeft,   // used for CellDingbat
    SectionFrameLabelMarginRight,  // not yet used
    SectionFrameLabelMarginTop,    // not yet used
    SectionFrameLabelMarginBottom, // not yet used
    
    EdgeThickness,
    PointSize,
    Thickness,
    
    ButtonBoxDefaultFrameMarginLeft     = FrameMarginLeft     + (int)DefaultStyleOptionOffsets::ButtonBox,
    ButtonBoxDefaultFrameMarginRight    = FrameMarginRight    + (int)DefaultStyleOptionOffsets::ButtonBox,
    ButtonBoxDefaultFrameMarginTop      = FrameMarginTop      + (int)DefaultStyleOptionOffsets::ButtonBox,
    ButtonBoxDefaultFrameMarginBottom   = FrameMarginBottom   + (int)DefaultStyleOptionOffsets::ButtonBox,
    ButtonBoxDefaultImageSizeCommon     = ImageSizeCommon     + (int)DefaultStyleOptionOffsets::ButtonBox,
    ButtonBoxDefaultImageSizeHorizontal = ImageSizeHorizontal + (int)DefaultStyleOptionOffsets::ButtonBox,
    ButtonBoxDefaultImageSizeVertical   = ImageSizeVertical   + (int)DefaultStyleOptionOffsets::ButtonBox,
    
    FrameBoxDefaultFrameMarginLeft   = FrameMarginLeft   + (int)DefaultStyleOptionOffsets::FrameBox,
    FrameBoxDefaultFrameMarginRight  = FrameMarginRight  + (int)DefaultStyleOptionOffsets::FrameBox,
    FrameBoxDefaultFrameMarginTop    = FrameMarginTop    + (int)DefaultStyleOptionOffsets::FrameBox,
    FrameBoxDefaultFrameMarginBottom = FrameMarginBottom + (int)DefaultStyleOptionOffsets::FrameBox,
    
    GraphicsBoxDefaultImageSizeCommon        = ImageSizeCommon        + (int)DefaultStyleOptionOffsets::GraphicsBox,
    GraphicsBoxDefaultImageSizeHorizontal    = ImageSizeHorizontal    + (int)DefaultStyleOptionOffsets::GraphicsBox,
    GraphicsBoxDefaultImageSizeVertical      = ImageSizeVertical      + (int)DefaultStyleOptionOffsets::GraphicsBox,
    GraphicsBoxDefaultPlotRangePaddingLeft   = PlotRangePaddingLeft   + (int)DefaultStyleOptionOffsets::GraphicsBox,
    GraphicsBoxDefaultPlotRangePaddingRight  = PlotRangePaddingRight  + (int)DefaultStyleOptionOffsets::GraphicsBox,
    GraphicsBoxDefaultPlotRangePaddingTop    = PlotRangePaddingTop    + (int)DefaultStyleOptionOffsets::GraphicsBox,
    GraphicsBoxDefaultPlotRangePaddingBottom = PlotRangePaddingBottom + (int)DefaultStyleOptionOffsets::GraphicsBox,
    
    InputFieldBoxDefaultFrameMarginLeft     = FrameMarginLeft     + (int)DefaultStyleOptionOffsets::InputFieldBox,
    InputFieldBoxDefaultFrameMarginRight    = FrameMarginRight    + (int)DefaultStyleOptionOffsets::InputFieldBox,
    InputFieldBoxDefaultFrameMarginTop      = FrameMarginTop      + (int)DefaultStyleOptionOffsets::InputFieldBox,
    InputFieldBoxDefaultFrameMarginBottom   = FrameMarginBottom   + (int)DefaultStyleOptionOffsets::InputFieldBox,
    InputFieldBoxDefaultImageSizeCommon     = ImageSizeCommon     + (int)DefaultStyleOptionOffsets::InputFieldBox,
    InputFieldBoxDefaultImageSizeHorizontal = ImageSizeHorizontal + (int)DefaultStyleOptionOffsets::InputFieldBox,
    InputFieldBoxDefaultImageSizeVertical   = ImageSizeVertical   + (int)DefaultStyleOptionOffsets::InputFieldBox,
    
    PaneBoxDefaultImageSizeCommon     = ImageSizeCommon     + (int)DefaultStyleOptionOffsets::PaneBox,
    PaneBoxDefaultImageSizeHorizontal = ImageSizeHorizontal + (int)DefaultStyleOptionOffsets::PaneBox,
    PaneBoxDefaultImageSizeVertical   = ImageSizeVertical   + (int)DefaultStyleOptionOffsets::PaneBox,
    
    PanelBoxDefaultFrameMarginLeft     = FrameMarginLeft     + (int)DefaultStyleOptionOffsets::PanelBox,
    PanelBoxDefaultFrameMarginRight    = FrameMarginRight    + (int)DefaultStyleOptionOffsets::PanelBox,
    PanelBoxDefaultFrameMarginTop      = FrameMarginTop      + (int)DefaultStyleOptionOffsets::PanelBox,
    PanelBoxDefaultFrameMarginBottom   = FrameMarginBottom   + (int)DefaultStyleOptionOffsets::PanelBox,
    PanelBoxDefaultImageSizeCommon     = ImageSizeCommon     + (int)DefaultStyleOptionOffsets::PanelBox,
    PanelBoxDefaultImageSizeHorizontal = ImageSizeHorizontal + (int)DefaultStyleOptionOffsets::PanelBox,
    PanelBoxDefaultImageSizeVertical   = ImageSizeVertical   + (int)DefaultStyleOptionOffsets::PanelBox,
    
    PaneSelectorBoxDefaultImageSizeCommon     = ImageSizeCommon     + (int)DefaultStyleOptionOffsets::PaneSelectorBox,
    PaneSelectorBoxDefaultImageSizeHorizontal = ImageSizeHorizontal + (int)DefaultStyleOptionOffsets::PaneSelectorBox,
    PaneSelectorBoxDefaultImageSizeVertical   = ImageSizeVertical   + (int)DefaultStyleOptionOffsets::PaneSelectorBox,
    
    ProgressIndicatorBoxDefaultImageSizeCommon     = ImageSizeCommon     + (int)DefaultStyleOptionOffsets::ProgressIndicatorBox,
    ProgressIndicatorBoxDefaultImageSizeHorizontal = ImageSizeHorizontal + (int)DefaultStyleOptionOffsets::ProgressIndicatorBox,
    ProgressIndicatorBoxDefaultImageSizeVertical   = ImageSizeVertical   + (int)DefaultStyleOptionOffsets::ProgressIndicatorBox,
    
    SetterBoxDefaultFrameMarginLeft     = FrameMarginLeft     + (int)DefaultStyleOptionOffsets::SetterBox,
    SetterBoxDefaultFrameMarginRight    = FrameMarginRight    + (int)DefaultStyleOptionOffsets::SetterBox,
    SetterBoxDefaultFrameMarginTop      = FrameMarginTop      + (int)DefaultStyleOptionOffsets::SetterBox,
    SetterBoxDefaultFrameMarginBottom   = FrameMarginBottom   + (int)DefaultStyleOptionOffsets::SetterBox,
    SetterBoxDefaultImageSizeCommon     = ImageSizeCommon     + (int)DefaultStyleOptionOffsets::SetterBox,
    SetterBoxDefaultImageSizeHorizontal = ImageSizeHorizontal + (int)DefaultStyleOptionOffsets::SetterBox,
    SetterBoxDefaultImageSizeVertical   = ImageSizeVertical   + (int)DefaultStyleOptionOffsets::SetterBox,
    
    SliderBoxDefaultImageSizeCommon     = ImageSizeCommon     + (int)DefaultStyleOptionOffsets::SliderBox,
    SliderBoxDefaultImageSizeHorizontal = ImageSizeHorizontal + (int)DefaultStyleOptionOffsets::SliderBox,
    SliderBoxDefaultImageSizeVertical   = ImageSizeVertical   + (int)DefaultStyleOptionOffsets::SliderBox,
  };
  
  enum StringStyleOptionName {
    BaseStyleName = 0x40000, // greater than any LengthStyleOptionName value
    Method,
    
    LanguageCategory,
    SectionLabel,
    
    WindowTitle,
    
    ButtonBoxDefaultMethod = Method + (int)DefaultStyleOptionOffsets::ButtonBox
  };
  
  enum ObjectStyleOptionName {
    Axes = 0x50000, // greater than any StringStyleOptionName value
    Alignment,
    Ticks,
    Frame,
    FrameTicks,
    AxesOrigin,
    BaselinePosition,
    ScriptSizeMultipliers,
    TextShadow,
    FontFeatures,
    MathFontFamily,
    UnknownOptions,
    EvaluationContext,
    
    ButtonData,
    ButtonFunction,
    CachedValue,
    
    InternalRegisteredBoxID,
    BoxID,
    BoxRotation,
    BoxTransformation,
    PlotRange,
    BorderRadius,
    FrameStyle,
    
    ContextMenu,         // StyleType::AnyFlatList
    DragDropContextMenu, // StyleType::AnyFlatList
    
    DefaultDuplicateSectionStyle,
    DefaultNewSectionStyle,
    DefaultReturnCreatedSectionStyle,
    
    DockedSections,
    DockedSectionsTop,         // StyleType::AnyFlatList
    DockedSectionsTopGlass,    // StyleType::AnyFlatList
    DockedSectionsBottom,      // StyleType::AnyFlatList
    DockedSectionsBottomGlass, // StyleType::AnyFlatList
    
    DocumentEventActions,   // StyleType::AnyFlatList
    FontFamilies,           // StyleType::AnyFlatList
    GeneratedSectionStyles, // StyleType::AnyFlatList
    InputAliases,           // StyleType::AnyFlatList
    InputAutoReplacements,  // StyleType::AnyFlatList
    
    ButtonBoxOptions,
    DynamicBoxOptions,
    DynamicLocalBoxOptions,
    FillBoxOptions,
    FractionBoxOptions,
    FrameBoxOptions,
    GraphicsBoxOptions,
    GridBoxOptions,
    InputFieldBoxOptions,
    OverscriptBoxOptions,
    PaneBoxOptions,
    PanelBoxOptions,
    PaneSelectorBoxOptions,
    ProgressIndicatorBoxOptions,
    SetterBoxOptions,
    SliderBoxOptions,
    TemplateBoxOptions,
    UnderoverscriptBoxOptions,
    UnderscriptBoxOptions,
    
    InternalDeinitialization,
    Deinitialization,
    Initialization,
    TrackedSymbols,
    DynamicLocalValues,
    UnsavedVariables,
    InternalUpdateCause,
    
    DisplayFunction,
    InterpretationFunction,
    SyntaxForm,
    Tooltip,
    
    StyleDefinitions,
    InternalLastStyleDefinitions,
    
    SectionDingbat,
    SectionEvaluationFunction,
    Appearance,
    
    CharacterNameStyle,
    CommentStyle,
    ContainingCallHighlightStyle,
    ExcessOrMissingArgumentStyle,
    FunctionLocalVariableStyle,
    FunctionNameStyle,
    ImplicitOperatorStyle,
    InlineAutoCompletionStyle,
    InlineSectionEditingStyle,
    KeywordSymbolStyle,
    LocalScopeConflictStyle,
    LocalVariableStyle,
    MatchingBracketHighlightStyle,
    OccurenceHighlightStyle,
    PatternVariableStyle,
    StringStyle,
    SymbolShadowingStyle,
    SyntaxErrorStyle,
    UndefinedSymbolStyle,
    UnknownOptionStyle,
    
    Dashing,
    EdgeDashing,
    JoinForm,
    EdgeJoinForm,
    
    ButtonBoxDefaultAlignment                = Alignment              + (int)DefaultStyleOptionOffsets::ButtonBox,
    ButtonBoxDefaultAppearance               = Appearance             + (int)DefaultStyleOptionOffsets::ButtonBox,
    ButtonBoxDefaultBaselinePosition         = BaselinePosition       + (int)DefaultStyleOptionOffsets::ButtonBox,
    ButtonBoxDefaultButtonData               = ButtonData             + (int)DefaultStyleOptionOffsets::ButtonBox,
    ButtonBoxDefaultButtonFunction           = ButtonFunction         + (int)DefaultStyleOptionOffsets::ButtonBox,
    
    DynamicBoxDefaultCachedValue             = CachedValue            + (int)DefaultStyleOptionOffsets::DynamicBox,
    DynamicBoxDefaultDeinitialization        = Deinitialization       + (int)DefaultStyleOptionOffsets::DynamicBox,
    DynamicBoxDefaultInitialization          = Initialization         + (int)DefaultStyleOptionOffsets::DynamicBox,
    DynamicBoxDefaultTrackedSymbols          = TrackedSymbols         + (int)DefaultStyleOptionOffsets::DynamicBox,
    
    DynamicLocalBoxDefaultDeinitialization   = Deinitialization       + (int)DefaultStyleOptionOffsets::DynamicLocalBox,
    DynamicLocalBoxDefaultInitialization     = Initialization         + (int)DefaultStyleOptionOffsets::DynamicLocalBox,
    DynamicLocalBoxDefaultUnsavedVariables   = UnsavedVariables       + (int)DefaultStyleOptionOffsets::DynamicLocalBox,
    DynamicLocalBoxDefaultDynamicLocalValues = DynamicLocalValues     + (int)DefaultStyleOptionOffsets::DynamicLocalBox,
    
    FrameBoxDefaultBorderRadius              = BorderRadius           + (int)DefaultStyleOptionOffsets::FrameBox,
    FrameBoxDefaultBaselinePosition          = BaselinePosition       + (int)DefaultStyleOptionOffsets::FrameBox,
    FrameBoxDefaultFrameStyle                = FrameStyle             + (int)DefaultStyleOptionOffsets::FrameBox,
    
    GraphicsBoxDefaultAxes             = Axes             + (int)DefaultStyleOptionOffsets::GraphicsBox,
    GraphicsBoxDefaultAxesOrigin       = AxesOrigin       + (int)DefaultStyleOptionOffsets::GraphicsBox,
    GraphicsBoxDefaultBaselinePosition = BaselinePosition + (int)DefaultStyleOptionOffsets::GraphicsBox,
    GraphicsBoxDefaultFrame            = Frame            + (int)DefaultStyleOptionOffsets::GraphicsBox,
    GraphicsBoxDefaultFrameTicks       = FrameTicks       + (int)DefaultStyleOptionOffsets::GraphicsBox,
    GraphicsBoxDefaultPlotRange        = PlotRange        + (int)DefaultStyleOptionOffsets::GraphicsBox,
    GraphicsBoxDefaultTicks            = Ticks            + (int)DefaultStyleOptionOffsets::GraphicsBox,
    
    InputFieldBoxDefaultAppearance           = Appearance             + (int)DefaultStyleOptionOffsets::InputFieldBox,
    InputFieldBoxDefaultBaselinePosition     = BaselinePosition       + (int)DefaultStyleOptionOffsets::InputFieldBox,
    
    PaneBoxDefaultAlignment                  = Alignment              + (int)DefaultStyleOptionOffsets::PaneBox,
    PaneBoxDefaultBaselinePosition           = BaselinePosition       + (int)DefaultStyleOptionOffsets::PaneBox,
    
    PanelBoxDefaultAlignment                 = Alignment              + (int)DefaultStyleOptionOffsets::PanelBox,
    PanelBoxDefaultAppearance                = Appearance             + (int)DefaultStyleOptionOffsets::PanelBox,
    PanelBoxDefaultBaselinePosition          = BaselinePosition       + (int)DefaultStyleOptionOffsets::PanelBox,
    
    PaneSelectorBoxDefaultAlignment          = Alignment              + (int)DefaultStyleOptionOffsets::PaneSelectorBox,
    PaneSelectorBoxDefaultBaselinePosition   = BaselinePosition       + (int)DefaultStyleOptionOffsets::PaneSelectorBox,
    
    SetterBoxDefaultAlignment                = Alignment              + (int)DefaultStyleOptionOffsets::SetterBox,
    SetterBoxDefaultBaselinePosition         = BaselinePosition       + (int)DefaultStyleOptionOffsets::SetterBox,
    
    SliderBoxDefaultAppearance               = Appearance             + (int)DefaultStyleOptionOffsets::SliderBox,
    
    TemplateBoxDefaultDisplayFunction        = DisplayFunction        + (int)DefaultStyleOptionOffsets::TemplateBox,
    TemplateBoxDefaultInterpretationFunction = InterpretationFunction + (int)DefaultStyleOptionOffsets::TemplateBox,
    TemplateBoxDefaultSyntaxForm             = SyntaxForm             + (int)DefaultStyleOptionOffsets::TemplateBox,
    TemplateBoxDefaultTooltip                = Tooltip                + (int)DefaultStyleOptionOffsets::TemplateBox,
  };
  
  class StyleOptionName {
      static const int DynamicFlag  = 0x10000000;
      static const int VolatileFlag = 0x20000000;
  
    public:
      StyleOptionName() = default;
      
      explicit StyleOptionName(int value) : _value(value) {}
      StyleOptionName(ColorStyleOptionName value) : _value((int)value) {}
      StyleOptionName(IntStyleOptionName value) : _value((int)value) {}
      StyleOptionName(FloatStyleOptionName value) : _value((int)value) {}
      StyleOptionName(LengthStyleOptionName value) : _value((int)value) {}
      StyleOptionName(StringStyleOptionName value) : _value((int)value) {}
      StyleOptionName(ObjectStyleOptionName value) : _value((int)value) {}
      
      explicit operator ColorStyleOptionName() const {  return (ColorStyleOptionName)_value; }
      explicit operator IntStyleOptionName() const {    return (IntStyleOptionName)_value; }
      explicit operator FloatStyleOptionName() const {  return (FloatStyleOptionName)_value; }
      explicit operator LengthStyleOptionName() const { return (LengthStyleOptionName)_value; }
      explicit operator StringStyleOptionName() const { return (StringStyleOptionName)_value; }
      explicit operator ObjectStyleOptionName() const { return (ObjectStyleOptionName)_value; }
      explicit operator int() const { return _value; }
      
      explicit operator bool() const { return _value >= 0; }
      bool is_valid() const {          return _value >= 0; }
      
      bool is_literal() const {  return (_value & (DynamicFlag | VolatileFlag)) == 0; }
      bool is_dynamic() const {  return (_value & DynamicFlag) != 0; }
      bool is_volatile() const { return (_value & VolatileFlag) != 0;}
      StyleOptionName to_literal() const {  return StyleOptionName { _value & ~(DynamicFlag | VolatileFlag) }; }
      StyleOptionName to_dynamic() const {  return StyleOptionName { (_value & ~VolatileFlag) | DynamicFlag }; }
      StyleOptionName to_volatile() const { return StyleOptionName { (_value & ~DynamicFlag) | VolatileFlag }; }
      
      unsigned int hash() const { return (unsigned int)_value; }
      bool operator==(StyleOptionName other) const { return _value == other._value; }
      bool operator!=(StyleOptionName other) const { return _value != other._value; }
      
    private:
      int _value;
  };
  
  enum class StyleType {
    None,
    Bool,
    AutoBool,
    Color,
    Integer,
    Number,
    Length,
    AutoPositive,
    Margin,
    Size,
    String,
    Any,
    AnyFlatList,
    Enum,
    RuleSet
  };
  
  union IntFloatUnion {
    int   int_value;
    float float_value;
  };
  
  class StyleData: public Observable, public Shareable {
    friend class StyleImpl;
    public:
      StyleData();
      explicit StyleData(Expr options);
      virtual ~StyleData();
      
      void clear();
      static void reset(SharedPtr<StyleData> &style, String base_style_name);
      
      void add_pmath(Expr options, bool amend = true);
      
      void merge(SharedPtr<StyleData> other);
      static bool contains_inherited(Expr expr);
      static Expr merge_style_values(StyleOptionName n, Expr newer, Expr older);
      static Expr finish_style_merge(StyleOptionName n, Expr value);
      
      bool contains(ColorStyleOptionName  n) const { Color  _; return get(n, &_); }
      bool contains(IntStyleOptionName    n) const { int    _; return get(n, &_); }
      bool contains(FloatStyleOptionName  n) const { float  _; return get(n, &_); }
      bool contains(LengthStyleOptionName n) const { Length _; return get(n, &_); }
      bool contains(StringStyleOptionName n) const { String _; return get(n, &_); }
      bool contains(ObjectStyleOptionName n) const { Expr   _; return get(n, &_); }
      
      virtual bool get(ColorStyleOptionName  n, Color  *value) const;
      virtual bool get(IntStyleOptionName    n, int    *value) const;
      virtual bool get(FloatStyleOptionName  n, float  *value) const;
      virtual bool get(LengthStyleOptionName n, Length *value) const;
      virtual bool get(StringStyleOptionName n, String *value) const;
      virtual bool get(ObjectStyleOptionName n, Expr   *value) const;
      
      virtual void set(ColorStyleOptionName  n, Color  value);
      virtual void set(IntStyleOptionName    n, int    value);
      virtual void set(FloatStyleOptionName  n, float  value);
      virtual void set(LengthStyleOptionName n, Length value);
      virtual void set(StringStyleOptionName n, String value);
      virtual void set(ObjectStyleOptionName n, Expr   value);
      
      virtual void remove(ColorStyleOptionName  n);
      virtual void remove(IntStyleOptionName    n);
      virtual void remove(FloatStyleOptionName  n);
      virtual void remove(LengthStyleOptionName n);
      virtual void remove(StringStyleOptionName n);
      virtual void remove(ObjectStyleOptionName n);
      
      void flag_pending_dynamic() { set(InternalHasPendingDynamic, true); }
      
      bool get_dynamic(StyleOptionName n, Expr *value) const {
        return get((ObjectStyleOptionName)n.to_dynamic(), value);
      }
      
      unsigned int count() const;
      
      static int decode_enum(Expr expr, IntStyleOptionName n, int def);
      
      bool set_pmath(StyleOptionName n, Expr obj);
      
      Expr get_pmath(StyleOptionName n) const;
      
      void emit_pmath(StyleOptionName n) const;
      
      void emit_to_pmath(bool with_inherited = false) const;
      
      static bool modifies_size(StyleOptionName style_name);
      
      static bool is_style_name(Expr n);
      static StyleOptionName get_key(Expr n);
      
      static Expr get_name(StyleOptionName n);
      
      static enum StyleType get_type(StyleOptionName n);
      
      static Expr get_current_style_value(FrontEndObject *obj, Expr item);
      static bool put_current_style_value(FrontEndObject *obj, Expr item, Expr rhs);
      
    private:
      Hashtable<StyleOptionName, IntFloatUnion> int_float_values;
      Hashtable<StyleOptionName, Expr>          object_values;
  };
  
  class Style {
    public:
      Style() : data(nullptr) {}
      Style(decltype(nullptr)) : data(nullptr) {}
      explicit Style(SharedPtr<StyleData> data) : data(PMATH_CPP_MOVE(data)) {}
      explicit Style(Expr options) : data(new StyleData(PMATH_CPP_MOVE(options))) {}

      static Style New() { Style s; s.reset_new(); return s; }

      Style copy() const { Style s; if(data) { s.data = new StyleData(); s.data->merge(data); } return s; }
      
      void add_pmath(Expr options, bool amend = true) { if(!data) { if(options.expr_length() == 0) return; data = new StyleData(); } data->add_pmath(PMATH_CPP_MOVE(options), amend); }
      void merge(Style other) { if(!other) return; if(!data) { data = new StyleData(); } data->merge(other.data); }

      void clear() { if(data) data->clear(); }
      void reset_new() { data = new StyleData(); }
      void reset() { data = nullptr; }
      void reset(String base_style_name) { StyleData::reset(data, PMATH_CPP_MOVE(base_style_name)); }
      
      bool is_valid() const { return data.is_valid(); }
      explicit operator bool() const { return data.is_valid(); }

      bool get(ColorStyleOptionName  n, Color  *value) const { return data ? data->get(n, value) : false; }
      bool get(IntStyleOptionName    n, int    *value) const { return data ? data->get(n, value) : false; }
      bool get(FloatStyleOptionName  n, float  *value) const { return data ? data->get(n, value) : false; }
      bool get(LengthStyleOptionName n, Length *value) const { return data ? data->get(n, value) : false; }
      bool get(StringStyleOptionName n, String *value) const { return data ? data->get(n, value) : false; }
      bool get(ObjectStyleOptionName n, Expr   *value) const { return data ? data->get(n, value) : false; }
    
      Expr get_pmath(StyleOptionName n) const;
      
      bool get_dynamic(StyleOptionName n, Expr *value) const { return data ? data->get_dynamic(n, value) : false; }
      
      bool contains(ColorStyleOptionName  n) const { return data && data->contains(n); }
      bool contains(IntStyleOptionName    n) const { return data && data->contains(n); }
      bool contains(FloatStyleOptionName  n) const { return data && data->contains(n); }
      bool contains(LengthStyleOptionName n) const { return data && data->contains(n); }
      bool contains(StringStyleOptionName n) const { return data && data->contains(n); }
      bool contains(ObjectStyleOptionName n) const { return data && data->contains(n); }
      
      void set(ColorStyleOptionName  n, Color  value) { if(!data) { data = new StyleData(); } data->set(n, value); }
      void set(IntStyleOptionName    n, int    value) { if(!data) { data = new StyleData(); } data->set(n, value); }
      void set(FloatStyleOptionName  n, float  value) { if(!data) { data = new StyleData(); } data->set(n, value); }
      void set(LengthStyleOptionName n, Length value) { if(!data) { data = new StyleData(); } data->set(n, value); }
      void set(StringStyleOptionName n, String value) { if(!data) { data = new StyleData(); } data->set(n, PMATH_CPP_MOVE(value)); }
      void set(ObjectStyleOptionName n, Expr   value) { if(!data) { data = new StyleData(); } data->set(n, PMATH_CPP_MOVE(value)); }
      
      bool set_pmath(StyleOptionName n, Expr obj) { if(!data) { data = new StyleData(); } return data->set_pmath(n, PMATH_CPP_MOVE(obj)); }
      
      void remove(ColorStyleOptionName  n) { if(data) data->remove(n); }
      void remove(IntStyleOptionName    n) { if(data) data->remove(n); }
      void remove(FloatStyleOptionName  n) { if(data) data->remove(n); }
      void remove(LengthStyleOptionName n) { if(data) data->remove(n); }
      void remove(StringStyleOptionName n) { if(data) data->remove(n); }
      void remove(ObjectStyleOptionName n) { if(data) data->remove(n); }
      
      void emit_to_pmath(bool with_inherited = false) const { if(data) data->emit_to_pmath(with_inherited); }
      void emit_pmath(StyleOptionName n) const { if(data) { data->emit_pmath(n); } };
      
      void flag_pending_dynamic() { if(data) { data->flag_pending_dynamic(); } }
      
      void register_observer() const {                     if(data) { data->register_observer(); } }
      void register_observer(FrontEndReference id) const { if(data) { data->register_observer(id); } }
      
      void notify_all() { if(data) { data->notify_all(); } }

    public:
      SharedPtr<StyleData> data;
  };
  
  class Stylesheet: public FrontEndObject, public Shareable {
      friend class StylesheetImpl;
    public:
      Stylesheet();
      virtual ~Stylesheet() override;
      
      Style find_parent_style(Style s);
      
      // each get() ignores base:
      bool get(Style s, ColorStyleOptionName  n, Color  *value);
      bool get(Style s, IntStyleOptionName    n, int    *value);
      bool get(Style s, FloatStyleOptionName  n, float  *value);
      bool get(Style s, LengthStyleOptionName n, Length *value);
      bool get(Style s, StringStyleOptionName n, String *value);
      bool get(Style s, ObjectStyleOptionName n, Expr   *value);
      
      Expr get_pmath(Style s, StyleOptionName n);
      
      DynamicUpdateKind update_dynamic(Style s, StyledObject *parent, Evaluator evaluator);
      
      using IterBoxReferences = MultiMap<Expr, FrontEndReference>::ValuesIterable;
      static IterBoxReferences find_registered_box(Expr box_id);
      static void update_box_registry(StyledObject *obj);
      //static void unregister_box(StyledObject *obj);
      
      Color  get_or_default(Style s, ColorStyleOptionName n,  Color  fallback_result = Color::None) { get(PMATH_CPP_MOVE(s), n, &fallback_result); return fallback_result; }
      int    get_or_default(Style s, IntStyleOptionName n,    int    fallback_result = 0) {           get(PMATH_CPP_MOVE(s), n, &fallback_result); return fallback_result; }
      float  get_or_default(Style s, FloatStyleOptionName n,  float  fallback_result = 0.0f) {        get(PMATH_CPP_MOVE(s), n, &fallback_result); return fallback_result; }
      Length get_or_default(Style s, LengthStyleOptionName n, Length fallback_result = Length(0.0)) { get(PMATH_CPP_MOVE(s), n, &fallback_result); return fallback_result; }
      String get_or_default(Style s, StringStyleOptionName n, String fallback_result) {               get(PMATH_CPP_MOVE(s), n, &fallback_result); return fallback_result; }
      Expr   get_or_default(Style s, ObjectStyleOptionName n, Expr   fallback_result) {               get(PMATH_CPP_MOVE(s), n, &fallback_result); return fallback_result; }
      String get_or_default(Style s, StringStyleOptionName n) { return get_or_default(PMATH_CPP_MOVE(s), n, String{}); }
      Expr   get_or_default(Style s, ObjectStyleOptionName n) { return get_or_default(PMATH_CPP_MOVE(s), n, Expr{}); }
      
      Expr name() { return _name; }
      void unregister();
      bool register_as(Expr name);
      static SharedPtr<Stylesheet> find_registered(Expr name);
      
      static SharedPtr<Stylesheet> try_load(Expr expr);
      static Expr name_from_path(String filename);
      static String path_for_name(Expr name);
      void add(Expr expr);
      void reload();
      void reload(Expr expr);
      
      virtual void dynamic_updated() override {}
      
      void add_user(FrontEndObject *obj) const {
        RICHMATH_ASSERT(obj);
        if(users.add(obj->id())) {
          for(auto &other : used_stylesheets)
            other->add_user(obj);
        }
      }
      
      Hashset<FrontEndReference>::KeyEnum enum_users() const {
        return users.keys();
      }
    
    protected:
      virtual ObjectWithLimbo *next_in_limbo() final override { return _limbo_next; }
      virtual void next_in_limbo(ObjectWithLimbo *next) final override { RICHMATH_ASSERT(!_limbo_next); _limbo_next = next; }
    
    public:
      static SharedPtr<Stylesheet> Default;
      
      Hashtable<String, Style > styles;
    
    private:
      Hashset<SharedPtr<Stylesheet>> used_stylesheets;
      mutable Hashset<FrontEndReference> users;
      
    public:
      Style base;
      
    private:
      Expr _name;
      Expr _loaded_definition;
      ObjectWithLimbo *_limbo_next;
  };
};

#endif // RICHMATH__UTIL__STYLE_H__INCLUDED
