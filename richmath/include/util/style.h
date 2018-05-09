#ifndef __UTIL__STYLE_H__
#define __UTIL__STYLE_H__

#include <util/hashtable.h>
#include <util/pmath-extra.h>
#include <util/sharedptr.h>


namespace richmath {
  class Box;
  
  Expr color_to_pmath(int color);
  int pmath_to_color(Expr obj); // -2 on error, -1=None
  
  enum class DefaultStyleOptionOffsets {
    None = 0,
    TemplateBox = 0x00100000
  };
  
  enum IntStyleOptionName {
    Background = 0x00000,
    FontColor,
    SectionFrameColor,
    
    Antialiasing, // 0=off, 1=on, 2=default
    
    FontSlant,
    FontWeight,
    
    AutoDelete,
    AutoNumberFormating,
    AutoSpacing,
    ContinuousAction,
    Editable,
    Evaluatable,
    InternalHasModifiedWindowOption,
    InternalHasPendingDynamic,
    InternalUsesCurrentValueOfMouseOver,
    LineBreakWithin,
    Placeholder,
    ReturnCreatesNewSection,
    SectionEditDuplicate,
    SectionEditDuplicateMakesCopy,
    SectionGenerated,
    SectionLabelAutoDelete,
    Selectable,
    ShowAutoStyles,
    ShowSectionBracket,
    ShowStringCharacters,
    StripOnInput,
    Visible,
    
    ButtonFrame, // -1 = Automatic,  other: ContainerType value
    WindowFrame  // WindowFrameType
  };
  
  enum {
    FontWeightPlain = 0,
    FontWeightBold  = 100
  };
  
  typedef enum {
    WindowFrameNormal  = 0,
    WindowFramePalette = 1,
    WindowFrameDialog  = 2
  } WindowFrameType;
  
  static const float ImageSizeAutomatic = -1.0f;
  
  enum FloatStyleOptionName {
    FontSize = 0x10000, // greater than any IntStyleOptionName value
    
    AspectRatio,
    Magnification,
    
    GridBoxColumnSpacing,
    GridBoxRowSpacing,
    
    ImageSizeCommon,
    ImageSizeHorizontal, // > 0 or ImageSizeAutomatic or ImageSizeAll
    ImageSizeVertical,   // > 0 or ImageSizeAutomatic
    
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
    
    SectionGroupPrecedence
  };
  
  enum StringStyleOptionName {
    BaseStyleName = 0x20000, // greater than any FloatStyleOptionName value
    Method,
    
    LanguageCategory,
    SectionLabel,
    
    WindowTitle
  };
  
  enum ObjectStyleOptionName {
    Axes = 0x30000, // greater than any StringStyleOptionName value
    Ticks,
    Frame,
    FrameTicks,
    AxesOrigin,
    
    ButtonFunction,
    ScriptSizeMultipliers,
    TextShadow,
    FontFamilies,
    FontFeatures,
    UnknownOptions,
    
    BoxRotation,
    BoxTransformation,
    PlotRange,
    BorderRadius,
    
    DefaultDuplicateSectionStyle,
    DefaultNewSectionStyle,
    DefaultReturnCreatedSectionStyle,
    
    DockedSections,
    DockedSectionsTop,
    DockedSectionsTopGlass,
    DockedSectionsBottom,
    DockedSectionsBottomGlass,
    
    DisplayFunction,
    InterpretationFunction,
    SyntaxForm,
    Tooltip,
    TemplateBoxOptions,
    
    StyleDefinitions,
    InternalLastStyleDefinitions,
    GeneratedSectionStyles,
    
    TemplateBoxDefaultDisplayFunction        = (int)DefaultStyleOptionOffsets::TemplateBox + DisplayFunction,
    TemplateBoxDefaultInterpretationFunction = (int)DefaultStyleOptionOffsets::TemplateBox + InterpretationFunction
  };
  
  class StyleOptionName {
      static const int DynamicFlag = 0x10000000;
  
    public:
      StyleOptionName() = default;
      
      explicit StyleOptionName(int value) : _value(value) {}
      StyleOptionName(IntStyleOptionName value) : _value((int)value) {}
      StyleOptionName(FloatStyleOptionName value) : _value((int)value) {}
      StyleOptionName(StringStyleOptionName value) : _value((int)value) {}
      StyleOptionName(ObjectStyleOptionName value) : _value((int)value) {}
      
      explicit operator IntStyleOptionName() const {
        return (IntStyleOptionName)_value;
      }
      explicit operator FloatStyleOptionName() const {
        return (FloatStyleOptionName)_value;
      }
      explicit operator StringStyleOptionName() const {
        return (StringStyleOptionName)_value;
      }
      explicit operator ObjectStyleOptionName() const {
        return (ObjectStyleOptionName)_value;
      }
      explicit operator int() const {
        return _value;
      }
      
      bool is_valid() const {
        return _value >= 0;
      }
      
      bool is_literal() const {
        return (_value & DynamicFlag) == 0;
      }
      bool is_dynamic() const {
        return (_value & DynamicFlag) != 0;
      }
      StyleOptionName to_dynamic() const {
        return StyleOptionName { _value | DynamicFlag };
      }
      StyleOptionName to_literal() const {
        return StyleOptionName { _value & ~DynamicFlag };
      }
      
      unsigned int hash() const {
        return (unsigned int)_value;
      }
  
      bool operator==(StyleOptionName other) const {
        return _value == other._value;
      }
      
      bool operator!=(StyleOptionName other) const {
        return _value != other._value;
      }
      
    private:
      int _value;
  };
  
  enum StyleType {
    StyleTypeNone,
    
    StyleTypeBool,
    StyleTypeBoolAuto,
    StyleTypeColor,
    StyleTypeNumber,
    StyleTypeMargin,
    StyleTypeSize,
    StyleTypeString,
    StyleTypeAny,
    
    StyleTypeEnum,
    StyleTypeRuleSet
  };
  
  typedef union {
    int   int_value;
    float float_value;
  } IntFloatUnion;
  
  class Style: public Shareable {
    friend class StyleImpl;
    public:
      Style();
      Style(Expr options);
      virtual ~Style();
      
      void clear();
      void add_pmath(Expr options);
      
      void merge(SharedPtr<Style> other);
      
      virtual bool get(IntStyleOptionName    n, int    *value) const;
      virtual bool get(FloatStyleOptionName  n, float  *value) const;
      virtual bool get(StringStyleOptionName n, String *value) const;
      virtual bool get(ObjectStyleOptionName n, Expr   *value) const;
      
      virtual void set(IntStyleOptionName    n, int    value);
      virtual void set(FloatStyleOptionName  n, float  value);
      virtual void set(StringStyleOptionName n, String value);
      virtual void set(ObjectStyleOptionName n, Expr   value);
      
      virtual void remove(IntStyleOptionName    n);
      virtual void remove(FloatStyleOptionName  n);
      virtual void remove(StringStyleOptionName n);
      virtual void remove(ObjectStyleOptionName n);
      
      bool get_dynamic(StyleOptionName n, Expr *value) const {
        return get((ObjectStyleOptionName)n.to_dynamic(), value);
      }
      
      void set_dynamic(StyleOptionName n, Expr value) {
        ObjectStyleOptionName dn = (ObjectStyleOptionName)n.to_dynamic();
        remove(dn);
        set(dn, value);
        
        set(InternalHasPendingDynamic, true);
      }
      
      void remove_dynamic(StyleOptionName n) {
        remove((ObjectStyleOptionName)n.to_dynamic());
      }
      
      unsigned int count() const;
      
      void set_pmath(StyleOptionName n, Expr obj);
      void set_pmath_by_unknown_key(Expr lhs, Expr rhs);
      
      Expr get_pmath(StyleOptionName n) const;
      
      bool update_dynamic(Box *parent);
      
      void emit_pmath(StyleOptionName n) const;
      
      void emit_to_pmath(bool with_inherited = false) const;
      
      static bool modifies_size(StyleOptionName style_name);
      
      static bool is_style_name(Expr n);
      static StyleOptionName get_key(Expr n);
      
      static Expr get_name(StyleOptionName n);
      
      static enum StyleType get_type(StyleOptionName n);
      
    protected:
      Expr get_pmath_bool_auto( IntStyleOptionName    n) const;
      Expr get_pmath_bool(      IntStyleOptionName    n) const;
      Expr get_pmath_color(     IntStyleOptionName    n) const;
      Expr get_pmath_float(     FloatStyleOptionName  n) const;
      Expr get_pmath_margin(    FloatStyleOptionName  n) const; // n + {0,1,2,3} ~= {Left, Right, Top, Bottom}
      Expr get_pmath_size(      FloatStyleOptionName  n) const; // n + {0,1,2} ~= {Common, Horizontal, Vertical}
      Expr get_pmath_string(    StringStyleOptionName n) const;
      Expr get_pmath_object(    ObjectStyleOptionName n) const;
      Expr get_pmath_enum(      IntStyleOptionName    n) const;
      Expr get_pmath_ruleset(   ObjectStyleOptionName n) const;
      
    private:
      Hashtable<StyleOptionName, IntFloatUnion> int_float_values;
      Hashtable<StyleOptionName, Expr>          object_values;
  };
  
  class Stylesheet: public Shareable {
      friend class StylesheetImpl;
    public:
      virtual ~Stylesheet() override;
      
      SharedPtr<Style> find_parent_style(SharedPtr<Style> s);
      
      // each get() ignores base:
      bool get(SharedPtr<Style> s, IntStyleOptionName    n, int    *value);
      bool get(SharedPtr<Style> s, FloatStyleOptionName  n, float  *value);
      bool get(SharedPtr<Style> s, StringStyleOptionName n, String *value);
      bool get(SharedPtr<Style> s, ObjectStyleOptionName n, Expr   *value);
      
      Expr get_pmath(SharedPtr<Style> s, StyleOptionName n);
      
      bool update_dynamic(SharedPtr<Style> s, Box *parent);
      
      int    get_with_base(SharedPtr<Style> s, IntStyleOptionName    n);
      float  get_with_base(SharedPtr<Style> s, FloatStyleOptionName  n);
      String get_with_base(SharedPtr<Style> s, StringStyleOptionName n);
      Expr   get_with_base(SharedPtr<Style> s, ObjectStyleOptionName n);
      
      Expr get_pmath_with_base(SharedPtr<Style> s, StyleOptionName n);
      
      Expr name() { return _name; }
      void unregister();
      bool register_as(Expr name);
      static SharedPtr<Stylesheet> find_registered(Expr name);
      
      static SharedPtr<Stylesheet> try_load(Expr expr);
      void add(Expr expr);
      
    public:
      static SharedPtr<Stylesheet> Default;
      
      Hashtable<String, SharedPtr<Style> > styles;
      SharedPtr<Style> base;
      
    private:
      Expr _name;
  };
};

#endif // __UTIL__STYLE_H__
